!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
Use of this source code is subject to the terms of the Microsoft shared
source or premium shared source license agreement under which you licensed
this source code. If you did not accept the terms of the license agreement,
you are not authorized to use this source code. For the terms of the license,
please see the license agreement between you and Microsoft or, if applicable,
see the SOURCE.RTF on your install media or the root of your tools installation.
THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
!endif

TARGETTYPE=LIBRARY
PREPROCESSDEFFILE=1
DEFFILE_DEPENDSON=..\core_common.def ..\crt\corelib1.def ..\..\..\inc\crt_ordinals.h ..\..\fpcrt\fpcrt_ordinals.h 

COMMON_CDEFINES=-DWINCEMACRO -DCOREDLL -wd4204
COMMON_INCLUDES=..\..\inc;..\..\..\inc;..\..\..\nk\inc;..\..\fpcrt;..\..\buildflb;..\..\..\gwe\inc;$(_PRIVATEROOT)\servers\inc
 
NOMIPS16CODE=1
NOLTCG=1

ERRORFORBANNEDAPIS=

# WARNLEVEL=4

CONDITIONAL_INCLUDES=shx\shx.h

!IF "$(_TGTCPU)" == "SH4"
ADEFINES=$(ADEFINES) -DSH_CPU=64
!ENDIF

!IF "$(_TGTCPUFAMILY)" == "ARM"
ADEFINES=$(ADEFINES) -ignore 241
!ENDIF

CFPUDEFINES=-DMIPS_HAS_FPU

ASM_SAFESEH=1

SOURCELIBS= \
    $(_COMMONOAKROOT)\lib\$(_CPUINDPATH)\ccrtsecd.lib \

COMMON_SOURCES= \
    ..\apis.c     \
    ..\UsrCoredllCallbacks.c \
    ..\UsrGweCallbacks.c \
    ..\UsrRenderCoreCallbacks.c \
    ..\UsrAygshellCallbacks.c \
    ..\coredll.cpp  \
    ..\time.c     \
    ..\random.c   \
    ..\strsafea.c \
    ..\strsafea_sprintf.c \
    ..\strsafew.c \
    ..\username.cpp \
    ..\CePtrCoredll.cpp \
    ..\cnnclpth.c \
    ..\exdsptch.c \
    ..\resource.cpp \
    ..\loader.cpp \
    ..\AppVerifier.cpp \
    ..\wsprintf.c \
    ..\IMELoader.cpp \
    ..\gscore.cpp \
    ..\gscore2.cpp \
    ..\crtsupp.cpp \
    ..\cetls.cpp \
    ..\dlist.c \
    ..\cerwlock.cpp \
    ..\AssetManagement.cpp \
    ..\ResourceModuleCache.cpp\

ARM_SOURCES= \
    ..\arm\armpcb.s \
    ..\arm\intrlock.s \
    ..\arm\unwind.cpp \
    ..\arm\rtlsup.s \
    ..\muldiv.c \

MIPS_SOURCES= \
    ..\mips\intrlock.s \
    ..\mips\mipsuwnd.c \
    ..\mips\rtlsup.s \
    ..\muldiv.c \

SH_SOURCES= \
    ..\sh\intrlock.src \
    ..\sh\shunwind.c \
    ..\sh\rtlsup.src \
    ..\muldiv.c \

x86_SOURCES= \
    ..\x86\intrlock.c \
    ..\x86\rtlsup.c \
    ..\x86\rtlsupsafeseh.asm \
    ..\x86\muldiv.asm \


#xref ModuleMembership coredll.dll
#xref VIGUID {c01aec23-e12b-45f2-94bb-865e927c536f}
#xref VSGUID {4a9f4e8a-90c9-4c7b-8be2-c3acfdf520f8}
