//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************/
/**								Microsoft Windows							**/
/*****************************************************************************/

/*
	init.c

  DESCRIPTION:
	initialization routines for DhcpV6Lite

*/

#include "dhcpv6p.h"
#include "dhcpv6l.h"
//#include "precomp.h"

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("DHCP6"), {
    TEXT("Init"),	TEXT("Timer"),		TEXT("AutoIP"),		TEXT("Unused"),
    TEXT("Recv"),   TEXT("Send"),		TEXT("Request"),	TEXT("Media Sense"),
    TEXT("Notify"),	TEXT("Buffer"),		TEXT("Interface"),  TEXT("Misc"),
    TEXT("Alloc"),  TEXT("Function"),   TEXT("Warning"),    TEXT("Error") },
    0xc000
}; 
#endif

/*
typedef DWORD (*PFNDWORD2)(void *, void *);

PFNDWORD2 v_pfnIPDispatchDeviceControl;

PFNVOID *v_apSocketFns, *v_apAfdFns;

CRITICAL_SECTION		v_ProtocolListLock;
PDHCP_PROTOCOL_CONTEXT	v_ProtocolList;
HANDLE					v_TcpipDriver;
int						v_DhcpInitDelay;

extern CTEEvent v_DhcpEvent;
void DhcpEventWorker(CTEEvent * Event, void * Context);


STATUS HandleMediaConnect(unsigned Context, PTSTR pAdapter);
STATUS HandleMediaDisconnect(unsigned Context, PTSTR pAdapter);
*/


#if 0
// Handle by which to notify applications of changes to IP data structures.
PDHCP_PROTOCOL_CONTEXT
DhcpRegister(
	PWCHAR				wszProtocolName,
	PFNSetDHCPNTE		pfnSetNTE,
	PFNIPSetNTEAddr		pfnSetAddr, 
	PFN_DHCP_NOTIFY		*ppDhcpNotify)
//
//	If successful, returns a pointer to a DHCP protocol context
//	which the caller will pass to subsequent calls to DhcpNotify.
//
{

	PDHCP_PROTOCOL_CONTEXT pContext;

	DEBUGMSG(ZONE_INIT | ZONE_WARN, (TEXT("+DhcpRegister:\n")));

	EnterCriticalSection(&v_ProtocolListLock);

	pContext = FindProtocolContextByName(wszProtocolName);
	if (pContext == NULL) {
		pContext = LocalAlloc(LPTR, sizeof(*pContext));
		if (pContext) {
			pContext->pNext = v_ProtocolList;
			v_ProtocolList = pContext;
			wcsncpy(pContext->wszProtocolName, wszProtocolName, MAX_DHCP_PROTOCOL_NAME_LEN);
		}
	}

	if (pContext) {
		pContext->pfnSetNTE = pfnSetNTE;
		pContext->pfnSetAddr = pfnSetAddr;
		*ppDhcpNotify = DhcpNotify;
	}

	LeaveCriticalSection(&v_ProtocolListLock);

	DEBUGMSG(ZONE_INIT | ZONE_WARN,  (TEXT("-DhcpRegister: Ret = %x\n"), pContext));

	return pContext;
}	// DhcpRegister()
#endif


VOID
InitGlobalsAtStartup(
    )
{
    //
    // Init globals that aren't cleared on service stop to make sure
    // everything's in a known state on start.  This allows us to
    // stop/restart without having our DLL unloaded/reloaded first.
    //

    gbDHCPV6RPCServerUp           = FALSE;
    ghServiceStopEvent            = NULL;
    gdwServersListening           = 0;
    gbServerListenSection         = FALSE;
    gpDHCPV6SD                    = NULL;
    gbDHCPV6Section               = FALSE;

    gpDhcpV6IniInterfaceTblRWLock    = &gDhcpV6IniInterfaceTblRWLock;
    gpIniInterfaceHandle            = NULL;

    gbAdapterInit                 = FALSE;
    gbAdapterPendingDeleteInit    = FALSE;
    gbTimerModuleInit             = FALSE;

    memset(&MCastSockAddr, 0, sizeof(MCastSockAddr));

    gbReplyModuleInit             = FALSE;
    gbEventModule                 = FALSE;

    return;
}

