//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: autodial.c
Abstract: Autodialer functions
--*/

#include <autodprv.h>
#include <iphlpapi.h>
#include <Windev.h>

// Debug zones
#define ZONE_INIT          DEBUGZONE(0)
#define ZONE_CONNECTION    DEBUGZONE(11)
#define ZONE_IDLETIMEOUT   DEBUGZONE(12)
#define ZONE_FUNCTION      DEBUGZONE(13)
#define ZONE_WARN          DEBUGZONE(14)
#define ZONE_ERROR         DEBUGZONE(15)

#define STATE_OFF			0
#define STATE_ON			1
#define STATE_STARTING_UP	2
#define STATE_SHUTTING_DOWN	3

// FILETIME (100-ns intervals) to milleseonds (10 x 1000)
#define FILETIME_TO_MILLISECONDS  10000


#ifdef DEBUG
#define    myleave(e)    { ASSERT(dwRet != 0); err = e; goto done; }
#define    myretleave(r,e) { dwRet = r; err = e;  goto done; }
#else
#define    myleave(e)       { ASSERT(dwRet != 0);  goto done;}
#define    myretleave(r,e)  { dwRet = r; goto done; }
#endif


typedef struct
{
	HRASCONN 	m_hRasConn;					// Global connection context
	TCHAR       szRasEntryName[RAS_MaxEntryName + 1];

	// Used to determine when to end the connection.
	__int64    	m_ftIdleTimeout;			// amount of time before we hang up, measure in filetime.
	HANDLE  	m_hTimerThread;				// Handle to created timer thread
	HANDLE      m_hTimerEvent;              // Event to fire.
	DWORD 		m_dwBytesTxRx;				// Total # of bytes sent and received
	__int64		m_ftLastActivity;			// Treated as FILETIME, last time bytes were sent

	DWORD		m_dwRedialAttempts;         // # of times we'll try to go throug redial cycle
	DWORD		m_dwRedialPauseTime;        // Sleep this many ms between calls

	BOOL        m_fEnableOnPublicInterface;
	 
	// Used for FAIL_RETRY_WAIT monitoring
	__int64     m_ftFailRetryWait;           // Registry configured amount of time to wait on.
	int 		m_iLastFailIndex;               
	__int64     g_ftLastFailTime;		     // Element treated as FILETIME 
 }
AUTODIAL_CONNECTION_INFO, *PAUTODIAL_CONNECTION_INFO;


// Global variables
AUTODIAL_CONNECTION_INFO g_AutoDialConn;
DWORD            g_fState;
CRITICAL_SECTION g_GlobalCS;
BOOL			 g_fAutodialInitialized;
HINSTANCE        g_hInstance;


// Function definitions
DWORD AutoDialConnect(RASDIALPARAMS *pRasDialParams);
DWORD AutoDialEndConnectionInternal(BOOL fTimedOut);
DWORD AutodialReadRegistrySettings(TCHAR *szDialRasEntryName, TCHAR *pszEntryName1, TCHAR *pszEntryName2, AUTODIAL_CONNECTION_INFO *pConnInfo);
BOOL GetConnectedStatus(TCHAR *szEntryName, RASCONNSTATUS *lpRasConn, HRASCONN *phRasConn);
DWORD CheckConnected(TCHAR *szEntryName);
DWORD HangUpIdleConnectionThread(LPVOID lpv);
void SetFailTime(void);
DWORD DetermineIfAutodialConnectionsActive(RASCONNSTATUS *lpRasConnStatus, TCHAR *szRasName, HRASCONN *phRasConn);
void SetAsPublicInterface(TCHAR *szEntryName);
BOOL CheckFailRetry(void);

#if defined(DEBUG) && defined (UNDER_CE)
  DBGPARAM dpCurSettings = {
    TEXT("Autodial"), {
    TEXT("Init"),TEXT("Unused"),TEXT("Unused"),TEXT("Unused"),
    TEXT("Unused"),TEXT("Unused"),TEXT("Unused"),
    TEXT("Unused"),TEXT("Unused"),TEXT("Unused"),TEXT("Unused"),
    TEXT("Connection"),TEXT("IdleTimeout"),TEXT("Function"),TEXT("Warning"),TEXT("Error") },
    0x8801
  };
#endif


//***********************************************************************
// AutoDial Functions
//***********************************************************************

#define MAX_HANGUP_RETRIES             50
void MyHangUp(LPHRASCONN phRasConn)
{
	int i;
	RASCONNSTATUS RasConnStatus;
	RasConnStatus.dwSize = sizeof(RASCONNSTATUS);

	RasHangUp(*phRasConn);
	for (i = 0; i < MAX_HANGUP_RETRIES; i++)
	{
		if (ERROR_INVALID_HANDLE == RasGetConnectStatus(*phRasConn,&RasConnStatus))
			break;
		Sleep(1000);
	}
	ASSERT ( i != MAX_HANGUP_RETRIES);

	*phRasConn = 0;
}



// This function is called when network services start up to initialize
// the Autodial members.

// Note:  Calls to AutoDialInitializeModule and AutoDialCleanupModule
//        must be syncronized by the function calling it.  However,
//        thread syncronization for AutodialStartConnection and 
//        AutoDialEndConnection are done by Autodial itself.

