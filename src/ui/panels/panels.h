#ifndef TINYSPRITE_UI_PANELS_H
#define TINYSPRITE_UI_PANELS_H

#include <array>
#include <cstddef>

/* The side panels (Tools, Layers): each is docked to the left or right window edge, or floats as a
 * movable window. Docked panels can be resized by their inner edge, and a floating panel docks when
 * its title bar is dragged to an edge. */

enum class PanelId { Tools, Layers, Count };

class Panels
{
  public:
    /* The layout of all panels, in its default state. */
    Panels();

    /* Restores the default layout. */
    void reset();

    /* Assigns the docked panels their positions and returns the width the canvas loses on each
     * side. Call once per frame before begin. */
    void layout(float &left, float &right);

    /* Starts the panel's window. Draw the content when this returns true; always call end.
     * `height` is the height of a docked panel. */
    bool begin(PanelId id, float height);
    void end();

    /* The "Window" menu: show / hide, dock left / right, float, reset layout. Call inside the main
     * menu bar. */
    void windowMenu();

  private:
    enum class DockSide { Left, Right, Float };

    /* A side panel is docked to the left or right edge (taking space from the canvas, side by side
     * with the other panels on that edge) or floating (a movable, resizable, closable window).
     * Dragging a floating panel's title bar to an edge docks it there. Hidden panels give their
     * space away. */
    struct Panel {
        const char *title;
        float width;
        bool visible = true;
        DockSide side;
        DockSide home;      /* where "dock" puts the panel back */
        bool place = false; /* a panel just started floating: put it near where it was docked */
        bool titlePress = false;
        bool resizing = false;
        float dockX = 0.0f; /* left edge while docked, assigned by layout */
    };

    static void dock(Panel &panel, DockSide side);
    static void dragToDock(Panel &panel);
    static void resizeEdge(Panel &panel);
    static void titleButton(Panel &panel);

    std::array<Panel, static_cast<std::size_t>(PanelId::Count)> panels_;
};

#endif
