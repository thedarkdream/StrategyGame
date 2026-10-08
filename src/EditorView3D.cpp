#include <GL/glew.h>
#include "EditorView3D.h"
#include "Scene3D.h"
#include "Camera3D.h"
#include "Constants.h"
#include "Map.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <limits>

namespace {

using Scene3D::toVec3;

// Lowest and highest terrain corner under a tile footprint.
void footprintHeights(const Map& map, int tx, int ty, int tw, int th, float& lo, float& hi) {
    lo = std::numeric_limits<float>::max();
    hi = std::numeric_limits<float>::lowest();
    for (int vy = ty; vy <= ty + th; ++vy) {
        for (int vx = tx; vx <= tx + tw; ++vx) {
            const float h = map.getVertexHeight(vx, vy);
            lo = std::min(lo, h);
            hi = std::max(hi, h);
        }
    }
}

// Terrain-hugging translucent fill plus a border ribbon around a tile footprint.
void addHighlight(const Map& map, const EditorHighlight& hl, MeshData& fill, MeshData& outline) {
    const float ts = static_cast<float>(Constants::TILE_SIZE);
    const glm::vec3 color = toVec3(hl.color);
    constexpr float kLift = 0.8f;
    const int x0 = std::max(hl.tileX, 0);
    const int y0 = std::max(hl.tileY, 0);
    const int x1 = std::min(hl.tileX + hl.tilesW, map.getWidth());
    const int y1 = std::min(hl.tileY + hl.tilesH, map.getHeight());
    if (x1 <= x0 || y1 <= y0) return;

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            auto corner = [&](int vx, int vy) {
                return glm::vec3(static_cast<float>(vx) * ts,
                                 map.getVertexHeight(vx, vy) + kLift,
                                 static_cast<float>(vy) * ts);
            };
            fill.addUnlitQuad(corner(x, y), corner(x, y + 1), corner(x + 1, y + 1), corner(x + 1, y), color);
        }
    }

    // Border: ribbons centred on the footprint edge, sampled per tile.
    const float half = 1.5f;
    auto surface = [&](float x, float z) {
        return glm::vec3(x, map.getHeightAt(sf::Vector2f(x, z)) + kLift + 0.4f, z);
    };
    auto ribbon = [&](float ax, float az, float bx, float bz, float nx, float nz) {
        outline.addUnlitQuad(surface(ax - nx * half, az - nz * half), surface(ax + nx * half, az + nz * half),
                             surface(bx + nx * half, bz + nz * half), surface(bx - nx * half, bz - nz * half),
                             glm::vec3(1.0f));
    };
    const float left = static_cast<float>(x0) * ts, right = static_cast<float>(x1) * ts;
    const float top  = static_cast<float>(y0) * ts, bottom = static_cast<float>(y1) * ts;
    for (int x = x0; x < x1; ++x) {
        const float a = static_cast<float>(x) * ts, b = a + ts;
        ribbon(a, top,    b, top,    0.0f, 1.0f);
        ribbon(a, bottom, b, bottom, 0.0f, 1.0f);
    }
    for (int y = y0; y < y1; ++y) {
        const float a = static_cast<float>(y) * ts, b = a + ts;
        ribbon(left,  a, left,  b, 1.0f, 0.0f);
        ribbon(right, a, right, b, 1.0f, 0.0f);
    }
}

} // namespace

EditorView3D::EditorView3D(sf::RenderWindow& window)
    : m_window(window)
{
    Scene3D::initGL(m_window);
    m_shader = std::make_unique<GLShader>(Scene3D::sceneVertexSource(), Scene3D::sceneFragmentSource());
    m_terrain = std::make_unique<TerrainLayer3D>();
}

EditorView3D::~EditorView3D() {
    // GL objects must be released with the window's context current.
    m_window.setActive(true);
    m_terrain.reset();
    m_shader.reset();
}

void EditorView3D::render(const Map& map, const Camera& camera, sf::IntRect area,
                          const std::vector<EditorBox>& boxes,
                          const std::optional<EditorHighlight>& highlight) {
    if (area.size.x <= 0 || area.size.y <= 0) return;
    m_window.setActive(true);

    m_terrain->update(map);

    // Placed entities.
    const float ts = static_cast<float>(Constants::TILE_SIZE);
    m_boxData.clear();
    for (const EditorBox& b : boxes) {
        float lo, hi;
        footprintHeights(map, b.tileX, b.tileY, b.tilesW, b.tilesH, lo, hi);
        const float inset = 2.0f;
        m_boxData.addBox((static_cast<float>(b.tileX) + b.tilesW * 0.5f) * ts,
                         (static_cast<float>(b.tileY) + b.tilesH * 0.5f) * ts,
                         static_cast<float>(b.tilesW) * ts - inset * 2.0f,
                         static_cast<float>(b.tilesH) * ts - inset * 2.0f,
                         (hi - lo) + b.heightTiles * ts, toVec3(b.color), lo);
    }
    const bool hasBoxes = !m_boxData.indices.empty();
    if (hasBoxes) m_boxMesh.upload(m_boxData, true);

    // Hover / placement highlight.
    m_fillData.clear();
    m_outlineData.clear();
    if (highlight) {
        addHighlight(map, *highlight, m_fillData, m_outlineData);
        glm::vec3 color = toVec3(highlight->color);
        for (Vertex3D& v : m_fillData.vertices) v.color = color;
    }
    const bool hasHighlight = !m_fillData.indices.empty();
    if (hasHighlight) {
        m_fillMesh.upload(m_fillData, true);
        m_outlineMesh.upload(m_outlineData, true);
    }

    // ── 3D pass, confined to the map area ───────────────────────────────────
    const int winH = static_cast<int>(m_window.getSize().y);
    const GLint   vx = area.position.x;
    const GLint   vy = winH - area.position.y - area.size.y;
    const GLsizei vw = area.size.x;
    const GLsizei vh = area.size.y;
    glViewport(vx, vy, vw, vh);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vx, vy, vw, vh);
    glClearColor(15.0f / 255.0f, 17.0f / 255.0f, 22.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    const glm::mat4 viewProj = Camera3D::viewProjection(
        camera, sf::Vector2u(static_cast<unsigned>(area.size.x), static_cast<unsigned>(area.size.y)), map);
    m_terrain->drawTerrain(viewProj, Scene3D::lightDirection(), ts);

    m_shader->use();
    m_shader->setMat4("uViewProj", viewProj);
    m_shader->setVec3("uLightDir", Scene3D::lightDirection());
    m_shader->setFloat("uAlpha", 1.0f);
    m_shader->setFloat("uGridSize", 0.0f);
    if (hasBoxes) m_boxMesh.draw();

    if (hasHighlight) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        m_shader->setFloat("uAlpha", 0.4f);
        m_fillMesh.draw();
        m_shader->setFloat("uAlpha", 0.9f);
        m_outlineMesh.draw();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    // ── Hand the context back to SFML for the 2D UI ─────────────────────────
    glDisable(GL_SCISSOR_TEST);
    Scene3D::endGLPass(m_window);
}