DWORD AutoDialInitializeModule(void)
{
	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:+AutoDialInitializeModule\r\n")));

	if (!g_fAutodialInitialized)
	{
		memset(&g_AutoDialConn,0,sizeof(g_AutoDialConn));
		g_fState = STATE_OFF;
		g_AutoDialConn.m_iLastFailIndex  = 0;
		InitializeCriticalSection(&g_GlobalCS);
		
		if (g_AutoDialConn.m_hTimerEvent = CreateEvent(NULL,FALSE,FALSE,NULL))
			g_fAutodialInitialized = TRUE;
			
	    StatusInit(g_hInstance);
	}

	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:-AutoDialInitializeModule\r\n")));
	return g_fAutodialInitialized ? ERROR_SUCCESS : ERROR_RASAUTO_CANNOT_INITIALIZE;
}

// Called when shutting down AutoDial services.  After calling this function
// it will be necessary to call AutoDialInitializeModule again to use AutoDial.

DWORD AutoDialCleanupModule(void)
{
	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:+AutoDialCleanupModule\r\n")));

	g_fAutodialInitialized = FALSE;
	if (g_AutoDialConn.m_hRasConn)
		AutoDialEndConnectionInternal(FALSE);

	DeleteCriticalSection(&g_GlobalCS);
	CloseHandle(g_AutoDialConn.m_hTimerEvent);
	
	StatusUninit(g_hInstance);

	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:-AutoDialCleanupModule\r\n")));
	return ERROR_SUCCESS;
}


// NAT calls this routine when it is unable to deliver a packet
// The routine loads values from the registry to see who to dial and 
// attempts to create a connection using RasDial if: 
//     1)  The registry key is valid  
//     2)  AutoDial has been enabled
//     3)  We haven't failed 3 times in a row within a brief time span

// szDialRasEntryName is set to non-NULL if we wish to initiate a dial using
// the given RAS conn name and ignore the registry values.

DWORD AutoDialStartConnection(TCHAR *szDialRasEntryName)
{
#ifdef DEBUG
	DWORD           err = 0;
#endif
	DWORD 			dwRet = 0;
	TCHAR 			szEntryName1[RAS_MaxEntryName + 1];
	TCHAR 			szEntryName2[RAS_MaxEntryName + 1];
	RASDIALPARAMS 	RasDialParams1;
	RASDIALPARAMS 	RasDialParams2;
	RASDIALPARAMS 	*pRasDialParams;
	DWORD			i;
	BOOL			fExecute = FALSE;

	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:+AutoDialStartConnection\r\n")));
	ASSERT(!szDialRasEntryName || _tcslen(szDialRasEntryName) < RAS_MaxEntryName);

	if (!g_fAutodialInitialized)
	{
		DEBUGMSG(ZONE_ERROR,(TEXT("Autodial: AutoDialInitializeModule must be called before AutoDialStartConnection\r\n")));
		ASSERT(0);
		return ERROR_RASAUTO_CANNOT_INITIALIZE;
	}

	EnterCriticalSection(&g_GlobalCS);
	if (STATE_OFF == g_fState)
	{
		g_fState = STATE_STARTING_UP;
		fExecute = TRUE;
	}

	// It's possible we've lost the connection but are still in the STATE_ON
	else if (STATE_ON == g_fState && g_AutoDialConn.m_hRasConn)
	{
		RASCONNSTATUS 	RasConnStatus;
		RasConnStatus.dwSize = sizeof(RasConnStatus);

		if (ERROR_SUCCESS == RasGetConnectStatus(g_AutoDialConn.m_hRasConn,&RasConnStatus))
		{
			if (RASCS_Disconnected == RasConnStatus.rasconnstate)
			{
				MyHangUp(&g_AutoDialConn.m_hRasConn);
                
				g_fState = STATE_STARTING_UP;
				fExecute = TRUE;
			}
		}
	}
	// Note:  We leave the critical section here and rely on the g_fState so
	// that if another thread tries to call this function it will immediatly
	// receive an error (ERROR_DIAL_ALREADY_IN_PROGRESS) and won't be
	// blocked on the CS potentially waiting for RasDial() to complete.
	LeaveCriticalSection(&g_GlobalCS);

	if (!fExecute)
	{
		// We only allow 1 autodial connection at the same time.
		// Do not goto done, the cleanup code assumes we have fExecute = TRUE
	
		DEBUGMSG(ZONE_ERROR,(TEXT("AutoDial:AutoDialStartConnection failed -- another connection is active\r\n")));
		return ERROR_DIAL_ALREADY_IN_PROGRESS;
	}
	
	if (g_AutoDialConn.m_hTimerThread)
    {
	    SetEvent(g_AutoDialConn.m_hTimerEvent);
	    WaitForSingleObject(g_AutoDialConn.m_hTimerThread,INFINITE);
	    CloseHandle(g_AutoDialConn.m_hTimerThread);
	    g_AutoDialConn.m_hTimerThread = 0;
    }

	ASSERT(g_fState == STATE_STARTING_UP);
	ASSERT(g_AutoDialConn.m_hTimerThread == 0);
   	
	if (dwRet = AutodialReadRegistrySettings(szDialRasEntryName,szEntryName1,szEntryName2,&g_AutoDialConn))
		myleave(10);

	ASSERT( (szDialRasEntryName && !szEntryName1[0] && !szEntryName2[0]) ||
	        (!szDialRasEntryName && szEntryName1[0]));

	if ( ! CheckFailRetry())
		myretleave(ERROR_DISCONNECTION,3);  // just set the best RAS err code we can

	// It's possible that szEntryName has been connected through another application
	// already, in which case we don't try to RasDial on it.
	if (szDialRasEntryName && (dwRet = CheckConnected(szDialRasEntryName)))
		myleave(20);
	
	if (szEntryName1[0] && (dwRet = CheckConnected(szEntryName1)))
		myleave(30);

	if (szEntryName2[0] && (dwRet = CheckConnected(szEntryName2)))
		myleave(40);
		
	RasDialParams1.dwSize = sizeof(RASDIALPARAMS);

	if (szDialRasEntryName)
		_tcsncpy(RasDialParams1.szEntryName,szDialRasEntryName,RAS_MaxEntryName);
	else
		_tcsncpy(RasDialParams1.szEntryName,szEntryName1,RAS_MaxEntryName);

	if (szEntryName2[0])
	{
		RasDialParams2.dwSize = sizeof(RASDIALPARAMS);
		_tcsncpy(RasDialParams2.szEntryName,szEntryName2,RAS_MaxEntryName);
	}
	
	for (i = 0; i < g_AutoDialConn.m_dwRedialAttempts + 1; i++)
	{
		dwRet = AutoDialConnect(pRasDialParams = &RasDialParams1);
		if (!dwRet)
			break;

		if (szEntryName2[0])
		{
			dwRet = AutoDialConnect(pRasDialParams = &RasDialParams2);
			if (!dwRet)
				break;
		}

		// If we're not on final redial attempt, and if pause time was set.
		if (g_AutoDialConn.m_dwRedialPauseTime && i != g_AutoDialConn.m_dwRedialAttempts)
		{
			DEBUGMSG(ZONE_WARN,(TEXT("AutoDial: Connection attempt # %d failed, will sleep %d millisecs before attempting again\r\n"),
								  i,g_AutoDialConn.m_dwRedialPauseTime));
			Sleep(g_AutoDialConn.m_dwRedialPauseTime);
		}
	}

	// We only set fail time if our attempt to make a connection has failed,
	// we don't set this for misconfigured registry settings.
	if (i == g_AutoDialConn.m_dwRedialAttempts + 1)
	{
		DEBUGMSG(ZONE_ERROR,(TEXT("AutoDial: Unable to connect, RasError = %d\r\n"),dwRet));
		SetFailTime();
		myleave(50);
	}

done:
	ASSERT (g_fState == STATE_STARTING_UP);

#ifdef DEBUG
	if (dwRet)
		DEBUGMSG(ZONE_ERROR,(TEXT("AutoDial:StartConnection failed, returning = %d\r\n"),err,dwRet));
	else
		DEBUGMSG(ZONE_CONNECTION,(TEXT("AutoDial:StartConnection SUCCESS!\r\n")));
#endif // 0

	// Don't need CS around this because our state is STATE_STARTING_UP, no other
	// threads can modify g_fState when it's this value.
	if (dwRet)
	{
		g_fState = STATE_OFF;
	}
	else
	{
		GetCurrentFT((FILETIME*) &g_AutoDialConn.m_ftLastActivity);
		g_AutoDialConn.m_hTimerThread = CreateThread(NULL,0,HangUpIdleConnectionThread, NULL, 0, NULL);
		_tcsncpy(g_AutoDialConn.szRasEntryName,pRasDialParams->szEntryName,RAS_MaxEntryName);

#if defined (DEBUG)
		// If CreateThread fails just means we won't have an idle watch dog, 
		// all other parts of the connection will still run OK.
		if (!g_AutoDialConn.m_hTimerThread)
		{
			DEBUGMSG(ZONE_ERROR,(TEXT("Autodial: CreateThread for HangUpIdleConnectionThread failed, GLE=0x%08x\r\n"),GetLastError()));
		}
#endif
		g_fState = STATE_ON;
	}
	return dwRet;		
}





