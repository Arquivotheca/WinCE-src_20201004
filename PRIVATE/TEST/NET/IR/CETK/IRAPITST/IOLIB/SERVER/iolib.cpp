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
