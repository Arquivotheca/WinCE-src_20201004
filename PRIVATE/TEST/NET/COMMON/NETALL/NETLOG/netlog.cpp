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


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:
    NetLog.cpp

Abstract:
    Common logging interface for Windows CE NetQA programs

Revision History:
	19-Apr-1999 - added comments
	22-Jun-1999 - modified CE console logging
	20-Sep-1999 - modified logging of Unicode strings
	 8-Oct-1999 - made NTANSI-compatible using TCHAR macros
	14-Feb-2001 - Changed into a DLL

-------------------------------------------------------------------*/
#include "NetLog.h"
#ifdef UNDER_CE
#include <ceassert.h>
#else
#include <crtdbg.h>
#endif

//------------------------------------------------------------------------
//	PPSH Goodies
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#ifndef _O_RDONLY
#define _O_RDONLY     0x0000
#endif

#ifndef _O_WRONLY
#define _O_WRONLY     0x0001
#endif

#ifndef _O_SEQUENTIAL
#define _O_SEQUENTIAL 0x0020
#endif

#ifndef _O_CREAT
#define _O_CREAT      0x0100
#endif

#ifndef _O_EXCL
#define _O_EXCL       0x0400
#endif

#ifndef SEEK_SET
#define SEEK_SET      0
#endif

#ifndef SEEK_END
#define SEEK_END      2
#endif

#define countof(a)    (sizeof(a)/sizeof(*(a)))

// Console logging is only supported if "stdio" is present (not in "mincomm")
#if !defined(UNDER_CE) || defined(getwc)
#define NETLOG_CONSOLE_ENABLED
#include <stdio.h>
#endif

//******************************************************************************
extern "C" int U_ropen(const WCHAR *, UINT);
extern "C" int U_rread(int, BYTE *, int);
extern "C" int U_rwrite(int, BYTE *, int);
extern "C" int U_rlseek(int, int, int);
extern "C" int U_rclose(int);


const DWORD		CNetLog::sm_cnBuffer										= 1024;
HINSTANCE		CNetLog::sm_hKatoDll										= NULL;
DWORD			CNetLog::sm_nKatoUse										= 0;
BOOL 			(WINAPI  *CNetLog::sm_pKatoSetServerW)( LPCTSTR )					= NULL;
BOOL 			(WINAPI  *CNetLog::sm_pKatoFlush)( DWORD )						= NULL;
HANDLE 			(WINAPI  *CNetLog::sm_pKatoCreateW)( LPCTSTR )					= NULL;
BOOL   			(WINAPI  *CNetLog::sm_pKatoDestroy)( HANDLE )						= NULL;
BOOL			(WINAPIV *CNetLog::sm_pKatoLogW)( HANDLE , DWORD , LPCTSTR , ...)	= NULL;

#ifdef UNDER_CE
#define CEPROC( txt ) L##txt
#else
#define CEPROC( txt ) txt
#endif

#define REPLACECHAR		("?")


BOOL CNetLog::LoadKato( void )
{
	HANDLE	hKatoMutex	= NULL;
	BOOL	fRtn		= FALSE;

	hKatoMutex = CreateMutex( NULL, TRUE, TEXT("NetLogKato"));
	if( NULL == hKatoMutex ) return FALSE;

	if( sm_hKatoDll )
	{
		sm_nKatoUse++;
		ReleaseMutex( hKatoMutex );
		CloseHandle( hKatoMutex );
		return TRUE;

	} // end if( sm_hKatoDll )

	__try {

   sm_hKatoDll = LoadLibrary(TEXT("Kato.dll"));
	if( NULL == sm_hKatoDll ) __leave;

	sm_pKatoSetServerW			= (BOOL (WINAPI *)(LPCTSTR))GetProcAddress( sm_hKatoDll, CEPROC("KatoSetServerW") );
	_ASSERT( sm_pKatoSetServerW );
	sm_pKatoFlush				= (BOOL (WINAPI *)(DWORD))GetProcAddress( sm_hKatoDll, CEPROC("KatoFlush") );
	_ASSERT( sm_pKatoFlush );
	sm_pKatoCreateW				= (HANDLE (WINAPI *)(LPCTSTR))GetProcAddress( sm_hKatoDll, CEPROC("KatoCreateW") );
	_ASSERT( sm_pKatoCreateW );
	sm_pKatoDestroy				= (BOOL (WINAPI *)(HANDLE))GetProcAddress( sm_hKatoDll, CEPROC("KatoDestroy") );
	_ASSERT( sm_pKatoDestroy );
	sm_pKatoLogW				= (BOOL (WINAPIV *)(HANDLE, DWORD, LPCTSTR,...))GetProcAddress( sm_hKatoDll, CEPROC("KatoLogW") );
	_ASSERT( sm_pKatoLogW );

	sm_nKatoUse++;
	fRtn = TRUE;

	} __finally {

	if( FALSE == fRtn )
	{
		FreeLibrary( sm_hKatoDll );
		sm_hKatoDll					= NULL;
		sm_pKatoSetServerW			= NULL;
		sm_pKatoFlush				= NULL;
		sm_pKatoCreateW				= NULL;
		sm_pKatoDestroy				= NULL;
		sm_pKatoLogW				= NULL;
		
	} // end if( FALSE == fRtn )

	ReleaseMutex( hKatoMutex );
	CloseHandle( hKatoMutex );

	} // end try-finally

	return fRtn;

} // end BOOL CNetLog::LoadKato( void )


