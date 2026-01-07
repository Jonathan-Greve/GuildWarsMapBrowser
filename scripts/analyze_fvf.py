#!/usr/bin/env python3
"""Analyze submesh FVF (Flexible Vertex Format) and calculate vertex sizes"""
import struct
import os

# FVF lookup tables from ffna_type2.txt
fvf_array_0 = [
    0x0, 0x8, 0x8, 0x10, 0x8, 0x10, 0x10, 0x18,
    0x8, 0x10, 0x10, 0x18, 0x10, 0x18, 0x18, 0x20,
    0x0, 0x0, 0x0, 0x1, 0xFFFFFFFF, 0xFFFFFFFF
]

fvf_array_1 = [
    0x0, 0xC, 0xC, 0x18, 0xC, 0x18, 0x18, 0x24
]

fvf_array_2 = [
    0x0, 0xC, 0x4, 0x10, 0xC, 0x18, 0x10, 0x1C,
    0x4, 0x10, 0x8, 0x14, 0x10, 0x1C, 0x14, 0x20
]

def get_fvf(dat_fvf):
    """Convert data FVF to actual FVF"""
    return ((dat_fvf & 0xff0) << 4) | ((dat_fvf >> 8) & 0x30) | (dat_fvf & 0xf)

def get_vertex_size_from_fvf(fvf):
    """Calculate vertex size from FVF using lookup tables"""
    idx0a = (fvf >> 12) & 0xf
    idx0b = (fvf >> 8) & 0xf
    idx1 = (fvf >> 4) & 0x7
    idx2 = fvf & 0xf

    size = fvf_array_0[idx0a] + fvf_array_0[idx0b] + fvf_array_1[idx1] + fvf_array_2[idx2]
    return size

def decode_fvf_flags(fvf):
    """Decode FVF flags into human-readable format"""
    flags = []
    if fvf & 0x01:
        flags.append("Position (12 bytes)")
    if fvf & 0x02:
        flags.append("BoneIndex (4 bytes)")
    if fvf & 0x04:
        flags.append("Normal (12 bytes)")
    if fvf & 0x08:
        flags.append("Unknown 0x08")
    if fvf & 0x10:
        flags.append("Tangent (12 bytes)")
    if fvf & 0x20:
        flags.append("Bitangent (12 bytes)")

    # UV coords (bits 8-11)
    uv_count = (fvf >> 8) & 0xf
    if uv_count > 0:
        flags.append(f"UV0-{uv_count-1} ({uv_count * 8} bytes)")

    # Secondary UV (bits 12-15)
    uv2_count = (fvf >> 12) & 0xf
    if uv2_count > 0:
        flags.append(f"SecondaryUV ({uv2_count * 8} bytes)")

    if fvf & 0x1000:
        flags.append("BoneWeights")
    if fvf & 0x2000:
        flags.append("PerVertexBoneIdx")

    return flags

