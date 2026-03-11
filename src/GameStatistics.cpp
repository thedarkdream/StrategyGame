#include "GameStatistics.h"

void GameStatistics::setPlayerActive(int slot, Team team) {
    if (slot >= 0 && slot < MAX_PLAYERS) {
        m_stats[slot].isActive = true;
        m_stats[slot].team = team;
    }
}

void GameStatistics::recordUnitCreated(Team team) {
    int slot = getSlotFromTeam(team);
    if (slot >= 0 && slot < MAX_PLAYERS) {
        m_stats[slot].unitsCreated++;
    }
}

void GameStatistics::recordUnitDestroyed(Team team, Team killerTeam) {
    // Record unit lost for the owner
    int ownerSlot = getSlotFromTeam(team);
    if (ownerSlot >= 0 && ownerSlot < MAX_PLAYERS) {
        // This unit was destroyed (lost by owner)
    }
    
    // Record unit destroyed by the killer
    int killerSlot = getSlotFromTeam(killerTeam);
    if (killerSlot >= 0 && killerSlot < MAX_PLAYERS && killerTeam != Team::Neutral) {
        m_stats[killerSlot].unitsDestroyed++;
    }
}

void GameStatistics::recordBuildingCreated(Team team) {
    int slot = getSlotFromTeam(team);
    if (slot >= 0 && slot < MAX_PLAYERS) {
        m_stats[slot].buildingsCreated++;
    }
}

void GameStatistics::recordBuildingDestroyed(Team team, Team destroyerTeam) {
    // Record building lost for the owner
    int ownerSlot = getSlotFromTeam(team);
    if (ownerSlot >= 0 && ownerSlot < MAX_PLAYERS) {
        // This building was destroyed (lost by owner)
    }
    
    // Record building destroyed by the destroyer
    int destroyerSlot = getSlotFromTeam(destroyerTeam);
    if (destroyerSlot >= 0 && destroyerSlot < MAX_PLAYERS && destroyerTeam != Team::Neutral) {
        m_stats[destroyerSlot].buildingsDestroyed++;
    }
}

const GameStatistics::PlayerStats& GameStatistics::getStats(int slot) const {
    static PlayerStats emptyStats;
    if (slot >= 0 && slot < MAX_PLAYERS) {
        return m_stats[slot];
    }
    return emptyStats;
}

int GameStatistics::getActivePlayerCount() const {
    int count = 0;
    for (const auto& stats : m_stats) {
        if (stats.isActive) count++;
    }
    return count;
}

int GameStatistics::getSlotFromTeam(Team team) {
    switch (team) {
        case Team::Player1: return 0;
        case Team::Player2: return 1;
        case Team::Player3: return 2;
        case Team::Player4: return 3;
        default: return -1;
    }
}
