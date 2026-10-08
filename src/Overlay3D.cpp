#include "Overlay3D.h"
#include "Camera3D.h"
#include "Placement3D.h"
#include "Constants.h"
#include "Building.h"
#include "EntityData.h"
#include "Game.h"
#include "InputHandler.h"
#include "Map.h"
#include "Player.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

namespace Overlay3D {

void addSelectionRing(MeshData& flat, const Map& map, sf::Vector2f pos, float radius, const glm::vec3& color) {
    constexpr int   kSegments = 36;
    constexpr float kHalfWidth = 1.2f;
    constexpr float kLift = 0.8f;
    auto ringPoint = [&](float angle, float r) {
        const float x = pos.x + std::cos(angle) * r;
        const float z = pos.y + std::sin(angle) * r;
        return glm::vec3(x, map.getHeightAt(sf::Vector2f(x, z)) + kLift, z);
    };
    for (int i = 0; i < kSegments; ++i) {
        const float a0 = glm::two_pi<float>() * static_cast<float>(i)     / kSegments;
        const float a1 = glm::two_pi<float>() * static_cast<float>(i + 1) / kSegments;
        flat.addUnlitQuad(ringPoint(a0, radius - kHalfWidth), ringPoint(a0, radius + kHalfWidth),
                          ringPoint(a1, radius + kHalfWidth), ringPoint(a1, radius - kHalfWidth), color);
    }
}

void addHealthBar(MeshData& flat, const glm::vec3& topPoint, float width, float fraction) {
    const glm::vec3 right = Camera3D::billboardRight();
    const glm::vec3 up    = Camera3D::billboardUp();
    const float barH = 4.0f;
    const glm::vec3 centre = topPoint + up * 8.0f;
    const glm::vec3 half   = right * (width * 0.5f);
    const glm::vec3 lift   = up * (barH * 0.5f);
    const float pct = std::clamp(fraction, 0.0f, 1.0f);

    flat.addUnlitQuad(centre - half - lift, centre + half - lift,
                      centre + half + lift, centre - half + lift,
                      glm::vec3(0.7f, 0.0f, 0.0f));
    const glm::vec3 fillEnd = centre - half + right * (width * pct);
    flat.addUnlitQuad(centre - half - lift, fillEnd - lift,
                      fillEnd + lift, centre - half + lift,
                      glm::vec3(0.0f, 0.7f, 0.0f));
}

void addRallyPoints(Game& game, MeshData& flat, MeshData& solid) {
    const Map& map = game.getMap();
    const glm::vec3 right = Camera3D::billboardRight();
    const glm::vec3 up    = Camera3D::billboardUp();
    const glm::vec3 lineColor(0.0f, 1.0f, 0.4f);

    for (const auto& entity : game.getPlayer().getSelection()) {
        auto* building = entity->asBuilding();
        if (!building || !building->isConstructed()) continue;
        auto* def = ENTITY_DATA.getBuildingDef(entity->getType());
        if (!def || !def->canProduce()) continue;

        const sf::Vector2f from = building->getPosition();
        const sf::Vector2f to   = building->getRallyPoint();

        // Ground-hugging ribbon.
        const sf::Vector2f delta = to - from;
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (length > 1.0f) {
            const sf::Vector2f dir(delta.x / length, delta.y / length);
            const sf::Vector2f normal(-dir.y * 1.0f, dir.x * 1.0f);   // half-width 1
            const int segments = std::max(1, static_cast<int>(length / 16.0f));
            auto point = [&](float t, float side) {
                const float x = from.x + delta.x * t + normal.x * side;
                const float z = from.y + delta.y * t + normal.y * side;
                return glm::vec3(x, map.getHeightAt(sf::Vector2f(x, z)) + 0.9f, z);
            };
            for (int i = 0; i < segments; ++i) {
                const float t0 = static_cast<float>(i)     / static_cast<float>(segments);
                const float t1 = static_cast<float>(i + 1) / static_cast<float>(segments);
                flat.addUnlitQuad(point(t0, -1.0f), point(t0, 1.0f), point(t1, 1.0f), point(t1, -1.0f), lineColor);
            }
        }

        // Flag: a pole plus a camera-facing triangle at its top.
        const float groundY = map.getHeightAt(to);
        constexpr float kPoleHeight = 18.0f;
        solid.addBox(to.x, to.y, 2.0f, 2.0f, kPoleHeight, glm::vec3(0.4f, 0.2f, 0.08f), groundY);
        const glm::vec3 top(to.x, groundY + kPoleHeight, to.y);
        const glm::vec3 a = top;
        const glm::vec3 b = top + right * 12.0f - up * 3.5f;
        const glm::vec3 c = top - up * 7.0f;
        flat.addUnlitQuad(a, b, c, c, lineColor);
    }
}

void addBuildPreview(Game& game, MeshData& solid) {
    InputHandler& input = game.getInput();
    if (!input.isInBuildMode()) return;

    Map&               map  = game.getMap();
    const sf::Vector2f pos  = input.getBuildPreviewPosition();
    const sf::Vector2f size = ENTITY_DATA.getSize(input.getBuildingType());

    const int tilesW = static_cast<int>(size.x / Constants::TILE_SIZE);
    const int tilesH = static_cast<int>(size.y / Constants::TILE_SIZE);
    const int tileX  = static_cast<int>((pos.x - size.x / 2.0f) / Constants::TILE_SIZE);
    const int tileY  = static_cast<int>((pos.y - size.y / 2.0f) / Constants::TILE_SIZE);
    const bool canPlace = map.canPlaceBuilding(tileX, tileY, tilesW, tilesH);

    const Placement3D::Span span = Placement3D::footprintSpan(map, pos, size);
    const glm::vec3 color = canPlace ? glm::vec3(0.2f, 1.0f, 0.2f) : glm::vec3(1.0f, 0.2f, 0.2f);
    const float bodyHeight = std::min(size.x, size.y) * ENTITY_DATA.getVisual(input.getBuildingType()).bodyHeight;
    solid.addBox(pos.x, pos.y, size.x, size.y, span.height + bodyHeight, color, span.baseY);
}

} // namespace Overlay3D
