/* me7sum [firmware management tool for Bosch ME7.x firmware]
   By 360trev and nyet

   Inspired by work from Andy Whittaker's (tools and information)
   See http://www.andywhittaker.com/ECU/BoschMotronicME71.aspx

   Note: Uses configuration files (see my ini file example)

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>	/* isprint() */

#include "os/os.h"

#include "inifile_prop.h"
#include "crc32.h"
#include "str.h"
#include "utils.h"
#include "range.h"
#include "md5.h"
#include "rsa.h"

//#define DEBUG_ROM_INFO
//#define DEBUG_ROMSYS_MATCHING
//#define DEBUG_CRC_MATCHING
//#define DEBUG_ROMSYS_PP_MATCHING
//#define DEBUG_RSA_MATCHING
//#define DEBUG_MAIN_MATCHING
//#define DEBUG_MULTIPOINT_MATCHING

#include "debug.h"

#define CHECK_BOOTROM_MP
#define RSA_MODULUS_SIZE	1024
#define RSA_BLOCK_SIZE		(RSA_MODULUS_SIZE/8)

/* Images with 2+64 main mp descriptors do not have an end marker */
#ifdef CHECK_BOOTROM_MP
#define MAX_MP_BLOCK_LEN 66
#else
#define MAX_MP_BLOCK_LEN 64
#endif

// structures
struct ChecksumPair {
	uint32_t	v;	// value
	uint32_t	iv;	// inverse value
};

struct MultipointDescriptor {
	struct Range            r;
	struct ChecksumPair     csum;
};

#if 0
static int sbhexdump(struct strbuf *buf, const void *p, int len)
{
	int i=len;
	int ret=0;
	const uint8_t *ptr=p;
	while(i--)
		ret+=sbprintf(buf, "%02x%s", *ptr++, ((i&0xf)==0 && len>32)?"\n":i?" ":"");
	return ret;
}

static int sbprintdesc(struct strbuf *buf, const struct MultipointDescriptor *d)
{
	int ret=sbprintf(buf, " ");
	ret+=sbhexdump(buf, d, sizeof(*d));
	ret+=sbprintf(buf, "\n");
	ret+=sbprintf(buf, " %x-%x %x %x\n", d->r.start, d->r.end, d->csum.v, d->csum.iv);
	return ret;
}
#endif

#ifndef __GIT_VERSION
#define __GIT_VERSION "unknown"
#endif

#define MAX_CRC_BLKS 4
#define MD5_MAX_BLKS 4
// main firmware checksum validation
struct rom_config {
	int			readonly;
	uint32_t	base_address;				/* rom base address */

	struct {
		uint32_t	n;	/* offset of modulus */
		uint32_t	e;	/* offset of exponent */
		uint32_t	s;	/* offset of signature */
		uint32_t	ds; /* offset of default signature (unused?) */
		int			exponent;	/* actual exponent */
		struct Range md5[MD5_MAX_BLKS];
	} rsa;
	uint32_t	romsys;
	uint32_t	crctab[2];
	uint32_t	multipoint_block_start[2];	/* start of multipoint block descriptors (two sets, first one isn't always there) */
	uint32_t	multipoint_desc_len;		/* size of descriptors */
	uint32_t	main_checksum_offset;		/* two start/end pairs, one at offset, other at offset+8 */
	uint32_t	main_checksum_final;		/* two 4 byte checksum (one inv) for two blocks conctatenated above) */
	struct {
		struct Range r;
		uint32_t	offset;
	} crc[MAX_CRC_BLKS+1];					/* 0/4 is pre-region (for kbox and other) Up to 5 CRC blocks (total) to check */
	uint32_t	csm_offset;				/* ME7.1.1 */
};

struct info_config {
	InfoItem	EPK;
	InfoItem	sw_number;
	InfoItem	hw_number;
	InfoItem	part_number;
	InfoItem	sw_version;
	InfoItem	engine_id;
};

// globals
static FILE *ReportFile = NULL;
static struct rom_config Config;
static struct info_config InfoConfig;
#ifdef DEBUG_YES
static int Verbose = 2;
#else
static int Verbose = 0;
#endif

static int ChecksumsFound = 0;
static int ErrorsUncorrectable = 0;
static int ErrorsFound = 0;
static int ErrorsCorrected = 0;

//
// List of configurable properties to read from config file into our programme...
// [this stops us having to hardcode values into the code itself]
//
static PropertyListItem romProps[] = {
	// get rom region information
	{	GET_VALUE,  &Config.base_address,			"ignition", "rom_firmware_start",		"0x800000"},
	{	GET_VALUE,  &Config.multipoint_block_start[0],	"ignition", "rom_checksum_block_start0",	"0"},
	{	GET_VALUE,  &Config.multipoint_block_start[1],	"ignition", "rom_checksum_block_start",	"0"},
	{	GET_VALUE,  &Config.multipoint_desc_len,	"ignition", "rom_checksum_desc_len",	"0x10"},
	{	GET_VALUE,  &Config.main_checksum_offset,	"ignition", "rom_checksum_offset",		"0"},
	{	GET_VALUE,  &Config.main_checksum_final,	"ignition", "rom_checksum_final",		"0"},
	{	GET_VALUE,  &Config.crc[0].r.start,			"ignition", "rom_crc0_start",			"0"},
	{	GET_VALUE,  &Config.crc[0].r.end,			"ignition", "rom_crc0_end",				"0"},
	{	GET_VALUE,  &Config.crc[1].r.start,			"ignition", "rom_crc1_start",			"0"},
	{	GET_VALUE,  &Config.crc[1].r.end,			"ignition", "rom_crc1_end",				"0"},
	{	GET_VALUE,  &Config.crc[1].offset,			"ignition", "rom_crc1",					"0"},
	{	GET_VALUE,  &Config.crc[2].r.start,			"ignition", "rom_crc2_start",			"0"},
	{	GET_VALUE,  &Config.crc[2].r.end,			"ignition", "rom_crc2_end",				"0"},
	{	GET_VALUE,  &Config.crc[2].offset,			"ignition", "rom_crc2",					"0"},
	{	GET_VALUE,  &Config.crc[3].r.start,			"ignition", "rom_crc3_start",			"0"},
	{	GET_VALUE,  &Config.crc[3].r.end,			"ignition", "rom_crc3_end",				"0"},
	{	GET_VALUE,  &Config.crc[3].offset,			"ignition", "rom_crc3",					"0"},
	{	GET_VALUE,  &Config.crc[4].r.start,			"ignition", "rom_crc4_start",			"0"},
	{	GET_VALUE,  &Config.crc[4].r.end,			"ignition", "rom_crc4_end",				"0"},
	{	GET_VALUE,  &Config.crc[4].offset,			"ignition", "rom_crc4",					"0"},
	{ END_LIST,   0, "",""},
};

static InfoListItem romInfo[] = {
	// get rom region information
	{	"EPK",			GET_VALUE, &InfoConfig.EPK,			"info", "epk",			"0", "41"},
	{	"Part Number",	GET_VALUE, &InfoConfig.part_number,	"info", "part_number",	"0", "12"},
	{	"Engine ID",	GET_VALUE, &InfoConfig.engine_id,	"info", "engine_id",	"0", "17"},
	{	"SW Version",	GET_VALUE, &InfoConfig.sw_version,	"info", "sw_version",	"0", "4"},
	{	"HW Number",	GET_VALUE, &InfoConfig.hw_number,	"info", "hw_number",	"0", "10"},
	{	"SW Number",	GET_VALUE, &InfoConfig.sw_number,	"info", "sw_number",	"0", "10"},
	{ NULL,END_LIST,NULL,NULL,NULL}
};

static int FindRomInfo(const struct ImageHandle *ih);
static int DoRomInfo(const struct ImageHandle *ih, struct section *osconfig);

static int FindROMSYS(struct ImageHandle *ih);
static int DoROMSYS(struct ImageHandle *ih); // Startup in RSA, MP; ParamPage in RSA, MP, Main CSM, Main CRC

static int FindMainCRCPreBlk(const struct ImageHandle *ih);
static int FindMainCRCBlks(const struct ImageHandle *ih);
static int FindMainCRCOffsets(const struct ImageHandle *ih);
static int DoMainCRCs(struct ImageHandle *ih); // In ROMSYS Program Pages (sometimes), Main CSM, MP

static int FindMainCSMOffsets(const struct ImageHandle *ih);
static int DoMainCSMs(struct ImageHandle *ih); // In Main Program CSM, MP

static int DoROMSYS_ProgramPages(struct ImageHandle *ih); // In RSA (sometimes, in tuned files), MP

static int FindRSAOffsets(struct ImageHandle *ih);
static int FindMD5Ranges(struct ImageHandle *ih);
static int DoRSA(struct ImageHandle *ih); // In Main Program CSM, MP

static int FindCRCTab(const struct ImageHandle *ih);
static int DoCRCTab(struct ImageHandle *ih);

static int FindMainProgramOffset(const struct ImageHandle *ih);
static int FindMainProgramFinal(const struct ImageHandle *ih);
static int DoMainProgramCSM(struct ImageHandle *ih); // In MP

static int FindChecksumBlks(const struct ImageHandle *ih, int which);
static int DoChecksumBlk(struct ImageHandle *ih, uint32_t nStartBlk, struct strbuf *buf, int bootrom);

static void usage(const char *prog)
{
	printf("Usage: %s [-v] [-i <config.ini>] <inrom.bin> [outrom.bin]\n", prog);
	printf("       %s [-v] [-i <config.ini>] [-r <report.txt>] [-s] <inrom.bin>\n", prog);
	exit(-1);
}

static int bytecmp(const void *buf, uint8_t byte, size_t len)
{
	int i;
	const uint8_t *p=buf;
	for(i=0;i<len;i++) {
		if (p[i]!=byte)
			return p[i]-byte;
	}
	return 0;
}

#ifdef CHECK_BOOTROM_MP
/* returns 0 if desc[0] and [1] not in bootrom */
/* returns 1 if desc[0] and [1] are in bootrom, updates ih->bootrom_whitelist if whitelisted */
/* returns -1 if not in whitelist AND does not match next pair of non-bootrom descriptors */
static int check_whitelist(struct ImageHandle *ih, uint32_t addr)
{
	struct MultipointDescriptor desc[4];
	/* Check for hardcoded bootrom csums... if there, treat as ok */
	static const uint32_t whitelist[][2] = {{0x0fa0f5cf, 0x0f4716b3},
											{0x0e59d5c8, 0x1077fb35}};
	int i;

	for(i=0;i<4;i++)
		memcpy_from_le32(desc+i, ih->d.u8+addr+Config.multipoint_desc_len*i,
			sizeof(struct MultipointDescriptor));

	for(i=0;i<2;i++) {
		if (desc[i].r.start>=desc[i].r.end) return 0;
		if (desc[i].r.start>=Config.base_address) return 0;
		if (desc[i].r.end>=Config.base_address) return 0;
	}

	if (desc[0].r.start!=0 || desc[0].r.end!=0x3fff) return 0;
	if (desc[1].r.start!=0x4000 || desc[1].r.end!=0x7fff) return 0;

	for(i=0;i<2;i++) {
		if (desc[0].csum.v==~desc[0].csum.iv &&
			desc[1].csum.v==~desc[1].csum.iv &&
			whitelist[i][0]==desc[0].csum.v &&
			whitelist[i][1]==desc[1].csum.v) {
			ih->bootrom_whitelist=1;
			return 1;
		}
	}

	if ((desc[0].csum.v != desc[2].csum.v) ||
		(desc[1].csum.v != desc[3].csum.v) ||
		(desc[0].csum.iv != desc[2].csum.iv) ||
		(desc[1].csum.iv != desc[3].csum.iv)) {
		printf("ERROR! Inconsistency in non-whitelisted bootrom multipoint descriptors!\n");
		ErrorsUncorrectable++;
		return -1;
	}

	return 1;
}
#endif

/*
 * main()
 *
 */
