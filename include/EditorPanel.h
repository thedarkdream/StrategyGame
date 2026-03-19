#pragma once

#include "Types.h"
#include "EntityData.h"
#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// ===========================================================================
// EditorPanel — the left-side sidebar of the Map Editor.
//
// Owns all sidebar widget state, layout, and rendering.  MapEditorScreen
// calls rebuild() whenever editor state changes (window resize, selection
// change, etc.) and handleClick() / handleHover() for mouse interaction.
// Both return a PanelEvent that describes what (if anything) the user did.
// ===========================================================================

class EditorPanel {
public:
    // -----------------------------------------------------------------------
    // Input: a snapshot of the editor state that the panel needs for layout
    // -----------------------------------------------------------------------
    struct State {
        int          mapW          = 0;
        int          mapH          = 0;
        int          playerCount   = 2;
        std::string  mapName;
        bool         nameActive    = false;
        bool         eraseMode     = false;
        EntityType   pendingType   = EntityType::None;
        Team         pendingTeam   = Team::Player1;
        Team         bldTeam       = Team::Player1;
        Team         unitTeam      = Team::Player1;
        TileType     selectedTile  = TileType::Grass;
        float        scrollY       = 0.f;
        const sf::Font* font       = nullptr;
    };

    // -----------------------------------------------------------------------
    // Output: what happened in the panel on a given mouse event
    // -----------------------------------------------------------------------
    struct EvNone   {};
    struct EvBack   {};                         // "Back to Menu" clicked
    struct EvNew    {};                         // "New" clicked
    struct EvLoad   {};                         // "Load" clicked
    struct EvSave   {};                         // "Save" clicked
    struct EvEraseToggle {};                    // "Eraser" button clicked
    struct EvSelectTile   { TileType type; };   // tile swatch selected
    struct EvSelectEntity { EntityType type; Team team; }; // entity item selected
    struct EvCancelEntity {};                   // entity deselected (same item)
    struct EvBldTeamChanged  { Team team; };
    struct EvUnitTeamChanged { Team team; };
    struct EvNameFocused     {};
    struct EvNameUnfocused   {};
    struct EvScrollChanged   { float newScrollY; };

    using PanelEvent = std::variant<
        EvNone,
        EvBack, EvNew, EvLoad, EvSave,
        EvEraseToggle,
        EvSelectTile, EvSelectEntity, EvCancelEntity,
        EvBldTeamChanged, EvUnitTeamChanged,
        EvNameFocused, EvNameUnfocused,
        EvScrollChanged
    >;

    // -----------------------------------------------------------------------
    // Structs for palette items (visible to MapEditorScreen for  tile-swatch
    // hover-feedback; otherwise purely internal)
    // -----------------------------------------------------------------------
    struct PanelButton {
        sf::RectangleShape      shape;
        std::optional<sf::Text> label;
        bool hovered  = false;
        bool selected = false;
    };

    struct EntityItem {
        EntityType              type;
        sf::RectangleShape      shape;
        std::optional<sf::Text> abbrev;
        std::optional<sf::Text> label;
        bool                    hovered  = false;
        bool                    selected = false;
    };

    struct TileSwatch {
        TileType                type;
        sf::Color               baseColor;
        std::string             name;
        sf::RectangleShape      shape;
        std::optional<sf::Text> label;
    };

    // -----------------------------------------------------------------------
    // Public interface
    // -----------------------------------------------------------------------
    static constexpr float WIDTH = 260.0f;

    // Rebuild all widget positions/colours from the given state.
    // Call whenever the window resizes or editor state changes.
    void rebuild(const State& state, sf::Vector2u winSize);

    // Returns the total scrollable height of panel contents (needed for
    // MapEditorScreen to clamp m_panelScrollY).
    float contentHeight() const { return m_contentH; }

    // Hit-test: is pixel (screen coords) inside the panel?
    bool isOnPanel(sf::Vector2i pixel) const;

    // Process a mouse-move (updates hover states).
    void handleHover(sf::Vector2f panelMouse);

    // Process a left-click.  Returns the corresponding event.
    PanelEvent handleClick(sf::Vector2f panelMouse, const State& state);

    // Process mouse-wheel scroll over the panel (updates internal scroll and
    // returns EvScrollChanged with the clamped new scroll value).
    PanelEvent handleScroll(float delta, float windowHeight);

    // Render the panel (sets its own UI view).
    void render(sf::RenderWindow& window) const;

private:
    // -----------------------------------------------------------------------
    // Colours (defined in the .cpp translation unit)
    // -----------------------------------------------------------------------
    // (all colour helpers are free functions / anonymous-namespace in .cpp)

    // -----------------------------------------------------------------------
    // Widget members
    // -----------------------------------------------------------------------
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
    PanelButton m_btnErase;

    std::vector<PanelButton> m_bldTeamButtons;
    std::vector<PanelButton> m_unitTeamButtons;

    std::vector<TileSwatch>  m_swatches;
    std::vector<EntityItem>  m_neutralItems;
    std::vector<EntityItem>  m_startPosItems;
    std::vector<EntityItem>  m_buildingItems;
    std::vector<EntityItem>  m_unitItems;

    // -----------------------------------------------------------------------
    // Scroll state (maintained across rebuilds)
    // -----------------------------------------------------------------------
    float  m_scrollY  = 0.f;
    float  m_contentH = 0.f;

    // -----------------------------------------------------------------------
    // Cached font pointer (set during rebuild)
    // -----------------------------------------------------------------------
    const sf::Font* m_font = nullptr;

    // -----------------------------------------------------------------------
    // Builder helpers
    // -----------------------------------------------------------------------
    void makeDivider(sf::RectangleShape& div, float y);
    void makeSectionLabel(std::optional<sf::Text>& lbl,
                          const std::string& text, float y);
    PanelButton makePanelButton(const std::string& text,
                                sf::Vector2f pos, sf::Vector2f size,
                                unsigned int fontSize = 13u);
    EntityItem  makeEntityItem(EntityType type,
                               sf::Vector2f pos, sf::Vector2f size);

    void buildTileSwatches(float& y, TileType selectedTile);
    void buildNeutralItems(float& y, EntityType pendingType, Team pendingTeam);
    void buildStartPosItems(float& y, int playerCount,
                            EntityType pendingType, Team pendingTeam);
    void buildBuildingItems(float& y, int playerCount,
                            Team bldTeam, EntityType pendingType, Team pendingTeam);
    void buildUnitItems(float& y, int playerCount,
                        Team unitTeam, EntityType pendingType, Team pendingTeam);

    // -----------------------------------------------------------------------
    // Hit-test helpers
    // -----------------------------------------------------------------------
    static bool btnHit(const PanelButton& btn, sf::Vector2f pm);
    static bool itemHit(const EntityItem& item, sf::Vector2f pm);

    void updateButtonHover(PanelButton& btn, sf::Vector2f pm);
    void updateEntityItemHover(EntityItem& item, sf::Vector2f pm);
};
