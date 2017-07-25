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

#define _GNU_SOURCE
#include <features.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <ecb2gconstants.h>
#include "ecb2gdebug.h"
#include "ecb2gmakeutils.h"

char ecb2gMakefile[PATH_MAX];
char ecb2gObjDir[PATH_MAX];
char ecb2gSecObjDir[PATH_MAX];

/*
 * Extract the obj dir
 */
static void
extractObjDir(char *line)
{
    char *p = line + strlen(FLATFILE_OBJDIR_KEY);
    memset(ecb2gObjDir, 0, sizeof ecb2gObjDir);
    /* Avoid the last \n */
    strncpy(ecb2gObjDir, p, strlen(p) - 1);
    if (!isExisting(ecb2gObjDir)) {
	ecb2gDebug(0, "Objdir : %s does not exist\n", ecb2gObjDir);
	exit(1);
    }
}

/*
 * Extract the secondary obj dir
 */
static void
extractSecondaryObjDir(char *line)
{
    char *p = line + strlen(FLATFILE_SECONDARY_OBJDIR_KEY);
    memset(ecb2gSecObjDir, 0, sizeof ecb2gSecObjDir);
    /* Avoid the last \n */
    strncpy(ecb2gSecObjDir, p, strlen(p) - 1);
}

/*
 * Extract the Makefile name
 */
static void
extractMakefile(char *line)
{
    char *p = line + strlen(FLATFILE_MAKEFILE_KEY);
    memset(ecb2gMakefile, 0, sizeof ecb2gMakefile);
    /* Avoid the last \n */
    strncpy(ecb2gMakefile, p, strlen(p) - 1);
    if (!isExisting(ecb2gMakefile)) {
	ecb2gDebug(0, "Makefile: %s does not exist\n", ecb2gMakefile);
	exit(1);
    }
}

/*
 * Parsing the Translated Makefile
 * The keys to look for are predefined and it is assumed to
 * be present in the file
 */
int
parseTranslatedFile(char *fname)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char *objdirpath = NULL;

    fp = fopen(fname, "r");
    if (fp == NULL) {
	ecb2gDebug(0, "Error opening translated makefile  '%s'\n%s\n",\
		   fname, strerror(errno));
	exit(1);
    }
    while ((read = getline(&line, &len, fp)) != -1) {
	if (strstr(line, FLATFILE_OBJDIR_KEY)) {
	    extractObjDir(line);
	}
	if (strstr(line, FLATFILE_MAKEFILE_KEY)) {
	    extractMakefile(line);
	}
	if (strstr(line, FLATFILE_SECONDARY_OBJDIR_KEY)) {
	    extractSecondaryObjDir(line);
	}
	
	if (strlen(ecb2gObjDir) && strlen(ecb2gMakefile) \
	    && strlen(ecb2gSecObjDir))
	    break;
    }

    free(line);
    fclose(fp);
    if (!strlen(ecb2gObjDir)) {
	ecb2gDebug(0, "Parsing Failed to retrive objdir from %s\n", fname);
	exit(1);
    }
    if (!strlen(ecb2gMakefile)) {
	ecb2gDebug(0, "Parsing Failed to retrive makefile from %s\n", fname);
	exit(1);
    }
    ecb2gDebug(2, "Parse: Makefile = %s\n", ecb2gMakefile);
    ecb2gDebug(2, "Parse: ObjDir = %s\n", ecb2gObjDir);
    return 0;
}
