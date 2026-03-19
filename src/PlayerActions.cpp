#include "PlayerActions.h"
#include "Game.h"
#include "Player.h"
#include "Worker.h"
#include "Building.h"
#include "ResourceManager.h"
#include "EntityData.h"
#include "Constants.h"
#include "MathUtil.h"
#include <limits>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace {
    // Build concentric-ring offsets for `count` units around (0,0).
    // Ring 0: 1 slot (center). Ring k: k*6 slots at radius k*spacing.
    // `spacing` should be 2*maxCollisionRadius + a small gap so units don't
    // overlap in their destination slots.
    std::vector<sf::Vector2f> buildFormationOffsets(int count, float spacing) {
        std::vector<sf::Vector2f> out;
        out.reserve(count);
        out.push_back({0.f, 0.f});
        for (int ring = 1; static_cast<int>(out.size()) < count; ++ring) {
            int slots = ring * 6;
            float radius = ring * spacing;
            for (int i = 0; i < slots && static_cast<int>(out.size()) < count; ++i) {
                float angle = static_cast<float>(i) / static_cast<float>(slots)
                              * 2.f * 3.14159265f;
                out.push_back({radius * std::cos(angle), radius * std::sin(angle)});
            }
        }
        return out;
    }

    // Compute formation slot spacing for a group of units:
    // diameter of the largest unit + a small gap so slots don't overlap.
    float formationSpacing(const std::vector<EntityPtr>& units) {
        float maxRadius = 12.0f;  // sensible minimum (one soldier)
        for (const auto& e : units) {
            if (const Unit* u = e->asUnit())
                maxRadius = std::max(maxRadius, u->getCollisionRadius());
        }
        constexpr float GAP = 8.0f;
        return 2.0f * maxRadius + GAP;
    }
}

PlayerActions::PlayerActions(Player& player, Game& game)
    : m_player(player)
    , m_game(game)
{}

// ---------------------------------------------------------------------------
// Economy / Construction
// ---------------------------------------------------------------------------

bool PlayerActions::trainUnit(BuildingPtr building, EntityType type) {
    if (!building || !building->isConstructed()) return false;
    int mineralCost = ENTITY_DATA.getMineralCost(type);
    int gasCost     = ENTITY_DATA.getGasCost(type);
    if (!m_player.canAfford(mineralCost, gasCost)) return false;
    if (!building->trainUnit(type)) return false;
    m_player.spendResources(mineralCost, gasCost);
    return true;
}

bool PlayerActions::constructBuilding(EntityType type, sf::Vector2f worldPos, Worker* worker, bool append) {
    int mineralCost = ResourceManager::getMineralCost(type);
    if (!m_player.canAfford(mineralCost, 0)) return false;

    // Dependency check
    for (const auto& action : ENTITY_DATA.getActions(EntityType::Worker)) {
        if (action.type == ActionDef::Type::Build && action.producesType == type) {
            if (action.requires != EntityType::None &&
                !m_player.hasCompletedBuilding(action.requires)) return false;
            break;
        }
    }

    // Placement check
    sf::Vector2i tileSize = ResourceManager::getBuildingSize(type);
    int tileX = static_cast<int>(worldPos.x / Constants::TILE_SIZE);
    int tileY = static_cast<int>(worldPos.y / Constants::TILE_SIZE);
    if (!m_game.getMap().canPlaceBuilding(tileX, tileY, tileSize.x, tileSize.y))
        return false;

    // Worker pick
    if (!worker) worker = findNearestIdleWorker(worldPos);
    if (!worker) return false;

    // Commit
    m_player.spendResources(mineralCost, 0);
    sf::Vector2f pixelSize = ENTITY_DATA.getSize(type);
    sf::Vector2f buildPos(
        tileX * Constants::TILE_SIZE + pixelSize.x / 2.0f,
        tileY * Constants::TILE_SIZE + pixelSize.y / 2.0f
    );
    EntityPtr newBuilding = m_game.spawnBuilding(type, m_player.getTeam(), buildPos, false);
    if (newBuilding) {
        if (append) {
            std::weak_ptr<Entity> bWeak = newBuilding;
            worker->appendToQueue([worker, bWeak]{
                if (auto b = bWeak.lock(); b && b->isAlive()) worker->buildAt(b);
            });
        } else {
            worker->clearActionQueue();
            worker->buildAt(newBuilding);
        }
    }
    return newBuilding != nullptr;
}

void PlayerActions::continueConstruction(EntityPtr building, const std::vector<EntityPtr>& units, bool append) {
    if (!building) return;
    std::weak_ptr<Entity> bWeak = building;
    for (const auto& entity : units) {
        if (entity && entity->isAlive()) {
            if (Worker* w = entity->asWorker()) {
                if (append) {
                    w->appendToQueue([w, bWeak]{
                        if (auto b = bWeak.lock(); b && b->isAlive()) w->buildAt(b);
                    });
                } else {
                    w->clearActionQueue();
                    w->buildAt(building);
                }
            }
        }
    }
}

