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
////////////////////////////////////////////////////////////////////////////////
//
//  Common USB loader functions.
//  Common Kato logging functions.
//
//
//  Module: loadfn.cpp
//
////////////////////////////////////////////////////////////////////////////////

//******************************************************************************
//***** Include files
//******************************************************************************
#include    <windows.h>
#include    <tchar.h>
#include    <excpt.h>

#include    <katoex.h>

#ifndef UNDER_CE
#include    <stdio.h>
#endif

#include    <usbdi.h>
#include    "loadfn.h"

#ifdef  UNDER_CE
#define printf  NKDbgPrintfW
#endif
#define MAX_WARN_ALLOWED 2

//******************************************************************************
//***** Defines
//******************************************************************************



//******************************************************************************
//***** Global function declarations
//******************************************************************************

BOOL    WINAPI      LoadGlobalKato      ( void );
void    WINAPI      Log                 ( LPTSTR szFormat, ... );
void    WINAPI      Fail                ( LPTSTR szFormat, ... );

BOOL    WINAPI      LoadDllGetAddress   ( LPTSTR szDllName,
                                          LPTSTR szDllFnText[],
                                          LPDLL_ENTRY_POINTS lpDll );
BOOL    WINAPI      UnloadDll           ( LPDLL_ENTRY_POINTS lpDll );

LPTSTR  WINAPI      SplitCommandLine    ( LPTSTR lpCmd );
LPTSTR  WINAPI      GetUSBErrorString   ( LPTSTR lpStr, DWORD dwError );


//******************************************************************************
//***** Local function declarations
//******************************************************************************

//******************************************************************************
//***** Local static variables
//******************************************************************************

//******************************************************************************
//***** Global static variables
//******************************************************************************

HKATO   g_hKato = NULL;
DWORD	m_dwFailCount=0;
DWORD	m_dwWarnCount=0;

//******************************************************************************
//******************************************************************************


//******************************************************************************
BOOL    WINAPI      LoadGlobalKato    ( void )
{
    // is Kato logging object already there?
    if ( g_hKato )
        return ( TRUE );

    // create Kato logging object
    g_hKato = KatoGetDefaultObject();
    if ( g_hKato == NULL )
    {
        printf( TEXT("FATAL ERROR: Could not get Kato Logging Object" ) );
        return FALSE;
    }
	m_dwFailCount=0;
    KatoDebug( TRUE, LOG_DETAIL, KATO_MAX_VERBOSITY, KATO_MAX_LEVEL );

    return ( TRUE );
}

BOOL    WINAPI		CheckKatoInit( void )
{
	static fDoneThisOnce = FALSE;

	if ( ! g_hKato )
	{
        if ( fDoneThisOnce )
            return FALSE;
        fDoneThisOnce = TRUE;
        if ( !LoadGlobalKato() )
            return FALSE;
    }
	return TRUE;
}

//******************************************************************************
void    WINAPI      Log(LPTSTR szFormat, ...)
{
    if ( !CheckKatoInit() )
		return;

    va_list pArgs;

    va_start(pArgs, szFormat);
    ((CKato *)g_hKato)->LogV( LOG_DETAIL, szFormat, pArgs );
    va_end(pArgs);
}

//******************************************************************************
void    WINAPI      LCondVerbose(BOOL fVerbose, LPTSTR szFormat, ...)
{
    if ( !CheckKatoInit() )
		return;

    va_list pArgs;

    va_start(pArgs, szFormat);
	if (fVerbose)
		((CKato *)g_hKato)->LogV( LOG_DETAIL, szFormat, pArgs );
	else
		((CKato *)g_hKato)->LogV( LOG_COMMENT, szFormat, pArgs );
    va_end(pArgs);
}

//******************************************************************************
void    WINAPI      LogVerboseV(BOOL fVerbose, LPTSTR szFormat, va_list pArgs)
{
    if ( !CheckKatoInit() )
		return;

	if (fVerbose)
		((CKato *)g_hKato)->LogV( LOG_DETAIL, szFormat, pArgs );
	else
		((CKato *)g_hKato)->LogV( LOG_COMMENT, szFormat, pArgs );
}

#define LOCAL_STRING_BUFFER_SIZE 0x200
//******************************************************************************
void    WINAPI      Fail(LPTSTR szFormat, ...)
{
	TCHAR sBuffer[LOCAL_STRING_BUFFER_SIZE]; // 512 byte local buffer.

    if ( !CheckKatoInit() )
		return;

    va_list pArgs;
    va_start(pArgs, szFormat);
	_vsntprintf_s(sBuffer,_countof(sBuffer),_countof(sBuffer)-1,szFormat,pArgs);
    va_end(pArgs);
	sBuffer[LOCAL_STRING_BUFFER_SIZE-1]=0;
    ((CKato *)g_hKato)->Log( LOG_FAIL,TEXT("FAIL: %s"), sBuffer);
	m_dwFailCount++;
}

