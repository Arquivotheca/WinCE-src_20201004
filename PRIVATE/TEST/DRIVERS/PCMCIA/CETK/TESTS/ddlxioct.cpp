//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************


// ----------------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------------

#include "TestMain.h"
#include "ddlxioct.h"
#include <msgqueue.h>

// ----------------------------------------------------------------------------
// Command line support
// ----------------------------------------------------------------------------

TCHAR glpCommandLines[10][256] = { 0 };

//CRITICAL_SECTION gcsCritSect;

typedef struct _EX_STRUCT
{
	DWORD dwOpenData;
	DWORD dwNotOpenData;
}
EX_STRUCT, *LPEX_STRUCT;

// ----------------------------------------------------------------------------
// Function Prototypes
// ----------------------------------------------------------------------------

BOOL ProcessShellCommand( DWORD dwOpenData,		// handle identifying the open context of the device
						  UINT  uMsg, 			// shell message
						  PBYTE pBufIn,			// input buffer
						  DWORD dwLenIn,		// input buffer length
						  PBYTE pBufOut,		// output buffer
						  DWORD dwLenOut, 		// output buffer length
						  PDWORD pdwActualOut);	// actual output length

BOOL ProcessTestCommand( DWORD dwOpenData,		// handle identifying the open context of the device
						 UINT  uMsg, 			// test message
						 PBYTE pBufIn,			// input buffer
						 DWORD dwLenIn,			// input buffer length
						 PBYTE pBufOut,			// output buffer
						 DWORD dwLenOut, 		// output buffer length
						 PDWORD pdwActualOut);	// actual output length

void DumpFunctionTableEntry ( LPFUNCTION_TABLE_ENTRY );
LPCTSTR ShellCmdToString( UINT );
LPCTSTR TestCmdToString( UINT );

SHELLPROCAPI ShellProc(UINT, SPPARAM);

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

static const LPTSTR	cszThisFile = TEXT("DRVIOCTL.CPP");
static const LPTSTR	cszThisTest = TEXT("Generic Device Driver Loader Client");

static int g_iTblSize;

/*// ----------------------------------------------------------------------------
// Debug zone support
// ----------------------------------------------------------------------------

#define DDLXDEBUGZONE(n)  (dpPrivCurSettings.ulZoneMask&(0x00000001<<n))

//
// These zones are shared between ddlxdll and ddlxioct, so don't change one
// without changing the other.
//
#define ZONE_ERROR      DDLXDEBUGZONE(0)
#define ZONE_WARNING    DDLXDEBUGZONE(1)
#define ZONE_FUNCTION   DDLXDEBUGZONE(2)
#define ZONE_INIT       DDLXDEBUGZONE(3)
#define ZONE_VERBOSE    DDLXDEBUGZONE(4)
#define ZONE_IOCTL      DDLXDEBUGZONE(5)

#define DBG_ERROR      1
#define DBG_WARNING    2
#define DBG_FUNCTION   4
#define DBG_INIT       8
#define DBG_VERBOSE   16
#define DBG_IOCTL     32

DBGPARAM dpPrivCurSettings =
{
    TEXT("DDLX IOCTL"),
	{
    TEXT("Errors"),TEXT("Warnings"),TEXT("Functions"),TEXT("Initialization"),
    TEXT("Verbose"),TEXT("Ioctl"),TEXT("Undefined"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined")
	},
    DBG_ERROR | DBG_INIT|DBG_VERBOSE | 0xFFFFFFFF
};
*/
// ----------------------------------------------------------------------------
//
// Note: All required entry points are represented, but not all are fully
//       implemented.
//
// ----------------------------------------------------------------------------
CRITICAL_SECTION * pcsCritSect=NULL;

BOOL 
WINAPI DdlxDLLEntry(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) 
{
    switch (dwReason) {
      case DLL_PROCESS_ATTACH:
		pcsCritSect = new CRITICAL_SECTION;
		if (pcsCritSect)
			InitializeCriticalSection( pcsCritSect );
		break;
      case DLL_PROCESS_DETACH:
		if (pcsCritSect) {
			DeleteCriticalSection( pcsCritSect );
			delete pcsCritSect;
			pcsCritSect=NULL;
		};
      default:
		break;
    };
	return TRUE;
};

