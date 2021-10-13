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
/*****************************************************************************/
/**                                                             Microsoft Windows                                                       **/
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
    TEXT("Init"),       TEXT("Timer"),          TEXT("AutoIP"),         TEXT("Unused"),
    TEXT("Recv"),   TEXT("Send"),               TEXT("Request"),        TEXT("Media Sense"),
    TEXT("Notify"),     TEXT("Buffer"),         TEXT("Interface"),  TEXT("Misc"),
    TEXT("Alloc"),  TEXT("Function"),   TEXT("Warning"),    TEXT("Error") },
    0xc000
};
#endif

/*
typedef DWORD (*PFNDWORD2)(void *, void *);

PFNDWORD2 v_pfnIPDispatchDeviceControl;

PFNVOID *v_apSocketFns, *v_apAfdFns;

CRITICAL_SECTION                v_ProtocolListLock;
PDHCP_PROTOCOL_CONTEXT  v_ProtocolList;
HANDLE                                  v_TcpipDriver;
int                                             v_DhcpInitDelay;

extern CTEEvent v_DhcpEvent;
void DhcpEventWorker(CTEEvent * Event, void * Context);


STATUS HandleMediaConnect(unsigned Context, PTSTR pAdapter);
STATUS HandleMediaDisconnect(unsigned Context, PTSTR pAdapter);
*/


#if 0
// Handle by which to notify applications of changes to IP data structures.
PDHCP_PROTOCOL_CONTEXT
DhcpRegister(
        PWCHAR                          wszProtocolName,
        PFNSetDHCPNTE           pfnSetNTE,
        PFNIPSetNTEAddr         pfnSetAddr,
        PFN_DHCP_NOTIFY         *ppDhcpNotify)
//
//      If successful, returns a pointer to a DHCP protocol context
//      which the caller will pass to subsequent calls to DhcpNotify.
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
}       // DhcpRegister()
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

//              InitializeCriticalSection(&v_ProtocolListLock);
//              CTEInitEvent(&v_DhcpEvent, DhcpEventWorker);
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
}       // dllentry()

extern unsigned int v_cXid;

extern BOOL WCE_Initialize();

#define DHCPV6_DEV_PREFIX   TEXT("DP6")
#define DHCPV6_DEV_INDEX    0
#define DHCPV6_DEVICE_NAME  TEXT("DP60:")



//      IMPORTANT: The name of this fn must be the same as the HelperName
//      note: the parameters for registration is different: Unused, OpCode,
//                      pVTable, cVTable, pMTable, cMTable, pIndexNum

