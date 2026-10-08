#include "Placement3D.h"
#include "EntityVisual.h"
#include "Map.h"
#include <algorithm>
#include <cmath>

namespace Placement3D {

Span footprintSpan(const Map& map, sf::Vector2f pos, sf::Vector2f size) {
    const float hx = size.x * 0.5f;
    const float hz = size.y * 0.5f;
    float lo = map.getHeightAt(pos);
    float hi = lo;
    for (const sf::Vector2f& corner : { sf::Vector2f(pos.x - hx, pos.y - hz), sf::Vector2f(pos.x + hx, pos.y - hz),
                                        sf::Vector2f(pos.x + hx, pos.y + hz), sf::Vector2f(pos.x - hx, pos.y + hz) }) {
        const float h = map.getHeightAt(corner);
        lo = std::min(lo, h);
        hi = std::max(hi, h);
    }
    return { lo, hi - lo };
}

Span bodySpan(const EntityVisual& visual, const Map& map, sf::Vector2f pos, sf::Vector2f size) {
    const Span span = footprintSpan(map, pos, size);
    return { span.baseY, span.height + visual.bodyHeight };
}

Billboard placeBillboard(const Map& map, sf::Vector2f pos, sf::Vector2f spriteSize, float anchor) {
    const float sinPitch = std::sin(glm::radians(Camera3D::kPitchDegrees));
    const float shift = anchor * spriteSize.y / sinPitch;
    const float baseY = std::max(map.getHeightAt(pos),
                                 map.getHeightAt(sf::Vector2f(pos.x, pos.y + shift)));
    return { glm::vec3(pos.x, baseY, pos.y + shift), spriteSize.x * 0.5f, spriteSize.y };
}

bool rayHitsBillboard(const Camera3D::Ray& ray, const Billboard& quad, float& t) {
    const glm::vec3 right = Camera3D::billboardRight();
    const glm::vec3 up    = Camera3D::billboardUp();
    const glm::vec3 normal = glm::cross(right, up);
    const float denom = glm::dot(ray.dir, normal);
    if (std::abs(denom) < 1e-6f) return false;
    t = glm::dot(quad.bottom - ray.origin, normal) / denom;
    if (t < 0.0f) return false;
    const glm::vec3 local = ray.origin + ray.dir * t - quad.bottom;
    const float a = glm::dot(local, right);
    const float b = glm::dot(local, up);
    return std::abs(a) <= quad.halfWidth && b >= 0.0f && b <= quad.height;
}

} // namespace Placement3D
