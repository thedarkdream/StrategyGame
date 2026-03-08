#pragma once

#include "Types.h"
#include "AnimatedSprite.h"
#include <SFML/Graphics.hpp>
#include <memory>

class Entity : public std::enable_shared_from_this<Entity> {
public:
    Entity(EntityType type, Team team, sf::Vector2f position);
    virtual ~Entity() = default;
    
    // Core methods
    virtual void update(float deltaTime) = 0;
    virtual void render(sf::RenderTarget& target) = 0;
    
    // Getters
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
    void setSelected(bool selected) { m_selected = selected; }
    
    // Target highlight (blinking indicator when entity is targeted by a command)
    void startHighlight(float duration = 3.0f);
    void updateHighlight(float deltaTime);
    bool isHighlighted() const { return m_highlightTimeRemaining > 0.0f; }
    
    // Combat
    virtual void takeDamage(int damage);
    virtual void takeDamage(int damage, EntityPtr attacker);  // For retaliation
    
protected:
    EntityType m_type;
    Team m_team;
    sf::Vector2f m_position;
    sf::Vector2f m_size;
    int m_health;
    int m_maxHealth;
    bool m_selected = false;
    bool m_isDying = false;  // True while death animation is playing
    
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
};
