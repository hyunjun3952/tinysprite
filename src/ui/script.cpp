#include "script.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "imgui.h"
#include "text_util.hpp"

namespace
{

constexpr int DRAG_STEPS = 8;           /* frames a drag takes to get from its start to its end */
constexpr long MAX_WAIT = 100000;       /* the most frames one wait may last */
constexpr long MAX_SLEEP_MS = 10000;    /* the longest one sleep may last */
constexpr long MAX_WINDOW_SIDE = 16384; /* pixels, the largest side resize takes */

using Words = std::vector<std::string>;
using Frames = std::vector<ScriptInput>;

/* The text up to a " #" or a "#" at its start, which begins a comment. */
std::string withoutComment(const std::string &text)
{
    size_t at = text.find('#');
    while (at != std::string::npos && at > 0 &&
           std::isspace(static_cast<unsigned char>(text[at - 1])) == 0) {
        at = text.find('#', at + 1);
    }
    return at == std::string::npos ? text : text.substr(0, at);
}

Words split(const std::string &text)
{
    std::istringstream in(text);
    Words words;
    std::string word;
    while (in >> word) {
        words.push_back(word);
    }
    return words;
}

/* Reads a whole word as a finite number. */
bool parseFloat(const std::string &word, float &out)
{
    char *end = nullptr;
    float value = std::strtof(word.c_str(), &end);
    if (end == word.c_str() || *end != '\0' || !std::isfinite(value)) {
        return false;
    }
    out = value;
    return true;
}

/* Reads exactly `count` numbers (at most four) from `args`. */
bool parseNumbers(const Words &args, size_t count, float (&out)[4], std::string &why)
{
    if (args.size() != count) {
        why = "needs " + std::to_string(count) + " numbers";
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (!parseFloat(args[i], out[i])) {
            why = "'" + args[i] + "' is not a number";
            return false;
        }
    }
    return true;
}

ScriptInput pointerAt(float x, float y)
{
    ScriptInput input;
    input.moved = true;
    input.x = x;
    input.y = y;
    return input;
}

ScriptInput buttonAt(float x, float y, int button, bool down)
{
    ScriptInput input = pointerAt(x, y);
    input.button = button;
    input.buttonDown = down;
    return input;
}

/* The button a word names: left, right or middle; -1 for any other word. */
int buttonNamed(const std::string &word)
{
    static const char *const NAMES[] = {"left", "right", "middle"};
    for (int i = 0; i < 3; i++) {
        if (word == NAMES[i]) {
            return i;
        }
    }
    return -1;
}

/* Takes a button word off the end of `args` into `button`, if the last word is not a number.
 * False, with the reason, for a last word that is neither a number nor a button. */
bool takeButton(Words &args, int &button, std::string &why)
{
    float number = 0.0f;
    if (args.empty() || parseFloat(args.back(), number)) {
        return true;
    }
    button = buttonNamed(args.back());
    if (button < 0) {
        why = "'" + args.back() + "' is not a button (left, right or middle)";
        return false;
    }
    args.pop_back();
    return true;
}

/* click and rclick: `button` is the command's own, which a button word replaces. */
bool addClick(const Words &words, int button, Frames &frames, std::string &why)
{
    Words args = words;
    float n[4] = {};
    if (!takeButton(args, button, why) || !parseNumbers(args, 2, n, why)) {
        return false;
    }
    frames.push_back(pointerAt(n[0], n[1]));
    frames.push_back(buttonAt(n[0], n[1], button, true));
    frames.push_back(buttonAt(n[0], n[1], button, false));
    return true;
}

/* press or release (`down`): one frame that moves the mouse and changes one button. */
bool addPress(const Words &words, bool down, Frames &frames, std::string &why)
{
    Words args = words;
    float n[4] = {};
    int button = 0;
    if (!takeButton(args, button, why) || !parseNumbers(args, 2, n, why)) {
        return false;
    }
    frames.push_back(buttonAt(n[0], n[1], button, down));
    return true;
}

bool addMove(const Words &args, Frames &frames, std::string &why)
{
    float n[4] = {};
    if (!parseNumbers(args, 2, n, why)) {
        return false;
    }
    frames.push_back(pointerAt(n[0], n[1]));
    return true;
}

bool addDrag(const Words &words, Frames &frames, std::string &why)
{
    Words args = words;
    float n[4] = {};
    int button = 0;
    if (!takeButton(args, button, why) || !parseNumbers(args, 4, n, why)) {
        return false;
    }
    frames.push_back(pointerAt(n[0], n[1]));
    frames.push_back(buttonAt(n[0], n[1], button, true));
    for (int step = 1; step <= DRAG_STEPS; step++) {
        float t = static_cast<float>(step) / static_cast<float>(DRAG_STEPS);
        frames.push_back(pointerAt(n[0] + (n[2] - n[0]) * t, n[1] + (n[3] - n[1]) * t));
    }
    frames.push_back(buttonAt(n[2], n[3], button, false));
    return true;
}

/* path X0 Y0 X1 Y1 ...: press at the first point, move through the others, release at the last. */
bool addPath(const Words &words, Frames &frames, std::string &why)
{
    Words args = words;
    constexpr size_t MIN_NUMBERS = 4; /* two points */
    int button = 0;
    if (!takeButton(args, button, why)) {
        return false;
    }
    if (args.size() < MIN_NUMBERS || args.size() % 2 != 0) {
        why = "needs at least two points, as pairs of numbers";
        return false;
    }
    std::vector<float> n;
    for (const std::string &word : args) {
        float value = 0.0f;
        if (!parseFloat(word, value)) {
            why = "'" + word + "' is not a number";
            return false;
        }
        n.push_back(value);
    }
    frames.push_back(pointerAt(n[0], n[1]));
    frames.push_back(buttonAt(n[0], n[1], button, true));
    for (size_t i = 2; i < n.size(); i += 2) {
        const float x0 = n[i - 2];
        const float y0 = n[i - 1];
        for (int step = 1; step <= DRAG_STEPS; step++) {
            const float t = static_cast<float>(step) / static_cast<float>(DRAG_STEPS);
            frames.push_back(pointerAt(x0 + (n[i] - x0) * t, y0 + (n[i + 1] - y0) * t));
        }
    }
    frames.push_back(buttonAt(n[n.size() - 2], n[n.size() - 1], button, false));
    return true;
}

/* Reads "ctrl+shift+Z" into the modifiers and the key name. */
bool parseKeySpec(const std::string &spec, ScriptInput &input)
{
    std::istringstream in(spec);
    std::string part;
    std::string last;
    while (std::getline(in, part, '+')) {
        std::string lower = part;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        input.ctrl = input.ctrl || lower == "ctrl";
        input.shift = input.shift || lower == "shift";
        input.alt = input.alt || lower == "alt";
        last = part;
    }
    bool modifier = last == "ctrl" || last == "shift" || last == "alt";
    if (last.empty() || modifier || spec.back() == '+') {
        return false;
    }
    input.key = last;
    return true;
}

bool addKey(const Words &args, Frames &frames, std::string &why)
{
    ScriptInput down;
    if (args.size() != 1 || !parseKeySpec(args[0], down)) {
        why = "needs a key like Z or ctrl+shift+Z";
        return false;
    }
    down.keyDown = true;
    ScriptInput up = down;
    up.keyDown = false;
    ScriptInput held; /* the modifiers alone, around the key */
    held.ctrl = down.ctrl;
    held.shift = down.shift;
    held.alt = down.alt;
    bool modified = held.ctrl || held.shift || held.alt;
    if (modified) {
        frames.push_back(held);
    }
    frames.push_back(down);
    frames.push_back(up);
    if (modified) {
        frames.emplace_back();
    }
    return true;
}

/* Reads "ctrl+shift" (any of ctrl, shift and alt, joined by +, any case) into the modifiers of
 * `input`; false for any other word. */
bool parseModifiers(const std::string &spec, ScriptInput &input)
{
    std::istringstream in(spec);
    std::string part;
    while (std::getline(in, part, '+')) {
        std::transform(part.begin(), part.end(), part.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        input.ctrl = input.ctrl || part == "ctrl";
        input.shift = input.shift || part == "shift";
        input.alt = input.alt || part == "alt";
        if (part != "ctrl" && part != "shift" && part != "alt") {
            return false;
        }
    }
    return !spec.empty() && spec.back() != '+';
}

/* wheel X Y DX DY [MODS]: like a key, a modified turn holds the modifiers a frame before and lets
 * them go a frame after, so ImGui never sees them change in the turn's own frame. */
bool addWheel(const Words &words, Frames &frames, std::string &why)
{
    Words args = words;
    ScriptInput held;
    constexpr size_t NUMBERS = 4;
    if (args.size() == NUMBERS + 1) {
        if (!parseModifiers(args.back(), held)) {
            why = "'" + args.back() + "' is not a modifier (ctrl, shift, alt)";
            return false;
        }
        args.pop_back();
    }
    float n[4] = {};
    if (!parseNumbers(args, NUMBERS, n, why)) {
        return false;
    }
    ScriptInput turn = held;
    turn.moved = true;
    turn.x = n[0];
    turn.y = n[1];
    turn.wheelX = n[2];
    turn.wheelY = n[3];
    const bool modified = held.ctrl || held.shift || held.alt;
    if (modified) {
        frames.push_back(held);
    }
    frames.push_back(turn);
    if (modified) {
        frames.emplace_back();
    }
    return true;
}

/* Reads a whole word as a whole number within [min, max]. */
bool parseCount(const std::string &word, long min, long max, long &out)
{
    char *end = nullptr;
    const long value = std::strtol(word.c_str(), &end, 10);
    if (end == word.c_str() || *end != '\0' || value < min || value > max) {
        return false;
    }
    out = value;
    return true;
}

bool addResize(const Words &args, Frames &frames, std::string &why)
{
    long width = 0;
    long height = 0;
    if (args.size() != 2 || !parseCount(args[0], 1, MAX_WINDOW_SIDE, width) ||
        !parseCount(args[1], 1, MAX_WINDOW_SIDE, height)) {
        why = "needs a width and a height from 1 to " + std::to_string(MAX_WINDOW_SIDE);
        return false;
    }
    ScriptInput input;
    input.resizeWidth = static_cast<int>(width);
    input.resizeHeight = static_cast<int>(height);
    frames.push_back(input);
    return true;
}

bool addWait(const Words &args, Frames &frames, std::string &why)
{
    long count = 0;
    if (args.size() != 1 || !parseCount(args[0], 0, MAX_WAIT, count)) {
        why = "needs a number of frames from 0 to " + std::to_string(MAX_WAIT);
        return false;
    }
    frames.insert(frames.end(), static_cast<size_t>(count), ScriptInput());
    return true;
}

bool addSleep(const Words &args, Frames &frames, std::string &why)
{
    long millis = 0;
    if (args.size() != 1 || !parseCount(args[0], 0, MAX_SLEEP_MS, millis)) {
        why = "needs milliseconds from 0 to " + std::to_string(MAX_SLEEP_MS);
        return false;
    }
    ScriptInput input;
    input.sleepMs = static_cast<int>(millis);
    frames.push_back(input);
    return true;
}

/* A command whose whole rest of the line is one text: it goes into `field` of a new input, and
 * `missing` is the reason given when the line has none. */
bool addText(const std::string &rest, std::string ScriptInput::*field, const char *missing,
             Frames &frames, std::string &why)
{
    if (rest.empty()) {
        why = missing;
        return false;
    }
    ScriptInput input;
    input.*field = rest;
    frames.push_back(input);
    return true;
}

/* A command whose arguments are words: it adds its frames, or says why it cannot. */
using WordCommand = bool (*)(const Words &, Frames &, std::string &);

/* The commands whose arguments are words, by name. */
struct WordEntry {
    const char *name;
    WordCommand add;
};

const WordEntry WORD_COMMANDS[] = {
    {"move", addMove},
    {"click", [](const Words &a, Frames &f, std::string &w) { return addClick(a, 0, f, w); }},
    {"rclick", [](const Words &a, Frames &f, std::string &w) { return addClick(a, 1, f, w); }},
    {"press", [](const Words &a, Frames &f, std::string &w) { return addPress(a, true, f, w); }},
    {"release", [](const Words &a, Frames &f, std::string &w) { return addPress(a, false, f, w); }},
    {"drag", addDrag},
    {"path", addPath},
    {"wheel", addWheel},
    {"resize", addResize},
    {"key", addKey},
    {"wait", addWait},
    {"sleep", addSleep},
};

/* The commands whose argument is the rest of the line, comment marks and spaces included. */
struct TextEntry {
    const char *name;
    std::string ScriptInput::*field;
    const char *missing; /* the reason given when the line has no argument */
};

const TextEntry TEXT_COMMANDS[] = {
    {"type", &ScriptInput::text, "needs the text to type"},
    {"shot", &ScriptInput::shot, "needs a file to save to"},
    {"drop", &ScriptInput::drop, "needs the file to drop"},
};

/* Expands one command line; `why` says what is wrong when it returns false. */
bool addCommand(const std::string &line, Frames &frames, std::string &why)
{
    size_t space = line.find_first_of(" \t");
    std::string command = line.substr(0, space);
    std::string rest = space == std::string::npos ? "" : trim(line.substr(space));
    for (const TextEntry &entry : TEXT_COMMANDS) {
        if (command == entry.name) {
            return addText(rest, entry.field, entry.missing, frames, why);
        }
    }
    for (const WordEntry &entry : WORD_COMMANDS) {
        if (command == entry.name) {
            return entry.add(split(withoutComment(rest)), frames, why);
        }
    }
    why = "unknown command '" + command + "'";
    return false;
}

} // namespace

bool Script::parse(const std::string &text, std::string &error)
{
    frames_.clear();
    pos_ = 0;
    error.clear();
    std::istringstream in(text);
    std::string raw;
    for (int number = 1; std::getline(in, raw); number++) {
        std::string line = trim(raw);
        std::string why;
        if (line.empty() || line[0] == '#' || addCommand(line, frames_, why)) {
            continue;
        }
        error = "line " + std::to_string(number) + ": " + why;
        frames_.clear();
        return false;
    }
    return true;
}

bool Script::done() const
{
    return pos_ >= frames_.size();
}

ScriptInput Script::next()
{
    return done() ? ScriptInput() : frames_[pos_++];
}

namespace
{

/* The ImGui key called `name` (as ImGui::GetKeyName spells it), or ImGuiKey_None. */
ImGuiKey findKey(const std::string &name)
{
    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++) {
        if (name == ImGui::GetKeyName(static_cast<ImGuiKey>(key))) {
            return static_cast<ImGuiKey>(key);
        }
    }
    return ImGuiKey_None;
}

} // namespace

