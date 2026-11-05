// SPDX-License-Identifier: GPL-3.0-or-later
// ScribbleLit's setup, reports and removal, shared by its own app and the BadgeCade helper.
#include "sl_core.h"
#include <3ds.h>
#include <curl/curl.h>
#include <mbedtls/md.h>
#include <mbedtls/md5.h>
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "letter_art.h"
#include "sl_find.h"
#include "sl_forward.h"
#include "sl_patches.h"

#define SERVER_SECURE "https://sl.badgecade.com"
#define SERVER_PLAIN "sl.badgecade.com:18100"
#define BOSS_TITLE 0x0004013000003402ULL
#define BOSS_VERSION 14337  // system 11.17.0, the only one the patch fits
#define BOSS_PATCH "sdmc:/luma/sysmodules/0004013000003402.ips"
#define BOSS_SAVED SL_FOLDER "/spotpass-patch-before.ips"  // another app's patch, back in place on Remove
#define SETTINGS_USA 0x0004001000021000ULL
#define INSTALLED SL_FOLDER "/installed.txt"
#define AGE_CHANGED SL_FOLDER "/age-flag-changed"  // versions 0.1.4 and 0.1.5 cleared this setting
#define AGE_BLOCK 0xC0001
#define PRETENDO_SLOT 2
#define OWN_APP_FILE SL_FOLDER "/own-app.cia"
#define OWN_APP_SD_PATH "/3ds/ScribbleLit/own-app.cia"
#define APPLIED_VERSION SL_FOLDER "/applied-version"

char sl_reply[MAX_REPLY];
static size_t reply_size;

bool sl_file_exists(const char *path) { return access(path, F_OK) == 0; }

static bool make_folders(const char *path) {
    char folder[128];
    for (const char *slash = strchr(path + 6, '/'); slash; slash = strchr(slash + 1, '/')) {
        snprintf(folder, sizeof folder, "%.*s", (int)(slash - path), path);
        mkdir(folder, 0777);
    }
    return true;
}

static bool write_file(const char *path, const void *data, size_t size) {
    make_folders(path);
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    return !fclose(f) && ok;
}

static bool same_file(const char *path, const unsigned char *data, size_t size) {
    static unsigned char buffer[256];
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t got = fread(buffer, 1, sizeof buffer, f);
    fclose(f);
    return got == size && !memcmp(buffer, data, size);
}

static bool listed(const char *path) {
    char line[128];
    FILE *f = fopen(INSTALLED, "r");
    if (!f) return false;
    bool found = false;
    while (!found && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        found = !strcmp(line, path);
    }
    fclose(f);
    return found;
}

// ---- network ----

static size_t to_reply(void *data, size_t size, size_t count, void *user) {
    size_t bytes = size * count, room = MAX_REPLY - 1 - reply_size;
    size_t taken = bytes < room ? bytes : room;
    memcpy(sl_reply + reply_size, data, taken);
    reply_size += taken;
    sl_reply[reply_size] = 0;
    return bytes;
}

static size_t to_file(void *data, size_t size, size_t count, void *user) {
    return fwrite(data, size, count, user) * size;
}

static void common(CURL *c, const char *url, struct curl_blob *ca) {
    ca->data = (void *)ISRG_ROOT_X1;
    ca->len = sizeof ISRG_ROOT_X1 - 1;
    ca->flags = CURL_BLOB_NOCOPY;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "ScribbleLit/" APP_VERSION " (3DS)");
    curl_easy_setopt(c, CURLOPT_CAINFO_BLOB, ca);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 0L);
}

// GET, or POST when body is set; the answer lands in sl_reply. Returns the HTTP status, 0 if none.
long sl_request(const char *path, const char *body, const char *signature) {
    char url[128], header[64];
    long status = 0;
    struct curl_blob ca;
    struct curl_slist *headers = NULL;
    reply_size = 0;
    sl_reply[0] = 0;
    CURL *c = curl_easy_init();
    if (!c) return 0;
    snprintf(url, sizeof url, SERVER_SECURE "%s", path);
    common(c, url, &ca);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, to_reply);
    if (body) curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    if (signature) {
        snprintf(header, sizeof header, "X-Sign: %s", signature);
        headers = curl_slist_append(headers, header);
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    }
    if (curl_easy_perform(c) == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(c);
    return status;
}

