#!/usr/bin/env python3
"""
Compare normal (0xFA0) and other (0xBB8) model formats to ensure
we can produce identical rendering data from both.

Goal: Extract from both formats:
- texture_indices: which textures to use
- uv_coords_indices: which UV set for each texture
- blend_flags: per-texture blend flags
- num_uv_coords_to_use: how many texture slots are used
"""

import struct
import os
from dataclasses import dataclass, field
from typing import List, Optional, Dict, Tuple

@dataclass
class NormalFormatData:
    """Data extracted from normal format (0xFA0) geometry chunk"""
    # From Chunk1_sub1 header
    some_num0: int = 0  # max_UV_index / num texture groups
    some_num1: int = 0  # num1 - number of texture entries
    f0x20: int = 0
    num_models: int = 0

    # From TextureAndVertexShader
    uts0: List[dict] = field(default_factory=list)  # UnknownTexStruct0 entries
    flags0: List[int] = field(default_factory=list)  # uint16 per entry
    tex_array: List[int] = field(default_factory=list)  # UV set indices
    blend_state: List[int] = field(default_factory=list)  # blend flags
    texture_index_UV_mapping: List[int] = field(default_factory=list)  # texture indices

    # From uts1 (modern format)
    uts1: List[dict] = field(default_factory=list)
    unknown_tex_stuff1: List[int] = field(default_factory=list)

    # Texture filenames
    texture_filenames: List[int] = field(default_factory=list)  # decoded file hashes

@dataclass
class OtherFormatData:
    """Data extracted from other format (0xBB8) geometry chunk"""
    # From BB8 header
    num_texture_groups: int = 0
    num_textures: int = 0
    num_materials: int = 0

    # Texture groups (9 bytes each)
    texture_groups: List[dict] = field(default_factory=list)

    # Parsed texture indices (like unknown_tex_stuff1 in normal format)
    parsed_tex_indices: List[int] = field(default_factory=list)

    # Submesh info
    submeshes: List[dict] = field(default_factory=list)

    # Texture filenames from 0xBBB chunk
    texture_filenames: List[int] = field(default_factory=list)

def decode_filename(id0: int, id1: int) -> int:
    """Decode texture filename to file hash"""
    return (id0 - 0xFF00FF) + (id1 * 0xFF00)

def parse_normal_format(data: bytes) -> NormalFormatData:
    """Parse normal format model file (uses 0xFA0 geometry chunk)"""
    result = NormalFormatData()

    if len(data) < 5:
        return result

    # FFNA header: signature(4) + type(1)
    sig = data[0:4]
    file_type = data[4]
    print(f"  Signature: {sig}, Type: {file_type}")

    offset = 5
    chunks = {}

    # Parse all chunks
    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]

        if chunk_size > len(data) - offset:
            break

        chunks[chunk_id] = (offset, chunk_size)
        print(f"  Chunk 0x{chunk_id:X} at 0x{offset:X}, size={chunk_size}")
        offset += 8 + chunk_size

    # Parse geometry chunk (0xFA0)
    if 0xFA0 in chunks:
        chunk_offset, chunk_size = chunks[0xFA0]
        parse_fa0_geometry(data, chunk_offset + 8, chunk_size, result)

    # Parse texture filenames chunk (0xFA5)
    if 0xFA5 in chunks:
        chunk_offset, chunk_size = chunks[0xFA5]
        parse_texture_filenames(data, chunk_offset + 8, chunk_size, result.texture_filenames)

    return result