DWORD AutoDialEndConnection(void)
{
	return AutoDialEndConnectionInternal(FALSE);
}

//  Closes the Autodial connection and frees associated resources.
//  fTimedOut = TRUE if there's been a long idle time, 
//  fTimedOut = FALSE if we're being disconnected manually.
DWORD AutoDialEndConnectionInternal(BOOL fTimedOut)
{
	BOOL fExecute = FALSE;
	BOOL fShutdownInOtherProc = FALSE;

	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:+AutoDialEndConnectionInternal\r\n")));

	EnterCriticalSection(&g_GlobalCS);
	if (STATE_ON == g_fState)
	{
		g_fState = STATE_SHUTTING_DOWN;		
		fExecute = TRUE;
	}
	else if (STATE_OFF == g_fState)
	{
		HRASCONN hRasConn;
		RASCONNSTATUS rasConnStatus;
		TCHAR szRasName[RAS_MaxEntryName+1];

		// If we've been connected from another process, still terminate the request.
		if (ERROR_SUCCESS == DetermineIfAutodialConnectionsActive(&rasConnStatus,szRasName,&hRasConn) && 
		    (rasConnStatus.rasconnstate != RASCS_Disconnected))
		{
			MyHangUp(&hRasConn);
		}
		fShutdownInOtherProc = TRUE;
	}
	LeaveCriticalSection(&g_GlobalCS);

	if (!fExecute)
	{
		DEBUGMSG(ZONE_WARN,(TEXT("AutoDial:AutoDialEndConnectionInternal - can not shut down\r\n")));

		// Another thread is disconnecting.  
		if (g_fState == STATE_SHUTTING_DOWN)
			return ERROR_ALREADY_DISCONNECTING;
		else
			return fShutdownInOtherProc ? ERROR_SUCCESS : ERROR_NO_CONNECTION;
	}

    // Reset PublicInterface registry entry to what it was before we initialized the dialup connection
    SetAsPublicInterface(NULL);

	if (g_AutoDialConn.m_hRasConn)
		MyHangUp(&g_AutoDialConn.m_hRasConn);

	if (!fTimedOut)
	{
		// if g_AutoDialConn.m_hTimerEvent = 0, it'd mean that either the CreateThread
		// failed at startup OR that there's a syncronization 
		// that someone else closed it on us.  Not good in either case.
		ASSERT(g_AutoDialConn.m_hTimerEvent);
		if (g_AutoDialConn.m_hTimerEvent)
		{
			SetEvent(g_AutoDialConn.m_hTimerEvent);
			WaitForSingleObject(g_AutoDialConn.m_hTimerThread,INFINITE);
		}
	}
	CloseHandle(g_AutoDialConn.m_hTimerThread);	
	g_AutoDialConn.m_hTimerThread = 0;

	ASSERT (g_fState == STATE_SHUTTING_DOWN);
	g_fState = STATE_OFF;

	g_AutoDialConn.szRasEntryName[0] = 0;

	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:-AutoDialEndConnectionInternal\r\n")));
	return 0;
}

