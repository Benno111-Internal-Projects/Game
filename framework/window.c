#include "window.h"
#include "raylib.h"

bool CanToggleGameFullscreen(void)
{
#if defined(RENDER_FRAMEWORK_MOBILE)
    return false;
#else
    return IsWindowReady();
#endif
}

void ToggleGameFullscreen(void)
{
    if (CanToggleGameFullscreen()) ToggleBorderlessWindowed();
}
