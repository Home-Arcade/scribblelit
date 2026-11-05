#include "ips.h"
#include <stdlib.h>
#include <string.h>

#define HEADER "PATCH"
#define HEADER_SIZE 5
#define END "EOF"
#define END_SIZE 3

// one record of the file at *at: its place, how many bytes it changes, how long it is in the file
static bool next_record(const unsigned char *file, size_t size, size_t *at, unsigned *addr, size_t *span, size_t *length) {
    size_t i = *at;
    if (i + 5 > size) return false;
    *addr = file[i] << 16 | file[i + 1] << 8 | file[i + 2];
    size_t n = file[i + 3] << 8 | file[i + 4];
    if (n) {
        *span = n;
        *length = 5 + n;
    } else {  // run of one byte
        if (i + 8 > size) return false;
        *span = file[i + 5] << 8 | file[i + 6];
        *length = 8;
    }
    if (i + *length > size) return false;
    *at = i + *length;
    return true;
}

static bool at_end(const unsigned char *file, size_t size, size_t at) {
    return at + END_SIZE <= size && !memcmp(file + at, END, END_SIZE);
}

static unsigned char *put_record(unsigned char *p, const IpsRecord *r) {
    *p++ = r->addr >> 16;
    *p++ = r->addr >> 8;
    *p++ = r->addr;
    *p++ = r->size >> 8;
    *p++ = r->size;
    memcpy(p, r->data, r->size);
    return p + r->size;
}

static bool valid(const unsigned char *file, size_t size) {
    return !size || (size >= HEADER_SIZE && !memcmp(file, HEADER, HEADER_SIZE));
}

static bool overlaps(unsigned addr, size_t span, const IpsRecord *mine, int count) {
    for (int k = 0; k < count; k++)
        if (addr < mine[k].addr + mine[k].size && mine[k].addr < addr + span) return true;
    return false;
}

bool ips_add(const unsigned char *file, size_t file_size, const IpsRecord *mine, int count,
             unsigned char **out, size_t *out_size) {
    if (!valid(file, file_size)) return false;
    size_t room = HEADER_SIZE + file_size + END_SIZE;
    for (int k = 0; k < count; k++) room += 5 + mine[k].size;
    unsigned char *result = malloc(room), *p = result;
    if (!result) return false;
    memcpy(p, HEADER, HEADER_SIZE);
    p += HEADER_SIZE;
    size_t at = file_size ? HEADER_SIZE : 0;
    while (file_size && !at_end(file, file_size, at)) {
        size_t start = at, span, length;
        unsigned addr;
        if (!next_record(file, file_size, &at, &addr, &span, &length)) {
            free(result);
            return false;
        }
        if (!overlaps(addr, span, mine, count)) {
            memcpy(p, file + start, length);
            p += length;
        }
    }
    for (int k = 0; k < count; k++) p = put_record(p, &mine[k]);
    memcpy(p, END, END_SIZE);
    *out = result;
    *out_size = p + END_SIZE - result;
    return true;
}

bool ips_remove(const unsigned char *file, size_t file_size, const IpsRecord *mine, int count,
                unsigned char **out, size_t *out_size) {
    if (!valid(file, file_size)) return false;
    unsigned char *result = malloc(HEADER_SIZE + file_size + END_SIZE), *p = result;
    if (!result) return false;
    memcpy(p, HEADER, HEADER_SIZE);
    p += HEADER_SIZE;
    size_t at = file_size ? HEADER_SIZE : 0;
    bool left = false;
    while (file_size && !at_end(file, file_size, at)) {
        size_t start = at, span, length;
        unsigned addr;
        if (!next_record(file, file_size, &at, &addr, &span, &length)) {
            free(result);
            return false;
        }
        bool is_mine = false;
        for (int k = 0; k < count; k++)
            if (addr == mine[k].addr && length == 5 + mine[k].size && !memcmp(file + start + 5, mine[k].data, mine[k].size))
                is_mine = true;
        if (!is_mine) {
            memcpy(p, file + start, length);
            p += length;
            left = true;
        }
    }
    memcpy(p, END, END_SIZE);
    *out = result;
    *out_size = left ? (size_t)(p + END_SIZE - result) : 0;
    return true;
}
