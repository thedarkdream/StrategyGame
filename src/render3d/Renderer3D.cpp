#include <GL/glew.h>
#include "render3d/Renderer3D.h"
#include "render3d/Scene3D.h"
#include "render3d/Camera3D.h"
#include "render3d/TerrainLayer3D.h"
#include "render3d/SpriteBatch3D.h"
#include "render3d/ModelBatch3D.h"
#include "render3d/Overlay3D.h"
#include "render3d/Picking3D.h"
#include "render3d/Placement3D.h"
#include "render/EntityVisual.h"
#include "game/Game.h"
#include "entities/Entity.h"
#include "game/FogOfWar.h"
#include "fx/EffectsManager.h"
#include "game/Player.h"
#include "ui/InputHandler.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>

using Scene3D::toVec3;

Renderer3D::Renderer3D(sf::RenderWindow& window)
    : m_window(window)
    , m_hud(window)
{
    Scene3D::initGL(m_window);

    m_shader  = std::make_unique<GLShader>(Scene3D::sceneVertexSource(), Scene3D::sceneFragmentSource());
    m_terrain = std::make_unique<TerrainLayer3D>();
    m_sprites = std::make_unique<SpriteBatch3D>();
    m_models  = std::make_unique<ModelBatch3D>();
}

Renderer3D::~Renderer3D() {
    // GL objects owned by the meshes / shaders must be released with the
    // window's context current.
    m_window.setActive(true);
    m_models.reset();
    m_sprites.reset();
    m_terrain.reset();
    m_shader.reset();
}

void Renderer3D::invalidateMinimapTerrain() {
    m_hud.invalidateMinimapTerrain();
    m_terrain->invalidate();
}

// ---------------------------------------------------------------------------
// Picking – mouse rays are cast through the same projection used for drawing.
// ---------------------------------------------------------------------------
sf::Vector2f Renderer3D::screenToWorld(const Camera& camera, sf::Vector2i pixel, const Map& map) const {
    return Camera3D::screenToWorld(camera, pixel, m_window.getSize(), map);
}

EntityPtr Renderer3D::pickEntity(const Camera& camera, sf::Vector2i pixel, Game& game) const {
    return Picking3D::pickEntity(camera, pixel, m_window.getSize(), *m_models, game);
}

std::vector<EntityPtr> Renderer3D::pickEntitiesInRect(const Camera& camera, sf::Vector2i cornerA,
                                                      sf::Vector2i cornerB, Team team, Game& game) const {
    return Picking3D::pickEntitiesInRect(camera, cornerA, cornerB, m_window.getSize(), *m_models, team, game);
}

