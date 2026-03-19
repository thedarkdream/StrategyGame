#pragma once

namespace Constants {

    // =========================================================================
    // Window
    // =========================================================================
    namespace Window {
        constexpr int   WIDTH      = 1280;
        constexpr int   HEIGHT     = 720;
        constexpr int   FRAME_RATE = 60;

        // Reference resolution for UI / camera scaling.
        // At 720p: 1 game unit = 1 pixel.
        constexpr float BASE_WIDTH  = 1280.0f;
        constexpr float BASE_HEIGHT = 720.0f;
    }

    // =========================================================================
    // Map
    // =========================================================================
    namespace Map {
        constexpr int TILE_SIZE  = 32;
        constexpr int MAP_WIDTH  = 64;   // tiles
        constexpr int MAP_HEIGHT = 64;   // tiles
    }

    // =========================================================================
    // UI
    // =========================================================================
    namespace UI {
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
    }

    // =========================================================================
    // Balance
    // =========================================================================
    namespace Balance {
        constexpr int   STARTING_MINERALS = 500;
        constexpr int   STARTING_GAS      = 0;
        constexpr int   MINERALS_PER_TRIP = 8;
        constexpr float GATHERING_TIME    = 2.0f;
    }

    // =========================================================================
    // Colors (RGBA)
    // =========================================================================
    namespace Colors {
        constexpr unsigned int MINERAL_COLOR = 0x00CED1FF; // Cyan
        constexpr unsigned int GAS_COLOR     = 0x00FF00FF; // Green
    }

    // =========================================================================
    // Backward-compatible aliases (flat Constants:: names)
    // Prefer the sub-namespaced versions in new code.
    // =========================================================================

    // Window
    constexpr int   WINDOW_WIDTH  = Window::WIDTH;
    constexpr int   WINDOW_HEIGHT = Window::HEIGHT;
    constexpr int   FRAME_RATE    = Window::FRAME_RATE;
    constexpr float BASE_WIDTH    = Window::BASE_WIDTH;
    constexpr float BASE_HEIGHT   = Window::BASE_HEIGHT;

    // Map
    constexpr int TILE_SIZE  = Map::TILE_SIZE;
    constexpr int MAP_WIDTH  = Map::MAP_WIDTH;
    constexpr int MAP_HEIGHT = Map::MAP_HEIGHT;

    // UI
    constexpr float CAMERA_SPEED             = UI::CAMERA_SPEED;
    constexpr float CAMERA_EDGE_MARGIN       = UI::CAMERA_EDGE_MARGIN;
    constexpr float MINIMAP_SIZE             = UI::MINIMAP_SIZE;
    constexpr float MINIMAP_PADDING          = UI::MINIMAP_PADDING;
    constexpr float ACTION_BAR_WIDTH         = UI::ACTION_BAR_WIDTH;
    constexpr float ACTION_BAR_HEIGHT        = UI::ACTION_BAR_HEIGHT;
    constexpr float ACTION_BAR_BUTTON_SIZE   = UI::ACTION_BAR_BUTTON_SIZE;
    constexpr float ACTION_BAR_BUTTON_SPACING = UI::ACTION_BAR_BUTTON_SPACING;
    constexpr float ACTION_BAR_PADDING       = UI::ACTION_BAR_PADDING;
    constexpr int   ACTION_BAR_MAX_BUTTONS   = UI::ACTION_BAR_MAX_BUTTONS;

    // Balance
    constexpr int   STARTING_MINERALS = Balance::STARTING_MINERALS;
    constexpr int   STARTING_GAS      = Balance::STARTING_GAS;
    constexpr int   MINERALS_PER_TRIP = Balance::MINERALS_PER_TRIP;
    constexpr float GATHERING_TIME    = Balance::GATHERING_TIME;

} // namespace Constants
