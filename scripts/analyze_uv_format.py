#!/usr/bin/env python3
"""Analyze UV format in OLD format model files"""
import struct
import os

def analyze_file(path):
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

    print(f"0xBB8 chunk: 0x{chunk_start:X} to 0x{chunk_end:X}")

    # Parse header
    num_texture_groups = data[chunk_start + 0x19]
    bVar5 = data[chunk_start + 0x18]  # num_bone_groups
    bVar4 = data[chunk_start + 0x1C]  # num_bone_weights
    field_0x20 = struct.unpack_from('<I', data, chunk_start + 0x20)[0]

    is_modern = num_texture_groups != 0xFF and num_texture_groups > 0

    print(f"Format: {'MODERN' if is_modern else 'OLD'}")
    print(f"bVar5={bVar5}, bVar4={bVar4}, field_0x20={field_0x20}")

    # Navigate to submesh section
    curr = chunk_start + 0x30

    # OLD format: skip bVar5 * 8 + bVar4 * 9 section
    if not is_modern and bVar5 != 0xFF and bVar5 != 0:
        if field_0x20 != 0:
            section_size = bVar4 + bVar5 * 8 + bVar4 * 9
        else:
            section_size = bVar5 * 8 + bVar4 * 9
        print(f"Skipping OLD section: {section_size} bytes")
        curr += section_size

    # Read submesh count
    num_submeshes = struct.unpack_from('<I', data, curr)[0]
    print(f"\nSubmesh count: {num_submeshes}")
    curr += 4

    # Skip 8-zero prefix
    if data[curr:curr+8] == b'\x00' * 8:
        print("Skipping 8-zero prefix")
        curr += 8

    # Read submesh header
    num_indices = struct.unpack_from('<I', data, curr)[0]
    num_vertices = struct.unpack_from('<I', data, curr + 4)[0]
    num_uv_sets = struct.unpack_from('<I', data, curr + 8)[0]
    num_groups = struct.unpack_from('<I', data, curr + 12)[0]
    num_colors = struct.unpack_from('<I', data, curr + 16)[0]
    num_normals = struct.unpack_from('<I', data, curr + 20)[0]

    print(f"\nSubmesh header at 0x{curr:X}:")
    print(f"  num_indices={num_indices}, num_vertices={num_vertices}")
    print(f"  uv_sets={num_uv_sets}, groups={num_groups}, colors={num_colors}, normals={num_normals}")
    curr += 24

    # Skip indices
    idx_start = curr
    print(f"\nIndices at 0x{idx_start:X}")
    curr += num_indices * 2

    # Position data
    pos_start = curr
    pos_size = num_vertices * 12
    print(f"Positions at 0x{pos_start:X} (size={pos_size})")

    # Show first few positions
    for i in range(min(3, num_vertices)):
        x, y, z = struct.unpack_from('<fff', data, pos_start + i * 12)
        print(f"  Pos[{i}]: ({x:.2f}, {y:.2f}, {z:.2f})")

    pos_end = pos_start + pos_size

    # Now look at what comes after positions
    print(f"\n--- Data after positions (0x{pos_end:X}) ---")

    # Dump raw bytes
    remaining = min(256, chunk_end - pos_end)
    for row in range(0, remaining, 16):
        addr = pos_end + row
        hex_str = ' '.join(f'{data[addr + j]:02X}' for j in range(min(16, remaining - row)))
        print(f"  0x{addr:04X}: {hex_str}")

    # Try different UV interpretations
    print(f"\n--- UV interpretations ---")

    # Interpretation 1: uint16 pairs right after positions
    print(f"\n1) uint16 pairs at 0x{pos_end:X}:")
    for i in range(min(5, num_vertices)):
        uv_off = pos_end + i * 4
        u_raw = struct.unpack_from('<H', data, uv_off)[0]
        v_raw = struct.unpack_from('<H', data, uv_off + 2)[0]
        u = u_raw / 65536.0
        v = v_raw / 65536.0
        print(f"  UV[{i}]: raw=({u_raw}, {v_raw}) -> ({u:.4f}, {v:.4f})")

    # Interpretation 2: Skip 4 bytes per vertex of "extra" data, then UV
    extra_size = num_vertices * 4
    uv_start = pos_end + extra_size
    print(f"\n2) uint16 pairs at 0x{uv_start:X} (after {extra_size} bytes extra):")
    for i in range(min(5, num_vertices)):
        uv_off = uv_start + i * 4
        u_raw = struct.unpack_from('<H', data, uv_off)[0]
        v_raw = struct.unpack_from('<H', data, uv_off + 2)[0]
        u = u_raw / 65536.0
        v = v_raw / 65536.0
        print(f"  UV[{i}]: raw=({u_raw}, {v_raw}) -> ({u:.4f}, {v:.4f})")

    # Interpretation 3: Float UVs right after positions
    print(f"\n3) Float UVs at 0x{pos_end:X}:")
    for i in range(min(5, num_vertices)):
        uv_off = pos_end + i * 8
        if uv_off + 8 <= len(data):
            u, v = struct.unpack_from('<ff', data, uv_off)
            print(f"  UV[{i}]: ({u:.4f}, {v:.4f})")

    # Interpretation 4: Look for UV header pattern
    print(f"\n4) Looking for UV section with header...")
    # GW UV sections sometimes have a header with counts
    for scan_off in range(pos_end, min(pos_end + 200, chunk_end - 8), 4):
        val0 = struct.unpack_from('<I', data, scan_off)[0]
        val1 = struct.unpack_from('<I', data, scan_off + 4)[0]
        # Check if this could be UV header (small counts)
        if val0 == num_vertices or val1 == num_vertices:
            print(f"  Potential header at 0x{scan_off:X}: ({val0}, {val1})")
            # Try UV after this
            test_uv = scan_off + 8
            if test_uv + 8 <= len(data):
                u_raw = struct.unpack_from('<H', data, test_uv)[0]
                v_raw = struct.unpack_from('<H', data, test_uv + 2)[0]
                print(f"    First UV after: raw=({u_raw}, {v_raw}) -> ({u_raw/65536:.4f}, {v_raw/65536:.4f})")

    # Interpretation 5: Look at the actual vertex data as 20-byte chunks
    print(f"\n5) 20-byte vertices starting at 0x{pos_start:X}:")
    for i in range(min(3, num_vertices)):
        voff = pos_start + i * 20
        if voff + 20 <= len(data):
            x, y, z = struct.unpack_from('<fff', data, voff)
            u, v = struct.unpack_from('<ff', data, voff + 12)
            print(f"  Vtx[{i}]: pos=({x:.2f}, {y:.2f}, {z:.2f}), uv=({u:.4f}, {v:.4f})")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"
    analyze_file(os.path.join(sample_dir, "0x21448_other.gwraw"))

if __name__ == "__main__":
    main()
