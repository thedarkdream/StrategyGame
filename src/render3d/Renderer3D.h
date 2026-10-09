#pragma once

// ---------------------------------------------------------------------------
// Renderer3D — OpenGL implementation of IRenderer.
//
// Orchestrates one frame of the 3D view seen through a fixed-angle perspective
// camera derived from the shared Camera (see Camera3D.h).  The 2D HUD is drawn
// on top through SFML (Renderer2D::renderHud).
//
// Renderer3D knows no entity types: how an entity looks comes from
// describeVisual (EntityVisual.h), whether it is shown from FogOfWar, and the
// individual passes live in their own classes:
//   TerrainLayer3D  heightmap and fog overlay
//   SpriteBatch3D   textured quads (entities, effects)
//   ModelBatch3D    glTF models (entities); also owns the loaded models
//   Overlay3D       rings, health bars, rally points, build preview
//   Picking3D       mouse picking
//
// The SFML window supplies the GL context; GLEW loads the entry points.
// ---------------------------------------------------------------------------

#include "render/IRenderer.h"
#include "render2d/Renderer2D.h"
#include "gl/GLShader.h"
#include "gl/GLMesh.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>

class Map;
class InputHandler;
class TerrainLayer3D;
class SpriteBatch3D;
class ModelBatch3D;

class Renderer3D : public IRenderer {
public:
    // Throws std::runtime_error when the GL context / loader / shaders are
    // unusable; callers should fall back to Renderer2D.
    explicit Renderer3D(sf::RenderWindow& window);
    ~Renderer3D() override;

    void render(Game& game, FrameContext& frame) override;
    void setCamera(const Camera& camera) override {
        m_camera = camera;
        m_hud.setCamera(camera);
    }
    void invalidateMinimapTerrain() override;

    sf::Vector2f screenToWorld(const Camera& camera, sf::Vector2i pixel, const Map& map) const override;
    EntityPtr pickEntity(const Camera& camera, sf::Vector2i pixel, Game& game) const override;
    std::vector<EntityPtr> pickEntitiesInRect(const Camera& camera, sf::Vector2i cornerA,
                                              sf::Vector2i cornerB, Team team,
                                              Game& game) const override;

private:
    void buildFrame(Game& game);
    void drawSelectionRect(const InputHandler& input);

    sf::RenderWindow&         m_window;
    Camera                    m_camera;
    Renderer2D                m_hud;
    std::unique_ptr<GLShader> m_shader;
    std::unique_ptr<TerrainLayer3D> m_terrain;
    std::unique_ptr<SpriteBatch3D>  m_sprites;
    std::unique_ptr<ModelBatch3D>   m_models;

    GLMesh   m_entityMesh;    // lit boxes: entities without artwork, flag poles
    MeshData m_entityData;
    GLMesh   m_ringMesh;      // unlit flat geometry on the ground: rings, rally lines
    MeshData m_ringData;
    GLMesh   m_barMesh;       // health bars (drawn over everything)
    MeshData m_barData;
    GLMesh   m_previewMesh;
    MeshData m_previewData;
};
