/* The per-tool table the editor, the canvas and the tools panel ask instead of switching on
 * ToolKind. No window is needed. */
#include <set>

#include "minitest.h"
#include "tool.h"

TEST(every_tool_has_a_row_in_enum_order)
{
    for (int i = 0; i < TOOL_COUNT; i++) {
        ASSERT_TRUE(tool_traits(static_cast<ToolKind>(i)).kind == static_cast<ToolKind>(i));
    }
}

TEST(each_shape_belongs_to_exactly_one_shape_tool)
{
    std::set<int> seen;
    for (int i = 0; i < TOOL_COUNT; i++) {
        const ToolTraits &t = tool_traits(static_cast<ToolKind>(i));
        if (t.group == ToolGroup::Shape) {
            ASSERT_TRUE(seen.insert(static_cast<int>(t.shape)).second);
        }
    }
    ASSERT_EQ_INT(SHAPE_COUNT, static_cast<int>(seen.size()));
}

TEST(shape_tools_map_to_their_shape)
{
    ASSERT_TRUE(tool_traits(ToolKind::Line).shape == ShapeKind::Line);
    ASSERT_TRUE(tool_traits(ToolKind::Rect).shape == ShapeKind::Rect);
    ASSERT_TRUE(tool_traits(ToolKind::Star).shape == ShapeKind::Star);
    ASSERT_TRUE(tool_traits(ToolKind::Cross).shape == ShapeKind::Cross);
}

TEST(tools_that_edit_pixels_are_the_ones_clipped_by_a_selection)
{
    ASSERT_TRUE(tool_draws(ToolKind::Pencil));
    ASSERT_TRUE(tool_draws(ToolKind::Eraser));
    ASSERT_TRUE(tool_draws(ToolKind::Bucket));
    ASSERT_TRUE(tool_draws(ToolKind::Hexagon));
    ASSERT_TRUE(!tool_draws(ToolKind::Select));
    ASSERT_TRUE(!tool_draws(ToolKind::Eyedropper));
}

TEST(only_the_pencil_and_eraser_are_brushes)
{
    for (int i = 0; i < TOOL_COUNT; i++) {
        ToolKind tool = static_cast<ToolKind>(i);
        bool brush = tool == ToolKind::Pencil || tool == ToolKind::Eraser;
        ASSERT_TRUE((tool_traits(tool).group == ToolGroup::Brush) == brush);
    }
}

int main(void)
{
    RUN(every_tool_has_a_row_in_enum_order);
    RUN(each_shape_belongs_to_exactly_one_shape_tool);
    RUN(shape_tools_map_to_their_shape);
    RUN(tools_that_edit_pixels_are_the_ones_clipped_by_a_selection);
    RUN(only_the_pencil_and_eraser_are_brushes);
    return TEST_SUMMARY();
}
