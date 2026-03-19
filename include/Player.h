#pragma once

#include "Types.h"
#include "FogOfWar.h"
#include <vector>

class Map;

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
    EntityPtr getFirstSelectedEntity() const;      // Returns first selected entity, or nullptr
    EntityPtr getFirstOwnedSelectedEntity() const; // Returns first selected entity belonging to this player, or nullptr
    
    // Team
    Team getTeam() const { return m_team; }
    
    // Game state
    bool isDefeated() const;
    int getUnitCount() const { return static_cast<int>(m_units.size()); }
    int getBuildingCount() const { return static_cast<int>(m_buildings.size()); }
    bool hasCompletedBuilding(EntityType type) const;
    
    // Update
    void update(float deltaTime);
    // Prune dead/dying entities from the selection list.
    // Called every frame by Game so the HUD reacts immediately when health hits zero.
    void cleanupSelection();

    // Fog of War
    FogOfWar& getFog() { return m_fog; }
    const FogOfWar& getFog() const { return m_fog; }
    // Recompute fog visibility and update ghost snapshots.
    // Pass the full entity list so resource nodes and buildings can be tracked.
    void updateFog(const Map& map, const EntityList& allEntities);
    
private:
    Team m_team;
    Resources m_resources;
    UnitList m_units;
    BuildingList m_buildings;
    std::vector<EntityPtr> m_selection;
    FogOfWar m_fog;
};
