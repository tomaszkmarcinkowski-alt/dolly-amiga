#!/usr/bin/env python3
"""Deterministic, synthetic, read-only CLI integration smoke tests."""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

BLOCK = 512

def put_be32(buffer, offset, value):
    struct.pack_into('>I', buffer, offset, value & 0xFFFFFFFF)

def checksum(buffer, nlongs=64):
    put_be32(buffer, 4, nlongs)
    put_be32(buffer, 8, 0)
    total = sum(struct.unpack_from('>I', buffer, i * 4)[0] for i in range(nlongs))
    put_be32(buffer, 8, -total)

def image_file(path):
    mbr = bytearray(BLOCK)
    for slot, ptype, lba, count in [(0, 0x0c, 2048, 2048), (1, 0x76, 4096, 4096)]:
        entry = 446 + 16 * slot
        mbr[entry + 4] = ptype
        struct.pack_into('<II', mbr, entry + 8, lba, count)
    mbr[510:512] = b'\x55\xaa'
    rdb = bytearray(BLOCK)
    rdb[:4] = b'RDSK'
    put_be32(rdb, 16, BLOCK)
    put_be32(rdb, 28, 2)
    put_be32(rdb, 64, 100)
    put_be32(rdb, 68, 16)
    put_be32(rdb, 72, 4)
    checksum(rdb)
    part = bytearray(BLOCK)
    part[:4] = b'PART'
    put_be32(part, 16, 0xFFFFFFFF)
    part[36:40] = b'\x03DH0'
    put_be32(part, 128, 17)
    put_be32(part, 132, 128)
    put_be32(part, 140, 4)
    put_be32(part, 148, 16)
    put_be32(part, 164, 2)
    put_be32(part, 168, 4)
    put_be32(part, 192, 0x444F5301)
    checksum(part)
    with path.open('wb') as out:
        out.write(mbr)
        out.seek(4096 * BLOCK + BLOCK)
        out.write(rdb)
        out.write(part)
        out.truncate(8192 * BLOCK)

def main(layout, iff):
    with tempfile.TemporaryDirectory(prefix='dolly-cli-') as directory:
        disk = Path(directory) / 'sd.img'
        archive = Path(directory) / 'test.dly'
        image_file(disk)
        result = subprocess.run([layout, str(disk)], capture_output=True, text=True)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert 'type=76' in result.stdout and 'DH0' in result.stdout
        assert 'type=0C' in result.stdout and 'RDB block' in result.stdout
        anno = b'abc'
        archive.write_bytes(b'FORM' + struct.pack('>I', 16) + b'DOLY' +
                            b'ANNO' + struct.pack('>I', 3) + anno + b'\x00')
        result = subprocess.run([iff, str(archive)], capture_output=True, text=True)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert 'ANNO' in result.stdout and 'envelope valid' in result.stdout
        archive.write_bytes(b'FORM' + struct.pack('>I', 16) + b'DOLY' +
                            b'ANNO' + struct.pack('>I', 65536) + anno + b'\x00')
        result = subprocess.run([iff, str(archive)], capture_output=True, text=True)
        assert result.returncode != 0 and 'IFF:' in result.stderr
    print('Synthetic layout and IFF CLI smoke tests passed')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit('Usage: test_cli_smoke.py DOLLY_LAYOUT DOLLY_IFF')
    main(*sys.argv[1:])
