#include <GL/glew.h>
#include "gl/GLShader.h"
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <vector>

namespace {

unsigned int compile(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len > 0 ? len : 1));
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error(std::string("Shader compile error: ") + log.data());
    }
    return shader;
}

} // namespace

GLShader::GLShader(const std::string& vertexSource, const std::string& fragmentSource) {
    GLuint vs = compile(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = 0;
    try {
        fs = compile(GL_FRAGMENT_SHADER, fragmentSource);
    } catch (...) {
        glDeleteShader(vs);
        throw;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len > 0 ? len : 1));
        glGetProgramInfoLog(m_program, len, nullptr, log.data());
        glDeleteProgram(m_program);
        m_program = 0;
        throw std::runtime_error(std::string("Shader link error: ") + log.data());
    }
}

GLShader::~GLShader() {
    if (m_program) glDeleteProgram(m_program);
}

void GLShader::use() const {
    glUseProgram(m_program);
}

int GLShader::uniformLocation(const char* name) const {
    return glGetUniformLocation(m_program, name);
}

void GLShader::setMat4(const char* name, const glm::mat4& value) const {
    glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void GLShader::setMat4Array(const char* name, const glm::mat4* values, int count) const {
    glUniformMatrix4fv(uniformLocation(name), count, GL_FALSE, glm::value_ptr(*values));
}

void GLShader::setVec3(const char* name, const glm::vec3& value) const {
    glUniform3fv(uniformLocation(name), 1, glm::value_ptr(value));
}

void GLShader::setVec4(const char* name, const glm::vec4& value) const {
    glUniform4fv(uniformLocation(name), 1, glm::value_ptr(value));
}

void GLShader::setFloat(const char* name, float value) const {
    glUniform1f(uniformLocation(name), value);
}

void GLShader::setInt(const char* name, int value) const {
    glUniform1i(uniformLocation(name), value);
}