// ----------------------------------------------------------------------------
// gen_Init()
// ----------------------------------------------------------------------------

DWORD gen_Init (DWORD Index)
{
	static int i = 1;

//	InitializeCriticalSection( & gcsCritSect );
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("%s: gen_Init called...\r\n"),
		cszThisFile));

	return i++;
}

// ----------------------------------------------------------------------------
// gen_Deinit()
// ----------------------------------------------------------------------------

BOOL gen_Deinit(DWORD dwData)
{
//	DeleteCriticalSection( & gcsCritSect );
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("%s: gen_Deinit called...\r\n"),
		cszThisFile));
	return TRUE;
}

// ----------------------------------------------------------------------------
// gen_Open()
// ----------------------------------------------------------------------------

DWORD gen_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode)
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("%s: gen_Open called...\r\n"),
		cszThisFile));
	if (pcsCritSect)
		EnterCriticalSection( pcsCritSect );

	int i;

	for( i = 1; i < 10; i++ )
	{
		if( glpCommandLines[i][0] == 0 )
		{
			// This is the first available slot. Take it and mark the entry as
			// being in use.
			memset( glpCommandLines[i], 0, 256 );
			glpCommandLines[i][0] = 1;
			break;
		}
	}
	if (pcsCritSect)
		LeaveCriticalSection( pcsCritSect );

	return i;
}

// ----------------------------------------------------------------------------
// gen_Close()
// ----------------------------------------------------------------------------

BOOL gen_Close (DWORD dwData)
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("%s: gen_Close called...\r\n"),
		cszThisFile));
	if(g_pKato){
		g_pKato->Stop();
		delete g_pKato;
	}

	if (pcsCritSect)
		EnterCriticalSection( pcsCritSect );
	// Mark our 'slot' as being available again.
	glpCommandLines[dwData][0] = 0;
	if (pcsCritSect)
		LeaveCriticalSection( pcsCritSect );

	return TRUE;
}

// ----------------------------------------------------------------------------
// gen_Read()
// ----------------------------------------------------------------------------

DWORD gen_Read (DWORD dwData, LPVOID pBuf, DWORD Len)
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("%s: gen_Read called...\r\n"),
		cszThisFile));
	return 0;
}

// ----------------------------------------------------------------------------
// gen_Write()
// ----------------------------------------------------------------------------

DWORD gen_Write (DWORD dwData, LPCVOID pBuf, DWORD Len)
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("%s: gen_Write called...\r\n"),
		cszThisFile));
	return 0;
}

// ----------------------------------------------------------------------------
// gen_Seek()
// ----------------------------------------------------------------------------

DWORD gen_Seek (DWORD dwData, long pos, DWORD type)
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("%s: gen_Seek called...\r\n"),
		cszThisFile));
	return 0;
}

// ----------------------------------------------------------------------------
// gen_PowerUp()
// ----------------------------------------------------------------------------

void gen_PowerUp(DWORD dwData)
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("%s: gen_PowerUp called...\r\n"),
		cszThisFile));
	return;
}


// ----------------------------------------------------------------------------
// gen_PowerDown()
// ----------------------------------------------------------------------------

void gen_PowerDown(DWORD dwData)
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("%s: gen_PowerDown called...\r\n"),
		cszThisFile));
	return;
}

// ----------------------------------------------------------------------------
// gen_IOControl()
// ----------------------------------------------------------------------------

