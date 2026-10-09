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

Headers and sources live together, one folder per module under `src/`.
`src/` is the include root: `#include "entities/Unit.h"`. CMake globs `src/**`,
so new files are picked up automatically (re-run the build).

```
joc_strategie/
+-- CMakeLists.txt
+-- assets/ maps/ sounds/ aiscripts/   # runtime data
+-- tools/                             # asset generators (e.g. make_soldier_glb.py)
+-- src/
    +-- main.cpp
    +-- app/        Application: window, GL context, screen switching
    +-- screens/    Screen interface + Menu, Game, MapEditor, Victory screens
    +-- game/       Game (session + composition root), Player, PlayerActions,
    ¦               InputHandler, FogOfWar, GameStatistics, ResourceManager
    +-- entities/   Entity, Unit/Worker/Soldier/..., Building, Turret, ResourceNode,
    ¦               Projectile, EntityWorld, EntityData (registry), IGameContext
    +-- ai/         AIController, AIScript, PlayerController
    +-- world/      Map, Pathfinder, MapSerializer, TerrainTiling (no rendering)
    +-- ui/         ActionBar, Minimap, DebugConsole, EditorPanel (2D HUD widgets)
    +-- fx/         Effect, EffectsManager
    +-- sprite/     Animation, AnimatedSprite (sprite animation state)
    +-- media/      TextureManager, SoundManager, FontManager
    +-- render/     Renderer-agnostic: IRenderer, Camera, EntityVisual
    +-- render2d/   SFML renderer: Renderer2D, EntityRenderer2D, TerrainRenderer
    +-- render3d/   OpenGL renderer: Renderer3D, Scene3D, terrain/sprite/model
    ¦               batches, Camera3D, picking, glTF Model/ModelLoader, EditorView3D
    +-- gl/         GLShader, GLMesh
    +-- core/       Types, Constants, MathUtil, IdGenerator (no dependencies)
```

Intended dependency direction (lower modules must not include higher ones):

```
app -> screens -> game/ui/ai -> entities -> world/fx/sprite/media -> core
render2d, render3d -> game/entities/world (read-only);  gl <- render3d
```

Known violations are listed in the architecture notes in `copilot-instructions.md`.
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
