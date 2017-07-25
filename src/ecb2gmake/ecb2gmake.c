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
#include <features.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include "ecb2gdebug.h"
#include "ecb2gmakeutils.h"
#include "ecb2gconstants.h"

char *ecb2g;	       /* path to patched bmake (ECB2G env) */
char *ecb2gmake;	       /* path to wrapper */
char *ecb2g_retry_notify;       /* path to notify */
char *makelevel;
char *gmake;
char *emake;
char *cmdArgsBuffer;		/* Invocation command */
char *ecb2gcwd;

extern char ecb2gObjDir[];
extern char ecb2gSecObjDir[];
extern char ecb2gMakefile[];

#define MAXARGS 1024
#define DEFAULT_ARGS_SIZE 4096 
#define TRANSLATOR "ecb2g"
#define EMAKE "emake"
#define LEVEL_ZERO_MAKEFILE "EMakefile"

int debug = 0;

static char *progname;
static char *translationDir;
static char *prohibitedDir;
/* keep track of the level */
static int ecb2gmakelevel = 0;

/* External Functions */
void buildWithEmake(char **args, int argcount, char *objdir, 
		    char *emakefile, char *runfile);
unsigned int getHash(char **args, char **env);
void translate(char *fname, int argcount, char **args);
int parseTranslatedFile(char *fname);

/*
 * Set the level
 */
static void 
exportEcb2gmakeLevel(int level)
{
    char strLevel[15] = { 0 };
    sprintf(strLevel, "%d", level);
    if (setenv(ECB2G_ENV_MAKELEVEL, strLevel, 1)) {
	ecb2gDebug(0, "Unable to set ECB2GMAKE LEVEL env variable\n");
	exit(1);
    }
}

/*
 * One-time init function
 */
static void 
ecb2gmakeInit(int argc, char *argv[])
{
    char *debugStr;
    char *dotmake;
    char *justmake;
    char ecb2gPath[PATH_MAX] = {0};
    char *ecb2gmakelevelp = NULL;

    /* Store the program name */
    progname = argv[0];
    char *n = strdup(progname);
    char *ecb2gmakedir = dirname(n);
    strcpy(ecb2gPath, ecb2gmakedir);
    strcat(ecb2gPath, "/");
    strcat(ecb2gPath, TRANSLATOR);
    free(n);


    /* Get the variables from environment */
    ecb2gmake = getEnvVar("ECB2GMAKE=", environ, progname);
    ecb2g = getEnvVar("ECB2G=", (char **) environ, ecb2gPath);
    ecb2g_retry_notify = getEnvVar("ECB2G_RETRY_NOTIFY=", environ, NULL);
    emake = getEnvVar("EMAKE=", (char **) environ, EMAKE);
    gmake = getEnvVar("GMAKE=", (char **) environ, "/usr/bin/make");
    makelevel = getEnvVar("MAKELEVEL=", (char **)environ, "0");
    dotmake =  getEnvVar(".MAKE=", (char **)environ, "<unset>");
    justmake =  getEnvVar("MAKE=", (char **)environ, "<unset>");
    translationDir =  getEnvVar("ECB2G_TRANSLATION_DIR=", (char **)environ, NULL); 
    prohibitedDir = getEnvVar("ECB2G_TRANSLATION_PROHIBITED_DIR=", (char **)environ, NULL);

    /* Check if this is the first level instance */
    ecb2gmakelevelp = getenv(ECB2G_ENV_MAKELEVEL);
    if (!ecb2gmakelevelp || !strlen(ecb2gmakelevelp)) {
	/* ecb2gmakelevel initialized to 0 */ 
	exportEcb2gmakeLevel(ecb2gmakelevel);
    } else {
	ecb2gmakelevel = atoi(ecb2gmakelevelp);
	if (!ecb2gmakelevel) {
	    ecb2gDebug(0, "Unable to determine the ecb2gmake level");
	    exit(1);
	}
    }

    debugStr = getenv("ECB2GMAKEDEBUG");
    debug = debugStr ? atoi(debugStr) : 0;
    ecb2gcwd = get_current_dir_name();

    ecb2gDebug(9, "ECB2GMAKE='%s'\n", ecb2gmake);
    ecb2gDebug(9, "EMAKE2GMAKE='%s'\n", ecb2g);
    ecb2gDebug(9, "EMAKE='%s'\n", emake);
    ecb2gDebug(9, "EMAKEFLAGS='%s'\n", getenv("EMAKEFLAGS"));
    ecb2gDebug(9, "MAKEFLAGS='%s'\n", getenv("MAKEFLAGS"));
    ecb2gDebug(9, "GMAKE='%s'\n", gmake);
    ecb2gDebug(9, "BMAKELOCATION='%s'\n", getenv("BMAKELOCATION"));
    ecb2gDebug(9, ".MAKE='%s'\n", dotmake);
    ecb2gDebug(9, "MAKE='%s'\n", justmake);
}