bool sl_download(const char *path, const char *destination) {
    char url[128];
    long status = 0;
    struct curl_blob ca;
    make_folders(destination);
    FILE *f = fopen(destination, "wb");
    if (!f) return false;
    CURL *c = curl_easy_init();
    if (!c) {
        fclose(f);
        return false;
    }
    snprintf(url, sizeof url, SERVER_SECURE "%s", path);
    common(c, url, &ca);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 180L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, to_file);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
    if (curl_easy_perform(c) == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(c);
    return !fclose(f) && status == 200;
}

// ---- who this console is ----

void sl_to_hex(const unsigned char *in, size_t size, char *out) {
    for (size_t i = 0; i < size; i++) sprintf(out + i * 2, "%02x", in[i]);
}

static bool pretendo_mode(void) {
    u8 slot = 1;
    return R_SUCCEEDED(FRD_GetMyLocalAccountId(&slot)) && slot == PRETENDO_SLOT;
}

static u32 my_pid(void) {
    FriendKey key;
    return R_SUCCEEDED(FRD_GetMyFriendKey(&key)) ? key.principalId : 0;
}

// The key SpotPass signs every call with. The password itself never leaves the 3DS.
static bool console_key(u32 pid, unsigned char key[16]) {
    static char password[0x800];
    memset(password, 0, sizeof password);
    if (R_FAILED(FRD_GetMyPassword(password, sizeof password)) || !password[0]) return false;
    mbedtls_md5((const unsigned char *)password, strlen(password), key);
    for (u32 i = 1; i < 65000 + pid % 1024; i++) mbedtls_md5(key, 16, key);
    memset(password, 0, sizeof password);
    return true;
}

// In Pretendo mode that key could act as a live account, so it stays here. A random token stands in.
static bool console_token(u32 pid, unsigned char token[16]) {
    char path[64];
    snprintf(path, sizeof path, SL_FOLDER "/token-%lu", (unsigned long)pid);
    FILE *f = fopen(path, "rb");
    if (f) {
        bool ok = fread(token, 1, 16, f) == 16;
        fclose(f);
        return ok;
    }
    bool ps = R_SUCCEEDED(psInit());
    bool random = ps && R_SUCCEEDED(PS_GenerateRandomBytes(token, 16));
    if (ps) psExit();
    return random && write_file(path, token, 16);
}

static bool request_key(u32 pid, unsigned char key[16]) {
    return pretendo_mode() ? console_token(pid, key) : console_key(pid, key);
}

static void sign(const unsigned char key[16], const char *body, char out[33]) {
    unsigned char mac[16];
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_MD5), key, 16, (const unsigned char *)body, strlen(body), mac);
    sl_to_hex(mac, 16, out);
}

// ---- patches ----

// Why the last game couldn't be patched, for the "All set" screen.
static char patch_problem[64];

static unsigned char *ips_add(unsigned char *at, long offset, const void *data, u16 size) {
    *at++ = offset >> 16;
    *at++ = offset >> 8;
    *at++ = offset;
    *at++ = size >> 8;
    *at++ = size;
    memcpy(at, data, size);
    return at + size;
}

// One of ours even if the list of installed patches is gone: it points at badgecade.com or carries the
// launch check's "BCFW". It can be replaced.
static bool ours_from_before(const char *path) {
    static char held[4096];
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t got = fread(held, 1, sizeof held, f);
    fclose(f);
    return memmem(held, got, "badgecade.com", 13) || memmem(held, got, "BCFW", 4);
}

