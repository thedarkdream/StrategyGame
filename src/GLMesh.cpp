#include <GL/glew.h>
#include "GLMesh.h"
#include <cstddef>

// ---------------------------------------------------------------------------
// MeshData
// ---------------------------------------------------------------------------

void MeshData::addGroundQuad(float x, float z, float w, float d, float y, const glm::vec3& color) {
    const auto base = static_cast<std::uint32_t>(vertices.size());
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    vertices.push_back({{x,     y, z    }, up, color});
    vertices.push_back({{x + w, y, z    }, up, color});
    vertices.push_back({{x + w, y, z + d}, up, color});
    vertices.push_back({{x,     y, z + d}, up, color});
    // Counter-clockwise when viewed from above (+Y).
    indices.insert(indices.end(), {base, base + 3, base + 2, base, base + 2, base + 1});
}

void MeshData::addUnlitQuad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                            const glm::vec3& d, const glm::vec3& color) {
    const auto base = static_cast<std::uint32_t>(vertices.size());
    const glm::vec3 none(0.0f);
    vertices.push_back({a, none, color});
    vertices.push_back({b, none, color});
    vertices.push_back({c, none, color});
    vertices.push_back({d, none, color});
    indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

void MeshData::addBox(float cx, float cz, float sizeX, float sizeZ, float height,
                      const glm::vec3& color, float baseY) {
    const float hx = sizeX * 0.5f;
    const float hz = sizeZ * 0.5f;
    const float y0 = baseY;
    const float y1 = baseY + height;

    struct Face { glm::vec3 n; glm::vec3 p[4]; };  // p[] counter-clockwise seen from outside
    const Face faces[6] = {
        {{ 0, 1, 0}, {{cx-hx, y1, cz-hz}, {cx-hx, y1, cz+hz}, {cx+hx, y1, cz+hz}, {cx+hx, y1, cz-hz}}},  // top
        {{ 0,-1, 0}, {{cx-hx, y0, cz-hz}, {cx+hx, y0, cz-hz}, {cx+hx, y0, cz+hz}, {cx-hx, y0, cz+hz}}},  // bottom
        {{ 0, 0, 1}, {{cx-hx, y0, cz+hz}, {cx+hx, y0, cz+hz}, {cx+hx, y1, cz+hz}, {cx-hx, y1, cz+hz}}},  // south (+Z)
        {{ 0, 0,-1}, {{cx+hx, y0, cz-hz}, {cx-hx, y0, cz-hz}, {cx-hx, y1, cz-hz}, {cx+hx, y1, cz-hz}}},  // north (-Z)
        {{ 1, 0, 0}, {{cx+hx, y0, cz+hz}, {cx+hx, y0, cz-hz}, {cx+hx, y1, cz-hz}, {cx+hx, y1, cz+hz}}},  // east (+X)
        {{-1, 0, 0}, {{cx-hx, y0, cz-hz}, {cx-hx, y0, cz+hz}, {cx-hx, y1, cz+hz}, {cx-hx, y1, cz-hz}}},  // west (-X)
    };

    for (const Face& f : faces) {
        const auto base = static_cast<std::uint32_t>(vertices.size());
        for (const glm::vec3& p : f.p) vertices.push_back({p, f.n, color});
        indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
}

// ---------------------------------------------------------------------------
// GLMesh
// ---------------------------------------------------------------------------

GLMesh::~GLMesh() {
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

void GLMesh::upload(const MeshData& data, bool dynamic) {
    if (!m_vao) {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);

        const GLsizei stride = sizeof(Vertex3D);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex3D, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex3D, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(Vertex3D, color)));
    } else {
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    }

    const GLenum usage = dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW;
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.vertices.size() * sizeof(Vertex3D)),
                 data.vertices.data(), usage);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.indices.size() * sizeof(std::uint32_t)),
                 data.indices.data(), usage);
    m_indexCount = static_cast<int>(data.indices.size());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLMesh::draw() const {
    if (!m_vao || m_indexCount == 0) return;
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
