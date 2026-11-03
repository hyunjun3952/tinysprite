#ifndef TINYSPRITE_UI_TIMELINE_PANEL_HPP
#define TINYSPRITE_UI_TIMELINE_PANEL_HPP

#include <cstdint>
#include <vector>

#include "editor.hpp"
#include "imgui.h"
#include "tinysprite/sprite.hpp"

/* The timeline: a strip along the bottom of the window with the playback and frame buttons, the
 * duration of the current frame and a grid of cels (a column per frame, a row per layer, top layer
 * first). Clicking a cel selects its frame and layer; a dot marks a cel with something painted on
 * it, a bar one that shares its pixels with another frame (a linked cel), and dragging a frame
 * number onto another one moves the frame there. A row above the frame
 * numbers shows the tags as colored bands; clicking one selects it for editing and for playback. */
/* The lane (row of the tag strip) of each tag, in list order: the first lane in which it overlaps
 * no tag put there before it, so overlapping tags never hide each other. */
std::vector<int> tag_lanes(const std::vector<Sprite::Tag> &tags);

/* The tooltip of the Play button, which reads Stop while `playing`. */
const char *play_button_tip(bool playing);

/* What dragging the timeline's top edge means (like a VS Code panel). `requested` is the distance
 * from the mouse to the bottom of the canvas area; it is kept between `minHeight` and `maxHeight`
 * (the minimum wins if the window is too short for it), and below `hideBelow` the timeline folds
 * away: the result is not visible and keeps `previous`, the height it comes back at. All in pixels
 * at UI scale 1. */
struct TimelineSize {
    bool visible;
    float height;
};
TimelineSize timeline_drag(float requested, float previous, float minHeight, float maxHeight,
                           float hideBelow);

class TimelinePanel
{
  public:
    /* The height a new window starts with, the smallest height a drag leaves it, and the height
     * below which a drag folds it away; at UI scale 1. */
    static constexpr float BASE_HEIGHT = 214.0f; /* the controls and two layers, no empty grid */
    static constexpr float MIN_HEIGHT = 96.0f;
    static constexpr float HIDE_BELOW = 60.0f;

    explicit TimelinePanel(Editor &editor);

    /* The line beside the "+ Tag" button while no tag is selected: how to pick one, which only a
     * sprite that has tags (`tagCount`) can use; null for a sprite with none. */
    static const char *tagHint(size_t tagCount);

    /* Shows another document from now on (a tab was picked); what the panel remembers about the
     * old one is dropped. */
    void setEditor(Editor &editor);

    /* Places the strip with its top-left corner at (x, y), `width` wide and `height` tall, and
     * draws it. */
    void show(float x, float y, float width, float height);

    /* Where the cel of (layer, frame) was drawn in the last show(), for tests; a zero rectangle if
     * it was not drawn. */
    struct Rect {
        ImVec2 min;
        ImVec2 max;
    };
    Rect cellRect(int layer, int frame) const;
    Rect headerRect(int frame) const;
    Rect tagCellRect(int frame, int lane = 0) const;
    /* Where the selected tag's From (or `to`: To) label, or its field, was drawn in the last
     * show(); a zero rectangle if no tag is selected. */
    Rect tagRangeRect(bool to, bool label) const;

  private:
    void toolbar();
    void onionControls();
    void tagControls();
    void tagRow(int frames, int lane, const std::vector<int> &lanes);
    void tagBand(int frame, int shown, ImVec2 min, ImVec2 max);
    void grid();
    /* A frame dragged onto another by its number; from < 0 for none. */
    struct FrameMove {
        int from = -1;
        int to = -1;
    };
    void frameHeaders(int frames, FrameMove &move);
    void frameDragDrop(int frame, FrameMove &move);
    bool tagNameField(Sprite::Tag &tag);
    void layerRow(int layer, int frames);
    void celMenu(int layer, int frame);
    void celMarks(int layer, int frame, bool active, Rect cell);
    void refreshFilled();
    bool filled(int layer, int frame) const;

    Editor *editor_;                    /* the document shown; changed by setEditor */
    std::vector<unsigned char> filled_; /* [layer * frames + frame]: the cel has a visible pixel */
    int filledLayers_ = 0;
    int filledFrames_ = 0;
    uint64_t filledRevision_ = 0;
    bool filledValid_ = false;

    std::vector<Rect> cells_;    /* as drawn, [layer * frames + frame] */
    std::vector<Rect> headers_;  /* as drawn, per frame */
    std::vector<Rect> tagCells_; /* the tag rows, lane by lane, per frame */
    int tagFrames_ = 0;          /* the frames of each lane in tagCells_ */
    char tagName_[TAG_NAME_MAX + 1] = "";
    bool tagNameActive_ = false; /* the name field is being edited: keep its text */
    Rect tagRange_[2][2] = {};   /* [From, To][label, field] as last drawn, for tests */
    int drawnFrames_ = 0;
};

#endif