int main(int argc, char **argv)
{
	int Step=0;
	int	iTemp;
	int summary=0;
	char *prog=argv[0];
	char *inifile=NULL;
	char *reportfile=NULL;
	char *input=NULL;
	char *output=NULL;
	int i, c;
	struct ImageHandle ih;
	struct section *osconfig=NULL;
	struct strbuf buf;

	memset(&buf, 0, sizeof(buf));

	// information about the tool
	printf("ME7Sum (%s) [Management tool for Bosch ME7.x firmwares]\n",
		__GIT_VERSION);
	printf("Inspiration from Andy Whittaker's tools and information.\n");
	printf("Written by 360trev and nyet [BSD License Open Source].\n");

	opterr=0;

	while ((c = getopt(argc, argv, "qsvi:r:")) != -1)
	{
		switch (c)
		{
			case 'q':
				Verbose--;
				break;
			case 's':
				summary++;
				break;
			case 'v':
				Verbose++;
				break;
			case 'i':
				inifile=optarg;
				break;
			case 'r':
				reportfile=optarg;
				break;
			case '?':
				if (optopt == 'i')
					fprintf(stderr, "Option -%c requires an argument.\n", optopt);
				else if (isprint(optopt))
					fprintf(stderr, "Unknown option '-%c'.\n", optopt);
				// break; // fallthrough
			default:
				usage(prog);
				return -1;
		}
	}

	if (Verbose<0) Verbose=0;

	argc-=optind;
	argv+=optind;

	if (argc==0 || argc>2)
		usage(prog);

	input = argv[0];

	if (argc>1)
		output = argv[1];
	else
		Config.readonly=1;

	if (summary && output) {
		fprintf(stderr, "-s cannot be used with output file\n");
		usage(prog);
		return -1;
	}

	if (reportfile && output) {
		fprintf(stderr, "-r cannot be used with output file\n");
		usage(prog);
		return -1;
	}

	if (inifile)
	{
		printf("Attempting to open firmware config file '%s'\n",inifile);
		// load properties file into memory
		osconfig = read_properties(inifile);
		if(osconfig == NULL)
		{
			fprintf(stderr, "failed to open ini file %s\n", inifile);
			return -1;
		}
	}

	if (reportfile) {
		ReportFile = fopen(reportfile, "w");
		if (!ReportFile) {
			fprintf(stderr, "failed to open report file %s: %s\n", reportfile, strerror(errno));
			return -1;
		}
	}

	// get rom region information from config file (see defined property list)
	process_properties_list(osconfig, romProps);
	process_info_list(osconfig, romInfo);

	// open the firmware file
	printf("\nAttempting to open firmware file '%s'\n",input);
	i=iload_file(&ih, input, 0, &buf);
	if (buf.pbuf) {
		if (i || Verbose>1) printf("%s", buf.pbuf);
		free(buf.pbuf);
	}
	if (i)
	{
		printf("Failed to open firmware file '%s'\n",input);
		ErrorsFound++;
		return -1;
	}

	// sanity check: validate firmware file is at least 512kbytes length before proceeding.
	if(ih.len != 512*1024 && ih.len != 1024*1024)
	{
		printf("File is an odd size (%d bytes). Are you sure this is a firmware dump?\n",
			(int)ih.len);
		ErrorsFound++;
		goto out;
	}

	if(ih.len == (1024*1024))
	{
		/* Check to make sure it isn't a doubled up file or padded
		   with ff or 00 */
		if (memcmp(ih.d.u8, ih.d.u8+512*1024, 512*1024)==0) {
			printf("File is doubled up 512k dump. Treating as 512k\n");
				ih.len=512*1024;
			ih.pad = PADDING_DOUBLED;
		} else if (bytecmp(ih.d.u8+512*1024, 0xff, 512*1024)==0) {
			printf("File is padded from 512k to 1024k with 0xFF. Treating as 512k\n");
				ih.len=512*1024;
			ih.pad = PADDING_FF;
		} else if (bytecmp(ih.d.u8+512*1024, 0xff, 512*1024-32)==0) {
			printf("File is padded from 512k to 1024k with 0xFF. Treating as 1024k but will try 512k CRC hardcoded blocks\n");
			ih.pad = PADDING_TRY_512K_CRC;
/*
		} else if (bytecmp(ih.d.u8+512*1024, 0, 512*1024)==0) {
			printf("File is padded from 512k to 1024k with zeros. Treating as 512k\n");
				ih.len=512*1024;
			ih.pad = PADDING_00;
*/
		}
	}


	//
	// ROM info
	//

	printf("\nStep #%d: Reading ROM info ..\n", ++Step);
	if(InfoConfig.part_number.off==0)
	{
		FindRomInfo(&ih);
	}

	if(InfoConfig.part_number.off)
	{
		DoRomInfo(&ih, osconfig);
	}
	else
	{
		printf("Step #%d: ERROR! Skipping ROM info.. UNDEFINED\n", Step);
		ErrorsUncorrectable++;
	}

	if(summary && summary<=Step) goto out;

	DEBUG_EXIT_ROM;


	//
	// ROMSYS
	//
	printf("\nStep #%d: Reading ROMSYS ..\n", ++Step);

	if(Config.romsys==0)
	{
		FindROMSYS(&ih);
	}

	if(Config.romsys)
	{
		DoROMSYS(&ih);
	}
	else
	{
		printf("Step #%d: ERROR! Skipping ROMSYS.. UNDEFINED\n", Step);
		ErrorsUncorrectable++;
	}

	if(summary && summary<=Step) goto out;

	DEBUG_EXIT_ROMSYS;

	//
	// CRC table(s)
	//
	printf("\nStep #%d: Finding CRC table(s) ..\n", ++Step);
	if(!Config.crctab[0])
		FindCRCTab(&ih);
	if(Config.crctab[0]) {
		DoCRCTab(&ih);
	} else {
		printf("Step #%d: ERROR! Couldn't find CRC table(s)\n", Step);
		ErrorsUncorrectable++;
	}

	if(summary && summary<=Step) goto out;


	//
	// RSA
	//
	printf("\nStep #%d: Reading RSA signatures ..\n", ++Step);

	FindRSAOffsets(&ih);
	if(Config.rsa.n && Config.rsa.s && Config.rsa.e) {
		FindMD5Ranges(&ih);
		if (Config.rsa.md5[0].start && Config.rsa.md5[0].end) {
			DoRSA(&ih);
		} else {
			printf("Step #%d: ERROR! Detected RSA signature, but no MD5 regions\n", Step);
			ErrorsUncorrectable++;
		}
	}

	if(summary && summary<=Step) goto out;

	DEBUG_EXIT_RSA;


	//
	// Main data CRC/checksums if specified
	//
	printf("\nStep #%d: Reading Main Data CRC/Checksums ..\n", ++Step);

	if(Config.crc[0].r.start==0 && Config.crc[0].r.end==0)
	{
		FindMainCRCPreBlk(&ih);
	}

	if(Config.crc[1].r.start==0 && Config.crc[1].r.end==0)
	{
		FindMainCRCBlks(&ih);
	}

	// note, crc0 and crc4 don't have offsets!
	if(Config.crc[1].offset==0)
	{
		FindMainCRCOffsets(&ih);	/* Detect if using CRC algo */
	}

	if(Config.csm_offset==0)
	{
		FindMainCSMOffsets(&ih);	/* Detect if using Checksum algo */
	}

	if(Config.crc[1].r.start && Config.crc[1].r.end &&
		(Config.crc[1].offset || Config.csm_offset)) {
		if(Verbose && Config.csm_offset) {
			if(Config.crc[1].offset) {
				printf(" %s has both main CRC and checksum offsets!\n",
					ih.filename);
			} else {
				printf("WARNING: %s has no main CRC offset(s) but does have a main checksum offset!\n",
					ih.filename);
				DoMainCRCs(&ih);
			}
		}

		/* Note: both CRC and checksum are possible! */
		if(Config.crc[1].offset)
		{
			DoMainCRCs(&ih);
		}

		if(Config.csm_offset)
		{
			DoMainCSMs(&ih);
		}
	}
	else
	{
		printf("Step #%d: ERROR! Skipping Main Data checksums ... UNDEFINED\n",
			Step);
#ifdef DEBUG_CRC_MATCHING
		DoMainCRCs(&ih);
		DoMainCSMs(&ih);
#endif
		ErrorsUncorrectable++;
	}

	if(summary && summary<=Step) goto out;

	DEBUG_EXIT_CRC;


	//
	// ROMSYS Program Pages
	//
	if(Config.romsys)
	{
		printf("\nStep #%d: ROMSYS Program Pages\n", ++Step);
		DoROMSYS_ProgramPages(&ih);
	}
	else
	{
		printf("Step #%d: ERROR! Skipping ROMSYS Program Pages.. UNDEFINED\n", Step);
		ErrorsUncorrectable++;
	}

	if(summary && summary<=Step) goto out;

	DEBUG_EXIT_ROMSYS_PP;


	//
	// Main program checksums
	//
	printf("\nStep #%d: Reading Main Program Checksums ..\n", ++Step);
	if(Config.main_checksum_offset==0)
	{
		FindMainProgramOffset(&ih);
	}

	if(Config.main_checksum_final==0)
	{
		FindMainProgramFinal(&ih);
	}

	if (Config.main_checksum_offset && Config.main_checksum_final)
	{
		//DoMainProgramCSM(&ih, Config.main_checksum_offset, Config.main_checksum_final);
		DoMainProgramCSM(&ih);
	}
	else
	{
		printf("Step #%d: ERROR! Skipping Main Program Checksums.. UNDEFINED\n", Step);
		ErrorsUncorrectable++;
	}

	if(summary && summary<=Step) goto out;

	DEBUG_EXIT_MAIN;


	//
	// Multi point checksums
	//
	printf("\nStep #%d: Reading Multipoint Checksum Blocks ..\n", ++Step);

	for (i=0;i<2;i++) {
		if(Config.multipoint_block_start[i]==0)
		{
			FindChecksumBlks(&ih, i);
		}

		if(Config.multipoint_block_start[i])
		{
			int bootrom=0;
			int printed_dots=0;

#ifdef CHECK_BOOTROM_MP
			/* Only check for whitelist in main multipoint block */
			if (i==1)
				bootrom = check_whitelist(&ih, Config.multipoint_block_start[i]);
#endif

			/* Images with 2+64 main MP descriptors do not have an end marker */
			for(iTemp=0; iTemp<MAX_MP_BLOCK_LEN; iTemp++)
			{
				int result=0;
				struct strbuf buf;

				if (iTemp>1) bootrom=0;

				memset(&buf, 0, sizeof(buf));
				sbprintf(&buf, "%2d) ",iTemp+1);
				result = DoChecksumBlk(&ih,
					Config.multipoint_block_start[i]+(Config.multipoint_desc_len*iTemp),
					&buf, bootrom);
				if (buf.pbuf) {
					if (iTemp<3 || result<0 || Verbose>0 || iTemp>MAX_MP_BLOCK_LEN-4)
					{
						printf("%s", buf.pbuf);
						printed_dots=0;
					}
					else if (!printed_dots) {
						printed_dots=1;
						printf(" ..........\n");
					}
					free (buf.pbuf);
				}

				if (result == 1) { break; } // end of blocks;
			}
			printf(" Multipoint #%d: [%d blocks x <16> = %d bytes]\n", i+1, iTemp, iTemp*16);
		}
		else
		{
			if (i!=0) {
				printf("Step #%d: ERROR! Skipping Multipoint Checksum Block... UNDEFINED\n", Step);
				ErrorsUncorrectable++;
			}
		}
	}

	DEBUG_EXIT_MULTIPOINT;

	/* if (!Config.readonly) */ {
		int errs;
		printf("\nStep #%d: Looking for rechecks ..\n", ++Step);
		if ((errs=ProcessRecordDeps())) {
			printf("\n*** WARNING! Unsatisfied rechecks. You may have to rerun ME7Sum on this file!\n");
			ErrorsFound+=errs;
		}
	}

	//
	// All done!
	//
	printf("\n*** Found %d checksums in %s\n", ChecksumsFound, input);

	if(ErrorsUncorrectable)
	{
		printf("\n*** ABORTING! %d uncorrectable error(s) in %s! ***\n", ErrorsUncorrectable, input);
		return -1;
	}

	if(output && ErrorsCorrected > 0)
	{
		struct strbuf buf;

		memset(&buf, 0, sizeof(buf));
		printf("\nAttempting to output corrected firmware file '%s'\n",output);
		// write crc corrected file out
		if (ih.pad == PADDING_DOUBLED) {
			memcpy(ih.d.u8, ih.d.u8+512*1024, 512*1024);
		}
		save_file(output,ih.d.p,ih.pad==PADDING_NONE?ih.len:ih.len*2, &buf);
		if(buf.pbuf) {
			printf("%s", buf.pbuf);
			free(buf.pbuf);
		}
	}

out:

	// close the file
	if(ih.d.p != 0) { ifree_file(&ih); }

	// free config
	if(osconfig != 0) { free_properties(osconfig); }

	// Made minor alterations in output to circumvent issue #9 @nyetwurk
	if (ErrorsCorrected!=ErrorsFound) {
		printf("\n*** WARNING! %d/%d uncorrected error(s) in %s! ***\n",
			ErrorsFound-ErrorsCorrected, ErrorsFound, input);
	} else if (ErrorsFound == 0 && output){
		printf("\n*** No errors were found and so no \"%s\" was generated.\n", output);
	} else if (output) {
		printf("\n*** DONE! %d/%d error(s) in %s corrected in %s! ***\n", ErrorsCorrected,
			ErrorsFound, input, output);
	} else {
		printf("\n*** DONE! %d error(s) in %s! ***\n", ErrorsFound, input);
	}

	if (ReportFile) {
		PrintAllRecords(ReportFile);
		fclose(ReportFile);
	}

	FreeAllRecords();

	return 0;
}

