#include "pch.h"
#include "draw_pathfinding_panel.h"
#include "draw_dat_browser.h"
#include "GuiGlobalConstants.h"
#include <commdlg.h>
#include <algorithm>
#include <cmath>

extern FFNA_MapFile selected_ffna_map_file;
extern FileType selected_file_type;

// Static instance of the visualizer
static PathfindingVisualizer s_pathfinding_visualizer;
static int s_last_map_file_index = -1;
extern int selected_map_file_index;

// Helper function for HSV to RGB conversion
RGBA PathfindingVisualizer::HsvToRgb(float h, float s, float v, uint8_t a) {
    float r, g, b;

    int i = static_cast<int>(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
        default: r = g = b = 0; break;
    }

    RGBA color;
    color.r = static_cast<uint8_t>(b * 255.0f);  // BGRA format
    color.g = static_cast<uint8_t>(g * 255.0f);
    color.b = static_cast<uint8_t>(r * 255.0f);
    color.a = a;
    return color;
}

void PathfindingVisualizer::DrawLine(int x0, int y0, int x1, int y1, RGBA color) {
    // Bresenham's line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x0 >= 0 && x0 < m_width && y0 >= 0 && y0 < m_height) {
            m_image_data[y0 * m_width + x0] = color;
        }

        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void PathfindingVisualizer::FillPolygon(const std::vector<std::pair<int, int>>& points, RGBA color) {
    if (points.size() < 3) return;

    // Find bounding box
    int min_y = INT_MAX, max_y = INT_MIN;
    for (const auto& p : points) {
        min_y = std::min(min_y, p.second);
        max_y = std::max(max_y, p.second);
    }

    min_y = std::max(0, min_y);
    max_y = std::min(m_height - 1, max_y);

    // Scanline fill
    for (int y = min_y; y <= max_y; ++y) {
        std::vector<int> intersections;

        for (size_t i = 0; i < points.size(); ++i) {
            size_t j = (i + 1) % points.size();
            int y0 = points[i].second;
            int y1 = points[j].second;
            int x0 = points[i].first;
            int x1 = points[j].first;

            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                float t = static_cast<float>(y - y0) / static_cast<float>(y1 - y0);
                int x = static_cast<int>(x0 + t * (x1 - x0));
                intersections.push_back(x);
            }
        }

        std::sort(intersections.begin(), intersections.end());

        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
            int x_start = std::max(0, intersections[i]);
            int x_end = std::min(m_width - 1, intersections[i + 1]);
            for (int x = x_start; x <= x_end; ++x) {
                m_image_data[y * m_width + x] = color;
            }
        }
    }
}

void PathfindingVisualizer::DrawTrapezoid(const PathfindingTrapezoid& trap,
                                          float min_x, float min_y, float scale_x, float scale_y,
                                          RGBA fill_color, RGBA outline_color) {
    // Transform coordinates to image space
    // Note: Y is flipped (max_y - y) for proper orientation
    auto transform = [&](float x, float y) -> std::pair<int, int> {
        int px = static_cast<int>((x - min_x) * scale_x);
        int py = m_height - 1 - static_cast<int>((y - min_y) * scale_y);  // Flip Y
        return {px, py};
    };

    // Get corners: bottom-left, bottom-right, top-right, top-left
    auto bl = transform(trap.xbl, trap.yb);
    auto br = transform(trap.xbr, trap.yb);
    auto tr = transform(trap.xtr, trap.yt);
    auto tl = transform(trap.xtl, trap.yt);

    // Fill the trapezoid
    std::vector<std::pair<int, int>> corners = {bl, br, tr, tl};
    FillPolygon(corners, fill_color);

    // Draw outline
    DrawLine(bl.first, bl.second, br.first, br.second, outline_color);
    DrawLine(br.first, br.second, tr.first, tr.second, outline_color);
    DrawLine(tr.first, tr.second, tl.first, tl.second, outline_color);
    DrawLine(tl.first, tl.second, bl.first, bl.second, outline_color);
}

