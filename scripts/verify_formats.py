#!/usr/bin/env python3
"""Simple script to verify key fields in both format files"""
import struct
import os

def hex_dump(data, start, length, label=""):
    """Print hex dump of bytes"""
    end = min(start + length, len(data))
    bytes_data = data[start:end]
    hex_str = ' '.join(f'{b:02X}' for b in bytes_data)
    print(f"  {label} [{start:04X}-{end-1:04X}]: {hex_str}")

def verify_other_format(path):
    """Verify key fields in other format (0xBB8)"""
    print(f"\n=== Verifying: {os.path.basename(path)} ===")
    with open(path, 'rb') as f:
        data = f.read()

    # Find 0xBB8 chunk
    offset = 5
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]
        if chunk_id == 0xBB8:
            print(f"Found 0xBB8 chunk at offset 0x{offset:X}, size={chunk_size}")
            chunk_data_start = offset + 8

            # Header at chunk_data_start (48 bytes = 0x30)
            num_texture_groups = data[chunk_data_start + 0x19]
            num_textures = struct.unpack_from('<H', data, chunk_data_start + 0x1A)[0]
            num_materials = struct.unpack_from('<H', data, chunk_data_start + 0x1E)[0]

            print(f"  num_texture_groups: {num_texture_groups}")
            print(f"  num_textures: {num_textures}")
            print(f"  num_materials: {num_materials}")

            # Texture groups start at chunk_data_start + 0x30 (48 bytes)
            if num_texture_groups > 0:
                print(f"\n  Texture groups (9 bytes each):")
                tg_start = chunk_data_start + 0x30
                for i in range(num_texture_groups):
                    tg_offset = tg_start + i * 9
                    tg_bytes = data[tg_offset:tg_offset + 9]
                    hex_str = ' '.join(f'{b:02X}' for b in tg_bytes)
                    print(f"    Group[{i}]: {hex_str}")
                    print(f"      byte[6] (num_textures_to_use): {tg_bytes[6]}")
                    print(f"      byte[8] (f0x8, NOT blend_flag): {tg_bytes[8]}")

                # After texture groups, there's more data
                after_tg = tg_start + num_texture_groups * 9
                hex_dump(data, after_tg, 20, "After texture groups")
            else:
                print(f"\n  No texture groups (uses OLD format)")
                # For OLD format, dump the area after header
                hex_dump(data, chunk_data_start + 0x30, 40, "After header (submesh data)")

            break
        offset += 8 + chunk_size

def verify_normal_format(path):
    """Verify key fields in normal format (0xFA0)"""
    print(f"\n=== Verifying: {os.path.basename(path)} ===")
    with open(path, 'rb') as f:
        data = f.read()

    # Find 0xFA0 chunk
    offset = 5
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]
        if chunk_id == 0xFA0:
            print(f"Found 0xFA0 chunk at offset 0x{offset:X}, size={chunk_size}")
            chunk_data_start = offset + 8

            # Chunk1_sub1 header
            some_num0 = data[chunk_data_start + 0x18]  # max_UV_index / num_texture_groups for OLD
            some_num1 = data[chunk_data_start + 0x1C]  # num entries for OLD format
            f0x19 = data[chunk_data_start + 0x19]     # num_texture_groups for MODERN format
            f0x1d = data[chunk_data_start + 0x1D]     # num texture indices for MODERN format

            print(f"  some_num0 (OLD max_UV_idx): {some_num0}")
            print(f"  some_num1 (OLD num_entries): {some_num1}")
            print(f"  f0x19 (MODERN num_texture_groups): {f0x19}")
            print(f"  f0x1d (MODERN num_tex_indices): {f0x1d}")

            if f0x19 > 0:
                print(f"\n  MODERN format detected (uts1)")
                print(f"  blend_flag defaults to 0 (or from AMAT)")
            else:
                print(f"\n  OLD format detected (uts0/tex_array/blend_state)")
                print(f"  blend_flag comes from blend_state array")

            break
        offset += 8 + chunk_size