BOOL
gen_IOControl( DWORD dwOpenData,		// handle identifying the open context of the device
			   DWORD dwCode,			// command code and uMsg
			   PBYTE pBufIn,			// input buffer
			   DWORD dwLenIn,			// input buffer length
			   PBYTE pBufOut,			// output buffer
			   DWORD dwLenOut,			// output buffer length
			   PDWORD pdwActualOut) {	// actual output length

	BOOL fRet = FALSE;
		
	// Extract the i/o control code and message from dwCode
	UINT uCmd = LOWORD(dwCode);
	UINT uMsg = HIWORD(dwCode);

	switch( uCmd )
	{
	case DRV_IOCTL_CMD_SHELL:
		
		DEBUGMSG(ZONE_IOCTL, (TEXT("DRV_IOCTL_CMD_SHELL: uMsg=%s\r\n"),
			ShellCmdToString( uMsg )));
		fRet = ProcessShellCommand( dwOpenData, uMsg, pBufIn, dwLenIn, pBufOut, dwLenOut, pdwActualOut);
		break;

	case DRV_IOCTL_CMD_TEST:
		NKDbgPrintfW(TEXT("DRV_IOCTL_CMD_TEST: uMsg=%s\r\n"),
			TestCmdToString( uMsg ));
		
		DEBUGMSG(ZONE_IOCTL, (TEXT("DRV_IOCTL_CMD_TEST: uMsg=%s\r\n"),
			TestCmdToString( uMsg )));
		fRet = ProcessTestCommand( dwOpenData, uMsg, pBufIn, dwLenIn, pBufOut, dwLenOut, pdwActualOut);
		break;

	case DRV_IOCTL_CMD_PING:
		
		DEBUGMSG(ZONE_IOCTL, (TEXT("DRV_IOCTL_CMD_PING called\r\n")));

		g_pKato = NULL;
		g_pKato = new DDLXKato_Talk(TRUE);
		ASSERT(g_pKato);
		
		if(g_pKato->Init() == FALSE){
			NKDbgPrintfW(_T("Debug output message won't be able to redirect to a file!"));
		}

		fRet = TRUE;
		break;

	case DRV_IOCTL_CMD_DEBUG:

		DEBUGMSG(ZONE_IOCTL, (TEXT("DRV_IOCTL_CMD_DEBUG called\r\n")));
		
		// Not going to return the value of the current ddlxioct debug zone
		// mask, since it should be the same as the mask in ddlx.
		//dpPrivCurSettings.ulZoneMask = dwLenIn;
		fRet = TRUE;
		break;

	default:
		DEBUGMSG(ZONE_IOCTL, (TEXT("IOControl UNKNOWN, FAILED.\r\n")));
		break;
	}

	return fRet;
}

// ----------------------------------------------------------------------------
// ProcessShellCommand
// ----------------------------------------------------------------------------

