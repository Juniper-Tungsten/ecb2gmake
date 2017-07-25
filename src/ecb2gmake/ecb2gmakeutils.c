/*
 * Copyright (c) 2015, Juniper Networks, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "ecb2gmakeutils.h"

/*
 * ARGV and ENVIRON management functions
 */
char **
getVal(char **list, char *var, int jmp)
{
    int i, len = strlen(var);
    for (i = 0; list[i]; i++) {
	if (!strncmp(var, list[i], len)) {
	    if ( i != jmp) return &list[i];
	}
    }
    return NULL;
}

/*
 * Used to remove some environment variables from the environment
 * array instead of environ[]. The first exec out of the ec agent
 * needs that
 */
char *
getEnvVar(char *name, char **list, char* def)
{
    char **val = getVal(list, name, -1);
    return val ? strdup((*val) + strlen(name)) : def;
}

/* Add a value to a list, possibly extending */
char **
addVal(char **list, char *val)
{
    int i;
    char **new;

    if (!getVal(list, val, -1)) { /* we add 10 more slots */

	for (i = 0; list[i]; i++) /* no-op */ ;

	new = (char **)calloc( (i + 10) * sizeof(*list), 1);

	/* copy all existing slots */
	for (i = 0 ; list[i]; i++) new[i] = list[i];

	new[i] = strdup(val);
	return new;
    }
    return list;
}

/* Remove an entry from a list*/
void
rmVal(char **list, char *var, int two, int jmp)
{
    char **pos;

    while (pos = getVal(list, var, jmp)) {
	int i, inc, loc = (int)(pos - list);
	char *opt = pos[0];
	/* in the case where we remove an option like '-C dir'
	   we need to check if the there is a space between the option and value.
	   This assumes that options and values are close to each other and that
	   the getopt() feature where options can be posed in groups followed
	   by the coresponding value gourp (ordered the same way as the options)
	   is not being used.
	*/
	inc = (two && opt[0] == '-' && strlen(opt) < 3) ? 2 : 1;

	for (i = inc; pos[ i - inc]; i++) pos[i - inc] = pos[i];
	if (loc <= jmp) jmp -= inc;
    }
}

/* Utility function to check existence of a file or dir */
int 
isExisting(char *dir) 
{
    struct stat stats;
    return !(stat(dir, &stats));
}

/* Get the current time in milliseconds */
long long 
timestamp()
{
    struct timeval t;

    gettimeofday(&t, NULL);
    return (t.tv_sec*1000LL + t.tv_usec/1000);
}

