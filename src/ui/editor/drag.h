#ifndef TINYSPRITE_UI_DRAG_H
#define TINYSPRITE_UI_DRAG_H

class Editor;

/* What the mouse is doing between a tool's press and its release. The editor owns at most one;
 * each tool's drag is in tools.cpp. Cel coordinates may lie outside the sprite. */
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

    /* One step with the cel under the mouse, called every frame while the button is held.
     * Returns false if it ran out of memory; the editor then cancels the drag. */
    virtual bool update(int cx, int cy) = 0;

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
};

#endif
