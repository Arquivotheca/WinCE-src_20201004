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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    pager.h - internal kernel pager related header
//
#ifndef _NK_PAGER_H_
#define _NK_PAGER_H_

#include "process.h"

#define PAGEIN_FAILURE          0       // paging failed
#define PAGEIN_SUCCESS          1       // paging successful
#define PAGEIN_RETRY            2       // retry to paging

DWORD PageFromLoader (PPROCESS pprc, DWORD dwAddr, BOOL fWrite);
DWORD PageFromMapper (PPROCESS pprc, DWORD dwAddr, BOOL fWrite);


#endif  // _NK_PAGER_H_

