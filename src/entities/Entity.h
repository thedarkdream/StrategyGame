#pragma once

#include "core/Types.h"
#include "sprite/AnimatedSprite.h"
#include "core/IdGenerator.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <cmath>
#include <cstdint>
#include <string>

class Entity : public std::enable_shared_from_this<Entity> {
public:
    Entity(EntityType type, Team team, sf::Vector2f position);
    virtual ~Entity() = default;
    
    // Core methods
    virtual void update(float deltaTime) = 0;
    // NOTE: entities no longer draw themselves; see EntityRenderer2D.
    
    // Getters
    uint32_t     getId()     const { return m_id; }
    EntityType getType() const { return m_type; }
    Team getTeam() const { return m_team; }
    sf::Vector2f getPosition() const { return m_position; }
    sf::Vector2f getSize()     const { return m_size; }
    // Direction the entity looks, radians in the game plane (atan2(dy, dx)); south until it moves.
    float        getFacingAngle() const { return m_facing; }
    sf::FloatRect getBounds() const;
    int getHealth() const { return m_health; }
    int getMaxHealth() const { return m_maxHealth; }
    bool isAlive() const { return m_health > 0; }
    bool isDying() const { return m_isDying; }
    bool isReadyForRemoval() const { return m_health <= 0 && !m_isDying; }
    bool isSelected() const { return m_selected; }

    // Visual state consumed by the render layer (no drawing happens in Entity).
    sf::Color       getColor() const { return m_color; }
    bool            hasSprite() const { return m_hasSprite; }
    AnimatedSprite& getAnimatedSprite() { return m_animatedSprite; }

    // The frame to draw now (texture == nullptr: the entity has no artwork).
    // Entities with their own artwork override this.
    virtual SpriteFrame getSpriteFrame() const {
        return m_hasSprite ? m_animatedSprite.getFrame() : SpriteFrame{};
    }
    // Skeletal animation of the entity's 3D model: the clip to play (an AnimationState
    // name such as "walk") and how long it has been playing.  Models without such a
    // clip stay in their rest pose.
    const std::string& getModelClip() const { return m_modelClip; }
    float              getModelClipTime() const { return m_modelClipTime; }

    // 1 = fully solid; less while the entity is still being constructed.
    virtual float getVisualOpacity() const { return 1.0f; }
    
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
    bool isHighlighted()          const { return m_highlightTimeRemaining > 0.0f; }
    // True during the "visible" half-period of the blink cycle.
    bool isHighlightBlinkVisible() const {
        return m_highlightBlinkTimer < (HIGHLIGHT_BLINK_PERIOD * 0.5f);
    }
    
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
    float m_facing = 1.5707963f;   // pi / 2: looking at +y (south on screen)
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
    sf::Color m_color;
    
    // Animation system - AnimatedSprite holds reference to shared AnimationSet
    AnimatedSprite m_animatedSprite;
    bool m_hasSprite = false;
    std::string m_modelClip = "idle";
    float m_modelClipTime = 0.0f;
    
    // Animation helpers
    void loadAnimations(const std::string& basePath);  // e.g., "units/worker"
    void loadStaticSprite(const std::string& texturePath);  // For non-animated sprites
    void playAnimation(const std::string& animName);
    void updateSpriteDirection(sf::Vector2f movement);
    void startDeathAnimation();
    void updateDeathAnimation(float deltaTime);

    // Turns the 3D model toward a world point (sprites keep their own 8-way direction).
    void faceTowards(sf::Vector2f point) {
        const sf::Vector2f d = point - m_position;
        if (d.x * d.x + d.y * d.y > 1.0f) m_facing = std::atan2(d.y, d.x);
    }

    // Model clip helpers: switching clips restarts the clip, advancing moves its time.
    void playModelClip(const char* clip) {
        if (m_modelClip != clip) { m_modelClip = clip; m_modelClipTime = 0.0f; }
    }
    void advanceModelClip(float deltaTime) { m_modelClipTime += deltaTime; }
    
    // Death hook - called when entity starts dying (play sounds, effects, etc.)
    virtual void onDeath();
};