void PathfindingVisualizer::GenerateImage(const PathfindingChunk& pathfinding_chunk, int image_size) {
    Clear();

    if (!pathfinding_chunk.valid || pathfinding_chunk.all_trapezoids.empty()) {
        return;
    }

    m_trapezoid_count = pathfinding_chunk.all_trapezoids.size();
    m_plane_count = pathfinding_chunk.plane_count;

    // Find bounds of all trapezoids
    float min_x = FLT_MAX, max_x = -FLT_MAX;
    float min_y = FLT_MAX, max_y = -FLT_MAX;

    for (const auto& trap : pathfinding_chunk.all_trapezoids) {
        min_x = std::min({min_x, trap.xtl, trap.xtr, trap.xbl, trap.xbr});
        max_x = std::max({max_x, trap.xtl, trap.xtr, trap.xbl, trap.xbr});
        min_y = std::min({min_y, trap.yt, trap.yb});
        max_y = std::max({max_y, trap.yt, trap.yb});
    }

    // Add padding
    float padding = 0.05f;
    float width = max_x - min_x;
    float height = max_y - min_y;

    if (width <= 0 || height <= 0) return;

    min_x -= width * padding;
    max_x += width * padding;
    min_y -= height * padding;
    max_y += height * padding;

    width = max_x - min_x;
    height = max_y - min_y;

    // Calculate image dimensions maintaining aspect ratio
    float scale = std::min(static_cast<float>(image_size) / width,
                          static_cast<float>(image_size) / height);
    m_width = static_cast<int>(width * scale);
    m_height = static_cast<int>(height * scale);

    if (m_width <= 0 || m_height <= 0) return;

    // Initialize image with dark background
    m_image_data.resize(m_width * m_height);
    RGBA bg_color = {30, 20, 20, 255};  // Dark background (BGRA)
    std::fill(m_image_data.begin(), m_image_data.end(), bg_color);

    // Calculate scale factors for coordinate transformation
    float scale_x = static_cast<float>(m_width - 1) / width;
    float scale_y = static_cast<float>(m_height - 1) / height;

    // Draw each trapezoid with golden ratio coloring
    const float golden_ratio = 0.618033988749895f;
    for (size_t idx = 0; idx < pathfinding_chunk.all_trapezoids.size(); ++idx) {
        float hue = fmod(idx * golden_ratio, 1.0f);
        RGBA fill_color = HsvToRgb(hue, 0.6f, 0.8f, 120);  // Semi-transparent fill
        RGBA outline_color = HsvToRgb(hue, 0.6f, 0.8f, 255);  // Solid outline

        DrawTrapezoid(pathfinding_chunk.all_trapezoids[idx],
                     min_x, min_y, scale_x, scale_y,
                     fill_color, outline_color);
    }

    m_image_ready = true;
}

int PathfindingVisualizer::CreateTexture(TextureManager* texture_manager) {
    if (!m_image_ready || m_image_data.empty()) {
        return -1;
    }

    // Remove old texture if exists
    if (m_texture_id >= 0) {
        texture_manager->RemoveTexture(m_texture_id);
        m_texture_id = -1;
    }

    // Create new texture (-1 for file_hash since this is generated)
    HRESULT hr = texture_manager->CreateTextureFromRGBA(
        m_width, m_height, m_image_data.data(), &m_texture_id, -1);

    return SUCCEEDED(hr) ? m_texture_id : -1;
}

void PathfindingVisualizer::Clear() {
    m_image_data.clear();
    m_width = 0;
    m_height = 0;
    m_image_ready = false;
    m_trapezoid_count = 0;
    m_plane_count = 0;
    // Note: texture_id is not cleared here - call RemoveTexture separately if needed
}

// File dialog for saving PNG
std::wstring OpenSaveFileDialog(const std::wstring& default_name, const std::wstring& extension) {
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = {0};
    wcscpy_s(szFile, default_name.c_str());

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);

    std::wstring filter = L"PNG Files\0*.png\0All Files\0*.*\0";
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = extension.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn)) {
        return std::wstring(szFile);
    }
    return L"";
}