DWORD
InitDHCPV6ThruRegistry(
    )
{
    DWORD dwError = 0;

    gbDHCPV6PDEnabled = TRUE;
    return (dwError);
}

DWORD
InitDHCPV6Globals(
    )
{
    DWORD dwError = 0;
    SECURITY_ATTRIBUTES SecurityAttributes;
    ULONG uAddressLength = 0;


    DhcpV6Trace(DHCPV6_MISC, DHCPV6_LOG_LEVEL_TRACE, ("Initializing Globals"));

    dwError = InitializeDHCPV6Security(&gpDHCPV6SD);
    BAIL_ON_WIN32_ERROR(dwError);

    memset(&SecurityAttributes, 0, sizeof(SECURITY_ATTRIBUTES));

    SecurityAttributes.nLength = sizeof(SecurityAttributes);
    SecurityAttributes.lpSecurityDescriptor = NULL;
    SecurityAttributes.bInheritHandle = TRUE;

    ghServiceStopEvent = CreateEvent(
                             &SecurityAttributes,
                             TRUE,
                             FALSE,
                             NULL
                             );
    if (!ghServiceStopEvent) {
        dwError = GetLastError();
        BAIL_ON_WIN32_ERROR(dwError);
    }

    __try {
        InitializeCriticalSection(&gcServerListenSection);
        gbServerListenSection = TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = GetExceptionCode();
        BAIL_ON_WIN32_ERROR(dwError);
    }

    gdwServersListening = 0;

    __try {
        InitializeCriticalSection(&gcDHCPV6Section);
        gbDHCPV6Section = TRUE;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        dwError = GetExceptionCode();
        BAIL_ON_WIN32_ERROR(dwError);
    }

    dwError = InitializeRWLock(gpDhcpV6IniInterfaceTblRWLock);
    BAIL_ON_WIN32_ERROR(dwError);

    // Adapter List
    dwError = InitializeRWLock(gpAdapterRWLock);
    BAIL_ON_WIN32_ERROR(dwError);

    InitializeListHead(&AdapterList);
    gbAdapterInit = TRUE;

    // Pending Delete Adapters
    dwError = InitializeRWLock(gpAdapterPendingDeleteRWLock);
    BAIL_ON_WIN32_ERROR(dwError);

    InitializeListHead(&AdapterPendingDeleteList);
    gbAdapterPendingDeleteInit = TRUE;


    // Init Winsock2
    dwError = WSAStartup(0x202, gpWSAData);
    BAIL_ON_WIN32_ERROR(dwError);

//    MCastSockAddr.sin6_addr.u = { 0xff02, 0, 0, 0, 0, 0, 0x0001, 0x0002};
    MCastSockAddr.sin6_addr.u.Word[0] = 0x2ff;
    MCastSockAddr.sin6_addr.u.Word[6] = 0x0100;
    MCastSockAddr.sin6_addr.u.Word[7] = 0x0200;

/*
    dwError = RtlIpv6StringToAddressEx(
                All_DHCP_Relay_Agents_and_Servers,
                &MCastSockAddr.sin6_addr,
                &MCastSockAddr.sin6_scope_id,
                &MCastSockAddr.sin6_port
                );
*/
    BAIL_ON_WIN32_ERROR(dwError);

    MCastSockAddr.sin6_family = AF_INET6;
    MCastSockAddr.sin6_port = htons(DHCPV6_SERVER_LISTEN_PORT);

    dwError = InitDhcpV6EventMgr(gpDhcpV6EventModule);
    BAIL_ON_WIN32_ERROR(dwError);
    gbEventModule = TRUE;

    dwError = InitDhcpV6TimerModule(gpDhcpV6TimerModule);
    BAIL_ON_WIN32_ERROR(dwError);
    gbTimerModuleInit = TRUE;

    dwError = InitDhcpV6ReplyManager(gpDhcpV6ReplyModule);
    BAIL_ON_WIN32_ERROR(dwError);
    gbReplyModuleInit = TRUE;

    dwError = DhcpV6InitWMI();
    BAIL_ON_WIN32_ERROR(dwError);

#ifdef UNDER_CE
    dwError = DhcpV6InitTdiNotifications();
    BAIL_ON_WIN32_ERROR(dwError);
#endif

//    srand((unsigned)time(NULL));

    DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_TRACE, ("End Initializing Globals"));

    return dwError;

