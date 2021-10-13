!if 0
Copyright (c) Microsoft Corporation.  All rights reserved.
!endif
!if 0
This source code is licensed under Microsoft Shared Source License
Version 1.0 for Windows CE.
For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
!endif
$(SDK_INC_PATH)\asp.h:asp.h
   copy $(_PRIVATEROOT)\bat\pbcopyc.txt+asp.h $(SDK_INC_PATH)\asp.h