/*
 * GetRomInfo
 *
 * - uses config file to parse rom data and show interesting information about this rom dump
 */

static int GetRomInfo(const struct ImageHandle *ih, struct section *osconfig)
{
	InfoListItem *info;
	int max_len=0;

	if(ih == NULL) return(-1);

	// Find the longest label so we know how big the label column should be
	for(info=romInfo; info->attr_type!=END_LIST; info++)
	{
		if(info->item->off && info->item->len && strlen(info->label) > max_len) {
			max_len=strlen(info->label);
		}
	}

	if (!max_len) { return -1; }

	for(info=romInfo; info->attr_type!=END_LIST; info++)
	{
		char *str_data;
		InfoItem *item=info->item;
		if(item->off == 0 || item->len == 0)
		{
			continue;
		}

		if(item->off+item->len >= ih->len)
		{
			printf("%s = INVALID OFFSET/LEN 0x%x/%d\n",info->label, item->off, item->len);
			continue;
		}
		str_data=malloc(item->len+1);
		/* snprintf null terminates for us if string is too long :) */
		snprintf(str_data, item->len+1, "%s", ih->d.s+item->off);	// Leave room for null termination
		printf(" %-*s : '%s'\n", max_len, info->label, str_data);
		free(str_data);
	}
	return 0;
}

static int DoRomInfo(const struct ImageHandle *ih, struct section *osconfig)
{
	uint32_t num_of;
	int i, max_len=0;

	if(ih == NULL) return(-1);

	GetRomInfo(ih, osconfig);

	if ((num_of = get_property_value(osconfig, "dumps", "dump_show", NULL))<=0)
	{
		return 0;
	}

	// Find the longest label so we know how big the label column should be
	for(i=1;i<=num_of;i++)
	{
		char label_str[81];
		const char * ptr_label;

		snprintf(label_str, sizeof(label_str), "dump_%d_label", i);
		ptr_label = get_property(osconfig, "dumps", label_str,  NULL);
		if(ptr_label) {
			if(strlen(ptr_label)>max_len) {
				max_len = strlen(ptr_label);
			}
			ptr_label=NULL;
		}
	}

	printf("\nROM Dumps:\n");

	//
	// Dynamically walks through the config file and shows all properties defined...
	//
	for(i=1;i<=num_of;i++)
	{
		char type_str[81];
		char visible_str[81];
		char label_str[81];
		char offset_str[81];
		char length_str[81];

#ifdef DEBUG_ROM_INFO
		const char * ptr_type;
#endif
		const char * ptr_visible;
		const char * ptr_label;
		uint32_t ptr_offset;
		uint32_t ptr_length;

		snprintf(type_str,   sizeof(type_str), "dump_%d_type",      i);
		snprintf(visible_str,sizeof(visible_str), "dump_%d_visible",i);
		snprintf(label_str,  sizeof(label_str), "dump_%d_label",    i);
		snprintf(offset_str, sizeof(offset_str), "dump_%d_offset",  i);
		snprintf(length_str, sizeof(length_str), "dump_%d_len",     i);

		// get config out of ini file...
#ifdef DEBUG_ROM_INFO
		ptr_type    = get_property(       osconfig, "dumps", type_str,    NULL);
#endif
		ptr_visible = get_property(       osconfig, "dumps", visible_str, NULL);
		ptr_label   = get_property(       osconfig, "dumps", label_str,   NULL);
		ptr_offset  = get_property_value( osconfig, "dumps", offset_str,  NULL);
		ptr_length  = get_property_value( osconfig, "dumps", length_str,  NULL);

		if(ptr_length == 0)
		{
			// zero length, skip
		}
		else if(ptr_offset+ptr_length >= ih->len)
		{
			printf("%s = INVALID OFFSET/LEN 0x%x/%d\n",ptr_label, ptr_offset, ptr_length);
		}
		else
		{
			char str_data[1024];
			// restrict maximum dump to 1kbyte [buffer size]
			if(ptr_length > sizeof(str_data) - 1) ptr_length = sizeof(str_data) - 1;	// Leave room for null termination
			DEBUG_ROM("\n%s = %s\n",type_str,    ptr_type);
			DEBUG_ROM("%s = %s\n",visible_str,   ptr_visible);
			DEBUG_ROM("%s = '%s'\n",label_str,   ptr_label);
			DEBUG_ROM("%s = 0x%x\n",offset_str,  ptr_offset);
			DEBUG_ROM("%s = %d\n",length_str,    ptr_length);

			/* snprintf null terminates for us if string is too long :) */
			snprintf(str_data, sizeof(str_data), "%s", ih->d.s+ptr_offset);
			if(! strcmp("true",ptr_visible))
			{
				printf(" %-*s : '%s'\n", max_len, ptr_label, str_data);
			}
			else
			{
				printf(" %-*s = 'HIDDEN'\n", max_len, ptr_label);
			}
		}
	}
	return 0;
}

/* NEEDLE/HAYSTACK util */
static int FindData(const struct ImageHandle *ih, const char *what,
	const uint8_t *n, const uint8_t *m, int len,	// needle, mask, len of needle/mask
	int off_l, int off_h,							// where to find hi/lo (short word offset into find array)
	uint32_t *offset, size_t offset_len,			// array to store discovered offsets, len of array
	uint32_t *where)								// address of match (ONLY if single match), NULL if not needed
{
	/* Note that off_l and off_h are SHORT WORD offsets, i.e. 1 == 2 bytes */

	int i, found=0;
	uint32_t last_where=0;

	assert((len&1)==0); // make sure its even

	for(i=0;i+len<ih->len;i+=2)
	{
		i=search_image(ih, i, n, m, len, 2);
		if (i<0) break;
		else {
			int high_shift=16;
			uint16_t low=le16toh(ih->d.u16[i/2+off_l]);
			uint16_t high=le16toh(ih->d.u16[i/2+off_h]);
			uint32_t addr;

			/* maybe segment address */
			if (high&0xfe00) high_shift=14;

			addr=(high<<high_shift) | low;

			if (Verbose>1) {
				printf(" Found possible %s #%d at 0x%x (from 0x%x)\n",
					what, found+1, addr, i);
			}

			if (addr>Config.base_address && addr-Config.base_address<ih->len) {
				if (Verbose>2) {
					hexdump(ih->d.u8+i-4, 4, " [");
					hexdump(ih->d.u8+i, len, "] ");
					hexdump(ih->d.u8+i+1, 4, "\n");
				}

				if(found<offset_len)
				{
					offset[found]=addr-Config.base_address;
					last_where = i;
				}
				found++;
			} else if (Verbose>1) {
				printf(" %s #%d at 0x%x (from 0x%x) out of range\n",
					what, found+1, addr, i);
			}
		}
	}
	if (found==1 && where) *where=last_where;
	return found;
}

//
// Calculate the Bosch Motronic ME71 checksum for the given range
//
static uint32_t CalcChecksumBlk8(const struct ImageHandle *ih, const struct Range *r)
{
	uint32_t	nChecksum = 0, nIndex;

	for(nIndex = r->start; nIndex <= r->end; nIndex++)
	{
		nChecksum+=ih->d.u8[nIndex]; // 8 bit source, 32 bit accumulator
	}

	return nChecksum;
}

static uint32_t CalcChecksumBlk16(const struct ImageHandle *ih, const struct Range *r)
{
	uint32_t	nChecksum = 0, nIndex;

	for(nIndex = r->start/2; nIndex <= r->end/2; nIndex++)
	{
		nChecksum+=le16toh(ih->d.u16[nIndex]);
	}

	return nChecksum;
}

static int NormalizeRange(const struct ImageHandle *ih, struct Range *r)
{
	// special case: leave end markers alone
	if (r->start==0xffffffff && r->end==0xffffffff) return 0;

	// We are only reading the ROM. Therefore the start address must be
	// after Config.base_address. Ignore addresses lower than this and
	// remove the offset for addresses we're interested in.
	if (r->start < Config.base_address || r->end < Config.base_address)
	{
		// The checksum block is outside our range
		printf(" ERROR: INVALID STARTADDDR/ENDADDR 0x%x/0x%x is less than base address 0x%x\n",
			r->start, r->end, Config.base_address);
		return -1;
	}

	r->start -= Config.base_address;
	r->end   -= Config.base_address;

	if(r->start>r->end)
	{
		// start is after end!
		printf(" ERROR: INVALID STARTADDDR/ENDADDR: 0x%x>0x%x\n", r->start, r->end);
		return -1;
	}

	if(r->start>=ih->len || r->end>=ih->len)
	{
		// The checksum block is outside our range
		printf(" ERROR: INVALID STARTADDDR/ENDADDR: 0x%x/0x%x is past 0x%x\n", r->start, r->end, (int)ih->len);
		return -1;
	}

	return 0;
}

/* Actual work */
static int FindEPK(const struct ImageHandle *ih)
{
	//										LL    LL                      HH?
	static const uint8_t n[]={0x43, 0xF8, 0x00, 0x00, 0x9d, 0x07, 0x09, 0x80};
	static const uint8_t m[]={0xf3, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xf0};
	int i, off=0, high, low, found=0;
	int max = 0x30000;
	const char *start;
	int len=0;
	int ret=-1;

	printf(" Searching for EPK signature...");

	for(i=0;i<max;i+=2)
	{
		i=search_image(ih, i, n, m, sizeof(n), 2);
		if (i<0 || i>max) break;
		low = le16toh(ih->d.u16[i/2+1]);
		high = le16toh(ih->d.u16[i/2+3])>>8;	// ??
		off = (high<<16) | low;
		if (off<Config.base_address || off+sizeof(n) > Config.base_address+ih->len) {
			printf(" ERROR: INVALID ADDR 0x%x\n", off);
			break;
		}
		off -= Config.base_address;
		if (Verbose) {
			printf( "%s: possible EPK @0x%x, ASM @0x%x\n", ih->filename, off, i);
			if (Verbose>1) {
				hexdump(ih->d.u8+i-4, 4, "[");
				hexdump(ih->d.u8+i, sizeof(n), "]");
				hexdump(ih->d.u8+i+sizeof(n), 4,  "\n\n");
				hexdump(ih->d.u8+off, 0x40, "\n");
			}
		}
		found++;
	}

	if (found==1) {
		ret=0;
	} else {
		static const char sig[]={0xc3, 0x3c, 0x5a, 0x5a, 0xff, 0xff};
		i=0x10000-2;
		if (memcmp(ih->d.u8+i, sig, sizeof(sig))==0) {
			off=i+6;
			if (Verbose) {
				printf(" %s: found EPK @0x%x, sig 0x%x\n", ih->filename, off, i);
				if (Verbose>1) {
					hexdump(ih->d.u8+i, 6, "\n\n");
					hexdump(ih->d.u8+i+6, 0x40, "\n");
				}
			}
			ret=0;
		} else {
			off=0;
		}
	}

	if (ret) {
		printf("missing\n");
		return ret;
	}

	start = ih->d.s+off+1;

	if(start[1]==0x0a) start+=2;

	for(len=1;len<0x40;len++) {
		if (start[len-1]=='/' && start[len]==(char)0xff) break;
	}

	if(len>=0x40) {
		printf("missing\n");
		return -1;
	}

	InfoConfig.EPK.off=start-ih->d.s;
	InfoConfig.EPK.len=len;

	printf("OK\n");
	return 0;
}

