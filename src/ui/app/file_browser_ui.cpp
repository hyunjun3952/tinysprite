/* App: the folder tree and search of the file dialog (what it lists is in file_browser.hpp). */

#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "app.hpp"
#include "file_browser.hpp"
#include "file_paths.hpp"
#include "imgui.h"
#include "theme.hpp"

namespace
{

constexpr float TREE_ROWS = 12.0f;     /* the height of the list, in text lines */
constexpr int EXPAND_DEPTH = 4;        /* how many levels "Expand all" opens */
constexpr int EXPAND_BUDGET = 400;     /* and how many folders at most */
constexpr int SEARCH_DEPTH = 6;        /* how many folder levels a search goes down */
constexpr size_t SEARCH_RESULTS = 300; /* and how many hits it keeps */

/* Whether `path` is `root` or lies below it. */
bool isUnder(const std::string &path, const std::string &root)
{
    if (path == root) {
        return true;
    }
    const std::string prefix = root.back() == '/' ? root : root + "/";
    return path.compare(0, prefix.size(), prefix) == 0;
}

} // namespace

/* Puts `directory` in front of the file name in the path field (the name stays as typed, and a
 * dialog that reads has none), and starts the search over. */
void App::browseTo(const std::string &directory)
{
    const std::string typed = pathInput_;
    std::error_code ignored;
    const bool hasName = !typed.empty() && typed.back() != '/' && typed.back() != '\\' &&
                         !std::filesystem::is_directory(expand_home(typed), ignored);
    const std::string name = fileInfo(fileAction_).writes() && hasName ? file_name_of(typed) : "";
    std::snprintf(pathInput_, sizeof(pathInput_), "%s", join_path(directory, name).c_str());
    browseSearch_[0] = '\0';
}

/* A file was picked: its path goes in the path field. True when the click was a double click in a
 * dialog that reads, which counts as OK. */
bool App::pickFile(const FileActionInfo &info, const std::string &path)
{
    std::snprintf(pathInput_, sizeof(pathInput_), "%s", path.c_str());
    return !info.writes() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
}

const std::vector<DirEntry> &App::listing(const std::string &directory, const FileActionInfo &info)
{
    auto it = browseCache_.find(directory);
    if (it == browseCache_.end()) {
        it = browseCache_.emplace(directory, list_directory(directory, info.filter)).first;
    }
    return it->second;
}

/* What "Expand all" / "Collapse all" ask of the next folder row: forced open (within a depth and a
 * budget, so a huge folder cannot make the list endless), forced closed, or left as it is. */
static void applyOpenAll(int request, int depth, int &budget)
{
    if (request < 0) {
        ImGui::SetNextItemOpen(false, ImGuiCond_Always);
    } else if (request > 0 && depth < EXPAND_DEPTH && budget > 0) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        budget--;
    }
}

/* One folder row of the tree: its name selects it as the place to save to, its arrow opens it. */
// NOLINTNEXTLINE(misc-no-recursion): the tree draws a folder's rows inside its node, so it recurses
bool App::folderNode(const FileActionInfo &info, const std::string &path, const std::string &name,
                     bool selected, int depth)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_FramePadding;
    if (selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    applyOpenAll(browseOpenAll_, depth, browseOpenBudget_);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(ImGui::GetStyle().FramePadding.x, theme::px(TREE_ROW_PADDING)));
    const bool open =
        theme::tree_node((name + "/").c_str(), flags, ImGui::GetColorU32(ImGuiCol_Text));
    ImGui::PopStyleVar();
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        browseTo(path);
    }
    bool activated = false;
    if (open) {
        activated = treeRows(info, path, depth + 1);
        ImGui::TreePop();
    }
    return activated;
}

/* One file row: a click puts it in the path field. */
bool App::fileRow(const FileActionInfo &info, const std::string &path, const std::string &name,
                  bool selected)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                               ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
    if (selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(ImGui::GetStyle().FramePadding.x, theme::px(TREE_ROW_PADDING)));
    ImGui::TreeNodeEx(name.c_str(), flags);
    ImGui::PopStyleVar();
    return ImGui::IsItemClicked(ImGuiMouseButton_Left) && pickFile(info, path);
}

/* The rows below `directory`: its folders (each can be opened) and the files of the action's kinds.
 */
