#ifndef STR_H
#define STR_H

#include <stdarg.h>
#include <stdlib.h> /* most callers want the prototype for free() too */
#include <memory.h>

/* GCC have printf type attribute check.  */
#ifdef __GNUC__
#define PRINTF_ATTRIBUTE(a,b) __attribute__ ((__format__ (__printf__, a, b)))
#else
#define PRINTF_ATTRIBUTE(a,b)
#endif /* __GNUC__ */

#ifndef STR
# define X__STR(x) # x
# define STR(x) X__STR(x)
#endif

#ifdef  __cplusplus
extern "C" {
#endif

/** expanding string buffer for \ref sbprintf */
struct strbuf {
    char *pbuf;
    size_t offset;
    size_t len;
};

int atomac(unsigned char * mac, char * buf);
int atomac_no_delim(unsigned char * mac, char * buf);
int atohex(unsigned char * mac, char * buf, int *digits);
int atohex_no_delim(unsigned char * mac, char * buf, int *digits);

int chomp(char *str);
int chompn(char *str, int len);
int str_is_digit(char *str, int len);
void str_clean(char *str, int len);
int str_split(char *str, char *argv[], int maxargc, const char *split);
int iszero(const void *buf, int len);
int isempty(const char *buf, int len);
int comma_separated(char *buf, int space);

int sbprintf(struct strbuf *param, const char *fmt, ...) PRINTF_ATTRIBUTE(2, 3);
int vsbprintf(struct strbuf *param, const char *fmt, va_list ap);
void sprint_spaces(char *buf, int nspaces);
void sbensure(struct strbuf *sb, int len);
char *sbfill(struct strbuf *sb, char fill, int width);
void sbfree(struct strbuf *sb);

/* init sb with malloc'ed string. */
#ifdef _WIN32
#define __INLINE __inline
#else
#define __INLINE static inline
#endif

__INLINE void strbuf_from_str(struct strbuf *sb, char *str)
{
    sb->pbuf = str;
    sb->len = sb->offset = strlen(str);
}

/* safety string copy */
__INLINE int snputs(char *dest, const char *src, size_t n)
{
    if (n<=0) return 0;

    strncpy(dest, src, n);
    dest[n-1]='\0';

    return strnlen(dest, n);
}

void sbputs(struct strbuf *sb, const char *s);

#ifdef  __cplusplus
}
#endif

#endif /* STR_H */
/* vim: set sw=4 ts=8 expandtab: */