//******************************************************************************
void    WINAPI      LWarn(LPTSTR szFormat, ...)
{
	TCHAR sBuffer[LOCAL_STRING_BUFFER_SIZE]; // 512 byte local buffer.

    if ( !CheckKatoInit() )
		return;

    va_list pArgs;
    va_start(pArgs, szFormat);
	_vsntprintf_s(sBuffer,_countof(sBuffer),_countof(sBuffer)-1,szFormat,pArgs);
    va_end(pArgs);
	sBuffer[LOCAL_STRING_BUFFER_SIZE-1]=0;
	m_dwWarnCount++;
	if(m_dwWarnCount++ > MAX_WARN_ALLOWED)
		Fail(sBuffer);
	else
		((CKato *)g_hKato)->Log( LOG_WARNING,TEXT("WARNING: %s"), sBuffer);
}

void WINAPI ClearFailWarnCount (void)
{
	m_dwFailCount=0;
	m_dwWarnCount=0;
}

void WINAPI ClearFailCount (void)
{
	m_dwFailCount=0;
}

DWORD WINAPI GetFailCount(void)
{
	return m_dwFailCount;
}

DWORD WINAPI GetWarnCount(void)
{
	return m_dwWarnCount;
}
//******************************************************************************
BOOL    WINAPI    LoadDllGetAddress ( LPTSTR szDllName,
                                      LPTSTR szDllFnText[],
                                      LPDLL_ENTRY_POINTS lpDll )
{
    BOOL        fException;
    DWORD       dwExceptionCode = 0;
    HINSTANCE   hInst;

    /*
    *   We will be logging a lot, make sure Kato is up
    */
    if ( ! LoadGlobalKato() )
        return ( FALSE );

    /*
    *   Try loading the library
    */
    Log(
        TEXT("Library \"%s\", trying to load...\r\n"),
        szDllName
        );

    hInst = LoadLibrary( szDllName );
    __try
    {
        /*
        *   This is where most of the failures occurr,
        *   if we were passed a bad pointer.
        *   LoadLibrary() system call handles exceptions
        *   based on the szDllName parameter.
        */
        fException = FALSE;
        lpDll->hInst = hInst;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        fException = TRUE;
        dwExceptionCode = _exception_code();
    }

    if ( fException )
    {
        Fail(
            TEXT("Library \"%s\" NOT loaded! Exception 0x%08X! GetLastError()=%u.\r\n"),
            szDllName,
            dwExceptionCode,
            GetLastError()
            );
        return FALSE;
    }

    if( !lpDll->hInst )
    {
        Fail(
            TEXT("Library \"%s\" NOT loaded! GetLastError()=%u.\r\n"),
            szDllName,
            GetLastError()
            );
        return FALSE;
    }


    /*
    *   Now that library is loaded, make local copy of its name
    */
    __try
    {
        fException = FALSE;
        _tcscpy_s( lpDll->szDllName, MAX_PATH, szDllName );
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        fException = TRUE;
        dwExceptionCode = _exception_code();
    }
    if ( fException )
    {
        Fail(
            TEXT("Library \"%s\" NOT loaded! Exception 0x%08X! GetLastError()=%u.\r\n"),
            szDllName,
            dwExceptionCode,
            GetLastError()
            );
        return FALSE;
    }
    Log(
        TEXT("Library \"%s\" loaded: hInst=0x%08X.\r\n"),
        lpDll->szDllName,
        lpDll->hInst
        );

    /*
    *   Was this just a call to get the library loaded
    */
    if ( ! szDllFnText )
        return ( TRUE );

    /*
    *   Cycle through and find addresses of all functions
    */
    for ( lpDll->dwCount=0; szDllFnText[lpDll->dwCount]; lpDll->dwCount++ )
    {
        lpDll->lpProcAddr[lpDll->dwCount] =
            GetProcAddress( lpDll->hInst, szDllFnText[lpDll->dwCount] );
        if ( !lpDll->lpProcAddr[lpDll->dwCount] )
        {
            Fail(
                TEXT("Unable to find function \"%s\". GetLastError()=%u.\r\n"),
                szDllFnText[lpDll->dwCount],
                GetLastError()
                );
#ifdef  FAIL_ON_FUNCTION_NOT_FOUND
            return ( FALSE );
#endif
        }
        else
        {
            Log(
                TEXT("Found function @ 0x%08X: \"%s()\".\r\n"),
                lpDll->lpProcAddr[lpDll->dwCount],
                szDllFnText[lpDll->dwCount]
                );
        }
    }

    return ( TRUE );
}


