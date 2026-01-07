#!/usr/bin/env python3
"""Detailed comparison of all submeshes between normal and other format"""
import struct
import os

def extract_all_submesh_data_normal(path):
    """Extract mesh data for ALL submeshes from normal format"""
    with open(path, 'rb') as f:
        data = f.read()

    results = []

    # Find 0xFA0 chunk
    offset = 5
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]
        if chunk_id == 0xFA0:
            chunk_start = offset + 8

            some_num0 = data[chunk_start + 0x18]
            some_num1 = data[chunk_start + 0x1C]
            f0x19 = data[chunk_start + 0x19]
            f0x1d = data[chunk_start + 0x1D]
            num_some_struct = data[chunk_start + 0x30]
            num_some_struct2 = struct.unpack_from('<H', data, chunk_start + 0x50)[0]

            # Skip to TextureAndVertexShader
            curr = chunk_start + 0x54
            curr += num_some_struct * 0x1C

            for _ in range(num_some_struct2):
                f0x8 = struct.unpack_from('<I', data, curr + 8)[0]
                f0xC = struct.unpack_from('<I', data, curr + 12)[0]
                f0x10 = struct.unpack_from('<I', data, curr + 16)[0]
                f0x14 = struct.unpack_from('<I', data, curr + 20)[0]
                data_size = f0x8 * 7 + (f0xC + f0x10 + f0x14) * 8
                curr += 24 + data_size

            if f0x19 > 0:  # MODERN format
                # Skip uts0
                curr += some_num0 * 8
                curr += some_num1 * 2  # flags0
                curr += some_num1      # tex_array
                curr += some_num1 * 4  # zeros
                curr += some_num1      # blend_state
                curr += some_num1      # texture_index_UV_mapping

                # Parse uts1
                uts1 = []
                for i in range(f0x19):
                    raw_bytes = data[curr:curr + 9]
                    num_textures_to_use = data[curr + 6]
                    f0x8_val = data[curr + 8]
                    uts1.append({
                        'num_textures_to_use': num_textures_to_use,
                        'f0x8': f0x8_val,
                        'raw': list(raw_bytes)
                    })
                    curr += 9

                # Skip unknown_tex_stuff0
                curr += f0x1d * 2

                # Parse unknown_tex_stuff1 (texture indices)
                tex_indices_raw = list(data[curr:curr + f0x1d])

                print(f"Normal format: f0x19={f0x19} texture groups, f0x1d={f0x1d} texture indices")
                print(f"  uts1 entries: {uts1}")
                print(f"  tex_indices_raw: {tex_indices_raw}")

                # Compute for each submesh
                for sub_model in range(f0x19):
                    tex_index_start = sum(uts1[i]['num_textures_to_use'] for i in range(sub_model))
                    num_tex = uts1[sub_model]['num_textures_to_use']
                    f0x8_val = uts1[sub_model]['f0x8']

                    tex_indices = tex_indices_raw[tex_index_start:tex_index_start + num_tex]
                    # UV indices - need to figure out what num_vertex_uvs is
                    uv_indices = [i % 1 for i in range(num_tex)]  # Assuming 1 UV set for now
                    # blend_flag is 0 for modern format in C++ (FFNA_ModelFile.h line 1521)
                    # NOT f0x8 from uts1 - that field is not used as blend_flag
                    blend_flags = [0] * num_tex

                    results.append({
                        'sub_model': sub_model,
                        'tex_indices': tex_indices,
                        'uv_indices': uv_indices,
                        'blend_flags': blend_flags,
                        'f0x8': f0x8_val,
                        'num_textures_to_use': num_tex
                    })
            else:
                # OLD format
                uts0 = []
                for i in range(some_num0):
                    num_uv_coords_to_use = data[curr + 7]
                    uts0.append({'num_uv_coords_to_use': num_uv_coords_to_use})
                    curr += 8

                curr += some_num1 * 2  # flags0
                tex_array = list(data[curr:curr + some_num1])
                curr += some_num1
                curr += some_num1 * 4  # zeros
                blend_state = list(data[curr:curr + some_num1])
                curr += some_num1
                texture_index_mapping = list(data[curr:curr + some_num1])

                print(f"Normal format: OLD style, some_num0={some_num0}, some_num1={some_num1}")
                print(f"  tex_array (UV indices): {tex_array}")
                print(f"  blend_state: {blend_state}")
                print(f"  texture_index_mapping: {texture_index_mapping}")

                for sub_model in range(some_num0):
                    start_idx = sum(uts0[i]['num_uv_coords_to_use'] for i in range(sub_model))
                    num_entries = uts0[sub_model]['num_uv_coords_to_use']

                    tex_indices = texture_index_mapping[start_idx:start_idx + num_entries]
                    uv_indices = tex_array[start_idx:start_idx + num_entries]
                    blend_flags = blend_state[start_idx:start_idx + num_entries]

                    results.append({
                        'sub_model': sub_model,
                        'tex_indices': tex_indices,
                        'uv_indices': uv_indices,
                        'blend_flags': blend_flags
                    })
            break
        offset += 8 + chunk_size

    return results

