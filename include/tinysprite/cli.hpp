#ifndef TINYSPRITE_CLI_HPP
#define TINYSPRITE_CLI_HPP

/* The headless `tinysprite export` command, for build scripts: it opens no window and needs no
 * display. `argv[0]` is the program, `argv[1]` is "export".
 *
 *   export <input> --sheet out.png [--json out.json] [--layout horizontal|vertical|grid]
 *                  [--columns N] [--spacing N] [--scale N] [--first N] [--last N]
 *   export <input> --image out.png [--frame N] [--scale N]
 *   export <input> --gif out.gif [--scale N]
 *   export <input> --tilemap out.tmj
 *
 * `--tilemap` writes the Tiled-style map and one PNG per tileset beside it (see tilemap_export);
 * it fails when the file has no tilemap.
 * `--columns` without `--layout` lays the sheet out as a grid.
 * <input> is a .pxa file, or any image the editor can import. Returns 0 on success, 2 for a bad
 * command line (a message with the usage goes to stderr), 1 when the input cannot be read or an
 * output cannot be written. */
int cli_export(int argc, char **argv);

#endif
