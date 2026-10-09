#pragma once

// SFML-specific helpers for Camera (2D backend only).

#include "render/Camera.h"
#include <SFML/Graphics/View.hpp>

inline sf::View toSfView(const Camera& camera) {
    return sf::View(camera.getCenter(), camera.getSize());
}
