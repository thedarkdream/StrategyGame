#pragma once

// ---------------------------------------------------------------------------
// Placement3D — where an entity stands in the 3D scene: the ground it covers,
// the camera-facing quad its sprite is drawn on, and the ray test against that
// quad.  Shared by drawing and picking so both agree on what is under the mouse.
//
// Axis convention: game (x, y) -> GL (x, height, y).
// ---------------------------------------------------------------------------

#include "Camera3D.h"
#include <SFML/System/Vector2.hpp>
#include <glm/glm.hpp>

class Map;
struct EntityVisual;

namespace Placement3D {

// Vertical extent of a stand-in box: its base on the lowest ground under the
// footprint, tall enough to cover any slope and then `bodyHeight` more.
struct Span {
    float baseY;
    float height;
};

// Lowest ground under a footprint and the relief above it.
Span footprintSpan(const Map& map, sf::Vector2f pos, sf::Vector2f size);

// Stand-in box of an entity without artwork (also its picking volume).
Span bodySpan(const EntityVisual& visual, const Map& map, sf::Vector2f pos, sf::Vector2f size);

// A sprite's camera-facing quad: its bottom edge on the ground, shifted toward
// the camera so the image's ground point lands on the entity.
struct Billboard {
    glm::vec3 bottom;      // middle of the bottom edge
    float     halfWidth;
    float     height;
};

Billboard placeBillboard(const Map& map, sf::Vector2f pos, sf::Vector2f spriteSize, float anchor);

// Ray vs. billboard; t is the distance along the (unit) ray direction.
bool rayHitsBillboard(const Camera3D::Ray& ray, const Billboard& quad, float& t);

} // namespace Placement3D
