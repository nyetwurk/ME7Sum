/* Permission is hereby granted, free of charge, to any person obtaining
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

#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

int iload_file(struct ImageHandle *ih, const char *fname, int rw, struct strbuf *buf)
{
    // init image handle structure to zero's
    memset(ih, 0, sizeof(*ih));
    // load file into memory
    if(((ih->d.p)= (void *)load_file(fname,&ih->len,buf)) == 0) return -1;
    snprintf(ih->filename, sizeof(ih->filename), "%s", fname);
    return 0;
}

int ifree_file(struct ImageHandle *ih)
{
    if((ih->d.p) != 0) free(ih->d.p);
    memset(ih, 0, sizeof(*ih));
    return 0;
}

/* load a file into memory and return buffer */
uint8_t *load_file(const char *filename, size_t *filelen, struct strbuf *buf)
{
    FILE *fp;
    uint8_t *data;
    size_t size,bytesRead;

    /* open file */
    sbprintf(buf, "þ Opening '%s'\n",filename);
    if ((fp = (FILE *)fopen(filename, "rb")) == NULL) { sbprintf(buf, "\nCan't open '%s'.\n", filename); return(0); }

    /* get file length */
    sbprintf(buf, "þ Getting length of '%s'\n",filename);
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if(size <= 0) { sbprintf(buf, "Error: Problem with seeking filesize\n"); fclose(fp); return(0); }

    *filelen = size;                /* return size of file to caller */

    /* alloc buffer for file */
    sbprintf(buf, "þ Allocating buffer of %d bytes\n",(int)size);
    data = (uint8_t *)malloc(size);
    if(data == 0) { sbprintf(buf, "\nfailed to allocate memory to load module\n"); fclose(fp); return 0; }

    /* load file into buffer */
    sbprintf(buf, "þ Reading file to buffer\n");
    bytesRead = fread(data, 1, size, fp);

    /* validate it all loaded correctly */
    sbprintf(buf, "þ Validating size correct %d=%d\n",(int)bytesRead,(int)size);
    if(bytesRead != size) { sbprintf(buf, "\nfailed to load module into buffer\n"); free(data); fclose(fp); return 0; }

    /* close the file */
    sbprintf(buf, "þ Closing file\n");
    fclose(fp);
    return(data);
}

/* load a file into memory and return buffer */
int save_file(const char *filename, const uint8_t *filebuf, size_t filelen, struct strbuf *buf)
{
    FILE *fp;
    size_t bytesWritten;

    /* open file */
    sbprintf(buf, "þ Opening '%s' file for writing\n",filename);
    if ((fp = (FILE *)fopen(filename, "wb")) == NULL){ sbprintf(buf, "\nCan't open file \"%s\".", filename); return(-1); }

    /* load file into buffer */
    sbprintf(buf, "þ Writing to file\n");
    bytesWritten = fwrite((void *)filebuf, (size_t)1, (size_t)filelen, fp);

    /* validate it all loaded correctly */
    sbprintf(buf, "þ Validating size correct %d=%d\n",(int)bytesWritten,(int)filelen);
    if(bytesWritten != filelen) { sbprintf(buf, "\nfailed to write buffer\n"); fclose(fp); return(-2); }

    /* close the file */
    sbprintf(buf, "þ All OK, closing file\n");
    fclose(fp);

    return(0);
}

static int memcmp_mask(const void *ptr1, const void *ptr2, const void *mask, size_t len)
{
    const uint8_t *p1 = (const uint8_t*)ptr1;
    const uint8_t *p2 = (const uint8_t*)ptr2;
    const uint8_t *m = (const uint8_t*)mask;

    while(len--)
    {
        int diff = m?(*p2 & *m)-(*p1 & *m):*p2-*p1;
        if (diff) return diff>0?1:-1;
        p1++;
        p2++;
        if (m) m++;
    }
    return 0;
}

/* returns -1 on failure, start if found */
int search_image(const struct ImageHandle *ih, size_t start, const void *needle, const void *mask, int len, int align)
{
    if (start<0) return -1;

    for (;start+len<ih->len;start+=align)
    {
        if(memcmp_mask(ih->d.u8+start, needle, mask, len)==0)
        {
            // printf("got one at 0x%x\n", start);
            return start;
        }
    }
    return -1;
}

void hexdump(const uint8_t *buf, int len, const char *end)
{
    int i=len;
    while(i--)
    printf("%02x%s", *buf++, ((i&0xf)==0 && len>32)?"\n":i?" ":"");
    printf("%s", end);
}

#if 0
#ifdef _WIN32
int snprintf(char *str, size_t size, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = _vsnprintf(str, size, format, ap);
    va_end(ap);
    str[size-1]='\0';

    return count;
}
#endif
#endif
/* vim: set sw=4 ts=8 expandtab: */
