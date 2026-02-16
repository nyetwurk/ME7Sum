#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <unistd.h>
#endif

#include "inifile.h"

static char *trimstr(char *s, char *end)
{
    char *str;
    char *t;
    int ch;
    int i;

    while (end > s)
    {
        if (*(end - 1) == ' ' || *(end - 1) == '\t')
            end--;
        else
            break;
    }
    if (end <= s) return NULL;

    t = str = (char *) malloc(end - s + 1);
    while (s < end)
    {
        if (*s == '^')
        {
            s++;
            ch = 0;
            for (i = 0; i < 2; i++)
            {
                if (s == end) break;

                if (*s >= '0' && *s <= '9')
                    ch = (ch << 4) + *s - '0';
                else if (*s >= 'A' && *s <= 'F')
                    ch = (ch << 4) + *s + 10 - 'A';
                else if (*s >= 'a' && *s <= 'f')
                    ch = (ch << 4) + *s + 10 - 'a';
                else
                    break;

                s++;
            }
            *t++ = ch;
        }
        else
            *t++ = *s++;
    }

    *t = 0;
    return str;
}

const struct section *find_section(const struct section *sect, const char *name)
{
    while (sect)
    {
        if (strcmp(sect->name, name) == 0) return sect;
        sect = sect->next;
    }

    return NULL;
}

int get_section_size(struct section *sect)
{
    struct property *prop;
    int n;

    if (!sect) return 0;
    prop = sect->properties;

    n = 0;
    while (prop)
    {
        n++;
        prop = prop->next;
    }

    return n;
}

const char *find_property(const struct section *sect, const char *name)
{
    struct property *prop;

    if (!sect) return NULL;
    prop = sect->properties;

    while (prop)
    {
        if (strcmp(prop->name, name) == 0) return prop->value ? prop->value : "";
        prop = prop->next;
    }

    return NULL;
}

const char *get_property(struct section *sections, const char *sectname, const char *propname, const char *defval)
{
    const struct section *sect;
    const char *val;

    sect = find_section(sections, sectname);
    // must return a pointer that can be freed!
    if (!sect) return defval;

    val = find_property(sect, propname);
    // must return a pointer that can be freed!
    return val ? val : defval;
}

int get_numeric_property(struct section *sections, const char *sectname, const char *propname, int defval)
{
    const char *val;

    val = get_property(sections, sectname, propname, NULL);
    return val ? atoi(val) : defval;
}

void free_properties(struct section *sect)
{
    struct section *nextsect;
    struct property *prop;
    struct property *nextprop;

/*      printf("\nfree_properties();\n");*/
    while (sect)
    {
        if (sect->name) free(sect->name);

        prop = sect->properties;
        while (prop)
        {
            if (prop->name)  { /*printf(" freed property name=\"%s\"\n",prop->name); */  free(prop->name); }
            // prop->value is allocated with the name
            // if (prop->value) { /*printf(" freed property value=\"%s\"\n",prop->value);*/ free(prop->value); }

            nextprop = prop->next;
            // printf("     freed prop %p\n",prop);
            free(prop);
            prop = nextprop;
        }

        nextsect = sect->next;
        // printf("freed sect %p\n\n",sect);
        free(sect);
        sect = nextsect;
    }
}

struct section *parse_properties(char *props)
{
    struct section *secthead = NULL;
    struct section *sect = NULL;
    struct property *prop = NULL;
    char *p;
    char *end;
    char *split;

