#include "icons.h"

#include <algorithm>
#include <cmath>

#include "theme.h"

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
}};

constexpr float ROUND = 3.0f;

ImU32 withAlpha(unsigned hex, float alpha)
{
    return ImGui::ColorConvertFloat4ToU32(theme::rgb(hex, alpha));
}

} // namespace

const std::array<const char *, SIZE> &bitmap(Icon icon)
{
    return BITMAPS[static_cast<size_t>(icon)];
}

void draw(ImDrawList *list, Icon icon, ImVec2 min, ImVec2 max, ImU32 color)
{
    const std::array<const char *, SIZE> &rows = bitmap(icon);
    /* Center what is lit, not the 16x16 cell, so a small icon does not sit in a corner. */
    int x0 = SIZE;
    int x1 = -1;
    int y0 = SIZE;
    int y1 = -1;
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            if (rows[static_cast<size_t>(y)][x] == 'X') {
                x0 = std::min(x0, x);
                x1 = std::max(x1, x);
                y0 = std::min(y0, y);
                y1 = std::max(y1, y);
            }
        }
    }
    if (x1 < 0) {
        return;
    }
    float originX = std::floor((min.x + max.x) / 2.0f - static_cast<float>(x0 + x1 + 1) / 2.0f);
    float originY = std::floor((min.y + max.y) / 2.0f - static_cast<float>(y0 + y1 + 1) / 2.0f);
    for (int y = y0; y <= y1; y++) {
        const char *row = rows[static_cast<size_t>(y)];
        for (int x = x0; x <= x1; x++) {
            if (row[x] != 'X') {
                continue;
            }
            int end = x;
            while (end + 1 <= x1 && row[end + 1] == 'X') {
                end++;
            }
            list->AddRectFilled(
                ImVec2(originX + static_cast<float>(x), originY + static_cast<float>(y)),
                ImVec2(originX + static_cast<float>(end + 1), originY + static_cast<float>(y + 1)),
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
