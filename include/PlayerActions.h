#pragma once

#include "Types.h"
#include <vector>

class Player;
class Game;
class Worker;

// ---------------------------------------------------------------------------
// PlayerActions — unified interface for all player-issued game actions.
//
// Both the human player (InputHandler / ActionBar) and the AI (AIController)
// route every game action through this class, ensuring resource checks,
// placement validation and entity setup happen in exactly one place.
//
// Human path:  InputHandler/ActionBar → Game::issue*() → PlayerActions
// AI path:     AIController           → PlayerActions  (direct)
// ---------------------------------------------------------------------------
class PlayerActions {
public:
    PlayerActions(Player& player, Game& game);

    // Mark this instance as belonging to the local human player.
    // Only local-player actions trigger target highlight effects.
    void setLocalPlayer(bool isLocal) { m_isLocal = isLocal; }

    // --- Economy / Construction ------------------------------------------

    // Check affordability + start production + deduct cost.
    // Returns false if the building can't accept the unit or player can't afford it.
    bool trainUnit(BuildingPtr building, EntityType type);

    // Validate cost + dependencies + placement; spawn the foundation,
    // deduct resources, then send a worker to build it.
    // worldPos: world-space point anywhere inside the desired tile footprint.
    // worker:   explicit worker to assign, or nullptr to auto-pick the nearest
    //           idle worker owned by this player.
    bool constructBuilding(EntityType type, sf::Vector2f worldPos, Worker* worker = nullptr);

    // Send units (typically workers from selection) to continue building an
    // existing incomplete foundation.
    void continueConstruction(EntityPtr building, const std::vector<EntityPtr>& units);

    // Cancel an incomplete building: release builder, free tiles, refund cost.
    void cancelConstruction(EntityPtr building);

    // --- Unit orders -----------------------------------------------------
    // All methods accept an explicit entity list so they work identically for
    // selection-based (human) and programmatic (AI) callers.

    void move(const std::vector<EntityPtr>& units, sf::Vector2f target);
    void follow(const std::vector<EntityPtr>& units, EntityPtr target);
    void attack(const std::vector<EntityPtr>& units, EntityPtr target);
    void attackMove(const std::vector<EntityPtr>& units, sf::Vector2f target);
    void gather(const std::vector<EntityPtr>& units, EntityPtr resource);
    void stop(const std::vector<EntityPtr>& units);

private:
    Player& m_player;
    Game&   m_game;
    bool    m_isLocal = false;

    Worker* findNearestIdleWorker(sf::Vector2f pos) const;
};
