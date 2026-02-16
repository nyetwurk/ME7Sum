#ifndef _OS_H
#define _OS_H

#if _WIN32
#include "getopt.h"
#include <stdint.h>
#include <winsock2.h>   /* ntohl() */
#else // ! _WIN32

#ifdef __GNUC__
#include <getopt.h>
#include <arpa/inet.h>  /* ntohl() */
#endif

#endif // ! _WIN32

#endif
/* vim: set sw=4 ts=8 expandtab: */
