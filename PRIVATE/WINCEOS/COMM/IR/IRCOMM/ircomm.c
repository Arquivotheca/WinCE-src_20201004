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
// ircomm.c - Emulate a com port on top of IRDA winsock
//
#include "windows.h"
#include "memory.h"
#include "pegdser.h"
#include "winsock2.h"
#include "af_irda.h"
#include <devload.h>

#undef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))

DWORD WINAPI AcceptThread(PVOID pThreadParm);
DWORD WINAPI TxThread(PVOID pThreadParm);
DWORD WINAPI RxThread(PVOID pThreadParm);

int     IrCOMMConnect(void);
int     InitializeConnection(SOCKET);
TCHAR   *GetLastErrorText(void);
int     CreateSockets();

#ifdef DEBUG

// Debug zone definitions.
#define ZONE_STATE      DEBUGZONE(0)    // 0x0001
#define ZONE_SOCKET     DEBUGZONE(1)    // 0x0002
#define ZONE_RECV       DEBUGZONE(7)    // 0x0080
#define ZONE_CONN       DEBUGZONE(8)    // 0x0100
#define ZONE_SEND       DEBUGZONE(9)    // 0x0200
#define ZONE_INTERFACE  DEBUGZONE(10)   // 0x0400
#define ZONE_MISC       DEBUGZONE(11)   // 0x0800
#define ZONE_ALLOC      DEBUGZONE(12)   // 0x1000
#define ZONE_FUNCTION   DEBUGZONE(13)   // 0x2000
#define ZONE_WARN       DEBUGZONE(14)   // 0x4000
#define ZONE_ERROR      DEBUGZONE(15)   // 0x8000

DBGPARAM dpCurSettings =
{
    TEXT("IrComm"),
    {
        TEXT("State"), TEXT("Socket"),  TEXT("Unused"),  TEXT("Unused"),
        TEXT("Unused"), TEXT("Unused"), TEXT("Unused"),  TEXT("Recv"),
        TEXT("Conn"),  TEXT("Send"),    TEXT("Interface"), TEXT("Misc"),
        TEXT("Alloc"), TEXT("Function"), TEXT("Warning"), TEXT("Error")
    },
    0xC003
};

TCHAR *StateStr[] = {
    TEXT("IRCOMM_CLOSED"),
    TEXT("IRCOMM_OPENED"),
    TEXT("IRCOMM_DISCOVERY_PENDING"),
    TEXT("IRCOMM_CONNECT_PENDING"),
    TEXT("IRCOMM_CONNECTED")
};
#endif

typedef enum
{
    IRCOMM_CLOSED,
    IRCOMM_OPENED,
    IRCOMM_DISCOVERY_PENDING,
    IRCOMM_CONNECT_PENDING,
    IRCOMM_CONNECTED
} IRCOMM_CONN_STATES;

typedef struct
{
    BYTE    PIDataRate_0x10;
    BYTE    PLDataRate_4;
    ULONG   PVDataRateBigEndian;
    BYTE    PIDataFmt_0x11;
    BYTE    PLDataFmt_1;
    BYTE    PVDataFmt;
    BYTE    PIFlowCtl_0x12;
    BYTE    PLFlowCtl_1;
    BYTE    PVFlowCtl;
    BYTE    PIXOnXoff_0x13;
    BYTE    PLXOnXoff_2;
    BYTE    PVXOn_0x11;
    BYTE    PVXOff_0x13;
    BYTE    PIEnqAck_0x14;
    BYTE    PLEnqAck_2;
    BYTE    PVEnq_0x05;
    BYTE    PVAck_0x06;
} IRCOMM_9WIRE_PARMS;

SOCKADDR_IRDA SockAddr =
{
    AF_IRDA,
    0, 0, 0, 0,
    "IrDA:IrCOMM"
};

#define MAX_OPENS 8
typedef struct _IRCOMM_OPEN {
    DWORD dwEventMask;
    DWORD dwEventData;
    HANDLE hCommEvent;
    DWORD  dwWaitThds;
    DWORD dwRefCnt;
} IRCOMM_OPEN, *PIRCOMM_OPEN;

PIRCOMM_OPEN g_Opens[MAX_OPENS];

HANDLE              hTxEvent;
HANDLE              hRxEvent;

PIRCOMM_OPEN        vAccessOwner;
DWORD               vEventMask; // OR of wait masks of all opens
CRITICAL_SECTION    IrcommCs;
CRITICAL_SECTION    IrcommCs;

IRCOMM_CONN_STATES  State;

int                 SendMaxPDU;

SOCKET              ListenSock = INVALID_SOCKET;
SOCKET              AcceptSock = INVALID_SOCKET;
SOCKET              ConnectSock = INVALID_SOCKET;
SOCKET              DataSock = INVALID_SOCKET;
BOOL                SocketsCreated = FALSE;
DWORD               g_dwListenInstance = 0;

BYTE                *pMemBase;

#define             IRCOMM_TX_RING_SIZE     4096
BYTE                *TxRing;
BYTE                *pTxRingRead;
BYTE                *pTxRingWrite;
BYTE                *pTxRingMax;

#define             IRCOMM_TX_BUF_SIZE      2048
BYTE                *TxBuf;

#define             IRCOMM_RX_RING_SIZE     4096
BYTE                *RxRing;
BYTE                *pRxRingRead;
BYTE                *pRxRingWrite;
BYTE                *pRxRingMax;

COMMTIMEOUTS        CommTimeouts;

// WaitEvent thread priority
#define                         DEFAULT_THREAD_PRIORITY 148
#define                         REGISTRY_PRIORITY_VALUE TEXT("Priority256")
DWORD                           g_dwHighThreadPrio;


BOOL __stdcall
DllEntry (HANDLE  hinstDLL,
          DWORD   Op,
          LPVOID  lpvReserved)
{
    switch (Op)
    {
      case DLL_PROCESS_ATTACH:
        DEBUGREGISTER(hinstDLL);
                DisableThreadLibraryCalls ((HMODULE)hinstDLL);
        break;

      case DLL_PROCESS_DETACH:
        break;

      case DLL_THREAD_DETACH:
        break;

      case DLL_THREAD_ATTACH:
        break;

        default :
        break;
    }

    return(TRUE);
}


//
// Allocate and initialize a new IRCOMM_OPEN
//
DWORD
NewOpen(
    DWORD dwAccess
    )
{
    DWORD i;
    PIRCOMM_OPEN pOpen;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:+NewOpen\n")));
    EnterCriticalSection(&IrcommCs);
    for (i = 0; i < MAX_OPENS; i++) {
        if (NULL == g_Opens[i]) {
            pOpen = LocalAlloc(LPTR, sizeof(IRCOMM_OPEN));
            if (NULL == pOpen) {
                i = MAX_OPENS;
                DEBUGMSG(ZONE_WARN, (TEXT("IRCOMM:NewOpen LocalAlloc failed\n")));
                break;
            }
            pOpen->hCommEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            pOpen->dwRefCnt = 1;
            pOpen->dwWaitThds = 0;
            if (NULL == pOpen->hCommEvent) {
                LocalFree(pOpen);
                i = MAX_OPENS;
                DEBUGMSG(ZONE_WARN, (TEXT("IRCOMM:NewOpen CreateEvent failed\n")));
                break;
            }
            g_Opens[i] = pOpen;
            if (dwAccess & (GENERIC_READ|GENERIC_WRITE)) {
                vAccessOwner = pOpen;
            }
            break;
        }
    }
    LeaveCriticalSection(&IrcommCs);
    DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:-NewOpen %d\n"), i));
    return i+1;
}   // NewOpen


