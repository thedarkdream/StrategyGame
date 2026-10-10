#pragma once

// ---------------------------------------------------------------------------
// TerrainTiling — which terrain art a tile uses, independent of the renderer.
//
// The art is a fixed set of 64x64 images (grass and water variants plus the
// water/grass transition tiles).  describe() applies the autotile rules and
// returns image ids; each renderer (2D sprites, 3D texture array) loads
// texturePath(id) for every id in [0, textureCount()) and draws what describe()
// asks for.
// ---------------------------------------------------------------------------

class Map;

namespace TerrainTiling {

constexpr int kNone = -1;

struct TileArt {
    int base    = kNone;   // drawn first, opaque
    int overlay = kNone;   // drawn over the base (water notches on grass), may be transparent
};

int         textureCount();
const char* texturePath(int id);   // relative to the assets folder, as TextureManager expects

// Seamless rock texture for cliffs (any size, power of two recommended); used
// by the 3D view only, mapped triplanar rather than per tile.  Relative to the
// assets folder, like texturePath().
constexpr const char* kCliffTexturePath = "terrain/cliff_1.png";

// Art for the tile at (x, y); the tile must be valid.
TileArt describe(const Map& map, int x, int y);

} // namespace TerrainTiling
