#ifndef TINYSPRITE_UI_TOOL_HPP
#define TINYSPRITE_UI_TOOL_HPP

#include "tinysprite/tools.hpp"

/* The order is the tool's number in the settings file, so new tools go at the end. */
enum class ToolKind {
    Pencil,
    Eraser,
    Bucket,
    Eyedropper,
    Line,
    Rect,
    Ellipse,
    Select,
    Triangle,
    Diamond,
    Hexagon,
    Star,
    Cross,
    Slice,
    Count,
};

inline constexpr int TOOL_COUNT = static_cast<int>(ToolKind::Count);

/* What a tool does with a press, which is all the editor, the canvas and the tools panel need to
 * know about it. */
enum class ToolGroup {
    Brush,  /* pencil, eraser: strokes, Shift+click draws a line from the last stroke */
    Fill,   /* bucket: a single click */
    Pick,   /* eyedropper: samples a color while the button is held */
    Shape,  /* a shape dragged between two corners */
    Select, /* drags out a selection, or moves the pixels inside it */
    Slice,  /* drags out a slice (a named rectangle for the game), or moves one */
    Count,
};

struct ToolTraits {
    ToolKind kind;
    ToolGroup group;
    ShapeKind shape; /* what a Shape tool draws; ignored by the others */
};

/* The row for `kind`; the table is in ToolKind order. */
const ToolTraits &tool_traits(ToolKind kind);

/* Whether the tool changes cel pixels (and so only reaches inside a selection and needs a layer
 * that can be edited). */
inline bool tool_draws(ToolKind kind)
{
    ToolGroup group = tool_traits(kind).group;
    return group == ToolGroup::Brush || group == ToolGroup::Fill || group == ToolGroup::Shape;
}

#endif
