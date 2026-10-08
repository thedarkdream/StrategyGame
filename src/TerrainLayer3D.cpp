#include <GL/glew.h>
#include "TerrainLayer3D.h"
#include "Scene3D.h"
#include "TerrainTiling.h"
#include "Constants.h"
#include "TextureManager.h"
#include "FogOfWar.h"
#include "Map.h"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

// The fog mesh reuses Vertex3D: its colour slot carries the fog-texture UV.
const char* kFogVertexSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aUV;
uniform mat4 uViewProj;
out vec2 vUV;
void main() {
    vUV = aUV.xy;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

const char* kFogFragmentSrc = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
    FragColor = texture(uTex, vUV);
}
)";

// Textured terrain: the colour slot of Vertex3D carries (u, v, texture layer).
const char* kTerrainVertexSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aUVLayer;
uniform mat4 uViewProj;
out vec3 vNormal;
out vec3 vUVLayer;
out vec3 vWorld;
void main() {
    vNormal  = aNormal;
    vUVLayer = aUVLayer;
    vWorld   = aPos;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

// Shared by the terrain and cliff fragment shaders: darkens the tile borders
// when gridSize > 0 (map editor).
const char* kGridGlsl = R"(
vec3 applyGrid(vec3 color, vec3 world, float gridSize) {
    if (gridSize <= 0.0) return color;
    vec2 g = world.xz / gridSize;
    vec2 w = max(fwidth(g), vec2(1e-5));
    vec2 d = abs(fract(g - 0.5) - 0.5) / w;      // distance to the nearest border, in pixels
    float line = 1.0 - min(min(d.x, d.y), 1.0);
    return mix(color, color * 0.55, line * 0.7);
}
)";

const std::string kTerrainFragmentSrc = std::string("#version 330 core\n") + kGridGlsl + R"(
in vec3 vNormal;
in vec3 vUVLayer;
in vec3 vWorld;
uniform sampler2DArray uTerrain;
uniform vec3 uLightDir;
uniform float uGridSize;
out vec4 FragColor;
void main() {
    vec4 texel = texture(uTerrain, vUVLayer);
    float diffuse = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
    float light = 0.45 + 0.55 * diffuse;
    FragColor = vec4(applyGrid(texel.rgb * light, vWorld, uGridSize), texel.a);
}
)";

// Cliffs: triplanar mapping (the rock is projected along X, Y and Z and the
// three samples are blended by the surface orientation), so steep faces are
// not stretched.  The projection weights use the true face normal; lighting
// uses the smooth vertex normal like the rest of the terrain.
const char* kCliffVertexSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uViewProj;
out vec3 vNormal;
out vec3 vWorld;
void main() {
    vNormal = aNormal;
    vWorld  = aPos;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

const std::string kCliffFragmentSrc = std::string("#version 330 core\n") + kGridGlsl + R"(
in vec3 vNormal;
in vec3 vWorld;
uniform sampler2D uRock;
uniform vec3 uLightDir;
uniform float uRockScale;     // texture repeats per world unit
uniform float uGridSize;
out vec4 FragColor;
void main() {
    vec3 face = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
    vec3 w = pow(abs(face), vec3(4.0));
    w /= (w.x + w.y + w.z);

    vec3 p = vWorld * uRockScale;
    vec3 rock = texture(uRock, p.zy).rgb * w.x     // faces looking along X
              + texture(uRock, p.xz).rgb * w.y     // faces looking up / down
              + texture(uRock, p.xy).rgb * w.z;    // faces looking along Z

    float diffuse = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
    float light = 0.45 + 0.55 * diffuse;
    FragColor = vec4(applyGrid(rock * light, vWorld, uGridSize), 1.0);
}
)";

} // namespace

TerrainLayer3D::TerrainLayer3D()
    : m_cliffShader(std::make_unique<GLShader>(kCliffVertexSrc, kCliffFragmentSrc))
    , m_terrainShader(std::make_unique<GLShader>(kTerrainVertexSrc, kTerrainFragmentSrc))
    , m_fogShader(std::make_unique<GLShader>(kFogVertexSrc, kFogFragmentSrc))
{
    loadTerrainArt();
    loadCliffArt();
}

TerrainLayer3D::~TerrainLayer3D() {
    if (m_terrainArray) glDeleteTextures(1, &m_terrainArray);
}