struct string_desc {
	uint8_t tag;
	uint8_t len;
	uint16_t ptr;
	uint16_t seg;
};

static int getInfoItem(const struct ImageHandle *ih, InfoItem *ii, const struct string_desc *d)
{
	int ptr = le16toh(d->ptr);
	int seg = le16toh(d->seg);
	int addr = (seg<<14) | ptr;
	if(d->tag==6) {
		if(addr>Config.base_address && addr+d->len<Config.base_address+ih->len) {
			ii->off=addr-Config.base_address;
			ii->len=d->len;
			return d->len;
		}
	}
	return -1;
}

static int dump_string_desc(const struct ImageHandle *ih, const struct string_desc *d)
{
	struct InfoItem ii={0,0};
	char buf[257];
	int ret = getInfoItem(ih, &ii, d);

	if (ret<0) return -1;

	if (ii.len) {
		snprintf(buf, ii.len+1, "%s", ih->d.s+ii.off);
		printf("%d: '%s'\n", ret, buf);
	}

	return 0;
}

static int FindECUID(const struct ImageHandle *ih)
{
	int found;
	uint32_t offset[2]={0,0};
	uint32_t where=0;
	// E6 F4 .. .. E6 F5 06 02 F6 F4 42 E2 F6 F5 44 E2
	// E6 F4 .. .. E6 F5 06 02 F6 F4 40 E2 F6 F5 42 E2
	// \e6f4..e6f50602f6f4.e2f6f5.e2
	//                                LL    LL                HH    HH
	uint8_t needle[] = {0xE6, 0xF4, 0x00, 0x00, 0xE6, 0xF5, 0x06, 0x02, 0xF6, 0xF4, 0x42, 0xE2, 0xF6, 0xF5, 0x44, 0xE2};
	uint8_t   mask[] = {0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xf0, 0xff};

	printf(" Searching for ECUID table...");

	found=FindData(ih, "ECUID table", needle, mask, sizeof(needle), 1, 3, offset, 2, &where);

	if (found>0)
	{
		int i;
		const struct string_desc *d0=(struct string_desc *)(ih->d.u8+offset[0]);
		const struct string_desc *d1=(struct string_desc *)(ih->d.u8+offset[1]);
		if (Verbose>2) {
			for(i=0; i<30; i++) {
				if (d0[i].tag==6) {
					printf("0 %d:", i);
					dump_string_desc(ih, d0+i);
					printf("\n");
				}
			}
			if (found>1) {
				for(i=0; i<30; i++) {
					if (d1[i].tag==6) {
						printf("1 %d:", i);
						dump_string_desc(ih, d1+i);
						printf("\n");
					}
				}
			}
		}
		if (getInfoItem(ih, &InfoConfig.hw_number, d0+2)<=0 && found>1) {
			d0=(const struct string_desc *)(ih->d.u8+offset[1]);
			d1=(const struct string_desc *)(ih->d.u8+offset[0]);
			getInfoItem(ih, &InfoConfig.hw_number, d0+2);
		}
		/*
		dump_string_desc(ih, d0+4);
		dump_string_desc(ih, d0+10);
		dump_string_desc(ih, d0+11);
		dump_string_desc(ih, d0+19);
		*/
		getInfoItem(ih, &InfoConfig.sw_number, d0+4);
		getInfoItem(ih, &InfoConfig.part_number, d0+10);
		getInfoItem(ih, &InfoConfig.sw_version, d0+11);
		getInfoItem(ih, &InfoConfig.engine_id, d0+19);
		//if(found>1)
		//	getInfoItem(ih, &InfoConfig.HW_MAN, d1+1);
		printf("OK\n");
		return 0;
	}

	if (found>1)
	{
		printf("Too many matches (%d). ECUID table find failed\n", found);
	}

	printf("%d matches, missing\n", found);
	return 0;
}

static int FindRomInfo(const struct ImageHandle *ih)
{
	int ret=0;
	ret+=FindEPK(ih);
	ret+=FindECUID(ih);
	return ret;
}

static int FindRSAOffsets(struct ImageHandle *ih)
{
	int s=0,n=0,e=0;
	int exponent=0;
	int i;
	int ret=0;
	static const uint8_t needle[2][14] = {
		//                                     LL    LL                HH    HH
		{0x80, 0x00, 0x88, 0x40, 0xE6, 0xF4, 0x00, 0x00, 0xE6, 0xF5, 0x80, 0x00, 0x88, 0x50},
		{0xE0, 0x44, 0x88, 0x40, 0xE6, 0xF4, 0x00, 0x00, 0xE6, 0xF5, 0x80, 0x00, 0x88, 0x50}
	};
	static const uint8_t mask[] =
		{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff};

	for(i=0;i<2;i++) {
		int found;
		uint32_t offset[2]={0,0};
		uint32_t where=0;

		printf(" Searching for RSA offset #%d...", i);
		found=FindData(ih, "RSA offset", needle[i], mask, sizeof(needle[i]), 3, 5, offset, 2, &where);

		if (found==(i?1:2))
		{
			DEBUG_RSA(" Found RSA offset[0] #%d 0x%x\n", i, offset[0]);

			if (i==0) {
				DEBUG_RSA(" Found RSA offset[1] #%d 0x%x\n", i, offset[1]);
				s=offset[0];
				n=offset[1];
			} else {
				e=offset[0];
			}

			printf("OK\n");
			continue;
		}

		if (found>(i?1:2))
		{
			DEBUG_RSA("%d: Too many matches (%d). RSA find failed\n", i, found);
			continue;
		}

		printf("missing\n");
		ret=-1;
	}

	if (ret) return ret;

	if (s+RSA_BLOCK_SIZE>=ih->len) return -1;
	if (n+RSA_BLOCK_SIZE>=ih->len) return -1;
	if (e+4+RSA_BLOCK_SIZE>=ih->len) return -1;

	exponent=ntohl(*(uint32_t*)(ih->d.u8+e));

	if (exponent!=3) return -1;

	Config.rsa.s=s;
	Config.rsa.n=n;
	Config.rsa.e=e;
	Config.rsa.ds=e+4;
	Config.rsa.exponent=exponent;

	printf("         Signature: @%x-%x\n", s, s+RSA_BLOCK_SIZE);
	printf("           Modulus: @%x-%x\n", n, n+RSA_BLOCK_SIZE);
	printf("          Exponent: @%x = %d\n", e, exponent);
	if (Verbose) {
		printf(" Default Signature: @%x-%x\n", e+4, e+4+RSA_BLOCK_SIZE);
	}

	return 0;
}

static int FindMD5Ranges(struct ImageHandle *ih)
{
	//           r                                         LL    LL
	uint8_t needle[] =
		{0xE1, 0x08, 0xF7, 0xF8, 0x00, 0xF0, 0xF2, 0xF4, 0x00, 0x00, 0xF2, 0xF5, 0x00, 0x00, 0xF6, 0xF4};
	uint8_t   mask[] =
		{0xff, 0xcf, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff};

	int found=0, i=0;
	int addr=-1;
	int table=0, count=0;

	printf(" Searching for MD5 ranges...");
	for(i=0;i+sizeof(needle)+2<ih->len;i+=2) {
		i=search_image(ih, i, needle, mask, sizeof(needle), 2);
		if (i<0) break;
		found++;
		DEBUG_RSA(" Found possible MD5 ASM #%d @0x%x\n", found, i);
		addr=i;
	}

	if (found!=1) {
		printf("missing\n");
		return -1;
	} else {
		uint8_t *p = ih->d.u8+addr;
		uint16_t *p16 = ih->d.u16+(addr/2);

		count = ((p[1]&0xf0)>>4)+1;
		/* FIXME: hardcoded HH HH to 0x0081xxxx? */
		table =le16toh(p16[4])|0x10000;
		// printf("MD5 arg2 0x%04x\n", le16toh(p16[6]));

		DEBUG_RSA(" Found MD5 ASM @0x%x (table=%x, count=%d)\n", addr, table,
			count);
		if(count>0 && count<=MD5_MAX_BLKS) {
			uint32_t buf[MD5_MAX_BLKS*2];
			memcpy_from_le32(buf, ih->d.u8+table, sizeof(buf));
			for (i=0;i<count;i++) {
				Config.rsa.md5[i].start = buf[i];	/* first n are start */
				Config.rsa.md5[i].end = buf[i+4];	/* next n are end */
				DEBUG_RSA("0x%08x: 0x%08x-0x%08x\n", table+i,
					Config.rsa.md5[i].start, Config.rsa.md5[i].end);
				NormalizeRange(ih, Config.rsa.md5+i);
			}
		}
	}

	printf("OK\n");

	printf(" MD5 Block Offset Table @%05x [%d bytes]:\n",
		table, count * 2 * (int)sizeof(uint32_t));

	return 0;
}

static int mpz_export_buf(uint8_t *buf, int len, mpz_t x)
{
	size_t size = (mpz_sizeinbase(x, 256));
	int off = len-size;

	if (size>len) {
		printf("size %d>len %d\n", (int)size, len);
		return -1;
	}

	/* pad with zeros */
	if (off>0) memset(buf, 0, off);

	mpz_export(buf+off, &size, 1, 1, 0, 0, x);

	return size;
}

static int rsa_block_pad(uint8_t *blk, const uint8_t *data, int len)
{
	if (len+3>RSA_BLOCK_SIZE) {
		if (Verbose)
			printf("Data too long %d+3>%d\n", len, RSA_BLOCK_SIZE);
		return -1;
	}

	blk[0]=0;
	blk[1]=1;
	memset(blk+2, 0xff, RSA_BLOCK_SIZE-2-len-1);
	blk[RSA_BLOCK_SIZE-len-1]=0;
	memcpy(blk+RSA_BLOCK_SIZE-len, data, len);
	return 0;
}

static int rsa_block_unpad(uint8_t *data, int len, const uint8_t *blk)
{
	int i;

	/* expect 00 01 prefix */
	if(blk[0]!=0x00 || blk[1]!=0x01) {
		if (Verbose>1)
			printf("bad prefix [%x %x]!\n", blk[0], blk[1]);
		return 0;
	}

	for(i=2;blk[i] && i<RSA_BLOCK_SIZE-len;i++);

	if (i+1<11) {
		printf(" ERROR: Only %d bytes of padding (expected at least 11)\n", i+1);
		hexdump(blk, RSA_BLOCK_SIZE, "\n");
		return -1;
	}

	if (len+i+1<RSA_BLOCK_SIZE) {
		if (Verbose) {
			printf(" Warning: Padded block is only %d (%d+%d+1) bytes of %d\n", len+i+1, len, i, RSA_BLOCK_SIZE);
			hexdump(blk, RSA_BLOCK_SIZE, "\n");
		}
	}

	if(blk[i]!=0) {
		if (Verbose)
			printf("no null term! %x [%x] %x\n",
				blk[i-1], blk[i], blk[i+1]);

		return -1;
	}
	memcpy(data, blk+i+1, len);
	return 0;
}

