#!/usr/bin/env python3
"""Trace through the 0xBB8 chunk step by step to find all submeshes"""
import struct
import os

def trace_chunk(path):
    """Trace through 0xBB8 chunk following the parsing logic"""
    print(f"\n{'='*70}")
    print(f"Tracing: {os.path.basename(path)}")
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

    # Parse header
    class_flags = struct.unpack_from('<I', data, chunk_start + 0x08)[0]
    bVar5 = data[chunk_start + 0x18]
    num_texture_groups = data[chunk_start + 0x19]
    num_textures = struct.unpack_from('<H', data, chunk_start + 0x1A)[0]
    bVar4 = data[chunk_start + 0x1C]
    num_bone_weight_sets = data[chunk_start + 0x1D]
    num_materials = struct.unpack_from('<H', data, chunk_start + 0x1E)[0]
    field_0x20 = struct.unpack_from('<I', data, chunk_start + 0x20)[0]

    print(f"\nHeader fields:")
    print(f"  class_flags: 0x{class_flags:04X}")
    print(f"  bVar5 (0x18): {bVar5}")
    print(f"  num_texture_groups (0x19): {num_texture_groups}")
    print(f"  num_textures (0x1A): {num_textures}")
    print(f"  bVar4 (0x1C): {bVar4}")
    print(f"  num_bone_weight_sets (0x1D): {num_bone_weight_sets}")
    print(f"  num_materials (0x1E): {num_materials}")
    print(f"  field_0x20: {field_0x20}")

    # Current position after header
    curr = chunk_start + 0x30
    print(f"\nData starts at: 0x{curr:X}")

    # Skip bone groups if flag 0x002
    if class_flags & 0x002:
        bone_count = struct.unpack_from('<I', data, curr)[0]
        print(f"\nBone groups: count={bone_count}")
        curr += 4 + bone_count * 28
        print(f"  After: 0x{curr:X}")

    # Skip bone weights if flag 0x040
    if class_flags & 0x040:
        print(f"\nBone weights present (flag 0x040) - not implemented")

    # OLD format (bVar5 != 0xFF)
    if bVar5 != 0xFF:
        if bVar5 != 0:
            if field_0x20 != 0:
                section_size = bVar4 + bVar5 * 8 + bVar4 * 9
            else:
                section_size = bVar5 * 8 + bVar4 * 9
            print(f"\nOLD texture section: {section_size} bytes")
            print(f"  Data: {' '.join(f'{data[curr+i]:02X}' for i in range(min(32, section_size)))}")
            curr += section_size
            print(f"  After: 0x{curr:X}")

    # Texture groups (MODERN format)
    if num_texture_groups != 0xFF and num_textures < 0x100 and num_materials < 0x100:
        if num_texture_groups > 0:
            print(f"\nTexture groups: {num_texture_groups}")
            for i in range(num_texture_groups):
                tg = data[curr + i*9:curr + i*9 + 9]
                print(f"  Group[{i}]: {' '.join(f'{b:02X}' for b in tg)}")
            curr += num_texture_groups * 9
            print(f"  After texture groups: 0x{curr:X}")

            # Bone index section
            bone_mult = (1 if field_0x20 != 0 else 0) + 3
            bone_idx_size = bone_mult * num_bone_weight_sets
            if bone_idx_size > 0:
                print(f"\n  Bone index section: {bone_mult} * {num_bone_weight_sets} = {bone_idx_size} bytes")
                print(f"    Data: {' '.join(f'{data[curr+i]:02X}' for i in range(min(16, bone_idx_size)))}")
            curr += bone_idx_size
            print(f"  After bone index: 0x{curr:X}")

            # Texture refs
            tex_ref_size = num_textures * 8
            if tex_ref_size > 0:
                print(f"\n  Texture refs: {num_textures} * 8 = {tex_ref_size} bytes")
                curr += tex_ref_size
                print(f"  After texture refs: 0x{curr:X}")

            # Texture names
            for i in range(num_textures):
                name_start = curr
                while curr < len(data) and data[curr] != 0:
                    curr += 1
                name = data[name_start:curr].decode('ascii', errors='replace')
                if name:
                    print(f"  Texture[{i}]: '{name}'")
                curr += 1

            # Materials
            mat_size = num_materials * 8
            if mat_size > 0:
                print(f"\n  Materials: {num_materials} * 8 = {mat_size} bytes")
                curr += mat_size
                print(f"  After materials: 0x{curr:X}")

    # Vertex buffer (flag 0x020)
    if class_flags & 0x020:
        vb_size = struct.unpack_from('<I', data, curr)[0]
        vb_field2 = struct.unpack_from('<I', data, curr + 4)[0]
        print(f"\nVertex buffer: size={vb_size}, field2={vb_field2}")
        curr += 8 + vb_size
        print(f"  After: 0x{curr:X}")

    # Morph targets (flag 0x080)
    if class_flags & 0x080:
        print(f"\nMorph targets at 0x{curr:X}")
        curr += 12

    # Submesh data (flag 0x008)
    if class_flags & 0x008:
        print(f"\n{'='*50}")
        print(f"SUBMESH DATA at 0x{curr:X}")
        print(f"{'='*50}")

        num_submeshes = struct.unpack_from('<I', data, curr)[0]
        print(f"num_submeshes: {num_submeshes}")
        curr += 4

        # Show bytes before first submesh
        print(f"\nBytes after submesh count (looking for 8-zero pattern):")
        for row in range(0, 48, 16):
            addr = curr + row
            if addr + 16 <= len(data):
                hex_str = ' '.join(f'{data[addr + j]:02X}' for j in range(16))
                print(f"  0x{addr:04X}: {hex_str}")

        # Parse each submesh
        for sub_idx in range(min(num_submeshes, 10)):
            print(f"\n--- Parsing Submesh {sub_idx} ---")
            print(f"Current position: 0x{curr:X}")

            # Check for 8-zero prefix
            if curr + 32 > len(data):
                print("  ERROR: Not enough data")
                break

            prefix = data[curr:curr+8]
            if prefix == b'\x00\x00\x00\x00\x00\x00\x00\x00':
                print(f"  Found 8-zero prefix")
                curr += 8
            else:
                print(f"  WARNING: No 8-zero prefix, bytes: {' '.join(f'{b:02X}' for b in prefix)}")
                # Try to continue anyway

            # Read header
            num_indices = struct.unpack_from('<I', data, curr)[0]
            num_vertices = struct.unpack_from('<I', data, curr + 4)[0]
            f5 = struct.unpack_from('<I', data, curr + 8)[0]
            f6 = struct.unpack_from('<I', data, curr + 12)[0]
            f7 = struct.unpack_from('<I', data, curr + 16)[0]
            f8 = struct.unpack_from('<I', data, curr + 20)[0]

            print(f"  num_indices: {num_indices}")
            print(f"  num_vertices: {num_vertices}")
            print(f"  f5={f5}, f6={f6}, f7={f7}, f8={f8}")

            if num_indices > 100000 or num_vertices > 100000:
                print(f"  ERROR: Invalid counts, trying different offset...")
                # Scan forward for valid pattern
                found = False
                for scan in range(curr, min(curr + 256, chunk_end - 32), 4):
                    if data[scan:scan+8] == b'\x00\x00\x00\x00\x00\x00\x00\x00':
                        ni = struct.unpack_from('<I', data, scan + 8)[0]
                        nv = struct.unpack_from('<I', data, scan + 12)[0]
                        if 10 <= ni <= 50000 and 10 <= nv <= 20000:
                            print(f"  Found valid header at 0x{scan:X}: indices={ni}, vertices={nv}")
                            curr = scan + 8
                            num_indices = ni
                            num_vertices = nv
                            f5 = struct.unpack_from('<I', data, curr + 8)[0]
                            f6 = struct.unpack_from('<I', data, curr + 12)[0]
                            f7 = struct.unpack_from('<I', data, curr + 16)[0]
                            f8 = struct.unpack_from('<I', data, curr + 20)[0]
                            found = True
                            break
                if not found:
                    print(f"  Giving up on submesh {sub_idx}")
                    break

            curr += 24  # Skip header (6 DWORDs)

            # Indices
            idx_size = num_indices * 2
            print(f"  Indices: {num_indices} * 2 = {idx_size} bytes at 0x{curr:X}")

            # Sample indices
            indices = [struct.unpack_from('<H', data, curr + i*2)[0] for i in range(min(5, num_indices))]
            max_idx = max(indices) if indices else 0
            print(f"    First indices: {indices}, max={max_idx}")

            curr += idx_size

            # Vertex data (20 bytes per vertex = FVF 0x101)
            vertex_size = 20
            vertex_data_size = num_vertices * vertex_size
            print(f"  Vertices: {num_vertices} * {vertex_size} = {vertex_data_size} bytes at 0x{curr:X}")

            # Show first vertex
            if curr + 20 <= len(data):
                x, y, z = struct.unpack_from('<fff', data, curr)
                u, v = struct.unpack_from('<ff', data, curr + 12)
                print(f"    First vertex: pos=({x:.2f}, {y:.2f}, {z:.2f}), uv=({u:.4f}, {v:.4f})")

            curr += vertex_data_size
            print(f"  After vertex data: 0x{curr:X}")

            # Check if there's extra data before next submesh
            if sub_idx + 1 < num_submeshes:
                print(f"\n  Looking for next submesh...")
                # Show bytes at current position
                if curr + 32 <= len(data):
                    hex_str = ' '.join(f'{data[curr + j]:02X}' for j in range(32))
                    print(f"    Bytes: {hex_str}")

        print(f"\n--- End of submesh parsing at 0x{curr:X} ---")
        print(f"Remaining bytes to chunk end: {chunk_end - curr}")

        # Show remaining data
        if chunk_end - curr > 0 and chunk_end - curr < 256:
            print(f"\nRemaining data:")
            for row in range(0, chunk_end - curr, 16):
                addr = curr + row
                end = min(addr + 16, chunk_end)
                hex_str = ' '.join(f'{data[addr + j]:02X}' for j in range(end - addr))
                print(f"  0x{addr:04X}: {hex_str}")

        # Search for more 8-zero patterns in remaining data
        if chunk_end - curr > 256:
            print(f"\nSearching for 8-zero patterns in remaining {chunk_end - curr} bytes...")
            found_patterns = []
            for off in range(curr, chunk_end - 32, 1):
                if data[off:off+8] == b'\x00\x00\x00\x00\x00\x00\x00\x00':
                    ni = struct.unpack_from('<I', data, off + 8)[0]
                    nv = struct.unpack_from('<I', data, off + 12)[0]
                    if 3 <= ni <= 50000 and 3 <= nv <= 20000:
                        found_patterns.append((off, ni, nv))

            print(f"Found {len(found_patterns)} potential submesh headers:")
            for off, ni, nv in found_patterns:
                print(f"  At 0x{off:X}: indices={ni}, vertices={nv}")
                # Show header bytes
                header = ' '.join(f'{data[off + j]:02X}' for j in range(32))
                print(f"    Header: {header}")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    trace_chunk(os.path.join(sample_dir, "0x21448_other.gwraw"))
    trace_chunk(os.path.join(sample_dir, "0x26B44_other.gwraw"))
    trace_chunk(os.path.join(sample_dir, "0x54992_other.gwraw"))

if __name__ == "__main__":
    main()
