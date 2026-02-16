#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

extern const uint32_t crc32_tab[1024/4];
extern uint32_t crc32(uint32_t crc, const void *buf, size_t size);

#endif
/* vim: set sw=4 ts=8 expandtab: */
