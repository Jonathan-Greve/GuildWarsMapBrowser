#!/usr/bin/env python3
"""Compare UV data between other and normal format model files in detail"""
import struct
import os

def parse_other_format(path):
    """Parse UV from other format file"""
    print(f"\n{'='*70}")
    print(f"OTHER format: {os.path.basename(path)}")
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
            break
        offset += 8 + chunk_size
    else:
        print("No 0xBB8 chunk found")
        return [], []

    # Parse header
    bVar5 = data[chunk_start + 0x18]
    bVar4 = data[chunk_start + 0x1C]
    field_0x20 = struct.unpack_from('<I', data, chunk_start + 0x20)[0]

    # Navigate to submesh
    curr = chunk_start + 0x30
    if bVar5 != 0xFF and bVar5 != 0:
        if field_0x20 != 0:
            section_size = bVar4 + bVar5 * 8 + bVar4 * 9
        else:
            section_size = bVar5 * 8 + bVar4 * 9
        curr += section_size

    # Skip submesh count
    curr += 4
    # Skip 8-zero prefix
    if data[curr:curr+8] == b'\x00' * 8:
        curr += 8

    # Read header
    num_indices = struct.unpack_from('<I', data, curr)[0]
    num_vertices = struct.unpack_from('<I', data, curr + 4)[0]
    curr += 24

    # Skip indices
    curr += num_indices * 2
    pos_start = curr
    pos_size = num_vertices * 12

    print(f"  num_indices={num_indices}, num_vertices={num_vertices}")

    # Read positions
    positions = []
    for i in range(num_vertices):
        x, y, z = struct.unpack_from('<fff', data, pos_start + i * 12)
        positions.append((x, y, z))

    # UV right after positions
    uv_start = pos_start + pos_size
    uvs = []
    for i in range(num_vertices):
        uv_off = uv_start + i * 4
        u_raw = struct.unpack_from('<H', data, uv_off)[0]
        v_raw = struct.unpack_from('<H', data, uv_off + 2)[0]
        u = u_raw / 65536.0
        v = v_raw / 65536.0
        uvs.append((u, v, u_raw, v_raw))

    print(f"  First 5 vertices:")
    for i in range(min(5, num_vertices)):
        print(f"    V[{i}]: pos=({positions[i][0]:.2f}, {positions[i][1]:.2f}, {positions[i][2]:.2f}) UV=({uvs[i][0]:.4f}, {uvs[i][1]:.4f}) raw=({uvs[i][2]}, {uvs[i][3]})")

    return positions, uvs

def parse_normal_format(path):
    """Parse UV from normal format file"""
    print(f"\n{'='*70}")
    print(f"NORMAL format: {os.path.basename(path)}")
    print(f"{'='*70}")

    with open(path, 'rb') as f:
        data = f.read()

    # Find geometry chunk (0xFA0 or similar)
    offset = 5
    geometry_chunk_start = None
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]
        print(f"  Chunk 0x{chunk_id:X} at 0x{offset:X}, size={chunk_size}")
        if chunk_id == 0xFA0:
            geometry_chunk_start = offset + 8
        offset += 8 + chunk_size

    if geometry_chunk_start is None:
        print("  No 0xFA0 geometry chunk found, scanning for submesh pattern...")

    # Scan entire file for submesh pattern
    positions = []
    uvs = []

    for scan in range(0, len(data) - 32, 2):
        # Look for submesh header pattern: num_indices, num_vertices, then other fields
        ni = struct.unpack_from('<I', data, scan)[0]
        nv = struct.unpack_from('<I', data, scan + 4)[0]

        # Look for 66 indices, 26 vertices (matching 0x21448)
        if ni == 66 and nv == 26:
            print(f"\n  Found potential submesh at 0x{scan:X}: indices={ni}, vertices={nv}")

            # Read header fields
            for h in range(6):
                val = struct.unpack_from('<I', data, scan + h * 4)[0]
                print(f"    Header[{h}]: {val} (0x{val:X})")

            # Try different vertex sizes
            for vertex_size in [20, 24, 28, 32]:
                idx_off = scan + 24  # After 6 DWORD header
                vtx_off = idx_off + ni * 2

                # Verify indices are valid
                valid = True
                for j in range(min(5, ni)):
                    idx = struct.unpack_from('<H', data, idx_off + j * 2)[0]
                    if idx >= nv:
                        valid = False
                        break

                if not valid:
                    continue

                print(f"\n    Trying vertex_size={vertex_size}, vtx_off=0x{vtx_off:X}")

                # Read first few vertices
                temp_pos = []
                temp_uvs = []
                for i in range(min(5, nv)):
                    voff = vtx_off + i * vertex_size
                    if voff + vertex_size > len(data):
                        break

                    x, y, z = struct.unpack_from('<fff', data, voff)
                    temp_pos.append((x, y, z))

                    # Try UV at different offsets within vertex
                    for uv_off_in_vtx in [12, 16, 20, 24]:
                        if uv_off_in_vtx + 8 <= vertex_size:
                            u, v = struct.unpack_from('<ff', data, voff + uv_off_in_vtx)
                            if -10 < u < 10 and -10 < v < 10:  # Reasonable UV range
                                print(f"      V[{i}]: pos=({x:.2f},{y:.2f},{z:.2f}) UV@{uv_off_in_vtx}=({u:.4f},{v:.4f})")

                if len(temp_pos) >= 3:
                    # Check if positions look similar to other format
                    print(f"\n    Positions found:")
                    for i, (x, y, z) in enumerate(temp_pos):
                        print(f"      P[{i}]: ({x:.2f}, {y:.2f}, {z:.2f})")

    return positions, uvs

def main():
    other_dir = r"C:\Users\Jonathan\Downloads\other model files"

    # Parse both formats
    other_pos, other_uvs = parse_other_format(os.path.join(other_dir, "0x21448_other.gwraw"))
    normal_pos, normal_uvs = parse_normal_format(os.path.join(other_dir, "0x21448_normal.gwraw"))

if __name__ == "__main__":
    main()
