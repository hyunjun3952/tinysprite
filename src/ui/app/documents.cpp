/* App: the open documents (tabs): adding, showing and closing them. */

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

#include "app.hpp"
#include "file_paths.hpp"
#include "recovery.hpp"
#include "tinysprite/io.hpp"

namespace
{

/* True if both paths name the same file however they are spelled: relative or absolute, with "..",
 * through a symbolic link, or (on Windows) in another case. A file that does not exist yet is
 * compared by its normalized path. */
bool sameFile(const std::string &a, const std::string &b)
{
    namespace fs = std::filesystem;
    std::error_code error;
    /* the names are UTF-8, which a narrow string is not taken for on Windows */
    const fs::path pathA = path_from_utf8(a);
    const fs::path pathB = path_from_utf8(b);
    if (a == b || fs::equivalent(pathA, pathB, error)) {
        return true;
    }
    fs::path first = fs::weakly_canonical(pathA, error);
    fs::path second = error ? fs::path() : fs::weakly_canonical(pathB, error);
    return !error && first == second;
}

} // namespace

bool App::dirty(const Document &document) const
{
    return !document.editor->history().atSavePoint() || document.editor->modifiedOutsideHistory();
}

bool App::dirty() const
{
    return dirty(doc());
}

bool App::pristine(const Document &document) const
{
    return document.path.empty() && !dirty(document) && !document.editor->history().canUndo() &&
           !document.editor->history().canRedo();
}

/* "name.pxa", or "untitled" / "untitled 2" ... for a document that has no file yet. */
std::string App::documentTitle(const Document &document) const
{
    if (!document.path.empty()) {
        return file_name_of(document.path);
    }
    return document.serial <= 1 ? "untitled" : "untitled " + std::to_string(document.serial);
}

int App::findDocument(const std::string &path) const
{
    if (path.empty()) {
        return -1;
    }
    auto found = std::find_if(docs_.begin(), docs_.end(), [&path](const Document &d) {
        return !d.path.empty() && sameFile(d.path, path);
    });
    return found == docs_.end() ? -1 : static_cast<int>(found - docs_.begin());
}

void App::activateDocument(int index)
{
    if (index < 0 || index >= static_cast<int>(docs_.size()) || (editor_ && editor_->stroking())) {
        return;
    }
    Editor *next = docs_[static_cast<size_t>(index)].editor.get();
    Editor *previous = editor_;
    active_ = index;
    editor_ = next;
    if (split_) {
        /* a document that is in the other pane just takes the focus; any other fills this pane */
        const int other = 1 - activePane_;
        if (pane_[other] == next) {
            activePane_ = other;
        } else {
            pane_[activePane_] = next;
        }
    } else {
        pane_[0] = next;
    }
    if (previous == next) {
        return;
    }
    if (previous) {
        export_.tag = -1; /* an index into the other document's tags means nothing here */
        previous->stopPlayback();
        previous->tempPick(false); /* a middle-button pick must not stay stuck in the old tab */
        next->adoptPreferences(*previous);
    }
    bindPanes();
}

bool App::addDocument(Sprite sprite, const std::string &path, bool reuse)
{
    if (reuse && pristine(doc())) {
        if (!editor_->setSprite(std::move(sprite))) {
            return false;
        }
        filePath() = path;
        return true;
    }
    std::unique_ptr<Editor> editor = Editor::create(renderer_, sprite.width(), sprite.height());
    if (!editor || !editor->setSprite(std::move(sprite))) {
        return false;
    }
    docs_.push_back({std::move(editor), path, nextSerial_++});
    activateDocument(static_cast<int>(docs_.size()) - 1);
    return true;
}

bool App::addBlankDocument(int width, int height)
{
    std::unique_ptr<Editor> editor = Editor::create(renderer_, width, height);
    if (!editor) {
        return false;
    }
    docs_.push_back({std::move(editor), "", nextSerial_++});
    activateDocument(static_cast<int>(docs_.size()) - 1);
    return true;
}

void App::moveDocument(int from, int to)
{
    const int count = static_cast<int>(docs_.size());
    if (from < 0 || from >= count || to < 0 || to >= count || from == to) {
        return;
    }
    const Editor *shown = editor_;
    Document moved = std::move(docs_[static_cast<size_t>(from)]);
    docs_.erase(docs_.begin() + from);
    docs_.insert(docs_.begin() + to, std::move(moved));
    for (int i = 0; i < count; i++) {
        if (docs_[static_cast<size_t>(i)].editor.get() == shown) {
            active_ = i;
        }
    }
}

void App::removeDocument(int index)
{
    if (index < 0 || index >= static_cast<int>(docs_.size()) ||
        (index == active_ && editor_->stroking())) {
        return; /* the active document cannot be left while a tool is being dragged */
    }
    const Editor *going = docs_[static_cast<size_t>(index)].editor.get();
    if (split_ && (going == pane_[0] || going == pane_[1])) {
        unsplit(); /* a pane would be left without its document */
    }
    if (docs_.size() == 1) {
        if (!addBlankDocument(newWidth_, newHeight_)) { /* the app always has a document */
            toast("Not enough memory to close this document", true);
            return;
        }
        index = 0;
    }
    if (index == active_) {
        activateDocument(index + 1 < static_cast<int>(docs_.size()) ? index + 1 : index - 1);
    }
    if (docs_[static_cast<size_t>(index)].recovery) {
        recovery_clear_slot(configDir_, docs_[static_cast<size_t>(index)].serial);
    }
    retired_.push_back(std::move(docs_[static_cast<size_t>(index)].editor));
    docs_.erase(docs_.begin() + index);
    if (index < active_) {
        active_--;
    }
}

int App::indexOf(const Editor *editor) const
{
    for (int i = 0; i < static_cast<int>(docs_.size()); i++) {
        if (docs_[static_cast<size_t>(i)].editor.get() == editor) {
            return i;
        }
    }
    return -1;
}

void App::removeEditor(const Editor *editor)
{
    removeDocument(indexOf(editor));
}

void App::closeDocument(int index)
{
    if (index < 0 || index >= static_cast<int>(docs_.size()) || editor_->stroking()) {
        return;
    }
    if (!dirty(docs_[static_cast<size_t>(index)])) {
        removeDocument(index);
        return;
    }
    /* the question is about the active document, so show the one being closed first */
    activateDocument(index);
    const Editor *closing = docs_[static_cast<size_t>(index)].editor.get();
    guardUnsaved([this, closing] { removeEditor(closing); });
}

void App::quit()
{
    if (editor_->stroking()) {
        editor_->cancelDrag(); /* leaving is not refused because a tool is being dragged */
    }
    auto unsaved =
        std::find_if(docs_.begin(), docs_.end(), [this](const Document &d) { return dirty(d); });
    if (unsaved == docs_.end()) {
        running_ = false;
        return;
    }
    const int index = static_cast<int>(unsaved - docs_.begin());
    activateDocument(index);
    const Editor *closing = docs_[static_cast<size_t>(index)].editor.get();
    /* each document with unsaved changes asks once; Cancel anywhere stops the quit */
    guardUnsaved([this, closing] {
        removeEditor(closing);
        quit();
    });
}
