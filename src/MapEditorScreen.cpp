#include "MapEditorScreen.h"
#include "Constants.h"
#include "ResourceManager.h"
#include "MapSerializer.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <filesystem>

// ---------------------------------------------------------------------------
// Palette / colour tables
// ---------------------------------------------------------------------------
namespace {

struct TileInfo { TileType type; sf::Color color; const char* name; };
constexpr TileInfo TILE_INFOS[] = {
    { TileType::Ground,  sf::Color(34, 139, 34), "Ground"  },
    { TileType::Blocked, sf::Color(80,  80,  80), "Blocked" },
};
constexpr int NUM_TILE_INFOS = 2;

// Neutral entity definitions (no team)
struct NeutralInfo { EntityType type; const char* name; sf::Color color; };
constexpr NeutralInfo NEUTRAL_INFOS[] = {
    { EntityType::MineralPatch, "Minerals", sf::Color( 80, 160, 255) },
    { EntityType::GasGeyser,    "Geyser",   sf::Color( 60, 200, 120) },
};
constexpr int NUM_NEUTRAL = 2;

// Building entity definitions
struct EntInfo { EntityType type; const char* name; };
constexpr EntInfo BUILDING_INFOS[] = {
    { EntityType::Base,     "Base"     },
    { EntityType::Barracks, "Barracks" },
    { EntityType::Refinery, "Refinery" },
    { EntityType::Factory,  "Factory"  },
};
constexpr int NUM_BUILDINGS = 4;

constexpr EntInfo UNIT_INFOS[] = {
    { EntityType::Worker,    "Worker"    },
    { EntityType::Soldier,   "Soldier"   },
    { EntityType::Brute,     "Brute"     },
    { EntityType::LightTank, "LightTank" },
};
constexpr int NUM_UNITS = 4;

// Panel colour palette
const sf::Color COL_PANEL_BG      = sf::Color( 40,  44,  52);
const sf::Color COL_MAP_BG        = sf::Color( 15,  17,  22);
const sf::Color COL_DIVIDER       = sf::Color( 70,  75,  85);
const sf::Color COL_LABEL         = sf::Color(150, 155, 165);
const sf::Color COL_TITLE_TXT     = sf::Color(220, 225, 235);
const sf::Color COL_BTN_NORMAL    = sf::Color( 60,  65,  75);
const sf::Color COL_BTN_HOVER     = sf::Color( 80,  90, 110);
const sf::Color COL_BTN_SEL       = sf::Color( 55,  90, 160);
const sf::Color COL_BTN_OUTLINE   = sf::Color( 90,  95, 105);
const sf::Color COL_INPUT_BG      = sf::Color( 28,  31,  38);
const sf::Color COL_SELECTED      = sf::Color( 80, 140, 255);
const sf::Color COL_SWATCH_SEL    = sf::Color(255, 220,  50);
const sf::Color COL_SWATCH_NRM    = sf::Color( 70,  75,  85);
const sf::Color COL_ITEM_SEL_OUT  = sf::Color(255, 220,  50);

} // namespace

// ===========================================================================
// Static helpers
// ===========================================================================
sf::Color MapEditorScreen::teamColor(Team t) {
    switch (t) {
        case Team::Player:  return sf::Color( 50, 120, 220);
        case Team::Enemy:   return sf::Color(200,  50,  50);
        default:            return sf::Color(180, 180, 180);
    }
}

// ===========================================================================
// Constructor
// ===========================================================================
MapEditorScreen::MapEditorScreen()
    : m_map(Constants::MAP_WIDTH, Constants::MAP_HEIGHT, /*generateRandom=*/false)
    , m_gridLines(sf::PrimitiveType::Lines)
{
    if (!m_font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        if (!m_font.openFromFile("C:/Windows/Fonts/segoeui.ttf")) {
            std::cerr << "MapEditorScreen: could not load font\n";
        } else {
            m_fontLoaded = true;
        }
    } else {
        m_fontLoaded = true;
    }

    m_hoverRect.setSize({ static_cast<float>(Constants::TILE_SIZE),
                          static_cast<float>(Constants::TILE_SIZE) });
    m_hoverRect.setFillColor(sf::Color(255, 255, 120, 55));
    m_hoverRect.setOutlineColor(sf::Color(255, 255, 0, 200));
    m_hoverRect.setOutlineThickness(2.f);

    m_placementPreview.setFillColor(sf::Color(255, 255, 255, 60));
    m_placementPreview.setOutlineThickness(2.f);
}

void MapEditorScreen::onEnter() {
    m_lastWinSize = { 0u, 0u };
}

// ===========================================================================
// Panel layout helpers
// ===========================================================================
void MapEditorScreen::makeDivider(sf::RectangleShape& div, float y) {
    div.setPosition({ 0.f, y - m_panelScrollY });
    div.setSize({ PANEL_WIDTH, 1.f });
    div.setFillColor(COL_DIVIDER);
}

void MapEditorScreen::makeSectionLabel(std::optional<sf::Text>& lbl,
                                       const std::string& text, float y)
{
    if (!m_fontLoaded) return;
    lbl.emplace(m_font, text, 11u);
    lbl->setFillColor(COL_LABEL);
    lbl->setPosition({ 10.f, y - m_panelScrollY });
}

MapEditorScreen::PanelButton MapEditorScreen::makePanelButton(
    const std::string& text, sf::Vector2f pos, sf::Vector2f size,
    unsigned int fontSize)
{
    PanelButton btn;
    sf::Vector2f scrolledPos = { pos.x, pos.y - m_panelScrollY };
    btn.shape.setPosition(scrolledPos);
    btn.shape.setSize(size);
    btn.shape.setFillColor(COL_BTN_NORMAL);
    btn.shape.setOutlineColor(COL_BTN_OUTLINE);
    btn.shape.setOutlineThickness(1.f);

    if (m_fontLoaded) {
        btn.label.emplace(m_font, text, fontSize);
        btn.label->setFillColor(COL_TITLE_TXT);
        sf::FloatRect lb = btn.label->getLocalBounds();
        btn.label->setOrigin({ lb.position.x + lb.size.x * 0.5f,
                               lb.position.y + lb.size.y * 0.5f });
        btn.label->setPosition({ scrolledPos.x + size.x * 0.5f,
                                  scrolledPos.y + size.y * 0.5f });
    }
    return btn;
}

