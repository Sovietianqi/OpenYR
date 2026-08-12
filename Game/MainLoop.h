#pragma once

#include "Core/Definitions.h"

// ═══════════════════════════════════════════════════════════════════════════
// MainLoop.h — main game loop functions
// ═══════════════════════════════════════════════════════════════════════════

// ── Main entry into the game loop (called from WinMain after init) ────────

void Main_Game();

// ── Per-frame update (called once per iteration of the main loop) ─────────

void Main_Loop();

// ── Sub-step functions ────────────────────────────────────────────────────

void Update_GameSpeed();
void ProcessWindowsMessages();
void Update_Input();
void Update_Network();
void Update_AI();
void Update_Objects();
void Update_Combat();
void Update_Locomotion();
void Update_Animations();
void Update_Particles();
void Update_SuperWeapons();
void Update_SpecialEffects();
void Update_Radar();
void Update_Sidebar();
void Update_Display();
void Render_Frame();
void Update_Audio();

// ── Frame timing ──────────────────────────────────────────────────────────

void Sync_FrameTime();
void Record_FrameStats();