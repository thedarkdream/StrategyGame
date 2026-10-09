# GitHub Copilot — Basic Instructions

Purpose: provide minimal repository-specific guidance for Copilot and contributors on how to build this project on Windows using PowerShell.

Build (Windows PowerShell):

Run the following in PowerShell to ensure the correct MSYS2/ucrt64 toolchain is on the path and to build the project:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
cmake --build build -j4 2>&1
Write-Host "Exit: $LASTEXITCODE"
```

Notes:
- Use PowerShell (not cmd) when running the commands above.
- The command prepends `C:\msys64\ucrt64\bin` so the correct compiler/toolchain from MSYS2/UCRT64 is used.
- If you prefer to configure and generate the build files manually, run the appropriate `cmake` configure command first.

Code layout and conventions:
- Sources live in `src/<module>/` (header next to its .cpp); `src/` is the include root, so always write `#include "module/File.h"`. See README "Project Structure" for the modules. CMake globs `src/**`; re-run configure/build after adding files.
- Dependencies must point downwards: app -> screens -> game/ui/ai -> entities -> world/fx/sprite/media -> core. Renderers (render2d, render3d) read the simulation but the simulation must never include a renderer. `world/` and `entities/` must not draw.
- Load images only through `TextureManager` (paths relative to `assets/`).

Known architecture debts (fix opportunistically, do not make worse):
- Singleton macros (`TEXTURES`, `SOUNDS`, `EFFECTS`, `ENTITY_DATA`) are used all over `entities/` although `IGameContext` offers service accessors.
- `Entity` depends on `sprite/` (AnimatedSprite), SFML Graphics types, `media/` (loads textures, plays voice lines) and `fx/`.
- `PlayerCommandScope` (game/PlayerActions.h) is a global flag read by `Worker` to decide whether to play a voice line: a hidden channel between `game/` and `entities/`.
- `Game` is both the simulation and the composition root: it owns `ActionBar`, `DebugConsole`, `InputHandler` and the renderer (game <-> ui, game <-> ai, game -> render2d/render3d). Split into a simulation class and a presentation class (owned by `GameScreen`).
- `ai/AIController` includes `game/Game.h` while `Game` owns the controllers (cycle); the AI should depend on a narrow interface.
- `Renderer2D` contains HUD drawing (unit panel, resource bar) that duplicates `ui/` widgets; `Renderer3D` reuses it (render3d -> render2d). Fog-visibility rules are duplicated between `Renderer2D` and `FogOfWar::isEntityShown`.
- Very long functions: `EntityRegistry::initializeDefaults` (~430 lines, should be data files), `MapEditorScreen::handleEvent` (~320), `ActionBar::renderButtons`, `EditorPanel::rebuild`. Very large files: `MapEditorScreen.cpp`, `AIController.cpp`, `Unit.cpp`.
- `Types.h` is a grab bag (enums, Tile, Command, aliases); `Soldier`/`Brute`/`LightTank` are near-empty subclasses (data-driven candidates).
- Debug output uses `std::cout/cerr` directly (no logger).
