//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "common.h"

//
// Print Utility
//
void RASPrint(TCHAR *pFormat, ...)
{
	va_list ArgList;
	TCHAR	Buffer[256];

	va_start (ArgList, pFormat);

	(void)wvsprintf (Buffer, pFormat, ArgList);

#ifndef UNDER_CE
	_putts(Buffer);
#else
	OutputDebugString(Buffer);
#endif

	va_end(ArgList);
}

// END OF FILE
