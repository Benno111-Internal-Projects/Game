#include "render.h"
#include "rlgl.h"

#include <math.h>

static TextureCubemap LoadSky(const char *path)
{
    const int size = 128;
    Image image;
    if (path)
    {
        image = LoadImage(path);
        if (!IsImageValid(image)) return (TextureCubemap){ 0 };
        if (image.width != 6 * image.height)
        {
            UnloadImage(image);
            TraceLog(LOG_ERROR, "Skybox must be a horizontal strip of six square faces");
            return (TextureCubemap){ 0 };
        }
    }
    else
    {
        image = GenImageColor(size, size * 6, BLACK);
        // OpenGL cubemap order: +X, -X, +Y, -Y, +Z, -Z.
        for (int face = 0; face < 6; ++face)
        {
            for (int y = 0; y < size; ++y)
            {
                for (int x = 0; x < size; ++x)
                {
                    float u = 2.0f * (x + 0.5f) / size - 1.0f;
                    float v = 2.0f * (y + 0.5f) / size - 1.0f;
                    float height = face == 2 ? 1.0f : (face == 3 ? -1.0f : -v);
                    height /= sqrtf(1.0f + u*u + v*v);
                    Color horizon = { 136, 179, 200, 255 };
                    Color zenith = { 25, 60, 110, 255 };
                    Color ground = { 42, 58, 66, 255 };
                    Color pixel = ColorLerp(horizon, height < 0 ? ground : zenith, fabsf(height));
                    ImageDrawPixel(&image, x, y + face * size, pixel);
                }
            }
        }
    }
    TextureCubemap cubemap = LoadTextureCubemap(image,
        path ? CUBEMAP_LAYOUT_LINE_HORIZONTAL : CUBEMAP_LAYOUT_LINE_VERTICAL);
    UnloadImage(image);
    return cubemap;
}

void DrawSky(Model sky, Vector3 cameraPosition)
{
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    DrawModel(sky, cameraPosition, 1.0f, WHITE);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

void UnloadImportedModelTextures(Model model)
{
    // Imported materials can share textures. Clear later references before deleting an ID.
    for (int material = 0; material < model.materialCount; ++material)
    {
        for (int map = 0; map < (MATERIAL_MAP_BRDF + 1); ++map)
        {
            Texture2D texture = model.materials[material].maps[map].texture;
            if (texture.id == 0 || texture.id == rlGetTextureIdDefault()) continue;
            for (int later = material; later < model.materialCount; ++later)
            {
                for (int slot = later == material ? map : 0; slot < (MATERIAL_MAP_BRDF + 1); ++slot)
                {
                    if (model.materials[later].maps[slot].texture.id == texture.id)
                        model.materials[later].maps[slot].texture.id = 0;
                }
            }
            UnloadTexture(texture);
        }
    }
}

Model LoadSkybox(const char *path, Shader shader)
{
    TextureCubemap cubemap = LoadSky(path);
    if (!IsTextureValid(cubemap)) return (Model){ 0 };

    Model sky = LoadModelFromMesh(GenMeshCube(40, 40, 40));
    shader.locs[SHADER_LOC_MAP_CUBEMAP] = GetShaderLocation(shader, "skybox");
    // DrawMesh binds the cube UVs even when they are unused on the non-VAO path.
    if (shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] < 0)
        shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD;
    sky.materials[0].shader = shader;
    SetMaterialTexture(&sky.materials[0], MATERIAL_MAP_CUBEMAP, cubemap);
    return sky;
}

void UnloadSkybox(Model sky)
{
    if (!sky.meshCount) return;
    UnloadTexture(sky.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture);
    UnloadModel(sky);
}
