!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
This source code is licensed under Microsoft Shared Source License
Version 1.0 for Windows CE.
For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
!endif

$(OAK_INC_PATH)\av_dcp.idl : av_dcp.idl
    copy av_dcp.idl $(OAK_INC_PATH)\av_dcp.idl


$(OAK_INC_PATH)\av_dcp.h : av_dcp.h
    copy $(_PRIVATEROOT)\bat\pbcopyc.txt+av_dcp.h $(OAK_INC_PATH)\av_dcp.h