static bool install_patch(const char *path, const unsigned char *start, unsigned char *end) {
    // somebody else's patch stays where it is
    if (sl_file_exists(path) && !listed(path) && !ours_from_before(path)) {
        snprintf(patch_problem, sizeof patch_problem, "another patch is already there");
        return false;
    }
    memcpy(end, "EOF", 3);
    if (!write_file(path, start, end + 3 - start)) {
        snprintf(patch_problem, sizeof patch_problem, "couldn't write to the SD card");
        return false;
    }
    if (!listed(path)) {
        make_folders(INSTALLED);
        FILE *f = fopen(INSTALLED, "a");
        if (f) {
            fprintf(f, "%s\n", path);
            fclose(f);
        }
    }
    return true;
}

// Swaps a text address in the code for ours, wherever this region's version keeps it.
static bool swap_address(const u8 *code, size_t size, const char *old_text, const char *new_text, const char *path) {
    unsigned char ips[128];
    char padded[64] = {0};
    size_t room = strlen(old_text) + 1;
    long at = code_find(code, size, old_text, room);
    if (at < 0 || strlen(new_text) >= room || room > sizeof padded) {
        snprintf(patch_problem, sizeof patch_problem, "this version isn't known yet");
        return false;
    }
    strcpy(padded, new_text);
    memcpy(ips, "PATCH", 5);
    return install_patch(path, ips, ips_add(ips + 5, at, padded, room));
}

// The code that actually runs: an installed update's if there is one. Its patch still goes in the game's own folder.
static u8 *read_game(u64 title, size_t *size) {
    u64 update = title | 0x0000000E00000000ULL;
    AM_TitleEntry entry;
    u8 *code = code_read(R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &update, &entry)) ? update : title, MEDIATYPE_SD, size);
#ifdef TEST_UNREADABLE
    free(code);
    code = NULL;
    code_read_error = 0xD8E0000F;
#endif
    if (!code) snprintf(patch_problem, sizeof patch_problem, "couldn't read the game (%08lX)", (unsigned long)code_read_error);
    return code;
}

// The game can't be read on this console, but this exact US version needs a patch we already have.
static bool install_known_patch(u64 title) {
    static const struct { u64 title; u16 version; const unsigned char *ips; size_t size; } known[] = {
        {0x00040000001A2D00ULL, 4160, SWAPDOODLE_IPS, sizeof SWAPDOODLE_IPS},
        {0x0004000000051700ULL, 3120, SWAPNOTE_IPS, sizeof SWAPNOTE_IPS},
        {0x00040000000C6600ULL, 2128, FS3D_IPS, sizeof FS3D_IPS},
    };
    static unsigned char copy[128];
    u64 update = title | 0x0000000E00000000ULL;
    AM_TitleEntry entry;
    if (R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &update, &entry)) || R_FAILED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &title, &entry))) return false;
    for (size_t i = 0; i < sizeof known / sizeof *known; i++) {
        if (known[i].title != title || known[i].version != entry.version || known[i].size > sizeof copy) continue;
        char path[64];
        snprintf(path, sizeof path, "sdmc:/luma/titles/%016llX/code.ips", title);
        memcpy(copy, known[i].ips, known[i].size);
        return install_patch(path, copy, copy + known[i].size - 3);
    }
    return false;
}

static bool patch_swapdoodle(u64 title) {
    size_t size;
    char path[64];
    u8 *code = read_game(title, &size);
    if (!code) return false;
    snprintf(path, sizeof path, "sdmc:/luma/titles/%016llX/code.ips", title);
    // Each region's game has its own 16-character code after this; the relay keeps it
    static const char base[] = "https://npdl.cdn.nintendowifi.net/p01/nsa/";
    bool ok = false;
    long at = code_find(code, size, base, sizeof base - 1);
    if (at >= 0 && strnlen((const char *)code + at, 64) == sizeof base - 1 + 16) {
        char new_text[64];
        snprintf(new_text, sizeof new_text, "http://" SERVER_PLAIN "/p01/nsa/%s", (const char *)code + at + sizeof base - 1);
        ok = swap_address(code, size, (const char *)code + at, new_text, path);
    } else {
        snprintf(patch_problem, sizeof patch_problem, "this version isn't known yet");
    }
    free(code);
    return ok;
}

