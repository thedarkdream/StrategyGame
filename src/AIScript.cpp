#include "AIScript.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

// Trim whitespace from both ends
static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Convert string to lowercase
static std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool AIScript::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    // Extract script name from filepath
    fs::path path(filepath);
    m_name = path.stem().string();
    
    m_commands.clear();
    
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        auto cmd = parseLine(line);
        if (cmd.has_value()) {
            m_commands.push_back(cmd.value());
        }
    }
    
    // Post-process: resolve each EndLoop's loopStartIndex to its matching Loop.
    // Uses a stack to support nested loops.
    {
        std::vector<int> loopStack;
        for (int i = 0; i < static_cast<int>(m_commands.size()); ++i) {
            if (m_commands[i].type == AICommand::Type::LoopStart) {
                loopStack.push_back(i);
            } else if (m_commands[i].type == AICommand::Type::LoopEnd) {
                if (!loopStack.empty()) {
                    m_commands[i].loopStartIndex = loopStack.back();
                    loopStack.pop_back();
                }
                // Unmatched EndLoop: loopStartIndex stays -1 (ignored at runtime)
            }
        }
    }

    return !m_commands.empty();
}

std::optional<AICommand> AIScript::parseLine(const std::string& line) {
    std::istringstream iss(line);
    std::string commandStr;
    iss >> commandStr;
    
    if (commandStr.empty()) {
        return std::nullopt;
    }
    
    std::string cmdLower = toLower(commandStr);
    AICommand cmd;
    
    if (cmdLower == "train") {
        // Train UnitType Count
        std::string unitTypeStr;
        int count = 1;
        
        if (!(iss >> unitTypeStr)) {
            return std::nullopt;
        }
        iss >> count;  // Optional, defaults to 1
        
        cmd.type = AICommand::Type::Train;
        cmd.entityType = stringToEntityType(unitTypeStr);
        cmd.count = count;
        
        if (cmd.entityType == EntityType::None) {
            return std::nullopt;
        }
    }
    else if (cmdLower == "build") {
        // Build BuildingType
        std::string buildingTypeStr;
        
        if (!(iss >> buildingTypeStr)) {
            return std::nullopt;
        }
        
        cmd.type = AICommand::Type::Build;
        cmd.entityType = stringToEntityType(buildingTypeStr);
        cmd.count = 1;
        
        if (cmd.entityType == EntityType::None) {
            return std::nullopt;
        }
    }
    else if (cmdLower == "attack_add") {
        // Attack_Add UnitType Count
        std::string unitTypeStr;
        int count = 1;
        
        if (!(iss >> unitTypeStr)) {
            return std::nullopt;
        }
        iss >> count;  // Optional, defaults to 1
        
        cmd.type = AICommand::Type::AttackAdd;
        cmd.entityType = stringToEntityType(unitTypeStr);
        cmd.count = count;
        
        if (cmd.entityType == EntityType::None) {
            return std::nullopt;
        }
    }
    else if (cmdLower == "attack") {
        cmd.type = AICommand::Type::Attack;
    }
    else if (cmdLower == "loop") {
        // Loop [N]  — N optional, 0 or omitted means infinite
        int n = 0;
        iss >> n;
        cmd.type  = AICommand::Type::LoopStart;
        cmd.count = n;
    }
    else if (cmdLower == "endloop") {
        cmd.type = AICommand::Type::LoopEnd;
        // loopStartIndex is resolved in the post-processing pass below
    }
    else {
        // Unknown command
        return std::nullopt;
    }
    
    return cmd;
}

EntityType AIScript::stringToEntityType(const std::string& str) {
    std::string lower = toLower(str);
    
    // Units
    if (lower == "worker") return EntityType::Worker;
    if (lower == "soldier") return EntityType::Soldier;
    if (lower == "brute") return EntityType::Brute;
    if (lower == "lighttank") return EntityType::LightTank;
    
    // Buildings
    if (lower == "base") return EntityType::Base;
    if (lower == "barracks") return EntityType::Barracks;
    if (lower == "refinery") return EntityType::Refinery;
    if (lower == "factory") return EntityType::Factory;
    
    return EntityType::None;
}

std::vector<AIScript> AIScriptLoader::loadAllScripts(const std::string& directory) {
    std::vector<AIScript> scripts;
    
    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".scr") {
                AIScript script;
                if (script.loadFromFile(entry.path().string())) {
                    scripts.push_back(std::move(script));
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // Directory doesn't exist or other error
    }
    
    return scripts;
}
