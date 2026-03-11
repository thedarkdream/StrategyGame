#pragma once

#include "Types.h"
#include <array>
#include <string>

// Tracks game statistics for each player
class GameStatistics {
public:
    static constexpr int MAX_PLAYERS = 4;
    
    struct PlayerStats {
        int unitsCreated = 0;
        int unitsDestroyed = 0;
        int buildingsCreated = 0;
        int buildingsDestroyed = 0;
        bool isActive = false;  // Was this player slot used in the game?
        Team team = Team::Neutral;
    };
    
    GameStatistics() = default;
    
    // Mark a player slot as active
    void setPlayerActive(int slot, Team team);
    
    // Record statistics events
    void recordUnitCreated(Team team);
    void recordUnitDestroyed(Team team, Team killerTeam);
    void recordBuildingCreated(Team team);
    void recordBuildingDestroyed(Team team, Team destroyerTeam);
    
    // Access statistics
    const PlayerStats& getStats(int slot) const;
    int getActivePlayerCount() const;
    
    // Get player slot index from team
    static int getSlotFromTeam(Team team);
    
private:
    std::array<PlayerStats, MAX_PLAYERS> m_stats;
};
