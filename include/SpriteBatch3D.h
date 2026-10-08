#pragma once

// ---------------------------------------------------------------------------
// SpriteBatch3D — textured quads in the 3D scene (entity billboards, effects,
// ground markers), collected per frame, depth-sorted and drawn in as few calls
// as the textures allow.
//
// Sprites are tagged "post-fog": the pre-fog set (remembered buildings,
// resource nodes) is drawn first, then the fog overlay darkens it, then the
// post-fog set (everything currently visible) goes on top.
// ---------------------------------------------------------------------------

#include "AnimatedSprite.h"
#include "GLShader.h"
#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <unordered_set>
#include <vector>

class SpriteBatch3D {
public:
    // Needs a current GL context with loaded entry points (Scene3D::initGL).
    SpriteBatch3D();
    ~SpriteBatch3D();

    SpriteBatch3D(const SpriteBatch3D&)            = delete;
    SpriteBatch3D& operator=(const SpriteBatch3D&) = delete;

    void begin();

    // Adds a quad showing `frame`.  Corners are given top-left, top-right,
    // bottom-right, bottom-left of the image.  `depth` orders sprites within a
    // pass: larger is drawn later (closer to the camera).
    void addQuad(const SpriteFrame& frame, const std::array<glm::vec3, 4>& corners,
                 const glm::vec4& tint, bool postFog, float depth);

    // Sorts and uploads what was added since begin().
    void end();

    void draw(const glm::mat4& viewProj, bool postFog);

private:
    struct Vertex {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec4 tint;
    };
    struct Pending {
        unsigned int          texture;
        bool                  postFog;
        float                 depth;
        std::array<Vertex, 6> verts;
    };
    struct Batch {
        unsigned int texture;
        bool         postFog;
        int          first;
        int          count;
    };

    void prepareTexture(unsigned int handle, bool mipmaps);

    std::unique_ptr<GLShader>        m_shader;
    std::vector<Pending>             m_pending;
    std::vector<Vertex>              m_verts;
    std::vector<Batch>               m_batches;
    std::unordered_set<unsigned int> m_preparedTextures;
    unsigned int                     m_vao = 0;
    unsigned int                     m_vbo = 0;
};
