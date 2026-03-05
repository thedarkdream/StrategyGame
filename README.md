# Strategy Game

A simple 2D real-time strategy game inspired by StarCraft, built with C++ and SFML.

## Features

- **Resource Gathering**: Workers collect minerals to fund your army
- **Base Building**: Construct buildings to produce units
- **Unit Production**: Train workers and soldiers
- **Combat System**: Units attack enemies automatically when in range
- **AI Opponent**: Basic AI that builds units and attacks

## Requirements

- C++17 compatible compiler
- CMake 3.16+
- SFML 2.5+

## Building

### Windows (with vcpkg)

1. Install vcpkg and SFML:
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg install sfml:x64-windows
   ```

2. Build the game:
   ```powershell
   cd path\to\joc_strategie
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]\scripts\buildsystems\vcpkg.cmake
   cmake --build . --config Release
   ```

### Linux

1. Install SFML:
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libsfml-dev
   
   # Fedora
   sudo dnf install SFML-devel
   
   # Arch
   sudo pacman -S sfml
   ```

2. Build:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

## Controls

### Camera
- **Arrow keys / WASD**: Scroll camera
- **Mouse at screen edge**: Scroll camera

### Selection
- **Left click**: Select unit/building
- **Left click + drag**: Box select multiple units
- **Escape**: Deselect all

### Commands
- **Right click on ground**: Move selected units
- **Right click on enemy**: Attack target
- **Right click on minerals**: Gather resources (workers only)

### Building (when units/buildings selected)
- **B**: Place Barracks (150 minerals)
- **H**: Place Command Center (400 minerals)
- **T**: Train unit (Worker from Base, Soldier from Barracks)
- **Q**: Stop current action

### Other
- **Escape**: Cancel build mode

## Unit Costs

| Unit/Building | Mineral Cost |
|---------------|-------------|
| Worker        | 50          |
| Soldier       | 100         |
| Barracks      | 150         |
| Command Center| 400         |

## Project Structure

```
joc_strategie/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── include/                # Header files
│   ├── AIController.h      # AI logic
│   ├── Building.h          # Building entities
│   ├── Constants.h         # Game settings
│   ├── Entity.h            # Base entity class
│   ├── Game.h              # Main game class
│   ├── InputHandler.h      # Input processing
│   ├── Map.h               # Map/terrain system
│   ├── Player.h            # Player state
│   ├── Renderer.h          # Rendering system
│   ├── ResourceManager.h   # Entity factory
│   ├── Types.h             # Type definitions
│   └── Unit.h              # Unit entities
└── src/                    # Source files
    ├── AIController.cpp
    ├── Building.cpp
    ├── Entity.cpp
    ├── Game.cpp
    ├── InputHandler.cpp
    ├── main.cpp
    ├── Map.cpp
    ├── Player.cpp
    ├── Renderer.cpp
    ├── ResourceManager.cpp
    └── Unit.cpp
```

## Future Improvements

- [ ] Proper A* pathfinding
- [ ] Fog of war
- [ ] Tech tree / upgrades
- [ ] Multiple unit types
- [ ] Sound effects and music
- [ ] Multiple factions
- [ ] Networked multiplayer
- [ ] Better AI behaviors
- [ ] Sprite-based graphics
- [ ] Save/Load system

## License

This project is open source. Feel free to use and modify it for your own projects.
