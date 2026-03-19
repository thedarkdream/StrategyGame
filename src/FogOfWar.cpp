#include "FogOfWar.h"
#include "EntityData.h"
#include "Map.h"
#include "Unit.h"
#include "Building.h"
#include "ResourceNode.h"
#include <cmath>

FogOfWar::FogOfWar() {
    // Arrays are sized dynamically on the first update() call; nothing to
    // initialise here.  rebuildTexture() produces a minimal valid texture so
    // the Renderer always has something to draw before the first update().
    rebuildTexture();
}

// ---------------------------------------------------------------------------
void FogOfWar::resize(int w, int h) {
    m_W = w;
    m_H = h;
    // All tiles start EXPLORED (terrain + starting mineral positions are
    // always known from the beginning of the game).
    m_explored.assign(static_cast<size_t>(w * h), 1);
    m_visible .assign(static_cast<size_t>(w * h), 0);
}

// ---------------------------------------------------------------------------
void FogOfWar::update(const UnitList& units, const BuildingList& buildings, const Map& map) {
    // Lazily initialise (or reinitialise on map change)
    const int mapW = map.getWidth();
    const int mapH = map.getHeight();
    if (m_W != mapW || m_H != mapH)
        resize(mapW, mapH);

    // Clear per-frame visibility
    for (int i = 0; i < m_W * m_H; ++i)
        m_visible[i] = 0;

    // Helper: given a world-space position and a vision radius (in world units),
    // mark all tiles whose centres fall within that circle as visible.
    auto markVisible = [&](sf::Vector2f worldPos, float visionRadius) {
        if (visionRadius <= 0.0f) return;

        sf::Vector2i centre = map.worldToTile(worldPos);
        int tileRadius = static_cast<int>(std::ceil(visionRadius / static_cast<float>(Constants::TILE_SIZE)));

        for (int dy = -tileRadius; dy <= tileRadius; ++dy) {
            for (int dx = -tileRadius; dx <= tileRadius; ++dx) {
                int tx = centre.x + dx;
                int ty = centre.y + dy;
                if (tx < 0 || tx >= m_W || ty < 0 || ty >= m_H) continue;

                // Distance check: centre of tile vs entity position (world units)
                sf::Vector2f tileCentre = map.tileToWorldCenter(tx, ty);
                float ddx = tileCentre.x - worldPos.x;
                float ddy = tileCentre.y - worldPos.y;
                if (ddx * ddx + ddy * ddy <= visionRadius * visionRadius) {
                    m_visible [ty * m_W + tx] = 1;
                    m_explored[ty * m_W + tx] = 1;   // Mark as discovered forever
                }
            }
        }
    };

    for (const auto& unit : units) {
        if (!unit || !unit->isAlive()) continue;
        float vr = ENTITY_DATA.getVisionRadius(unit->getType());
        markVisible(unit->getPosition(), vr);
    }
    for (const auto& building : buildings) {
        if (!building || !building->isAlive()) continue;
        float vr = ENTITY_DATA.getVisionRadius(building->getType());
        markVisible(building->getPosition(), vr);
    }

    m_textureDirty = true;
}

// ---------------------------------------------------------------------------
bool FogOfWar::isVisible(int tx, int ty) const {
    if (m_revealed) return true;
    if (m_W == 0 || tx < 0 || tx >= m_W || ty < 0 || ty >= m_H) return false;
    return m_visible[ty * m_W + tx] != 0;
}

bool FogOfWar::isExplored(int tx, int ty) const {
    if (m_revealed) return true;
    if (m_W == 0 || tx < 0 || tx >= m_W || ty < 0 || ty >= m_H) return false;
    return m_explored[ty * m_W + tx] != 0;
}

bool FogOfWar::isVisibleAtWorld(sf::Vector2f worldPos, const Map& map) const {
    sf::Vector2i tile = map.worldToTile(worldPos);
    return isVisible(tile.x, tile.y);
}

// ---------------------------------------------------------------------------
void FogOfWar::recordGhosts(const EntityList& allEntities, const Map& map) {
    for (const auto& entity : allEntities) {
        if (!entity) continue;

        const bool isResource = entity->asResourceNode() != nullptr;
        const bool isBuilding = entity->asBuilding()     != nullptr;
        if (!isResource && !isBuilding) continue;

        sf::Vector2i tile = map.worldToTile(entity->getPosition());
        if (m_W == 0 || tile.x < 0 || tile.x >= m_W || tile.y < 0 || tile.y >= m_H) continue;

        const bool explored = m_explored[tile.y * m_W + tile.x] != 0;
        const bool visible  = m_visible [tile.y * m_W + tile.x] != 0;

        // Resource nodes: record whenever the tile is explored (they are
        // known from game start because the whole map starts explored).
        // Buildings: only record when actually in vision (must be "spotted").
        const bool shouldRecord = isResource ? explored : visible;
        if (!shouldRecord) continue;

        auto it = m_ghosts.find(entity->getId());
        if (it == m_ghosts.end()) {
            // First time we can record this entity.
            m_ghosts[entity->getId()] = { entity };
        } else if (visible) {
            // Refresh the snapshot while the entity is currently visible so
            // that the ghost always reflects the most recent observed state.
            it->second.entity = entity;
        }
        // When explored but not visible: do NOT update — preserve last known state.
    }
}

// ---------------------------------------------------------------------------
void FogOfWar::rebuildTexture() {
    // If the map hasn't been initialised yet, produce a minimal 1×1 placeholder.
    const int texW = (m_W > 0) ? m_W : 1;
    const int texH = (m_H > 0) ? m_H : 1;

    // Build a texW×texH RGBA image: one pixel per tile.
    sf::Image img(sf::Vector2u(static_cast<unsigned>(texW), static_cast<unsigned>(texH)),
                  sf::Color::Transparent);

    // When fully revealed the texture is left all-transparent (no overlay).
    if (!m_revealed && m_W > 0) {
        for (int y = 0; y < m_H; ++y) {
            for (int x = 0; x < m_W; ++x) {
                sf::Color c;
                if (m_visible[y * m_W + x]) {
                    c = sf::Color(0, 0, 0, 0);      // Fully transparent – no fog
                } else if (m_explored[y * m_W + x]) {
                    c = sf::Color(0, 0, 0, 160);    // Shroud – previously seen
                } else {
                    c = sf::Color(0, 0, 0, 230);    // Unexplored – total darkness
                }
                img.setPixel(sf::Vector2u(static_cast<unsigned>(x),
                                          static_cast<unsigned>(y)), c);
            }
        }
    }

    if (!m_fogTexture.loadFromImage(img)) {
        return;
    }
    // Smooth = true gives a nicer soft fog edge when the tiny texture is
    // scaled up to cover the full map.
    m_fogTexture.setSmooth(true);
    m_textureDirty = false;
}