BOOL
DerefOpen(
    DWORD dwOpen
    )
{
    PIRCOMM_OPEN pOpen;
    BOOL bDelete = FALSE;
    DWORD dwIndex = dwOpen-1;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:+DerefOpen %d\n"), dwOpen));
    EnterCriticalSection(&IrcommCs);
    pOpen = g_Opens[dwIndex];
    pOpen->dwRefCnt--;
    if (0 == pOpen->dwRefCnt) {
        g_Opens[dwIndex] = NULL;
        if (pOpen == vAccessOwner) {
            vAccessOwner = NULL;
        }
        bDelete = TRUE;
    }

    if (bDelete) {
        DEBUGMSG(ZONE_WARN, (TEXT("IRCOMM:DerefOpen delete %d\n"), dwOpen));

        dwIndex = pOpen->dwWaitThds;
        while (dwIndex) {
            SetEvent(pOpen->hCommEvent);
            dwIndex--;
        }

        LeaveCriticalSection(&IrcommCs);

        CloseHandle(pOpen->hCommEvent);
        LocalFree(pOpen);
    } else {
        LeaveCriticalSection(&IrcommCs);
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:-DerefOpen %d\n"), dwOpen));
    return bDelete;
}   // DerefOpen


//
// Verify an open handle and reference it.
//
PIRCOMM_OPEN
IsValidOpen(
    DWORD dwOpen
    )
{
    PIRCOMM_OPEN pRet;
    DWORD dwIndex = dwOpen-1;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:+IsValidOpen %d\n"), dwOpen));

    if (dwIndex >= MAX_OPENS) {
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:-IsValidOpen %d too large\n"), dwOpen));
        return NULL;
    }

    EnterCriticalSection(&IrcommCs);
    if (pRet = g_Opens[dwIndex]) {
        g_Opens[dwIndex]->dwRefCnt++;
    }
    LeaveCriticalSection(&IrcommCs);
    DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:-IsValidOpen 0x%x\n"), pRet));
    return pRet;
}   // IsValidOpen

//
// Wake any threads waiting on a new event
//
void
NewCommEvent(
    DWORD dwEventData
    )
{
    DWORD i, j;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:+NewCommEvent 0x%x\n"), dwEventData));
    EnterCriticalSection(&IrcommCs);
    if (dwEventData & vEventMask) { // Is anyone interested in this EV_*
        for (i = 0; i < MAX_OPENS; i++) {
            if (g_Opens[i]) {
                if (g_Opens[i]->dwEventMask & dwEventData) {
                    g_Opens[i]->dwEventData |= g_Opens[i]->dwEventMask & dwEventData;
                    j = g_Opens[i]->dwWaitThds;
                    while (j) {
                        SetEvent(g_Opens[i]->hCommEvent);
                        j--;
                    }
                }
            }
        }
    }
    LeaveCriticalSection(&IrcommCs);
    DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:-NewCommEvent\n")));
}   // NewCommEvent


void
ComputeEventMaskOR(void)
{
    int i;

    EnterCriticalSection(&IrcommCs);
    //
    // Recompute the OR of all wait masks
    //
    vEventMask = 0;
    for (i = 0; i < MAX_OPENS; i++) {
        if (g_Opens[i]) {
            vEventMask |= g_Opens[i]->dwEventMask;
        }
    }
    LeaveCriticalSection(&IrcommCs);
}

__inline
void
NewState(IRCOMM_CONN_STATES NewState)
{
    DEBUGMSG(ZONE_STATE, (TEXT("IRCOMM:NewState %s\r\n"), StateStr[NewState]));

    State = NewState;
}


//
// If the app is monitoring incoming calls (like PPP server) then don't transition to CLOSED.
// The next WaitCommEvent or ReadFile will cause IRCOMM to create a new set of sockets.
//
void
Shutdown(
    BOOL bClose
    )
{
    EnterCriticalSection(&IrcommCs);

    if (State != IRCOMM_CLOSED)
    {
        NewState(bClose ? IRCOMM_CLOSED : IRCOMM_OPENED);

        NewCommEvent(EV_RLSD);

        if (SocketsCreated) {
            // Indicate to any existing accept threads that their ListenSock is no longer valid.
            ++g_dwListenInstance;

            DEBUGMSG (ZONE_WARN, (TEXT("IRCOMM:Shutdown: Closing socket %d (ListenSock)\r\n"), ListenSock));
            closesocket(ListenSock);
            DEBUGMSG (ZONE_WARN, (TEXT("IRCOMM:Shutdown: Closing socket %d (ConnectSock)\r\n"), ConnectSock));
            closesocket(ConnectSock);
            DEBUGMSG (ZONE_WARN, (TEXT("IRCOMM:Shutdown: Closing socket %d (DataSock)\r\n"), DataSock));
            closesocket(DataSock);

                    ListenSock = INVALID_SOCKET;
            ConnectSock = INVALID_SOCKET;
            DataSock = INVALID_SOCKET;
        }
        SocketsCreated = FALSE;

        // make Rx/TxThread exit

        SetEvent(hRxEvent);
        SetEvent(hTxEvent);

        if (bClose) {
            CloseHandle(hTxEvent);
            CloseHandle(hRxEvent);
            VirtualFree(pMemBase, 0, MEM_RELEASE);
        } else {
            CreateSockets();
        }
    } else {
                DEBUGMSG (1, (TEXT("IRCOMM:Shutdown called and State == IRCOMM_CLOSED\r\n")));
                DEBUGMSG (1, (TEXT("IRCOMM:ListenSock=%d\r\n"), ListenSock));
                DEBUGMSG (1, (TEXT("IRCOMM:ConnectSock=%d\r\n"), ConnectSock));
                DEBUGMSG (1, (TEXT("IRCOMM:DataSock=%d\r\n"), DataSock));
        }

    LeaveCriticalSection(&IrcommCs);
}

// -----------------------------------------------------------------------------
// Function to read the thread priority from the registry.
// If it is not in the registry then a default value is returned.
// -----------------------------------------------------------------------------
static DWORD
GetRegistryThreadPriority(
        LPWSTR lpszActiveKey
        )
{
    HKEY hDevKey;
    DWORD dwValType;
    DWORD dwValLen;
    DWORD dwPrio;

    dwPrio = DEFAULT_THREAD_PRIORITY;

    hDevKey = OpenDeviceKey(lpszActiveKey);

    if (hDevKey) {
        dwValLen = sizeof(DWORD);
        RegQueryValueEx(
                hDevKey,
            REGISTRY_PRIORITY_VALUE,
            NULL,
            &dwValType,
            (PUCHAR)&dwPrio,
            &dwValLen);
        RegCloseKey(hDevKey);
    }

    return dwPrio;
}

//  @func PVOID | ttt_Init | Device initialization routine
//  @parm DWORD | dwInfo | info passed to RegisterDevice
//  @rdesc  Returns a DWORD which will be passed to Open & Deinit or NULL if
//          unable to initialize the device.
//  @remark Routine exported by a device driver.  "ttt" is the string passed
//          in as lpszType in RegisterDevice

DWORD
COM_Init(DWORD Index)
{
    DWORD i;
    WORD    WsaVer = WINSOCK_VERSION;
    WSADATA WsaData;

    DEBUGMSG(ZONE_INTERFACE | ZONE_FUNCTION | ZONE_CONN, (TEXT("IRCOMM:+COM_Init(%d)\r\n"), Index));

    i = WSAStartup(WsaVer, &WsaData);
    if (i) {
        DEBUGMSG(1, (L"IRCOMM:-COM_Init:WSAStartup failed %d!!\n", i));
        return 0;
    }

    // Retrieve higher thread priority from registry
    g_dwHighThreadPrio = GetRegistryThreadPriority((LPWSTR)Index);

    InitializeCriticalSection(&IrcommCs);
    for (i = 0; i < MAX_OPENS; i++) {
        g_Opens[i] = NULL;
    }

    SocketsCreated = FALSE;
    DataSock = INVALID_SOCKET;
    vAccessOwner = NULL;

    NewState(IRCOMM_CLOSED);
    DEBUGMSG (ZONE_INTERFACE, (TEXT("IRCOMM:-COM_Init: return 1\r\n")));
    return 1;
}

//  @func PVOID | ttt_Deinit | Device deinitialization routine
//  @parm DWORD | dwData | value returned from ttt_Init call
//  @rdesc  Returns TRUE for success, FALSE for failure.
//  @remark Routine exported by a device driver.  "ttt" is the string
//          passed in as lpszType in RegisterDevice

BOOL
COM_Deinit(DWORD dwData)
{
    DEBUGMSG(ZONE_FUNCTION | ZONE_CONN, (TEXT("IRCOMM:COM_Deinit(0x%X)\r\n"), dwData));

    DeleteCriticalSection(&IrcommCs);
    WSACleanup();
    return TRUE;
}

//  @func PVOID | ttt_Open      | Device open routine
//  @parm DWORD | dwData        | value returned from ttt_Init call
//  @parm DWORD | dwAccess      | requested access (combination of GENERIC_READ
//                                and GENERIC_WRITE)
//  @parm DWORD | dwShareMode   | requested share mode (combination of
//                                FILE_SHARE_READ and FILE_SHARE_WRITE)
//  @rdesc  Returns a DWORD which will be passed to Read, Write, etc or NULL if
//          unable to open device.
//  @remark Routine exported by a device driver.  "ttt" is the string passed
//          in as lpszType in RegisterDevice
DWORD
COM_Open(DWORD dwData, DWORD dwAccess, DWORD dwShareMode)
{
    DWORD dwThisOpen;
    DEBUGMSG(ZONE_INTERFACE | ZONE_FUNCTION | ZONE_CONN, (TEXT("IRCOMM:+COM_Open(0x%X, 0x%X, 0x%X)\r\n"),
              dwData, dwAccess, dwShareMode));

    EnterCriticalSection(&IrcommCs);
    if ((dwAccess & (GENERIC_READ|GENERIC_WRITE)) && vAccessOwner) {
        LeaveCriticalSection(&IrcommCs);
        SetLastError(ERROR_INVALID_ACCESS);
        return 0;
    }

    if ((dwThisOpen = NewOpen(dwAccess)) == MAX_OPENS+1) {
        LeaveCriticalSection(&IrcommCs);
        SetLastError(ERROR_OPEN_FAILED);
        return 0;
    }

    if (State != IRCOMM_CLOSED)
    {
        LeaveCriticalSection(&IrcommCs);
        return dwThisOpen;
    }

    hTxEvent        = CreateEvent(NULL, FALSE, FALSE, NULL);
    hRxEvent        = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (!hTxEvent || !hRxEvent)
    {
        DEBUGMSG(ZONE_INTERFACE|ZONE_ERROR, (TEXT("IRCOMM:-COM_Open failed to create event\r\n")));
        if (hTxEvent) {
            CloseHandle(hTxEvent);
        }
        if (hRxEvent) {
            CloseHandle(hRxEvent);
        }
        DerefOpen(dwThisOpen);
        LeaveCriticalSection(&IrcommCs);
        SetLastError(ERROR_OPEN_FAILED);
        return 0;
    }

    pMemBase = (BYTE *) VirtualAlloc(NULL,
                           IRCOMM_TX_RING_SIZE +
                           IRCOMM_RX_RING_SIZE +
                           IRCOMM_TX_BUF_SIZE,
                           MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (pMemBase == NULL)
    {
        DEBUGMSG(ZONE_INTERFACE|ZONE_ERROR, (TEXT("IRCOMM:-COM_Open failed to create event\r\n")));
        CloseHandle(hTxEvent);
        CloseHandle(hRxEvent);
        DerefOpen(dwThisOpen);
        LeaveCriticalSection(&IrcommCs);
        SetLastError(ERROR_OPEN_FAILED);
        return 0;
    }

    TxRing  = pMemBase;
    RxRing  = pMemBase + IRCOMM_TX_RING_SIZE;
    TxBuf   = RxRing + IRCOMM_RX_RING_SIZE;

    pTxRingRead = pTxRingWrite = TxRing;
    pTxRingMax  = TxRing + IRCOMM_TX_RING_SIZE;

    pRxRingRead = pRxRingWrite = RxRing;
    pRxRingMax  = RxRing + IRCOMM_RX_RING_SIZE;

    vEventMask = 0;

    NewState(IRCOMM_OPENED);

    LeaveCriticalSection(&IrcommCs);

    DEBUGMSG(ZONE_INTERFACE, (TEXT("IRCOMM:-COM_Open: Success\r\n")));
    return dwThisOpen;
}


//  @func BOOL | ttt_Close | Device close routine
//  @parm DWORD | dwOpenData | value returned from ttt_Open call
//  @rdesc  Returns TRUE for success, FALSE for failure
//  @remark Routine exported by a device driver.  "ttt" is the string passed
//          in as lpszType in RegisterDevice
BOOL
COM_Close(DWORD dwData)
{
    BOOL bShutdown;
    PIRCOMM_OPEN pOpen;
    DWORD i;

    DEBUGMSG(ZONE_INTERFACE | ZONE_FUNCTION | ZONE_CONN, (TEXT("IRCOMM:+COM_Close(0x%X)\r\n"), dwData));

    EnterCriticalSection(&IrcommCs);

    if (NULL == (pOpen = IsValidOpen(dwData))) {
        SetLastError(ERROR_INVALID_HANDLE);
        LeaveCriticalSection(&IrcommCs);
        return FALSE;

    }

    for (i = pOpen->dwRefCnt; i; i--) {
        SetEvent(pOpen->hCommEvent);
    }
    LeaveCriticalSection(&IrcommCs);

    DerefOpen(dwData);  // once for IsValidOpen
    DerefOpen(dwData);  // once to delete

    EnterCriticalSection(&IrcommCs);
    //
    // If there are no open handles, then we need to shutdown
    //
    bShutdown = TRUE;
    for (dwData = 0; dwData < MAX_OPENS; dwData++) {
        if (g_Opens[dwData]) {
            bShutdown = FALSE;
            break;
        }
    }
    if (bShutdown) {
        Shutdown(TRUE); // TRUE => Close
    } else {
        ComputeEventMaskOR();
    }
    LeaveCriticalSection(&IrcommCs);

    DEBUGMSG(ZONE_INTERFACE, (TEXT("IRCOMM:-COM_Close: Success\r\n")));

    return TRUE;
}

#define BIND_RETRIES 5
#define BIND_RETRY_SLEEP 500

BOOL
CreateSockets()
{
    // max attribute size 6
    BYTE        IASSetBuff[sizeof(IAS_SET) - 3 + 6];
    int         IASSetLen  = sizeof(IASSetBuff);
    PIAS_SET    pIASSet    = (PIAS_SET)IASSetBuff;
    HANDLE      hAcceptThread;
    int         Enable9WireMode  = 1;
    BOOL        Status = FALSE;
    int         cBindRetry;

    DEBUGMSG(ZONE_FUNCTION|ZONE_CONN, (TEXT("IRCOMM:+CreateSockets\r\n")));

    EnterCriticalSection(&IrcommCs);

    if (SocketsCreated)
    {
        Status = TRUE;
        goto done;
    }

    if (State == IRCOMM_CLOSED)
        goto done;

    if ((ConnectSock = socket(AF_IRDA, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM:CreateSockets ConnectSock=socket() failed %s\n\r"), GetLastErrorText()));
        goto done;
    }

    if ((ListenSock = socket(AF_IRDA, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM:CreateSockets ListenSock=socket() failed %s\n\r"), GetLastErrorText()));
        goto done;
    }

    if (setsockopt(ListenSock, SOL_IRLMP, IRLMP_9WIRE_MODE,
                   (const char *) &Enable9WireMode, sizeof(int))
        == SOCKET_ERROR)
    {
        goto done;
    }

    cBindRetry = BIND_RETRIES;
    while (cBindRetry) {
        if (bind(ListenSock, (const struct sockaddr *) &SockAddr, sizeof(SOCKADDR_IRDA)) == SOCKET_ERROR) {
            if (GetLastError() == WSAEADDRINUSE) {
                Sleep(BIND_RETRY_SLEEP);
            } else {
                DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM:CreateSockets bind(ListenSock) failed %s\n\r"), GetLastErrorText()));
                goto done;
            }
        } else {
            break;
        }
        cBindRetry--;
    }

    ASSERT(cBindRetry);
    if (0 == cBindRetry) {
        DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM:CreateSockets bind(ListenSock) failed %s\n\r"), GetLastErrorText()));
        goto done;
    }

    if (listen(ListenSock, 1) == SOCKET_ERROR)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM:CreateSockets listen() failed %s\n\r"), GetLastErrorText()));
        goto done;
    }

/*
    memset ((char *) CommTimeouts, 0, sizeof(COMMTIMEOUTS));
*/
    // add IrCOMM IAS attributes
    memcpy(&pIASSet->irdaClassName[0],  "IrDA:IrCOMM", 12);
    memcpy(&pIASSet->irdaAttribName[0], "Parameters",  11);

    pIASSet->irdaAttribType                       = IAS_ATTRIB_OCTETSEQ;
    pIASSet->irdaAttribute.irdaAttribOctetSeq.Len = 6;

    memcpy(&pIASSet->irdaAttribute.irdaAttribOctetSeq.OctetSeq[0],
           "\000\001\006\001\001\001", 6);

    if (setsockopt(ListenSock, SOL_IRLMP, IRLMP_IAS_SET,
                   (const char *) pIASSet, IASSetLen) == SOCKET_ERROR)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM:CreateSockets setsockopt(,,IRLMP_IAS_SET,,) %s\n\r"), GetLastErrorText()));

        goto done;
    }

    if ((hAcceptThread = CreateThread(NULL, 0, AcceptThread, (PVOID)g_dwListenInstance, 0, NULL))
        == 0)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM:CreateSockets CreateThread() failed\r\n")));
        goto done;
    }

    CloseHandle(hAcceptThread);

    SocketsCreated = TRUE;

    NewState(IRCOMM_DISCOVERY_PENDING);

    Status = TRUE;

done:

    if (!SocketsCreated) {
        if (ListenSock != INVALID_SOCKET) {
            closesocket(ListenSock);
            ListenSock = INVALID_SOCKET;
        }
        if (ConnectSock != INVALID_SOCKET) {
            closesocket(ConnectSock);
            ConnectSock = INVALID_SOCKET;
        }
    }

    LeaveCriticalSection(&IrcommCs);

    DEBUGMSG(ZONE_FUNCTION|ZONE_CONN, (TEXT("IRCOMM:-CreateSockets %s\r\n"), Status ? L"TRUE" : L"FALSE"));
    return Status;
}

//  @func DWORD | ttt_Read | Device read routine
//  @parm DWORD | dwOpenData | value returned from ttt_Open call
//  @parm LPVOID | pBuf | buffer to receive data
//  @parm DWORD | len | maximum length to read
//  @rdesc  Returns 0 for end of file, -1 for error, otherwise the number of
//          bytes read.  The length returned is guaranteed to be the length
//          requested unless end of file or an error condition occurs.
//  @remark Routine exported by a device driver.  "ttt" is the string passed
//          in as lpszType in RegisterDevice
DWORD
COM_Read(DWORD dwData, LPVOID pBuf, DWORD Len)
{
    DWORD   RxLen = 0;

    DEBUGMSG(ZONE_INTERFACE | ZONE_FUNCTION, (TEXT("IRCOMM:+COM_Read %d\r\n"), Len));

    if (!IsValidOpen(dwData)) {
        SetLastError(ERROR_INVALID_HANDLE);
        DEBUGMSG(ZONE_INTERFACE | ZONE_ERROR,(TEXT("IRCOMM:-COM_Read: Invalid handle\r\n")));
        return -1;
    }

    if (!SocketsCreated)
    {
        if (CreateSockets() == FALSE) {
            SetLastError(ERROR_GEN_FAILURE);
            DEBUGMSG(ZONE_INTERFACE | ZONE_ERROR, (TEXT("IRCOMM:-COM_Read: CreateSockets failed\r\n")));
            DerefOpen(dwData);
            return -1;
        }
    }

    EnterCriticalSection(&IrcommCs);  // protect ring

    if (State == IRCOMM_CLOSED) {
        SetLastError(ERROR_GEN_FAILURE);
        RxLen = -1;
        goto exit;
    }

    if (pRxRingRead == pRxRingWrite &&
        CommTimeouts.ReadTotalTimeoutConstant == MAXDWORD)
    {
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:COM_Read waiting...\r\n")));

        LeaveCriticalSection(&IrcommCs);

        WaitForSingleObject(hRxEvent, INFINITE);

        EnterCriticalSection(&IrcommCs);
    }

    if (State == IRCOMM_CLOSED) {
        SetLastError(ERROR_GEN_FAILURE);
        goto exit;
    }

    try {
        for (RxLen = 0; RxLen < Len && pRxRingRead != pRxRingWrite;
             RxLen++)
        {
            ((BYTE *)pBuf)[RxLen] = *pRxRingRead++;
            if (pRxRingRead == pRxRingMax)
                pRxRingRead = RxRing;
        }
    } except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_ADDRESS);
        RxLen = -1;
    }

exit:
    DerefOpen(dwData);
    LeaveCriticalSection(&IrcommCs);
    DEBUGMSG(ZONE_INTERFACE | ZONE_FUNCTION, (TEXT("IRCOMM:-COM_Read: Returning %d\r\n"), RxLen));
    return RxLen;
}

//  @func DWORD | ttt_Write | Device write routine
//  @parm DWORD | dwOpenData | value returned from ttt_Open call
//  @parm LPCVOID | pBuf | buffer containing data
//  @parm DWORD | len | maximum length to write
//  @rdesc  Returns -1 for error, otherwise the number of bytes written.  The
//          length returned is guaranteed to be the length requested unless an
//          error condition occurs.
//  @remark Routine exported by a device driver.  "ttt" is the string passed
//          in as lpszType in RegisterDevice
DWORD
COM_Write(DWORD dwData, LPCVOID pBuf, DWORD Len)
{
    DWORD   BytesSent = 0;
    BYTE    *pLast, *pRead;

    DEBUGMSG(ZONE_INTERFACE | ZONE_FUNCTION, (TEXT("IRCOMM:+COM_Write len %d\r\n"),Len));

    if (!IsValidOpen(dwData)) {
        SetLastError(ERROR_INVALID_HANDLE);
        DEBUGMSG(ZONE_INTERFACE | ZONE_ERROR,(TEXT("IRCOMM:-COM_Write: Invalid handle\r\n")));
        return -1;
    }

    if (!SocketsCreated)
    {
        if (CreateSockets() == FALSE) {
            DEBUGMSG(ZONE_INTERFACE | ZONE_ERROR, (TEXT("IRCOMM:-COM_Write: CreateSockets failed\r\n")));
            DerefOpen(dwData);
            SetLastError(ERROR_INTERNAL_ERROR);
            return (DWORD)-1;
        }
    }

    if (State != IRCOMM_CONNECTED)
    {
        if (IrCOMMConnect() == -1) {
            DEBUGMSG(ZONE_INTERFACE | ZONE_ERROR, (TEXT("IRCOMM:-COM_Write: IrCOMMConnect failed\r\n")));
            DerefOpen(dwData);
            SetLastError(ERROR_INTERNAL_ERROR);
            return (DWORD)-1;
        }
    }

    EnterCriticalSection(&IrcommCs);

    if (State == IRCOMM_CLOSED) {
        BytesSent = -1;
        goto exit;
    }

    pRead = pTxRingRead;
    pLast = pRead == TxRing ? pTxRingMax-1 : pRead-1;

    try {
        while (BytesSent < Len && pTxRingWrite != pLast)
        {
            *pTxRingWrite++ = *((BYTE *) pBuf)++;
            BytesSent++;

            if (pTxRingWrite == pTxRingMax)
                pTxRingWrite = TxRing;

            pRead = pTxRingRead;
            pLast = pRead == TxRing ? pTxRingMax-1 : pRead-1;
        }
    } except (EXCEPTION_EXECUTE_HANDLER) {
          SetLastError(ERROR_INVALID_ADDRESS);
          BytesSent = -1;
    }

    if (BytesSent < Len)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM:COM_Write TxRing overflow Sent %d, Total %d\n"), BytesSent, Len));
    }

    SetEvent(hTxEvent);

exit:
    DerefOpen(dwData);
    LeaveCriticalSection(&IrcommCs);

    DEBUGMSG(ZONE_INTERFACE, (TEXT("IRCOMM:-COM_Write: returning %d\r\n"), BytesSent));
    return BytesSent;
}

