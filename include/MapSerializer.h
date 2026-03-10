#pragma once

#include "Types.h"
#include <string>
#include <vector>
#include <optional>

// ---------------------------------------------------------------------------
// Data containers (POD – no SFML, no engine deps)
// ---------------------------------------------------------------------------
struct MapTileData {
    int      x, y;
    TileType type;   // only non-Ground tiles are stored
};

struct MapEntityData {
    EntityType type;
    Team       team;
    int        tileX, tileY;
};

struct MapData {
    int                      version  = 1;
    std::string              name     = "untitled";
    int                      width    = 0;
    int                      height   = 0;
    std::vector<MapTileData>   tiles;    // sparse – Ground is default
    std::vector<MapEntityData> entities;
};

// ---------------------------------------------------------------------------
// Format: plain-text ".stmap"
//
//   version 1
//   name    untitled
//   size    40 30
//   tile    5  3  Blocked
//   entity  MineralPatch  Neutral  10  5
//
// Lines starting with '#' are ignored.
// ---------------------------------------------------------------------------
namespace MapSerializer {

// Returns true on success.  Path must include filename + extension.
bool save(const MapData& data, const std::string& filePath);

// Returns nullopt on parse/IO error.
std::optional<MapData> load(const std::string& filePath);

// Utility: list all .stmap file stems in a directory (sorted)
std::vector<std::string> listMaps(const std::string& dirPath);

// String <-> enum converters (exposed for tests / debugging)
const char* tileTypeToString(TileType t);
TileType    stringToTileType(const std::string& s);

const char* entityTypeToString(EntityType t);
EntityType  stringToEntityType(const std::string& s);

const char* teamToString(Team t);
Team        stringToTeam(const std::string& s);

} // namespace MapSerializer
