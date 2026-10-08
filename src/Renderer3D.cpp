#include <GL/glew.h>
#include "Renderer3D.h"
#include "Scene3D.h"
#include "Camera3D.h"
#include "Game.h"
#include "Constants.h"
#include "Entity.h"
#include "EntityData.h"
#include "Building.h"
#include "ResourceNode.h"
#include "Unit.h"
#include "Turret.h"
#include "AnimatedSprite.h"
#include "FogOfWar.h"
#include "EffectsManager.h"
#include "Player.h"
#include "InputHandler.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>
#include <iostream>
#include <stdexcept>

namespace {

const char* kSpriteVertexSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aTint;
uniform mat4 uViewProj;
out vec2 vUV;
out vec4 vTint;
void main() {
    vUV   = aUV;
    vTint = aTint;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

const char* kSpriteFragmentSrc = R"(#version 330 core
in vec2 vUV;
in vec4 vTint;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
    vec4 c = texture(uTex, vUV) * vTint;
    if (c.a < 0.02) discard;
    FragColor = c;
}
)";

using Scene3D::toVec3;

// The fog mesh reuses Vertex3D: its colour slot carries the fog-texture UV.
const char* kFogVertexSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aUV;
uniform mat4 uViewProj;
out vec2 vUV;
void main() {
    vUV = aUV.xy;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

const char* kFogFragmentSrc = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 FragColor;
void main() {
    FragColor = texture(uTex, vUV);
}
)";

// Vertical extent of the box drawn for an entity: its base sits on the lowest
// ground under the footprint and it is extended upward to cover any slope.
struct EntityBox {
    float baseY;
    float height;
};

// Lowest ground under a footprint and the relief above it (baseY, height).
EntityBox footprintSpan(const Map& map, sf::Vector2f pos, sf::Vector2f size) {
    const float hx = size.x * 0.5f;
    const float hz = size.y * 0.5f;
    float lo = map.getHeightAt(pos);
    float hi = lo;
    for (const sf::Vector2f& corner : { sf::Vector2f(pos.x - hx, pos.y - hz), sf::Vector2f(pos.x + hx, pos.y - hz),
                                        sf::Vector2f(pos.x + hx, pos.y + hz), sf::Vector2f(pos.x - hx, pos.y + hz) }) {
        const float h = map.getHeightAt(corner);
        lo = std::min(lo, h);
        hi = std::max(hi, h);
    }
    return { lo, hi - lo };
}

EntityBox entityBox(const Entity& entity, const Map& map) {
    const sf::Vector2f pos  = entity.getPosition();
    const sf::Vector2f size = entity.getSize();

    float height;
    if (entity.asBuilding())          height = std::min(size.x, size.y) * 0.8f;
    else if (entity.asResourceNode()) height = std::min(size.x, size.y) * 0.5f;
    else                              height = std::min(size.x, size.y);

    const EntityBox span = footprintSpan(map, pos, size);
    return { span.baseY, span.height + height };
}

// Fog rules shared by drawing and picking: own entities and resource nodes are
// always shown, everything else only while its tile is visible.
bool isShown(const Entity& entity, const FogOfWar& fog, const Map& map, Team myTeam) {
    if (entity.asResourceNode()) return true;
    return entity.getTeam() == myTeam || fog.isVisibleAtWorld(entity.getPosition(), map);
}

// What to draw for an entity that has artwork: the current texture region, its
// size in world units and where the entity's ground point sits in the image.
struct SpriteView {
    const sf::Texture* texture = nullptr;
    sf::IntRect        rect;
    sf::Vector2f       size;
    float              anchor = 0.25f;   // fraction of the image height under the ground point
    float              alpha  = 1.0f;
};

