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
    char *end = nullptr;
    long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < min || value > max) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

constexpr const char *USAGE = "usage: %s [file | width [height]] [--frames N [--screenshot "
                              "out.png]] [--config-dir DIR] [--scale S] [--help]\n";

/* The command line, one argument at a time; a flag takes the argument after it as its value. */
class Args
{
  public:
    Args(int argc, char **argv) : argc_(argc), argv_(argv)
    {
    }
    bool atEnd() const
    {
        return i_ >= argc_;
    }
    const char *current() const
    {
        return argv_[i_];
    }
    void advance()
    {
        i_++;
    }
    /* Moves to the flag's value and returns it; null if the flag was the last argument. */
    const char *value()
    {
        i_++;
        return atEnd() ? nullptr : argv_[i_];
    }

  private:
    int argc_;
    char **argv_;
    int i_ = 1;
};

enum class Flag { NotAFlag, Parsed, Invalid };

/* Reads the flag at the current argument, if it is one, with its value. */
Flag parseFlag(Args &args, AppOptions &options)
{
    const char *name = args.current();
    bool known = std::strcmp(name, "--frames") == 0 || std::strcmp(name, "--config-dir") == 0 ||
                 std::strcmp(name, "--scale") == 0 || std::strcmp(name, "--screenshot") == 0;
    if (!known) {
        return Flag::NotAFlag;
    }
    const char *value = args.value();
    if (!value) {
        return Flag::Invalid;
    }
    bool ok = true;
    if (std::strcmp(name, "--frames") == 0) {
        ok = parseNumber(value, 0, INT_MAX, options.maxFrames);
    } else if (std::strcmp(name, "--config-dir") == 0) {
        options.configDir = value;
    } else if (std::strcmp(name, "--scale") == 0) {
        int percent = 0; /* given as a percent */
        ok = parseNumber(value, 100, 300, percent);
        options.uiScale = static_cast<float>(percent) / 100.0f;
    } else {
        options.screenshot = value;
    }
    return ok ? Flag::Parsed : Flag::Invalid;
}

/* A width, a height or the file to open: `sizes` / `sizeCount` collect the numbers. */
bool parsePositional(const char *arg, AppOptions &options, int (&sizes)[2], int &sizeCount)
{
    if (sizeCount < 2 && parseNumber(arg, 1, CEL_MAX_DIMENSION, sizes[sizeCount])) {
        sizeCount++;
        return true;
    }
    if (arg[0] != '-' && options.file.empty() && sizeCount == 0) {
        options.file = arg;
        return true;
    }
    return false;
}

/* usage: [file | width [height]] [--frames N]; a lone width is also the height. Any other
 * argument is a file to open (so a size and a file exclude each other). */
bool parseOptions(int argc, char **argv, AppOptions &options)
{
    int sizes[2];
    int sizeCount = 0;
    for (Args args(argc, argv); !args.atEnd(); args.advance()) {
        Flag flag = parseFlag(args, options);
        if (flag == Flag::Invalid) {
            return false;
        }
        if (flag == Flag::Parsed) {
            continue;
        }
        if (!parsePositional(args.current(), options, sizes, sizeCount)) {
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