BOOL
ProcessShellCommand( DWORD dwOpenData,			// handle identifying the open context of the device
					 UINT uMsg,					// shell message
					 PBYTE pBufIn,				// input buffer
					 DWORD dwLenIn,				// input buffer length
					 PBYTE pBufOut,				// output buffer
					 DWORD dwLenOut,			// output buffer length
					 PDWORD pdwActualOut)		// actual output length
{
	static SPS_REGISTER	spsRegister;

	DWORD dwRet = SPR_HANDLED;
	BOOL fRet  = TRUE;

	switch (uMsg)
	{
	case SPM_LOAD_DLL:

		// Originally used for setting Unicode flag, but not really useful
		// at this point.
		SPS_LOAD_DLL spsLoadDll;

		dwRet = ShellProc( SPM_LOAD_DLL, (DWORD) &spsLoadDll );

		break;

	case SPM_UNLOAD_DLL:

		dwRet = ShellProc( SPM_UNLOAD_DLL, NULL );
		break;

	case SPM_SHELL_INFO:

		static	SPL_SHELL_INFO	IoctlShellInfo;

		//
		// Copy the command line to our (ddlxioct) table containing command lines.
		//
		if( ((LPSPI_SHELL_INFO)pBufIn)->tDllCmdLine &&
			((LPSPI_SHELL_INFO)pBufIn)->tDllCmdLine[0] )
		{
			_tcscpy( (glpCommandLines[dwOpenData])+1, ((LPSPI_SHELL_INFO)pBufIn)->tDllCmdLine );
			DEBUGMSG(ZONE_VERBOSE, (TEXT("Command line: %s\r\n"), glpCommandLines[dwOpenData]+1));
		}
		else
		{
			DEBUGMSG(ZONE_VERBOSE, (TEXT("No command line.\r\n")));
		}

		if( !pBufIn )
		{
			DEBUGMSG(ZONE_FUNCTION, (TEXT("IOControl, SHELLPROC, SPM_SHELL_INFO pBufIn==NULL\r\n")));
			break;
		}

		IoctlShellInfo.PassedShellInfo = *((LPSPI_SHELL_INFO)pBufIn);
		IoctlShellInfo.LocalShellInfo.hInstance 	= NULL;
		IoctlShellInfo.LocalShellInfo.hWnd			= NULL;
		IoctlShellInfo.LocalShellInfo.hLib			= NULL;
		IoctlShellInfo.LocalShellInfo.hevmTerminate = NULL;
		IoctlShellInfo.LocalShellInfo.fUsingServer	= IoctlShellInfo.PassedShellInfo.fUsingServer;
		IoctlShellInfo.LocalShellInfo.szDllCmdLine	= IoctlShellInfo.PassedShellInfo.tDllCmdLine;
        //Modified to take the command line parameters.
		dwRet = ShellProc( SPM_SHELL_INFO, (DWORD) &IoctlShellInfo.LocalShellInfo );

		break;

	case SPM_REGISTER:

		LPFUNCTION_TABLE_ENTRY lpFTEFrom;
		LPFUNCTION_TABLE_ENTRY lpFTETo;
		LPCTSTR lpDescFrom;
		LPTSTR lpDescTo;
		int ndx;

		dwRet = ShellProc( SPM_REGISTER, (DWORD)&spsRegister );

		if ( !pBufIn )
		{
			DEBUGMSG(ZONE_FUNCTION, (TEXT("ERROR: gen_IOControl, SHELLPROC, SPM_REGISTER pBufIn==NULL\r\n")));
			break;
		}

		// First, copy the function table over from device.exe space to ddlx.exe
		// space. (LPFUNCTION_TABLE_ENTRY)pBufIn points to the 16K function table
		// in the tux.exe memory space.
		lpFTEFrom = spsRegister.lpFunctionTable;
		lpFTETo = (LPFUNCTION_TABLE_ENTRY)pBufIn;
		g_iTblSize = 0;
		
		do
		{
			DumpFunctionTableEntry( lpFTEFrom );
		
			*lpFTETo = *lpFTEFrom;
			lpFTETo++;
			g_iTblSize++;
		}
		while( (lpFTEFrom++)->lpDescription );

		// Then, walk along and copy and fix function description strings
		lpDescTo = (LPTSTR) lpFTETo;
		lpFTETo  = (LPFUNCTION_TABLE_ENTRY)pBufIn;
		
		for( ndx=0; ndx < g_iTblSize; ndx++ ) {

			lpDescFrom = spsRegister.lpFunctionTable[ndx].lpDescription;
			
			if( !lpDescFrom )
			{
				break;
			}
			
			lpFTETo[ndx].lpDescription = lpDescTo;
			
			do
			{
				*lpDescTo = *lpDescFrom;
				lpDescTo++;
			}
			while ( *lpDescFrom++ );
			
			DumpFunctionTableEntry( & lpFTETo[ndx] );
		}

		break;

	case SPM_START_SCRIPT:

		dwRet = ShellProc( SPM_START_SCRIPT, NULL );
		break;

	case SPM_STOP_SCRIPT:

		dwRet = ShellProc( SPM_STOP_SCRIPT, NULL );
		break;

	case SPM_BEGIN_GROUP:

		dwRet = ShellProc( SPM_BEGIN_GROUP, NULL );
		break;

	case SPM_END_GROUP:

		dwRet = ShellProc( SPM_END_GROUP, NULL );
		break;

	case SPM_BEGIN_TEST:
	{
		LPSPS_BEGIN_TEST  lpBeginTest;

		lpBeginTest = (LPSPS_BEGIN_TEST)pBufIn;
		
		if( !lpBeginTest )
		{
			DEBUGMSG(ZONE_FUNCTION, (TEXT("IOControl, SHELLPROC, SPM_BEGIN_TEST pBufIn==NULL\r\n")));
			dwRet = SPR_NOT_HANDLED;
			break;
		}

		if( (int)dwLenIn < 0 )
		{
			DEBUGMSG(ZONE_FUNCTION, (TEXT("IOControl, SHELLPROC, SPM_BEGIN_TEST dwLenIn < 0\r\n")));
			dwRet = SPR_NOT_HANDLED;
			break;
		}

		if( (int)dwLenIn > g_iTblSize )
		{
			DEBUGMSG(ZONE_FUNCTION, (TEXT("IOControl, SHELLPROC, SPM_BEGIN_TEST dwLenIn > iTblSize\r\n")));
			dwRet = SPR_NOT_HANDLED;
			break;
		}

		lpBeginTest->lpFTE = & spsRegister.lpFunctionTable[dwLenIn];
		dwRet = ShellProc( SPM_BEGIN_TEST, (DWORD)lpBeginTest );

		break;
	}

	case SPM_END_TEST:
	{
		LPSPS_END_TEST lpEndTest;
		lpEndTest = (LPSPS_END_TEST)pBufIn;

		if( !lpEndTest )
		{
			DEBUGMSG(ZONE_FUNCTION, (TEXT("IOControl, SHELLPROC, SPM_END_TEST pBufIn==NULL\r\n")));
			dwRet = SPR_NOT_HANDLED;
			break;
		}
		
		if( (int)dwLenIn < 0 )
		{
			DEBUGMSG(ZONE_FUNCTION, (TEXT("IOControl, SHELLPROC, SPM_END_TEST dwLenIn < 0\r\n")));
			dwRet = SPR_NOT_HANDLED;
			break;
		}

		if( (int)dwLenIn > g_iTblSize )
		{
			DEBUGMSG(ZONE_FUNCTION, (TEXT("IOControl, SHELLPROC, SPM_END_TEST dwLenIn > g_iTblSize\r\n")));
			dwRet = SPR_NOT_HANDLED;
			break;
		}

		lpEndTest->lpFTE = & spsRegister.lpFunctionTable[dwLenIn];
		dwRet = ShellProc( SPM_END_TEST, (DWORD)lpEndTest );

		break;
	}

	case SPM_EXCEPTION:

		dwRet = SPR_NOT_HANDLED;
		break;

	default:

		dwRet = SPR_NOT_HANDLED;
		break;
	}

	// Return code is returned in pdwActualOut (if possible)
	if( pdwActualOut )
	{
		*pdwActualOut = dwRet;
	}

	return fRet;
}

