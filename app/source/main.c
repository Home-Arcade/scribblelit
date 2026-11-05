// SPDX-License-Identifier: GPL-3.0-or-later
// ScribbleLit: gets a 3DS ready for friend notes in Swapdoodle and Swapnote and for
// Flipnote Studio 3D's gallery. It only installs things; the games do the rest.
#include <3ds.h>
#include <curl/curl.h>
#include <malloc.h>
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "sl_core.h"
#include "sl_forward.h"
#include "ui.h"

#define SOC_SIZE 0x100000
#define UPDATE_FILE SL_FOLDER "/update.cia"
#define UPDATE_SD_PATH "/3ds/ScribbleLit/update.cia"
#define UPDATER_FILE SL_FOLDER "/updater.cia"
#define UPDATER_SD_PATH "/3ds/ScribbleLit/updater.cia"
#define UPDATER_ID 0x0004000005CB1900ULL
#define BADGECADE_ID 0x000400000BADCA00ULL
#define BADGECADE_WITH_MENU ((1 << 10) | (4 << 4) | 15)  // 1.4.15: its helper carries ScribbleLit and hands it back

int sl_choose(const char *title, const char *text, const char *const *buttons, int count) {
    return ui_choose(title, text, buttons, count);
}

void sl_busy(const char *title, const char *text) { ui_busy(title, text); }

// With a new enough BadgeCade helper there's no need for a second icon: its menu takes over.
static bool helper_has_menu(void) {
    u64 helper = BADGECADE_ID;
    AM_TitleEntry entry;
    return R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &helper, &entry)) && entry.version >= BADGECADE_WITH_MENU;
}

static bool move_into_badgecade(void) {
    if (!helper_has_menu() || !sl_move_to_helper()) return false;  // the letter on the HOME Menu says where it went
    // the HOME Menu can lose an icon when a title is removed like this, until it reloads
    static const char *const choices[] = {"Restart now", "Later"};
    if (ui_choose("Restart your 3DS?", "Restart so your HOME Menu shows the right icons.", choices, 2) == 0) APT_HardwareResetAsync();
    return true;
}

// ---- updates ----

static u32 packed_version(const char *text) {
    unsigned major = 0, minor = 0, micro = 0;
    sscanf(text, "%u.%u.%u", &major, &minor, &micro);
    return major << 16 | minor << 8 | micro;
}

// How often the games come here for a check; 0 (no forward.txt on the server) takes the patch out again.
static u32 forward_hours(bool *known) {
    unsigned hours = 0;
    long status = sl_request("/forward.txt", NULL, NULL);
    *known = status == 200 || status == 404;
    if (status == 200) sscanf(sl_reply, "%u", &hours);
    return hours;
}

// True when the updater should take over as this app closes.
static bool check_update(void) {
    static const char *const choices[] = {"Update now", "Later"};
    char version[16], digest[65], text[128];
    if (sl_request("/app.txt", NULL, NULL) != 200 || sscanf(sl_reply, "%15s %64s", version, digest) != 2) return false;
    if (packed_version(version) <= packed_version(APP_VERSION)) return false;
    snprintf(text, sizeof text, "Version %s is ready (you have " APP_VERSION ").", version);
    if (ui_choose("Update available", text, choices, 2) != 0) return false;
    ui_busy("Updating...", "Downloading...");
    if (!sl_download("/ScribbleLit.cia", UPDATE_FILE) || !sl_sha256_matches(UPDATE_FILE, digest)) {
        remove(UPDATE_FILE);
        sl_tell("Update", "The download didn't work. Your app wasn't changed. Try again later.");
        return false;
    }
    FILE *in = fopen("romfs:/updater.cia", "rb"), *out = in ? fopen(UPDATER_FILE, "wb") : NULL;
    if (out) {
        static char chunk[0x4000];
        size_t got;
        while ((got = fread(chunk, 1, sizeof chunk, in)) > 0) fwrite(chunk, 1, got, out);
        fclose(out);
    }
    if (in) fclose(in);
    AM_DeleteTitle(MEDIATYPE_SD, UPDATER_ID);
    bool ok = out && sl_install_cia(UPDATER_SD_PATH);
    remove(UPDATER_FILE);
    if (!ok) {
        sl_tell("Update", "Couldn't start the update. Your app wasn't changed.");
        return false;
    }
    aptSetChainloader(UPDATER_ID, MEDIATYPE_SD);
    return true;
}

int main(void) {
    // a game sent the player here for a check, or the update of that check just finished
    u64 back_to_game = sl_forward_caller();
    u64 after_update = sl_forward_remembered(APP_VERSION);
    if (!back_to_game) back_to_game = after_update;
    ui_init();
    romfsInit();
    amInit();
    cfguInit();
    frdInit(true);
    u32 *soc = memalign(0x1000, SOC_SIZE);
    bool net = soc && R_SUCCEEDED(socInit(soc, SOC_SIZE));
    curl_global_init(CURL_GLOBAL_DEFAULT);
    AM_DeleteTitle(MEDIATYPE_SD, UPDATER_ID);
    remove(UPDATE_FILE);

    if (!helper_has_menu()) remove(SL_IN_BADGECADE);  // the helper's gone: this app is ScribbleLit's home again
    // set up already, and the helper arrived later
    bool leaving = sl_is_set_up() && move_into_badgecade();
    // straight away, so a game can't send the player back here again if anything below goes wrong
    if (back_to_game && !leaving && !sl_forward_install(SL_TITLE, 1, false)) back_to_game = 0;
    if (back_to_game) ui_busy("ScribbleLit", "Just checking for updates. Your game's coming right up!");
    if (back_to_game && !leaving && net) sl_forward_remember(back_to_game, APP_VERSION);
    leaving = leaving || (net && check_update());
    if (!leaving) {
        remove(SL_FOLDER "/return-to-game");  // no update happened
        sl_apply_after_update();
        bool known = false;
        u32 hours = net ? forward_hours(&known) : 0;
        if (known && hours) { if (!sl_forward_install(SL_TITLE, hours, false)) back_to_game = 0; }
        else if (known) sl_forward_off();
    }
    if (back_to_game && !leaving) {
        aptSetChainloader(back_to_game, MEDIATYPE_SD);
        leaving = true;
    }
    static const char *const menu[] = {"Set up ScribbleLit", "Report a note", "Remove ScribbleLit", "Exit"};
    while (!leaving && aptMainLoop()) {
        int pick = ui_choose("ScribbleLit " APP_VERSION, "Friend notes for Swapdoodle and Swapnote, and Flipnote Studio 3D's gallery.\n\nYou only need to set up once. After that, just play your games.", menu, 4);
        if (pick == 0 && net) leaving = sl_set_up() && move_into_badgecade();
        else if (pick == 1 && net) sl_report_note();
        else if (pick == 2) sl_remove_all();
        else if (pick < 2 && pick >= 0) sl_tell("No network", "Check your Wi-Fi and try again.");
        else break;
    }

    curl_global_cleanup();
    if (net) socExit();
    free(soc);
    frdExit();
    cfguExit();
    amExit();
    romfsExit();
    ui_exit();
    return 0;
}