//******************************************************************************
BOOL    WINAPI    UnloadDll ( LPDLL_ENTRY_POINTS lpDll )
{
    /*
    *   We will be logging a lot, make sure Kato is up
    */
    if ( ! LoadGlobalKato() )
        return ( FALSE );

    if ( ! lpDll )
    {
        Fail(
            TEXT("Unable to free library. NULL pointer passed!.\r\n")
            );
        return ( FALSE );
    }

    if ( ! lpDll->hInst )
    {
        Fail(
            TEXT("Unable to free library. NULL instance handle passed!.\r\n")
            );
        return ( FALSE );
    }

    if ( ! FreeLibrary( lpDll->hInst ) )
    {
        Fail(
            TEXT("Library \"%s\" failed to free, hInstance=0x%08X. GetLastError()=%u.\r\n"),
            lpDll->szDllName,
            lpDll->hInst,
            GetLastError()
            );
        return ( FALSE );
    }

    Log(
        TEXT("Library \"%s\" freed, hInstance=0x%08X.\r\n"),
        lpDll->szDllName,
        lpDll->hInst
        );

    lpDll->hInst = NULL;

    return ( TRUE );
}



//******************************************************************************
/*
*   Check if command line had any parameters and separate them from the application name.
*
*   Routine returns the pointer to the rest of the command line after the parameter, or NULL.
*/
LPTSTR  WINAPI  SplitCommandLine( LPTSTR lpCmd )
{
    if ( ! lpCmd )
    	return ( NULL );

    /*
    *   Skip blanks at the very beginning.
    */
    for ( ;; lpCmd++)
    {
    	if ( ! *lpCmd )
	        return ( NULL );
    	if ( *lpCmd != ' ' )
            break;
    }

    /*
    *   Separate next command line argument from the rest of the string.
    */
    for ( ;; lpCmd++)
    {
    	if ( ! *lpCmd )
	        return ( NULL );
    	if ( *lpCmd == ' ' )
	    {
	        *lpCmd++ = '\0';
    	    break;
	    }
    }


    /*
    *   Whatever is left is a parameter
    */
    return ( lpCmd );
}


//******************************************************************************
/*
*   Return string given the USB error.
*   If lpStr is NULL, pointer to local static buffer is returned.
*/
LPTSTR  WINAPI  GetUSBErrorString   ( LPTSTR lpStr, DWORD dwError )
{
    static  TCHAR   szError[64];
    static  TCHAR   szUnknown[32];

    LPTSTR  lpErr;

    if ( !lpStr )
        lpStr = szError;

    switch( dwError )
    {
        case     USB_NO_ERROR                        : lpErr = TEXT("USB_NO_ERROR");                    break;
        case     USB_CRC_ERROR                       : lpErr = TEXT("USB_CRC_ERROR");                   break;
        case     USB_BIT_STUFFING_ERROR              : lpErr = TEXT("USB_BIT_STUFFING_ERROR");          break;
        case     USB_DATA_TOGGLE_MISMATCH_ERROR      : lpErr = TEXT("USB_DATA_TOGGLE_MISMATCH_ERROR");  break;
        case     USB_STALL_ERROR                     : lpErr = TEXT("USB_STALL_ERROR");                 break;
        case     USB_DEVICE_NOT_RESPONDING_ERROR     : lpErr = TEXT("USB_DEVICE_NOT_RESPONDING_ERROR"); break;
        case     USB_PID_CHECK_FAILURE_ERROR         : lpErr = TEXT("USB_PID_CHECK_FAILURE_ERROR");     break;
        case     USB_UNEXPECTED_PID_ERROR            : lpErr = TEXT("USB_UNEXPECTED_PID_ERROR");        break;
        case     USB_DATA_OVERRUN_ERROR              : lpErr = TEXT("USB_DATA_OVERRUN_ERROR");          break;
        case     USB_DATA_UNDERRUN_ERROR             : lpErr = TEXT("USB_DATA_UNDERRUN_ERROR");         break;
        case     USB_BUFFER_OVERRUN_ERROR            : lpErr = TEXT("USB_BUFFER_OVERRUN_ERROR");        break;
        case     USB_BUFFER_UNDERRUN_ERROR           : lpErr = TEXT("USB_BUFFER_UNDERRUN_ERROR");       break;
        case     USB_NOT_ACCESSED_ERROR              : lpErr = TEXT("USB_NOT_ACCESSED_ERROR");          break;
        case     USB_ISOCH_ERROR                     : lpErr = TEXT("USB_ISOCH_ERROR");                 break;
        case     USB_CANCELED_ERROR                  : lpErr = TEXT("USB_CANCELED_ERROR");              break;
        case     USB_NOT_COMPLETE_ERROR              : lpErr = TEXT("USB_NOT_COMPLETE_ERROR");          break;

        default:
            _stprintf_s( szUnknown, _countof(szUnknown), TEXT("USB_UNKNOWN_ERROR_%u"), dwError );
            lpErr = szUnknown;
            break;
    }

    _tcscpy_s( lpStr, _tcslen(lpStr), lpErr );

    return lpStr;
}
