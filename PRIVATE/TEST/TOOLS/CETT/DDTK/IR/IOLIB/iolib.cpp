//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include <oscfg.h>

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
	OutputDebugString(&szBuffer[0]);

#else // UNDER_CE
	_vtprintf(format, pArgs);
#endif
	va_end(pArgs);
}
