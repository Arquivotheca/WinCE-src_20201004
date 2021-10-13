!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
This source code is licensed under Microsoft Shared Source License
Version 1.0 for Windows CE.
For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
!endif
$(SDK_INC_PATH)\upnp.idl : upnp.idl
    copy upnp.idl $(SDK_INC_PATH)\upnp.idl

$(SDK_INC_PATH)\upnphost.idl : upnphost.idl
    copy upnphost.idl $(SDK_INC_PATH)\upnphost.idl

$(SDK_INC_PATH)\upnp.h : upnp.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+upnp.h $(SDK_INC_PATH)\upnp.h

$(SDK_INC_PATH)\upnphost.h : upnphost.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+upnphost.h $(SDK_INC_PATH)\upnphost.h