BOOL CNetLog::UnloadKato( void )
{
	HANDLE	hKatoMutex	= NULL;

	hKatoMutex = CreateMutex( NULL, TRUE, TEXT("NetLogKato"));
	if( NULL == hKatoMutex ) return FALSE;

	//--------------------------------------------------------------------
	//	Decrement use cound
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	sm_nKatoUse--;

	if( 0 == sm_nKatoUse )
	{
		FreeLibrary( sm_hKatoDll );
		sm_hKatoDll					= NULL;
		sm_pKatoSetServerW			= NULL;
		sm_pKatoFlush				= NULL;
		sm_pKatoCreateW				= NULL;
		sm_pKatoDestroy				= NULL;
		sm_pKatoLogW				= NULL;

	} // end if( 0 == sm_nKatoUse )

	ReleaseMutex( hKatoMutex );
	CloseHandle( hKatoMutex );

	return TRUE;

} // end BOOL CNetLog::UnloadKato( void )


///////////////////////////////////////////////////////////////////////////
//  Construct the class using input parameters.
//  
//  Initalizes the object and performs sanity checks to make sure that 
//  the desired logging tools are available for usage.
// 
//  This function performs error checking and returns an error if there is a
//  problem, however since it is only called from the object constructors there
//  is no way to inform the user that there was a problem.  The errors should be
//  replaced with asserts so that the user can see the problem.
//
//  History:
//         3/31/99  Added comments (much needed).
//         3/31/99  Removed the 'inline' keyword, no reason to have it there.
//		   10/8/99  Introduced TCHAR mappings to make this ANSI-compatible
///////////////////////////////////////////////////////////////////////////
BOOL CNetLog::Construct( LPCTSTR lpszName, DWORD fType, LPCTSTR lpszServer )
{

	BOOL		fRtn;
	TCHAR		szLogFile[MAX_PATH+1];
	SYSTEMTIME	SystemTime;

	m_dwVerbosity		= LOG_MAX_VERBOSITY;
	m_dwLastError		= ERROR_SUCCESS;

	//--------------------------------------------------------------------
	//	Initalize member variables
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	m_lpszThisName	= NULL;
	m_lpszServer	= NULL;
	m_fDbg			= FALSE;
	m_fKato			= FALSE;
	m_fPpsh			= FALSE;
	m_fCon			= FALSE;
	m_fFile			= FALSE;

	_tcscpy(szLogFile, TEXT("NetLog.Log"));

	//--------------------------------------------------------------------
	//	Class resources
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	m_lpwstrLogBuffer = new WCHAR[sm_cnBuffer+1];
	if( NULL == m_lpwstrLogBuffer )
	{
			m_dwLastError = ERROR_NOT_ENOUGH_MEMORY;
			return FALSE;

	} 

	m_lpstrLogBuffer = new CHAR[sm_cnBuffer+1];
	if( NULL == m_lpwstrLogBuffer )
	{
			m_dwLastError = ERROR_NOT_ENOUGH_MEMORY;
			return FALSE;

	} 

	InitializeCriticalSection( &m_BufferCritSec );
	
	//--------------------------------------------------------------------
	//	Copy names
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	if (lpszName )
	{
		m_lpszThisName = new TCHAR[_tcslen(lpszName) + 1 ];
		if (NULL == m_lpszThisName ) 
		{
			m_dwLastError = ERROR_NOT_ENOUGH_MEMORY;
			return FALSE;
		}

		_tcscpy( m_lpszThisName, lpszName );

		if (_tcslen(m_lpszThisName) > MAX_PATH )
		{
			delete [] m_lpszThisName;
			m_lpszThisName = NULL;
			m_dwLastError = ERROR_INVALID_PARAMETER;
			return FALSE;
		}

		_tcscpy(szLogFile, m_lpszThisName );
		_tcscat(szLogFile, TEXT(".log"));

	} // end if (lpszName )


	if (lpszServer )
	{
		m_lpszServer = new TCHAR[_tcslen(lpszServer) + 1 ];
		if (NULL == m_lpszServer ) 
		{
			m_dwLastError = ERROR_NOT_ENOUGH_MEMORY;
			return FALSE;
		}

		_tcscpy(m_lpszServer, lpszServer);

	} // end if (lpszServer )

	//--------------------------------------------------------------------
	//	If Kato is requested initalize it.
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	if( (fType & NETLOG_KATO_OP) )
	{
		fRtn = LoadKato( );
		if( fRtn )
		{
			fRtn = sm_pKatoSetServerW( m_lpszServer );
			if( fRtn )
			{
				m_hKato = sm_pKatoCreateW( NULL );
				if( m_hKato )
				{
					m_fKato = TRUE;

				} 
				else if( NETLOG_KATO_RQ == (fType & NETLOG_KATO_RQ) )
				{
					DEBUGMSG( 1, (TEXT("FAIL @ %d: NetLog: Kato required\r\n"), __LINE__) );
					m_dwLastError = ERROR_BAD_COMMAND;
					return FALSE;

				} 

			} 
			else if( NETLOG_KATO_RQ == (fType & NETLOG_KATO_RQ) )
			{
				m_dwLastError = GetLastError();
				return FALSE;

			} 

		} 

		else if( NETLOG_KATO_RQ == (fType & NETLOG_KATO_RQ) ) 
		{
			DEBUGMSG( 1, (TEXT("FAIL @ %d: NetLog: Kato required\r\n"), __LINE__) );
			m_dwLastError = ERROR_BAD_COMMAND;
			return FALSE;

		} 

	}

    //--------------------------------------------------------------------
	//	If file is requested initalize it.
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	if( (fType & NETLOG_FILE_OP) )
	{
		m_hLogFile = CreateFile(szLogFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL ); 
		if(  INVALID_HANDLE_VALUE != m_hLogFile )
		{
			m_fFile = TRUE;
		} 

		else if( NETLOG_FILE_RQ == (fType & NETLOG_FILE_RQ) ) 
		{
			m_dwLastError = GetLastError();
			return FALSE;
		} 
	} 

	//--------------------------------------------------------------------
	//	Set debug if requested
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	if( (fType & NETLOG_DEBUG_OP) )
	{
		m_fDbg = TRUE;
	} 
	
	//--------------------------------------------------------------------
	//	Setup PPSH logging if requested
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#ifdef UNDER_CE
	if( (fType & NETLOG_PPSH_OP) )
	{
		m_iPPSHFile = U_ropen(szLogFile, _O_CREAT | _O_WRONLY | _O_SEQUENTIAL);
		
        if( -1 != m_iPPSHFile )
		{
			m_fPpsh = TRUE;
		}
		else if( NETLOG_FILE_RQ == (fType & NETLOG_FILE_RQ) )
		{
			m_dwLastError = GetLastError();
			return FALSE;
		}
	}
#else
	if( NETLOG_PPSH_RQ == (fType & NETLOG_PPSH_RQ) )
	{
		DEBUGMSG( 1, (TEXT("FAIL @ %d: NetLog: PPSH required\r\n"), __LINE__) );
		m_dwLastError = ERROR_BAD_COMMAND;
		return FALSE;

	} 
#endif // UNDER_CE

	//--------------------------------------------------------------------
	//	Setup Console logging if requested (disabled if console not present)
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#ifdef NETLOG_CONSOLE_ENABLED
	if( (fType & NETLOG_CON_OP) )
    {
		m_fCon = TRUE;
        
        //
        //  Try to use the console, if it does not exist we will get an exception and disable console output.
        //
        __try {
            _tprintf(_T("\r\n"));
        } __except(1)
        {
            m_fCon = FALSE;
        }
    }
#endif

	//--------------------------------------------------------------------
	//	Log the creation of the file.
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	GetLocalTime( &SystemTime );
	Log(LOG_DETAIL, TEXT("Log created at %02d:%02d:%02d.%02d on %02d/%02d/%04d\r\n"), 
		 SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond, SystemTime.wMilliseconds,
		 SystemTime.wMonth, SystemTime.wDay, SystemTime.wYear );

	return TRUE;
} 