bool spriteView(Entity& entity, SpriteView& out) {
    if (auto* turret = dynamic_cast<Turret*>(&entity)) {
        const sf::Texture* tex = turret->getCurrentTexture();
        if (!tex) return false;
        const sf::Vector2u ts = tex->getSize();
        out.texture = tex;
        out.rect    = sf::IntRect(sf::Vector2i(0, 0), sf::Vector2i(static_cast<int>(ts.x), static_cast<int>(ts.y)));
        out.size    = sf::Vector2f(static_cast<float>(ts.x) * 0.25f, static_cast<float>(ts.y) * 0.25f);
        out.anchor  = 0.25f;
    } else if (entity.hasSprite()) {
        const AnimatedSprite& sprite = entity.getAnimatedSprite();
        out.texture = sprite.getCurrentTexture();
        if (!out.texture) return false;
        out.rect   = sprite.getCurrentTextureRect();
        out.size   = sprite.getSize();
        out.anchor = entity.asUnit() ? 0.25f : 0.22f;
    } else {
        return false;
    }

    if (const Building* building = entity.asBuilding(); building && !building->isConstructed())
        out.alpha = 0.5f + 0.5f * building->getConstructionProgress();
    return true;
}

// Where a sprite's camera-facing quad stands: its bottom edge on the ground,
// shifted toward the camera so the image's ground point lands on the entity.
struct BillboardQuad {
    glm::vec3 bottom;      // middle of the bottom edge
    float     halfWidth;
    float     height;
};

BillboardQuad placeBillboard(const Map& map, sf::Vector2f pos, const SpriteView& view) {
    const float sinPitch = std::sin(glm::radians(Camera3D::kPitchDegrees));
    const float shift = view.anchor * view.size.y / sinPitch;
    const float baseY = std::max(map.getHeightAt(pos),
                                 map.getHeightAt(sf::Vector2f(pos.x, pos.y + shift)));
    return { glm::vec3(pos.x, baseY, pos.y + shift), view.size.x * 0.5f, view.size.y };
}

// Ray vs. billboard quad; t is the distance along the (unit) ray direction.
bool rayHitsBillboard(const Camera3D::Ray& ray, const BillboardQuad& quad, float& t) {
    const glm::vec3 right = Camera3D::billboardRight();
    const glm::vec3 up    = Camera3D::billboardUp();
    const glm::vec3 normal = glm::cross(right, up);
    const float denom = glm::dot(ray.dir, normal);
    if (std::abs(denom) < 1e-6f) return false;
    t = glm::dot(quad.bottom - ray.origin, normal) / denom;
    if (t < 0.0f) return false;
    const glm::vec3 local = ray.origin + ray.dir * t - quad.bottom;
    const float a = glm::dot(local, right);
    const float b = glm::dot(local, up);
    return std::abs(a) <= quad.halfWidth && b >= 0.0f && b <= quad.height;
}

} // namespace

Renderer3D::Renderer3D(sf::RenderWindow& window)
    : m_window(window)
    , m_hud(window)
{
    Scene3D::initGL(m_window);

    m_shader = std::make_unique<GLShader>(Scene3D::sceneVertexSource(), Scene3D::sceneFragmentSource());
    m_spriteShader = std::make_unique<GLShader>(kSpriteVertexSrc, kSpriteFragmentSrc);
    m_fogShader = std::make_unique<GLShader>(kFogVertexSrc, kFogFragmentSrc);
}

Renderer3D::~Renderer3D() {
    // GL objects owned by the meshes / shader must be released with the
    // window's context current.
    m_window.setActive(true);
    if (m_spriteVbo) glDeleteBuffers(1, &m_spriteVbo);
    if (m_spriteVao) glDeleteVertexArrays(1, &m_spriteVao);
    m_fogShader.reset();
    m_spriteShader.reset();
    m_shader.reset();
}

// ---------------------------------------------------------------------------
// Picking – mouse rays are cast through the same projection used for drawing.
// ---------------------------------------------------------------------------
sf::Vector2f Renderer3D::screenToWorld(const Camera& camera, sf::Vector2i pixel, const Map& map) const {
    return Camera3D::screenToWorld(camera, pixel, m_window.getSize(), map);
}

