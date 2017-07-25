/*
 * Copyright (c) 2014, Juniper Networks, Inc.
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
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <ecb2gconstants.h>
#include "ecb2gdebug.h"
#include "ecb2gmakeutils.h"

extern char *ecb2g;	       /* path to patched bmake (ECB2G env) */
extern char *ecb2gmake;	       /* path to wrapper */
extern char *ecb2g_retry_notify;       /* path to notify */
extern char *ecb2gcwd;		/* current working dir */
extern char *cmdArgsBuffer;
extern char *ecb2gcwd;

/* we have to make sure we remove any gMake specific settings from MAKEFLAGS
   and remove map/remove any gmake specific arguments as well */
static void
bMakeEnv(char ***argvp, char ***envp, char *fname)
{
    char **val;

    ecb2gDebug(7, "bMakeEnv() called with these args:\n");
    prArgs(7, *argvp);

    rmVal(*argvp, "--no-print-directory", 0, -1);
    rmVal(*argvp, "-j", 1, -1);

    if (val = getVal(*envp, "MAKEFLAGS", -1)) {
	/* clean up MAKEFLAGS which gMake insists in making look like
	   "m -- VAR=VALUE" */
	char *sep = strstr(*val, " -- ");
	if (sep) memmove(*val + 10, sep + 4, strlen(sep + 4) + 1);
	/* make(1) or emake(1) may export our --no-print-directory */
	sep = strstr(*val, "--no-print-directory");
	if (sep) memmove(sep, sep + 20, strlen(sep + 20) + 1);
    }

    /*
     * Re-map the saved bmake variables to their original variables
     * i.e. All _BMAKE_<variable> are moved to <variable>.
     */
    {
	int i = 0;
	char **list = *envp;

	while (1) {
	    for (i = 0; list[i]; i++) {
		if (!strncmp("_BMAKE_", list[i], 7)) {
		    char *name, *equal;
		    int len;
		    /* copy over self */
		    list[i] = strdup(list[i] + 7);
		    len = strlen(list[i]);
		    if (equal = strchr(list[i], '=')) len = equal - list[i];
		    name = strndup(list[i], len);
		    /* remove any others like that , skipping over
		       this one */
		    rmVal(*envp, name, 0, i);
		    ecb2gDebug(7, "Remapped _BMAKE_'%s' is '%s'\n", name, *getVal(*envp, name, -1));
		    free(name);
		    break;
		}
	    }
	    if (!list[i]) break;
	}
    }
    {
	char *val = malloc(strlen(fname) + 30);
	sprintf(val, ECB2G_ENV_FLATFILE"=%s", fname);
	*envp = addVal(*envp, val);
    }
}
static void
notify(char *mfile)
{
    int pid;
    char **new_args;
    extern char **environ;

    if (ecb2g_retry_notify == NULL) {
        ecb2gDebug(0, "no ecb2g_retry_notify defined\n");
        return;
    }
    pid = fork();
    if (pid == 0) {
        char buf[1024];

        new_args[0] = ecb2g_retry_notify;
        setenv("ECB2G_MAKEFILE", mfile, 1);
        ecb2gDebug(0,"  system %s\n", new_args[0]);
        ecb2gDebug(0,"  cwd=%s\n", getcwd(buf, sizeof buf));
        system(ecb2g_retry_notify);

    } else { /* wait for the child process. */
        int status;

        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);

            if (code) {
                ecb2gDebug(0, "ecb2g_retry_notification failed with code %d\n", errno);
	    }
        } else {
	    ecb2gDebug(0, "ecb2g_retry_notification failed with unknown status\n");
        }
    }
}
void
translate(char *fname, int argcount, char **args)
{
    int pid;
    struct stat stats;
    int argc = argcount;
    char **new_args = args; 

    pid = fork();
    if (pid == 0) {
	char buf[1024];
	char *dotmake_exe_flag = 0;
	char *make_exe_flag = 0;

	/* Should probably look for any MAKE=/.MAKE= and replace them */
	dotmake_exe_flag = malloc(strlen(ecb2gmake) + 7); /* Build .MAKE= flag */
	sprintf(dotmake_exe_flag, ".MAKE=%s", ecb2gmake);
	make_exe_flag = malloc(strlen(ecb2gmake) + 6); /* Build MAKE= flag */
	sprintf(make_exe_flag, "MAKE=%s", ecb2gmake);

	setenv(ECB2G_ENV_FLATFILE, fname, 1);
	setenv(ECB2G_ENV_CWD, ecb2gcwd, 1);
	setenv(ECB2G_ENV_CMDARGS, cmdArgsBuffer, 1);
	rmVal((char **)environ, ECB2G_ENV_FLATFILE"=", 0, -1);

	ecb2gDebug(6, "ecb2g ECB2G_FLATFILE=%s\n", fname);

	new_args[0] = ecb2g;
	new_args[argc++] = dotmake_exe_flag; /* end of args */
	new_args[argc++] = make_exe_flag;
	bMakeEnv(&new_args, (char ***) &environ, fname);
	ecb2gDebug(6,"  exec %s\n", new_args[0]);
	ecb2gDebug(6,"  cwd=%s\n", getcwd(buf, sizeof buf));
	prArgs(6, new_args);
	execvpe(ecb2g, new_args, (char**) environ);

	ecb2gDebug(0, "Failed to execvpe '%s' : '%s'\n", ecb2g, strerror(errno));
	exit(errno);
    } else { /* wait for the child process. */
	int status;

	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
	    int code = WEXITSTATUS(status);
	    if (code) {
		ecb2gDebug(0, "ecb2g : error code (%d)\n", code);
		ecb2gDebug(0, "cwd '%s'\n", ecb2gcwd);
		exit(1);
	    }
	} else {
	    ecb2gDebug(0, "ecb2g : error unknown\n");
	    ecb2gDebug(0, "cwd '%s'\n", ecb2gcwd);
	    exit(1);
	}
	if (stat(fname, &stats)) {
	    ecb2gDebug(0, "Flat file '%s' not found - '%s'\n", fname, strerror(errno));
	    ecb2gDebug(0, "makefile '%s'\n", fname);
	    exit(1);
	} 
    }
}
