# $Id: own.mk,v 1.4 2013/08/05 17:20:42 sjg Exp $

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__:

.if !target(__init.mk__)
.include "init.mk"
.endif

.ifndef NOMAKECONF
MAKECONF?=	/etc/mk.conf
.-include "${MAKECONF}"
.endif

.include <host-target.mk>

TARGET_OSNAME?= ${_HOST_OSNAME}
TARGET_OSREL?= ${_HOST_OSREL}
TARGET_OSTYPE?= ${HOST_OSTYPE}
TARGET_HOST?= ${HOST_TARGET}

# these may or may not exist
.-include "${TARGET_HOST}.mk"
.-include "config.mk"

RM?= rm
LN?= ln
INSTALL?= install

prefix?=	/usr
.if exists(${prefix}/lib)
libprefix?=	${prefix}
.else
libprefix?=	/usr
.endif

# FreeBSD at least does not set this
MACHINE_ARCH?=${MACHINE}
# we need to make sure these are defined too in case sys.mk fails to.
COMPILE.s?=	${CC} ${AFLAGS} -c
LINK.s?=	${CC} ${AFLAGS} ${LDFLAGS}
COMPILE.S?=	${CC} ${AFLAGS} ${CPPFLAGS} -c -traditional-cpp
LINK.S?=	${CC} ${AFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.c?=	${CC} ${CFLAGS} ${CPPFLAGS} -c
LINK.c?=	${CC} ${CFLAGS} ${CPPFLAGS} ${LDFLAGS}
CXXFLAGS?=	${CFLAGS}
COMPILE.cc?=	${CXX} ${CXXFLAGS} ${CPPFLAGS} -c
LINK.cc?=	${CXX} ${CXXFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.m?=	${OBJC} ${OBJCFLAGS} ${CPPFLAGS} -c
LINK.m?=	${OBJC} ${OBJCFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.f?=	${FC} ${FFLAGS} -c
LINK.f?=	${FC} ${FFLAGS} ${LDFLAGS}
COMPILE.F?=	${FC} ${FFLAGS} ${CPPFLAGS} -c
LINK.F?=	${FC} ${FFLAGS} ${CPPFLAGS} ${LDFLAGS}
COMPILE.r?=	${FC} ${FFLAGS} ${RFLAGS} -c
LINK.r?=	${FC} ${FFLAGS} ${RFLAGS} ${LDFLAGS}
LEX.l?=		${LEX} ${LFLAGS}
COMPILE.p?=	${PC} ${PFLAGS} ${CPPFLAGS} -c
LINK.p?=	${PC} ${PFLAGS} ${CPPFLAGS} ${LDFLAGS}
YACC.y?=	${YACC} ${YFLAGS}

# for suffix rules
IMPFLAGS?=	${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}}
.for s in .c .cc 
COMPILE.$s += ${IMPFLAGS}
LINK.$s +=  ${IMPFLAGS}
.endfor

PRINT.VAR.MAKE = MAKESYSPATH=${MAKESYSPATH:U${.PARSEDIR}} ${.MAKE}
.if empty(.MAKEFLAGS:M-V*)
.if defined(MAKEOBJDIRPREFIX) || defined(MAKEOBJDIR)
PRINTOBJDIR=	${PRINT.VAR.MAKE} -r -V .OBJDIR -f /dev/null xxx
.else
PRINTOBJDIR=	${PRINT.VAR.MAKE} -V .OBJDIR
.endif
.else
PRINTOBJDIR=	echo # prevent infinite recursion
.endif

# we really like to have SRCTOP and OBJTOP defined...
.if !defined(SRCTOP) || !defined(OBJTOP)
.-include "srctop.mk"
.endif

.if !defined(SRCTOP) || !defined(OBJTOP)
# dpadd.mk is rather pointless without these
OPTIONS_DEFAULT_NO+= DPADD_MK
.endif

# process options
OPTIONS_DEFAULT_NO+= \
	INSTALL_AS_USER \
	GPROF \
	LIBTOOL \
	LINT \
	META_MODE \

OPTIONS_DEFAULT_YES+= \
	ARCHIVE \
	AUTODEP \
	AUTO_OBJ \
	CRYPTO \
	DOC \
	DPADD_MK \
	GDB \
	KERBEROS \
	LINKLIB \
	MAN \
	NLS \
	OBJ \
	PIC \
	SHARE \
	SKEY \
	YP \

OPTIONS_DEFAULT_DEPENDENT+= \
	CATPAGES/MAN \
	OBJDIRS/OBJ \
	PICINSTALL/LINKLIB \
	PICLIB/PIC \
	PROFILE/LINKLIB \

.include <options.mk>

.if ${MK_INSTALL_AS_USER} == "yes"
# We ignore this if user is root.
_uid!=  id -u
.if ${_uid} != 0
.if !defined(USERGRP)
USERGRP!=  id -g
.export USERGRP
.endif
.for x in BIN CONF DOC INFO KMOD LIB MAN NLS SHARE
$xOWN=  ${USER}
$xGRP=  ${USERGRP}
.endfor
.endif
.endif

# override this in sys.mk
ROOT_GROUP?=	wheel
BINGRP?=	${ROOT_GROUP}
BINOWN?=	root
BINMODE?=	555
NONBINMODE?=	444

# Define MANZ to have the man pages compressed (gzip)
#MANZ=		1

MANTARGET?= cat
MANDIR?=	${prefix}/share/man/${MANTARGET}
MANGRP?=	${BINGRP}
MANOWN?=	${BINOWN}
MANMODE?=	${NONBINMODE}

LIBDIR?=	${libprefix}/lib
SHLIBDIR?=	${libprefix}/lib
.if ${USE_SHLIBDIR:Uno} == "yes"
_LIBSODIR?=	${SHLIBDIR}
.else
_LIBSODIR?=	${LIBDIR}
.endif
# this is where ld.*so lives
SHLINKDIR?=	/usr/libexec
LINTLIBDIR?=	${libprefix}/libdata/lint
LIBGRP?=	${BINGRP}
LIBOWN?=	${BINOWN}
LIBMODE?=	${NONBINMODE}

DOCDIR?=        ${prefix}/share/doc
DOCGRP?=	${BINGRP}
DOCOWN?=	${BINOWN}
DOCMODE?=       ${NONBINMODE}

NLSDIR?=	${prefix}/share/nls
NLSGRP?=	${BINGRP}
NLSOWN?=	${BINOWN}
NLSMODE?=	${NONBINMODE}

KMODDIR?=	${prefix}/lkm
KMODGRP?=	${BINGRP}
KMODOWN?=	${BINOWN}
KMODMODE?=	${NONBINMODE}

COPY?=		-c
STRIP_FLAG?=	-s

.if ${TARGET_OSNAME} == "NetBSD"
.if exists(/usr/libexec/ld.elf_so)
OBJECT_FMT=ELF
.endif
OBJECT_FMT?=a.out
.endif
# sys.mk should set something appropriate if need be.
OBJECT_FMT?=ELF

.if (${_HOST_OSNAME} == "FreeBSD")
CFLAGS+= ${CPPFLAGS}
.endif

# allow for per target flags
# apply the :T:R first, so the more specific :T can override if needed
CPPFLAGS += ${CPPFLAGS_${.TARGET:T:R}} ${CPPFLAGS_${.TARGET:T}} 
CFLAGS += ${CFLAGS_${.TARGET:T:R}} ${CFLAGS_${.TARGET:T}} 

# Define SYS_INCLUDE to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
#SYS_INCLUDE= 	symlinks

# don't try to generate PIC versions of libraries on machines
# which don't support PIC.
.if  (${MACHINE_ARCH} == "vax") || \
    ((${MACHINE_ARCH} == "mips") && defined(STATIC_TOOLCHAIN)) || \
    ((${MACHINE_ARCH} == "alpha") && defined(ECOFF_TOOLCHAIN))
MK_PIC=no
.endif

# No lint, for now.
NOLINT=


.if ${MK_LINKLIB} == "no"
MK_PICINSTALL=	no
MK_PROFILE=	no
.endif

.if ${MK_MAN} == "no"
MK_CATPAGES=	no
.endif

.if ${MK_OBJ} == "no"
MK_OBJDIRS=	no
MK_AUTO_OBJ=	no
.endif

.if ${MK_SHARE} == "no"
MK_CATPAGES=	no
MK_DOC=		no
MK_INFO=	no
MK_MAN=		no
MK_NLS=		no
.endif

.endif