MapEditorScreen::EntityItem MapEditorScreen::makeEntityItem(
    EntityType type, sf::Vector2f pos, sf::Vector2f size)
{
    EntityItem item;
    item.type = type;
    sf::Vector2f scrolledPos = { pos.x, pos.y - m_panelScrollY };
    item.shape.setPosition(scrolledPos);
    item.shape.setSize(size);
    item.shape.setFillColor(COL_BTN_NORMAL);
    item.shape.setOutlineColor(COL_SWATCH_NRM);
    item.shape.setOutlineThickness(2.f);

    if (m_fontLoaded) {
        std::string abbr = ENTITY_DATA.getShortName(type);
        item.abbrev.emplace(m_font, abbr, 14u);
        item.abbrev->setFillColor(COL_TITLE_TXT);
        item.abbrev->setStyle(sf::Text::Bold);
        sf::FloatRect ab = item.abbrev->getLocalBounds();
        item.abbrev->setOrigin({ ab.position.x + ab.size.x * 0.5f,
                                  ab.position.y + ab.size.y * 0.5f });
        item.abbrev->setPosition({ scrolledPos.x + size.x * 0.5f,
                                    scrolledPos.y + size.y * 0.5f - 6.f });

        std::string name = ENTITY_DATA.getName(type);
        if (name.size() > 8) name = name.substr(0, 8);
        item.label.emplace(m_font, name, 9u);
        item.label->setFillColor(COL_LABEL);
        sf::FloatRect lb = item.label->getLocalBounds();
        item.label->setOrigin({ lb.position.x + lb.size.x * 0.5f, 0.f });
        item.label->setPosition({ scrolledPos.x + size.x * 0.5f,
                                   scrolledPos.y + size.y - 14.f });
    }
    return item;
}

// ---------------------------------------------------------------------------
// Build the full panel layout. y tracks content position (unscrolled).
// ---------------------------------------------------------------------------
void MapEditorScreen::buildLayout(sf::Vector2u winSize) {
    const bool firstTime = (m_lastWinSize.x == 0 && m_lastWinSize.y == 0);

    const float PW  = PANEL_WIDTH;
    const float PAD = 10.f;
    const float IW  = PW - PAD * 2.f;
    float y = 0.f;

    m_panelBg.setPosition({ 0.f, 0.f });
    m_panelBg.setSize({ PW, static_cast<float>(winSize.y) });
    m_panelBg.setFillColor(COL_PANEL_BG);

    // ---- Title bar ---------------------------------------------------------
    if (m_fontLoaded) {
        m_lblTitle.emplace(m_font, "MAP EDITOR", 15u);
        m_lblTitle->setFillColor(COL_TITLE_TXT);
        m_lblTitle->setStyle(sf::Text::Bold);
        sf::FloatRect lb = m_lblTitle->getLocalBounds();
        m_lblTitle->setOrigin({ lb.position.x + lb.size.x * 0.5f,
                                lb.position.y + lb.size.y * 0.5f });
        m_lblTitle->setPosition({ PW * 0.5f, 18.f });
    }
    y = 38.f;

    makeDivider(m_divider1, y); y += 9.f;

    // ---- Properties --------------------------------------------------------
    makeSectionLabel(m_lblProps, "PROPERTIES", y); y += 18.f;
    makeSectionLabel(m_lblName,  "Map Name",   y); y += 16.f;

    m_nameBox.setPosition({ PAD, y - m_panelScrollY });
    m_nameBox.setSize({ IW, 24.f });
    m_nameBox.setFillColor(COL_INPUT_BG);
    m_nameBox.setOutlineColor(m_nameActive ? COL_SELECTED : COL_DIVIDER);
    m_nameBox.setOutlineThickness(1.f);

    if (m_fontLoaded) {
        m_nameText.emplace(m_font, m_mapName, 13u);
        m_nameText->setFillColor(COL_TITLE_TXT);
        m_nameText->setPosition({ PAD + 4.f, y + 5.f - m_panelScrollY });
    }
    y += 30.f;

    if (m_fontLoaded) {
        std::string sz = "Size:  " + std::to_string(m_mapW) + " x " +
                         std::to_string(m_mapH) + "  tiles";
        m_lblSize.emplace(m_font, sz, 12u);
        m_lblSize->setFillColor(COL_LABEL);
        m_lblSize->setPosition({ PAD, y - m_panelScrollY });
    }
    y += 22.f;

    {
        float bW = (IW - 8.f) / 3.f, bH = 26.f;
        m_btnNew  = makePanelButton("New",  { PAD,                   y }, { bW, bH });
        m_btnLoad = makePanelButton("Load", { PAD + bW + 4.f,        y }, { bW, bH });
        m_btnSave = makePanelButton("Save", { PAD + 2.f*(bW + 4.f),  y }, { bW, bH });
    }
    y += 32.f;
    m_btnBack = makePanelButton("Back to Menu", { PAD, y }, { IW, 26.f }); y += 34.f;

    makeDivider(m_dividerPalette, y); y += 9.f;

    // ---- Tile Palette ------------------------------------------------------
    makeSectionLabel(m_lblPalette, "TILE PALETTE", y); y += 20.f;
    buildTileSwatches(y);

    makeDivider(m_dividerNeutral, y); y += 9.f;

    // ---- Neutral entities --------------------------------------------------
    makeSectionLabel(m_lblNeutral, "NEUTRAL", y); y += 20.f;
    buildNeutralItems(y);

    makeDivider(m_dividerBuildings, y); y += 9.f;

    // ---- Buildings ---------------------------------------------------------
    makeSectionLabel(m_lblBuildings, "BUILDINGS", y); y += 20.f;
    buildBuildingItems(y);

    makeDivider(m_dividerUnits, y); y += 9.f;

    // ---- Units -------------------------------------------------------------
    makeSectionLabel(m_lblUnits, "UNITS", y); y += 20.f;
    buildUnitItems(y);

    y += 10.f;
    m_panelContentH = y;

    // ---- Camera ------------------------------------------------------------
    updateCameraViewport(winSize);
    if (firstTime) {
        resetCamera(winSize);
    } else {
        float oldAreaW = static_cast<float>(m_lastWinSize.x) - PANEL_WIDTH;
        if (oldAreaW > 0.f && m_camera.getSize().x > 0.f) {
            float wpp = m_camera.getSize().x / oldAreaW;
            float newAreaW = static_cast<float>(winSize.x) - PANEL_WIDTH;
            float newAreaH = static_cast<float>(winSize.y);
            m_camera.setSize({ newAreaW * wpp, newAreaH * wpp });
        }
    }
}