// The check that says "the service has ended": found by its shape, makes it say "running".
static bool patch_swapnote(u64 title) {
    static const u32 returns_one[] = {0xE3A00001, 0xE12FFF1E};
    size_t size;
    char path[64];
    unsigned char ips[64];
    u8 *code = read_game(title, &size);
    if (!code) return false;
    long at = -1;
    for (size_t i = 0; i + 16 <= size; i += 4) {
        u32 *word = (u32 *)(code + i);
        if (word[0] != 0xE92D4010 || word[2] != 0xE3A01005 || (word[1] >> 24) != 0xEB || (word[3] >> 24) != 0xEB) continue;
        if (at >= 0) {  // not sure which one: leave the game alone
            free(code);
            snprintf(patch_problem, sizeof patch_problem, "this version isn't known yet");
            return false;
        }
        at = i;
    }
    free(code);
    if (at < 0) {
        snprintf(patch_problem, sizeof patch_problem, "this version isn't known yet");
        return false;
    }
    snprintf(path, sizeof path, "sdmc:/luma/titles/%016llX/code.ips", title);
    memcpy(ips, "PATCH", 5);
    return install_patch(path, ips, ips_add(ips + 5, at, returns_one, sizeof returns_one));
}

static bool patch_flipnote(u64 title, long nasc_branch) {
    static const u8 branch_if_equal[] = {0x04, 0x00, 0x00, 0x0A}, nothing[4] = {0};
    static const char old_text[] = "https://web.jkz.ctr.app.nintendo.net";
    size_t size;
    char path[64], padded[sizeof old_text] = "http://" SERVER_PLAIN "/fs3d";
    unsigned char ips[96];
    u8 *code = read_game(title, &size);
    if (!code) return false;
    long at = code_find(code, size, old_text, sizeof old_text);
    bool fits = at >= 0 && (size_t)nasc_branch + 4 <= size && !memcmp(code + nasc_branch, branch_if_equal, 4);
    free(code);
    if (!fits) {
        snprintf(patch_problem, sizeof patch_problem, "this version isn't known yet");
        return false;
    }
    snprintf(path, sizeof path, "sdmc:/luma/titles/%016llX/code.ips", title);
    memcpy(ips, "PATCH", 5);
    return install_patch(path, ips, ips_add(ips_add(ips + 5, nasc_branch, nothing, 4), at, padded, sizeof padded));
}

// US and Canadian consoles: the birthday step games send you to asks Nintendo's age server, which is gone.
static bool patch_settings(void) {
    size_t size;
    u8 *code = code_read(SETTINGS_USA, MEDIATYPE_NAND, &size);
    if (!code) return false;
    bool ok = swap_address(code, size, "https://cpx.c.app.nintendowifi.net/checkage", "http://" SERVER_PLAIN "/checkage",
                           "sdmc:/luma/titles/0004001000021000/code.ips");
    free(code);
    return ok;
}

// ---- screens ----

void sl_tell(const char *title, const char *text) {
    static const char *const ok[] = {"OK"};
    sl_choose(title, text, ok, 1);
}

static bool has_title(u64 title) {
    AM_TitleEntry entry;
    return R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &title, &entry));
}

