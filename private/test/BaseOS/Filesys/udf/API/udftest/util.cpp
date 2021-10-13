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
#include "util.h"

//******************************************************************************
VOID
Debug(
    LPCTSTR szFormat, 
    ...) 
//******************************************************************************    
{
   TCHAR szBuffer[1024] = TEXT("CDFSTEST: ");

   va_list pArgs; 
   va_start(pArgs, szFormat);
   _vsntprintf(szBuffer + 9, countof(szBuffer) - 11, szFormat, pArgs);
   va_end(pArgs);

   _tcscat(szBuffer, TEXT("\r\n"));

   OutputDebugString(szBuffer);
}

//******************************************************************************
void 
SystemErrorMessage( 
    DWORD dwMessageId )
//
//  Prints the English meaning of a system error code.
//
//  Typical usage:
//
//      SystemErrorMessage( GetLastError() )
//
//******************************************************************************  
{
    LPTSTR msgBuffer;       // string returned from system
    DWORD cBytes;           // length of returned string

    cBytes = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL,
        dwMessageId,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        (TCHAR *)&msgBuffer,
        500,
        NULL );
    if( msgBuffer && cBytes > 0 ) {
        msgBuffer[ cBytes ] = TEXT('\0');
        g_pKato->Log(LOG_DETAIL, TEXT( "SYSTEM ERROR: %s"), msgBuffer );
        LocalFree( msgBuffer );
    }
    else {
        g_pKato->Log(LOG_DETAIL, TEXT( "SYSTEM ERROR: %d"), dwMessageId);
    }
}

//****************************************************************************** 
BOOL 
IsTestFileName(
    LPCTSTR szFileName )
//
//  Returns TRUE if the file name passed in has the proper file extension for
//  a test file. It is okay to pass in just the file or the full path because
//  only the extension is looked at.
//
//  Returns FALSE if file extension does not match.
//
//******************************************************************************     
{
    ASSERT(szFileName);
    if( NULL == szFileName ) 
        return FALSE;

    UINT length = lstrlen(szFileName);

    if( length <= lstrlen(TEST_FILE_EXT) )
        return FALSE;

    // get pointer to the file extension in the name passed in
    LPCTSTR szExt = szFileName + (length - lstrlen(TEST_FILE_EXT));

    if( lstrcmp(TEST_FILE_EXT, szExt) )
        return FALSE;

    return TRUE;
}
