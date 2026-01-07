#!/usr/bin/env python3
"""Compare UV data between other and normal format model files"""
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
        return []

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
    print(f"  pos_start=0x{pos_start:X}")

    # Read positions and UVs
    uvs = []

    # Method 1: UV right after positions (no extra data gap)
    uv_start = pos_start + pos_size
    print(f"\n  UV method 1 (right after pos, at 0x{uv_start:X}):")
    for i in range(min(10, num_vertices)):
        uv_off = uv_start + i * 4
        u_raw = struct.unpack_from('<H', data, uv_off)[0]
        v_raw = struct.unpack_from('<H', data, uv_off + 2)[0]
        u = u_raw / 65536.0
        v = v_raw / 65536.0
        uvs.append((u, v))
        if i < 5:
            print(f"    UV[{i}]: ({u:.4f}, {v:.4f})")

    return uvs

def parse_normal_format(path):
    """Parse UV from normal format file using ImHex pattern knowledge"""
    print(f"\n{'='*70}")
    print(f"NORMAL format: {os.path.basename(path)}")
    print(f"{'='*70}")

    with open(path, 'rb') as f:
        data = f.read()

    # Find 0xFA0 chunk (geometry chunk in normal format)
    offset = 5
    chunk_start = None
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]
        if chunk_id == 0xFA0:
            chunk_start = offset + 8
            break
        offset += 8 + chunk_size

    if chunk_start is None:
        print("No 0xFA0 chunk found")
        return []

    print(f"  0xFA0 chunk at 0x{chunk_start:X}")

    # Normal format has different structure - need to find submesh data
    # Look for submesh pattern: reasonable indices/vertices counts

    uvs = []

    # Scan for submesh header pattern
    for scan in range(chunk_start, min(chunk_start + 500, len(data) - 24), 2):
        ni = struct.unpack_from('<I', data, scan)[0]
        nv = struct.unpack_from('<I', data, scan + 4)[0]

        # Check for reasonable values
        if 10 <= ni <= 1000 and 10 <= nv <= 500 and ni % 3 == 0:
            # Verify indices are valid
            idx_off = scan + 24  # After 6-DWORD header
            if idx_off + ni * 2 > len(data):
                continue

            valid = True
            for j in range(min(5, ni)):
                idx = struct.unpack_from('<H', data, idx_off + j * 2)[0]
                if idx >= nv:
                    valid = False
                    break

            if valid:
                print(f"  Found submesh at 0x{scan:X}: indices={ni}, vertices={nv}")

                # Read 6 DWORD header
                f3 = struct.unpack_from('<I', data, scan + 8)[0]
                f4 = struct.unpack_from('<I', data, scan + 12)[0]
                fvf = struct.unpack_from('<I', data, scan + 16)[0]
                f6 = struct.unpack_from('<I', data, scan + 20)[0]
                print(f"    f3={f3}, f4={f4}, fvf=0x{fvf:X}, f6={f6}")

                # Calculate vertex size from FVF
                # FVF 0x112 = Position (12) + Normal (12) + UV (8) = 32 bytes
                # FVF 0x102 = Position (12) + UV (8) = 20 bytes
                if fvf == 0x112:
                    vertex_size = 32
                    uv_offset_in_vtx = 24  # After pos + normal
                elif fvf == 0x102:
                    vertex_size = 20
                    uv_offset_in_vtx = 12  # After pos
                else:
                    # Try to calculate from FVF bits
                    vertex_size = 20
                    uv_offset_in_vtx = 12

                print(f"    vertex_size={vertex_size}, uv_offset={uv_offset_in_vtx}")

                vtx_start = idx_off + ni * 2
                print(f"    vertices at 0x{vtx_start:X}")

                # Read UVs from vertex data
                for i in range(min(10, nv)):
                    voff = vtx_start + i * vertex_size + uv_offset_in_vtx
                    if voff + 8 > len(data):
                        break
                    u, v = struct.unpack_from('<ff', data, voff)
                    uvs.append((u, v))
                    if i < 5:
                        print(f"    UV[{i}]: ({u:.4f}, {v:.4f})")

                break

    return uvs

def main():
    other_dir = r"C:\Users\Jonathan\Downloads\other model files"

    # Compare 0x21448
    other_uvs = parse_other_format(os.path.join(other_dir, "0x21448_other.gwraw"))
    normal_uvs = parse_normal_format(os.path.join(other_dir, "0x21448_normal.gwraw"))

    if other_uvs and normal_uvs:
        print(f"\n{'='*70}")
        print("UV COMPARISON for 0x21448:")
        print(f"{'='*70}")
        for i in range(min(5, len(other_uvs), len(normal_uvs))):
            ou, ov = other_uvs[i]
            nu, nv = normal_uvs[i]
            print(f"  Vertex {i}: OTHER=({ou:.4f},{ov:.4f}) NORMAL=({nu:.4f},{nv:.4f})")

    # Compare 0x26B44
    print("\n\n")
    other_uvs = parse_other_format(os.path.join(other_dir, "0x26B44_other.gwraw"))
    normal_uvs = parse_normal_format(os.path.join(other_dir, "0x26B44_normal.gwraw"))

    if other_uvs and normal_uvs:
        print(f"\n{'='*70}")
        print("UV COMPARISON for 0x26B44:")
        print(f"{'='*70}")
        for i in range(min(5, len(other_uvs), len(normal_uvs))):
            ou, ov = other_uvs[i]
            nu, nv = normal_uvs[i]
            print(f"  Vertex {i}: OTHER=({ou:.4f},{ov:.4f}) NORMAL=({nu:.4f},{nv:.4f})")

if __name__ == "__main__":
    main()
