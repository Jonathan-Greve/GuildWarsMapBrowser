#pragma once
#include "MapBrowser.h"

void draw_ui(InitializationState initialization_state, int dat_files_to_read, int dat_total_files,
             DATManager& dat_manager, MapRenderer* map_renderer);
