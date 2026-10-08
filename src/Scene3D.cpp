#include <GL/glew.h>
#include "Scene3D.h"
#include "Map.h"
#include "Constants.h"
#include "TerrainTiling.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace Scene3D {

const char* sceneVertexSource() {
    return R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
uniform mat4 uViewProj;
out vec3 vNormal;
out vec3 vColor;
out vec3 vWorld;
void main() {
    vNormal = aNormal;
    vColor  = aColor;
    vWorld  = aPos;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";
}

const char* sceneFragmentSource() {
    return R"(#version 330 core
in vec3 vNormal;
in vec3 vColor;
in vec3 vWorld;
uniform vec3 uLightDir;
uniform float uAlpha;
uniform float uGridSize;
out vec4 FragColor;
void main() {
    // A zero normal marks flat, unlit geometry (selection rings, health bars).
    float light = 1.0;
    if (dot(vNormal, vNormal) > 0.001) {
        float diffuse = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
        light = 0.45 + 0.55 * diffuse;
    }
    vec3 color = vColor * light;
    if (uGridSize > 0.0) {
        vec2 g = vWorld.xz / uGridSize;
        vec2 w = max(fwidth(g), vec2(1e-5));
        vec2 d = abs(fract(g - 0.5) - 0.5) / w;      // distance to the nearest border, in pixels
        float line = 1.0 - min(min(d.x, d.y), 1.0);
        color = mix(color, color * 0.55, line * 0.7);
    }
    FragColor = vec4(color, uAlpha);
}
)";
}

glm::vec3 lightDirection() {
    return glm::vec3(-0.4f, 1.0f, 0.5f);
}

glm::vec3 toVec3(const sf::Color& c) {
    return glm::vec3(c.r, c.g, c.b) / 255.0f;
}

void endGLPass(sf::RenderWindow& window) {
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    window.resetGLStates();
}

void initGL(sf::RenderWindow& window) {
    if (!window.setActive(true))
        throw std::runtime_error("cannot activate the window's OpenGL context");

    const sf::ContextSettings settings = window.getSettings();
    if (settings.majorVersion < 3)
        throw std::runtime_error("OpenGL 3.x context required, got "
                                 + std::to_string(settings.majorVersion) + "."
                                 + std::to_string(settings.minorVersion));
    if (settings.depthBits == 0)
        throw std::runtime_error("window has no depth buffer");

    glewExperimental = GL_TRUE;
    const GLenum err = glewInit();
    glGetError();  // glewInit may leave a spurious GL_INVALID_ENUM behind
    if (err != GLEW_OK)
        throw std::runtime_error(std::string("glewInit failed: ")
                                 + reinterpret_cast<const char*>(glewGetErrorString(err)));
}

