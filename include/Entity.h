#pragma once

#include "Types.h"
#include "AnimatedSprite.h"
#include "IdGenerator.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <cstdint>

class Entity : public std::enable_shared_from_this<Entity> {
public:
    Entity(EntityType type, Team team, sf::Vector2f position);
    virtual ~Entity() = default;
    
    // Core methods
    virtual void update(float deltaTime) = 0;
    virtual void render(sf::RenderTarget& target) = 0;
    
    // Getters
    uint32_t     getId()     const { return m_id; }
    EntityType getType() const { return m_type; }
    Team getTeam() const { return m_team; }
    sf::Vector2f getPosition() const { return m_position; }
    sf::FloatRect getBounds() const;
    int getHealth() const { return m_health; }
    int getMaxHealth() const { return m_maxHealth; }
    bool isAlive() const { return m_health > 0; }
    bool isDying() const { return m_isDying; }
    bool isReadyForRemoval() const { return m_health <= 0 && !m_isDying; }
    bool isSelected() const { return m_selected; }
    
    // Setters
    void setPosition(sf::Vector2f position) { m_position = position; }
    void setSelected(bool selected) { m_selected = selected; if (selected) onSelected(); }
    void setIsLocalTeam(bool val) { m_isLocalTeam = val; }

    // Called once when this entity becomes selected; override for unit voice lines etc.
    virtual void onSelected() {}
    // Called once after the entity is fully spawned and wired into the world.
    virtual void onSpawned() {}
    bool isLocalTeam() const { return m_isLocalTeam; }

    // Category helpers — virtual downcast without RTTI overhead.
    // Return a typed pointer if this entity IS that type, otherwise nullptr.
    virtual Unit*               asUnit()               { return nullptr; }
    virtual const Unit*         asUnit()         const { return nullptr; }
    virtual Building*           asBuilding()           { return nullptr; }
    virtual const Building*     asBuilding()     const { return nullptr; }
    virtual Worker*             asWorker()             { return nullptr; }
    virtual const Worker*       asWorker()       const { return nullptr; }
    virtual ResourceNode*       asResourceNode()       { return nullptr; }
    virtual const ResourceNode* asResourceNode() const { return nullptr; }

    // Target highlight (blinking indicator when entity is targeted by a command)
    void startHighlight(float duration = 3.0f);
    void updateHighlight(float deltaTime);
    bool isHighlighted() const { return m_highlightTimeRemaining > 0.0f; }
    
    // Combat
    virtual void takeDamage(int damage);
    virtual void takeDamage(int damage, EntityPtr attacker);  // For retaliation
    virtual void takeDamage(int damage, Team attackerTeam);   // For projectiles (source may be dead)
    
    // Track last attacker (for statistics)
    Team getLastAttackerTeam() const { return m_lastAttackerTeam; }

    // Under-attack detection: true within UNDER_ATTACK_WINDOW seconds of last hit.
    bool isUnderAttack() const { return m_underAttackTimer > 0.f; }

    // True when this entity is a resource node (mineral patch, gas geyser, etc.).
    // Delegates to EntityDef::isResource() so the check stays data-driven.
    bool isResource() const;

protected:
    void markUnderAttack()   { m_underAttackTimer = UNDER_ATTACK_WINDOW; }
    void tickUnderAttack(float dt) { m_underAttackTimer = std::max(0.f, m_underAttackTimer - dt); }

    uint32_t     m_id;
    EntityType m_type;
    Team m_team;
    sf::Vector2f m_position;
    sf::Vector2f m_size;
    int m_health;
    int m_maxHealth;
    bool m_selected = false;
    bool m_isLocalTeam = false;  // True if this entity belongs to the local human player
    bool m_isDying = false;  // True while death animation is playing
    Team m_lastAttackerTeam = Team::Neutral;  // Track who dealt the killing blow

    float m_underAttackTimer = 0.f;
    static constexpr float UNDER_ATTACK_WINDOW = 4.0f;  // seconds
    
    // Target highlight (blinking indicator)
    float m_highlightTimeRemaining = 0.0f;
    float m_highlightBlinkTimer = 0.0f;
    static constexpr float HIGHLIGHT_BLINK_PERIOD = 1.0f;  // Once per second
    
    // Visual
    sf::RectangleShape m_shape;
    sf::Color m_color;
    
    // Animation system - AnimatedSprite holds reference to shared AnimationSet
    AnimatedSprite m_animatedSprite;
    bool m_hasSprite = false;
    
    void updateShape();
    void renderHealthBar(sf::RenderTarget& target);
    void renderSelectionIndicator(sf::RenderTarget& target);
    
    // Animation helpers
    void loadAnimations(const std::string& basePath);  // e.g., "units/worker"
    void loadStaticSprite(const std::string& texturePath);  // For non-animated sprites
    void playAnimation(const std::string& animName);
    void updateSpriteDirection(sf::Vector2f movement);
    void startDeathAnimation();
    void updateDeathAnimation(float deltaTime);
    
    // Death hook - called when entity starts dying (play sounds, effects, etc.)
    virtual void onDeath();
};