void MapEditorScreen::buildTileSwatches(float& y) {
    m_swatches.clear();
    const float PAD = 10.f, SW = 56.f, SH = 68.f, GAP = 8.f;
    for (int i = 0; i < NUM_TILE_INFOS; ++i) {
        const auto& info = TILE_INFOS[i];
        int col = i % 4, row = i / 4;
        float x = PAD + col * (SW + GAP);
        float sy = y + row * (SH + GAP);

        TileSwatch sw;
        sw.type = info.type;
        sw.baseColor = info.color;
        sw.name = info.name;
        sw.shape.setPosition({ x, sy - m_panelScrollY });
        sw.shape.setSize({ SW, SH });
        sw.shape.setFillColor(info.color);
        sw.shape.setOutlineThickness(2.f);
        sw.shape.setOutlineColor(info.type == m_selectedTile ? COL_SWATCH_SEL : COL_SWATCH_NRM);
        if (m_fontLoaded) {
            sw.label.emplace(m_font, info.name, 9u);
            sw.label->setFillColor(sf::Color::White);
            sf::FloatRect lb = sw.label->getLocalBounds();
            sw.label->setOrigin({ lb.position.x + lb.size.x * 0.5f, 0.f });
            sw.label->setPosition({ x + SW * 0.5f, sy + SH - 15.f - m_panelScrollY });
        }
        m_swatches.push_back(std::move(sw));
    }
    int rows = (NUM_TILE_INFOS + 3) / 4;
    y += rows * (SH + GAP) + 4.f;
}

void MapEditorScreen::buildNeutralItems(float& y) {
    m_neutralItems.clear();
    const float PAD = 10.f, IW = PANEL_WIDTH - PAD * 2.f;
    const float SW = 56.f, SH = 60.f, GAP = 8.f;
    for (int i = 0; i < NUM_NEUTRAL; ++i) {
        const auto& info = NEUTRAL_INFOS[i];
        int col = i % 4, row = i / 4;
        float x = PAD + col * (SW + GAP);
        float sy = y + row * (SH + GAP);

        EntityItem item = makeEntityItem(info.type, { x, sy }, { SW, SH });
        item.shape.setFillColor(info.color * sf::Color(100, 100, 100, 255));
        item.shape.setOutlineColor(
            (m_pendingEntityType == info.type && m_pendingTeam == Team::Neutral)
            ? COL_ITEM_SEL_OUT : COL_SWATCH_NRM);
        m_neutralItems.push_back(std::move(item));
    }
    int rows = (NUM_NEUTRAL + 3) / 4;
    y += rows * (SH + GAP) + 4.f;
    (void)IW;
}

void MapEditorScreen::buildBuildingItems(float& y) {
    m_buildingItems.clear();
    const float PAD = 10.f, IW = PANEL_WIDTH - PAD * 2.f;
    const float BH = 24.f, SW = 56.f, SH = 60.f, GAP = 8.f;

    // Team selector
    float bW = (IW - 4.f) / 2.f;
    m_btnBldTeamPlayer = makePanelButton("Player", { PAD,           y }, { bW, BH });
    m_btnBldTeamEnemy  = makePanelButton("Enemy",  { PAD + bW + 4.f, y }, { bW, BH });
    m_btnBldTeamPlayer.selected = (m_bldTeam == Team::Player);
    m_btnBldTeamEnemy.selected  = (m_bldTeam == Team::Enemy);
    m_btnBldTeamPlayer.shape.setFillColor(m_bldTeam == Team::Player ? COL_BTN_SEL : COL_BTN_NORMAL);
    m_btnBldTeamEnemy.shape.setFillColor(m_bldTeam == Team::Enemy   ? COL_BTN_SEL : COL_BTN_NORMAL);
    y += BH + 6.f;

    for (int i = 0; i < NUM_BUILDINGS; ++i) {
        const auto& info = BUILDING_INFOS[i];
        int col = i % 4, row = i / 4;
        float x = PAD + col * (SW + GAP);
        float sy = y + row * (SH + GAP);

        EntityItem item = makeEntityItem(info.type, { x, sy }, { SW, SH });
        item.shape.setOutlineColor(
            (m_pendingEntityType == info.type && m_pendingTeam == m_bldTeam)
            ? COL_ITEM_SEL_OUT : COL_SWATCH_NRM);
        m_buildingItems.push_back(std::move(item));
    }
    int rows = (NUM_BUILDINGS + 3) / 4;
    y += rows * (SH + GAP) + 4.f;
    (void)IW;
}

void MapEditorScreen::buildUnitItems(float& y) {
    m_unitItems.clear();
    const float PAD = 10.f, IW = PANEL_WIDTH - PAD * 2.f;
    const float BH = 24.f, SW = 56.f, SH = 60.f, GAP = 8.f;

    // Team selector
    float bW = (IW - 4.f) / 2.f;
    m_btnUnitTeamPlayer = makePanelButton("Player", { PAD,            y }, { bW, BH });
    m_btnUnitTeamEnemy  = makePanelButton("Enemy",  { PAD + bW + 4.f, y }, { bW, BH });
    m_btnUnitTeamPlayer.selected = (m_unitTeam == Team::Player);
    m_btnUnitTeamEnemy.selected  = (m_unitTeam == Team::Enemy);
    m_btnUnitTeamPlayer.shape.setFillColor(m_unitTeam == Team::Player ? COL_BTN_SEL : COL_BTN_NORMAL);
    m_btnUnitTeamEnemy.shape.setFillColor(m_unitTeam == Team::Enemy   ? COL_BTN_SEL : COL_BTN_NORMAL);
    y += BH + 6.f;

    for (int i = 0; i < NUM_UNITS; ++i) {
        const auto& info = UNIT_INFOS[i];
        int col = i % 4, row = i / 4;
        float x = PAD + col * (SW + GAP);
        float sy = y + row * (SH + GAP);

        EntityItem item = makeEntityItem(info.type, { x, sy }, { SW, SH });
        item.shape.setOutlineColor(
            (m_pendingEntityType == info.type && m_pendingTeam == m_unitTeam)
            ? COL_ITEM_SEL_OUT : COL_SWATCH_NRM);
        m_unitItems.push_back(std::move(item));
    }
    int rows = (NUM_UNITS + 3) / 4;
    y += rows * (SH + GAP) + 4.f;
    (void)IW;
}

// ===========================================================================
// Camera
// ===========================================================================
void MapEditorScreen::resetCamera(sf::Vector2u winSize) {
    float mapW = static_cast<float>(m_mapW * Constants::TILE_SIZE);
    float mapH = static_cast<float>(m_mapH * Constants::TILE_SIZE);
    float aW   = static_cast<float>(winSize.x) - PANEL_WIDTH;
    float aH   = static_cast<float>(winSize.y);
    float scale = std::min(aW / mapW, aH / mapH);
    m_camera.setSize({ aW / scale, aH / scale });
    m_camera.setCenter({ mapW * 0.5f, mapH * 0.5f });
}

