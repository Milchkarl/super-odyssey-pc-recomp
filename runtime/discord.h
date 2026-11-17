// Discord Rich Presence (runtime/discord.c): "Playing Super Odyssey PC Recomp - Cap Kingdom" on the player's profile.
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Starts the presence thread (once; a no-op with ODYSSEY_DISCORD=0). It never blocks the caller.
void discord_start(void);
// The stage the game just started (StageScene, e.g. "CapWorldHomeStage"): shown as its kingdom. Any thread.
void discord_set_stage(const char *stage);
// The kingdom shown for a stage name ("Cap Kingdom", in English), or NULL: not a kingdom's (kept from the last one)
const char *discord_kingdom_of(const char *stage);
#ifdef __cplusplus
}
#endif