def parse_fa0_geometry(data: bytes, offset: int, chunk_size: int, result: NormalFormatData):
    """Parse 0xFA0 geometry chunk to extract texture/UV data"""
    start = offset

    # Chunk1_sub1 header (84 bytes = 0x54)
    if offset + 0x54 > len(data):
        return

    result.some_num0 = data[offset + 0x18]  # some_num0 (max_UV_index)
    result.some_num1 = data[offset + 0x1C]  # some_num1 (num entries)
    result.f0x20 = struct.unpack_from('<I', data, offset + 0x20)[0]
    result.num_models = struct.unpack_from('<I', data, offset + 0x44)[0]

    f0x19 = data[offset + 0x19]
    f0x1a = data[offset + 0x1A]
    f0x1d = data[offset + 0x1D]
    num_some_struct = data[offset + 0x30]
    num_some_struct2 = struct.unpack_from('<H', data, offset + 0x50)[0]
    f0x52 = struct.unpack_from('<H', data, offset + 0x52)[0]
    f0x8 = struct.unpack_from('<I', data, offset + 0x08)[0]

    print(f"    some_num0={result.some_num0}, some_num1={result.some_num1}")
    print(f"    f0x19={f0x19}, f0x1a={f0x1a}, f0x1d={f0x1d}")
    print(f"    num_some_struct={num_some_struct}, num_some_struct2={num_some_struct2}")
    print(f"    f0x52={f0x52}, f0x8=0x{f0x8:X}")
    print(f"    num_models={result.num_models}")

    curr = offset + 0x54  # After Chunk1_sub1

    # Skip unknown0[num_some_struct * 0x1C]
    curr += num_some_struct * 0x1C

    # Skip SomeStruct2[num_some_struct2] - each has variable size
    for _ in range(num_some_struct2):
        if curr + 24 > len(data):
            return
        f0x8_ss = struct.unpack_from('<I', data, curr + 8)[0]
        f0xC_ss = struct.unpack_from('<I', data, curr + 12)[0]
        f0x10_ss = struct.unpack_from('<I', data, curr + 16)[0]
        f0x14_ss = struct.unpack_from('<I', data, curr + 20)[0]
        data_size = f0x8_ss * 7 + (f0xC_ss + f0x10_ss + f0x14_ss) * 8
        curr += 24 + data_size

    # Now parse TextureAndVertexShader
    max_UV_index = result.some_num0
    num1 = result.some_num1

    print(f"    TextureAndVertexShader at 0x{curr:X}: max_UV_index={max_UV_index}, num1={num1}")

    # UnknownTexStruct0[max_UV_index] - 8 bytes each
    for i in range(max_UV_index):
        if curr + 8 > len(data):
            return
        using_no_cull = data[curr]
        f0x1 = data[curr + 1]
        f0x2 = struct.unpack_from('<I', data, curr + 2)[0]
        pixel_shader_id = data[curr + 6]
        num_uv_coords_to_use = data[curr + 7]  # f0x7

        result.uts0.append({
            'using_no_cull': using_no_cull,
            'f0x1': f0x1,
            'f0x2': f0x2,
            'pixel_shader_id': pixel_shader_id,
            'num_uv_coords_to_use': num_uv_coords_to_use
        })
        curr += 8

    # flags0[num1] - uint16 each
    for i in range(num1):
        if curr + 2 > len(data):
            return
        result.flags0.append(struct.unpack_from('<H', data, curr)[0])
        curr += 2

    # tex_array[num1] - uint8 each (UV set indices)
    for i in range(num1):
        if curr + 1 > len(data):
            return
        result.tex_array.append(data[curr])
        curr += 1

    # zeros[num1 * 4]
    curr += num1 * 4

    # blend_state[num1] - uint8 each
    for i in range(num1):
        if curr + 1 > len(data):
            return
        result.blend_state.append(data[curr])
        curr += 1

    # texture_index_UV_mapping_maybe[num1] - uint8 each
    for i in range(num1):
        if curr + 1 > len(data):
            return
        result.texture_index_UV_mapping.append(data[curr])
        curr += 1

    # Optional unknown[num1] if f0x20 != 0
    if result.f0x20 != 0:
        curr += num1

    print(f"    After TextureAndVertexShader: 0x{curr:X}")

    # Parse uts1 if f0x19 > 0 (modern format)
    if f0x19 > 0:
        print(f"    Parsing uts1[{f0x19}]...")
        for i in range(f0x19):
            if curr + 9 > len(data):
                break
            some_flags0 = struct.unpack_from('<H', data, curr)[0]
            some_flags1 = struct.unpack_from('<H', data, curr + 2)[0]
            f0x4 = data[curr + 4]
            f0x5 = data[curr + 5]
            num_textures_to_use = data[curr + 6]
            f0x7 = data[curr + 7]
            f0x8 = data[curr + 8]

            result.uts1.append({
                'some_flags0': some_flags0,
                'some_flags1': some_flags1,
                'f0x4': f0x4,
                'f0x5': f0x5,
                'num_textures_to_use': num_textures_to_use,
                'f0x7': f0x7,
                'f0x8': f0x8
            })
            curr += 9

        # unknown_tex_stuff0[f0x1d] - uint16
        for i in range(f0x1d):
            if curr + 2 > len(data):
                break
            curr += 2

        # unknown_tex_stuff1[f0x1d] - uint8
        for i in range(f0x1d):
            if curr + 1 > len(data):
                break
            result.unknown_tex_stuff1.append(data[curr])
            curr += 1