// ----------------------------------------------------------------------------
// ProcessTestCommand
// ----------------------------------------------------------------------------

BOOL
ProcessTestCommand(	DWORD dwOpenData,		// handle identifying the open context of the device
				    UINT  uMsg, 			// test message
				   	PBYTE pBufIn,			// input buffer
					DWORD dwLenIn,			// input buffer length
					PBYTE pBufOut,			// output buffer
					DWORD dwLenOut, 		// output buffer length
					PDWORD pdwActualOut) {	// actual output length

	BOOL fRet = TRUE;
	BOOL fException = FALSE;
	DWORD	dwRet = TPR_PASS;
	DWORD dwException = 0;
	DWORD dwStructSize;
	LPFUNCTION_TABLE_ENTRY	lpFTE;
	LPEX_STRUCT lpvNewStructure;

	switch( uMsg )
	{
	case TPM_QUERY_THREAD_COUNT:
		dwStructSize = sizeof(TPS_QUERY_THREAD_COUNT);
		break;
	case TPM_EXECUTE:
		dwStructSize = sizeof(TPS_EXECUTE);
		break;
	default:
		DEBUGMSG(ZONE_WARNING, (TEXT("IOControl, TESTPROC, uMsg=0x%X (not handled).\r\n")));
		return FALSE;
	}

	if( (int)dwLenIn < 0 )
	{
		DEBUGMSG(ZONE_WARNING, (TEXT("IOControl, TESTPROC, dwLenIn < 0\r\n")));
		return SPR_NOT_HANDLED;
	}

	if( (int)dwLenIn > g_iTblSize )
	{
		DEBUGMSG(ZONE_WARNING, (TEXT("IOControl, TESTPROC, dwLenIn > FTE_TBL_SIZE\r\n")));
		return FALSE;
	}

	lpvNewStructure = (LPEX_STRUCT) malloc( sizeof(EX_STRUCT) + dwStructSize );
	if(lpvNewStructure == NULL){
		DEBUGMSG(ZONE_WARNING, (TEXT("IOControl, TESTPROC, not enough memory\r\n")));
		return FALSE;
	}
	memcpy( (lpvNewStructure+1), pBufIn, dwStructSize );
	lpvNewStructure->dwOpenData = dwOpenData;
	lpvNewStructure->dwNotOpenData = ~dwOpenData;

	lpFTE = & g_lpFTE[dwLenIn];

	__try
	{
		dwRet = (lpFTE->lpTestProc)( uMsg, (TPPARAM)(lpvNewStructure+1), lpFTE );
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
		fException = TRUE;
		dwException = _exception_code();
	}

	if ( fException )
	{
		DEBUGMSG(ZONE_ERROR, (TEXT("ProcessTestCommand, lpTestProc=0x%08x caused EXCEPTION 0x%08x.\r\n"),
			lpFTE->lpTestProc,
			dwException));
		free(lpvNewStructure);
		return FALSE;
	}

	// Return code is returned in pdwActualOut (if possible)
	if( pdwActualOut )
	{
		*pdwActualOut = dwRet;
	}

	// copy the data back to the input buffer (in case user data was requested)
	if( dwLenIn >= dwStructSize )
	{
		// this handles the case that the the TPM_QUERY_THREAD_COUNT
		// message was sent to the testproc and value > 0 was returned in 
		// the tpParam struct 
		memcpy( pBufIn, (lpvNewStructure+1), dwStructSize );
	}

	// Release the memory we asked for earlier.
	free( lpvNewStructure );
	
	return fRet;
}