DWORD AutoDialGetConnectionStatus(RASCONNSTATUS *lpRasConnStatus, TCHAR *szRasName)
{
	DWORD dwRet;
	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:+AutoDialGetConnectionStatus\r\n")));

	if (!lpRasConnStatus || !szRasName)
	{
		return ERROR_INVALID_PARAMETER;
	}

	szRasName[0] = 0;
	
	if (!g_fAutodialInitialized)
	{
		DEBUGMSG(ZONE_ERROR,(TEXT("Autodial: AutoDialInitializeModule must be called before AutoDialStartConnection\r\n")));
		ASSERT(0);
		return ERROR_RASAUTO_CANNOT_INITIALIZE;
	}
	
	EnterCriticalSection(&g_GlobalCS);
	if (STATE_ON == g_fState)
	{
		lpRasConnStatus->dwSize = sizeof(RASCONNSTATUS);
		_tcscpy(szRasName,g_AutoDialConn.szRasEntryName);
		dwRet = RasGetConnectStatus(g_AutoDialConn.m_hRasConn,lpRasConnStatus);
	}	
	else
	{
		HRASCONN hRasConn;
		dwRet = DetermineIfAutodialConnectionsActive(lpRasConnStatus,szRasName,&hRasConn);
	}
	LeaveCriticalSection(&g_GlobalCS);

	DEBUGMSG(ZONE_CONNECTION | ZONE_FUNCTION,
	         (TEXT("AutoDial:-AutoDialGetConnectionStatus: szRasName=%s,RAS Err=%d,RasConnState = %d\r\n"),
	                       szRasName,dwRet,lpRasConnStatus->rasconnstate));

	return dwRet;
}

// Private helper functions
DWORD AutoDialConnect(RASDIALPARAMS *pRasDialParams)
{
    static BOOL bWndClassRegistered = FALSE;
#ifdef DEBUG
	DWORD   err = 0;
#endif	
	DWORD   dwRet = 0;
	BOOL    fPassword;	
	HWND    hwndStatus = NULL;
    MSG     msg;
    
	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial: AutoDialConnect attempting to dial %s\r\n"),
	                       pRasDialParams->szEntryName));
	
	if (dwRet = RasGetEntryDialParams(NULL,pRasDialParams,&fPassword))
		myleave(51);

	if(g_AutoDialConn.m_fEnableOnPublicInterface)
	{
	    RASENTRY*   pRasEntry = NULL;
	    DWORD       dwRasEntrySize = 0;
	    
	    if(ERROR_BUFFER_TOO_SMALL != (dwRet = RasGetEntryProperties(NULL, pRasDialParams->szEntryName, pRasEntry, &dwRasEntrySize, NULL, 0)))
	        goto done;
    	
	    if(!(pRasEntry = (RASENTRY*)LocalAlloc(LMEM_FIXED, dwRasEntrySize)))
	    {
	        dwRet = ERROR_OUTOFMEMORY;
	        goto done;
	    }
    	
    	pRasEntry->dwSize = sizeof(RASENTRY);
    	
	    if(ERROR_SUCCESS == (dwRet = RasGetEntryProperties(NULL, pRasDialParams->szEntryName, pRasEntry, &dwRasEntrySize, NULL, 0)))
	    {
            //
            // Set the network adapter associated with this RAS entry as NAT's public interface.
            //
            SetAsPublicInterface(pRasEntry->szDeviceName);
        }
        
        LocalFree(pRasEntry);
        
        if(ERROR_SUCCESS != dwRet)
            goto done;
    }
    
	if(!bWndClassRegistered)
	{
	    if(IsAPIReady(SH_WMGR))
	    {
	        WNDCLASS wc = {0};
	        
	        wc.lpfnWndProc = StatusWindowProc;
	        wc.hInstance = g_hInstance;
	        wc.lpszClassName = WND_CLASS;

	        RegisterClass(&wc);
	        
	        bWndClassRegistered = TRUE;
	    }
	}
	
	if(bWndClassRegistered)
	    hwndStatus = CreateWindowEx(0, WND_CLASS, NULL, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, g_hInstance, 0);
	
	dwRet = RasDial(NULL, NULL, pRasDialParams, hwndStatus ? 0xFFFFFFFF : 0, hwndStatus, &g_AutoDialConn.m_hRasConn);

	if (!dwRet)
	{
		if(!hwndStatus)
		{
		    // Successful connection!
		    DEBUGMSG(ZONE_CONNECTION,(TEXT("AutoDial: Successfully connected to %s\r\n"),
	                                pRasDialParams->szEntryName));
		    return 0;
		}
		else
		{
		    RASCONNSTATUS RasConnStatus;
    		
	        while(GetMessage(&msg, NULL, 0, 0))
	        {
		        TranslateMessage(&msg);
		        DispatchMessage(&msg);
	        }
    	    
		    dwRet = RasGetConnectStatus(g_AutoDialConn.m_hRasConn, &RasConnStatus);
    		
		    if(ERROR_SUCCESS == dwRet)
		    dwRet = RasConnStatus.dwError;
    		   
		    if(ERROR_SUCCESS == dwRet)   
		    {
		        DestroyWindow(hwndStatus);
    		    
		        // Successful connection!
		        DEBUGMSG(ZONE_CONNECTION,(TEXT("AutoDial: Successfully connected to %s\r\n"),
	                                    pRasDialParams->szEntryName));
		        return 0;
		    }
		}
	}
	
	if(hwndStatus)
	    DestroyWindow(hwndStatus);

	// Even though there was an error we still have to free resources associated with RasDial
	if (g_AutoDialConn.m_hRasConn)
		MyHangUp(&g_AutoDialConn.m_hRasConn);
	g_AutoDialConn.m_hRasConn = 0;

