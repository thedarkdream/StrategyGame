#pragma once

// ---------------------------------------------------------------------------
// Camera — backend-neutral description of the visible part of the world.
//
// The camera is a plain value type: a focus point on the ground plane
// (world units) plus the extent of ground it covers.  It knows nothing about
// SFML views or OpenGL matrices; each renderer builds whatever it needs from
// these values (see CameraSFML.h for the 2D backend and Camera3D.h for the 3D
// one).  Camera::screenToWorld() is the top-down mapping; the 3D renderer ray
// casts against the terrain instead (Camera3D::screenToWorld).
// ---------------------------------------------------------------------------

#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Rect.hpp>

class Camera {
public:
    // Ground-plane point the camera is centred on (world units).
    sf::Vector2f getCenter() const { return m_center; }
    void setCenter(sf::Vector2f center) { m_center = center; }
    void move(sf::Vector2f delta) { m_center += delta; }

    // Extent of ground covered by the screen (world units).
    sf::Vector2f getSize() const { return m_size; }
    void setSize(sf::Vector2f size) { m_size = size; }

    // Axis-aligned world rectangle currently visible.
    sf::FloatRect getVisibleRect() const {
        return sf::FloatRect(m_center - m_size / 2.0f, m_size);
    }

    // Converts a window pixel into a ground-plane world position.
    sf::Vector2f screenToWorld(sf::Vector2i pixel, sf::Vector2u windowSize) const {
        const sf::Vector2f topLeft = m_center - m_size / 2.0f;
        return sf::Vector2f(
            topLeft.x + static_cast<float>(pixel.x) / static_cast<float>(windowSize.x) * m_size.x,
            topLeft.y + static_cast<float>(pixel.y) / static_cast<float>(windowSize.y) * m_size.y);
    }

private:
    sf::Vector2f m_center{0.0f, 0.0f};
    sf::Vector2f m_size{1.0f, 1.0f};
};