static int RSASign(struct ImageHandle *ih)
{
	uint8_t calc_md5[16];
	uint8_t msg[RSA_BLOCK_SIZE];
	uint8_t nmsg[RSA_BLOCK_SIZE];
	uint8_t n[RSA_BLOCK_SIZE];
	uint8_t sig[RSA_BLOCK_SIZE];
	private_key ku;
	public_key kp;
	mpz_t M, C;
	MD5_CTX ctx;
	int i;
	int ret;

	// Initialize public key
	mpz_init(kp.n);
	mpz_init(kp.e);
	// Initialize private key
	mpz_init(ku.n);
	mpz_init(ku.e);
	mpz_init(ku.d);
	mpz_init(ku.p);
	mpz_init(ku.q);

	/* assumes exp 3 */
	ret=generate_keys(&ku, &kp);
	if (ret) goto out;

	if (Verbose>1) {
		printf("\n");
		printf("---------------Private Key-----------------\n");
		printf("kp.n is [%s]\n", mpz_get_str(NULL, 16, kp.n));
		printf("kp.e is [%s]\n", mpz_get_str(NULL, 16, kp.e));
		printf("---------------Public Key------------------\n");
		printf("ku.n is [%s]\n", mpz_get_str(NULL, 16, ku.n));
		printf("ku.e is [%s]\n", mpz_get_str(NULL, 16, ku.e));
		printf("ku.d is [%s]\n", mpz_get_str(NULL, 16, ku.d));
		printf("ku.p is [%s]\n", mpz_get_str(NULL, 16, ku.p));
		printf("ku.q is [%s]\n", mpz_get_str(NULL, 16, ku.q));
	}

	/* put new modulus in place */
	memset(n, 0, sizeof(n));
	mpz_export_buf(n, sizeof(n), kp.n);
	if (Verbose>1) {
		printf("new n is:\n");
		hexdump(msg, RSA_BLOCK_SIZE, "\n");
	}
	memcpy(ih->d.u8+Config.rsa.n, n, RSA_BLOCK_SIZE);

	/* recalc MD5 */
	MD5_Init(&ctx);
	for(i=0;i<MD5_MAX_BLKS;i++) {
		int len=Config.rsa.md5[i].end-Config.rsa.md5[i].start+1;
		if (len>0) {
			MD5_Update(&ctx, ih->d.u8+Config.rsa.md5[i].start, len);
		}
	}
	MD5_Final(calc_md5, &ctx);

	/* pad msg, append md5 */
	rsa_block_pad(msg, calc_md5, 16);

	/* setup decryption */
	mpz_init(C);
	mpz_init(M);
	mpz_import(C, sizeof(msg), 1, 1, 0, 0, msg);
	block_decrypt(M, C, ku);

	memset(sig, 0, sizeof(sig));
	mpz_export_buf(sig, sizeof(sig), M);

	if (Verbose>1) {
		printf("padded md5 is:\n");
		hexdump(msg, RSA_BLOCK_SIZE, "\n");
		printf("new sig is:\n");
		hexdump(sig, RSA_BLOCK_SIZE, "\n");
	}

	/* verify that the signature will generate the new MD5 with new pub key */
	block_encrypt(C, M, kp);
	memset(nmsg, 0, sizeof(nmsg));
	mpz_export_buf(nmsg, sizeof(nmsg), C);

	if(Verbose>1) {
		printf("new padded md5 is:\n");
		hexdump(nmsg, RSA_BLOCK_SIZE, "\n");
	}

	if(memcmp(msg, nmsg, sizeof(msg))==0) {
		memcpy(ih->d.u8+Config.rsa.s, sig, RSA_BLOCK_SIZE);
	} else {
		printf("padded md5 is:\n");
		hexdump(msg, RSA_BLOCK_SIZE, "\n");
		printf("new padded md5 is:\n");
		hexdump(nmsg, RSA_BLOCK_SIZE, "\n");
		printf("FAILED\n");
		ret=-1;
	}

	mpz_clear(C);
	mpz_clear(M);

out:
	mpz_clear(kp.n);
	mpz_clear(kp.e);
	mpz_clear(ku.n);
	mpz_clear(ku.e);
	mpz_clear(ku.d);
	mpz_clear(ku.p);
	mpz_clear(ku.q);

	return ret;
}

static int DoRSA(struct ImageHandle *ih)
{
	public_key kp;
	mpz_t M, DM, C, DC;
	uint8_t buf[RSA_BLOCK_SIZE];
	uint8_t dbuf[RSA_BLOCK_SIZE];
	uint8_t md5[16];
	uint8_t dmd5[16];
	uint8_t calc_md5[16];
	MD5_CTX ctx;
	int i;

	struct ReportRecord *rrn = CreateRecord("RSA mod", Config.rsa.n, RSA_BLOCK_SIZE);
	struct ReportRecord *rrs = CreateRecord("RSA sig", Config.rsa.s, RSA_BLOCK_SIZE);

	memset(md5, 0, sizeof(md5));
	memset(dmd5, 0, sizeof(dmd5));
	memset(calc_md5, 0, sizeof(calc_md5));

	mpz_init(kp.n);
	mpz_init(kp.e);
	mpz_init(M);
	mpz_init(DM);
	mpz_init(C);
	mpz_init(DC);

	mpz_import(kp.n, RSA_BLOCK_SIZE, 1, 1, 0, 0, ih->d.u8+Config.rsa.n);
	mpz_set_ui(kp.e, Config.rsa.exponent);
	mpz_import(M, RSA_BLOCK_SIZE, 1, 1, 0, 0, ih->d.u8+Config.rsa.s);
	mpz_import(DM, RSA_BLOCK_SIZE, 1, 1, 0, 0, ih->d.u8+Config.rsa.ds);

	block_encrypt(C, M, kp);
	block_encrypt(DC, DM, kp);

	if (Verbose>1) {
		printf("modulus:\n");
		hexdump(ih->d.u8+Config.rsa.n, RSA_BLOCK_SIZE, "\n");
		printf("signature:\n");
		hexdump(ih->d.u8+Config.rsa.s, RSA_BLOCK_SIZE, "\n");
	}

	memset(buf, 0, sizeof(buf));
	memset(dbuf, 0, sizeof(dbuf));

	mpz_export_buf(buf, sizeof(buf), C);
	mpz_export_buf(dbuf, sizeof(dbuf), DC);

	mpz_clear(kp.n);
	mpz_clear(kp.e);
	mpz_clear(M);
	mpz_clear(DM);
	mpz_clear(C);
	mpz_clear(DC);

	if (Verbose>1) {
		printf("sig->padded MD5:\n");
		hexdump(buf, RSA_BLOCK_SIZE, "\n");
		printf("defsig->padded MD5:\n");
		hexdump(dbuf, RSA_BLOCK_SIZE, "\n");
	}

	ChecksumsFound ++;

	MD5_Init(&ctx);
	for(i=0;i<MD5_MAX_BLKS;i++) {
		int len=Config.rsa.md5[i].end-Config.rsa.md5[i].start+1;
		if (len>0) {
			AddRange(rrn, Config.rsa.md5+i);
			AddRange(rrs, Config.rsa.md5+i);
			printf(" %d) 0x%08X-0x%08X\n", i+1,
				Config.rsa.md5[i].start,
				Config.rsa.md5[i].end);
			MD5_Update(&ctx, ih->d.u8+Config.rsa.md5[i].start, len);
		}
	}

	MD5_Final(calc_md5, &ctx);

	/*
	printf("DEncrMD5: ");
	if (rsa_block_unpad(dmd5, 16, dbuf))
		ErrorsUncorrectable++;
	else
		hexdump(dmd5, 16, "\n");
	*/

	if (rsa_block_unpad(md5, 16, buf))
		ErrorsUncorrectable++;
	else {
		printf(" EncrMD5: ");
		hexdump(md5, 16, "\n");
	}

	printf(" CalcMD5: ");
	hexdump(calc_md5, 16, "\n");

	if (memcmp(md5, calc_md5, 16)) {
		ErrorsFound++;
		if (Config.readonly)
		{
			printf(" @%x-%x sig ** NOT OK **\n", Config.rsa.s, Config.rsa.s+RSA_BLOCK_SIZE);
			printf(" @%x-%x mod ** NOT OK **\n", Config.rsa.n, Config.rsa.n+RSA_BLOCK_SIZE);
			return -1;
		}
		else
		{
			if (RSASign(ih)) {
				ErrorsUncorrectable++;
				printf(" ** UNFIXABLE **\n");
			} else {
				ErrorsCorrected++;
				printf(" ** FIXED **\n");
			}
		}
	}
	else
	{
		printf("  OK\n");
	}

	return 0;
}

static int FindROMSYS(struct ImageHandle *ih)
{
	/* autodetect? */
	/* verify stuff is in range? */
	Config.romsys=0x8000;
	return 0;
}

struct ROMSYSDescriptor {
	uint32_t		res00_0F[4];		/* +0x00-0x0F */

	uint32_t		all_param_sum_p;	/* +0x10 */
	uint32_t		res14_1F[3];		/* +0x14-0x1F */

	uint32_t		res20_2F[4];		/* +0x20-0x2F */

	struct Range	all_param;			/* +0x30-0x37 */
	uint32_t		startup_sum;		/* +0x38 */
	uint32_t		program_pages_csum;	/* +0x3C */
};

static int DoROMSYS_Startup(struct ImageHandle *ih, const struct ROMSYSDescriptor *desc)
{
	uint16_t *r16[2];
	uint32_t nCalcStartupSum;
	int off = Config.romsys + offsetof(struct ROMSYSDescriptor, startup_sum);

	struct ReportRecord *rr = CreateRecord("ROMSYS Startup", off, 3);
	AddRangeStartLength(rr, 0x8000, 2);
	AddRangeStartLength(rr, 0xFFFE, 2);

	r16[0]=(uint16_t *)(ih->d.u8 + 0x8000);
	r16[1]=(uint16_t *)(ih->d.u8 + 0xFFFE);
	nCalcStartupSum = le16toh(*r16[0])+le16toh(*r16[1]);

	printf(" Startup section: word[0x008000]+word[0x00FFFE]\n");
	printf(" @%05x Add=0x%08X CalcAdd=0x%08X", off,
		nCalcStartupSum, desc->startup_sum);

	ChecksumsFound ++;

	if (nCalcStartupSum != desc->startup_sum)
	{
		ErrorsFound++;
		if (Config.readonly)
		{
			printf(" ** NOT OK **\n");
			return -1;
		}
		else
		{
			uint32_t *p32 = (uint32_t *)(ih->d.u8 + off);
			*p32=le32toh(nCalcStartupSum);
			ErrorsCorrected++;
			printf(" ** FIXED **\n");
		}
	}
	else
	{
		printf("  ADD OK\n");
	}

	return 0;
}

static uint32_t ProgramPageSum(struct ImageHandle *ih, const struct Range *r, struct ReportRecord *rr)
{
	uint32_t sum=0;
	int addr;
	for(addr=r->start;addr<r->end;addr+=8*1024) {
		uint16_t *p16[2];
		p16[0]=(uint16_t *)(ih->d.u8+addr);			/* first word of page */
		AddRangeStartLength(rr, addr, 2);
		p16[1]=(uint16_t *)(ih->d.u8+addr+8*1024-2);	/* last word of page */
		AddRangeStartLength(rr, addr+8*1024-2, 2);
		if (Verbose>4)
			printf("      word[0x%06X]+word[0x%06X]\n",
				addr, addr+8*1024-2);
		sum+=le16toh(*p16[0]) + le16toh(*p16[1]);
	}
	return sum;
}

static int DoROMSYS_ProgramPages(struct ImageHandle *ih)
{
	uint32_t nCalcProgramPagesSum;
	struct Range r;
	int off = Config.romsys + offsetof(struct ROMSYSDescriptor, program_pages_csum);
	uint32_t *p32 = (uint32_t *)(ih->d.u8 + off);
	struct ReportRecord *rr;

	printf(" Program pages: 8k page first+last in 0x0000-0xFFFF and 0x20000-0x%X\n",
		(int)ih->len-1);

	rr = CreateRecord("ROMSYS ProgramPages", off, 4);

	r.start=0x00000; r.end=0x0FFFF;
	nCalcProgramPagesSum=ProgramPageSum(ih, &r, rr);

	r.start=0x20000; r.end=ih->len-1;
	nCalcProgramPagesSum+=ProgramPageSum(ih, &r, rr);

	printf(" @%06x Add=0x%06X CalcAdd=0x%06X", off, nCalcProgramPagesSum, *p32);

	ChecksumsFound ++;

	if (nCalcProgramPagesSum != *p32)
	{
		ErrorsFound++;
		if (Config.readonly)
		{
			printf(" ** NOT OK **\n");
			return -1;
		}
		else
		{
			*p32=le32toh(nCalcProgramPagesSum);
			ErrorsCorrected++;
			printf(" ** FIXED **\n");
		}
	}
	else
	{
		printf("  ADD OK\n");
	}

	return 0;
}