//  @func DWORD | ttt_Seek | Device seek routine
//  @parm DWORD | dwOpenData | value returned from ttt_Open call
//  @parm long | pos | position to seek to (relative to type)
//  @parm DWORD | type | FILE_BEGIN, FILE_CURRENT, or FILE_END
//  @rdesc  Returns current position relative to start of file, or -1 on error
//  @remark Routine exported by a device driver.  "ttt" is the string passed
//       in as lpszType in RegisterDevice

DWORD
COM_Seek (DWORD dwData, long pos, DWORD type)
{
    return((DWORD)-1);
}

BOOL
WaitEvent(PIRCOMM_OPEN pOpen, PULONG pEventMask)
{
    DWORD Priority256 = CeGetThreadPriority(GetCurrentThread());

    EnterCriticalSection(&IrcommCs);
    if (State == IRCOMM_CLOSED) {
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:WaitEvent state==IRCOMM_CLOSED\r\n")));
        *pEventMask = 0;
        LeaveCriticalSection(&IrcommCs);
        SetLastError(ERROR_GEN_FAILURE);
        return FALSE;
    }

    /* We should return immediately if mask is 0
     */
    if ((vEventMask == 0) || (pOpen->dwEventMask == 0))
    {
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:WaitEvent, empty event mask\r\n")));
        *pEventMask = 0;
        LeaveCriticalSection(&IrcommCs);
        return FALSE;
    }

    CeSetThreadPriority(GetCurrentThread(), g_dwHighThreadPrio);

    /* Do we need to wait?
    */
    if (pOpen->dwEventMask & pOpen->dwEventData) {
        goto we_gotit;
    }

    if (!SocketsCreated)
    {
        if (CreateSockets() == FALSE) {
            LeaveCriticalSection(&IrcommCs);
            CeSetThreadPriority(GetCurrentThread(), Priority256);
            DEBUGMSG(ZONE_INTERFACE | ZONE_FUNCTION| ZONE_ERROR, (TEXT("IRCOMM:WaitEvent: CreateSockets failed\r\n")));
            SetLastError(ERROR_GEN_FAILURE);
            return FALSE;
        }
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:WaitEvent: Waiting for CommEvent\r\n")));

    pOpen->dwWaitThds++;
    WaitForSingleObject(pOpen->hCommEvent, (ULONG)0);

    LeaveCriticalSection(&IrcommCs);

    WaitForSingleObject(pOpen->hCommEvent, (ULONG)-1);

    /* Get the critical section protecting the data
     */
    EnterCriticalSection(&IrcommCs);
    pOpen->dwWaitThds--;

we_gotit:

    DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:WaitEvent Got event %x\r\n"), pOpen->dwEventData));


    /* Get the data.
     */

    *pEventMask = pOpen->dwEventData;

    /* Clear the events that we just handled
     */
    pOpen->dwEventData = 0;
    LeaveCriticalSection(&IrcommCs);

    CeSetThreadPriority(GetCurrentThread(), Priority256);

    return TRUE;
}

BOOL
SafeWriteLPDWORD(
    LPDWORD lpdw,
    DWORD   dw
    )
{
    BOOL bRet = TRUE;

    try {
        *lpdw = dw;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        bRet = FALSE;
    }
    return bRet;
}

BOOL
SafeReadLPDWORD(
    LPDWORD lpdwRead,
    LPDWORD lpdwWrite
    )
{
    BOOL bRet = TRUE;

    try {
        *lpdwWrite = *lpdwRead;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        bRet = FALSE;
    }
    return bRet;
}


#define MAX_IRCOMM_SAFE_ZERO_MEM_SIZE 256

BOOL
SafeZeroMem(
    LPVOID lpDest,
    DWORD  dwSize
    )
{
    BOOL bRet = TRUE;
    //
    // Make Prefast happy
    //
    if (dwSize > MAX_IRCOMM_SAFE_ZERO_MEM_SIZE) {
        dwSize = MAX_IRCOMM_SAFE_ZERO_MEM_SIZE;
    }

    try {
        memset(lpDest, 0, dwSize);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        bRet = FALSE;
    }
    return bRet;
}


//  @func BOOL | ttt_IOControl | Device IO control routine
//  @parm DWORD | dwOpenData | value returned from ttt_Open call
//  @parm DWORD | dwCode | io control code to be performed
//  @parm PBYTE | pBufIn | input data to the device
//  @parm DWORD | dwLenIn | number of bytes being passed in
//  @parm PBYTE | pBufOut | output data from the device
//  @parm DWORD | dwLenOut |maximum number of bytes to receive from device
//  @parm PDWORD | pdwActualOut | actual number of bytes received from device
//  @rdesc  Returns TRUE for success, FALSE for failure
//  @remark Routine exported by a device driver.  "ttt" is the string passed
//      in as lpszType in RegisterDevice

BOOL
COM_IOControl(DWORD dwOpenData, DWORD dwCode, PBYTE pBufIn,
              DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
              PDWORD pdwActualOut)
{
    BOOL RetVal = TRUE; // Initialize to success
    DWORD Priority256;
    PIRCOMM_OPEN pOpen;
    DWORD dwActualOut = 0;
    DWORD dwError = 0;


    DEBUGMSG(ZONE_INTERFACE,
        (TEXT("IRCOMM:+COM_IOControl(0x%X, %d, 0x%X, %d, 0x%X, %d, 0x%X)\r\n"),
         dwOpenData, dwCode, pBufIn, dwLenIn, pBufOut, dwLenOut, pdwActualOut));


    if (NULL == (pOpen = IsValidOpen(dwOpenData))) {
        SetLastError(ERROR_INVALID_HANDLE);
        DEBUGMSG(ZONE_INTERFACE | ZONE_ERROR,(TEXT("IRCOMM:-COM_IOControl: Invalid handle\r\n")));
        return -1;
    }

    switch (dwCode)
    {
        //  @func   BOOL | IOCTL_SERIAL_SET_BREAK_ON |
        //      Device IO control routine to set the break state.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_SET_BREAK_ON
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @remark Sets the transmission line in a break state until
        //      <f IOCTL_SERIAL_SET_BREAK_OFF> is called.
        //
      case IOCTL_SERIAL_SET_BREAK_ON:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_SET_BREAK_ON\r\n")));
        break;

        //  @func   BOOL | IOCTL_SERIAL_SET_BREAK_OFF |
        //      Device IO control routine to clear the break state.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_SET_BREAK_OFF
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @remark Restores character transmission for the communications
        //      device and places the transmission line in a nonbreak state
        //      (called after <f IOCTL_SERIAL_SET_BREAK_ON>).
        //
      case IOCTL_SERIAL_SET_BREAK_OFF:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_SET_BREAK_OFF\r\n")));
        break;

        //  @func   BOOL | IOCTL_SERIAL_SET_DTR |
        //      Device IO control routine to set DTR high.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_SET_DTR
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @xref   <f IOCTL_SERIAL_CLR_DTR>
        //
      case IOCTL_SERIAL_SET_DTR:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_SET_DTR\r\n")));
        break;
        //  @func   BOOL | IOCTL_SERIAL_CLR_DTR |
        //      Device IO control routine to set DTR low.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_CLR_DTR
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @xref   <f IOCTL_SERIAL_SET_DTR>
        //
      case IOCTL_SERIAL_CLR_DTR:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_CLR_DTR\r\n")));
        break;

        //  @func   BOOL | IOCTL_SERIAL_SET_RTS |
        //      Device IO control routine to set RTS high.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_SET_RTS
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @xref   <f IOCTL_SERIAL_CLR_RTS>
        //
      case IOCTL_SERIAL_SET_RTS:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_SET_RTS\r\n")));
        break;

        //  @func   BOOL | IOCTL_SERIAL_CLR_RTS |
        //      Device IO control routine to set RTS low.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_CLR_RTS
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @xref   <f IOCTL_SERIAL_SET_RTS>
        //
      case IOCTL_SERIAL_CLR_RTS:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_CLR_RTS\r\n")));
        break;

        //  @func   BOOL | IOCTL_SERIAL_SET_XOFF |
        //      Device IO control routine to cause transmission
        //      to act as if an XOFF character has been received.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_SET_XOFF
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @xref   <f IOCTL_SERIAL_SET_XON>
        //
      case IOCTL_SERIAL_SET_XOFF:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_SET_XOFF\r\n")));
        break;

        //  @func   BOOL | IOCTL_SERIAL_SET_XON |
        //      Device IO control routine to cause transmission
        //      to act as if an XON character has been received.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_SET_XON
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @xref   <f IOCTL_SERIAL_SET_XOFF>
        //
      case IOCTL_SERIAL_SET_XON:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_SET_XON\r\n")));
        break;

        //  @func   BOOL | IOCTL_SERIAL_GET_WAIT_MASK |
        //      Device IO control routine to retrieve the value
        //      of the event mask.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_GET_WAIT_MASK
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Points to DWORD to place event mask
        //  @parm DWORD | dwLenOut | should be sizeof(DWORD) or larger
        //  @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //      of returned data (should be set to sizeof(DWORD) if no error)
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @xref   <f IOCTL_SERIAL_SET_WAIT_MASK>
        //          <f IOCTL_SERIAL_WAIT_ON_MASK>
        //
      case IOCTL_SERIAL_GET_WAIT_MASK:

        if ((dwLenOut < sizeof(DWORD)) ||
            (NULL == pBufOut)          ||
            (NULL == pdwActualOut))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }


        if (SafeWriteLPDWORD((DWORD *)pBufOut, pOpen->dwEventMask)) {  // Set The Wait Mask
            dwActualOut = sizeof(DWORD);
        } else {
            dwError = ERROR_INVALID_ADDRESS;
        }
        DEBUGMSG (ZONE_FUNCTION, (TEXT("IRCOMM: GET_WAIT_MASK %x\r\n"), pOpen->dwEventMask));
        break;

        //  @func   BOOL | IOCTL_SERIAL_SET_WAIT_MASK |
        //      Device IO control routine to set the value
        //      of the event mask.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_SET_WAIT_MASK
        //  @parm PBYTE | pBufIn | Pointer to the DWORD mask value
        //  @parm DWORD | dwLenIn | should be sizeof(DWORD)
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @xref   <f IOCTL_SERIAL_GET_WAIT_MASK>
        //          <f IOCTL_SERIAL_WAIT_ON_MASK>
        //
      case IOCTL_SERIAL_SET_WAIT_MASK:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: SET_WAIT_MASK 0x%X\r\n"), vEventMask));

                if (State == IRCOMM_CLOSED)
                {
                        DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM: ERROR SetCommEvent while state==IRCOMM_CLOSED\n")));
                        dwError = ERROR_INVALID_HANDLE;
                        break;
                }
        else if ((dwLenIn < sizeof(DWORD)) || (NULL == pBufIn))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }

        EnterCriticalSection(&IrcommCs);
        if (!SocketsCreated) {
            if (CreateSockets() == FALSE) {
                LeaveCriticalSection(&IrcommCs);
                DEBUGMSG(ZONE_FUNCTION|ZONE_ERROR, (TEXT("IRCOMM:SetCommEvent: CreateSockets failed\r\n")));
                dwError = ERROR_GEN_FAILURE;
                break;
            }
        }

        // Set the event so any currently waiting will return with an error
        Priority256 = CeGetThreadPriority(GetCurrentThread());
        CeSetThreadPriority(GetCurrentThread(), g_dwHighThreadPrio);

        {
            DWORD i;
            i = pOpen->dwWaitThds;
            while (i) {
                SetEvent(pOpen->hCommEvent);
                i--;
            }
        }

        if (SafeReadLPDWORD((DWORD *) pBufIn, &pOpen->dwEventMask)) {
            ComputeEventMaskOR();
        } else {
            dwError = ERROR_INVALID_ADDRESS;
        }

        LeaveCriticalSection(&IrcommCs);
        CeSetThreadPriority(GetCurrentThread(), Priority256);
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: New MASK 0x%X\r\n"), vEventMask));
        break;

        //  @func   BOOL | IOCTL_SERIAL_WAIT_ON_MASK |
        //      Device IO control routine to wait for a communications
        //      event that matches one in the event mask
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_WAIT_ON_MASK
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Points to DWORD to place event mask.
        //      The returned mask will show the event that terminated
        //      the wait.  If a process attempts to change the device
        //      handle's event mask by using the IOCTL_SERIAL_SET_WAIT_MASK
        //      call the driver should return immediately with (DWORD)0 as
        //      the returned event mask.
        //  @parm DWORD | dwLenOut | should be sizeof(DWORD) or larger
        //  @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //      of returned data (should be set to sizeof(DWORD) if no error)
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @xref   <f IOCTL_SERIAL_GET_WAIT_MASK>
        //          <f IOCTL_SERIAL_SET_WAIT_MASK>
        //
        case IOCTL_SERIAL_WAIT_ON_MASK:
        {
            DWORD dwEventMask;

            DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_WAIT_ON_MASK\r\n")));

            if ((dwLenOut < sizeof(DWORD)) ||
                (NULL == pBufOut)          ||
                (NULL == pdwActualOut))
            {
                dwError = ERROR_INVALID_PARAMETER;
                break;
            }

            dwEventMask = 0;
            RetVal = WaitEvent(pOpen, &dwEventMask);
            if (SafeWriteLPDWORD((DWORD *)pBufOut, dwEventMask)) {
                dwActualOut = sizeof(DWORD);
            } else {
                dwError = ERROR_INVALID_ADDRESS;
            }
        }
        break;

        //  @func   BOOL | IOCTL_SERIAL_GET_COMMSTATUS |
        //      Device IO control routine to clear any pending
        //      communications errors and return the current communication
        //      status.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_GET_COMMSTATUS
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Points to a <f SERIAL_DEV_STATUS>
        //      structure for the returned status information
        //  @parm DWORD | dwLenOut | should be sizeof(SERIAL_DEV_STATUS)
        //      or larger
        //  @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //      of returned data (should be set to sizeof(SERIAL_DEV_STATUS)
        //      if no error)
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //
        case IOCTL_SERIAL_GET_COMMSTATUS:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_GET_COMMSTATUS\r\n")));

        if ((dwLenOut < sizeof(SERIAL_DEV_STATUS)) ||
            (NULL == pBufOut)                      ||
            (NULL == pdwActualOut))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }

        if (SafeZeroMem(pBufOut, sizeof(SERIAL_DEV_STATUS))) {
            dwActualOut = sizeof(SERIAL_DEV_STATUS);
        } else {
            dwError = ERROR_INVALID_ADDRESS;
        }
        break;

        //  @func   BOOL | IOCTL_SERIAL_GET_MODEMSTATUS |
        //      Device IO control routine to retrieve current
        //      modem control-register values
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_GET_MODEMSTATUS
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Points to a DWORD for the returned
        //      modem status information
        //  @parm DWORD | dwLenOut | should be sizeof(DWORD)
        //      or larger
        //  @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //      of returned data (should be set to sizeof(DWORD)
        //      if no error)
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //
        case IOCTL_SERIAL_GET_MODEMSTATUS:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_GET_MODEMSTATUS\r\n")));

        if ((dwLenOut < sizeof(DWORD))  ||
            (NULL == pBufOut)           ||
            (NULL == pdwActualOut))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }

        // Set the Modem Status dword
        if (SafeWriteLPDWORD((DWORD *)pBufOut, (State == IRCOMM_CONNECTED) ? MS_RLSD_ON : 0)) {
            dwActualOut = sizeof(DWORD);
        } else {
            dwError = ERROR_INVALID_ADDRESS;
        }
        break;

        //  @func   BOOL | IOCTL_SERIAL_GET_PROPERTIES |
        //      Device IO control routine to retrieve information
        //      about the communications properties for the device.
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_GET_PROPERTIES
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Points to a <f COMMPROP> structure
        //      for the returned information.
        //  @parm DWORD | dwLenOut | should be sizeof(COMMPROP)
        //      or larger
        //  @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //      of returned data (should be set to sizeof(COMMPROP)
        //      if no error)
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //
      case IOCTL_SERIAL_GET_PROPERTIES:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_GET_PROPERTIES\r\n")));

        if ((dwLenOut < sizeof(COMMPROP)) ||
            (NULL == pBufOut)             ||
            (NULL == pdwActualOut))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }

        // Clear the ComMProp structure
        if (SafeZeroMem(pBufOut, sizeof(COMMPROP))) {
            dwActualOut = sizeof(COMMPROP);
        } else {
            dwError = ERROR_INVALID_ADDRESS;
        }
        break;

        //  @func   BOOL | IOCTL_SERIAL_SET_TIMEOUTS |
        //      Device IO control routine to set the time-out parameters
        //      for all read and write operations on a specified
        //      communications device
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_SET_TIMEOUTS
        //  @parm PBYTE | pBufIn | Pointer to the <f COMMTIMEOUTS> structure
        //  @parm DWORD | dwLenIn | should be sizeof(COMMTIMEOUTS)
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @xref   <f IOCTL_SERIAL_GET_TIMEOUTS>
        //
      case IOCTL_SERIAL_SET_TIMEOUTS :
        if ((dwLenIn < sizeof(COMMTIMEOUTS)) || (NULL == pBufIn))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }

        DEBUGMSG (ZONE_FUNCTION,
            (TEXT("IRCOMM: IOCTL_SERIAL_SET_COMMTIMEOUTS (%d,%d,%d,%d,%d)\r\n"),
            CommTimeouts.ReadIntervalTimeout,
            CommTimeouts.ReadTotalTimeoutMultiplier,
            CommTimeouts.ReadTotalTimeoutConstant,
            CommTimeouts.WriteTotalTimeoutMultiplier,
            CommTimeouts.WriteTotalTimeoutConstant));

        if (0 == CeSafeCopyMemory(&CommTimeouts, pBufIn, sizeof(COMMTIMEOUTS))) {
            dwError = ERROR_INVALID_ADDRESS;
        }
        break;

        //  @func   BOOL | IOCTL_SERIAL_GET_TIMEOUTS |
        //      Device IO control routine to set the time-out parameters
        //      for all read and write operations on a specified
        //      communications device
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_GET_TIMEOUTS
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Pointer to a <f COMMTIMEOUTS> structure
        //      for the returned data
        //  @parm DWORD | dwLenOut | should be sizeof(COMMTIMEOUTS)
        //  @parm PDWORD | pdwActualOut | Points to DWORD to return length
        //      of returned data (should be set to sizeof(COMMTIMEOUTS)
        //      if no error)
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //  @xref   <f IOCTL_SERIAL_GET_TIMEOUTS>
        //
      case IOCTL_SERIAL_GET_TIMEOUTS:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_GET_TIMEOUTS\r\n")));

        if ((dwLenOut < sizeof(COMMTIMEOUTS))   ||
            (NULL == pBufOut)                   ||
            (NULL == pdwActualOut))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }

        if (CeSafeCopyMemory(pBufOut, &CommTimeouts,  sizeof(COMMTIMEOUTS))) {
            dwActualOut = sizeof(COMMTIMEOUTS);
        } else {
            dwError = ERROR_INVALID_ADDRESS;
        }
        break;

        //  @func   BOOL | IOCTL_SERIAL_PURGE |
        //      Device IO control routine to discard characters from the
        //      output or input buffer of a specified communications
        //      resource.  It can also terminate pending read or write
        //      operations on the resource
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_PURGE
        //  @parm PBYTE | pBufIn | Pointer to a DWORD containing the action
        //  @parm DWORD | dwLenIn | Should be sizeof(DWORD)
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //
      case IOCTL_SERIAL_PURGE:
        DEBUGMSG(ZONE_FUNCTION,
            (TEXT("IRCOMM: IOCTL_SERIAL_PURGE\r\n")));

        if ((dwLenIn < sizeof(DWORD)) || (NULL == pBufIn))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }


        // Normally we would do something with the passed in parameter.
        break;

        //  @func   BOOL | IOCTL_SERIAL_SET_QUEUE_SIZE |
        //      Device IO control routine to set the queue sizes of of a
        //      communications device
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_SET_QUEUE_SIZE
        //  @parm PBYTE | pBufIn | Pointer to a <f SERIAL_QUEUE_SIZES>
        //      structure
        //  @parm DWORD | dwLenIn | should be sizeof(<f SERIAL_QUEUE_SIZES>)
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //
      case IOCTL_SERIAL_SET_QUEUE_SIZE:
        DEBUGMSG(ZONE_FUNCTION,
            (TEXT("IRCOMM: IOCTL_SERIAL_SET_QUEUE_SIZE\r\n")));
        if ((dwLenIn < sizeof(SERIAL_QUEUE_SIZES)) || (NULL == pBufIn))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }

        // Normally we would do something with the passed in parameter.
        break;

        //  @func   BOOL | IOCTL_SERIAL_IMMEDIATE_CHAR |
        //      Device IO control routine to transmit a specified character
        //      ahead of any pending data in the output buffer of the
        //      communications device
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_IMMEDIATE_CHAR
        //  @parm PBYTE | pBufIn | Pointer to a UCHAR to send
        //  @parm DWORD | dwLenIn | should be sizeof(UCHAR)
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //
      case IOCTL_SERIAL_IMMEDIATE_CHAR:
        DEBUGMSG(ZONE_FUNCTION,
            (TEXT("IRCOMM: IOCTL_SERIAL_IMMEDIATE_CHAR\r\n")));

        if ((dwLenIn < sizeof(UCHAR)) || (NULL == pBufIn))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }


        // Normally we would do something with the passed in parameter.
        break;

        //  @func   BOOL | IOCTL_SERIAL_GET_DCB |
        //      Device IO control routine to get the device-control
        //      block from a specified communications device
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_GET_DCB
        //  @parm PBYTE | pBufIn | Ignored
        //  @parm DWORD | dwLenIn | Ignored
        //  @parm PBYTE | pBufOut | Pointer to a <f DCB> structure
        //  @parm DWORD | dwLenOut | Should be sizeof(<f DCB>)
        //  @parm PDWORD | pdwActualOut | Pointer to DWORD to return length
        //      of returned data (should be set to sizeof(<f DCB>) if
        //      no error)
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //
      case IOCTL_SERIAL_GET_DCB:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_GET_DCB\r\n")));

        if ((dwLenOut < sizeof(DCB))    ||
            (NULL == pBufOut)           ||
            (NULL == pdwActualOut))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }

        // Clear the structure
        if (SafeZeroMem(pBufOut, sizeof(DCB))) {
            dwActualOut = sizeof(DCB);
        } else {
            dwError = ERROR_INVALID_ADDRESS;
        }
        break;

        //  @func   BOOL | IOCTL_SERIAL_SET_DCB |
        //      Device IO control routine to set the device-control
        //      block on a specified communications device
        //
        //  @parm DWORD | dwOpenData | value returned from ttt_Open call
        //  @parm DWORD | dwCode | IOCTL_SERIAL_SET_DCB
        //  @parm PBYTE | pBufIn | Pointer to a <f DCB> structure
        //  @parm DWORD | dwLenIn | should be sizeof(<f DCB>)
        //  @parm PBYTE | pBufOut | Ignored
        //  @parm DWORD | dwLenOut | Ignored
        //  @parm PDWORD | pdwActualOut | Ignored
        //
        //  @rdesc  Returns TRUE for success, FALSE for failure (and
        //      sets thread error code)
        //
        //
      case IOCTL_SERIAL_SET_DCB:
        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: IOCTL_SERIAL_SET_DCB\r\n")));
        if ((dwLenIn < sizeof(DCB)) || (NULL == pBufIn))
        {
            dwError = ERROR_INVALID_PARAMETER;
            break;
        }

        // Normally we would do something with the passed in parameter.
        break;

      default:
        dwError = ERROR_INVALID_PARAMETER;
        break;
    }

    DerefOpen(dwOpenData);

    if (dwActualOut) {
        if (FALSE == SafeWriteLPDWORD(pdwActualOut, dwActualOut)) {
            dwError = ERROR_INVALID_ADDRESS;
        }
    }

    if (dwError) {
        RetVal = FALSE;
        SetLastError(dwError);
    }

    DEBUGMSG(ZONE_INTERFACE | ZONE_FUNCTION,
        (TEXT("IRCOMM:-COM_IOControl %s (len=%d)\r\n"),
        (RetVal == TRUE) ? TEXT("Success") : TEXT("Error"),
        dwActualOut));

    return(RetVal);
}

