#ifndef TINYSPRITE_UI_COMMON_H
#define TINYSPRITE_UI_COMMON_H

#include "imgui.h"

#include "tinysprite/palette.h"

inline ImVec4 imvec4FromRgba(Rgba color)
{
    return ImVec4(static_cast<float>(color.r) / 255.0f, static_cast<float>(color.g) / 255.0f,
                  static_cast<float>(color.b) / 255.0f, static_cast<float>(color.a) / 255.0f);
}

inline ImVec4 imvec4FromFloats(const float color[4])
{
    return ImVec4(color[0], color[1], color[2], color[3]);
}

#endif
