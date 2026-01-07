#!/usr/bin/env python3
"""Analyze all chunks in other format files"""
import struct
import os

def analyze_all_chunks(path):
    """Analyze all chunks in the file"""
    print(f"\n{'='*70}")
    print(f"Analyzing: {os.path.basename(path)}")
    print(f"{'='*70}")

    with open(path, 'rb') as f:
        data = f.read()

    print(f"File size: {len(data)} bytes")

    if data[0:4] != b'ffna':
        print("Not an FFNA file")
        return

    print(f"FFNA type: {data[4]}")

    # Parse all chunks
    offset = 5
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]
        chunk_start = offset + 8
        chunk_end = chunk_start + chunk_size

        print(f"\n--- Chunk 0x{chunk_id:X} at 0x{offset:X}, size={chunk_size} ---")

        # Dump first 64 bytes of chunk data
        print(f"First 64 bytes:")
        for i in range(0, min(64, chunk_size), 16):
            hex_str = ' '.join(f'{data[chunk_start + i + j]:02X}'
                               for j in range(min(16, chunk_size - i)))
            print(f"  +0x{i:02X}: {hex_str}")

        # For 0xFA3 (submesh geometry chunk?), do deeper analysis
        if chunk_id == 0xFA3:
            analyze_fa3_chunk(data, chunk_start, chunk_size)

        # For 0xFAA, analyze
        if chunk_id == 0xFAA:
            analyze_faa_chunk(data, chunk_start, chunk_size)

        offset += 8 + chunk_size

def analyze_fa3_chunk(data, chunk_start, chunk_size):
    """Analyze the 0xFA3 chunk which might contain submesh geometry"""
    print(f"\n  ** 0xFA3 Analysis **")

    # Try to find submesh patterns
    print(f"  Scanning for submesh patterns...")

    found = []
    end = chunk_start + chunk_size
    for offset in range(chunk_start, end - 32, 4):
        # Try counts at offset +0
        num_indices = struct.unpack_from('<I', data, offset)[0]
        num_vertices = struct.unpack_from('<I', data, offset + 4)[0]

        if 10 <= num_indices <= 10000 and 10 <= num_vertices <= 5000:
            if num_indices % 3 == 0:
                # Check if indices follow after various header sizes
                for header_size in [8, 16, 20, 24, 32, 40]:
                    idx_offset = offset + header_size
                    if idx_offset + num_indices * 2 <= len(data):
                        indices = [struct.unpack_from('<H', data, idx_offset + i*2)[0]
                                   for i in range(min(5, num_indices))]
                        max_idx = max(indices) if indices else 0

                        if 0 < max_idx < num_vertices:
                            found.append((offset, header_size, num_indices, num_vertices, indices, max_idx))

    for offset, header_size, num_indices, num_vertices, indices, max_idx in found:
        print(f"\n  FOUND at 0x{offset:X} (header={header_size}): indices={num_indices}, vertices={num_vertices}")
        print(f"    First indices: {indices}, max={max_idx}")
        rel = offset - chunk_start
        print(f"    Relative offset: +0x{rel:X}")
        print(f"    Header: {' '.join(f'{data[offset+i]:02X}' for i in range(header_size))}")

def analyze_faa_chunk(data, chunk_start, chunk_size):
    """Analyze the 0xFAA chunk"""
    print(f"\n  ** 0xFAA Analysis **")
    # Just dump structure
    if chunk_size >= 4:
        first_dword = struct.unpack_from('<I', data, chunk_start)[0]
        print(f"  First DWORD: {first_dword}")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    # All files
    analyze_all_chunks(os.path.join(sample_dir, "0x21448_other.gwraw"))
    analyze_all_chunks(os.path.join(sample_dir, "0x26B44_other.gwraw"))
    analyze_all_chunks(os.path.join(sample_dir, "0x54992_other.gwraw"))

if __name__ == "__main__":
    main()