done:
#ifdef DEBUG
	DEBUGMSG(ZONE_ERROR,(TEXT("Autodial: AutoDialConnect failed to connect to %s with RAS err = %d, err = %d\r\n"),
		                       pRasDialParams->szEntryName,dwRet,err));	 
#endif
	
	return dwRet;
}


// Read registry settings and populate the pConnInfo structure.
DWORD AutodialReadRegistrySettings(TCHAR *pszDialRasEntryName, TCHAR *pszEntryName1, TCHAR *pszEntryName2, AUTODIAL_CONNECTION_INFO *pConnInfo)
{
#ifdef DEBUG
	DWORD err = 0;
#endif
	DWORD dwRet = 0;
	DWORD dwLen;
	DWORD dwIdleTimeOut;
	DWORD dwFailRetry;

	HKEY hkAutoDial = 0;

	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:+AutoDialReadRegistrySettings\r\n")));
	
	// If they don't even have a registry key setup for this, fail.
	if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, RK_AUTODIAL, 0, KEY_ALL_ACCESS, &hkAutoDial) && !pszDialRasEntryName)
		myretleave(ERROR_RASAUTO_CANNOT_INITIALIZE,60);
	dwLen = sizeof(DWORD);

	// pszDialRasEntryName forces us to use the passed RAS connectiod name, ignoring
	// whether autodial is disabled an the RAS names associated with it.
	if (!pszDialRasEntryName)
	{
		DWORD dwEnabled;
		if (ERROR_SUCCESS != RegQueryValueEx(hkAutoDial,RV_ENABLED,0,0,(LPBYTE)&dwEnabled,&dwLen))
			dwEnabled = 0;	// by default, autodial is disabled.

		if (!dwEnabled)
			myretleave(ERROR_RASAUTO_CANNOT_INITIALIZE,70);
	
		// Read the RAS entry name.  The name MUST be set, or we return an error.
		dwLen = (RAS_MaxEntryName + 1) * sizeof(TCHAR);
		if (ERROR_SUCCESS != RegQueryValueEx(hkAutoDial,RV_ENTRYNAME1,0,0,(LPBYTE) pszEntryName1, &dwLen))
			myretleave(ERROR_RASAUTO_CANNOT_INITIALIZE,80);

		// The 2nd RAS entry name is optional.
		dwLen = (RAS_MaxEntryName + 1) * sizeof(TCHAR);
		if (ERROR_SUCCESS != RegQueryValueEx(hkAutoDial,RV_ENTRYNAME2,0,0,(LPBYTE) pszEntryName2, &dwLen))
			pszEntryName2[0] = 0;
	}
	else
	{
		pszEntryName1[0] = pszEntryName2[0] = 0;
	}
	
	dwLen = sizeof(DWORD);

	if (ERROR_SUCCESS != RegQueryValueEx(hkAutoDial,RV_IDLE_TIMEOUT,0,0,(LPBYTE)&dwIdleTimeOut,&dwLen))
		dwIdleTimeOut = DEFAULT_IDLE_TIMEOUT;
	pConnInfo->m_ftIdleTimeout = dwIdleTimeOut*FILETIME_TO_MILLISECONDS;

	if (ERROR_SUCCESS != RegQueryValueEx(hkAutoDial,RV_FAIL_RETRY_WAIT,0,0,(LPBYTE)&dwFailRetry,&dwLen))
		dwFailRetry = DEFAULT_FAIL_RETRY_WAIT;
	pConnInfo->m_ftFailRetryWait = dwFailRetry*FILETIME_TO_MILLISECONDS;

	if (ERROR_SUCCESS != RegQueryValueEx(hkAutoDial,RV_REDIAL_ATTEMPTS,0,0,(LPBYTE)&pConnInfo->m_dwRedialAttempts,&dwLen))
		pConnInfo->m_dwRedialAttempts = DEFAULT_REDIAL_ATTEMPTS;

	if (ERROR_SUCCESS != RegQueryValueEx(hkAutoDial,RV_REDIAL_PAUSE_TIME,0,0,(LPBYTE)&pConnInfo->m_dwRedialPauseTime,&dwLen))
		pConnInfo->m_dwRedialPauseTime = DEFAULT_REDIAL_PAUSE_TIME;

	if (ERROR_SUCCESS != RegQueryValueEx(hkAutoDial,RV_MAKE_PUBLIC_INTERFACE,0,0,(LPBYTE)&pConnInfo->m_fEnableOnPublicInterface,&dwLen))
		pConnInfo->m_fEnableOnPublicInterface = TRUE; 
		
