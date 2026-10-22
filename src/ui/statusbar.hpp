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

    struct Toast {
        std::string text;
        bool error;
        double start;
    };

    void push(std::string text, bool error, double now);

    /* The message to show at `now`, nullptr once it has expired. */
    const Toast *current(double now) const;

    /* 1 while it is fully shown, fading to 0 over its last FADE_SECONDS; 0 with no message. */
    float alpha(double now) const;

    /* Draws the message above the status bar, centered in the main viewport. */
    void draw(double now, float bottomMargin) const;

  private:
    static double lifetime(const Toast &toast);

    bool has_ = false;
    Toast toast_{"", false, 0.0};
};

/* One line for the status bar: sprite size, tool, frame, layer, zoom, the selection size when there
 * is one and the cel under the mouse. */
std::string status_text(const Editor &editor);

/* Height of the bar at the current UI scale. */
float status_bar_height();

/* The bar along the bottom edge of the main viewport, `width` wide from `x`. */
void status_bar_show(const Editor &editor, float x, float y, float width);

#endif
