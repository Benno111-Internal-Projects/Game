# Dorfplatformer 10

A first-person 3D platformer built on benno111 engene and raylib.

- `game/` contains the campaign, editor, runtime assets, and shaders.
- The reusable `render-framework` library comes from [Benno111Engene](https://github.com/Benno111-Internal-Projects/Benno111Engene). `framework-revision.txt` pins the version used by this game.

Set `GAME_CODENAME` in `game/game.h` to change the window title and in-game heading, then rebuild.

## Build and run

Use CMake 3.20+ and a C compiler. CMake uses installed raylib 5.5 when available or fetches the pinned source. A Linux source build needs the raylib/GLFW development dependencies for X11 and OpenGL.

Clone the private framework repo with an account that has read access, then
check out the pinned revision. Use `-DBENNO111_FRAMEWORK_ROOT=/path/to/Benno111Engene`
if you already have that revision checked out elsewhere.

```sh
gh repo clone Benno111-Internal-Projects/Benno111Engene .deps/Benno111Engene
git -C .deps/Benno111Engene checkout --detach "$(cat framework-revision.txt)"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/benno111-engene
```

With a multi-config generator, look for the executable and its asset folders in `build/Release/`. The sample is a UV-mapped OBJ with a material and PNG texture. Drag to orbit, scroll to zoom, press **Space** to compare the custom lighting with raylib's default shader, or use **B** for sky visibility, **G** for grid visibility, and **R** to reset the camera.

On desktop, **F11** switches between borderless fullscreen and the previous window
size. Other games can call `ToggleGameFullscreen()` from `framework/window.h`.

## Main Campaign

Choose **Main Campaign** on the title screen. Pick any world, use **1–7**,
or press **Enter** in the picker to play the full campaign. The route crosses Willow Basin, Copper Dunes, Rainmire, Windbreak Highlands, Ember Expanse,
Frostglass Range, and Crown Plateau. Activate three beacons in each world, then
follow the heading in the HUD to the world gate. The last beacon reached is
your checkpoint. **R** retries it, and **Esc** opens the pause menu. Mobile uses
the on-screen movement stick, look gesture, Jump, and Pause controls. Hold
**Sprint** while moving to run. Press
jump anytime in the air to queue one jump for your next landing. On desktop,
hold **Left Shift** to sprint.

Every world spans 1,024 × 1,024 units and uses 32 × 32-unit terrain chunks.
The landscape builds nearby chunks as they enter the camera view and frees
chunks left behind. Collision samples the same terrain grid, including
terrain beyond the current view. Terrain blends ground, sediment, and rock
textures by elevation and slope. The textures use world coordinates so they
line up across chunks. The Sandbox terrain editor previews the same layers. Willow Basin has trees
that stream with its terrain chunks.

### Gameplay debug tools

Press **F3** on desktop or tap **Debug** on mobile during gameplay. The panel
lets you toggle damage, wall collision, slope sliding, and the live overlay
or teleport to a random playable location. The overlay shows mode and level
progress, player coordinates and velocity, and whether a jump is queued.
Mazes can continue past zero lives with debug mode active. **Exit debug mode**
restores normal rules and gives you one life if your count is zero or lower.

### Edit and ship world updates

Choose **Edit** beside a world in the campaign picker. Raise or lower terrain
by dragging or swiping the brush, or place the spawn, three beacons, and gate on the map.
**Save update** writes a numbered `.world` file to the game's writable data
directory and applies it locally. Return to the picker to play the edited
world. Saved edits load again on startup and take priority over bundled files.

To ship a world update, copy the saved `.world` file to the matching filename
under `game/assets/worlds/` and submit the change to GitHub. The game logs the
save path. Desktop installations can replace their files in `assets/worlds/`
and restart. Mobile builds receive bundled changes with the next app package.
GitHub Actions validates the world's text format and publishes the seven
files as **dorfplatformer-world-pack**. Run
`./build/benno111-engene --check-worlds game/assets/worlds` to check the pack
locally.

## Mazes bonus mode

Choose **Mazes** on the title screen. **Classic** has seven levels with
nine bosses and two minibosses. **Infinite** generates a new maze after every boss and cycles
through the nine bosses. Each generated maze has an independently random
width and height from 17 to 200 cells. The minimap follows the player in
large mazes.

Wall torches light the dark corridors and burn on contact. Each maze has three
floors. Walk into the marked elevator on floors 1 and 2 to ride up. On
floor 3, find the hidden passage to the enclosed boss room. Land on a boss from above
to damage it, jump over its attacks, and use the gate once the room is clear.
Several bosses change movement and attacks as they take damage. The Hollow Crown
has three phases.

Each run starts with three lives and three health points per life. Dying or
retrying spends one life and restarts the current floor. Reaching a boss room
sets a checkpoint for that maze. The death screen shows what killed you and how many lives remain.
Press **Enter** or select the respawn button to continue. Lives carry forward
between mazes, and the run ends when they run out.
Debug level select gives unlimited retries.

| Level | Bosses |
| --- | --- |
| Meadow Gate | Moss Bell |
| Brook Crossing | Brook Lantern |
| Quarry Steps | Copper Maw |
| Windmill Ridge | Ridge Spinner, Gale Warden |
| Lantern Marsh | Marsh Wisp → second maze → Amber Tide |
| Frost Terrace | Glacier Eye |
| Crown Keep | Crown Sentinel → second maze → Cinder Regent and The Hollow Crown |

On desktop, use **WASD**, mouse look, **Space** to jump, **Left Shift** to
sprint, **R** to spend a life and retry, and **Esc** to pause. On mobile, use the on-screen
stick, swipe-to-look, Jump, and the **Pause** button. The pause menu can resume
play, retry from the latest checkpoint, or return to the Mazes menu or debug
level picker.

**Debug Level Select** opens any stage directly. Click or tap its name, or
press **1–7** on a keyboard. Debug runs return to the picker after the stage
ends. Level layouts are in `game/assets/levels/`. Boss behavior is in `game/campaign_content.c`.
Choose **Sandbox** to play the editable scene.

### LAN co-op

Connect two devices to the same local network. On one device, open
**Mazes → LAN co-op** and choose **Host Classic** or **Host Infinite**.
On the other device, choose **Find games** and select the host. **Join by IP**
also accepts the address shown by the host, such as `192.168.1.20:39273`.
An address without a port uses 39273. Both players can explore, ride the
elevators, and fight bosses. Either player can advance the run, and lives
are shared.

The built-in host uses TCP port 39273 for play and UDP port 39273 for discovery.
On iOS, allow local network access when prompted. If the host does not appear
in the list, try **Join by IP**. Guest Wi-Fi isolation and firewalls can block
connections between devices.

### Edit a maze

Choose **Mazes → Edit mazes** and select a Classic level. Drag or swipe to
paint walls and passages, or use the spawn and exit tools. **Edit second
maze** switches layouts in levels 5 and 7. The editor sets the first-floor
layouts. The game generates the two upper floors for each maze. **Save update**
checks that every
passage connects to the spawn and exit, then applies the edit locally.
**Play saved** opens the saved level with unlimited retries and returns to
the editor when you leave.

The save path appears in the game log. To share a local edit through the
level pack, copy `maze-05.level` (or the matching number) to
`game/assets/levels/05.level`. Local saves take priority over the bundled
level on startup.

### Ship a level update

Edit `game/assets/levels/01.level` through `07.level` in GitHub and open a pull
request. The levels use `DORF-LEVEL 2` or `DORF-LEVEL 3`, followed by these fields in order.

| Field | Values |
| --- | --- |
| `name` | The level name shown in the HUD and debug selector |
| `stone`, `trim` | Three RGB integers from 0 to 255 |
| `bosses` | First boss ID (1–9) and number of consecutive bosses (1–2) |
| `maze` | Row count and column count, each from 7 to 31, followed by the grid |

Use `#` for walls, `.` for passages, `S` for one interior spawn, and `E` for one
exit on the top edge outside the corners. Each passage must connect to the
spawn, and the rest of the boundary must be walls. Each tile spans 2.5 world
units. The arena attaches to the exit automatically. The exit tile is rendered
as a wall that the player can walk through. Assign each of the nine boss IDs
once across the pack. Use **Debug Level Select** to play-test changes.
Levels 5 and 7 use `DORF-LEVEL 3` with two consecutive `maze` blocks. The first
maze leads to the miniboss and the second leads to the encounters listed in
`bosses`. The game also loads legacy `DORF-LEVEL 1` platform layouts.

Run `./build/benno111-engene --check-levels game/assets/levels` to validate the
pack without opening a window. GitHub Actions validates the bundled pack on
Linux and Windows, checks that all seven files are in the mobile packages,
and uploads a separate **dorfplatformer-level-pack** artifact. To update an
existing desktop build, replace its seven files under `assets/levels/` and
restart. Mobile builds receive the files in the next app package.

## Sandbox controls

Press **F** from orbit to take control of the player. Move with **WASD**, look
with the mouse, press **Space** to jump, and hold **Left Shift** to sprint.
Approach a crate, jump while moving toward it, then ease off the movement
keys to land on top. **R** resets the player. **F** or **Esc** switches back to
orbit, where the player avatar remains visible. On Android and iOS, use
**Player**, the on-screen stick, a right-side swipe to look, and **Jump**.

Terrain up to **45°** is walkable. The player stays grounded while moving
down a walkable slope and can jump from it. Steeper faces block uphill
movement and make the player slide downhill on contact.

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
- **Shaders** live alongside the executable after building. Edit the copied GLSL files and relaunch for quick iteration. Edit the source copies under `game/shaders/` and rebuild to keep changes in the project. Add new meshes and draw calls inside `BeginMode3D` to extend the scene.

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

In **Settings → Secrets and variables → Actions** for the Game repo, add
`YY` with Contents read access to the private
`Benno111Engene` repo. All four jobs check out the revision pinned in
`framework-revision.txt`. Fork pull requests require a trusted maintainer build
because GitHub withholds repository secrets from fork workflows.

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
from `game/shaders/es/`.

### Install on iOS with SideStore

1. Download the `benno111-engene-ios` artifact from the GitHub Actions run.
2. Extract the GitHub artifact ZIP and save `benno111-engene-ios.ipa` to Files on the device.
3. In SideStore, open **My Apps**, tap **+**, and select the IPA. SideStore signs and installs it.

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).

## Title screen and prefabs

The title screen includes a live scene preview. Choose **Sandbox** to enter
the demo. **Esc** leaves first-person mode, returns
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
Choose **Sandbox** to play the scene as edited.

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