def parse_texture_filenames(data: bytes, offset: int, chunk_size: int, filenames: List[int]):
    """Parse texture filenames chunk"""
    if chunk_size < 4:
        return

    num_filenames = struct.unpack_from('<I', data, offset)[0]
    curr = offset + 4

    # Each filename entry is 6 bytes
    actual_count = min(num_filenames, (chunk_size - 4) // 6)

    for i in range(actual_count):
        if curr + 6 > offset + chunk_size:
            break
        id0 = struct.unpack_from('<H', data, curr)[0]
        id1 = struct.unpack_from('<H', data, curr + 2)[0]
        # unknown = struct.unpack_from('<H', data, curr + 4)[0]

        file_hash = decode_filename(id0, id1)
        filenames.append(file_hash)
        curr += 6

def parse_other_format(data: bytes) -> OtherFormatData:
    """Parse other format model file (uses 0xBB8 geometry chunk)"""
    result = OtherFormatData()

    if len(data) < 5:
        return result

    sig = data[0:4]
    file_type = data[4]
    print(f"  Signature: {sig}, Type: {file_type}")

    offset = 5
    chunks = {}
    chunk_list = []  # Keep order for ATEX chunks

    while offset < len(data) - 8:
        chunk_id = struct.unpack_from('<I', data, offset)[0]
        chunk_size = struct.unpack_from('<I', data, offset + 4)[0]

        if chunk_size > len(data) - offset:
            break

        chunk_list.append((chunk_id, offset, chunk_size))
        if chunk_id not in chunks:
            chunks[chunk_id] = (offset, chunk_size)
        print(f"  Chunk 0x{chunk_id:X} at 0x{offset:X}, size={chunk_size}")
        offset += 8 + chunk_size

    # Parse geometry chunk (0xBB8)
    if 0xBB8 in chunks:
        chunk_offset, chunk_size = chunks[0xBB8]
        parse_bb8_geometry(data, chunk_offset + 8, chunk_size, result)

    # Parse texture filenames chunk (0xBBB or 0xBBC)
    for chunk_id in [0xBBB, 0xBBC]:
        if chunk_id in chunks:
            chunk_offset, chunk_size = chunks[chunk_id]
            parse_other_texture_filenames(data, chunk_offset + 8, chunk_size, result.texture_filenames)
            break

    return result

def parse_bb8_geometry(data: bytes, offset: int, chunk_size: int, result: OtherFormatData):
    """Parse 0xBB8 geometry chunk - dump raw data to understand format"""
    if chunk_size < 48:
        return

    # BB8 Header - 48 bytes (0x30)
    print(f"    BB8 Header (48 bytes):")
    header = data[offset:offset + 48]

    field_0x00 = struct.unpack_from('<I', header, 0x00)[0]
    field_0x04 = struct.unpack_from('<I', header, 0x04)[0]
    class_flags = struct.unpack_from('<I', header, 0x08)[0]
    signature0 = struct.unpack_from('<I', header, 0x0C)[0]
    signature1 = struct.unpack_from('<I', header, 0x10)[0]
    field_0x14 = struct.unpack_from('<I', header, 0x14)[0]
    num_bone_groups = header[0x18]
    num_texture_groups = header[0x19]
    num_textures = struct.unpack_from('<H', header, 0x1A)[0]
    num_bone_weights = header[0x1C]
    num_bone_indices = header[0x1D]
    num_materials = struct.unpack_from('<H', header, 0x1E)[0]

    result.num_texture_groups = num_texture_groups
    result.num_textures = num_textures
    result.num_materials = num_materials

    print(f"      0x00: {field_0x00}, 0x04: {field_0x04}, class_flags: 0x{class_flags:X}")
    print(f"      0x18: num_bone_groups={num_bone_groups}, num_texture_groups={num_texture_groups}")
    print(f"      0x1A: num_textures={num_textures}, 0x1C: num_bone_weights={num_bone_weights}")
    print(f"      0x1D: num_bone_indices={num_bone_indices}, 0x1E: num_materials={num_materials}")

    # Dump raw bytes after header to see the actual structure
    raw_after_header = data[offset + 48:offset + min(chunk_size, 200)]
    print(f"\n    Raw bytes after header (first {len(raw_after_header)} bytes):")
    for i in range(0, min(80, len(raw_after_header)), 16):
        hex_str = ' '.join(f'{b:02X}' for b in raw_after_header[i:i+16])
        ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in raw_after_header[i:i+16])
        print(f"      {i:04X}: {hex_str:48s} {ascii_str}")

    # Try to parse texture group data if num_texture_groups > 0
    if num_texture_groups > 0:
        print(f"\n    Potential texture group data ({num_texture_groups} groups, 9 bytes each):")
        curr = offset + 48
        for i in range(num_texture_groups):
            if curr + 9 > offset + chunk_size:
                break
            group_bytes = data[curr:curr + 9]
            hex_str = ' '.join(f'{b:02X}' for b in group_bytes)
            print(f"      Group[{i}]: {hex_str}")
            # Store raw bytes for analysis
            result.texture_groups.append({
                'raw': list(group_bytes)
            })
            curr += 9

    # After texture groups, look for texture indices
    # Layout: texture_groups (num_texture_groups * 9) + unknown_data + texture_indices
    if num_texture_groups > 0:
        # Calculate total textures needed
        total_tex_count = 0
        for tg in result.texture_groups:
            total_tex_count += tg['raw'][6]  # byte[6] is num_textures

        # After texture groups, skip some data and find texture indices
        # Looking for 4 bytes of texture indices at offset 0x1A from after groups
        tex_idx_offset = offset + 48 + num_texture_groups * 9
        # There seem to be 8 bytes of unknown data between groups and texture indices
        # Let me dump the area to see
        print(f"\n    Looking for texture indices after groups (offset 0x{tex_idx_offset - offset:X}):")
        area = data[tex_idx_offset:tex_idx_offset + 20]
        print(f"      {' '.join(f'{b:02X}' for b in area[:20])}")

        # The texture indices seem to be at a specific offset
        # For 0x54992: groups end at 0x12, unknown 8 bytes at 0x12-0x1A, tex indices at 0x1A
        # That's 8 bytes after groups
        tex_indices_start = tex_idx_offset + 8
        result.parsed_tex_indices = list(data[tex_indices_start:tex_indices_start + total_tex_count])
        print(f"      Parsed texture indices: {result.parsed_tex_indices}")

    # Find submesh by scanning for valid pattern
    raw_data = data[offset:offset + chunk_size]
    for scan_off in range(30, min(300, chunk_size - 24), 2):
        if scan_off + 24 > chunk_size:
            break

        num_indices = struct.unpack_from('<I', raw_data, scan_off)[0]
        num_vertices = struct.unpack_from('<I', raw_data, scan_off + 4)[0]
        num_uv_sets = struct.unpack_from('<I', raw_data, scan_off + 8)[0]

        if (6 <= num_indices <= 100000 and
            3 <= num_vertices <= 50000 and
            1 <= num_uv_sets <= 8 and
            num_indices >= num_vertices):

            idx_start = scan_off + 24
            if idx_start + num_indices * 2 <= chunk_size:
                idx0 = struct.unpack_from('<H', raw_data, idx_start)[0]
                idx1 = struct.unpack_from('<H', raw_data, idx_start + 2)[0]
                if idx0 < num_vertices and idx1 < num_vertices:
                    result.submeshes.append({
                        'offset': scan_off,
                        'num_indices': num_indices,
                        'num_vertices': num_vertices,
                        'num_uv_sets': num_uv_sets
                    })
                    print(f"\n    Submesh found at 0x{scan_off:X}: indices={num_indices}, vertices={num_vertices}, uv_sets={num_uv_sets}")
                    break

