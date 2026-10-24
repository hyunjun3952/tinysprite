/* App: the Filter dialog (outline, replace color, hue / brightness). */

#include "app.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "imgui.h"
#include "theme.hpp"

namespace
{

uint8_t toByte(float v)
{
    return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
}

Rgba toRgba(const float *c)
{
    return Rgba{toByte(c[0]), toByte(c[1]), toByte(c[2]), toByte(c[3])};
}

void copyFloats(float *dst, const float *src)
{
    std::copy(src, src + 4, dst);
}

const char *title(FilterParams::Kind kind)
{
    switch (kind) {
    case FilterParams::Kind::Outline:
        return "Outline";
    case FilterParams::Kind::ReplaceColor:
        return "Replace color";
    case FilterParams::Kind::HueBrightness:
        return "Hue / Brightness";
    }
    return "Filter";
}

void colorRow(const char *label, const char *id, float *color, char (&hex)[theme::HEX_BUFFER_SIZE])
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(theme::px(80.0f));
    theme::hex_field(id, color, hex);
}

} // namespace

/* Starts the dialog for `kind` with the foreground color as the outline color and the color to
 * replace, and the background color as the one to replace it with. */
void App::openFilter(FilterParams::Kind kind)
{
    filter_.kind = kind;
    copyFloats(filterColors_[0], editor_->foregroundFloats());
    copyFloats(filterColors_[1], editor_->foregroundFloats());
    copyFloats(filterColors_[2], editor_->backgroundFloats());
    openFilter_ = true;
}

void App::filterFields()
{
    switch (filter_.kind) {
    case FilterParams::Kind::Outline:
        colorRow("Color", "outline_hex", filterColors_[0], filterHex_[0]);
        ImGui::Checkbox("Around the corners too", &filter_.diagonals);
        theme::tip("Also outlines diagonal neighbours, for a thicker corner.");
        break;
    case FilterParams::Kind::ReplaceColor:
        colorRow("Replace", "from_hex", filterColors_[1], filterHex_[1]);
        colorRow("With", "to_hex", filterColors_[2], filterHex_[2]);
        break;
    case FilterParams::Kind::HueBrightness:
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Hue");
        ImGui::SameLine(theme::px(80.0f));
        theme::slider_int("##filter_hue", &filter_.hue, -180, 180, " deg");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Brightness");
        ImGui::SameLine(theme::px(80.0f));
        theme::slider_int("##filter_brightness", &filter_.brightness, -100, 100, "%");
        break;
    }
    ImGui::Checkbox("All frames", &filterAllFrames_);
    theme::tip("Off: only the current frame. Linked cels are filtered once.");
}

/* Applies the dialog's filter; true when it did something (the dialog then closes). */
bool App::runFilter()
{
    FilterParams params = filter_;
    params.color = toRgba(filterColors_[0]);
    params.from = toRgba(filterColors_[1]);
    params.to = toRgba(filterColors_[2]);
    if (!editor_->applyFilter(params, filterAllFrames_)) {
        toast("Nothing to filter: the layers are hidden or locked.", true);
        return false;
    }
    return true;
}

void App::filterDialog()
{
    if (openFilter_) {
        ImGui::OpenPopup("Filter");
        openFilter_ = false;
    }
    filterShown_ = theme::begin_modal("Filter");
    if (!filterShown_) {
        return;
    }
    /* Enter in a hex field commits its text; it must not also press Apply */
    const bool typing = ImGui::GetIO().WantTextInput;
    ImGui::TextUnformatted(title(filter_.kind));
    ImGui::Spacing();
    filterFields();
    ImGui::TextDisabled(editor_->hasSelection() ? "Inside the selection." : "The whole cel.");
    bool close = false;
    const int clicked = theme::dialog_buttons({"Cancel", "Apply"}, 1);
    if ((clicked == 1 || (!typing && ImGui::IsKeyPressed(ImGuiKey_Enter))) && runFilter()) {
        close = true;
    }
    if (clicked == 0 || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close = true;
    }
    if (close) {
        ImGui::CloseCurrentPopup();
    }
    theme::end_modal();
}
