#include "MapEditorScreen.h"
#include "FontManager.h"
#include "Constants.h"
#include "ResourceManager.h"
#include "MapSerializer.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <set>

// ---------------------------------------------------------------------------
// Local colour used only by the map-area background
// ---------------------------------------------------------------------------
namespace {
const sf::Color COL_MAP_BG      = sf::Color( 15,  17,  22);
const sf::Color COL_BTN_NORMAL  = sf::Color( 60,  65,  75);
const sf::Color COL_BTN_HOVER   = sf::Color( 80,  90, 110);
const sf::Color COL_BTN_SEL     = sf::Color( 55,  90, 160);
const sf::Color COL_BTN_OUTLINE = sf::Color( 90,  95, 105);
const sf::Color COL_TITLE_TXT   = sf::Color(220, 225, 235);
const sf::Color COL_LABEL       = sf::Color(150, 155, 165);
const sf::Color COL_DIVIDER     = sf::Color( 70,  75,  85);
} // namespace

// ===========================================================================
// Constructor
// ===========================================================================
MapEditorScreen::MapEditorScreen()
    : m_map(Constants::MAP_WIDTH, Constants::MAP_HEIGHT)
    , m_gridLines(sf::PrimitiveType::Lines)
{
    m_font = FontManager::instance().defaultFont();

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
// Panel state / rebuild
// ===========================================================================
EditorPanel::State MapEditorScreen::makePanelState() const {
    EditorPanel::State s;
    s.mapW         = m_mapW;
    s.mapH         = m_mapH;
    s.playerCount  = m_mapPlayerCount;
    s.mapName      = m_mapName;
    s.nameActive   = m_nameActive;
    s.eraseMode    = m_eraseMode;
    s.pendingType  = m_pendingEntityType;
    s.pendingTeam  = m_pendingTeam;
    s.bldTeam      = m_bldTeam;
    s.unitTeam     = m_unitTeam;
    s.selectedTile = m_selectedTile;
    s.scrollY      = m_panelScrollY;
    s.font         = m_font;
    return s;
}

void MapEditorScreen::rebuildPanel() {
    m_panel.rebuild(makePanelState(), m_lastWinSize);
}

// ===========================================================================
// Layout
// ===========================================================================
void MapEditorScreen::buildLayout(sf::Vector2u winSize) {
    const bool firstTime = (m_lastWinSize.x == 0 && m_lastWinSize.y == 0);

    m_panel.rebuild(makePanelState(), winSize);

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
// UI hit-testing helpers (dialog buttons only)
// ===========================================================================
bool MapEditorScreen::btnHit(const PanelButton& btn, sf::Vector2f pm) {
    return sf::FloatRect(btn.shape.getPosition(), btn.shape.getSize()).contains(pm);
}

void MapEditorScreen::updateButtonHover(PanelButton& btn, sf::Vector2f pm) {
    btn.hovered = btnHit(btn, pm);
    if (!btn.selected)
        btn.shape.setFillColor(btn.hovered ? COL_BTN_HOVER : COL_BTN_NORMAL);
}

MapEditorScreen::PanelButton MapEditorScreen::makePanelButton(
    const std::string& text, sf::Vector2f pos, sf::Vector2f size,
    unsigned int fontSize)
{
    PanelButton btn;
    btn.shape.setPosition(pos);
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
        btn.label->setPosition({ pos.x + size.x * 0.5f,
                                  pos.y + size.y * 0.5f });
    }
    return btn;
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
    m_eraseMode         = false;
    m_pendingEntityType = type;
    m_pendingTeam       = team;

    sf::Vector2i tileSize = ENTITY_DATA.getBuildingTileSize(type);
    float pw = static_cast<float>(tileSize.x * Constants::TILE_SIZE);
    float ph = static_cast<float>(tileSize.y * Constants::TILE_SIZE);
    m_placementPreview.setSize({ pw, ph });

    sf::Color tc = teamColor(team);
    m_placementPreview.setFillColor(sf::Color(tc.r, tc.g, tc.b, 70));
    m_placementPreview.setOutlineColor(sf::Color(tc.r, tc.g, tc.b, 200));
    m_placementPreview.setOutlineThickness(2.f);
}

void MapEditorScreen::clearPendingEntity() {
    m_pendingEntityType = EntityType::None;
}

void MapEditorScreen::tryEraseAt(sf::Vector2i pixel) {
    sf::Vector2i t = screenToTile(pixel);
    if (t.x < 0) return;

    auto it = std::find_if(m_placedEntities.begin(), m_placedEntities.end(),
        [&](const PlacedEntity& pe) {
            sf::Vector2i ps = ENTITY_DATA.getBuildingTileSize(pe.type);
            bool inX = (t.x >= pe.tileX && t.x < pe.tileX + ps.x);
            bool inY = (t.y >= pe.tileY && t.y < pe.tileY + ps.y);
            return inX && inY;
        });

    if (it == m_placedEntities.end()) return;

    const EntityDef* def = ENTITY_DATA.get(it->type);
    if (def && def->isBuilding() && it->type != EntityType::StartPosition) {
        sf::Vector2i ps = ENTITY_DATA.getBuildingTileSize(it->type);
        for (int dy = 0; dy < ps.y; ++dy)
            for (int dx = 0; dx < ps.x; ++dx)
                m_map.setTileType(it->tileX + dx, it->tileY + dy, TileType::Grass);
    }

    m_placedEntities.erase(it);
}

void MapEditorScreen::tryPlaceEntity(sf::Vector2i pixel) {
    if (m_pendingEntityType == EntityType::None) return;
    sf::Vector2i t = screenToTile(pixel);
    if (t.x < 0) return;

    sf::Vector2i tileSize = ENTITY_DATA.getBuildingTileSize(m_pendingEntityType);
    if (t.x + tileSize.x > m_mapW || t.y + tileSize.y > m_mapH) return;

    if (m_pendingEntityType == EntityType::StartPosition) {
        m_placedEntities.erase(
            std::remove_if(m_placedEntities.begin(), m_placedEntities.end(),
                [&](const PlacedEntity& pe) {
                    return pe.type == EntityType::StartPosition && pe.team == m_pendingTeam;
                }),
            m_placedEntities.end());
    } else {
        m_placedEntities.erase(
            std::remove_if(m_placedEntities.begin(), m_placedEntities.end(),
                [&](const PlacedEntity& pe) {
                    if (pe.type == EntityType::StartPosition) return false;
                    sf::Vector2i ps = ENTITY_DATA.getBuildingTileSize(pe.type);
                    bool overlapX = !(t.x >= pe.tileX + ps.x || t.x + tileSize.x <= pe.tileX);
                    bool overlapY = !(t.y >= pe.tileY + ps.y || t.y + tileSize.y <= pe.tileY);
                    return overlapX && overlapY;
                }),
            m_placedEntities.end());
    }

    m_placedEntities.push_back({ m_pendingEntityType, m_pendingTeam, t.x, t.y });

    const EntityDef* def = ENTITY_DATA.get(m_pendingEntityType);
    if (def && def->isBuilding() && m_pendingEntityType != EntityType::StartPosition) {
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

    auto panelMouse = [&](sf::Vector2i px) -> sf::Vector2f {
        return { static_cast<float>(px.x), static_cast<float>(px.y) };
    };

    // ---- Resize ------------------------------------------------------------
    if (const auto* r = event.getIf<sf::Event::Resized>()) {
        sf::Vector2u ns = r->size;
        m_panelScrollY = std::min(m_panelScrollY,
            std::max(0.f, m_panel.contentHeight() - static_cast<float>(ns.y)));
        buildLayout(ns);
        m_lastWinSize = ns;
        return {};
    }

    // ---- Mouse wheel (scroll panel OR zoom map) ----------------------------
    if (const auto* mw = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (!inMapArea(mw->position)) {
            auto ev = m_panel.handleScroll(mw->delta, static_cast<float>(m_lastWinSize.y));
            if (auto* sc = std::get_if<EditorPanel::EvScrollChanged>(&ev)) {
                m_panelScrollY = sc->newScrollY;
                rebuildPanel();
            }
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

        m_panel.handleHover(pm);

        if (m_showNewMapDialog) {
            for (auto& btn : m_newMapSizeButtons) updateButtonHover(btn, pm);
            for (auto& btn : m_newMapPlayerBtns)  updateButtonHover(btn, pm);
            updateButtonHover(m_btnNewMapConfirm, pm);
            updateButtonHover(m_btnNewMapCancel,  pm);
        }

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

            // ---- New-map dialog (intercepts all clicks while open) ---------
            if (m_showNewMapDialog) {
                if (btnHit(m_btnNewMapCancel, pm)) {
                    m_showNewMapDialog = false;
                    return {};
                }
                if (btnHit(m_btnNewMapConfirm, pm)) {
                    confirmNewMap();
                    return {};
                }
                static const int SIZES[] = { 32, 48, 64, 96, 128 };
                for (size_t i = 0; i < m_newMapSizeButtons.size(); ++i) {
                    if (btnHit(m_newMapSizeButtons[i], pm)) {
                        m_newMapW = SIZES[i]; m_newMapH = SIZES[i];
                        buildNewMapDialog(m_lastWinSize);
                        return {};
                    }
                }
                for (size_t i = 0; i < m_newMapPlayerBtns.size(); ++i) {
                    if (btnHit(m_newMapPlayerBtns[i], pm)) {
                        m_newMapPlayers = static_cast<int>(i) + 2;
                        buildNewMapDialog(m_lastWinSize);
                        return {};
                    }
                }
                sf::FloatRect ovr(m_newMapOverlayBg.getPosition(), m_newMapOverlayBg.getSize());
                if (!ovr.contains(pm)) m_showNewMapDialog = false;
                return {};
            }

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
                        if (m_font) {
                            m_statusText.emplace(*m_font,
                                ok ? "Loaded: " + m_loadMapFiles[i] : "Load failed!",
                                12u);
                            m_statusText->setFillColor(ok ? sf::Color(80,220,80) : sf::Color(220,80,80));
                        }
                        m_statusTimer = 4.f;
                        return {};
                    }
                }
                sf::FloatRect ovr(m_loadOverlayBg.getPosition(), m_loadOverlayBg.getSize());
                if (!ovr.contains(pm)) m_showLoadPanel = false;
                return {};
            }

            // ---- Delegate to EditorPanel ----------------------------------
            if (m_panel.isOnPanel(mb->position)) {
                auto ev = m_panel.handleClick(pm, makePanelState());
                std::visit([&](auto&& e) {
                    using T = std::decay_t<decltype(e)>;
                    if constexpr (std::is_same_v<T, EditorPanel::EvBack>) {
                        // handled below via return value — set a flag instead
                        // (cannot return from lambda; handled outside visit)
                    } else if constexpr (std::is_same_v<T, EditorPanel::EvNew>) {
                        buildNewMapDialog(m_lastWinSize);
                        m_showNewMapDialog = true;
                    } else if constexpr (std::is_same_v<T, EditorPanel::EvLoad>) {
                        refreshLoadPanel(m_lastWinSize);
                        m_showLoadPanel = true;
                    } else if constexpr (std::is_same_v<T, EditorPanel::EvSave>) {
                        bool ok = saveCurrentMap();
                        if (m_font) {
                            m_statusText.emplace(*m_font,
                                ok ? "Saved to maps/" + m_mapName + ".stmap" : "Save failed!",
                                12u);
                            m_statusText->setFillColor(ok ? sf::Color(80,220,80) : sf::Color(220,80,80));
                        }
                        m_statusTimer = 4.f;
                    } else if constexpr (std::is_same_v<T, EditorPanel::EvEraseToggle>) {
                        m_eraseMode = !m_eraseMode;
                        if (m_eraseMode) clearPendingEntity();
                        rebuildPanel();
                    } else if constexpr (std::is_same_v<T, EditorPanel::EvSelectTile>) {
                        m_selectedTile = e.type;
                        m_eraseMode    = false;
                        clearPendingEntity();
                        rebuildPanel();
                    } else if constexpr (std::is_same_v<T, EditorPanel::EvSelectEntity>) {
                        selectPendingEntity(e.type, e.team);
                        rebuildPanel();
                    } else if constexpr (std::is_same_v<T, EditorPanel::EvCancelEntity>) {
                        clearPendingEntity();
                        rebuildPanel();
                    } else if constexpr (std::is_same_v<T, EditorPanel::EvBldTeamChanged>) {
                        m_bldTeam = e.team;
                        // update pending entity team if a building was selected
                        if (m_pendingEntityType != EntityType::None) {
                            const EntityDef* def = ENTITY_DATA.get(m_pendingEntityType);
                            if (def && def->isBuilding() && m_pendingEntityType != EntityType::StartPosition)
                                selectPendingEntity(m_pendingEntityType, m_bldTeam);
                        }
                        rebuildPanel();
                    } else if constexpr (std::is_same_v<T, EditorPanel::EvUnitTeamChanged>) {
                        m_unitTeam = e.team;
                        if (m_pendingEntityType != EntityType::None) {
                            const EntityDef* def = ENTITY_DATA.get(m_pendingEntityType);
                            if (def && !def->isBuilding())
                                selectPendingEntity(m_pendingEntityType, m_unitTeam);
                        }
                        rebuildPanel();
                    } else if constexpr (std::is_same_v<T, EditorPanel::EvNameFocused>) {
                        m_nameActive = true;
                        rebuildPanel();
                    } else if constexpr (std::is_same_v<T, EditorPanel::EvNameUnfocused>) {
                        m_nameActive = false;
                        rebuildPanel();
                    }
                }, ev);

                // Handle EvBack after visit (needs to return from handleEvent)
                if (std::holds_alternative<EditorPanel::EvBack>(ev))
                    return { ScreenResult::Action::BackToMenu, "" };

                return {};
            }

            // ---- Map interaction -------------------------------------------
            if (inMapArea(mb->position)) {
                if (m_eraseMode) {
                    tryEraseAt(mb->position);
                    m_hoveredTile = mb->position;
                    m_isPainting  = true;
                } else if (m_pendingEntityType != EntityType::None) {
                    tryPlaceEntity(mb->position);
                    m_hoveredTile = mb->position;
                    m_isPainting  = false;
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
                m_eraseMode = false;
                clearPendingEntity();
                rebuildPanel();
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
            if (m_showNewMapDialog) {
                m_showNewMapDialog = false;
            } else if (m_showLoadPanel) {
                m_showLoadPanel = false;
            } else if (m_pendingEntityType != EntityType::None) {
                clearPendingEntity();
                rebuildPanel();
            } else if (m_eraseMode) {
                m_eraseMode = false;
                rebuildPanel();
            } else if (m_nameActive) {
                m_nameActive = false;
                rebuildPanel();
            }
        }
        if (m_nameActive) {
            if (kp->code == sf::Keyboard::Key::Backspace && !m_mapName.empty()) {
                m_mapName.pop_back();
                rebuildPanel();
            }
            if (kp->code == sf::Keyboard::Key::Enter) {
                m_nameActive = false;
                rebuildPanel();
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
            rebuildPanel();
        }
    }

    return {};
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
// Save / Load helpers
// ===========================================================================
static constexpr const char* MAPS_DIR = "maps";

bool MapEditorScreen::saveCurrentMap() {
    namespace fs = std::filesystem;
    MapData data;
    data.name   = m_mapName;
    data.width  = m_mapW;
    data.height = m_mapH;
    data.playerCount = m_mapPlayerCount;

    for (int y = 0; y < m_mapH; ++y) {
        for (int x = 0; x < m_mapW; ++x) {
            const Tile& tile = m_map.getTile(x, y);
            if (tile.type != TileType::Grass || tile.variant != 1)
                data.tiles.push_back({ x, y, tile.type, tile.variant });
        }
    }

    for (const auto& pe : m_placedEntities)
        data.entities.push_back({ pe.type, pe.team, pe.tileX, pe.tileY });

    std::string stem = m_mapName;
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
    m_mapName       = data.name;
    m_mapW          = data.width;
    m_mapH          = data.height;
    m_mapPlayerCount = std::max(2, data.playerCount);
    if (teamToIndex(m_bldTeam)  >= m_mapPlayerCount) m_bldTeam  = Team::Player1;
    if (teamToIndex(m_unitTeam) >= m_mapPlayerCount) m_unitTeam = Team::Player1;

    m_map = Map(m_mapW, m_mapH);
    m_placedEntities.clear();

    for (const auto& t : data.tiles)
        m_map.setTileType(t.x, t.y, t.type, t.variant);

    for (const auto& e : data.entities)
        m_placedEntities.push_back({ e.type, e.team, e.tileX, e.tileY });

    m_gridDirty = true;
    buildLayout(m_lastWinSize);
}

// ===========================================================================
// New-map dialog
// ===========================================================================
void MapEditorScreen::buildNewMapDialog(sf::Vector2u winSize) {
    const float wf  = static_cast<float>(winSize.x);
    const float hf  = static_cast<float>(winSize.y);
    const float ow  = 360.f;
    const float oh  = 290.f;
    const float ox  = (wf - ow) * 0.5f;
    const float oy  = (hf - oh) * 0.5f;
    const float pad = 16.f;
    const float bH  = 30.f;
    const float gap = 8.f;

    m_newMapOverlayBg.setPosition({ ox, oy });
    m_newMapOverlayBg.setSize({ ow, oh });
    m_newMapOverlayBg.setFillColor(sf::Color(30, 33, 42, 245));
    m_newMapOverlayBg.setOutlineColor(sf::Color(80, 90, 130));
    m_newMapOverlayBg.setOutlineThickness(2.f);

    if (m_font) {
        m_lblNewMapTitle.emplace(*m_font, "New Map", 18u);
        m_lblNewMapTitle->setFillColor(sf::Color(220, 225, 235));
        m_lblNewMapTitle->setStyle(sf::Text::Bold);
        sf::FloatRect lb = m_lblNewMapTitle->getLocalBounds();
        m_lblNewMapTitle->setOrigin({ lb.position.x + lb.size.x * 0.5f,
                                      lb.position.y + lb.size.y * 0.5f });
        m_lblNewMapTitle->setPosition({ ox + ow * 0.5f, oy + pad + 9.f });

        m_lblNewMapSizeHdr.emplace(*m_font, "Map Size", 12u);
        m_lblNewMapSizeHdr->setFillColor(COL_LABEL);
        m_lblNewMapSizeHdr->setPosition({ ox + pad, oy + pad + 36.f });

        m_lblNewMapPlayersHdr.emplace(*m_font, "Number of Players", 12u);
        m_lblNewMapPlayersHdr->setFillColor(COL_LABEL);
        m_lblNewMapPlayersHdr->setPosition({ ox + pad, oy + pad + 36.f + bH + gap + 20.f });
    }

    static const int SIZES[] = { 32, 48, 64, 96, 128 };
    constexpr int NUM_SIZES  = 5;
    m_newMapSizeButtons.clear();
    {
        float innerW = ow - pad * 2.f;
        float bw     = (innerW - gap * (NUM_SIZES - 1)) / NUM_SIZES;
        float by     = oy + pad + 54.f;
        for (int i = 0; i < NUM_SIZES; ++i) {
            float bx = ox + pad + i * (bw + gap);
            PanelButton btn = makePanelButton(std::to_string(SIZES[i]),
                                              { bx, by }, { bw, bH }, 12u);
            bool sel = (m_newMapW == SIZES[i] && m_newMapH == SIZES[i]);
            btn.selected = sel;
            btn.shape.setFillColor(sel ? COL_BTN_SEL : COL_BTN_NORMAL);
            m_newMapSizeButtons.push_back(std::move(btn));
        }
    }

    m_newMapPlayerBtns.clear();
    {
        float innerW = ow - pad * 2.f;
        float bw     = (innerW - gap * 2.f) / 3.f;
        float by     = oy + pad + 36.f + bH + gap + 20.f + 18.f;
        for (int i = 2; i <= 4; ++i) {
            float bx = ox + pad + (i - 2) * (bw + gap);
            PanelButton btn = makePanelButton(std::to_string(i) + " Players",
                                              { bx, by }, { bw, bH }, 12u);
            bool sel = (m_newMapPlayers == i);
            btn.selected = sel;
            btn.shape.setFillColor(sel ? COL_BTN_SEL : COL_BTN_NORMAL);
            m_newMapPlayerBtns.push_back(std::move(btn));
        }
    }

    {
        float bw = (ow - pad * 2.f - gap) * 0.5f;
        float by = oy + oh - bH - pad;
        m_btnNewMapConfirm = makePanelButton("Create",
            { ox + pad, by }, { bw, bH });
        m_btnNewMapConfirm.shape.setFillColor(sf::Color(45, 110, 55));
        m_btnNewMapCancel  = makePanelButton("Cancel",
            { ox + pad + bw + gap, by }, { bw, bH });
    }
}

void MapEditorScreen::renderNewMapDialog(sf::RenderWindow& window) {
    if (!m_showNewMapDialog) return;

    sf::Vector2u ws = window.getSize();
    sf::View uiView(sf::FloatRect({ 0.f, 0.f },
        { static_cast<float>(ws.x), static_cast<float>(ws.y) }));
    window.setView(uiView);

    sf::RectangleShape dim({ static_cast<float>(ws.x), static_cast<float>(ws.y) });
    dim.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(dim);

    window.draw(m_newMapOverlayBg);
    if (m_lblNewMapTitle)      window.draw(*m_lblNewMapTitle);
    if (m_lblNewMapSizeHdr)    window.draw(*m_lblNewMapSizeHdr);
    if (m_lblNewMapPlayersHdr) window.draw(*m_lblNewMapPlayersHdr);

    auto drawBtn = [&](PanelButton& btn) {
        window.draw(btn.shape);
        if (btn.label) window.draw(*btn.label);
    };
    for (auto& btn : m_newMapSizeButtons) drawBtn(btn);
    for (auto& btn : m_newMapPlayerBtns)  drawBtn(btn);
    drawBtn(m_btnNewMapConfirm);
    drawBtn(m_btnNewMapCancel);
}

void MapEditorScreen::confirmNewMap() {
    m_mapW           = m_newMapW;
    m_mapH           = m_newMapH;
    m_mapPlayerCount = m_newMapPlayers;
    m_mapName        = "untitled";
    m_bldTeam        = Team::Player1;
    m_unitTeam       = Team::Player1;
    m_map            = Map(m_mapW, m_mapH);
    m_placedEntities.clear();
    m_gridDirty      = true;
    buildLayout(m_lastWinSize);
    resetCamera(m_lastWinSize);
    m_showNewMapDialog = false;
}

void MapEditorScreen::refreshLoadPanel(sf::Vector2u winSize) {
    m_loadMapFiles = MapSerializer::listMaps(MAPS_DIR);
    m_loadButtons.clear();

    float wf = static_cast<float>(winSize.x);
    float hf = static_cast<float>(winSize.y);
    float ow = std::min(400.f, wf - 40.f);
    float bH = 34.f, gap = 8.f, pad = 16.f;
    float totalH = pad + 32.f + pad
                 + static_cast<float>(m_loadMapFiles.size()) * (bH + gap)
                 + bH + pad;
    float oh = std::min(totalH, hf - 40.f);
    float ox = (wf - ow) * 0.5f;
    float oy = (hf - oh) * 0.5f;

    m_loadOverlayBg.setPosition({ ox, oy });
    m_loadOverlayBg.setSize({ ow, oh });
    m_loadOverlayBg.setFillColor(sf::Color(30, 33, 42, 245));
    m_loadOverlayBg.setOutlineColor(sf::Color(80, 90, 130));
    m_loadOverlayBg.setOutlineThickness(2.f);

    if (m_font) {
        m_lblLoadTitle.emplace(*m_font, "Load Map", 18u);
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

    sf::RectangleShape dim({ static_cast<float>(ws.x), static_cast<float>(ws.y) });
    dim.setFillColor(sf::Color(0, 0, 0, 160));
    window.draw(dim);

    window.draw(m_loadOverlayBg);
    if (m_lblLoadTitle) window.draw(*m_lblLoadTitle);

    if (m_loadMapFiles.empty() && m_font) {
        sf::Text empty(*m_font, "No maps found in maps/", 13u);
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

        sf::Color tc = teamColor(pe.team);

        sf::RectangleShape box({ pw, ph });
        box.setPosition({ wx, wy });
        box.setFillColor(sf::Color(tc.r, tc.g, tc.b, 90));
        box.setOutlineColor(tc);
        box.setOutlineThickness(2.f);
        window.draw(box);

        if (m_font) {
            sf::Text abbr(*m_font, ENTITY_DATA.getShortName(pe.type), 14u);
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
    m_panel.render(window);
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
                m_placementPreview.setPosition({ wx, wy });
                window.draw(m_placementPreview);
            } else if (m_eraseMode) {
                m_hoverRect.setFillColor(sf::Color(255, 60, 60, 80));
                m_hoverRect.setOutlineColor(sf::Color(255, 60, 60, 220));
                m_hoverRect.setPosition({ wx, wy });
                window.draw(m_hoverRect);
                m_hoverRect.setFillColor(sf::Color(255, 255, 120, 55));
                m_hoverRect.setOutlineColor(sf::Color(255, 255, 0, 200));

                if (m_isPainting)
                    tryEraseAt(m_hoveredTile);
            } else {
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

    // ---- New-map dialog (modal) -------------------------------------------
    renderNewMapDialog(window);

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
