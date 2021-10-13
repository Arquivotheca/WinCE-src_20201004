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
/*****************************************************************************/
/**                            Microsoft Windows                            **/
/*****************************************************************************/

/*

wceinit.c

installation and thread local storage related functions

FILE HISTORY:
    OmarM     14-Sep-2000


*/


#include "winsock2p.h"
#include "cxport.h"
#include <perfcomm.h>

extern void FreeLookups();
extern int FreeProviders(uint Flags);
extern void FreeNsProviders();
extern void UnloadWsCmExt(void);
extern BOOL g_bWSAAsyncCSInit;
extern CRITICAL_SECTION g_WSAAsyncCS;
extern WSAEVENT g_WSAAsyncSelectSignal;

//  Global variables

int                 v_FreeSockets;
int                 v_NextSockHandle;
LPSOCK_INFO         v_aSockHandles[NUM_SOCKETS];

CRITICAL_SECTION    v_DllCS;
HINSTANCE           v_hDll;
int                 v_fCleaningUp;
int                 v_fProcessDetached;

extern CTELock s_NsProvListLock;
extern CTELock s_NsProvListLock;
extern CTELock s_NsLookupsLock;
extern CTELock s_ProviderListLock;

// Debug Zones.
//
#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("WinSock"), {
    TEXT("Accept"),TEXT("Bind"),TEXT("Connect"),TEXT("Select"),
    TEXT("Recv"),TEXT("Send"),TEXT("Socket"),TEXT("Listen"),
    TEXT("Ioctl"),TEXT("Ssl"),TEXT("Undefined"),TEXT("Misc"),
    TEXT("Alloc"),TEXT("Function"),TEXT("Warning"),TEXT("Error") },
    0
};
#endif



//  Static Variables

static DWORD        s_SockTlsSlot = INVALID_TLS_INDEX;
DWORD               s_cDllRefs;


int SOCKAPI sethostname(IN char FAR *pName, IN int cName);

// Get it working for now, can optimize later...


int Started() {
    int Err;

    if (s_cDllRefs > 0)
        Err = 0;
    else
        Err = WSANOTINITIALISED;

    return Err;

}   // Started()


int WSAAPI WSAStartup(
    IN WORD wVersionRequired,
    OUT LPWSADATA lpWSAData) {

    int     Err = 0;
    WORD    Major, Minor, OfferMajor, OfferMinor;


    if (v_fProcessDetached)
        Err = WSAENETDOWN;

    if (!v_hDll)
        Err = WSASYSNOTREADY;

    if (! Err) {
        CTEGetLock(&v_DllCS, 0);
        if (v_fCleaningUp)
            Err = WSASYSNOTREADY;
        CTEFreeLock(&v_DllCS, 0);
    }

    if (Err)
        goto Exit;

    Major = LOBYTE(wVersionRequired);
    Minor = HIBYTE(wVersionRequired);

    if (Major < 1) {
        Err = WSAVERNOTSUPPORTED;
        goto Exit;
    } else if (1 == Major) {
        OfferMajor = 1;
        if (Minor <= 1)
            OfferMinor = Minor;
        else
            OfferMinor = 1;
    } else {
        OfferMajor = 2;
        if (Major > 2 || Minor > 2)
            OfferMinor = 2;
        else
            OfferMinor = Minor;
    }

    __try {
        lpWSAData->wVersion = MAKEWORD(OfferMajor, OfferMinor);
        lpWSAData->wHighVersion = WINSOCK_VERSION;
        StringCchCopyA(lpWSAData->szDescription, _countof(lpWSAData->szDescription), "Winsock 2.2");
        lpWSAData->szSystemStatus[0]= '\0';

        // the following fields are to be ignored by Winsock 2 but fill them
        // for Winsock 1 apps
        // Two new socket options are introduced to supply provider-specific
        // information: SO_MAX_MSG_SIZE (replaces the iMaxUdpDg element) and
        // PVD_CONFIG (allows other provider-specific configuration to occur)

        lpWSAData->iMaxUdpDg = 0;

        if (1 == Major) {
            lpWSAData->iMaxSockets = NUM_SOCKETS;
            lpWSAData->lpVendorInfo = NULL;
        } else {
            lpWSAData->iMaxSockets = 0;
            // we no longer set lpVendorInfo b/c of the following note in NT

    // The following line is commented-out due to annoying and totally
    // nasty alignment problems in WINSOCK[2].H. The exact location of
    // the lpVendorInfo field of the WSAData structure is dependent on
    // the structure alignment used when compiling the source. Since we
    // cannot change the structure alignment of existing apps, the best
    // way to handle this mess is to just not set this value. This turns
    // out to not be too bad a solution, as neither the WinNT nor the Win95
    // WinSock implementations set this value, and nobody appears to pay
    // any attention to it anyway.

            // lpWSAData->lpVendorInfo = NULL;

        }
    }

    __except(EXCEPTION_EXECUTE_HANDLER) {
        Err = WSAEFAULT;
    }

Exit:
    if (0 == Err) {
        CTEGetLock(&v_DllCS, 0);
        s_cDllRefs++;
        CTEFreeLock(&v_DllCS, 0);
    }

    return Err;

}   // WSAStartup()

