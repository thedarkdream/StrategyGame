#pragma once

namespace Constants {
    // Window
    constexpr int   WINDOW_WIDTH  = 1280;
    constexpr int   WINDOW_HEIGHT = 720;
    constexpr int   FRAME_RATE    = 60;
    constexpr float BASE_WIDTH    = 1280.0f;  // reference resolution for scaling
    constexpr float BASE_HEIGHT   = 720.0f;

    // Map
    constexpr int TILE_SIZE  = 32;
    constexpr int MAP_WIDTH  = 64;  // tiles
    constexpr int MAP_HEIGHT = 64;  // tiles

    // Camera
    constexpr float CAMERA_SPEED       = 500.0f;  // pixels per second
    constexpr float CAMERA_EDGE_MARGIN = 20.0f;

    // Minimap
    constexpr float MINIMAP_SIZE    = 240.0f;
    constexpr float MINIMAP_PADDING = 10.0f;

    // Action bar
    constexpr float ACTION_BAR_WIDTH          = 280.0f;
    constexpr float ACTION_BAR_HEIGHT         = 180.0f;
    constexpr float ACTION_BAR_BUTTON_SIZE    = 50.0f;
    constexpr float ACTION_BAR_BUTTON_SPACING = 5.0f;
    constexpr float ACTION_BAR_PADDING        = 10.0f;
    constexpr int   ACTION_BAR_MAX_BUTTONS    = 4;

    // Balance
    constexpr int   STARTING_MINERALS = 500;
    constexpr int   STARTING_GAS      = 0;
    constexpr int   MINERALS_PER_TRIP = 8;
    constexpr float GATHERING_TIME    = 2.0f;

    // Colors (RGBA)
    namespace Colors {
        constexpr unsigned int MINERAL_COLOR = 0x00CED1FF;
        constexpr unsigned int GAS_COLOR     = 0x00FF00FF;
    }
} // namespace Constants
