#pragma once

// ---------------------------------------------------------------------------
// Scene3D — pieces shared by the 3D game renderer (Renderer3D) and the 3D map
// editor view (EditorView3D): GL context set-up and the terrain mesh.
//
// Axis convention: game (x, y) -> GL (x, height, y).
// ---------------------------------------------------------------------------

#include "GLMesh.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <glm/glm.hpp>

class Map;

namespace Scene3D {

// Activates the window's GL context and loads the GL entry points.
// Throws std::runtime_error when 3D is unusable (old context, no depth buffer,
// loader failure); callers fall back to the 2D view.
void initGL(sf::RenderWindow& window);

// Tiles that are drawn with terrain art instead of a flat colour.  Both meshes
// use the colour slot of Vertex3D as (u, v, texture layer), the layer being a
// TerrainTiling image id.
struct TexturedTerrain {
    MeshData base;      // opaque tile image
    MeshData overlay;   // transparent image over the base (water notches on grass)
};

// Builds the terrain: one quad per tile on shared corner heights with smooth
// normals.  When `fogGrid` is given it also receives the same grid, lifted a
// hair above the ground, whose colour slot carries UVs (x / mapW, y / mapH)
// into a map-sized texture.
//
// Without `textured`, every tile goes to `terrain` with a flat colour.  With it,
// water and walkable tiles go to `textured` and only cliffs (steep slopes) stay
// in `terrain` as flat-coloured quads.
void buildTerrainMesh(const Map& map, MeshData& terrain, MeshData* fogGrid = nullptr,
                      TexturedTerrain* textured = nullptr);

// Sources of the lit-geometry shader (zero normal = unlit).  Uniforms: uViewProj,
// uLightDir, uAlpha and uGridSize (> 0 overlays a tile grid of that size; 0 = off).
const char* sceneVertexSource();
const char* sceneFragmentSource();

// Direction toward the light, for the uLightDir uniform.
glm::vec3 lightDirection();

// sf::Color -> RGB in [0, 1].
glm::vec3 toVec3(const sf::Color& c);

// Leaves the GL state clean for SFML's own drawing (program/VAO/buffers unbound,
// depth test off, GL states reset).  Call after the last 3D draw of a frame.
void endGLPass(sf::RenderWindow& window);

} // namespace Scene3D