done:
#ifdef DEBUG
	if (err)
		DEBUGMSG(ZONE_INIT,(TEXT("Autodial: Initialization failed, err = %d, RasError = %d\r\n"),err,dwRet));
	else
		DEBUGMSG(ZONE_INIT,(TEXT("Autodial: Init complete, idleTimeout = %d, FailRetryWait=%d, entry name1 = %s, entry name2 = %s, RedialAttempts = %d, RedialPauseTime = %d\r\n"),
							dwIdleTimeOut,dwFailRetry,
							pszEntryName1 ? pszEntryName1 : TEXT("NULL"),
							pszEntryName2 ? pszEntryName2 : TEXT("NULL"),
							pConnInfo->m_dwRedialAttempts,pConnInfo->m_dwRedialPauseTime));
#endif

	if (hkAutoDial)
		RegCloseKey(hkAutoDial);

	return dwRet;
}



// This function runs in its own thread.  Once the first connection has been established,
// it will wake up every minute and see when the last activity occured.  After
// g_AutoDialConn.m_dwIdleTimeout milliseconds, it will hang up connection.

DWORD HangUpIdleConnectionThread(LPVOID lpv)
{
	RAS_STATS pppStats;
	DWORD cbPPPStats = sizeof(pppStats);
	DWORD dwActualOut;
	__int64    ftCur;		// treated as a FILETIME, current time
	RASCONNSTATUS rasConnStatus;

	pppStats.dwSize      = sizeof(pppStats);
	rasConnStatus.dwSize = sizeof (RASCONNSTATUS);
	
	while ( WAIT_TIMEOUT == WaitForSingleObject(g_AutoDialConn.m_hTimerEvent,HANGUP_SLEEP_INTERVAL))
	{
		// If the connection came down in another way (for instance someone unplugging phone)
		// then we need to shut down AutoDial stuff.  
        EnterCriticalSection(&g_GlobalCS);
		
		ASSERT(g_fState == STATE_ON || g_fState == STATE_SHUTTING_DOWN || g_fState == STATE_STARTING_UP);
		
		if (g_fState == STATE_SHUTTING_DOWN || g_fState == STATE_STARTING_UP)
		{
			// In this state the timer fired after we initiated a shutdown
			// but before we could kill this thread from AutodialEndConnection,
			// so just keep going through loop in this case.
			DEBUGMSG(ZONE_WARN,(TEXT("AutoDial: HangupIdleConnectionThread called when state=STATE_SHUTTING_DOWN or STATE_STARTING_UP\r\n")));
			LeaveCriticalSection(&g_GlobalCS);
			continue;
		}
		
		if (g_fState != STATE_ON)
		{
			DEBUGMSG(ZONE_ERROR | ZONE_CONNECTION,(TEXT("AutoDial: Warning, leaving HangUpIdleConnectionThread because g_fState is not on.  Invalid case\r\n")));
			LeaveCriticalSection(&g_GlobalCS);
			return 0;
		}

		if (ERROR_SUCCESS != RasGetConnectStatus(g_AutoDialConn.m_hRasConn,&rasConnStatus) ||
		   (rasConnStatus.rasconnstate == RASCS_Disconnected))
		{
			DEBUGMSG(ZONE_CONNECTION,(TEXT("AutoDial: Leaving HangUpIdleConnectionThread because connection has been disconnected\r\n")));
			LeaveCriticalSection(&g_GlobalCS);
			AutoDialEndConnectionInternal(TRUE);
			return 0;
		}
		LeaveCriticalSection(&g_GlobalCS);

		GetCurrentFT((FILETIME *) &ftCur);
		RasIOControl(g_AutoDialConn.m_hRasConn,RASCNTL_STATISTICS,0,0,(PVOID) &pppStats,cbPPPStats,&dwActualOut);
		DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:+HangUpIdleConnectionThread.  pppStats.dwBytesXmited=%d, pppStats.dwBytesRcved=%d,g_AutoDialConn.m_dwBytesTxRx=%d\r\n"),
										pppStats.dwBytesXmited,pppStats.dwBytesRcved,g_AutoDialConn.m_dwBytesTxRx));


		// Has there been activity since the last time this thread ran?
		if (pppStats.dwBytesXmited + pppStats.dwBytesRcved - g_AutoDialConn.m_dwBytesTxRx > 0)
		{
			g_AutoDialConn.m_dwBytesTxRx = pppStats.dwBytesXmited +
										   pppStats.dwBytesRcved;
			g_AutoDialConn.m_ftLastActivity = ftCur;
		}
		else if (ftCur - g_AutoDialConn.m_ftLastActivity > g_AutoDialConn.m_ftIdleTimeout)
		{
			DEBUGMSG(ZONE_CONNECTION,(TEXT("Autodial:  Disconnecting RAS connection due to long idle time\r\n")));
			AutoDialEndConnectionInternal(TRUE); 
			break;
		}
	}
	return 0;
}