static int DoROMSYS_ParamPage(struct ImageHandle *ih, struct ROMSYSDescriptor *desc)
{
	uint16_t *r16[2];
	uint32_t *p32;
	int off;
	uint32_t nAllParamSum, nCalcAllParamSum;

	struct ReportRecord *rr;

	if(desc->all_param_sum_p<Config.base_address ||
		desc->all_param_sum_p-Config.base_address>=ih->len) {
		printf(" ERROR: INVALID ADDR 0x%x\n", desc->all_param_sum_p);
		return -1;
	}

	off = desc->all_param_sum_p-Config.base_address;
	rr = CreateRecord("ROMSYS ParamPage", off, 4);

	p32 = (uint32_t *)(ih->d.u8 + off);
	nAllParamSum=le32toh(*p32);

	NormalizeRange(ih, &desc->all_param);

	AddRangeStartLength(rr, desc->all_param.start, 2);
	AddRangeStartLength(rr, desc->all_param.end, 2);

	r16[0]=(uint16_t *)(ih->d.u8 + desc->all_param.start);
	r16[1]=(uint16_t *)(ih->d.u8 + desc->all_param.end);
	nCalcAllParamSum = le16toh(*r16[0])+le16toh(*r16[1]);

	printf(" All param page: word[0x%06X]+word[0x%06X]\n",
		desc->all_param.start, desc->all_param.end);
	printf(" @%06x Add=0x%06X CalcAdd=0x%06X", off,
		nAllParamSum, nCalcAllParamSum);

	ChecksumsFound ++;

	if (nCalcAllParamSum != nAllParamSum)
	{
		ErrorsFound++;
		if (Config.readonly)
		{
			printf(" ** NOT OK **\n");
			return -1;
		}
		else
		{
			*p32=le32toh(nCalcAllParamSum);
			ErrorsCorrected++;
			printf(" ** FIXED **\n");
		}
	}
	else
	{
		printf("  ADD OK\n");
	}

	return 0;
}

static int DoROMSYS(struct ImageHandle *ih)
{
	struct ROMSYSDescriptor desc;
	int result = 0;

	if (ih->d.u16[0]==0) {
		printf(" ** ERROR! First word is zero... corrupted bin? **\n");
		ErrorsUncorrectable++;
	}

	memcpy_from_le32(&desc, ih->d.u8+Config.romsys, sizeof(desc));

	DEBUG_ROMSYS("00 0x%08X 0x%08X 0x%08X 0x%08X\n",
		desc.res00_0F[0], desc.res00_0F[1],
		desc.res00_0F[2], desc.res00_0F[3]);

	DEBUG_ROMSYS("allparam csum @0x%08X\n", desc.all_param_sum_p);

	DEBUG_ROMSYS("14 0x%08X 0x%08X 0x%08X\n",
		desc.res14_1F[0], desc.res14_1F[1],
		desc.res14_1F[2]);

	DEBUG_ROMSYS("20 0x%08X 0x%08X 0x%08X 0x%08X\n",
		desc.res20_2F[0], desc.res20_2F[1],
		desc.res20_2F[2], desc.res20_2F[3]);

	DEBUG_ROMSYS("allparam first/last: 0x%08X, 0x%08X\n",
		desc.all_param.start, desc.all_param.end);

	DEBUG_ROMSYS("startup_sum %08X\n", desc.startup_sum);

	DEBUG_ROMSYS("program_pages_csum %08X\n", desc.program_pages_csum);

	result |= DoROMSYS_Startup(ih, &desc);
	result |= DoROMSYS_ParamPage(ih, &desc);

	return result;
}

static int crc_table_fallback(const struct ImageHandle *ih, uint32_t *off)
{
	uint8_t needle[16];
	int i;
	int found=0;

	memcpy_to_le32(needle, crc32_tab, sizeof(needle));

	for(i=0;i+sizeof(needle)<ih->len;i+=2)
	{
		i=search_image(ih, i, needle, NULL, sizeof(needle), 2);
		if (i<0) break;
		else {
			if (Verbose>1) {
				printf(" Found possible CRC table #%d @0x%06x\n", found+1, i);
			}
			if(found<2) off[found]=i;
			found++;
		}
	}
	return found;
}

static int locate_helper(const struct ImageHandle *ih, uint32_t addr)
{
	uint8_t needle[6]={0,0,0,0,0,0};
	uint8_t mask[6]={0xff, 0xff, 0x00, 0x00, 0xff, 0xff};
	int i,found=0;

	addr+=Config.base_address;

	needle[0] = addr&0xff;
	needle[1] = (addr>>8)&0xff;
	needle[4] = (addr>>16)&0xff;
	needle[5] = (addr>>24)&0xff;

	for(i=0;i+sizeof(needle)<ih->len;i+=2)
	{
		i=search_image(ih, i, needle, mask, sizeof(needle), 2);
		if (i<0) break;
		printf("ref #%d %08X @0x%06x\n", ++found, addr, i);
		hexdump(ih->d.u8+i-8, 8, " [");
		hexdump(ih->d.u8+i, 2, "] ");
		hexdump(ih->d.u8+i+2, 2, " [");
		hexdump(ih->d.u8+i+4, 2, "] ");
		hexdump(ih->d.u8+i+6, 8, "\n");
	}

	return 0;
}

static int FindCRCTab(const struct ImageHandle *ih)
{
	uint32_t off[2]={0,0};
	int i, found=0;
	uint32_t where=0;


	// E6 F4 02 D8 E6 F5 81 00 A9 60 C0 62 5C 22
	// E6 F4 6A E7 E6 F5 81 00 A9 60 C0 62 5C 22
	// e6 f4 5a 77 e6 f5 82 00 a9 60 c0 62 5c 22

	// E6 F4 A6 DF E6 F5 81 00 C0 C2 5C 22
	// E6 F4 52 DB E6 f5 81 00 C0 C2 5C 22
	// e6 f4 32 b5 e6 f5 81 00 c0 c2 5c 22
	// e6 f4 28 dc e6 f5 81 00 c0 c2 5c 22

	static const uint8_t
	//                     LL   LL             HH   HH
	needle0[]={0xe6,0xf4,0x00,0x00,0xe6,0xf5,0x80,0x00,0xa9,0x60,0xc0,0x62,0x5c,0x22},
	  mask0[]={0xff,0xff,0x01,0x00,0xff,0xff,0xfc,0xff,0xff,0xff,0xff,0xff,0xff,0xff},

	needle1[]={0xe6,0xf4,0x00,0x00,0xe6,0xf5,0x80,0x00,0xc0,0xc2,0x5c,0x22},
	  mask1[]={0xff,0xff,0x01,0x00,0xff,0xff,0xfc,0xff,0xff,0xff,0xff,0xff};


	printf(" Searching for CRC table(s)...");
	DEBUG_FLUSH_CRC;

	found=FindData(ih, "CRC table", needle0, mask0, sizeof(needle0), 1, 3, off, 2, &where);

	if (found<1 || found>2)
		found=FindData(ih, "CRC table", needle1, mask1, sizeof(needle1), 1, 3, off, 2, &where);

	if (found<1 || found>2) {
		printf("missing\n");
	} else {
		int temp=found;
		found=0;
		for (i=0;i<temp;i++) {
			uint32_t crc0=le32toh(*((uint32_t *)(ih->d.u8+off[i])));
			if(crc0!=0) {
				printf("*** WARNING: ASM detect @0x%06x, CRC[0]=%08X in %s\n",
					off[i], crc0, ih->filename);
				continue;
			}
			Config.crctab[found++]=off[i];
		}
	}

	if (found<1 || found>2) {
		int temp = crc_table_fallback(ih, off);
		printf(" Searching for CRC table(s) using fallback...");
		if (temp<1 || temp>2) {
			printf("UNDEFINED\n");
			return -1;
		}

		for (i=0;i<found;i++) {
			locate_helper(ih, off[i]);
		}

		printf("*** WARNING: ASM detect failed, fell back (found %d) in %s\n",
			found, ih->filename);
	}

	printf("OK\n");
	return 0;
}

static int DoCRCTab(struct ImageHandle *ih)
{
	uint8_t letab[1024];
	int i;

	assert(sizeof(letab)==sizeof(crc32_tab));

	memcpy_to_le32(letab, crc32_tab, sizeof(letab));

	for(i=0;i<2;i++) {
		if(!Config.crctab[i]) continue;
		if(memcmp(ih->d.u8+Config.crctab[i], letab, sizeof(letab))) {
			printf(" CRC table #%d: ", i);
			ErrorsFound++;
			if(Verbose>2) {
				hexdump(ih->d.u8+Config.crctab[i], 1024, "\n");
				hexdump(letab, 1024, "\n");
			}
			if (Config.readonly)
			{
				printf(" ** NOT OK - TUNER MODIFIED? **\n");
				return -1;
			}
			memcpy(ih->d.u8+Config.crctab[i], letab, sizeof(letab));
			ErrorsCorrected++;
			printf(" ** FIXED - WARNING: REVERTED TUNER MODIFICATION! **\n");
		}
	}

	printf(" CRC table(s) OK\n");
	return 0;
}

static int FindMainCRCPreBlk(const struct ImageHandle *ih)
{
	int found;
	uint32_t offset=0;
	uint32_t where=0;
	//                                LL    LL                HH    HH          s
	uint8_t needle[] = {0xE6, 0xFC, 0x00, 0x00, 0xE6, 0xFD, 0x80, 0x00, 0xE0, 0x0E, 0xDA, 0x00, 0x00, 0x00, 0xF6, 0xF4};
	uint8_t   mask[] = {0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xf0, 0xff, 0xff, 0x0f, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff};

	printf(" Searching for main data CRC pre block...");
	DEBUG_FLUSH_CRC;

	found=FindData(ih, "CRC pre block", needle, mask, sizeof(needle), 1, 3, &offset, 1, &where);

	if (found==1)
	{
		// crc0 is reserved for pre-region
		Config.crc[0].r.start=offset;
		Config.crc[0].r.end=offset+(ih->d.u8[where+9]>>4)-1;	// s
		DEBUG_CRC("Found %s #%d 0x%x-0x%x (0x%x): ", "CRC pre block", 0, offset, Config.crc[0].r.end, where);

		printf("OK\n");
		return 0;
	}

	if (found>1)
	{
		DEBUG_CRC("Too many matches (%d). CRC/csum block start find failed\n", found);
	}

	printf("missing\n");
	return 0;
}

static int FindMainCRCBlks(const struct ImageHandle *ih)
{
	int i, found, ret0=-1, ret1=-1;
	uint32_t offset[MAX_CRC_BLKS];
	//                            LL    LL                HH    HH
	uint8_t n0[] = {0xE6, 0xF8, 0x00, 0x00, 0xE6, 0xF9, 0x80, 0x00, 0xF2, 0xF4 /*, 0x00, 0x00, 0x24, 0x8F */};
	uint8_t m0[] = {0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff /*, 0x00, 0x00, 0xff, 0xff */};
	//                                        LL    LL                HH    HH
	uint8_t n1[] = {0x10, 0x9B, 0xE6, 0xF4, 0x00, 0x00, 0xE6, 0xF5, 0x80, 0x00 /*, 0x26, 0xF4, 0x9B, 0xE6 */};
	uint8_t m1[] = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0xf0, 0xff /*, 0xff, 0xff, 0xff, 0xff */};

	printf(" Searching for main data CRC/csum blocks...");
	DEBUG_FLUSH_CRC;

	found=FindData(ih, "CRC/csum block starts", n0, m0, sizeof(n0), 1, 3, offset, MAX_CRC_BLKS, NULL);

	if (found>0 && found<=MAX_CRC_BLKS)
	{
		for (i=0;i<found;i++)
		{
			DEBUG_CRC("Found %s #%d at 0x%x\n", "CRC/csum block start", i+1, offset[i]);
			// crc0 is reserved for pre-region
			if (i<MAX_CRC_BLKS)
				Config.crc[i+1].r.start=offset[i];
			ret0=0;
		}
	}

	if (!found)
	{
		DEBUG_CRC("CRC/csum block start find failed\n");
	}
	else if (found>MAX_CRC_BLKS)
	{
		DEBUG_CRC("Too many matches (%d). CRC/csum block start find failed\n", found);
	}

	found=FindData(ih, "CRC/csum block end", n1, m1, sizeof(n1), 2, 4, offset, MAX_CRC_BLKS, NULL);

	if (found>0 && found<=MAX_CRC_BLKS)
	{
		for (i=0;i<found;i++)
		{
			DEBUG_CRC("Found %s #%d at 0x%x\n", "CRC/csum block end", i+1, offset[i]);
			// crc0 is reserved for pre-region
			if (i<MAX_CRC_BLKS)
				Config.crc[i+1].r.end=offset[i];
			ret1=0;
		}
	}

	if (!found)
	{
		DEBUG_CRC("CRC/csum block end find failed\n");
	}
	else if (found>MAX_CRC_BLKS)
	{
		DEBUG_CRC("Too many matches (%d). CRC/csum block end find failed\n", found);
	}

	if (ret0||ret1)
	{
		if (ih->len==512*1024 || ih->pad == PADDING_TRY_512K_CRC)
		{
			printf("missing\n");
			printf(" Falling back to default 512k CRC blocks...");
			Config.crc[1].r.start=0x10000;
			Config.crc[1].r.end=0x13fff;
			Config.crc[2].r.start=0x14300;
			Config.crc[2].r.end=0x17f67;
			Config.crc[3].r.start=0x18191;
			Config.crc[3].r.end=0x1fbff;
			ret0=ret1=0;
		}
	}

	printf("%s\n", (ret0||ret1)?"FAIL":"OK");
	return ret0||ret1;
}

