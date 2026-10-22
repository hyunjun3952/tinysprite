#include "slices_panel.hpp"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <string>

#include "imgui.h"
#include "theme.hpp"

namespace
{

constexpr float LIST_ROWS = 5.0f;          /* rows of the list shown before it scrolls */
constexpr float FIELD_NAME_COLUMN = 64.0f; /* the label column of the details, at scale 1 */

/* A numeric field of the details; true when the value was entered (Enter, a step button or the
 * field was left after typing). */
bool intField(const char *id, int *value, const char *unit)
{
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool entered =
        theme::input_int(id, value, 1, 8, unit, ImGuiInputTextFlags_EnterReturnsTrue);
    return entered || ImGui::IsItemDeactivatedAfterEdit();
}

void label(const char *text)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", text);
    ImGui::TableSetColumnIndex(1);
}

} // namespace

Rect slice_rect_fit(Rect wanted, int spriteWidth, int spriteHeight)
{
    wanted.w = std::clamp(wanted.w, 1, spriteWidth);
    wanted.h = std::clamp(wanted.h, 1, spriteHeight);
    wanted.x = std::clamp(wanted.x, 0, spriteWidth - wanted.w);
    wanted.y = std::clamp(wanted.y, 0, spriteHeight - wanted.h);
    return wanted;
}

void slice_pivot_fit(Sprite::Slice &slice)
{
    if (slice.hasPivot) {
        slice.pivotX = std::clamp(slice.pivotX, 0, slice.rect.w);
        slice.pivotY = std::clamp(slice.pivotY, 0, slice.rect.h);
    }
}

void slice_pivot_center(Sprite::Slice &slice)
{
    slice.hasPivot = true;
    slice.pivotX = slice.rect.w / 2;
    slice.pivotY = slice.rect.h / 2;
}

SlicesPanel::SlicesPanel(Editor &editor) : editor_(&editor)
{
}

void SlicesPanel::setEditor(Editor &editor)
{
    editor_ = &editor;
    nameActive_ = false;
}

void SlicesPanel::show()
{
    /* Slices cannot change under a tool that is mid-drag. */
    ImGui::BeginDisabled(editor_->stroking());
    buttons();
    list();
    const int selected = editor_->selectedSlice();
    if (selected >= 0) {
        details(selected);
    }
    ImGui::EndDisabled();
}

void SlicesPanel::buttons()
{
    const float width = theme::columnWidth(2);
    if (ImGui::Button("New", ImVec2(width, 0.0f))) {
        /* over the selection when there is one, else over the whole sprite */
        editor_->addSlice(editor_->hasSelection()
                              ? editor_->selection()
                              : Rect{0, 0, editor_->sprite().width(), editor_->sprite().height()});
    }
    theme::tip(
        "A new slice over the selection, or the whole sprite. The Slice tool (K) drags one out "
        "on the canvas.");
    ImGui::SameLine();
    const bool any = editor_->selectedSlice() >= 0;
    if (theme::actionButton("Delete", "Remove the selected slice.", width, any)) {
        editor_->removeSlice(editor_->selectedSlice());
    }
}

void SlicesPanel::list()
{
    const std::vector<Sprite::Slice> &slices = editor_->sprite().slices();
    if (slices.empty()) {
        ImGui::TextDisabled("No slices yet.");
        return;
    }
    const float rows = std::min(static_cast<float>(slices.size()), LIST_ROWS);
    const ImVec2 size(0.0f, ImGui::GetTextLineHeightWithSpacing() * rows +
                                ImGui::GetStyle().WindowPadding.y);
    if (!ImGui::BeginChild("##slice_list", size, ImGuiChildFlags_Borders)) {
        ImGui::EndChild();
        return;
    }
    for (int i = 0; i < static_cast<int>(slices.size()); i++) {
        const Sprite::Slice &slice = slices[static_cast<size_t>(i)];
        ImGui::PushID(i);
        if (ImGui::Selectable(slice.name.c_str(), i == editor_->selectedSlice())) {
            editor_->selectSlice(i);
        }
        char text[32];
        std::snprintf(text, sizeof(text), "%d\xC3\x97%d", slice.rect.w, slice.rect.h);
        const float textWidth = ImGui::CalcTextSize(text).x;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - textWidth);
        ImGui::TextDisabled("%s", text);
        ImGui::PopID();
    }
    ImGui::EndChild();
}

bool SlicesPanel::nameField(Sprite::Slice &slice)
{
    if (!nameActive_) {
        std::snprintf(name_, sizeof(name_), "%s", slice.name.c_str());
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##slice_name", name_, sizeof(name_));
    nameActive_ = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit() && name_[0] != '\0') {
        slice.name = name_;
        return true;
    }
    return false;
}

bool SlicesPanel::rectFields(Sprite::Slice &slice)
{
    bool changed = false;
    label("X");
    changed = intField("##slice_x", &slice.rect.x, " px") || changed;
    label("Y");
    changed = intField("##slice_y", &slice.rect.y, " px") || changed;
    label("Width");
    changed = intField("##slice_w", &slice.rect.w, " px") || changed;
    label("Height");
    changed = intField("##slice_h", &slice.rect.h, " px") || changed;
    return changed;
}

bool SlicesPanel::pivotFields(Sprite::Slice &slice)
{
    bool changed = false;
    label("Pivot");
    bool has = slice.hasPivot;
    if (ImGui::Checkbox("##slice_pivot", &has)) {
        if (has) {
            slice_pivot_center(slice);
        } else {
            slice.hasPivot = false;
        }
        changed = true;
    }
    theme::tip("A point in the slice (from its top-left cel) a game can place or turn the sprite "
               "around.");
    if (slice.hasPivot) {
        label("Pivot X");
        changed = intField("##slice_px", &slice.pivotX, " px") || changed;
        label("Pivot Y");
        changed = intField("##slice_py", &slice.pivotY, " px") || changed;
    }
    return changed;
}

void SlicesPanel::details(int index)
{
    Sprite::Slice edit = editor_->sprite().slices()[static_cast<size_t>(index)];
    if (!ImGui::BeginTable("##slice_details", 2, ImGuiTableFlags_SizingFixedFit)) {
        return;
    }
    ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthFixed,
                            theme::px(FIELD_NAME_COLUMN));
    ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);
    label("Name");
    bool changed = nameField(edit);
    changed = rectFields(edit) || changed;
    changed = pivotFields(edit) || changed;
    ImGui::EndTable();
    if (changed) {
        edit.rect =
            slice_rect_fit(edit.rect, editor_->sprite().width(), editor_->sprite().height());
        slice_pivot_fit(edit);
        editor_->updateSlice(index, edit);
    }
}
