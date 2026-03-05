#include "Entity.h"
#include "Constants.h"
#include <cmath>

Entity::Entity(EntityType type, Team team, sf::Vector2f position)
    : m_type(type)
    , m_team(team)
    , m_position(position)
    , m_size(32.0f, 32.0f)
    , m_health(100)
    , m_maxHealth(100)
{
    // Set color based on team using Constants
    switch (team) {
        case Team::Player:
            m_color = sf::Color(Constants::Colors::PLAYER_COLOR);
            break;
        case Team::Enemy:
            m_color = sf::Color(Constants::Colors::ENEMY_COLOR);
            break;
        case Team::Neutral:
            m_color = sf::Color(Constants::Colors::NEUTRAL_COLOR);
            break;
    }
    
    updateShape();
}

sf::FloatRect Entity::getBounds() const {
    return sf::FloatRect(
        sf::Vector2f(m_position.x - m_size.x / 2.0f, m_position.y - m_size.y / 2.0f),
        m_size
    );
}

void Entity::takeDamage(int damage) {
    m_health -= damage;
    if (m_health < 0) {
        m_health = 0;
    }
}

void Entity::takeDamage(int damage, EntityPtr attacker) {
    // Base implementation just applies damage
    takeDamage(damage);
}

void Entity::updateShape() {
    m_shape.setSize(m_size);
    m_shape.setOrigin(sf::Vector2f(m_size.x / 2.0f, m_size.y / 2.0f));
    m_shape.setPosition(m_position);
    m_shape.setFillColor(m_color);
    m_shape.setOutlineThickness(1.0f);
    m_shape.setOutlineColor(sf::Color::Black);
}

void Entity::renderHealthBar(sf::RenderTarget& target) {
    if (m_health >= m_maxHealth) return;  // Don't show full health
    
    const float barWidth = m_size.x;
    const float barHeight = 4.0f;
    const float yOffset = -m_size.y / 2.0f - 8.0f;
    
    // Background (red)
    sf::RectangleShape bgBar(sf::Vector2f(barWidth, barHeight));
    bgBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
    bgBar.setPosition(sf::Vector2f(m_position.x, m_position.y + yOffset));
    bgBar.setFillColor(sf::Color(180, 0, 0));
    target.draw(bgBar);
    
    // Health (green)
    float healthPercent = static_cast<float>(m_health) / static_cast<float>(m_maxHealth);
    sf::RectangleShape healthBar(sf::Vector2f(barWidth * healthPercent, barHeight));
    healthBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
    healthBar.setPosition(sf::Vector2f(m_position.x, m_position.y + yOffset));
    healthBar.setFillColor(sf::Color(0, 180, 0));
    target.draw(healthBar);
}

void Entity::renderSelectionIndicator(sf::RenderTarget& target) {
    if (!m_selected) return;
    
    sf::CircleShape circle(m_size.x / 2.0f + 4.0f);
    circle.setOrigin(sf::Vector2f(circle.getRadius(), circle.getRadius()));
    circle.setPosition(m_position);
    circle.setFillColor(sf::Color::Transparent);
    circle.setOutlineThickness(2.0f);
    circle.setOutlineColor(sf::Color::Green);
    target.draw(circle);
}
