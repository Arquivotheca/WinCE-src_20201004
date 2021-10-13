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
#ifndef IFDGUID_H
#define IFDGUID_H
///////////////////////////////////////////////////////////
/*++

    Module Name: ifdguid.h

    Abstract: Header file for ifdguid.c.

    Author(s): Eric St.John (ericstj) 4.1.2006

    Environment: User Mode

    Revision History:

--*/
///////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <windows.h>
#include <strsafe.h>

///////////////////////////////////////////////////////////
// PUBLIC FUNCTION PROTOTYPES
///////////////////////////////////////////////////////////

const LPWSTR GetTestCaseUID();

///////////////////////////////////////////////////////////
// PUBLIC GLOBALS
///////////////////////////////////////////////////////////
extern UCHAR g_uiFriendlyPhase;
extern UINT g_uiFriendlyTest1;
extern UINT g_uiFriendlyTest2;
extern UINT g_uiFriendlyTest3;

#endif //end IFDGUID_H

