#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app.h"
#include "tinysprite/cel.h"

namespace
{

/* Parses a whole-string decimal number within [min, max]. */
bool parseNumber(const char *text, int min, int max, int &out)
{
    char *end;
    long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < min || value > max) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

constexpr const char *USAGE = "usage: %s [file | width [height]] [--frames N] [--help]\n";

/* usage: [file | width [height]] [--frames N]; a lone width is also the height. Any other
 * argument is a file to open (so a size and a file exclude each other). */
bool parseOptions(int argc, char **argv, AppOptions &options)
{
    int sizes[2];
    int sizeCount = 0;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--frames") == 0) {
            i++;
            if (i >= argc || !parseNumber(argv[i], 0, INT_MAX, options.maxFrames)) {
                return false;
            }
        } else if (sizeCount < 2 && parseNumber(argv[i], 1, CEL_MAX_DIMENSION, sizes[sizeCount])) {
            sizeCount++;
        } else if (argv[i][0] != '-' && options.file.empty() && sizeCount == 0) {
            options.file = argv[i];
        } else {
            return false;
        }
    }
    if (!options.file.empty() && sizeCount > 0) {
        return false;
    }
    if (sizeCount > 0) {
        options.width = sizes[0];
        options.height = sizes[sizeCount - 1];
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::printf(USAGE, argv[0]);
            return 0;
        }
    }
    AppOptions options;
    if (!parseOptions(argc, argv, options)) {
        std::fprintf(stderr, USAGE, argv[0]);
        return 2;
    }
    App app;
    if (!app.init(options)) {
        return 1;
    }
    app.run(options);
    return 0;
}
