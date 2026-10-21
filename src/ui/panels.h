#ifndef TINYSPRITE_UI_PANELS_H
#define TINYSPRITE_UI_PANELS_H

/* The side panels (Tools, Layers): each is docked to the left or right window edge, or floats as a
 * movable window. Docked panels can be resized by their inner edge, and a floating panel docks when
 * its title bar is dragged to an edge. State lives in this module for the whole run. */

typedef enum PanelId { PANEL_TOOLS, PANEL_LAYERS, PANEL_COUNT } PanelId;

/* Restores the default layout. Call once at start-up. */
void panels_reset(void);

/* Assigns the docked panels their positions and returns the width the canvas loses on each side.
 * Call once per frame before panel_begin. */
void panels_layout(float *left, float *right);

/* Starts the panel's window. Draw the content when this returns non-zero; always call panel_end.
 * `height` is the height of a docked panel. */
int panel_begin(PanelId id, float height);
void panel_end(void);

/* The "Window" menu: show / hide, dock left / right, float, reset layout. Call inside the main menu
 * bar. */
void panels_window_menu(void);

#endif
