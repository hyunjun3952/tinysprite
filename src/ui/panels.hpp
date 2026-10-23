#ifndef TINYSPRITE_UI_PANELS_HPP
#define TINYSPRITE_UI_PANELS_HPP

#include <array>
#include <cstddef>
#include <vector>

#include "settings.hpp"

/* The side panels (Tools: the narrow tool bar; Layers: the right panel, titled "Inspector", with
 * the color, the layers and their properties): each is docked to the left or right window edge, or
 * floats as a movable window. Docked panels can be resized by their inner edge and are pulled out
 * by dragging their title bar; a floating panel docks again when its title bar is dragged to an
 * edge. */

enum class PanelId { Tools, Layers, Count };

/* The collapsible sections of each panel. The panels draw a section only while it is visible, and
 * the Window menu lists them, so the names here are the single source for both. */
namespace sections
{
inline constexpr const char *TOOLS = "Tools";
inline constexpr const char *SHAPES = "Shapes";
inline constexpr const char *COLOR = "Color";
inline constexpr const char *LAYERS = "Layers";
inline constexpr const char *SLICES = "Slices";
inline constexpr const char *TILEMAPS = "Tilemaps";
inline constexpr const char *PROPERTIES = "Properties";
} // namespace sections

/* The canvas keeps at least this much width (at UI scale 1) beside the docked panels. */
inline constexpr float PANELS_CANVAS_MIN = 240.0f;

class Panels
{
  public:
    /* The layout of all panels, in its default state. */
    Panels();

    /* Restores the default layout. */
    void reset();

    /* Multiplies every panel width by `ratio` (the UI scale changed), within the limits at the
     * current scale. */
    void rescale(float ratio);

    /* Assigns the docked panels their positions and returns the width the canvas loses on each
     * side. Call once per frame before begin. When the window is too narrow to leave the canvas
     * PANELS_CANVAS_MIN (at UI scale 1) the resizable panels are drawn narrower, down to their
     * minimum; the width the user chose is kept and comes back with a wider window. */
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

    /* Writes the layout (visibility, dock side, width, shown sections of each panel) into / reads
     * it back from the settings; a missing or invalid entry keeps the current value. */
    void store(SettingsMap &settings) const;
    void load(const SettingsMap &settings);

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
        const char *key; /* settings keys: panel.<key>.*; never changes, unlike the title */
        const char *title;
        float width;        /* what the user chose (saved) */
        float shown = 0.0f; /* what is drawn: width, or less in a window too narrow for it */
        float defaultWidth; /* at UI scale 1: what a double click on the inner edge restores */
        float minWidth;     /* the limits of a resize, at UI scale 1; equal for a fixed width */
        float maxWidth;
        bool resizable() const
        {
            return minWidth < maxWidth;
        }
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

    void fitWidths(float available);
    static void dock(Panel &panel, DockSide side);
    void panelMenu(Panel &panel);
    static void dragToDock(Panel &panel);
    static void resizeEdge(Panel &panel);
    static bool titleHovered();
    static void undockOnDrag(Panel &panel);

    std::array<Panel, static_cast<std::size_t>(PanelId::Count)> panels_;
    bool windowOpen_ = false; /* begin called ImGui::Begin, so end must call ImGui::End */
};

#endif
