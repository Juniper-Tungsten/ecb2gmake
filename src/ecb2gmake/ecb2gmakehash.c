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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "ecb2gdebug.h"
#include "ecb2gmakeutils.h"


#ifdef ECB2G_HASHTAG
typedef struct hstr_s {
    struct hstr_s * next;
    char *str;
} hstr_t;

typedef struct ctx {
    unsigned int b;
    unsigned int a;
    unsigned int hash;
    hstr_t *hs;
} ctx_t;

static
void hash_init(ctx_t *c)
{
    c->b    = 378551;
    c->a    = 63689;
    c->hs   = NULL;
    c->hash = 0;
}

static
void hash (ctx_t *c, char* stri, unsigned int len)
{
    unsigned int b    = c->b;
    unsigned int a    = c->a;
    unsigned int hash = c->hash;
    unsigned int i    = 0;
    char *str = stri;
    hstr_t *hs = NULL;

    for (i = 0; i < len; str++, i++) {
	hash = hash * a + (*str);
	a = a * b;
    }

    hs = malloc(sizeof *hs);
    hs->str = strndup(stri, len);
    hs->next = c->hs;
    c->hs = hs;
    c->hash = hash;
}

/*
 * Compute a unique hash value based on context (i.e., environ vars)
 */
unsigned int
getHash(char **args, char **env)
{
    /* list all the environment variables that should be part of this context */
    static char *envhash[]={
	"AR=",
	"ARFLAGS=",
	"AS=",
	"ASFLAGS=",
	"BMAKE=",
	"CC=",
	"CFLAGS=",
	"CPP=",
	"CPPFLAGS=",
	"CXX=",
	"CXXFLAGS=",
	"ECB2GMAKE=",
	"ECB2G=",
	"EMAKE=",
	"FC=",
	"FFLAGS=",
	"FWTOOLS_PREFIX=",
	"GMAKE=",
	"HABTOOLS=",
	"HABTOOLS_PREFIX=",
	"HOST_ARCH=",
	"HOST_MACHINE=",
	"HOST_MACHINE_ARCH=",
	"HOST_OBJTOP=",
	"HOST_OS=",
	"HOST_OS_ID=",
	"HOST_TARGET=",
	"JOB_MAX=",
	"LDFLAGS =",
	"LEX=",
	"LFLAGS=",
	"LINT=",
	"LINTFLAGS=",
	"LOCKF=",
	"MACHINE=",
	"MACHINE_ARCH=",
	"MAKEFLAGS=",
	"MAKELEVEL=",
	"OBJDIR=",
	"__objdir=",
	"PC=",
	"PFLAGS=",
	"REQUESTED_MACHINE=",
	"RFLAGS=",
	"RM=",
	"SIGS=",
	"TARGET_SPEC=",
	"TZ=",
	"UPDATE_DEPENDFILE=",
	"USE_CCACHE=",
	"YACC=",
	"YFLAGS=",
	NULL
    };
    char *pwd = get_current_dir_name();
    char **evar;
    ctx_t c;

    hash_init(&c);
    hash(&c, pwd, strlen(pwd));
    while (*args) {
	hash(&c, *args, strlen(*args));
	args++;
    }

    for (evar = envhash; *evar; evar++) {
	char **val;
	if ( val = getVal(env, *evar, -1) ) hash(&c, *val, strlen(*val));
    }
    return c.hash;
}
#else
unsigned int
getHash(char **args, char **env)
{
    unsigned int rval;
    /* get a random uniq value */
    int fd = open("/dev/urandom", O_RDONLY);

    if (read(fd, &rval, sizeof rval) != sizeof rval) {
	ecb2gDebug(0, "Failed 4 bytes read from urandom! - using pid\n");
	rval = getpid();
    }
    return rval;
}

#endif