void PlayerActions::cancelConstruction(EntityPtr entity) {
    if (!entity) return;
    Building* building = entity->asBuilding();
    if (!building || building->isConstructed()) return;

    building->releaseBuilder();

    sf::Vector2i bSize = ResourceManager::getBuildingSize(building->getType());
    sf::Vector2i tile = MathUtil::buildingTileOrigin(building->getPosition(), bSize, Constants::TILE_SIZE);
    m_game.getMap().removeBuilding(tile.x, tile.y, bSize.x, bSize.y);

    m_player.addResources(
        ENTITY_DATA.getMineralCost(building->getType()),
        ENTITY_DATA.getGasCost(building->getType()));
    m_player.clearSelection();
    // Remove from the player's own building list so countBuildingsOfType and
    // isDefeated() no longer see this ghost after it leaves the world.
    m_player.removeBuilding(std::static_pointer_cast<Building>(entity));
    m_game.removeEntity(entity);
}

// ---------------------------------------------------------------------------
// Unit orders
// ---------------------------------------------------------------------------

void PlayerActions::move(const std::vector<EntityPtr>& units, sf::Vector2f target, bool append) {
    // For queued (shift-click) commands we don't know the future positions of
    // units, so we cannot assign meaningful formation slots.
    if (append) {
        for (const auto& e : units)
            if (auto* u = e->asUnit())
                u->appendToQueue([u, target]{ u->moveTo(target); });
        return;
    }

    // Build formation offsets and sort units so the closest one gets the
    // center slot — this minimises path-crossing inside the group.
    auto offsets = buildFormationOffsets(static_cast<int>(units.size()),
                                         formationSpacing(units));

    std::vector<size_t> order(units.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return MathUtil::distance(units[a]->getPosition(), target)
             < MathUtil::distance(units[b]->getPosition(), target);
    });

    for (size_t i = 0; i < order.size(); ++i) {
        auto* u = units[order[i]]->asUnit();
        if (!u) continue;
        u->clearActionQueue();
        u->moveTo(target + offsets[i]);
    }
}

void PlayerActions::follow(const std::vector<EntityPtr>& units, EntityPtr target, bool append) {
    if (m_isLocal && target) target->startHighlight();
    std::weak_ptr<Entity> tWeak = target;
    for (const auto& e : units) {
        if (auto* u = e->asUnit(); u && e != target) {
            if (append) {
                u->appendToQueue([u, tWeak]{
                    if (auto t = tWeak.lock(); t && t->isAlive()) u->follow(t);
                });
            } else {
                u->clearActionQueue();
                u->follow(target);
            }
        }
    }
}

void PlayerActions::attack(const std::vector<EntityPtr>& units, EntityPtr target, bool append) {
    if (m_isLocal && target) target->startHighlight();
    std::weak_ptr<Entity> tWeak = target;
    for (const auto& e : units) {
        if (auto* u = e->asUnit()) {
            if (append) {
                u->appendToQueue([u, tWeak]{
                    if (auto t = tWeak.lock(); t && t->isAlive()) u->attack(t);
                });
            } else {
                u->clearActionQueue();
                u->attack(target);
            }
        }
    }
}

void PlayerActions::attackMove(const std::vector<EntityPtr>& units, sf::Vector2f target, bool append) {
    if (append) {
        for (const auto& e : units)
            if (auto* u = e->asUnit())
                u->appendToQueue([u, target]{ u->attackMoveTo(target); });
        return;
    }

    auto offsets = buildFormationOffsets(static_cast<int>(units.size()),
                                         formationSpacing(units));

    std::vector<size_t> order(units.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return MathUtil::distance(units[a]->getPosition(), target)
             < MathUtil::distance(units[b]->getPosition(), target);
    });

    for (size_t i = 0; i < order.size(); ++i) {
        auto* u = units[order[i]]->asUnit();
        if (!u) continue;
        u->clearActionQueue();
        u->attackMoveTo(target + offsets[i]);
    }
}

void PlayerActions::gather(const std::vector<EntityPtr>& units, EntityPtr resource, bool append) {
    if (m_isLocal && resource && !append) resource->startHighlight();
    std::weak_ptr<Entity> rWeak = resource;
    for (const auto& e : units) {
        if (Worker* w = e->asWorker()) {
            if (append) {
                w->appendToQueue([w, rWeak]{
                    if (auto r = rWeak.lock(); r && r->isAlive()) w->gather(r);
                });
            } else {
                w->clearActionQueue();
                w->gather(resource);
            }
        }
    }
}

void PlayerActions::stop(const std::vector<EntityPtr>& units) {
    for (const auto& e : units)
        if (auto* u = e->asUnit()) u->stop();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

Worker* PlayerActions::findNearestIdleWorker(sf::Vector2f pos) const {
    Worker* best = nullptr;
    float   bestDist = std::numeric_limits<float>::max();
    for (const auto& entity : m_game.getAllEntities()) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getTeam() != m_player.getTeam()) continue;
        if (entity->getType() != EntityType::Worker) continue;
        Worker* w = entity->asWorker();
        if (!w || w->isBuilding()) continue;
        float dist = MathUtil::distance(w->getPosition(), pos);
        if (dist < bestDist) { bestDist = dist; best = w; }
    }
    return best;
}
