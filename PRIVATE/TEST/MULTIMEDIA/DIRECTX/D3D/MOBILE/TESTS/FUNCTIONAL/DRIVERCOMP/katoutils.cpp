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
#include <windows.h>
#include <katoutils.h>
#include <tchar.h>

//
// IsKatoInit
//
//   This function may be called from the beginning of any function that may
//   potentially make calls to the Kato logging engine.
//
// Return Value:
// 
//   BOOL:  True if Kato is initialized; False if Kato is not initialized.
//
BOOL IsKatoInit()
{
	if (NULL == g_pKato)
	{
		OutputDebugString(_T("Kato interface is not initialized."));
		return FALSE;
	}
	return TRUE;
}
