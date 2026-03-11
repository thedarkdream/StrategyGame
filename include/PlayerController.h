#pragma once

// ---------------------------------------------------------------------------
// PlayerController — abstract base for whoever drives a Player slot.
//
// This abstraction makes it trivial to add networked or replay-based
// controllers later: just add a new subclass.
//
// HumanController    – the local human (input is event-driven via InputHandler)
// AIPlayerController – wraps AIController for computer-managed slots
// ---------------------------------------------------------------------------

#include "AIController.h"   // AIController forward-included via full include
#include <memory>

class Player;
class Game;

// ---------------------------------------------------------------------------
class PlayerController {
public:
    virtual ~PlayerController() = default;

    // Called every game tick.  Human controller is a no-op here (events drive
    // it); AI controller runs its decision loop.
    virtual void update(float dt) = 0;

    // True only for the local human seat — used by Game to know which Player
    // owns the camera, selection, action bar, etc.
    virtual bool isLocalHuman() const = 0;
};

// ---------------------------------------------------------------------------
// Human (local) controller — driven by InputHandler; update() is empty.
// ---------------------------------------------------------------------------
class HumanController : public PlayerController {
public:
    void update(float) override {}
    bool isLocalHuman() const override { return true; }
};

// ---------------------------------------------------------------------------
// AI controller — wraps the existing AIController logic.
// ---------------------------------------------------------------------------
class AIPlayerController : public PlayerController {
public:
    AIPlayerController(Player& player, Game& game)
        : m_ai(player, game) {}

    void update(float dt) override { m_ai.update(dt); }
    bool isLocalHuman() const override { return false; }

    // Load AI scripts from a directory
    void loadScripts(const std::string& directory) { m_ai.loadScripts(directory); }

private:
    AIController m_ai;
};