// Every game on this console; what happened goes in report.
static bool patch_games(char *report, size_t room) {
    static const struct { const char *name; u64 title; int kind; long extra; } games[] = {
        {"Swapdoodle", 0x00040000001A2D00ULL, 0, 0}, {"Swapdoodle", 0x00040000001A2E00ULL, 0, 0}, {"Swapdoodle", 0x00040000001A2C00ULL, 0, 0},
        {"Swapnote", 0x0004000000051700ULL, 1, 0}, {"Letter Box", 0x0004000000051800ULL, 1, 0}, {"Swapnote", 0x0004000000051600ULL, 1, 0},
        {"Flipnote Studio 3D", 0x00040000000C6600ULL, 2, 2133192}, {"Flipnote Studio 3D", 0x00040000000C6700ULL, 2, 2133192}, {"Flipnote Studio 3D", 0x0004000000056C00ULL, 2, 2229332},
    };
    size_t used = 0;
    bool all_ok = true;
    for (size_t i = 0; i < sizeof games / sizeof *games; i++) {
        if (!has_title(games[i].title)) continue;
        patch_problem[0] = 0;
        bool ok = games[i].kind == 0 ? patch_swapdoodle(games[i].title)
                : games[i].kind == 1 ? patch_swapnote(games[i].title) : patch_flipnote(games[i].title, games[i].extra);
        if (!ok && !strncmp(patch_problem, "couldn't read", 13)) ok = install_known_patch(games[i].title);
        all_ok = all_ok && ok;
        if (used < room) {
            if (ok) used += snprintf(report + used, room - used, "%s: ready\n", games[i].name);
            else used += snprintf(report + used, room - used, "%s: couldn't be patched: %s\n", games[i].name, patch_problem);
        }
    }
    return all_ok;
}

// After an update: whatever the new version does differently goes on by itself, no questions (the
// player agreed when they set ScribbleLit up). Only once per version.
// Luma ignores every patch on the SD card unless these are on, and nothing tells the player. Takes effect at the next restart.
static bool luma_set_on(char *text, const char *key) {
    char *at = strstr(text, key);
    char *value = at ? strchr(at, '=') : NULL;
    if (!value) return false;
    value++;
    while (*value == ' ') value++;
    if (*value != '0') return false;
    *value = '1';
    return true;
}

static void luma_allow_patches(void) {
    static char text[8192];
    FILE *f = fopen("sdmc:/luma/config.ini", "rb");
    if (!f) return;
    size_t size = fread(text, 1, sizeof text - 1, f);
    bool whole = feof(f);
    fclose(f);
    text[size] = 0;
    if (!whole) return;
    bool changed = luma_set_on(text, "enable_game_patching");
    changed = luma_set_on(text, "enable_external_firm_and_modules") || changed;
    if (!changed) return;
    remove("sdmc:/luma/config.ini.scribblelit-before");
    rename("sdmc:/luma/config.ini", "sdmc:/luma/config.ini.scribblelit-before");
    if (!write_file("sdmc:/luma/config.ini", text, size)) rename("sdmc:/luma/config.ini.scribblelit-before", "sdmc:/luma/config.ini");
}

void sl_apply_after_update(void) {
    char before[16] = "";
    AM_TitleEntry entry;
    u64 boss = BOSS_TITLE;
    FILE *f = fopen(APPLIED_VERSION, "r");
    if (f) {
        if (fscanf(f, "%15s", before) != 1) before[0] = 0;
        fclose(f);
    }
    if (!strcmp(before, APP_VERSION) || !sl_is_set_up()) return;
    sl_busy("Getting your games ready...", "Applying the new version...");
    luma_allow_patches();
    if (R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_NAND, 1, &boss, &entry)) && entry.version == BOSS_VERSION &&
        (!sl_file_exists(BOSS_PATCH) || same_file(BOSS_PATCH, BOSS_IPS, sizeof BOSS_IPS) || ours_from_before(BOSS_PATCH)))
        write_file(BOSS_PATCH, BOSS_IPS, sizeof BOSS_IPS);
    static char report[320];
    patch_games(report, sizeof report);
    patch_settings();
    write_file(APPLIED_VERSION, APP_VERSION, strlen(APP_VERSION));
}

