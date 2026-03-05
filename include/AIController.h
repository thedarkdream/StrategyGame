#pragma once

#include "Types.h"
#include <random>

class Player;
class Map;
class Game;

class AIController {
public:
    AIController(Player& player, Game& game);
    
    void update(float deltaTime);
    
    // AI settings
    void setDifficulty(int level) { m_difficulty = level; }
    
private:
    Player& m_player;
    Game& m_game;
    Map* m_map = nullptr;
    
    int m_difficulty = 1;  // 1 = easy, 2 = medium, 3 = hard
    float m_decisionTimer = 0.0f;
    float m_decisionInterval = 2.0f;  // seconds between decisions
    
    std::mt19937 m_rng;
    
    // AI behaviors
    void makeDecision();
    void manageEconomy();
    void manageProduction();
    void manageArmy();
    void sendScout();
    void attackEnemy();
    
    // Helpers
    BuildingPtr findIdleBase();
    BuildingPtr findIdleBarracks();
    int countUnitsOfType(EntityType type);
    int countBuildingsOfType(EntityType type);
    sf::Vector2f findBuildLocation(EntityType buildingType);
    EntityPtr findNearestEnemy(sf::Vector2f from);
};
