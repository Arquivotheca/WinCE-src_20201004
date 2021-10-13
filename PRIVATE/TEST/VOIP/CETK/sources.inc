!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
This source code is licensed under Microsoft Shared Source License
Version 1.0 for Windows CE.
For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
