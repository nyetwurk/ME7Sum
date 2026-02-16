#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "range.h"

static LIST_HEAD(Records);

#define MAX_NAME_LENGTH 256

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

// Tests if a newly discovered record checksum location is in an already
// checksummed data.
// This means we found a checksum that, if corrected, will cause a previously
// calculated checksum to be wrong!
static int test_for_data_overlap(const struct ReportRecord *rec, struct list_head *records)
{
    int ret=0;
    const struct Range *csum = &rec->checksum;
    struct ReportRecordList *rrl;
    list_for_each_entry(rrl, records, list) {
        struct ReportRecord *rr = &rrl->rr;
        const struct RangeList *rl;
        if (strncmp(rec->name, rr->name, MAX_NAME_LENGTH)) {
            list_for_each_entry(rl, &rr->data.list, list) {
                const struct Range *data = &rl->r;
                if (MAX(csum->start, data->start)<=MIN(csum->end, data->end)) {
                    sbprintf(&rr->msg, "%d-%s checksum is in %d-%s data (recheck required): (0x%08x-0x%08x) overlaps with (0x%08x-0x%08x): %s\n",
                        rec->index, rec->name, rr->index, rr->name,
                        csum->start, csum->end,
                        data->start, data->end,
                        rec->index > rr->index?"ERROR":"OK");
                    rr->dep_errs ++;
                    ret ++;
                }
            }
        }
    }
    return ret;
}

// Tests if new data range is in an already calculated checksum
// This is expected.
static int test_for_csum_overlap(struct ReportRecord *rec, struct list_head *records, const struct Range *data)
{
    int ret=0;
    struct ReportRecordList *rrl;
    list_for_each_entry(rrl, records, list) {
        struct ReportRecord *rr = &rrl->rr;
        if (strncmp(rec->name, rr->name, MAX_NAME_LENGTH)) {
            const struct Range *csum = &rr->checksum;
            if (MAX(csum->start, data->start)<=MIN(csum->end, data->end)) {
                sbprintf(&rec->msg, "%d-%s checksum is in %d-%s data: (0x%08x-0x%08x) overlaps with (0x%08x-0x%08x): %s\n",
                    rr->index, rr->name, rec->index, rec->name,
                    csum->start, csum->end,
                    data->start, data->end,
                    rr->index > rec->index?"ERROR":"OK");
                ret ++;
                // exit(-1);
            }
        }
    }
    return ret;
}

#if _WIN32
static inline char *strndup( const char *s1, size_t n)
{
    char *copy= (char*)malloc( n+1 );
    memcpy( copy, s1, n );
    copy[n] = 0;
    return copy;
};
#endif

struct ReportRecord *CreateRecord(const char *name, uint32_t start, int len)
{
    struct ReportRecordList *rrl = calloc(1, sizeof(struct ReportRecordList));
    struct ReportRecord *rr = &rrl->rr;
    //fprintf(stderr,"******* CREATE %s *******\n", name);
    rr->name = strndup(name, MAX_NAME_LENGTH);
    INIT_LIST_HEAD(&rr->data.list);
    rr->checksum.start = start;
    rr->checksum.end = start+len-1;

    if (!list_empty(&Records)) {
        struct ReportRecordList *prev =
            list_entry(Records.prev, struct ReportRecordList, list);
        rr->index = prev->rr.index+1;
    } else {
        rr->index = 1;
    }


    // check if this new checksum is in existing data ranges
    test_for_data_overlap(rr, &Records);
    list_add_tail(&rrl->list, &Records);
    return rr;
}

void PrintRecord(FILE *fh, struct ReportRecord *rr)
{
    struct RangeList *rl;

    if (!fh) return;

    fprintf(fh, "0x%08x-0x%08x %s CSUM\n", rr->checksum.start, rr->checksum.end, rr->name);
    if(rr->msg.pbuf) fprintf(fh, "%s", rr->msg.pbuf);
    list_for_each_entry(rl, &rr->data.list, list) {
        fprintf(fh, " 0x%08x-0x%08x\n", rl->r.start, rl->r.end);
    }
    return;
}

void AddRange(struct ReportRecord *rr, struct Range *r)
{
    struct RangeList *rl = calloc(1, sizeof(struct RangeList));
    rl->r = *r;     /* memcpy */

    // check if this new data range is in existing checksum ranges
    test_for_csum_overlap(rr, &Records, &rl->r);

    list_add_tail(&rl->list, &rr->data.list);
}

void AddRangeStartEnd(struct ReportRecord *rr, uint32_t start, uint32_t end)
{
    struct RangeList *rl = calloc(1, sizeof(struct RangeList));
    rl->r.start = start;
    rl->r.end = end;

    // check if this new data range is in existing checksum ranges
    test_for_csum_overlap(rr, &Records, &rl->r);

    list_add_tail(&rl->list, &rr->data.list);
}

void AddRangeStartLength(struct ReportRecord *rr, uint32_t start, int len)
{
    struct RangeList *rl = calloc(1, sizeof(struct RangeList));
    rl->r.start = start;
    rl->r.end = start+len-1;

    // check if this new data range is in existing checksum ranges
    test_for_csum_overlap(rr, &Records, &rl->r);

    list_add_tail(&rl->list, &rr->data.list);
}

static void FreeRecord(struct ReportRecordList *rrl)
{
    struct RangeList *rl, *tmp;
    list_for_each_entry_safe(rl, tmp, &rrl->rr.data.list, list) {
        //struct RangeList *rl = list_entry(e, struct RangeList, list);
        list_del(&rl->list);
        free(rl);
    }
    struct ReportRecord *rr = &rrl->rr;
    if (rr->name) free(rr->name);
    if (rr->msg.pbuf) free(rr->msg.pbuf);
    free(rrl);
}

void PrintAllRecords(FILE *fh)
{
    struct ReportRecordList *rrl;
    list_for_each_entry(rrl, &Records, list) {
        PrintRecord(fh, &rrl->rr);
    }
}

void FreeAllRecords(void)
{
    struct ReportRecordList *rrl, *tmp;
    list_for_each_entry_safe(rrl, tmp, &Records, list) {
        list_del(&rrl->list);
        FreeRecord(rrl);
    }
}

int ProcessRecordDeps(void)
{
    int errs=0;
    struct ReportRecordList *rrl;
    list_for_each_entry(rrl, &Records, list) {
        struct ReportRecord *rr = &rrl->rr;
        if (rr->dep_errs) {
            if (rr->callback) {
                errs += rr->callback(rr->cb_data, rr);
            } else {
                printf("%s: no callback for recheck\n", rr->name);
                errs++;
            }
            printf("*** WARNING! %s\n", rr->msg.pbuf);
        }
    }
    return errs;
}

/* vim: set sw=4 ts=8 expandtab: */
