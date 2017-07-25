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

/*
 * Author: Luc Chouinard <lchouinard@juniper.net>
 *
 * Editor: Karl Klashinsky <klash@juniper.net>
 *
 * This file integrates with the ecb2gmake wrapper mechanism developed
 * for support of the Electric Accelerator (emake) tool.
 *
 * The ecb2gmake wrapper will setup an environment variable
 * (ECB2G_FLATFILE) for the name of the gmake(1) file to
 * write.
 *
 * If ECB2G_FLATFILE is not defined, ecb2g (bmake) will continue to
 * work normally with no change whatsoever to its functionality
 * (unverified).

 * When ECB2G_FLATFILE is defined, we will replace the execution phase
 * of with a dumping of the list of targets, dependencies and
 * commands. The will have been fully resolved with no bmake(1)
 * constructs like variable manupulations.
 *
 * This "flattened" version of the original makefile is then passed to
 * emake by the ecb2gmake wrapper.
 */

#define	_GNU_SOURCE
#include    <sys/types.h>
#include    <sys/stat.h>
#include    <fcntl.h>
#include    <ctype.h>
#include    <errno.h>
#include    <stdio.h>
#include    <stdarg.h>
#include    <unistd.h>
#include    <string.h>
#include    <signal.h>
#include    <sys/socket.h>
#include    <netinet/in.h>
#include    <arpa/inet.h>
#include    <libgen.h>
#include    "make.h"
#include    "dir.h"
#include    "meta.h"
#include    "ecb2g.h"
#include    "ecb2gconstants.h"
#include    "lst.lib/lstInt.h"

static FILE *ecFile = NULL;	/* Where we write expanded gmake content  */
static char *defaultTarget = NULL;
static int ecDebug = 0;
static char *ecIncludeFilename = "emake.inc";
static char makefileDir[PATH_MAX];
#ifdef ECB2G_SPLIT_SANDBOX
static char *bmakeObjroot = NULL; 
static char *splitSbObjroot = NULL;
#endif


/*
 * Return boolean of whether we are in bmake-to-gmake translation mode.
 */
int
ecb2gEnabled(void)
{
    return (ecFile != NULL);
}

/*
 * Simple varargs style debug utility.
 */
static void
ecb2gDebug(int level, char *fmt, ...)
{
    int fd = fileno(stderr);

    if (level <= ecDebug) {
	va_list ap;
	char msgBuf[4096];
	int pos = 0;
	if (!level || fd >= 0) {
	    int ret, idx = 0;

	    va_start(ap, fmt);
	    pos += snprintf(&msgBuf[pos], sizeof msgBuf - pos, "ecb2g: ");
	    pos += vsnprintf(&msgBuf[pos], sizeof msgBuf - pos, fmt, ap);
	    va_end(ap);

	    while (idx < pos) {
		ret = write(fd, msgBuf + idx, pos - idx);
		if (ret < 0) {
		    fprintf(stdout, "ecb2gDebug write() error - %s\n", strerror(errno));
		    break;
		}
		idx += ret;
	    }
	}
    }
}

/*
 * Varargs output to our gmake file
 */
static void
ecb2gPrintf(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(ecFile ? ecFile : stderr, fmt, ap);
    fflush(ecFile ? ecFile : stderr);
    va_end(ap);
}

/*
 * SIGSEGV handler
 */