// NOLINTNEXTLINE(misc-no-recursion): see folderNode
bool App::treeRows(const FileActionInfo &info, const std::string &directory, int depth)
{
    const std::string target = browse_directory(expand_home(pathInput_), startDirectory());
    const std::string current = expand_home(pathInput_);
    bool activated = false;
    /* a copy: opening a node may add to the cache */
    const std::vector<DirEntry> entries = listing(directory, info);
    for (const DirEntry &entry : entries) {
        const std::string path = join_path(directory, entry.name);
        ImGui::PushID(entry.name.c_str());
        if (entry.directory) {
            activated = folderNode(info, path, entry.name, path == target, depth) || activated;
        } else {
            activated = fileRow(info, path, entry.name, path == current) || activated;
        }
        ImGui::PopID();
    }
    if (entries.empty()) {
        ImGui::TextDisabled("Nothing to show here");
    }
    return activated;
}

/* The search results: matches anywhere below the tree's root, with their relative paths. */
bool App::searchRows(const FileActionInfo &info)
{
    const std::string key = browseRoot_ + '\n' + info.filter + '\n' + browseSearch_;
    if (key != browseFoundKey_) {
        browseFound_ =
            search_directory(browseRoot_, info.filter, browseSearch_, SEARCH_DEPTH, SEARCH_RESULTS);
        browseFoundKey_ = key;
    }
    bool activated = false;
    for (const DirEntry &entry : browseFound_) {
        const std::string path = join_path(browseRoot_, entry.name);
        ImGui::PushID(entry.name.c_str());
        if (ImGui::Selectable((entry.name + (entry.directory ? "/" : "")).c_str())) {
            if (entry.directory) {
                browseTo(path);
            } else {
                activated = pickFile(info, path);
            }
        }
        ImGui::PopID();
    }
    if (browseFound_.empty()) {
        ImGui::TextDisabled("Nothing found");
    }
    return activated;
}

/* Asks the next frame of the tree to open (+1) or close (-1) every folder. */
void App::requestOpenAll(int direction)
{
    browseOpenAll_ = direction;
    browseOpenBudget_ = EXPAND_BUDGET;
}

/* The search field with the buttons that open or close the whole tree beside it. */
void App::browseToolbar()
{
    const ImGuiStyle &style = ImGui::GetStyle();
    const float expandWidth = ImGui::CalcTextSize("Expand all").x + style.FramePadding.x * 2.0f;
    const float collapseWidth = ImGui::CalcTextSize("Collapse all").x + style.FramePadding.x * 2.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - expandWidth - collapseWidth -
                            style.ItemSpacing.x * 2.0f);
    ImGui::InputTextWithHint("##search", "Search in this folder and below", browseSearch_,
                             sizeof(browseSearch_));
    ImGui::BeginDisabled(browseSearch_[0] != '\0'); /* the results are a flat list */
    ImGui::SameLine();
    if (ImGui::Button("Expand all")) {
        requestOpenAll(1);
    }
    theme::tip("Open every folder (down to a few levels)");
    ImGui::SameLine();
    if (ImGui::Button("Collapse all")) {
        requestOpenAll(-1);
    }
    theme::tip("Close every folder");
    ImGui::EndDisabled();
}

bool App::fileBrowser(const FileActionInfo &info)
{
    /* The tree keeps its root while folders are picked in it; a path typed or picked elsewhere (or
     * "../") moves it. */
    const std::string target = browse_directory(expand_home(pathInput_), startDirectory());
    if (browseRoot_.empty() || !isUnder(target, browseRoot_)) {
        browseRoot_ = target;
    }
    browseToolbar();
    bool activated = false;
    const ImVec2 size(ImGui::GetContentRegionAvail().x,
                      ImGui::GetTextLineHeightWithSpacing() * TREE_ROWS);
    ImGui::PushStyleColor(ImGuiCol_Header, theme::rgb(theme::SLIDER_FILL));
    if (ImGui::BeginChild("##files", size, ImGuiChildFlags_Borders)) {
        if (browseOpenAll_ < 0) {
            ImGui::GetStateStorage()->Clear(); /* forget the open folders that are not drawn now */
        }
        if (browseSearch_[0] != '\0') {
            activated = searchRows(info);
        } else {
            /* as tall as the rows under it, its text in the middle */
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
            const bool up = ImGui::Selectable(
                "../", false, ImGuiSelectableFlags_None,
                ImVec2(0.0f, ImGui::GetFontSize() + 2.0f * theme::px(TREE_ROW_PADDING)));
            ImGui::PopStyleVar();
            if (up) {
                browseRoot_ = parent_directory(browseRoot_);
                browseTo(browseRoot_);
            }
            activated = treeRows(info, browseRoot_, 0);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    browseOpenAll_ = 0;
    return activated;
}
