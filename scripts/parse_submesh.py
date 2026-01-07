#!/usr/bin/env python3
"""Parse submesh data by tracing through Ghidra logic"""
import struct
import os

def parse_bb8_chunk(path):
    """Parse 0xBB8 chunk following Ghidra logic exactly"""
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

    print(f"Chunk at 0x{chunk_start:X}, size={chunk_size}")

    # Header as DWORDs (puVar1[0-11])
    header = [struct.unpack_from('<I', data, chunk_start + i*4)[0] for i in range(12)]

    # Key fields from Ghidra perspective
    class_flags = header[2]  # puVar1[2] at offset 0x08
    bVar5 = data[chunk_start + 0x18]  # byte at 0x18 (OLD format count)
    bVar4 = data[chunk_start + 0x1C]  # byte at 0x1C
    num_texture_groups = data[chunk_start + 0x19]
    num_textures = struct.unpack_from('<H', data, chunk_start + 0x1A)[0]
    num_bone_weight_sets = data[chunk_start + 0x1D]
    num_materials = struct.unpack_from('<H', data, chunk_start + 0x1E)[0]
    field_0x20 = header[8]  # puVar1[8] at offset 0x20

    print(f"\nHeader fields:")
    print(f"  class_flags (0x08): 0x{class_flags:04X}")
    print(f"  bVar5 (0x18): {bVar5}")
    print(f"  num_texture_groups (0x19): {num_texture_groups}")
    print(f"  num_textures (0x1A): {num_textures}")
    print(f"  bVar4 (0x1C): {bVar4}")
    print(f"  num_bone_weight_sets (0x1D): {num_bone_weight_sets}")
    print(f"  num_materials (0x1E): {num_materials}")
    print(f"  field_0x20: {field_0x20}")

    # Current position starts after 48-byte header
    curr = chunk_start + 0x30
    print(f"\nRaw data starts at: 0x{curr:X}")

    # Skip bone groups if flag 0x002 is set
    if class_flags & 0x002:
        bone_count = struct.unpack_from('<I', data, curr)[0]
        print(f"Bone groups: count={bone_count}")
        curr += 4 + bone_count * 28
        print(f"  After bone groups: 0x{curr:X}")

    # Skip bone weights if flag 0x040 is set
    if class_flags & 0x040:
        print(f"Bone weights section present (flag 0x040) - skipping logic not implemented")
        # Complex parsing needed - for now we'll try without it

    # OLD format texture section (bVar5 != 0xFF)
    if bVar5 != 0xFF:
        if bVar5 != 0:
            # Calculate size: -(field_0x20 != 0 ? bVar4 : 0) + bVar5*8 + bVar4*9
            # Actually: ((field_0x20 != 0) ? -1 : 0) & bVar4) + bVar5*8 + bVar4*9
            # = (field_0x20 != 0 ? bVar4 : 0) + bVar5*8 + bVar4*9
            # Wait, -(uint) creates -1 when true which ANDed with bVar4 gives bVar4
            # But then it's ADDED... let me re-check

            # From Ghidra: puVar12 = (-(uint)(puVar1[8] != 0) & bVar4) + bVar5 * 8 + bVar4 * 9 + curr
            # This is calculating end position, not size to skip
            # size = bVar4 (if field_0x20 != 0) + bVar5*8 + bVar4*9

            if field_0x20 != 0:
                section_size = bVar4 + bVar5 * 8 + bVar4 * 9
            else:
                section_size = bVar5 * 8 + bVar4 * 9

            print(f"OLD texture section: bVar5={bVar5}, bVar4={bVar4}, field_0x20={field_0x20}")
            print(f"  Section size: {section_size} bytes")
            print(f"  Data:")
            hex_str = ' '.join(f'{data[curr + i]:02X}' for i in range(min(section_size, 32)))
            print(f"    {hex_str}")
            curr += section_size
            print(f"  After OLD texture section: 0x{curr:X}")

    # Texture groups section (num_texture_groups != 0xFF)
    if num_texture_groups != 0xFF and num_textures < 0x100 and num_materials < 0x100:
        if num_texture_groups != 0:
            print(f"\nTexture groups: {num_texture_groups} groups")
            for i in range(num_texture_groups):
                tg = data[curr + i*9:curr + i*9 + 9]
                print(f"  Group[{i}]: {' '.join(f'{b:02X}' for b in tg)}")
            curr += num_texture_groups * 9

            # Bone index section - from Ghidra: ((field_0x20 != 0) + 3) * byte_at_0x1d
            # Where byte_at_0x1d is num_bone_weight_sets
            bone_mult = (1 if field_0x20 != 0 else 0) + 3
            bone_idx_size = bone_mult * num_bone_weight_sets
            print(f"  Bone index: ({1 if field_0x20 != 0 else 0}+3) * {num_bone_weight_sets} = {bone_idx_size} bytes")
            if bone_idx_size > 0:
                print(f"    Data: {' '.join(f'{data[curr+i]:02X}' for i in range(min(16, bone_idx_size)))}")
            curr += bone_idx_size

            # Texture references
            tex_ref_size = num_textures * 8
            print(f"  Texture refs: {num_textures} * 8 = {tex_ref_size} bytes")
            curr += tex_ref_size

            # Texture names (null-terminated strings)
            for i in range(num_textures):
                name_start = curr
                while curr < len(data) and data[curr] != 0:
                    curr += 1
                name = data[name_start:curr].decode('ascii', errors='replace')
                print(f"  Texture[{i}]: '{name}'")
                if curr < len(data):
                    curr += 1

            # Material data
            mat_size = num_materials * 8
            print(f"  Materials: {num_materials} * 8 = {mat_size} bytes")
            curr += mat_size

    # Vertex buffer section (flag 0x020)
    if class_flags & 0x020:
        vb_size = struct.unpack_from('<I', data, curr)[0]
        vb_field2 = struct.unpack_from('<I', data, curr + 4)[0]
        print(f"\nVertex buffer: size={vb_size}, field2={vb_field2}")
        curr += 8 + vb_size
        print(f"  After vertex buffer: 0x{curr:X}")

    # Morph targets (flag 0x080)
    if class_flags & 0x080:
        print(f"\nMorph targets at 0x{curr:X}")
        # Skip for now - need more RE
        curr += 12

    # Submesh data (flag 0x008)
    if class_flags & 0x008:
        print(f"\n--- SUBMESH DATA at 0x{curr:X} ---")

        # First DWORD is submesh count
        num_submeshes = struct.unpack_from('<I', data, curr)[0]
        print(f"num_submeshes: {num_submeshes}")
        curr += 4

        if num_submeshes > 254 or num_submeshes == 0:
            print(f"  WARNING: num_submeshes={num_submeshes}, scanning for submesh patterns...")
            scan_for_submesh_header(data, chunk_start + 0x30, chunk_end)
            # Continue to see if we can still find something
            if num_submeshes == 0:
                return

        # Parse each submesh
        for sub_idx in range(num_submeshes):
            print(f"\n  Submesh {sub_idx} at 0x{curr:X}:")

            # Submesh header - try 24 bytes (6 DWORDs)
            # num_indices, num_vertices, num_uv_sets, num_vertex_groups, num_colors, num_normals
            if curr + 24 > len(data):
                print(f"    ERROR: Not enough data for submesh header")
                break

            # Try different header interpretations
            print(f"    Raw header bytes: {' '.join(f'{data[curr+i]:02X}' for i in range(32))}")

            # Interpretation 1: Standard 24-byte header
            num_indices = struct.unpack_from('<I', data, curr)[0]
            num_vertices = struct.unpack_from('<I', data, curr + 4)[0]

            print(f"    Interpretation 1 (offset +0): num_indices={num_indices}, num_vertices={num_vertices}")

            # Interpretation 2: 8-byte prefix + counts at +8
            num_indices_v2 = struct.unpack_from('<I', data, curr + 8)[0]
            num_vertices_v2 = struct.unpack_from('<I', data, curr + 12)[0]
            f5 = struct.unpack_from('<I', data, curr + 16)[0]
            f6 = struct.unpack_from('<I', data, curr + 20)[0]

            print(f"    Interpretation 2 (offset +8): num_indices={num_indices_v2}, num_vertices={num_vertices_v2}")
            print(f"      f5={f5}, f6={f6}")

            # Use interpretation 2 if it looks better
            if num_indices_v2 > 0 and num_vertices_v2 > 0 and num_indices_v2 < 100000 and num_vertices_v2 < 50000:
                num_indices = num_indices_v2
                num_vertices = num_vertices_v2
                curr += 8  # Skip prefix
                print(f"    Using interpretation 2!")

            # Sanity check
            if num_indices < 100000 and num_vertices < 50000 and num_indices > 0:
                curr += 24

                # Read first few indices
                if curr + num_indices * 2 <= len(data):
                    indices = [struct.unpack_from('<H', data, curr + i*2)[0]
                               for i in range(min(10, num_indices))]
                    max_idx = max(indices) if indices else 0
                    print(f"    First indices: {indices}")
                    print(f"    Max index: {max_idx}, expected < {num_vertices}")

                    if max_idx < num_vertices:
                        print(f"    *** VALID SUBMESH! ***")
                        curr += num_indices * 2

                        # Skip vertex data (estimated)
                        # Position: 12 bytes, plus normals, UVs, etc.
                        estimated_vertex_size = 12  # Just position for now
                        curr += num_vertices * estimated_vertex_size
                        print(f"    After vertex data: 0x{curr:X}")
                    else:
                        print(f"    Indices reference invalid vertices, trying different header size")
            else:
                print(f"    Values don't look like valid submesh header")

