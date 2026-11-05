// SPDX-License-Identifier: GPL-3.0-or-later
#define _GNU_SOURCE
#include "sl_forward.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "forward_cave.h"
#include "ips.h"

#define CODE_BASE 0x100000
#define GAME_ENTRY 0xEB000007u  // bl 0x100024, the first instruction of all of these
#define RETURN_FILE "sdmc:/3ds/ScribbleLit/return-to-game"
#define UNIT_MS 0x400000ULL  // the patch's clock: osGetTime() >> 22

// Only versions whose free space was checked against a copy of the game. In the code's unused tail,
// past the 0x110 bytes Luma writes after the end of .text when a game has a romfs folder.
static const struct { u64 title; u16 version; u32 code, table; bool badge_arcade; } games[] = {
    {0x0004000000153500ULL, 5136, 0x3D7E00, 0x3D7F20, true},   // Badge Arcade USA, around its other patches
    {0x0004000000153600ULL, 5136, 0x3D7E00, 0x3D7F20, true},   // EUR
    {0x0004000000134600ULL, 5200, 0x3D7C20, 0x3D7EF0, true},   // JPN
    {0x0004000000051700ULL, 3120, 0x2E1800, 0x2E1920, false},  // Swapnote USA
    {0x00040000001A2D00ULL, 4160, 0x377800, 0x377920, false},  // Swapdoodle USA
    {0x00040000000C6600ULL, 2128, 0x35B800, 0x35B920, false},  // Flipnote Studio 3D USA
};
#define GAME_COUNT (sizeof games / sizeof *games)

static void put32(unsigned char *at, u32 value) { memcpy(at, &value, 4); }

static unsigned char *read_all(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = length > 0 ? malloc(length) : NULL;
    if (data && fread(data, 1, length, f) != (size_t)length) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *size = length;
    return data;
}

static bool add_records(const char *path, const unsigned char *file, size_t size, const IpsRecord *records, int count) {
    unsigned char *out;
    size_t out_size;
    if (!ips_add(file, size, records, count, &out, &out_size)) return false;
    FILE *f = fopen(path, "wb");
    bool ok = f && fwrite(out, 1, out_size, f) == out_size;
    if (f && fclose(f)) ok = false;
    free(out);
    return ok;
}

static u16 running_version(u64 title) {
    u64 update = title | 0x0000000E00000000ULL;
    AM_TitleEntry entry;
    if (R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &update, &entry))) return entry.version;
    return R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &title, &entry)) ? entry.version : 0;
}

// The game's own first instruction again. The rest of the patch is never reached after this.
static bool put_entry_back(const char *path, const unsigned char *file, size_t size) {
    static const unsigned char start[5] = {0, 0, 0, 0, 4};
    if (!memmem(file, size, start, 5)) return true;
    unsigned char word[4];
    put32(word, GAME_ENTRY);
    IpsRecord record = {0, word, 4};
    return add_records(path, file, size, &record, 1);
}

bool sl_forward_install(u64 app, u32 hours, bool badge_arcade) {
    bool all_ok = true;
    if (!hours) hours = 1;
    u32 units = hours * 3600000ULL / UNIT_MS + 1;
    u32 next = (osGetTime() >> 22) + units;
    for (size_t i = 0; i < GAME_COUNT; i++) {
        char path[64];
        size_t size;
        if (games[i].badge_arcade && !badge_arcade) continue;
        snprintf(path, sizeof path, "sdmc:/luma/titles/%016llX/code.ips", games[i].title);
        unsigned char *file = read_all(path, &size);  // only games we've already patched
        if (!file) continue;
        if (running_version(games[i].title) != games[i].version) {  // updated since: the space isn't known
            if (!put_entry_back(path, file, size)) all_ok = false;
            free(file);
            continue;
        }
        static unsigned char code[sizeof FORWARD_CODE], table[sizeof FORWARD_TABLE];
        unsigned char word[4];
        memcpy(code, FORWARD_CODE, sizeof code);
        memcpy(table, FORWARD_TABLE, sizeof table);
        put32(code + FORWARD_TABLE_AT, games[i].table);
        put32(table + FORWARD_TID_LO, (u32)app);
        put32(table + FORWARD_TID_HI, (u32)(app >> 32));
        put32(table + FORWARD_NEXT, next);
        put32(table + FORWARD_SPAN, units);
        put32(table + FORWARD_PARAM_AT, games[i].table + FORWARD_MAGIC);
        put32(table + FORWARD_GAME_LO, (u32)games[i].title);
        put32(table + FORWARD_GAME_HI, (u32)(games[i].title >> 32));
        IpsRecord records[3] = {
            {0, word, 4},
            {games[i].code - CODE_BASE, code, sizeof code},
            {games[i].table - CODE_BASE, table, sizeof table},
        };
        // bl from the game's first instruction: the patch finishes by going where that bl went
        put32(word, 0xEB000000u | (((games[i].code - (CODE_BASE + 8)) >> 2) & 0xFFFFFF));
        if (!add_records(path, file, size, records, 3)) all_ok = false;
        free(file);
    }
    return all_ok;
}

void sl_forward_off(void) {
    for (size_t i = 0; i < GAME_COUNT; i++) {
        char path[64];
        size_t size;
        snprintf(path, sizeof path, "sdmc:/luma/titles/%016llX/code.ips", games[i].title);
        unsigned char *file = read_all(path, &size);
        if (!file) continue;
        put_entry_back(path, file, size);
        free(file);
    }
}

u64 sl_forward_caller(void) {
    static u8 param[0x300], hmac[0x20];
    u64 sender = 0, game;
    bool received = false;
    // the game's patch says who it is: on the way here the HOME Menu passes this on, not the sender's ID
    if (R_FAILED(APT_ReceiveDeliverArg(param, sizeof param, hmac, &sender, &received)) || !received) return 0;
    if (memcmp(param, "BCFW", 4)) return 0;
    memcpy(&game, param + 4, 8);
    for (size_t i = 0; i < GAME_COUNT; i++)
        if (games[i].title == game) return game;
    return 0;
}

void sl_forward_remember(u64 game, const char *version) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/ScribbleLit", 0777);
    FILE *f = fopen(RETURN_FILE, "w");
    if (!f) return;
    fprintf(f, "%016llX %s\n", game, version);
    fclose(f);
}

u64 sl_forward_remembered(const char *version) {
    unsigned long long game = 0;
    char before[16] = "";
    FILE *f = fopen(RETURN_FILE, "r");
    if (!f) return 0;
    int got = fscanf(f, "%llx %15s", &game, before);
    fclose(f);
    remove(RETURN_FILE);
    // the same version means the update never happened: this is a normal open
    return got == 2 && strcmp(before, version) ? game : 0;
}
