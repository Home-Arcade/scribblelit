// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui.h"
#include <3ds.h>
#include <citro2d.h>

#define PAPER C2D_Color32(255, 248, 225, 255)
#define PANEL C2D_Color32(255, 255, 255, 255)
#define INK C2D_Color32(47, 72, 145, 255)
#define SOFT C2D_Color32(214, 222, 245, 255)
#define TEXT C2D_Color32(45, 45, 60, 255)
#define WHITE C2D_Color32(255, 255, 255, 255)
#define BUTTON_H 40
#define BUTTON_GAP 6
#define VISIBLE 5

static C3D_RenderTarget *top, *bottom;
static C2D_TextBuf text_buffer;

void ui_init(void) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    text_buffer = C2D_TextBufNew(4096);
}

void ui_exit(void) {
    C2D_TextBufDelete(text_buffer);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

static void words(const char *line, float x, float y, float scale, u32 color, u32 flags, float width) {
    C2D_Text text;
    C2D_TextParse(&text, text_buffer, line);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor | flags, x, y, 0, scale, scale, color, width);
}

static void draw_top(const char *title, const char *text) {
    C2D_TargetClear(top, PAPER);
    C2D_SceneBegin(top);
    C2D_DrawRectSolid(0, 0, 0, 400, 44, INK);
    words(title, 14, 9, 0.8f, WHITE, 0, 0);
    words(text, 16, 58, 0.55f, TEXT, C2D_WordWrap, 368);
}

static void draw_frame(const char *title, const char *text, const char *const *buttons, int count, int selected, int first) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(text_buffer);
    draw_top(title, text);
    C2D_TargetClear(bottom, PAPER);
    C2D_SceneBegin(bottom);
    for (int i = first; i < count && i < first + VISIBLE; i++) {
        float y = 12 + (i - first) * (BUTTON_H + BUTTON_GAP);
        C2D_DrawRectSolid(16, y, 0, 288, BUTTON_H, i == selected ? INK : SOFT);
        words(buttons[i], 28, y + 10, 0.6f, i == selected ? WHITE : TEXT, 0, 0);
    }
    if (first > 0) words("^", 306, 8, 0.6f, INK, 0, 0);
    if (first + VISIBLE < count) words("v", 306, 210, 0.6f, INK, 0, 0);
    C3D_FrameEnd(0);
}

int ui_choose(const char *title, const char *text, const char *const *buttons, int count) {
    int selected = 0, first = 0;
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_B) return -1;
        if (down & KEY_A) return selected;
        if (down & KEY_DOWN && selected + 1 < count) selected++;
        if (down & KEY_UP && selected > 0) selected--;
        if (selected < first) first = selected;
        if (selected >= first + VISIBLE) first = selected - VISIBLE + 1;
        if (down & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            int row = (touch.py - 12) / (BUTTON_H + BUTTON_GAP);
            if (touch.px >= 16 && touch.px < 304 && touch.py >= 12 && row < VISIBLE && first + row < count) return first + row;
        }
        draw_frame(title, text, buttons, count, selected, first);
    }
    return -1;
}

void ui_busy(const char *title, const char *text) {
    draw_frame(title, text, NULL, 0, 0, 0);
}