/*
 * Copy the input Arguments to ecb2gmake.
 * It is used to pass it over to child processes
 */
static void 
copyArguments(char **args, char **newargv, int maxargs, int *argcp)
{
    int argc = 0;
    char **argv = args;
    int cmdArgsBufferSize = DEFAULT_ARGS_SIZE;

    cmdArgsBuffer = malloc(cmdArgsBufferSize);
    if (!cmdArgsBuffer) {
	ecb2gDebug(0, "Malloc failure for cmdArgsBuffer\n");
	exit(1);
    }
    memset(cmdArgsBuffer, 0, cmdArgsBufferSize);

    /* Calculate the memory to hold the command */
    while (*argv) {
	if ((argc + 1) >= maxargs) {
	    ecb2gDebug(0, "Maxargs exceeded !\n");
	    exit(1);
	}
	/* Size = strlen + space */
	int newBuffSize = strlen(*argv) + strlen(cmdArgsBuffer) + 1 + 1;
	if (newBuffSize > cmdArgsBufferSize) {
	    /* Reallocate memory */
	    cmdArgsBufferSize += DEFAULT_ARGS_SIZE;
	    if (newBuffSize > cmdArgsBufferSize) {
		ecb2gDebug(0, "Reallocation is not going to help\n");
		exit(1);
	    }
	    cmdArgsBuffer = realloc(cmdArgsBuffer, cmdArgsBufferSize);
	    if (!cmdArgsBuffer) {
		ecb2gDebug(0, "realloc failed for cmdArgsBuffer (size = %d)\n", cmdArgsBufferSize);
		exit(1);
	    }
	}
	    
	newargv[argc] = *argv;
	strcat(cmdArgsBuffer, *argv);
	strcat(cmdArgsBuffer, " ");
	argc++;
	argv++;
    }
    newargv[argc] = NULL;
    *argcp = argc;
    ecb2gDebug(2, "Command copied = %s\n", cmdArgsBuffer);
}

