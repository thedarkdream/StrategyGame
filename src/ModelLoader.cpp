#include "ModelLoader.h"
#include "Model.h"
#include "TextureManager.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <unordered_map>
#include <variant>

namespace {

struct LoadContext {
    const fastgltf::Asset&      asset;
    std::string                 path;   // asset-relative path of the model file
    std::string                 dir;    // its folder, with trailing '/'
    std::vector<ModelPartData>  parts;
    ModelSkeleton               skeleton;   // from the first skin; empty when the model has none
    std::unordered_map<size_t, int> nodeToJoint;
};

glm::mat4 nodeMatrix(const fastgltf::Node& node) {
    // Matrices are decomposed while parsing (Options::DecomposeNodeMatrices).
    const auto* trs = std::get_if<fastgltf::TRS>(&node.transform);
    if (!trs) return glm::mat4(1.0f);

    const glm::vec3 translation(trs->translation[0], trs->translation[1], trs->translation[2]);
    const glm::quat rotation(trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]);   // w, x, y, z
    const glm::vec3 scale(trs->scale[0], trs->scale[1], trs->scale[2]);
    return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
}

bool startsWithTeam(const std::string_view name) {
    constexpr std::string_view kPrefix = "team";
    if (name.size() < kPrefix.size()) return false;
    for (size_t i = 0; i < kPrefix.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(name[i])) != kPrefix[i]) return false;
    return true;
}

// Base-colour texture of a material through TextureManager: an external file is
// loaded by path, an embedded image from its bytes.
const sf::Texture* loadBaseColorTexture(const LoadContext& ctx, const fastgltf::Material& material) {
    if (!material.pbrData.baseColorTexture.has_value()) return nullptr;

    const fastgltf::Texture& texture = ctx.asset.textures[material.pbrData.baseColorTexture->textureIndex];
    if (!texture.imageIndex.has_value()) return nullptr;
    const size_t imageIndex = *texture.imageIndex;
    const fastgltf::Image& image = ctx.asset.images[imageIndex];

    sf::Texture* loaded = nullptr;
    if (const auto* file = std::get_if<fastgltf::sources::URI>(&image.data)) {
        loaded = TEXTURES.loadTexture(ctx.dir + file->uri.fspath().generic_string());
    } else if (const auto* view = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
        const fastgltf::BufferView& bufferView = ctx.asset.bufferViews[view->bufferViewIndex];
        const auto& source = ctx.asset.buffers[bufferView.bufferIndex].data;
        const std::byte* bytes = nullptr;
        if (const auto* array = std::get_if<fastgltf::sources::Array>(&source))
            bytes = array->bytes.data();
        else if (const auto* bufferBytes = std::get_if<fastgltf::sources::ByteView>(&source))
            bytes = bufferBytes->bytes.data();
        if (bytes) {
            loaded = TEXTURES.loadTextureFromMemory(ctx.path + "#image" + std::to_string(imageIndex),
                                                    bytes + bufferView.byteOffset,
                                                    bufferView.byteLength);
        }
    }

    if (loaded) {   // glTF samplers default to repeating, filtered, mipmapped textures
        loaded->setRepeated(true);
        loaded->setSmooth(true);
        if (!loaded->generateMipmap())
            std::cerr << "ModelLoader: cannot generate mipmaps for an image of " << ctx.path << "\n";
    } else {
        std::cerr << "ModelLoader: cannot load the base-colour image of " << ctx.path << "\n";
    }
    return loaded;
}

