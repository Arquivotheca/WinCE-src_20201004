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
#include "common.h"

//
// Print Utility
//
void RASPrint(TCHAR *pFormat, ...)
{
    va_list ArgList;
    TCHAR    Buffer[256];

    va_start (ArgList, pFormat);

    (void)StringCchVPrintf(Buffer, 256, pFormat, ArgList);

#ifndef UNDER_CE
    _putts(Buffer);
#else
    OutputDebugString(Buffer);
#endif

    va_end(ArgList);
}

// END OF FILE
