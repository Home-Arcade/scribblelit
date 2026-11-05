// SPDX-License-Identifier: GPL-3.0-or-later
#define _GNU_SOURCE
#include "sl_find.h"
#include <stdlib.h>
#include <string.h>

#define MAX_CODE (32 * 1024 * 1024)

// ExeFS's backwards LZ. False if the data doesn't hold together.
static bool unpack(const u8 *packed, u32 size, u8 *out, u32 out_size) {
    u32 bounds, grow;
    memcpy(&bounds, packed + size - 8, 4);
    memcpy(&grow, packed + size - 4, 4);
    u32 stop = size - (bounds & 0xFFFFFF), in = size - (bounds >> 24), at = out_size;
    memcpy(out, packed, size);
    while (in > stop) {
        u8 control = packed[--in];
        for (int bit = 0; bit < 8 && in > stop && at > 0; bit++, control <<= 1) {
            if (control & 0x80) {
                if (in < 2) return false;
                in -= 2;
                u32 pair = packed[in] | packed[in + 1] << 8, count = (pair >> 12) + 3, back = (pair & 0xFFF) + 2;
                if (at < count) return false;
                while (count--) {
                    if (at + back >= out_size) return false;
                    u8 byte = out[at + back];
                    out[--at] = byte;
                }
            } else {
                if (in < 1) return false;
                out[--at] = packed[--in];
            }
        }
    }
    return true;
}

Result code_read_error;

u8 *code_read(u64 title, FS_MediaType media, size_t *size) {
    u32 archive_path[4] = {(u32)title, (u32)(title >> 32), media, 0};
    u32 file_path[5] = {0, 0, 2, 0x646F632E, 0x00000065};  // the ExeFS file ".code"
    FS_Path archive = {PATH_BINARY, sizeof archive_path, archive_path}, file = {PATH_BINARY, sizeof file_path, file_path};
    Handle handle;
    code_read_error = FSUSER_OpenFileDirectly(&handle, ARCHIVE_SAVEDATA_AND_CONTENT, archive, file, FS_OPEN_READ, 0);
    if (R_FAILED(code_read_error)) return NULL;
    code_read_error = 0;
    u64 packed_size = 0;
    u8 *packed = NULL, *out = NULL;
    u32 read = 0;
    if (R_FAILED(FSFILE_GetSize(handle, &packed_size)) || packed_size < 16 || packed_size > MAX_CODE) goto done;
    if (!(packed = malloc(packed_size))) goto done;
    if (R_FAILED(FSFILE_Read(handle, &read, 0, packed, packed_size)) || read != packed_size) goto done;
    u32 grow;
    memcpy(&grow, packed + packed_size - 4, 4);
    if (grow == 0 || grow > MAX_CODE) {  // not packed
        out = packed;
        packed = NULL;
        *size = packed_size;
        goto done;
    }
    *size = packed_size + grow;
    if ((out = malloc(*size)) && !unpack(packed, packed_size, out, *size)) {
        free(out);
        out = NULL;
    }
done:
    free(packed);
    FSFILE_Close(handle);
    return out;
}

long code_find(const u8 *code, size_t size, const void *needle, size_t length) {
    long found = -1;
    for (const u8 *at = code; (at = memmem(at, code + size - at, needle, length)); at++) {
        if (found >= 0) return -1;
        found = at - code;
    }
    return found;
}
