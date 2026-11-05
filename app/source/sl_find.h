// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <3ds.h>
#include <stddef.h>
// The title's own code, unpacked on the console. Free it with free(). NULL if it can't be read,
// and code_read_error then holds why (0 when the code itself didn't hold together).
extern Result code_read_error;
u8 *code_read(u64 title, FS_MediaType media, size_t *size);
// Where needle occurs, if it occurs exactly once; -1 otherwise.
long code_find(const u8 *code, size_t size, const void *needle, size_t length);
