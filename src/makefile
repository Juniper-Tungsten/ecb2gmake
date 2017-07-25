# $Id: makefile,v 1.24 2014/03/18 22:07:48 sjg Exp $
#
# Yes makefile not Makefile - relying on the fact that make(1)
# looks for this first.
#

MKOBJDIRS = auto
WARNINGS_SET = HIGH
.include <warnings.mk>
W_cast-qual=
#W_error=
XDEFS += ${WARNINGS}
.export XDEFS

DBG=-g -O2
.export DBG

.ifndef host_os
HOST_OS != uname -s
host_os := ${HOST_OS:tl}
.endif
 
.if ${host_os} == "freebsd"
FILEMON ?= ${.CURDIR:H}/jbuild/filemon
.else
FILEMON ?= ${.CURDIR:H}/filemon/${host_os}
.endif
.if exists(${FILEMON}/filemon.h)
CONFIGURE_ARGS += --with-filemon=${FILEMON}
.endif
.undef MAKE_PRINT_VAR_ON_ERROR

# bmake now includes *.mk
MAKESYSPATH:= ${.PARSEDIR:tA}/mk
.export MAKESYSPATH

all:	test

# pickup auto.obj.mk
.include <obj.mk>

MAKE_VERSION != sed -n '/^MAKE_VERSION/{ s,.*= *,,p;q; }' ${.CURDIR}/Makefile


FWTOOLS_PREFIX ?= /volume/fwtools
FWTOOLS_BIN ?= ${FWTOOLS_PREFIX}/bmake/${MAKE_VERSION}/bin

.if make(*home*)
prefix= ${HOME}
.else
prefix= ${FWTOOLS_BIN:H}
.endif

.if exists(config.status)
infiles!= cd ${.CURDIR} && find * -name '*.in'
config:	Makefile ${infiles}
	${.CURDIR}/boot-strap -o . --prefix=${prefix} ${CONFIGURE_ARGS} op=configure
	touch $@

bmake build test: config
.endif

bmake build test clean install:
	${.CURDIR}/boot-strap ${LDSTATIC:DLDSTATIC=${LDSTATIC}} -o . --prefix=${prefix} ${CONFIGURE_ARGS} op=$@

install-home: test
	${.CURDIR}/boot-strap -o . --prefix=${prefix} --install-host-target -DWITH_PROG_VERSION op=install

install-fwtools: test
	${.CURDIR}/boot-strap -o . --prefix=${prefix} --install op=install