    p = props;
    while (*p)
    {
        // Skip white at start of line
        while (*p == ' ' || *p == '\t') p++;

        // Skip comments
        if (*p == '#' || *p == ';')
        {
            while (*p && *p != '\r' && *p != '\n') p++;
            if (*p == '\r') p++;
            if (*p == '\n') p++;
            continue;
        }

        // Skip blank lines
        if (*p == 0 || *p == '\r' || *p == '\n')
        {
            if (*p == '\r') p++;
            if (*p == '\n') p++;
            continue;
        }

        // Check for section or property
        if (*p == '[')
        {
            struct section *newsect;

            p++;
            end = p;
            while (*end && *end != ']') end++;

            newsect = (struct section *) malloc(sizeof(struct section));
            if (!newsect) return NULL;

            newsect->name = trimstr(p, end);
            newsect->next = NULL;
            newsect->properties = NULL;
            if (!secthead) secthead = newsect;
            if (sect) sect->next = newsect;
            sect = newsect;
            prop = NULL;

            p = end;
            if (*p == ']') p++;
        }
        else
        {
            struct property *newprop;

            end = p;
            split = NULL;
            while (*end && *end != '\r' && *end != '\n')
            {
        if (!split && (*end == '=' || *end == ':')) split = end;
        end++;
            }

            if (sect)
            {
                newprop = (struct property *) malloc(sizeof(struct property));
                if (!newprop) return NULL;

                if (split)
                {
                    newprop->name = trimstr(p, split);
                    split++;
                    while (*split == ' ' || *split == '\t') split++;
                    newprop->value = trimstr(split, end);
                }
                else
                {
                    newprop->name = trimstr(p, end);
                    newprop->value = NULL;
                }

                newprop->next = NULL;
                if (prop)
                    prop->next = newprop;
                else
                    sect->properties = newprop;

                prop = newprop;
            }

            p = end;
            if (*p == '\r') p++;
            if (*p == '\n') p++;
        }
    }

    return secthead;
}

int dump_section_properties(struct section *sections, const char *sectname)
{
    const struct section *sect;
    struct property *prop;

    sect = find_section(sections, sectname);
    if (!sect) return -1;

    prop = sect->properties;
    while (prop)
    {
        printf("%s\n",prop->name);
        prop = prop->next;
    }
    return 0;
}

static ssize_t safe_write(int fd, const void *buf, size_t count)
{
    ssize_t ret = write(fd, buf, count);
    if (ret<0) {
        perror("write");
        exit(1);
    }
    return ret;
}

void list_properties(int f, struct section *sect)
{
    struct property *prop;

    while (sect)
    {
        safe_write(f, "[", 1);
        safe_write(f, sect->name, strlen(sect->name));
        safe_write(f, "]\r\n", 3);

        prop = sect->properties;
        while (prop)
        {
            safe_write(f, prop->name, strlen(prop->name));
            if (prop->value)
            {
                safe_write(f, "=", 1);
                safe_write(f, prop->value, strlen(prop->value));
            }
            safe_write(f, "\r\n", 2);
            prop = prop->next;
        }

        if (sect->next) safe_write(f, "\r\n", 2);
        sect = sect->next;
    }
}

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

struct section *read_properties(const char *filename)
{
    FILE *f;
    int size, siz;
    int result;
    char *props;
    struct section *sect;
    struct stat buf;

    f = fopen(filename, "rb");
    if (!f) {
        printf("failed to open %s\n",filename);
        return NULL;
    } else {
    // printf("success open() on %s\n",filename);
    }

    result = stat( filename, &buf );
    if( result != 0 )
    {
        // bad
        printf("failed to stat() %s", filename);
        size = 0;
    } else {
        size = (int) buf.st_size;
    // printf("size is %ld\n",size);
    }

    props = (char *) malloc(size + 1);
    if (!props)
    {
        printf("failed to alloc().. ");
        fclose(f);
        return NULL;
    }

    // printf("read()..\n");
    siz = fread(props, 1,size, f);
#if 1
    if (siz != size)
    {
        printf("read() siz=%d == size=%d , failed\n", siz, size);
        free(props);
        fclose(f);
        return NULL;
    } else {
    // printf("read(%s) OK!\n",props);
    }
#endif

    fclose(f);

    props[size] = 0;

    // printf("parse()..\n");

    sect = parse_properties(props);
    // printf("parse done()..\n");

    free(props);

    return sect;
}

#if 0
int main(int argc, char **argv)
{
    struct section *osconfig;
    char *str;
    char *host;
    osconfig = read_properties("os.ini");
    if(osconfig != 0)
    {
        str = get_property(osconfig, "os", "user", NULL);
        host = get_property(osconfig, "os", "libpath", NULL);

        printf("os init=%s\n",str);
        printf("os hostname=%s\n",host);

        free_properties(osconfig);
    } else {
        printf("failed to read_properties\n");
    }
}
#endif

// vim:ts=4:sw=4
/* vim: set sw=4 ts=8 expandtab: */
