#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <optional>

// Result returned by a screen to signal transitions
struct ScreenResult {
    enum class Action {
        None,       // Stay on this screen
        StartGame,  // Transition to gameplay
        OpenEditor, // Transition to map editor
        BackToMenu, // Return to main menu
        Quit        // Close the application
    };
    
    Action action = Action::None;
    std::string mapFile;        // For StartGame: which map to load
    int localPlayerSlot = 0;    // For StartGame: which team slot (0=Player1, 1=Player2…) the human controls
};

// Abstract base for all screens (menu, game, editor, etc.)
class Screen {
public:
    virtual ~Screen() = default;
    
    // Process a single SFML event. Return a result to trigger transition.
    virtual ScreenResult handleEvent(const sf::Event& event) = 0;
    
    // Per-frame update
    virtual ScreenResult update(float deltaTime) = 0;
    
    // Render to the window
    virtual void render(sf::RenderWindow& window) = 0;
    
    // Called when this screen becomes active (e.g. returning from another screen)
    virtual void onEnter() {}
    
    // Called when this screen is about to be replaced
    virtual void onExit() {}
};
