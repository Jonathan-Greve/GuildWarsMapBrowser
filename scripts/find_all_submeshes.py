#!/usr/bin/env python3
"""Find all submesh headers in a file"""
import struct
import os

def find_submeshes(path):
    """Find all valid submesh headers"""
    print(f"\n{'='*70}")
    print(f"Scanning: {os.path.basename(path)}")
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
        print("No 0xBB8 chunk")
        return

    print(f"0xBB8 chunk: 0x{chunk_start:X} to 0x{chunk_end:X}")

    # Scan for header pattern: 00 00 00 00 00 00 00 00 XX XX 00 00 YY YY 00 00 01 00 00 00 01 00 00 00
    print(f"\nSearching for submesh header pattern...")
    found = []

    for off in range(chunk_start, chunk_end - 32, 1):  # Step by 1 to not miss any
        # Check for 8 zero bytes
        if data[off:off+8] == b'\x00\x00\x00\x00\x00\x00\x00\x00':
            num_indices = struct.unpack_from('<I', data, off + 8)[0]
            num_vertices = struct.unpack_from('<I', data, off + 12)[0]
            f5 = struct.unpack_from('<I', data, off + 16)[0]
            f6 = struct.unpack_from('<I', data, off + 20)[0]
            f7 = struct.unpack_from('<I', data, off + 24)[0]
            f8 = struct.unpack_from('<I', data, off + 28)[0]

            # Check if it looks like a valid header
            if 10 <= num_indices <= 10000 and 10 <= num_vertices <= 5000:
                if f5 == 1 and f6 == 1 and f7 == 1 and f8 == 0:
                    # Verify indices
                    idx_start = off + 32
                    if idx_start + num_indices * 2 <= len(data):
                        indices = [struct.unpack_from('<H', data, idx_start + i*2)[0]
                                   for i in range(min(10, num_indices))]
                        max_idx = max(indices) if indices else 0

                        if max_idx < num_vertices:
                            found.append({
                                'offset': off,
                                'num_indices': num_indices,
                                'num_vertices': num_vertices,
                                'first_indices': indices,
                                'max_idx': max_idx
                            })

    print(f"Found {len(found)} submesh headers:")
    for i, sub in enumerate(found):
        print(f"\n  Submesh {i} at 0x{sub['offset']:X}:")
        print(f"    num_indices: {sub['num_indices']} ({sub['num_indices']//3} triangles)")
        print(f"    num_vertices: {sub['num_vertices']}")
        print(f"    first indices: {sub['first_indices']}, max={sub['max_idx']}")

        # Calculate sizes
        indices_size = sub['num_indices'] * 2
        idx_start = sub['offset'] + 32
        idx_end = idx_start + indices_size
        print(f"    indices: 0x{idx_start:X} - 0x{idx_end:X}")

        # Find next submesh to determine vertex data size
        if i + 1 < len(found):
            next_offset = found[i + 1]['offset']
            vertex_data_size = next_offset - idx_end
            vertex_size = vertex_data_size // sub['num_vertices']
            print(f"    vertex data: 0x{idx_end:X} - 0x{next_offset:X} = {vertex_data_size} bytes")
            print(f"    vertex size: {vertex_data_size} / {sub['num_vertices']} = {vertex_size} bytes/vertex")
        else:
            # Last submesh - estimate to end of chunk
            vertex_data_size = chunk_end - idx_end
            vertex_size = vertex_data_size // sub['num_vertices'] if sub['num_vertices'] > 0 else 0
            print(f"    vertex data: 0x{idx_end:X} - 0x{chunk_end:X} = {vertex_data_size} bytes (to chunk end)")
            print(f"    vertex size estimate: {vertex_data_size} / {sub['num_vertices']} = {vertex_size} bytes/vertex")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    find_submeshes(os.path.join(sample_dir, "0x21448_other.gwraw"))
    find_submeshes(os.path.join(sample_dir, "0x26B44_other.gwraw"))
    find_submeshes(os.path.join(sample_dir, "0x54992_other.gwraw"))

if __name__ == "__main__":
    main()
