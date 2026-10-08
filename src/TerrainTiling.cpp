#include "TerrainTiling.h"
#include "Map.h"
#include <cstdint>
#include <string>

namespace TerrainTiling {

namespace {

// Image ids: contiguous groups, in this order.
constexpr int kGrassFirst = 0;    // 8 grass variants
constexpr int kWaterFirst = 8;    // 4 water variants
constexpr int kEdgeFirst  = 12;   // 14 cardinal edge / corner tiles (see kEdges)
constexpr int kInnerFirst = 26;   // 4 inner corners: SW, SE, NW, NE diagonal water
constexpr int kStripFirst = 30;   // 4 strips: N, S, E, W
constexpr int kCount      = 34;

const char* const kFiles[kCount] = {
    "grass_1.png", "grass_2.png", "grass_3.png", "grass_4.png",
    "grass_5.png", "grass_6.png", "grass_7.png", "grass_8.png",
    "water_1.png", "water_2.png", "water_3.png", "water_4.png",
    // Single edges
    "grass_water_N.png", "grass_water_E.png", "grass_water_S.png", "grass_water_W.png",
    // Outer corners (2 adjacent cardinal sides)
    "grass_water_NE.png", "grass_water_NW.png", "grass_water_SE.png", "grass_water_SW.png",
    // Opposite pairs (thin grass strip)
    "grass_water_NS.png", "grass_water_EW.png",
    // Three-sided (grass strip on one edge only)
    "grass_water_NES.png", "grass_water_SEW.png", "grass_water_NSW.png", "grass_water_NEW.png",
    // Inner corners: grass with a small water notch at one corner
    "water_grass_NE.png", "water_grass_NW.png", "water_grass_SE.png", "water_grass_SW.png",
    // Strips: two same-side diagonal water neighbours
    "grass_water_N_strip.png", "grass_water_S_strip.png", "grass_water_E_strip.png", "grass_water_W_strip.png",
};

// Edge tile for each cardinal water mask (bit0=N, bit1=E, bit2=S, bit3=W); -1 = no art.
int edgeForMask(uint8_t mask) {
    switch (mask) {
        case 0b0001: return kEdgeFirst + 0;
        case 0b0010: return kEdgeFirst + 1;
        case 0b0100: return kEdgeFirst + 2;
        case 0b1000: return kEdgeFirst + 3;
        case 0b0011: return kEdgeFirst + 4;    // NE
        case 0b1001: return kEdgeFirst + 5;    // NW
        case 0b0110: return kEdgeFirst + 6;    // SE
        case 0b1100: return kEdgeFirst + 7;    // SW
        case 0b0101: return kEdgeFirst + 8;    // NS
        case 0b1010: return kEdgeFirst + 9;    // EW
        case 0b0111: return kEdgeFirst + 10;   // NES
        case 0b1110: return kEdgeFirst + 11;   // SEW
        case 0b1101: return kEdgeFirst + 12;   // NSW
        case 0b1011: return kEdgeFirst + 13;   // NEW
        default:     return kNone;
    }
}

bool isWater(const Map& map, int x, int y) {
    if (x < 0 || x >= map.getWidth() || y < 0 || y >= map.getHeight()) return false;
    return map.getTile(x, y).type == TileType::Water;
}

} // namespace

int textureCount() { return kCount; }

const char* texturePath(int id) {
    static const std::string prefix = "terrain/";
    static std::string paths[kCount];
    if (id < 0 || id >= kCount) return "";
    if (paths[id].empty()) paths[id] = prefix + kFiles[id];
    return paths[id].c_str();
}

TileArt describe(const Map& map, int x, int y) {
    const Tile& tile = map.getTile(x, y);
    TileArt art;

    // Water tiles are drawn as they are.
    if (tile.type == TileType::Water) {
        const int v = tile.variant;
        art.base = kWaterFirst + ((v >= 1 && v <= 4) ? v - 1 : 0);
        return art;
    }

    // Everything else (grass / resource / building): look at the cardinal neighbours first.
    uint8_t cardinal = 0;
    if (isWater(map, x,     y - 1)) cardinal |= 0b0001;   // N
    if (isWater(map, x + 1, y    )) cardinal |= 0b0010;   // E
    if (isWater(map, x,     y + 1)) cardinal |= 0b0100;   // S
    if (isWater(map, x - 1, y    )) cardinal |= 0b1000;   // W

    if (cardinal != 0) {
        // Missing art falls back to plain grass so the gap is obvious.
        const int edge = edgeForMask(cardinal);
        art.base = (edge != kNone) ? edge : kGrassFirst;
        return art;
    }

    // No cardinal water: grass, plus a notch/strip overlay for diagonal water.
    const int v = tile.variant;
    art.base = kGrassFirst + ((v >= 1 && v <= 8) ? v - 1 : 0);

    const bool sw = isWater(map, x - 1, y + 1);
    const bool se = isWater(map, x + 1, y + 1);
    const bool nw = isWater(map, x - 1, y - 1);
    const bool ne = isWater(map, x + 1, y - 1);

    if      (nw && ne) art.overlay = kStripFirst + 0;   // N strip
    else if (sw && se) art.overlay = kStripFirst + 1;   // S strip
    else if (ne && se) art.overlay = kStripFirst + 2;   // E strip
    else if (nw && sw) art.overlay = kStripFirst + 3;   // W strip
    else if (sw)       art.overlay = kInnerFirst + 0;   // SW diagonal -> water_grass_NE
    else if (se)       art.overlay = kInnerFirst + 1;   // SE diagonal -> water_grass_NW
    else if (nw)       art.overlay = kInnerFirst + 2;   // NW diagonal -> water_grass_SE
    else if (ne)       art.overlay = kInnerFirst + 3;   // NE diagonal -> water_grass_SW
    return art;
}

} // namespace TerrainTiling
