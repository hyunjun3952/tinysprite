/* Editor: filters over the selected layers. */

#include "editor.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace
{

/* The cels of `layer` a filter should run on, each once however many frames share it. */
std::vector<Cel *> celsToFilter(Layer &layer, int frame, bool allFrames)
{
    std::vector<Cel *> cels;
    for (int f = 0; f < layer.frameCount(); f++) {
        Cel *cel = layer.cel(f);
        bool wanted = allFrames || f == frame;
        if (wanted && std::find(cels.begin(), cels.end(), cel) == cels.end()) {
            cels.push_back(cel);
        }
    }
    return cels;
}

} // namespace

bool Editor::applyFilter(const FilterParams &params, bool allFrames)
{
    if (stroking() || playing()) {
        return false;
    }
    std::vector<Cel *> cels;
    for (int i : selectedLayers()) {
        Layer &layer = *sprite().layer(i);
        if (!layer.visible() || layer.locked()) {
            continue;
        }
        for (Cel *cel : celsToFilter(layer, frame_, allFrames)) {
            cels.push_back(cel);
        }
    }
    if (cels.empty()) {
        return false;
    }
    bool done = false;
    if (!guarded([&] { done = filter_apply_cels(history(), cels, params, selection_); })) {
        return false;
    }
    if (done) {
        markAllDirty();
    }
    return done;
}
