#pragma once

// ---------------------------------------------------------------------------
// EditorView3D — draws the map editor's map area in 3D: the terrain (the same
// TerrainLayer3D as the game, plus a tile grid), the placed entities as
// team-coloured boxes and the hover / placement highlight.  The 2D SFML UI
// (panel, dialogs) is drawn by the editor afterwards.
// ---------------------------------------------------------------------------

#include "Camera.h"
#include "GLMesh.h"
#include "GLShader.h"
#include "TerrainLayer3D.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <vector>

class Map;

struct EditorBox {
    int          tileX = 0, tileY = 0;     // top-left tile
    int          tilesW = 1, tilesH = 1;   // footprint in tiles
    float        heightTiles = 1.0f;       // box height above the terrain, in tiles
    sf::Color    color;
};

struct EditorHighlight {
    int          tileX = 0, tileY = 0;
    int          tilesW = 1, tilesH = 1;
    sf::Color    color;
};

class EditorView3D {
public:
    // Throws std::runtime_error when 3D is unavailable; the editor stays in 2D.
    explicit EditorView3D(sf::RenderWindow& window);
    ~EditorView3D();

    EditorView3D(const EditorView3D&)            = delete;
    EditorView3D& operator=(const EditorView3D&) = delete;

    // Call whenever tiles / elevations of the map changed.
    void invalidateTerrain() { m_terrain->invalidate(); }

    // Draws into `area` (window pixels, top-left origin); the rest of the window is untouched.
    void render(const Map& map, const Camera& camera, sf::IntRect area,
                const std::vector<EditorBox>& boxes,
                const std::optional<EditorHighlight>& highlight);

private:
    sf::RenderWindow&         m_window;
    std::unique_ptr<GLShader> m_shader;
    std::unique_ptr<TerrainLayer3D> m_terrain;
    GLMesh                    m_boxMesh;
    MeshData                  m_boxData;
    GLMesh                    m_fillMesh;      // translucent highlight on the ground
    MeshData                  m_fillData;
    GLMesh                    m_outlineMesh;   // opaque highlight border
    MeshData                  m_outlineData;
};
