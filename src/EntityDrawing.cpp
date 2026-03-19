#include "EntityDrawing.h"
#include "Entity.h"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

namespace EntityDrawing {

void drawHealthBar(sf::RenderTarget& target, const Entity& entity) {
    // Always show when selected; otherwise only when damaged
    if (entity.getHealth() >= entity.getMaxHealth() && !entity.isSelected()) return;

    const sf::Vector2f size = entity.getSize();
    const sf::Vector2f pos  = entity.getPosition();
    const float barWidth  = size.x;
    const float barHeight = 4.0f;
    const float yOffset   = -size.y / 2.0f - 8.0f;

    // Background (red)
    sf::RectangleShape bgBar(sf::Vector2f(barWidth, barHeight));
    bgBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
    bgBar.setPosition(sf::Vector2f(pos.x, pos.y + yOffset));
    bgBar.setFillColor(sf::Color(180, 0, 0));
    target.draw(bgBar);

    // Filled portion (green)
    const float healthPct = static_cast<float>(entity.getHealth()) /
                            static_cast<float>(entity.getMaxHealth());
    sf::RectangleShape healthBar(sf::Vector2f(barWidth * healthPct, barHeight));
    healthBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
    healthBar.setPosition(sf::Vector2f(pos.x, pos.y + yOffset));
    healthBar.setFillColor(sf::Color(0, 180, 0));
    target.draw(healthBar);
}

void drawSelectionIndicator(sf::RenderTarget& target, const Entity& entity) {
    bool showIndicator = entity.isSelected();

    // Blink while highlighted
    if (entity.isHighlighted() && entity.isHighlightBlinkVisible())
        showIndicator = true;

    if (!showIndicator) return;

    const sf::Color ringColor = entity.isLocalTeam() ? sf::Color::Green : sf::Color::Red;
    const float radius = entity.getSize().x / 2.0f + 4.0f;

    sf::CircleShape circle(radius);
    circle.setOrigin(sf::Vector2f(radius, radius));
    circle.setPosition(entity.getPosition());
    circle.setFillColor(sf::Color::Transparent);
    circle.setOutlineThickness(2.0f);
    circle.setOutlineColor(ringColor);
    target.draw(circle);
}

} // namespace EntityDrawing
