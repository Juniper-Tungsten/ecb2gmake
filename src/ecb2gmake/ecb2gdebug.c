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
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include "ecb2gdebug.h"

extern int  debug;

/*
 * Simple varargs style debug utility.
 */
void
ecb2gDebug(int level,char *fmt, ...)
{
    int fd = fileno(stderr);
    if (level <= debug) {
	va_list ap;
	char msgBuf[4096];
	int pos = 0;
	if (!level || fd >= 0) {
	    int ret, idx=0;

	    va_start(ap, fmt);
	    pos += snprintf(&msgBuf[pos], sizeof msgBuf - pos, "ecb2gmake: ");
	    pos += vsnprintf(&msgBuf[pos], sizeof msgBuf - pos, fmt, ap);
	    va_end(ap);

	    while (idx < pos) {
		ret = write(fd, msgBuf + idx, pos - idx);
		if (ret < 0) {
		    fprintf(stdout, "ecb2gDebug: write() error - %s\n", strerror(errno));
		    break;
		}
		idx += ret;
	    }
	}
    }
}

/* print a NULL terminted list of char * strings */
void
prArgs(int level, char **args)
{
    while (*args) ecb2gDebug(level, "    [%s]\n", *args++);
}

