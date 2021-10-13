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


RELEASETYPE=OAK


ERRORFORBANNEDAPIS=

INCLUDES=$(INCLUDES)\
         $(_PUBLICROOT)\common\sdk\inc; \
         $(_PUBLICROOT)\common\oak\inc; \
         ..\..\..\..\common\inc; \
         ..\..\..\..\PowerMan\common\inc; \
         ..; \
         $(_WINCEROOT)\tools\public\ext\sdk\inc\mfc42; \
         $(_WINCEROOT)\tools\public\ext\sdk\inc; \

TARGETLIBS= \
    $(_COREDLL) \
    $(_BCRYPT) \
    $(TEST_OAK_LIB_PATH)\pwrtstutil.lib \
    $(_KATO) \
#TARGETLIBS=$(SDKTOOLS_LIB_PATH)\wttlog.lib

SOURCES= \
        ..\afxutil.cpp \
        ..\IfdTest.cpp \
        ..\ifdscrpt.cpp \
        ..\ifdrdr.cpp \
        ..\ifdmsg.cpp \
        ..\errmap.c \
        ..\ifdguid.cpp \
        ..\tuxmain.cpp \
        ..\MultiReaderTests.cpp \
        ..\USBReaderSwitchBox.cpp \
        ..\InsertRemoveTests.cpp
