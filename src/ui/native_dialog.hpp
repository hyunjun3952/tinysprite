#ifndef TINYSPRITE_UI_NATIVE_DIALOG_HPP
#define TINYSPRITE_UI_NATIVE_DIALOG_HPP

#include <SDL3/SDL.h>

#include <optional>
#include <string>

/*
 * The operating system's own window for choosing a file, opened from the path dialog beside the
 * folder tree. The system answers later and perhaps from another thread, so the answer is kept
 * here until a frame of the dialog takes it. At most one window is open at a time.
 */

/* What the window is for: choosing a file that exists, or a name to save to. */
enum class NativeDialogKind {
    Open,
    Save,
};

/*
 * The filter text SDL wants ("png;gif") for the extensions a dialog lists (".png .gif", separated
 * by spaces). An entry that is empty or has a character other than a letter, a digit, '_' or '-'
 * is left out, as the system reads ';' and '*' in it as something else. "" when none is left,
 * which means no filter.
 */
std::string native_dialog_patterns(const std::string &extensions);

/*
 * Reserves the one window slot and keeps `startPath` and the filter for `extensions` alive until
 * the window answers. Returns false, changing nothing, if a window is open already. This is the
 * first step of native_dialog_show, apart so that the slot can be looked at without a display.
 */
bool native_dialog_claim(const std::string &startPath, const std::string &extensions);

/*
 * Opens the window over `window`, starting at `startPath` (a folder or a file; "" for the
 * system's choice) and listing only files with one of `extensions` (see native_dialog_patterns;
 * "" lists all). Returns false if a window is open already. Main thread only. The answer comes
 * through native_dialog_take.
 */
bool native_dialog_show(SDL_Window *window, NativeDialogKind kind, const std::string &startPath,
                        const std::string &extensions);

/* Whether a window opened by native_dialog_show has not answered yet. */
bool native_dialog_open();

/* The file the user chose, once; nothing when the window was closed, failed, or was answered
 * already. */
std::optional<std::string> native_dialog_take();

/*
 * The system's answer: `files` is a list of paths ending in a null pointer, empty when the user
 * closed the window, or null when the system could not show it. Only the first path is kept (one
 * file is asked for). Safe to call from any thread; it is what the SDL callback does.
 */
void native_dialog_finish(const char *const *files);

#endif
