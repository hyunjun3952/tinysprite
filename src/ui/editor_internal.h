#ifndef TINYSPRITE_UI_EDITOR_INTERNAL_H
#define TINYSPRITE_UI_EDITOR_INTERNAL_H

#include <cstdint>

#include "tinysprite/palette.h"
#include "tinysprite/sprite.h"

/* Helpers the editor_*.cpp files share (editor.cpp is split by what the code is about). */
namespace editor_detail
{

/* 0..1 float to 0..255 byte, and back into the four floats of a color. */
uint8_t toByte(float value);
void setFloats(float out[4], Rgba color);

/* A starting palette so the panel is useful before the first color is added. */
Palette defaultPalette();

/* The index of `layer` in `sprite`, or -1 when it is not one of its layers. */
int indexOfLayer(const Sprite &sprite, const Layer *layer);

} // namespace editor_detail

#endif