void addPrimitive(LoadContext& ctx, const fastgltf::Primitive& primitive, const glm::mat4& nodeWorld, bool inSkinnedNode) {
    if (primitive.type != fastgltf::PrimitiveType::Triangles) return;

    const auto position = primitive.findAttribute("POSITION");
    if (position == primitive.attributes.end()) return;

    // A skinned mesh stays in the space its joints were bound in: the node's own
    // transform is ignored (glTF spec) and the joint matrices place the vertices.
    const auto jointsAttribute  = primitive.findAttribute("JOINTS_0");
    const auto weightsAttribute = primitive.findAttribute("WEIGHTS_0");
    const bool skinned = inSkinnedNode && jointsAttribute != primitive.attributes.end()
                                       && weightsAttribute != primitive.attributes.end();
    const glm::mat4 world = skinned ? glm::mat4(1.0f) : nodeWorld;

    const fastgltf::Asset& asset = ctx.asset;
    ModelPartData part;
    part.skinned = skinned;

    const fastgltf::Accessor& positions = asset.accessors[position->accessorIndex];
    part.vertices.resize(positions.count);
    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positions, [&](glm::vec3 p, size_t i) {
        part.vertices[i].position = glm::vec3(world * glm::vec4(p, 1.0f));
        part.vertices[i].normal   = glm::vec3(0.0f);
        part.vertices[i].uv       = glm::vec2(0.0f);
    });

    // Indices (a primitive without them draws its vertices in order).
    if (primitive.indicesAccessor.has_value()) {
        const fastgltf::Accessor& indices = asset.accessors[*primitive.indicesAccessor];
        part.indices.reserve(indices.count);
        fastgltf::iterateAccessor<std::uint32_t>(asset, indices, [&](std::uint32_t index) {
            part.indices.push_back(index);
        });
    } else {
        part.indices.resize(part.vertices.size());
        for (size_t i = 0; i < part.indices.size(); ++i) part.indices[i] = static_cast<std::uint32_t>(i);
    }

    // A mirroring transform flips the triangle winding.
    if (glm::determinant(glm::mat3(world)) < 0.0f) {
        for (size_t i = 0; i + 2 < part.indices.size(); i += 3)
            std::swap(part.indices[i + 1], part.indices[i + 2]);
    }

    const auto normal = primitive.findAttribute("NORMAL");
    if (normal != primitive.attributes.end()) {
        const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(world));
        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normal->accessorIndex],
            [&](glm::vec3 n, size_t i) { part.vertices[i].normal = glm::normalize(normalMatrix * n); });
    } else {
        // No normals in the file: smooth ones from the triangles (area-weighted).
        for (size_t i = 0; i + 2 < part.indices.size(); i += 3) {
            ModelVertex& a = part.vertices[part.indices[i]];
            ModelVertex& b = part.vertices[part.indices[i + 1]];
            ModelVertex& c = part.vertices[part.indices[i + 2]];
            const glm::vec3 faceNormal = glm::cross(b.position - a.position, c.position - a.position);
            a.normal += faceNormal; b.normal += faceNormal; c.normal += faceNormal;
        }
        for (ModelVertex& v : part.vertices)
            if (glm::dot(v.normal, v.normal) > 0.0f) v.normal = glm::normalize(v.normal);
    }

    const auto uv = primitive.findAttribute("TEXCOORD_0");
    if (uv != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[uv->accessorIndex],
            [&](glm::vec2 t, size_t i) { part.vertices[i].uv = t; });
    }

    if (skinned) {
        const unsigned jointCount = static_cast<unsigned>(ctx.skeleton.joints.size());
        fastgltf::iterateAccessorWithIndex<glm::u16vec4>(asset, asset.accessors[jointsAttribute->accessorIndex],
            [&](glm::u16vec4 j, size_t i) { part.vertices[i].joints = j; });
        fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[weightsAttribute->accessorIndex],
            [&](glm::vec4 w, size_t i) { part.vertices[i].weights = w; });

        for (ModelVertex& v : part.vertices) {
            const float sum = v.weights.x + v.weights.y + v.weights.z + v.weights.w;
            if (sum > 1.0e-6f) v.weights /= sum;
            else               v.weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
            for (int k = 0; k < 4; ++k)
                if (v.joints[k] >= jointCount) { v.joints[k] = 0; v.weights[k] = 0.0f; }
        }
    }

    if (primitive.materialIndex.has_value()) {
        const fastgltf::Material& material = asset.materials[*primitive.materialIndex];
        const auto& color = material.pbrData.baseColorFactor;
        part.baseColor   = glm::vec4(color[0], color[1], color[2], color[3]);
        part.texture     = loadBaseColorTexture(ctx, material);
        part.teamTinted  = startsWithTeam(std::string_view(material.name.data(), material.name.size()));
        part.doubleSided = material.doubleSided;
    }

    ctx.parts.push_back(std::move(part));
}

