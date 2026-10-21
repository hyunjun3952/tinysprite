# Third-party dependencies

| Name | Version | License | Used by | Status |
|------|---------|---------|---------|--------|
| minitest.h | in-tree | project | tests | vendored |
| stb_image / stb_image_write | TBD (pin at milestone 7) | Public domain / MIT | tinysprite_io | planned |
| SDL2 | 2.32.10 (system package `libsdl2-dev`) | zlib | tinysprite_ui | used |
| cimgui | commit 2a6a93fccd10f275ee9206167dc9698f6f00fac1 | MIT | tinysprite_ui | vendored (`third_party/cimgui/`) |
| Dear ImGui | 1.93.0 (commit 6d32174508fb8c518e7a832cc1d571fadd5cba76, cimgui submodule) | MIT | tinysprite_ui | vendored (`third_party/cimgui/imgui/`, core + SDL2/SDL_Renderer2 backends) |
