#pragma once

// ---------------------------------------------------------------------------
// EntityDrawing — free functions for the common per-entity rendering overlays.
//
// Moving these out of Entity keeps the base class free of rendering concerns:
// it holds only identity, position, health and combat state.  The functions
// operate exclusively through Entity's public interface so they remain
// decoupled from the internals of any particular subclass.
// ---------------------------------------------------------------------------

#include <SFML/Graphics/RenderTarget.hpp>

class Entity;

namespace EntityDrawing {

// Draw the HP bar above `entity`.  Visible whenever the entity is selected
// or its health is below maximum.
void drawHealthBar(sf::RenderTarget& target, const Entity& entity);

// Draw the circular selection ring / highlight blink around `entity`.
void drawSelectionIndicator(sf::RenderTarget& target, const Entity& entity);

} // namespace EntityDrawing
