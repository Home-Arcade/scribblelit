// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <3ds.h>
#include <stdbool.h>

// A small patch makes the games open this app first when a check is due. The app checks for an update
// (it only updates if there is a newer version), applies what's needed, and opens the game again.

// Every game with our code.ips points at this app, next check in `hours`. False if a file couldn't be
// written: then the caller must not send the player back to the game, which would come straight back.
bool sl_forward_install(u64 app, u32 hours, bool badge_arcade);
// The patch comes out of every game again (a game may then open normally without this app).
void sl_forward_off(void);
// The game that sent the player here, or 0 if the app was opened normally.
u64 sl_forward_caller(void);
// The update replaces this app, and the new one has to know which game to go back to.
void sl_forward_remember(u64 game, const char *version);
u64 sl_forward_remembered(const char *version);
