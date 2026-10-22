#ifndef TINYSPRITE_UI_WINDOW_SIZE_HPP
#define TINYSPRITE_UI_WINDOW_SIZE_HPP

/* A window size in pixels. */
struct WindowSize {
    int width;
    int height;
};

/* What the title bar and borders take of the usable area (the window size excludes them). */
inline constexpr int WINDOW_FRAME_MARGIN = 40;

/* `wanted`, shrunk so a window with its frame fits inside `usable` (the display area a window may
 * cover, e.g. SDL_GetDisplayUsableBounds). An unknown area (0 x 0) leaves it unchanged. */
WindowSize window_fit(WindowSize wanted, WindowSize usable);

/* The smallest window the layout stays usable in (tool bar, canvas, inspector and the status bar
 * all visible), at UI scale `scale`. */
WindowSize window_minimum(float scale);

#endif
