#ifndef TINYSPRITE_UI_COMMON_H
#define TINYSPRITE_UI_COMMON_H

/* cimgui only declares its structs and enums when this is defined before its header. */
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include "cimgui.h"

#include "tinysprite/palette.h"

#define COL32(r, g, b, a)                                                                          \
    ((ImU32)(((ImU32)(a) << 24) | ((ImU32)(b) << 16) | ((ImU32)(g) << 8) | (ImU32)(r)))

static inline ImVec4 imvec4_from_rgba(Rgba color)
{
    return (ImVec4){(float)color.r / 255.0f, (float)color.g / 255.0f, (float)color.b / 255.0f,
                    (float)color.a / 255.0f};
}

static inline ImVec4 imvec4_from_floats(const float color[4])
{
    return (ImVec4){color[0], color[1], color[2], color[3]};
}

#endif
