#include "icons.hpp"

#include <algorithm>
#include <cmath>

#include "theme.hpp"

namespace icons
{

namespace
{

constexpr size_t ICON_COUNT = static_cast<size_t>(Icon::Count);

/* Indexed by Icon. */
const std::array<std::array<const char *, SIZE>, ICON_COUNT> BITMAPS = {{
    /* Eye */
    {{
        "................",
        "................",
        "................",
        "....XXXXXXXX....",
        "..XXX......XXX..",
        ".XX...XXXX...XX.",
        "XX...XXXXXX...XX",
        "XX...XXXXXX...XX",
        ".XX...XXXX...XX.",
        "..XXX......XXX..",
        "....XXXXXXXX....",
        "................",
        "................",
        "................",
        "................",
        "................",
    }},
    /* EyeOff */
    {{
        "................",
        "................",
        ".............XX.",
        "....XXXXXXXXXX..",
        "..XXX......XXX..",
        ".XX.......XX.XX.",
        "XX.......XX...XX",
        "XX......XX....XX",
        ".XX....XX....XX.",
        "..XXX.XX...XXX..",
        "....XXXXXXXX....",
        "....XX..........",
        "...XX...........",
        "..XX............",
        "................",
        "................",
    }},
    /* Lock */
    {{
        "................",
        ".....XXXXXX.....",
        "....XX....XX....",
        "....XX....XX....",
        "....XX....XX....",
        "..XXXXXXXXXXXX..",
        "..XXXXXXXXXXXX..",
        "..XXXXX..XXXXX..",
        "..XXXXX..XXXXX..",
        "..XXXXX..XXXXX..",
        "..XXXXXXXXXXXX..",
        "..XXXXXXXXXXXX..",
        "..XXXXXXXXXXXX..",
        "................",
        "................",
        "................",
    }},
    /* Unlock */
    {{
        "................",
        ".....XXXXXX.....",
        "....XX....XX....",
        "....XX..........",
        "....XX..........",
        "..XXXXXXXXXXXX..",
        "..XXXXXXXXXXXX..",
        "..XXXXX..XXXXX..",
        "..XXXXX..XXXXX..",
        "..XXXXX..XXXXX..",
        "..XXXXXXXXXXXX..",
        "..XXXXXXXXXXXX..",
        "..XXXXXXXXXXXX..",
        "................",
        "................",
        "................",
    }},
    /* Plus */
    {{
        "................",
        "................",
        "................",
        ".......XX.......",
        ".......XX.......",
        ".......XX.......",
        ".......XX.......",
        "...XXXXXXXXXX...",
        "...XXXXXXXXXX...",
        ".......XX.......",
        ".......XX.......",
        ".......XX.......",
        ".......XX.......",
        "................",
        "................",
        "................",
    }},
    /* Copy */
    {{
        "................",
        "................",
        "..XXXXXXXX......",
        "..X......X......",
        "..X......X......",
        "..X..XXXXXXXXX..",
        "..X..X.......X..",
        "..X..X.......X..",
        "..X..X.......X..",
        "..XXXX.......X..",
        ".....X.......X..",
        ".....X.......X..",
        ".....X.......X..",
        ".....XXXXXXXXX..",
        "................",
        "................",
    }},
    /* Trash */
    {{
        "................",
        "................",
        "......XXXX......",
        "...XXXXXXXXXX...",
        "................",
        "....XXXXXXXX....",
        "....X......X....",
        "....X.X..X.X....",
        "....X.X..X.X....",
        "....X.X..X.X....",
        "....X.X..X.X....",
        "....X.X..X.X....",
        "....X......X....",
        "....XXXXXXXX....",
        "................",
        "................",
    }},
    /* Merge */
    {{
        "................",
        ".......XX.......",
        ".......XX.......",
        ".......XX.......",
        ".......XX.......",
        ".......XX.......",
        "....XXXXXXXX....",
        ".....XXXXXX.....",
        "......XXXX......",
        ".......XX.......",
        "................",
        "................",
        "..XXXXXXXXXXXX..",
        "..XXXXXXXXXXXX..",
        "................",
        "................",
    }},
    /* Line */
    {{
        "................",
        "................",
        "............XX..",
        "...........XX...",
        "..........XX....",
        ".........XX.....",
        "........XX......",
        ".......XX.......",
        "......XX........",
        ".....XX.........",
        "....XX..........",
        "...XX...........",
        "..XX............",
        ".XX.............",
        "................",
        "................",
    }},
    /* Rect */
    {{
        "................",
        "................",
        "..XXXXXXXXXXXX..",
        "..X..........X..",
        "..X..........X..",
        "..X..........X..",
        "..X..........X..",
        "..X..........X..",
        "..X..........X..",
        "..X..........X..",
        "..X..........X..",
        "..X..........X..",
        "..X..........X..",
        "..XXXXXXXXXXXX..",
        "................",
        "................",
    }},
    /* Ellipse */
    {{
        "................",
        "................",
        "................",
        ".....XXXXX......",
        "...XX.....XX....",
        "..X.........X...",
        ".X...........X..",
        ".X...........X..",
        ".X...........X..",
        "..X.........X...",
        "...XX.....XX....",
        ".....XXXXX......",
        "................",
        "................",
        "................",
        "................",
    }},
    /* Triangle */
    {{
        "................",
        ".......X........",
        "......X.X.......",
        "......X.X.......",
        ".....X...X......",
        ".....X...X......",
        "....X.....X.....",
        "....X.....X.....",
        "...X.......X....",
        "...X.......X....",
        "..X.........X...",
        "..X.........X...",
        ".X...........X..",
        ".XXXXXXXXXXXXX..",
        "................",
        "................",
    }},
    /* Diamond */
    {{
        "................",
        ".......X........",
        "......X.X.......",
        ".....X...X......",
        "....X.....X.....",
        "...X.......X....",
        "..X.........X...",
        ".X...........X..",
        "..X.........X...",
        "...X.......X....",
        "....X.....X.....",
        ".....X...X......",
        "......X.X.......",
        ".......X........",
        "................",
        "................",
    }},
    /* Hexagon */
    {{
        "................",
        "....XXXXXXX.....",
        "...X.......X....",
        "...X.......X....",
        "..X.........X...",
        "..X.........X...",
        ".X...........X..",
        ".X...........X..",
        "..X.........X...",
        "..X.........X...",
        "...X.......X....",
        "...X.......X....",
        "....X.....X.....",
        "....XXXXXXX.....",
        "................",
        "................",
    }},
    /* Star */
    {{
        "................",
        "................",
        ".......X........",
        "......X.X.......",
        "......X.X.......",
        ".XXXXX...XXXXX..",
        "..X.........X...",
        "...X.......X....",
        "....X.....X.....",
        "....X.....X.....",
        "...X...X...X....",
        "...X..X.X..X....",
        "..X..X...X..X...",
        "..XXX.....XXX...",
        "................",
        "................",
    }},
    /* Cross */
    {{
        "................",
        ".....XXXXX......",
        ".....X...X......",
        ".....X...X......",
        ".....X...X......",
        ".XXXXX...XXXXX..",
        ".X...........X..",
        ".X...........X..",
        ".X...........X..",
        ".XXXXX...XXXXX..",
        ".....X...X......",
        ".....X...X......",
        ".....X...X......",
        ".....XXXXX......",
        "................",
        "................",
    }},
    /* Minus */
    {{
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "...XXXXXXXXXX...",
        "...XXXXXXXXXX...",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    }},
    /* Reset */
    {{
        "........X.......",
        "........XX......",
        ".....XXXXXX.....",
        "....X...XX......",
        "...X....X.......",
        "..X.............",
        ".X..............",
        ".X..............",
        ".X...........X..",
        ".X...........X..",
        ".X...........X..",
        "..X.........X...",
        "...X.......X....",
        "....X.....X.....",
        ".....XXXXX......",
        "................",
    }},
    /* ArrowUp */
    {{
        "................",
        "................",
        ".......XX.......",
        "......XXXX......",
        ".....XXXXXX.....",
        "....XXXXXXXX....",
        "...XXXXXXXXXX...",
        "......XXXX......",
        "......XXXX......",
        "......XXXX......",
        "......XXXX......",
        "................",
        "................",
        "................",
        "................",
        "................",
    }},
    /* ArrowDown */
    {{
        "................",
        "................",
        "................",
        "................",
        "................",
        "......XXXX......",
        "......XXXX......",
        "......XXXX......",
        "......XXXX......",
        "...XXXXXXXXXX...",
        "....XXXXXXXX....",
        ".....XXXXXX.....",
        "......XXXX......",
        ".......XX.......",
        "................",
        "................",
    }},
    /* Pencil */
    {{
        "................",
        "............XX..",
        "..........XXXXX.",
        ".........XXXXXX.",
        "........XXXXX...",
        ".......XXXXX....",
        "......XXXXX.....",
        ".....XXXXX......",
        "....XXXXX.......",
        "...XXXXX........",
        "...XXXX.........",
        "...XXX..........",
        "..XXX...........",
        ".XXX............",
        ".XX.............",
        "................",
    }},
    /* Eraser */
    {{
        "................",
        "................",
        "................",
        "................",
        "..XXXXXXXXXXXX..",
        "..XXXXX......X..",
        "..XXXXX......X..",
        "..XXXXX......X..",
        "..XXXXX......X..",
        "..XXXXX......X..",
        "..XXXXX......X..",
        "..XXXXXXXXXXXX..",
        "................",
        "..XXXXXXXXXXXX..",
        "................",
        "................",
    }},
    /* Bucket */
    {{
        "................",
        "................",
        "................",
        "......XXX.......",
        ".....X...X......",
        ".....X...X......",
        "...XXXXXXXXX....",
        "...XXXXXXXXX....",
        "...XXXXXXXXX....",
        "...XXXXXXXXX.X..",
        "...X.......X.XX.",
        "...X.......X.XX.",
        "...X.......X.X..",
        "...XXXXXXXXX....",
        "................",
        "................",
    }},
    /* Eyedropper */
    {{
        "...........XXX..",
        "..........XXXXX.",
        "..........XXXXX.",
        "..........XXXXX.",
        "..........XXXXX.",
        ".........XXXXXX.",
        "........XXX.....",
        ".......XXX......",
        "......XXX.......",
        ".....XXX........",
        "....XXX.........",
        "...XXX..........",
        "..XXX...........",
        "..XX............",
        ".XX.............",
        "................",
    }},
    /* Select */
    {{
        "................",
        "................",
        "..XX.XX.XX.XX...",
        "..X..........X..",
        "..X..........X..",
        "................",
        "..X..........X..",
        "..X..........X..",
        "................",
        "..X..........X..",
        "..X..........X..",
        "................",
        "..X..........X..",
        "..X.XX.XX.XX.X..",
        "................",
        "................",
    }},
}};

constexpr float ROUND = 3.0f;

ImU32 withAlpha(unsigned hex, float alpha)
{
    return ImGui::ColorConvertFloat4ToU32(theme::rgb(hex, alpha));
}

} // namespace

int pixel_scale(float uiScale)
{
    return std::max(1, static_cast<int>(std::floor(uiScale + 0.5f)));
}

const std::array<const char *, SIZE> &bitmap(Icon icon)
{
    return BITMAPS[static_cast<size_t>(icon)];
}

namespace
{

/* The box around the lit pixels of a bitmap (inclusive); x1 < 0 when none is lit. */
struct LitBounds {
    int x0 = SIZE;
    int x1 = -1;
    int y0 = SIZE;
    int y1 = -1;
};

LitBounds litBounds(const std::array<const char *, SIZE> &rows)
{
    LitBounds b;
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            if (rows[static_cast<size_t>(y)][x] == 'X') {
                b.x0 = std::min(b.x0, x);
                b.x1 = std::max(b.x1, x);
                b.y0 = std::min(b.y0, y);
                b.y1 = std::max(b.y1, y);
            }
        }
    }
    return b;
}

