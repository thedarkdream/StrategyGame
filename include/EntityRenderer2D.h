#pragma once

// ---------------------------------------------------------------------------
// EntityRenderer2D — SFML drawing of simulation entities.
//
// Entities no longer draw themselves: they only expose read-only visual state
// (team colour, animation state, construction progress, ...).  All SFML
// drawing lives here so a different backend (e.g. a 3D renderer) can replace
// this module without touching any simulation class.
// ---------------------------------------------------------------------------

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>

class Entity;
class Building;
class AnimatedSprite;

namespace EntityRenderer2D {

// Draws the current frame of `sprite` centred on its origin at `position`.
void drawAnimatedSprite(sf::RenderTarget& target, const AnimatedSprite& sprite,
                        sf::Vector2f position, sf::Color tint = sf::Color::White);

// Draws `entity` (body + selection indicator + health bar + extras such as the
// carried-resource icon on workers).
void draw(sf::RenderTarget& target, Entity& entity);

// Draws a tinted placement preview of `building` (no selection / health UI).
void drawBuildingPreview(sf::RenderTarget& target, Building& building, sf::Color tint);

} // namespace EntityRenderer2D
