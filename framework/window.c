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
    if (rlGetVersion() == RL_OPENGL_ES_20)
    {
        // Raylib 5.5 binds the default batch's normal stream even when
        // its shader omits that attribute and no VAO is available.
        int *locations = rlGetShaderLocsDefault();
        if (locations[SHADER_LOC_VERTEX_NORMAL] < 0)
            locations[SHADER_LOC_VERTEX_NORMAL] = RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL;
    }
    // A depth clear honors the write mask and scissor left by the previous pass.
    rlEnableDepthMask();
    rlDisableScissorTest();
    rlDisableDepthTest();
}
