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
# func.mk - General !include file for sources in the functional tree
!if "$(DDRAW_TST_ROOT)"==""
DDRAW_TST_ROOT=..\..
!endif

!include $(DDRAW_TST_ROOT)\ddraw.mk

TARGETLIBS=$(TARGETLIBS)                                        \
    $(TEST_OAK_LIB_PATH)\qalib.lib             \
    $(TEST_OAK_LIB_PATH)\syslib.lib            \
    $(TEST_OAK_LIB_PATH)\hqahelp.lib           \
    $(TEST_OAK_LIB_PATH)\QATestUty.lib         \
    $(TEST_OAK_LIB_PATH)\DebugStreamUty.lib    \
    $(TEST_OAK_LIB_PATH)\WinAppUty.lib         \
    $(TEST_OAK_LIB_PATH)\TuxDLLUty.lib         \
    $(TEST_OAK_LIB_PATH)\DDrawUty.lib          \
