/* The only translation unit that compiles the stb implementations (built without warnings, see
 * CMakeLists.txt). */
/* Open files by UTF-8 name on Windows too (a no-op elsewhere). */
#define STBI_WINDOWS_UTF8
#define STBIW_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
