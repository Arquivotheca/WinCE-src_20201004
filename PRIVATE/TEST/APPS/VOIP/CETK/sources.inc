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

!IF 0 

Module Name:

    sources.inc

Abstract:

    This file specifies the list of
    commong INCLUDES and TARGETLIBS needed to build voip components.

!ENDIF


INCLUDES=$(_PUBLICROOT)\voip\sdk\inc;$(_PUBLICROOT)\Common\sdk\inc;$(_PUBLICROOT)\directx\sdk\inc;\
	 $(_WINCEROOT)\PRIVATE\TEST\INC;$(_WINCEROOT)\others\WCETK\tux\inc\;

TARGETLIBS= \
    $(_PROJECTROOT)\cesysgen\sdk\lib\$(_TGTCPU)\$(WINCEDEBUG)\coredll.lib \
    $(_PROJECTROOT)\cesysgen\sdk\lib\$(_TGTCPU)\$(WINCEDEBUG)\ole32.lib \
    $(_PROJECTROOT)\cesysgen\sdk\lib\$(_TGTCPU)\$(WINCEDEBUG)\oleaut32.lib \
    $(_PROJECTROOT)\cesysgen\sdk\lib\$(_TGTCPU)\$(WINCEDEBUG)\uuid.lib \
    $(_PUBLICROOT)\voip\oak\lib\$(_TGTCPU)\$(WINCEDEBUG)\voipguid.lib \