EntityPtr Renderer3D::pickEntity(const Camera& camera, sf::Vector2i pixel, Game& game) const {
    const sf::Vector2u winSize = m_window.getSize();
    if (winSize.x == 0 || winSize.y == 0) return nullptr;

    const Map&      map    = game.getMap();
    const FogOfWar& fog    = game.getPlayer().getFog();
    const Team      myTeam = game.getPlayer().getTeam();
    const Camera3D::Ray ray = Camera3D::pixelRay(camera, pixel, winSize, map);

    // Terrain hit distance: entities behind a hill must not be picked.
    const sf::Vector2f ground = Camera3D::screenToWorld(camera, pixel, winSize, map);
    const glm::vec3 groundPoint(ground.x, map.getHeightAt(ground), ground.y);
    const float terrainT = glm::dot(groundPoint - ray.origin, ray.dir);

    EntityPtr best;
    float bestKey = -std::numeric_limits<float>::max();
    for (const auto& entity : game.getWorld().all()) {
        if (!entity || !entity->isAlive()) continue;
        if (entity->getType() == EntityType::Rocket) continue;
        if (!isShown(*entity, fog, map, myTeam)) continue;

        const sf::Vector2f pos  = entity->getPosition();
        const sf::Vector2f size = entity->getSize();

        // Entities with artwork are hit where their sprite is drawn; the rest by their box.
        // Among hits the one drawn in front (larger ground z) wins.
        float t;
        float key;
        SpriteView view;
        if (spriteView(*entity, view) && view.size.x > 0.0f && view.size.y > 0.0f) {
            const BillboardQuad quad = placeBillboard(map, pos, view);
            if (!rayHitsBillboard(ray, quad, t)) continue;
            key = quad.bottom.z;
        } else {
            const EntityBox box = entityBox(*entity, map);
            const glm::vec3 lo(pos.x - size.x * 0.5f, box.baseY,              pos.y - size.y * 0.5f);
            const glm::vec3 hi(pos.x + size.x * 0.5f, box.baseY + box.height, pos.y + size.y * 0.5f);
            if (!Camera3D::rayHitsBox(ray, lo, hi, t)) continue;
            key = pos.y + size.y * 0.5f;
        }

        if (t <= terrainT + 1.0f && key > bestKey) {
            bestKey = key;
            best    = entity;
        }
    }
    return best;
}

std::vector<EntityPtr> Renderer3D::pickEntitiesInRect(const Camera& camera, sf::Vector2i cornerA,
                                                      sf::Vector2i cornerB, Team team,
                                                      Game& game) const {
    const sf::Vector2u winSize = m_window.getSize();
    const Map& map = game.getMap();

    const float left   = static_cast<float>(std::min(cornerA.x, cornerB.x));
    const float right  = static_cast<float>(std::max(cornerA.x, cornerB.x));
    const float top    = static_cast<float>(std::min(cornerA.y, cornerB.y));
    const float bottom = static_cast<float>(std::max(cornerA.y, cornerB.y));

    std::vector<EntityPtr> result;
    for (const auto& entity : game.getWorld().all()) {
        if (!entity || !entity->isAlive() || entity->getTeam() != team) continue;

        // An entity is inside the rubber-band when the middle of what is drawn projects into it.
        const sf::Vector2f pos = entity->getPosition();
        glm::vec3 centre;
        SpriteView view;
        if (spriteView(*entity, view) && view.size.x > 0.0f && view.size.y > 0.0f) {
            const BillboardQuad quad = placeBillboard(map, pos, view);
            centre = quad.bottom + Camera3D::billboardUp() * (quad.height * 0.5f);
        } else {
            const EntityBox box = entityBox(*entity, map);
            centre = glm::vec3(pos.x, box.baseY + box.height * 0.5f, pos.y);
        }
        sf::Vector2f screen;
        if (!Camera3D::projectPoint(camera, centre, winSize, map, screen))
            continue;
        if (screen.x >= left && screen.x <= right && screen.y >= top && screen.y <= bottom)
            result.push_back(entity);
    }
    return result;
}

void Renderer3D::rebuildTerrain(const Map& map) {
    MeshData terrain;
    MeshData fogGrid;
    Scene3D::buildTerrainMesh(map, terrain, &fogGrid);
    m_terrainMesh.upload(terrain);
    m_fogMesh.upload(fogGrid);
    m_terrainDirty = false;
}

