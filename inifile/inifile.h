//
// inifile.h
//
// Property files
//
// Copyright (C) 2002 Michael Ringgaard. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//

#ifndef INIFILE_H
#define INIFILE_H

struct property;

struct section
{
    char *name;
    struct section *next;
    struct property *properties;
};

struct property
{
    char *name;
    char *value;
    struct property *next;
};

#ifdef __cplusplus
extern "C" {
#endif

const struct section *find_section(const struct section *sect, const char *name);
int get_section_size(struct section *sect);
const char *find_property(const struct section *sect, const char *name);
const char *get_property(struct section *sections, const char *sectname, const char *propname, const char *defval);
int get_numeric_property(struct section *sections, const char *sectname, const char *propname, int defval);
void free_properties(struct section *sect);
struct section *parse_properties(char *props);
void list_properties(int f, struct section *sect);
struct section *read_properties(const char *filename);
int dump_section_properties(struct section *sections, const char *sectname);

#ifdef __cplusplus
}
#endif

#endif
/* vim: set sw=4 ts=8 expandtab: */