// ----------------------------------------------------------------------------
// void DumpFunctionTableEntry ( LPFUNCTION_TABLE_ENTRY lpFTE )
// ----------------------------------------------------------------------------

void DumpFunctionTableEntry ( LPFUNCTION_TABLE_ENTRY lpFTE )
{
	DEBUGMSG(ZONE_VERBOSE, (TEXT("LPFUNCTION_TABLE_ENTRY = %lX\r\n"), lpFTE));

	if( lpFTE )
	{
		DEBUGMSG(ZONE_VERBOSE, (TEXT("uDepth         = %u\r\n"),
			lpFTE->uDepth));
		DEBUGMSG(ZONE_VERBOSE, (TEXT("dwUserData     = %lu\r\n"),
			lpFTE->uDepth));
		DEBUGMSG(ZONE_VERBOSE, (TEXT("dwUniqueId     = %lu\r\n"),
			lpFTE->uDepth));
		DEBUGMSG(ZONE_VERBOSE, (TEXT("lpDescription  = %lX\r\n"),
			lpFTE->lpDescription));
		DEBUGMSG(ZONE_VERBOSE, (TEXT("*lpDescription = \"%s\"\r\n"),
			lpFTE->lpDescription));
	}
}

// ----------------------------------------------------------------------------
// LPCTSTR ShellCmdToString( UINT uMsg )
// ----------------------------------------------------------------------------

LPCTSTR ShellCmdToString( UINT uMsg )
{
	LPCTSTR lpStr;

	switch( uMsg )
	{
		case SPM_LOAD_DLL:		lpStr = TEXT("SPM_LOAD_DLL");		break;
		case SPM_UNLOAD_DLL:	lpStr = TEXT("SPM_UNLOAD_DLL"); 	break;
		case SPM_SHELL_INFO:	lpStr = TEXT("SPM_SHELL_INFO"); 	break;
		case SPM_REGISTER:		lpStr = TEXT("SPM_REGISTER");		break;
		case SPM_START_SCRIPT:	lpStr = TEXT("SPM_START_SCRIPT");	break;
		case SPM_STOP_SCRIPT:	lpStr = TEXT("SPM_STOP_SCRIPT");	break;
		case SPM_BEGIN_GROUP:	lpStr = TEXT("SPM_BEGIN_GROUP");	break;
		case SPM_END_GROUP: 	lpStr = TEXT("SPM_END_GROUP");		break;
		case SPM_BEGIN_TEST:	lpStr = TEXT("SPM_BEGIN_TEST"); 	break;
		case SPM_END_TEST:		lpStr = TEXT("SPM_END_TEST");		break;
		case SPM_EXCEPTION: 	lpStr = TEXT("SPM_EXCEPTION");		break;
		default:				lpStr = TEXT("UNKNOWN");			break;
	}
	
	return lpStr;
}

