# Third-party dependencies

| Name | Version | License | Used by | Status |
|------|---------|---------|---------|--------|
| stb_image 2.30 / stb_image_write 1.16 | nothings/stb master @ a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d (fetched 2026-10-21) | Public domain / MIT | tinysprite_io | vendored (`thirdparty/stb/`, compiled only in `src/io/stb_impl.cpp`) |
| SDL2 | 2.32.10 (system package `libsdl2-dev`) | zlib | tinysprite_ui | used |
| Dear ImGui | 1.93.0 (commit 6d32174508fb8c518e7a832cc1d571fadd5cba76) | MIT | tinysprite_ui | vendored (`thirdparty/imgui/`, core + SDL2/SDL_Renderer2 backends) |
| Geist Regular 1.8.2 (subset) | vercel/geist-font v1.8.2 `Geist/ttf/Geist-Regular.ttf`, reduced with fonttools pyftsubset to Latin + UI punctuation (see the header comment in `thirdparty/geist/geist_regular_subset.hpp` for the command) | SIL OFL 1.1 (`thirdparty/geist/OFL.txt`) | tinysprite_ui (`theme::load_fonts`) | vendored as a C array |
| Material Symbols Outlined (subset) | google/material-design-icons `variablefont/MaterialSymbolsOutlined[FILL,GRAD,opsz,wght].ttf`, instanced at FILL 0 / GRAD 0 / opsz 20 / wght 400 and subset to 28 glyphs (4 KB; command in `thirdparty/material-symbols/material_symbols_outlined_subset.hpp`) | Apache 2.0 (`thirdparty/material-symbols/LICENSE.txt`) | tinysprite_ui (`icons`, merged into the UI font by `theme::load_fonts`) | vendored as a C array |
