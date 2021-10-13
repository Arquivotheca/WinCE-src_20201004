!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
This source code is licensed under Microsoft Shared Source License
Version 1.0 for Windows CE.
For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
!endif

$(OAK_INC_PATH)\av_upnp.h : av_upnp.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+av_upnp.h $(OAK_INC_PATH)\av_upnp.h

$(OAK_INC_PATH)\av_upnp_impl.h : av_upnp_impl.h
	copy $(_PRIVATEROOT)\bat\pbcopyc.txt+av_upnp_impl.h $(OAK_INC_PATH)\av_upnp_impl.h

$(OAK_INC_PATH)\av_upnp_internal.h : av_upnp_internal.h
	copy $(_PRIVATEROOT)\bat\pbcopyc.txt+av_upnp_internal.h $(OAK_INC_PATH)\av_upnp_internal.h
