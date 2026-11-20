#ifndef RENDER_FRAMEWORK_RENDER_H
#define RENDER_FRAMEWORK_RENDER_H

#include "raylib.h"

// The returned skybox owns its cubemap. The caller owns the shader.
Model LoadSkybox(const char *path, Shader shader);
void DrawSky(Model sky, Vector3 cameraPosition);
void UnloadSkybox(Model sky);

// Releases textures owned by the imported model. Restore borrowed overrides first.
void UnloadImportedModelTextures(Model model);

#endif