void MapEditorScreen::updateCameraViewport(sf::Vector2u winSize) {
    float wf   = static_cast<float>(winSize.x);
    float left = PANEL_WIDTH / wf;
    float wide = (wf - PANEL_WIDTH) / wf;
    m_camera.setViewport(sf::FloatRect({ left, 0.f }, { wide, 1.f }));
}

// ===========================================================================
// Grid
// ===========================================================================
void MapEditorScreen::buildGridLines() {
    float ts = static_cast<float>(Constants::TILE_SIZE);
    float mW = static_cast<float>(m_mapW * Constants::TILE_SIZE);
    float mH = static_cast<float>(m_mapH * Constants::TILE_SIZE);
    m_gridLines.resize(static_cast<std::size_t>((m_mapH + 1 + m_mapW + 1) * 2));
    sf::Color gc(0, 0, 0, 50);
    std::size_t vi = 0;
    for (int r = 0; r <= m_mapH; ++r) {
        float py = r * ts;
        m_gridLines[vi  ].position = { 0.f, py }; m_gridLines[vi  ].color = gc;
        m_gridLines[vi+1].position = { mW,  py }; m_gridLines[vi+1].color = gc;
        vi += 2;
    }
    for (int c = 0; c <= m_mapW; ++c) {
        float px = c * ts;
        m_gridLines[vi  ].position = { px, 0.f }; m_gridLines[vi  ].color = gc;
        m_gridLines[vi+1].position = { px, mH  }; m_gridLines[vi+1].color = gc;
        vi += 2;
    }
    m_gridDirty = false;
}

// ===========================================================================
// UI hit-testing helpers
// ===========================================================================
void MapEditorScreen::updateButtonHover(PanelButton& btn, sf::Vector2f pm) {
    btn.hovered = btnHit(btn, pm);
    if (!btn.selected)
        btn.shape.setFillColor(btn.hovered ? COL_BTN_HOVER : COL_BTN_NORMAL);
}

void MapEditorScreen::updateEntityItemHover(EntityItem& item, sf::Vector2f pm) {
    item.hovered = itemHit(item, pm);
}

bool MapEditorScreen::btnHit(const PanelButton& btn, sf::Vector2f pm) const {
    return sf::FloatRect(btn.shape.getPosition(), btn.shape.getSize()).contains(pm);
}

bool MapEditorScreen::itemHit(const EntityItem& item, sf::Vector2f pm) const {
    return sf::FloatRect(item.shape.getPosition(), item.shape.getSize()).contains(pm);
}

bool MapEditorScreen::inMapArea(sf::Vector2i px) const {
    return px.x >= static_cast<int>(PANEL_WIDTH);
}

sf::Vector2i MapEditorScreen::screenToTile(sf::Vector2i pixel) const
{
    if (m_lastWinSize.x == 0) return { -1, -1 };
    float aW = static_cast<float>(m_lastWinSize.x) - PANEL_WIDTH;
    float aH = static_cast<float>(m_lastWinSize.y);
    if (aW <= 0.f || aH <= 0.f) return { -1, -1 };
    float localX = static_cast<float>(pixel.x) - PANEL_WIDTH;
    float localY = static_cast<float>(pixel.y);
    sf::Vector2f cSz = m_camera.getSize();
    sf::Vector2f cCt = m_camera.getCenter();
    float wx = cCt.x - cSz.x * 0.5f + (localX / aW) * cSz.x;
    float wy = cCt.y - cSz.y * 0.5f + (localY / aH) * cSz.y;
    int tx = static_cast<int>(std::floor(wx / Constants::TILE_SIZE));
    int ty = static_cast<int>(std::floor(wy / Constants::TILE_SIZE));
    if (tx < 0 || tx >= m_mapW || ty < 0 || ty >= m_mapH) return { -1, -1 };
    return { tx, ty };
}

// ===========================================================================
// Entity placement
// ===========================================================================
void MapEditorScreen::selectPendingEntity(EntityType type, Team team) {
    m_pendingEntityType = type;
    m_pendingTeam       = team;

    // Size the preview based on tile footprint
    sf::Vector2i tileSize = ENTITY_DATA.getBuildingTileSize(type);
    // For units, tileSize comes back {1,1} which is fine
    float pw = static_cast<float>(tileSize.x * Constants::TILE_SIZE);
    float ph = static_cast<float>(tileSize.y * Constants::TILE_SIZE);
    m_placementPreview.setSize({ pw, ph });

    sf::Color tc = (team == Team::Neutral) ? sf::Color(180, 180, 180) : teamColor(team);
    m_placementPreview.setFillColor(sf::Color(tc.r, tc.g, tc.b, 70));
    m_placementPreview.setOutlineColor(sf::Color(tc.r, tc.g, tc.b, 200));
    m_placementPreview.setOutlineThickness(2.f);
}

void MapEditorScreen::clearPendingEntity() {
    m_pendingEntityType = EntityType::None;
}

void MapEditorScreen::tryPlaceEntity(sf::Vector2i pixel) {
    if (m_pendingEntityType == EntityType::None) return;
    sf::Vector2i t = screenToTile(pixel);
    if (t.x < 0) return;

    sf::Vector2i tileSize = ENTITY_DATA.getBuildingTileSize(m_pendingEntityType);
    // Bounds check
    if (t.x + tileSize.x > m_mapW || t.y + tileSize.y > m_mapH) return;

    // Remove any existing entity whose footprint overlaps
    m_placedEntities.erase(
        std::remove_if(m_placedEntities.begin(), m_placedEntities.end(),
            [&](const PlacedEntity& pe) {
                sf::Vector2i ps = ENTITY_DATA.getBuildingTileSize(pe.type);
                bool overlapX = !(t.x >= pe.tileX + ps.x || t.x + tileSize.x <= pe.tileX);
                bool overlapY = !(t.y >= pe.tileY + ps.y || t.y + tileSize.y <= pe.tileY);
                return overlapX && overlapY;
            }),
        m_placedEntities.end());

    m_placedEntities.push_back({ m_pendingEntityType, m_pendingTeam, t.x, t.y });

    // Mark building tiles on map; units leave tiles as-is
    const EntityDef* def = ENTITY_DATA.get(m_pendingEntityType);
    if (def && def->isBuilding()) {
        TileType tt = def->isResource() ? TileType::Resource : TileType::Building;
        for (int dy = 0; dy < tileSize.y; ++dy)
            for (int dx = 0; dx < tileSize.x; ++dx)
                m_map.setTileType(t.x + dx, t.y + dy, tt);
    }
}