// Joints (with rest pose and inverse bind matrices) of the model's first skin,
// and every animation that drives them.  Leaves the skeleton empty when the
// model has no usable skin.
void buildSkeleton(LoadContext& ctx) {
    const fastgltf::Asset& asset = ctx.asset;
    if (asset.skins.empty()) return;

    const fastgltf::Skin& skin = asset.skins[0];
    if (skin.joints.empty()) return;
    if (skin.joints.size() > static_cast<size_t>(Model::kMaxJoints)) {
        std::cerr << "ModelLoader: " << ctx.path << " has " << skin.joints.size() << " joints (at most "
                  << Model::kMaxJoints << " are supported); drawn without animation\n";
        return;
    }

    std::vector<long> nodeParent(asset.nodes.size(), -1);
    for (size_t n = 0; n < asset.nodes.size(); ++n)
        for (const size_t child : asset.nodes[n].children) nodeParent[child] = static_cast<long>(n);

    // Static (unanimated) model-space transform of a node, for the non-joint nodes above the skeleton.
    auto baseWorld = [&](size_t node) {
        glm::mat4 world(1.0f);
        for (long n = static_cast<long>(node); n >= 0; n = nodeParent[n])
            world = nodeMatrix(asset.nodes[n]) * world;
        return world;
    };

    for (size_t j = 0; j < skin.joints.size(); ++j) ctx.nodeToJoint[skin.joints[j]] = static_cast<int>(j);

    std::vector<glm::mat4> inverseBind(skin.joints.size(), glm::mat4(1.0f));
    if (skin.inverseBindMatrices.has_value()) {
        fastgltf::iterateAccessorWithIndex<glm::mat4>(asset, asset.accessors[*skin.inverseBindMatrices],
            [&](glm::mat4 m, size_t i) { if (i < inverseBind.size()) inverseBind[i] = m; });
    }

    ctx.skeleton.joints.resize(skin.joints.size());
    for (size_t j = 0; j < skin.joints.size(); ++j) {
        const size_t nodeIndex = skin.joints[j];
        const fastgltf::Node& node = asset.nodes[nodeIndex];
        ModelJoint& joint = ctx.skeleton.joints[j];

        joint.name        = std::string(node.name.data(), node.name.size());
        joint.inverseBind = inverseBind[j];

        const long parentNode = nodeParent[nodeIndex];
        const auto parentJoint = parentNode >= 0 ? ctx.nodeToJoint.find(static_cast<size_t>(parentNode)) : ctx.nodeToJoint.end();
        if (parentJoint != ctx.nodeToJoint.end()) joint.parent = parentJoint->second;
        else if (parentNode >= 0)                 joint.parentTransform = baseWorld(static_cast<size_t>(parentNode));

        if (const auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
            joint.restTranslation = glm::vec3(trs->translation[0], trs->translation[1], trs->translation[2]);
            joint.restRotation    = glm::quat(trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]);
            joint.restScale       = glm::vec3(trs->scale[0], trs->scale[1], trs->scale[2]);
        }
    }

    for (size_t a = 0; a < asset.animations.size(); ++a) {
        const fastgltf::Animation& animation = asset.animations[a];
        ModelClip clip;
        clip.name = animation.name.empty() ? "animation" + std::to_string(a)
                                           : std::string(animation.name.data(), animation.name.size());

        for (const fastgltf::AnimationChannel& source : animation.channels) {
            if (!source.nodeIndex.has_value()) continue;
            const auto joint = ctx.nodeToJoint.find(*source.nodeIndex);
            if (joint == ctx.nodeToJoint.end()) continue;   // only joints are animated

            ModelChannel channel;
            channel.joint = joint->second;
            switch (source.path) {
                case fastgltf::AnimationPath::Translation: channel.path = ModelChannel::Path::Translation; break;
                case fastgltf::AnimationPath::Rotation:    channel.path = ModelChannel::Path::Rotation;    break;
                case fastgltf::AnimationPath::Scale:       channel.path = ModelChannel::Path::Scale;       break;
                default: continue;                          // morph weights are not supported
            }

            const fastgltf::AnimationSampler& sampler = animation.samplers[source.samplerIndex];
            channel.step = sampler.interpolation == fastgltf::AnimationInterpolation::Step;
            fastgltf::iterateAccessor<float>(asset, asset.accessors[sampler.inputAccessor],
                [&](float t) { channel.times.push_back(t); });

            // Cubic-spline output is (in-tangent, value, out-tangent) per key: keep the value
            // and blend linearly, which is close enough for exported game animations.
            const bool cubic = sampler.interpolation == fastgltf::AnimationInterpolation::CubicSpline;
            size_t index = 0;
            auto addValue = [&](const glm::vec4& v) {
                if (!cubic || index % 3 == 1) channel.values.push_back(v);
                ++index;
            };
            const fastgltf::Accessor& output = asset.accessors[sampler.outputAccessor];
            if (channel.path == ModelChannel::Path::Rotation)
                fastgltf::iterateAccessor<glm::vec4>(asset, output, [&](glm::vec4 v) { addValue(v); });
            else
                fastgltf::iterateAccessor<glm::vec3>(asset, output, [&](glm::vec3 v) { addValue(glm::vec4(v, 0.0f)); });

            if (channel.times.empty() || channel.times.size() != channel.values.size()) continue;
            clip.duration = std::max(clip.duration, channel.times.back());
            clip.channels.push_back(std::move(channel));
        }
        if (!clip.channels.empty()) ctx.skeleton.clips.push_back(std::move(clip));
    }
}

