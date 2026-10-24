#include "tool.hpp"

#include <array>

namespace
{

/* A new tool is one row here (plus its entry in the tools panel). */
const std::array<ToolTraits, TOOL_COUNT> TRAITS = {{
    {ToolKind::Pencil, ToolGroup::Brush, ShapeKind::Line},
    {ToolKind::Eraser, ToolGroup::Brush, ShapeKind::Line},
    {ToolKind::Bucket, ToolGroup::Fill, ShapeKind::Line},
    {ToolKind::Eyedropper, ToolGroup::Pick, ShapeKind::Line},
    {ToolKind::Line, ToolGroup::Shape, ShapeKind::Line},
    {ToolKind::Rect, ToolGroup::Shape, ShapeKind::Rect},
    {ToolKind::Ellipse, ToolGroup::Shape, ShapeKind::Ellipse},
    {ToolKind::Select, ToolGroup::Select, ShapeKind::Line},
    {ToolKind::Triangle, ToolGroup::Shape, ShapeKind::Triangle},
    {ToolKind::Diamond, ToolGroup::Shape, ShapeKind::Diamond},
    {ToolKind::Hexagon, ToolGroup::Shape, ShapeKind::Hexagon},
    {ToolKind::Star, ToolGroup::Shape, ShapeKind::Star},
    {ToolKind::Cross, ToolGroup::Shape, ShapeKind::Cross},
    {ToolKind::Slice, ToolGroup::Slice, ShapeKind::Line},
    {ToolKind::Pan, ToolGroup::Pan, ShapeKind::Line},
}};

} // namespace

const ToolTraits &tool_traits(ToolKind kind)
{
    return TRAITS[static_cast<size_t>(kind)];
}