void script_feed(ImGuiIO &io, const ScriptInput &input)
{
    io.AddKeyEvent(ImGuiKey_LeftCtrl, input.ctrl);
    io.AddKeyEvent(ImGuiMod_Ctrl, input.ctrl);
    io.AddKeyEvent(ImGuiKey_LeftShift, input.shift);
    io.AddKeyEvent(ImGuiMod_Shift, input.shift);
    io.AddKeyEvent(ImGuiKey_LeftAlt, input.alt);
    io.AddKeyEvent(ImGuiMod_Alt, input.alt);
    if (input.moved) {
        io.AddMousePosEvent(input.x, input.y);
    }
    if (input.button >= 0) {
        io.AddMouseButtonEvent(input.button, input.buttonDown);
    }
    if (input.wheelX != 0.0f || input.wheelY != 0.0f) {
        io.AddMouseWheelEvent(input.wheelX, input.wheelY);
    }
    if (!input.key.empty()) {
        ImGuiKey key = findKey(input.key);
        if (key == ImGuiKey_None) {
            std::fprintf(stderr, "script: unknown key '%s'\n", input.key.c_str());
        } else {
            io.AddKeyEvent(key, input.keyDown);
        }
    }
    if (!input.text.empty()) {
        io.AddInputCharactersUTF8(input.text.c_str());
    }
}
