#!/usr/bin/env python3
"""Analyze geometry structure in other format files"""
import struct
import os

def analyze_other_format(path):
    """Analyze the 0xBB8 chunk structure in detail"""
    print(f"\n{'='*70}")
    print(f"Analyzing: {os.path.basename(path)}")
    print(f"{'='*70}")

    with open(path, 'rb') as f:
        data = f.read()

    print(f"File size: {len(data)} bytes")

    # Check FFNA header
    if data[0:4] != b'ffna':
        print("ERROR: Not an FFNA file")
        return

    file_type = data[4]
    print(f"FFNA type: {file_type}")

    # Find all chunks
    offset = 5
    chunks = []
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]
        chunks.append((chunk_id, offset, chunk_size))
        print(f"Chunk 0x{chunk_id:X} at offset 0x{offset:X}, size={chunk_size}")
        offset += 8 + chunk_size

    # Find 0xBB8 chunk
    for chunk_id, chunk_offset, chunk_size in chunks:
        if chunk_id == 0xBB8:
            analyze_bb8_chunk(data, chunk_offset + 8, chunk_size)
            break

def analyze_bb8_chunk(data, chunk_start, chunk_size):
    """Analyze the 0xBB8 geometry chunk in detail"""
    print(f"\n--- 0xBB8 Chunk Analysis ---")
    print(f"Chunk data starts at: 0x{chunk_start:X}")

    # Header (48 bytes at 0x30)
    # Based on GeometryChunkHeaderOther structure
    print(f"\nHeader bytes (first 0x30 = 48 bytes):")
    for i in range(0, 0x30, 16):
        hex_str = ' '.join(f'{data[chunk_start + i + j]:02X}' for j in range(min(16, 0x30 - i)))
        print(f"  0x{i:02X}: {hex_str}")

    # Parse header fields
    class_flags = struct.unpack_from('<H', data, chunk_start)[0]
    f0x2 = data[chunk_start + 2]
    f0x3 = data[chunk_start + 3]
    f0x4_float = struct.unpack_from('<f', data, chunk_start + 4)[0]
    f0x8 = struct.unpack_from('<I', data, chunk_start + 8)[0]
    num_models = data[chunk_start + 0xC]
    f0xD = data[chunk_start + 0xD]
    f0xE = struct.unpack_from('<H', data, chunk_start + 0xE)[0]
    collision_count = struct.unpack_from('<H', data, chunk_start + 0x10)[0]
    f0x12 = struct.unpack_from('<H', data, chunk_start + 0x12)[0]
    f0x14 = struct.unpack_from('<I', data, chunk_start + 0x14)[0]
    max_uv_index = data[chunk_start + 0x18]
    num_texture_groups = data[chunk_start + 0x19]
    num_textures = struct.unpack_from('<H', data, chunk_start + 0x1A)[0]
    num_bone_indices = data[chunk_start + 0x1C]
    num_bone_weight_sets = data[chunk_start + 0x1D]
    num_materials = struct.unpack_from('<H', data, chunk_start + 0x1E)[0]

    print(f"\nParsed header fields:")
    print(f"  class_flags: 0x{class_flags:04X}")
    print(f"  f0x2: {f0x2}")
    print(f"  f0x3: {f0x3}")
    print(f"  f0x4_float: {f0x4_float}")
    print(f"  f0x8: {f0x8}")
    print(f"  num_models: {num_models}")
    print(f"  f0xD: {f0xD}")
    print(f"  f0xE: {f0xE}")
    print(f"  collision_count: {collision_count}")
    print(f"  f0x12: {f0x12}")
    print(f"  f0x14: {f0x14}")
    print(f"  max_uv_index (0x18): {max_uv_index}")
    print(f"  num_texture_groups (0x19): {num_texture_groups}")
    print(f"  num_textures (0x1A): {num_textures}")
    print(f"  num_bone_indices (0x1C): {num_bone_indices}")
    print(f"  num_bone_weight_sets (0x1D): {num_bone_weight_sets}")
    print(f"  num_materials (0x1E): {num_materials}")

    # Check flags
    print(f"\nClass flags breakdown:")
    print(f"  0x002 HasBoneGroups: {bool(class_flags & 0x002)}")
    print(f"  0x008 HasSubmeshData: {bool(class_flags & 0x008)}")
    print(f"  0x020 HasVertexBuffer: {bool(class_flags & 0x020)}")
    print(f"  0x040 HasBoneWeights: {bool(class_flags & 0x040)}")
    print(f"  0x080 HasMorphTargets: {bool(class_flags & 0x080)}")

    # Parse raw geometry data (after 0x30 header)
    curr = chunk_start + 0x30
    print(f"\nRaw geometry data starts at: 0x{curr:X}")

    # Dump first 64 bytes of geometry data
    print(f"\nFirst 64 bytes of geometry data:")
    for i in range(0, 64, 16):
        if curr + i < len(data):
            hex_str = ' '.join(f'{data[curr + i + j]:02X}' for j in range(min(16, len(data) - curr - i)))
            print(f"  +0x{i:02X}: {hex_str}")

    # Skip bone groups if present
    if class_flags & 0x002:
        if curr + 4 <= len(data):
            bone_group_count = struct.unpack_from('<I', data, curr)[0]
            print(f"\nBone groups: count={bone_group_count}")
            curr += 4 + bone_group_count * 28
            print(f"  After bone groups: 0x{curr:X}")

    # Parse texture groups if present
    if num_texture_groups > 0 and num_texture_groups < 0xFF:
        print(f"\nTexture groups ({num_texture_groups} groups, 9 bytes each):")
        for i in range(num_texture_groups):
            tg_bytes = data[curr + i * 9:curr + i * 9 + 9]
            hex_str = ' '.join(f'{b:02X}' for b in tg_bytes)
            print(f"  Group[{i}]: {hex_str}")
            print(f"    num_textures_to_use (byte 6): {tg_bytes[6]}")
        curr += num_texture_groups * 9
        print(f"  After texture groups: 0x{curr:X}")

        # Bone index data section
        bone_idx_multiplier = 4 if num_bone_weight_sets != 0 else 3
        bone_idx_size = bone_idx_multiplier * num_bone_indices
        print(f"\nBone index section: multiplier={bone_idx_multiplier}, num_bone_indices={num_bone_indices}, size={bone_idx_size}")

        # Texture indices at offset +8
        if bone_idx_size >= 8:
            print(f"  First 16 bytes of bone index section:")
            hex_str = ' '.join(f'{data[curr + i]:02X}' for i in range(min(16, bone_idx_size)))
            print(f"    {hex_str}")

            # Calculate total texture indices
            total_tex = 0
            for i in range(num_texture_groups):
                tg_offset = chunk_start + 0x30 + i * 9
                if class_flags & 0x002:
                    bone_group_count = struct.unpack_from('<I', data, chunk_start + 0x30)[0]
                    tg_offset += 4 + bone_group_count * 28
                total_tex += data[tg_offset + 6]  # num_textures_to_use

            print(f"  Texture indices at offset +8 (total {total_tex} indices):")
            tex_indices = list(data[curr + 8:curr + 8 + total_tex])
            print(f"    {tex_indices}")

        curr += bone_idx_size

        # Texture references
        tex_ref_size = num_textures * 8
        print(f"\nTexture references: {num_textures} * 8 = {tex_ref_size} bytes")
        curr += tex_ref_size

        # Texture names (null-terminated strings)
        print(f"\nTexture names:")
        for i in range(num_textures):
            name_start = curr
            while curr < len(data) and data[curr] != 0:
                curr += 1
            name = data[name_start:curr].decode('ascii', errors='replace')
            print(f"  [{i}]: '{name}'")
            if curr < len(data):
                curr += 1  # Skip null terminator

        # Material data
        mat_size = num_materials * 8
        print(f"\nMaterial data: {num_materials} * 8 = {mat_size} bytes at 0x{curr:X}")
        curr += mat_size

    # Skip vertex buffer if present
    if class_flags & 0x020:
        if curr + 8 <= len(data):
            vb_size = struct.unpack_from('<I', data, curr)[0]
            print(f"\nVertex buffer metadata: size={vb_size} at 0x{curr:X}")
            curr += 8 + vb_size

    # Submesh data section
    if class_flags & 0x008:
        print(f"\n--- Submesh Data at 0x{curr:X} ---")
        if curr + 4 <= len(data):
            submesh_section_size = struct.unpack_from('<I', data, curr)[0]
            print(f"Submesh section size: {submesh_section_size}")
            curr += 4

            # Dump first 128 bytes of submesh data
            print(f"\nFirst 128 bytes of submesh data:")
            for i in range(0, 128, 16):
                if curr + i < len(data):
                    hex_str = ' '.join(f'{data[curr + i + j]:02X}' for j in range(min(16, len(data) - curr - i)))
                    print(f"  +0x{i:02X}: {hex_str}")

            # Try to parse submesh headers
            print(f"\nTrying to parse {num_models} submesh(es):")
            for model_idx in range(num_models):
                if curr + 40 > len(data):
                    print(f"  Submesh {model_idx}: Not enough data")
                    break

                print(f"\n  Submesh {model_idx} at 0x{curr:X}:")
                # SubModelOther header (40 bytes)
                unknown = struct.unpack_from('<I', data, curr)[0]
                index_count = struct.unpack_from('<I', data, curr + 4)[0]
                vertex_count = struct.unpack_from('<I', data, curr + 8)[0]

                print(f"    unknown (material_index?): {unknown}")
                print(f"    index_count: {index_count}")
                print(f"    vertex_count: {vertex_count}")

                # Dump raw header bytes
                header_hex = ' '.join(f'{data[curr + i]:02X}' for i in range(40))
                print(f"    Header bytes: {header_hex}")

                curr += 40

                # Indices
                indices_size = index_count * 2
                print(f"    Indices: {index_count} * 2 = {indices_size} bytes")
                if index_count > 0 and curr + min(20, indices_size) <= len(data):
                    first_indices = [struct.unpack_from('<H', data, curr + i*2)[0] for i in range(min(10, index_count))]
                    print(f"      First indices: {first_indices}")
                curr += indices_size

                # Try to figure out vertex format
                # Look at remaining data to estimate vertex size
                remaining = len(data) - curr
                if vertex_count > 0:
                    estimated_vertex_size = remaining // vertex_count if vertex_count > 0 else 0
                    print(f"    Estimated vertex size: {estimated_vertex_size} bytes (remaining={remaining}, count={vertex_count})")

                    # Dump first vertex
                    if curr + 32 <= len(data):
                        print(f"    First 32 bytes of vertex data:")
                        hex_str = ' '.join(f'{data[curr + i]:02X}' for i in range(32))
                        print(f"      {hex_str}")

                        # Try to interpret as floats (position)
                        if curr + 12 <= len(data):
                            x = struct.unpack_from('<f', data, curr)[0]
                            y = struct.unpack_from('<f', data, curr + 4)[0]
                            z = struct.unpack_from('<f', data, curr + 8)[0]
                            print(f"    As position (floats): ({x:.4f}, {y:.4f}, {z:.4f})")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    # Analyze problematic file
    analyze_other_format(os.path.join(sample_dir, "0x21448_other.gwraw"))

    # Compare with working file
    print("\n\n" + "="*70)
    print("COMPARISON WITH WORKING FILE")
    print("="*70)
    analyze_other_format(os.path.join(sample_dir, "0x54992_other.gwraw"))

if __name__ == "__main__":
    main()