#ifdef DEBUG_CRC_MATCHING
#define MAX_CRC_OFFSETS 10
#else
#define MAX_CRC_OFFSETS MAX_CRC_BLKS
#endif
static int FindMainCRCOffsets(const struct ImageHandle *ih)
{
	int i, found;
	uint32_t offset[MAX_CRC_OFFSETS];
	//                                                        LL    LL                HH    HH
	uint8_t needle[] = {0xF6, 0xF5, 0x00, 0x00, 0xE6, 0xF4, 0x00, 0x00, 0xE6, 0xF5, 0x80, 0x00, 0xDA, 0x00 /*, 0x00, 0x00, 0xe6, 0x00, 0x04, 0x02 */};
	uint8_t   mask[] = {0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x01, 0x00, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff /*, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff */};

	printf(" Searching for main data CRC offsets...");
	DEBUG_FLUSH_CRC;

	found=FindData(ih, "CRC offset", needle, mask, sizeof(needle), 3, 5, offset, MAX_CRC_OFFSETS, NULL);

	if (found>0 && found<=MAX_CRC_OFFSETS)
	{
		for (i=0;i<found;i++)
		{
			DEBUG_CRC("Found CRC offset #%d at 0x%x\n", i+1, offset[i]);
			// crc0 is reserved for pre-region
			if (i<MAX_CRC_BLKS)
				Config.crc[i+1].offset=offset[i];
		}
	}

	if (found!=3)
	{
		DEBUG_CRC("Did not find exactly 3 matches (got %d). CRC offset find failed\n", found);
		for(i=1;i<MAX_CRC_BLKS;i++) {
			Config.crc[i].offset=0;
		}
		printf("missing\n");
		return -1;
	}

	/* if we found exactly 3 crc values, and region 4 exists, use region 3's crc for calcing 3+4 */
	if (Config.crc[4].r.start && Config.crc[4].r.end)
	{
		Config.crc[4].offset=Config.crc[3].offset;
		Config.crc[3].offset=0;
	}

	printf("OK\n");
	return 0;
}

static int FindMainCSMOffsets(const struct ImageHandle *ih)
{
	int found;
	uint32_t offset;
	//                                             LL    LL                HH    HH
	uint8_t needle0[] = {0xE1, 0x0C, 0xE6, 0xF4, 0x00, 0x00, 0xE6, 0xF5, 0x80, 0x00, 0xDA, 0x00 /*, 0xf0, 0xe1, 0x0c, 0xe6 */};
	uint8_t needle1[] = {0x04, 0x00, 0xE6, 0xF4, 0x00, 0x00, 0xE6, 0xF5, 0x80, 0x00, 0xDA, 0x00 /*, 0xd8, 0x7e, 0xe6, 0x00 */};
	uint8_t    mask[] = {0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff /*, 0xf0, 0xff, 0xff, 0xff */};

	printf(" Searching for main data checksum offsets...");
	DEBUG_FLUSH_CRC;

	found=FindData(ih, "Checksum offset", needle0, mask, sizeof(needle0), 2, 4, &offset, 1, NULL);

	if(found<1) {
		/* try alternate pattern */
		found=FindData(ih, "Checksum offset", needle1, mask, sizeof(needle1), 2, 4, &offset, 1, NULL);
		if (found!=1) {
			printf("missing\n");
			return -1;
		}
	} else if (found>1) {
		DEBUG_CRC("Did not find exactly 1 match (got %d). Checksum offset find failed\n", found);
		printf("missing\n");
		return -1;
	}

	DEBUG_CRC("Found checksum offset at 0x%x\n", offset);
	Config.csm_offset=offset;
	printf("OK\n");
	return 0;
}

static int DoMainCRCs(struct ImageHandle *ih)
{
	int result=0;
	int i;
	uint32_t nCalcCRCSeed = 0;

	printf(" Main CRCs:\n");
	for (i=0; i<5; i++)
	{
		if(Config.crc[i].r.start && Config.crc[i].r.end)
		{
			uint32_t nStart = Config.crc[i].r.start;
			size_t nLen = Config.crc[i].r.end - Config.crc[i].r.start + 1;
			uint32_t nCRCAddr = Config.crc[i].offset;
			uint32_t nCRC;
			uint32_t nCalcCRC;
			uint32_t *p32;

			nCalcCRC = crc32(nCalcCRCSeed, ih->d.u8+nStart, nLen);

			printf(" %d) 0x%06X-0x%06X", i, Config.crc[i].r.start, Config.crc[i].r.end);

			if (nCRCAddr+4>ih->len)
			{
				printf(" @%05x INVALID ADDRESS\n", nCRCAddr);
			}
			else if (nCRCAddr)
			{
				struct ReportRecord *rr = CreateRecord("Main CRC", nCRCAddr, 3);
				AddRangeStartLength(rr, nStart, nLen);

				/* possibly unaligned, so we cant do tricks wtih ih->d.u32 */
				p32=(uint32_t *)(ih->d.u8 + nCRCAddr);
				nCRC=le32toh(*p32);

				printf(" @%05x CRC: %08X CalcCRC: %08X%s", nCRCAddr, nCRC, nCalcCRC, nCalcCRCSeed?"(r)":"   ");
				ChecksumsFound ++;

				if (nCalcCRC != nCRC)
				{
					ErrorsFound++;
					if (Config.readonly)
					{
						printf(" ** NOT OK **\n");
						result|=-1;
					}
					else
					{
						*p32=le32toh(nCalcCRC);
						ErrorsCorrected++;
						printf(" ** FIXED **\n");
					}
				}
				else
				{
					printf(" CRC OK\n");
				}
			} else {
				printf("                      CalcCRC: %08X%s\n", nCalcCRC, nCalcCRCSeed?"(r)":"   ");
			}

			if (Config.crc[0].r.start && Config.crc[0].r.end)
				nCalcCRCSeed=nCalcCRC;
		}
		else
		{
			DEBUG_CRC(" %d) 0x%06X-0x%06X SKIPPED\n", i,
				Config.crc[0].r.start, Config.crc[0].r.end);
		}
	}
	return result;
}

static int DoMainCSMs(struct ImageHandle *ih)
{
	int result=0;
	int i;
	uint32_t nCalcCSM = 0;
	uint32_t nCSMAddr = Config.csm_offset;
	uint32_t *p32;
	uint32_t nCSM, nCSMinv;
	struct ReportRecord *rr;

	if (nCSMAddr+4>ih->len)
	{
		printf(" @%05x INVALID ADDRESS\n", nCSMAddr);
		return -1;
	}

	/* possibly unaligned, so we cant do tricks wtih ih->d.u32 */
	p32 =(uint32_t *)(ih->d.u8 + nCSMAddr);
	nCSM = le32toh(p32[0]);
	nCSMinv = le32toh(p32[1]);

	printf(" Main Checksums:\n");
	rr = CreateRecord("Main Checksums", nCSMAddr, 3);

	for (i=1; i<5; i++)
	{
		if(Config.crc[i].r.start && Config.crc[i].r.end)
		{
			AddRange(rr, &Config.crc[i].r);
			printf(" %d) 0x%06X-0x%06X", i,
				Config.crc[i].r.start, Config.crc[i].r.end);

			/* bytewise checksum */
			nCalcCSM += CalcChecksumBlk8(ih, &Config.crc[i].r);

			printf(" CalcCSM: %08X\n", nCalcCSM);
		}
	}

	printf(" @%05x CSM: %08X CalcCSM: %08X", nCSMAddr, nCSM, nCalcCSM);
	ChecksumsFound ++;

	if (nCalcCSM != nCSM)
	{
		ErrorsFound++;
		if (Config.readonly)
		{
			printf(" ** NOT OK **\n");
			result|=-1;
		}
		else
		{
			p32[0]=le32toh(nCalcCSM);
			p32[1]=le32toh(~nCalcCSM);
			ErrorsCorrected++;
			printf(" ** FIXED **\n");
		}
	} else if (nCSM != ~nCSMinv) {
		printf(" @%05x CSM: %08X CSMinv: %08X (%08X)", nCSMAddr, nCSM, nCSMinv, ~nCSMinv);

		ErrorsFound++;
		if (Config.readonly)
		{
			printf(" ** NOT OK **\n");
			result|=-1;
		}
		else
		{
			p32[1]=le32toh(~nCSM);
			ErrorsCorrected++;
			printf(" ** FIXED **\n");
		}
	}

	else
	{
		printf(" OK\n");
	}
	return result;
}

static int FindMainProgramOffset(const struct ImageHandle *ih)
{
	int i, found=0, offset=0;
	uint32_t needle[4];
	uint32_t mask[4];

	printf(" Searching for main program checksum..");
	DEBUG_FLUSH_MAIN;

	needle[0]=htole32(Config.base_address);			/* 0x000000 */
	needle[1]=htole32(Config.base_address+0x0fbff);	/* 0x00fbff */
	needle[2]=htole32(Config.base_address+0x20000);	/* 0x020000 */
	needle[3]=htole32(Config.base_address+0xf00ff);	/* 0x07ffff 512k flash
													   0x0febff oddball?
													   0x0fffff 1M flash */
	mask[0]=htole32(0xffffffff);
	mask[1]=htole32(0xffffffff);
	mask[2]=htole32(0xffffffff);
	mask[3]=htole32(0xfff700ff);

	for(i=0;i+sizeof(needle)<ih->len;i+=2)
	{
		i=search_image(ih, i, needle, mask, sizeof(needle), 2);
		if (i<0) break;
		DEBUG_MAIN("Found possible main block descriptor at 0x%x\n", i);
		offset=i;
		found++;
	}

	if (found==1)
	{
		DEBUG_MAIN("Found main block descriptor at 0x%x\n", offset);
		Config.main_checksum_offset=offset;
		printf("OK\n");
		return 0;
	}
	else if (found>1)
	{
		DEBUG_MAIN("Found %d main block descriptor matches\n", found);
	}

	printf("FAIL\n");
	return -1;
}

static int FindMainProgramFinal(const struct ImageHandle *ih)
{
	int offset=ih->len-0x20;
	struct ChecksumPair *csum = (struct ChecksumPair *)(ih->d.u8+offset);

	if (csum->v == ~csum->iv)
	{
		DEBUG_MAIN("Found main csum at 0x%x\n", offset);
		Config.main_checksum_final=offset;
		return 0;
	}

	return -1;
}

