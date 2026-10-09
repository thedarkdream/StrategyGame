#include "Picking3D.h"
#include "Camera3D.h"
#include "Placement3D.h"
#include "EntityVisual.h"
#include "ModelBatch3D.h"
#include "Entity.h"
#include "FogOfWar.h"
#include "Game.h"
#include "Map.h"
#include "Player.h"
#include <algorithm>
#include <limits>

namespace Picking3D {

EntityPtr pickEntity(const Camera& camera, sf::Vector2i pixel, sf::Vector2u winSize,
                     const ModelBatch3D& models, Game& game) {
    if (winSize.x == 0 || winSize.y == 0) return nullptr;

    const Map&      map    = game.getMap();
    const FogOfWar& fog    = game.getPlayer().getFog();
    const Team      myTeam = game.getPlayer().getTeam();
    const Camera3D::Ray ray = Camera3D::pixelRay(camera, pixel, winSize, map);

    // Terrain hit distance: entities behind a hill must not be picked.
    const sf::Vector2f ground = Camera3D::screenToWorld(camera, pixel, winSize, map);
    const glm::vec3 groundPoint(ground.x, map.getHeightAt(ground), ground.y);
    const float terrainT = glm::dot(groundPoint - ray.origin, ray.dir);

    EntityPtr best;
    float bestKey = -std::numeric_limits<float>::max();
    for (const auto& entity : game.getWorld().all()) {
        if (!entity || !entity->isAlive()) continue;
        const EntityVisual visual = describeVisual(*entity);
        if (!visual.selectable) continue;
        if (!fog.isEntityShown(*entity, map, myTeam)) continue;

        const sf::Vector2f pos  = entity->getPosition();
        const sf::Vector2f size = entity->getSize();

        // Entities with a model are hit by its bounds, with artwork where their sprite is
        // drawn, the rest by their box.  Among hits the one drawn in front (larger ground z) wins.
        float t;
        float key;
        const Model* model = visual.hasModel() ? models.loaded(visual.model) : nullptr;
        if (model) {
            const Placement3D::ModelPlacement placed = Placement3D::placeModel(*model, visual, map, pos);
            if (!Camera3D::rayHitsBox(ray, placed.lo, placed.hi, t)) continue;
            key = pos.y + size.y * 0.5f;
        } else if (visual.hasSprite()) {
            const Placement3D::Billboard quad =
                Placement3D::placeBillboard(map, pos, visual.sprite.size, visual.spriteAnchor);
            if (!Placement3D::rayHitsBillboard(ray, quad, t)) continue;
            key = quad.bottom.z;
        } else {
            const Placement3D::Span box = Placement3D::bodySpan(visual, map, pos, size);
            const glm::vec3 lo(pos.x - size.x * 0.5f, box.baseY,              pos.y - size.y * 0.5f);
            const glm::vec3 hi(pos.x + size.x * 0.5f, box.baseY + box.height, pos.y + size.y * 0.5f);
            if (!Camera3D::rayHitsBox(ray, lo, hi, t)) continue;
            key = pos.y + size.y * 0.5f;
        }

        if (t <= terrainT + 1.0f && key > bestKey) {
            bestKey = key;
            best    = entity;
        }
    }
    return best;
}

std::vector<EntityPtr> pickEntitiesInRect(const Camera& camera, sf::Vector2i cornerA, sf::Vector2i cornerB,
                                          sf::Vector2u winSize, const ModelBatch3D& models,
                                          Team team, Game& game) {
    const Map& map = game.getMap();

    const float left   = static_cast<float>(std::min(cornerA.x, cornerB.x));
    const float right  = static_cast<float>(std::max(cornerA.x, cornerB.x));
    const float top    = static_cast<float>(std::min(cornerA.y, cornerB.y));
    const float bottom = static_cast<float>(std::max(cornerA.y, cornerB.y));

    std::vector<EntityPtr> result;
    for (const auto& entity : game.getWorld().all()) {
        if (!entity || !entity->isAlive() || entity->getTeam() != team) continue;
        const EntityVisual visual = describeVisual(*entity);
        if (!visual.selectable) continue;

        // An entity is inside the rubber-band when the middle of what is drawn projects into it.
        const sf::Vector2f pos = entity->getPosition();
        glm::vec3 centre;
        const Model* model = visual.hasModel() ? models.loaded(visual.model) : nullptr;
        if (model) {
            const Placement3D::ModelPlacement placed = Placement3D::placeModel(*model, visual, map, pos);
            centre = (placed.lo + placed.hi) * 0.5f;
        } else if (visual.hasSprite()) {
            const Placement3D::Billboard quad =
                Placement3D::placeBillboard(map, pos, visual.sprite.size, visual.spriteAnchor);
            centre = quad.bottom + Camera3D::billboardUp() * (quad.height * 0.5f);
        } else {
            const Placement3D::Span box = Placement3D::bodySpan(visual, map, pos, entity->getSize());
            centre = glm::vec3(pos.x, box.baseY + box.height * 0.5f, pos.y);
        }
        sf::Vector2f screen;
        if (!Camera3D::projectPoint(camera, centre, winSize, map, screen))
            continue;
        if (screen.x >= left && screen.x <= right && screen.y >= top && screen.y <= bottom)
            result.push_back(entity);
    }
    return result;
}

} // namespace Picking3D
