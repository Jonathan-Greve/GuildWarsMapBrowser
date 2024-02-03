import pandas as pd

# Load the CSV file into a DataFrame
skydome_df = pd.read_csv('data/skydome.csv')

# Adjust column names to remove leading spaces
skydome_df.columns = skydome_df.columns.str.strip()

# Find the min and max for x, y, and z coordinates for normalization
min_x, max_x = skydome_df['in_Position0.x'].min(), skydome_df['in_Position0.x'].max()
min_y, max_y = skydome_df['in_Position0.y'].min(), skydome_df['in_Position0.y'].max()
min_z, max_z = skydome_df['in_Position0.z'].min(), skydome_df['in_Position0.z'].max()

# Calculate the maximum radius extent and scale factors
max_radius = max(max_x - min_x, max_y - min_y) / 2
scale_factor = 1 / max_radius  # Scale factor to center radius around 0
scale_factor_height = 1 / (max_z - min_z)  # Scale factor to normalize height to 0 to 1

skydome_df['temp_y'] = (skydome_df['in_Position0.z'])
skydome_df['in_Position0.z'] = (skydome_df['in_Position0.y']) * scale_factor
skydome_df['in_Position0.y'] = -skydome_df['temp_y'] * scale_factor_height
skydome_df['in_Position0.x'] = (skydome_df['in_Position0.x']) * scale_factor
skydome_df.drop('temp_y', axis=1, inplace=True)


# Define the function to generate the C++ code with adjusted coordinates
def generate_cpp_code_corrected(df):
    cpp_code = '#pragma once\n#include "MeshInstance.h"\n\n'
    cpp_code += 'class GWSkyCylinder : public MeshInstance\n{\npublic:\n    GWSkyCylinder(ID3D11Device* device, int id, float radius = 1.0f, float height = 1.0f) : MeshInstance(device, GenerateCylinderMesh(radius, height), id)\n    {\n    }\n\nprivate:\n    Mesh GenerateCylinderMesh(float radius, float height)\n    {\n        std::vector<GWVertex> vertices;\n        std::vector<uint32_t> indices = {\n            '

    # Adding indices
    indices_str = ", ".join(df['VTX'].astype(str).tolist())
    cpp_code += indices_str + '\n        };\n\n'

    # Adding vertices with positions transformed from (x, y, z) to (x, -z, y)
    cpp_code += '        // Vertices\n'
    for _, row in df.iterrows():
        pos = f'DirectX::XMFLOAT3({row["in_Position0.x"]:.6f}f * radius, {row["in_Position0.y"]:.6f}f * height, {row["in_Position0.z"]:.6f}f * radius)'  # Swap y and z, negate z
        tex = f'DirectX::XMFLOAT2({row["in_Texcoord0.x"]:.6f}f, {row["in_Texcoord0.y"]:.6f}f)'
        normal = 'DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f)'
        cpp_code += f'        vertices.emplace_back({pos}, {normal}, {tex});\n'

    cpp_code += '\n        return Mesh(vertices, indices);\n    }\n};\n'
    return cpp_code


# Generate the corrected C++ code
cpp_code_corrected = generate_cpp_code_corrected(skydome_df)

# Write the C++ code to a .h file
cpp_code_corrected_path = 'data/output/GWSkyCylinder_Corrected_Adjusted.h'
with open(cpp_code_corrected_path, 'w') as file:
    file.write(cpp_code_corrected)
