//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef _DBG_H
#define _DBG_H

//
// Message verbosity: lower values indicate higher urgency
//
#define DBG_LOUDER           0x00000010
#define DBG_LOUD             0x00000008
#define DBG_INFO             0x00000004
#define DBG_WARN             0x00000002
#define DBG_SERIOUS          0x00000001                  

//-----------------------------------------------------------------------------------------------//
VOID  
DebugPrint(
   DWORD    level,
   LPCTSTR  format, ...
   );

#endif