// Returns TRUE if it found connection (and sets pConnState), FALSE otherwise.
BOOL GetConnectedStatus(TCHAR *szEntryName, RASCONNSTATUS *lpRasConn, HRASCONN *phRasConn)
{
	DWORD 			dwConnections;
	DWORD           dwErr;
	LPRASCONN		pRasConn;
	BYTE			RasConnData[10*sizeof(RASCONN)];
	DWORD       	cb;
	DWORD 			i;

	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:+CheckConnected\r\n")));
	
	// This will create the default entries if the key does not exist.
	RasEnumEntries (NULL, NULL, NULL, &cb, NULL);

	pRasConn = (LPRASCONN) RasConnData;
	pRasConn->dwSize = sizeof(RASCONN);
	cb = sizeof(RasConnData);

	// Get a list of all connections
	if (ERROR_SUCCESS != (dwErr = RasEnumConnections(pRasConn,&cb,&dwConnections)))
	{
		DEBUGMSG(ZONE_ERROR,(L"RasEnumConnections failed, error=0x%08x\r\n",dwErr));
		return FALSE;
	}
		
	// Look through the list for a value of szEntryName.
	for (i = 0; i < dwConnections; i++)
	{
		if (! (_tcsncmp (szEntryName,pRasConn[i].szEntryName,RAS_MaxEntryName)))
			break;
	}

	if (i == dwConnections)
		return FALSE; // did not find szEntryName in current active connections.

	lpRasConn->dwSize = sizeof(RASCONNSTATUS);
	if (ERROR_SUCCESS != (dwErr = RasGetConnectStatus(pRasConn[i].hrasconn,lpRasConn))) 
	{
		DEBUGMSG(ZONE_ERROR,(L"RasGetConnectStatus failed, error=0x%08x\r\n",dwErr));
		return FALSE;
	}

	*phRasConn = pRasConn[i].hrasconn;
	return TRUE;
}

// This function takes the entry name and makes sure that no existing connections
// exist with it yet.

DWORD CheckConnected(TCHAR *szEntryName)
{
	RASCONNSTATUS RasConnStatus;
	HRASCONN      hRasConn;

	if (GetConnectedStatus(szEntryName,&RasConnStatus,&hRasConn) && (RasConnStatus.rasconnstate != RASCS_Disconnected))
	{
		DEBUGMSG(ZONE_ERROR,(TEXT("AutoDial: Checking to see if %s connectoid connection returns ERROR_DIAL_ALREADY_IN_PROGRESS")
		                     TEXT("will not initiate dial\r\n"),szEntryName));
		return ERROR_DIAL_ALREADY_IN_PROGRESS;
	}
	return ERROR_SUCCESS;
}

// It's possible another process is using autodial (i.e. device.exe
// IPNat loaded autodial, but we're doing this query from services.exe NATAdmin).
// In this case see if the connection is active and fill in state appropriatly.
DWORD DetermineIfAutodialConnectionsActive(RASCONNSTATUS *lpRasConnStatus, TCHAR *szRasName, HRASCONN *phRasConn)
{
	AUTODIAL_CONNECTION_INFO adConnInfo;
	TCHAR 			szEntryName1[RAS_MaxEntryName + 1];
	TCHAR 			szEntryName2[RAS_MaxEntryName + 1];
	DWORD           dwRet;

	szEntryName1[0] = szEntryName2[0] = 0;

	if (ERROR_SUCCESS == AutodialReadRegistrySettings(NULL,szEntryName1,szEntryName2,&adConnInfo)) 
	{
		DEBUGCHK(szEntryName1[0]);

		if (GetConnectedStatus(szEntryName1,lpRasConnStatus,phRasConn)) 
		{
			_tcscpy(szRasName,szEntryName1);
			dwRet = ERROR_SUCCESS;
		}
		else if (szEntryName2[0] && GetConnectedStatus(szEntryName2,lpRasConnStatus,phRasConn)) 
		{
			_tcscpy(szRasName,szEntryName2);
			dwRet = ERROR_SUCCESS;
		}
		else 
		{
			dwRet = ERROR_NO_CONNECTION;
		}
	}
	else
	{
		lpRasConnStatus->rasconnstate = RASCS_Disconnected;
		szRasName[0] = 0;
		dwRet = ERROR_NO_CONNECTION;
	}
	return dwRet;
}



// SetFailTime and CheckFailRetry are functions that help maintain and check
// the last time of failure list.  The idea here is that we don't want autodial
// to keep trying to dial again and again if it constantly fails.

// Called on failure of Autodial to establish a connection.
void SetFailTime(void)
{
	__int64    ftNow;		// treated as a FILETIME
	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:+SetFailTime\r\n")));

	if (!g_AutoDialConn.m_ftFailRetryWait)
		return;
	
	GetCurrentFT((FILETIME *) &ftNow);
	if (ftNow - g_AutoDialConn.g_ftLastFailTime < g_AutoDialConn.m_ftFailRetryWait)
	{
		g_AutoDialConn.m_iLastFailIndex += 1;
	}
	else
	{
		g_AutoDialConn.g_ftLastFailTime = ftNow;
		g_AutoDialConn.m_iLastFailIndex = 1;
	}
}

