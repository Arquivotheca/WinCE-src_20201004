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

#ifndef DEBUG_H
#define DEBUG_H

#include <sd_tst_cmn.h>
#include <SDCardDDK.h>
#include "sdtst.h"

#ifdef DEBUG
void LogDebugV(BOOL bZone, LPCTSTR lpctstr, va_list pVAL);
void LogDebugV(UINT uiIndent, BOOL bZone, LPCTSTR lpctstr, va_list pVAL);
void LogDebugV(LPCTSTR lpctstr, va_list pVAL);
#endif //DEBUG

void LogDebug(BOOL bZone, LPCTSTR lpctstr, ...);
void LogDebug(UINT uiIndent, BOOL bZone, LPCTSTR lpctstr, ...);
void LogDebug(LPCTSTR lpctstr, ...);

#ifdef DEBUG
void LogFailV(LPCTSTR lpctstr, va_list pVAL);
void LogFailV(UINT uiIndent, LPCTSTR lpctstr, va_list pVAL);
#endif //DEBUG

void LogFail(LPCTSTR lpctstr, ...);
void LogFail(UINT uiIndent, LPCTSTR lpctstr, ...);

#ifdef DEBUG
void LogWarnV(LPCTSTR lpctstr, va_list pVAL);
void LogWarnV(UINT uiIndent, LPCTSTR lpctstr, va_list pVAL);
#endif //DEBUG

void LogWarn(LPCTSTR lpctstr, ...);
void LogWarn(UINT uiIndent, LPCTSTR lpctstr, ...);

#ifdef DEBUG
void LogAbortV(LPCTSTR lpctstr, va_list pVAL);
void LogAbortV(UINT uiIndent, LPCTSTR lpctstr, va_list pVAL);
#endif //DEBUG

void LogAbort(LPCTSTR lpctstr, ...);
void LogAbort(UINT uiIndent, LPCTSTR lpctstr, ...);

void InitializeTestParamsBufferAndResult(PTEST_PARAMS pTP);

BOOL IsBufferEmpty(PTEST_PARAMS pTP);

void SetMessageBufferAndResult(PTEST_PARAMS pTP, int iRes, LPCTSTR lpctstr, ...);

void AppendMessageBufferAndResult(PTEST_PARAMS pTP, int iRes, LPCTSTR lpctstr, ...);

void DeleteTestBufferCriticalSection();

void DebugPrintState(UINT indent, BOOL bZone, LPCTSTR lpctstrPre, SD_DEVICE_HANDLE hDevice, PSD_CARD_STATUS pCrdStat = NULL);

void GenerateSucessMessage(PTEST_PARAMS pTP, LPTSTR lptstr, ...);

void GenerateSucessMessage(PTEST_PARAMS pTP);

#endif //DEBUG_H