error:

    DhcpV6Trace(DHCPV6_PNP, DHCPV6_LOG_LEVEL_FATAL,
                ("FAILED Global Init Error: %!status!, EventModule: %x, TimerModule: %x, ReplyModule: %x",
                 dwError, gbEventModule, gbTimerModuleInit, gbReplyModuleInit));

    return dwError;
}

//
// This is the calling function when residing in an existing service
//
DWORD
DhcpV6Startup(
    )
{
    DWORD dwError = 0;


    InitGlobalsAtStartup();

    dwError = InitDHCPV6ThruRegistry();
    BAIL_ON_WIN32_ERROR(dwError);

    dwError = InitDHCPV6Globals();
    BAIL_ON_WIN32_ERROR(dwError);

    BAIL_ON_WIN32_ERROR( DHCPv6ManagePrefixInit() ? 0 : 1);

    dwError = InitDHCPV6AdaptAtStartup();
    BAIL_ON_WIN32_ERROR(dwError);

//    dwError = DHCPV6StartRPCServer();
//    BAIL_ON_WIN32_ERROR(dwError);

    return dwError;

error:

    DhcpV6Shutdown(dwError);

    return dwError;
}


VOID
DhcpV6Shutdown(
    DWORD dwErrorCode
    )
{
    DWORD dwError = 0;


    if (gbDHCPV6RPCServerUp) {
        gbDHCPV6RPCServerUp = FALSE;
//        DHCPV6StopRPCServer();
    }

    //
    // Signal adapters going down
    //
    if (gbAdapterInit) {
        gbAdapterInit = FALSE;
//        ShutdownAllDHCPV6Adapt();
    }

    AcquireSharedLock(gpAdapterPendingDeleteRWLock);
    while (!IsListEmpty(&AdapterPendingDeleteList)) {
        ReleaseSharedLock(gpAdapterPendingDeleteRWLock);

        Sleep(2000);

        AcquireSharedLock(gpAdapterPendingDeleteRWLock);
    }
    ReleaseSharedLock(gpAdapterPendingDeleteRWLock);

    DhcpV6DeInitWMI();

    if (gbReplyModuleInit) {
        gbReplyModuleInit = FALSE;
        DeInitDhcpV6ReplyManager(gpDhcpV6ReplyModule);
    }

    if (gbTimerModuleInit) {
        gbTimerModuleInit = FALSE;
        DeInitDhcpV6TimerModule(gpDhcpV6TimerModule);
    }

    // Clean up Winsock2
    WSACleanup();

    if (gpIniInterfaceHandle) {
        DestroyIniInterfaceHandleList(gpIniInterfaceHandle);
        gpIniInterfaceHandle = NULL;
    }

    DestroyRWLock(gpAdapterPendingDeleteRWLock);
    DestroyRWLock(gpAdapterRWLock);

    DHCPv6ManagePrefixDeinit();

    return;
}


