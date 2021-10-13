//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
