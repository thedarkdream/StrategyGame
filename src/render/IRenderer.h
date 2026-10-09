#pragma once

// ---------------------------------------------------------------------------
// IRenderer — backend-agnostic entry point used by GameScreen.
//
// GameScreen owns a std::unique_ptr<IRenderer> and never touches a concrete
// backend.  Renderer2D is the SFML implementation, Renderer3D the OpenGL one;
// F9 switches between them at runtime.
//
// Backend-neutral input: the camera is a plain Camera value (see Camera.h).
// ---------------------------------------------------------------------------

#include "render/Camera.h"
#include "core/Types.h"
#include <SFML/System/Vector2.hpp>
#include <vector>

class Game;
class Map;
class InputHandler;
class ActionBar;

// Presentation state a frame needs besides the simulation (Game): what the player
// is currently doing (selection rectangle, build mode, ...) and the HUD widget.
struct FrameContext {
    InputHandler& input;
    ActionBar&    actionBar;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void setCamera(const Camera& camera) = 0;

    // Draws the whole game frame (world + HUD).
    virtual void render(Game& game, FrameContext& frame) = 0;

    // Call whenever the map tiles change so cached minimap terrain is re-baked.
    virtual void invalidateMinimapTerrain() = 0;

    // --- Picking -----------------------------------------------------------
    // How the world is projected depends on the backend, so mouse -> world and
    // mouse -> entity queries live here.  The camera is passed explicitly so the
    // caller always picks against the camera it is actually moving.

    // Ground position (world units) under a window pixel.
    virtual sf::Vector2f screenToWorld(const Camera& camera, sf::Vector2i pixel,
                                       const Map& map) const = 0;

    // Entity under a window pixel, or nullptr.
    virtual EntityPtr pickEntity(const Camera& camera, sf::Vector2i pixel, Game& game) const = 0;

    // Entities of `team` inside the rubber-band spanned by two window pixels.
    virtual std::vector<EntityPtr> pickEntitiesInRect(const Camera& camera, sf::Vector2i cornerA,
                                                      sf::Vector2i cornerB, Team team,
                                                      Game& game) const = 0;
};
