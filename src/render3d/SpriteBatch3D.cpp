#include <GL/glew.h>
#include "render3d/SpriteBatch3D.h"
#include <SFML/Graphics/Texture.hpp>
#include <algorithm>
#include <cstddef>

namespace {

const char* kVertexSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aTint;
uniform mat4 uViewProj;
out vec2 vUV;
out vec4 vTint;
void main() {
    vUV   = aUV;
    vTint = aTint;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

const char* kFragmentSrc = R"(#version 330 core
in vec2 vUV;
in vec4 vTint;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
    vec4 c = texture(uTex, vUV) * vTint;
    if (c.a < 0.02) discard;
    FragColor = c;
}
)";

} // namespace

SpriteBatch3D::SpriteBatch3D()
    : m_shader(std::make_unique<GLShader>(kVertexSrc, kFragmentSrc))
{
}

SpriteBatch3D::~SpriteBatch3D() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    m_shader.reset();
}

void SpriteBatch3D::begin() {
    m_pending.clear();
    m_verts.clear();
    m_batches.clear();
}

void SpriteBatch3D::addQuad(const SpriteFrame& frame, const std::array<glm::vec3, 4>& corners,
                            const glm::vec4& tint, bool postFog, float depth) {
    if (!frame.texture) return;

    const sf::Vector2u ts = frame.texture->getSize();
    const float tw = static_cast<float>(ts.x);
    const float th = static_cast<float>(ts.y);
    const float u0 = static_cast<float>(frame.rect.position.x) / tw;
    const float u1 = static_cast<float>(frame.rect.position.x + frame.rect.size.x) / tw;
    const float v0 = static_cast<float>(frame.rect.position.y) / th;
    const float v1 = static_cast<float>(frame.rect.position.y + frame.rect.size.y) / th;

    const Vertex tl{ corners[0], { u0, v0 }, tint };
    const Vertex tr{ corners[1], { u1, v0 }, tint };
    const Vertex br{ corners[2], { u1, v1 }, tint };
    const Vertex bl{ corners[3], { u0, v1 }, tint };

    const unsigned int handle = frame.texture->getNativeHandle();
    prepareTexture(handle, frame.rect.size.x == static_cast<int>(ts.x)
                        && frame.rect.size.y == static_cast<int>(ts.y));
    m_pending.push_back({ handle, postFog, depth, { bl, br, tr, bl, tr, tl } });
}

void SpriteBatch3D::end() {
    // Pre-fog pass first, then post-fog; each back to front so overlapping
    // translucent pixels blend correctly.
    std::stable_sort(m_pending.begin(), m_pending.end(),
                     [](const Pending& a, const Pending& b) {
                         if (a.postFog != b.postFog) return !a.postFog;
                         return a.depth < b.depth;
                     });
    m_verts.reserve(m_pending.size() * 6);
    for (const Pending& s : m_pending) {
        if (m_batches.empty() || m_batches.back().texture != s.texture
            || m_batches.back().postFog != s.postFog)
            m_batches.push_back({ s.texture, s.postFog, static_cast<int>(m_verts.size()), 0 });
        m_verts.insert(m_verts.end(), s.verts.begin(), s.verts.end());
        m_batches.back().count += 6;
    }

    if (!m_vao) {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        const GLsizei stride = sizeof(Vertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, uv)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex, tint)));
    } else {
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    }
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_verts.size() * sizeof(Vertex)),
                 m_verts.data(), GL_STREAM_DRAW);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void SpriteBatch3D::draw(const glm::mat4& viewProj, bool postFog) {
    if (m_batches.empty() || !m_vao) return;

    glBindVertexArray(m_vao);

    m_shader->use();
    m_shader->setMat4("uViewProj", viewProj);
    m_shader->setInt("uTex", 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glActiveTexture(GL_TEXTURE0);
    for (const Batch& batch : m_batches) {
        if (batch.postFog != postFog) continue;
        glBindTexture(GL_TEXTURE_2D, batch.texture);
        glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glBindVertexArray(0);
}

// Sprites are scaled down a lot; filter them (and mip-map whole-image textures,
// but not sprite sheets whose frames would bleed into each other).
void SpriteBatch3D::prepareTexture(unsigned int handle, bool mipmaps) {
    if (!m_preparedTextures.insert(handle).second) return;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, handle);
    if (mipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}
