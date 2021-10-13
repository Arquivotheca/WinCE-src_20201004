//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "ShellProc.h"
#include "NdisDetectionTests.h"

//------------------------------------------------------------------------------

#define BASE 0x00000001

//------------------------------------------------------------------------------

extern LPCTSTR g_szTestGroupName = _T("NdisDetection");

//------------------------------------------------------------------------------

extern enum NDT_PROTOCOL_TYPE;

//------------------------------------------------------------------------------

extern FUNCTION_TABLE_ENTRY g_lpFTE[] = {
   _T("IsEthernetMiniportPresent"              ), 0, 0, BASE + 0, TestIsEthernetMiniportPresent,
   NULL, 0, 0, 0, NULL 
};

void PrintUsage()
{
}

//------------------------------------------------------------------------------

BOOL ParseCommands(INT argc, LPTSTR argv[])
{
   BOOL bResult = FALSE;
   LPTSTR szOption = NULL;

   bResult = TRUE;
 
   if (!bResult) PrintUsage();
   return bResult;
}

//------------------------------------------------------------------------------