def scan_for_submesh_header(data, start, end):
    """Scan for patterns that look like submesh headers"""
    print(f"  Scanning from 0x{start:X} to 0x{end:X}")

    found = []
    for offset in range(start, end - 32, 4):
        # Try multiple header interpretations

        # Interpretation 1: Counts at offset +0, header 24 bytes
        num_indices_1 = struct.unpack_from('<I', data, offset)[0]
        num_vertices_1 = struct.unpack_from('<I', data, offset + 4)[0]

        # Interpretation 2: Counts at offset +8, header 32 bytes
        num_indices_2 = struct.unpack_from('<I', data, offset + 8)[0]
        num_vertices_2 = struct.unpack_from('<I', data, offset + 12)[0]

        for interp, (num_indices, num_vertices, header_size) in enumerate([
            (num_indices_1, num_vertices_1, 24),
            (num_indices_2, num_vertices_2, 32)
        ], 1):
            # Check if values are reasonable
            if 10 <= num_indices <= 10000 and 10 <= num_vertices <= 5000:
                if num_indices % 3 == 0:  # Triangles
                    # Check if following bytes look like 16-bit indices
                    idx_offset = offset + header_size
                    if idx_offset + num_indices * 2 <= len(data):
                        indices = [struct.unpack_from('<H', data, idx_offset + i*2)[0]
                                   for i in range(min(5, num_indices))]
                        max_idx = max(indices) if indices else 0

                        if 0 < max_idx < num_vertices:
                            found.append((offset, interp, num_indices, num_vertices, indices, max_idx, header_size))

    # Print found candidates
    for offset, interp, num_indices, num_vertices, indices, max_idx, header_size in found:
        print(f"\n  FOUND at 0x{offset:X} (interp {interp}, header={header_size}): indices={num_indices}, vertices={num_vertices}")
        print(f"    First indices: {indices}, max={max_idx}")
        print(f"    Header bytes: {' '.join(f'{data[offset+i]:02X}' for i in range(header_size))}")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    # All files
    parse_bb8_chunk(os.path.join(sample_dir, "0x21448_other.gwraw"))
    parse_bb8_chunk(os.path.join(sample_dir, "0x26B44_other.gwraw"))
    parse_bb8_chunk(os.path.join(sample_dir, "0x54992_other.gwraw"))

if __name__ == "__main__":
    main()
