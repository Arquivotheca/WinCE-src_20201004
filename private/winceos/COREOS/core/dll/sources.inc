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
DEFFILE_DEPENDSON=..\core_common.def

COMMON_CDEFINES=-DWINCEMACRO -DCOREDLL -D_WCE_CORECRT -wd4204
COMMON_INCLUDES=..\..\inc;..\..\..\inc;..\..\..\nk\inc;..\..\..\gwe\inc;$(_PRIVATEROOT)\servers\inc
 
NOLTCG=1

ERRORFORBANNEDAPIS=

# WARNLEVEL=4

!IF "$(_TGTCPUFAMILY)" == "ARM"
ADEFINES=$(ADEFINES) -ignore 241
!ENDIF

ASM_SAFESEH=1

COMMON_SOURCES= \
    ..\apis.c     \
    ..\UsrCoredllCallbacks.c \
    ..\UsrGweCallbacks.c \
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
    ..\crtsupp.cpp \
    ..\cetls.cpp \
    ..\dlist.c \
    ..\cerwlock.cpp \
    ..\AssetManagement.cpp \
    ..\ResourceModuleCache.cpp\
    ..\slist_common.c \

ARM_SOURCES= \
    ..\arm\armdsptch.c \
    ..\arm\armpcb.s \
    ..\arm\intrlock.s \
    ..\arm\unwind.c \
    ..\arm\rtlsup.s \
    ..\arm\slist.s \
    ..\muldiv.c \

x86_SOURCES= \
    ..\x86\x86dsptch.c \
    ..\x86\intrlock.c \
    ..\x86\rtlsup.c \
    ..\x86\rtlsupsafeseh.asm \
    ..\x86\muldiv.asm \
    ..\x86\slist.asm \


#xref ModuleMembership coredll.dll
#xref VIGUID {c01aec23-e12b-45f2-94bb-865e927c536f}
#xref VSGUID {4a9f4e8a-90c9-4c7b-8be2-c3acfdf520f8}
