#ifndef INIFILE_PROP_H
#define INIFILE_PROP_H

#include <stdint.h>

#include "inifile/inifile.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define END_LIST        -1
#define GET_VALUE        1
#define GET_STRING       2


typedef struct PropertyListItem {
    int attr_type;
    uint32_t *attr_adr;
    const char *attr_path;
    const char *attr_name;
    const char *attr_default;
} PropertyListItem;

typedef struct InfoItem {
    uint32_t        off;
    uint32_t        len;
} InfoItem;

typedef struct InfoListItem {
    const char *label;
    int attr_type;
    InfoItem *item;
    const char *attr_path;
    const char *attr_name;
    const char *attr_default;
    const char *attr_default_len;
} InfoListItem;

uint32_t get_property_value(struct section *sections, const char *sectname, const char *propname, const char *def);
int process_properties_list(struct section *osconfig, PropertyListItem *ci);
int process_info_list(struct section *osconfig, InfoListItem *ci);

#ifdef  __cplusplus
}
#endif

#endif

/* vim: set sw=4 ts=8 expandtab: */
