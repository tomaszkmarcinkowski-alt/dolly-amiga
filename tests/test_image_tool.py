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


def main(tool):
    with tempfile.TemporaryDirectory() as temp:
        root = pathlib.Path(temp)
        test_ffs_resize(tool, root)
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