DWORD WINAPI
AcceptThread(PVOID pThreadParm)
{
    int     sizeofInt       = sizeof(int);
    int     sizeofSockAddr  = sizeof(SOCKADDR_IRDA);
    DWORD   dwListenInstance = (DWORD)pThreadParm;
    SOCKET  LocalListenSock;
    SOCKADDR_IRDA SockAddrRemote;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("+IRCOMM:AcceptThread: Start\r\n")));

    EnterCriticalSection(&IrcommCs);
    LocalListenSock = ListenSock;
    LeaveCriticalSection(&IrcommCs);

    while (1) {
        EnterCriticalSection(&IrcommCs);
        if (dwListenInstance != g_dwListenInstance) {
            //
            // If the global instance count has changed then the ListenSock that
            // this thread was created for has been closed.  Because the handle value
            // itself may be reused, we keep track using this simple counter instead
            // of watching the handle value or its state.
            //
            DEBUGMSG (ZONE_WARN, (TEXT("IRCOMM: accept() exiting, instance %08X != %08X\r\n"),
                                  dwListenInstance, g_dwListenInstance));
            LeaveCriticalSection(&IrcommCs);
            break;
        }
        LeaveCriticalSection(&IrcommCs);


        if ((AcceptSock = accept(LocalListenSock,
                                 (struct sockaddr *) &SockAddrRemote,
                                 &sizeofSockAddr)) == INVALID_SOCKET)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM: accept() %s\n\r"), GetLastErrorText()));

            if (GetLastError() == WSAENOTSOCK) {
                DEBUGMSG (ZONE_FUNCTION, (TEXT("-IRCOMM:AcceptThread: End Accept returned WASENOTSOCK\r\n")));
                return(0);
            } else {
                continue;
            }
        }


        if (getsockopt(AcceptSock, SOL_IRLMP, IRLMP_SEND_PDU_LEN,
                       (char *) &SendMaxPDU, &sizeofInt) == SOCKET_ERROR)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM: getsockopt(,,IRLMP_SEND_PDU_LEN,,) %s\n\r"), GetLastErrorText()));
            closesocket(AcceptSock);
            continue;
        }

        memcpy(&SockAddr.irdaDeviceID[0], &SockAddrRemote.irdaDeviceID, 4);

        if (InitializeConnection(AcceptSock) != 0) {
            // Error occured?
            closesocket(AcceptSock);
            AcceptSock = INVALID_SOCKET;
        }
        break;
    }

    DEBUGMSG (ZONE_FUNCTION, (TEXT("-IRCOMM:AcceptThread: End, return 0\r\n")));
    return(0);
}

