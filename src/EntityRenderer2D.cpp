#include "EntityRenderer2D.h"
#include "Entity.h"
#include "Unit.h"
#include "Worker.h"
#include "Building.h"
#include "Turret.h"
#include "LightTank.h"
#include "Projectile.h"
#include "ResourceNode.h"
#include "EntityDrawing.h"
#include "TextureManager.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cstdint>

namespace {

// Coloured rectangle used when an entity has no sprite.
sf::RectangleShape makeFallbackShape(const Entity& e, sf::Color fill) {
    const sf::Vector2f size = e.getSize();
    sf::RectangleShape shape(size);
    shape.setOrigin(sf::Vector2f(size.x / 2.0f, size.y / 2.0f));
    shape.setPosition(e.getPosition());
    shape.setFillColor(fill);
    shape.setOutlineThickness(1.0f);
    shape.setOutlineColor(sf::Color::Black);
    return shape;
}

void drawBody(sf::RenderTarget& target, Entity& e) {
    if (e.hasSprite()) {
        EntityRenderer2D::drawAnimatedSprite(target, e.getAnimatedSprite(), e.getPosition());
    } else {
        target.draw(makeFallbackShape(e, e.getColor()));
    }
}

void drawUnit(sf::RenderTarget& target, Unit& unit) {
    drawBody(target, unit);
    EntityDrawing::drawSelectionIndicator(target, unit);
    EntityDrawing::drawHealthBar(target, unit);
}

void drawWorker(sf::RenderTarget& target, Worker& worker) {
    drawUnit(target, worker);

    // Carried resources indicator
    if (worker.getCarriedResources() <= 0) return;

    const sf::Vector2f pos  = worker.getPosition();
    const sf::Vector2f size = worker.getSize();
    const sf::Vector2f indicatorPos(pos.x + size.x / 2.0f, pos.y - size.y / 2.0f);

    static sf::Texture* mineralTex = TextureManager::instance().loadTexture("resources/mineral_carried.png");
    if (mineralTex) {
        sf::Sprite indicator(*mineralTex);
        // Display at a fixed 12x12 pixel size regardless of source resolution
        float scale = 12.0f / static_cast<float>(std::max(mineralTex->getSize().x, mineralTex->getSize().y));
        indicator.setScale(sf::Vector2f(scale, scale));
        indicator.setOrigin(sf::Vector2f(mineralTex->getSize().x / 2.0f,
                                         mineralTex->getSize().y / 2.0f));
        indicator.setPosition(indicatorPos);
        target.draw(indicator);
    } else {
        // Fallback cyan dot if texture not found
        sf::CircleShape resourceIndicator(4.0f);
        resourceIndicator.setOrigin(sf::Vector2f(4.0f, 4.0f));
        resourceIndicator.setPosition(indicatorPos);
        resourceIndicator.setFillColor(sf::Color::Cyan);
        target.draw(resourceIndicator);
    }
}

void drawLightTank(sf::RenderTarget& target, LightTank& tank) {
    const float radius = tank.getSize().x / 2.0f;

    // Selection ring (colour depends on ownership)
    if (tank.isSelected()) {
        sf::CircleShape ring;
        ring.setRadius(radius + 3.0f);
        ring.setOrigin({radius + 3.0f, radius + 3.0f});
        ring.setPosition(tank.getPosition());
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(tank.isLocalTeam() ? sf::Color(0, 220, 0) : sf::Color(220, 0, 0));
        ring.setOutlineThickness(2.0f);
        target.draw(ring);
    }

    sf::CircleShape circle;
    circle.setRadius(radius);
    circle.setOrigin({radius, radius});
    circle.setPosition(tank.getPosition());
    circle.setFillColor(tank.getColor());
    circle.setOutlineColor(sf::Color::Black);
    circle.setOutlineThickness(1.5f);
    target.draw(circle);

    EntityDrawing::drawHealthBar(target, tank);
}

void drawProjectile(sf::RenderTarget& target, Projectile& projectile) {
    if (!projectile.isAlive()) return;

    sf::CircleShape circle(3.0f);
    circle.setOrigin({3.0f, 3.0f});
    circle.setPosition(projectile.getPosition());
    circle.setFillColor(projectile.getColor());
    circle.setOutlineColor(sf::Color(200, 150, 0));
    circle.setOutlineThickness(1.0f);
    target.draw(circle);
}

void drawResourceNode(sf::RenderTarget& target, ResourceNode& node) {
    drawBody(target, node);
    EntityDrawing::drawSelectionIndicator(target, node);
}

void drawBuilding(sf::RenderTarget& target, Building& building) {
    if (building.hasSprite()) {
        EntityRenderer2D::drawAnimatedSprite(target, building.getAnimatedSprite(), building.getPosition());
    } else {
        // Fade the placeholder while under construction
        sf::Color fill = building.getColor();
        if (!building.isConstructed()) {
            fill.a = static_cast<std::uint8_t>(128 + 127 * building.getConstructionProgress());
        }
        target.draw(makeFallbackShape(building, fill));
    }

    EntityDrawing::drawSelectionIndicator(target, building);
    EntityDrawing::drawHealthBar(target, building);
}

void drawTurret(sf::RenderTarget& target, Turret& turret) {
    const sf::Texture* tex = turret.getCurrentTexture();
    if (!tex) return;

    sf::Sprite sprite(*tex);
    const sf::FloatRect bounds = sprite.getLocalBounds();
    sprite.setOrigin({bounds.size.x / 2.f, bounds.size.y / 2.f});
    sprite.setScale({0.25f, 0.25f});
    sprite.setPosition(turret.getPosition());

    // Semi-transparent while under construction
    if (!turret.isConstructed()) {
        sprite.setColor(sf::Color(255, 255, 255,
            static_cast<std::uint8_t>(128 + static_cast<int>(127 * turret.getConstructionProgress()))));
    }
    target.draw(sprite);

    EntityDrawing::drawSelectionIndicator(target, turret);
    EntityDrawing::drawHealthBar(target, turret);
}

} // namespace

