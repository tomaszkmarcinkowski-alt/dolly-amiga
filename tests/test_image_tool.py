import pathlib
import struct
import subprocess
import sys
import tempfile


def run(tool, *args, success=True):
    result = subprocess.run([tool, *map(str, args)], capture_output=True)
    assert (result.returncode == 0) == success, (
        result.args, result.stdout, result.stderr
    )
    return result


def test_ffs_resize(tool, folder):
    def block(words, checksum):
        words[checksum] = -sum(words) & 0xffffffff
        return struct.pack(">128I", *words)

    sectors, old_root, child, bitmap = 200, 100, 16, 3
    source = folder / "ffs-source.raw"
    content = bytearray(sectors * 512)
    struct.pack_into(">3I", content, 0, 0x444f5303, 0, old_root)

    root_words = [0] * 128
    root_words[0], root_words[3], root_words[6] = 2, 72, child
    root_words[78], root_words[79], root_words[127] = 0xffffffff, bitmap, 1
    content[old_root * 512:(old_root + 1) * 512] = block(root_words, 5)

    child_words = [0] * 128
    child_words[0], child_words[1], child_words[125], child_words[127] = 2, child, old_root, 2
    content[child * 512:(child + 1) * 512] = block(child_words, 5)

    bitmap_words = [0] * 128
    for sector in range(2, sectors):
        if sector not in (old_root, child, bitmap):
            offset = sector - 2
            bitmap_words[1 + offset // 32] |= 1 << (offset % 32)
    content[bitmap * 512:(bitmap + 1) * 512] = block(bitmap_words, 0)
    source.write_bytes(content)

    for target in (150, 12000):
        destination = folder / f"resized-{target}.raw"
        run(tool, "resize-ffs", source, destination, 0, sectors, target)
        resized = destination.read_bytes()
        new_root = (target + 1) // 2
        assert len(resized) == target * 512
        assert struct.unpack_from(">I", resized, 8)[0] == new_root
        assert struct.unpack_from(">I", resized, child * 512 + 125 * 4)[0] == new_root
        old_bit = old_root - 2
        word, = struct.unpack_from(">I", resized, bitmap * 512 + (1 + old_bit // 32) * 4)
        assert word & (1 << (old_bit % 32))
        run(tool, "resize-ffs", source, destination, 0, sectors, target, success=False)
        assert destination.read_bytes() == resized
        assert source.read_bytes() == content

    content[512 + 13] = 1
    source.write_bytes(content)
    rejected = folder / "unsupported-boot.raw"
    run(tool, "resize-ffs", source, rejected, 0, sectors, 150, success=False)
    assert not rejected.exists()


def test_apple_export(tool, folder):
    sector = 512

    def entry(index, name, kind, start, length, data_start=0, data_length=None):
        at = index * sector
        content[at:at + sector] = bytes(sector)
        struct.pack_into(">HHIII32s32sII", content, at, 0x504d, 0, 4, start,
                         length, name, kind, data_start,
                         length if data_length is None else data_length)

    content = bytearray(32 * sector)
    struct.pack_into(">HHI8xHIHH", content, 0, 0x4552, sector, 30, 1, 5, 1, 1)
    entry(1, b"Apple", b"Apple_partition_map", 1, 4)
    entry(2, b"Driver", b"Apple_Driver43", 5, 1)
    entry(3, b"System", b"Apple_HFS", 6, 10, 1, 8)
    entry(4, b"Data", b"Apple_HFS", 16, 14)
    content[5 * sector:7 * sector] = b"D" * (2 * sector)
    content[30 * sector:] = b"T" * (2 * sector)
    struct.pack_into(">HH", content, 9 * sector, 0x482b, 4)
    struct.pack_into(">II", content, 9 * sector + 40, sector, 8)
    struct.pack_into(">HH", content, 18 * sector, 0x4858, 5)
    struct.pack_into(">II", content, 18 * sector + 40, sector, 14)
    source, selected, whole = (folder / name for name in ("apple.hda", "volume.hfv", "whole.hda"))
    source.write_bytes(content)

    result = run(tool, "export-apple", source, selected, success=False)
    assert b"3  System  HFS Plus" in result.stderr and b"4  Data  HFSX" in result.stderr
    assert not selected.exists()
    run(tool, "export-apple", source, selected, 2, success=False)
    assert not selected.exists()
    run(tool, "export-apple", source, selected, 3)
    assert selected.read_bytes() == content[7 * sector:15 * sector]
    run(tool, "export-apple", source, selected, 4, success=False)
    assert selected.read_bytes() == content[7 * sector:15 * sector]
    run(tool, "export-apple", source, whole, "--whole")
    assert whole.read_bytes() == source.read_bytes() == content

    single_disk, auto = (folder / name for name in ("single.hda", "auto.hfv"))
    entry(4, b"Free", b"Apple_Free", 16, 14)
    single_disk.write_bytes(content)
    run(tool, "export-apple", single_disk, auto)
    assert auto.read_bytes() == content[7 * sector:15 * sector]

    broken = folder / "broken.hda"
    struct.pack_into(">I", content, 4 * sector + 8, 15)
    broken.write_bytes(content)
    rejected = folder / "unsafe.hfv"
    run(tool, "export-apple", broken, rejected, 3, success=False)
    assert not rejected.exists()


def test_apple_import(tool, folder):
    sector = 512
    volume = bytearray(9 * sector)
    volume[0:4] = b"boot"
    struct.pack_into(">HH", volume, 2 * sector, 0x482b, 4)
    struct.pack_into(">II", volume, 2 * sector + 40, sector, 9)
    volume[-8:] = b"sentinel"
    template = bytearray(16 * sector)
    struct.pack_into(">HHI8xHIHH", template, 0, 0x4552, sector, 16, 1, 8, 2, 1)
    for index, (kind, start, length) in enumerate((
        (b"Apple_partition_map", 1, 5), (b"Apple_Driver43", 8, 2), (b"Apple_HFS", 12, 4)
    ), 1):
        struct.pack_into(">HHIII32s32sII", template, index * sector, 0x504d, 0,
                         3, start, length, b"Template", kind, 0, length)
    template[4 * sector:8 * sector] = b"G" * (4 * sector)
    template[8 * sector:10 * sector] = b"D" * (2 * sector)
    template[10 * sector:12 * sector] = b"G" * (2 * sector)
    template[12 * sector:] = b"V" * (4 * sector)
    source, donor, output, exported = (folder / name for name in (
        "import.hfv", "driver.hda", "generated.hda", "generated.hfv"))
    source.write_bytes(volume)
    donor.write_bytes(template)

    run(tool, "import-apple", source, donor, output)
    disk = output.read_bytes()
    assert len(disk) == 21 * sector
    assert struct.unpack_from(">I", disk, 4)[0] == 21
    assert struct.unpack_from(">I", disk, 3 * sector + 12)[0] == 9
    assert struct.unpack_from(">I", disk, 3 * sector + 84)[0] == 9
    assert disk[8 * sector:10 * sector] == template[8 * sector:10 * sector]
    assert disk[4 * sector:8 * sector] == bytes(4 * sector)
    assert disk[10 * sector:12 * sector] == bytes(2 * sector)
    assert disk[12 * sector:] == volume
    assert source.read_bytes() == volume and donor.read_bytes() == template
    run(tool, "export-apple", output, exported)
    assert exported.read_bytes() == volume
    run(tool, "import-apple", source, donor, output, success=False)
    assert output.read_bytes() == disk

    invalid = folder / "invalid-driver.hda"
    rejected = folder / "rejected.hda"
    struct.pack_into(">I", template, 18, 12)
    invalid.write_bytes(template)
    run(tool, "import-apple", source, invalid, rejected, success=False)
    assert not rejected.exists()
    run(tool, "import-apple", output, donor, rejected, success=False)
    assert not rejected.exists()


def main(tool):
    with tempfile.TemporaryDirectory() as temp:
        root = pathlib.Path(temp)
        test_ffs_resize(tool, root)
        test_apple_export(tool, root)
        test_apple_import(tool, root)
        source = root / "disk.raw"
        payload = b"".join(bytes([i]) * 512 for i in range(9))
        source.write_bytes(payload)
        parts = root / "parts"
        run(tool, "split", source, parts, 4)
        assert sorted(f.name for f in parts.iterdir()) == [
            "disk.001", "disk.002", "disk.dly"
        ]
        first = parts / "disk.dly"
        later = parts / "disk.001"
        run(tool, "verify", first)
        run(tool, "verify", later, success=False)
        joined = root / "joined.raw"
        run(tool, "join", first, joined)
        assert joined.read_bytes() == payload
        run(tool, "join", first, joined, success=False)
        assert joined.read_bytes() == payload
        run(tool, "split", source, parts, 4, success=False)

        second_set = root / "other"
        run(tool, "split", source, second_set, 4)
        original = later.read_bytes()
        later.write_bytes((second_set / later.name).read_bytes())
        run(tool, "verify", first, success=False)
        later.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
        failed = root / "failed.raw"
        run(tool, "join", first, failed, success=False)
        assert not failed.exists()
        later.unlink()
        run(tool, "verify", first, success=False)

        single = root / "single"
        run(tool, "split", source, single)
        run(tool, "verify", single / first.name)
        standalone = root / "standalone.raw"
        run(tool, "join", single / first.name, standalone)
        assert standalone.read_bytes() == payload

        fast = root / "fast"
        run(tool, "split", source, fast, "--fast", 4)
        assert sum(f.stat().st_size for f in fast.iterdir()) < len(payload)
        for part in fast.iterdir():
            assert int.from_bytes(part.read_bytes()[20:24], "big") == 2
        run(tool, "verify", fast / first.name)
        expanded = root / "expanded.raw"
        run(tool, "join", fast / first.name, expanded)
        assert expanded.read_bytes() == payload
        run(tool, "split", source, root / "invalid", 4, "--unknown", success=False)


        sparse_source = root / "sparse.raw"
        sparse_payload = bytes(16384) + b"!" * 16384 + bytes(33280)
        sparse_source.write_bytes(sparse_payload)
        sparse_parts = root / "sparse-parts"
        run(tool, "split", sparse_source, sparse_parts, 32, "--fast")
        sparse_output = root / "sparse-joined.raw"
        run(tool, "join", sparse_parts / "sparse.dly", sparse_output, "--sparse")
        assert sparse_output.read_bytes() == sparse_payload
        zero_source = root / "zero.raw"
        zero_source.write_bytes(bytes(512))
        zero_dir = root / "zero"
        run(tool, "split", zero_source, zero_dir, "--fast")
        zero_output = root / "zero-out.raw"
        run(tool, "join", zero_dir / "zero.dly", zero_output, "--sparse")
        assert zero_output.read_bytes() == bytes(512)


if __name__ == "__main__":
    main(sys.argv[1])
