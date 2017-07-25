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

#ifndef _ecb2gconstants_h_
#define _ecb2gconstants_h_

/* Input to ecb2g are via environment variables */
#define ECB2G_ENV_FLATFILE "ECB2G_FLATFILE"
#define ECB2G_ENV_CWD "ECB2G_CURRENTDIR" 
#define ECB2G_ENV_CMDARGS "ECB2G_CMDARGS" 
#define ECB2G_ENV_MAKELEVEL "_ECB2GMAKE_LEVEL_"

/* Output from ecb2g are via flat file entries */
#define FLATFILE_OBJDIR_KEY "# ECB2G_OUT OBJDIR = "
#define FLATFILE_SECONDARY_OBJDIR_KEY "# ECB2G_OUT SECONDARY OBJDIR = "
#define FLATFILE_MAKEFILE_KEY "# ECB2G_OUT MAKEFILE = "
#define FLATFILE_CMD_KEY "# ECB2G_OUT CMD = "
#define FLATFILE_CWD_KEY "# ECB2G_OUT CWD = "
#define FLATFILE_NOT_DEFINED_KEY "<Not Defined>"

#endif