// ----------------------------------------------------------------------------
// LPCTSTR TestCmdToString( UINT uMsg )
// ----------------------------------------------------------------------------

LPCTSTR TestCmdToString( UINT uMsg )
{
	LPCTSTR lpStr;

	switch( uMsg )
	{
		case	TPM_QUERY_THREAD_COUNT: lpStr = TEXT("TPM_QUERY_THREAD_COUNT"); break;
		case	TPM_EXECUTE:			lpStr = TEXT("TPM_EXECUTE");			break;
		default:						lpStr = TEXT("UNKNOWN");				break;
	}
	
	return lpStr;
}

// ----------------------------------------------------------------------------
// LPCTSTR DdlxGetCmdLine( TPPARAM tpParam )
// ----------------------------------------------------------------------------

extern "C" LPCTSTR DdlxGetCmdLine( TPPARAM tpParam )
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("++DdlxGetCmdLine\r\n")));

	LPEX_STRUCT lpExStruct = (LPEX_STRUCT)((PBYTE)tpParam - sizeof(EX_STRUCT));

	__try
	{
		if( lpExStruct->dwNotOpenData == ~(lpExStruct->dwOpenData) )
		{
			DEBUGMSG(ZONE_FUNCTION, (TEXT("--DdlxGetCmdLine - Command line[%d]: %s\r\n"),
				lpExStruct->dwOpenData, glpCommandLines[lpExStruct->dwOpenData]+1));
			return glpCommandLines[lpExStruct->dwOpenData]+1;
		}
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
		DEBUGMSG(ZONE_FUNCTION, (TEXT("--DdlxGetCmdLine - no valid pointer (exception)\r\n")));
		return NULL;
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("--DdlxGetCmdLine - no valid pointer\r\n")));
	return NULL;
}

// ----------------------------------------------------------------------------
// extern "C" DWORD DdlxGetOpenContext( TPPARAM tpParam )
// ----------------------------------------------------------------------------

extern "C" DWORD DdlxGetOpenContext( TPPARAM tpParam )
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("++DdlxGetOpenContext\r\n")));

	LPEX_STRUCT lpExStruct = (LPEX_STRUCT)((PBYTE)tpParam - sizeof(EX_STRUCT));

	__try
	{
		if( lpExStruct->dwNotOpenData == ~(lpExStruct->dwOpenData) )
		{
			DEBUGMSG(ZONE_FUNCTION, (TEXT("--DdlxGetOpenContext - dwOpenData: %d\r\n"), lpExStruct->dwOpenData));
			return lpExStruct->dwOpenData;
		}
	}
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
		DEBUGMSG(ZONE_FUNCTION, (TEXT("--DdlxGetOpenContext - no valid pointer (exception)\r\n")));
		return 0;
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("--DdlxGetOpenContext - no valid pointer\r\n")));
	return 0;
}


//
// implementation of the kato listener class
//

BOOL
DDLXKato_Talk::Init(){

	MSGQUEUEOPTIONS msgqopts = {0};

	//if we do not use message queue, we 
	//just return 
	if(bUseMsgQueue == FALSE){
		pKato = (CKato *)KatoGetDefaultObject();
		return TRUE;
	}
 	// create a global message queue
    	msgqopts.dwSize = sizeof(MSGQUEUEOPTIONS);
    	msgqopts.dwFlags = MSGQUEUE_ALLOW_BROKEN;
    	msgqopts.dwMaxMessages = 0;
    	msgqopts.cbMaxMessage = sizeof(KATOMSGFORMAT);
    	msgqopts.bReadAccess = FALSE; 
		
    	hMsgQueue = NULL;
    	hMsgQueue = CreateMsgQueue(QUEUENAME, &msgqopts);
	if(hMsgQueue == NULL){
		NKDbgPrintfW(_T("WARNING: MessageQueue \"%s\" can not be created"), QUEUENAME);
		return FALSE;
	}

	return TRUE;
}

BOOL
DDLXKato_Talk::Stop(){
	
	CloseMsgQueue(hMsgQueue);

	return TRUE;

}

