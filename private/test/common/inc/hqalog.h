//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  WHPU Test Help Suite
//
//  Module: hqalog.h
//          Help for logging to Kato and/or OutputDebugString.
//
//  Revision history:
//      10/09/1997  lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __HQALOG_H__
#define __HQALOG_H__
#include "hqatk.h"

////////////////////////////////////////////////////////////////////////////////
// Helper Macros

#define FAILURE_HEADER  TEXT("### ")
#define PRESET          0xCA510DAD

////////////////////////////////////////////////////////////////////////////////
// Helper enumerated types

enum RESULT_TYPE
{
    // Result type
    RESULT_EXCEPTION            = 0x0001,
    RESULT_FAILURE              = 0x0002,
    RESULT_FAILURENOHRESULT     = 0x0003,
    RESULT_FAILURENOTHRESULT    = 0x0004,
    RESULT_INVALID              = 0x0005,
    RESULT_INVALIDNOTPOINTER    = 0x0006,
    RESULT_NULL                 = 0x0007,
    RESULT_NULLNOTPOINTER       = 0x0008,
    RESULT_SUCCESS              = 0x0009,
    RESULT_UNEXPECTED           = 0x000A,
    RESULT_DEBUGBREAK           = 0x000B,
    RESULT_TYPEMASK             = 0x00FF,

    // Options
    RESULT_COMMENTFAILURES      = 0x0100,
    RESULT_ABORTFAILURES        = 0x0200,
    RESULT_OPTIONSMASK          = 0xFF00,

    // Combos
    RESULT_ABORT                = RESULT_FAILURE | RESULT_ABORTFAILURES
};

enum CRP_FLAGS
{
    // Resolution flags
    CRP_DONTRELEASE         = 0x0001,
    CRP_NOTINTERFACE        = CRP_DONTRELEASE,
    CRP_RELEASE             = 0x0002,
    CRP_RELEASEONFAILURE    = 0x0004,
    CRP_NOTPOINTER          = 0x0009,   // Don't release non-pointers
    CRP_RESOLUTIONMASK      = 0x000F,

    // Failure flags
    CRP_FAILFAILURES        = 0x0000,   // Default
    CRP_COMMENTFAILURES     = 0x0100,
    CRP_ABORTFAILURES       = 0x0200,
    CRP_REPORTINGMASK       = 0x0300,

    // Other flags
    CRP_ALLOWNONULLOUT      = 0x1000,
    CRP_NULLNOTFAILURE      = 0x2000
};

enum RAID_DB
{
    RAID_INVALID,
    RAID_KEEP,
    RAID_WINCE,
    RAID_HPURAID
};

////////////////////////////////////////////////////////////////////////////////
// Prototypes

bool    SetKatoServer(LPCTSTR pszKatoServer);
void    DontSetKatoServer(void);
HANDLE  GetKatoHandle(void);
void    KatoSetOptions(BOOL fEnabled, DWORD dwMaxLog, DWORD dwMaxComment, DWORD dwMaxLevel);
void    HQA_Debug(KATOVERBOSITY nVerbosity, LPCTSTR pszFormat, ...);
void    HQA_DebugV(KATOVERBOSITY nVerbosity, LPCTSTR pszFormat, va_list pArgs);
void    HQA_DebugBeginLevel(DWORD dwID, LPCTSTR pszFormat, ...);
void    HQA_DebugBeginLevelV(DWORD dwID, LPCTSTR pszFormat, va_list pArgs);
void    HQA_DebugEndLevel(LPCTSTR pszFormat, ...);
void    HQA_DebugEndLevelV(LPCTSTR pszFormat, va_list pArgs);
bool    UseKato(bool bUse);
bool    UseOutputDebugString(bool bUse);
bool    KatoLogToFile(LPCTSTR pszFileName, bool fAppend, bool fFlush);
void    KatoCloseLogFile();
void    Report(
            int         type,       // Use RESULT_TYPE
            LPCTSTR     lpszInterface,
            LPCTSTR     lpszMethod = NULL,
            HRESULT     hr = S_OK,
            LPCTSTR     lpszQualifier = NULL,
            LPCTSTR     lpszAdditional = NULL,
            HRESULT     hrExpected = S_OK,
            RAID_DB     dbRAID = RAID_INVALID,
            ULONG       nBugID = 0);
bool    CheckReturnedPointer(
            int         crpFlags,   // Use CRP_FLAGS
            IUnknown    *pCheck,
            LPCTSTR     lpszInterface,
            LPCTSTR     lpszMethod,
            HRESULT     hr,
            LPCTSTR     lpszQualifier = NULL,
            LPCTSTR     lpszAdditional = NULL,
            HRESULT     hrExpected = S_OK,
            RAID_DB     dbRAID = RAID_INVALID,
            ULONG       nBugID = 0);

// Inline version for non-interface pointers. If they aren't interface pointers,
// some special treatment is necessary. This overload just makes sure that the
// right flag is passed.
inline bool CheckReturnedPointer(
            int         crpFlags,   // Use CRP_FLAGS
            void        *pCheck,
            LPCTSTR     lpszInterface,
            LPCTSTR     lpszMethod,
            HRESULT     hr,
            LPCTSTR     lpszQualifier = NULL,
            LPCTSTR     lpszAdditional = NULL,
            HRESULT     hrExpected = S_OK,
            RAID_DB     dbRAID = RAID_INVALID,
            ULONG       nBugID = 0)
{
    return CheckReturnedPointer(
        CRP_NOTINTERFACE | (crpFlags&~CRP_RESOLUTIONMASK),
        (IUnknown *)pCheck,
        lpszInterface,
        lpszMethod,
        hr,
        lpszQualifier,
        lpszAdditional,
        hrExpected,
        dbRAID,
        nBugID);
}

// Inline version for non-pointers. If they aren't pointers, some special
// treatment is necessary. This overload just makes sure that the right flags
// are passed.
inline bool CheckReturnedPointer(
            int         crpFlags,   // Use CRP_FLAGS
            DWORD       dwCheck,
            LPCTSTR     lpszInterface,
            LPCTSTR     lpszMethod,
            HRESULT     hr,
            LPCTSTR     lpszQualifier = NULL,
            LPCTSTR     lpszAdditional = NULL,
            HRESULT     hrExpected = S_OK,
            RAID_DB     dbRAID = RAID_INVALID,
            ULONG       nBugID = 0)
{
    return CheckReturnedPointer(
        CRP_NOTPOINTER | (crpFlags&~CRP_RESOLUTIONMASK),
        (IUnknown *)dwCheck,
        lpszInterface,
        lpszMethod,
        hr,
        lpszQualifier,
        lpszAdditional,
        hrExpected,
        dbRAID,
        nBugID);
}

#define Debug               HQA_Debug
#define DebugV              HQA_DebugV
#define DebugBeginLevel     HQA_DebugBeginLevel
#define DebugBeginLevelV    HQA_DebugBeginLevelV
#define DebugEndLevel       HQA_DebugEndLevel
#define DebugEndLevelV      HQA_DebugEndLevelV

#endif // __HQALOG_H__