/* The end (inclusive) of the run of lit pixels that starts at `x` in `row`, not past `limit`. */
int runEnd(const char *row, int x, int limit)
{
    int end = x;
    while (end + 1 <= limit && row[end + 1] == 'X') {
        end++;
    }
    return end;
}

} // namespace

void draw(ImDrawList *list, Icon icon, ImVec2 min, ImVec2 max, ImU32 color)
{
    const std::array<const char *, SIZE> &rows = bitmap(icon);
    /* Center what is lit, not the 16x16 cell, so a small icon does not sit in a corner. */
    LitBounds b = litBounds(rows);
    if (b.x1 < 0) {
        return;
    }
    const auto k = static_cast<float>(pixel_scale(theme::scale()));
    float originX =
        std::floor((min.x + max.x) / 2.0f - k * static_cast<float>(b.x0 + b.x1 + 1) / 2.0f);
    float originY =
        std::floor((min.y + max.y) / 2.0f - k * static_cast<float>(b.y0 + b.y1 + 1) / 2.0f);
    for (int y = b.y0; y <= b.y1; y++) {
        const char *row = rows[static_cast<size_t>(y)];
        for (int x = b.x0; x <= b.x1; x++) {
            if (row[x] != 'X') {
                continue;
            }
            int end = runEnd(row, x, b.x1);
            list->AddRectFilled(
                ImVec2(originX + k * static_cast<float>(x), originY + k * static_cast<float>(y)),
                ImVec2(originX + k * static_cast<float>(end + 1),
                       originY + k * static_cast<float>(y + 1)),
                color);
            x = end;
        }
    }
}