//
// Reads the main checksum for the whole ROM
//
static int DoMainProgramCSM(struct ImageHandle *ih)
{
	int errors=0;
	struct Range r[2];
	struct ChecksumPair csum;
	uint32_t nCsumAddr = Config.main_checksum_final;
	uint32_t nCalcChksum;
	uint32_t nCalcChksum2;
	int inside=0;

	struct ReportRecord *rr = CreateRecord("Main Program Checksum", nCsumAddr, 8);

	printf(" ROM Checksum Block Offset Table @%05x [16 bytes]:\n",
		Config.main_checksum_offset);

	// C16x processors are little endian
	// copy from (le) buffer into our descriptor
	memcpy_from_le32(r, ih->d.u8+Config.main_checksum_offset, sizeof(r));

	if (NormalizeRange(ih, r) || NormalizeRange(ih, r+1) ||
		r[0].start==0xffffffff || r[1].start==0xffffffff)
	{
		printf(" ERROR! BAD MAIN CHECKSUM DESCRIPTOR(s)\n");
		ErrorsUncorrectable++;
		return -1;
	}

	// block 1
	nCalcChksum = CalcChecksumBlk16(ih, &r[0]);
	printf(" 1) 0x%06X-0x%06X CalcChk: %08X\n", r[0].start, r[0].end, nCalcChksum);

	if (r[0].end + 1 != r[1].start)
	{
		struct Range sr;
		uint32_t ss, sc;
		AddRange(rr, &sr);
		sr.start = r[0].end+1;
		sr.end = r[1].start-1;
		//struct Range sr = {.start = 0x10000, .end = 0x1FFFF};
		ss = CalcChecksumBlk16(ih, &sr);
		sc = crc32(0, ih->d.u8+sr.start, sr.end-sr.start+1);
		printf("    0x%06X-0x%06X CalcChk: %08X CalcCRC: %08X SKIPPED\n",
			sr.start, sr.end, ss, sc);
	}

	// C16x processors are little endian
	// copy from (le) buffer
	memcpy_from_le32(&csum, ih->d.u8+nCsumAddr, sizeof(csum));

	// block 2
	/* test if checksum is inside block, if so, mark it */
	if (nCsumAddr+8 >= r[1].start && nCsumAddr <= r[1].end) inside++;

	AddRange(rr, &r[1]);
	if (inside && csum.iv != ~csum.v) {
		// if csum inside and iv!=~v, pre-correct iv so v+iv cancels out
		// properly
		uint32_t temp;
		volatile struct ChecksumPair *cs=
			(struct ChecksumPair *)(ih->d.u8+nCsumAddr);
		temp = cs->iv;	// save inv
		cs->iv = ~cs->v;
		nCalcChksum2 = CalcChecksumBlk16(ih, &r[1]);
		cs->iv = temp;	// restore inv
	} else {
		nCalcChksum2 = CalcChecksumBlk16(ih, &r[1]);
	}

	nCalcChksum += nCalcChksum2;

	printf(" 2) 0x%06X-0x%06X CalcChk: %08X\n", r[1].start, r[1].end,
		nCalcChksum);

	printf(" @%05x Chk: %08X CalcChk: %08X", nCsumAddr, csum.v, nCalcChksum);
	ChecksumsFound ++;
	if (csum.v != nCalcChksum) { errors++; }

	if (csum.iv != ~csum.v) { errors++; }

	if(!errors)
	{
		printf(" OK%s\n", inside?" (i)":"");
		return 0;
	}

	ErrorsFound+=errors;

	if(Config.readonly)
	{
		printf(" ** NOT OK **\n");
		if (csum.iv!=~csum.v) {
			printf(" %08X!=%08X, ChkInv: %08X ** NOT OK **\n",
				csum.v, ~csum.iv, csum.iv);
			if (inside) {
				printf("*** WARNING! Checksum offset %x inside block 0x%x-0x%x!\n",
					nCsumAddr, r[1].start, r[1].end);
			}
		}
		return -1;
	}

	csum.v = nCalcChksum;
	csum.iv = ~nCalcChksum;

	memcpy_to_le32(ih->d.u8+nCsumAddr, &csum, sizeof(csum));

	printf(" ** FIXED **\n");
	ErrorsCorrected+=errors;
	return 0;
}

/* which=0: MP block #1 */
/* which=1: MP block #2 */
static int FindChecksumBlks(const struct ImageHandle *ih, int which)
{
	int i, found=0, offset=0;
	uint32_t needle[2];
	int size=0;

	printf(" Searching for multipoint block descriptor #%d...",
		which+1);
	DEBUG_FLUSH_MULTIPOINT;

	if (which==0) {
		needle[0]=htole32(Config.base_address+0x24000);
		/* actually, mp #1 isn't allowed to match this,
		   its in mp #2 */
		needle[1]=htole32(Config.base_address+0x27fff);
		size=4;
	} else {
#ifdef CHECK_BOOTROM_MP
		needle[0]=htole32(0);
		needle[1]=htole32(0x3fff);
#else
		needle[0]=htole32(Config.base_address);
		needle[1]=htole32(Config.base_address+0x3fff);
#endif
		size=8;
	}

	for(i=0x10000;i+Config.multipoint_desc_len<ih->len;i+=2)
	{
		DEBUG_MULTIPOINT("%d: Searching starting at 0x%d\n", which+1, i);
		i=search_image(ih, i, needle, NULL, size, 2);
		if (i<0) break;
		else {
			struct MultipointDescriptor *desc =
				(struct MultipointDescriptor *)(ih->d.u8+i);

			/* for mp block 1, don't be picky about inv */
			if ((which==0) || desc->csum.v==~desc->csum.iv)
			{
				/* make sure we don't match the mp #2 when looking for #1 */
				if(which || desc->r.end != needle[1]) {
					DEBUG_MULTIPOINT("%d: Found possible multipoint descriptor #%d at 0x%x\n",
						which+1, found+1, i);
					DEBUG_MULTIPOINT("0x%x-0x%x\n", desc->r.start, desc->r.end);
					offset=i;
					found++;
				}
			}
		}
	}

	if (found==1)
	{
		/* test next block to make sure it looks reasonable */
		struct MultipointDescriptor *desc =
			(struct MultipointDescriptor *)(ih->d.u8+offset+Config.multipoint_desc_len);

		/* for mp block 1, don't be picky about inv */
		if ((which==0) || desc->csum.v==~desc->csum.iv)
		{
			DEBUG_MULTIPOINT("Found descriptor #%d at 0x%x\n", which+1,
				offset);
			Config.multipoint_block_start[which]=offset;
			printf("OK\n");
			return 0;
		}
	}

	printf(which==0?"missing\n":"FAIL\n");
	return -1;
}

static int MP_callback(void *data, struct ReportRecord *rr)
{
	struct ChecksumPair *pcsum, csum;
	struct ImageHandle *ih = data;
	uint32_t nCalcChksum = 0;
	struct RangeList *rl;
	struct Range full={0xffffffff,0};

	/* assume we already corrected v vs iv */
	list_for_each_entry(rl, &rr->data.list, list) {
		if (rl->r.start<full.start) full.start=rl->r.start;
		if (rl->r.end>full.end) full.end=rl->r.end;
		nCalcChksum += CalcChecksumBlk16(ih, &rl->r);
	}

	pcsum=(struct ChecksumPair *)(ih->d.u8+rr->checksum.start);
	memcpy_from_le32(&csum, pcsum, sizeof(csum));
	printf("    <%x>  0x%06X-0x%06X Chk: %08X CalcChk: %08X",
		rr->checksum.start-8, full.start, full.end, csum.v, nCalcChksum);
	if (csum.v != nCalcChksum) {
		ErrorsFound++;
		csum.v = nCalcChksum;
		csum.iv = ~nCalcChksum;
		if (Config.readonly) {
			printf(" ** NOT OK ** (recheck)\n");
		} else {
			memcpy_to_le32(pcsum, &csum, sizeof(csum));
			ErrorsCorrected++;
			printf(" ** FIXED ** (recheck)\n");
		}
	} else {
		printf(" OK (recheck)\n");
	}

	return 0;
}

// Reads the individual checksum blocks that start at nStartBlk
// -2 for ignored bootrom block
// -1 for error
//  0 for no error
//  1 for last block
static int DoChecksumBlk(struct ImageHandle *ih, uint32_t nStartBlk, struct strbuf *buf, int bootrom)
{
	// read the ROM byte by byte to make this code endian independant
	// C16x processors are little endian
	struct MultipointDescriptor desc;
	struct MultipointDescriptor *pDesc;
	uint32_t nCsumAddr, nCalcChksum;
	int errors=0;
	int inside=0;

	sbprintf(buf, "<%05x> ", nStartBlk);

	if(nStartBlk + sizeof(desc) >= ih->len)
	{
		sbprintf(buf, " ERROR! INVALID STARTBLK/LEN 0x%x/%ld ** NOT OK **\n", nStartBlk, (long int)ih->len);
		ErrorsUncorrectable++;
		return -1;	// Uncorrectable Error
	}

	nCsumAddr = nStartBlk + offsetof(struct MultipointDescriptor, csum.v);

	// C16x processors are little endian
	// copy from (le) buffer into our descriptor
	memcpy_from_le32(&desc, ih->d.u8+nStartBlk, sizeof(desc));
	if (!bootrom) {
		if (NormalizeRange(ih, &desc.r))
		{
			ErrorsUncorrectable++;
			return -1;
		}
	}

	/*
	if (Verbose>2) {
		sbprintf(buf, "\n");
		sbprintdesc(buf, &desc);
	}
	*/

	sbprintf(buf, " 0x%06X-0x%06X ", desc.r.start, desc.r.end);

	if(desc.r.start==0xffffffff)
	{
		sbprintf(buf, " END\n");
		return 1;	// end of blks
	}

	sbprintf(buf, "Chk: %08X", desc.csum.v);

	/* test if checksum is inside block, if so, mark it */
	if (nCsumAddr+8 >= desc.r.start && nCsumAddr <= desc.r.end) inside++;

	if (bootrom && ih->bootrom_whitelist) {
		/* whitelisted */
		nCalcChksum = desc.csum.v;
		sbprintf(buf, " Boot: (whitelisted)");
	} else {
		struct ReportRecord *rr = CreateRecord("MP Block", nCsumAddr, sizeof(desc.csum));
		rr->callback = MP_callback;
		rr->cb_data = ih;
		AddRange(rr, &desc.r);
		if (inside && desc.csum.iv != ~desc.csum.v) {
			// if csum inside and iv!=~v, pre-correct iv so v+iv cancels out
			// properly
			uint32_t temp;
			volatile struct ChecksumPair *cs=
				(struct ChecksumPair *)(ih->d.u8+nCsumAddr);
			temp = cs->iv;	 // save inv
			cs->iv = ~cs->v;
			nCalcChksum = CalcChecksumBlk16(ih, &desc.r);
			cs->iv = temp;	// restore inv
		} else {
			nCalcChksum = CalcChecksumBlk16(ih, &desc.r);
		}

		sbprintf(buf, " CalcChk: %08X", nCalcChksum);
		ChecksumsFound ++;

		if (desc.csum.v != nCalcChksum) { errors++; }
	}

	if (desc.csum.iv != ~desc.csum.v) { errors++; }

	if (!errors)
	{
		sbprintf(buf, " OK%s\n", inside?" (i)":"");
		return bootrom?-2:0;
	}

	ErrorsFound+=errors;

	if (Config.readonly)
	{
		sbprintf(buf, " ** NOT OK **\n");
		if (desc.csum.iv != ~desc.csum.v) {
			sbprintf(buf, "%26s%08X!=%08X, ChkInv: %08X ** NOT OK **\n",
				"", desc.csum.v, ~desc.csum.iv, desc.csum.iv);
			if (inside) {
				sbprintf(buf, "*** WARNING! Checksum offset %x inside block 0x%x-0x%x!\n",
					nCsumAddr, desc. r.start, desc.r.end);
			}
		}
		return -1;
	}

	desc.csum.v = nCalcChksum;
	desc.csum.iv = ~nCalcChksum;
	pDesc=(struct MultipointDescriptor *)(ih->d.u8+nStartBlk);
	memcpy_to_le32(&pDesc->csum, &desc.csum, sizeof(desc.csum));

	sbprintf(buf, " ** FIXED **\n");
	ErrorsCorrected+=errors;
	return 0;
}

// vim:ts=4:sw=4:noexpandtab