BOOL DllEntry (HANDLE hinstDLL, DWORD Op, PVOID lpvReserved) {
	BOOL Status = TRUE;

	switch (Op) {
	case DLL_PROCESS_ATTACH:
		DEBUGREGISTER(hinstDLL);
		DEBUGMSG (ZONE_INIT|ZONE_WARN, 
			(TEXT("Dhcp: dllentry() %d\r\n"), hinstDLL));

//		InitializeCriticalSection(&v_ProtocolListLock);
//		CTEInitEvent(&v_DhcpEvent, DhcpEventWorker);
		DisableThreadLibraryCalls ((HMODULE)hinstDLL);
		break;

	case DLL_PROCESS_DETACH:
		break;

	case DLL_THREAD_DETACH :
		break;

	case DLL_THREAD_ATTACH :
		break;
	
	default :
		break;
	}
	return Status;
}	// dllentry()

extern unsigned int v_cXid;

extern BOOL WCE_Initialize();

#define DHCPV6_DEV_PREFIX   TEXT("DP6")
#define DHCPV6_DEV_INDEX    0
#define DHCPV6_DEVICE_NAME  TEXT("DP60:")



//	IMPORTANT: The name of this fn must be the same as the HelperName
//	note: the parameters for registration is different: Unused, OpCode, 
//			pVTable, cVTable, pMTable, cMTable, pIndexNum

BOOL DhcpV6L(DWORD Unused, DWORD OpCode, 
			 PBYTE pName, DWORD cBuf1,
			 PBYTE pBuf1, DWORD cBuf2,
			 PDWORD pBuf2) {
	STATUS	Status = DHCP_SUCCESS;
	BOOL	fStatus = TRUE;
	PWSTR	pAdapterName = (PWSTR)pName;
	//unsigned int    cRand, cRand2;
	//FILETIME	CurTime;
	//__int64 cBigRand;
	HKEY	hKey;
	DWORD	Disp;
	DWORD	Index;
	LPCWSTR DHCPV6DevPath = L"TCPIP6\\DHCPV6L";

	switch(OpCode) {
	case DHCP_REGISTER:
//		v_apAfdFns = (PFNVOID *)pName;
//		v_apSocketFns = (PFNVOID *)pBuf1;
		*pBuf2 = 0;
//		CTEInitLock(&v_GlobalListLock);
//        InitializeListHead(&v_EstablishedList);
/*
		v_ProtocolList = NULL;

		GetCurrentFT(&CurTime);
		cRand = (CurTime.dwHighDateTime & 0x788) | (CurTime.dwLowDateTime & 0xf800);
		cBigRand = CeGetRandomSeed();
		cRand2 = (uint)(cBigRand >> 32);
		cRand2 ^= (uint)(cBigRand & 0xffFFffFF);
		v_cXid = cRand ^ cRand2;

		hRes = RegOpenKeyEx (HKEY_LOCAL_MACHINE, TEXT("Comm\\Tcpip\\Parms"), 
			0, 0, &hKey);
		if (ERROR_SUCCESS == hRes) {
			GetRegDWORDValue(hKey, TEXT("DhcpGlobalInitDelayInterval"), 
				&v_DhcpInitDelay);
			RegCloseKey(hKey);
		}

		DEBUGMSG(ZONE_INIT, (TEXT("\tDhcpV6L:Register: FT %d, Xid %d\r\n"),
		    cRand, v_cXid));
*/

        // Create Reg keys for activate device...
        if (ERROR_SUCCESS == 
            RegCreateKeyEx(HKEY_LOCAL_MACHINE, DHCPV6DevPath, 0, NULL, 0, 0,
                NULL, &hKey, &Disp)) {
            RegSetValueEx(hKey, L"Dll", 0, REG_SZ, (BYTE *) L"dhcpv6L.dll", 24);
            RegSetValueEx(hKey, L"Prefix", 0, REG_SZ, 
                (BYTE *) DHCPV6_DEV_PREFIX, 8);
            Index = DHCPV6_DEV_INDEX;
            RegSetValueEx(hKey, L"Index", 0, REG_DWORD, (BYTE *) &Index, 
                sizeof(DWORD));
            RegCloseKey(hKey);
        }

        if (! ActivateDevice(DHCPV6DevPath, 0)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("DhcpV6L: ActivateDevice call failed!\r\n")));
        }
    	
        // Timer Q support
        WCE_Initialize();

		return fStatus;

	case DHCP_PROBE:
		return TRUE;
		break;

	default:
		fStatus = FALSE;
		break;
	}
	
	if (Status != DHCP_SUCCESS) {
		SetLastError(Status);
		fStatus = FALSE;
	}

	return fStatus;
}	// DhcpV6L()

