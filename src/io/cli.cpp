#include "tinysprite/cli.hpp"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

#include "path_util.hpp"
#include "tinysprite/io.hpp"

namespace
{

constexpr int EXIT_FAILED = 1;
constexpr int EXIT_USAGE = 2;

constexpr const char *USAGE =
    "usage: %s export <input> --sheet out.png [--json out.json] [--layout "
    "horizontal|vertical|grid|packed]\n"
    "          [--columns N] [--spacing N] [--scale N] [--first N] [--last N]\n"
    "       %s export <input> --image out.png [--frame N] [--scale N]\n"
    "       %s export <input> --gif out.gif [--scale N]\n";

struct Job {
    std::string input;
    std::string sheet;
    std::string json;
    std::string image;
    std::string gif;
    int frame = 0;
    bool layoutGiven = false;
    SheetOptions sheetOptions;
};

bool parseNumber(const char *text, int min, int &out)
{
    char *end = nullptr;
    long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < min || value > INT_MAX) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

bool parseLayout(const char *text, SheetLayout &layout)
{
    if (std::strcmp(text, "horizontal") == 0) {
        layout = SheetLayout::Horizontal;
    } else if (std::strcmp(text, "vertical") == 0) {
        layout = SheetLayout::Vertical;
    } else if (std::strcmp(text, "grid") == 0) {
        layout = SheetLayout::Grid;
    } else if (std::strcmp(text, "packed") == 0) {
        layout = SheetLayout::Packed;
    } else {
        return false;
    }
    return true;
}

/* A flag that names a file: the value is stored in `target`. */
std::string *pathFlag(Job &job, const char *name)
{
    const struct {
        const char *flag;
        std::string Job::*member;
    } table[] = {{"--sheet", &Job::sheet},
                 {"--json", &Job::json},
                 {"--image", &Job::image},
                 {"--gif", &Job::gif}};
    for (const auto &row : table) {
        if (std::strcmp(name, row.flag) == 0) {
            return &(job.*row.member);
        }
    }
    return nullptr;
}

/* A flag that takes a number: where it goes and its smallest value. */
struct NumberFlag {
    int *target;
    int min;
};

std::optional<NumberFlag> numberFlag(Job &job, const char *name)
{
    SheetOptions &o = job.sheetOptions;
    const struct {
        const char *flag;
        int *target;
        int min;
    } table[] = {{"--columns", &o.columns, 0}, {"--spacing", &o.spacing, 0},
                 {"--scale", &o.scale, 1},     {"--first", &o.first, 0},
                 {"--last", &o.last, 0},       {"--frame", &job.frame, 0}};
    for (const auto &row : table) {
        if (std::strcmp(name, row.flag) == 0) {
            return NumberFlag{row.target, row.min};
        }
    }
    return std::nullopt;
}

/* Reads the flag at argv[i] and its value, moving `i` to the value. */
bool parseFlag(Job &job, int argc, char **argv, int &i)
{
    const char *name = argv[i];
    if (i + 1 >= argc) {
        return false;
    }
    const char *value = argv[++i];
    if (std::string *path = pathFlag(job, name)) {
        *path = value;
        return true;
    }
    if (std::optional<NumberFlag> number = numberFlag(job, name)) {
        return parseNumber(value, number->min, *number->target);
    }
    job.layoutGiven = true;
    return std::strcmp(name, "--layout") == 0 && parseLayout(value, job.sheetOptions.layout);
}

/* Whether the outputs and options given fit together: exactly one of --sheet, --image and --gif,
 * --json only with a sheet, and a frame range only where it means something (a single image has
 * --frame). */
bool jobConsistent(const Job &job)
{
    int outputs = 0;
    for (const std::string *out : {&job.sheet, &job.image, &job.gif}) {
        outputs += out->empty() ? 0 : 1;
    }
    bool jsonOk = job.json.empty() || !job.sheet.empty();
    bool rangeOk =
        job.image.empty() || (job.sheetOptions.first == 0 && job.sheetOptions.last == -1);
    return !job.input.empty() && outputs == 1 && jsonOk && rangeOk;
}

std::optional<Job> parseJob(int argc, char **argv)
{
    Job job;
    for (int i = 2; i < argc; i++) {
        if (argv[i][0] != '-') {
            if (!job.input.empty()) {
                return std::nullopt;
            }
            job.input = argv[i];
        } else if (!parseFlag(job, argc, argv, i)) {
            return std::nullopt;
        }
    }
    if (!jobConsistent(job)) {
        return std::nullopt;
    }
    if (!job.layoutGiven && job.sheetOptions.columns > 0) {
        job.sheetOptions.layout = SheetLayout::Grid; /* --columns alone means a grid */
    }
    return job;
}

std::optional<Sprite> loadInput(const std::string &path)
{
    return path_has_extension(path, ".pxa") ? pxa_load(path) : image_import(path);
}

bool runJob(const Job &job, const Sprite &sprite)
{
    const SheetOptions &o = job.sheetOptions;
    if (!job.sheet.empty()) {
        return job.json.empty() ? spritesheet_export(sprite, job.sheet, o)
                                : spritesheet_export_with_json(sprite, job.sheet, job.json, o);
    }
    if (!job.image.empty()) {
        return image_export(sprite, job.frame, job.image, o.scale);
    }
    GifOptions gif;
    gif.first = o.first;
    gif.last = o.last;
    gif.scale = o.scale;
    return gif_export(sprite, job.gif, gif);
}

} // namespace

int cli_export(int argc, char **argv)
{
    std::optional<Job> job = parseJob(argc, argv);
    if (!job) {
        std::fprintf(stderr, USAGE, argv[0], argv[0], argv[0]);
        return EXIT_USAGE;
    }
    std::optional<Sprite> sprite = loadInput(job->input);
    if (!sprite) {
        std::fprintf(stderr, "%s: cannot read %s\n", argv[0], job->input.c_str());
        return EXIT_FAILED;
    }
    if (!runJob(*job, *sprite)) {
        std::fprintf(stderr, "%s: export failed (bad option, format or path)\n", argv[0]);
        return EXIT_FAILED;
    }
    return 0;
}