void visitNode(LoadContext& ctx, size_t nodeIndex, const glm::mat4& parent) {
    const fastgltf::Node& node = ctx.asset.nodes[nodeIndex];
    const glm::mat4 world = parent * nodeMatrix(node);

    if (node.meshIndex.has_value()) {
        const bool inSkinnedNode = node.skinIndex.has_value() && *node.skinIndex == 0 && !ctx.skeleton.joints.empty();
        for (const fastgltf::Primitive& primitive : ctx.asset.meshes[*node.meshIndex].primitives)
            addPrimitive(ctx, primitive, world, inSkinnedNode);
    }
    for (const size_t child : node.children)
        visitNode(ctx, child, world);
}

} // namespace

namespace ModelLoader {

std::unique_ptr<Model> load(const std::string& path) {
    const std::string fullPath = TextureManager::getAssetPath(path);

    auto data = fastgltf::GltfDataBuffer::FromPath(fullPath);
    if (data.error() != fastgltf::Error::None) {
        std::cerr << "ModelLoader: cannot read " << fullPath << ": " << fastgltf::getErrorMessage(data.error()) << "\n";
        return nullptr;
    }

    // External buffers (.bin) are loaded here; images stay on disk for TextureManager.
    fastgltf::Parser parser;
    auto asset = parser.loadGltf(data.get(), std::filesystem::path(fullPath).parent_path(),
                                 fastgltf::Options::LoadExternalBuffers | fastgltf::Options::DecomposeNodeMatrices);
    if (asset.error() != fastgltf::Error::None) {
        std::cerr << "ModelLoader: cannot parse " << fullPath << ": " << fastgltf::getErrorMessage(asset.error()) << "\n";
        return nullptr;
    }

    const size_t slash = path.find_last_of('/');
    LoadContext ctx{ asset.get(), path, slash == std::string::npos ? std::string() : path.substr(0, slash + 1), {}, {}, {} };
    buildSkeleton(ctx);

    const fastgltf::Asset& gltf = asset.get();
    const size_t sceneIndex = gltf.defaultScene.has_value() ? *gltf.defaultScene : 0;
    if (sceneIndex >= gltf.scenes.size()) {
        std::cerr << "ModelLoader: " << fullPath << " has no scene\n";
        return nullptr;
    }
    for (const size_t root : gltf.scenes[sceneIndex].nodeIndices)
        visitNode(ctx, root, glm::mat4(1.0f));

    if (ctx.parts.empty()) {
        std::cerr << "ModelLoader: " << fullPath << " contains no triangle meshes\n";
        return nullptr;
    }
    return std::make_unique<Model>(ctx.parts, std::move(ctx.skeleton));
}

} // namespace ModelLoader
