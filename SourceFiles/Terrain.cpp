#include "pch.h"
#include "Terrain.h"
#include <algorithm>
#include <map>

static const float ATLAS_SIZE = 2048.0f;
static const float TILE_SIZE = 256.0f;
static const float QUADRANT_SIZE = 128.0f;
static const float BORDER = 8.5f;

// Key: corner mask (TL=1, TR=2, BL=4, BR=8)
// Value: pair(primary_encoded, secondary_encoded)
static const std::pair<uint16_t, int> VARIANT_LOOKUP[16] = {
    {0x8000, 0x0000},   // 0000 - all same corners
    {0x8003, -1},       // 0001
    {0x0001, -1},       // 0010
    {0x8000, -1},       // 0011
    {0x8001, -1},       // 0100
    {0x0002, -1},       // 0101
    {0x8001, 0x0001},   // 0110
    {0x0002, 0x0001},   // 0111
    {0x0003, -1},       // 1000
    {0x8003, 0x0003},   // 1001
    {0x8002, -1},       // 1010
    {0x8000, 0x0003},   // 1011
    {0x0000, -1},       // 1100
    {0x0000, 0x8003},   // 1101
    {0x0000, 0x0001},   // 1110
    {0x8002, 0x0002},   // 1111
};

static const float OFFSET_X[4] = { 0.0f, 128.0f, 0.0f, 128.0f };
static const float OFFSET_Y[4] = { 0.0f, 0.0f, 128.0f, 128.0f };

static uint32_t prng_next(uint32_t& state) {
    uint32_t mult = (48271 * state) & 0xFFFFFFFF;
    uint32_t quot = state / 44488;
    uint32_t correction = (0x7FFFFFFF * quot) & 0xFFFFFFFF;
    uint32_t v2 = (mult - correction) & 0xFFFFFFFF;

    if (v2 > 0x7FFFFFFF) {
        v2 = (v2 + 0x80000000) & 0xFFFFFFFF;
    }
    if (v2 == 0) {
        v2 = 123459876;
    }
    state = v2;
    return v2;
}

static XMFLOAT2 calculate_corner_uv(int tex_idx, int quadrant, bool rotated, int corner_in_quad) {
    int atlas_idx = 0;
    if (tex_idx == -1) {
        atlas_idx = 0;
    } else {
        atlas_idx = tex_idx + 1;
    }

    int tile_col = atlas_idx % 8;
    int tile_row = atlas_idx / 8;
    float tile_x = tile_col * TILE_SIZE;
    float tile_y = tile_row * TILE_SIZE;

    float quad_off_x = OFFSET_X[quadrant];
    float quad_off_y = OFFSET_Y[quadrant];

    int effective_corner = rotated ? (3 - corner_in_quad) : corner_in_quad;
    
    int cx = effective_corner % 2; // 0=left, 1=right
    int cy = effective_corner / 2; // 0=top, 1=bottom

    float local_x = (cx == 0) ? BORDER : (QUADRANT_SIZE - BORDER);
    float local_y = (cy == 0) ? BORDER : (QUADRANT_SIZE - BORDER);

    float pixel_x = tile_x + quad_off_x + local_x;
    float pixel_y = tile_y + quad_off_y + local_y;

    return XMFLOAT2(pixel_x / ATLAS_SIZE, pixel_y / ATLAS_SIZE);
}

static XMFLOAT2 make_neutral_uv() {
    return calculate_corner_uv(-1, 3, false, 0);
}

float Terrain::get_height_at(float world_x, float world_z) const
{
    float grid_x = (world_x - m_bounds.map_min_x) / (m_bounds.map_max_x - m_bounds.map_min_x) * m_grid_dim_x;
    float grid_z = (world_z - m_bounds.map_min_z) / (m_bounds.map_max_z - m_bounds.map_min_z) * m_grid_dim_z;

    int cell_x = static_cast<int>(grid_x);
    int cell_z = static_cast<int>(grid_z);

    if (cell_x >= m_grid_dim_x - 1) cell_x = m_grid_dim_x - 2;
    if (cell_z >= m_grid_dim_z - 1) cell_z = m_grid_dim_z - 2;

    float dx = grid_x - cell_x;
    float dz = grid_z - cell_z;

    float h00 = grid[cell_z][cell_x];     
    float h10 = grid[cell_z][cell_x + 1]; 
    float h01 = grid[cell_z + 1][cell_x]; 
    float h11 = grid[cell_z + 1][cell_x + 1]; 

    float height = h00 * (1 - dx) * (1 - dz) +
        h10 * dx * (1 - dz) +
        h01 * (1 - dx) * dz +
        h11 * dx * dz;

    return height;
}

