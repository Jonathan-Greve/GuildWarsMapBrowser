#!/usr/bin/env python3
"""Find submesh data by looking for reasonable index/vertex counts"""
import struct
import os

def find_submesh_data(path):
    """Scan for submesh-like structures"""
    print(f"\n{'='*70}")
    print(f"Analyzing: {os.path.basename(path)}")
    print(f"{'='*70}")

    with open(path, 'rb') as f:
        data = f.read()

    # Find 0xBB8 chunk
    offset = 5
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]
        if chunk_id == 0xBB8:
            chunk_start = offset + 8
            chunk_end = chunk_start + chunk_size
            break
        offset += 8 + chunk_size
    else:
        print("No 0xBB8 chunk found")
        return

    print(f"0xBB8 chunk: 0x{chunk_start:X} to 0x{chunk_end:X} (size={chunk_size})")

    # Dump full chunk for inspection
    print(f"\nFull 0xBB8 chunk dump:")
    for i in range(0, chunk_size, 16):
        abs_offset = chunk_start + i
        hex_str = ' '.join(f'{data[abs_offset + j]:02X}' for j in range(min(16, chunk_size - i)))
        ascii_str = ''.join(chr(data[abs_offset + j]) if 32 <= data[abs_offset + j] < 127 else '.'
                           for j in range(min(16, chunk_size - i)))
        print(f"  0x{abs_offset:04X} (+0x{i:03X}): {hex_str:<48} {ascii_str}")

    # Search for submesh headers
    # Looking for patterns like: num_indices (reasonable), num_vertices (reasonable), ...
    # A reasonable model might have 10-10000 indices and 10-5000 vertices
    print(f"\n--- Searching for potential submesh headers ---")
    print(f"Looking for: num_indices (10-50000), num_vertices (10-20000), followed by small values")

    raw_start = chunk_start + 0x30  # After header
    for i in range(raw_start, chunk_end - 24, 4):
        # Check if this looks like a submesh count followed by submesh header
        # First try: [count][submesh_header...]
        count = struct.unpack_from('<I', data, i)[0]
        if 1 <= count <= 10:  # Reasonable submesh count
            # Check if next values look like a submesh header
            num_indices = struct.unpack_from('<I', data, i + 4)[0]
            num_vertices = struct.unpack_from('<I', data, i + 8)[0]

            if 3 <= num_indices <= 50000 and 3 <= num_vertices <= 20000:
                # Check if num_indices is divisible by 3 (triangles)
                if num_indices % 3 == 0:
                    field3 = struct.unpack_from('<I', data, i + 12)[0]
                    field4 = struct.unpack_from('<I', data, i + 16)[0]
                    field5 = struct.unpack_from('<I', data, i + 20)[0]
                    field6 = struct.unpack_from('<I', data, i + 24)[0]

                    print(f"\n  Potential at 0x{i:X} (+0x{i - chunk_start:X}):")
                    print(f"    Count(?): {count}")
                    print(f"    num_indices: {num_indices}")
                    print(f"    num_vertices: {num_vertices}")
                    print(f"    field3: {field3}")
                    print(f"    field4: {field4}")
                    print(f"    field5: {field5}")
                    print(f"    field6: {field6}")

                    # Verify by checking if indices follow
                    idx_start = i + 28  # After count + 24-byte header
                    if idx_start + num_indices * 2 <= len(data):
                        # Sample some indices
                        indices = [struct.unpack_from('<H', data, idx_start + j*2)[0]
                                   for j in range(min(5, num_indices))]
                        max_idx = max(indices) if indices else 0
                        print(f"    First indices: {indices}, max={max_idx}")
                        if max_idx < num_vertices:
                            print(f"    *** LIKELY VALID! Indices reference valid vertex range ***")

        # Also try: [submesh_header...] directly (count might be earlier)
        num_indices = struct.unpack_from('<I', data, i)[0]
        num_vertices = struct.unpack_from('<I', data, i + 4)[0]

        if 3 <= num_indices <= 50000 and 3 <= num_vertices <= 20000 and num_indices % 3 == 0:
            field3 = struct.unpack_from('<I', data, i + 8)[0]
            field4 = struct.unpack_from('<I', data, i + 12)[0]

            # Check if indices follow immediately after 24-byte header
            idx_start = i + 24
            if idx_start + num_indices * 2 <= len(data):
                indices = [struct.unpack_from('<H', data, idx_start + j*2)[0]
                           for j in range(min(5, num_indices))]
                max_idx = max(indices) if indices else 0

                if max_idx < num_vertices and max_idx > 0:
                    print(f"\n  Direct submesh at 0x{i:X} (+0x{i - chunk_start:X}):")
                    print(f"    num_indices: {num_indices}")
                    print(f"    num_vertices: {num_vertices}")
                    print(f"    field3: {field3}")
                    print(f"    field4: {field4}")
                    print(f"    First indices: {indices}, max={max_idx}")
                    print(f"    *** LIKELY VALID! ***")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    # Problematic files
    find_submesh_data(os.path.join(sample_dir, "0x21448_other.gwraw"))
    find_submesh_data(os.path.join(sample_dir, "0x26B44_other.gwraw"))
    # Working file for comparison
    find_submesh_data(os.path.join(sample_dir, "0x54992_other.gwraw"))

if __name__ == "__main__":
    main()