// The rock texture stays owned by TextureManager; the shader samples its GL
// handle directly (like the fog texture), so it only needs repeat wrapping and
// mipmaps switched on.
void TerrainLayer3D::loadCliffArt() {
    sf::Texture* rock = TEXTURES.loadTexture(TerrainTiling::kCliffTexturePath);
    if (!rock) return;   // TextureManager already reported it; cliffs render black
    rock->setRepeated(true);
    rock->setSmooth(true);
    if (!rock->generateMipmap())
        std::cerr << "TerrainLayer3D: cannot generate mipmaps for " << TerrainTiling::kCliffTexturePath << "\n";
    m_cliffTexture = rock->getNativeHandle();
}

// Loads every TerrainTiling image into one layer of a texture array (so tiles
// can pick their image per vertex, with no atlas bleeding between neighbours).
void TerrainLayer3D::loadTerrainArt() {
    constexpr unsigned int kSize = 64;   // every terrain image is 64x64
    const int layers = TerrainTiling::textureCount();

    std::vector<std::uint8_t> pixels(static_cast<size_t>(layers) * kSize * kSize * 4);
    for (int id = 0; id < layers; ++id) {
        std::uint8_t* dst = pixels.data() + static_cast<size_t>(id) * kSize * kSize * 4;

        const sf::Texture* texture = TEXTURES.loadTexture(TerrainTiling::texturePath(id));
        const sf::Image image = texture ? texture->copyToImage() : sf::Image();
        if (texture && image.getSize() == sf::Vector2u(kSize, kSize)) {
            std::copy(image.getPixelsPtr(), image.getPixelsPtr() + kSize * kSize * 4, dst);
        } else {
            std::cerr << "TerrainLayer3D: cannot use " << TerrainTiling::texturePath(id)
                      << " (missing or not " << kSize << "x" << kSize << ")\n";
            for (unsigned int i = 0; i < kSize * kSize; ++i) {   // magenta marks the gap
                dst[i * 4 + 0] = 255; dst[i * 4 + 1] = 0; dst[i * 4 + 2] = 255; dst[i * 4 + 3] = 255;
            }
        }
    }

    glGenTextures(1, &m_terrainArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainArray);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kSize, kSize, layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void TerrainLayer3D::update(const Map& map) {
    if (!m_dirty) return;
    MeshData cliffs;
    MeshData fogGrid;
    Scene3D::TexturedTerrain textured;
    Scene3D::buildTerrainMesh(map, cliffs, &fogGrid, &textured);
    m_cliffMesh.upload(cliffs);
    m_groundMesh.upload(textured.base);
    m_overlayMesh.upload(textured.overlay);
    m_fogMesh.upload(fogGrid);
    m_dirty = false;
}

void TerrainLayer3D::drawTerrain(const glm::mat4& viewProj, const glm::vec3& lightDir, float gridSize) {
    // Textured ground, then its transparent overlays one polygon-offset step above it.
    m_terrainShader->use();
    m_terrainShader->setMat4("uViewProj", viewProj);
    m_terrainShader->setVec3("uLightDir", lightDir);
    m_terrainShader->setInt("uTerrain", 0);
    m_terrainShader->setFloat("uGridSize", gridSize);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_terrainArray);
    m_groundMesh.draw();

    // The grid is already on the ground; overlays must not darken it twice.
    m_terrainShader->setFloat("uGridSize", 0.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    m_overlayMesh.draw();
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // Cliffs: triplanar rock, one texture repeat per 3 tiles.
    m_cliffShader->use();
    m_cliffShader->setMat4("uViewProj", viewProj);
    m_cliffShader->setVec3("uLightDir", lightDir);
    m_cliffShader->setInt("uRock", 0);
    m_cliffShader->setFloat("uRockScale", 1.0f / (3.0f * Constants::TILE_SIZE));
    m_cliffShader->setFloat("uGridSize", gridSize);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_cliffTexture);
    m_cliffMesh.draw();
    glBindTexture(GL_TEXTURE_2D, 0);
}

void TerrainLayer3D::drawFog(const glm::mat4& viewProj, const FogOfWar& fog) {
    const unsigned int handle = fog.getFogTexture().getNativeHandle();
    if (!handle) return;

    m_fogShader->use();
    m_fogShader->setMat4("uViewProj", viewProj);
    m_fogShader->setInt("uTex", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, handle);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);
    m_fogMesh.draw();
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
}