def extract_normal_mesh_data(path, sub_model=0):
    """Extract tex_indices, uv_coords_indices, blend_flags from normal format"""
    with open(path, 'rb') as f:
        data = f.read()

    # Find 0xFA0 chunk
    offset = 5
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]
        if chunk_id == 0xFA0:
            chunk_start = offset + 8

            some_num0 = data[chunk_start + 0x18]  # OLD format entries
            some_num1 = data[chunk_start + 0x1C]
            f0x19 = data[chunk_start + 0x19]      # MODERN format entries
            f0x1d = data[chunk_start + 0x1D]
            num_some_struct = data[chunk_start + 0x30]
            num_some_struct2 = struct.unpack_from('<H', data, chunk_start + 0x50)[0]

            # Skip to TextureAndVertexShader
            curr = chunk_start + 0x54
            curr += num_some_struct * 0x1C  # Skip unknown0

            # Skip SomeStruct2 (variable size)
            for _ in range(num_some_struct2):
                f0x8 = struct.unpack_from('<I', data, curr + 8)[0]
                f0xC = struct.unpack_from('<I', data, curr + 12)[0]
                f0x10 = struct.unpack_from('<I', data, curr + 16)[0]
                f0x14 = struct.unpack_from('<I', data, curr + 20)[0]
                data_size = f0x8 * 7 + (f0xC + f0x10 + f0x14) * 8
                curr += 24 + data_size

            # Now at TextureAndVertexShader
            if f0x19 > 0:  # MODERN format
                # Skip uts0
                curr += some_num0 * 8
                # Skip flags0, tex_array, zeros, blend_state, texture_index_UV_mapping
                curr += some_num1 * 2  # flags0
                curr += some_num1      # tex_array
                curr += some_num1 * 4  # zeros
                curr += some_num1      # blend_state
                curr += some_num1      # texture_index_UV_mapping

                # Parse uts1
                uts1 = []
                for i in range(f0x19):
                    num_textures_to_use = data[curr + 6]
                    uts1.append({'num_textures_to_use': num_textures_to_use})
                    curr += 9

                # Skip unknown_tex_stuff0
                curr += f0x1d * 2

                # Parse unknown_tex_stuff1 (texture indices)
                tex_indices_raw = list(data[curr:curr + f0x1d])

                # Compute for sub_model
                tex_index_start = sum(uts1[i]['num_textures_to_use'] for i in range(sub_model))
                num_tex = uts1[sub_model]['num_textures_to_use']

                tex_indices = tex_indices_raw[tex_index_start:tex_index_start + num_tex]
                uv_indices = [i % 1 for i in range(num_tex)]  # Assuming 1 UV set
                blend_flags = [0] * num_tex  # Default for MODERN

                return tex_indices, uv_indices, blend_flags, 'MODERN'
            else:  # OLD format
                # Parse uts0
                uts0 = []
                for i in range(some_num0):
                    num_uv_coords_to_use = data[curr + 7]
                    uts0.append({'num_uv_coords_to_use': num_uv_coords_to_use})
                    curr += 8

                # flags0
                curr += some_num1 * 2

                # tex_array (UV set indices)
                tex_array = list(data[curr:curr + some_num1])
                curr += some_num1

                # zeros
                curr += some_num1 * 4

                # blend_state
                blend_state = list(data[curr:curr + some_num1])
                curr += some_num1

                # texture_index_UV_mapping
                texture_index_mapping = list(data[curr:curr + some_num1])

                # Compute for sub_model
                start_idx = sum(uts0[i]['num_uv_coords_to_use'] for i in range(sub_model))
                num_entries = uts0[sub_model]['num_uv_coords_to_use']

                tex_indices = texture_index_mapping[start_idx:start_idx + num_entries]
                uv_indices = tex_array[start_idx:start_idx + num_entries]
                blend_flags = blend_state[start_idx:start_idx + num_entries]

                return tex_indices, uv_indices, blend_flags, 'OLD'

        offset += 8 + chunk_size
    return [], [], [], None