static void
printException(int sig)
{
    ecb2gDebug(0, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    ecb2gDebug(0, "!! Received exception - SEGV !!!!!!!!!!!!!!!!!!!!");
    ecb2gDebug(0, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    abort();
}

static Hash_Table hTab;

/*
 * Determine if we are wanted (i.e., ECB2G_FLATFILE var exists), and
 * set up for use.
 */
void
ecb2gInit()
{
    char *flatfile = getenv(ECB2G_ENV_FLATFILE);
    char *ecb2gmakeArgs = NULL;
    char *ecb2gmakeCwd = NULL;
    char *inc_filename_env = getenv("ECB2G_INC_FILENAME");

    ecb2gmakeArgs = getenv(ECB2G_ENV_CMDARGS);
    ecb2gmakeCwd = getenv(ECB2G_ENV_CWD);

    if (inc_filename_env != NULL) {
	ecIncludeFilename = inc_filename_env;
    }

#ifdef ECB2G_SPLIT_SANDBOX
    bmakeObjroot = getenv("ECB2G_BMAKE_OBJROOT");
    splitSbObjroot = getenv("ECB2G_SPLITSB_OBJROOT");
#endif

    if (flatfile) {
	char *debugStr = getenv("ECB2GDEBUG");

	signal(SIGSEGV, printException);
	Hash_InitTable(&hTab, 256);
	ecFile = fopen(flatfile, "w");
	if (!ecFile) {
	    fprintf(stderr, "Could not open flatfile '%s' : '%s'\n", flatfile, strerror(errno));
	    exit(1);
	}
	unsetenv(ECB2G_ENV_FLATFILE);
	ecDebug = debugStr ? atoi(debugStr) : 0;
	if (ecb2gmakeCwd) {
	    ecb2gPrintf(FLATFILE_CWD_KEY"%s\n", ecb2gmakeCwd);
	}
	if (ecb2gmakeArgs) {
	    ecb2gPrintf(FLATFILE_CMD_KEY"%s\n", ecb2gmakeArgs);
	}
    }
}

/*
 * NOTE:
 * Because bmake(1) exports environment variables with bmake(1)
 * instax in them, we need to export 2 version of the variables. One
 * version fully processed and ready for consumption by the make(1)
 * and a second to save for the next run of bmake(1). All saved
 * values will be preceeded by BMAKETAG prefix.
*/
#define BMAKETAG "_BMAKE_"
void
ecb2gExportNotify(const char *name, char *val)
{
    int new;

    if (ecFile) {
	Hash_Entry *e = Hash_CreateEntry(&hTab, strdup(name), &new);
	Hash_SetValue(e, strdup(val));
    }
}

#ifdef ECB2G_SPLIT_SANDBOX
/* 
 * Replace string while printing
 */
static void 
printReplaced(const char *input, const char *replacewhat, 
	      const char *replacewith)
{

    const char *p = input;
    char *replace = strstr(p, replacewhat);

    /* Search for all the occurance that needs to be replaced */
    while(replace)
    {
	int len = strlen(p) - strlen(replace);
	if (len < 0) {
	    fprintf(stderr, 
		    "Unexpected condition while replacing %s with %s\n", 
		    replacewhat, replacewith);
	    exit(1);
	}
	ecb2gPrintf("%.*s", len, p);
	ecb2gPrintf("%s", replacewith);
	p = replace + strlen(replacewhat);
	replace = strstr(p, replacewhat);
    }
    ecb2gPrintf("%s", p);
}
#endif

/*
 * Print the env value 
 */
static void
printEnvValue(char *name, char *value)
{
#ifdef ECB2G_SPLIT_SANDBOX
    if (bmakeObjroot && splitSbObjroot && 
	strcmp(name, "ECB2G_TRANSLATION_DIR") && 
	strcmp(name, "ECB2G_SPLITSB_OBJROOT") && 
	strstr(value, splitSbObjroot)) {
	printReplaced(value, splitSbObjroot, bmakeObjroot);
    } else {
	ecb2gPrintf("%s",value);
    }
#else
    ecb2gPrintf("%s", value);
#endif
}

/*
 * Generate an 'export VAR=value' string, where value is fully
 * expanded.
 */
static void
envExport(char *name, char *vali)
{
    char *buf2, *val;
    int new;

    val = Var_Subst(NULL, vali, VAR_GLOBAL, 0);
    if (!val)
	return;
    if (strchr(val, '$')) {
	int i, j;
	int len = strlen(val);
	char *buf = malloc(len * 2);
	/* we also have to re-escape the $ */
	for(i = 0, j = 0; i < len && j < ((len * 2) - 2); i++, j++) {
	    if (val[i] == '$') buf[j++] = '$';
	    buf[j] = val[i];
	}
	buf[j] = '\0';
	ecb2gPrintf("export "BMAKETAG"%s=", name);
	printEnvValue(name, buf);
	ecb2gPrintf("\n");
	free(buf);
    }
    /* simple fully resolved value of the variable for compsumption by gmake(1) */
    ecb2gPrintf("export %s=", name);
    printEnvValue(name, val);
    ecb2gPrintf("\n");
}

/*
 * Generate a comment that contains values for a global var
 */
static void
varComment(char *varName, GNode   *gn)
{
    char *val = NULL, *p;
    while (1) {
	if (gn) {
	    val = Var_Value(varName, gn, &p);
	    if (val) break;
	}
	val = Var_Value(varName, VAR_GLOBAL, &p);
	break;
    }
    ecb2gPrintf("# ECB2GDEBUG VAR %s '%s'\n", varName, val);
    if (p)
	free(p);
}

/*
 * Set the Makefile which is being translated 
 */
void
ecb2gSetMakefile(char *makefile)
{
    char *m = NULL;
    if (!ecFile)
	return;
    m = strdup(makefile);
    strcpy(makefileDir, dirname(m));
    free(m);
    ecb2gPrintf(FLATFILE_MAKEFILE_KEY"%s\n", makefile);
}

/*
 * Set the Makefile which is being translated 
 */
void
ecb2gSetObjDir(char *objdir)
{
    char objpath[MAXPATHLEN] = {0};

    if (!ecFile)
	return;

    /* Make sure we set the absolute path.
     * Let us avoid soft links  or relative path
     * in the absolute path
     */
    if (!realpath(objdir, objpath)) {
	fprintf(stderr, "ecb2g: Failed to find the realpath for %s\n", objdir);
	exit(1);
    }
    ecb2gPrintf(FLATFILE_OBJDIR_KEY"%s\n", objpath);
}

/*
 * This where the core parts of bmake(1) call so that we can print the
 * default target.
 */
void
ecb2gDefault(Lst targs, Lst vpaths)
{
    if (ecFile) {
	static char const *impVars[] = {
	    "AR.ARFLAGS","AS.ASFLAGS","CC.CFLAGS","CXX.CXXFLAGS","CPP.CPPFLAGS","FC","M2C","PC","CO",
	    "GET","LEX","YACC","LINT","MAKEINFO","TEX","TEXI2DVI",
	    "WEAVE","CWEAVE","TANGLE","CTANGLE","RM",
	    "COFLAGS","FFLAGS","GFLAGS","LDFLAGS","LDLIBS","LFLAGS",
	    "YFLAGS","PFLAGS","RFLAGS","LINTFLAGS", NULL,
	};
	if (!Lst_IsEmpty(targs)) {
	    ListNode tln;
	    GNode *gn;
	    tln = Lst_Last(targs);
	    gn = (GNode *)(tln->datum);

	    /* JUNOS objdir calculation is not acurate for some targets
	     * (eg: clean). As a solution we need to check for __objdir and
	     * MAKEOBJDIR. Please note this is applicable only for 
	     * JUNOS build. We need to have this check in place until 
	     * all Juniper Targets do the objdir calculation properly.
	     */
	    char *pobj = NULL;
	    char *objval = NULL;
	    objval = Var_Value("__objdir", gn, &pobj);
	    if (!objval) {
		objval = Var_Value("__objdir", VAR_GLOBAL, &pobj);
		if (!objval) {
		    objval = Var_Value("MAKEOBJDIR", gn, &pobj);
		    if (!objval) {
			objval = Var_Value("MAKEOBJDIR", VAR_GLOBAL, &pobj);
		    }
		}
	    }
	    ecb2gPrintf(FLATFILE_SECONDARY_OBJDIR_KEY"%s\n", \
			(objval ? objval : FLATFILE_NOT_DEFINED_KEY));
	    if (pobj)
		free(pobj);

	    ecb2gPrintf("\n\n");
	    /* Include emake.inc if present in the source direcory. */
	    ecb2gPrintf("-include %s/%s\n\n", makefileDir, ecIncludeFilename);

	    ecb2gPrintf("\n.INTERMEDIATE: .BEGIN .END\n.BEGIN:\n.END:\n");
	    defaultTarget = strdup(gn->path ? gn->path : gn->name);
	    ecb2gPrintf("\n%s:\n\n", defaultTarget);

	    {
		char *p = NULL;
		char *val = Var_Value(".MAKE.EXPORTED", gn, &p);

		if (val) {
		    char *tok = strtok(strdup(val), " \t");
		    while (tok) {
			int new;
			Hash_Entry *e = Hash_CreateEntry(&hTab, tok, &new);
			if (new) {
			    char *p = NULL;
			    Hash_SetValue(e, Var_Value(e->name, VAR_GLOBAL, &p));
			    if (p)
				free(p);
			}
			tok = strtok(NULL, " \t");
		    }
		}
		if (p)
		    free(p);
	    }

	    {
		Hash_Search cursor;
		Hash_Entry *e = Hash_EnumFirst(&hTab, &cursor);
		while (e) {
		    char *val;
		    envExport(e->name, Hash_GetValue(e));
		    e = Hash_EnumNext(&cursor);
		}
	    }

	    /* also export the MAKEOVERRIDES at this time */
	    {
		char *p = NULL;
		char *val = Var_Value(".MAKEOVERRIDES", VAR_GLOBAL, &p);

		if (val) {
		    char *p2 = NULL;
		    char *exname = strtok(val, " ");

		    while (exname) {
			char *val2 = Var_Value(exname, VAR_GLOBAL, &p2);
			ecb2gPrintf("export %s=%s\n", exname, val2);
			if (p2) 
			    free(p2);
			exname = strtok(NULL, " ");
		    }
		}
		if (p) 
		    free(p);
		val = Var_Value(".CURDIR", VAR_GLOBAL, &p);
		if (val) {
		    ecb2gPrintf("export BMAKELOCATION=%s\n", val);
		} else {
		    ecb2gPrintf("export BMAKELOCATION=unknown\n");
		}
		if (p) 
		    free(p);

		varComment("__objdir", gn);
		varComment("__path", gn);
		varComment(".OBJDIR", gn);
		varComment(".PATH", gn);
		varComment("MAKEOBJDIRPREFIX", gn);
		varComment("MAKEOBJDIR", gn);
		varComment(".CURDIR", gn);
		varComment(".PARSEDIR", gn);
		varComment("OBJS", gn);
		varComment("SRCS", gn);
		varComment("CFLAGS", gn);
		varComment("CC", gn);
		varComment("_CC", gn);
		varComment("ISSU_OBJDIR", gn);
		varComment("JKERNEL_OBJDIR", gn);
		varComment("SHARED_ROOTDIR", gn);
		varComment(".MAKE.EXPORTED", gn);

		/* if implicit rules are used, we need to make the implicit rules'
		   variables available */
		{
		    int idx = 0;
		    while (impVars[idx]) {
			/* bmake includes parameters in the CC, CXX program. Gmake expects an executable path.
			   So impVar associates such program vars with their correspoinding FLAGS var and we
			   transfer the variables from the program to the start of the FLAGS */
			char *dot = strchr(impVars[idx], '.');
			if (dot) {
			    char *prog, *flags, *pname;
			    char *moreflags = "";
			    char *p1 = NULL;
			    char *p2 = NULL;

			    pname = strndup(impVars[idx], dot - impVars[idx]);
			    prog = Var_Value(pname, gn, &p1);
			    flags = Var_Value(dot + 1, gn, &p2);
			    if (!flags) flags="";

			    if (prog) {
				prog = Var_Subst(NULL, prog, gn, FALSE);
				/* stump it */
				while (*prog) if (*prog != ' ' && *prog != '\t') break; else prog++;
				moreflags = prog;
				while (*moreflags) if (*moreflags == ' ' || *moreflags++ == '\t') break;
				if (*moreflags) *moreflags++ = '\0';
				ecb2gPrintf("%s=%s\n", pname, Var_Subst(NULL, prog, gn, FALSE));
			    }
			    if (flags) {
				char *allflags;
				flags = Var_Subst(NULL, flags, gn, FALSE);
				allflags = malloc(strlen(flags) + strlen(moreflags) + 2);
				sprintf(allflags, "%s %s", moreflags, flags);
				ecb2gPrintf("%s=%s\n", dot + 1, Var_Subst(NULL, allflags, gn, FALSE));
			    }
			    if (p1)
				free(p1);
			    if (p2)
				free(p2);
			} else {
			    val = Var_Value(impVars[idx], gn, &p);
			    if (!val) val = Var_Value(impVars[idx], VAR_GLOBAL, &p);
			    if (val) {
				ecb2gPrintf("%s=%s\n", impVars[idx], Var_Subst(NULL, val, gn, FALSE));
			    }
			    if (p)
				free(p);
			}
			idx++;
		    }
		}

		val = Var_Value("MAKEFLAGS", VAR_GLOBAL, &p);
		if (val) {
		    ecb2gPrintf("export MAKEFLAGS=%s\n", val);
		    free(p);
		}
		val = Var_Value("SHELL", VAR_GLOBAL, &p);
		if (val) {
		    ecb2gPrintf("SHELL=%s\n", val);
		    free(p);
		}
		{
		    /* unexported make variables that need to be exposed for
		     * dependency update */
		    static const char *depVars[] = {
			"DPADD", "FORCE_DPADD", 
			"MACHINE_ARCH", "META_FILE_FILTER",
			"META_XTRAS", "OBJS",
			"SUPPRESS_DEPEND", "UPDATE_DEPENDFILE", NULL
		    };
		    /* unexported make variables that need to be exposed for
		     * dependency update and need to have dollar signs
		     * escaped */
		    static const char *depdollarVars[] = {
			"GENDIRDEPS_DIR_LIST_XTRAS", 
			"GENDIRDEPS_FILTER", NULL
		    };
		    int idx = 0;
		    char *val2;

		    while (depVars[idx]) {
			val = Var_Value(depVars[idx], VAR_GLOBAL, &p);
			if(val) {
			    val2 = Var_Subst(NULL, val, VAR_GLOBAL, TRUE);
			    ecb2gPrintf("%s=%s\n", depVars[idx], val2);
			}
			if (p)
			    free(p);
			idx++;
		    }
		    idx = 0;
		    while (depdollarVars[idx]) {
			val = Var_Value(depdollarVars[idx], VAR_GLOBAL, &p);
			if(val) {
			    val2 = Var_Subst(NULL, val, VAR_GLOBAL, TRUE);
			    if (strchr(val2, '$')) {
				int i, j;
				int len = strlen(val2);
				char *buf = malloc(len*2);

				for (i=0, j=0; i<len && j<((len*2)-2); i++, j++) {
				    if (val2[i] == '$') buf[j++] = '$';
				    buf[j] = val2[i];
				}
				buf[j] = '\0';
				ecb2gPrintf("%s=%s\n", depdollarVars[idx], buf);
				free(buf);
			    } else {
				ecb2gPrintf("%s=%s\n", depdollarVars[idx], val2);
			    }
			}
			if (p)
			    free(p);
			idx++;
		    }
		};
		/* dependency update variables with dots in their
		   names which need to be removed for gmake. */
		val = Var_Value(".MAKE.DEPENDFILE", VAR_GLOBAL, &p);
		if(val) {
		    ecb2gPrintf("MAKE_DEPENDFILE=%s\n", val);
		    free(p);
		}
		val = Var_Value(".MAKE.MAKEFILE_PREFERENCE", VAR_GLOBAL, &p);
		if(val) {
		    ecb2gPrintf("MAKE_MAKEFILE_PREFERENCE=%s\n", val);
		    free(p);
		}
		/* define and export a newline variable */
		ecb2gPrintf("define newline\n\n\nendef\nexport newline\n");
	    }
	}
    }
}

/*
 * Only one "well known" target for now needs to be renamed for
 * gmake(1) at this point.
 */
static char *
ecb2gMapTargetName(char *name)
{
    static struct {
	char *name;
	char *target;
    } map[] = {
	{ "phony", ".PHONY" },
    };
    int i;

    for (i = 0; i < sizeof(map) /sizeof(map[0]); i++)
	if (!strcmp(map[i].name , name)) return map[i].target;

    return name;
}

/*
 * This callback will be called once for each of the commands associated
 * with a target.
 */
static int
ecb2gTargetCommands(void *cmdp, void *gnp)
{
    if (!ecFile) return 1;	/* If gmake translation not enabled, bail */

    GNode *gn = (GNode *)gnp;
    char *cmd = (char *)cmdp;
    int len, blen, i, j, dollars, newlines, first = 0;
    char *xtra, *buf;

    /* expand it */
    cmd = Var_Subst(NULL, cmd, gn, FALSE);
    len = strlen(cmd);

    /* all command start with a tab at the beginning of the line */
    ecb2gPrintf("\t");

    /* since the final expanded commands would have been fed to a bash and not
       to, yet another, makefile, we need to re-escape the $.
       We also replace line feed characters within the command to
       '"$${newline}"' so that emake will be able to parse the commands
       and not be tripped up by newline characters */
    for (i = 0, dollars = 0; i < len; i++) if (cmd[i] == '$') dollars++;
    for (i = 0, newlines = 0; i < len-1; i++) if (cmd[i] == '\n') newlines++;
    /* strlen of '"$${newline}"' is 15, but we are replacing a character, so
       only need to add 14 chars for each substitution. */
    blen = len + dollars + 14*newlines;
    buf = malloc(blen + 1);
    buf[blen] = '\0';
    for (i = 0, j = 0; i < len; i++) {
	buf[j++] = cmd[i];
	if (cmd[i] == '$') buf[j++] = cmd[i];
	else if ((i<len-1) && (cmd[i] == '\n')) {
	    strcpy(buf+j-1,"\'\"$${newline}\"\'");
	    /* only need to add 14, not 15 because we already did j++ above */
	    j = j+ 14;
	}
    }
    /* bmake(1) does not require continuation characters for
       multiple line commands, so we need to add them. */
    while (first < len) {
	if (buf[first] == '@') {
	    first++;
	    ecb2gPrintf("@");
	} else if (buf[first] == '-') {
	    first++;
	    ecb2gPrintf("-");
	} else if (buf[first] == '+') {
	    first++;
	    ecb2gPrintf("+");
	}
	else break;
    }
    /* we include file:line info if debug is above 4 */
    if (gn->fname) ecb2gPrintf("export BMAKELOCATION=%s:%d; ", gn->fname, gn->lineno);

    for (i = first; i < blen; /* no expr */) {
	j = i + strlen(buf + i);
	xtra =  (j < blen) ? "\\n" : "";
	ecb2gPrintf("%s%s", buf + i, xtra);
	i += strlen(buf + i) + 1;
    }
    ecb2gPrintf("\n");
    free(buf);
    return 0;
}

/*
 * This callback function will be called once for each of the child
 * nodes of a target.
*/
static int
ecb2gDependencies(void *cnp, void *gnp)
{
    GNode *cn = (GNode *)cnp;
    ecb2gPrintf("%s ", cn->path ? cn->path : ecb2gMapTargetName(cn->name));
    return 0;
}

/**********************************************************************
 * This is invoked once it is decided to create meta file for
 * a given target.
 ***********************************************************************/
void
ecb2gMetaWrite(void)
{
    ecb2gPrintf("\t@EC_DPADD=\"$(DPADD)\"\n");
    ecb2gPrintf("\t@EC_META_XTRAS=\"$(META_XTRAS)\"\n");
    ecb2gPrintf("\t@EC_UPDATE=$(UPDATE_DEPENDFILE)\n");
    ecb2gPrintf("\t@FORCE_DPADD=\"$(FORCE_DPADD)\"\n");
    ecb2gPrintf("\t@GENDIRDEPS_DIR_LIST_XTRAS=\"$(GENDIRDEPS_DIR_LIST_XTRAS)\"\n");
    ecb2gPrintf("\t@GENDIRDEPS_FILTER=\"$(GENDIRDEPS_FILTER)\"\n");
    ecb2gPrintf("\t@MAKE_DEPENDFILE=\"$(MAKE_DEPENDFILE)\"\n");
    ecb2gPrintf("\t@MAKE_MAKEFILE_PREFERENCE=\"$(MAKE_MAKEFILE_PREFERENCE)\"\n");
    ecb2gPrintf("\t@META_FILE_FILTER=\"$(META_FILE_FILTER)\"\n");
    ecb2gPrintf("\t@OBJS=\"$(OBJS)\"\n");
    ecb2gPrintf("\t@SUPPRESS_DEPEND=\"$(SUPPRESS_DEPEND)\"\n");
	
}

/**********************************************************************
 *
 * This is the main entry point from bmake. It is called after the
 * makefile had been fully parsed and processed, and just before what
 * would normally be the execution of the commands for all defined
 * targets.
 *
 ***********************************************************************/
listCallback
ecb2gOut(listCallback cb, void *gnp)
{
    if (!ecFile) return cb;	/* If gmake translation not enabled, bail */

    GNode *gn = (GNode *)gnp;
    char *name = gn->path ? gn->path : ecb2gMapTargetName(gn->name);

    /* if this the .END or .BEGIN target, for which the default target
       as a initial and final dependency which we include in the lines
       below, we must check that there are command */
    if (!strcmp(name, ".END") || !strcmp(name, ".BEGIN")) {
	ecb2gPrintf(".INTERMEDIATE:%s\n", name);
    }

    /* do not include targets with no depends and no commands */
    if ((gn->type & (OP_PHONY|OP_SPECIAL|OP_DEPENDS)) ||
	!Lst_IsEmpty(gn->children) || !Lst_IsEmpty(gn->commands)) {

	if (gn->type & OP_PHONY) /* check for forced target */
	    ecb2gPrintf("\n.PHONY: %s", name);

	ecb2gPrintf("\n%s: ", name); /* target itself */

	if (defaultTarget && !strcmp(gn->name, defaultTarget))
	    ecb2gPrintf(".BEGIN "); /* Always depend on INTERMEDIATE .BEGIN target */

	Lst_ForEach(gn->children, ecb2gDependencies, gn); /* Output the dependencies here */

	if (defaultTarget && !strcmp(gn->name, defaultTarget))
	    ecb2gPrintf(".END "); /* Always depend on INTERMEDIATE .END target */

	ecb2gPrintf("\n");
    }
    /*
     * Return our own callback for the commands. ecb2gTargetCommands
     * will be called once for each command line associated with
     * this target
     */
    return ecb2gTargetCommands;
}