void ReadyToGo() {
    HANDLE  hThread;
    DWORD   dwThreadId;
	
	DEBUGMSG(ZONE_INIT, (TEXT("*DhcpV6L:ReadyToGo\r\n")));

	if (hThread = CreateThread(NULL, 0, 
		(LPTHREAD_START_ROUTINE)DhcpV6Startup, NULL, 0, &dwThreadId)) {

        CloseHandle(hThread);
   }

}



// Return Non-Zero for success, will be passed to Deinit and Open

DWORD
DP6_Init(
	DWORD dwContext) // Specifies a pointer to a string containing the registry path to the active key for the installable device driver.
{
   DEBUGMSG(1, (TEXT("+DP6_Init(%x)\n"), dwContext));

   return 1;
}


//	@func PVOID | DP6_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from IP6_Init call
//  @rdesc	Returns TRUE for success, FALSE for failure.
//	@remark	Routine exported by a device driver.  "NDT" is the string
//			passed in as lpszType in RegisterDevice

BOOL
DP6_Deinit(DWORD dwData)
{
	return TRUE;
}

//	@func PVOID | IP6_Open		| Device open routine
//  @parm DWORD | dwData		| value returned from IP6_Init call
//  @parm DWORD | dwAccess		| requested access (combination of GENERIC_READ
//								  and GENERIC_WRITE)
//  @parm DWORD | dwShareMode	| requested share mode (combination of
//								  FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc	Returns a DWORD which will be passed to Read, Write, etc or NULL if
//			unable to open device.
//	@remark	Routine exported by a device driver.  "NDT" is the string passed
//			in as lpszType in RegisterDevice
DWORD
DP6_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode)
{
	// Return Non-Null for success, other will be passed to Read etc.
	return 2;
}


BOOL
DP6_Close (DWORD dwData) 
{
	//DEBUGMSG (ZONE_TRACE, (TEXT("IP6_Close(0x%X)\r\n"), dwData));
	return TRUE;
}



//  @rdesc	Returns 0 for end of file, -1 for error, otherwise the number of
//			bytes read.  The length returned is guaranteed to be the length
//			requested unless end of file or an error condition occurs.

DWORD
DP6_Read (DWORD dwData, LPVOID pBuf, DWORD Len)
{
	// Return length read
	return 0;
}


//  @rdesc	Returns -1 for error, otherwise the number of bytes written.  The
//			length returned is guaranteed to be the length requested unless an
//			error condition occurs.

DWORD
DP6_Write (DWORD dwData, LPCVOID pBuf, DWORD Len)
{
	// return number of bytes written (or -1 for error)
	return 0;
}


//  @rdesc	Returns current position relative to start of file, or -1 on error
//	@remark	Routine exported by a device driver.  "NDT" is the string passed
//		 in as lpszType in RegisterDevice

DWORD
DP6_Seek (DWORD dwData, long pos, DWORD type)
{
	// return an error
	return (DWORD)-1;
}

//	@func void | IP6_PowerUp | Device powerup routine
//	@comm	Called to restore device from suspend mode.  You cannot call any
//			routines aside from those in your dll in this call.

void
DP6_PowerUp(void)
{
}
//	@func void | IP6_PowerDown | Device powerdown routine
//	@comm	Called to suspend device.  You cannot call any routines aside from
//			those in your dll in this call.
void
DP6_PowerDown(void)
{
}


