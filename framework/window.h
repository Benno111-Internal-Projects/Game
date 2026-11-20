#ifndef RENDER_FRAMEWORK_WINDOW_H
#define RENDER_FRAMEWORK_WINDOW_H

#include <stdbool.h>

bool CanToggleGameFullscreen(void);

// Toggle borderless fullscreen on the current monitor. Call on the window thread.
void ToggleGameFullscreen(void);

#endif
