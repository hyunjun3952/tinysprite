#ifndef TINYSPRITE_UI_LAYER_SELECTION_HPP
#define TINYSPRITE_UI_LAYER_SELECTION_HPP

#include <utility>
#include <vector>

#include "tinysprite/sprite.hpp"

/* How a click on a layer changes the layer selection (see LayerSelection::pick). */
enum class LayerPick {
    Only,   /* select just this layer (a plain click) */
    Toggle, /* add it to the selection, or remove it (Ctrl+click); the last one stays */
    Range,  /* select from the last plain / Ctrl pick to this one (Shift+click) */
};

/* The layer selection: the active layer (what the tools draw on) plus any layers picked with Ctrl
 * or Shift. The picked layers are tracked by identity, not by index, so reordering the stack keeps
 * them; refresh() forgets the ones that left the sprite. The sprite is passed to each call rather
 * than held, so a selection stays valid when the document is replaced (after reset()). */
class LayerSelection
{
  public:
    int active() const
    {
        return active_;
    }

    /* Makes `index` the active layer without touching the other picks; for edits that move layers
     * around the selection. */
    void setActive(int index)
    {
        active_ = index;
    }

    /* Applies a click on the layer at `index`; ignores an index with no layer. */
    void pick(const Sprite &sprite, int index, LayerPick mode);

    bool contains(const Sprite &sprite, int index) const;

    /* Indexes of the selected layers, ascending. */
    std::vector<int> indexes(const Sprite &sprite) const;

    /* Replaces the picks besides the active layer. */
    void setPicked(std::vector<const Layer *> layers)
    {
        picked_ = std::move(layers);
    }

    /* The layer at `from` was moved to `to`: the active index keeps pointing at the same layer. */
    void moved(int from, int to);

    /* After layers were added, removed or reordered behind the selection's back (undo / redo):
     * the active index follows `active` to its new place if it is still in the sprite and is
     * otherwise clamped into the stack, and picks of layers that are gone are dropped. */
    void refresh(const Sprite &sprite, const Layer *active);

    /* Back to just the first layer, for a new document. */
    void reset();

  private:
    void toggle(const Sprite &sprite, int index);
    void range(const Sprite &sprite, int index);

    int active_ = 0;
    std::vector<const Layer *> picked_; /* selected layers besides the active one */
    const Layer *anchor_ = nullptr;     /* where a Shift pick starts */
};

#endif
