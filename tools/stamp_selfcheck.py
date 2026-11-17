#!/usr/bin/env python3
"""Stamp/verify the CRC32 field INSIDE the Dolly executable, no sidecar.
CRC32 covers the entire executable, with ONLY the eight CRC ASCII digits
temporarily treated as '00000000'. Includes 16-byte release stamp.
This detects accidental damage, NOT malicious tampering/authenticity.
"""
import argparse
from pathlib import Path
import zlib

MAGIC = b"DOLLY:CRC32:V1!!"
RECORD_SIZE = 40
CRC_START = 16
CRC_END = 24
VERSION_END = 40
UNSTAMPED = b"DOLLY-UNSTAMPED!"

def locate(payload: bytes) -> int:
    pos = payload.find(MAGIC)
    if pos < 0 or payload.find(MAGIC, pos + 1) != -1:
        raise ValueError("expected exactly one Dolly CRC record")
    if len(payload) < pos + RECORD_SIZE:
        raise ValueError("truncated Dolly CRC record")
    return pos

def checksum(payload: bytes, pos: int) -> int:
    # The CRC slot is exactly eight ASCII zeroes while calculating.
    value = zlib.crc32(payload[:pos+CRC_START])
    value = zlib.crc32(b"00000000", value)
    value = zlib.crc32(payload[pos+CRC_END:], value)
    return value & 0xFFFFFFFF

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--release", help="ASCII release ID, 1..16 bytes")
    parser.add_argument("--verify", action="store_true")
    opts = parser.parse_args()
    payload = bytearray(opts.executable.read_bytes())
    pos = locate(payload)
    if opts.verify:
        expected = payload[pos+CRC_START:pos+CRC_END]
        version = bytes(payload[pos+CRC_END:pos+VERSION_END]).decode("ascii")
        if expected == b"00000000" or version == UNSTAMPED.decode("ascii"):
            parser.error("executable has not been stamped")
        if opts.release and version.rstrip() != opts.release:
            parser.error("release mismatch")
        if expected.decode("ascii") != f"{checksum(payload, pos):08X}":
            parser.error("executable CRC32 mismatch")
        print(f"Dolly self CRC32 OK, release {version.rstrip()}")
        return
    if not opts.release:
        parser.error("--release is required for stamping")
    try:
        release = opts.release.encode("ascii")
    except UnicodeEncodeError:
        parser.error("release ID must be ASCII")
    if not 1 <= len(release) <= 16:
        parser.error("release ID must be 1..16 ASCII bytes")
    if (payload[pos+CRC_START:pos+CRC_END] != b"00000000" or
        payload[pos+CRC_END:pos+VERSION_END] != UNSTAMPED):
        parser.error("already stamped or unexpected CRC/version placeholder")
    payload[pos+CRC_END:pos+VERSION_END] = release.ljust(16, b" ")
    payload[pos+CRC_START:pos+CRC_END] = b"00000000"
    payload[pos+CRC_START:pos+CRC_END] = f"{checksum(payload, pos):08X}".encode("ascii")
    opts.executable.write_bytes(payload)
    print(f"Stamped embedded CRC32 for release {opts.release}")

if __name__ == "__main__":
    main()