bool sl_set_up(void) {
    static const char *const go[] = {"Set up", "Not now"};
    AM_TitleEntry entry;
    u64 boss = BOSS_TITLE;
    char text[640];
    if (sl_choose("Set up ScribbleLit",
                  "This changes where SpotPass sends notes and sends a code made from your friend account to sl.badgecade.com "
                  "(not your password; in Pretendo mode not even that).\n\nNotes aren't encrypted on the way, and only "
                  "the friends you choose receive them.", go, 2) != 0)
        return false;
    if (R_FAILED(AM_GetTitleInfo(MEDIATYPE_NAND, 1, &boss, &entry)) || entry.version != BOSS_VERSION)
        { sl_tell("Update your 3DS first", "ScribbleLit needs system version 11.17.0. Nothing changed."); return false; }
    // The 3DS takes one SpotPass patch. Another app's stays on the SD card even with that app's SpotPass turned off.
    bool swap = sl_file_exists(BOSS_PATCH) && !same_file(BOSS_PATCH, BOSS_IPS, sizeof BOSS_IPS) && !ours_from_before(BOSS_PATCH);
    if (swap) {
        static const char *const use[] = {"Use ScribbleLit's", "Keep the other one"};
        if (sl_choose("Another SpotPass patch is here",
                      "Another app (maybe NetPass) has a SpotPass patch on this 3DS, and only one can be on.\n\n"
                      "ScribbleLit can save it and use its own. That app's SpotPass stops until you remove ScribbleLit, "
                      "which puts the saved patch back.", use, 2) != 0)
            return false;
    }
    sl_busy("Setting up...", "Joining ScribbleLit...");
    u32 pid = my_pid();
    bool pretendo = pretendo_mode();
    unsigned char key[16];
    if (!pid || !request_key(pid, key)) { sl_tell("Friend account", "Couldn't read your friend account. Nothing changed."); return false; }
    char hex[33], body[96];
    sl_to_hex(key, 16, hex);
    snprintf(body, sizeof body, "{\"pid\": %lu, \"%s\": \"%s\"}", (unsigned long)pid, pretendo ? "token" : "key", hex);
    long status = sl_request("/register", body, NULL);
    if (status == 409) { sl_tell("Already joined", "This friend account already joined with a different code. Nothing changed.\n\nIf that wasn't you, ask for help on the BadgeCade Discord."); return false; }
    if (status == 429) { sl_tell("Too many new consoles", "Too many consoles joined from this connection today. Try again tomorrow. Nothing changed."); return false; }
    if (status == 403) { sl_tell("Turn the proxy off", "For Pretendo mode, ScribbleLit has to see your own connection. Turn off any Wi-Fi proxy and try again. Nothing changed."); return false; }
    if (status != 204) {
        snprintf(text, sizeof text, "Couldn't reach ScribbleLit (%ld). Check your Wi-Fi and try again. Nothing changed.", status);
        { sl_tell("No connection", text); return false; }
    }
    sl_busy("Setting up...", "Getting your games ready...");
    luma_allow_patches();
    if (swap) {
        make_folders(BOSS_SAVED);
        remove(BOSS_SAVED);
        if (rename(BOSS_PATCH, BOSS_SAVED) != 0) { sl_tell("SD card", "Couldn't save the other patch. Nothing changed."); return false; }
    }
    if (!write_file(BOSS_PATCH, BOSS_IPS, sizeof BOSS_IPS)) {
        if (swap) rename(BOSS_SAVED, BOSS_PATCH);
        sl_tell("SD card", "Couldn't write to the SD card.");
        return false;
    }
    size_t used = snprintf(text, sizeof text, "Restart your 3DS, and you and your friends can send notes. You both need ScribbleLit.%s\n\n",
                           pretendo ? " In Pretendo mode, run this again if your internet address changes." : "");
    bool all_ok = patch_games(text + used, sizeof text - used);
    patch_settings();
    sl_tell(all_ok ? "All set!" : "Not everything worked", text);
    return true;
}

