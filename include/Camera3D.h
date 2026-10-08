#pragma once

// ---------------------------------------------------------------------------
// Camera3D — projection math shared by Renderer3D (drawing) and the picking
// code (mouse -> world).  Header-only.
//
// The fixed-angle camera looks at the terrain point under Camera::getCenter().
// Axis convention: game (x, y) -> GL (x, height, y).
// ---------------------------------------------------------------------------

#include "Camera.h"
#include "Map.h"
#include "Constants.h"
#include <SFML/System/Vector2.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Camera3D {

constexpr float kPitchDegrees = 55.0f;   // camera angle above the ground plane
constexpr float kFovDegrees   = 45.0f;   // vertical field of view

struct Ray {
    glm::vec3 origin;
    glm::vec3 dir;   // normalised
};

// Screen-aligned axes in world space (the camera never rolls or yaws), used to
// orient billboards so they face the camera.
inline glm::vec3 billboardRight() { return glm::vec3(1.0f, 0.0f, 0.0f); }
inline glm::vec3 billboardUp() {
    const float pitch = glm::radians(kPitchDegrees);
    return glm::vec3(0.0f, std::cos(pitch), -std::sin(pitch));
}

inline glm::mat4 viewProjection(const Camera& camera, float aspect, float targetHeight) {
    const float pitch = glm::radians(kPitchDegrees);
    const float fov   = glm::radians(kFovDegrees);

    // Distance chosen so the ground covered vertically at the screen centre
    // matches the Camera's visible height.
    const float distance = camera.getSize().y * std::sin(pitch) / (2.0f * std::tan(fov * 0.5f));

    const sf::Vector2f c = camera.getCenter();
    const glm::vec3 target(c.x, targetHeight, c.y);
    const glm::vec3 eye = target + glm::vec3(0.0f, distance * std::sin(pitch), distance * std::cos(pitch));

    const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 proj = glm::perspective(fov, aspect, 10.0f, 10000.0f);
    return proj * view;
}

// View-projection for a window, with the look-at height taken from the map.
inline glm::mat4 viewProjection(const Camera& camera, sf::Vector2u windowSize, const Map& map) {
    return viewProjection(camera,
                          static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y),
                          map.getHeightAt(camera.getCenter()));
}

// World-space ray through a window pixel.
inline Ray pixelRay(const Camera& camera, sf::Vector2i pixel, sf::Vector2u windowSize, const Map& map) {
    const glm::mat4 inv = glm::inverse(viewProjection(camera, windowSize, map));
    const float nx =  2.0f * static_cast<float>(pixel.x) / static_cast<float>(windowSize.x) - 1.0f;
    const float ny = -2.0f * static_cast<float>(pixel.y) / static_cast<float>(windowSize.y) + 1.0f;

    glm::vec4 nearP = inv * glm::vec4(nx, ny, -1.0f, 1.0f);
    glm::vec4 farP  = inv * glm::vec4(nx, ny,  1.0f, 1.0f);
    const glm::vec3 a = glm::vec3(nearP) / nearP.w;
    const glm::vec3 b = glm::vec3(farP)  / farP.w;
    return { a, glm::normalize(b - a) };
}

// Ground position (world x, y) under a window pixel: the ray is marched down
// through the terrain's height band and refined by bisection.
inline sf::Vector2f screenToWorld(const Camera& camera, sf::Vector2i pixel,
                                  sf::Vector2u windowSize, const Map& map) {
    const Ray ray = pixelRay(camera, pixel, windowSize, map);
    auto heightAt = [&](float t) {
        const glm::vec3 p = ray.origin + ray.dir * t;
        return p.y - map.getHeightAt(sf::Vector2f(p.x, p.z));
    };
    auto groundPoint = [&](float t) {
        const glm::vec3 p = ray.origin + ray.dir * t;
        return sf::Vector2f(p.x, p.z);
    };

    if (ray.dir.y >= -1e-4f) return groundPoint(0.0f);   // looking at the sky

    const float maxHeight = static_cast<float>(Constants::MAX_ELEVATION) * Constants::ELEVATION_STEP;
    const float tTop    = std::max(0.0f, (ray.origin.y - maxHeight) / -ray.dir.y);
    const float tBottom = ray.origin.y / -ray.dir.y;         // reaches height 0
    const float step    = static_cast<float>(Constants::TILE_SIZE) * 0.25f;

    float prev = tTop;
    for (float t = tTop; t < tBottom; t += step) {
        if (heightAt(t) <= 0.0f) {
            float lo = prev, hi = t;
            for (int i = 0; i < 16; ++i) {
                const float mid = 0.5f * (lo + hi);
                if (heightAt(mid) > 0.0f) lo = mid; else hi = mid;
            }
            return groundPoint(0.5f * (lo + hi));
        }
        prev = t;
    }
    return groundPoint(tBottom);
}

// Window pixel of a world-space 3D point; returns false when behind the camera.
inline bool projectPoint(const Camera& camera, glm::vec3 point, sf::Vector2u windowSize,
                         const Map& map, sf::Vector2f& out) {
    const glm::vec4 clip = viewProjection(camera, windowSize, map) * glm::vec4(point, 1.0f);
    if (clip.w <= 0.0f) return false;
    const float nx = clip.x / clip.w;
    const float ny = clip.y / clip.w;
    out.x = (nx * 0.5f + 0.5f) * static_cast<float>(windowSize.x);
    out.y = (1.0f - (ny * 0.5f + 0.5f)) * static_cast<float>(windowSize.y);
    return true;
}

// Slab test against an axis-aligned box.  On a hit, `tOut` is the distance
// along the ray to the entry point.
inline bool rayHitsBox(const Ray& ray, const glm::vec3& lo, const glm::vec3& hi, float& tOut) {
    float tMin = 0.0f;
    float tMax = 1e9f;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(ray.dir[axis]) < 1e-8f) {
            if (ray.origin[axis] < lo[axis] || ray.origin[axis] > hi[axis]) return false;
            continue;
        }
        float t0 = (lo[axis] - ray.origin[axis]) / ray.dir[axis];
        float t1 = (hi[axis] - ray.origin[axis]) / ray.dir[axis];
        if (t0 > t1) std::swap(t0, t1);
        tMin = std::max(tMin, t0);
        tMax = std::min(tMax, t1);
        if (tMin > tMax) return false;
    }
    tOut = tMin;
    return true;
}

} // namespace Camera3D
