# $Id: final.mk,v 1.3 2013/01/30 20:26:58 sjg Exp $

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__:

# provide a hook for folk who want to do scary stuff
.-include "${.CURDIR}/../Makefile-final.inc"

.if !empty(STAGE)
.-include <stage.mk>
.endif

.-include <local.final.mk>
.endif