def analyze_file(path):
    """Analyze a file for FVF patterns in submeshes"""
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

    # Look for submesh count in header
    # Dump header area to find structure
    print(f"\nChunk header (first 0x80 bytes):")
    for row in range(0, min(0x80, chunk_size), 16):
        addr = chunk_start + row
        hex_str = ' '.join(f'{data[addr + j]:02X}' for j in range(min(16, chunk_end - addr)))
        dwords = [struct.unpack_from('<I', data, addr + j*4)[0] for j in range(min(4, (chunk_end - addr)//4))]
        dwords_str = ' '.join(f'{d:08X}' for d in dwords)
        print(f"  0x{addr:04X} (+0x{row:02X}): {hex_str}")
        print(f"              DWORDs: {dwords_str}")

    # Scan for submesh headers using the known pattern:
    # 8 zeros + num_indices + num_vertices + f5(1) + f6(1) + f7(1) + f8(0) = 32 bytes
    print(f"\nScanning for submesh headers...")

    submeshes = []
    for off in range(chunk_start, chunk_end - 40, 1):
        # Check for 8 zero bytes
        if data[off:off+8] == b'\x00\x00\x00\x00\x00\x00\x00\x00':
            num_indices = struct.unpack_from('<I', data, off + 8)[0]
            num_vertices = struct.unpack_from('<I', data, off + 12)[0]
            f5 = struct.unpack_from('<I', data, off + 16)[0]
            f6 = struct.unpack_from('<I', data, off + 20)[0]
            f7 = struct.unpack_from('<I', data, off + 24)[0]
            f8 = struct.unpack_from('<I', data, off + 28)[0]

            # Check if it looks like a valid header
            if 10 <= num_indices <= 50000 and 10 <= num_vertices <= 20000:
                if f5 == 1 and f6 == 1 and f7 == 1 and f8 == 0:
                    # Verify indices
                    idx_start = off + 32
                    if idx_start + num_indices * 2 <= len(data):
                        indices = [struct.unpack_from('<H', data, idx_start + i*2)[0]
                                   for i in range(min(10, num_indices))]
                        max_idx = max(indices) if indices else 0

                        if max_idx < num_vertices:
                            submeshes.append({
                                'offset': off,
                                'num_indices': num_indices,
                                'num_vertices': num_vertices,
                                'idx_start': idx_start,
                                'max_idx': max_idx
                            })

    print(f"Found {len(submeshes)} submeshes")

    # Analyze each submesh
    for i, sub in enumerate(submeshes):
        print(f"\n--- Submesh {i} at 0x{sub['offset']:X} ---")
        print(f"  num_indices: {sub['num_indices']} ({sub['num_indices']//3} triangles)")
        print(f"  num_vertices: {sub['num_vertices']}")

        # Dump the full header area (before and after offset)
        header_start = max(chunk_start, sub['offset'] - 8)
        print(f"\n  Extended header bytes (from 0x{header_start:X}):")
        for row in range(0, 48, 16):
            addr = header_start + row
            if addr + 16 <= len(data):
                hex_str = ' '.join(f'{data[addr + j]:02X}' for j in range(16))
                print(f"    0x{addr:04X} (+{row:2d}): {hex_str}")

        # Look at all DWORDs in submesh header area
        print(f"\n  Header DWORDs from submesh start (0x{sub['offset']:X}):")
        for j in range(8):
            dw_off = sub['offset'] + j * 4
            if dw_off + 4 <= len(data):
                dw = struct.unpack_from('<I', data, dw_off)[0]
                print(f"    +{j*4:2d} (0x{dw_off:04X}): 0x{dw:08X} ({dw})")

        indices_end = sub['idx_start'] + sub['num_indices'] * 2
        vertex_start = indices_end

        # Calculate vertex data size using next submesh or chunk end
        if i + 1 < len(submeshes):
            next_offset = submeshes[i + 1]['offset']
        else:
            next_offset = chunk_end

        vertex_data_size = next_offset - vertex_start
        vertex_size = vertex_data_size // sub['num_vertices'] if sub['num_vertices'] > 0 else 0
        remainder = vertex_data_size % sub['num_vertices'] if sub['num_vertices'] > 0 else 0

        print(f"  vertex data: 0x{vertex_start:X} - 0x{next_offset:X}")
        print(f"  vertex_data_size: {vertex_data_size} bytes")
        print(f"  bytes per vertex: {vertex_size} (remainder: {remainder})")

        # Try to find matching FVF
        print(f"\n  Looking for matching FVF...")
        found_match = False
        for test_fvf in range(0, 0xFFFF):
            size = get_vertex_size_from_fvf(test_fvf)
            if size == vertex_size:
                dat_fvf = None
                # Try to reverse the FVF conversion
                for df in range(0, 0xFFFF):
                    if get_fvf(df) == test_fvf:
                        dat_fvf = df
                        break

                flags = decode_fvf_flags(test_fvf)
                dat_fvf_str = f"0x{dat_fvf:04X}" if dat_fvf is not None else "None"
                print(f"    FVF match: 0x{test_fvf:04X} (dat_fvf: {dat_fvf_str})")
                print(f"      Flags: {', '.join(flags)}")
                found_match = True
                break

        if not found_match:
            print(f"    No exact FVF match for size {vertex_size}")

        # Always show common FVF sizes for comparison
        common_fvfs = [0x001, 0x005, 0x007, 0x101, 0x105, 0x107, 0x112, 0x115, 0x117, 0x305, 0x307]
        print(f"    Common FVF sizes:")
        for fvf in common_fvfs:
            size = get_vertex_size_from_fvf(fvf)
            flags = decode_fvf_flags(fvf)
            verts_fit = vertex_data_size // size if size > 0 else 0
            print(f"      0x{fvf:04X}: {size} bytes ({verts_fit} vertices would fit, need {sub['num_vertices']})")

        # Read first few vertices to analyze structure
        print(f"\n  First vertex bytes:")
        if vertex_start + 48 <= len(data):
            for row in range(0, min(48, max(vertex_size, 20)), 16):
                hex_str = ' '.join(f'{data[vertex_start + row + j]:02X}'
                                   for j in range(min(16, len(data) - vertex_start - row)))
                print(f"    +0x{row:02X}: {hex_str}")

            # Try to decode as position + UV
            if vertex_start + 20 <= len(data):
                x, y, z = struct.unpack_from('<fff', data, vertex_start)
                print(f"\n  If Position: ({x:.4f}, {y:.4f}, {z:.4f})")

                if vertex_start + 20 <= len(data):
                    u, v = struct.unpack_from('<ff', data, vertex_start + 12)
                    print(f"  If UV at +12: ({u:.4f}, {v:.4f})")

        # If 20 bytes/vertex, calculate where vertex data ends
        if vertex_size != 20:
            print(f"\n  Testing FVF 0x101 (20 bytes/vertex):")
            actual_vertex_end = vertex_start + sub['num_vertices'] * 20
            print(f"    Vertex data would end at: 0x{actual_vertex_end:X}")
            remaining = next_offset - actual_vertex_end
            print(f"    Remaining bytes to end: {remaining}")

            # Show what's after vertices
            if actual_vertex_end + 32 <= len(data):
                print(f"    Bytes after vertex data:")
                for row in range(0, 32, 16):
                    addr = actual_vertex_end + row
                    hex_str = ' '.join(f'{data[addr + j]:02X}' for j in range(16))
                    print(f"      0x{addr:04X}: {hex_str}")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"

    analyze_file(os.path.join(sample_dir, "0x21448_other.gwraw"))
    analyze_file(os.path.join(sample_dir, "0x26B44_other.gwraw"))
    analyze_file(os.path.join(sample_dir, "0x54992_other.gwraw"))

if __name__ == "__main__":
    main()