// ===========================================================================
// Event handling
// ===========================================================================
ScreenResult MapEditorScreen::handleEvent(const sf::Event& event) {

    // Convert raw pixel to "panel-scrolled" space for UI hit tests
    auto panelMouse = [&](sf::Vector2i px) -> sf::Vector2f {
        return { static_cast<float>(px.x),
                 static_cast<float>(px.y) };  // panel widgets already scrolled
    };

    // ---- Resize ------------------------------------------------------------
    if (const auto* r = event.getIf<sf::Event::Resized>()) {
        sf::Vector2u ns = r->size;
        m_panelScrollY = std::min(m_panelScrollY,
            std::max(0.f, m_panelContentH - static_cast<float>(ns.y)));
        buildLayout(ns);
        m_lastWinSize = ns;
        return {};
    }

    // ---- Mouse wheel (scroll panel OR zoom map) ----------------------------
    if (const auto* mw = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (!inMapArea(mw->position)) {
            // Scroll panel
            float maxScroll = std::max(0.f,
                m_panelContentH - static_cast<float>(m_lastWinSize.y));
            m_panelScrollY = std::clamp(m_panelScrollY - mw->delta * 20.f,
                                        0.f, maxScroll);
            buildLayout(m_lastWinSize);
        } else {
            float factor = (mw->delta > 0) ? 0.85f : 1.18f;
            sf::Vector2f ns = m_camera.getSize() * factor;
            float minW = 4.f * Constants::TILE_SIZE;
            float maxW = 4.f * static_cast<float>(m_mapW * Constants::TILE_SIZE);
            ns.x = std::clamp(ns.x, minW, maxW);
            ns.y = ns.x * (m_camera.getSize().y / m_camera.getSize().x);
            m_camera.setSize(ns);
        }
        return {};
    }

    // ---- Mouse move --------------------------------------------------------
    if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
        sf::Vector2f pm = panelMouse(mm->position);

        updateButtonHover(m_btnNew,  pm);
        updateButtonHover(m_btnLoad, pm);
        updateButtonHover(m_btnSave, pm);
        updateButtonHover(m_btnBack, pm);
        updateButtonHover(m_btnBldTeamPlayer, pm);
        updateButtonHover(m_btnBldTeamEnemy,  pm);
        updateButtonHover(m_btnUnitTeamPlayer, pm);
        updateButtonHover(m_btnUnitTeamEnemy,  pm);

        for (auto& sw   : m_swatches)      { (void)sw; }
        for (auto& item : m_neutralItems)  updateEntityItemHover(item, pm);
        for (auto& item : m_buildingItems) updateEntityItemHover(item, pm);
        for (auto& item : m_unitItems)     updateEntityItemHover(item, pm);

        // Camera pan (right-drag)
        if (m_isPanning && m_lastWinSize.x > 0) {
            float aW = static_cast<float>(m_lastWinSize.x) - PANEL_WIDTH;
            float aH = static_cast<float>(m_lastWinSize.y);
            float sx = m_camera.getSize().x / aW;
            float sy = m_camera.getSize().y / aH;
            sf::Vector2f delta(static_cast<float>(mm->position.x - m_panStart.x),
                               static_cast<float>(mm->position.y - m_panStart.y));
            m_camera.setCenter({ m_panCameraStart.x - delta.x * sx,
                                  m_panCameraStart.y - delta.y * sy });
        }

        if (inMapArea(mm->position))
            m_hoveredTile = mm->position;
        else
            m_hoveredTile = { -1, -1 };

        return {};
    }

    // ---- Mouse button pressed ----------------------------------------------
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
        sf::Vector2f pm = panelMouse(mb->position);

        if (mb->button == sf::Mouse::Button::Left) {

            // ---- Load overlay (intercepts all clicks while open) -----------
            if (m_showLoadPanel) {
                if (btnHit(m_btnLoadCancel, pm)) {
                    m_showLoadPanel = false;
                    return {};
                }
                for (size_t i = 0; i < m_loadButtons.size(); ++i) {
                    if (btnHit(m_loadButtons[i], pm)) {
                        bool ok = loadMapByName(m_loadMapFiles[i]);
                        m_showLoadPanel = false;
                        if (m_fontLoaded) {
                            m_statusText.emplace(m_font,
                                ok ? "Loaded: " + m_loadMapFiles[i] : "Load failed!",
                                12u);
                            m_statusText->setFillColor(ok ? sf::Color(80,220,80) : sf::Color(220,80,80));
                        }
                        m_statusTimer = 4.f;
                        return {};
                    }
                }
                // Click outside overlay dismisses it
                sf::FloatRect ovr(m_loadOverlayBg.getPosition(), m_loadOverlayBg.getSize());
                if (!ovr.contains(pm)) m_showLoadPanel = false;
                return {};
            }

            // ---- File / navigation buttons ---------------------------------
            if (btnHit(m_btnBack, pm)) return { ScreenResult::Action::BackToMenu, "" };
            if (btnHit(m_btnNew,  pm)) { m_map.initEmpty(); m_placedEntities.clear(); return {}; }
            if (btnHit(m_btnLoad, pm)) {
                refreshLoadPanel(m_lastWinSize);
                m_showLoadPanel = true;
                return {};
            }
            if (btnHit(m_btnSave, pm)) {
                bool ok = saveCurrentMap();
                if (m_fontLoaded) {
                    m_statusText.emplace(m_font,
                        ok ? "Saved to maps/" + m_mapName + ".stmap" : "Save failed!",
                        12u);
                    m_statusText->setFillColor(ok ? sf::Color(80, 220, 80) : sf::Color(220, 80, 80));
                }
                m_statusTimer = 4.f;
                return {};
            }

            // ---- Building team selector ------------------------------------
            if (btnHit(m_btnBldTeamPlayer, pm)) {
                m_bldTeam = Team::Player;
                if (m_pendingEntityType != EntityType::None) {
                    bool isBld = false;
                    for (int i = 0; i < NUM_BUILDINGS; ++i)
                        if (BUILDING_INFOS[i].type == m_pendingEntityType) { isBld = true; break; }
                    if (isBld) selectPendingEntity(m_pendingEntityType, m_bldTeam);
                }
                buildLayout(m_lastWinSize);
                return {};
            }
            if (btnHit(m_btnBldTeamEnemy, pm)) {
                m_bldTeam = Team::Enemy;
                if (m_pendingEntityType != EntityType::None) {
                    bool isBld = false;
                    for (int i = 0; i < NUM_BUILDINGS; ++i)
                        if (BUILDING_INFOS[i].type == m_pendingEntityType) { isBld = true; break; }
                    if (isBld) selectPendingEntity(m_pendingEntityType, m_bldTeam);
                }
                buildLayout(m_lastWinSize);
                return {};
            }

            // ---- Unit team selector ----------------------------------------
            if (btnHit(m_btnUnitTeamPlayer, pm)) {
                m_unitTeam = Team::Player;
                if (m_pendingEntityType != EntityType::None) {
                    bool isUnit = false;
                    for (int i = 0; i < NUM_UNITS; ++i)
                        if (UNIT_INFOS[i].type == m_pendingEntityType) { isUnit = true; break; }
                    if (isUnit) selectPendingEntity(m_pendingEntityType, m_unitTeam);
                }
                buildLayout(m_lastWinSize);
                return {};
            }
            if (btnHit(m_btnUnitTeamEnemy, pm)) {
                m_unitTeam = Team::Enemy;
                if (m_pendingEntityType != EntityType::None) {
                    bool isUnit = false;
                    for (int i = 0; i < NUM_UNITS; ++i)
                        if (UNIT_INFOS[i].type == m_pendingEntityType) { isUnit = true; break; }
                    if (isUnit) selectPendingEntity(m_pendingEntityType, m_unitTeam);
                }
                buildLayout(m_lastWinSize);
                return {};
            }

            // ---- Tile swatches ---------------------------------------------
            for (auto& sw : m_swatches) {
                if (sf::FloatRect(sw.shape.getPosition(), sw.shape.getSize()).contains(pm)) {
                    m_selectedTile = sw.type;
                    clearPendingEntity();
                    for (auto& s : m_swatches)
                        s.shape.setOutlineColor(s.type == m_selectedTile ? COL_SWATCH_SEL : COL_SWATCH_NRM);
                    return {};
                }
            }

            // ---- Neutral items ---------------------------------------------
            for (auto& item : m_neutralItems) {
                if (itemHit(item, pm)) {
                    bool alreadySel = (m_pendingEntityType == item.type);
                    clearPendingEntity();
                    if (!alreadySel) selectPendingEntity(item.type, Team::Neutral);
                    buildLayout(m_lastWinSize);
                    return {};
                }
            }

            // ---- Building items --------------------------------------------
            for (auto& item : m_buildingItems) {
                if (itemHit(item, pm)) {
                    bool alreadySel = (m_pendingEntityType == item.type && m_pendingTeam == m_bldTeam);
                    clearPendingEntity();
                    if (!alreadySel) selectPendingEntity(item.type, m_bldTeam);
                    buildLayout(m_lastWinSize);
                    return {};
                }
            }

            // ---- Unit items ------------------------------------------------
            for (auto& item : m_unitItems) {
                if (itemHit(item, pm)) {
                    bool alreadySel = (m_pendingEntityType == item.type && m_pendingTeam == m_unitTeam);
                    clearPendingEntity();
                    if (!alreadySel) selectPendingEntity(item.type, m_unitTeam);
                    buildLayout(m_lastWinSize);
                    return {};
                }
            }

            // ---- Name box focus --------------------------------------------
            sf::FloatRect nr(m_nameBox.getPosition(), m_nameBox.getSize());
            m_nameActive = nr.contains(pm);
            m_nameBox.setOutlineColor(m_nameActive ? COL_SELECTED : COL_DIVIDER);

            // ---- Map interaction -------------------------------------------
            if (inMapArea(mb->position)) {
                if (m_pendingEntityType != EntityType::None) {
                    tryPlaceEntity(mb->position);
                    m_hoveredTile = mb->position;
                    m_isPainting  = false;   // entity placement is single-click
                } else {
                    m_isPainting  = true;
                    m_hoveredTile = mb->position;
                }
            }
        }

            if (mb->button == sf::Mouse::Button::Right) {
            if (m_showLoadPanel) { m_showLoadPanel = false; return {}; }
            if (inMapArea(mb->position)) {
                m_isPanning      = true;
                m_panStart       = mb->position;
                m_panCameraStart = m_camera.getCenter();
            } else {
                // Right-click on panel: cancel entity placement
                clearPendingEntity();
                buildLayout(m_lastWinSize);
            }
        }
        return {};
    }

    // ---- Mouse button released ---------------------------------------------
    if (const auto* mb = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (mb->button == sf::Mouse::Button::Left)  m_isPainting = false;
        if (mb->button == sf::Mouse::Button::Right) m_isPanning  = false;
        return {};
    }

    // ---- Keyboard ----------------------------------------------------------
    if (const auto* kp = event.getIf<sf::Event::KeyPressed>()) {
        if (kp->code == sf::Keyboard::Key::Escape) {
            if (m_pendingEntityType != EntityType::None) {
                clearPendingEntity();
                buildLayout(m_lastWinSize);
            } else if (m_nameActive) {
                m_nameActive = false;
                m_nameBox.setOutlineColor(COL_DIVIDER);
            }
        }
        if (m_nameActive) {
            if (kp->code == sf::Keyboard::Key::Backspace && !m_mapName.empty()) {
                m_mapName.pop_back();
                if (m_nameText) m_nameText->setString(m_mapName);
            }
            if (kp->code == sf::Keyboard::Key::Enter) {
                m_nameActive = false;
                m_nameBox.setOutlineColor(COL_DIVIDER);
            }
        } else if (m_pendingEntityType == EntityType::None) {
            const float PAN = static_cast<float>(Constants::TILE_SIZE * 3);
            sf::Vector2f c = m_camera.getCenter();
            if (kp->code == sf::Keyboard::Key::Left)  c.x -= PAN;
            if (kp->code == sf::Keyboard::Key::Right) c.x += PAN;
            if (kp->code == sf::Keyboard::Key::Up)    c.y -= PAN;
            if (kp->code == sf::Keyboard::Key::Down)  c.y += PAN;
            m_camera.setCenter(c);
        }
    }

    if (const auto* te = event.getIf<sf::Event::TextEntered>()) {
        if (m_nameActive && te->unicode >= 32u && te->unicode < 127u) {
            m_mapName += static_cast<char>(te->unicode);
            if (m_nameText) m_nameText->setString(m_mapName);
        }
    }

    return {};
}

