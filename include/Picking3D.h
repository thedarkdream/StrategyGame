#pragma once

// ---------------------------------------------------------------------------
// Picking3D — mouse picking in the 3D view.  Entities are hit where they are
// drawn (glTF model bounds, sprite quad, or stand-in box when they have no
// artwork), using the same EntityVisual and Placement3D as the renderer.
// ---------------------------------------------------------------------------

#include "Camera.h"
#include "Types.h"
#include <SFML/System/Vector2.hpp>
#include <vector>

class Game;
class ModelBatch3D;

namespace Picking3D {

// Front-most selectable entity under a window pixel, or nullptr.
EntityPtr pickEntity(const Camera& camera, sf::Vector2i pixel, sf::Vector2u windowSize,
                     const ModelBatch3D& models, Game& game);

// Selectable entities of `team` whose drawn centre lies inside the rubber-band
// spanned by two window pixels.
std::vector<EntityPtr> pickEntitiesInRect(const Camera& camera, sf::Vector2i cornerA, sf::Vector2i cornerB,
                                          sf::Vector2u windowSize, const ModelBatch3D& models,
                                          Team team, Game& game);

} // namespace Picking3D
