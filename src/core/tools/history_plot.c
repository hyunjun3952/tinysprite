#include "history_plot.h"

void history_plot(int x, int y, void *user)
{
    HistoryPlot *plot = user;
    if (cel_in_bounds(plot->cel, x, y) &&
        history_set_pixel(plot->history, x, y, plot->color) != 0) {
        plot->failed = true;
    }
}
