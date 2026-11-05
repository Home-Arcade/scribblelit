// SPDX-License-Identifier: GPL-3.0-or-later
// ScribbleLit updater: the app can't replace itself while it runs, so it hands over to this small
// title, which installs the new version and hands back. The new app then removes this title.
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>

#define APP_ID 0x0004000005CB1700ULL
#define UPDATE_SD_PATH "/3ds/ScribbleLit/update.cia"
#define CHUNK 0x20000

static bool install(Handle file, u64 size, u32 flags) {
    Handle cia;
    u64 id = APP_ID;
    AM_TitleInfo installed;
    bool replacing = R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &id, &installed));
    if (R_FAILED(replacing ? AM_StartCiaInstallOverwrite(&cia, MEDIATYPE_SD) : AM_StartCiaInstall(MEDIATYPE_SD, &cia))) return false;
    u8 *buffer = malloc(CHUNK);
    u64 offset = 0;
    bool ok = buffer != NULL;
    while (ok && offset < size) {
        u32 read = 0;
        ok = R_SUCCEEDED(FSFILE_Read(file, &read, offset, buffer, CHUNK)) && read;
        for (u32 done = 0; ok && done < read;) {
            u32 written = 0;
            ok = R_SUCCEEDED(FSFILE_Write(cia, &written, offset + done, buffer + done, read - done, flags)) && written;
            done += written;
        }
        offset += read;
        printf("\r%3u%%", (unsigned)(offset * 100 / size));
        gfxFlushBuffers();
        gfxSwapBuffers();
    }
    free(buffer);
    if (!ok) {
        AM_CancelCIAInstall(cia);
        return false;
    }
    return R_SUCCEEDED(AM_FinishCiaInstall(cia));
}

int main(void) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    amInit();
    printf("ScribbleLit updater\n\nKeep your 3DS on.\n\n");
    Handle file;
    u64 size = 0;
    bool ok = R_SUCCEEDED(FSUSER_OpenFileDirectly(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, UPDATE_SD_PATH), FS_OPEN_READ, 0));
    if (ok) {
        ok = R_SUCCEEDED(FSFILE_GetSize(file, &size)) && (install(file, size, 0) || install(file, size, FS_WRITE_FLUSH));
        FSFILE_Close(file);
    }
    if (!ok) {
        printf("\n\nThat didn't work. Your app wasn't changed.\nPress A to close.\n");
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_A) break;
            gspWaitForVBlank();
        }
    }
    remove("sdmc:" UPDATE_SD_PATH);
    aptSetChainloader(APP_ID, MEDIATYPE_SD);
    amExit();
    gfxExit();
    return 0;
}
