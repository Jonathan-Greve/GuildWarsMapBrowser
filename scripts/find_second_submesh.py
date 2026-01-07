#!/usr/bin/env python3
"""Find all possible submesh headers in 0x54992"""
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

    # Find ALL 8-zero patterns in entire chunk
    print(f"\nSearching for ALL 8-zero patterns followed by reasonable counts...")
    found = []
    for off in range(chunk_start, chunk_end - 32, 1):
        if data[off:off+8] == b'\x00\x00\x00\x00\x00\x00\x00\x00':
            ni = struct.unpack_from('<I', data, off + 8)[0]
            nv = struct.unpack_from('<I', data, off + 12)[0]
            if ni < 100000 and nv < 100000 and ni > 0:
                found.append((off, ni, nv))

    print(f"Found {len(found)} patterns:")
    for off, ni, nv in found:
        print(f"\n  At 0x{off:X}: indices={ni}, vertices={nv}")
        # Show header
        print(f"    Header bytes:")
        for row in range(0, 32, 16):
            hex_str = ' '.join(f'{data[off + row + j]:02X}' for j in range(16))
            print(f"      +{row:2d}: {hex_str}")

        # Check if indices look valid
        idx_start = off + 32
        if idx_start + min(ni, 10) * 2 <= len(data):
            indices = [struct.unpack_from('<H', data, idx_start + i*2)[0]
                       for i in range(min(10, ni))]
            max_idx = max(indices) if indices else 0
            print(f"    First indices: {indices}")
            print(f"    Max index: {max_idx}, expected < {nv}")
            if max_idx < nv:
                print(f"    *** VALID ***")

    # Also look at the structure after submesh 0's vertex data
    # Submesh 0: 180 indices, 69 vertices, starts at 0x93 for indices
    print(f"\n{'='*50}")
    print(f"Structure after first submesh")
    print(f"{'='*50}")

    # Known submesh 0 location
    submesh0_idx_start = 0x93
    submesh0_idx_count = 180
    submesh0_vtx_count = 69
    submesh0_vtx_start = submesh0_idx_start + submesh0_idx_count * 2  # 0x1FB
    submesh0_vtx_end = submesh0_vtx_start + submesh0_vtx_count * 20  # 0x75F

    print(f"Submesh 0 vertex data ends at: 0x{submesh0_vtx_end:X}")

    # Dump bytes from 0x75F
    print(f"\nBytes at 0x{submesh0_vtx_end:X}:")
    for row in range(0, 256, 16):
        addr = submesh0_vtx_end + row
        if addr + 16 <= len(data):
            hex_str = ' '.join(f'{data[addr + j]:02X}' for j in range(16))
            dwords = [struct.unpack_from('<I', data, addr + j*4)[0] for j in range(4)]
            print(f"  0x{addr:04X}: {hex_str}")
            print(f"           DWORDs: {' '.join(f'{d:08X}' for d in dwords)}")

    # Try different header interpretations starting from 0x75F
    print(f"\nTrying different header interpretations at 0x{submesh0_vtx_end:X}:")

    for header_skip in [0, 8, 16, 24, 32, 40, 48]:
        test_off = submesh0_vtx_end + header_skip
        if test_off + 32 > len(data):
            continue

        ni = struct.unpack_from('<I', data, test_off)[0]
        nv = struct.unpack_from('<I', data, test_off + 4)[0]

        if 3 <= ni <= 50000 and 3 <= nv <= 20000:
            print(f"\n  Skip {header_skip} bytes: indices={ni}, vertices={nv}")
            # Check indices
            idx_start = test_off + 24  # 6 DWORD header
            if idx_start + min(ni, 10) * 2 <= len(data):
                indices = [struct.unpack_from('<H', data, idx_start + i*2)[0]
                           for i in range(min(10, ni))]
                max_idx = max(indices) if indices else 0
                print(f"    First indices: {indices}, max={max_idx}")
                if max_idx < nv:
                    print(f"    *** COULD BE VALID ***")

    # Search entire remaining range for ANY valid (indices, vertices) pair
    print(f"\n{'='*50}")
    print(f"Broad search for valid submesh pattern from 0x{submesh0_vtx_end:X} to 0x{chunk_end:X}")
    print(f"{'='*50}")

    for off in range(submesh0_vtx_end, chunk_end - 32, 2):
        ni = struct.unpack_from('<I', data, off)[0]
        nv = struct.unpack_from('<I', data, off + 4)[0]

        if 30 <= ni <= 5000 and 10 <= nv <= 2000 and ni % 3 == 0:
            # This looks like reasonable submesh counts
            # Check if following bytes look like header (small values)
            f5 = struct.unpack_from('<I', data, off + 8)[0]
            f6 = struct.unpack_from('<I', data, off + 12)[0]

            if f5 < 100 and f6 < 100:
                print(f"\n  Possible at 0x{off:X}: indices={ni}, vertices={nv}")
                print(f"    f5={f5}, f6={f6}")

                # Show header bytes
                header = ' '.join(f'{data[off + j]:02X}' for j in range(24))
                print(f"    Header: {header}")

                # Try checking indices at different offsets
                for idx_off in [16, 20, 24]:
                    idx_start = off + idx_off
                    if idx_start + min(ni, 10) * 2 <= len(data):
                        indices = [struct.unpack_from('<H', data, idx_start + i*2)[0]
                                   for i in range(min(10, ni))]
                        max_idx = max(indices) if indices else 0
                        if max_idx < nv:
                            print(f"      Indices at +{idx_off}: {indices[:5]}, max={max_idx} *** VALID ***")

    # Also look for the EXPECTED second submesh based on normal format
    # In normal format, we'd expect submesh headers to have FVF
    print(f"\n{'='*50}")
    print(f"Looking for FVF 0x101 (or similar) pattern")
    print(f"{'='*50}")

    # FVF 0x101 = Position + 1 UV set = 20 bytes per vertex
    # Search for DWORDs that could be FVF
    for off in range(submesh0_vtx_end, chunk_end - 32, 4):
        val = struct.unpack_from('<I', data, off)[0]
        if val in [0x101, 0x105, 0x107, 0x112, 0x115, 0x005, 0x007]:
            # Check surrounding context
            prev_val = struct.unpack_from('<I', data, off - 4)[0] if off > submesh0_vtx_end else 0
            print(f"  FVF 0x{val:X} found at 0x{off:X}, prev_val={prev_val}")

            # If prev_val could be num_vertices
            if 10 <= prev_val <= 5000:
                # Check two before for num_indices
                prev_prev = struct.unpack_from('<I', data, off - 8)[0] if off > submesh0_vtx_end + 4 else 0
                if 30 <= prev_prev <= 10000 and prev_prev % 3 == 0:
                    print(f"    Possible submesh: indices={prev_prev}, vertices={prev_val}, FVF=0x{val:X}")

def main():
    sample_dir = r"C:\Users\Jonathan\Downloads\other model files"
    analyze_file(os.path.join(sample_dir, "0x54992_other.gwraw"))

if __name__ == "__main__":
    main()