void draw_pathfinding_panel(MapRenderer* map_renderer) {
    // Only show for map files
    if (selected_file_type != FFNA_Type3) {
        return;
    }

    if (!GuiGlobalConstants::is_pathfinding_panel_open) {
        return;
    }

    // Check if we need to regenerate the visualization
    if (selected_map_file_index != s_last_map_file_index) {
        s_last_map_file_index = selected_map_file_index;

        // Generate new visualization
        if (selected_ffna_map_file.pathfinding_chunk.valid) {
            s_pathfinding_visualizer.GenerateImage(selected_ffna_map_file.pathfinding_chunk, 1024);
            s_pathfinding_visualizer.CreateTexture(map_renderer->GetTextureManager());
        } else {
            s_pathfinding_visualizer.Clear();
        }
    }

    const float min_width = 400;
    const float min_height = 450;
    ImGui::SetNextWindowSizeConstraints(ImVec2(min_width, min_height), ImVec2(FLT_MAX, FLT_MAX));

    if (ImGui::Begin("Pathfinding Map", &GuiGlobalConstants::is_pathfinding_panel_open, ImGuiWindowFlags_NoFocusOnAppearing)) {
        const auto& pf = selected_ffna_map_file.pathfinding_chunk;

        if (!pf.valid) {
            ImGui::Text("No pathfinding data available for this map.");
            ImGui::End();
            return;
        }

        // Display info
        ImGui::Text("Planes: %u", pf.plane_count);
        ImGui::SameLine();
        ImGui::Text("  Trapezoids: %zu", pf.all_trapezoids.size());

        ImGui::Separator();

        // Display the visualization
        int tex_id = s_pathfinding_visualizer.GetTextureId();
        if (tex_id >= 0 && s_pathfinding_visualizer.IsReady()) {
            ID3D11ShaderResourceView* texture = map_renderer->GetTextureManager()->GetTexture(tex_id);
            if (texture) {
                // Calculate scaling to fit window
                ImVec2 window_size = ImGui::GetContentRegionAvail();
                float img_width = static_cast<float>(s_pathfinding_visualizer.GetWidth());
                float img_height = static_cast<float>(s_pathfinding_visualizer.GetHeight());

                float scale_x = (window_size.x - 20) / img_width;
                float scale_y = (window_size.y - 60) / img_height;
                float scale = std::min(scale_x, scale_y);
                scale = std::max(0.1f, scale);  // Minimum scale

                ImVec2 scaled_size(img_width * scale, img_height * scale);
                ImGui::Image((ImTextureID)texture, scaled_size);
            }
        } else {
            ImGui::Text("Generating visualization...");
        }

        ImGui::Separator();

        // Export button
        if (ImGui::Button("Export as PNG")) {
            if (s_pathfinding_visualizer.IsReady()) {
                std::wstring default_name = std::format(L"pathfinding_map_{}", selected_map_file_index);
                std::wstring save_path = OpenSaveFileDialog(default_name, L"png");

                if (!save_path.empty()) {
                    // Get texture and save
                    int tex_id = s_pathfinding_visualizer.GetTextureId();
                    if (tex_id >= 0) {
                        ID3D11ShaderResourceView* texture = map_renderer->GetTextureManager()->GetTexture(tex_id);
                        if (texture && SaveTextureToPng(texture, save_path, map_renderer->GetTextureManager())) {
                            // Success - could show a message
                        }
                    }
                }
            }
        }

        // Show individual plane info in a collapsible section
        if (ImGui::CollapsingHeader("Plane Details")) {
            for (size_t i = 0; i < pf.planes.size(); ++i) {
                ImGui::Text("Plane %zu: %u trapezoids", i, pf.planes[i].traps_count);
            }
        }
    }
    ImGui::End();
}
