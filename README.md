# benno111 engene

A native C99 demo for Windows, macOS, and Linux. raylib opens the window, loads the meshes and textures, and submits draws. The project has two parts:

- `framework/` builds the reusable `render-framework` library. It contains skybox rendering, imported texture cleanup, first-person camera math, and the GLSL shaders.
- `game/` builds the `benno111-engene` executable. It contains the scene, render loop, key bindings, movement speed, and sample assets.

Set `GAME_WINDOW_TITLE` in `game/game.h` to change the window title and in-game header, then rebuild.

## Build and run

Use CMake 3.20+ and a C compiler. CMake uses installed raylib 5.5 when available or fetches the pinned source. A Linux source build needs the raylib/GLFW development dependencies for X11 and OpenGL.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/benno111-engene
```

With a multi-config generator, look for the executable and its asset folders in `build/Release/`. The sample is a UV-mapped OBJ with a material and PNG texture. Drag to orbit, scroll to zoom, press **Space** to compare the custom lighting with raylib's default shader, or use **B** for sky visibility, **G** for grid visibility, and **R** to reset the camera.

On desktop, **F11** switches between borderless fullscreen and the previous window
size. Other games can call `ToggleGameFullscreen()` from `framework/window.h`.

## Player controls

Press **F** from orbit to take control of the player. Move with **WASD**, look
with the mouse, press **Space** to jump, and hold **Left Shift** to sprint.
Approach a crate, jump while moving toward it, then ease off the movement
keys to land on top. **R** resets the player. **F** or **Esc** switches back to
orbit, where the player avatar remains visible. On Android and iOS, use
**Player**, the on-screen stick, a right-side swipe to look, and **Jump**.

The example's crate instances are solid. The collision controller uses
axis-aligned box bounds, including conservative bounds for rotated crates.
Custom imported models are rendered without box collision. Framework users
can supply their own bounds and ground-height sampler through
`PlayerWorld` in `framework/player.h`.

## Bring your own assets

```sh
./build/benno111-engene path/to/mesh.glb
./build/benno111-engene path/to/mesh.obj path/to/texture.png
./build/benno111-engene path/to/mesh.gltf - path/to/sky-strip.png
```

- **Models** use raylib's loaders. OBJ, glTF, and GLB are the intended formats. Keep sidecar files such as OBJ materials and glTF textures next to the model or in the paths the model references.
- **Textures** use the model's first UV set and diffuse or base-color map. The optional second argument overrides that map across all materials. Use `-` to keep the imported maps. The demo shader uses a single diffuse texture, a material tint, and a directional light.
- **Skyboxes** accept a horizontal strip of six square faces in OpenGL order `+X, -X, +Y, -Y, +Z, -Z`. With no strip, the app creates a gradient cubemap.
- **Shaders** live alongside the executable after building. Edit the copied GLSL files and relaunch for quick iteration. Edit the source copies under `framework/shaders/` and rebuild to keep changes in the project. Add new meshes and draw calls inside `BeginMode3D` to extend the scene.

## Framework modules

- **Physics** (`framework/physics.h`) steps spheres with gravity, floor collision, bounce, and settling. The demo samples the height map when terrain is enabled and uses a flat plane elsewhere.
- **Input** (`framework/input.h`) reads game-defined key bindings and pointer state once per frame.
- **UI** (`framework/ui.h`) provides immediate-mode panels and buttons with pointer hit testing. A button activates on press and can be disabled.
- **Dynamic buffers** (`framework/dynamic_buffer.h`) provide a growable, GPU-backed triangle position buffer. Replace its active vertices with an upload, draw with a raylib material, and release it before closing the graphics context.

Click **Drop ball** in orbit mode or press **P** in either camera mode. The ball bounces and settles on the ground. Its ground marker updates through the dynamic vertex buffer while the ball moves.

## Scene, detail, and visibility

- **Scene** (`framework/scene.h`) draws a caller-owned list of objects. Each entry has world-space bounds, an enable flag, and a draw callback. Bounds must cover all geometry in the callback at every LOD.
- **Level of detail** (`framework/lod.h`) selects a level by distance from the camera to the center of the object's bounds. Give an object sorted distance thresholds, then use the selected level to choose geometry in its callback. The demo ball uses three levels of sphere tessellation.
- **Occlusion** (`framework/occlusion.h`) skips an object if one explicit opaque box fully hides its bounds. It keeps partially visible objects and ignores blockers intersecting the camera's near plane. Only flag an object as an opaque box if all of its LODs fully cover those bounds. The bundled crate qualifies. User-supplied meshes and texture overrides are left unflagged.

Press **C** to toggle occlusion culling and compare the onscreen draw counts. Drop the ball and orbit around the crate to see the ball culled when it is fully hidden.

## Game logic API

Include `game_logic.h` and call `RunGameLoop` after opening the raylib window. Pass
a `GameLogic` callback table and a pointer to your game state. The starter in
`game/main.c` shows a complete example.

- `init(state)` optionally loads resources. Return `false` on failure.
- `update(state, deltaTime)` handles input and advances gameplay. Delta time is
  in seconds. Return `false` to quit before drawing that frame.
- `draw(state)` renders inside the framework's `BeginDrawing` / `EndDrawing`
  pair. The callback can open its own 3D pass with `BeginMode3D`.
- `shutdown(state)` optionally releases resources. It runs before the caller
  closes the window, including after a failed initialization, so it must accept
  partially initialized state.

The caller owns the game state and window. The API runs synchronously on the
window thread. `RunGameLoop` returns `true` after a normal exit and `false`
if the callbacks or window are invalid, or initialization fails.

## Audio API

`framework/audio.h` wraps a raylib `Sound` in independent voices. Create an
alias per simultaneous playback, then call `PlayAudioVoice` with options from
`DefaultAudioVoiceOptions`. A 2D voice uses manual stereo pan. Set `spatial`
and `position` for 3D audio relative to the listener camera. Spatial options
include stereo width, a pan offset, minimum and maximum distance, and no,
linear, or inverse falloff. Audio uses stereo panning and distance gain.

Both modes support gain, pitch, mute, looping, pause, resume, stop, and playback
state. Call `UpdateAudioVoice` each frame to apply changes to the options and
listener. Loops restart on a game update and can have a short gap. Use raylib's
`SetMasterVolume` for device-wide gain.

Initialize raylib audio and load the source sound before creating voices.
Destroy all aliases before unloading their source and closing the audio device.
The demo plays a 2D cue when you drop the ball and a positional sound on impact.
Press **M** to mute or unmute the demo audio.

### Background music per scene

Set a scene's track in `SceneMusicSettings` from `framework/scene_music.h`.
The demo configures its scene in `InitStarter` in `game/main.c`.

```c
SceneMusicSettings music = {
    .path = "assets/starter-ambient.ogg",
    .volume = 0.28f,
    .loop = true,
};
SetSceneMusic(&sceneMusicPlayer, music);
```

Initialize the player to zero and load tracks with the raylib audio device
open. Call `SetSceneMusic` when entering a scene. A null or empty path stops
the current music. A failed load leaves it playing. Call `UpdateSceneMusic`
each frame so raylib can refill the stream, and unload the player before
closing the audio device. **M** mutes both music and effects in the demo.
The track continues advancing while muted.

## GitHub Actions builds

Put this project at the root of the GitHub repository so GitHub can discover
`.github/workflows/gamebuild.yml`. The **Game build** workflow runs on pushes,
pull requests, and manual dispatches. It uploads one artifact per platform.

| Platform | Artifact |
| --- | --- |
| Windows x64 | Release executable with game assets and shaders |
| Linux x64 | Tar archive with the release executable, game assets, and shaders |
| Android arm64 | Debug-signed APK |
| iOS arm64 | Unsigned device IPA for SideStore |

For Android local builds, install SDK 35, NDK `28.2.13676358`, CMake
`3.22.1`, Java 17, and Gradle `8.10.2`, then run
`gradle -p android assembleDebug`. iOS builds use the SDL2 backend,
Xcode, and CMake 3.24 or newer. Mobile builds load the OpenGL ES shaders
from `framework/shaders/es/`.

### Install on iOS with SideStore

1. Download the `benno111-engene-ios` artifact from the GitHub Actions run.
2. Extract the GitHub artifact ZIP and save `benno111-engene-ios.ipa` to Files on the device.
3. In SideStore, open **My Apps**, tap **+**, and select the IPA. SideStore signs and installs it.

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).

## Title screen and prefabs

The demo opens on a title screen with a live scene preview. Click **Start** or
press **Enter** to enter the demo. **Esc** leaves first-person mode, returns
from orbit to the title screen, and quits from the title screen. **M** or the
title screen's audio button toggles music and effects.

`framework/prefab.h` defines `ScenePrefab`, a reusable template with local
bounds, draw data, and optional LOD distances. Create a `PrefabInstance`
with a position, positive uniform scale, and enabled flag. Use
`MakePrefabSceneObject` to submit it to the scene. Rebuild that scene entry
after changing the instance so its world bounds and visibility stay in sync.
The caller owns the template, instance, and draw resources. The title preview
and game share a crate prefab, and the game updates a ball instance as physics
moves it.

## Desktop installers

The GitHub Actions run also uploads two installer artifacts. Both install for
the current user and include the game, assets, shaders, and the GPLv3 license.

| Platform | Installer | Removal |
| --- | --- | --- |
| Windows | Run `benno111-engene-windows-setup.exe`. Setup installs to `%LOCALAPPDATA%\benno111-engene` and offers Start menu and desktop shortcuts. | Use **Settings → Apps → Installed apps** or run `uninstall.exe` from the install folder. |
| Linux | Extract the installer tarball and run `./benno111-engene-linux-setup.run`. Setup installs under `${XDG_DATA_HOME:-$HOME/.local/share}/benno111-engene` and creates an applications-menu entry. | Run `uninstall` in the install folder, or run setup again with `--uninstall`. |

Linux setup prompts in the terminal. It can also use Zenity when launched from
a graphical desktop. Pass `--yes` for unattended installation or removal.
Windows setup and its uninstaller accept `/S` for silent mode.

Build a Linux installer with
`bash installers/build-linux.sh build benno111-engene-linux-setup.run`.
The Windows workflow compiles `installers/windows.nsi` with NSIS using the
desktop build directory, repository directory, and output filename.

## Scene editor

Open **Scene Editor** from the title screen. Use **Add model** or **Add sphere**
to place up to 64 objects. Click or tap an object to select it, or cycle with
**Previous** and **Next**. Move the selection along X, Y, or Z, rotate it around
Y, change its uniform scale, or delete it. Drag the preview to orbit and use
the zoom buttons or mouse wheel to zoom. **Menu** returns to the title screen.
Choose **Start** to play the scene as edited.

**Save** and **Load** use one versioned `edited-scene.benno` file. It lives in
the app's private storage on Android and iOS, in
`%LOCALAPPDATA%/benno111-engene` on Windows, or under
`${XDG_DATA_HOME:-$HOME/.local/share}/benno111-engene` on Linux. A new session
starts with the example scene. Use **Load** to restore a saved layout.

Prefabs can opt into yaw rotation with `ScenePrefab.drawRotated`. The scene
system updates their world bounds and skips using rotated boxes as occluders.

### Edit terrain

In the scene editor, choose **Terrain** to add a 20 × 20 world-unit
height map. Hold and drag a **Raise**, **Lower**, or **Level** brush over the
map. **Level** brings the surface back toward its starting height. Adjust its
size with **Radius −/+**, or choose **Orbit** to move the camera without
sculpting. Use **Objects** to return to prefab editing.

**Save** stores the terrain in the same scene file as the objects. Older
object-only saves still load. In play mode, the player follows the ground height while walking and the physics ball collides with the terrain. Place
prefabs on the terrain using the existing object controls.