// ===========================================================================
// Update
// ===========================================================================
// ===========================================================================
// Save / Load helpers
// ===========================================================================
static constexpr const char* MAPS_DIR = "maps";

bool MapEditorScreen::saveCurrentMap() {
    namespace fs = std::filesystem;
    MapData data;
    data.name   = m_mapName;
    data.width  = m_mapW;
    data.height = m_mapH;

    // Collect non-Ground tiles
    for (int y = 0; y < m_mapH; ++y) {
        for (int x = 0; x < m_mapW; ++x) {
            TileType tt = m_map.getTile(x, y).type;
            if (tt != TileType::Ground)
                data.tiles.push_back({ x, y, tt });
        }
    }

    // Collect entities
    for (const auto& pe : m_placedEntities)
        data.entities.push_back({ pe.type, pe.team, pe.tileX, pe.tileY });

    std::string stem = m_mapName;
    // Replace spaces with underscores in filename only
    std::replace(stem.begin(), stem.end(), ' ', '_');
    std::string path = std::string(MAPS_DIR) + "/" + stem + ".stmap";
    return MapSerializer::save(data, path);
}

bool MapEditorScreen::loadMapByName(const std::string& stem) {
    std::string path = std::string(MAPS_DIR) + "/" + stem + ".stmap";
    auto result = MapSerializer::load(path);
    if (!result) return false;
    applyMapData(*result);
    return true;
}

