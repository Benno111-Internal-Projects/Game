#include "campaign_content.h"

const BossSpec campaignBosses[CAMPAIGN_BOSS_COUNT] = {
    { "Moss Bell", { 137, 212, 106, 255 }, 2, 0.82f, 1.05f, 1, {
        { 0.0f, 0.0f, 3.2f, { BOSS_PULSE, BOSS_PULSE } },
    } },
    { "Brook Lantern", { 101, 209, 224, 255 }, 4, 0.74f, 1.15f, 2, {
        { 1.35f, 0.7f, 2.7f, { BOSS_AIMED, BOSS_AIMED } },
        { 1.7f, 1.05f, 2.15f, { BOSS_FAN, BOSS_CROSS } },
    } },
    { "Copper Maw", { 246, 164, 93, 255 }, 4, 0.88f, 1.20f, 2, {
        { 0.65f, 0.8f, 2.6f, { BOSS_FAN, BOSS_PULSE } },
        { 1.2f, 1.2f, 2.05f, { BOSS_SWEEP, BOSS_PULSE } },
    } },
    { "Ridge Spinner", { 195, 154, 243, 255 }, 4, 0.78f, 1.15f, 2, {
        { 1.1f, 1.1f, 2.5f, { BOSS_SPIRAL, BOSS_SPIRAL } },
        { 1.5f, 1.45f, 1.95f, { BOSS_CROSS, BOSS_SWEEP } },
    } },
    { "Gale Warden", { 133, 209, 245, 255 }, 4, 0.80f, 1.25f, 2, {
        { 1.9f, 1.25f, 2.3f, { BOSS_FAN, BOSS_AIMED } },
        { 2.4f, 1.5f, 1.85f, { BOSS_SWEEP, BOSS_AIMED } },
    } },
    { "Amber Tide", { 251, 201, 96, 255 }, 4, 0.91f, 1.15f, 2, {
        { 1.1f, 0.9f, 2.15f, { BOSS_PULSE, BOSS_SPIRAL } },
        { 1.7f, 1.28f, 1.75f, { BOSS_CROSS, BOSS_FAN } },
    } },
    { "Glacier Eye", { 162, 225, 255, 255 }, 5, 0.82f, 1.35f, 2, {
        { 1.7f, 1.15f, 2.0f, { BOSS_AIMED, BOSS_FAN } },
        { 2.15f, 1.42f, 1.7f, { BOSS_SWEEP, BOSS_SPIRAL } },
    } },
    { "Cinder Regent", { 251, 132, 108, 255 }, 4, 0.92f, 1.30f, 2, {
        { 1.4f, 1.15f, 1.85f, { BOSS_FAN, BOSS_PULSE } },
        { 1.9f, 1.45f, 1.6f, { BOSS_SWEEP, BOSS_CROSS } },
    } },
    { "The Hollow Crown", { 249, 205, 133, 255 }, 6, 0.98f, 1.40f, 3, {
        { 1.8f, 1.30f, 1.85f, { BOSS_SPIRAL, BOSS_FAN } },
        { 2.2f, 1.48f, 1.6f, { BOSS_CROSS, BOSS_SWEEP } },
        { 2.6f, 1.65f, 1.35f, { BOSS_SWEEP, BOSS_SPIRAL } },
    } },
};

const BossSpec campaignMinibosses[CAMPAIGN_LEVEL_COUNT] = {
    [4] = { "Marsh Wisp", { 171, 239, 160, 255 }, 3, 0.62f, 0.95f, 1, {
        { 1.65f, 0.9f, 2.05f, { BOSS_AIMED, BOSS_FAN } },
    } },
    [6] = { "Crown Sentinel", { 228, 169, 227, 255 }, 4, 0.72f, 1.12f, 2, {
        { 1.25f, 1.15f, 2.05f, { BOSS_FAN, BOSS_PULSE } },
        { 1.75f, 1.50f, 1.60f, { BOSS_CROSS, BOSS_SWEEP } },
    } },
};