int
IrCOMMConnect(void)
{
    // allow discovery of 5 devices
    BYTE        DevListBuff[sizeof(DEVICELIST) + (sizeof(IRDA_DEVICE_INFO) * 4)];
    int         DevListLen = sizeof(DevListBuff);
    PDEVICELIST pDevList   = (PDEVICELIST)DevListBuff;
    int         sizeofInt = sizeof(int);
    int         Enable9WireMode  = 1;
    int         i;


    if (setsockopt(ConnectSock, SOL_IRLMP, IRLMP_9WIRE_MODE,
                   (const char *) &Enable9WireMode, sizeof(int))
        == SOCKET_ERROR)
    {
        closesocket(ConnectSock);
        ConnectSock = INVALID_SOCKET;
        return(-1);
    }

    if (State == IRCOMM_DISCOVERY_PENDING)
    {
        pDevList->numDevice = 0;

        for (i = 0; (i < 6) && (pDevList->numDevice == 0); i++)
        {
            if (getsockopt(ConnectSock, SOL_IRLMP, IRLMP_ENUMDEVICES,
                           (char *) pDevList, &DevListLen) == SOCKET_ERROR)
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM: getsockopt(,,IRLMP_ENUM_DEVICES,,) %s\n\r"), GetLastErrorText()));
                return(-1);
            }
        }

        if (pDevList->numDevice == 0)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM: No devices found\r\n")));
            return(-1);
        }

        memcpy(&SockAddr.irdaDeviceID[0],
               &pDevList->Device[0].irdaDeviceID[0], 4);

        NewState(IRCOMM_CONNECT_PENDING);