bool button(Icon icon, const char *id, const char *tooltip, ImVec2 size, bool enabled)
{
    ImGui::BeginDisabled(!enabled);
    bool clicked = ImGui::Button(id, size);
    ImGui::EndDisabled();
    draw(ImGui::GetWindowDrawList(), icon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
         enabled ? withAlpha(theme::INK, 1.0f) : withAlpha(theme::MUTE, 0.45f));
    theme::hint(tooltip, enabled);
    return clicked && enabled;
}

bool choice(Icon icon, const char *id, const char *tooltip, bool selected, ImVec2 size)
{
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, theme::rgb(theme::ACCENT));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::rgb(theme::ACCENT));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::rgb(theme::ACCENT_STRONG));
    }
    bool clicked = ImGui::Button(id, size);
    if (selected) {
        ImGui::PopStyleColor(3);
    }
    draw(ImGui::GetWindowDrawList(), icon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
         withAlpha(selected ? theme::ACCENT_TEXT : theme::INK, 1.0f));
    theme::tip(tooltip);
    return clicked;
}

bool toggle(Icon on, Icon off, const char *id, const char *tooltip, bool state, float size)
{
    bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList *list = ImGui::GetWindowDrawList();
    if (ImGui::IsItemHovered()) {
        list->AddRectFilled(min, max, withAlpha(theme::HOVER, 1.0f), ROUND);
    }
    draw(list, state ? on : off, min, max,
         state ? withAlpha(theme::INK, 1.0f) : withAlpha(theme::MUTE, 0.7f));
    theme::tip(tooltip);
    return clicked;
}

} // namespace icons
