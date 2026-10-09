#include <GL/glew.h>
#include "ModelBatch3D.h"
#include "ModelLoader.h"
#include <SFML/Graphics/Texture.hpp>

namespace {

std::string vertexSource() {
    return std::string(R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in uvec4 aJoints;
layout(location = 4) in vec4 aWeights;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform int  uSkinned;
)") + "uniform mat4 uJoints[" + std::to_string(Model::kMaxJoints) + "];\n" + R"(
out vec3 vNormal;
out vec2 vUV;
void main() {
    vec4 position = vec4(aPos, 1.0);
    vec3 normal = aNormal;
    if (uSkinned != 0) {
        mat4 skin = aWeights.x * uJoints[aJoints.x] + aWeights.y * uJoints[aJoints.y]
                  + aWeights.z * uJoints[aJoints.z] + aWeights.w * uJoints[aJoints.w];
        position = skin * position;
        normal = mat3(skin) * normal;
    }
    vNormal = mat3(uModel) * normal;    // models are only ever scaled uniformly
    vUV = aUV;
    gl_Position = uViewProj * uModel * position;
}
)";
}

const char* kModelFragmentSrc = R"(#version 330 core
in vec3 vNormal;
in vec2 vUV;
uniform sampler2D uTex;
uniform int   uHasTexture;
uniform vec4  uBaseColor;
uniform int   uTeamTinted;
uniform vec3  uTeamColor;
uniform vec3  uLightDir;
uniform float uBrightness;
uniform float uAlpha;
out vec4 FragColor;
void main() {
    vec4 color = uBaseColor;
    if (uHasTexture != 0) color *= texture(uTex, vUV);
    if (uTeamTinted != 0) color.rgb *= uTeamColor;
    float diffuse = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
    float light = 0.45 + 0.55 * diffuse;
    FragColor = vec4(min(color.rgb * light * uBrightness, vec3(1.0)), color.a * uAlpha);
}
)";

} // namespace

ModelBatch3D::ModelBatch3D()
    : m_shader(std::make_unique<GLShader>(vertexSource(), kModelFragmentSrc))
{
}

ModelBatch3D::~ModelBatch3D() = default;

const Model* ModelBatch3D::get(const std::string& path) {
    auto it = m_models.find(path);
    if (it == m_models.end())
        it = m_models.emplace(path, ModelLoader::load(path)).first;
    return it->second.get();
}

const Model* ModelBatch3D::loaded(const std::string& path) const {
    const auto it = m_models.find(path);
    return it == m_models.end() ? nullptr : it->second.get();
}

void ModelBatch3D::begin() {
    m_instances.clear();
    m_jointPool.clear();
}

void ModelBatch3D::add(const Model& model, const glm::mat4& transform, const glm::vec3& teamColor,
                       float brightness, float opacity, bool postFog,
                       const std::string& clip, float clipTime) {
    size_t jointOffset = 0;
    int    jointCount  = 0;
    if (model.skinned()) {
        model.pose(model.findClip(clip), clipTime, true, m_poseScratch);
        jointOffset = m_jointPool.size();
        jointCount  = static_cast<int>(m_poseScratch.size());
        m_jointPool.insert(m_jointPool.end(), m_poseScratch.begin(), m_poseScratch.end());
    }
    m_instances.push_back({ &model, transform, teamColor, brightness, opacity, postFog, jointOffset, jointCount });
}

void ModelBatch3D::draw(const glm::mat4& viewProj, const glm::vec3& lightDir, bool postFog) {
    bool any = false;
    for (const Instance& instance : m_instances) any = any || instance.postFog == postFog;
    if (!any) return;

    m_shader->use();
    m_shader->setMat4("uViewProj", viewProj);
    m_shader->setVec3("uLightDir", lightDir);
    m_shader->setInt("uTex", 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);

    for (const Instance& instance : m_instances) {
        if (instance.postFog != postFog) continue;

        m_shader->setMat4("uModel", instance.transform);
        m_shader->setVec3("uTeamColor", instance.teamColor);
        m_shader->setFloat("uBrightness", instance.brightness);
        m_shader->setFloat("uAlpha", instance.opacity);
        if (instance.jointCount > 0)
            m_shader->setMat4Array("uJoints", &m_jointPool[instance.jointOffset], instance.jointCount);

        for (const Model::Part& part : instance.model->parts()) {
            m_shader->setInt("uSkinned", part.skinned && instance.jointCount > 0 ? 1 : 0);
            m_shader->setVec4("uBaseColor", part.baseColor);
            m_shader->setInt("uTeamTinted", part.teamTinted ? 1 : 0);
            m_shader->setInt("uHasTexture", part.texture ? 1 : 0);
            glBindTexture(GL_TEXTURE_2D, part.texture ? part.texture->getNativeHandle() : 0);

            if (part.doubleSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);

            glBindVertexArray(part.vao);
            glDrawElements(GL_TRIANGLES, part.indexCount, GL_UNSIGNED_INT, nullptr);
        }
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}