#ifdef DEBUG
        for (i = 0; i < (int)pDevList->numDevice; i++) {
            DEBUGMSG(ZONE_CONN, (L"IRCOMM: Device %d = %a\n", i, pDevList->Device[i].irdaDeviceName));
        }
#endif

    }

    if (State == IRCOMM_CONNECT_PENDING)
    {
        if (connect(ConnectSock, (const struct sockaddr *) &SockAddr,
                    sizeof(SOCKADDR_IRDA)) == SOCKET_ERROR)
        {
            DEBUGMSG(ZONE_ERROR,
                (TEXT("IRCOMM: connect() %s\n\r"), GetLastErrorText()));
            return(-1);
        }
        else
        {
            if (getsockopt(ConnectSock, SOL_IRLMP, IRLMP_SEND_PDU_LEN,
                           (char *) &SendMaxPDU, &sizeofInt) == SOCKET_ERROR)
            {
                DEBUGMSG(ZONE_ERROR,
                    (TEXT("IRCOMM: getsockopt(,,IRLMP_SEND_PDU_LEN,,) %s\n\r"),
                    GetLastErrorText()));

                return(-1);
            }

            return InitializeConnection(ConnectSock);
        }
    }

    return(0);
}

int
InitializeConnection(SOCKET p_DataSock)
{
    HANDLE      hThrd;
    int         result = -1;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:+InitializeConnection %d\n"), p_DataSock));

    EnterCriticalSection(&IrcommCs);

    if (DataSock != INVALID_SOCKET) {
        // A connection is already established, and only 1 connection is allowed at a time.
        DEBUGMSG(ZONE_FUNCTION|ZONE_WARN, (TEXT("IRCOMM:-InitializeConnection - WARNING connection already established\n")));

        LeaveCriticalSection(&IrcommCs);
        return result;
    }

    NewState(IRCOMM_CONNECTED);

    ASSERT(DataSock == INVALID_SOCKET);

    DataSock    = p_DataSock;

    pRxRingRead = pRxRingWrite = RxRing;
    pRxRingMax  = RxRing + IRCOMM_RX_RING_SIZE;

    pTxRingRead = pTxRingWrite = TxRing;
    pTxRingMax  = TxRing + IRCOMM_TX_RING_SIZE;

    // Indicate to any existing accept threads that their ListenSock is no longer valid.
    ++g_dwListenInstance;

    closesocket(ListenSock);
    ListenSock = INVALID_SOCKET;
    LeaveCriticalSection(&IrcommCs);

    ASSERT(SendMaxPDU <= IRCOMM_TX_BUF_SIZE);

    if ((hThrd = CreateThread(NULL, 0, TxThread, NULL, 0, NULL)) != 0) {
        CeSetThreadPriority(hThrd, g_dwHighThreadPrio);
        CloseHandle(hThrd);

        if ((hThrd = CreateThread(NULL, 0, RxThread, NULL, 0, NULL)) != 0) {
            CeSetThreadPriority(hThrd, g_dwHighThreadPrio);
            CloseHandle(hThrd);
            NewCommEvent(EV_RLSD);
            result = 0;
        } else {
            // Need to terminate TxThread at this point
            NewState(IRCOMM_OPENED);
            SetEvent(hTxEvent);
        }
    }

        DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM: -InitializeConnection result = %d\r\n"), result));

    return result;
}

