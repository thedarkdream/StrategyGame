#pragma once

// ---------------------------------------------------------------------------
// Model — a 3D model on the GPU: one indexed mesh per material, the bounds of
// the whole model (used for placement and picking) and, for skinned models, a
// skeleton with its animation clips.
//
// Model space follows glTF: +Y up, +Z is the front of the model, 1 unit is a
// metre.  Renderers scale and turn it into the game's world (see Placement3D).
//
// Skeletal animation: pose() samples a clip into one matrix per joint (model
// space, already multiplied by the inverse bind matrix); the vertex shader
// blends them by each vertex's joint weights.
// ---------------------------------------------------------------------------

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace sf { class Texture; }

struct ModelVertex {
    glm::vec3    position;
    glm::vec3    normal;
    glm::vec2    uv;
    glm::u16vec4 joints{0};         // skeleton joints influencing the vertex (skinned parts only)
    glm::vec4    weights{0.0f};     // their weights, summing to 1
};

// CPU-side mesh of one material, as produced by the model loader.
struct ModelPartData {
    std::vector<ModelVertex>   vertices;
    std::vector<std::uint32_t> indices;
    glm::vec4                  baseColor{1.0f};
    const sf::Texture*         texture    = nullptr;   // owned by TextureManager
    bool                       teamTinted = false;     // multiplied with the owner's team colour
    bool                       doubleSided = false;
    bool                       skinned    = false;     // vertices carry joints / weights
};

struct ModelJoint {
    std::string name;
    int         parent = -1;               // index into the joint list, -1 for a root joint
    glm::mat4   parentTransform{1.0f};     // for a root joint: model-space transform of its non-joint parent
    glm::mat4   inverseBind{1.0f};
    glm::vec3   restTranslation{0.0f};     // local pose when no clip drives the joint
    glm::quat   restRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3   restScale{1.0f};
};

struct ModelChannel {
    enum class Path { Translation, Rotation, Scale };
    int                    joint = 0;
    Path                   path  = Path::Translation;
    bool                   step  = false;       // hold each value until the next key instead of blending
    std::vector<float>     times;               // ascending, seconds
    std::vector<glm::vec4> values;              // xyz (translation / scale) or xyzw (rotation quaternion)
};

struct ModelClip {
    std::string               name;
    float                     duration = 0.0f;
    std::vector<ModelChannel> channels;
};

struct ModelSkeleton {
    std::vector<ModelJoint> joints;
    std::vector<ModelClip>  clips;
};

class Model {
public:
    // The most joints a skinned model may have (the size of the shader's matrix array).
    static constexpr int kMaxJoints = 128;

    struct Part {
        unsigned int       vao = 0, vbo = 0, ebo = 0;
        int                indexCount = 0;
        glm::vec4          baseColor{1.0f};
        const sf::Texture* texture = nullptr;
        bool               teamTinted = false;
        bool               doubleSided = false;
        bool               skinned = false;
    };

    // Uploads the parts.  Needs a current GL context with loaded entry points.
    explicit Model(const std::vector<ModelPartData>& parts, ModelSkeleton skeleton = {});
    ~Model();

    Model(const Model&)            = delete;
    Model& operator=(const Model&) = delete;

    const std::vector<Part>& parts() const { return m_parts; }

    // Bounds of every vertex in model space (bind pose).
    const glm::vec3& boundsMin() const { return m_boundsMin; }
    const glm::vec3& boundsMax() const { return m_boundsMax; }

    // Skeletal animation.
    bool             skinned() const { return !m_skeleton.joints.empty(); }
    const ModelClip* findClip(const std::string& name) const;   // nullptr when there is no such clip

    // Fills `out` (one matrix per joint) with the pose of `clip` at `time`
    // seconds; a null clip gives the rest pose.  A looping clip wraps around,
    // otherwise time is clamped to the clip's end.
    void pose(const ModelClip* clip, float time, bool loop, std::vector<glm::mat4>& out) const;

private:
    std::vector<Part> m_parts;
    ModelSkeleton     m_skeleton;
    glm::vec3         m_boundsMin{0.0f};
    glm::vec3         m_boundsMax{0.0f};
};
