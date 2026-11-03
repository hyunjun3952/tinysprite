/* Editor: the guides pulled out of the rulers. */

#include "editor.hpp"
#include "tinysprite/guide_ops.hpp"

void Editor::commitGuides(const Guides &before)
{
    const Guides done = sprite_.guides();
    sprite_.guides() = before;
    if (!guide_op_set(history_, sprite_, done)) {
        sprite_.guides() = done;
        markModified();
    }
}

void Editor::clearGuides()
{
    const Guides before = sprite_.guides();
    sprite_.guides().clear();
    commitGuides(before);
}
