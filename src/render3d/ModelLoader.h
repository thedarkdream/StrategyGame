#pragma once

// ---------------------------------------------------------------------------
// ModelLoader — reads a glTF 2.0 file (.glb or .gltf) into a Model.
//
// The only place that knows about glTF.  Supported: triangle meshes (node
// transforms are baked into the vertices), base-colour factor and texture
// (external file or embedded; both loaded through TextureManager), double-sided
// flag, and skeletal animation of the first skin (joint translation / rotation /
// scale clips; meshes attached rigidly to a joint node are not animated).  A
// material whose name starts with "team" is tinted with the owner's team colour
// at draw time.
// ---------------------------------------------------------------------------

#include <memory>
#include <string>

class Model;

namespace ModelLoader {

// `path` is relative to the assets folder, like TextureManager paths
// (e.g. "models/soldier.glb").  Returns nullptr (after logging why) on failure.
// Needs a current GL context.
std::unique_ptr<Model> load(const std::string& path);

} // namespace ModelLoader
