/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Copyright (c) 1996-1999 Microsoft Corporation

Module Name:
    NetLog.h

Abstract:
    Declaration of the NetLog Functions and Class.


    David J. Simons (dsimons) 23-Sep-1997

Modified:
    Doug Hines (dhines) 31-April-1999 - Added comments
	Michael Albee (a-malb) 8-Oct-1999 - Revised for NTANSI compatibility

-------------------------------------------------------------------*/
#ifndef __NETLOG_H__
#define __NETLOG_H__

#include <windows.h>
#ifndef UNDER_CE
#include <stdio.h>
#include <tchar.h>
#define DEBUGMSG( expr, p ) ((expr) ? _tprintf p, 1 : 0)
#endif

//------------------------------------------------------------------------
//	Object creation flags.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define NETLOG_DEBUG_OP	0x00000001  // Optional Debug output
#define NETLOG_DEBUG_RQ	0x00000003  // Required Debug output
#define NETLOG_KATO_OP	0x00000004  // Optional Kato output
#define NETLOG_KATO_RQ	0x0000000C  // Required Kato output
#define NETLOG_PPSH_OP	0x00000010  // Optional PPSH output
#define NETLOG_PPSH_RQ	0x00000030  // Required PPSH output
#define NETLOG_FILE_OP	0x00000040  // Optional file output
#define NETLOG_FILE_RQ	0x000000C0  // Required file output
#define NETLOG_CON_OP		0x00000100  // Optional console logging
#define NETLOG_CON_RQ		0x00000300  // Required console logging
#define NETLOG_DRV_OP		0x00000400
#define NETLOG_DRV_RQ		0x00000C00
#define NETLOG_DEF		(NETLOG_DEBUG_RQ | NETLOG_CON_OP | NETLOG_KATO_OP)

#ifdef __cplusplus

//------------------------------------------------------------------------
//	The Login class
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class CNetLog {
public:
	// Overload new and delete to prevent mismatched heaps (KB:Q122675)
	//void* __cdecl operator new(size_t stAllocate);
	//void  __cdecl operator delete(void *pvMemory);

	//--------------------------------------------------------------------
	//	Constructors and destructors
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	CNetLog( LPCTSTR lpszName = NULL )                                    { Construct( lpszName, NETLOG_DEF, NULL ); } 
	CNetLog( LPCTSTR lpszName, LPCTSTR lpszServer = NULL )                { Construct( lpszName, NETLOG_DEF, lpszServer ); } 
	CNetLog( DWORD fType, LPCTSTR lpszServer = NULL )                     { Construct( NULL, fType, lpszServer ); } 
	CNetLog( LPCTSTR lpszName, DWORD fType, LPCTSTR lpszServer = NULL )   { Construct( lpszName, fType, lpszServer ); } 

	~CNetLog( );

	DWORD GetLastError( void ) { return m_dwLastError; }
	
	// Unicode functions
	BOOL SetServer( LPCWSTR wszServer );
	BOOL WINAPIV Log (DWORD dwVerbosity, LPCWSTR wszFormat, ...);
	BOOL WINAPIV Log (DWORD dwVerbosity, LPCSTR wszFormat, ...);
	
	BOOL WINAPIV LogV(DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs);
	BOOL WINAPIV LogV(DWORD dwVerbosity, LPCSTR wszFormat, va_list pArgs);

	BOOL Flush( void );
	void SetMaxVerbosity( DWORD dwVerbosity ) { m_dwVerbosity = dwVerbosity; }

private:

	DWORD m_dwLastError;
	//--------------------------------------------------------------------
	//	The real constructor;
	//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	Construct( LPCWSTR wszName, DWORD fType, LPCWSTR wszServer = NULL );
	Construct( LPCSTR wszName, DWORD fType, LPCSTR wszServer = NULL );
	
	BOOL WriteTLog(LPTSTR wszLog, DWORD nBytes );
	BOOL WriteLog( LPSTR szLog, DWORD nBytes );

	LPTSTR	m_lpszThisName;
	LPTSTR	m_lpszServer;
	BOOL	m_fDbg;
	BOOL	m_fKato;
	BOOL	m_fPpsh;
	BOOL	m_fCon;
	BOOL	m_fFile;

	CRITICAL_SECTION	m_BufferCritSec;
	LPWSTR				m_lpwstrLogBuffer;
	LPSTR				m_lpstrLogBuffer;
	
	DWORD	m_dwVerbosity;

	HANDLE	m_hLogFile;
	int		m_iPPSHFile;
	HANDLE	m_hKato;

	// Dynamic Kato loading static members
	static  const DWORD		sm_cnBuffer;
	static  HINSTANCE		sm_hKatoDll;
	static  BOOL			LoadKato( void );
	static  BOOL			UnloadKato( void );
	static	DWORD			sm_nKatoUse;
	static	BOOL 			(WINAPI *sm_pKatoSetServerW)( LPCTSTR );
	static	BOOL 			(WINAPI *sm_pKatoFlush)( DWORD );
	static	HANDLE		 	(WINAPI *sm_pKatoCreateW)( LPCTSTR );
	static	BOOL			(WINAPI *sm_pKatoDestroy)( HANDLE );
	static	BOOL			(WINAPIV *sm_pKatoLogW)( HANDLE , DWORD , LPCTSTR , ...);

};  // end CNetLog

#endif 

//------------------------------------------------------------------------
//	C Interface 
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
typedef HANDLE HQANET;

#ifdef __cplusplus
extern "C" {
#endif

HQANET NetLogCreateW( LPCTSTR lpszName, DWORD fType, LPCTSTR lpszServer );
BOOL WINAPIV NetLogW( HQANET hQaNet, DWORD dwVerbosity, LPCWSTR wszFormat, ... );
BOOL WINAPIV NetLogVW( HQANET hQaNet, DWORD dwVerbosity, LPCWSTR wszFormat, va_list pArgs );
BOOL WINAPIV NetLogA( HQANET hQaNet, DWORD dwVerbosity, LPCSTR wszFormat, ... );	

BOOL NetLogFlush(  HQANET hQaNet, DWORD fFlushType );
BOOL NetLogSetMaxVerbosity(  HQANET hQaNet, DWORD dwVerbosity );

BOOL NetLogDestroy( HQANET hQaNet );

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------
//	Log verbosity flags
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#ifndef __KATOEX_H__
#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14
#endif

#define LOG_WARNING            3
#define LOG_MAX_VERBOSITY     15
#define LOG_DEFAULT           13

#endif // __NETLOG_H__
