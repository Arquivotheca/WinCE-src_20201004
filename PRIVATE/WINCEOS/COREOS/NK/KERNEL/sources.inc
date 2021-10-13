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
THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
!endif

# KERN_CORE should only be defined when compiling nk\kernel and no where else
WINCECPU=1
TARGETTYPE=LIBRARY
CDEFINES=$(CDEFINES) -DIN_KERNEL -DWINCEMACRO -DKERN_CORE
NOMIPS16CODE=1
NOLTCG=1
CONDITIONAL_INCLUDES=stdio.h

# Machine independent defines
CDEFINES=$(CDEFINES) -DDBGSUPPORT
INCLUDES=..\..\inc;..\..\..\inc

!IFDEF DP_SETTINGS
CDEFINES=$(CDEFINES) -DDP_SETTINGS=$(DP_SETTINGS)
!ENDIF

!IF "$(TGTCPUISANAME)" == "MIPSII"
CDEFINES=$(CDEFINES) -DR4300
ADEFINES=$(ADEFINES) -DR4300
!ENDIF
!IF "$(_TGTCPU)" == "MIPS16"
CDEFINES=$(CDEFINES) -DR4100 -DR4111
ADEFINES=$(ADEFINES) -DR4100 -DR4111
!ENDIF
!IF "$(_TGTCPU)" == "SH4"
ADEFINES=$(ADEFINES) -DSH_CPU=64
!ENDIF
!IF "$(_TGTCPU)" == "SH3"
ADEFINES=$(ADEFINES) -DSH_CPU=48
!ENDIF
!IF "$(_TGTCPU)" == "SH3e"
ADEFINES=$(ADEFINES) -DSH_CPU=62
!ENDIF

!IF "$(_TGTCPUFAMILY)" == "ARM"
ADEFINES=-pd "$(_TGTCPU) SETL {TRUE}" $(ADEFINES) -ignore 221 -ignore 241

!ELSEIF "$(_TGTCPUFAMILY)" == "x86"

CDEFINES=$(CDEFINES) -Gs100000 -DENCODED_GS_COOKIE

!ENDIF

SOURCES= \
    ..\apicall.c   \
    ..\compr2.c    \
    ..\dbgapi.c    \
    ..\debug.c     \
    ..\exdsptch.c  \
    ..\fscall.c    \
    ..\gsnk.c      \
    ..\handle.c    \
    ..\heap.c      \
    ..\intrapi.c   \
    ..\kacl.c      \
    ..\kcalls.c    \
    ..\kdriver.c   \
    ..\kmisc.c     \
    ..\ksysdbg.c   \
    ..\kwin32.c    \
    ..\loader.c    \
    ..\nkglobal.c  \
    ..\nkinit.c    \
    ..\pgpool.c    \
    ..\physmem.c   \
    ..\ppfs.c      \
    ..\printf.c    \
    ..\process.c   \
    ..\resource.c  \
    ..\ruheap.c    \
    ..\schedule.c  \
    ..\softlog.c   \
    ..\syncobj.c   \
    ..\time.c      \
    ..\stubs.c     \
    ..\thread.c    \
    ..\vm.c        \
    ..\watchdog.c  \
    ..\profiler_stub.c  \


