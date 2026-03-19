#include "EditorPanel.h"
#include "Constants.h"
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Palette / colour tables shared between panel and MapEditorScreen
// ---------------------------------------------------------------------------
namespace {

struct TileInfo { TileType type; sf::Color color; const char* name; };
constexpr TileInfo TILE_INFOS[] = {
    { TileType::Grass,  sf::Color(34, 139, 34), "Grass"  },
    { TileType::Water,  sf::Color(30,  80, 160), "Water"  },
};
constexpr int NUM_TILE_INFOS = 2;

struct NeutralInfo { EntityType type; const char* name; sf::Color color; };
constexpr NeutralInfo NEUTRAL_INFOS[] = {
    { EntityType::MineralPatch, "Minerals", sf::Color( 80, 160, 255) },
    { EntityType::GasGeyser,    "Geyser",   sf::Color( 60, 200, 120) },
};
constexpr int NUM_NEUTRAL = 2;

struct EntInfo { EntityType type; const char* name; };
constexpr EntInfo BUILDING_INFOS[] = {
    { EntityType::Base,     "Base"     },
    { EntityType::Barracks, "Barracks" },
    { EntityType::Refinery, "Refinery" },
    { EntityType::Factory,  "Factory"  },
    { EntityType::Turret,   "Turret"   },
};
constexpr int NUM_BUILDINGS = 5;

constexpr EntInfo UNIT_INFOS[] = {
    { EntityType::Worker,    "Worker"    },
    { EntityType::Soldier,   "Soldier"   },
    { EntityType::Brute,     "Brute"     },
    { EntityType::LightTank, "LightTank" },
};
constexpr int NUM_UNITS = 4;

const sf::Color COL_PANEL_BG      = sf::Color( 40,  44,  52);
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
// EditorPanel — public interface
// ===========================================================================

bool EditorPanel::isOnPanel(sf::Vector2i pixel) const {
    return pixel.x < static_cast<int>(WIDTH);
}

// ---------------------------------------------------------------------------
// rebuild — reconstruct all widget positions from the supplied state
// ---------------------------------------------------------------------------
void EditorPanel::rebuild(const State& s, sf::Vector2u winSize) {
    m_font    = s.font;
    m_scrollY = s.scrollY;

    const float PW  = WIDTH;
    const float PAD = 10.f;
    const float IW  = PW - PAD * 2.f;
    float y = 0.f;

    // ---- Panel background --------------------------------------------------
    m_panelBg.setPosition({ 0.f, 0.f });
    m_panelBg.setSize({ PW, static_cast<float>(winSize.y) });
    m_panelBg.setFillColor(COL_PANEL_BG);

    // ---- Title bar ---------------------------------------------------------
    if (m_font) {
        m_lblTitle.emplace(*m_font, "MAP EDITOR", 15u);
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

    m_nameBox.setPosition({ PAD, y - m_scrollY });
    m_nameBox.setSize({ IW, 24.f });
    m_nameBox.setFillColor(COL_INPUT_BG);
    m_nameBox.setOutlineColor(s.nameActive ? COL_SELECTED : COL_DIVIDER);
    m_nameBox.setOutlineThickness(1.f);

    if (m_font) {
        m_nameText.emplace(*m_font, s.mapName, 13u);
        m_nameText->setFillColor(COL_TITLE_TXT);
        m_nameText->setPosition({ PAD + 4.f, y + 5.f - m_scrollY });
    }
    y += 30.f;

    if (m_font) {
        std::string sz = "Size:  " + std::to_string(s.mapW) + " x " +
                         std::to_string(s.mapH) + "  tiles";
        m_lblSize.emplace(*m_font, sz, 12u);
        m_lblSize->setFillColor(COL_LABEL);
        m_lblSize->setPosition({ PAD, y - m_scrollY });
    }
    y += 22.f;

    // ---- File/nav buttons --------------------------------------------------
    {
        float bW = (IW - 8.f) / 3.f, bH = 26.f;
        m_btnNew  = makePanelButton("New",  { PAD,                   y }, { bW, bH });
        m_btnLoad = makePanelButton("Load", { PAD + bW + 4.f,        y }, { bW, bH });
        m_btnSave = makePanelButton("Save", { PAD + 2.f*(bW + 4.f),  y }, { bW, bH });
    }
    y += 32.f;
    m_btnBack = makePanelButton("Back to Menu", { PAD, y }, { IW, 26.f }); y += 34.f;

    m_btnErase = makePanelButton(s.eraseMode ? "[Eraser ON]" : "Eraser",
                                  { PAD, y }, { IW, 26.f });
    m_btnErase.selected = s.eraseMode;
    m_btnErase.shape.setFillColor(s.eraseMode ? sf::Color(160, 50, 50) : COL_BTN_NORMAL);
    y += 34.f;

    makeDivider(m_dividerPalette, y); y += 9.f;

    // ---- Tile Palette ------------------------------------------------------
    makeSectionLabel(m_lblPalette, "TILE PALETTE", y); y += 20.f;
    buildTileSwatches(y, s.selectedTile);

    makeDivider(m_dividerNeutral, y); y += 9.f;

    // ---- Neutral entities --------------------------------------------------
    makeSectionLabel(m_lblNeutral, "NEUTRAL", y); y += 20.f;
    buildNeutralItems(y, s.pendingType, s.pendingTeam);

    makeDivider(m_dividerStartPos, y); y += 9.f;

    // ---- Start positions ---------------------------------------------------
    makeSectionLabel(m_lblStartPos, "START POSITIONS", y); y += 20.f;
    buildStartPosItems(y, s.playerCount, s.pendingType, s.pendingTeam);

    makeDivider(m_dividerBuildings, y); y += 9.f;

    // ---- Buildings ---------------------------------------------------------
    makeSectionLabel(m_lblBuildings, "BUILDINGS", y); y += 20.f;
    buildBuildingItems(y, s.playerCount, s.bldTeam, s.pendingType, s.pendingTeam);

    makeDivider(m_dividerUnits, y); y += 9.f;

    // ---- Units -------------------------------------------------------------
    makeSectionLabel(m_lblUnits, "UNITS", y); y += 20.f;
    buildUnitItems(y, s.playerCount, s.unitTeam, s.pendingType, s.pendingTeam);

    y += 10.f;
    m_contentH = y;
}

// ---------------------------------------------------------------------------
// handleScroll
// ---------------------------------------------------------------------------
EditorPanel::PanelEvent EditorPanel::handleScroll(float delta, float windowHeight) {
    float maxScroll = std::max(0.f, m_contentH - windowHeight);
    m_scrollY = std::clamp(m_scrollY - delta * 20.f, 0.f, maxScroll);
    return EvScrollChanged{ m_scrollY };
}

// ---------------------------------------------------------------------------
// handleHover
// ---------------------------------------------------------------------------
void EditorPanel::handleHover(sf::Vector2f pm) {
    updateButtonHover(m_btnNew,   pm);
    updateButtonHover(m_btnLoad,  pm);
    updateButtonHover(m_btnSave,  pm);
    updateButtonHover(m_btnBack,  pm);
    updateButtonHover(m_btnErase, pm);

    for (auto& btn : m_bldTeamButtons)  updateButtonHover(btn, pm);
    for (auto& btn : m_unitTeamButtons) updateButtonHover(btn, pm);

    for (auto& item : m_neutralItems)   updateEntityItemHover(item, pm);
    for (auto& item : m_startPosItems)  updateEntityItemHover(item, pm);
    for (auto& item : m_buildingItems)  updateEntityItemHover(item, pm);
    for (auto& item : m_unitItems)      updateEntityItemHover(item, pm);
}

// ---------------------------------------------------------------------------
// handleClick — left button only
// ---------------------------------------------------------------------------
EditorPanel::PanelEvent EditorPanel::handleClick(sf::Vector2f pm, const State& s) {
    // Back / New / Load / Save
    if (btnHit(m_btnBack, pm)) return EvBack{};
    if (btnHit(m_btnNew,  pm)) return EvNew{};
    if (btnHit(m_btnLoad, pm)) return EvLoad{};
    if (btnHit(m_btnSave, pm)) return EvSave{};

    // Erase toggle
    if (btnHit(m_btnErase, pm)) return EvEraseToggle{};

    // Building team selector
    for (int i = 0; i < static_cast<int>(m_bldTeamButtons.size()); ++i) {
        if (btnHit(m_bldTeamButtons[i], pm))
            return EvBldTeamChanged{ teamFromIndex(i) };
    }

    // Unit team selector
    for (int i = 0; i < static_cast<int>(m_unitTeamButtons.size()); ++i) {
        if (btnHit(m_unitTeamButtons[i], pm))
            return EvUnitTeamChanged{ teamFromIndex(i) };
    }

    // Tile swatches
    for (auto& sw : m_swatches) {
        if (sf::FloatRect(sw.shape.getPosition(), sw.shape.getSize()).contains(pm))
            return EvSelectTile{ sw.type };
    }

    // Neutral items
    for (auto& item : m_neutralItems) {
        if (itemHit(item, pm)) {
            bool alreadySel = (s.pendingType == item.type);
            if (alreadySel) return EvCancelEntity{};
            return EvSelectEntity{ item.type, Team::Neutral };
        }
    }

    // Start position items
    for (size_t i = 0; i < m_startPosItems.size(); ++i) {
        if (itemHit(m_startPosItems[i], pm)) {
            Team team = teamFromIndex(static_cast<int>(i));
            bool alreadySel = (s.pendingType == EntityType::StartPosition &&
                               s.pendingTeam == team);
            if (alreadySel) return EvCancelEntity{};
            return EvSelectEntity{ EntityType::StartPosition, team };
        }
    }

    // Building items
    for (auto& item : m_buildingItems) {
        if (itemHit(item, pm)) {
            bool alreadySel = (s.pendingType == item.type && s.pendingTeam == s.bldTeam);
            if (alreadySel) return EvCancelEntity{};
            return EvSelectEntity{ item.type, s.bldTeam };
        }
    }

    // Unit items
    for (auto& item : m_unitItems) {
        if (itemHit(item, pm)) {
            bool alreadySel = (s.pendingType == item.type && s.pendingTeam == s.unitTeam);
            if (alreadySel) return EvCancelEntity{};
            return EvSelectEntity{ item.type, s.unitTeam };
        }
    }

    // Name box focus
    sf::FloatRect nr(m_nameBox.getPosition(), m_nameBox.getSize());
    if (nr.contains(pm)) return EvNameFocused{};
    if (s.nameActive)    return EvNameUnfocused{};

    return EvNone{};
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
void EditorPanel::render(sf::RenderWindow& window) const {
    sf::Vector2u ws = window.getSize();
    sf::View uiView(sf::FloatRect({ 0.f, 0.f },
        { static_cast<float>(ws.x), static_cast<float>(ws.y) }));
    window.setView(uiView);

    window.draw(m_panelBg);
    window.draw(m_divider1);
    window.draw(m_dividerPalette);
    window.draw(m_dividerNeutral);
    window.draw(m_dividerStartPos);
    window.draw(m_dividerBuildings);
    window.draw(m_dividerUnits);

    if (m_lblTitle)     window.draw(*m_lblTitle);
    if (m_lblProps)     window.draw(*m_lblProps);
    if (m_lblName)      window.draw(*m_lblName);
    if (m_lblSize)      window.draw(*m_lblSize);
    if (m_lblPalette)   window.draw(*m_lblPalette);
    if (m_lblNeutral)   window.draw(*m_lblNeutral);
    if (m_lblStartPos)  window.draw(*m_lblStartPos);
    if (m_lblBuildings) window.draw(*m_lblBuildings);
    if (m_lblUnits)     window.draw(*m_lblUnits);

    window.draw(m_nameBox);
    if (m_nameText) window.draw(*m_nameText);

    // Text cursor in name box
    if (m_nameText && m_nameBox.getOutlineColor() == COL_SELECTED && m_font) {
        float tw = 0.f;
        sf::FloatRect lb = m_nameText->getLocalBounds();
        tw = lb.position.x + lb.size.x;
        sf::Text cur(*m_font, "|", 14u);
        cur.setFillColor(COL_TITLE_TXT);
        cur.setPosition({ m_nameBox.getPosition().x + 4.f + tw,
                           m_nameBox.getPosition().y + 4.f });
        window.draw(cur);
    }

    auto drawBtn = [&](const PanelButton& btn) {
        window.draw(btn.shape);
        if (btn.label) window.draw(*btn.label);
    };
    drawBtn(m_btnNew);
    drawBtn(m_btnLoad);
    drawBtn(m_btnSave);
    drawBtn(m_btnBack);
    drawBtn(m_btnErase);
    for (const auto& btn : m_bldTeamButtons)  drawBtn(btn);
    for (const auto& btn : m_unitTeamButtons) drawBtn(btn);

    for (const auto& sw : m_swatches) {
        window.draw(sw.shape);
        if (sw.label) window.draw(*sw.label);
    }
    for (const auto& item : m_neutralItems) {
        window.draw(item.shape);
        if (item.abbrev) window.draw(*item.abbrev);
        if (item.label)  window.draw(*item.label);
    }
    for (const auto& item : m_startPosItems) {
        window.draw(item.shape);
        if (item.abbrev) window.draw(*item.abbrev);
        if (item.label)  window.draw(*item.label);
    }
    for (const auto& item : m_buildingItems) {
        window.draw(item.shape);
        if (item.abbrev) window.draw(*item.abbrev);
        if (item.label)  window.draw(*item.label);
    }
    for (const auto& item : m_unitItems) {
        window.draw(item.shape);
        if (item.abbrev) window.draw(*item.abbrev);
        if (item.label)  window.draw(*item.label);
    }

    // Scroll indicator
    float wH = static_cast<float>(ws.y);
    if (m_contentH > wH) {
        float ratio = wH / m_contentH;
        float barH  = std::max(20.f, wH * ratio);
        float barY  = (m_scrollY / (m_contentH - wH)) * (wH - barH);
        sf::RectangleShape bar({ 4.f, barH });
        bar.setPosition({ WIDTH - 5.f, barY });
        bar.setFillColor(COL_DIVIDER);
        window.draw(bar);
    }
}

// ===========================================================================
// Private builder helpers
// ===========================================================================

void EditorPanel::makeDivider(sf::RectangleShape& div, float y) {
    div.setPosition({ 0.f, y - m_scrollY });
    div.setSize({ WIDTH, 1.f });
    div.setFillColor(COL_DIVIDER);
}

void EditorPanel::makeSectionLabel(std::optional<sf::Text>& lbl,
                                   const std::string& text, float y) {
    if (!m_font) return;
    lbl.emplace(*m_font, text, 11u);
    lbl->setFillColor(COL_LABEL);
    lbl->setPosition({ 10.f, y - m_scrollY });
}

EditorPanel::PanelButton EditorPanel::makePanelButton(
    const std::string& text, sf::Vector2f pos, sf::Vector2f size,
    unsigned int fontSize)
{
    PanelButton btn;
    sf::Vector2f sp = { pos.x, pos.y - m_scrollY };
    btn.shape.setPosition(sp);
    btn.shape.setSize(size);
    btn.shape.setFillColor(COL_BTN_NORMAL);
    btn.shape.setOutlineColor(COL_BTN_OUTLINE);
    btn.shape.setOutlineThickness(1.f);

    if (m_font) {
        btn.label.emplace(*m_font, text, fontSize);
        btn.label->setFillColor(COL_TITLE_TXT);
        sf::FloatRect lb = btn.label->getLocalBounds();
        btn.label->setOrigin({ lb.position.x + lb.size.x * 0.5f,
                               lb.position.y + lb.size.y * 0.5f });
        btn.label->setPosition({ sp.x + size.x * 0.5f,
                                  sp.y + size.y * 0.5f });
    }
    return btn;
}

EditorPanel::EntityItem EditorPanel::makeEntityItem(
    EntityType type, sf::Vector2f pos, sf::Vector2f size)
{
    EntityItem item;
    item.type = type;
    sf::Vector2f sp = { pos.x, pos.y - m_scrollY };
    item.shape.setPosition(sp);
    item.shape.setSize(size);
    item.shape.setFillColor(COL_BTN_NORMAL);
    item.shape.setOutlineColor(COL_SWATCH_NRM);
    item.shape.setOutlineThickness(2.f);

    if (m_font) {
        std::string abbr = ENTITY_DATA.getShortName(type);
        item.abbrev.emplace(*m_font, abbr, 14u);
        item.abbrev->setFillColor(COL_TITLE_TXT);
        item.abbrev->setStyle(sf::Text::Bold);
        sf::FloatRect ab = item.abbrev->getLocalBounds();
        item.abbrev->setOrigin({ ab.position.x + ab.size.x * 0.5f,
                                  ab.position.y + ab.size.y * 0.5f });
        item.abbrev->setPosition({ sp.x + size.x * 0.5f,
                                    sp.y + size.y * 0.5f - 6.f });

        std::string name = ENTITY_DATA.getName(type);
        if (name.size() > 8) name = name.substr(0, 8);
        item.label.emplace(*m_font, name, 9u);
        item.label->setFillColor(COL_LABEL);
        sf::FloatRect lb = item.label->getLocalBounds();
        item.label->setOrigin({ lb.position.x + lb.size.x * 0.5f, 0.f });
        item.label->setPosition({ sp.x + size.x * 0.5f,
                                   sp.y + size.y - 14.f });
    }
    return item;
}

void EditorPanel::buildTileSwatches(float& y, TileType selectedTile) {
    m_swatches.clear();
    const float PAD = 10.f, SW = 56.f, SH = 68.f, GAP = 8.f;
    for (int i = 0; i < NUM_TILE_INFOS; ++i) {
        const auto& info = TILE_INFOS[i];
        int col = i % 4, row = i / 4;
        float x  = PAD + col * (SW + GAP);
        float sy = y + row * (SH + GAP);

        TileSwatch sw;
        sw.type = info.type;
        sw.baseColor = info.color;
        sw.name = info.name;
        sw.shape.setPosition({ x, sy - m_scrollY });
        sw.shape.setSize({ SW, SH });
        sw.shape.setFillColor(info.color);
        sw.shape.setOutlineThickness(2.f);
        sw.shape.setOutlineColor(info.type == selectedTile ? COL_SWATCH_SEL : COL_SWATCH_NRM);

        if (m_font) {
            sw.label.emplace(*m_font, info.name, 9u);
            sw.label->setFillColor(sf::Color::White);
            sf::FloatRect lb = sw.label->getLocalBounds();
            sw.label->setOrigin({ lb.position.x + lb.size.x * 0.5f, 0.f });
            sw.label->setPosition({ x + SW * 0.5f, sy + SH - 15.f - m_scrollY });
        }
        m_swatches.push_back(std::move(sw));
    }
    int rows = (NUM_TILE_INFOS + 3) / 4;
    y += rows * (SH + GAP) + 4.f;
}

void EditorPanel::buildNeutralItems(float& y, EntityType pendingType, Team pendingTeam) {
    m_neutralItems.clear();
    const float PAD = 10.f;
    const float SW = 56.f, SH = 60.f, GAP = 8.f;
    for (int i = 0; i < NUM_NEUTRAL; ++i) {
        const auto& info = NEUTRAL_INFOS[i];
        int col = i % 4, row = i / 4;
        float x  = PAD + col * (SW + GAP);
        float sy = y + row * (SH + GAP);

        EntityItem item = makeEntityItem(info.type, { x, sy }, { SW, SH });
        item.shape.setFillColor(info.color * sf::Color(100, 100, 100, 255));
        item.shape.setOutlineColor(
            (pendingType == info.type && pendingTeam == Team::Neutral)
            ? COL_ITEM_SEL_OUT : COL_SWATCH_NRM);
        m_neutralItems.push_back(std::move(item));
    }
    int rows = (NUM_NEUTRAL + 3) / 4;
    y += rows * (SH + GAP) + 4.f;
}

void EditorPanel::buildStartPosItems(float& y, int playerCount,
                                     EntityType pendingType, Team pendingTeam) {
    m_startPosItems.clear();
    const float PAD = 10.f;
    const float SW = 56.f, SH = 60.f, GAP = 8.f;

    for (int i = 0; i < playerCount; ++i) {
        Team team = teamFromIndex(i);
        int col = i % 4, row = i / 4;
        float x  = PAD + col * (SW + GAP);
        float sy = y + row * (SH + GAP);

        EntityItem item = makeEntityItem(EntityType::StartPosition, { x, sy }, { SW, SH });

        sf::Color tc = teamColor(team);
        item.shape.setFillColor(sf::Color(tc.r, tc.g, tc.b, 130));
        bool isSelected = (pendingType == EntityType::StartPosition && pendingTeam == team);
        item.shape.setOutlineColor(isSelected ? COL_ITEM_SEL_OUT : COL_SWATCH_NRM);

        if (m_font) {
            std::string lbl = "Plr " + std::to_string(i + 1);
            item.label.emplace(*m_font, lbl, 9u);
            item.label->setFillColor(sf::Color(200, 210, 220));
            sf::FloatRect lb = item.label->getLocalBounds();
            item.label->setOrigin({ lb.position.x + lb.size.x * 0.5f, 0.f });
            item.label->setPosition({ x + SW * 0.5f, sy + SH - 14.f - m_scrollY });
        }

        m_startPosItems.push_back(std::move(item));
    }

    int rows = (playerCount + 3) / 4;
    y += rows * (SH + GAP) + 4.f;
}

void EditorPanel::buildBuildingItems(float& y, int playerCount,
                                      Team bldTeam, EntityType pendingType,
                                      Team pendingTeam)
{
    m_buildingItems.clear();
    const float PAD = 10.f, IW = WIDTH - PAD * 2.f;
    const float BH = 24.f, SW = 56.f, SH = 60.f, GAP = 8.f;

    m_bldTeamButtons.clear();
    {
        const float BGAP = 4.f;
        float bW = (IW - BGAP * (playerCount - 1)) / playerCount;
        unsigned int fs = (playerCount <= 3) ? 13u : 11u;
        for (int i = 0; i < playerCount; ++i) {
            Team t = teamFromIndex(i);
            float bx = PAD + i * (bW + BGAP);
            std::string lbl = "Player " + std::to_string(i + 1);
            PanelButton btn = makePanelButton(lbl, { bx, y }, { bW, BH }, fs);
            btn.selected = (bldTeam == t);
            btn.shape.setFillColor(bldTeam == t ? COL_BTN_SEL : COL_BTN_NORMAL);
            m_bldTeamButtons.push_back(std::move(btn));
        }
    }
    y += BH + 6.f;

    for (int i = 0; i < NUM_BUILDINGS; ++i) {
        const auto& info = BUILDING_INFOS[i];
        int col = i % 4, row = i / 4;
        float x  = PAD + col * (SW + GAP);
        float sy = y + row * (SH + GAP);

        EntityItem item = makeEntityItem(info.type, { x, sy }, { SW, SH });
        item.shape.setOutlineColor(
            (pendingType == info.type && pendingTeam == bldTeam)
            ? COL_ITEM_SEL_OUT : COL_SWATCH_NRM);
        m_buildingItems.push_back(std::move(item));
    }
    int rows = (NUM_BUILDINGS + 3) / 4;
    y += rows * (SH + GAP) + 4.f;
    (void)IW;
}

void EditorPanel::buildUnitItems(float& y, int playerCount,
                                  Team unitTeam, EntityType pendingType,
                                  Team pendingTeam)
{
    m_unitItems.clear();
    const float PAD = 10.f, IW = WIDTH - PAD * 2.f;
    const float BH = 24.f, SW = 56.f, SH = 60.f, GAP = 8.f;

    m_unitTeamButtons.clear();
    {
        const float BGAP = 4.f;
        float bW = (IW - BGAP * (playerCount - 1)) / playerCount;
        unsigned int fs = (playerCount <= 3) ? 13u : 11u;
        for (int i = 0; i < playerCount; ++i) {
            Team t = teamFromIndex(i);
            float bx = PAD + i * (bW + BGAP);
            std::string lbl = "Player " + std::to_string(i + 1);
            PanelButton btn = makePanelButton(lbl, { bx, y }, { bW, BH }, fs);
            btn.selected = (unitTeam == t);
            btn.shape.setFillColor(unitTeam == t ? COL_BTN_SEL : COL_BTN_NORMAL);
            m_unitTeamButtons.push_back(std::move(btn));
        }
    }
    y += BH + 6.f;

    for (int i = 0; i < NUM_UNITS; ++i) {
        const auto& info = UNIT_INFOS[i];
        int col = i % 4, row = i / 4;
        float x  = PAD + col * (SW + GAP);
        float sy = y + row * (SH + GAP);

        EntityItem item = makeEntityItem(info.type, { x, sy }, { SW, SH });
        item.shape.setOutlineColor(
            (pendingType == info.type && pendingTeam == unitTeam)
            ? COL_ITEM_SEL_OUT : COL_SWATCH_NRM);
        m_unitItems.push_back(std::move(item));
    }
    int rows = (NUM_UNITS + 3) / 4;
    y += rows * (SH + GAP) + 4.f;
    (void)IW;
}

// ===========================================================================
// Hit-test helpers
// ===========================================================================

bool EditorPanel::btnHit(const PanelButton& btn, sf::Vector2f pm) {
    return sf::FloatRect(btn.shape.getPosition(), btn.shape.getSize()).contains(pm);
}

bool EditorPanel::itemHit(const EntityItem& item, sf::Vector2f pm) {
    return sf::FloatRect(item.shape.getPosition(), item.shape.getSize()).contains(pm);
}

void EditorPanel::updateButtonHover(PanelButton& btn, sf::Vector2f pm) {
    btn.hovered = btnHit(btn, pm);
    if (!btn.selected)
        btn.shape.setFillColor(btn.hovered ? COL_BTN_HOVER : COL_BTN_NORMAL);
}

void EditorPanel::updateEntityItemHover(EntityItem& item, sf::Vector2f pm) {
    item.hovered = itemHit(item, pm);
}
