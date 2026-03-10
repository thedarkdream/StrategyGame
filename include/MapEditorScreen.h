#pragma once

#include "Screen.h"
#include "Map.h"
#include "Types.h"
#include "EntityData.h"
#include "MapSerializer.h"
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
    static constexpr float PANEL_WIDTH = 260.0f;

    // ---- Font ---------------------------------------------------------------
    const sf::Font* m_font{ nullptr };

    // ---- Map ----------------------------------------------------------------
    Map m_map;

    // ---- Camera -------------------------------------------------------------
    sf::View     m_camera;
    bool         m_isPanning     = false;
    sf::Vector2i m_panStart;
    sf::Vector2f m_panCameraStart;

    // =========================================================================
    // Editor modes
    // =========================================================================
    enum class EditorMode { Tile, Entity };
    EditorMode m_mode = EditorMode::Tile;

    // ---- Tile painting ------------------------------------------------------
    TileType m_selectedTile = TileType::Ground;

    struct TileSwatch {
        TileType                type;
        sf::Color               baseColor;
        std::string             name;
        sf::RectangleShape      shape;
        std::optional<sf::Text> label;
    };
    std::vector<TileSwatch> m_swatches;

    // ---- Entity placement ---------------------------------------------------
    EntityType m_pendingEntityType = EntityType::None;
    Team       m_pendingTeam       = Team::Player1;

    // Preview ghost drawn at hovered tile
    sf::RectangleShape m_placementPreview;

    struct PlacedEntity {
        EntityType type;
        Team       team;
        int        tileX, tileY;
    };
    std::vector<PlacedEntity> m_placedEntities;

    // =========================================================================
    // Overlays
    // =========================================================================
    bool         m_isPainting  = false;
    sf::Vector2i m_hoveredTile = { -1, -1 };

    sf::RectangleShape m_hoverRect;
    sf::VertexArray    m_gridLines;
    bool               m_gridDirty = true;

    // =========================================================================
    // Panel UI
    // =========================================================================
    struct PanelButton {
        sf::RectangleShape      shape;
        std::optional<sf::Text> label;
        bool hovered  = false;
        bool selected = false;
    };

    sf::RectangleShape      m_panelBg;
    sf::RectangleShape      m_divider1;
    sf::RectangleShape      m_dividerProps;
    sf::RectangleShape      m_dividerPalette;
    sf::RectangleShape      m_dividerNeutral;
    sf::RectangleShape      m_dividerStartPos;
    sf::RectangleShape      m_dividerBuildings;
    sf::RectangleShape      m_dividerUnits;

    std::optional<sf::Text> m_lblTitle;
    std::optional<sf::Text> m_lblProps;
    std::optional<sf::Text> m_lblName;
    std::optional<sf::Text> m_lblSize;
    std::optional<sf::Text> m_lblPalette;
    std::optional<sf::Text> m_lblNeutral;
    std::optional<sf::Text> m_lblStartPos;
    std::optional<sf::Text> m_lblBuildings;
    std::optional<sf::Text> m_lblUnits;

    sf::RectangleShape      m_nameBox;
    std::optional<sf::Text> m_nameText;

    PanelButton m_btnNew;
    PanelButton m_btnLoad;
    PanelButton m_btnSave;
    PanelButton m_btnBack;

    std::vector<PanelButton> m_bldTeamButtons;
    std::vector<PanelButton> m_unitTeamButtons;

    struct EntityItem {
        EntityType              type;
        sf::RectangleShape      shape;
        std::optional<sf::Text> abbrev;
        std::optional<sf::Text> label;
        bool                    hovered  = false;
        bool                    selected = false;
    };
    std::vector<EntityItem> m_neutralItems;
    std::vector<EntityItem> m_startPosItems;
    std::vector<EntityItem> m_buildingItems;
    std::vector<EntityItem> m_unitItems;

    // ---- New-map dialog -----------------------------------------------------
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

    // ---- Load overlay -------------------------------------------------------
    bool                    m_showLoadPanel = false;
    std::vector<std::string> m_loadMapFiles;
    std::vector<PanelButton> m_loadButtons;
    PanelButton              m_btnLoadCancel;
    std::optional<sf::Text>  m_lblLoadTitle;
    sf::RectangleShape       m_loadOverlayBg;

    // ---- Status feedback ----------------------------------------------------
    std::optional<sf::Text>  m_statusText;
    float                    m_statusTimer = 0.f;

    // ---- State --------------------------------------------------------------
    std::string  m_mapName       = "untitled";
    bool         m_nameActive    = false;
    int          m_mapW          = Constants::MAP_WIDTH;
    int          m_mapH          = Constants::MAP_HEIGHT;
    int          m_mapPlayerCount = 2;
    sf::Vector2u m_lastWinSize = { 0u, 0u };
    Team         m_bldTeam    = Team::Player1;
    Team         m_unitTeam   = Team::Player1;

    float        m_panelScrollY  = 0.f;
    float        m_panelContentH = 0.f;

    // ---- Helpers ------------------------------------------------------------
    void buildLayout(sf::Vector2u winSize);
    void buildTileSwatches(float& y);
    void buildNeutralItems(float& y);
    void buildStartPosItems(float& y);
    void buildBuildingItems(float& y);
    void buildUnitItems(float& y);
    void buildNewMapDialog(sf::Vector2u winSize);
    void renderNewMapDialog(sf::RenderWindow& window);
    void confirmNewMap();

    void resetCamera(sf::Vector2u winSize);
    void updateCameraViewport(sf::Vector2u winSize);
    void buildGridLines();

    PanelButton makePanelButton(const std::string& text,
                                sf::Vector2f pos, sf::Vector2f size,
                                unsigned int fontSize = 13u);
    EntityItem  makeEntityItem(EntityType type, sf::Vector2f pos, sf::Vector2f size);

    void updateButtonHover(PanelButton& btn, sf::Vector2f panelMouse);
    void updateEntityItemHover(EntityItem& item, sf::Vector2f panelMouse);
    bool btnHit(const PanelButton& btn, sf::Vector2f panelMouse) const;
    bool itemHit(const EntityItem& item, sf::Vector2f panelMouse) const;

    void makeDivider(sf::RectangleShape& div, float y);
    void makeSectionLabel(std::optional<sf::Text>& lbl,
                          const std::string& text, float y);

    void selectPendingEntity(EntityType type, Team team);
    void clearPendingEntity();

    sf::Vector2i screenToTile(sf::Vector2i pixel) const;
    bool         inMapArea(sf::Vector2i pixel) const;
    void         tryPlaceEntity(sf::Vector2i pixel);
    void         renderPlacedEntities(sf::RenderWindow& window);
    void         renderPanel(sf::RenderWindow& window);
    void         renderLoadOverlay(sf::RenderWindow& window);

    // Save / load
    bool         saveCurrentMap();           // writes to maps/<name>.stmap
    bool         loadMapByName(const std::string& stem); // reads maps/<stem>.stmap
    void         applyMapData(const MapData& data);      // rebuilds in-editor state
    void         refreshLoadPanel(sf::Vector2u winSize); // populate m_loadButtons
};
