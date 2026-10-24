/* App: the tab bar over the canvas, one tab per open document. */

#include <string>
#include <vector>

#include "app.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "theme.hpp"

namespace
{

constexpr float TAB_PADDING_X = 6.0f; /* the bar's own padding */
constexpr float TAB_PADDING_Y = 3.0f;

} // namespace

float App::tabBarHeight()
{
    return ImGui::GetFontSize() + theme::px(TAB_PADDING_Y) * 2.0f +
           ImGui::GetStyle().FramePadding.y * 2.0f;
}

/* The menu of a tab: close it, the others, or the ones after it. */
void App::tabMenu(int index, int &closeRequest)
{
    if (!ImGui::BeginPopupContextItem("##tab_menu")) {
        return;
    }
    if (ImGui::MenuItem("Close")) {
        closeRequest = index;
    }
    ImGui::Separator();
    ImGui::BeginDisabled(docs_.size() < 2);
    if (ImGui::MenuItem("Split right")) {
        splitView(index == active_ ? -1 : index, true);
    }
    if (ImGui::MenuItem("Split down")) {
        splitView(index == active_ ? -1 : index, false);
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::BeginDisabled(docs_.size() < 2);
    if (ImGui::MenuItem("Close others")) {
        closeOthers_ = doc(index).editor.get();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(index + 1 >= static_cast<int>(docs_.size()));
    if (ImGui::MenuItem("Close to the right")) {
        closeRightOf_ = doc(index).editor.get();
    }
    ImGui::EndDisabled();
    ImGui::EndPopup();
}

/* Dragging a tab reorders ImGui's own list of tabs; this brings the documents into the same order.
 * `ids` are the tabs' ids in the documents' order. One move is applied per frame. */
void App::syncTabOrder(const std::vector<ImGuiID> &ids)
{
    const ImGuiTabBar *bar = ImGui::GetCurrentTabBar();
    if (bar == nullptr) {
        return;
    }
    std::vector<int> shown; /* the document of each tab, in the order the tab bar shows them */
    for (const ImGuiTabItem &tab : bar->Tabs) {
        for (size_t i = 0; i < ids.size(); i++) {
            if (ids[i] == tab.ID) {
                shown.push_back(static_cast<int>(i));
            }
        }
    }
    for (size_t k = 0; k < shown.size(); k++) {
        if (shown[k] != static_cast<int>(k)) {
            moveDocument(shown[k], static_cast<int>(k));
            return;
        }
    }
}

/* One tab. A tab that was picked is returned in `picked`; the close button and the menu set
 * `closeRequest`. */
void App::tabItem(int index, bool forced, int &picked, int &closeRequest, ImGuiID &id)
{
    const Document &document = doc(index);
    ImGuiTabItemFlags flags = ImGuiTabItemFlags_NoPushId;
    if (dirty(document)) {
        flags |= ImGuiTabItemFlags_UnsavedDocument;
    }
    if (forced && index == active_) {
        flags |= ImGuiTabItemFlags_SetSelected;
    }
    bool open = true;
    /* the id after ### is the document's own number, so a tab keeps its identity when others go */
    const std::string label = documentTitle(document) + "###doc" + std::to_string(document.serial);
    if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
        if (!forced && index != active_) {
            picked = index;
        }
        ImGui::EndTabItem();
    }
    id = ImGui::GetItemID();
    if (ImGui::IsItemHovered() && !document.path.empty()) {
        ImGui::SetTooltip("%s", document.path.c_str());
    }
    tabMenu(index, closeRequest);
    if (!open) {
        closeRequest = index;
    }
}

void App::tabBar(float x, float y, float width, float height)
{
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    constexpr ImGuiWindowFlags FLAGS =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(theme::px(TAB_PADDING_X), theme::px(TAB_PADDING_Y)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::rgb(theme::CANVAS));
    int picked = -1;
    int closeRequest = -1;
    /* a tab chosen by the app (a shortcut, a file opened) is told to ImGui; the tab bar's own
     * answer is only believed when the app did not just change it */
    const bool forced = tabSelected_ != active_;
    if (ImGui::Begin("##tabs", nullptr, FLAGS)) {
        if (ImGui::BeginTabBar("##doc_tabs", ImGuiTabBarFlags_FittingPolicyResizeDown |
                                                 ImGuiTabBarFlags_Reorderable)) {
            std::vector<ImGuiID> ids(docs_.size(), 0);
            for (int i = 0; i < static_cast<int>(docs_.size()); i++) {
                tabItem(i, forced, picked, closeRequest, ids[static_cast<size_t>(i)]);
            }
            syncTabOrder(ids);
            if (ImGui::TabItemButton("+",
                                     ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
                openNew_ = true;
            }
            theme::tip("New document");
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (picked >= 0) {
        activateDocument(picked);
    }
    tabSelected_ = active_;
    if (closeRequest >= 0) {
        closeDocument(closeRequest);
    }
    closeMarked();
}

/* "Close others" / "Close to the right" from a tab's menu: every other tab (or those after it) is
 * closed, asking about unsaved changes tab by tab. */
void App::closeMarked()
{
    if (closeOthers_ == nullptr && closeRightOf_ == nullptr) {
        return;
    }
    const Editor *keep = closeOthers_ != nullptr ? closeOthers_ : closeRightOf_;
    const bool rightOnly = closeOthers_ == nullptr;
    closeOthers_ = nullptr;
    closeRightOf_ = nullptr;
    closeAfter(keep, rightOnly);
}

/* The index of the first tab that "Close others" / "Close to the right" would close, or -1. */
int App::nextToClose(const Editor *keep, bool rightOnly) const
{
    bool seenKeep = false;
    for (int i = 0; i < static_cast<int>(docs_.size()); i++) {
        if (doc(i).editor.get() == keep) {
            seenKeep = true;
        } else if (!rightOnly || seenKeep) {
            return i;
        }
    }
    return -1;
}

/* Closes the tabs that go. A tab with unsaved changes asks through the dialog; once it is answered
 * this carries on with the rest (Cancel stops it). */
void App::closeAfter(const Editor *keep, bool rightOnly)
{
    for (int index = nextToClose(keep, rightOnly); index >= 0;
         index = nextToClose(keep, rightOnly)) {
        if (dirty(doc(index))) {
            activateDocument(index);
            const Editor *closing = doc(index).editor.get();
            guardUnsaved([this, closing, keep, rightOnly] {
                removeEditor(closing);
                closeAfter(keep, rightOnly);
            });
            return;
        }
        removeDocument(index);
    }
}