BOOL DhcpV6L(DWORD Unused, DWORD OpCode,
                         PBYTE pName, DWORD cBuf1,
                         PBYTE pBuf1, DWORD cBuf2,
                         PDWORD pBuf2) {
        STATUS  Status = DHCP_SUCCESS;
        BOOL    fStatus = TRUE;
        PWSTR   pAdapterName = (PWSTR)pName;
        //unsigned int    cRand, cRand2;
        //FILETIME      CurTime;
        //__int64 cBigRand;
        LPCWSTR DHCPV6DevPath = L"Comm\\Devices\\TCPIP6\\DHCPV6L";

        switch(OpCode) {
        case DHCP_REGISTER:
//              v_apAfdFns = (PFNVOID *)pName;
//              v_apSocketFns = (PFNVOID *)pBuf1;
                *pBuf2 = 0;
//              CTEInitLock(&v_GlobalListLock);
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
}       // DhcpV6L()

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


//      @func PVOID | DP6_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from IP6_Init call
//  @rdesc      Returns TRUE for success, FALSE for failure.
//      @remark Routine exported by a device driver.  "NDT" is the string
//                      passed in as lpszType in RegisterDevice

BOOL
DP6_Deinit(DWORD dwData)
{
        return TRUE;
}

//      @func PVOID | IP6_Open          | Device open routine
//  @parm DWORD | dwData                | value returned from IP6_Init call
//  @parm DWORD | dwAccess              | requested access (combination of GENERIC_READ
//                                                                and GENERIC_WRITE)
//  @parm DWORD | dwShareMode   | requested share mode (combination of
//                                                                FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc      Returns a DWORD which will be passed to Read, Write, etc or NULL if
//                      unable to open device.
//      @remark Routine exported by a device driver.  "NDT" is the string passed
//                      in as lpszType in RegisterDevice
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



//  @rdesc      Returns 0 for end of file, -1 for error, otherwise the number of
//                      bytes read.  The length returned is guaranteed to be the length
//                      requested unless end of file or an error condition occurs.

DWORD
DP6_Read (DWORD dwData, LPVOID pBuf, DWORD Len)
{
        // Return length read
        return 0;
}


//  @rdesc      Returns -1 for error, otherwise the number of bytes written.  The
//                      length returned is guaranteed to be the length requested unless an
//                      error condition occurs.

DWORD
DP6_Write (DWORD dwData, LPCVOID pBuf, DWORD Len)
{
        // return number of bytes written (or -1 for error)
        return 0;
}


//  @rdesc      Returns current position relative to start of file, or -1 on error
//      @remark Routine exported by a device driver.  "NDT" is the string passed
//               in as lpszType in RegisterDevice

DWORD
DP6_Seek (DWORD dwData, long pos, DWORD type)
{
        // return an error
        return (DWORD)-1;
}

BOOL IoctlEnumDhcpV6Interfaces(PBYTE pBufIn, DWORD dwLenIn, void * pBufOut,
    DWORD dwLenOut, PDWORD pdwActualOut) {

    DHCPV6_IOCTL_ENUM_PARAMS    LocalParams, *pParams = pBufOut;
    DWORD                       RetStatus;

    if (dwLenOut < sizeof(LocalParams)) {
        RetStatus = ERROR_OUTOFMEMORY;
        goto Error;
    }

    if (! CeSafeCopyMemory(&LocalParams, pBufOut, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    LocalParams.Status = EnumDhcpV6Interfaces(NULL, LocalParams.dwVersion,
        NULL, LocalParams.dwPreferredNumEntries,
        (void *)&(LocalParams.pDhcpV6Interfaces),
        &LocalParams.dwNumInterfaces, &LocalParams.dwTotalNumInterfaces,
        &LocalParams.dwResumeHandle, NULL);

    LocalParams.CallType = IOCTL_DHCPV6L_ENUMERATE_INTERFACES;

    if (! CeSafeCopyMemory(pBufOut, &LocalParams, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    return TRUE;

Error:

    __try {
        pParams->Status = RetStatus;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ;   //nothing to do here!
    }
    return FALSE;

}   // IoctlEnumDhcpV6Interfaces()

BOOL IoctlOpenDhcpV6InterfaceHandle(PBYTE pBufIn, DWORD dwLenIn,
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {

    DHCPV6_IOCTL_INTERFACE_PARAMS   LocalParams, *pParams = pBufOut;
    DWORD                           RetStatus;
    LPWSTR                          pszTemp;

    if (dwLenOut < sizeof(LocalParams)) {
        RetStatus = ERROR_OUTOFMEMORY;
        goto Error;
    }

    if (! CeSafeCopyMemory(&LocalParams, pBufOut, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    // make sure no one accesses this
    if (pszTemp = LocalParams.DhcpV6Interface.pszDescription)
        LocalParams.DhcpV6Interface.pszDescription = NULL;

    LocalParams.Status = OpenDhcpV6InterfaceHandle(NULL,
        LocalParams.dwVersion, (void *)&LocalParams.DhcpV6Interface,
        NULL, &LocalParams.hInterface);

    LocalParams.CallType = IOCTL_DHCPV6L_OPEN_HANDLE;

    if (pszTemp)
        LocalParams.DhcpV6Interface.pszDescription = pszTemp;

    if (! CeSafeCopyMemory(pBufOut, &LocalParams, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    return TRUE;

Error:

    __try {
        pParams->Status = RetStatus;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ;   //nothing to do here!
    }
    return FALSE;

}   // IoctlOpenDhcpV6InterfaceHandle()


BOOL IoctlCloseDhcpV6InterfaceHandle(PBYTE pBufIn, DWORD dwLenIn,
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {

    DHCPV6_IOCTL_INTERFACE_PARAMS    LocalParams, *pParams = pBufOut;
    DWORD                       RetStatus;

    if (dwLenOut < sizeof(LocalParams)) {
        RetStatus = ERROR_OUTOFMEMORY;
        goto Error;
    }

    if (! CeSafeCopyMemory(&LocalParams, pBufOut, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    LocalParams.Status = CloseDhcpV6InterfaceHandle(LocalParams.hInterface);

    LocalParams.CallType = IOCTL_DHCPV6L_CLOSE_HANDLE;

    if (! CeSafeCopyMemory(pBufOut, &LocalParams, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    return TRUE;

Error:

    __try {
        pParams->Status = RetStatus;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ;   //nothing to do here!
    }
    return FALSE;

}   // IoctlCloseDhcpV6InterfaceHandle()


BOOL IoctlPerformDhcpV6Refresh(PBYTE pBufIn, DWORD dwLenIn,
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {

    DHCPV6_IOCTL_INTERFACE_PARAMS    LocalParams, *pParams = pBufOut;
    DWORD                       RetStatus;

    if (dwLenOut < sizeof(LocalParams)) {
        RetStatus = ERROR_OUTOFMEMORY;
        goto Error;
    }

    if (! CeSafeCopyMemory(&LocalParams, pBufOut, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    LocalParams.Status = PerformDhcpV6Refresh(LocalParams.hInterface,
            LocalParams.dwVersion, NULL);

    LocalParams.CallType = IOCTL_DHCPV6L_REFRESH;

    if (! CeSafeCopyMemory(pBufOut, &LocalParams, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    return TRUE;

Error:

    __try {
        pParams->Status = RetStatus;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ;   //nothing to do here!
    }
    return FALSE;

}   // IoctlPerformDhcpV6Refresh()


BOOL IoctlGetDhcpV6DNSList(PBYTE pBufIn, DWORD dwLenIn,
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {

    DHCPV6_IOCTL_INTERFACE_PARAMS    LocalParams, *pParams = pBufOut;
    DWORD                       RetStatus;

    if (dwLenOut < sizeof(LocalParams)) {
        RetStatus = ERROR_OUTOFMEMORY;
        goto Error;
    }

    if (! CeSafeCopyMemory(&LocalParams, pBufOut, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    LocalParams.Status = GetDhcpV6DNSList(LocalParams.hInterface,
            LocalParams.dwVersion, (void *)&(LocalParams.pDhcpV6DNSList),
            NULL);

    LocalParams.CallType = IOCTL_DHCPV6L_GET_DNS_LIST;

    if (! CeSafeCopyMemory(pBufOut, &LocalParams, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    return TRUE;

Error:

    __try {
        pParams->Status = RetStatus;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ;   //nothing to do here!
    }
    return FALSE;

}   // IoctlGetDhcpV6DNSList()

BOOL IoctlGetDhcpV6PDList(PBYTE pBufIn, DWORD dwLenIn,
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {

    DHCPV6_IOCTL_INTERFACE_PARAMS    LocalParams, *pParams = pBufOut;
    DWORD                       RetStatus;

    if (dwLenOut < sizeof(LocalParams)) {
        RetStatus = ERROR_OUTOFMEMORY;
        goto Error;
    }

    if (! CeSafeCopyMemory(&LocalParams, pBufOut, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    LocalParams.Status = GetDhcpV6PDList(LocalParams.hInterface,
            LocalParams.dwVersion, (void *)&(LocalParams.pPrefixD),
            NULL);

    LocalParams.CallType = IOCTL_DHCPV6L_GET_PD_LIST;

    if (! CeSafeCopyMemory(pBufOut, &LocalParams, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    return TRUE;

Error:

    __try {
        pParams->Status = RetStatus;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ;   //nothing to do here!
    }
    return FALSE;

}   // IoctlGetDhcpV6PDList()


BOOL IoctlGetDhcpV6DomainList(PBYTE pBufIn, DWORD dwLenIn,
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {

    DHCPV6_IOCTL_INTERFACE_PARAMS   LocalParams, *pParams = pBufOut;
    DWORD                       RetStatus;

    if (dwLenOut < sizeof(LocalParams)) {
        RetStatus = ERROR_OUTOFMEMORY;
        goto Error;
    }

    if (! CeSafeCopyMemory(&LocalParams, pBufOut, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    LocalParams.Status = GetDhcpV6DomainList(LocalParams.hInterface,
            LocalParams.dwVersion, (void *)&(LocalParams.pDomainList), NULL);

    LocalParams.CallType = IOCTL_DHCPV6L_GET_DOMAIN_LIST;

    if (! CeSafeCopyMemory(pBufOut, &LocalParams, sizeof(LocalParams))) {
        RetStatus = ERROR_INVALID_PARAMETER;
        goto Error;
    }

    return TRUE;

Error:

    __try {
        pParams->Status = RetStatus;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ;   //nothing to do here!
    }
    return FALSE;


}   // IoctlGetDhcpV6DomainList()


BOOL IoctlDhcpV6FreeMem(PBYTE pBufIn, DWORD dwLenIn,
    void * pBufOut, DWORD dwLenOut, PDWORD pdwActualOut) {

    DHCPV6_IOCTL_HDR *pHdr = (DHCPV6_IOCTL_HDR *)pBufIn;
    DWORD   i;
    DHCPV6_IOCTL_ENUM_PARAMS *pEnum;
    DHCPV6_IOCTL_INTERFACE_PARAMS   *pParams =
        (DHCPV6_IOCTL_INTERFACE_PARAMS *)pBufIn;
    DWORD   dwNumEntries;
    DHCPV6_INTERFACE *pInterface;
    DHCPV6_DNS_LIST *pDnsList;
    void    *pVoid;

    // we're not holding any resources here, therefore no try/except
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
        if (dwLenIn < sizeof(*pEnum)) {
            pHdr->Status = ERROR_INSUFFICIENT_BUFFER;
            goto Exit;
        }
        pEnum = (DHCPV6_IOCTL_ENUM_PARAMS *)pBufIn;
        dwNumEntries = pEnum->dwNumInterfaces;

        // technically below mult. could overflow, but we're accessing serially
        pInterface = pEnum->pDhcpV6Interfaces;
        if (pInterface && IsValidUsrPtr(pInterface, 
            dwNumEntries * sizeof(*pInterface), FALSE)) {

            for (i = 0; i < dwNumEntries; i++) {
                if (pVoid = pInterface[i].pszDescription)
                    DHCPv6FreeBuffer(pVoid);
            }
            DHCPv6FreeBuffer(pInterface);
            pEnum->pDhcpV6Interfaces = NULL;
        }
        break;

    case IOCTL_DHCPV6L_GET_DNS_LIST:

        if (dwLenIn < sizeof(*pParams)) {
            pHdr->Status = ERROR_INSUFFICIENT_BUFFER;
            goto Exit;
        }

        pDnsList = pParams->pDhcpV6DNSList;
        if (pDnsList) {

			dwNumEntries = pDnsList->uNumOfEntries;
            if (dwNumEntries && IsValidUsrPtr(pDnsList, 
				dwNumEntries * sizeof(*pDnsList), FALSE)) {
                if (pVoid = pDnsList->pDhcpV6DNS) {
                    DHCPv6FreeBuffer(pVoid);
                }
            }

            DHCPv6FreeBuffer(pDnsList);
            pParams->pDhcpV6DNSList = NULL;
        }
        break;

    case IOCTL_DHCPV6L_GET_PD_LIST:
        if (dwLenIn < sizeof(*pParams)) {
            pHdr->Status = ERROR_INSUFFICIENT_BUFFER;
            goto Exit;
        }

        pVoid = pParams->pPrefixD;
        if (pVoid) {

            DHCPv6FreeBuffer(pVoid);
            pParams->pPrefixD = NULL;
        }
        break;

    case IOCTL_DHCPV6L_GET_DOMAIN_LIST:
        if (dwLenIn < sizeof(*pParams)) {
            pHdr->Status = ERROR_INSUFFICIENT_BUFFER;
            goto Exit;
        }

        pVoid = pParams->pDomainList;
        if (pVoid) {
            DHCPv6FreeBuffer(pVoid);
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


//      @func BOOL | DP6_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from DP6_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device
//  @rdesc      Returns TRUE for success, FALSE for failure
//      @remark Routine exported by a device driver.  "NDT" is the string passed
//              in as lpszType in RegisterDevice


BOOL
DP6_IOControl(
        IN              DWORD dwOpenData,
        IN              DWORD dwCode,
        IN              PBYTE pBufIn,
        IN              DWORD dwLenIn,
        IN      OUT     PBYTE pBufOut,
        IN              DWORD dwLenOut,
        IN      OUT     PDWORD pdwActualOut)
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