void MapEditorScreen::applyMapData(const MapData& data) {
    m_mapName = data.name;
    m_mapW    = data.width;
    m_mapH    = data.height;

    m_map = Map(m_mapW, m_mapH, /*generateRandom=*/false);
    m_placedEntities.clear();

    for (const auto& t : data.tiles)
        m_map.setTileType(t.x, t.y, t.type);

    for (const auto& e : data.entities)
        m_placedEntities.push_back({ e.type, e.team, e.tileX, e.tileY });

    m_gridDirty = true;
    buildLayout(m_lastWinSize);
}

void MapEditorScreen::refreshLoadPanel(sf::Vector2u winSize) {
    m_loadMapFiles = MapSerializer::listMaps(MAPS_DIR);
    m_loadButtons.clear();

    float wf = static_cast<float>(winSize.x);
    float hf = static_cast<float>(winSize.y);
    float ow = std::min(400.f, wf - 40.f);
    float bH = 34.f, gap = 8.f, pad = 16.f;
    float totalH = pad + 32.f + pad   // title
                 + static_cast<float>(m_loadMapFiles.size()) * (bH + gap)
                 + bH + pad;          // cancel
    float oh = std::min(totalH, hf - 40.f);

    float ox = (wf - ow) * 0.5f;
    float oy = (hf - oh) * 0.5f;

    m_loadOverlayBg.setPosition({ ox, oy });
    m_loadOverlayBg.setSize({ ow, oh });
    m_loadOverlayBg.setFillColor(sf::Color(30, 33, 42, 245));
    m_loadOverlayBg.setOutlineColor(sf::Color(80, 90, 130));
    m_loadOverlayBg.setOutlineThickness(2.f);

    if (m_fontLoaded) {
        m_lblLoadTitle.emplace(m_font, "Load Map", 18u);
        m_lblLoadTitle->setFillColor(sf::Color(220, 225, 235));
        m_lblLoadTitle->setStyle(sf::Text::Bold);
        sf::FloatRect lb = m_lblLoadTitle->getLocalBounds();
        m_lblLoadTitle->setOrigin({ lb.position.x + lb.size.x * 0.5f,
                                     lb.position.y + lb.size.y * 0.5f });
        m_lblLoadTitle->setPosition({ ox + ow * 0.5f, oy + pad + 10.f });
    }

    float by = oy + pad + 36.f;
    for (const auto& stem : m_loadMapFiles) {
        PanelButton btn = makePanelButton(stem, { ox + pad, by }, { ow - pad * 2.f, bH });
        m_loadButtons.push_back(std::move(btn));
        by += bH + gap;
    }

    m_btnLoadCancel = makePanelButton("Cancel",
        { ox + pad, oy + oh - bH - pad }, { ow - pad * 2.f, bH });
}

void MapEditorScreen::renderLoadOverlay(sf::RenderWindow& window) {
    if (!m_showLoadPanel) return;

    sf::Vector2u ws = window.getSize();
    sf::View uiView(sf::FloatRect({ 0.f, 0.f },
        { static_cast<float>(ws.x), static_cast<float>(ws.y) }));
    window.setView(uiView);

    // Semi-transparent full-screen dim
    sf::RectangleShape dim({ static_cast<float>(ws.x), static_cast<float>(ws.y) });
    dim.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(dim);

    window.draw(m_loadOverlayBg);
    if (m_lblLoadTitle) window.draw(*m_lblLoadTitle);

    if (m_loadMapFiles.empty() && m_fontLoaded) {
        sf::Text empty(m_font, "No maps found in maps/", 13u);
        empty.setFillColor(sf::Color(150, 155, 165));
        sf::FloatRect lb = empty.getLocalBounds();
        empty.setOrigin({ lb.position.x + lb.size.x * 0.5f,
                          lb.position.y + lb.size.y * 0.5f });
        sf::Vector2f oc = { m_loadOverlayBg.getPosition().x + m_loadOverlayBg.getSize().x * 0.5f,
                             m_loadOverlayBg.getPosition().y + m_loadOverlayBg.getSize().y * 0.5f };
        empty.setPosition(oc);
        window.draw(empty);
    }

    for (auto& btn : m_loadButtons) {
        window.draw(btn.shape);
        if (btn.label) window.draw(*btn.label);
    }
    window.draw(m_btnLoadCancel.shape);
    if (m_btnLoadCancel.label) window.draw(*m_btnLoadCancel.label);
}

// ===========================================================================
// Update
// ===========================================================================
ScreenResult MapEditorScreen::update(float dt) {
    if (m_statusTimer > 0.f) {
        m_statusTimer -= dt;
        if (m_statusTimer <= 0.f) {
            m_statusText.reset();
            m_statusTimer = 0.f;
        }
    }
    return {};
}

// ===========================================================================
// Render helpers
// ===========================================================================
void MapEditorScreen::renderPlacedEntities(sf::RenderWindow& window) {
    window.setView(m_camera);
    const float TS = static_cast<float>(Constants::TILE_SIZE);

    for (const auto& pe : m_placedEntities) {
        sf::Vector2i tileSize = ENTITY_DATA.getBuildingTileSize(pe.type);
        float pw = static_cast<float>(tileSize.x) * TS;
        float ph = static_cast<float>(tileSize.y) * TS;
        float wx = static_cast<float>(pe.tileX) * TS;
        float wy = static_cast<float>(pe.tileY) * TS;

        sf::Color tc = (pe.team == Team::Neutral)
            ? sf::Color(180, 180, 180) : teamColor(pe.team);

        sf::RectangleShape box({ pw, ph });
        box.setPosition({ wx, wy });
        box.setFillColor(sf::Color(tc.r, tc.g, tc.b, 90));
        box.setOutlineColor(tc);
        box.setOutlineThickness(2.f);
        window.draw(box);

        if (m_fontLoaded) {
            sf::Text abbr(m_font, ENTITY_DATA.getShortName(pe.type), 14u);
            abbr.setFillColor(sf::Color::White);
            abbr.setStyle(sf::Text::Bold);
            sf::FloatRect lb = abbr.getLocalBounds();
            abbr.setOrigin({ lb.position.x + lb.size.x * 0.5f,
                              lb.position.y + lb.size.y * 0.5f });
            abbr.setPosition({ wx + pw * 0.5f, wy + ph * 0.5f });
            window.draw(abbr);
        }
    }
}

