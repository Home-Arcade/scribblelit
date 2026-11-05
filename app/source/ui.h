// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
void ui_init(void);
void ui_exit(void);
// title and text on top, buttons below. Returns the button chosen, -1 for B or closing.
int ui_choose(const char *title, const char *text, const char *const *buttons, int count);
void ui_busy(const char *title, const char *text);
