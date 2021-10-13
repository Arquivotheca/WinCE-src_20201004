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
#ifndef TUX_MAIN_H
#define TUX_MAIN_H

#include <windows.h>
#include <tux.h>
#include <kato.h>

#define countof(x)  (sizeof(x)/sizeof(*(x)))

#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

struct AudioQalityTestData
{
    DWORD dwDeviceId;
    DWORD dwVolume;
    TCHAR *pszInputFile;
    TCHAR *pszString;
    AudioQalityTestData()
    {
        dwDeviceId = 0;                     //Default audio device id
        dwVolume = 0xC000C000;              //Default master volume
        pszInputFile = NULL;                //Pointer to input file name
        pszString = NULL;                   //Pointer to a temp string
    }
};

BOOL ProcessCommandLine( LPCTSTR szCmdLine );
TESTPROCAPI HandleTuxMessages(UINT uMsg, TPPARAM tpParam);

//
// audio quality drift test
//
TESTPROCAPI 
AudioQualityDriftTest( UINT uMsg, 
				       TPPARAM tpParam, 
				       LPFUNCTION_TABLE_ENTRY lpFTE );

//extern SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam);
extern BOOL WINAPI DllMain( HANDLE hInstance, ULONG dwReason, LPVOID lpReserved );
extern void Debug( LPCTSTR szFormat, ... );

static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    TEXT("Audio Quality Tests"), 0, 0, 0, NULL,
    TEXT("Audio Quality Drift Test"), 1, NULL, 1, AudioQualityDriftTest,

    NULL, 0, 0, 0, NULL
};

#endif
