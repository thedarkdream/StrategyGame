#pragma once

// ---------------------------------------------------------------------------
// TerrainLayer3D — the heightmap terrain and the fog-of-war overlay lying on
// it.  Walkable ground and water use the same terrain art as the 2D view
// (TerrainTiling); steep slopes (cliffs) use a triplanar-mapped rock texture.
// All meshes are rebuilt together when the map changes.
// ---------------------------------------------------------------------------

#include "GLMesh.h"
#include "GLShader.h"
#include <glm/glm.hpp>
#include <memory>

class Map;
class FogOfWar;

class TerrainLayer3D {
public:
    // Needs a current GL context with loaded entry points (Scene3D::initGL).
    TerrainLayer3D();
    ~TerrainLayer3D();

    TerrainLayer3D(const TerrainLayer3D&)            = delete;
    TerrainLayer3D& operator=(const TerrainLayer3D&) = delete;

    // The tiles or heights changed: rebuild the meshes before the next draw.
    void invalidate() { m_dirty = true; }

    // Rebuilds the meshes when invalidated.
    void update(const Map& map);

    // Textured ground, then the rock cliffs.  `gridSize` > 0 overlays a
    // tile grid of that size (map editor).  Leaves its own shader bound; the
    // caller sets up whatever it draws next.
    void drawTerrain(const glm::mat4& viewProj, const glm::vec3& lightDir, float gridSize = 0.0f);

    // Shroud / darkness over the terrain, sampled from the fog-of-war texture.
    void drawFog(const glm::mat4& viewProj, const FogOfWar& fog);

private:
    void loadTerrainArt();
    void loadCliffArt();

    std::unique_ptr<GLShader> m_cliffShader;     // triplanar rock
    std::unique_ptr<GLShader> m_terrainShader;   // textured tiles
    std::unique_ptr<GLShader> m_fogShader;
    unsigned int              m_terrainArray = 0;   // GL_TEXTURE_2D_ARRAY, one layer per TerrainTiling image
    unsigned int              m_cliffTexture = 0;   // GL handle of TextureManager's rock texture (not owned)
    GLMesh                    m_cliffMesh;
    GLMesh                    m_groundMesh;
    GLMesh                    m_overlayMesh;
    GLMesh                    m_fogMesh;   // terrain-hugging grid textured with the fog map
    bool                      m_dirty = true;
};