void sl_report_note(void) {
    u32 pid = my_pid();
    unsigned char key[16];
    char body[40], signature[33];
    if (!pid || !request_key(pid, key)) return sl_tell("Friend account", "Couldn't read your friend account.");
    sl_busy("Report a note", "Looking for notes you received...");
    snprintf(body, sizeof body, "%lu", (unsigned long)pid);
    sign(key, body, signature);
    long status = sl_request("/my-notes", body, signature);
    if (status == 403) return sl_tell("Report a note", "Set up ScribbleLit first.");
    if (status != 200) return sl_tell("Report a note", "Couldn't reach ScribbleLit. Check your Wi-Fi and try again.");
    static char labels[12][48];
    static long ids[12];
    static const char *pointers[12];
    int count = 0;
    for (char *line = strtok(sl_reply, "\n"); line && count < 12; line = strtok(NULL, "\n")) {
        char date[8], game[16], who[16];
        if (sscanf(line, "%ld %7s %15s %15s", &ids[count], date, game, who) != 4) continue;
        snprintf(labels[count], sizeof labels[count], "%s from %s (%s)", date, who, game);
        pointers[count] = labels[count];
        count++;
    }
    if (!count) return sl_tell("Report a note", "No notes to report. Notes are kept for 7 days after you receive them.");
    int pick = sl_choose("Which note?", "Pick the note to report. A person will look at it, and it's kept until then.", pointers, count);
    if (pick < 0) return;
    static const char *const sure[] = {"Report it", "Cancel"};
    if (sl_choose("Report this note?", labels[pick], sure, 2) != 0) return;
    snprintf(body, sizeof body, "%lu %ld", (unsigned long)pid, ids[pick]);
    sign(key, body, signature);
    sl_tell("Report a note", sl_request("/report", body, signature) == 204 ? "Thanks. A person will look at it. You won't get any more notes from this sender." : "That didn't work. Please try again.");
}

bool sl_remove_all(void) {
    static const char *const sure[] = {"Remove", "Cancel"};
    if (sl_choose("Remove ScribbleLit?", "This takes its patches off your 3DS. Your notes stay in your games.", sure, 2) != 0) return false;
    char line[128];
    FILE *f = fopen(INSTALLED, "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            line[strcspn(line, "\n")] = 0;
            remove(line);
        }
        fclose(f);
        remove(INSTALLED);
    }
    if (same_file(BOSS_PATCH, BOSS_IPS, sizeof BOSS_IPS) || ours_from_before(BOSS_PATCH)) remove(BOSS_PATCH);
    if (!sl_file_exists(BOSS_PATCH)) rename(BOSS_SAVED, BOSS_PATCH);  // the other app's patch, if one was saved
    static const struct { const char *path; const unsigned char *data; size_t size; } older[] = {
        {"sdmc:/luma/titles/0004000000051700/code.ips", SWAPNOTE_IPS, sizeof SWAPNOTE_IPS},
        {"sdmc:/luma/titles/00040000001A2D00/code.ips", SWAPDOODLE_IPS, sizeof SWAPDOODLE_IPS},
        {"sdmc:/luma/titles/00040000000C6600/code.ips", FS3D_IPS, sizeof FS3D_IPS},
        {"sdmc:/luma/titles/0004001000021000/code.ips", SETTINGS_IPS, sizeof SETTINGS_IPS},
    };
    for (size_t i = 0; i < sizeof older / sizeof *older; i++)
        if (same_file(older[i].path, older[i].data, older[i].size)) remove(older[i].path);
    u8 block[0x14];
    if (sl_file_exists(AGE_CHANGED) && R_SUCCEEDED(CFG_GetConfigInfoBlk8(sizeof block, AGE_BLOCK, block))) {
        block[0] = 1;
        if (R_SUCCEEDED(CFG_SetConfigInfoBlk8(sizeof block, AGE_BLOCK, block)) && R_SUCCEEDED(CFG_UpdateConfigSavegame()))
            remove(AGE_CHANGED);
    }
    remove(SL_IN_BADGECADE);
    sl_tell("Removed", "Restart your 3DS.");
    return true;
}


// ---- moving between its own app and the BadgeCade helper; the patches stay put ----

