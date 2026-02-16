#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#ifdef __linux__
#include <malloc.h>
#endif

#include "str.h"

#define ASSERT assert
#define MAC_DELIMITER(c) ( c == ':' || c == '-' )

#ifdef _WIN32
#ifndef va_copy
#define va_copy(d,s) ((d) = (s))
#endif
#endif

#if 0
/** Strip all cr/lf from end of string. Modifies string in place */
int chomp_crlf(char *str, int len)
{
    if (len) {
        while (str[len-1] == '\n' || str[len-1] == '\r') {
            str[--len] = '\0';
            if (!len)
            break;
        }
    }
    return len;
}
#endif

/** chomp whitespace from the end of the string */
int chomp(char *str)
{
    int len=strlen(str);
    while ((len > 0) && isspace((unsigned char)str[len-1]))
        str[--len] = '\0';
    return len;
}

int chompn(char *str, int len)
{
    len=strnlen(str,len);
    while ((len > 0) && isspace((unsigned char)str[len-1]))
        str[--len] = '\0';
    return len;
}

#if 0 /* unused */
char *sbchomp(struct strbuf *buf)
{
    if(!buf) return NULL;
    buf->offset=chompn(buf->pbuf,buf->offset);
    return buf->pbuf;
}
#endif

/** Converts ascii MAC address with any delimiters hex values without delimiters.
    return  0 = success
        1 = error
 */
int atomac(unsigned char * mac, char * buf)
{
    int len=0;

    atohex(mac, buf, &len);

    if (len != 6)
        return 1;
    return 0;
}

int atomac_no_delim(unsigned char * mac, char * buf)
{
    int len=0;

    atohex_no_delim(mac, buf, &len);

    if (len != 6)
        return 1;
    return 0;
}

/* ensure that strbuf has room enough for at least n bytes */
void sbensure(struct strbuf *sb, int n)
{
    size_t len;
    char *buf = NULL;

    ASSERT(sb != NULL);
    len = sb->len;
    if ((sb->pbuf != NULL) && ((len - sb->offset) >= n))
        return;

    /* Grow buffer exponentially, rather than per addendum */
    if (len == 0) len = 100;
    while ((len - sb->offset) < n)
        len *= 2;

    buf = realloc(sb->pbuf, len);
    // ASSERT_INFO((buf != NULL), "%zu", len);
#ifdef _MALLOC_H
    {
        size_t usable = malloc_usable_size(buf);
        /* Take advantage of full usable allocation */
        sb->len = (usable > len) ? usable : len;
    }
#else
    sb->len = len;
#endif
    sb->pbuf = buf;
}

/** Printf into an automatically extended string buffer */
int vsbprintf(struct strbuf *param, const char *fmt, va_list ap)
{
    int ret;
    char *buf;

    if (!param) return 0;

    buf = param->pbuf;

    if(!buf) {
        if (param->len == 0)
                param->len = 100;
        buf = malloc(param->len);
        // ASSERT_INFO((buf != NULL), "%zu", param->len);
#ifdef _MALLOC_H
        {
            /* Take advantage of full usable allocation */
            size_t usable = malloc_usable_size(buf);
            if (param->len < usable)
                param->len = usable;
        }
#endif
    }

    while(1) {
        int rem = param->len - param->offset;
        va_list cp;
        va_copy(cp, ap);
        ret = vsnprintf(buf + param->offset, rem, fmt, cp);
        va_end(cp);
        if(ret == -1) buf = realloc(buf, param->len *= 2);
        else if(ret >= rem) buf = realloc(buf, param->len += ret);
        else break;
        //ASSERT_INFO((buf != NULL), "%zu", param->len);
#ifdef _MALLOC_H
        {
            /* Take advantage of full usable allocation */
            size_t usable = malloc_usable_size(buf);
            if (param->len < usable)
                param->len = usable;
        }
#endif
    }
    param->offset += ret;
    param->pbuf = buf;
    return ret;
}

int sbprintf(struct strbuf *param, const char *fmt, ...)
{
    va_list ap;
    int ret;
    va_start(ap, fmt);
    ret=vsbprintf(param, fmt, ap);
    va_end(ap);
    return ret;
}

void sbputs(struct strbuf *sb, const char *s)
{
    int len = strlen(s);
    ASSERT(sb != NULL);
    sbensure(sb, len + 1);
    memcpy(sb->pbuf + sb->offset, s, len + 1);
    sb->offset += len;
}

char *sbfill(struct strbuf *sb, char fill, int width)
{
    char buf[]={fill,'\0'};
    int off=sb->offset;
    if(width) {
        sbprintf(sb, "%*s", width, buf);
        memset(sb->pbuf+off, fill, width);
    }
    return sb->pbuf;
}

