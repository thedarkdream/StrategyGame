#pragma once

namespace Constants {
    // Window settings (initial size, window is resizable)
    constexpr int WINDOW_WIDTH = 1280;
    constexpr int WINDOW_HEIGHT = 720;
    constexpr int FRAME_RATE = 60;
    
    // Reference resolution for scaling
    // All coordinates are in "game units" (1 tile = 32 game units)
    // The visible area is always BASE_HEIGHT game units tall
    // At 720p: 1 game unit = 1 pixel
    // At 1080p: 1 game unit = 1.5 pixels (scaled up)
    constexpr float BASE_WIDTH = 1280.0f;
    constexpr float BASE_HEIGHT = 720.0f;
    
    // Map settings
    constexpr int TILE_SIZE = 32;
    constexpr int MAP_WIDTH = 64;   // tiles
    constexpr int MAP_HEIGHT = 64;  // tiles
    
    // Camera settings
    constexpr float CAMERA_SPEED = 500.0f;  // pixels per second
    constexpr float CAMERA_EDGE_MARGIN = 20.0f;
    
    // Minimap settings
    constexpr float MINIMAP_SIZE    = 240.0f;  // 150 * 1.6
    constexpr float MINIMAP_PADDING = 10.0f;
    
    // Action bar settings (fixed size)
    constexpr float ACTION_BAR_WIDTH = 280.0f;
    constexpr float ACTION_BAR_HEIGHT = 180.0f;
    constexpr float ACTION_BAR_BUTTON_SIZE = 50.0f;
    constexpr float ACTION_BAR_BUTTON_SPACING = 5.0f;
    constexpr float ACTION_BAR_PADDING = 10.0f;
    constexpr int ACTION_BAR_MAX_BUTTONS = 4;  // Max buttons in a row
    
    // Game balance
    constexpr int STARTING_MINERALS = 500;
    constexpr int STARTING_GAS = 0;
    
    // Resource gathering
    constexpr int MINERALS_PER_TRIP = 8;
    constexpr float GATHERING_TIME = 2.0f;

    // Colors (RGBA) — used by ResourceNode for mineral/gas tints
    namespace Colors {
        constexpr unsigned int MINERAL_COLOR = 0x00CED1FF; // Cyan
        constexpr unsigned int GAS_COLOR = 0x00FF00FF;     // Green
    }
}