DWORD WINAPI
TxThread(PVOID pThreadParm)
{
    int TxLen;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+IrCOMM:TxThread: Start\r\n")));

    while (1)
    {
        // Wait for something to send

        if (pTxRingRead == pTxRingWrite)
        {
            NewCommEvent(EV_TXEMPTY);

            DEBUGMSG(ZONE_FUNCTION, (TEXT("IRCOMM:TxThread waiting...\r\n")));

            WaitForSingleObject(hTxEvent, INFINITE);
        }

        EnterCriticalSection(&IrcommCs);

        if (State != IRCOMM_CONNECTED)
        {
            DEBUGMSG(1, (TEXT("IRCOMM:TxThread exit1\r\n")));

            LeaveCriticalSection(&IrcommCs);

            DEBUGMSG (ZONE_FUNCTION|ZONE_ERROR, (TEXT("-IRCOMM:TxThread: Exit (State!=IRCOMM_CONNECTED)\r\n")));
            return 0;
        }

        for (TxLen=0; TxLen < SendMaxPDU - 1 && pTxRingRead != pTxRingWrite;
             TxLen++)
        {
            TxBuf[TxLen] = *pTxRingRead++;
            if (pTxRingRead == pTxRingMax)
            {
                pTxRingRead = TxRing;
            }
        }

        LeaveCriticalSection(&IrcommCs);

        if (send(DataSock, TxBuf, TxLen, 0) == SOCKET_ERROR)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("IRCOMM: send() %s\n\r"), GetLastErrorText()));

            Shutdown(FALSE);    // FALSE => Don't close

            DEBUGMSG (ZONE_FUNCTION|ZONE_ERROR, (TEXT("-IRCOMM:TxThread: Exit Error from send(%d)\r\n"), GetLastError()));
            return 0;
        }
    }

    DEBUGMSG (ZONE_FUNCTION, (TEXT("-IRCOMM:TxThread: End\r\n")));

    return 0;
}


