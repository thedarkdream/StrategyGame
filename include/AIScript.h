#pragma once

#include "Types.h"
#include <string>
#include <vector>
#include <optional>

// Represents a single command in an AI script
struct AICommand {
    enum class Type {
        Train,      // Train UnitType Count
        Build,      // Build BuildingType
        AttackAdd,  // Attack_Add UnitType Count
        Attack,     // Attack (execute attack with accumulated units)
        LoopStart,  // Loop [N]  — begin loop body; count=N (0 = infinite)
        LoopEnd     // EndLoop   — jump back to matching LoopStart
    };
    
    Type type;
    EntityType entityType = EntityType::None;  // For Train/Build/AttackAdd
    int count = 1;          // For Train/AttackAdd; iterations for LoopStart (0=infinite)
    int loopStartIndex = -1;// For LoopEnd: index of matching LoopStart (set at load time)
};

// Parses and holds an AI script
class AIScript {
public:
    AIScript() = default;
    
    // Load script from file, returns true on success
    bool loadFromFile(const std::string& filepath);
    
    // Get all commands
    const std::vector<AICommand>& getCommands() const { return m_commands; }
    
    // Get script name (filename without extension)
    const std::string& getName() const { return m_name; }
    
private:
    std::string m_name;
    std::vector<AICommand> m_commands;
    
    // Parse a single line into a command
    std::optional<AICommand> parseLine(const std::string& line);
    
    // Convert string to EntityType
    static EntityType stringToEntityType(const std::string& str);
};

// Loads all scripts from a directory
class AIScriptLoader {
public:
    // Load all .scr files from directory
    static std::vector<AIScript> loadAllScripts(const std::string& directory);
};
