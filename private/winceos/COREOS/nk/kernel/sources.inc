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

# KERN_CORE should only be defined when compiling nk\kernel and no where else
WINCECPU=1
TARGETTYPE=LIBRARY
CDEFINES=$(CDEFINES) -DIN_KERNEL -DWINCEMACRO -DKERN_CORE
NOMIPS16CODE=1
NOLTCG=1
NOTHUMB=1
CONDITIONAL_INCLUDES=stdio.h

# Machine independent defines
# disable warning 4204: non-standard C extension
CDEFINES=$(CDEFINES) -DDBGSUPPORT -wd4204
INCLUDES=..\..\inc;..\..\..\inc

!IFDEF DP_SETTINGS
CDEFINES=$(CDEFINES) -DDP_SETTINGS=$(DP_SETTINGS)
!ENDIF

!IF "$(_TGTCPUFAMILY)" == "ARM"
ADEFINES=-pd "$(_TGTCPU) SETL {TRUE}" $(ADEFINES) -ignore 221 -ignore 241

!ELSEIF "$(_TGTCPUFAMILY)" == "x86"

ASM_SAFESEH=1
CDEFINES=$(CDEFINES) -Gs100000

!ENDIF

WARNLEVEL=4

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
    ..\kcalls.c    \
    ..\kdriver.c   \
    ..\kmisc.c     \
    ..\ksysdbg.c   \
    ..\kwin32.c    \
    ..\loader.c    \
    ..\nkglobal.c  \
    ..\nkinit.c    \
    ..\nksnap.c    \
    ..\pgpool.c    \
    ..\physmem.c   \
    ..\ppfs.c      \
    ..\printf.c    \
    ..\process.c   \
    ..\resource.c  \
    ..\ruheap.c    \
    ..\schedule.c  \
    ..\softlog.c   \
    ..\spinlock.c  \
    ..\syncobj.c   \
    ..\time.c      \
    ..\stubs.c     \
    ..\thread.c    \
    ..\vm.c        \
    ..\watchdog.c  \
    ..\profiler_stub.c  \
    ..\sqmutil.c

ARM_SOURCES= \
    ..\arm\mdarm.c      \
    ..\arm\vmarm.c      \
    ..\arm\ExVector.s   \
    ..\arm\armtrap.s    \
    ..\arm\armpcb.s     \
    ..\arm\intrlock.s

x86_SOURCES=    \
    ..\x86\sysinit.c       \
    ..\x86\mdx86.c         \
    ..\x86\fault.c    \
    ..\x86\vmx86.c    \
    ..\x86\splheap.c