Mesh Terrain::GenerateTerrainMesh()
{
    // 1. Populate Grids
    uint32_t grid_dims = 32;
    uint32_t sub_grid_rows = m_grid_dim_z / grid_dims;
    uint32_t sub_grid_cols = m_grid_dim_x / grid_dims;

    m_texture_index_grid.resize(m_grid_dim_z + 1, std::vector<uint32_t>(m_grid_dim_x + 1, 0));
    m_terrain_shadow_map_grid.resize(m_grid_dim_z + 1, std::vector<uint32_t>(m_grid_dim_x + 1, 0));

    float min_h = FLT_MAX;
    float max_h = FLT_MIN;

    int count = 0;
    for (int j = 0; j < sub_grid_rows; j++)
    {
        for (int i = 0; i < sub_grid_cols; i++)
        {
            int col_start = i * grid_dims;
            int col_end = col_start + grid_dims;
            int row_start = j * grid_dims;
            int row_end = row_start + grid_dims;

            for (int k = row_start; k < row_end; k++)
            {
                for (int l = col_start; l < col_end; l++)
                {
                    // FLIP STORAGE: Map Top-Down File Data to Bottom-Up Grid Index
                    int grid_row_idx = m_grid_dim_z - k;
                    
                    grid[grid_row_idx][l] = -m_height_map[count];
                    m_texture_index_grid[grid_row_idx][l] = m_terrain_texture_indices[count];
                    m_terrain_shadow_map_grid[grid_row_idx][l] = m_terrain_shadow_map[count];
                    
                    float h = grid[grid_row_idx][l];
                    if (h < min_h) min_h = h;
                    if (h > max_h) max_h = h;
                    
                    count++;
                }
            }
        }
    }

    m_bounds.map_max_y = max_h;
    m_bounds.map_min_y = min_h;

    float delta_x = (m_bounds.map_max_x - m_bounds.map_min_x) / m_grid_dim_x;
    float delta_z = (m_bounds.map_max_z - m_bounds.map_min_z) / m_grid_dim_z;

    // 2. Pre-calculate Normals
    std::vector<XMFLOAT3> grid_normals((m_grid_dim_z + 1) * (m_grid_dim_x + 1), XMFLOAT3(0, 1, 0));
    
    for (uint32_t z = 0; z < m_grid_dim_z - 1; z++) {
        for (uint32_t x = 0; x < m_grid_dim_x - 1; x++) {
            float y00 = grid[z][x];
            float y10 = grid[z][x + 1];
            float y01 = grid[z + 1][x];
            
            XMFLOAT3 p00(m_bounds.map_min_x + x * delta_x, y00, m_bounds.map_min_z + z * delta_z);
            XMFLOAT3 p10(m_bounds.map_min_x + (x+1) * delta_x, y10, m_bounds.map_min_z + z * delta_z);
            XMFLOAT3 p01(m_bounds.map_min_x + x * delta_x, y01, m_bounds.map_min_z + (z+1) * delta_z);
            
            XMFLOAT3 n = compute_normal(p00, p10, p01); 
            
            int idx00 = z * (m_grid_dim_x + 1) + x;
            int idx10 = z * (m_grid_dim_x + 1) + x + 1;
            int idx01 = (z + 1) * (m_grid_dim_x + 1) + x;
            int idx11 = (z + 1) * (m_grid_dim_x + 1) + x + 1;

            grid_normals[idx00] = AddXMFLOAT3(grid_normals[idx00], n);
            grid_normals[idx10] = AddXMFLOAT3(grid_normals[idx10], n);
            grid_normals[idx01] = AddXMFLOAT3(grid_normals[idx01], n);
            grid_normals[idx11] = AddXMFLOAT3(grid_normals[idx11], n); 
        }
    }
    for(auto& n : grid_normals) n = NormalizeXMFLOAT3(n);

    // 3. Generate Mesh (Quads)
    std::vector<GWVertex> vertices;
    std::vector<uint32_t> indices;
    
    vertices.reserve((m_grid_dim_x - 1) * (m_grid_dim_z - 1) * 4);
    indices.reserve((m_grid_dim_x - 1) * (m_grid_dim_z - 1) * 6);

    int chunks_in_x = (m_grid_dim_x - 1 + 31) / 32;
    int chunks_in_z = (m_grid_dim_z - 1 + 31) / 32;
    
    // Process chunks Top-Down (PRNG Order)
    for (int cz = 0; cz < chunks_in_z; cz++) {
        for (int cx = 0; cx < chunks_in_x; cx++) {
            
            uint32_t seed_cx = cx;
            uint32_t seed_cz = cz;
            uint32_t seed = seed_cz ^ (seed_cx << 16);
            uint32_t prng_state = seed;
            
            for (int lz = 0; lz < 32; lz++) {
                for (int lx = 0; lx < 32; lx++) {
                    uint32_t rnd = prng_next(prng_state);
                    
                    int grid_x = cx * 32 + lx;
                    // Invert Z Index: Map Chunk (Top-Down) to Grid (Bottom-Up)
                    // tex_tl is read from grid_z+1, which should map to file row (cz*32+lz)
                    // File row F is stored at grid index (m_grid_dim_z - F)
                    // So grid_z+1 = m_grid_dim_z - (cz*32+lz), thus grid_z = m_grid_dim_z - 1 - cz*32 - lz
                    int grid_z = (m_grid_dim_z - 1) - (cz * 32 + lz);
                    
                    if (grid_x >= (int)m_grid_dim_x - 1 || grid_z < 0) {
                        continue; 
                    }

                    int tex_bl = m_texture_index_grid[grid_z][grid_x];
                    int tex_br = m_texture_index_grid[grid_z][grid_x + 1];
                    int tex_tl = m_texture_index_grid[grid_z + 1][grid_x];
                    int tex_tr = m_texture_index_grid[grid_z + 1][grid_x + 1];
                    
                    int prng_quadrant = rnd & 3;
                    
                    bool is_uniform = (tex_tl == tex_tr && tex_tl == tex_bl && tex_tl == tex_br);
                    
                    auto get_uvs_for_corner = [&](int corner) -> std::vector<XMFLOAT2> {
                        std::vector<XMFLOAT2> layers;

                        if (is_uniform) {
                            layers.push_back(calculate_corner_uv(tex_tl, prng_quadrant, false, corner));
                            layers.push_back(make_neutral_uv());
                            layers.push_back(make_neutral_uv());
                            return layers;
                        }

                        // Build per-texture corner masks (matching Python's tex_corners)
                        std::map<int, int> tex_to_corners;
                        tex_to_corners[tex_tl] |= 1;  // TL = bit 0
                        tex_to_corners[tex_tr] |= 2;  // TR = bit 1
                        tex_to_corners[tex_bl] |= 4;  // BL = bit 2
                        tex_to_corners[tex_br] |= 8;  // BR = bit 3

                        // std::map iterates by key order (sorted texture indices)
                        std::vector<std::pair<int, int>> tex_list;
                        for (auto const& [tex, mask] : tex_to_corners) {
                            tex_list.push_back({tex, mask});
                        }

                        // Primary variants loop (matching Python exactly)
                        for (size_t i = 0; i < tex_list.size(); i++) {
                            int tex = tex_list[i].first;
                            int mask = tex_list[i].second;

                            auto entry = VARIANT_LOOKUP[mask];
                            uint16_t primary = entry.first;

                            if (i == 0) {
                                // First texture uses random quadrant
                                layers.push_back(calculate_corner_uv(tex, prng_quadrant, false, corner));
                            } else {
                                // Other textures use LUT quadrant with rotation
                                int quad = primary & 0x3;
                                bool rot = (primary & 0x8000) != 0;
                                layers.push_back(calculate_corner_uv(tex, quad, rot, corner));
                            }
                        }

                        // Add secondary variant ONLY for 2-texture case (after loop, matching Python)
                        if (tex_list.size() == 2) {
                            int tex2 = tex_list[1].first;
                            int mask2 = tex_list[1].second;
                            int secondary = VARIANT_LOOKUP[mask2].second;

                            if (secondary != -1) {
                                int quad2 = secondary & 0x3;
                                bool rot2 = (secondary & 0x8000) != 0;
                                layers.push_back(calculate_corner_uv(tex2, quad2, rot2, corner));
                            } else {
                                layers.push_back(make_neutral_uv());
                            }
                        }

                        // Pad to 3 variants
                        while (layers.size() < 3) {
                            layers.push_back(make_neutral_uv());
                        }

                        return layers;
                    }; 

                    float xPos = m_bounds.map_min_x + grid_x * delta_x;
                    float zPos = m_bounds.map_min_z + grid_z * delta_z;
                    float xL = xPos;
                    float xR = xPos + delta_x;
                    float zB = zPos;
                    float zT = zPos + delta_z;
                    
                    auto uvs_TL = get_uvs_for_corner(0);
                    GWVertex vTL;
                    vTL.position = { xL, grid[grid_z+1][grid_x], zT };
                    vTL.normal = grid_normals[(grid_z+1)*(m_grid_dim_x+1) + grid_x];
                    vTL.tex_coord0 = uvs_TL[0];
                    vTL.tex_coord1 = uvs_TL[1];
                    vTL.tex_coord2 = uvs_TL[2];
                    vTL.tex_coord3 = {(float)lx/32.0f, (float)lz/32.0f}; 
                    
                    auto uvs_TR = get_uvs_for_corner(1);
                    GWVertex vTR;
                    vTR.position = { xR, grid[grid_z+1][grid_x+1], zT };
                    vTR.normal = grid_normals[(grid_z+1)*(m_grid_dim_x+1) + grid_x+1];
                    vTR.tex_coord0 = uvs_TR[0];
                    vTR.tex_coord1 = uvs_TR[1];
                    vTR.tex_coord2 = uvs_TR[2];
                    vTR.tex_coord3 = {(float)(lx+1)/32.0f, (float)lz/32.0f};

                    auto uvs_BL = get_uvs_for_corner(2);
                    GWVertex vBL;
                    vBL.position = { xL, grid[grid_z][grid_x], zB };
                    vBL.normal = grid_normals[grid_z*(m_grid_dim_x+1) + grid_x];
                    vBL.tex_coord0 = uvs_BL[0];
                    vBL.tex_coord1 = uvs_BL[1];
                    vBL.tex_coord2 = uvs_BL[2];
                    vBL.tex_coord3 = {(float)lx/32.0f, (float)(lz+1)/32.0f};

                    auto uvs_BR = get_uvs_for_corner(3);
                    GWVertex vBR;
                    vBR.position = { xR, grid[grid_z][grid_x+1], zB };
                    vBR.normal = grid_normals[grid_z*(m_grid_dim_x+1) + grid_x+1];
                    vBR.tex_coord0 = uvs_BR[0];
                    vBR.tex_coord1 = uvs_BR[1];
                    vBR.tex_coord2 = uvs_BR[2];
                    vBR.tex_coord3 = {(float)(lx+1)/32.0f, (float)(lz+1)/32.0f};
                    
                    uint32_t base_idx = (uint32_t)vertices.size();
                    vertices.push_back(vTL);
                    vertices.push_back(vTR);
                    vertices.push_back(vBL);
                    vertices.push_back(vBR);
                    
                    indices.push_back(base_idx + 2);
                    indices.push_back(base_idx + 0);
                    indices.push_back(base_idx + 1);
                    
                    indices.push_back(base_idx + 2);
                    indices.push_back(base_idx + 1);
                    indices.push_back(base_idx + 3);
                }
            }
        }
    }
    
    m_per_terrain_cb = PerTerrainCB(m_grid_dim_x, m_grid_dim_z, m_bounds.map_min_x, m_bounds.map_max_x, m_bounds.map_min_y, m_bounds.map_max_y, m_bounds.map_min_z, m_bounds.map_max_z, 0, 0.03, 0.03, {0});

    return Mesh(vertices, indices, {}, {}, {0}, { 0 }, { 0 }, { 0 }, true, BlendState::Opaque, 1, { 10000000, 10000000, 10000000 });
}
