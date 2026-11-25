#ifndef DORF_MAZE_LAN_H
#define DORF_MAZE_LAN_H

#include "campaign.h"
#include <stdint.h>

enum { MAZE_LAN_PORT = 39273, MAZE_LAN_ADDRESS_CAPACITY = 22 };
typedef struct MazeLan MazeLan;
typedef struct MazeLanDiscovery MazeLanDiscovery;
enum { MAZE_LAN_GAMES = 4 };
typedef struct MazeLanGame {
    char address[MAZE_LAN_ADDRESS_CAPACITY];
    bool infinite;
} MazeLanGame;

bool AdvertiseMazeLan(MazeLan *lan, bool infinite);
MazeLanDiscovery *OpenMazeLanDiscovery(void);
void CloseMazeLanDiscovery(MazeLanDiscovery *discovery);
void PollMazeLanDiscovery(MazeLanDiscovery *discovery);
int ListMazeLanGames(const MazeLanDiscovery *discovery, MazeLanGame games[MAZE_LAN_GAMES]);
bool MazeLanAccessDenied(void);
void ResetMazeLanPermissionCheck(void);
// Wire payloads use 32-bit integers and floats on the supported little-endian targets.
typedef struct MazeLanInput {
    Vector2 move, look;
    uint32_t jump, sprint, continueRun, retry;
} MazeLanInput;
typedef struct MazeLanFrame {
    uint32_t stage, level, floor, flags, lives, boss, bossHealth, bossPhase;
    uint32_t health, death;
    float time, elevator, attackTimer, guardTimer;
    Vector3 feet, eye, target, hostFeet, bossPosition;
    // Each entry has position followed by its remaining lifetime.
    Vector4 orbs[CAMPAIGN_MAX_ORBS];
} MazeLanFrame;
enum {
    LAN_INFINITE = 1, LAN_SECOND = 2, LAN_CHECKPOINT = 4,
    LAN_CLEARED = 8, LAN_GAME_OVER = 16
};
typedef struct MazeLanLayout {
    uint32_t rows, cols;
    char tiles[CAMPAIGN_MAZE_SIDE * CAMPAIGN_MAZE_SIDE];
} MazeLanLayout;

MazeLan *OpenMazeLan(bool host, const char *address);
void CloseMazeLan(MazeLan *lan);
const char *MazeLanAddress(const MazeLan *lan);
bool MazeLanReady(const MazeLan *lan);
bool MazeLanFailed(const MazeLan *lan);
void PollMazeLan(MazeLan *lan);
bool SendMazeLanInput(MazeLan *lan, MazeLanInput *input);
bool ReceiveMazeLanInput(MazeLan *lan, MazeLanInput *input);
bool SendMazeLanFrame(MazeLan *lan, const MazeLanFrame *frame, const MazeLanLayout *layout);
bool ReceiveMazeLanFrame(MazeLan *lan, MazeLanFrame *frame, MazeLanLayout *layout, bool *changed);
#endif
