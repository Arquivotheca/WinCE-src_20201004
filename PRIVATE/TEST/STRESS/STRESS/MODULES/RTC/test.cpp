//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>

#include "rtccore.h"
#include "rtcerr.h"
#include "eventimpl.h"
#include "tls.h"
#include "server.h"
#include "client.h"
//#include "rtccore_i.c"

enum
{
	MODE_UNKNOWN,
	MODE_SERVER,
	MODE_CLIENT
};

UINT CleanupClient();
int	g_nMode = MODE_UNKNOWN;
HANDLE g_hMutex = NULL;
LPTSTR g_lpszServer = NULL;
DWORD g_dwDuration = 0;
#define CLIENT_ITER_DUR 30000
#define MUTEX_NAME L"S_RTCIM Mutex"
#define MAX_ATTEMPTS 5

///////////////////////////////////////////////////////////////////////////////
//
//	InitServer
//	
//	Initializes the stress module in server mode (receives calls)
//
UINT InitServer()
{
	IRTCClient*			pIRTCClient = NULL;
	IRTCClientProvisioning* pIRTCProvisioning = NULL;	
	HRESULT				hr;
	CTls				tls;
	CServerEventImpl*	pServer;
	CRTCEvents*			pEvents;
	BSTR				bstrUserURI = NULL;	
	UINT				uRet = CESTRESS_FAIL;

	tls.AddRef();

	hr = CoInitializeEx( NULL, COINIT_MULTITHREADED );
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitServer:CoInitialize failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	pServer = new CServerEventImpl;
	if( !pServer )
	{
		LogFail( TEXT("InitServer:new CServerEventImpl failed") );
	
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	pEvents = new CRTCEvents;
	if( !pEvents )
	{
		LogFail( TEXT("InitServer:new CRTCEvents failed") );
	
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	hr = CoCreateInstance( __uuidof(RTCClient), NULL, CLSCTX_INPROC_SERVER, __uuidof(IRTCClient), \
		reinterpret_cast<void **> (&pIRTCClient) );
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitServer:CoCreateInstance failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	pServer->SetClient( pIRTCClient );
	pServer->SetEvents( pEvents );

	tls.SetValue( (void*) pServer );
    
	hr = pIRTCClient->Initialize();
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitServer:IRTCClient->Initialize failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pIRTCClient->put_EventFilter( RTCEF_ALL );
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitServer:IRTCClient->put_EventFilter failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pEvents->Advise( pIRTCClient, pServer );
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitServer:pEvents->Advise failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pIRTCClient->QueryInterface( __uuidof(IRTCClientProvisioning), (void**)&pIRTCProvisioning );
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitServer:IRTCClient->QueryInterface failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pIRTCClient->put_ListenForIncomingSessions( RTCLM_BOTH );
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitServer:IRTCClient->put_ListenForIncomingSessions failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	uRet = CESTRESS_PASS;

cleanup:

	SafeRelease( pIRTCProvisioning );
	return uRet;
}


///////////////////////////////////////////////////////////////////////////////
//
//	InitClient
//	
//	Initializes the stress module in client mode (makes calls)
//
UINT InitClient()
{
	IRTCClient*			pIRTCClient = NULL;
	IRTCClientProvisioning* pIRTCProvisioning = NULL;	
	HRESULT				hr;
	CTls				tls;
	CClientEventImpl*	pClient;
	CRTCEvents*			pEvents;
	BSTR				bstrUserURI = NULL;
	UINT				uRet = CESTRESS_ABORT;

	tls.AddRef();

	hr = CoInitializeEx( NULL, COINIT_MULTITHREADED );
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitClient:CoInitialize failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	pClient = new CClientEventImpl;
	if( !pClient )
	{
		LogFail( TEXT("InitClient:new CClientEventImpl failed") );
	
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	pEvents = new CRTCEvents;
	if( !pEvents )
	{
		LogFail( TEXT("InitClient:new CRTCEvents failed") );
	
		hr = E_OUTOFMEMORY;
		goto cleanup;
	}

	hr = CoCreateInstance( __uuidof(RTCClient), NULL, CLSCTX_INPROC_SERVER, __uuidof(IRTCClient), \
		reinterpret_cast<void **> (&pIRTCClient) );
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitClient:CoCreateInstance failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	pClient->SetClient( pIRTCClient );
	pClient->SetEvents( pEvents );

	tls.SetValue( (void*) pClient );
    
	hr = pIRTCClient->Initialize();
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitClient:IRTCClient->Initialize failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pIRTCClient->put_EventFilter( RTCEF_ALL );
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitClient:IRTCClient->put_EventFilter failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	hr = pEvents->Advise( pIRTCClient, pClient );
	if( FAILED(hr) )
	{
		LogFail( TEXT("InitClient:pEvents->Advise failed hr = 0x%08x"), hr );
		goto cleanup;
	}	

	uRet = CESTRESS_PASS;

cleanup:


	SafeRelease( pIRTCProvisioning );
	return uRet;

}


///////////////////////////////////////////////////////////////////////////////
//
//	ServerIteration
//	
//	All of the server side execution happens in the event implementation
//	(server.cpp), so we just pump messages until or duration is up
//
UINT ServerIteration( )
{	
	DWORD	dwStart;
	MSG		sMsg;
	UINT	uRet = CESTRESS_FAIL;

	dwStart = GetTickCount();

	while( TRUE )
	{
		if( PeekMessage( &sMsg, NULL, 0, 0, PM_REMOVE ) )
		{
			DispatchMessage( &sMsg );
		}
		else
		{
			Sleep( 1000 );
		}

		if( !CheckTime( g_dwDuration, dwStart ) )
			break;
		
	}

	uRet = CESTRESS_PASS;

	return uRet;
}


///////////////////////////////////////////////////////////////////////////////
//
//	ClientIteration
//	
//	Sends messages over and over again to the server specified in MODULE_PARAMS
//
UINT ClientIteration()
{	
	DWORD				dwTick, dwStart, dwMessageSent;
	MSG					sMsg;	
	IRTCClient*			pIRTCClient = NULL;
	IRTCSession*		pSess = NULL;
	IRTCParticipant*	pParty = NULL;	
	CClientEventImpl*	pClient = NULL;
	CTls				tls;
	HRESULT				hr;
	BSTR				bstrDestURI= NULL;
	BSTR				bstrDestName = NULL;
	BSTR				bstrMessageHeader = NULL;
	BSTR				bstrMessage = NULL;
	WCHAR				wszName[256];
	RTC_SESSION_STATE	enState;
	
	LONG				lCookie;
	UINT				uRet = CESTRESS_FAIL;
	int					nIter;
	bool				bInitialSend = true;

	static int			cAttempts = 0;

	//	check to see if we've attempted to connect a number of times but failed
	if( cAttempts >= MAX_ATTEMPTS )
	{
		LogAbort( TEXT("ClientIteration: exceeded max attempts to connect, aborting test.") );
		uRet = CESTRESS_ABORT;
		goto cleanup;
	}
	
	if( g_lpszServer && _tcsicmp( g_lpszServer, TEXT("localhost") ) == 0 )
	{
		LogWarn2( TEXT("ClientIteration:servername is localhost, returning abort") );
		uRet = CESTRESS_ABORT;
		goto cleanup;
	}

	pClient = (CClientEventImpl*)tls.GetValue();

    if( !pClient )
	{
		LogFail( TEXT("ClientIteration:tls.GetValue returned NULL") );
		uRet = CESTRESS_ABORT;
		goto cleanup;
	}

	pIRTCClient = pClient->GetClient();
	if( !pIRTCClient )
	{
		LogFail( TEXT("ClientIteration:pClient->GetClient returned NULL") );
		goto cleanup;
	}

	pIRTCClient->AddRef();

	hr = pIRTCClient->CreateSession( RTCST_IM, NULL, NULL, 0, &pSess );
	if( FAILED(hr) )
	{
		LogFail( TEXT("ClientIteration:pEvents->Advise failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	swprintf( wszName, L"sip:%s", g_lpszServer );
	bstrDestURI = SysAllocString( wszName );
	if( !bstrDestURI )
	{
		LogFail( TEXT("ClientIteration:SysAllocString( wszName ) failed") );
		goto cleanup;
	}

	swprintf( wszName, L"%s", g_lpszServer );
	bstrDestName = SysAllocString( wszName );
	if( !bstrDestName )
	{
		LogFail( TEXT("ClientIteration:SysAllocString( wszName ) failed") );
		goto cleanup;
	}

	bstrMessageHeader = SysAllocString( L"text/plain" );
	if( !bstrMessageHeader )
	{
		LogFail( TEXT("ClientIteration:SysAllocString( bstrMessageHeader ) failed") );
		goto cleanup;
	}

	swprintf( wszName, L"Hello %s", bstrDestName );
	bstrMessage = SysAllocString( wszName );
	if( !bstrMessage )
	{
		LogFail( TEXT("ClientIteration:SysAllocString( wszName ) failed") );
		goto cleanup;
	}


	hr = pSess->AddParticipant( bstrDestURI, bstrDestName, &pParty );
	if( FAILED(hr) )
	{
		cAttempts++;
		LogFail( TEXT("ClientIteration:pSess->AddParticipant failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	dwStart = GetTickCount();

	nIter = 0;
	while( TRUE )
	{
		if( PeekMessage( &sMsg, NULL, 0, 0, PM_REMOVE ) )
		{
			DispatchMessage( &sMsg );
		}
		else
		{
			if( bInitialSend )
			{
				//	send the initial message			
				lCookie = pClient->GetCookie();
				lCookie++;
				pClient->SetCookie( lCookie );
				pClient->SetOperationComplete( false );

				hr = pSess->SendMessage( bstrMessageHeader, bstrMessage, lCookie );
				if( FAILED(hr) )
				{
					LogFail( TEXT("ClientIteration (%d):pSess->SendMessage failed hr = 0x%08x"), nIter, hr );
					if( hr == RTC_E_INVALID_SESSION_STATE )
					{
						pSess->get_State( &enState );
						LogFail( TEXT("ClientIteration (%d):state is 0x%08x"), nIter, enState );						
						cAttempts++;												
					}
					else if( hr == RTC_E_SIP_DNS_FAIL )
					{
						//	DNS server couldn't find the machine name
						cAttempts++;
					}


					goto cleanup;
				}

				//	wait for the operation to complete
				dwMessageSent = GetTickCount();
				while( !pClient->IsOperationComplete() )
				{					
					if( PeekMessage( &sMsg, NULL, 0, 0, PM_REMOVE ) )
					{
						DispatchMessage( &sMsg );
					}
					else
					{
						Sleep( 500 );
					}

					//	wait for only so long
					if( (GetTickCount() - dwMessageSent) > CLIENT_ITER_DUR )
						break;
				}
				if( pClient->IsOperationComplete() )
				{
					LogComment( TEXT("Operation completed.  (Initial send)") );
				}
				else
				{
					LogComment( TEXT("Operation DID NOT complete.  (Initial send)") );
				}

				bInitialSend = false;
			}
			else
			{
				//	make sure we are in a connected state
				if( pClient->IsConnected() )
				{
					//	send the rest of the messages
					lCookie = pClient->GetCookie();
					lCookie++;
					pClient->SetCookie( lCookie );
					pClient->SetOperationComplete( false );

					hr = pSess->SendMessage( bstrMessageHeader, bstrMessage, lCookie );
					if( FAILED(hr) )
					{
						LogFail( TEXT("ClientIteration (%d):pSess->SendMessage failed hr = 0x%08x"), nIter, hr );
						if( hr == RTC_E_INVALID_SESSION_STATE )
						{
							pSess->get_State( &enState );
							LogFail( TEXT("ClientIteration (%d):state is 0x%08x"), nIter, enState );
						}
						else if( hr == RTC_E_SIP_DNS_FAIL )
						{
							//	DNS server couldn't find the machine name
							cAttempts++;
						}

						goto cleanup;
					}

					//	wait for the operation to complete
					dwMessageSent = GetTickCount();
					while( !pClient->IsOperationComplete() )
					{
						if( PeekMessage( &sMsg, NULL, 0, 0, PM_REMOVE ) )
						{
							DispatchMessage( &sMsg );
						}
						else
						{
							Sleep( 500 );
						}

						//	wait for only so long
						if( (GetTickCount() - dwMessageSent) > CLIENT_ITER_DUR )
							break;
					}
					if( pClient->IsOperationComplete() )
					{
						LogComment( TEXT("Operation completed.") );
					}
					else
					{
						LogComment( TEXT("Operation DID NOT complete.") );
					}

				}
			}

			Sleep(1000);
		}

		dwTick = GetTickCount();
		if( (dwTick - dwStart) > CLIENT_ITER_DUR )
		{
			//	times up
			break;
		}
		nIter++;
	}

	//	pump messages here
	
	while( PeekMessage( &sMsg, NULL, 0, 0, PM_REMOVE ) )
	{
		DispatchMessage( &sMsg );
	}



	hr = pSess->Terminate( RTCTR_NORMAL );
	if( FAILED(hr) )
	{
		LogFail( TEXT("ClientIteration:pSess->Terminate failed hr = 0x%08x"), hr );
		goto cleanup;
	}

	//	pump messages here
	while( PeekMessage( &sMsg, NULL, 0, 0, PM_REMOVE ) )
	{
		DispatchMessage( &sMsg );
	}

	uRet = CESTRESS_PASS;

cleanup:

	SafeRelease( pSess );
	SafeRelease( pParty );
	SafeRelease( pIRTCClient );
	
	SafeFreeString( bstrDestURI );
	SafeFreeString( bstrDestName );

	if( uRet == CESTRESS_ABORT )
	{
		//	since stressmod doesn't call CleanupTestThread if we abort
		//	we will just call the cleanup code directly
		CleanupClient();
	}
	return uRet;
}


///////////////////////////////////////////////////////////////////////////////
//
//	DoTest
//	
//	Calls appropriate test function depending on what mode we are in
//
UINT DoTest()
{	
	UINT uRet;
	
	if( g_nMode == MODE_SERVER )
	{
		return ServerIteration();
	}
	else if( g_nMode == MODE_CLIENT )
	{
		uRet = ClientIteration();

		if( uRet == CESTRESS_FAIL || uRet == CESTRESS_ABORT )
			return uRet;				
	}
	else
	{
		return CESTRESS_FAIL;
	}

	return CESTRESS_PASS;
}


///////////////////////////////////////////////////////////////////////////////
//
//	CleanupClient
//	
//	Closes down RTC
//
UINT CleanupClient()
{
	CTls				tls;
	IRTCClient*			pIRTCClient = NULL;
	CClientEventImpl*	pClient = NULL;
	CRTCEvents*			pEvents = NULL;
	HRESULT				hr;
	MSG					sMsg;
	UINT				uRet = CESTRESS_FAIL;

	pClient = (CClientEventImpl*)tls.GetValue();

	if( pClient )
	{
		pEvents = pClient->GetEvents();
		pIRTCClient = pClient->GetClient();
	}

	if( pIRTCClient )
	{
		hr = pIRTCClient->PrepareForShutdown();
		if( FAILED(hr) )
		{
			LogFail( TEXT("CleanupClient:pIRTCClient->PrepareForShutdown failed, hr = 0x%08x"), hr );
			goto cleanup;
		}

		//	pump messages here
		while( WaitForSingleObject( pClient->GetShutdownEvent(), 1000 ) == WAIT_TIMEOUT )
		{
			while( PeekMessage( &sMsg, NULL, 0, 0, PM_REMOVE ) )
			{
				DispatchMessage( &sMsg );
			}

		}

		hr = pIRTCClient->Shutdown();
		if( FAILED(hr) )
		{
			LogFail( TEXT("CleanupClient:pIRTCClient->Shutdown failed, hr = 0x%08x"), hr );
			goto cleanup;
		}

		pEvents->Unadvise( pIRTCClient );
	
	}
	else
	{
		LogFail( TEXT("CleanupClient:IRTCClient is NULL") );
		goto cleanup;
	}

	uRet = CESTRESS_PASS;

	
cleanup:

	//	Releasing pIRTCClient will also release pEvents
	SafeRelease( pIRTCClient );


	if( pClient )
		delete pClient;

	tls.Release();

	CoUninitialize();

	return uRet;
}


///////////////////////////////////////////////////////////////////////////////
//
//	CleanupServer
//	
//	Closes down RTC
//
UINT CleanupServer()
{
	CTls				tls;
	IRTCClient*			pIRTCClient = NULL;
	CServerEventImpl*	pServer = NULL;
	CRTCEvents*			pEvents = NULL;
	HRESULT				hr;
	MSG					sMsg;
	UINT				uRet = CESTRESS_FAIL;
	DWORD				dwStart;

	pServer = (CServerEventImpl*)tls.GetValue();

	if( pServer )
	{
		pEvents = pServer->GetEvents();
		pIRTCClient = pServer->GetClient();
	}

	if( pIRTCClient )
	{
		hr = pIRTCClient->PrepareForShutdown();
		if( FAILED(hr) )
		{
			LogFail( TEXT("CleanupServer:pIRTCClient->PrepareForShutdown failed, hr = 0x%08x"), hr );
			goto cleanup;
		}

		//	pump messages here for one minute

		dwStart = GetTickCount();
		while( (GetTickCount() - dwStart) < 60000 )
		{
			while( PeekMessage( &sMsg, NULL, 0, 0, PM_REMOVE ) )
			{
				DispatchMessage( &sMsg );
			}
			Sleep( 500 );
		}


		if( WaitForSingleObject( pServer->GetShutdownEvent(), 60000 ) == WAIT_OBJECT_0 )
		{
			hr = pIRTCClient->Shutdown();
			if( FAILED(hr) )
			{
				LogFail( TEXT("CleanupServer:pIRTCClient->Shutdown failed, hr = 0x%08x"), hr );
				goto cleanup;
			}

			pEvents->Unadvise( pIRTCClient );
		}
		else
		{
				LogFail( TEXT("CleanupServer:Wait for shutdown event timed out") );
				goto cleanup;
		}
	}
	else
	{
		LogFail( TEXT("CleanupServer:IRTCClient is NULL") );
		goto cleanup;
	}

	uRet = CESTRESS_PASS;

	
cleanup:

	//	Releasing pIRTCClient will also release pEvents
	SafeRelease( pIRTCClient );


	if( pServer )
		delete pServer;

	tls.Release();

	CoUninitialize();

	return uRet;
}

///////////////////////////////////////////////////////////////////////////////
//
//	CleanupThread
//	
//	Calls appropriate cleanup code depending on what mode we are in
//
int CleanupThread()
{
	if( g_nMode == MODE_SERVER )
	{
		return CleanupServer();
	}
	else if( g_nMode == MODE_CLIENT )
	{
		return CleanupClient();
	}
	else
	{
		return 0;
	}	

}



///////////////////////////////////////////////////////////////////////////////
//
// @doc S_RTCIM
//
//
// @topic Dll Modules for CE Stress |
//
//	The simplest way to implement a CE Stress module is by creating a DLL
//  that can be managed by the stress harness.  Each stress DLL is loaded and 
//	run by a unique instance of the container: stressmod.exe.  The container
//  manages the duration of your module, collects and reports results to the
//  harness, and manages multiple test threads.
//
//	Your moudle is required to export an initialization function (<f InitializeStressModule>)
//	and a termination function (<f TerminateStressModule>).  Between these calls 
//  your module's main test function (<f DoStressIteration>) will be called in a 
//  loop for the duration of the module's run.  The harness will aggragate and report
//  the results of your module's run based on this function's return values.
//
//	You may wish to run several concurrent threads in your module.  <f DoStressIteration>
//	will be called in a loop on each thread.  In addition, you may implement per-thread
//  initialization and cleanup functions (<f InitializeTestThread> and <f CleanupTestThread>).
//	  
//	<nl>
//	Required functions:
//
//    <f InitializeStressModule> <nl>
//    <f DoStressIteration> <nl>
//    <f TerminateStressModule>
//
//  Optional functions:
//
//    <f InitializeTestThread> <nl>
//    <f CleanupTestThread>
//

//
// @topic Stress utilities |
//
//	Documentation for the utility functions in StressUtils.Dll can be
//  found on:
//
//     \\\\cestress\\docs\\stresss <nl>
//     %_WINCEROOT%\\private\\test\\stress\\stress\\docs
//
//
// @topic Sample code |
//
//	Sample code can be found at: 
//       <nl>\t%_WINCEROOT%\\private\\test\\stress\\stress\\samples <nl>
//
//	Actual module examples can be found at: 
//       <nl>%_WINCEROOT%\\private\\test\\stress\\stress\\modules
//

HANDLE g_hInst = NULL;



///////////////////////////////////////////////////////////////////////////////
//
// @func	(Required) BOOL | InitializeStressModule |
//			Called by the harness after your DLL is loaded.
// 
// @rdesc	Return TRUE if successful.  If you return FALSE, CEStress will 
//			terminate your module.
// 
// @parm	[in] <t MODULE_PARAMS>* | pmp | Pointer to the module params info 
//			passed in by the stress harness.  Most of these params are handled 
//			by the harness, but you can retrieve the module's user command
//          line from this structure.
// 
// @parm	[out] UINT* | pnThreads | Set the value pointed to by this param 
//			to the number of test threads you want your module to run.  The 
//			module container that loads this DLL will manage the life-time of 
//			these threads.  Each thread will call your <f DoStressIteration>
//			function in a loop.
// 		
// @comm    You must call InitializeStressUtils( ) (see \\\\cestress\\docs\\stress\\stressutils.hlp) and 
//			pass it the <t MODULE_PARAMS>* that was passed in to this function by
//			the harness.  You may also do any test-specific initialization here.
//
//
//

BOOL InitializeStressModule (
							/*[in]*/ MODULE_PARAMS* pmp, 
							/*[out]*/ UINT* pnThreads
							)
{
	
	*pnThreads = 1;
	// !! You must call this before using any stress utils !!

#ifdef RTC_CLIENT
	InitializeStressUtils (
							_T("S_RTCIM"),									// Module name to be used in logging
							LOGZONE(SLOG_SPACE_GDI, SLOG_FONT | SLOG_RGN),	// Logging zones used by default
							pmp												// Forward the Module params passed on the cmd line
							);
#else
	InitializeStressUtils (
							_T("S_RTCIMSVR"),									// Module name to be used in logging
							LOGZONE(SLOG_SPACE_GDI, SLOG_FONT | SLOG_RGN),	// Logging zones used by default
							pmp												// Forward the Module params passed on the cmd line
							);
#endif

	//	try and get our named mutex
	g_hMutex = CreateMutex( NULL, FALSE, MUTEX_NAME );
	if( !g_hMutex )
	{
		LogFail( TEXT("CreateMutex failed, GLE = %d"), GetLastError() );
		return FALSE;
	}

	//	try to grab the mutex, if successful, then we are a server
	if( WaitForSingleObject( g_hMutex, 0 ) == WAIT_OBJECT_0 )
	{
		g_nMode = MODE_SERVER;
		LogComment( TEXT("Acting as Server") );
		g_dwDuration = INFINITE;
	}
	else
	{
		g_nMode = MODE_CLIENT;
		LogComment( TEXT("Acting as Client") );
		g_dwDuration = pmp->dwDuration;
	}

	
	//	get the remote server name	
	g_lpszServer = pmp->tszServer;	
	LogComment( TEXT("g_dwDuration = %d"), g_dwDuration );
	

	// Note on Logging Zones: 
	//
	// Logging is filtered at two different levels: by "logging space" and
	// by "logging zones".  The 16 logging spaces roughly corresponds to the major
	// feature areas in the system (including apps).  Each space has 16 sub-zones
	// for a finer filtering granularity.
	//
	// Use the LOGZONE(space, zones) macro to indicate the logging space
	// that your module will log under (only one allowed) and the zones
	// in that space that you will log under when the space is enabled
	// (may be multiple OR'd values).
	//
	// See \test\stress\stress\inc\logging.h for a list of available
	// logging spaces and the zones that are defined under each space.



	// (Optional) 
	// Get the number of modules of this type (i.e. same exe or dll)
	// that are currently running.  This allows you to bail (return FALSE)
	// if your module can not be run with more than a certain number of
	// instances.

	LONG count = GetRunningModuleCount(g_hInst);

	LogVerbose(_T("Running Modules = %d"), count);


	// TODO:  Any module-specific initialization here


	
	
	return TRUE;
}




///////////////////////////////////////////////////////////////////////////////
//
// @func	(Optional) UINT | InitializeTestThread | 
//			Called once at the start of each test thread.  
// 
// @rdesc	Return <t CESTRESS_PASS> if successful.  If your return <t CESTRESS_ABORT>
//			your module will be terminated (although <f TerminateStressModule> 
//			will be called first).
// 
// @parm	[in] HANDLE | hThread | A pseudohandle for the current thread. 
//			A pseudohandle is a special constant that is interpreted as the 
//			current thread handle. The calling thread can use this handle to 
//			specify itself whenever a thread handle is required. 
//
// @parm	[in] DWORD | dwThreadId | This thread's identifier.
//
// @parm	[in] int | index | Zero-based index of this thread.  You can use this
//			for indexing arrays of per-thread data.
// 		
// @comm    There is no required action.  This is provided for test-specific 
//			initialization only.
//
//
//

UINT InitializeTestThread (
						/*[in]*/ HANDLE hThread, 
						/*[in]*/ DWORD dwThreadId, 
						/*[in]*/ int index)
{
	LogVerbose(_T("InitializeTestThread(), thread handle = 0x%x, Id = %d, index %i"), 
					hThread, 
					dwThreadId, 
					index 
					);

	if( g_nMode == MODE_SERVER )
	{
		return InitServer();
	}
	else if( g_nMode == MODE_CLIENT )
	{
		return InitClient();
	}
	else
	{
		return CESTRESS_FAIL;
	}

	return CESTRESS_PASS;
}





///////////////////////////////////////////////////////////////////////////////
//
// @func	(Required) UINT | DoStressIteration | 
//			Called once at the start of each test thread.  
// 
// @rdesc	Return a <t CESTRESS return value>.  
// 
// @parm	[in] HANDLE | hThread | A pseudohandle for the current thread. 
//			A pseudohandle is a special constant that is interpreted as the 
//			current thread handle. The calling thread can use this handle to 
//			specify itself whenever a thread handle is required. 
//
// @parm	[in] DWORD | dwThreadId | This thread's identifier.
//
// @parm	[in] LPVOID | pv | This can be cast to a pointer to an <t ITERATION_INFO>
//			structure.
// 		
// @comm    This is the main worker function for your test.  A test iteration should 
//			begin and end in this function (though you will likely call other test 
//			functions along the way).  The return value represents the result for a  
//			single iteration, or test case.  
//
//
//

UINT DoStressIteration (
						/*[in]*/ HANDLE hThread, 
						/*[in]*/ DWORD dwThreadId, 
						/*[in]*/ LPVOID pv)
{
	ITERATION_INFO* pIter = (ITERATION_INFO*) pv;

	LogVerbose(_T("Thread %i, iteration %i"), pIter->index, pIter->iteration);


	// TODO:  Do your actual testing here.
	//
	//        You can do whatever you want here, including calling other functions.
	//        Just keep in mind that this is supposed to represent one iteration
	//        of a stress test (with a single result), so try to do one discrete 
	//        operation each time this function is called. 

	return DoTest();



	// You must return one of these values:

	
	//return CESTRESS_FAIL;
	//return CESTRESS_WARN1;
	//return CESTRESS_WARN2;
	//return CESTRESS_ABORT;  // Use carefully.  This will cause your module to be terminated immediately.
}



///////////////////////////////////////////////////////////////////////////////
//
// @func	(OPTIONAL) UINT | CleanupTestThread | 
//			Called once before each test thread exits.  
// 
// @rdesc	Return a <f CESTRESS return value>.  If you return anything other than
//			<t CESTRESS_PASS> this will count toward total test results (i.e. as
//			a final iteration of your test thread).
// 
// @parm	[in] HANDLE | hThread | A pseudohandle for the current thread. 
//			A pseudohandle is a special constant that is interpreted as the 
//			current thread handle. The calling thread can use this handle to 
//			specify itself whenever a thread handle is required. 
//
// @parm	[in] DWORD | dwThreadId | This thread's identifier.
//
// @parm	[in] int | index | Zero-based index of this thread.  You can use this
//			for indexing arrays of per-thread data.
// 		
// @comm    There is no required action.  This is provided for test-specific 
//			clean-up only.
//
//

UINT CleanupTestThread (
						/*[in]*/ HANDLE hThread, 
						/*[in]*/ DWORD dwThreadId, 
						/*[in]*/ int index)
{
	LogComment(_T("CleanupTestThread(), thread handle = 0x%x, Id = %d, index %i"), 
					hThread, 
					dwThreadId, 
					index 
					);

	if( g_nMode == MODE_SERVER )
	{
		return CleanupServer();
	}
	else if( g_nMode == MODE_CLIENT )
	{
		return CleanupClient();
	}
	else
	{
		return CESTRESS_FAIL;
	}	

	return CESTRESS_WARN2;
}



///////////////////////////////////////////////////////////////////////////////
//
// @func	(Required) DWORD | TerminateStressModule | 
//			Called once before your module exits.  
// 
// @rdesc	Unused.
// 		
// @comm    There is no required action.  This is provided for test-specific 
//			clean-up only.
//

DWORD TerminateStressModule (void)
{
	// TODO:  Do test-specific clean-up here.

	//	if we are a server, release the mutex
	if( g_nMode == MODE_SERVER )
	{
		if( g_hMutex )
		{
		ReleaseMutex( g_hMutex );
		}
	}

	if( g_hMutex )
	{
		CloseHandle( g_hMutex );
	}


	// This will be used as the process exit code.
	// It is not currently used by the harness.

	return ((DWORD) -1);
}



///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
					HANDLE hInstance, 
					ULONG dwReason, 
					LPVOID lpReserved
					)
{
	g_hInst = hInstance;
    return TRUE;
}
