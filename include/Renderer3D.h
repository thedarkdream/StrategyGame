#pragma once

// ---------------------------------------------------------------------------
// Renderer3D — OpenGL implementation of IRenderer.
//
// Draws the heightmap terrain, sprite entities as camera-facing billboards (or
// boxes when an entity has no artwork), the fog-of-war overlay, effects, rally
// points and selection/health markers, seen through a fixed-angle perspective
// camera derived from the shared Camera (see Camera3D.h).  The 2D HUD is drawn
// on top through SFML (Renderer2D::renderHud).
//
// The SFML window supplies the GL context; GLEW loads the entry points.
// ---------------------------------------------------------------------------

#include "IRenderer.h"
#include "Renderer2D.h"
#include "GLShader.h"
#include "GLMesh.h"
#include <glm/glm.hpp>
#include <SFML/Graphics.hpp>
#include <memory>
#include <unordered_set>
#include <vector>

class Map;
class InputHandler;
class FogOfWar;
class Renderer3D : public IRenderer {
public:
    // Throws std::runtime_error when the GL context / loader / shaders are
    // unusable; callers should fall back to Renderer2D.
    explicit Renderer3D(sf::RenderWindow& window);
    ~Renderer3D() override;

    void render(Game& game) override;
    void setCamera(const Camera& camera) override {
        m_camera = camera;
        m_hud.setCamera(camera);
    }
    void invalidateMinimapTerrain() override {
        m_hud.invalidateMinimapTerrain();
        m_terrainDirty = true;
    }

    sf::Vector2f screenToWorld(const Camera& camera, sf::Vector2i pixel, const Map& map) const override;
    EntityPtr pickEntity(const Camera& camera, sf::Vector2i pixel, Game& game) const override;
    std::vector<EntityPtr> pickEntitiesInRect(const Camera& camera, sf::Vector2i cornerA,
                                              sf::Vector2i cornerB, Team team,
                                              Game& game) const override;

private:
    // Camera-facing textured quad vertex (sprites are drawn as billboards).
    struct SpriteVertex {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec4 tint;
    };
    struct SpriteBatch {
        unsigned int texture;
        bool         postFog;   // drawn after the fog overlay (visible entities, effects)
        int          first;
        int          count;
    };

    void rebuildTerrain(const Map& map);
    void buildEntities(Game& game);
    void buildBuildPreview(Game& game);
    void buildRallyPoints(Game& game);
    void uploadSprites();
    void drawSprites(const glm::mat4& viewProj, bool postFog);
    void drawFog(const glm::mat4& viewProj, const FogOfWar& fog);
    void prepareSpriteTexture(unsigned int handle, bool mipmaps);
    void drawSelectionRect(const InputHandler& input);

    sf::RenderWindow&        m_window;
    Camera                   m_camera;
    Renderer2D               m_hud;
    std::unique_ptr<GLShader> m_shader;
    std::unique_ptr<GLShader> m_spriteShader;
    std::unique_ptr<GLShader> m_fogShader;
    GLMesh                   m_terrainMesh;
    GLMesh                   m_fogMesh;       // terrain-hugging grid textured with the fog map
    GLMesh                   m_entityMesh;
    MeshData                 m_entityData;
    GLMesh                   m_ringMesh;      // selection rings on the ground
    MeshData                 m_ringData;
    GLMesh                   m_barMesh;       // health bars (drawn over everything)
    MeshData                 m_barData;
    GLMesh                   m_previewMesh;
    MeshData                 m_previewData;
    std::vector<SpriteVertex> m_spriteVerts;
    std::vector<SpriteBatch>  m_spriteBatches;
    unsigned int             m_spriteVao = 0;
    unsigned int             m_spriteVbo = 0;
    std::unordered_set<unsigned int> m_preparedTextures;
    bool                     m_terrainDirty = true;
};
