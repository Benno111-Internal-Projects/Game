#ifndef DORF_DEBUG_TOOLS_H
#define DORF_DEBUG_TOOLS_H

#include "input.h"
#include "player.h"

typedef struct DebugOptions {
    bool enabled, noDamage, noWalls, noSliding;
} DebugOptions;

typedef struct DebugTools {
    DebugOptions options;
    bool open, overlay;
} DebugTools;

typedef enum DebugAction { DEBUG_PLAY, DEBUG_BLOCK, DEBUG_TELEPORT } DebugAction;

DebugAction UpdateDebugTools(DebugTools *debug, InputPointer pointer, bool toggle, bool escape);
void DrawDebugTools(const DebugTools *debug, InputPointer pointer, const PlayerController *player,
                    const char *gameState, const char *progress);

#endif