// Called on initilization, makes sure numerous calls to the autodialer haven't
// failed in a short time frame.
BOOL CheckFailRetry(void)
{
	__int64    ftNow;	// treated as a FILETIME

	DEBUGMSG(ZONE_FUNCTION,(TEXT("AutoDial:+CheckFailRetry\r\n")));
	if (!g_AutoDialConn.m_ftFailRetryWait || 
	     g_AutoDialConn.m_iLastFailIndex < FAIL_RETRY_ATTEMPTS)
	{
		return TRUE;
	}

	GetCurrentFT((FILETIME *) &ftNow);
	
	if (ftNow - g_AutoDialConn.g_ftLastFailTime < g_AutoDialConn.m_ftFailRetryWait)
	{
		DEBUGMSG(ZONE_ERROR,(TEXT("Autodial:CheckFailRetry has failed, too many retries\r\n")));
		return FALSE;
	}
	
	return TRUE;
}

typedef DWORD (WINAPI *PFN_GETINTERFACEINFO)(IN PIP_INTERFACE_INFO pIfTable, OUT PULONG dwOutBufLen);
typedef DWORD (WINAPI *PFN_IPRELEASEADDRESS)(PIP_ADAPTER_INDEX_MAP AdapterInfo);
typedef DWORD (WINAPI *PFN_IPRENEWADDRESS)(PIP_ADAPTER_INDEX_MAP AdapterInfo);

PFN_GETINTERFACEINFO pfnGetInterfaceInfo;
PFN_IPRELEASEADDRESS pfnIpReleaseAddress;
PFN_IPRENEWADDRESS   pfnIpRenewAddress;

// Connection Sharing (i.e. NAT, i.e. ICS, i.e. Home Gateway) determines whether
// to enable Address translation when it receives a LanaUp call on a given
// interface based on whether the reg key HKLM\Comm\ConnectionSharing\PublicInterface
// mateches the name of interface coming up.  Before we call RasDial we reset
// this reg key.

void SetAsPublicInterface(TCHAR *szEntryName)
{
	HKEY            hKey;
	static TCHAR    PrimaryPublicInterface[257];

	if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, ICSRegKey, 0, 
	                                  KEY_ALL_ACCESS, &hKey))
	{
		return;
	}
	
	if(szEntryName != NULL)
	{
	    //
	    // Store the name of our primary public interface before overriding it with autodial adapter
	    //
	    if(PrimaryPublicInterface[0] == TEXT('\x0'))
	    {
	        DWORD dw = sizeof(PrimaryPublicInterface);
	    
	        RegQueryValueEx(hKey, ICSRegPublicInterface, 0, NULL, (LPBYTE)PrimaryPublicInterface, &dw);
	    }
	    
	    //
	    // Set PublicInterface registry value to the name of autodial network adapter
	    //
	    RegSetValueEx(hKey, ICSRegPublicInterface, 0, REG_SZ, (LPBYTE)szEntryName, sizeof(TCHAR)*(lstrlen(szEntryName)+1));
	}
	else
	{
	    //
	    // Restore PublicInterface registry value to our primary public interface
	    //
	    RegSetValueEx(hKey, ICSRegPublicInterface, 0, REG_SZ, (LPBYTE)PrimaryPublicInterface, sizeof(TCHAR)*(lstrlen(PrimaryPublicInterface)+1));
	    
	    if(pfnGetInterfaceInfo && pfnIpReleaseAddress && pfnIpRenewAddress)
	    {
	        DWORD dwSize = 0;
	        
	        if(ERROR_INSUFFICIENT_BUFFER == (*pfnGetInterfaceInfo)(NULL, &dwSize))
	        {
	            PIP_INTERFACE_INFO pIfTable;
	            
	            if(pIfTable = (PIP_INTERFACE_INFO)LocalAlloc(LMEM_FIXED, dwSize))
	            {
	                if(ERROR_SUCCESS == (*pfnGetInterfaceInfo)(pIfTable, &dwSize))
	                {
	                    int i;
    	                
	                    for(i = 0; i < pIfTable->NumAdapters; ++i)
	                    {
	                        if(0 == wcscmp(pIfTable->Adapter[i].Name, PrimaryPublicInterface))
	                        {
	                            //
	                            // Release and immediately renew IP address of the primary public interface
	                            // so that NAT can reinitialize itself to use the primary interface.
	                            //
	                            (*pfnIpReleaseAddress)(&pIfTable->Adapter[i]);
	                            (*pfnIpRenewAddress)(&pIfTable->Adapter[i]);
	                            break;
	                        }
	                    }
	                }
    	            
	                LocalFree(pIfTable);
	            }
	        }
	    }
	    
	    PrimaryPublicInterface[0] = TEXT('\x0');
	}
	
	RegCloseKey(hKey);
	return;
}



BOOL WINAPI DllEntry( HANDLE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
	static HANDLE hIphplapi = NULL;
	
	switch(fdwReason) 
	{
        case DLL_PROCESS_ATTACH:
#ifdef UNDER_CE
			DEBUGREGISTER((HINSTANCE)hInstDll);
#endif		
            g_hInstance = hInstDll;
            
            DisableThreadLibraryCalls ((HMODULE)hInstDll);
            
            if(hIphplapi = LoadLibrary(L"iphlpapi.dll"))
            {
                pfnGetInterfaceInfo = (PFN_GETINTERFACEINFO) GetProcAddress(hIphplapi, L"GetInterfaceInfo");
                pfnIpReleaseAddress = (PFN_IPRELEASEADDRESS) GetProcAddress(hIphplapi, L"IpReleaseAddress");
                pfnIpRenewAddress = (PFN_IPRENEWADDRESS) GetProcAddress(hIphplapi, L"IpRenewAddress");
            }
		    break;

		case DLL_PROCESS_DETACH:
		    
		    if(hIphplapi)
		        FreeLibrary(hIphplapi);
		
		break;
	}
	return TRUE;
}
