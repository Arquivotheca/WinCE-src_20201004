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
CDEFINES= \
    -DUNICODE \
    -D_UNICODE

INCLUDES= \
    ..\..\inc; \
    $(TEST_ROOT)\net\common\inc; \
    $(_PUBLICROOT)\common\sdk\inc; \
    $(_PUBLICROOT)\common\oak\inc; \
    $(_PUBLICROOT)\common\ddk\inc; 

SOURCES= \
    ..\NdsNdt1c.rc \
    ..\ndt_1c.cxx \
    ..\OpenClose.cxx \
    ..\Send.cxx \
    ..\LoopbackSend.cxx \
    ..\LoopbackStress.cxx \
    ..\SetMulticast.cxx \
    ..\CancelSend.cxx \
    ..\Reset.cxx \
    ..\FaultHandling.cxx \
    ..\Oids.cxx \
    ..\64BitOids.cxx \
    ..\ShellProc.cxx \
    ..\Utils.cxx \
    ..\SusRes.cxx \
    ..\StressSusRes.cxx \
    ..\ResetOnResume.cxx