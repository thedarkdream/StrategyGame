#pragma once

#include "Screen.h"
#include "Map.h"
#include "Types.h"
#include "EntityData.h"
#include "MapSerializer.h"
#include "EditorPanel.h"
#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <vector>

class MapEditorScreen : public Screen {
public:
    MapEditorScreen();

    ScreenResult handleEvent(const sf::Event& event) override;
    ScreenResult update(float deltaTime) override;
    void render(sf::RenderWindow& window) override;
    void onEnter() override;

private:
    static constexpr float PANEL_WIDTH = EditorPanel::WIDTH;

    // ---- Font ---------------------------------------------------------------
    const sf::Font* m_font{ nullptr };

    // ---- Map ----------------------------------------------------------------
    Map m_map;

    // ---- Camera -------------------------------------------------------------
    sf::View     m_camera;
    bool         m_isPanning     = false;
    sf::Vector2i m_panStart;
    sf::Vector2f m_panCameraStart;

    // ---- Editor modes -------------------------------------------------------
    enum class EditorMode { Tile, Entity };
    EditorMode m_mode = EditorMode::Tile;

    // ---- Tile painting ------------------------------------------------------
    TileType m_selectedTile = TileType::Grass;

    // ---- Entity placement ---------------------------------------------------
    EntityType m_pendingEntityType = EntityType::None;
    Team       m_pendingTeam       = Team::Player1;

    sf::RectangleShape m_placementPreview;

    struct PlacedEntity {
        EntityType type;
        Team       team;
        int        tileX, tileY;
    };
    std::vector<PlacedEntity> m_placedEntities;

    // ---- Painting / hover ---------------------------------------------------
    bool         m_isPainting  = false;
    sf::Vector2i m_hoveredTile = { -1, -1 };

    sf::RectangleShape m_hoverRect;
    sf::VertexArray    m_gridLines;
    bool               m_gridDirty = true;

    // ---- Sidebar panel ------------------------------------------------------
    EditorPanel m_panel;

    // ---- Dialog local button (New-map dialog and Load overlay) --------------
    struct PanelButton {
        sf::RectangleShape      shape;
        std::optional<sf::Text> label;
        bool hovered  = false;
        bool selected = false;
    };

    bool m_showNewMapDialog = false;
    int  m_newMapW = 64, m_newMapH = 64, m_newMapPlayers = 2;
    sf::RectangleShape       m_newMapOverlayBg;
    std::optional<sf::Text>  m_lblNewMapTitle;
    std::optional<sf::Text>  m_lblNewMapSizeHdr;
    std::optional<sf::Text>  m_lblNewMapPlayersHdr;
    std::vector<PanelButton> m_newMapSizeButtons;
    std::vector<PanelButton> m_newMapPlayerBtns;
    PanelButton              m_btnNewMapConfirm;
    PanelButton              m_btnNewMapCancel;

    bool                     m_showLoadPanel = false;
    std::vector<std::string> m_loadMapFiles;
    std::vector<PanelButton> m_loadButtons;
    PanelButton              m_btnLoadCancel;
    std::optional<sf::Text>  m_lblLoadTitle;
    sf::RectangleShape       m_loadOverlayBg;

    // ---- Status feedback ----------------------------------------------------
    std::optional<sf::Text>  m_statusText;
    float                    m_statusTimer = 0.f;

    // ---- State --------------------------------------------------------------
    bool         m_eraseMode      = false;
    std::string  m_mapName        = "untitled";
    bool         m_nameActive     = false;
    int          m_mapW           = Constants::MAP_WIDTH;
    int          m_mapH           = Constants::MAP_HEIGHT;
    int          m_mapPlayerCount = 2;
    sf::Vector2u m_lastWinSize    = { 0u, 0u };
    Team         m_bldTeam        = Team::Player1;
    Team         m_unitTeam       = Team::Player1;

    float        m_panelScrollY   = 0.f;

    // ---- Helpers ------------------------------------------------------------
    EditorPanel::State makePanelState() const;
    void rebuildPanel();

    void buildLayout(sf::Vector2u winSize);
    void buildNewMapDialog(sf::Vector2u winSize);
    void renderNewMapDialog(sf::RenderWindow& window);
    void confirmNewMap();

    void resetCamera(sf::Vector2u winSize);
    void updateCameraViewport(sf::Vector2u winSize);
    void buildGridLines();

    // Dialog buttons (local PanelButton)
    static bool btnHit(const PanelButton& btn, sf::Vector2f pm);
    void        updateButtonHover(PanelButton& btn, sf::Vector2f pm);
    PanelButton makePanelButton(const std::string& text,
                                sf::Vector2f pos, sf::Vector2f size,
                                unsigned int fontSize = 13u);

    void selectPendingEntity(EntityType type, Team team);
    void clearPendingEntity();
    void tryEraseAt(sf::Vector2i pixel);

    sf::Vector2i screenToTile(sf::Vector2i pixel) const;
    bool         inMapArea(sf::Vector2i pixel) const;
    void         tryPlaceEntity(sf::Vector2i pixel);
    void         renderPlacedEntities(sf::RenderWindow& window);
    void         renderPanel(sf::RenderWindow& window);
    void         renderLoadOverlay(sf::RenderWindow& window);

    // Save / load
    bool         saveCurrentMap();
    bool         loadMapByName(const std::string& stem);
    void         applyMapData(const MapData& data);
    void         refreshLoadPanel(sf::Vector2u winSize);
};
