!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
Use of this source code is subject to the terms of the Microsoft
premium shared source license agreement under which you licensed
this source code. If you did not accept the terms of the license
agreement, you are not authorized to use this source code.
For the terms of the license, please see the license agreement
signed by you and Microsoft.
THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
!endif
# ddraw.mk - General !include file for sources in the ddraw tree

!if "$(DDRAW_TST_ROOT)"==""
!error Must Define DDRAW_TST_ROOT
!endif

SHARED_TST_ROOT=$(TEST_ROOT)

# Turn on warning level 4 and disable benign warnings
# CDEFINES=/W4 /FI$(SHARED_TST_ROOT)\inc\warning.h
CDEFINES=\
    $(CDEFINES) \
    /MT /U_MT\
    /EHac

INCLUDES=\
    $(DX_TST_ROOT)\inc;                  \
    $(DDRAW_TST_ROOT)\inc;               \
    $(SHARED_TST_ROOT)\Multimedia\common\inc \
    
!if "$(_TGTOS)"=="NT" || "$(_TGTOS)"=="NTANSI"

!  if "$(_DESKTOP_DX_SDK_ROOT)"==""
!    error Must have _DESKTOP_DX_SDK_ROOT set
!  endif

INCLUDES=\
    $(INCLUDES);                        \
    $(_DESKTOP_DX_SDK_ROOT)\include     \

DX_LIB_DIR=$(_DESKTOP_DX_SDK_ROOT)\lib

!if ("$(WINCEDEBUG)" == "retail")
PRTLIB=msvcprt.lib
!else
PRTLIB=msvcprtd.lib
!endif

TARGETLIBS=\
    $(TARGETLIBS)                       \
    $(OS_SDK_LIB_PATH)\uuid.lib            \
    $(OS_SDK_LIB_PATH)\$(PRTLIB)            \

!else

TARGETLIBS=\
    $(_COREDLL)           \
    $(_UUID)             \

!endif

TARGETLIBS=\
    $(TARGETLIBS)                       \
    $(_DDGUID)