BOOL IoctlEnumDhcpV6Interfaces(PBYTE pBufIn, DWORD dwLenIn, void * pBufOut, 
    DWORD dwLenOut, PDWORD pdwActualOut) {
    
    DHCPV6_IOCTL_ENUM_PARAMS    *pParams = pBufOut;

    if (dwLenOut >= sizeof(*pParams)) {    
        pParams->Status = EnumDhcpV6Interfaces(NULL, pParams->dwVersion, NULL,
            pParams->dwPreferredNumEntries, 
            (void *)&(pParams->pDhcpV6Interfaces),
            &pParams->dwNumInterfaces, &pParams->dwTotalNumInterfaces,
            &pParams->dwResumeHandle, &pParams->pvReserved);

        pParams->CallType = IOCTL_DHCPV6L_ENUMERATE_INTERFACES;
    } else {
        pParams->Status = ERROR_OUTOFMEMORY;
    }

    return (ERROR_SUCCESS == pParams->Status);

}   // IoctlEnumDhcpV6Interfaces()

BOOL IoctlOpenDhcpV6InterfaceHandle(PBYTE pBufIn, DWORD dwLenIn, 
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {
    
    DHCPV6_IOCTL_INTERFACE_PARAMS   *pParams = pBufOut;

    if (dwLenOut >= sizeof(*pParams)) {    
        pParams->Status = OpenDhcpV6InterfaceHandle(NULL, pParams->dwVersion,
            (void *)&pParams->DhcpV6Interface, &pParams->pvReserved,
            &pParams->hInterface);
        pParams->CallType = IOCTL_DHCPV6L_OPEN_HANDLE;
    } else {
        pParams->Status = ERROR_OUTOFMEMORY;
    }

    return (ERROR_SUCCESS == pParams->Status);

}   // IoctlOpenDhcpV6InterfaceHandle()


BOOL IoctlCloseDhcpV6InterfaceHandle(PBYTE pBufIn, DWORD dwLenIn, 
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {
    
    DHCPV6_IOCTL_INTERFACE_PARAMS   *pParams = pBufOut;

    if (dwLenOut >= sizeof(*pParams)) {    
        pParams->Status = CloseDhcpV6InterfaceHandle(pParams->hInterface);
        pParams->CallType = IOCTL_DHCPV6L_CLOSE_HANDLE;
    } else {
        pParams->Status = ERROR_OUTOFMEMORY;
    }

    return (ERROR_SUCCESS == pParams->Status);

}   // IoctlCloseDhcpV6InterfaceHandle()


BOOL IoctlPerformDhcpV6Refresh(PBYTE pBufIn, DWORD dwLenIn, 
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {
    
    DHCPV6_IOCTL_INTERFACE_PARAMS   *pParams = pBufOut;

    if (dwLenOut >= sizeof(*pParams)) {    
        pParams->Status = PerformDhcpV6Refresh(pParams->hInterface, 
            pParams->dwVersion, &pParams->pvReserved);
        pParams->CallType = IOCTL_DHCPV6L_REFRESH;
    } else {
        pParams->Status = ERROR_OUTOFMEMORY;
    }

    return (ERROR_SUCCESS == pParams->Status);

}   // IoctlPerformDhcpV6Refresh()


BOOL IoctlGetDhcpV6DNSList(PBYTE pBufIn, DWORD dwLenIn, 
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {
    
    DHCPV6_IOCTL_INTERFACE_PARAMS   *pParams = pBufOut;

    if (dwLenOut >= sizeof(*pParams)) {    
        pParams->Status = GetDhcpV6DNSList(pParams->hInterface, 
            pParams->dwVersion, (void *)&pParams->pDhcpV6DNSList, 
            &pParams->pvReserved);
        pParams->CallType = IOCTL_DHCPV6L_GET_DNS_LIST;
    } else {
        pParams->Status = ERROR_OUTOFMEMORY;
    }

    return (ERROR_SUCCESS == pParams->Status);

}   // IoctlGetDhcpV6DNSList()

BOOL IoctlGetDhcpV6PDList(PBYTE pBufIn, DWORD dwLenIn, 
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {
    
    DHCPV6_IOCTL_INTERFACE_PARAMS   *pParams = pBufOut;

    if (dwLenOut >= sizeof(*pParams)) {    
        pParams->Status = GetDhcpV6PDList(pParams->hInterface, 
            pParams->dwVersion, (void *)&pParams->pPrefixD, 
            &pParams->pvReserved);
        pParams->CallType = IOCTL_DHCPV6L_GET_PD_LIST;
    } else {
        pParams->Status = ERROR_OUTOFMEMORY;
    }

    return (ERROR_SUCCESS == pParams->Status);

}   // IoctlGetDhcpV6PDList()


BOOL IoctlGetDhcpV6DomainList(PBYTE pBufIn, DWORD dwLenIn, 
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {
    
    DHCPV6_IOCTL_INTERFACE_PARAMS   *pParams = pBufOut;

    if (dwLenOut >= sizeof(*pParams)) {    
        pParams->Status = GetDhcpV6DomainList(pParams->hInterface, 
            pParams->dwVersion, (void *)&pParams->pDomainList, 
            &pParams->pvReserved);
        pParams->CallType = IOCTL_DHCPV6L_GET_DOMAIN_LIST;
    } else {
        pParams->Status = ERROR_OUTOFMEMORY;
    }

    return (ERROR_SUCCESS == pParams->Status);

}   // IoctlGetDhcpV6DomainList()


BOOL IoctlDhcpV6FreeMem(PBYTE pBufIn, DWORD dwLenIn, 
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {

    DHCPV6_IOCTL_HDR *pHdr = (DHCPV6_IOCTL_HDR *)pBufIn;
    DWORD   i;
    DHCPV6_IOCTL_ENUM_PARAMS *pEnum;
    DHCPV6_IOCTL_INTERFACE_PARAMS   *pParams = 
        (DHCPV6_IOCTL_INTERFACE_PARAMS *)pBufIn;

    if (dwLenIn < sizeof(*pHdr)) {
        pHdr->Status = ERROR_INSUFFICIENT_BUFFER;
        goto Exit;
    }

    pHdr->Status = ERROR_SUCCESS;

    switch (pHdr->CallType) {
    case IOCTL_DHCPV6L_OPEN_HANDLE:
    case IOCTL_DHCPV6L_CLOSE_HANDLE:
    case IOCTL_DHCPV6L_REFRESH:

        // nothing to free in these cases!

        break;

        
    case IOCTL_DHCPV6L_ENUMERATE_INTERFACES:
        pEnum = (DHCPV6_IOCTL_ENUM_PARAMS *)pBufIn;
        if (dwLenIn < sizeof(*pEnum)) {
            pHdr->Status = ERROR_INSUFFICIENT_BUFFER;
            goto Exit;
        }

        if (pEnum->pDhcpV6Interfaces) {
            for (i = 0; i < pEnum->dwNumInterfaces; i++) {
                // DHCPv6FreeBuffer currently checks for NULL, if this
                // changes we'll need to check for NULL here!
                DHCPv6FreeBuffer(pEnum->pDhcpV6Interfaces[i].pszDescription);
            }
            DHCPv6FreeBuffer(pEnum->pDhcpV6Interfaces);
            pEnum->pDhcpV6Interfaces = NULL;
        }
        break;

    case IOCTL_DHCPV6L_GET_DNS_LIST:
        if (dwLenIn < sizeof(*pParams)) {
            pHdr->Status = ERROR_INSUFFICIENT_BUFFER;
            goto Exit;
        }

        if (pParams->pDhcpV6DNSList) {
            DHCPv6FreeBuffer(pParams->pDhcpV6DNSList->pDhcpV6DNS);
            DHCPv6FreeBuffer(pParams->pDhcpV6DNSList);
            pParams->pDhcpV6DNSList = NULL;
        }        
        break;

    case IOCTL_DHCPV6L_GET_PD_LIST:
        if (dwLenIn < sizeof(*pParams)) {
            pHdr->Status = ERROR_INSUFFICIENT_BUFFER;
            goto Exit;
        }

        if (pParams->pPrefixD) {
            DHCPv6FreeBuffer(pParams->pPrefixD);
            pParams->pPrefixD = NULL;
        }
        break;

    case IOCTL_DHCPV6L_GET_DOMAIN_LIST:
        if (dwLenIn < sizeof(*pParams)) {
            pHdr->Status = ERROR_INSUFFICIENT_BUFFER;
            goto Exit;
        }

        if (pParams->pDomainList) {
            DHCPv6FreeBuffer(pParams->pDomainList);
            pParams->pDomainList = NULL;
        }
        break;

    default:
        pHdr->Status = ERROR_INVALID_DATA;
        break;
    }

Exit:
    return (ERROR_SUCCESS == pHdr->Status);

}   // IoctlGetDhcpV6DomainList()


//	@func BOOL | DP6_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from DP6_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device
//  @rdesc	Returns TRUE for success, FALSE for failure
//	@remark	Routine exported by a device driver.  "NDT" is the string passed
//		in as lpszType in RegisterDevice


BOOL
DP6_IOControl(
	IN		DWORD dwOpenData, 
	IN		DWORD dwCode, 
	IN		PBYTE pBufIn,
	IN		DWORD dwLenIn,
	IN	OUT	PBYTE pBufOut,
	IN		DWORD dwLenOut,
	IN	OUT	PDWORD pdwActualOut)
{
    BOOL Status;

    if ((dwLenIn < sizeof(DHCPV6_IOCTL_HDR)) || 
        (dwLenOut < sizeof(DHCPV6_IOCTL_HDR))) {
        Status = FALSE;
        goto Exit;
    }

    switch (dwCode) {
    case IOCTL_DHCPV6L_ENUMERATE_INTERFACES:
        Status = IoctlEnumDhcpV6Interfaces(pBufIn, dwLenIn, pBufOut, 
            dwLenOut, pdwActualOut);
        break;
    case IOCTL_DHCPV6L_OPEN_HANDLE:
        Status = IoctlOpenDhcpV6InterfaceHandle(pBufIn, dwLenIn, pBufOut, 
            dwLenOut, pdwActualOut);
        break;
    case IOCTL_DHCPV6L_CLOSE_HANDLE:
        Status = IoctlCloseDhcpV6InterfaceHandle(pBufIn, dwLenIn, pBufOut, 
            dwLenOut, pdwActualOut);
        break;
    case IOCTL_DHCPV6L_REFRESH:
        Status = IoctlPerformDhcpV6Refresh(pBufIn, dwLenIn, pBufOut, 
            dwLenOut, pdwActualOut);
        break;
    case IOCTL_DHCPV6L_GET_DNS_LIST:
        Status = IoctlGetDhcpV6DNSList(pBufIn, dwLenIn, pBufOut, dwLenOut, 
            pdwActualOut);
        break;
    case IOCTL_DHCPV6L_GET_PD_LIST:
        Status = IoctlGetDhcpV6PDList(pBufIn, dwLenIn, pBufOut, 
            dwLenOut, pdwActualOut);
        break;
    case IOCTL_DHCPV6L_GET_DOMAIN_LIST:
        Status = IoctlGetDhcpV6DomainList(pBufIn, dwLenIn, pBufOut, 
            dwLenOut, pdwActualOut);
        break;
    case IOCTL_DHCPV6L_FREE_MEMORY:
        Status = IoctlDhcpV6FreeMem(pBufIn, dwLenIn, pBufOut, 
            dwLenOut, pdwActualOut);
        break;
    default:
        Status = FALSE;
    }

    if (Status) {
        DHCPV6_IOCTL_HDR *pHdr = (DHCPV6_IOCTL_HDR *)pBufOut;
        pHdr->CallType = dwCode;
    }
        

Exit:
    return Status;

}