namespace EntityRenderer2D {

void drawAnimatedSprite(sf::RenderTarget& target, const AnimatedSprite& animation,
                        sf::Vector2f position, sf::Color tint) {
    const sf::Texture* texture = animation.getCurrentTexture();
    if (!texture) return;

    sf::Sprite sprite(*texture);
    sprite.setTextureRect(animation.getCurrentTextureRect());
    sprite.setOrigin(animation.getOrigin());
    sprite.setScale(animation.getScale());
    sprite.setColor(tint);
    sprite.setPosition(position);
    target.draw(sprite);
}

void draw(sf::RenderTarget& target, Entity& entity) {
    // Most-derived types first.
    if (auto* p = dynamic_cast<Projectile*>(&entity))   { drawProjectile(target, *p);   return; }
    if (auto* t = dynamic_cast<Turret*>(&entity))       { drawTurret(target, *t);       return; }
    if (auto* l = dynamic_cast<LightTank*>(&entity))    { drawLightTank(target, *l);    return; }
    if (auto* r = entity.asResourceNode())              { drawResourceNode(target, *r); return; }
    if (auto* b = entity.asBuilding())                  { drawBuilding(target, *b);     return; }
    if (auto* w = entity.asWorker())                    { drawWorker(target, *w);       return; }
    if (auto* u = entity.asUnit())                      { drawUnit(target, *u);         return; }
}

void drawBuildingPreview(sf::RenderTarget& target, Building& building, sf::Color tint) {
    if (building.hasSprite()) {
        drawAnimatedSprite(target, building.getAnimatedSprite(), building.getPosition(), tint);
    } else {
        target.draw(makeFallbackShape(building, tint));
    }

    // Construction / production progress bar
    if (!building.isConstructed() || building.isProducing()) {
        const float progress = building.isConstructed() ? building.getProductionProgress()
                                                        : building.getConstructionProgress();
        const sf::Vector2f size = building.getSize();
        const sf::Vector2f pos  = building.getPosition();

        const float barWidth  = size.x * 0.8f;
        const float barHeight = 6.0f;
        const float yOffset   = size.y / 2.0f + 10.0f;

        sf::RectangleShape bgBar(sf::Vector2f(barWidth, barHeight));
        bgBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
        bgBar.setPosition(sf::Vector2f(pos.x, pos.y + yOffset));
        bgBar.setFillColor(sf::Color(50, 50, 50));
        target.draw(bgBar);

        sf::RectangleShape progressBar(sf::Vector2f(barWidth * progress, barHeight));
        progressBar.setOrigin(sf::Vector2f(barWidth / 2.0f, barHeight / 2.0f));
        progressBar.setPosition(sf::Vector2f(pos.x, pos.y + yOffset));
        progressBar.setFillColor(sf::Color(255, 200, 0));
        target.draw(progressBar);
    }
}

} // namespace EntityRenderer2D
