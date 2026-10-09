#pragma once

// ---------------------------------------------------------------------------
// ModelBatch3D — draws glTF models placed in the scene.  It also owns the
// loaded models (GPU resources, released with the renderer's GL context), so a
// model path is loaded once, on first use.
//
// Like SpriteBatch3D it is filled once per frame (begin / add) and drawn in
// two passes: models standing under the fog overlay and models drawn over it.
// ---------------------------------------------------------------------------

#include "GLShader.h"
#include "Model.h"
#include <glm/glm.hpp>
#include <memory>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class ModelBatch3D {
public:
    // Needs a current GL context with loaded entry points (Scene3D::initGL).
    ModelBatch3D();
    ~ModelBatch3D();

    ModelBatch3D(const ModelBatch3D&)            = delete;
    ModelBatch3D& operator=(const ModelBatch3D&) = delete;

    // The model at an assets-relative path, loaded on first use; nullptr when it
    // cannot be loaded (the failure is logged once).
    const Model* get(const std::string& path);

    // Already loaded model, never loads (safe without a GL context): picking uses it.
    const Model* loaded(const std::string& path) const;

    void begin();

    // `transform` takes model space to world space.  `teamColor` tints the
    // team-coloured materials; `brightness` > 1 lightens (selection).  A skinned
    // model is posed from the named clip at `clipTime` seconds (looping); an unknown
    // or empty clip name gives its rest pose.
    void add(const Model& model, const glm::mat4& transform, const glm::vec3& teamColor,
             float brightness, float opacity, bool postFog,
             const std::string& clip = std::string(), float clipTime = 0.0f);

    // Draws the models added for one of the two fog passes.
    void draw(const glm::mat4& viewProj, const glm::vec3& lightDir, bool postFog);

private:
    struct Instance {
        const Model* model;
        glm::mat4    transform;
        glm::vec3    teamColor;
        float        brightness;
        float        opacity;
        bool         postFog;
        size_t       jointOffset;   // first of this instance's matrices in m_jointPool (skinned models)
        int          jointCount;    // 0 = not skinned
    };

    std::unique_ptr<GLShader> m_shader;
    std::unordered_map<std::string, std::unique_ptr<Model>> m_models;   // null = failed to load
    std::vector<Instance>     m_instances;
    std::vector<glm::mat4>    m_jointPool;      // joint matrices of this frame's skinned instances
    std::vector<glm::mat4>    m_poseScratch;
};
