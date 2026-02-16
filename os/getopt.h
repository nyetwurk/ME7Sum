#ifndef _GETOPT_H
#define _GETOPT_H

#include "pgetopt.h"
#define getopt pgetopt
#define opterr popterr /* popterr does the WRONG thing? */
#define optind poptind
#define optarg poptarg
#define optopt poptopt

#endif
/* vim: set sw=4 ts=8 expandtab: */
