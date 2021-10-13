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


$(__PROJOAKINC)\av_dcp.idl : av_dcp.idl
    copy av_dcp.idl $(__PROJOAKINC)\av_dcp.idl

$(__PROJOAKINC)\av_dcp.h : av_dcp.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+av_dcp.h $(__PROJOAKINC)\av_dcp.h
