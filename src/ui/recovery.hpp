#ifndef TINYSPRITE_UI_RECOVERY_HPP
#define TINYSPRITE_UI_RECOVERY_HPP

#include <string>
#include <vector>

#include "tinysprite/sprite.hpp"

/* Autosave recovery. While a document has unsaved changes the app writes a copy of it into the
 * config directory every so often; a clean exit (or any save) removes it, so a copy that is still
 * there at the next start means the last session ended without saving, e.g. a crash. Every open
 * document has its own copy (its "slot"): <dir>/recovery_<slot>.pxa, with the path of the file the
 * document was saved as in <dir>/recovery_<slot>.path (empty for a document that never had one). An
 * earlier version kept a single copy as recovery.pxa / recovery.path; one left over is offered too.
 */
struct Recovered {
    Sprite sprite;
    std::string path;
};

/* Writes the copy of the document in `slot` (replacing its earlier one); false if it could not be
 * written. */
bool recovery_save(const std::string &dir, int slot, const Sprite &sprite, const std::string &path);

/* The copies left by an earlier session, in slot order (an old single copy first); the unreadable
 * ones are left out. */
std::vector<Recovered> recovery_load(const std::string &dir);

/* Removes the copy of one document, or all of them (no error if there are none). */
void recovery_clear_slot(const std::string &dir, int slot);
void recovery_clear(const std::string &dir);

#endif