def parse_other_texture_filenames(data: bytes, offset: int, chunk_size: int, filenames: List[int]):
    """Parse texture filenames from 0xBBB/0xBBC chunk"""
    if chunk_size < 8:
        return

    unknown = struct.unpack_from('<I', data, offset)[0]
    num_filenames = struct.unpack_from('<I', data, offset + 4)[0]
    curr = offset + 8

    # Each filename entry is 6 bytes
    actual_count = min(num_filenames, (chunk_size - 8) // 6)

    for i in range(actual_count):
        if curr + 6 > offset + chunk_size:
            break
        id0 = struct.unpack_from('<H', data, curr)[0]
        id1 = struct.unpack_from('<H', data, curr + 2)[0]

        file_hash = decode_filename(id0, id1)
        filenames.append(file_hash)
        curr += 6

def compute_mesh_data_normal(normal: NormalFormatData, sub_model_index: int = 0, num_vertex_uvs: int = 1) -> dict:
    """
    Compute the mesh texture/UV data as the C++ GetMesh function does for normal format.
    Returns: {texture_indices, uv_coords_indices, blend_flags}
    """
    tex_indices = []
    uv_coords_indices = []
    blend_flags = []

    # Check if using modern format (uts1/unknown_tex_stuff1)
    if normal.unknown_tex_stuff1:
        print(f"  Using MODERN format (uts1/unknown_tex_stuff1)")

        if not normal.uts1:
            return {'tex_indices': tex_indices, 'uv_coords_indices': uv_coords_indices, 'blend_flags': blend_flags}

        # Calculate tex_index_start for this sub_model
        tex_index_start = 0
        for i in range(sub_model_index):
            if i < len(normal.uts1):
                tex_index_start += normal.uts1[i]['num_textures_to_use']

        uts1 = normal.uts1[sub_model_index % len(normal.uts1)]
        num_textures_to_use = uts1['num_textures_to_use']

        print(f"  sub_model_index={sub_model_index}, num_textures_to_use={num_textures_to_use}, tex_index_start={tex_index_start}")

        blend_flag = 0  # Default, would come from AMAT file

        for i in range(num_textures_to_use):
            idx = (tex_index_start + i) % len(normal.unknown_tex_stuff1)
            texture_index = normal.unknown_tex_stuff1[idx]

            tex_indices.append(texture_index)
            uv_coords_indices.append(i % num_vertex_uvs)
            blend_flags.append(blend_flag)

        return {
            'tex_indices': tex_indices,
            'uv_coords_indices': uv_coords_indices,
            'blend_flags': blend_flags,
            'num_textures_to_use': num_textures_to_use,
            'format': 'modern'
        }

    # Old format (uts0/tex_array/blend_state)
    if not normal.uts0:
        return {'tex_indices': tex_indices, 'uv_coords_indices': uv_coords_indices, 'blend_flags': blend_flags}

    print(f"  Using OLD format (uts0/tex_array/blend_state)")

    # Get uts0 for this sub_model
    uts = normal.uts0[sub_model_index % len(normal.uts0)]
    num_uv_coords_to_use = uts['num_uv_coords_to_use']

    # Calculate start index for this sub_model
    num_uv_coords_start_index = 0
    for i in range(sub_model_index):
        if i < len(normal.uts0):
            num_uv_coords_start_index += normal.uts0[i]['num_uv_coords_to_use']

    print(f"  sub_model_index={sub_model_index}, num_uv_coords_to_use={num_uv_coords_to_use}, start_index={num_uv_coords_start_index}")

    # Process texture entries
    for i in range(num_uv_coords_start_index, num_uv_coords_start_index + num_uv_coords_to_use):
        if i >= len(normal.tex_array):
            break

        uv_set_index = normal.tex_array[i]

        # Skip if uv_set_index == 255
        if uv_set_index == 255:
            continue

        uv_coords_indices.append(uv_set_index)

        if i < len(normal.texture_index_UV_mapping):
            tex_indices.append(normal.texture_index_UV_mapping[i])

        if i < len(normal.blend_state):
            blend_flags.append(normal.blend_state[i])

    return {
        'tex_indices': tex_indices,
        'uv_coords_indices': uv_coords_indices,
        'blend_flags': blend_flags,
        'num_uv_coords_to_use': num_uv_coords_to_use,
        'format': 'old'
    }

def compute_mesh_data_other(other: OtherFormatData, sub_model_index: int = 0) -> dict:
    """
    Compute the mesh texture/UV data for other format.
    This needs to produce the SAME output as the normal format.

    Key insight from Ghidra analysis:
    - The 9-byte texture_groups in 0xBB8 map directly to UnknownTexStruct1 (uts1) in 0xFA0
    - bytes 0-1: some_flags0, bytes 2-3: some_flags1, byte 4-5: f0x4/f0x5
    - byte 6: num_textures_to_use, byte 7: f0x7, byte 8: f0x8 (NOT blend_flag!)

    Blend flag logic:
    - MODERN format (with texture_groups): blend_flag defaults to 0 (opaque)
    - OLD format (no texture_groups): blend_flag defaults to 8 (alpha blend)
    """
    tex_indices = []
    uv_coords_indices = []
    blend_flags = []

    num_vertex_uvs = 1
    if other.submeshes:
        num_vertex_uvs = other.submeshes[0].get('num_uv_sets', 1)

    total_textures = len(other.texture_filenames)

    print(f"  Available data: num_vertex_uvs={num_vertex_uvs}, total_textures={total_textures}")
    print(f"  num_texture_groups={other.num_texture_groups}, parsed_tex_indices={other.parsed_tex_indices}")

    # MODERN format: has texture_groups
    if other.texture_groups and sub_model_index < len(other.texture_groups):
        tg = other.texture_groups[sub_model_index]
        raw = tg.get('raw', [])
        num_textures = raw[6] if len(raw) > 6 else 0
        # MODERN format: blend_flag defaults to 0 (opaque)
        blend_flag = 0

        # Calculate tex_index_start (sum of previous groups' num_textures)
        tex_index_start = 0
        for i in range(sub_model_index):
            prev_raw = other.texture_groups[i].get('raw', [])
            tex_index_start += prev_raw[6] if len(prev_raw) > 6 else 0

        print(f"  MODERN format: texture_group[{sub_model_index}] num_textures={num_textures}, blend_flag={blend_flag} (default)")
        print(f"  tex_index_start={tex_index_start}")

        for i in range(num_textures):
            idx = tex_index_start + i
            if idx < len(other.parsed_tex_indices):
                tex_indices.append(other.parsed_tex_indices[idx])
            else:
                tex_indices.append(i)  # fallback
            uv_coords_indices.append(i % num_vertex_uvs)
            blend_flags.append(blend_flag)

    # OLD format: no texture_groups
    elif other.num_texture_groups == 0:
        # OLD format: blend_flag defaults to 8 (alpha blend)
        print(f"  OLD format: no texture groups, blend_flag=8 (default alpha blend)")
        tex_indices = [0]
        uv_coords_indices = [0]
        blend_flags = [8]

    return {
        'tex_indices': tex_indices,
        'uv_coords_indices': uv_coords_indices,
        'blend_flags': blend_flags
    }

def compare_formats(normal_path: str, other_path: str):
    """Compare normal and other format files"""
    print(f"\n{'='*70}")
    print(f"Comparing: {os.path.basename(normal_path)} vs {os.path.basename(other_path)}")
    print(f"{'='*70}")

    with open(normal_path, 'rb') as f:
        normal_data = f.read()
    with open(other_path, 'rb') as f:
        other_data = f.read()

    print(f"\n--- Normal Format ---")
    normal = parse_normal_format(normal_data)

    print(f"\n--- Other Format ---")
    other = parse_other_format(other_data)

    # Print extracted data
    print(f"\n--- Normal Format Extracted Data ---")
    print(f"  uts0 ({len(normal.uts0)} entries) - OLD format:")
    for i, uts in enumerate(normal.uts0):
        print(f"    [{i}] num_uv_coords_to_use={uts['num_uv_coords_to_use']}, pixel_shader={uts['pixel_shader_id']}")

    print(f"  uts1 ({len(normal.uts1)} entries) - MODERN format:")
    for i, uts in enumerate(normal.uts1):
        print(f"    [{i}] num_textures_to_use={uts['num_textures_to_use']}, flags0=0x{uts['some_flags0']:04X}")

    print(f"  unknown_tex_stuff1 (texture indices): {normal.unknown_tex_stuff1}")

    print(f"  tex_array (UV set indices): {normal.tex_array}")
    print(f"  blend_state: {normal.blend_state}")
    print(f"  texture_index_UV_mapping: {normal.texture_index_UV_mapping}")
    print(f"  texture_filenames ({len(normal.texture_filenames)}): {[hex(x) for x in normal.texture_filenames]}")

    print(f"\n--- Other Format Extracted Data ---")
    print(f"  texture_groups ({len(other.texture_groups)} entries):")
    for i, tg in enumerate(other.texture_groups):
        raw = tg.get('raw', [])
        if raw:
            # Interpret raw bytes - trying different interpretations
            print(f"    [{i}] raw: {' '.join(f'{b:02X}' for b in raw)}")
            print(f"         byte[6]={raw[6]}, byte[7]={raw[7]}, byte[8]={raw[8]}")

    print(f"  parsed_tex_indices: {other.parsed_tex_indices}")
    print(f"  submeshes ({len(other.submeshes)} entries):")
    for i, sm in enumerate(other.submeshes):
        print(f"    [{i}] num_uv_sets={sm['num_uv_sets']}")

    print(f"  texture_filenames ({len(other.texture_filenames)}): {[hex(x) for x in other.texture_filenames]}")

    # Compute mesh data for comparison
    print(f"\n--- Computed Mesh Data (sub_model_index=0) ---")
    print(f"\nNormal format GetMesh output:")
    normal_mesh = compute_mesh_data_normal(normal, 0)
    print(f"  tex_indices: {normal_mesh['tex_indices']}")
    print(f"  uv_coords_indices: {normal_mesh['uv_coords_indices']}")
    print(f"  blend_flags: {normal_mesh['blend_flags']}")

    print(f"\nOther format GetMesh output (current implementation):")
    other_mesh = compute_mesh_data_other(other, 0)
    print(f"  tex_indices: {other_mesh['tex_indices']}")
    print(f"  uv_coords_indices: {other_mesh['uv_coords_indices']}")
    print(f"  blend_flags: {other_mesh['blend_flags']}")

    # Check if they match
    print(f"\n--- Comparison ---")
    tex_match = normal_mesh['tex_indices'] == other_mesh['tex_indices']
    uv_match = normal_mesh['uv_coords_indices'] == other_mesh['uv_coords_indices']
    blend_match = normal_mesh['blend_flags'] == other_mesh['blend_flags']

    print(f"  tex_indices match: {tex_match}")
    print(f"  uv_coords_indices match: {uv_match}")
    print(f"  blend_flags match: {blend_match}")

    if not (tex_match and uv_match and blend_match):
        print(f"\n  MISMATCH! Need to adjust other format parsing.")
        print(f"\n  Expected (from normal): tex={normal_mesh['tex_indices']}, uv={normal_mesh['uv_coords_indices']}, blend={normal_mesh['blend_flags']}")
        print(f"  Got (from other):       tex={other_mesh['tex_indices']}, uv={other_mesh['uv_coords_indices']}, blend={other_mesh['blend_flags']}")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    # Compare the flower model
    compare_formats(
        os.path.join(sample_dir, "0x213A2_normal.gwraw"),
        os.path.join(sample_dir, "0x213A2_other.gwraw")
    )

    # Compare another model
    compare_formats(
        os.path.join(sample_dir, "0x54992_normal.gwraw"),
        os.path.join(sample_dir, "0x54992_other.gwraw")
    )

if __name__ == "__main__":
    main()