void buildTerrainMesh(const Map& map, MeshData& data, MeshData* fogGrid, TexturedTerrain* textured) {
    const float ts = static_cast<float>(Constants::TILE_SIZE);
    const int   mw = map.getWidth();
    const int   mh = map.getHeight();

    // Vertex heights and smooth normals (central differences) on the corner grid.
    const int vw = mw + 1;
    const int vh = mh + 1;
    std::vector<float> heights(static_cast<size_t>(vw * vh));
    for (int vy = 0; vy < vh; ++vy)
        for (int vx = 0; vx < vw; ++vx)
            heights[static_cast<size_t>(vy * vw + vx)] = map.getVertexHeight(vx, vy);

    auto heightAt = [&](int vx, int vy) {
        vx = std::clamp(vx, 0, vw - 1);
        vy = std::clamp(vy, 0, vh - 1);
        return heights[static_cast<size_t>(vy * vw + vx)];
    };
    auto normalAt = [&](int vx, int vy) {
        const float dx = (heightAt(vx + 1, vy) - heightAt(vx - 1, vy)) / (2.0f * ts);
        const float dz = (heightAt(vx, vy + 1) - heightAt(vx, vy - 1)) / (2.0f * ts);
        return glm::normalize(glm::vec3(-dx, 1.0f, -dz));
    };

    data.clear();
    data.vertices.reserve(static_cast<size_t>(mw * mh) * 4);
    data.indices.reserve(static_cast<size_t>(mw * mh) * 6);

    constexpr float kFogLift = 0.3f;
    if (fogGrid) {
        fogGrid->clear();
        fogGrid->vertices.reserve(static_cast<size_t>(mw * mh) * 4);
        fogGrid->indices.reserve(static_cast<size_t>(mw * mh) * 6);
    }
    if (textured) {
        textured->base.clear();
        textured->overlay.clear();
        textured->base.vertices.reserve(static_cast<size_t>(mw * mh) * 4);
        textured->base.indices.reserve(static_cast<size_t>(mw * mh) * 6);
    }

    // Appends one tile quad to `mesh`, `colors` being the colour slot of each corner
    // (NW, NE, SE, SW; game x right, y down).  Lit quads get the smooth terrain
    // normals; unlit ones (fog) a zero normal.
    using Corners = std::array<glm::vec3, 4>;
    auto addTile = [&](MeshData& mesh, int x, int y, float heightLift, bool lit, const Corners& colors) {
        const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
        const int vx[4] = { x, x + 1, x + 1, x };
        const int vy[4] = { y, y,     y + 1, y + 1 };
        for (int i = 0; i < 4; ++i) {
            mesh.vertices.push_back({
                { static_cast<float>(vx[i]) * ts, heightAt(vx[i], vy[i]) + heightLift, static_cast<float>(vy[i]) * ts },
                lit ? normalAt(vx[i], vy[i]) : glm::vec3(0.0f),
                colors[static_cast<size_t>(i)] });
        }
        // Counter-clockwise seen from above (+Y).
        mesh.indices.insert(mesh.indices.end(), {base, base + 3, base + 2, base, base + 2, base + 1});
    };

    // Colour slot of a textured tile corner: image UV and texture layer.
    auto imageCorners = [](int layer) {
        const float l = static_cast<float>(layer);
        return Corners{ glm::vec3(0.0f, 0.0f, l), glm::vec3(1.0f, 0.0f, l),
                        glm::vec3(1.0f, 1.0f, l), glm::vec3(0.0f, 1.0f, l) };
    };

    for (int y = 0; y < mh; ++y) {
        for (int x = 0; x < mw; ++x) {
            const Tile& tile = map.getTile(x, y);
            const bool isWater = tile.type == TileType::Water;

            if (textured && (isWater || !map.isCliffTile(x, y))) {
                const TerrainTiling::TileArt art = TerrainTiling::describe(map, x, y);
                addTile(textured->base, x, y, 0.0f, true, imageCorners(art.base));
                if (art.overlay != TerrainTiling::kNone)
                    addTile(textured->overlay, x, y, 0.0f, true, imageCorners(art.overlay));
            } else {
                const float shade = 1.0f + static_cast<float>((tile.variant * 37) % 9 - 4) * 0.012f;
                glm::vec3 color;
                if (isWater)                    color = glm::vec3(0.16f, 0.36f, 0.68f);
                else if (map.isCliffTile(x, y)) color = glm::vec3(0.45f, 0.42f, 0.38f);
                else                            color = glm::vec3(0.28f, 0.52f, 0.24f);
                // Higher ground is slightly lighter so relief reads at a glance.
                const float lift = 1.0f + static_cast<float>(map.getTileElevation(x, y)) * 0.05f;
                color = glm::min(color * shade * lift, glm::vec3(1.0f));
                addTile(data, x, y, 0.0f, true, { color, color, color, color });
            }

            if (fogGrid) {
                auto fogUV = [&](int vx, int vy) {
                    return glm::vec3(static_cast<float>(vx) / static_cast<float>(mw),
                                     static_cast<float>(vy) / static_cast<float>(mh), 0.0f);
                };
                addTile(*fogGrid, x, y, kFogLift, false,
                        { fogUV(x, y), fogUV(x + 1, y), fogUV(x + 1, y + 1), fogUV(x, y + 1) });
            }
        }
    }
}

} // namespace Scene3D
