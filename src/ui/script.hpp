#ifndef TINYSPRITE_UI_SCRIPT_HPP
#define TINYSPRITE_UI_SCRIPT_HPP

#include <cstddef>
#include <string>
#include <vector>

struct ImGuiIO;

/* The input of one frame of a scripted run: what the app feeds to ImGui before the frame and what
 * it saves after it. An all-default value is a frame with no input. */
struct ScriptInput {
    std::string key;  /* an ImGui key name ("Z", "Enter", "Escape"); "" = no key event */
    std::string text; /* characters typed, UTF-8; "" = none */
    std::string shot; /* save the rendered frame to this image; "" = do not */
    float x = 0.0f;   /* the mouse position, valid when moved */
    float y = 0.0f;
    int button = -1;         /* the mouse button that changes: 0 left, 1 right, -1 none */
    int sleepMs = 0;         /* real time to wait after this frame, for the animations to settle */
    bool moved = false;      /* the mouse is at (x, y) */
    bool buttonDown = false; /* with button: pressed or released */
    bool keyDown = false;    /* with key: pressed or released */
    bool ctrl = false;       /* the modifier keys held during this frame */
    bool shift = false;
    bool alt = false;
};

/*
 * A list of input commands for a run without a display (`--script FILE`), one per line:
 *
 *     move X Y             put the mouse there
 *     click X Y            move, press and release the left button (rclick: the right one)
 *     drag X0 Y0 X1 Y1     press at the start, move in steps, release at the end
 *     path X0 Y0 X1 Y1 ... press at the first point, move through the others, release at the
 *                          last (a lasso, a curve)
 *     key ctrl+shift+Z     press and release a key, with modifiers held (ctrl, shift, alt); a
 *                          modified key takes 4 frames (ImGui reads a modifier change and the
 *                          key of one frame apart, so the modifiers go down first, up last)
 *     type TEXT            type the rest of the line
 *     wait N               N frames with no input
 *     sleep MS             after this frame, wait MS milliseconds (a 0.12 s fade to finish)
 *     shot PATH            save the rendered frame to PATH (the rest of the line)
 *
 * A line starting with # is a comment, and so is the end of a line after " #", except for type
 * and shot, whose argument is the rest of the line. parse expands the commands into one
 * ScriptInput per frame, which next hands out in order.
 */
class Script
{
  public:
    /* Reads the commands in `text`. False, with `error` saying which line and why, if one is
     * not valid; the script is then empty. */
    bool parse(const std::string &text, std::string &error);
    /* Whether every frame has been handed out. */
    bool done() const;
    /* The input of the next frame; an empty input once the script is done. */
    ScriptInput next();

  private:
    std::vector<ScriptInput> frames_;
    size_t pos_ = 0;
};

/*
 * Gives ImGui the input of one scripted frame, to be read by the next ImGui::NewFrame. A modifier
 * is sent as its key and as the ImGuiMod_ one, as the SDL backend does: io.KeyCtrl reads only the
 * latter. A key name ImGui does not know is reported on stderr and skipped.
 */
void script_feed(ImGuiIO &io, const ScriptInput &input);

#endif
