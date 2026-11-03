#include "native_dialog.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <sstream>
#include <utility>

namespace
{

/* What the window needs to stay alive until it answers, and the answer. */
struct State {
    std::mutex lock;
    std::optional<std::string> answer; /* the chosen file, until taken */
    std::string pattern;               /* the text filter.pattern points at */
    std::string start;                 /* the text the start location points at */
    SDL_DialogFileFilter filter = {};  /* the one filter SDL is given */
    bool open = false;                 /* a window was opened and has not answered */
};

State &state()
{
    static State instance;
    return instance;
}

/* Whether `entry` is a plain extension: letters, digits, '_' and '-'. */
bool plain_extension(const std::string &entry)
{
    return !entry.empty() && std::all_of(entry.begin(), entry.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-';
    });
}

void SDLCALL on_choice(void * /*userdata*/, const char *const *files, int /*filter*/)
{
    native_dialog_finish(files);
}

} // namespace

std::string native_dialog_patterns(const std::string &extensions)
{
    std::istringstream words(extensions);
    std::string word;
    std::string patterns;
    while (words >> word) {
        const std::string entry = word.front() == '.' ? word.substr(1) : word;
        if (plain_extension(entry)) {
            patterns += (patterns.empty() ? "" : ";") + entry;
        }
    }
    return patterns;
}

bool native_dialog_claim(const std::string &startPath, const std::string &extensions)
{
    State &s = state();
    const std::lock_guard<std::mutex> guard(s.lock);
    if (s.open) {
        return false;
    }
    s.open = true;
    s.pattern = native_dialog_patterns(extensions);
    s.start = startPath;
    s.filter = SDL_DialogFileFilter{"Files", s.pattern.c_str()};
    return true;
}

bool native_dialog_show(SDL_Window *window, NativeDialogKind kind, const std::string &startPath,
                        const std::string &extensions)
{
    if (!native_dialog_claim(startPath, extensions)) {
        return false;
    }
    /* Not under the lock: SDL answers at once, on this thread, when it cannot show the window. */
    State &s = state();
    const SDL_DialogFileFilter *filters = s.pattern.empty() ? nullptr : &s.filter;
    const int count = s.pattern.empty() ? 0 : 1;
    const char *location = s.start.empty() ? nullptr : s.start.c_str();
    if (kind == NativeDialogKind::Save) {
        SDL_ShowSaveFileDialog(on_choice, nullptr, window, filters, count, location);
    } else {
        SDL_ShowOpenFileDialog(on_choice, nullptr, window, filters, count, location, false);
    }
    return true;
}

bool native_dialog_open()
{
    State &s = state();
    const std::lock_guard<std::mutex> guard(s.lock);
    return s.open;
}

std::optional<std::string> native_dialog_take()
{
    State &s = state();
    const std::lock_guard<std::mutex> guard(s.lock);
    return std::exchange(s.answer, std::nullopt);
}

void native_dialog_finish(const char *const *files)
{
    State &s = state();
    const std::lock_guard<std::mutex> guard(s.lock);
    s.open = false;
    if (files != nullptr && files[0] != nullptr) {
        s.answer = std::string(files[0]);
    }
}
