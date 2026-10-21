#ifndef TINYSPRITE_UI_PANELS_H
#define TINYSPRITE_UI_PANELS_H

#include <stdbool.h>

/* The side panels (Tools, Layers): each is docked to the left or right window edge, or floats as a
 * movable window. Docked panels can be resized by their inner edge, and a floating panel docks when
 * its title bar is dragged to an edge. */

typedef enum PanelId { PANEL_TOOLS, PANEL_LAYERS, PANEL_COUNT } PanelId;

/* The layout of all panels, in its default state. Returns NULL on allocation failure. */
typedef struct Panels Panels;
Panels *panels_create(void);
void panels_destroy(Panels *panels);

/* Restores the default layout. */
void panels_reset(Panels *panels);

/* Assigns the docked panels their positions and returns the width the canvas loses on each side.
 * Call once per frame before panel_begin. */
void panels_layout(Panels *panels, float *left, float *right);

/* Starts the panel's window. Draw the content when this returns true; always call panel_end.
 * `height` is the height of a docked panel. */
bool panel_begin(Panels *panels, PanelId id, float height);
void panel_end(void);

/* The "Window" menu: show / hide, dock left / right, float, reset layout. Call inside the main menu
 * bar. */
void panels_window_menu(Panels *panels);

#endif
