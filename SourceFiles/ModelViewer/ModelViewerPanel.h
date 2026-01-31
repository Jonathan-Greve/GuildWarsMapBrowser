#pragma once

#include <map>
#include <memory>

// Forward declarations
class DATManager;
class MapRenderer;

/**
 * @brief Draws the model viewer control panel.
 *
 * Provides controls for:
 * - View toggles (mesh, wireframe, bones, labels)
 * - Animation playback controls (play/pause, speed, looping)
 * - Bone list with selection
 * - Selected bone info (index, parent, position, vertex count)
 * - Background color picker
 * - Camera controls
 * - Context menu on animation search results for loading and saving data
 *
 * @param mapRenderer The MapRenderer for visualization control
 * @param dat_managers Map of DAT managers for animation loading
 */
void draw_model_viewer_panel(MapRenderer* mapRenderer, std::map<int, std::unique_ptr<DATManager>>& dat_managers);