// Builds everything drawn for entities: coloured boxes for entities without
// artwork, camera-facing sprite quads for those with it, plus selection rings
// and health bars for both.
void Renderer3D::buildEntities(Game& game) {
    m_entityData.clear();
    m_ringData.clear();
    m_barData.clear();
    m_spriteVerts.clear();
    m_spriteBatches.clear();

    const FogOfWar& fog   = game.getPlayer().getFog();
    const Map&      map   = game.getMap();
    const Team      myTeam = game.getPlayer().getTeam();

    const glm::vec3 right = Camera3D::billboardRight();
    const glm::vec3 up    = Camera3D::billboardUp();
    const float sinPitch  = std::sin(glm::radians(Camera3D::kPitchDegrees));

    struct PendingSprite {
        unsigned int texture;
        bool         postFog;
        float        depth;   // ground z of the quad: larger = closer to the camera
        std::array<SpriteVertex, 6> verts;
    };
    std::vector<PendingSprite> sprites;

    // Draws one entity. Pre-fog entities (remembered ghosts, resource nodes) are
    // darkened by the fog overlay; post-fog ones are drawn over it.
    auto emitEntity = [&](const auto& entity, bool postFog, bool ghost) {
        const sf::Vector2f pos  = entity->getPosition();
        const sf::Vector2f size = entity->getSize();
        const EntityBox box = entityBox(*entity, map);

        glm::vec3 topPoint;   // where the health bar hangs
        SpriteView view;
        if (spriteView(*entity, view) && view.size.x > 0.0f && view.size.y > 0.0f) {
            // Place the quad's bottom edge on the ground, shifted toward the
            // camera so the image's ground point lands on the entity position.
            const BillboardQuad quad = placeBillboard(map, pos, view);
            const glm::vec3 bottom = quad.bottom;
            const glm::vec3 dx = right * quad.halfWidth;
            const glm::vec3 dy = up * quad.height;

            const sf::Vector2u ts = view.texture->getSize();
            const float tw = static_cast<float>(ts.x);
            const float th = static_cast<float>(ts.y);
            const float u0 = static_cast<float>(view.rect.position.x) / tw;
            const float u1 = static_cast<float>(view.rect.position.x + view.rect.size.x) / tw;
            const float v0 = static_cast<float>(view.rect.position.y) / th;
            const float v1 = static_cast<float>(view.rect.position.y + view.rect.size.y) / th;

            const float bright = entity->isSelected() ? 1.2f : 1.0f;
            const glm::vec4 tint(bright, bright, bright, view.alpha);

            const SpriteVertex bl{ bottom - dx,      { u0, v1 }, tint };
            const SpriteVertex br{ bottom + dx,      { u1, v1 }, tint };
            const SpriteVertex tr{ bottom + dx + dy, { u1, v0 }, tint };
            const SpriteVertex tl{ bottom - dx + dy, { u0, v0 }, tint };

            const unsigned int handle = view.texture->getNativeHandle();
            prepareSpriteTexture(handle, view.rect.size.x == static_cast<int>(ts.x)
                                      && view.rect.size.y == static_cast<int>(ts.y));
            sprites.push_back({ handle, postFog, bottom.z, { bl, br, tr, bl, tr, tl } });
            topPoint = bottom + dy;
        } else {
            glm::vec3 color = (entity->getTeam() == Team::Neutral)
                ? toVec3(entity->getColor())
                : toVec3(teamColor(entity->getTeam()));
            if (entity->isSelected()) color = glm::min(color * 1.4f, glm::vec3(1.0f));
            if (entity->getType() == EntityType::Rocket) {
                // Projectiles fly: a small bright cube above the ground.
                const float flyY = box.baseY + 10.0f;
                m_entityData.addBox(pos.x, pos.y, size.x, size.y, size.x, color, flyY);
                topPoint = glm::vec3(pos.x, flyY + size.x, pos.y);
            } else {
                m_entityData.addBox(pos.x, pos.y, size.x, size.y, box.height, color, box.baseY);
                topPoint = glm::vec3(pos.x, box.baseY + box.height, pos.y);
            }
        }

        // Remembered buildings are just scenery: no rings or bars.
        if (ghost) return;

        // Selection ring on the ground (also shown while the entity blinks as a target).
        const bool ring = entity->isSelected()
                       || (entity->isHighlighted() && entity->isHighlightBlinkVisible());
        if (ring && entity->getType() != EntityType::Rocket) {
            const glm::vec3 ringColor = entity->isLocalTeam() ? glm::vec3(0.1f, 0.9f, 0.1f)
                                                              : glm::vec3(0.9f, 0.1f, 0.1f);
            const float radius = size.x * 0.5f + 4.0f;
            constexpr int   kSegments = 36;
            constexpr float kHalfWidth = 1.2f;
            constexpr float kLift = 0.8f;
            auto ringPoint = [&](float angle, float r) {
                const float x = pos.x + std::cos(angle) * r;
                const float z = pos.y + std::sin(angle) * r;
                return glm::vec3(x, map.getHeightAt(sf::Vector2f(x, z)) + kLift, z);
            };
            for (int i = 0; i < kSegments; ++i) {
                const float a0 = glm::two_pi<float>() * static_cast<float>(i)     / kSegments;
                const float a1 = glm::two_pi<float>() * static_cast<float>(i + 1) / kSegments;
                m_ringData.addUnlitQuad(ringPoint(a0, radius - kHalfWidth), ringPoint(a0, radius + kHalfWidth),
                                        ringPoint(a1, radius + kHalfWidth), ringPoint(a1, radius - kHalfWidth),
                                        ringColor);
            }
        }

        // Health bar: shown when selected or damaged (units and buildings only).
        if ((entity->asUnit() || entity->asBuilding())
            && (entity->isSelected() || entity->getHealth() < entity->getMaxHealth())
            && entity->getMaxHealth() > 0) {
            const float barW = std::max(size.x, 16.0f);
            const float barH = 4.0f;
            const glm::vec3 centre = topPoint + up * 8.0f;
            const glm::vec3 half   = right * (barW * 0.5f);
            const glm::vec3 lift   = up * (barH * 0.5f);
            const float pct = std::clamp(static_cast<float>(entity->getHealth())
                                         / static_cast<float>(entity->getMaxHealth()), 0.0f, 1.0f);

            m_barData.addUnlitQuad(centre - half - lift, centre + half - lift,
                                   centre + half + lift, centre - half + lift,
                                   glm::vec3(0.7f, 0.0f, 0.0f));
            const glm::vec3 fillEnd = centre - half + right * (barW * pct);
            m_barData.addUnlitQuad(centre - half - lift, fillEnd - lift,
                                   fillEnd + lift, centre - half + lift,
                                   glm::vec3(0.0f, 0.7f, 0.0f));
        }
    };

    // Live entities.
    for (const auto& entity : game.getWorld().all()) {
        if (!entity) continue;
        if (!entity->isAlive() && !entity->isDying()) continue;
        if (!isShown(*entity, fog, map, myTeam)) continue;
        emitEntity(entity, !entity->asResourceNode(), false);
    }

    // Remembered buildings and resource nodes standing in the shroud.
    for (const auto& [id, ghost] : fog.getGhosts()) {
        const auto& entity = ghost.entity;
        if (!entity) continue;
        if (entity->asResourceNode() && (entity->isAlive() || entity->isDying())) continue;  // drawn above
        const sf::Vector2i ghostTile = map.worldToTile(entity->getPosition());
        if (fog.isVisible(ghostTile.x, ghostTile.y)) continue;
        emitEntity(entity, false, true);
    }

    // Effects: explosions stand up as billboards, move pings lie on the ground.
    for (const auto& effect : EFFECTS.all()) {
        const AnimatedSprite& sprite = effect->getSprite();
        const sf::Texture* tex = sprite.getCurrentTexture();
        if (!tex) continue;
        const sf::Vector2f size = sprite.getSize();
        if (size.x <= 0.0f || size.y <= 0.0f) continue;

        const sf::IntRect rect = sprite.getCurrentTextureRect();
        const sf::Vector2u ts = tex->getSize();
        const float tw = static_cast<float>(ts.x);
        const float th = static_cast<float>(ts.y);
        const float u0 = static_cast<float>(rect.position.x) / tw;
        const float u1 = static_cast<float>(rect.position.x + rect.size.x) / tw;
        const float v0 = static_cast<float>(rect.position.y) / th;
        const float v1 = static_cast<float>(rect.position.y + rect.size.y) / th;
        const glm::vec4 tint(1.0f);
        const unsigned int handle = tex->getNativeHandle();
        prepareSpriteTexture(handle, rect.size.x == static_cast<int>(ts.x)
                                  && rect.size.y == static_cast<int>(ts.y));

        const sf::Vector2f pos = effect->getWorldPosition();
        if (effect->isGroundMarker()) {
            const float hw = size.x * 0.5f;
            const float hh = size.y * 0.5f;
            auto corner = [&](float dx, float dz) {
                const float x = pos.x + dx;
                const float z = pos.y + dz;
                return glm::vec3(x, map.getHeightAt(sf::Vector2f(x, z)) + 0.7f, z);
            };
            const SpriteVertex nw{ corner(-hw, -hh), { u0, v0 }, tint };
            const SpriteVertex ne{ corner( hw, -hh), { u1, v0 }, tint };
            const SpriteVertex se{ corner( hw,  hh), { u1, v1 }, tint };
            const SpriteVertex sw{ corner(-hw,  hh), { u0, v1 }, tint };
            sprites.push_back({ handle, true, -1.0e9f, { nw, sw, se, nw, se, ne } });   // under every billboard
        } else {
            constexpr float kAnchor = 0.4f;   // explosion centre sits slightly above the ground point
            const float shift = kAnchor * size.y / sinPitch;
            const float baseY = std::max(map.getHeightAt(pos),
                                         map.getHeightAt(sf::Vector2f(pos.x, pos.y + shift)));
            const glm::vec3 bottom(pos.x, baseY, pos.y + shift);
            const glm::vec3 dx = right * (size.x * 0.5f);
            const glm::vec3 dy = up * size.y;
            const SpriteVertex bl{ bottom - dx,      { u0, v1 }, tint };
            const SpriteVertex br{ bottom + dx,      { u1, v1 }, tint };
            const SpriteVertex tr{ bottom + dx + dy, { u1, v0 }, tint };
            const SpriteVertex tl{ bottom - dx + dy, { u0, v0 }, tint };
            sprites.push_back({ handle, true, bottom.z + 1000.0f, { bl, br, tr, bl, tr, tl } });   // in front
        }
    }

    // Pre-fog pass first, then post-fog; each back to front so overlapping
    // translucent pixels blend correctly.
    std::stable_sort(sprites.begin(), sprites.end(),
                     [](const PendingSprite& a, const PendingSprite& b) {
                         if (a.postFog != b.postFog) return !a.postFog;
                         return a.depth < b.depth;
                     });
    m_spriteVerts.reserve(sprites.size() * 6);
    for (const PendingSprite& s : sprites) {
        if (m_spriteBatches.empty() || m_spriteBatches.back().texture != s.texture
            || m_spriteBatches.back().postFog != s.postFog)
            m_spriteBatches.push_back({ s.texture, s.postFog, static_cast<int>(m_spriteVerts.size()), 0 });
        m_spriteVerts.insert(m_spriteVerts.end(), s.verts.begin(), s.verts.end());
        m_spriteBatches.back().count += 6;
    }
}