void MapEditorScreen::renderPanel(sf::RenderWindow& window) {
    sf::Vector2u ws = window.getSize();
    sf::View uiView(sf::FloatRect({ 0.f, 0.f },
        { static_cast<float>(ws.x), static_cast<float>(ws.y) }));
    window.setView(uiView);

    window.draw(m_panelBg);
    window.draw(m_divider1);
    window.draw(m_dividerPalette);
    window.draw(m_dividerNeutral);
    window.draw(m_dividerBuildings);
    window.draw(m_dividerUnits);

    if (m_lblTitle)    window.draw(*m_lblTitle);
    if (m_lblProps)    window.draw(*m_lblProps);
    if (m_lblName)     window.draw(*m_lblName);
    if (m_lblSize)     window.draw(*m_lblSize);
    if (m_lblPalette)  window.draw(*m_lblPalette);
    if (m_lblNeutral)  window.draw(*m_lblNeutral);
    if (m_lblBuildings)window.draw(*m_lblBuildings);
    if (m_lblUnits)    window.draw(*m_lblUnits);

    window.draw(m_nameBox);
    if (m_nameText) window.draw(*m_nameText);

    if (m_nameActive && m_fontLoaded) {
        float tw = 0.f;
        if (m_nameText) {
            sf::FloatRect lb = m_nameText->getLocalBounds();
            tw = lb.position.x + lb.size.x;
        }
        sf::Text cur(m_font, "|", 14u);
        cur.setFillColor(COL_TITLE_TXT);
        cur.setPosition({ m_nameBox.getPosition().x + 4.f + tw,
                           m_nameBox.getPosition().y + 4.f });
        window.draw(cur);
    }

    auto drawBtn = [&](PanelButton& btn) {
        window.draw(btn.shape);
        if (btn.label) window.draw(*btn.label);
    };
    drawBtn(m_btnNew);  drawBtn(m_btnLoad);
    drawBtn(m_btnSave); drawBtn(m_btnBack);
    drawBtn(m_btnBldTeamPlayer); drawBtn(m_btnBldTeamEnemy);
    drawBtn(m_btnUnitTeamPlayer); drawBtn(m_btnUnitTeamEnemy);

    for (auto& sw : m_swatches) {
        window.draw(sw.shape);
        if (sw.label) window.draw(*sw.label);
    }
    for (auto& item : m_neutralItems) {
        window.draw(item.shape);
        if (item.abbrev) window.draw(*item.abbrev);
        if (item.label)  window.draw(*item.label);
    }
    for (auto& item : m_buildingItems) {
        window.draw(item.shape);
        if (item.abbrev) window.draw(*item.abbrev);
        if (item.label)  window.draw(*item.label);
    }
    for (auto& item : m_unitItems) {
        window.draw(item.shape);
        if (item.abbrev) window.draw(*item.abbrev);
        if (item.label)  window.draw(*item.label);
    }

    // Scroll indicator if panel overflows
    float wH = static_cast<float>(ws.y);
    if (m_panelContentH > wH) {
        float ratio    = wH / m_panelContentH;
        float barH     = std::max(20.f, wH * ratio);
        float barY     = (m_panelScrollY / (m_panelContentH - wH)) * (wH - barH);
        sf::RectangleShape bar({ 4.f, barH });
        bar.setPosition({ PANEL_WIDTH - 5.f, barY });
        bar.setFillColor(COL_DIVIDER);
        window.draw(bar);
    }
}

// ===========================================================================
// Render
// ===========================================================================
void MapEditorScreen::render(sf::RenderWindow& window) {
    sf::Vector2u winSize = window.getSize();

    if (winSize != m_lastWinSize) {
        buildLayout(winSize);
        m_lastWinSize = winSize;
    }
    if (m_gridDirty) buildGridLines();

    // ---- Map area bg -------------------------------------------------------
    {
        sf::View uiView(sf::FloatRect({ 0.f, 0.f },
            { static_cast<float>(winSize.x), static_cast<float>(winSize.y) }));
        window.setView(uiView);
        sf::RectangleShape bg({ static_cast<float>(winSize.x) - PANEL_WIDTH,
                                 static_cast<float>(winSize.y) });
        bg.setPosition({ PANEL_WIDTH, 0.f });
        bg.setFillColor(COL_MAP_BG);
        window.draw(bg);
    }

    // ---- Map + grid --------------------------------------------------------
    window.setView(m_camera);
    m_map.render(window, m_camera);
    window.draw(m_gridLines);

    // ---- Placed entities ---------------------------------------------------
    renderPlacedEntities(window);

    // ---- Hover / paint / placement preview ---------------------------------
    window.setView(m_camera);
    if (m_hoveredTile.x >= 0) {
        sf::Vector2i tile = screenToTile(m_hoveredTile);
        if (tile.x >= 0) {
            float wx = static_cast<float>(tile.x * Constants::TILE_SIZE);
            float wy = static_cast<float>(tile.y * Constants::TILE_SIZE);

            if (m_pendingEntityType != EntityType::None) {
                // Show placement ghost
                m_placementPreview.setPosition({ wx, wy });
                window.draw(m_placementPreview);
            } else {
                // Show tile hover highlight
                m_hoverRect.setPosition({ wx, wy });
                window.draw(m_hoverRect);

                if (m_isPainting)
                    m_map.setTileType(tile.x, tile.y, m_selectedTile);
            }
        }
    }

    // ---- Panel (on top of everything) -------------------------------------
    renderPanel(window);

    // ---- Load overlay (modal) ---------------------------------------------
    renderLoadOverlay(window);

    // ---- Status toast (bottom-left of panel) ------------------------------
    if (m_statusText && m_statusTimer > 0.f) {
        sf::Vector2u ws = window.getSize();
        sf::View uiView(sf::FloatRect({ 0.f, 0.f },
            { static_cast<float>(ws.x), static_cast<float>(ws.y) }));
        window.setView(uiView);

        sf::RectangleShape toast({ PANEL_WIDTH - 16.f, 24.f });
        toast.setPosition({ 8.f, static_cast<float>(ws.y) - 32.f });
        toast.setFillColor(sf::Color(20, 22, 30, 210));
        toast.setOutlineColor(sf::Color(60, 65, 80));
        toast.setOutlineThickness(1.f);
        window.draw(toast);

        m_statusText->setPosition({ 12.f, static_cast<float>(ws.y) - 29.f });
        window.draw(*m_statusText);
    }
}
