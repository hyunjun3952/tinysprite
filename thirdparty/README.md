# Third-party dependencies

## Libraries: not in the repository

They are declared in [`vcpkg.json`](../vcpkg.json), and its `builtin-baseline` (a commit of vcpkg) pins every version.
To change a version, move the baseline and rebuild; to add a library, add it to `vcpkg.json` and record it here.

| Name | Version | License | Used by |
|------|---------|---------|---------|
| SDL3 | 3.4.18 | zlib | `tinysprite_ui` (window, input, renderer) |
| Dear ImGui | 1.93.0, with its SDL3 and SDL3 renderer backends | MIT | `tinysprite_ui` |
| stb (stb_image, stb_image_write) | 2026-08-02 | Public domain / MIT | `tinysprite_io`, compiled only in `src/io/stb_impl.cpp` |

`stb` can also come from the system (`libstb-dev`); the core and io libraries need nothing else.

## Fonts: in the repository

These are not libraries but fonts reduced to the glyphs the UI uses (a full font would weigh megabytes), so each one is a
C array with its license beside it and the command that made it in the header comment.

| Name | Source | License | Used by |
|------|--------|---------|---------|
| Geist Regular 1.8.2 (subset) | vercel/geist-font v1.8.2 `Geist/ttf/Geist-Regular.ttf`, Latin and UI punctuation (`thirdparty/geist/geist_regular_subset.hpp`) | SIL OFL 1.1 (`thirdparty/geist/OFL.txt`) | `theme::load_fonts` |
| Material Symbols Outlined (subset) | google/material-design-icons `variablefont/MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].ttf`, FILL 0 / GRAD 0 / opsz 20 / wght 400, 28 glyphs (`thirdparty/material-symbols/material_symbols_outlined_subset.hpp`) | Apache 2.0 (`thirdparty/material-symbols/LICENSE.txt`) | `icons`, merged into the UI font by `theme::load_fonts` |
