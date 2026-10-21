#ifndef TINYSPRITE_UI_PANELS_H
#define TINYSPRITE_UI_PANELS_H

#include <array>
#include <cstddef>
#include <vector>

/* The side panels (Tools, Layers): each is docked to the left or right window edge, or floats as a
 * movable window. Docked panels can be resized by their inner edge and are pulled out by dragging
 * their title bar; a floating panel docks again when its title bar is dragged to an edge. */

enum class PanelId { Tools, Layers, Count };

/* The collapsible sections of each panel. The panels draw a section only while it is visible, and
 * the Window menu lists them, so the names here are the single source for both. */
namespace sections
{
inline constexpr const char *TOOLS = "Tools";
inline constexpr const char *SHAPES = "Shapes";
inline constexpr const char *TOOL_OPTIONS = "Tool options";
inline constexpr const char *SYMMETRY = "Symmetry";
inline constexpr const char *SELECTION = "Selection";
inline constexpr const char *COLOR = "Color";
inline constexpr const char *RECENT = "Recent";
inline constexpr const char *LAYERS = "Layers";
} // namespace sections

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

    /* Starts the panel's window. Draw the content when this returns true; always call end (it
     * ends the window only if one was started: a hidden panel has none). `height` is the height of
     * a docked panel. */
    bool begin(PanelId id, float height);
    void end();

    /* Shows or hides a panel; a hidden panel gives its space away. */
    bool visible(PanelId id) const;
    void setVisible(PanelId id, bool visible);

    /* Whether the panel draws its section `name` (see namespace sections); true for a name it does
     * not have. Hiding every section leaves an empty panel, which is the user's choice. */
    bool sectionVisible(PanelId id, const char *name) const;
    void setSectionVisible(PanelId id, const char *name, bool visible);

    /* True while the panel is a free-floating window (not docked to an edge). */
    bool floating(PanelId id) const;

    /* The "Window" menu: show / hide, dock left / right, float, reset layout. Call inside the main
     * menu bar. */
    void windowMenu();

  private:
    enum class DockSide { Left, Right, Float };

    struct Section {
        const char *name;
        bool visible = true;
    };

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
        bool titlePress = false; /* the left button went down on the title bar and is still held */
        bool moving = false;     /* just pulled out of its dock: follows the mouse until release */
        float grabX = 0.0f;      /* where in the title bar the mouse holds the panel */
        float grabY = 0.0f;
        bool resizing = false;
        float dockX = 0.0f; /* left edge while docked, assigned by layout */
        std::vector<Section> sections;
    };

    static void dock(Panel &panel, DockSide side);
    static void dragToDock(Panel &panel);
    static void resizeEdge(Panel &panel);
    static bool titleHovered();
    static void undockOnDrag(Panel &panel);

    std::array<Panel, static_cast<std::size_t>(PanelId::Count)> panels_;
    bool windowOpen_ = false; /* begin called ImGui::Begin, so end must call ImGui::End */
};

#endif