DWORD WINAPI
RxThread(PVOID pThreadParm)
{
#define RXBUF_SIZE  256
    BYTE    RecvBuf[RXBUF_SIZE];
    int     RecvCnt, i;
    BYTE    *pLast, *pRead;

    DEBUGMSG (ZONE_FUNCTION, (TEXT("+IRCOMM:RxThread: Start\r\n")));

    while (1)
    {
        if ((RecvCnt = recv(DataSock, RecvBuf, RXBUF_SIZE, 0))
            == SOCKET_ERROR || RecvCnt == 0)
        {
            DEBUGMSG (ZONE_FUNCTION|ZONE_ERROR, (TEXT("-IRCOMM:RxThread: recv() failed %s/%d\r\n"),
                       GetLastErrorText(), GetLastError()));

            Shutdown(FALSE);    // FALSE => Don't close

            return 0;
        }

        EnterCriticalSection(&IrcommCs);

        if (State == IRCOMM_CLOSED)
        {
            LeaveCriticalSection(&IrcommCs);
            DEBUGMSG (ZONE_FUNCTION|ZONE_ERROR, (TEXT("-IRCOMM:RxThread: State == IRCOMM_CLOSED\r\n")));
            return 0;
        }

        pRead = pRxRingRead;
        pLast = pRead == RxRing ? pRxRingMax-1 : pRead-1;

        DEBUGMSG(ZONE_RECV|ZONE_MISC, (TEXT("IRCOMM:RxThread read %d \r\n"), RecvCnt));

        i = 0;

        while (i < RecvCnt && pRxRingWrite != pLast)
        {
            *pRxRingWrite++ = RecvBuf[i++];

            if (pRxRingWrite == pRxRingMax)
                pRxRingWrite = RxRing;

            pRead = pRxRingRead;
            pLast = pRead == TxRing ? pTxRingMax-1 : pRead-1;
        }

        LeaveCriticalSection(&IrcommCs);

        if (i < RecvCnt)
        {
            DEBUGMSG(ZONE_WARN, (TEXT("IRCOMM: RxRing overflow Read %d, Total %d\n"), i, RecvCnt));
        }

        SetEvent(hRxEvent);

        if (RecvCnt)
        {
            NewCommEvent(EV_RXCHAR);
        }
        else
            break;
    }

    DEBUGMSG (ZONE_FUNCTION, (TEXT("-IRCOMM:RxThread: State == IRCOMM_CLOSED\r\n")));
    return 0;
}

#ifdef DEBUG
TCHAR *
GetLastErrorText(void)
{
    switch (GetLastError())
    {
      case WSAEINTR:
        return (TEXT("WSAEINTR"));
        break;

      case WSAEBADF:
        return(TEXT("WSAEBADF"));
        break;

      case WSAEACCES:
        return(TEXT("WSAEACCES"));
        break;

      case WSAEFAULT:
        return(TEXT("WSAEFAULT"));
        break;

      case WSAEINVAL:
        return(TEXT("WSAEINVAL"));
        break;

      case WSAEMFILE:
        return(TEXT("WSAEMFILE"));
        break;

      case WSAEWOULDBLOCK:
        return(TEXT("WSAEWOULDBLOCK"));
        break;

      case WSAEINPROGRESS:
        return(TEXT("WSAEINPROGRESS"));
        break;

      case WSAEALREADY:
        return(TEXT("WSAEALREADY"));
        break;

      case WSAENOTSOCK:
        return(TEXT("WSAENOTSOCK"));
        break;

      case WSAEDESTADDRREQ:
        return(TEXT("WSAEDESTADDRREQ"));
        break;

      case WSAEMSGSIZE:
        return(TEXT("WSAEMSGSIZE"));
        break;

      case WSAEPROTOTYPE:
        return(TEXT("WSAEPROTOTYPE"));
        break;

      case WSAENOPROTOOPT:
        return(TEXT("WSAENOPROTOOPT"));
        break;

      case WSAEPROTONOSUPPORT:
        return(TEXT("WSAEPROTONOSUPPORT"));
        break;

      case WSAESOCKTNOSUPPORT:
        return(TEXT("WSAESOCKTNOSUPPORT"));
        break;

      case WSAEOPNOTSUPP:
        return(TEXT("WSAEOPNOTSUPP"));
        break;

      case WSAEPFNOSUPPORT:
        return(TEXT("WSAEPFNOSUPPORT"));
        break;

      case WSAEAFNOSUPPORT:
        return(TEXT("WSAEAFNOSUPPORT"));
        break;

      case WSAEADDRINUSE:
        return(TEXT("WSAEADDRINUSE"));
        break;

      case WSAEADDRNOTAVAIL:
        return(TEXT("WSAEADDRNOTAVAIL"));
        break;

      case WSAENETDOWN:
        return(TEXT("WSAENETDOWN"));
        break;

      case WSAENETUNREACH:
        return(TEXT("WSAENETUNREACH"));
        break;

      case WSAENETRESET:
        return(TEXT("WSAENETRESET"));
        break;

      case WSAECONNABORTED:
        return(TEXT("WSAECONNABORTED"));
        break;

      case WSAECONNRESET:
        return(TEXT("WSAECONNRESET"));
        break;

      case WSAENOBUFS:
        return(TEXT("WSAENOBUFS"));
        break;

      case WSAEISCONN:
        return(TEXT("WSAEISCONN"));
        break;

      case WSAENOTCONN:
        return(TEXT("WSAENOTCONN"));
        break;

      case WSAESHUTDOWN:
        return(TEXT("WSAESHUTDOWN"));
        break;

      case WSAETOOMANYREFS:
        return(TEXT("WSAETOOMANYREFS"));
        break;

      case WSAETIMEDOUT:
        return(TEXT("WSAETIMEDOUT"));
        break;

      case WSAECONNREFUSED:
        return(TEXT("WSAECONNREFUSED"));
        break;

      case WSAELOOP:
        return(TEXT("WSAELOOP"));
        break;

      case WSAENAMETOOLONG:
        return(TEXT("WSAENAMETOOLONG"));
        break;

      case WSAEHOSTDOWN:
        return(TEXT("WSAEHOSTDOWN"));
        break;

      case WSAEHOSTUNREACH:
        return(TEXT("WSAEHOSTUNREACH"));
        break;

      case WSAENOTEMPTY:
        return(TEXT("WSAENOTEMPTY"));
        break;

      case WSAEPROCLIM:
        return(TEXT("WSAEPROCLIM"));
        break;

      case WSAEUSERS:
        return(TEXT("WSAEUSERS"));
        break;

      case WSAEDQUOT:
        return(TEXT("WSAEDQUOT"));
        break;

      case WSAESTALE:
        return(TEXT("WSAESTALE"));
        break;

      case WSAEREMOTE:
        return(TEXT("WSAEREMOTE"));
        break;

      case WSAEDISCON:
        return(TEXT("WSAEDISCON"));
        break;

      case WSASYSNOTREADY:
        return(TEXT("WSASYSNOTREADY"));
        break;

      case WSAVERNOTSUPPORTED:
        return(TEXT("WSAVERNOTSUPPORTED"));
        break;

      case WSANOTINITIALISED:
        return(TEXT("WSANOTINITIALISED"));
        break;

        /*
      case WSAHOST:
        return(TEXT("WSAHOST"));
        break;

      case WSATRY:
        return(TEXT("WSATRY"));
        break;

      case WSANO:
        return(TEXT("WSANO"));
        break;
        */

      default:
        return(TEXT("Unknown Error"));
    }
}
#endif


