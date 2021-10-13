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


$(__PROJSDKINC)\upnp.idl : upnp.idl
    copy upnp.idl $(__PROJSDKINC)\upnp.idl

$(__PROJSDKINC)\upnphost.idl : upnphost.idl
    copy upnphost.idl $(__PROJSDKINC)\upnphost.idl

$(__PROJSDKINC)\upnp.h : upnp.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+upnp.h $(__PROJSDKINC)\upnp.h

$(__PROJSDKINC)\upnphost.h : upnphost.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+upnphost.h $(__PROJSDKINC)\upnphost.h

$(__PROJSDKINC)\dlna.h : dlna.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+dlna.h $(__PROJSDKINC)\dlna.h
