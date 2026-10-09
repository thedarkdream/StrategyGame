#pragma once

#include "core/Types.h"
#include "render/IRenderer.h"
#include "render2d/TerrainRenderer.h"
#include "ui/Minimap.h"
#include <SFML/Graphics.hpp>
class Game;
class Map;
class Player;
class InputHandler;

// SFML (2D) implementation of IRenderer.
class Renderer2D : public IRenderer {
public:
    Renderer2D(sf::RenderWindow& window);
    
    void render(Game& game) override;
    void setCamera(const Camera& camera) override { m_camera = camera; }

    // Call whenever the map tiles change so the minimap terrain is re-baked
    void invalidateMinimapTerrain() override { m_minimap.invalidate(); }

    sf::Vector2f screenToWorld(const Camera& camera, sf::Vector2i pixel, const Map& map) const override;
    EntityPtr pickEntity(const Camera& camera, sf::Vector2i pixel, Game& game) const override;
    std::vector<EntityPtr> pickEntitiesInRect(const Camera& camera, sf::Vector2i cornerA,
                                              sf::Vector2i cornerB, Team team,
                                              Game& game) const override;

    // Draws only the screen-space HUD (resource bar, minimap, action bar, unit
    // panel).  Shared with Renderer3D, which draws its world through OpenGL.
    void renderHud(Game& game);
    
private:
    sf::RenderWindow& m_window;
    Camera m_camera;
    const sf::Font* m_font{ nullptr };

    Minimap m_minimap;
    TerrainRenderer m_terrain;
    
    // Render layers
    void renderMap(Map& map);
    // Pre-fog pass: resource nodes (always) + ghost buildings/dead-resources
    // in the shroud.  Everything in this pass is naturally darkened by the fog
    // overlay that follows.
    void renderGhosts(Game& game);
    // The fog-of-war overlay texture scaled to cover the entire map.
    void renderFogOverlay(Game& game);
    // Pass 2 – all other entities (units / buildings) filtered by fog visibility.
    void renderEntities(Game& game);
    void renderRallyPoints(Game& game);
    void renderUI(Game& game);
    void renderSelectionBox(const InputHandler& input);
    void renderBuildPreview(const InputHandler& input, Map& map);
    void renderMinimap(Game& game);
    void renderResourceBar(Player& player);
    void renderUnitPanel(Game& game);
    void renderTargetingModeIndicator(Game& game);
};
