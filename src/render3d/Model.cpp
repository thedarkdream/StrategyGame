#include <GL/glew.h>
#include "render3d/Model.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

Model::Model(const std::vector<ModelPartData>& parts, ModelSkeleton skeleton)
    : m_skeleton(std::move(skeleton))
{
    bool first = true;
    for (const ModelPartData& data : parts) {
        if (data.vertices.empty() || data.indices.empty()) continue;

        for (const ModelVertex& v : data.vertices) {
            if (first) { m_boundsMin = m_boundsMax = v.position; first = false; }
            m_boundsMin = glm::min(m_boundsMin, v.position);
            m_boundsMax = glm::max(m_boundsMax, v.position);
        }

        Part part;
        part.indexCount  = static_cast<int>(data.indices.size());
        part.baseColor   = data.baseColor;
        part.texture     = data.texture;
        part.teamTinted  = data.teamTinted;
        part.doubleSided = data.doubleSided;
        part.skinned     = data.skinned;

        glGenVertexArrays(1, &part.vao);
        glGenBuffers(1, &part.vbo);
        glGenBuffers(1, &part.ebo);

        glBindVertexArray(part.vao);
        glBindBuffer(GL_ARRAY_BUFFER, part.vbo);
        glBufferData(GL_ARRAY_BUFFER, data.vertices.size() * sizeof(ModelVertex), data.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, part.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.indices.size() * sizeof(std::uint32_t), data.indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, uv)));
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 4, GL_UNSIGNED_SHORT, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, joints)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), reinterpret_cast<void*>(offsetof(ModelVertex, weights)));
        glBindVertexArray(0);

        m_parts.push_back(part);
    }
}

Model::~Model() {
    for (const Part& part : m_parts) {
        glDeleteBuffers(1, &part.vbo);
        glDeleteBuffers(1, &part.ebo);
        glDeleteVertexArrays(1, &part.vao);
    }
}

const ModelClip* Model::findClip(const std::string& name) const {
    for (const ModelClip& clip : m_skeleton.clips)
        if (clip.name == name) return &clip;
    return nullptr;
}

namespace {

// Value of a channel at `time`: blended between the two surrounding keys.
glm::vec4 sampleChannel(const ModelChannel& channel, float time) {
    const std::vector<float>& times = channel.times;
    const size_t next = static_cast<size_t>(std::upper_bound(times.begin(), times.end(), time) - times.begin());
    if (next == 0) return channel.values.front();
    if (next == times.size()) return channel.values.back();

    const size_t prev = next - 1;
    if (channel.step) return channel.values[prev];

    const float span = times[next] - times[prev];
    const float blend = span > 0.0f ? (time - times[prev]) / span : 0.0f;
    if (channel.path == ModelChannel::Path::Rotation) {
        const glm::vec4& a = channel.values[prev];
        const glm::vec4& b = channel.values[next];
        const glm::quat q = glm::slerp(glm::quat(a.w, a.x, a.y, a.z), glm::quat(b.w, b.x, b.y, b.z), blend);
        return glm::vec4(q.x, q.y, q.z, q.w);
    }
    return glm::mix(channel.values[prev], channel.values[next], blend);
}

} // namespace

void Model::pose(const ModelClip* clip, float time, bool loop, std::vector<glm::mat4>& out) const {
    const size_t count = std::min(m_skeleton.joints.size(), static_cast<size_t>(kMaxJoints));
    out.resize(count);

    std::array<glm::vec3, kMaxJoints> translation;
    std::array<glm::quat, kMaxJoints> rotation;
    std::array<glm::vec3, kMaxJoints> scale;
    for (size_t i = 0; i < count; ++i) {
        translation[i] = m_skeleton.joints[i].restTranslation;
        rotation[i]    = m_skeleton.joints[i].restRotation;
        scale[i]       = m_skeleton.joints[i].restScale;
    }

    if (clip && clip->duration > 0.0f) {
        if (loop) {
            time = std::fmod(time, clip->duration);
            if (time < 0.0f) time += clip->duration;
        } else {
            time = std::clamp(time, 0.0f, clip->duration);
        }
        for (const ModelChannel& channel : clip->channels) {
            if (channel.times.empty() || channel.values.size() != channel.times.size()) continue;
            if (channel.joint < 0 || static_cast<size_t>(channel.joint) >= count) continue;
            const glm::vec4 value = sampleChannel(channel, time);
            switch (channel.path) {
                case ModelChannel::Path::Translation: translation[channel.joint] = glm::vec3(value); break;
                case ModelChannel::Path::Rotation:    rotation[channel.joint]    = glm::normalize(glm::quat(value.w, value.x, value.y, value.z)); break;
                case ModelChannel::Path::Scale:       scale[channel.joint]       = glm::vec3(value); break;
            }
        }
    }

    // Joint matrices in model space, parents first (the joint list need not be ordered).
    std::array<glm::mat4, kMaxJoints> global;
    std::array<bool, kMaxJoints> done{};
    for (size_t remaining = count; remaining > 0;) {
        bool progressed = false;
        for (size_t i = 0; i < count; ++i) {
            const ModelJoint& joint = m_skeleton.joints[i];
            if (done[i] || (joint.parent >= 0 && !done[joint.parent])) continue;

            const glm::mat4 local = glm::translate(glm::mat4(1.0f), translation[i])
                                  * glm::mat4_cast(rotation[i])
                                  * glm::scale(glm::mat4(1.0f), scale[i]);
            global[i] = (joint.parent >= 0 ? global[joint.parent] : joint.parentTransform) * local;
            done[i] = true;
            progressed = true;
            --remaining;
        }
        if (!progressed) break;   // malformed hierarchy: leave the rest at identity
    }
    for (size_t i = 0; i < count; ++i)
        out[i] = done[i] ? global[i] * m_skeleton.joints[i].inverseBind : glm::mat4(1.0f);
}
