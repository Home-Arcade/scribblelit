#pragma once
#include <stdbool.h>
#include <stddef.h>

// IPS patch files with other people's patches in them (Luma reads one file per title: a second
// patch has to go in the same file, or it replaces the first one).
typedef struct {
    unsigned addr;              // offset in the code
    const unsigned char *data;
    size_t size;
} IpsRecord;

// file + these records, whatever of the file they overlap dropped; false if the file isn't IPS
bool ips_add(const unsigned char *file, size_t file_size, const IpsRecord *mine, int count,
             unsigned char **out, size_t *out_size);
// file - these records (same place, same bytes); *out_size = 0 when nothing is left in it
bool ips_remove(const unsigned char *file, size_t file_size, const IpsRecord *mine, int count,
                unsigned char **out, size_t *out_size);
