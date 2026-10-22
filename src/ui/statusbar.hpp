#ifndef TINYSPRITE_UI_STATUSBAR_HPP
#define TINYSPRITE_UI_STATUSBAR_HPP

#include <string>

#include "editor.hpp"

/* A short message that confirms an action ("Saved a.pxa") or reports why it failed. One shows at a
 * time: a new message replaces the old one. The times are seconds on any steady clock (the app
 * passes ImGui::GetTime). */
class Toasts
{
  public:
    static constexpr double INFO_SECONDS = 2.5;
    static constexpr double ERROR_SECONDS = 5.0; /* a failure stays long enough to be read */
    static constexpr double FADE_SECONDS = 0.4;
    static constexpr double ENTER_SECONDS = 0.15; /* it fades in while rising into place */

    struct Toast {
        std::string text;
        bool error;
        double start;
    };

    void push(std::string text, bool error, double now);

    /* The message to show at `now`, nullptr once it has expired. */
    const Toast *current(double now) const;

    /* 1 while it is fully shown, fading in over ENTER_SECONDS and out over its last FADE_SECONDS;
     * 0 with no message. */
    float alpha(double now) const;

    /* How far below its place the message still is, 1 as it appears easing to 0 over
     * ENTER_SECONDS; 0 after that and with no message. Multiply by the distance to travel. */
    float rise(double now) const;

    /* Draws the message above the status bar, centered in the main viewport. */
    void draw(double now, float bottomMargin) const;

  private:
    static double lifetime(const Toast &toast);

    bool has_ = false;
    Toast toast_{"", false, 0.0};
};

/* What the status bar can show, each as an item that is switched on or off from the bar's context
 * menu. Selection and Cell only have text while something is selected / the mouse is over a cel. */
enum class StatusItem { Tool, Frame, Layer, Zoom, Selection, Cell, Count };

constexpr unsigned status_item_bit(StatusItem item)
{
    return 1u << static_cast<unsigned>(item);
}
inline constexpr unsigned STATUS_ALL = (1u << static_cast<unsigned>(StatusItem::Count)) - 1;

/* The name of an item in the menu. */
const char *status_item_name(StatusItem item);

/* The text of one item, or an empty string when it has nothing to say now (no selection, the mouse
 * not over the sprite). */
std::string status_item_text(const Editor &editor, StatusItem item);

/* The text of the items in the bit set `items` that have something to say, three spaces apart. */
std::string status_text(const Editor &editor, unsigned items = STATUS_ALL);

/* Height of the bar at the current UI scale. */
float status_bar_height();

/* The bar along the bottom edge of the main viewport, `width` wide from `x`. A right click on an
 * item offers to hide it; a right click anywhere on the bar opens the menu that switches every
 * item and hides the bar (`editor.overlays()` holds the choices). */
void status_bar_show(Editor &editor, float x, float y, float width);

#endif
