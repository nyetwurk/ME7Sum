#ifndef _RANGE_H
#define _RANGE_H

#include <stdio.h>
#include <stdint.h>

#include "str.h"
#include "list.h"

struct Range {
    uint32_t start;
    uint32_t end;
};

struct RangeList {
    struct list_head list;  /* MSVC doesn't have typeof, so list must always be first */

    struct Range r;
};

struct ReportRecord {
    int index;
    char *name;             /* name of this region */
    struct RangeList data;  /* data ranges that it spans */
    struct Range checksum;  /* range of checksum data */
    struct strbuf msg;      /* dependency messages */
    int dep_errs;           /* number of other checksums found later that are in my data */

    int (*callback)(void *, struct ReportRecord *);
    void *cb_data;  /* pointer passed to callback */
};

struct ReportRecordList {
    struct list_head list; /* MSVC doesn't have typeof, so list must always
 be first */
    struct ReportRecord rr;
};

extern struct ReportRecord *CreateRecord(const char *name, uint32_t start, int len);
extern void AddRange(struct ReportRecord *rr, struct Range *r);
extern void AddRangeStartEnd(struct ReportRecord *rr, uint32_t start, uint32_t end);
extern void AddRangeStartLength(struct ReportRecord *rr, uint32_t start, int len);
extern void PrintRecord(FILE *fh, struct ReportRecord *rr);
extern void PrintAllRecords(FILE *fh);
extern void FreeAllRecords(void);
extern int ProcessRecordDeps(void);

#endif /* ! RANGE_H */
/* vim: set sw=4 ts=8 expandtab: */