CNetLog::~CNetLog( )
{
	SYSTEMTIME	SystemTime;
	//--------------------------------------------------------------------
	//	Close handle 
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	GetLocalTime( &SystemTime );
	Log(LOG_DETAIL, TEXT("Log ended at %02d:%02d:%02d.%02d on %02d/%02d/%04d\r\n"), 
		 SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond, SystemTime.wMilliseconds,
		 SystemTime.wMonth, SystemTime.wDay, SystemTime.wYear );

	if( m_fKato	) sm_pKatoDestroy( m_hKato );
#ifdef UNDER_CE
	if( m_fPpsh ) U_rclose( m_iPPSHFile );
#endif // UNDER_CE
	if( m_fFile	) CloseHandle( m_hLogFile );

	UnloadKato( );


	EnterCriticalSection( &m_BufferCritSec );

	if(m_lpwstrLogBuffer)
		delete [] m_lpwstrLogBuffer;

	if(m_lpszThisName)
		delete [] m_lpszThisName;

	if(m_lpszServer)
		delete [] m_lpszServer;

	if(m_lpstrLogBuffer)
		delete [] m_lpstrLogBuffer;

	m_lpwstrLogBuffer = NULL;
	m_lpszThisName = NULL;
	m_lpszServer = NULL;
	m_lpstrLogBuffer = NULL;

	LeaveCriticalSection( &m_BufferCritSec );

	DeleteCriticalSection( &m_BufferCritSec );

}

