//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _CONFIG_H_
#define _CONFIG_H_


BOOL
TSPI_EditMiniConfig(
   HWND        hWndOwner,
   PTLINEDEV   pLineDev,
   PDEVMINICFG lpSettingsIn,
   PDEVMINICFG lpSettingsOut
   );
   


#endif // _CONFIG_H_