INT
DDLXKato_Talk::BeginLevel (DWORD dwLevelID, LPCWSTR wszFormat, ...){
	va_list pArgs; 
   	va_start(pArgs, wszFormat);

	KATOMSGFORMAT	msg={0};
	DWORD	dwCount = 0;
	
	ASSERT(hMsgQueue);
	
	msg.dwCommand = C_BeginLevel;
	msg.dwOption = dwLevelID;
      _vsnwprintf(msg.szMsg, KATO_MAX_STRING_LENGTH, wszFormat, pArgs);

	if(bUseMsgQueue == FALSE){
		pKato->BeginLevel(dwLevelID, msg.szMsg);
		return 0;
	}
	
	do{
		if(WriteMsgQueue(hMsgQueue, &msg, sizeof(KATOMSGFORMAT), TALK_WAIT_TIME, 0) == FALSE){
			dwCount ++; //failed, go to retry again
		}
		else
			break;
	}while(dwCount < 100);

	va_end(pArgs);

	if(dwCount >= 100)
		return -1;

	return 0;
}


INT
DDLXKato_Talk::EndLevel (LPCWSTR wszFormat, ...){
	va_list pArgs; 
   	va_start(pArgs, wszFormat);

	KATOMSGFORMAT	msg={0};
	DWORD	dwCount = 0;
	
	ASSERT(hMsgQueue);
	
	msg.dwCommand = C_EndLevel;
      _vsnwprintf(msg.szMsg, KATO_MAX_STRING_LENGTH, wszFormat, pArgs);

	if(bUseMsgQueue == FALSE){
		pKato->EndLevel(msg.szMsg);
		return 0;
	}

	do{
		if(WriteMsgQueue(hMsgQueue, &msg, sizeof(KATOMSGFORMAT), TALK_WAIT_TIME, 0) == FALSE){
			dwCount ++; //failed, go to retry again
		}
		else
			break;
	}while(dwCount < 100);

	va_end(pArgs);

	if(dwCount >= 100)
		return -1;

	return 0;
}

INT
DDLXKato_Talk::Log (DWORD dwVerbosity, LPCWSTR wszFormat, ...){
	va_list pArgs; 
   	va_start(pArgs, wszFormat);

	KATOMSGFORMAT	msg={0};
	DWORD	dwCount = 0;
	
	ASSERT(hMsgQueue);
	
	msg.dwCommand = C_Log;
	msg.dwOption = dwVerbosity;
      _vsnwprintf(msg.szMsg, KATO_MAX_STRING_LENGTH, wszFormat, pArgs);

	if(bUseMsgQueue == FALSE){
		pKato->Log(dwVerbosity, msg.szMsg);
		return 0;
	}

	do{
		if(WriteMsgQueue(hMsgQueue, &msg, sizeof(KATOMSGFORMAT), TALK_WAIT_TIME, 0) == FALSE){
			dwCount ++; //failed, go to retry again
		}
		else
			break;
	}while(dwCount < 100);

	va_end(pArgs);

	if(dwCount >= 100)
		return -1;

	return 0;
}


INT
DDLXKato_Talk::Comment (DWORD dwVerbosity, LPCWSTR wszFormat, ...){
	va_list pArgs; 
   	va_start(pArgs, wszFormat);

	KATOMSGFORMAT	msg={0};
	DWORD	dwCount = 0;
	
	ASSERT(hMsgQueue);
	
	msg.dwCommand = C_Comment;
	msg.dwOption = dwVerbosity;
      _vsnwprintf(msg.szMsg, KATO_MAX_STRING_LENGTH, wszFormat, pArgs);

	if(bUseMsgQueue == FALSE){
		pKato->Comment(dwVerbosity, msg.szMsg);
		return 0;
	}

	do{
		if(WriteMsgQueue(hMsgQueue, &msg, sizeof(KATOMSGFORMAT), TALK_WAIT_TIME, 0) == FALSE){
			dwCount ++; //failed, go to retry again
		}
		else
			break;
	}while(dwCount < 100);

	va_end(pArgs);
	
	if(dwCount >= 100)
		return -1;

	return 0;
}