BOOL CNetLog::WriteLog()
{
	LPTSTR ptszLogBuffer = NULL;
	int nBytes, nBytesASCII;
	DWORD dwRtn;
	BOOL fRtn = TRUE;

	nBytes = wcslen(m_lpwstrLogBuffer) * sizeof(WCHAR);
	nBytesASCII = strlen(m_lpstrLogBuffer) * sizeof(char);

#ifdef UNICODE
	ptszLogBuffer = m_lpwstrLogBuffer;
#else
	ptszLogBuffer = m_lpstrLogBuffer;
#endif

	if( m_fKato )
		fRtn = fRtn && sm_pKatoLogW( m_hKato, m_dwVerbosity, ptszLogBuffer );

	if( m_fDbg )
		OutputDebugString( ptszLogBuffer );

	if( m_fCon )
	{
		__try
		{
#ifdef UNDER_CE
			fRtn = fRtn && ( _fputts( ptszLogBuffer, stdout ) >= 0);
#else
			fRtn = fRtn && ( fputs( m_lpstrLogBuffer, stdout ) >= 0);
#endif
		}
		__except(1)
		{
			if(!m_fDbg)
				OutputDebugString( ptszLogBuffer );
		}
	}

#ifdef UNDER_CE
	if( m_fPpsh )
	{
		fRtn = fRtn && (U_rwrite(m_iPPSHFile, (LPBYTE)m_lpstrLogBuffer, nBytesASCII) != (int)nBytesASCII );
	}
#endif

	if( m_fFile )
	{
		fRtn = fRtn && WriteFile( m_hLogFile, m_lpstrLogBuffer, nBytesASCII, &dwRtn, NULL );
		fRtn = fRtn && ( dwRtn == (DWORD)nBytesASCII );
	}

	if( !fRtn )
		m_dwLastError = GetLastError();

	return fRtn;
}


