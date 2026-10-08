#pragma once

// ---------------------------------------------------------------------------
// GLMesh — indexed triangle mesh (position, normal, colour) on the GPU, plus a
// small CPU-side MeshData builder with primitive helpers.
//
// World-to-GL axis convention used throughout the 3D renderer:
//   game (x, y)  ->  GL (x, height, y)      i.e. GL Y is up, game y is GL Z.
// ---------------------------------------------------------------------------

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

struct Vertex3D {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

struct MeshData {
    std::vector<Vertex3D>      vertices;
    std::vector<std::uint32_t> indices;

    void clear() { vertices.clear(); indices.clear(); }

    // Horizontal quad on the ground (normal +Y) covering [x, x+w] x [z, z+d] at height y.
    void addGroundQuad(float x, float z, float w, float d, float y, const glm::vec3& color);

    // Flat, unlit quad (zero normal = full brightness) from four corners in order.
    void addUnlitQuad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                      const glm::vec3& d, const glm::vec3& color);

    // Axis-aligned box standing on the ground, centred on (cx, cz).
    void addBox(float cx, float cz, float sizeX, float sizeZ, float height,
                const glm::vec3& color, float baseY = 0.0f);
};

class GLMesh {
public:
    GLMesh() = default;
    ~GLMesh();

    GLMesh(const GLMesh&)            = delete;
    GLMesh& operator=(const GLMesh&) = delete;

    // Uploads the data.  Pass dynamic = true for meshes rebuilt every frame.
    void upload(const MeshData& data, bool dynamic = false);
    void draw() const;

private:
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_ebo = 0;
    int          m_indexCount = 0;
};
