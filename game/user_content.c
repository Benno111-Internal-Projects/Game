#include "user_content.h"
#include "raylib.h"

#if defined(BENNO_IOS)
#include <SDL.h>
#endif

#include <stdio.h>
#include <stdlib.h>

bool UserContentPath(char *path, size_t capacity, const char *filename, bool create)
{
#if defined(PLATFORM_ANDROID)
    // raylib maps this path to the activity's private storage.
    (void)create;
    int length = snprintf(path, capacity, "%s", filename);
#elif defined(BENNO_IOS)
    (void)create;
    char *dir = SDL_GetPrefPath("Benno111", "Benno111Engene");
    if (!dir) return false;
    int length = snprintf(path, capacity, "%s%s", dir, filename);
    SDL_free(dir);
#else
    char dir[768];
#if defined(_WIN32)
    const char *base = getenv("LOCALAPPDATA");
    int dirLength = snprintf(dir, sizeof(dir), "%s/benno111-engene", base ? base : GetWorkingDirectory());
#else
    const char *base = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    int dirLength = base && *base ? snprintf(dir, sizeof(dir), "%s/benno111-engene", base)
        : snprintf(dir, sizeof(dir), "%s/.local/share/benno111-engene", home ? home : GetWorkingDirectory());
#endif
    if (dirLength < 0 || (size_t)dirLength >= sizeof(dir)) return false;
    if (create && !DirectoryExists(dir) && MakeDirectory(dir) != 0) return false;
    int length = snprintf(path, capacity, "%s/%s", dir, filename);
#endif
    return length >= 0 && (size_t)length < capacity;
}