/** Run isdigit() across this str. */
static int str_is_xdigit(char *str, int len)
{
    int i;

    for(i = 0; i < len; i++) {
        if (!isxdigit((unsigned char)str[i]))
            return 0;
    }
    return 1;
}

// Converts 2-byte/value ascii hex string with any delimiter into hex values
//   Examples:  11.22.33.44 , 0a-b9-c2 , aa1bb2cc3 (delim=1,2,3)
//
//   return  0 = success, 1 = error
//
int atohex(unsigned char * data, char * buf, int *digits)
{
    int i,j,len;

    len = strlen(buf);
    if ((len-2) % 3)
    return 1;

    for (i=0,j=0; i<len; i+=3,j++) {
        if(!str_is_xdigit(buf+i,2)) return 1;
        data[j] = (unsigned char)strtol(buf+i, NULL, 16);
    }
    *digits = j;
    return 0;
}

// Converts 2-byte/value ascii hex string with NO delimiter into hex values
//   Examples:  11223344 , 0ab9c2 , aabbcc
//
//   return  0 = success, 1 = error
//
int atohex_no_delim(unsigned char * data, char * buf, int *digits)
{
    int i,j,len;

    len = strlen(buf);
    if (len % 2)
    return 1;

    if(!str_is_xdigit(buf,len)) return 1;

    for (i=0,j=0; i<len; i+=2,j++) {
        char tmp[3]={buf[i],buf[i+1],0};
        data[j] = (unsigned char)strtol(tmp, NULL, 16);
    }
    *digits = j;
    return 0;
}

/** Run isdigit() across this str. */
int str_is_digit(char *str, int len)
{
    int i;

    for(i = 0; i < len; i++) {
        if (!isdigit((unsigned char)str[i]))
            return 0;
    }
    return 1;
}

/** Fill buffer with some spaces */
void sprint_spaces(char *buf, int nspaces)
{
    while (nspaces--)
    *buf++ = ' ';
    *buf = '\0';
}

/**
 * Is buf all zero?
 * Assumes buf is 256 bytes or smaller.
 */
int iszero(const void *buf, int len)
{
    static const unsigned char zeros[256]={0};
    ASSERT(len <= 256);
    return memcmp(buf, zeros, len) == 0;
}

/* empty string? */
int isempty(const char *buf, int len)
{
    while (len-- > 0)
    if (!isspace((unsigned char)(*buf++)))
        return 0;
    return 1;
}

/**
 * convert an ascii string of (up to a) 64-bit number (max 20 chars) to a comma-separated string
 * 'space' is buffer space for adding commas; if not enough, do nothing
 * return # of commas added
**/
int comma_separated(char *buf, int space)
{
    int i,j;
    int len=strlen(buf);
    int commas=0;
    char *dptr,*sptr;

    if (len>20)  // 20 chars = max for 64-bit decimal number
    return 0;

    for (i=0,j=0; i<len; i++,j++)
    {
        if (!isdigit((unsigned char)buf[i]))
            return 0;
        if (j==3)
        {
                commas++;
                j=0;
        }
    }
    if (space<commas) return 0;
    dptr = buf + len + commas;
    *dptr-- = 0;
    sptr = &buf[len-1];

    for (i=0,j=0; i<len+commas; i++,j++)
    {
        if (j==3)
        {
            *dptr-- =',';
            j=-1;
            continue;
        }
        *dptr-- = *sptr--;
    }
    return commas;
}

/*************************************************
 * Replace the unprintable character in the string
 * with space ie 0x20 - This is used in salvaging
 * good data coupled with bad data in the string
 *************************************************/
void str_clean(char *str, int len)
{
    int index = 0;
    for(index = 0;index < len; index++) {
        if (str[index] && !isprint((unsigned char)str[index])) {
                str[index] = '?';
        }
    }
    /* BZ 5307 - strip trailing spaces */
    chompn(str,len);
}

void sbfree(struct strbuf *sb)
{
    if (sb->pbuf) {
        free(sb->pbuf);
        sb->pbuf = NULL;
        sb->offset = sb->len = 0;
    }
}

#if _MSC_VER
static char *strtok_r(char *str, const char *delim, char **saveptr)
{
    return strtok(str, delim);
}
#endif

int str_split(char *buf, char *argv[], int maxargc, const char *split)
{
    int argc;
    char *lasts=NULL;

    ASSERT(buf);

    for(argc=0,argv[0]=buf;argc<maxargc;argc++) {
        argv[argc]=strtok_r(argc?NULL:argv[0],split,&lasts);
        if(!argv[argc]) break;
    }

    argv[maxargc-1]=NULL;       /* null terminate */
    return argc;
}
/* vim: set sw=4 ts=8 expandtab: */
