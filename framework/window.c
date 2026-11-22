#include "window.h"
#include "raylib.h"
#include "rlgl.h"

#if defined(RENDER_FRAMEWORK_IOS)
#include <OpenGLES/ES2/gl.h>
#include <SDL.h>
#include <SDL_syswm.h>
#endif

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

void PrepareWindowFrame(void)
{
#if defined(RENDER_FRAMEWORK_IOS)
    // UIKit presents a nonzero framebuffer and color renderbuffer. Raylib's
    // offscreen helpers can leave framebuffer 0 bound; restore the current drawable.
    SDL_Window *window = SDL_GL_GetCurrentWindow();
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (window && SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_UIKIT &&
        info.info.uikit.framebuffer && info.info.uikit.colorbuffer)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, info.info.uikit.framebuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, info.info.uikit.colorbuffer);
    }
#endif
    // A depth clear honors the write mask and scissor left by the previous pass.
    rlEnableDepthMask();
    rlDisableScissorTest();
    rlDisableDepthTest();
}
