/*
   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
   AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
   OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
   IN THE SOFTWARE.
*/

#ifndef _UTILS_H
#define _UTILS_H

#include <string.h>
#include <stdint.h>

struct ImageHandle {
	union {
//		uint32_t	*u32;
		uint16_t	*u16;
		uint8_t		*u8;
		char		*s;
		void		*p;
	} d;
	size_t	len;
};

/*
 * If htole16() is missing, let's assume that other *le*() functions
 * are also missing.
 *
 * OpenBSD - htole16 & 32 exist, but not le16toh etc
 */
#if defined(__OpenBSD__)
#define le16toh(x) htole16(x)
#define le32toh(x) htole32(x)
#endif

#if !defined(htole16)
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le32toh(x) (x)
#define le16toh(x) (x)
#define htole32(x) (x)
#define htole16(x) (x)
#else
#define le32toh(x) __bswap_32(x)
#define htole16(x) __bswap_16(x)
#define le32toh(x) __bswap_32(x)
#define htole16(x) __bswap_16(x)
#endif
#endif

// they're the same.
#define memcpy_to_le32 memcpy_from_le32

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define memcpy_from_le32(dest, src, len) \
	memcpy(dest, src, len)
#else
#define memcpy_from_le32(dest, src, len) { \
	int i; \
	for (i=0;i<len/4;i++) \
		((uint32_t *)dest)[i] = __bswap_32(((uint32_t *)src)[i]); \
}
#endif

int iload_file(struct ImageHandle *ih, const char *fname, int rw);
int ifree_file(struct ImageHandle *ih);
int save_file(const char *filename, const uint8_t *filebuf, size_t filelen);
uint8_t *load_file(const char *filename, size_t *filelen);

int search_image(const struct ImageHandle *ih, size_t start, const void *needle, const void *mask, int len, int align);

void hexdump(uint8_t *buf, int len, const char *end);

#endif
