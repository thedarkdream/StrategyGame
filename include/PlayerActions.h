#pragma once

#include "Types.h"
#include <vector>

class Player;
class Game;
class Worker;

// ---------------------------------------------------------------------------
// PlayerCommandScope — lightweight RAII scope guard.
//
// Activated at the entry of every PlayerActions method that belongs to the
// local human player (m_isLocal == true).  Queued lambdas capture a `bool`
// at creation time and re-activate the scope when executed, so deferred
// actions are treated the same as immediate ones.
//
// Any system (voice lines, UI feedback, …) can call
//   PlayerCommandScope::isActive()
// to know whether the current call stack originated from a direct player
// input rather than an automated game process (AI, rally points, auto-attack,
// automated regather, etc.).
// ---------------------------------------------------------------------------
struct PlayerCommandScope {
    explicit PlayerCommandScope(bool activate) : m_active(activate)
        { if (m_active) ++s_depth; }
    ~PlayerCommandScope()
        { if (m_active) --s_depth; }

    static bool isActive() { return s_depth > 0; }
private:
    bool m_active;
    static int s_depth;
};

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
    // append:   if true, the worker's movement is queued rather than issued immediately.
    bool constructBuilding(EntityType type, sf::Vector2f worldPos, Worker* worker = nullptr, bool append = false);

    // Send units (typically workers from selection) to continue building an
    // existing incomplete foundation.
    void continueConstruction(EntityPtr building, const std::vector<EntityPtr>& units, bool append = false);

    // Cancel an incomplete building: release builder, free tiles, refund cost.
    void cancelConstruction(EntityPtr building);

    // --- Unit orders -----------------------------------------------------
    // All methods accept an explicit entity list so they work identically for
    // selection-based (human) and programmatic (AI) callers.
    // append: when true the action is appended to each unit's queue (shift+click
    //         behaviour) instead of replacing the current action.

    void move(const std::vector<EntityPtr>& units, sf::Vector2f target, bool append = false);
    void follow(const std::vector<EntityPtr>& units, EntityPtr target, bool append = false);
    void attack(const std::vector<EntityPtr>& units, EntityPtr target, bool append = false);
    void attackMove(const std::vector<EntityPtr>& units, sf::Vector2f target, bool append = false);
    void gather(const std::vector<EntityPtr>& units, EntityPtr resource, bool append = false);
    void stop(const std::vector<EntityPtr>& units);

    // Smart right-click dispatch: inspect the target and selection to decide
    // whether to move, attack, gather, set a rally point, or continue construction.
    // This keeps the dispatch logic out of InputHandler so it can be reused by
    // other input surfaces (minimap right-click, unit AI queuing, etc.).
    void issueSmartRightClick(sf::Vector2f worldPos, EntityPtr target, bool append = false);

private:
    Player& m_player;
    Game&   m_game;
    bool    m_isLocal = false;

    Worker* findNearestIdleWorker(sf::Vector2f pos) const;
};
