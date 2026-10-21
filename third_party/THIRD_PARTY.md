# Third-party dependencies

| Name | Version | License | Used by | Status |
|------|---------|---------|---------|--------|
| minitest.h | in-tree | project | tests | vendored |
| stb_image 2.30 / stb_image_write 1.16 | nothings/stb master @ a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d (fetched 2026-10-21) | Public domain / MIT | tinysprite_io | vendored (`third_party/stb/`, compiled only in `src/io/stb_impl.cpp`) |
| SDL2 | 2.32.10 (system package `libsdl2-dev`) | zlib | tinysprite_ui | used |
| Dear ImGui | 1.93.0 (commit 6d32174508fb8c518e7a832cc1d571fadd5cba76) | MIT | tinysprite_ui | vendored (`third_party/imgui/`, core + SDL2/SDL_Renderer2 backends) |
