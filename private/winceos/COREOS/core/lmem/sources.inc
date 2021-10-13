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
INCLUDES=..\..\inc;..\..\..\inc;..\..\..\nk\inc
CDEFINES=-DCOREDLL -DWINCEMACRO
NOMIPS16CODE=1
!IFDEF MEMTRACKING
!IF $(MEMTRACKING)
CDEFINES=$(CDEFINES) -DMEMTRACKING
!ENDIF
!ENDIF
!IFDEF MEM_ACCOUNT
CDEFINES=$(CDEFINES) -DMEM_ACCOUNT=1
!ENDIF
!IFDEF FREE_POINTER_AT_LAST_FREE
CDEFINES=$(CDEFINES) -DFREE_POINTER_AT_LAST_FREE
!ENDIF
!IFDEF HEAP_STATISTICS
CDEFINES=$(CDEFINES) -DHEAP_STATISTICS
!ENDIF
!IFDEF HEAP_SENTINEL_EXTRA_FRAMES
CDEFINES=$(CDEFINES) -DHEAP_SENTINEL_EXTRA_FRAMES=$(HEAP_SENTINEL_EXTRA_FRAMES)
!ENDIF

# WARNLEVEL=4

SOURCES=           \
    ..\heap.c   \
    ..\rheap.cpp \
    ..\rmem.c   \
    ..\th32heap.c \
    ..\dllheapfunc.cpp

#xref ModuleMembership coredll.dll
#xref VIGUID {ce49bf32-b9d1-48a2-b56d-7a956a2dca66}
#xref VSGUID {cbae7c6b-bdf6-4d86-963d-4a0073e3a0dc}
