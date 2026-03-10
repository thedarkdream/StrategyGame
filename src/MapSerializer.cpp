#include "MapSerializer.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Enum <-> string tables
// ---------------------------------------------------------------------------
const char* MapSerializer::tileTypeToString(TileType t) {
    switch (t) {
        case TileType::Ground:   return "Ground";
        case TileType::Blocked:  return "Blocked";
        case TileType::Resource: return "Resource";
        case TileType::Building: return "Building";
    }
    return "Ground";
}

TileType MapSerializer::stringToTileType(const std::string& s) {
    if (s == "Blocked")  return TileType::Blocked;
    if (s == "Resource") return TileType::Resource;
    if (s == "Building") return TileType::Building;
    return TileType::Ground;
}

const char* MapSerializer::entityTypeToString(EntityType t) {
    switch (t) {
        case EntityType::Worker:       return "Worker";
        case EntityType::Soldier:      return "Soldier";
        case EntityType::Brute:        return "Brute";
        case EntityType::LightTank:    return "LightTank";
        case EntityType::Base:         return "Base";
        case EntityType::Barracks:     return "Barracks";
        case EntityType::Refinery:     return "Refinery";
        case EntityType::Factory:      return "Factory";
        case EntityType::MineralPatch: return "MineralPatch";
        case EntityType::GasGeyser:    return "GasGeyser";
        default:                       return "None";
    }
}

EntityType MapSerializer::stringToEntityType(const std::string& s) {
    if (s == "Worker")       return EntityType::Worker;
    if (s == "Soldier")      return EntityType::Soldier;
    if (s == "Brute")        return EntityType::Brute;
    if (s == "LightTank")    return EntityType::LightTank;
    if (s == "Base")         return EntityType::Base;
    if (s == "Barracks")     return EntityType::Barracks;
    if (s == "Refinery")     return EntityType::Refinery;
    if (s == "Factory")      return EntityType::Factory;
    if (s == "MineralPatch") return EntityType::MineralPatch;
    if (s == "GasGeyser")    return EntityType::GasGeyser;
    return EntityType::None;
}

const char* MapSerializer::teamToString(Team t) {
    switch (t) {
        case Team::Player:  return "Player";
        case Team::Enemy:   return "Enemy";
        case Team::Neutral: return "Neutral";
    }
    return "Neutral";
}

Team MapSerializer::stringToTeam(const std::string& s) {
    if (s == "Player")  return Team::Player;
    if (s == "Enemy")   return Team::Enemy;
    return Team::Neutral;
}

// ---------------------------------------------------------------------------
// save
// ---------------------------------------------------------------------------
bool MapSerializer::save(const MapData& data, const std::string& filePath) {
    // Ensure parent directory exists
    try {
        fs::path p(filePath);
        if (p.has_parent_path())
            fs::create_directories(p.parent_path());
    } catch (const std::exception& e) {
        std::cerr << "MapSerializer::save: cannot create directory: " << e.what() << "\n";
        return false;
    }

    std::ofstream out(filePath);
    if (!out.is_open()) {
        std::cerr << "MapSerializer::save: cannot open file: " << filePath << "\n";
        return false;
    }

    out << "version " << data.version << "\n";
    out << "name    " << data.name    << "\n";
    out << "size    " << data.width   << " " << data.height << "\n";

    // Sparse tile list – skip Ground tiles
    for (const auto& t : data.tiles) {
        if (t.type == TileType::Ground) continue;
        out << "tile    "
            << t.x << " " << t.y << " "
            << tileTypeToString(t.type) << "\n";
    }

    for (const auto& e : data.entities) {
        if (e.type == EntityType::None) continue;
        out << "entity  "
            << entityTypeToString(e.type) << " "
            << teamToString(e.team)       << " "
            << e.tileX << " " << e.tileY  << "\n";
    }

    out.flush();
    return out.good();
}

// ---------------------------------------------------------------------------
// load
// ---------------------------------------------------------------------------
std::optional<MapData> MapSerializer::load(const std::string& filePath) {
    std::ifstream in(filePath);
    if (!in.is_open()) {
        std::cerr << "MapSerializer::load: cannot open file: " << filePath << "\n";
        return std::nullopt;
    }

    MapData data;
    bool    hasSz = false;
    std::string line;

    while (std::getline(in, line)) {
        // Strip carriage returns (Windows line endings)
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // Skip blank lines and comments
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string key;
        ss >> key;

        if (key == "version") {
            ss >> data.version;
        } else if (key == "name") {
            std::string rest;
            std::getline(ss, rest);
            // Trim leading whitespace
            size_t start = rest.find_first_not_of(" \t");
            data.name = (start == std::string::npos) ? "" : rest.substr(start);
        } else if (key == "size") {
            ss >> data.width >> data.height;
            hasSz = true;
        } else if (key == "tile") {
            MapTileData t;
            std::string typeStr;
            ss >> t.x >> t.y >> typeStr;
            t.type = stringToTileType(typeStr);
            data.tiles.push_back(t);
        } else if (key == "entity") {
            MapEntityData e;
            std::string typeStr, teamStr;
            ss >> typeStr >> teamStr >> e.tileX >> e.tileY;
            e.type = stringToEntityType(typeStr);
            e.team = stringToTeam(teamStr);
            if (e.type != EntityType::None)
                data.entities.push_back(e);
        }
    }

    if (!hasSz || data.width <= 0 || data.height <= 0) {
        std::cerr << "MapSerializer::load: invalid or missing 'size' in " << filePath << "\n";
        return std::nullopt;
    }

    return data;
}

// ---------------------------------------------------------------------------
// listMaps
// ---------------------------------------------------------------------------
std::vector<std::string> MapSerializer::listMaps(const std::string& dirPath) {
    std::vector<std::string> result;
    try {
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath))
            return result;

        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".stmap")
                result.push_back(entry.path().stem().string());
        }
    } catch (...) {}

    std::sort(result.begin(), result.end());
    return result;
}
