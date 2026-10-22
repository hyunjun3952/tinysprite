#ifndef TINYSPRITE_UI_DRAG_H
#define TINYSPRITE_UI_DRAG_H

#include <memory>

class Editor;

/* What the mouse is doing between a tool's press and its release. The editor owns at most one;
 * each tool's drag is in drags.cpp. Cel coordinates may lie outside the sprite. */
class Drag
{
  public:
    Drag(Editor &editor, int anchorX, int anchorY)
        : editor_(editor), anchorX_(anchorX), anchorY_(anchorY), lastX_(anchorX), lastY_(anchorY)
    {
    }
    virtual ~Drag() = default;
    Drag(const Drag &) = delete;
    Drag &operator=(const Drag &) = delete;
    Drag(Drag &&) = delete;
    Drag &operator=(Drag &&) = delete;

    /* Whether Shift is held, set before each update: a shape then keeps even proportions. */
    void setShift(bool shift)
    {
        shift_ = shift;
    }

    /* One step with the cel under the mouse, called every frame while the button is held. */
    virtual void update(int cx, int cy) = 0;

    /* Commits the drag. */
    virtual void finish() = 0;

    /* Reverts what the drag changed. */
    virtual void cancel() = 0;

  protected:
    Editor &editor_;
    int anchorX_; /* cel under the mouse at the press */
    int anchorY_;
    int lastX_; /* cel under the mouse at the last step */
    int lastY_;
    bool shift_ = false;
};

/* Applies the editor's active tool to a press at cel (cx, cy). Returns the drag that follows the
 * mouse until the release, or null if the tool needs none (the bucket's single click) or does not
 * accept the press (a hidden layer). Used by Editor::press. */
std::unique_ptr<Drag> beginDrag(Editor &editor, int cx, int cy);

#endif