#define SOCK_HASH_SIZE  64
extern WsSocket *v_apWsSocks[];

int WSAAPI WSACleanup() {
    int Err = 0;
    int fDelete = FALSE;

    if (v_fProcessDetached) {
        Err = WSAENETDOWN;
        goto Exit;
    }

    CTEGetLock(&v_DllCS, 0);
    if ((0 == s_cDllRefs) || (NULL == v_hDll)) {
        Err = WSANOTINITIALISED;
    } else {
        if (0 == --s_cDllRefs) {
            fDelete = v_fCleaningUp = TRUE;
        }
    }
    CTEFreeLock(&v_DllCS, 0);

    // technically we should do this before freeing v_DllCS
    // but an app shouldn't usually be calling startup and cleanup right
    // one after the other
    if (fDelete) {
        CloseAllSockets();
        FreeLookups();
        FreeProviders(0);
        FreeNsProviders();
        UnloadWsCmExt();
        v_fCleaningUp = FALSE;
    }

Exit:
    if (Err) {
        SetLastError(Err);
        Err = SOCKET_ERROR;
    }

    return Err;

}   // WSACleanup()


BOOL GetThreadData (LPSOCK_THREAD *ppThread) {
    LPSOCK_THREAD   pThread;

    if (s_SockTlsSlot != INVALID_TLS_INDEX) {
#ifdef DEBUG
        if ((DWORD)GetOwnerProcess() != GetCurrentProcessId()) {
            DEBUGMSG (ZONE_WARN,
                      (TEXT("GetCurrentProcessId=0x%X ")
                       TEXT("GetCallerProcess=0x%X ")
                       TEXT("GetOwnerProcess=0x%X\r\n"),
                       GetCurrentProcessId(), GetCallerProcess(),
                       GetOwnerProcess()));
        }
#endif

        ASSERT((DWORD)GetOwnerProcess() == GetCurrentProcessId());

        pThread = (LPSOCK_THREAD)TlsGetValue(s_SockTlsSlot);
        if (NULL == pThread) {
            // Try to allocate the data.
            pThread = DllAllocMem (sizeof(SOCK_THREAD));
            if (pThread) {
                memset ((char *)pThread, 0, sizeof(SOCK_THREAD));
            }
            TlsSetValue (s_SockTlsSlot, (LPVOID)pThread);
        }
        if (pThread) {
            *ppThread = pThread;
            return TRUE;
        }
    }
    // Some error occured.
    return FALSE;
}

// Callback function for CeTlsAlloc().  This is called
// whenever a thread detaches so that TLS data can be
// cleaned up.

void
DeleteTlsData(DWORD ThreadId, LPVOID SlotValue)
{
    LPSOCK_THREAD    pThread;

    pThread = (LPSOCK_THREAD)TlsGetValue(s_SockTlsSlot);

    if (pThread) {
        // Free the sock data
        DllFreeMem (pThread);
    }
}

BOOL
__stdcall
DllEntry (
    HANDLE  hinstDLL,
    DWORD   Op,
    LPVOID  lpvReserved
    )
{

    switch (Op) {
    case DLL_PROCESS_ATTACH:

        DEBUGREGISTER(hinstDLL);
        DEBUGMSG (ZONE_MISC, (TEXT("WS2: dllentry()\r\n")));
        CTEInitLock(&v_DllCS);
        CTEInitLock(&s_NsProvListLock);
        CTEInitLock(&s_NsLookupsLock);
        CTEInitLock(&s_ProviderListLock);

        s_SockTlsSlot = CeTlsAlloc(DeleteTlsData);

        if(INVALID_TLS_INDEX == s_SockTlsSlot) {
            RETAILMSG(1, (TEXT("dllentry: TlsAlloc failed!!!\r\n")));
            return FALSE;
        }

        PerfCommInit();

        v_hDll = hinstDLL;
        break;

    case DLL_PROCESS_DETACH:

        if (v_hDll) {
            v_fProcessDetached = TRUE;
            CloseAllSockets();
            FreeLookups();
            FreeProviders(0);
            FreeNsProviders();
            CTEDeleteLock(&v_DllCS);
            CTEDeleteLock(&s_NsProvListLock);
            CTEDeleteLock(&s_NsLookupsLock);
            CTEDeleteLock(&s_ProviderListLock);
            // Calling CeTlsFree which will callback to DeleteTlsData and set the value of the slot to 0.
            // No more TlsSetValue(s_SockTlsSlot, 0) needed.
            CeTlsFree(s_SockTlsSlot);

            s_SockTlsSlot = INVALID_TLS_INDEX;

            if (g_bWSAAsyncCSInit) {
                g_bWSAAsyncCSInit = FALSE;
                DeleteCriticalSection(&g_WSAAsyncCS);
                CloseHandle(g_WSAAsyncSelectSignal);
            }

            PerfCommDeInit();
        }
        break;

    case DLL_THREAD_DETACH:

        if (s_SockTlsSlot != INVALID_TLS_INDEX) {
            CeTlsFree(s_SockTlsSlot);
        }
        break;

    case DLL_THREAD_ATTACH:
        // Don't bother allocating the slot data until they use sockets.
        // fall-thru
        default:
        break;
    }
    return TRUE;
}

#include <perfcomm.c>

