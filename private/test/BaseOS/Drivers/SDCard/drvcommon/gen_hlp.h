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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
// Copyright (c) Microsoft Corporation.  All rights reserved.//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//

#ifndef GEN_HLP_H
#define GEN_HLP_H
#include <windows.h>

#define STOP_SERIES  0xffffffff

typedef enum
{
    BUFF_ZERO = 0,
    BUFF_HIGH = 1,
    BUFF_RANDOM = 2,
    BUFF_RANDOM_NOSET = 3,
    BUFF_STRING = 4,
    BUFF_ZSTRING = 5,
    BUFF_ALPHANUMERIC = 6
}SD_BUFF_TYPE;

typedef enum
{
    STRING_ZERO = 0,
    STRING_HIGH = 1,
    STRING_TEXT = 2,
    STRING_HEX = 3
} SD_STRING_TYPE;



BOOL AllocBuffer(UINT indent, UINT numUchars, SD_BUFF_TYPE bt, PUCHAR *pBUFF, DWORD dwSeed = 0, LPCSTR txt = NULL);

void DeAllocBuffer(PUCHAR *pBUFF);

LPTSTR GenerateHexString(UCHAR const *pInBuff, DWORD dwLengthInBuff);

LPTSTR GenerateBinString(UCHAR const *pInBuff, DWORD dwLengthInBuff);

BOOL AllocateTCString(UINT indent, SD_STRING_TYPE st, UCHAR const *pInBuff, DWORD dwLength, LPTSTR *ppOutBuff);

void DeAllocTCString(LPTSTR *ppStr);

BOOL ReportPassFail(BOOL bCondition, UINT indent, LPCTSTR leadTxt, LPCTSTR failureMsg, BOOL bTrueIsPass=TRUE);

void ShiftBuffer(IN UINT indent, UCHAR const *inBuffer, OUT PUCHAR *poutBUFF, IN DWORD dwBuffSize, IN DWORD dwBitsShifted, IN BOOL bRight = FALSE);

UCHAR ReverseUCHAR (IN UCHAR uc);

DWORD dwRndUp(DWORD dwDividend,  DWORD dwDivisor);

BOOL SameVal(UINT first, ...);

BOOL InOrderVal(BOOL bAssend, UINT first, ...);

#endif //GEN_HLP_H
