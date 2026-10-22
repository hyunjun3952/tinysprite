#ifndef TINYSPRITE_UI_RECOVERY_HPP
#define TINYSPRITE_UI_RECOVERY_HPP

#include <optional>
#include <string>

#include "tinysprite/sprite.hpp"

/* Autosave recovery. While a document has unsaved changes the app writes a copy of it into the
 * config directory every so often; a clean exit (or any save) removes it, so a copy that is still
 * there at the next start means the last session ended without saving, e.g. a crash. The copy is
 * <dir>/recovery.pxa and the path of the file the document was saved as is kept in
 * <dir>/recovery.path (empty for a document that never had one). */
struct Recovered {
    Sprite sprite;
    std::string path;
};

/* Writes the copy; false if it could not be written. */
bool recovery_save(const std::string &dir, const Sprite &sprite, const std::string &path);

/* The copy left by an earlier session, or nothing if there is none or it is unreadable. */
std::optional<Recovered> recovery_load(const std::string &dir);

/* Removes the copy (no error if there is none). */
void recovery_clear(const std::string &dir);

#endif