def extract_all_submesh_data_other(path):
    """Extract mesh data for ALL submeshes from other format"""
    with open(path, 'rb') as f:
        data = f.read()

    results = []

    # Find 0xBB8 chunk
    offset = 5
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]
        if chunk_id == 0xBB8:
            chunk_start = offset + 8
            num_texture_groups = data[chunk_start + 0x19]
            num_materials = struct.unpack_from('<H', data, chunk_start + 0x1E)[0]

            print(f"Other format: num_texture_groups={num_texture_groups}, num_materials={num_materials}")

            if num_texture_groups > 0:
                tg_start = chunk_start + 0x30

                texture_groups = []
                for i in range(num_texture_groups):
                    tg_offset = tg_start + i * 9
                    raw_bytes = data[tg_offset:tg_offset + 9]
                    num_tex = data[tg_offset + 6]
                    f0x8_val = data[tg_offset + 8]
                    texture_groups.append({
                        'num_textures_to_use': num_tex,
                        'f0x8': f0x8_val,
                        'raw': list(raw_bytes)
                    })
                    print(f"  texture_group[{i}]: num_tex={num_tex}, f0x8={f0x8_val}, raw={' '.join(f'{b:02X}' for b in raw_bytes)}")

                # Texture indices at offset +8 after groups
                tex_idx_start = tg_start + num_texture_groups * 9 + 8
                total_tex = sum(tg['num_textures_to_use'] for tg in texture_groups)
                tex_indices_raw = list(data[tex_idx_start:tex_idx_start + total_tex])
                print(f"  tex_indices_raw: {tex_indices_raw}")

                # Compute for each submesh
                for sub_model in range(num_texture_groups):
                    start_idx = sum(texture_groups[i]['num_textures_to_use'] for i in range(sub_model))
                    num_tex = texture_groups[sub_model]['num_textures_to_use']
                    f0x8_val = texture_groups[sub_model]['f0x8']

                    tex_indices = tex_indices_raw[start_idx:start_idx + num_tex]
                    uv_indices = [i % 1 for i in range(num_tex)]  # Current implementation
                    # blend_flag is 0 for modern format (with texture_groups)
                    # NOT f0x8 - matching normal format C++ behavior
                    blend_flags = [0] * num_tex

                    results.append({
                        'sub_model': sub_model,
                        'tex_indices': tex_indices,
                        'uv_indices': uv_indices,
                        'blend_flags': blend_flags,
                        'f0x8': f0x8_val,
                        'num_textures_to_use': num_tex
                    })
            else:
                # No texture groups
                results.append({
                    'sub_model': 0,
                    'tex_indices': [0],
                    'uv_indices': [0],
                    'blend_flags': [8]
                })
            break
        offset += 8 + chunk_size

    return results

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    print("="*70)
    print("MODEL 0x54992 - DETAILED COMPARISON")
    print("="*70)

    print("\n--- NORMAL FORMAT ---")
    normal_results = extract_all_submesh_data_normal(os.path.join(sample_dir, "0x54992_normal.gwraw"))
    for r in normal_results:
        print(f"\n  Submesh {r['sub_model']}:")
        print(f"    tex_indices: {r['tex_indices']}")
        print(f"    uv_indices:  {r['uv_indices']}")
        print(f"    blend_flags: {r['blend_flags']}")

    print("\n--- OTHER FORMAT ---")
    other_results = extract_all_submesh_data_other(os.path.join(sample_dir, "0x54992_other.gwraw"))
    for r in other_results:
        print(f"\n  Submesh {r['sub_model']}:")
        print(f"    tex_indices: {r['tex_indices']}")
        print(f"    uv_indices:  {r['uv_indices']}")
        print(f"    blend_flags: {r['blend_flags']}")

    print("\n--- COMPARISON ---")
    for i, (n, o) in enumerate(zip(normal_results, other_results)):
        tex_match = n['tex_indices'] == o['tex_indices']
        uv_match = n['uv_indices'] == o['uv_indices']
        blend_match = n['blend_flags'] == o['blend_flags']
        print(f"\n  Submesh {i}:")
        print(f"    tex_indices: {'MATCH' if tex_match else 'MISMATCH'} - normal={n['tex_indices']}, other={o['tex_indices']}")
        print(f"    uv_indices:  {'MATCH' if uv_match else 'MISMATCH'} - normal={n['uv_indices']}, other={o['uv_indices']}")
        print(f"    blend_flags: {'MATCH' if blend_match else 'MISMATCH'} - normal={n['blend_flags']}, other={o['blend_flags']}")

if __name__ == "__main__":
    main()