BOOL WINAPIV CNetLog::LogV(DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs)
{
	BOOL		fRtn = TRUE;
	DWORD		dwRtn = 0;
	int			nBytes = 0;
	SYSTEMTIME	SystemTime;

	if( dwVerbosity > m_dwVerbosity ) return TRUE;
	GetLocalTime( &SystemTime );

	EnterCriticalSection( &m_BufferCritSec );

	nBytes =  swprintf( m_lpwstrLogBuffer, L"%02d:%02d:%02d %08X: ",
		SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond, GetCurrentThreadId() );
	nBytes +=  vswprintf( &m_lpwstrLogBuffer[nBytes], wszFormat, pArgs );
	nBytes *= sizeof( WCHAR );

	WideCharToMultiByte( CP_ACP, 0, m_lpwstrLogBuffer, -1, m_lpstrLogBuffer, sm_cnBuffer, NULL, NULL );
	m_lpstrLogBuffer[sm_cnBuffer] = '\0';

	if( 0 == nBytes ) {
		return FALSE;
	}

	WriteLog();

	LeaveCriticalSection( &m_BufferCritSec );

	return fRtn;

} // end BOOL WINAPIV CNetLog::LogV(DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs)


BOOL WINAPIV CNetLog::LogV(DWORD dwVerbosity, LPCSTR szFormat, va_list pArgs)
{
	BOOL		fRtn = TRUE;
	DWORD		dwRtn = 0;
	int			nBytes = 0;
	SYSTEMTIME	SystemTime;

	if( dwVerbosity > m_dwVerbosity ) return TRUE;
	GetLocalTime( &SystemTime );

	EnterCriticalSection( &m_BufferCritSec );

	nBytes =  sprintf( m_lpstrLogBuffer, "%02d:%02d:%02d %08X: ",
		SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond, GetCurrentThreadId() );
	nBytes +=  vsprintf( &m_lpstrLogBuffer[nBytes], szFormat, pArgs );

	MultiByteToWideChar(CP_ACP, 0, m_lpstrLogBuffer, -1, m_lpwstrLogBuffer, sm_cnBuffer);
	m_lpwstrLogBuffer[sm_cnBuffer] = L'\0';

	if( nBytes == 0 )
		return FALSE;

	WriteLog();

	LeaveCriticalSection( &m_BufferCritSec );

	return fRtn;
} // end BOOL WINAPIV CNetLog::LogV(DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs)


BOOL WINAPIV CNetLog::Log (DWORD dwVerbosity, LPCWSTR wszFormat, ...)
{
	BOOL		fRtn = TRUE;
	va_list		pArgs;

	va_start( pArgs, wszFormat );
	fRtn = LogV(dwVerbosity, wszFormat, pArgs);
	va_end( pArgs );

	return fRtn;

} // end BOOL CNetLog::WINAPIV Log (DWORD dwVerbosity, LPCWSTR wszFormat, ...)


BOOL WINAPIV CNetLog::Log (DWORD dwVerbosity, LPCSTR szFormat, ...)
{	
	BOOL		fRtn = TRUE;
	va_list		pArgs;

	va_start( pArgs, szFormat );
	fRtn = LogV(dwVerbosity, szFormat, pArgs);
	va_end( pArgs );

	return fRtn;
} // end BOOL CNetLog::WINAPIV Log (DWORD dwVerbosity, LPCWSTR wszFormat, ...)


BOOL CNetLog::Flush( void )
{
	if( m_fFile ) FlushFileBuffers( m_hLogFile ); 	
#ifndef UNDER_CE
	fflush( stdout );
#endif // NOT UNDER_CE
	return TRUE;

} // end BOOL CNetLog::Flush( DWORD fFlushType )

extern "C"
HQANET NetLogCreateW(LPCTSTR lpszName, DWORD fType, LPCTSTR lpszServer )
{

	CNetLog	*pNetLog;

	pNetLog = new CNetLog(lpszName, fType, lpszServer );
	if( NULL == pNetLog )
	{
		SetLastError( ERROR_NOT_ENOUGH_MEMORY );
		return NULL;
	}

	if( pNetLog->GetLastError() )
	{
		DEBUGMSG( 1, (TEXT("NetLog::GetLastError() == %d\r\n"), pNetLog->GetLastError()) );
		SetLastError( pNetLog->GetLastError() );
		delete pNetLog;
		return NULL;

	}  // end if( pNetLog->GetLastError() )

	return (HQANET) pNetLog;

} // end HQANET NetLogCreateW( LPCWSTR wszName, DWORD fType, LPCWSTR wszServer );

