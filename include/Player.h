#pragma once

#include "Types.h"
#include <vector>

class Player {
public:
    Player(Team team, const Resources& startingResources);
    
    // Resource management
    Resources& getResources() { return m_resources; }
    const Resources& getResources() const { return m_resources; }
    void addResources(int minerals, int gas);
    bool spendResources(int minerals, int gas);
    bool canAfford(int minerals, int gas) const;
    
    // Entity management
    void addUnit(UnitPtr unit);
    void addBuilding(BuildingPtr building);
    void removeUnit(UnitPtr unit);
    void removeBuilding(BuildingPtr building);
    
    const UnitList& getUnits() const { return m_units; }
    const BuildingList& getBuildings() const { return m_buildings; }
    
    // Selection
    void clearSelection();
    void selectEntity(EntityPtr entity);
    void selectEntities(const std::vector<EntityPtr>& entities);
    const std::vector<EntityPtr>& getSelection() const { return m_selection; }
    bool hasSelection() const { return !m_selection.empty(); }
    
    // Team
    Team getTeam() const { return m_team; }
    
    // Game state
    bool isDefeated() const;
    int getUnitCount() const { return static_cast<int>(m_units.size()); }
    int getBuildingCount() const { return static_cast<int>(m_buildings.size()); }
    
    // Update
    void update(float deltaTime);
    void cleanupDeadEntities();
    
private:
    Team m_team;
    Resources m_resources;
    UnitList m_units;
    BuildingList m_buildings;
    std::vector<EntityPtr> m_selection;
};
