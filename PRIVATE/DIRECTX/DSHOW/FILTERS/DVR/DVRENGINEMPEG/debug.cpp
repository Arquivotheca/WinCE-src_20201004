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
#include <stdafx.h>

#ifndef SHIP_BUILD

#undef DbgLog
#define DbgLog(_x_) RetailMsgConvert _x_

const DWORD Mask=ZONEMASK_ERROR;

DBGPARAM dpCurSettings = {
  TEXT("DVR Engine"),
  {
	DVRENGINE_NAMES
  },
  Mask
};

void RetailMsgConvert(BOOL fCond, DWORD /*Level*/, const TCHAR *pFormat, ...)
{
    va_list va;
    va_start(va, pFormat);

    TCHAR rgchBuf[MAX_DEBUG_STR];
    _vsntprintf (rgchBuf, (sizeof(rgchBuf)/sizeof(rgchBuf[0])), pFormat, va);
    rgchBuf[MAX_DEBUG_STR-1] = '\0';

	if (fCond)
		OutputDebugStringW(rgchBuf);

    va_end(va);
}

#else // debug

const DWORD Mask=0;

DBGPARAM dpCurSettings = {
  TEXT("DVR Engine"),
  {
	DVRENGINE_NAMES
  },
  Mask
};

#endif