// Sprites are scaled down a lot; filter them (and mip-map whole-image textures,
// but not sprite sheets whose frames would bleed into each other).
void Renderer3D::prepareSpriteTexture(unsigned int handle, bool mipmaps) {
    if (!m_preparedTextures.insert(handle).second) return;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, handle);
    if (mipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer3D::uploadSprites() {
    if (!m_spriteVao) {
        glGenVertexArrays(1, &m_spriteVao);
        glGenBuffers(1, &m_spriteVbo);
        glBindVertexArray(m_spriteVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_spriteVbo);
        const GLsizei stride = sizeof(SpriteVertex);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SpriteVertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SpriteVertex, uv)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(SpriteVertex, tint)));
    } else {
        glBindVertexArray(m_spriteVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_spriteVbo);
    }
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_spriteVerts.size() * sizeof(SpriteVertex)),
                 m_spriteVerts.data(), GL_STREAM_DRAW);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer3D::drawSprites(const glm::mat4& viewProj, bool postFog) {
    if (m_spriteBatches.empty() || !m_spriteVao) return;

    glBindVertexArray(m_spriteVao);

    m_spriteShader->use();
    m_spriteShader->setMat4("uViewProj", viewProj);
    m_spriteShader->setInt("uTex", 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glActiveTexture(GL_TEXTURE0);
    for (const SpriteBatch& batch : m_spriteBatches) {
        if (batch.postFog != postFog) continue;
        glBindTexture(GL_TEXTURE_2D, batch.texture);
        glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glBindVertexArray(0);
}

// Shroud / darkness over the terrain, sampled from the fog-of-war texture.
void Renderer3D::drawFog(const glm::mat4& viewProj, const FogOfWar& fog) {
    const unsigned int handle = fog.getFogTexture().getNativeHandle();
    if (!handle) return;

    m_fogShader->use();
    m_fogShader->setMat4("uViewProj", viewProj);
    m_fogShader->setInt("uTex", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, handle);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);
    m_fogMesh.draw();
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Rally points of the selected producing buildings: a line along the ground
// and a flag on a pole.
void Renderer3D::buildRallyPoints(Game& game) {
    const Map& map = game.getMap();
    const glm::vec3 right = Camera3D::billboardRight();
    const glm::vec3 up    = Camera3D::billboardUp();
    const glm::vec3 lineColor(0.0f, 1.0f, 0.4f);

    for (const auto& entity : game.getPlayer().getSelection()) {
        auto* building = entity->asBuilding();
        if (!building || !building->isConstructed()) continue;
        auto* def = ENTITY_DATA.getBuildingDef(entity->getType());
        if (!def || !def->canProduce()) continue;

        const sf::Vector2f from = building->getPosition();
        const sf::Vector2f to   = building->getRallyPoint();

        // Ground-hugging ribbon.
        const sf::Vector2f delta = to - from;
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (length > 1.0f) {
            const sf::Vector2f dir(delta.x / length, delta.y / length);
            const sf::Vector2f normal(-dir.y * 1.0f, dir.x * 1.0f);   // half-width 1
            const int segments = std::max(1, static_cast<int>(length / 16.0f));
            auto point = [&](float t, float side) {
                const float x = from.x + delta.x * t + normal.x * side;
                const float z = from.y + delta.y * t + normal.y * side;
                return glm::vec3(x, map.getHeightAt(sf::Vector2f(x, z)) + 0.9f, z);
            };
            for (int i = 0; i < segments; ++i) {
                const float t0 = static_cast<float>(i)     / static_cast<float>(segments);
                const float t1 = static_cast<float>(i + 1) / static_cast<float>(segments);
                m_ringData.addUnlitQuad(point(t0, -1.0f), point(t0, 1.0f), point(t1, 1.0f), point(t1, -1.0f), lineColor);
            }
        }

        // Flag: a pole plus a camera-facing triangle at its top.
        const float groundY = map.getHeightAt(to);
        constexpr float kPoleHeight = 18.0f;
        m_entityData.addBox(to.x, to.y, 2.0f, 2.0f, kPoleHeight, glm::vec3(0.4f, 0.2f, 0.08f), groundY);
        const glm::vec3 top(to.x, groundY + kPoleHeight, to.y);
        const glm::vec3 a = top;
        const glm::vec3 b = top + right * 12.0f - up * 3.5f;
        const glm::vec3 c = top - up * 7.0f;
        m_ringData.addUnlitQuad(a, b, c, c, lineColor);
    }
}

// Translucent box showing where the building being placed would go.
void Renderer3D::buildBuildPreview(Game& game) {
    m_previewData.clear();

    InputHandler& input = game.getInput();
    if (!input.isInBuildMode()) return;

    Map&               map  = game.getMap();
    const sf::Vector2f pos  = input.getBuildPreviewPosition();
    const sf::Vector2f size = ENTITY_DATA.getSize(input.getBuildingType());

    const int tilesW = static_cast<int>(size.x / Constants::TILE_SIZE);
    const int tilesH = static_cast<int>(size.y / Constants::TILE_SIZE);
    const int tileX  = static_cast<int>((pos.x - size.x / 2.0f) / Constants::TILE_SIZE);
    const int tileY  = static_cast<int>((pos.y - size.y / 2.0f) / Constants::TILE_SIZE);
    const bool canPlace = map.canPlaceBuilding(tileX, tileY, tilesW, tilesH);

    const EntityBox span = footprintSpan(map, pos, size);
    const glm::vec3 color = canPlace ? glm::vec3(0.2f, 1.0f, 0.2f) : glm::vec3(1.0f, 0.2f, 0.2f);
    m_previewData.addBox(pos.x, pos.y, size.x, size.y,
                         span.height + std::min(size.x, size.y) * 0.8f, color, span.baseY);
}

// Rubber-band rectangle in screen space (drawn through SFML after the HUD).
void Renderer3D::drawSelectionRect(const InputHandler& input) {
    if (!input.isSelecting()) return;

    // Earlier HUD passes may leave a different view/viewport active; use an
    // explicit pixel-for-pixel view so the rectangle matches the mouse position.
    const sf::Vector2u winSize = m_window.getSize();
    m_window.setView(sf::View(sf::FloatRect(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(static_cast<float>(winSize.x), static_cast<float>(winSize.y)))));

    const sf::FloatRect box = input.getSelectionBoxScreen();
    sf::RectangleShape rect(box.size);
    rect.setPosition(box.position);
    rect.setFillColor(sf::Color(0, 120, 200, 50));
    rect.setOutlineThickness(1.0f);
    rect.setOutlineColor(sf::Color(0, 180, 255, 200));
    m_window.draw(rect);
}

void Renderer3D::render(Game& game) {
    m_window.setActive(true);

    const sf::Vector2u size = m_window.getSize();
    if (size.x == 0 || size.y == 0) return;

    if (m_terrainDirty) rebuildTerrain(game.getMap());
    buildEntities(game);
    buildRallyPoints(game);
    m_entityMesh.upload(m_entityData, true);
    m_ringMesh.upload(m_ringData, true);
    m_barMesh.upload(m_barData, true);
    uploadSprites();
    buildBuildPreview(game);
    const bool hasPreview = !m_previewData.indices.empty();
    if (hasPreview) m_previewMesh.upload(m_previewData, true);

    // ── 3D pass ─────────────────────────────────────────────────────────────
    glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
    glClearColor(20.0f / 255.0f, 20.0f / 255.0f, 30.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    m_shader->use();
    const glm::mat4 viewProj = Camera3D::viewProjection(m_camera, size, game.getMap());
    m_shader->setMat4("uViewProj", viewProj);
    m_shader->setVec3("uLightDir", Scene3D::lightDirection());
    m_shader->setFloat("uAlpha", 1.0f);
    m_terrainMesh.draw();
    m_entityMesh.draw();
    m_ringMesh.draw();

    // Sprites: translucent quads, sorted back to front. Remembered buildings and
    // resource nodes first, then the fog over them, then everything currently visible.
    drawSprites(viewProj, false);
    drawFog(viewProj, game.getPlayer().getFog());
    drawSprites(viewProj, true);
    m_shader->use();

    if (hasPreview) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        m_shader->setFloat("uAlpha", 0.45f);
        m_previewMesh.draw();
        m_shader->setFloat("uAlpha", 1.0f);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    // Health bars sit on top of everything.
    glDisable(GL_DEPTH_TEST);
    m_barMesh.draw();
    glEnable(GL_DEPTH_TEST);

    // ── Hand the context back to SFML for the 2D HUD ───────────────────────
    Scene3D::endGLPass(m_window);

    m_hud.renderHud(game);
    drawSelectionRect(game.getInput());
}
