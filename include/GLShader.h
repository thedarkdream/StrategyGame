#pragma once

// ---------------------------------------------------------------------------
// GLShader — minimal RAII wrapper around an OpenGL shader program.
// Requires an initialised GL context (see Renderer3D).
// ---------------------------------------------------------------------------

#include <glm/glm.hpp>
#include <string>

class GLShader {
public:
    // Compiles and links the program.  Throws std::runtime_error with the
    // driver's info log on failure.
    GLShader(const std::string& vertexSource, const std::string& fragmentSource);
    ~GLShader();

    GLShader(const GLShader&)            = delete;
    GLShader& operator=(const GLShader&) = delete;

    void use() const;
    void setMat4(const char* name, const glm::mat4& value) const;
    void setVec3(const char* name, const glm::vec3& value) const;
    void setFloat(const char* name, float value) const;
    void setInt(const char* name, int value) const;

private:
    unsigned int m_program = 0;
    int uniformLocation(const char* name) const;
};
