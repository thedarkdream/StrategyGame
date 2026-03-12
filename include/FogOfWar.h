#pragma once

#include "Types.h"
#include "Constants.h"
#include <SFML/Graphics.hpp>
#include <unordered_map>

class Map;

// ---------------------------------------------------------------------------
// FogOfWar – per-player visibility tracker for the Fog of War system.
//
// Each player maintains one FogOfWar instance.  Every game frame the caller
// should invoke update() with the player's current unit and building lists,
// then recordGhosts() with the full entity list so that buildings and resource
// nodes that have been spotted are remembered even after vision is lost or
// after the entity is destroyed.
//
// Visibility rules:
//   • The map starts fully EXPLORED for every player (terrain + initial
//     mineral/gas positions are revealed from turn one).
//   • Only tiles within the sight radius of a friendly unit or building are
//     VISIBLE each frame.
//   • Visible  → transparent texel (no fog).
//   • Explored-but-not-visible → dark semi-transparent texel (shroud).
//   • Unexplored → fully black texel.
//
// Ghost entity rules:
//   • Every resource node is recorded as a ghost from the first frame
//     (the whole map is explored from the start).
//   • An enemy building is recorded the first time its tile enters vision;
//     afterwards it remains visible as a ghost even when out of vision or
//     after being destroyed.
// ---------------------------------------------------------------------------

// Snapshot of a building or resource node that has been seen at least once.
// The shared_ptr keeps the entity object alive (and renderable) even after
// Game removes it from its entity list upon destruction.
struct GhostRecord {
    EntityPtr entity;   // Last-known entity object (may be dead)
};

class FogOfWar {
public:
    FogOfWar();

    // Recompute which tiles are visible this frame.
    // Call once per game-update before the Renderer reads the fog texture.
    void update(const UnitList& units, const BuildingList& buildings, const Map& map);

    // Snapshot buildings and resource nodes that are on explored tiles.
    // Resource nodes are recorded immediately (all tiles start explored);
    // buildings are recorded only when their tile is currently visible.
    // Call after update() so that m_visible is up-to-date.
    void recordGhosts(const EntityList& allEntities, const Map& map);

    // Tile-coordinate queries (clamped; out-of-bounds → false)
    bool isVisible(int tx, int ty) const;
    bool isExplored(int tx, int ty) const;

    // World-space convenience (converts to tile internally)
    bool isVisibleAtWorld(sf::Vector2f worldPos, const Map& map) const;

    // Read-only access to the ghost snapshot table.
    const std::unordered_map<uint32_t, GhostRecord>& getGhosts() const { return m_ghosts; }

    // Returns the fog-overlay texture (W×H, one texel per tile).
    // Call rebuildTexture() to refresh after update().
    const sf::Texture& getFogTexture() const { return m_fogTexture; }
    bool isTextureDirty() const { return m_textureDirty; }
    void rebuildTexture();

private:
    static constexpr int W = Constants::MAP_WIDTH;
    static constexpr int H = Constants::MAP_HEIGHT;

    // [row y][col x] layout
    bool m_explored[H][W];
    bool m_visible[H][W];

    // Ghost snapshots: entity ID → last-known entity object.
    std::unordered_map<uint32_t, GhostRecord> m_ghosts;

    sf::Texture m_fogTexture;
    bool        m_textureDirty = true;
};
