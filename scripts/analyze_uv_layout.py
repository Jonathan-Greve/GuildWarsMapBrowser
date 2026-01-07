#!/usr/bin/env python3
"""Analyze UV layout - check if interleaved vs sequential"""
import struct
import os

def analyze_uv_layout(path):
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

    # Parse header
    bVar5 = data[chunk_start + 0x18]
    bVar4 = data[chunk_start + 0x1C]
    field_0x20 = struct.unpack_from('<I', data, chunk_start + 0x20)[0]

    # Navigate to submesh
    curr = chunk_start + 0x30
    if bVar5 != 0xFF and bVar5 != 0:
        if field_0x20 != 0:
            section_size = bVar4 + bVar5 * 8 + bVar4 * 9
        else:
            section_size = bVar5 * 8 + bVar4 * 9
        curr += section_size

    # Skip submesh count
    curr += 4
    # Skip 8-zero prefix
    if data[curr:curr+8] == b'\x00' * 8:
        curr += 8

    # Read header
    num_indices = struct.unpack_from('<I', data, curr)[0]
    num_vertices = struct.unpack_from('<I', data, curr + 4)[0]
    num_uv_sets = struct.unpack_from('<I', data, curr + 8)[0]
    curr += 24

    print(f"  num_indices={num_indices}, num_vertices={num_vertices}, num_uv_sets={num_uv_sets}")

    # Skip indices
    idx_start = curr
    curr += num_indices * 2
    pos_start = curr
    pos_size = num_vertices * 12
    uv_start = pos_start + pos_size

    print(f"  pos_start=0x{pos_start:X}, uv_start=0x{uv_start:X}")

    # Dump raw bytes at UV start
    print(f"\n  Raw UV data at 0x{uv_start:X}:")
    for row in range(0, min(128, len(data) - uv_start), 16):
        addr = uv_start + row
        hex_str = ' '.join(f'{data[addr + j]:02X}' for j in range(min(16, len(data) - addr)))
        print(f"    0x{addr:04X}: {hex_str}")

    # Try different UV interpretations
    print(f"\n  UV Interpretation 1: Interleaved (U,V) pairs")
    for i in range(min(10, num_vertices)):
        uv_off = uv_start + i * 4
        u_raw = struct.unpack_from('<H', data, uv_off)[0]
        v_raw = struct.unpack_from('<H', data, uv_off + 2)[0]
        u = u_raw / 65536.0
        v = v_raw / 65536.0
        v_flipped = 1.0 - v
        print(f"    UV[{i}]: ({u:.4f}, {v:.4f}) flipped=({u:.4f}, {v_flipped:.4f}) raw=({u_raw}, {v_raw})")

    print(f"\n  UV Interpretation 2: Sequential (all U then all V)")
    u_array_start = uv_start
    v_array_start = uv_start + num_vertices * 2
    for i in range(min(10, num_vertices)):
        u_raw = struct.unpack_from('<H', data, u_array_start + i * 2)[0]
        v_raw = struct.unpack_from('<H', data, v_array_start + i * 2)[0]
        u = u_raw / 65536.0
        v = v_raw / 65536.0
        print(f"    UV[{i}]: ({u:.4f}, {v:.4f}) raw=({u_raw}, {v_raw})")

    print(f"\n  UV Interpretation 3: Float pairs at position end")
    for i in range(min(5, num_vertices)):
        uv_off = uv_start + i * 8
        if uv_off + 8 <= len(data):
            u, v = struct.unpack_from('<ff', data, uv_off)
            print(f"    UV[{i}]: ({u:.4f}, {v:.4f})")

    # Check what vertex layout works by looking at vertex stride that makes sense
    print(f"\n  Checking position continuity...")
    print(f"  With 12-byte stride (position only):")
    for i in range(min(5, num_vertices)):
        x, y, z = struct.unpack_from('<fff', data, pos_start + i * 12)
        print(f"    P[{i}]: ({x:.2f}, {y:.2f}, {z:.2f})")

    print(f"\n  With 16-byte stride (position + 4 extra):")
    for i in range(min(5, num_vertices)):
        x, y, z = struct.unpack_from('<fff', data, pos_start + i * 16)
        print(f"    P[{i}]: ({x:.2f}, {y:.2f}, {z:.2f})")

def main():
    other_dir = r"C:\Users\Jonathan\Downloads\other model files"
    analyze_uv_layout(os.path.join(other_dir, "0x21448_other.gwraw"))

if __name__ == "__main__":
    main()
