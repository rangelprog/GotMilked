#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_WINDOWS_UTF8 // erlaubt UTF-8 Pfade unter Windows
// WICHTIG: KEIN STBI_NO_STDIO hier!
#include "stb_image.h"