/*
 * main
 * 1. Set ECB2G_FLATFILE in environment
 * 2. Invoke ec2bg to translate
 * 3. Invoke emake to actually build

    We need to map options for emake.
    If any options cannot be mapped (TBD) the exec should fail with that information
    so that we can address that within the original makefile.
*/
int main(int argc, char *argv[])
{
    extern char **environ;

    char **new_args;
    int new_args_count = 0;
    int pid;
    char fname[PATH_MAX] = {0};
    char *const *p = argv;
    unsigned int hash;
    struct stat stats;
    int ecb2g_count;
    char transMakefile[PATH_MAX] = {0};
    char emakefile[PATH_MAX] = {0};
    char emakerunfile[PATH_MAX] = {0};
    char *mfile = NULL;
    /* Makefile destination */
    char *mfileDest = NULL;

    ecb2gmakeInit(argc, argv);

    ecb2gDebug(9, "pid=%d, parent=%d\n", getpid(), getppid());


    /* any sub bmake(1) is a liability. It will trigger a submake that
       will enter "stub mode" on the agent. In stub mode, the emake(1)
       run on the flat file will exit immediately with success (see
       'emake stub mode' , 'emake submakes' for more details).

       Be verbose at level 1 about this.
    */
    ecb2gDebug(1, "BMAKE[%s] - [%s]\n", makelevel, argv[0]);
    ecb2gDebug(1, "  BMAKELOCATION=%s\n", getenv("BMAKELOCATION"));
    ecb2gDebug(1, "  cwd=%s\n", get_current_dir_name());
    ecb2gDebug(2, "  Called with these args: \n");
    prArgs(2, argv);

    /* special case of -V */
    while (*p) {
	if (strcmp(*p, "-V") == 0) {
	    argv[0] = ecb2g;
	    execvpe(ecb2g, argv, environ);
	    ecb2gDebug(0, "Failed to execvpe %s -V : '%s'\n", ecb2g, strerror(errno));
	    exit(errno);
	}
	p++;
    }

    new_args = malloc(MAXARGS * sizeof(*new_args));
    copyArguments((char **)argv, new_args, MAXARGS - 3, &new_args_count);

    sprintf(transMakefile, "%s/tmp.p%08d_t%014llX", \
	    (translationDir ? translationDir : ecb2gcwd), \
	    getpid(), timestamp());
    hash = getHash(new_args, (char **)environ);
    sprintf(fname, "%s.flat_%08X", transMakefile, hash);
    ecb2gDebug(2,"ECB2G_FLATFILE=%s\n", fname);

    /* Do the translation */
    translate(fname, new_args_count, new_args);

    /* Parse the File */
    parseTranslatedFile(fname);

    /* ********* Workaround to fix JUNOS build infra issue ********* 
     * Some targets like clean does not calculate ecb2gObjDir.
     * It is because of some issues in auto.obj.mk within JUNOS repo.
     *
     * Usually the translated file is moved to object directory.
     * But in some cases the destination is within the source directory
     * itself, which is wrong. We have a provision to declare
     * the directory where translated files should not be moved.
     * Apparently, the prohibited directory is src directory. 
     * If the object directory is within the prohibited dir, try
     * with the secondary obj directory
     */
    if (prohibitedDir && strstr(ecb2gObjDir, prohibitedDir)) {
	if (strstr(ecb2gSecObjDir, prohibitedDir)) {
	    ecb2gDebug(0, "Secondary obj dir (%s) is also  prohibited\n",\
		       ecb2gSecObjDir);
	    exit(1);
	}
	/* Treat ecb2gSecObjDir as ecb2gObjDir */
	strcpy(ecb2gObjDir, ecb2gSecObjDir);
    }

    
    /* Makefile name is always LEVEL_ZERO_MAKEFILE
     * for the first Makefile being generated.
     * This helps debugging the build order issues
     */
    if (ecb2gmakelevel) {
	mfile = strdup(ecb2gMakefile);
	strcpy(transMakefile, basename(mfile));
	free(mfile);
    } else {
	strcpy(transMakefile, LEVEL_ZERO_MAKEFILE); 
    }

    mfileDest = ecb2gObjDir;

    /* If the translation dir is set, let the
     * level 0 translated file be kept there.
     * It helps debugging in case of build dependency
     * issues reported in JUNOS SB
     */
    if (translationDir && !ecb2gmakelevel) {
	mfileDest = translationDir;
    }

    /* For level 0 makefile, let us not have any flat or run suffix */
    if (ecb2gmakelevel) {
	sprintf(emakefile, "%s/%s.flat_%08X", mfileDest, transMakefile, hash);
	sprintf(emakerunfile, "%s/%s.run_%08X", mfileDest, transMakefile, hash);
    } else {
	sprintf(emakefile, "%s/%s", mfileDest, transMakefile);
	sprintf(emakerunfile, "%s/%s.run", mfileDest, transMakefile);
    }

    ecb2gDebug(2,"Makefile Name is  = %s\n", transMakefile);
    ecb2gDebug(2,"New Makefile Name will be set to = %s\n", emakefile);
    if (rename(fname, emakefile)) {
	ecb2gDebug(0, "Failed to rename %s to %s\n", fname, emakefile);
	unlink(fname);
	exit(1);
    }

    /* Set the ecb2gmake level */
    exportEcb2gmakeLevel(ecb2gmakelevel + 1);

    /* Invoke the build with emake */
    buildWithEmake(new_args, new_args_count, ecb2gObjDir, \
		   emakefile, emakerunfile);
    ecb2gDebug(2, "ecb2gmake exec success\n");
    _exit(0);
}