extern "C"
BOOL WINAPIV NetLogW( HQANET hQaNet, DWORD dwVerbosity, LPCWSTR wszFormat, ... )
{
	CNetLog	*pNetLog = NULL;
	va_list		pArgs;
	BOOL		fRtn;

	pNetLog = (CNetLog *)hQaNet;

	va_start( pArgs, wszFormat );
	__try {
	fRtn = pNetLog->LogV( dwVerbosity, wszFormat, pArgs );
	SetLastError( pNetLog->GetLastError() );
	} __except( GetExceptionCode() == STATUS_ACCESS_VIOLATION ) {

	va_end( pArgs );
	SetLastError( ERROR_INVALID_HANDLE );
	return FALSE;

	} // end __try-__except
	va_end( pArgs );

	return fRtn;

} // end BOOL WINAPIV NetLogW( HQANET hQaNet, DWORD dwVerbosity, LPCWSTR wszFormat, ... )

extern "C"
BOOL WINAPIV NetLogA( HQANET hQaNet, DWORD dwVerbosity, LPCSTR wszFormat, ... )
{
	CNetLog	*pNetLog = NULL;
	va_list		pArgs;
	BOOL		fRtn;

	pNetLog = (CNetLog *)hQaNet;

	va_start( pArgs, wszFormat );
	__try {
	fRtn = pNetLog->LogV( dwVerbosity, wszFormat, pArgs );
	SetLastError( pNetLog->GetLastError() );
	} __except( GetExceptionCode() == STATUS_ACCESS_VIOLATION ) {

	va_end( pArgs );
	SetLastError( ERROR_INVALID_HANDLE );
	return FALSE;

	} // end __try-__except
	va_end( pArgs );

	return fRtn;

} // end BOOL WINAPIV NetLogA( HQANET hQaNet, DWORD dwVerbosity, LPCWSTR wszFormat, ... )


extern "C"
BOOL WINAPIV NetLogVW( HQANET hQaNet, DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs )
{

	CNetLog	*pNetLog = NULL;
	BOOL		fRtn;

	pNetLog = (CNetLog *)hQaNet;

	__try {
	fRtn = pNetLog->LogV( dwVerbosity, wszFormat, pArgs );
	SetLastError( pNetLog->GetLastError() );
	} __except( GetExceptionCode() == STATUS_ACCESS_VIOLATION ) {

	SetLastError( ERROR_INVALID_HANDLE );
	return FALSE;

	} // end __try-__except

	return fRtn;

} // BOOL WINAPIV NetLogVW( HQANET hQaNet, DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs )

extern "C"
BOOL NetLogFlush(  HQANET hQaNet, DWORD fFlushType )
{
	CNetLog	*pNetLog = NULL;
	BOOL		fRtn;

	pNetLog = (CNetLog *)hQaNet;
	__try {
	fRtn = pNetLog->Flush( );
	SetLastError( pNetLog->GetLastError() );
	} __except( GetExceptionCode() == STATUS_ACCESS_VIOLATION ) {

	SetLastError( ERROR_INVALID_HANDLE );
	return FALSE;

	} // end __try-__except

	return fRtn;

} // end BOOL NetFlush(  HQANET hQaNet, DWORD fFlushType )

extern "C"
BOOL NetLogSetMaxVerbosity(  HQANET hQaNet, DWORD dwVerbosity )
{
	CNetLog	*pNetLog = (CNetLog *)hQaNet;

	__try {
	pNetLog->SetMaxVerbosity( dwVerbosity );
	SetLastError( pNetLog->GetLastError() );
	} __except( GetExceptionCode() == STATUS_ACCESS_VIOLATION ) {

	SetLastError( ERROR_INVALID_HANDLE );
	return FALSE;

	} // end __try-__except

	return TRUE;

}  // end BOOL NetLogSetMaxVerbosity(  HQANET hQaNet, DWORD dwVerbosity )

extern "C"
BOOL NetLogDestroy( HQANET hQaNet )
{
	CNetLog	*pNetLog = (CNetLog *)hQaNet;

	__try {
	pNetLog->GetLastError( );
	} __except( GetExceptionCode() == STATUS_ACCESS_VIOLATION ) {

	SetLastError( ERROR_INVALID_HANDLE );
	return FALSE;

	} // end __try-__except

	delete pNetLog;

	return TRUE;

} // end BOOL NetLogDestroy( HQANET hQanet )
