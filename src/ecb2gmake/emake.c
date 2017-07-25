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
#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include "ecb2gdebug.h"
#include "ecb2gmakeutils.h"

extern char *gmake;
extern char *emake;
extern int  debug;

/* Debug utility mkRunFile */
static void
mkRunFile(char *fname, char **args, char **env)
{
    FILE *f = fopen(fname, "w");

    while (*env) {
	char *equal = strchr(*env, '=');
	char *val;
	if (!equal) val = "";
	else {
	    *equal = '\0';
	    val = equal + 1;
	}
	/* skip any . bmake variables */
	if (**env != '.' && strcmp(*env, "LD_PRELOAD")) fprintf(f, "export %s='%s'\n", *env, val);
	if (equal) *equal = '=';
	env++;
    }
    fprintf(f, "cd %s\n$MAKE ", get_current_dir_name());
    ++args;
    while (*args) fprintf(f,  "%s ", *args++);
    fprintf(f, "\n");
    fclose(f);
}

/* we have to make sure we remove any bMake specific settings from MAKEFLAGS
   and remove map/remove any bmake specific arguments as well */
static void
gMakeEnv(char ***argvp, char ***envp)
{
    char **val;

    ecb2gDebug(4, "gMakeEnv() called with these args:\n");
    prArgs(4, *argvp);

    /* NOTE: Junos uses output of make commands as input for other
       commands - Make must remain completely quiet */
    /* *argvp = addVal(*argvp, "--no-print-directory"); */
    *argvp = addVal(*argvp, "-s");
    rmVal(*argvp, "-B", 0, -1);
    rmVal(*argvp, "-D", 1, -1);	/* For -DSOMEVAR, we should be smarter about this */
    if (val = getVal(*envp, "MAKEFLAGS", -1)) {
	ecb2gDebug(4, "gmake MAKEFLAGS=\"%s\"\n", *val);
    }
}

/*
 * Remove certain options: -C, -f, and --file=
 */
static void
cleanGmakeArgs(char **argv, int *argcp)
{
    int i, j;

    for (i = 0, j = 0; j < *argcp; /* no incr */) {
	char *arg = argv[j];
	int len = strlen(arg);

	if (len >= 2) {
	    if (arg[0] == '-') {
		if (arg[1] == 'f') {
		    if (len > 2) j += 1;
		    else j += 2;
		    continue;
		} else if (arg[1] == '-' && len > 7 && !strncmp(arg+2, "file=", 5)) {
		    j += 1;
		    continue;
		} else if (arg[1] == 'C') {
		    if (len > 2) j += 1;
		    else j += 2;
		    continue;
		}
	    }
	}
	argv[i++] = argv[j++];
    }
    *argcp = i;
}

/*
 * Invoke emake to do the build
 * emakefile is the Makefile 
 */
void 
buildWithEmake(char **args, int argcount, char *objdir, 
	       char *emakefile, char *runfile) 
{
    int pid, status;
    char *make = emake ? emake : gmake;
    char **new_args = args;
    int argc = argcount;
    new_args[0] = make;

    if (make != emake) ecb2gDebug(0, "warning: no emake, using make=%s\n", make);

    pid = fork();
    if (pid == 0) {
	if (chdir(objdir)) {
	    ecb2gDebug(0, "Failed to chdir to  %s \n", objdir);
	    exit(1);
	}
	cleanGmakeArgs(new_args, &argc);
	new_args[argc++] = "-f";
	new_args[argc++] = emakefile;
	new_args[argc++] = "--no-builtin-rules";
	{
	    /* get the srcdir so we can pass -I <srcdir> */
	    char *objtop=getenv("OBJTOP");
	    char *host_objtop=getenv("HOST_OBJTOP");
	    char *cwd=get_current_dir_name();
	    char *srctop=getenv("SRCTOP");
	    char *match_objtop=NULL;

	    if ((objtop != NULL) && (host_objtop != NULL)
		&& (cwd != NULL) && (srctop != NULL)) {
		if (strstr(cwd, objtop) != NULL) {
		    match_objtop = objtop;
		} else if (strstr(cwd, host_objtop) != NULL) {
		    match_objtop = host_objtop;
		}
		if (match_objtop != NULL) {
		    char *srcdir = (char *)malloc(PATH_MAX);
		    int mo_len=strlen(match_objtop);

		    srcdir = strncpy(srcdir, srctop, mo_len);
		    ecb2gDebug(5, "cwd[mo_len]:%c\n", cwd[mo_len]); 
		    ecb2gDebug(5, "strlen(match_objtop):%d\n", mo_len); 
		    ecb2gDebug(5, "cwd[mo_len]:%s\n", &(cwd[mo_len])); 
		    srcdir = strncat(srcdir, cwd+mo_len, strlen(cwd)-mo_len);
		    ecb2gDebug(5, "constructed srcdir: %s\n", srcdir);
		    new_args[argc++] = "-I";
		    new_args[argc++] = srcdir;
		} else {
		    ecb2gDebug(5, "constructing srcdir failed: no match\n");
		}
	    } else {
		ecb2gDebug(5, "constructing srcdir failed: incomplete environment\n");
	    }
	}
	new_args[argc++] = 0;
	gMakeEnv((char***)&new_args, (char ***)&environ);
	ecb2gDebug(3, "exec make=%s\n", make);
	ecb2gDebug(3, "  cwd=%s\n", get_current_dir_name());
	prArgs(3, new_args);

	/* Generate the run file if needed */
	if (debug >= 9) mkRunFile(runfile, new_args, (char **)environ);

	execvpe(make, new_args, environ);
	/* How come execvpe failed?
	 * It could be because it could not locate emake.
	 * Provide a useful message so that user can set the specify the location
	 * in PATH variable
	 */
	if (ENOENT == errno) {
	    fprintf(stderr, "Unable to locate %s. $PATH updated?\n", make);
	} else {
	    ecb2gDebug(0, "Failed to execvpe %s (%s)\n", make, strerror(errno));
	}
	exit(errno);
    }
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
	int code = WEXITSTATUS(status);
	if (code) {
	    if (ENOENT != code) {
		ecb2gDebug(0, "exec %s failed with code %d\n", make, code);
		ecb2gDebug(0, "make: *** file [%s]\n", emakefile);
	    }
	    _exit(code);
	}
    } else {
	ecb2gDebug(0, "exec %S failed with unknown status\n", make);
	ecb2gDebug(0, "make: *** file [%s]\n", emakefile);
	_exit(1);
    }
}