// Collects everything drawn on top of the terrain: entities (as sprites or
// stand-in boxes, with rings and health bars), effects, rally points.
void Renderer3D::buildFrame(Game& game) {
    m_entityData.clear();
    m_ringData.clear();
    m_barData.clear();
    m_sprites->begin();
    m_models->begin();

    const FogOfWar& fog    = game.getPlayer().getFog();
    const Map&      map    = game.getMap();
    const Team      myTeam = game.getPlayer().getTeam();

    const glm::vec3 right = Camera3D::billboardRight();
    const glm::vec3 up    = Camera3D::billboardUp();

    // Draws one entity. Pre-fog entities (remembered ghosts, resource nodes) are
    // darkened by the fog overlay; post-fog ones are drawn over it.
    auto emitEntity = [&](const Entity& entity, bool postFog, bool ghost) {
        const EntityVisual visual = describeVisual(entity);
        const sf::Vector2f pos  = entity.getPosition();
        const sf::Vector2f size = entity.getSize();

        glm::vec3 topPoint;   // where the health bar hangs
        const Model* model = visual.hasModel() ? m_models->get(visual.model) : nullptr;
        if (model) {
            const Placement3D::ModelPlacement placed = Placement3D::placeModel(*model, visual, map, pos);
            m_models->add(*model, placed.transform, toVec3(visual.color),
                          entity.isSelected() ? 1.2f : 1.0f, visual.opacity, postFog,
                          visual.modelClip, visual.modelClipTime);
            topPoint = glm::vec3(pos.x, placed.hi.y, pos.y);
        } else if (visual.hasSprite()) {
            // Bottom edge on the ground, shifted toward the camera so the
            // image's ground point lands on the entity position.
            Placement3D::Billboard quad = Placement3D::placeBillboard(map, pos, visual.sprite.size, visual.spriteAnchor);
            quad.bottom.y += visual.flyHeight;
            const glm::vec3 dx = right * quad.halfWidth;
            const glm::vec3 dy = up * quad.height;

            const float bright = entity.isSelected() ? 1.2f : 1.0f;
            m_sprites->addQuad(visual.sprite,
                               { quad.bottom - dx + dy, quad.bottom + dx + dy, quad.bottom + dx, quad.bottom - dx },
                               glm::vec4(bright, bright, bright, visual.opacity), postFog, quad.bottom.z);
            topPoint = quad.bottom + dy;
        } else {
            const Placement3D::Span box = Placement3D::bodySpan(visual, map, pos, size);
            glm::vec3 color = toVec3(visual.color);
            if (entity.isSelected()) color = glm::min(color * 1.4f, glm::vec3(1.0f));
            if (visual.flyHeight > 0.0f) {
                const float flyY = box.baseY + visual.flyHeight;
                m_entityData.addBox(pos.x, pos.y, size.x, size.y, visual.bodyHeight, color, flyY);
                topPoint = glm::vec3(pos.x, flyY + visual.bodyHeight, pos.y);
            } else {
                m_entityData.addBox(pos.x, pos.y, size.x, size.y, box.height, color, box.baseY);
                topPoint = glm::vec3(pos.x, box.baseY + box.height, pos.y);
            }
        }

        // Remembered buildings are just scenery: no rings or bars.
        if (ghost) return;

        // Selection ring on the ground (also shown while the entity blinks as a target).
        const bool ring = entity.isSelected()
                       || (entity.isHighlighted() && entity.isHighlightBlinkVisible());
        if (ring && visual.selectable) {
            const glm::vec3 ringColor = entity.isLocalTeam() ? glm::vec3(0.1f, 0.9f, 0.1f)
                                                             : glm::vec3(0.9f, 0.1f, 0.1f);
            Overlay3D::addSelectionRing(m_ringData, map, pos, size.x * 0.5f + 4.0f, ringColor);
        }

        // Health bar: shown when selected or damaged.
        if (visual.showsHealthBar && entity.getMaxHealth() > 0
            && (entity.isSelected() || entity.getHealth() < entity.getMaxHealth())) {
            const float fraction = static_cast<float>(entity.getHealth()) / static_cast<float>(entity.getMaxHealth());
            Overlay3D::addHealthBar(m_barData, topPoint, std::max(size.x, 16.0f), fraction);
        }
    };

    // Live entities.
    for (const auto& entity : game.getWorld().all()) {
        if (!entity) continue;
        if (!entity->isAlive() && !entity->isDying()) continue;
        if (!fog.isEntityShown(*entity, map, myTeam)) continue;
        emitEntity(*entity, !entity->isResource(), false);
    }

    // Remembered buildings and resource nodes standing in the shroud.
    for (const auto& ghost : fog.getRememberedEntities(map))
        emitEntity(*ghost, false, true);

    // Effects: explosions stand up as billboards, move pings lie on the ground.
    const float sinPitch = std::sin(glm::radians(Camera3D::kPitchDegrees));
    for (const auto& effect : EFFECTS.all()) {
        const SpriteFrame frame = effect->getSprite().getFrame();
        if (!frame.valid()) continue;

        const sf::Vector2f size = frame.size;
        const sf::Vector2f pos  = effect->getWorldPosition();
        const glm::vec4 tint(1.0f);
        if (effect->isGroundMarker()) {
            const float hw = size.x * 0.5f;
            const float hh = size.y * 0.5f;
            auto corner = [&](float dx, float dz) {
                const float x = pos.x + dx;
                const float z = pos.y + dz;
                return glm::vec3(x, map.getHeightAt(sf::Vector2f(x, z)) + 0.7f, z);
            };
            // Under every billboard.
            m_sprites->addQuad(frame, { corner(-hw, -hh), corner(hw, -hh), corner(hw, hh), corner(-hw, hh) },
                               tint, true, -1.0e9f);
        } else {
            constexpr float kAnchor = 0.4f;   // explosion centre sits slightly above the ground point
            const float shift = kAnchor * size.y / sinPitch;
            const float baseY = std::max(map.getHeightAt(pos),
                                         map.getHeightAt(sf::Vector2f(pos.x, pos.y + shift)));
            const glm::vec3 bottom(pos.x, baseY, pos.y + shift);
            const glm::vec3 dx = right * (size.x * 0.5f);
            const glm::vec3 dy = up * size.y;
            // In front of everything.
            m_sprites->addQuad(frame, { bottom - dx + dy, bottom + dx + dy, bottom + dx, bottom - dx },
                               tint, true, bottom.z + 1000.0f);
        }
    }

    Overlay3D::addRallyPoints(game, m_ringData, m_entityData);
    m_sprites->end();
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

void Renderer3D::render(Game& game, FrameContext& frame) {
    m_window.setActive(true);

    const sf::Vector2u size = m_window.getSize();
    if (size.x == 0 || size.y == 0) return;

    m_terrain->update(game.getMap());
    buildFrame(game);
    m_entityMesh.upload(m_entityData, true);
    m_ringMesh.upload(m_ringData, true);
    m_barMesh.upload(m_barData, true);

    m_previewData.clear();
    Overlay3D::addBuildPreview(game, frame.input, m_previewData);
    const bool hasPreview = !m_previewData.indices.empty();
    if (hasPreview) m_previewMesh.upload(m_previewData, true);

    // ── 3D pass ─────────────────────────────────────────────────────────────
    glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
    glClearColor(20.0f / 255.0f, 20.0f / 255.0f, 30.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    const glm::mat4 viewProj = Camera3D::viewProjection(m_camera, size, game.getMap());
    m_terrain->drawTerrain(viewProj, Scene3D::lightDirection());

    m_shader->use();
    m_shader->setMat4("uViewProj", viewProj);
    m_shader->setVec3("uLightDir", Scene3D::lightDirection());
    m_shader->setFloat("uAlpha", 1.0f);
    m_entityMesh.draw();
    m_ringMesh.draw();

    // Models and sprites: opaque models first, then the translucent sprite quads
    // sorted back to front. Remembered buildings and resource nodes first, then the
    // fog over them, then everything currently visible.
    m_models->draw(viewProj, Scene3D::lightDirection(), false);
    m_sprites->draw(viewProj, false);
    m_terrain->drawFog(viewProj, game.getPlayer().getFog());
    m_models->draw(viewProj, Scene3D::lightDirection(), true);
    m_sprites->draw(viewProj, true);
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

    m_hud.renderHud(game, frame);
    drawSelectionRect(frame.input);
}
