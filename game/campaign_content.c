#include "campaign_content.h"

const BossSpec campaignBosses[CAMPAIGN_BOSS_COUNT] = {
    { "Moss Bell",       { 137, 212, 106, 255 }, 2, 0.82f, 1.05f, 0.0f, 0.0f, 3.2f, { BOSS_PULSE, BOSS_PULSE } },
    { "Brook Lantern",  { 101, 209, 224, 255 }, 2, 0.74f, 1.15f, 1.35f, 0.7f, 2.7f, { BOSS_AIMED, BOSS_AIMED } },
    { "Copper Maw",     { 246, 164, 93, 255 },  3, 0.88f, 1.20f, 0.65f, 0.8f, 2.6f, { BOSS_FAN, BOSS_PULSE } },
    { "Ridge Spinner",  { 195, 154, 243, 255 }, 2, 0.78f, 1.15f, 1.1f, 1.1f, 2.5f, { BOSS_SPIRAL, BOSS_SPIRAL } },
    { "Gale Warden",    { 133, 209, 245, 255 }, 3, 0.80f, 1.25f, 1.9f, 1.25f, 2.3f, { BOSS_FAN, BOSS_AIMED } },
    { "Amber Tide",     { 251, 201, 96, 255 },  3, 0.91f, 1.15f, 1.1f, 0.9f, 2.15f, { BOSS_PULSE, BOSS_SPIRAL } },
    { "Glacier Eye",    { 162, 225, 255, 255 }, 4, 0.82f, 1.35f, 1.7f, 1.15f, 2.0f, { BOSS_AIMED, BOSS_FAN } },
    { "Cinder Regent",  { 251, 132, 108, 255 }, 3, 0.92f, 1.30f, 1.4f, 1.15f, 1.85f, { BOSS_FAN, BOSS_PULSE } },
    { "The Hollow Crown", { 249, 205, 133, 255 }, 4, 0.98f, 1.40f, 1.8f, 1.30f, 1.65f,
      { BOSS_SPIRAL, BOSS_FAN } },
};
