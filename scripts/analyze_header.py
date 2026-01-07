#!/usr/bin/env python3
"""Analyze the header structure based on Ghidra analysis"""
import struct
import os

def analyze_header(path):
    """Analyze header with correct offsets from Ghidra"""
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
            break
        offset += 8 + chunk_size
    else:
        print("No 0xBB8 chunk found")
        return

    print(f"Chunk data at: 0x{chunk_start:X}")

    # Based on Ghidra analysis:
    # - puVar1[2] at offset 0x08 contains the class flags
    # - puVar1[6] at offset 0x18 = byte at 0x18
    # - puVar1[7] at offset 0x1C = byte at 0x1C
    # - puVar1[8] at offset 0x20 = byte at 0x20
    # - puVar1[9] at offset 0x24 = stored as some value
    # - puVar1[10] at offset 0x28 = used in submesh conversion
    # - puVar1[11] at offset 0x2C = used in submesh conversion

    # Dump first 0x30 bytes with DWORD interpretation
    print("\nHeader as DWORDs (from Ghidra perspective):")
    for i in range(12):  # 12 DWORDs = 48 bytes
        dword = struct.unpack_from('<I', data, chunk_start + i * 4)[0]
        print(f"  puVar1[{i:2d}] (offset 0x{i*4:02X}): 0x{dword:08X} ({dword})")

    # Key fields based on Ghidra:
    class_flags = struct.unpack_from('<I', data, chunk_start + 0x08)[0]
    field_0x18 = data[chunk_start + 0x18]  # Used as byte
    field_0x19 = data[chunk_start + 0x19]  # num_texture_groups
    field_0x1A = struct.unpack_from('<H', data, chunk_start + 0x1A)[0]  # num_textures
    field_0x1C = data[chunk_start + 0x1C]  #
    field_0x1D = data[chunk_start + 0x1D]  # num_bone_weight_sets
    field_0x1E = struct.unpack_from('<H', data, chunk_start + 0x1E)[0]  # num_materials

    print(f"\nKey fields (Ghidra interpretation):")
    print(f"  class_flags (0x08): 0x{class_flags:04X}")
    print(f"    HasBoneGroups (0x002): {bool(class_flags & 0x002)}")
    print(f"    HasVertexBuffer (0x020): {bool(class_flags & 0x020)}")
    print(f"    HasBoneWeights (0x040): {bool(class_flags & 0x040)}")
    print(f"    HasMorphTargets (0x080): {bool(class_flags & 0x080)}")
    print(f"    HasSubmeshData (0x008): {bool(class_flags & 0x008)}")
    print(f"    Flag 0x001: {bool(class_flags & 0x001)}")
    print(f"    Flag 0x004: {bool(class_flags & 0x004)}")
    print(f"    Flag 0x010: {bool(class_flags & 0x010)}")
    print(f"    Flag 0x100: {bool(class_flags & 0x100)}")
    print(f"    Flag 0x200: {bool(class_flags & 0x200)}")
    print(f"    Flag 0x400: {bool(class_flags & 0x400)}")
    print(f"  field_0x18: {field_0x18}")
    print(f"  num_texture_groups (0x19): {field_0x19}")
    print(f"  num_textures (0x1A): {field_0x1A}")
    print(f"  field_0x1C: {field_0x1C}")
    print(f"  num_bone_weight_sets (0x1D): {field_0x1D}")
    print(f"  num_materials (0x1E): {field_0x1E}")

    # Now trace through the data parsing
    print(f"\n--- Data Parsing Trace ---")
    curr = chunk_start + 0x30  # After 48-byte header
    print(f"Raw data starts at: 0x{curr:X}")

    # Bone groups (flag 0x002)
    if class_flags & 0x002:
        bone_count = struct.unpack_from('<I', data, curr)[0]
        print(f"\nBone groups: count={bone_count}")
        curr += 4 + bone_count * 28
        print(f"  After bone groups: 0x{curr:X}")

    # Check flag 0x040 (bone weights) - skip for now
    if class_flags & 0x040:
        print(f"\nBone weights section present (flag 0x040)")
        # Complex parsing needed

    # Texture/material section (num_texture_groups < 0xFF)
    bVar5 = field_0x18  # puVar1[6] as byte
    bVar4 = field_0x1C  # This might be puVar1[7] as byte?

    print(f"\nTexture section check:")
    print(f"  bVar5 (0x18 as byte): {bVar5}")
    print(f"  field_0x19 (num_texture_groups): {field_0x19}")

    if bVar5 != 0xFF:
        if bVar5 != 0:
            # Calculate size: -(uint)(puVar1[8] != 0) & bVar4 + bVar5 * 8 + bVar4 * 9
            # This is complex - let's simplify
            # If puVar1[8] != 0, subtract bVar4, else don't
            field_0x20 = struct.unpack_from('<I', data, chunk_start + 0x20)[0]
            size_adjust = field_0x1C if field_0x20 != 0 else 0
            section_size = bVar5 * 8 + field_0x1C * 9 - size_adjust
            print(f"  Section size calculation: {bVar5}*8 + {field_0x1C}*9 - {size_adjust} = {section_size}")
            print(f"  Data at curr:")
            hex_str = ' '.join(f'{data[curr + i]:02X}' for i in range(min(32, len(data) - curr)))
            print(f"    {hex_str}")

    # num_texture_groups section
    if field_0x19 != 0xFF and field_0x1A < 0x100 and field_0x1E < 0x100:
        print(f"\nTexture groups section:")
        if field_0x19 != 0:
            print(f"  {field_0x19} texture groups at 0x{curr:X}")
            for i in range(field_0x19):
                tg_bytes = data[curr + i * 9:curr + i * 9 + 9]
                hex_str = ' '.join(f'{b:02X}' for b in tg_bytes)
                print(f"    Group[{i}]: {hex_str}")
            curr += field_0x19 * 9

            # Bone index section
            bone_mult = 4 if field_0x1D != 0 else 3
            bone_size = bone_mult * field_0x1C
            print(f"  Bone index: {bone_mult} * {field_0x1C} = {bone_size} bytes")
            curr += bone_size

            # Texture refs
            tex_size = field_0x1A * 8
            print(f"  Texture refs: {field_0x1A} * 8 = {tex_size} bytes")
            curr += tex_size

            # Texture names
            for i in range(field_0x1A):
                name_start = curr
                while curr < len(data) and data[curr] != 0:
                    curr += 1
                name = data[name_start:curr].decode('ascii', errors='replace')
                print(f"  Texture name[{i}]: '{name}'")
                curr += 1

            # Materials
            mat_size = field_0x1E * 8
            print(f"  Materials: {field_0x1E} * 8 = {mat_size} bytes")
            curr += mat_size

    # Vertex buffer (flag 0x020)
    if class_flags & 0x020:
        print(f"\nVertex buffer section at 0x{curr:X}:")
        vb_size = struct.unpack_from('<I', data, curr)[0]
        vb_field2 = struct.unpack_from('<I', data, curr + 4)[0]
        print(f"  Size: {vb_size}, Field2: {vb_field2}")
        curr += 8 + vb_size

    # Morph targets (flag 0x080)
    if class_flags & 0x080:
        print(f"\nMorph targets at 0x{curr:X}")
        # Skip for now
        curr += 12  # Basic header

    # Submesh data (flag 0x008)
    if class_flags & 0x008:
        print(f"\n--- SUBMESH DATA at 0x{curr:X} ---")
        num_submeshes = struct.unpack_from('<I', data, curr)[0]
        print(f"  num_submeshes: {num_submeshes}")
        curr += 4

        if num_submeshes > 0 and num_submeshes <= 254:
            print(f"\n  First 64 bytes of submesh section:")
            for i in range(0, 64, 16):
                if curr + i < len(data):
                    hex_str = ' '.join(f'{data[curr + i + j]:02X}' for j in range(min(16, len(data) - curr - i)))
                    print(f"    +0x{i:02X}: {hex_str}")

            # Try to parse first submesh
            # Submesh header from MdlDecomp_ConvertSubmesh
            print(f"\n  Attempting to parse submesh 0:")
            if curr + 24 <= len(data):
                # Based on comment: "Submesh header is 24 bytes (6 DWORDs):
                # num_indices, num_vertices, num_uv_sets, num_vertex_groups, num_colors, num_normals"
                num_indices = struct.unpack_from('<I', data, curr)[0]
                num_vertices = struct.unpack_from('<I', data, curr + 4)[0]
                num_uv_sets = struct.unpack_from('<I', data, curr + 8)[0]
                num_vertex_groups = struct.unpack_from('<I', data, curr + 12)[0]
                num_colors = struct.unpack_from('<I', data, curr + 16)[0]
                num_normals = struct.unpack_from('<I', data, curr + 20)[0]

                print(f"    num_indices: {num_indices}")
                print(f"    num_vertices: {num_vertices}")
                print(f"    num_uv_sets: {num_uv_sets}")
                print(f"    num_vertex_groups: {num_vertex_groups}")
                print(f"    num_colors: {num_colors}")
                print(f"    num_normals: {num_normals}")

                # Sanity check
                if num_indices < 100000 and num_vertices < 100000:
                    print(f"    Looks reasonable!")
                else:
                    print(f"    WARNING: Values seem too large, header might be different")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    # Problematic files
    analyze_header(os.path.join(sample_dir, "0x21448_other.gwraw"))
    analyze_header(os.path.join(sample_dir, "0x26B44_other.gwraw"))
    # Working file for comparison
    analyze_header(os.path.join(sample_dir, "0x54992_other.gwraw"))

if __name__ == "__main__":
    main()
