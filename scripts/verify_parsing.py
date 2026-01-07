#!/usr/bin/env python3
"""Verify correct parsing of all three model files"""
import struct
import os

def parse_file(path):
    print(f"\n{'='*70}")
    print(f"Parsing: {os.path.basename(path)}")
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

    # Parse header
    class_flags = struct.unpack_from('<I', data, chunk_start + 0x08)[0]
    bVar5 = data[chunk_start + 0x18]
    num_texture_groups = data[chunk_start + 0x19]
    num_textures = struct.unpack_from('<H', data, chunk_start + 0x1A)[0]
    bVar4 = data[chunk_start + 0x1C]
    num_bone_weight_sets = data[chunk_start + 0x1D]
    num_materials = struct.unpack_from('<H', data, chunk_start + 0x1E)[0]
    field_0x20 = struct.unpack_from('<I', data, chunk_start + 0x20)[0]

    is_modern = num_texture_groups != 0xFF and num_texture_groups > 0 and num_textures < 0x100
    format_type = "MODERN" if is_modern else "OLD"
    print(f"Format: {format_type}")
    print(f"  num_texture_groups: {num_texture_groups}")
    print(f"  bVar5: {bVar5}")

    # Navigate to submesh data
    curr = chunk_start + 0x30

    # OLD format: skip bVar5 * 8 + bVar4 * 9 section
    if not is_modern and bVar5 != 0xFF and bVar5 != 0:
        if field_0x20 != 0:
            section_size = bVar4 + bVar5 * 8 + bVar4 * 9
        else:
            section_size = bVar5 * 8 + bVar4 * 9
        curr += section_size

    # MODERN format: skip texture groups + bone index + materials
    if is_modern and num_texture_groups > 0:
        curr += num_texture_groups * 9
        bone_mult = (1 if field_0x20 != 0 else 0) + 3
        curr += bone_mult * num_bone_weight_sets
        curr += num_textures * 8  # texture refs
        # texture names (null-terminated)
        for i in range(num_textures):
            while curr < len(data) and data[curr] != 0:
                curr += 1
            curr += 1
        curr += num_materials * 8

    # Submesh count
    if class_flags & 0x008:
        num_submeshes = struct.unpack_from('<I', data, curr)[0]
        print(f"\nSubmesh count: {num_submeshes}")
        curr += 4

        total_indices = 0
        total_vertices = 0
        all_valid = True

        for sub_idx in range(num_submeshes):
            print(f"\n--- Submesh {sub_idx} ---")

            # First submesh has 8-zero prefix, others don't
            if sub_idx == 0:
                if data[curr:curr+8] == b'\x00\x00\x00\x00\x00\x00\x00\x00':
                    print(f"  [Skipping 8-zero prefix]")
                    curr += 8
                else:
                    print(f"  WARNING: Expected 8-zero prefix at 0x{curr:X}")

            # Read header
            num_indices = struct.unpack_from('<I', data, curr)[0]
            num_vertices = struct.unpack_from('<I', data, curr + 4)[0]
            f5 = struct.unpack_from('<I', data, curr + 8)[0]
            f6 = struct.unpack_from('<I', data, curr + 12)[0]
            f7 = struct.unpack_from('<I', data, curr + 16)[0]
            f8 = struct.unpack_from('<I', data, curr + 20)[0]

            print(f"  Header at 0x{curr:X}: indices={num_indices}, vertices={num_vertices}")
            print(f"    f5={f5}, f6={f6}, f7={f7}, f8={f8}")
            curr += 24

            # Indices
            indices_start = curr
            indices = [struct.unpack_from('<H', data, curr + i*2)[0]
                       for i in range(min(10, num_indices))]
            max_idx = max(indices) if indices else 0
            print(f"  Indices at 0x{indices_start:X}: first={indices[:5]}, max={max_idx}")

            if max_idx >= num_vertices:
                print(f"  *** ERROR: max_idx {max_idx} >= num_vertices {num_vertices}")
                all_valid = False

            curr += num_indices * 2

            # Vertices (20 bytes each = FVF 0x101)
            vertex_start = curr
            if vertex_start + 20 <= len(data):
                x, y, z = struct.unpack_from('<fff', data, curr)
                u, v = struct.unpack_from('<ff', data, curr + 12)
                print(f"  Vertices at 0x{vertex_start:X}: first=({x:.2f}, {y:.2f}, {z:.2f}), uv=({u:.4f}, {v:.4f})")

            curr += num_vertices * 20

            total_indices += num_indices
            total_vertices += num_vertices

            print(f"  After vertices: 0x{curr:X}")

            # Extra data for MODERN format between submeshes
            if is_modern and sub_idx < num_submeshes - 1:
                # Calculate extra data size
                # Based on analysis: seems to be related to indices
                # Let's find the next submesh header
                next_submesh_found = False
                for scan_off in range(curr, min(curr + 1024, chunk_end - 24), 4):
                    ni = struct.unpack_from('<I', data, scan_off)[0]
                    nv = struct.unpack_from('<I', data, scan_off + 4)[0]
                    if 10 <= ni <= 50000 and 10 <= nv <= 20000 and ni % 3 == 0:
                        # Check if indices look valid
                        test_idx_start = scan_off + 24
                        if test_idx_start + 10 <= len(data):
                            test_indices = [struct.unpack_from('<H', data, test_idx_start + j*2)[0]
                                            for j in range(min(5, ni))]
                            test_max = max(test_indices) if test_indices else 0
                            if test_max < nv:
                                extra_size = scan_off - curr
                                print(f"  Extra data: {extra_size} bytes (0x{curr:X} to 0x{scan_off:X})")
                                curr = scan_off
                                next_submesh_found = True
                                break

                if not next_submesh_found:
                    print(f"  WARNING: Could not find next submesh header")

        print(f"\n--- Summary ---")
        print(f"Total indices: {total_indices}")
        print(f"Total vertices: {total_vertices}")
        print(f"All valid: {all_valid}")
        print(f"Final position: 0x{curr:X}")
        print(f"Remaining to chunk end: {chunk_end - curr} bytes")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    parse_file(os.path.join(sample_dir, "0x21448_other.gwraw"))
    parse_file(os.path.join(sample_dir, "0x26B44_other.gwraw"))
    parse_file(os.path.join(sample_dir, "0x54992_other.gwraw"))

if __name__ == "__main__":
    main()
