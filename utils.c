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

int iload_file(struct ImageHandle *ih, const char *fname, int rw)
{
	// init image handle structure to zero's
	memset(ih, 0, sizeof(*ih));
	// load file into memory
	if(((ih->d.p)= (void *)load_file(fname,&ih->len)) == 0) return -1;
	return 0;
}

int ifree_file(struct ImageHandle *ih)
{
	if((ih->d.p) != 0) free(ih->d.p);
	memset(ih, 0, sizeof(*ih));
	return 0;
}

/* load a file into memory and return buffer */
uint8_t *load_file(const char *filename, size_t *filelen)
{
	FILE *fp;
	uint8_t *data;
	size_t size,bytesRead;

	/* open file */
	printf("þ Opening '%s' file\n",filename);
	if ((fp = (FILE *)fopen(filename, "rb")) == NULL){ printf("\nCan't open file \"%s\".", filename); return(0); }

	/* get file length */
	printf("þ Getting length of '%s' file\n",filename);
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if(size <= 0) { printf("Error: Problem with seeking filesize\n"); fclose(fp); return(0); }

	*filelen = size;		/* return size of file to caller */

	/* alloc buffer for file */
	printf("þ Allocating buffer of %zd bytes\n",size);
	data = (uint8_t *)malloc(size);
	if(data == 0) { printf("\nfailed to allocate memory to load module\n"); fclose(fp); return 0; }

	/* load file into buffer */
	printf("þ Reading file to buffer\n");
	bytesRead = fread(data, 1, size, fp);

	/* validate it all loaded correctly */
	printf("þ Validating size correct %zd=%zd\n",bytesRead,size);
	if(bytesRead != size) { printf("\nfailed to load module into buffer\n"); free(data); fclose(fp); return 0; }

	/* close the file */
	printf("þ Closing file\n\n");
	fclose(fp);
	return(data);
}

/* load a file into memory and return buffer */
int save_file(const char *filename, const uint8_t *filebuf, size_t filelen)
{
	FILE *fp;
	size_t bytesWritten;

	/* open file */
	printf("þ Opening '%s' file for writing\n",filename);
	if ((fp = (FILE *)fopen(filename, "wb")) == NULL){ printf("\nCan't open file \"%s\".", filename); return(-1); }

	/* load file into buffer */
	printf("þ Writing to file\n");
	bytesWritten = fwrite((void *)filebuf, (size_t)1, (size_t)filelen, fp);

	/* validate it all loaded correctly */
	printf("þ Validating size correct %d=%d\n",(int)bytesWritten,(int)filelen);
	if(bytesWritten != filelen) { printf("\nfailed to write buffer\n"); fclose(fp); return(-2); }

	/* close the file */
	printf("þ All OK, closing file\n\n");
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

/* returns -1 on failure, start if found, start+align if not found */
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
    // printf("failed, returning 0x%x\n", start);
    return start;
}

void hexdump(uint8_t *buf, int len, const char *end)
{
    while(len--) printf("%02x%s", *buf++, len?" ":"");
    printf("%s", end);
}
