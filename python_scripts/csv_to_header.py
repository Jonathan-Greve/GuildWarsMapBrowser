import re
import csv

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
    csv_path = "data.csv"
    header_path = "header_file.h"

    csv_to_header(csv_path, header_path)
