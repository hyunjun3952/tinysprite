#ifndef TINYSPRITE_UI_SLICES_PANEL_HPP
#define TINYSPRITE_UI_SLICES_PANEL_HPP

#include "editor.hpp"
#include "tinysprite/sprite.hpp"

/* The Slices section of the Inspector: the sprite's slices (named rectangles with an optional
 * pivot, listed in the sprite sheet JSON) as a list, and the name, rectangle and pivot of the
 * selected one as fields. Every change is one undo step (Editor::updateSlice). */

/* `wanted` made valid for a sprite of this size: at least 1 x 1 and no bigger than the sprite, and
 * moved (not shrunk) so it lies inside it. */
Rect slice_rect_fit(Rect wanted, int spriteWidth, int spriteHeight);

/* The slice's pivot (if it has one) moved onto its rectangle: 0..width and 0..height cels from its
 * top-left corner. */
void slice_pivot_fit(Sprite::Slice &slice);

/* Gives the slice a pivot at the middle of its rectangle. */
void slice_pivot_center(Sprite::Slice &slice);

class SlicesPanel
{
  public:
    explicit SlicesPanel(Editor &editor);

    /* Shows another document from now on. */
    void setEditor(Editor &editor);

    /* The content of the section; call it inside the Inspector window. */
    void show();

  private:
    void buttons();
    void list();
    void details(int index);
    bool nameField(Sprite::Slice &slice);
    bool rectFields(Sprite::Slice &slice);
    bool pivotFields(Sprite::Slice &slice);

    Editor *editor_; /* the document shown; changed by setEditor */
    char name_[SLICE_NAME_MAX + 1] = "";
    bool nameActive_ = false; /* the name field is being edited: keep its text */
};

#endif
