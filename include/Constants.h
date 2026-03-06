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
    constexpr float MINIMAP_SIZE = 150.0f;
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
    
    // Unit costs
    constexpr int WORKER_COST_MINERALS = 50;
    constexpr int SOLDIER_COST_MINERALS = 75;
    constexpr int BRUTE_COST_MINERALS = 90;
    
    // Building costs
    constexpr int BASE_COST_MINERALS = 400;
    constexpr int BARRACKS_COST_MINERALS = 150;
    constexpr int REFINERY_COST_MINERALS = 100;
    
    // Unit stats
    constexpr float WORKER_SPEED = 100.0f;
    constexpr float SOLDIER_SPEED = 80.0f;
    constexpr float BRUTE_SPEED = 60.0f;
    constexpr int WORKER_HEALTH = 40;
    constexpr int SOLDIER_HEALTH = 100;
    constexpr int BRUTE_HEALTH = 120;
    constexpr int SOLDIER_DAMAGE = 10;
    constexpr int BRUTE_DAMAGE = 12;
    constexpr float SOLDIER_ATTACK_RANGE = 150.0f;
    constexpr float BRUTE_ATTACK_RANGE = 30.0f;
    constexpr float SOLDIER_ATTACK_COOLDOWN = 1.0f;
    constexpr float BRUTE_ATTACK_COOLDOWN = 0.8f;
    
    // Resource gathering
    constexpr int MINERALS_PER_TRIP = 8;
    constexpr float GATHERING_TIME = 2.0f;
    
    // Colors (RGBA)
    namespace Colors {
        constexpr unsigned int PLAYER_COLOR = 0x3498DBFF;  // Blue
        constexpr unsigned int ENEMY_COLOR = 0xE74C3CFF;   // Red
        constexpr unsigned int NEUTRAL_COLOR = 0x95A5A6FF; // Gray
        constexpr unsigned int MINERAL_COLOR = 0x00CED1FF; // Cyan
        constexpr unsigned int GAS_COLOR = 0x00FF00FF;     // Green
    }
}
