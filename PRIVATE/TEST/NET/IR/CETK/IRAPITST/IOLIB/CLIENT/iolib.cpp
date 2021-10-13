//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <oscfg.h>
#include <katoex.h>

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
extern CKato *g_pKato;

//******************************************************************************

//
//  OutStr: A formatted output routine that outputs to the serial port under
//		    CE or the console otherwise.
//
extern void
OutStr(
	   TCHAR *format, 
	   ...)
{
	va_list		 pArgs;

	va_start(pArgs, format);

#ifdef UNDER_CE
	TCHAR szBuffer[256];
	int	  cbWritten;

	cbWritten = _vsntprintf(&szBuffer[0], countof(szBuffer), format, pArgs);
	//OutputDebugString(&szBuffer[0]);
	g_pKato->Log(LOG_DETAIL, &szBuffer[0]);
	

#else // UNDER_CE
	_vtprintf(format, pArgs);
#endif
	va_end(pArgs);
}
