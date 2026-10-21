#ifndef TINYSPRITE_UI_TOOLS_H
#define TINYSPRITE_UI_TOOLS_H

#include "editor/editor.h"

/* Mouse handling for the active tool, in cel coordinates (which may lie outside the sprite).
 * press starts a drag when the tool accepts it, drag is called every frame while the button is
 * held, release commits. Each one marks the cels it changes dirty. */
void tools_press(Editor *editor, int cx, int cy);
void tools_drag(Editor *editor, int cx, int cy);
void tools_release(Editor *editor);

/* Aborts the current drag, reverting its pixels. Does nothing when no drag is active. */
void tools_cancel(Editor *editor);

/* Name for the tools panel. */
const char *tools_name(ToolKind tool);

#endif
