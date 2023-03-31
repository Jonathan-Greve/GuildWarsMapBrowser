import re
import csv

def parse_file(file_path):
    with open(file_path, "r") as f:
        content = f.read()

    pattern = re.compile(r'{(0x[\da-fA-F]+|\d+),\s*\{((?:\{[\s\S]*?\}(?:,\s*)?)+)\}}')
    matches = pattern.findall(content)
    data = {}

    for match in matches:
        key = int(match[0], 0)  # Automatically determine the base
        value_str = match[1]

        value_pattern = re.compile(r'\{(\d+),\s*(\d+),\s*(0x[\da-fA-F]+|\d+),\s*"([^"]+)",\s*(\w+)\}')
        value_matches = value_pattern.findall(value_str)
        data[key] = [
            {
                "map_id": int(m[0]),
                "file_hash": int(m[2], 0),  # Automatically determine the base
                "name": m[3],
                "is_pvp": m[4].lower() == "true"
            }
            for m in value_matches
        ]

    return data

def csv_to_header(csv_path, header_path):
    with open(csv_path, 'r', newline='') as f:
        reader = csv.reader(f)

        # Skip the header row
        next(reader)

        data = {}

        for row in reader:
            file_hash, map_id_, name, is_pvp_ = row
            key = int(file_hash, 0)  # Automatically determine the base
            map_id = int(map_id_)
            is_pvp = 'true' if is_pvp_.lower() == 'true' else 'false'

            if key not in data:
                data[key] = []

            data[key].append((map_id, name, is_pvp))

    with open(header_path, 'w') as f:
        f.write('std::unordered_map<int, std::vector<MapConstantInfo>> constant_maps_info = {\n')

        for key, values in data.items():
            hex_key = f'{key:#x}' if key > 9 else str(key)
            f.write(f'  {{{hex_key},\n   {{')

            for i, value in enumerate(values):
                map_id, name, is_pvp = value
                f.write(f'{{{map_id}, {hex_key}, "{name}", {is_pvp}}}')

                if i < len(values) - 1:
                    f.write(',\n    ')

            f.write('}},\n')

        f.write('};\n')

if __name__ == "__main__":
    file_path = "unordered_map.txt"
    csv_path = "output.csv"
    header_path = "header_file.h"

    data = parse_file(file_path)
    csv_to_header(csv_path, header_path)
