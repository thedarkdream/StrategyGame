#pragma once

// ---------------------------------------------------------------------------
// Overlay3D — flat helper geometry drawn over the scene: selection rings,
// health bars, rally-point markers and the build-placement preview.
//
// "flat" meshes receive unlit geometry (zero normals), "solid" ones lit boxes.
// ---------------------------------------------------------------------------

#include "gl/GLMesh.h"
#include <SFML/System/Vector2.hpp>
#include <glm/glm.hpp>

class Game;
class Map;

namespace Overlay3D {

// Ring lying on the ground around `pos`.
void addSelectionRing(MeshData& flat, const Map& map, sf::Vector2f pos, float radius, const glm::vec3& color);

// Camera-facing bar centred 8 units above `topPoint`; `fraction` of it is filled.
void addHealthBar(MeshData& flat, const glm::vec3& topPoint, float width, float fraction);

// Line to the rally point and a flag on a pole, for every selected producing building.
void addRallyPoints(Game& game, MeshData& flat, MeshData& solid);

// Translucent box showing where the building being placed would go (green when it fits).
void addBuildPreview(Game& game, MeshData& solid);

} // namespace Overlay3D
