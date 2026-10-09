#include "render3d/Placement3D.h"
#include "render/EntityVisual.h"
#include "render3d/Model.h"
#include "world/Map.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Placement3D {

ModelPlacement placeModel(const Model& model, const EntityVisual& visual, const Map& map, sf::Vector2f pos) {
    // A model faces +Z; rotating by (pi/2 - facing) about Y turns that onto the
    // game-plane direction (cos facing, sin facing).
    glm::mat4 transform = glm::translate(glm::mat4(1.0f),
        glm::vec3(pos.x, map.getHeightAt(pos) + visual.flyHeight, pos.y));
    transform = glm::rotate(transform, glm::half_pi<float>() - visual.facing + visual.modelYaw, glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::scale(transform, glm::vec3(visual.modelScale));

    const glm::vec3 a = model.boundsMin();
    const glm::vec3 b = model.boundsMax();
    glm::vec3 lo(std::numeric_limits<float>::max());
    glm::vec3 hi(std::numeric_limits<float>::lowest());
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 corner((i & 1) ? b.x : a.x, (i & 2) ? b.y : a.y, (i & 4) ? b.z : a.z);
        const glm::vec3 world = glm::vec3(transform * glm::vec4(corner, 1.0f));
        lo = glm::min(lo, world);
        hi = glm::max(hi, world);
    }
    return { transform, lo, hi };
}

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
