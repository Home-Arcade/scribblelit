// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stddef.h>

#define SL_FOLDER "sdmc:/3ds/ScribbleLit"
#define SL_IN_BADGECADE SL_FOLDER "/in-badgecade"  // the BadgeCade helper carries ScribbleLit's menu
#define MAX_REPLY 0x4000
#define SL_TITLE 0x0004000005CB1700ULL

// Each app supplies these two, drawn its own way. Choose returns -1 for B.
int sl_choose(const char *title, const char *text, const char *const *buttons, int count);
void sl_busy(const char *title, const char *text);

extern char sl_reply[MAX_REPLY];
bool sl_set_up(void);      // true once everything is in place
void sl_apply_after_update(void);
void sl_report_note(void);
bool sl_remove_all(void);  // true once removed
void sl_tell(const char *title, const char *text);
bool sl_file_exists(const char *path);
void sl_to_hex(const unsigned char *in, size_t size, char *out);
long sl_request(const char *path, const char *body, const char *signature);
bool sl_download(const char *path, const char *destination);
bool sl_sha256_matches(const char *path, const char *expected);
bool sl_install_cia(const char *sd_path);
bool sl_is_set_up(void);
bool sl_own_app_installed(void);
bool sl_move_to_helper(void);   // true once its own icon is gone
bool sl_move_to_own_app(void);  // true once its own app is back