def extract_other_mesh_data(path, sub_model=0):
    """Extract tex_indices, uv_coords_indices, blend_flags from other format"""
    with open(path, 'rb') as f:
        data = f.read()

    # Find 0xBB8 chunk
    offset = 5
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]
        if chunk_id == 0xBB8:
            chunk_start = offset + 8
            num_texture_groups = data[chunk_start + 0x19]

            if num_texture_groups > 0:  # MODERN format
                tg_start = chunk_start + 0x30

                # Parse texture groups
                texture_groups = []
                for i in range(num_texture_groups):
                    tg_offset = tg_start + i * 9
                    num_tex = data[tg_offset + 6]
                    texture_groups.append({'num_textures_to_use': num_tex})

                # Texture indices are at tg_start + num_texture_groups * 9 + 8
                tex_idx_start = tg_start + num_texture_groups * 9 + 8
                total_tex = sum(tg['num_textures_to_use'] for tg in texture_groups)
                tex_indices_raw = list(data[tex_idx_start:tex_idx_start + total_tex])

                # Compute for sub_model
                start_idx = sum(texture_groups[i]['num_textures_to_use'] for i in range(sub_model))
                num_tex = texture_groups[sub_model]['num_textures_to_use']

                tex_indices = tex_indices_raw[start_idx:start_idx + num_tex]
                uv_indices = [i % 1 for i in range(num_tex)]  # Assuming 1 UV set
                blend_flags = [0] * num_tex  # Default for MODERN

                return tex_indices, uv_indices, blend_flags, 'MODERN'
            else:  # OLD format
                # For OLD format, we need to use defaults
                # 1 texture, UV 0, blend_flag 8
                return [0], [0], [8], 'OLD'

        offset += 8 + chunk_size
    return [], [], [], None

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    # Verify flower model
    print("\n" + "="*70)
    print("FLOWER MODEL (0x213A2)")
    print("="*70)
    verify_normal_format(os.path.join(sample_dir, "0x213A2_normal.gwraw"))
    verify_other_format(os.path.join(sample_dir, "0x213A2_other.gwraw"))

    # Extract and compare mesh data
    n_tex, n_uv, n_blend, n_fmt = extract_normal_mesh_data(os.path.join(sample_dir, "0x213A2_normal.gwraw"))
    o_tex, o_uv, o_blend, o_fmt = extract_other_mesh_data(os.path.join(sample_dir, "0x213A2_other.gwraw"))
    print(f"\n  Normal ({n_fmt}): tex={n_tex}, uv={n_uv}, blend={n_blend}")
    print(f"  Other ({o_fmt}):  tex={o_tex}, uv={o_uv}, blend={o_blend}")
    print(f"  MATCH: tex={n_tex==o_tex}, uv={n_uv==o_uv}, blend={n_blend==o_blend}")

    # Verify 0x54992 model
    print("\n" + "="*70)
    print("MODEL 0x54992")
    print("="*70)
    verify_normal_format(os.path.join(sample_dir, "0x54992_normal.gwraw"))
    verify_other_format(os.path.join(sample_dir, "0x54992_other.gwraw"))

    # Extract and compare mesh data
    n_tex, n_uv, n_blend, n_fmt = extract_normal_mesh_data(os.path.join(sample_dir, "0x54992_normal.gwraw"))
    o_tex, o_uv, o_blend, o_fmt = extract_other_mesh_data(os.path.join(sample_dir, "0x54992_other.gwraw"))
    print(f"\n  Normal ({n_fmt}): tex={n_tex}, uv={n_uv}, blend={n_blend}")
    print(f"  Other ({o_fmt}):  tex={o_tex}, uv={o_uv}, blend={o_blend}")
    print(f"  MATCH: tex={n_tex==o_tex}, uv={n_uv==o_uv}, blend={n_blend==o_blend}")

if __name__ == "__main__":
    main()