bool sl_sha256_matches(const char *path, const char *expected) {
    static u8 chunk[0x8000];
    unsigned char digest[32];
    char hex[65];
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    mbedtls_sha256_starts(&context, 0);
    size_t got;
    while ((got = fread(chunk, 1, sizeof chunk, f)) > 0) mbedtls_sha256_update(&context, chunk, got);
    fclose(f);
    mbedtls_sha256_finish(&context, digest);
    mbedtls_sha256_free(&context);
    sl_to_hex(digest, 32, hex);
    return !strcmp(hex, expected);
}

bool sl_install_cia(const char *sd_path) {
    static u8 chunk[0x20000];
    Handle file, cia;
    u64 size = 0, offset = 0;
    if (R_FAILED(FSUSER_OpenFileDirectly(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, sd_path), FS_OPEN_READ, 0))) return false;
    bool ok = R_SUCCEEDED(FSFILE_GetSize(file, &size)) && R_SUCCEEDED(AM_StartCiaInstall(MEDIATYPE_SD, &cia));
    if (!ok) {
        FSFILE_Close(file);
        return false;
    }
    while (ok && offset < size) {
        u32 read = 0;
        ok = R_SUCCEEDED(FSFILE_Read(file, &read, offset, chunk, sizeof chunk)) && read;
        for (u32 done = 0; ok && done < read;) {
            u32 written = 0;
            ok = R_SUCCEEDED(FSFILE_Write(cia, &written, offset + done, chunk + done, read - done, 0)) && written;
            done += written;
        }
        offset += read;
    }
    FSFILE_Close(file);
    if (!ok) {
        AM_CancelCIAInstall(cia);
        return false;
    }
    return R_SUCCEEDED(AM_FinishCiaInstall(cia));
}

// A letter on this console only, in the HOME Menu's notifications.
static void leave_letter(const char *title_text, const char *message_text) {
    static u16 title[32], message[256];
    ssize_t title_size = utf8_to_utf16(title, (const u8 *)title_text, 31);
    ssize_t message_size = utf8_to_utf16(message, (const u8 *)message_text, 255);
    if (title_size <= 0 || message_size <= 0 || R_FAILED(newsInit())) return;
    NEWS_AddNotification(title, title_size, message, message_size, LETTER_JPEG, sizeof LETTER_JPEG, true);
    newsExit();
}

bool sl_is_set_up(void) {
    return sl_file_exists(INSTALLED) || same_file(BOSS_PATCH, BOSS_IPS, sizeof BOSS_IPS);
}

bool sl_own_app_installed(void) {
    u64 title = SL_TITLE;
    AM_TitleEntry entry;
    return R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &title, &entry));
}

bool sl_move_to_helper(void) {
    make_folders(SL_IN_BADGECADE);
    if (!write_file(SL_IN_BADGECADE, "", 0)) return false;
    sl_forward_off();  // the helper points the games at itself next time it opens
    leave_letter("ScribbleLit moved!", "ScribbleLit is now in your\nBadgeCade helper.\n\nOpen BadgeCade and go to\nMore options, then ScribbleLit,\nto report a note or remove it.\n\nYou don't need to do anything\nelse. Just play your games!");
    return R_SUCCEEDED(AM_DeleteTitle(MEDIATYPE_SD, SL_TITLE));
}

bool sl_move_to_own_app(void) {
    char version[16], digest[65];
    if (!sl_own_app_installed()) {
        if (sl_request("/app.txt", NULL, NULL) != 200 || sscanf(sl_reply, "%15s %64s", version, digest) != 2) return false;
        bool ok = sl_download("/ScribbleLit.cia", OWN_APP_FILE) && sl_sha256_matches(OWN_APP_FILE, digest) && sl_install_cia(OWN_APP_SD_PATH);
        remove(OWN_APP_FILE);
        if (!ok) return false;
    }
    remove(SL_IN_BADGECADE);
    leave_letter("ScribbleLit is back!", "BadgeCade was uninstalled, so\nScribbleLit has its own icon on\nthe HOME Menu again.\n\nNothing else changed. Your notes\nkeep working.");
    return true;
}
