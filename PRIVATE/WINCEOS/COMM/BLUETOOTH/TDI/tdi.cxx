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
//------------------------------------------------------------------------------
// 
//      Bluetooth TDI Layer
// 
// 
// Module Name:
// 
//      tdi.cxx
// 
// Abstract:
// 
//      This file implements Bluetooth TDI Layer
// 
// 
//------------------------------------------------------------------------------
#if defined (UNDER_CE)
#include <windows.h>
#include <svsutil.hxx>

#if ! defined (UNDER_CE)
#include <stddef.h>
#include <malloc.h>
#else
#include <stdlib.h>
#endif

#include <winsock.h>
#include <ws2bth.h>
#include <bt_debug.h>
#include <bt_os.h>
#include <bt_buffer.h>
#include <bt_hcip.h>
#include <bt_ddi.h>
#include <bt_api.h>

#include <memory.h>

#include <nspapi.h>
#include <cxport.h>
#include <ndis.h>
#include <tdi.h>
#include <tdistat.h>
#include <tdice.h>
#include <tdiinfo.h>
#include <ipexport.h>
#include <tcpinfo.h>
#include <ndislink.h>

#include <bthapi.h>
#include <bt_sdp.h>
#include <sdpnode.h>
#include <sdplib.h>

extern "C" {
#include <cclib.h>
};

//#define DEBUG_CREDIT_FLOW                1
//#define DEBUG_PEER_SYNCHRONIZATION            1    // Both sides MUST be compiled with this

#if defined (DEBUG_PEER_SYNCHRONIZATION)

#define DEBUG_PEER_MAGIC    'gbdP'

struct PeerDebugData {
    unsigned int magic;

#if defined (DEBUG_CREDIT_FLOW)
    int iHaveCredits;
    int iGaveCredits;
    int iPacketsSent;
    int iPacketsRecv;
    int iCreditsSent;
    int iCreditsRecv;
#endif

    PeerDebugData () {
        memset (this, 0, sizeof(*this));
        magic = DEBUG_PEER_MAGIC;
    }
};

#define DEBUG_OVERHEAD    sizeof(PeerDebugData)

#else
#define DEBUG_OVERHEAD    0
#endif

#define CLIENTPORTSIG 0x4D534254        //"MSBT"
#define PIN_SIZE        16
#define KEY_SIZE        16

typedef void (* PBT_CONNECT_COMPLETE)(
        PVOID            Context, 
        TDI_STATUS       FinalStatus, 
        unsigned long    ByteCount);

typedef void (* PBT_DISCONNECT_COMPLETE)(
        PVOID            Context, 
        TDI_STATUS       FinalStatus, 
        unsigned long    ByteCount);

typedef void (* PBT_SEND_COMPLETE)(
        PVOID            Context,
        TDI_STATUS       TdiStatus,
        DWORD            BytesSent);

typedef void (* PBT_RECV_COMPLETE)(
        PVOID            Context,
        TDI_STATUS       TdiStatus,
        DWORD            BytesRecvd);



const WINSOCK_MAPPING BT_MAPPING = {1, 3, AF_BT, SOCK_STREAM, BTHPROTO_RFCOMM};

#define RFCOMM_TRANSPORT_NAME    L"RFCOMM"

#define TDIR_V24_CONST      0x81    // +EA -FC -RTC -RTR 0 0 -IC +DV
#define TDIR_V24_FC         0x02    //     +FC
#define TDIR_V24_RTC        0x04    //         +RTC
#define TDIR_V24_RTR        0x08    //              +RTR
#define TDIR_V24_IC         0x08    //                       +IC

#define TAI_SIZE (offsetof(TDI_ADDRESS_INFO, Address.Address[0].AddressType) + sizeof(SOCKADDR_BTH))
#define TA_SIZE (offsetof(TRANSPORT_ADDRESS, Address[0].AddressType) + sizeof(SOCKADDR_BTH))


enum CONNECT_STAGE {
    ALLOCATED,
    INITED,
    OPENING,
    OPENED,
    CLOSED
};

#define OPT_CONFIG_AUTH            0x00000001
#define OPT_CONFIG_CRYPT        0x00000002
#define OPT_CONFIG_MTU            0x00000004
#define OPT_CONFIG_MTU_MAX        0x00000008
#define OPT_CONFIG_MTU_MIN        0x00000010
#define OPT_CONFIG_XON            0x00000020
#define OPT_CONFIG_XOFF            0x00000040
#define OPT_CONFIG_MAX_SEND        0x00000080
#define OPT_CONFIG_MAX_RECV        0x00000100
#define OPT_CONFIG_PIN            0x00000200

struct OPT {
    unsigned int    fAuthenticate : 1;      // Authentication required
    unsigned int    fEncrypt : 1;           // Encryption required

    int             mtu;                    // negotiated MTU

    int             imax_mtu;               // Min MTU
    int             imin_mtu;               // Max MTU

    int             xon_lim;                // xon_lim
    int             xoff_lim;               // xoff_lim

    int             imax_send;              // max_send
    int             imax_recv;              // max_recv

    int             cPinLength;             // pin length or 0
    unsigned char   caPinData[PIN_SIZE];    // PIN

    unsigned int    uiConfigMask;           // Non-default configuration mask

    OPT (void) {
        fAuthenticate   = 0;
        fEncrypt        = 0;

        mtu             = TDIR_MTU_DESIRED;

        imax_mtu        = TDIR_MTUMAX;
        imin_mtu        = TDIR_MTUMIN;

        xon_lim         = TDIR_XONLIM;
        xoff_lim        = TDIR_XOFFLIM;

        imax_send       = TDIR_SENDMAX;
        imax_recv       = TDIR_RECVMAX;

        cPinLength      = 0;
        memset (caPinData, 0, sizeof(caPinData));

        uiConfigMask    = 0;
    }
};

struct RFCOMM_ADDRESS_OBJECT : public SVSRefObj, public OPT {
    RFCOMM_ADDRESS_OBJECT       *pNext;                             // Next in list
    BD_ADDR                     b;                                  // BD_ADDR
    unsigned char               ch;                                 // server channel

    unsigned int                fListenOn : 1;                      // server?
    unsigned int                fAssigned : 1;                      // server channel dynamically reserved

    PConnectEvent               pEventConnect;                      // PConnectEvent
    PVOID                       pEventConnectContext;               // PVOID
    PDisconnectEvent            pEventDisconnect;                   // PDisconnectEvent
    PVOID                       pEventDisconnectContext;            // PVOID
    PErrorEvent                 pEventError;                        // PErrorEvent
    PVOID                       pEventErrorContext;                 // PVOID
    PRcvEvent                   pEventReceive;                      // PRcvEvent
    PVOID                       pEventReceiveContext;               // PVOID
    PRcvDGEvent                 pEventReceiveDatagram;              // PRcvDGEvent
    PVOID                       pEventReceiveDatagramContext;       // PVOID
    PRcvExpEvent                pEventReceiveExpedited;             // PRcvExpEvent
    PVOID                       pEventReceiveExpeditedContext;      // PVOID

    RFCOMM_ADDRESS_OBJECT (void) : OPT () {
        pNext = NULL;
        memset (&b, 0, sizeof(b));
        ch = 0;

        fListenOn = 0;
        fAssigned = 0;

        pEventConnect = NULL;
        pEventConnectContext = NULL;
        pEventDisconnect = NULL;
        pEventDisconnectContext = NULL;
        pEventError = NULL;
        pEventErrorContext = NULL;
        pEventReceive = NULL;
        pEventReceiveContext = NULL;
        pEventReceiveDatagram = NULL;
        pEventReceiveDatagramContext = NULL;
        pEventReceiveExpedited = NULL;
        pEventReceiveExpeditedContext = NULL;
    }

    void *operator new (size_t size);
    void operator delete (void *ptr);
};

struct RFCOMM_CONNECTION_OBJECT;

struct RFCOMM_PACKET_OBJECT {
    RFCOMM_PACKET_OBJECT        *pNext;             // Next in list
    RFCOMM_CONNECTION_OBJECT    *pConn;             // Connection
    int                         cBytesSent;         // sent bytes
    SVSHandle                   hCallContext;       // Unique handle for async call lookup

    RFCOMM_PACKET_OBJECT (void) {
        memset (this, 0, sizeof(*this));
    }

    ~RFCOMM_PACKET_OBJECT (void) {
        if (hCallContext != SVSUTIL_HANDLE_INVALID) {
            btutil_CloseHandle (hCallContext);
            hCallContext = SVSUTIL_HANDLE_INVALID;
        }
    }

    void *operator new (size_t size);
    void operator delete (void *ptr);
};

struct RFCOMM_SEND_OBJECT {
    RFCOMM_SEND_OBJECT          *pNext;             // Next in list
    RFCOMM_CONNECTION_OBJECT    *pConnect;          // Connection

    NDIS_BUFFER                 *pNdisBuffer;       // NDIS buffer
    int                         cBytesUsed;         // Bytes used in buffer
    int                         cBytesConfirmed;    // Confirmed (signal if == used)

    PBT_SEND_COMPLETE           pSendCallback;      // PBT_SEND_COMPLETE
    void                        *pSendContext;      // PVOID

    RFCOMM_SEND_OBJECT (void) {
        memset (this, 0, sizeof(*this));
    }

    void *operator new (size_t size);
    void operator delete (void *ptr);
};

struct RFCOMM_RECEIVE_OBJECT {
    RFCOMM_RECEIVE_OBJECT   *pNext;                 // Next in list
    BD_BUFFER               *pBuffer;               // BD_BUFFER (if pending data)

    NDIS_BUFFER             *pNdisBuffer;           // NDIS_BUFFER (if waiting)
    DWORD                   dwPerms;                // Perms if waiting

    PBT_RECV_COMPLETE       pRecvEvent;             // PBT_RECV_COMPLETE
    void                    *pRecvContext;          // PVOID
    int                     iNdisUsed;              // Used in NDIS buffer

    void *operator new (size_t size);
    void operator delete (void *ptr);
};

struct RFCOMM_CONNECTION_OBJECT : public OPT {
    RFCOMM_CONNECTION_OBJECT    *pNext;                         // Next in list
    RFCOMM_ADDRESS_OBJECT       *pAddr;                         // Address object
    RFCOMM_SEND_OBJECT          *pSendList;                     // List of sends
    RFCOMM_PACKET_OBJECT        *pPacketList;                   // List of packets
    RFCOMM_RECEIVE_OBJECT       *pRecvList;                     // List of recvs or data

    HANDLE                      hConnect;                       // Connection (NULL = not connected)
    SOCKADDR_BTH                BTAddr;

    CONNECT_STAGE               eStage;                         // CONNECT_STAGE

    int                         isent;                          // sent
    int                         irecvd;                         // received

    unsigned int                v24in;                          // v24 signals
    unsigned int                brin;                           // breaks
    unsigned int                rlsin;                          // line status

    unsigned int                fc_credit      : 1;             // Credit-based flow control
    unsigned int                fc_local       : 1;             // 1 = don't send (MSC)
    unsigned int                fc_remote      : 1;             // 1 = don't send (MSC)
    unsigned int                fc_aggregate   : 1;             // 1 = don't send (FCON/FCOFF)
    unsigned int                fCloseHaveRecv : 1;             // socket has closed down, but there's still more data to recv() on.
    unsigned int                fCloseHaveSend : 1;             // socket has closed down, but there's still more data to send on.
    unsigned int                fNoDisconnect  : 1;             // opening state before we know if error occured

    unsigned int                fParity        : 1;             // just store; no effect on anything

    unsigned int                fSending       : 1;             // In the process of sending

    int                         cbDataIndicated;                // Bytes already indicated to TDI

    int                         BaudRate;                       // just store; no effect on anything
    int                         ByteSize;                       // just store; no effect on anything
    int                         StopBits;                       // just store; no effect on anything
    int                         Parity;                         // just store; no effect on anything

    int                         iHaveCredits;
    int                         iGaveCredits;

#if defined (DEBUG_CREDIT_FLOW)
    int                         iPacketsSent;
    int                         iPacketsRecv;
    int                         iCreditsSent;
    int                         iCreditsRecv;
#endif

    PVOID                       pConnectionContext;             // PVOID

    PBT_CONNECT_COMPLETE        pConnectCompleteEvent;          // PBT_CONNECT_COMPLETE
    void                        *pConnectCompleteContext;       // PVOID

    // SDP connection setup structures
    unsigned short              sdpCid;

    SVSHandle                   hCallContext;                   // Unique handle for async call lookup

    RFCOMM_CONNECTION_OBJECT (void) : OPT() {
        pNext       = NULL;
        pAddr       = NULL;
        pSendList   = NULL;
        pPacketList = NULL;
        pRecvList   = NULL;

        hConnect    = NULL;
        memset (&BTAddr, 0, sizeof(BTAddr));

        eStage       = ALLOCATED;

        hCallContext = SVSUTIL_HANDLE_INVALID;

        isent = irecvd = 0;

        v24in = brin = rlsin = 0;

        fc_credit    = FALSE;
        fc_local     = FALSE;
        fc_remote    = FALSE;
        fc_aggregate = FALSE;

        fCloseHaveRecv = FALSE;
        fCloseHaveSend = FALSE;

        fNoDisconnect  = FALSE;

        cbDataIndicated = 0;

        iHaveCredits = 0;
        iGaveCredits = 0;

        pConnectionContext      = NULL;
        pConnectCompleteEvent   = NULL;
        pConnectCompleteContext = NULL;

        sdpCid = 0;

        fSending = FALSE;

        //    These are not accessible from the inside...
        fParity = FALSE;
        BaudRate = 115200;
        ByteSize = 8;
        StopBits = 0;
        Parity   = 0;

#if defined (DEBUG_CREDIT_FLOW)
        iPacketsSent = 0;
        iPacketsRecv = 0;
        iCreditsSent = 0;
        iCreditsRecv = 0;
#endif
    }

    ~RFCOMM_CONNECTION_OBJECT (void) {
        if (hCallContext != SVSUTIL_HANDLE_INVALID) {
            btutil_CloseHandle (hCallContext);
            hCallContext = SVSUTIL_HANDLE_INVALID;
        }
    }

    void *operator new (size_t size);
    void operator delete (void *ptr);
};

struct RFCOMM_SOCKET_OBJECT : OPT {
    RFCOMM_SOCKET_OBJECT        *pNext;
    RFCOMM_ADDRESS_OBJECT        *pao;

    HANDLE                        hSocketHandle;

    RFCOMM_SOCKET_OBJECT (void) : OPT() {
        pNext         = NULL;
        pao           = NULL;
        hSocketHandle = NULL;
    }

    void *operator new (size_t size);
    void operator delete (void *ptr);
};

struct RFCOMM_PN_OBJECT {
    RFCOMM_PN_OBJECT        *pNext;
    HANDLE                  hConnection;
    int                     imtu;

    int                     fCreditFlow;    // credit-based flow control
    int                     iCredits;       // credits granted

    void *operator new (size_t size);
    void operator delete (void *ptr);
};

struct RFCOMM_TDI : public SVSAllocClass, public SVSRefObj, public SVSSynch {
    FixedMemDescr               *pfmdAO;            // FMD for Address Objects
    FixedMemDescr               *pfmdCO;            // FMD for Connect Objects
    FixedMemDescr               *pfmdSO;            // FMD for Socket Objects
    FixedMemDescr               *pfmdSEND;          // FMD for sends
    FixedMemDescr               *pfmdRECV;          // FMD for recvs
    FixedMemDescr               *pfmdPL;            // FMD for data packets
    FixedMemDescr               *pfmdPN;            // FMD for parameter negotiation packets

    RFCOMM_ADDRESS_OBJECT       *paolist;           // List of addresses
    RFCOMM_CONNECTION_OBJECT    *pcolist;           // List of connections
    RFCOMM_SOCKET_OBJECT        *psolist;           // List of sockets
    RFCOMM_PN_OBJECT            *pnlist;            // List of PN associations

    HANDLE                      hRFCOMM;            // RFCOMM handle
    RFCOMM_INTERFACE            rfcomm_if;          // RFCOMM interface
    unsigned int                fIsConnected : 1;   // 1 = connected
    unsigned int                fIsInitialized : 1; // 1 = initialized

    HANDLE                      hSDP;
    SDP_INTERFACE               sdp_if;

    int                         cDeviceHeader;      // pre-allocate for front
    int                         cDeviceTrailer;     // pre-allocate for rear

    BT_ADDR                     btAddr;             // address of local device
    HANDLE                      hHCI;               // handles to HCI layer to read the address
    HCI_INTERFACE               hci_if;


    void reset (void) {
        paolist = NULL;
        pcolist = NULL;
        psolist = NULL;
        pnlist  = NULL;

        hRFCOMM = NULL;
        memset (&rfcomm_if, 0, sizeof(rfcomm_if));

        hSDP    = NULL;
        memset (&sdp_if, 0, sizeof(sdp_if));

        hHCI    = NULL;
        memset(&hci_if, 0, sizeof(hci_if));

        fIsConnected = fIsInitialized = FALSE;

        cDeviceHeader = cDeviceTrailer = 0;

        memset (&btAddr, 0, sizeof(btAddr));
    }

    RFCOMM_TDI (void) {
        pfmdAO   = NULL;
        pfmdCO   = NULL;
        pfmdSO   = NULL;
        pfmdSEND = NULL;
        pfmdRECV = NULL;
        pfmdPL   = NULL;
        pfmdPN   = NULL;

        reset ();
    }

    ~RFCOMM_TDI(void) {
        if (pfmdAO)
            svsutil_ReleaseFixedEmpty (pfmdAO);

        if (pfmdCO)
            svsutil_ReleaseFixedEmpty (pfmdCO);

        if (pfmdSO)
            svsutil_ReleaseFixedEmpty (pfmdSO);

        if (pfmdSEND)
            svsutil_ReleaseFixedEmpty (pfmdSEND);

        if (pfmdRECV)
            svsutil_ReleaseFixedEmpty (pfmdRECV);

        if (pfmdPL)
            svsutil_ReleaseFixedEmpty (pfmdPL);

        if (pfmdPN)
            svsutil_ReleaseFixedEmpty (pfmdPN);
        }

    int Init (void);
};

void sdp_UnLoadWSANameSpaceProvider(void);
void sdp_LoadWSANameSpaceProvider(void);

//
//    Globals
//
static RFCOMM_TDI    *gpTDIR = NULL;

//
//    Utility functions
//
static int ExtractPN (HANDLE hConnection, int *pmtu, int *pfcreditflow, int *picreditsgranted) {
    RFCOMM_PN_OBJECT *pPNO = gpTDIR->pnlist;
    RFCOMM_PN_OBJECT *pParent = NULL;
    while (pPNO && (pPNO->hConnection != hConnection)) {
        pParent = pPNO;
        pPNO = pPNO->pNext;
    }

    if (! pPNO)
        return FALSE;

    if (pmtu)
        *pmtu             = pPNO->imtu;

    if (pfcreditflow)
        *pfcreditflow     = pPNO->fCreditFlow;

    if (picreditsgranted)
        *picreditsgranted = pPNO->iCredits;

    if (pParent)
        pParent->pNext = pPNO->pNext;
    else
        gpTDIR->pnlist = pPNO->pNext;

    delete pPNO;

    return TRUE;
}

static RFCOMM_SOCKET_OBJECT *GetSocketObject (HANDLE h) {
    RFCOMM_SOCKET_OBJECT *pSock = gpTDIR->psolist;
    while (pSock && (pSock != h))
        pSock = pSock->pNext;

    return pSock;
}

static RFCOMM_SOCKET_OBJECT *GetSocketByHandle (HANDLE h) {
    RFCOMM_SOCKET_OBJECT *pSock = gpTDIR->psolist;
    while (pSock && (pSock->hSocketHandle != h))
        pSock = pSock->pNext;

    return pSock;
}

static RFCOMM_ADDRESS_OBJECT *GetAddressObject (HANDLE h) {
    RFCOMM_ADDRESS_OBJECT *pA = gpTDIR->paolist;
    while (pA && (pA != h))
        pA = pA->pNext;

    return pA;
}

static RFCOMM_CONNECTION_OBJECT *GetConnectionObject (HANDLE h) {
    RFCOMM_CONNECTION_OBJECT *pC = gpTDIR->pcolist;
    while (pC && (pC != h))
        pC = pC->pNext;

    return pC;
}

static RFCOMM_SEND_OBJECT *GetSendObject (HANDLE h) {
    HANDLE hConnectObj = NULL;

    __try {
        hConnectObj = (HANDLE)((RFCOMM_SEND_OBJECT *)h)->pConnect;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"RFCOMM_SEND_OBJECT faulted: returning FALSE\n"));
        return NULL;
    }

    RFCOMM_CONNECTION_OBJECT *pC = GetConnectionObject (hConnectObj);

    if (! pC)
        return NULL;

    RFCOMM_SEND_OBJECT *pSendO = pC->pSendList;

    while (pSendO && (pSendO != h))
        pSendO = pSendO->pNext;

    return pSendO;
}

static RFCOMM_PACKET_OBJECT *GetPacketObject (HANDLE h) {
    if (! h) {
        IFDBG(DebugOut (DEBUG_WARN, L"GetPacketObject failed: NULL context\n"));
        return NULL;
    }

    HANDLE hConnectObj = NULL;

    __try {
        hConnectObj = (HANDLE)((RFCOMM_PACKET_OBJECT *)h)->pConn;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_WARN, L"GetPacketObject faulted: returning FALSE\n"));
        return NULL;
    }

    RFCOMM_CONNECTION_OBJECT *pC = GetConnectionObject (hConnectObj);

    if (! pC)
        return NULL;

    RFCOMM_PACKET_OBJECT *pP = pC->pPacketList;

    while (pP && (pP != h))
        pP = pP->pNext;

    return pP;
}

static RFCOMM_CONNECTION_OBJECT *GetConnectionByHandle (HANDLE h) {
    RFCOMM_CONNECTION_OBJECT *pC = gpTDIR->pcolist;
    while (pC && (pC->hConnect != h))
        pC = pC->pNext;

    return pC;
}

static RFCOMM_CONNECTION_OBJECT *GetConnectionByContext (void *pCtx) {
    RFCOMM_CONNECTION_OBJECT *pC = gpTDIR->pcolist;
    while (pC && (pC->pConnectionContext != pCtx))
        pC = pC->pNext;

    return pC;
}

static void GetConnectionState (void) {
    gpTDIR->fIsConnected = FALSE;

    __try {
        int fConnected = FALSE;
        int dwRet = 0;
        gpTDIR->hci_if.hci_ioctl (gpTDIR->hHCI, BTH_STACK_IOCTL_GET_CONNECTED, 0, NULL, sizeof(fConnected), (char *)&fConnected, &dwRet);
        if ((dwRet == sizeof(fConnected)) && fConnected)
            gpTDIR->fIsConnected = TRUE;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] GetConnectionState : exception in hci_ioctl BTH_STACK_IOCTL_GET_CONNECTED\n"));
    }
}

static int CloseAddressObject (RFCOMM_ADDRESS_OBJECT *pao) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"CloseAddressObject 0x%08x\n", pao));

    RFCOMM_ADDRESS_OBJECT *pA = gpTDIR->paolist;
    RFCOMM_ADDRESS_OBJECT *pP = NULL;
    while (pA && (pA != pao)) {
        pP = pA;
        pA = pA->pNext;
    }

    if (! pA)
        return ERROR_NOT_FOUND;

    if (pP)
        pP->pNext = pA->pNext;
    else
        gpTDIR->paolist = pA->pNext;

    unsigned char cPort = pA->ch;
    int fAssigned = pA->fAssigned;

    delete pA;

    // unbind the socket...
    RFCOMM_SOCKET_OBJECT *pso = gpTDIR->psolist;
    while (pso) {
        if (pso->pao == pao)
            pso->pao = NULL;
        pso = pso->pNext;
    }

    // disassociate connections
    RFCOMM_CONNECTION_OBJECT *pco = gpTDIR->pcolist;
    while (pco) {
        if (pco->pAddr == pA) {
            SVSUTIL_ASSERT (0);
            pco->pAddr = NULL;
        }

        pco = pco->pNext;
    }

    if (gpTDIR->fIsInitialized && fAssigned) {
        HANDLE h = gpTDIR->hRFCOMM;
        BT_LAYER_IO_CONTROL pCallback = gpTDIR->rfcomm_if.rfcomm_ioctl;
        gpTDIR->AddRef ();
        gpTDIR->Unlock ();

        __try {
            pCallback (h, BTH_STACK_IOCTL_FREE_PORT, sizeof(cPort), (char *)&cPort, 0, NULL, NULL);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] CloseAddressObject Exception in BTH_STACK_IOCTL_FREE_PORT\n"));
        }

        gpTDIR->Lock ();
        gpTDIR->DelRef ();
    }

    return ERROR_SUCCESS;
}

static int DisassociateConnection (RFCOMM_CONNECTION_OBJECT *pC) {
    if (pC->pAddr) {
        pC->pAddr->DelRef ();

        RFCOMM_ADDRESS_OBJECT *pAddr = pC->pAddr;
        pC->pAddr = NULL;

        if (pAddr->GetRefCount () == 1)
            CloseAddressObject (pAddr);
    }

    return ERROR_SUCCESS;
}

static void TransferOptions (RFCOMM_CONNECTION_OBJECT *pConn, OPT *pOpts) {
    if (pOpts->uiConfigMask & OPT_CONFIG_MTU)
        pConn->imax_mtu      = pOpts->imax_mtu;

    if (pOpts->uiConfigMask & OPT_CONFIG_MTU_MIN)
        pConn->imin_mtu      = pOpts->imin_mtu;

    if (pOpts->uiConfigMask & OPT_CONFIG_MAX_SEND)
        pConn->imax_send     = pOpts->imax_send;

    if (pOpts->uiConfigMask & OPT_CONFIG_MAX_RECV)
        pConn->imax_recv     = pOpts->imax_recv;

    if (pOpts->uiConfigMask & OPT_CONFIG_XOFF)
        pConn->xoff_lim      = pOpts->xoff_lim;

    if (pOpts->uiConfigMask & OPT_CONFIG_XON)
        pConn->xon_lim       = pOpts->xon_lim;

    if (pOpts->uiConfigMask & OPT_CONFIG_AUTH)
        pConn->fAuthenticate = pOpts->fAuthenticate;

    if (pOpts->uiConfigMask & OPT_CONFIG_CRYPT)
        pConn->fEncrypt      = pOpts->fEncrypt;

    if (pOpts->uiConfigMask & OPT_CONFIG_MTU)
        pConn->mtu           = pOpts->mtu;

    if (pOpts->uiConfigMask & OPT_CONFIG_PIN) {
        pConn->cPinLength    = pOpts->cPinLength;
        memcpy (pConn->caPinData, pOpts->caPinData, sizeof(pConn->caPinData));
    }

    SVSUTIL_ASSERT ((pConn->cPinLength >= 0) && (pConn->cPinLength <= PIN_SIZE));
}

static void ResetConnection (RFCOMM_CONNECTION_OBJECT *pConn, int fClearCredits) {
    pConn->fc_local = pConn->fc_remote = pConn->fc_aggregate = FALSE;
    pConn->brin = pConn->v24in = pConn->rlsin = 0;
    pConn->irecvd          = pConn->isent = 0;

    pConn->fCloseHaveRecv = FALSE;
    pConn->fCloseHaveSend = FALSE;

    pConn->cbDataIndicated = FALSE;

    pConn->fSending = FALSE;

    pConn->fNoDisconnect = FALSE;

    if (fClearCredits) {
        pConn->fc_credit       = FALSE;
        pConn->iHaveCredits    = 0;
        pConn->iGaveCredits    = 0;

#if defined (DEBUG_CREDIT_FLOW)
        pConn->iPacketsSent = 0;
        pConn->iPacketsRecv = 0;
        pConn->iCreditsSent = 0;
        pConn->iCreditsRecv = 0;
#endif
    }
}

static int DisconnectConnection (RFCOMM_CONNECTION_OBJECT *pConn, HANDLE hConn, RFCOMM_CONNECTION_OBJECT *pCtx) {
    IFDBG(DebugOut (DEBUG_TDI_PACKETS, L"DisconnectConnection (0x%08x, 0x%08x, 0x%08x)\n", pConn, hConn, pCtx));

    if (pConn) {
        hConn = pConn->hConnect;
        pConn->hConnect = NULL;
    }

    int iRes = ERROR_SUCCESS;

    if (gpTDIR->fIsConnected && hConn) {
        SVSHandle hContext = NULL;
        
        if (pCtx) {
            SVSUTIL_ASSERT (pCtx->hCallContext != SVSUTIL_HANDLE_INVALID);
            hContext = pCtx->hCallContext;
        }
        
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"DisconnectConnection : physically closing 0x%08x\n", hConn));

        HANDLE h = gpTDIR->hRFCOMM;
        RFCOMM_Disconnect_In pCallback = gpTDIR->rfcomm_if.rfcomm_Disconnect_In;
        gpTDIR->AddRef ();
        gpTDIR->Unlock ();
        __try {
            iRes = pCallback (h, (LPVOID)hContext, hConn);
        } __except (1) {
            iRes = ERROR_INTERNAL_ERROR;
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] DisconnectConnection : exception in rfcomm_Disconnect_In\n"));
        }
        gpTDIR->Lock ();
        gpTDIR->DelRef ();
    }

    return iRes;
}

static int CloseConnectionObject (RFCOMM_CONNECTION_OBJECT *pco) {
    IFDBG(DebugOut (DEBUG_TDI_PACKETS, L"CloseConnectionObject 0x%08x\n", pco));

    RFCOMM_CONNECTION_OBJECT *pC = gpTDIR->pcolist;
    RFCOMM_CONNECTION_OBJECT *pP = NULL;

    while (pC && (pC != pco)) {
        pP = pC;
        pC = pC->pNext;
    }

    if (! pC)
        return ERROR_NOT_FOUND;

    if (pP)
        pP->pNext = pC->pNext;
    else
        gpTDIR->pcolist = pC->pNext;

    HANDLE hConn = pC->hConnect;

    SVSUTIL_ASSERT (pC->pPacketList == NULL);
    SVSUTIL_ASSERT (pC->pSendList   == NULL);
    SVSUTIL_ASSERT (pC->pRecvList   == NULL);
    SVSUTIL_ASSERT (pC->pAddr       == NULL);

    bt_addr bt = pC->BTAddr.btAddr;
    int cPinLength = pC->cPinLength;

    DisassociateConnection (pC);

    delete pC;

    DisconnectConnection (NULL, hConn, NULL);

    if (cPinLength != 0) {
        gpTDIR->AddRef ();
        gpTDIR->Unlock ();

        __try {
            BthRevokePIN (&bt);
        } __except (1) {
        }

        gpTDIR->Lock ();
        gpTDIR->DelRef ();
    }

    return ERROR_SUCCESS;
}

static int CloseSocketObject (RFCOMM_SOCKET_OBJECT *pso) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"CloseSocketObject 0x%08x\n", pso));

    RFCOMM_SOCKET_OBJECT *pS = gpTDIR->psolist;
    RFCOMM_SOCKET_OBJECT *pP = NULL;
    while (pS && (pS != pso)) {
        pP = pS;
        pS = pS->pNext;
    }

    if (! pS)
        return ERROR_NOT_FOUND;

    if (pP)
        pP->pNext = pS->pNext;
    else
        gpTDIR->psolist = pS->pNext;

    delete pS;

    return ERROR_SUCCESS;
}

static void SignalConnectionCompleteUp (RFCOMM_CONNECTION_OBJECT *pC, int iError) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"SignalConnectionCompleteUp 0x%08x : %d\n", pC, iError));

    if (iError != TDI_SUCCESS)
        pC->eStage = CLOSED;
    else
        pC->eStage = OPENED;

    PBT_CONNECT_COMPLETE        pConnectCompleteEvent    = pC->pConnectCompleteEvent;
    void                        *pConnectCompleteContext = pC->pConnectCompleteContext;

    pC->pConnectCompleteContext = NULL;
    pC->pConnectCompleteEvent   = NULL;

    SVSUTIL_ASSERT (pConnectCompleteEvent);
    gpTDIR->AddRef ();
    gpTDIR->Unlock ();

    pConnectCompleteEvent (pConnectCompleteContext, iError, 0);

    gpTDIR->Lock ();
    gpTDIR->DelRef ();
}

static int SignalConnectionLoss (RFCOMM_CONNECTION_OBJECT *pC, TDI_STATUS fHow, BOOL fImmediateCleanup) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"SignalConnectionLoss : closing connection 0x%08x to %04x%08x 0x%02x\n", pC, GET_NAP(pC->BTAddr.btAddr), GET_SAP(pC->BTAddr.btAddr), pC->BTAddr.port));
    CONNECT_STAGE eStage = pC->eStage;

    pC->eStage = CLOSED;

    while (pC->pPacketList) {
        RFCOMM_PACKET_OBJECT    *pNext = pC->pPacketList->pNext;
        delete pC->pPacketList;
        pC->pPacketList = pNext;
    }

    int iRes = TDI_SUCCESS;

    if (pC->pConnectCompleteEvent && (! pC->fNoDisconnect)) {
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"SignalConnectionLoss : signalling connection abort to complete event\n"));
        SignalConnectionCompleteUp (pC, TDI_CONNECTION_ABORTED);
        pC = GetConnectionObject ((HANDLE)pC);
    }

    if ((! fImmediateCleanup) && (eStage == OPENED) && pC && pC->pRecvList && pC->pRecvList->pBuffer) {
        // there's still data to recv(), and we want to leave it around.
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"SignalConnectionLoss : waiting to flush the recv list...\n"));
        pC->fCloseHaveRecv = TRUE;
    } else if (pC && ((eStage == OPENED) || fImmediateCleanup || (pC->fCloseHaveRecv && (! (pC->pRecvList && pC->pRecvList->pBuffer))))) {
        pC->fCloseHaveRecv = FALSE;

        while (pC && pC->pRecvList) {
            RFCOMM_RECEIVE_OBJECT *pThis = pC->pRecvList;
            pC->pRecvList = pC->pRecvList->pNext;

            if (pThis->pBuffer && pThis->pBuffer->pFree)
                pThis->pBuffer->pFree (pThis->pBuffer);

            NDIS_BUFFER       *pNdisBuffer = pThis->pNdisBuffer;
            PBT_RECV_COMPLETE  pCallback   = pThis->pRecvEvent;
            void              *pContext    = pThis->pRecvContext;
            int               iReceived    = pThis->iNdisUsed;

            delete pThis;

            IFDBG(DebugOut (DEBUG_TDI_TRACE, L"SignalConnectionLoss : cleaning up ndis buffer 0x%08x context 0x%08x, received %d\n", pNdisBuffer, pContext, iReceived));

            if (pNdisBuffer) {
                gpTDIR->AddRef ();
                gpTDIR->Unlock ();

                pCallback (pContext, fHow, iReceived);

                gpTDIR->Lock ();
                gpTDIR->DelRef ();
            }

            pC = GetConnectionObject ((HANDLE)pC);
        }

        while (pC && pC->pSendList) {
            RFCOMM_SEND_OBJECT *pNext = pC->pSendList->pNext;
            PBT_SEND_COMPLETE pCallback = pC->pSendList->pSendCallback;
            void *pCallbackContext = pC->pSendList->pSendContext;
            int cBytesSent = pC->pSendList->cBytesUsed;
            delete pC->pSendList;
            pC->pSendList = pNext;

            IFDBG(DebugOut (DEBUG_TDI_TRACE, L"SignalConnectionLoss : cleaning up send list context 0x%08x, sent %d\n", pCallbackContext, cBytesSent));

            gpTDIR->AddRef ();
            gpTDIR->Unlock ();

            pCallback (pCallbackContext, TDI_CONNECTION_ABORTED, cBytesSent);

            gpTDIR->Lock ();
            gpTDIR->DelRef ();

            pC = GetConnectionObject ((HANDLE)pC);
        }

        if (pC && pC->pAddr && pC->pAddr->pEventDisconnect) {
            PDisconnectEvent pEventDisconnect = pC->pAddr->pEventDisconnect;
            void *pEventContext = pC->pAddr->pEventDisconnectContext;
            void *pConnectionContext = pC->pConnectionContext;

            IFDBG(DebugOut (DEBUG_TDI_PACKETS, L"SignalConnectionLoss : forwarding up disconnection event conn 0x%08x\n", pC));

            gpTDIR->AddRef ();
            gpTDIR->Unlock ();
            iRes = pEventDisconnect (pEventContext, pConnectionContext, 0, NULL, 0, NULL,
                                        TDI_DISCONNECT_WAIT);
            gpTDIR->Lock ();
            gpTDIR->DelRef ();
        } else {
            IFDBG(DebugOut (DEBUG_WARN, L"[TDI] SignalConnectionLoss : nothing reported for conn 0x%08x\n", pC));
        }
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] SignalConnectionLoss : already closed\n"));
        SVSUTIL_ASSERT ((! pC) || ((! pC->pSendList) && (! pC->pRecvList)));
    }

    return iRes;
}

int SendMSC (RFCOMM_CONNECTION_OBJECT *pConn, unsigned char v24out, unsigned char brout) {
    HANDLE h = gpTDIR->hRFCOMM;
    RFCOMM_MSC_In pCallback = gpTDIR->rfcomm_if.rfcomm_MSC_In;
    HANDLE hConnection = pConn->hConnect;

    unsigned char v24 = TDIR_V24_CONST | (pConn->fc_local ? TDIR_V24_FC : 0) | v24out;

    gpTDIR->AddRef ();
    gpTDIR->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;
    __try {
        iRes = pCallback (h, NULL, hConnection, v24, brout);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] SendMSC Exception in rfcomm_MSC_In\n"));
    }

    gpTDIR->Lock ();
    gpTDIR->DelRef ();

    return iRes;
}

int SendRLS (RFCOMM_CONNECTION_OBJECT *pConn, unsigned char rls) {
    HANDLE h = gpTDIR->hRFCOMM;
    RFCOMM_RLS_In pCallback = gpTDIR->rfcomm_if.rfcomm_RLS_In;
    HANDLE hConnection = pConn->hConnect;

    gpTDIR->AddRef ();
    gpTDIR->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;
    __try {
        iRes = pCallback (h, NULL, hConnection, rls);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] SendMSC Exception in rfcomm_MSC_In\n"));
    }

    gpTDIR->Lock ();
    gpTDIR->DelRef ();

    return iRes;
}

static DWORD WINAPI StackDisconnect (LPVOID lpUnused) {
    tdiR_CloseDriverInstance ();
    return 0;
}

static DWORD WINAPI StackXXX (LPVOID lpWhat) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+StackXXX\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] StackXXX : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    for ( ; ; ) {
        gpTDIR->Lock ();

        if (! gpTDIR->fIsInitialized) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] StackXXX : ERROR_SERVICE_NOT_ACTIVE\n"));
            gpTDIR->Unlock ();
            return ERROR_SERVICE_NOT_ACTIVE;
        }

        if (gpTDIR->GetRefCount() == 1)
            break;

        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Waiting for ref count in StackXXX\n"));
        gpTDIR->Unlock ();
        Sleep (100);
    }

    if ((int)lpWhat == 1) {
        gpTDIR->fIsConnected = TRUE;
    } else if (((int)lpWhat == 2) || ((int)lpWhat == 3)) {

        while (gpTDIR->pnlist) {
            RFCOMM_PN_OBJECT *pNext = gpTDIR->pnlist->pNext;
            delete gpTDIR->pnlist;
            gpTDIR->pnlist = pNext;
        }

        gpTDIR->fIsConnected = FALSE;
        gpTDIR->AddRef ();

        for ( ; ; ) {
            RFCOMM_CONNECTION_OBJECT *pC = gpTDIR->pcolist;
            while (pC && (pC->eStage != OPENED) && (pC->eStage != OPENING))
                pC = pC->pNext;

            if (! pC)
                break;

            SignalConnectionLoss (pC, TDI_CONNECTION_ABORTED, TRUE);
        }

        gpTDIR->DelRef ();
        if ((int)lpWhat == 3)
            gpTDIR->fIsConnected = TRUE;
    }

    int iErr = ERROR_SUCCESS;
    if (gpTDIR->fIsConnected) {
        HANDLE h = gpTDIR->hHCI;
        HCI_ReadBDADDR_In pCallback = gpTDIR->hci_if.hci_ReadBDADDR_In;
        gpTDIR->AddRef ();
        gpTDIR->Unlock ();

        __try {
            iErr = pCallback (h, NULL);
        } __except (1) {
            iErr = ERROR_INTERNAL_ERROR;
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] : exception in StackXXX!\n"));
        }

        gpTDIR->Lock ();
        gpTDIR->DelRef ();
    }
    else
        gpTDIR->btAddr = SET_NAP_SAP(0, 0);

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-StackXXX ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return iErr;
}

static int QueryNdisSize (NDIS_BUFFER *pNdisB) {
    int cSize = 0;
    while (pNdisB) {
        PVOID pVA = NULL;
        unsigned int uiLength = 0;

        NdisQueryBuffer (pNdisB, &pVA, &uiLength);

        if (! pVA)
            break;

        cSize += uiLength;
        NdisGetNextBuffer(pNdisB, &pNdisB);
    }

    return cSize;
}

static int CopyNdisToFlat
(
NDIS_BUFFER        *pNdisB,
unsigned char    *pFlatBuf,
int                cCount,
int                cStartOffset
) {
    int cOffset = 0;
    int cCopied = 0;
    while (pNdisB) {
        unsigned char *pVA = NULL;
        unsigned int uiLength = 0;

        NdisQueryBuffer (pNdisB, (PVOID *)&pVA, &uiLength);

        if (! pVA)
            break;

        if (cOffset + (int)uiLength > cStartOffset) {
            SVSUTIL_ASSERT (cOffset <= cStartOffset);
            int cBytesAvailable = cOffset + uiLength - cStartOffset;
            int cOffsetInBuffer = cStartOffset - cOffset;
            int cBytesToCopy = cBytesAvailable > cCount ? cCount : cBytesAvailable;
            memcpy (pFlatBuf, pVA + cOffsetInBuffer, cBytesToCopy);

            cCount -= cBytesToCopy;
            cStartOffset += cBytesToCopy;
            cCopied += cBytesToCopy;
            pFlatBuf += cBytesToCopy;

            SVSUTIL_ASSERT (cCount >= 0);

            if (cCount == 0)
                break;
        }

        cOffset += uiLength;
        NdisGetNextBuffer(pNdisB, &pNdisB);
    }

    return cCopied;
}

static int CountPendingSends (RFCOMM_CONNECTION_OBJECT *pConn) {
    RFCOMM_SEND_OBJECT *pSendList = pConn->pSendList;
    int cTotalSize = 0;

    while (pSendList) {
        int cBytes = QueryNdisSize (pSendList->pNdisBuffer) - pSendList->cBytesUsed;
        SVSUTIL_ASSERT (cBytes >= 0);
        cTotalSize += cBytes;
        pSendList = pSendList->pNext;
    }

    return cTotalSize;
}

static int TransferPendingSends (RFCOMM_CONNECTION_OBJECT *pConn, unsigned char *pBuff, int cBuff) {
    RFCOMM_SEND_OBJECT *pSendList = pConn->pSendList;
    int cTotalSent = 0;

    while (pSendList && (cBuff > 0)) {
        int cBytes = QueryNdisSize (pSendList->pNdisBuffer) - pSendList->cBytesUsed;
        SVSUTIL_ASSERT (cBytes >= 0);
        if (cBytes > cBuff)
            cBytes = cBuff;

        if (cBytes > 0) {
            int cTransf = CopyNdisToFlat (pSendList->pNdisBuffer, pBuff, cBytes, pSendList->cBytesUsed);
            SVSUTIL_ASSERT (cTransf == cBytes);

            cBuff -= cBytes;
            pBuff += cBytes;
            pSendList->cBytesUsed += cBytes;
        }

        cTotalSent += cBytes;
        pSendList = pSendList->pNext;
    }

    SVSUTIL_ASSERT (cBuff == 0);
    return cTotalSent;
}

static int GetQuota (RFCOMM_CONNECTION_OBJECT *pC) {
    if (pC->fc_credit && (pC->imax_recv < (pC->mtu * 2 * PORTEMU_CREDITS_LOWEST)))
        return pC->mtu * 2 * PORTEMU_CREDITS_LOWEST;

    return pC->imax_recv;
}

static int GiveCredits (RFCOMM_CONNECTION_OBJECT *pC) {
    if ((! pC->fc_credit) || (pC->iGaveCredits > PORTEMU_CREDITS_LOW))
        return 0;

    int nCredits = (GetQuota (pC) - pC->irecvd) / pC->mtu - pC->iGaveCredits;

    return nCredits > 0 ? nCredits : 0;
}

static void SendCredits (RFCOMM_CONNECTION_OBJECT *pC) {
    SVSUTIL_ASSERT (pC->fc_credit);

    if (pC->iGaveCredits > PORTEMU_CREDITS_LOWEST)
        return;

    int nCredits = (GetQuota (pC) - pC->irecvd) / pC->mtu - pC->iGaveCredits;

    if (nCredits <= 0)
        return;

    IFDBG(DebugOut (DEBUG_TDI_PACKETS, L"[TDIR] SendCredits 0x%08x : sending %d credits\n", pC, nCredits));

    BD_BUFFER *pBuffer = BufferAlloc (gpTDIR->cDeviceHeader + gpTDIR->cDeviceTrailer + DEBUG_OVERHEAD);

    if (! pBuffer) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] SendCredits 0x%08x :  <out of memory>\n", pC));
        return;
    }

    pBuffer->cStart = gpTDIR->cDeviceHeader;
    pBuffer->cEnd   = pBuffer->cSize - gpTDIR->cDeviceTrailer;

    SVSUTIL_ASSERT (BufferTotal (pBuffer) == DEBUG_OVERHEAD);

    pC->iGaveCredits += nCredits;

#if defined (DEBUG_CREDIT_FLOW)
    pC->iCreditsSent += nCredits;
#endif

#if defined (DEBUG_PEER_SYNCHRONIZATION)
    PeerDebugData pd;

#if defined (DEBUG_CREDIT_FLOW)
    pd.iGaveCredits = pC->iGaveCredits;
    pd.iHaveCredits = pC->iHaveCredits;
    pd.iPacketsRecv = pC->iPacketsRecv;
    pd.iPacketsSent = pC->iPacketsSent;
    pd.iCreditsRecv = pC->iCreditsRecv;
    pd.iCreditsSent = pC->iCreditsSent;
#endif

    SVSUTIL_ASSERT (sizeof(pd) == DEBUG_OVERHEAD);
    memcpy (pBuffer->pBuffer + pBuffer->cEnd - sizeof(pd), &pd, sizeof(pd));
#endif

    HANDLE h = gpTDIR->hRFCOMM;
    HANDLE hConn = pC->hConnect;
    RFCOMM_DataDown_In pCallback = gpTDIR->rfcomm_if.rfcomm_DataDown_In;

    gpTDIR->Unlock ();
    __try {
        pCallback (h, NULL, hConn, pBuffer, nCredits);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] SendCredits : exception in rfcomm_DataDown_In\n"));
    }

    gpTDIR->Lock ();
}

static TDI_STATUS SendData (RFCOMM_CONNECTION_OBJECT *pConn) {
    if (pConn != GetConnectionObject ((HANDLE)pConn)) {
        IFDBG(DebugOut (DEBUG_WARN, L"[TDIR] SendData : Connection 0x%08x not found\n", pConn));
        return TDI_INVALID_CONNECTION;
    }

    if (pConn->fSending)        // Guard against packets coming out of order
        return TDI_PENDING;

    for ( ; ; ) {
        if (! gpTDIR->fIsConnected) {
            IFDBG(DebugOut (DEBUG_WARN, L"[TDIR] SendData : disconnected\n"));
            return TDI_INVALID_STATE;
        }

        if (pConn->eStage != OPENED) {
            IFDBG(DebugOut (DEBUG_WARN, L"[TDIR] SendData : Connection 0x%08x not OPEN\n", pConn));
            return TDI_CONNECTION_RESET;
        }

        if ((pConn->isent >= pConn->imax_send) ||
            (pConn->fc_credit && (pConn->iHaveCredits <= 0)) ||
            ((! pConn->fc_credit) && (pConn->fc_remote || pConn->fc_aggregate)))
            break;

        int cSend = CountPendingSends (pConn);
        SVSUTIL_ASSERT (cSend >= 0);

        if (cSend == 0) {
            if (pConn->fCloseHaveSend) {
                SignalConnectionLoss (pConn, TDI_GRACEFUL_DISC, TRUE);
                if (gpTDIR->fIsInitialized && (pConn == GetConnectionObject ((HANDLE)pConn)))
                    DisconnectConnection (pConn, NULL, NULL);
            }

            break;
        }

        if (cSend > pConn->mtu)
            cSend = pConn->mtu;

        if (cSend > pConn->imax_send - pConn->isent)
            cSend = pConn->imax_send - pConn->isent;

        RFCOMM_PACKET_OBJECT *pPacket = new RFCOMM_PACKET_OBJECT;
        if (! pPacket) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] SendData <out of memory>\n"));
            break;
        }

        pPacket->hCallContext = btutil_AllocHandle ((LPVOID)pPacket);
        if (SVSUTIL_HANDLE_INVALID == pPacket->hCallContext) {
            delete pPacket;
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] SendData <out of memory>\n"));
            break;
        }

        BD_BUFFER *pBuffer = BufferAlloc (cSend + gpTDIR->cDeviceHeader + gpTDIR->cDeviceTrailer + DEBUG_OVERHEAD);

        if (! pBuffer) {
            delete pPacket;
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] SendData <out of memory>\n"));
            break;
        }

        pPacket->pConn = pConn;
        pPacket->cBytesSent = cSend;
        pPacket->pNext = pConn->pPacketList;
        pConn->pPacketList = pPacket;

        pBuffer->cEnd = pBuffer->cSize - gpTDIR->cDeviceTrailer;
        pBuffer->cStart = gpTDIR->cDeviceHeader;

        SVSUTIL_ASSERT (BufferTotal (pBuffer) == (int)(cSend + DEBUG_OVERHEAD));

        int cCopiedData = TransferPendingSends (pConn, pBuffer->pBuffer + pBuffer->cStart, cSend);
        SVSUTIL_ASSERT (cCopiedData == cSend);

        pConn->isent += cSend;

        int iCredits = GiveCredits (pConn);

        if (pConn->fc_credit) {
            --pConn->iHaveCredits;
            pConn->iGaveCredits += iCredits;

#if defined (DEBUG_CREDIT_FLOW)
            pConn->iCreditsSent += iCredits;
            pConn->iPacketsSent += 1;

            if (pConn->iGaveCredits != (pConn->iCreditsSent - pConn->iPacketsRecv)) {
                SVSUTIL_ASSERT (0);
                RETAILMSG(1, (L"Bluetooth TDI Credit mismatch: GaveCredits = %d Total SentCredits = %d Total PacketsRecv = %d\n", pConn->iGaveCredits, pConn->iCreditsSent, pConn->iPacketsRecv));
            }

            if (pConn->iHaveCredits != (pConn->iCreditsRecv - pConn->iPacketsSent)) {
                RETAILMSG(1, (L"Bluetooth TDI Credit mismatch: HaveCredits = %d Total CreditsRecv = %d Total PacketsSent = %d\n", pConn->iHaveCredits, pConn->iCreditsRecv, pConn->iPacketsSent));
                SVSUTIL_ASSERT (0);
            }
#endif
        }

#if defined (DEBUG_PEER_SYNCHRONIZATION)
        PeerDebugData pd;

#if defined (DEBUG_CREDIT_FLOW)
        pd.iGaveCredits = pConn->iGaveCredits;
        pd.iHaveCredits = pConn->iHaveCredits;
        pd.iPacketsRecv = pConn->iPacketsRecv;
        pd.iPacketsSent = pConn->iPacketsSent;
        pd.iCreditsRecv = pConn->iCreditsRecv;
        pd.iCreditsSent = pConn->iCreditsSent;
#endif

        SVSUTIL_ASSERT (sizeof(pd) == DEBUG_OVERHEAD);
        memcpy (pBuffer->pBuffer + pBuffer->cEnd - sizeof(pd), &pd, sizeof(pd));
#endif

        SVSUTIL_ASSERT (pPacket->hCallContext != SVSUTIL_HANDLE_INVALID);

        HANDLE h = gpTDIR->hRFCOMM;
        SVSHandle hContext = pPacket->hCallContext;
        HANDLE hConnect = pConn->hConnect;
        RFCOMM_DataDown_In pCallback = gpTDIR->rfcomm_if.rfcomm_DataDown_In;

        IFDBG(DebugOut (DEBUG_TDI_PACKETS, L"SendData 0x%08x : sending %d bytes and %d credits\n", pConn, BufferTotal(pBuffer), iCredits));

        pConn->fSending = TRUE;

        gpTDIR->AddRef ();
        gpTDIR->Unlock ();
        __try {
            int iRes = pCallback (h, (LPVOID)hContext, hConnect, pBuffer, iCredits);
#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
            if (iRes != ERROR_SUCCESS)
                IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] SendData return %d\n", iRes));
#endif
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] SendData Exception in rfcomm_DataDown_In!\n"));
        }
        gpTDIR->Lock ();
        gpTDIR->DelRef ();

        if (pConn != GetConnectionObject ((HANDLE)pConn))
            return TDI_INVALID_CONNECTION;

        pConn->fSending = FALSE;
    }

    return TDI_PENDING;
}

static int RegisterTransferCompletion (RFCOMM_CONNECTION_OBJECT *pConn, int cBytes) {
    RFCOMM_SEND_OBJECT *pS = pConn->pSendList;
    pConn->isent -= cBytes;
    SVSUTIL_ASSERT (pConn->isent >= 0);

    while (pS && (cBytes > 0)) {
        int cUnconfirmed = pS->cBytesUsed - pS->cBytesConfirmed;
        int cConfirm = cUnconfirmed;
        if (cBytes < cConfirm)
            cConfirm = cBytes;
        pS->cBytesConfirmed += cConfirm;
        cBytes -= cConfirm;
        pS = pS->pNext;
    }

    SVSUTIL_ASSERT (cBytes == 0);

    for ( ; ; ) {
        if (! (gpTDIR->fIsConnected && (pConn == GetConnectionObject ((HANDLE)pConn))))
            return ERROR_OPERATION_ABORTED;

        if ((! pConn->pSendList) || (pConn->pSendList->cBytesConfirmed != QueryNdisSize (pConn->pSendList->pNdisBuffer)))
            break;
        
        RFCOMM_SEND_OBJECT *pNext = pConn->pSendList->pNext;
        PBT_SEND_COMPLETE pCallback = pConn->pSendList->pSendCallback;
        void *pCallbackContext = pConn->pSendList->pSendContext;
        int cBytesSent = pConn->pSendList->cBytesUsed;
        delete pConn->pSendList;
        pConn->pSendList = pNext;

        gpTDIR->AddRef ();
        gpTDIR->Unlock ();

        pCallback (pCallbackContext, TDI_SUCCESS, cBytesSent);

        gpTDIR->Lock ();
        gpTDIR->DelRef ();
    }

    SendData (pConn);

    return ERROR_SUCCESS;
}

static int AcceptIncomingCall (RFCOMM_ADDRESS_OBJECT *pAddr, HANDLE hConnection) {
    BD_ADDR            b;
    unsigned char    c;
    int                fLocal;
    int             fAcceptConnection = FALSE;

    int             imtu = TDIR_MTU;
    int                fcreditflow = FALSE;
    int                icredits = 0;
    
    ExtractPN (hConnection, &imtu, &fcreditflow, &icredits);

    if (ERROR_SUCCESS != gpTDIR->rfcomm_if.rfcomm_GetChannelAddress (gpTDIR->hRFCOMM, hConnection, &b, &c, &fLocal)) {
        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] AcceptIncomingCall : Cannot resolve address from 0x%08x\n", hConnection));
        return FALSE;
    } else
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Connection from %04x%08x 0x%02x accepted\n", b.NAP, b.SAP, c));

    BYTE                AddrBuf[sizeof(TRANSPORT_ADDRESS) + sizeof(SOCKADDR_BTH)];
    PTRANSPORT_ADDRESS    pTA = (PTRANSPORT_ADDRESS)AddrBuf;

    pTA->TAAddressCount           = 1;
    pTA->Address[0].AddressLength = sizeof(SOCKADDR_BTH) - 2;

    SOCKADDR_BTH *pSockAddr = (SOCKADDR_BTH *) &(pTA->Address[0].AddressType);
    BT_ADDR bt = SET_NAP_SAP(b.NAP, b.SAP);

    memset (pSockAddr, 0x00, sizeof(SOCKADDR_BTH));
    memcpy (&pSockAddr->btAddr, &bt, sizeof(bt));

    pSockAddr->addressFamily            = AF_BT;
    pSockAddr->port                        = c;

    void *pConnectionContext = NULL;
    ConnectEventInfo ci;
    PConnectEvent pEventConnect = pAddr->pEventConnect;

    gpTDIR->AddRef ();
    gpTDIR->Unlock ();

    TDI_STATUS TdiStatus = pEventConnect (pAddr->pEventConnectContext, TA_SIZE, 
                    pTA, 0x00, NULL, 0x00, NULL, &pConnectionContext, &ci);

    gpTDIR->Lock ();
    gpTDIR->DelRef ();

    if (gpTDIR->fIsConnected && (TdiStatus == TDI_MORE_PROCESSING)) {
        RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionByContext (pConnectionContext);
        if (pConn) {
            SVSUTIL_ASSERT (pConn->pAddr);    // This has to be an associated connection!
            SVSUTIL_ASSERT (! pConn->pPacketList);
            SVSUTIL_ASSERT (! pConn->pSendList);
            SVSUTIL_ASSERT (! pConn->pRecvList);

            SVSUTIL_ASSERT ((pConn->eStage == INITED) || (pConn->eStage == OPENING));

            ResetConnection (pConn, TRUE);        // Note - this resets the flow/credits
            TransferOptions (pConn, pConn->pAddr);    // Note - this overwrites MTU

            pConn->hConnect      = hConnection;

            memset (&pConn->BTAddr, 0, sizeof(pConn->BTAddr));
            pConn->BTAddr.addressFamily = AF_BTH;
            pConn->BTAddr.port   = c;
            pConn->BTAddr.btAddr = SET_NAP_SAP(b.NAP, b.SAP);

            pConn->mtu = imtu;

            if (fcreditflow) {
                pConn->fc_credit    = TRUE;
                pConn->iGaveCredits = RFCOMM_PN_CREDIT_MAX;
                pConn->iHaveCredits = icredits;
#if defined (DEBUG_CREDIT_FLOW)
                pConn->iCreditsRecv = icredits;
                pConn->iCreditsSent = RFCOMM_PN_CREDIT_MAX;
#endif
            } else {
                pConn->fc_credit    = FALSE;
                pConn->iGaveCredits = 0;
                pConn->iHaveCredits = 0;
#if defined (DEBUG_CREDIT_FLOW)
                pConn->iCreditsRecv = 0;
                pConn->iCreditsSent = 0;
#endif
            }

            IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+AcceptIncomingCall 0x%08x : negotiated parms mtu = %d, credit fc = %s, gave credits = %d, have credits = %d\n", pConn, pConn->mtu, pConn->fc_credit ? L"on" : L"off", pConn->iGaveCredits, pConn->iHaveCredits));

            pConn->eStage = OPENED;

            gpTDIR->AddRef ();
            gpTDIR->Unlock ();

            ci.cei_rtn(ci.cei_context, TDI_SUCCESS, 0);

            gpTDIR->Lock ();
            gpTDIR->DelRef ();

            return TRUE;
        } else
            ci.cei_rtn(ci.cei_context, TDI_INVALID_CONNECTION, 0);
    } else {
        if (TdiStatus == TDI_MORE_PROCESSING)
            ci.cei_rtn(ci.cei_context, TDI_CANCELLED, 0);

        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] AcceptIncomingCall : AFD doesn't want us :-(\n"));
    }

    return FALSE;
}

DWORD WINAPI AuthenticateOutgoingConnection (LPVOID arg) {
    HANDLE hConnection = (HANDLE)arg;

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+AuthenticateOutgoingConnection h = 0x%04x\n", hConnection));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] AuthenticateOutgoingConnection : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] AuthenticateOutgoingConnection : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionByHandle (hConnection);

    if (pConn) {
        SVSUTIL_ASSERT (pConn->pAddr);    // This must be an associated connection!
        SVSUTIL_ASSERT (! pConn->pPacketList);
        SVSUTIL_ASSERT (! pConn->pSendList);
        SVSUTIL_ASSERT (! pConn->pRecvList);
        SVSUTIL_ASSERT (pConn->hConnect == hConnection);
        SVSUTIL_ASSERT (pConn->fAuthenticate || pConn->fEncrypt);

        SVSUTIL_ASSERT (pConn->eStage == OPENING);

        int TdiStatus = TDI_SUCCESS;
        int fAuthenticate = pConn->fAuthenticate;
        int fEncrypt = pConn->fEncrypt;
        BT_ADDR bt = pConn->BTAddr.btAddr;

        gpTDIR->Unlock ();

        if (fAuthenticate && (BthAuthenticate (&bt) != ERROR_SUCCESS))
            TdiStatus = TDI_CONN_REFUSED;

        if (fEncrypt && (TdiStatus == TDI_SUCCESS) && (BthSetEncryption (&bt, TRUE) != ERROR_SUCCESS))
            TdiStatus = TDI_CONN_REFUSED;

        if (! gpTDIR)
            return ERROR_INTERNAL_ERROR;

        gpTDIR->Lock ();
        if (TdiStatus == TDI_SUCCESS)
            ResetConnection (pConn, FALSE);        // Leave credits be - they were set in PN process
        else
            TdiStatus = TDI_CONN_REFUSED;

        SignalConnectionCompleteUp (pConn, TdiStatus);

        if (gpTDIR->fIsConnected && (pConn == GetConnectionByHandle (hConnection)) && (pConn->eStage == OPENED))
            SendMSC (pConn, TDIR_V24_RTC|TDIR_V24_RTR, 0xff);
    } else
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] AuthenticateOutgoingConnection : no connection for handle 0x%08x\n", hConnection));

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-AuthenticateOutgoingConnection ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}


DWORD WINAPI AuthenticateConnectionRequest (LPVOID arg) {
    HANDLE hConnection = (HANDLE)arg;

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+AuthenticateConnectionRequest 0x%04x\n", hConnection));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] AuthenticateConnectionRequest : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] AuthenticateConnectionRequest : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    BD_ADDR            b;
    unsigned char    c;
    int                fLocal;
    int             fAcceptConnection = TRUE;

    if (ERROR_SUCCESS == gpTDIR->rfcomm_if.rfcomm_GetChannelAddress (gpTDIR->hRFCOMM, hConnection, &b, &c, &fLocal)) {
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Got connection request from %04x%08x 0x%02x\n", b.NAP, b.SAP, c));
        // Find address object...
        RFCOMM_ADDRESS_OBJECT *pAddr = gpTDIR->paolist;
        while (pAddr &&
            (! ((((pAddr->b.NAP == 0) && (pAddr->b.SAP == 0)) || (pAddr->b == b)) && pAddr->fListenOn && pAddr->pEventConnect && (pAddr->ch == c))))
            pAddr = pAddr->pNext;

        if (pAddr) {
            IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Found address object 0x%08x\n", pAddr));
            SVSUTIL_ASSERT (pAddr->fAuthenticate || pAddr->fEncrypt);

            int fAuthenticate = pAddr->fAuthenticate;
            int fEncrypt = pAddr->fEncrypt;
            BT_ADDR bt = SET_NAP_SAP (b.NAP, b.SAP);

            int cPinLength = pAddr->cPinLength;
            unsigned char caPinData[PIN_SIZE];
            memcpy (caPinData, pAddr->caPinData, sizeof(caPinData));

            SVSUTIL_ASSERT ((cPinLength >= 0) && (cPinLength <= PIN_SIZE));

            gpTDIR->Unlock ();

            __try {
                if (cPinLength)
                    BthSetPIN (&bt, cPinLength, caPinData);

                if (fAuthenticate && (BthAuthenticate (&bt) != ERROR_SUCCESS))
                    fAcceptConnection = FALSE;

                if (fEncrypt && fAcceptConnection && (BthSetEncryption (&bt, TRUE) != ERROR_SUCCESS))
                    fAcceptConnection = FALSE;
            } __except (1) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] exception in security code!\n"));
                fAcceptConnection = FALSE;
            }

            if (! gpTDIR)
                return ERROR_INTERNAL_ERROR;

            gpTDIR->Lock ();
            if (fAcceptConnection && gpTDIR->fIsConnected && (pAddr == GetAddressObject ((HANDLE)pAddr)))
                fAcceptConnection = AcceptIncomingCall (pAddr, hConnection);
            else
                fAcceptConnection = FALSE;
        } else {
            IFDBG(DebugOut (DEBUG_ERROR, L"Address translation failed for connection 0x%08x\n", hConnection));
            fAcceptConnection = FALSE;
        }
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"Address translation failed for connection 0x%08x\n", hConnection));
        fAcceptConnection = FALSE;
    }

    if (gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"AuthenticateConnectionRequest : %s connection\n", fAcceptConnection ? L"Accepting" : L"Declining"));
        HANDLE h = gpTDIR->hRFCOMM;
        RFCOMM_ConnectResponse_In pCallback = gpTDIR->rfcomm_if.rfcomm_ConnectResponse_In;

        gpTDIR->AddRef ();
        gpTDIR->Unlock ();
        __try {
            pCallback (h, NULL, hConnection, fAcceptConnection);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] AuthenticateConnectionRequest Exception in rfcomm_ConnectResponse_In\n"));
        }
        gpTDIR->Lock ();
        gpTDIR->DelRef ();
    }

    if (gpTDIR->fIsConnected && fAcceptConnection) {
        RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionByHandle (hConnection);
        if (pConn)
            SendMSC (pConn, TDIR_V24_RTC|TDIR_V24_RTR, 0xff);
    }

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-AuthenticateConnectionRequest ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

//
//    RFCOMM interface
//
static int tdir_connect_request_ind    (void *pUserContext, HANDLE hConnection, BD_ADDR *pba, unsigned char channel) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_connect_request_ind\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_connect_request_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_connect_request_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    BD_ADDR            b;
    unsigned char    c;
    int                fLocal;
    int             fAcceptConnection = FALSE;
    int                fSkipReply = FALSE;

    int                cPinLength = 0;
    unsigned char    caPinData[PIN_SIZE];

    if (ERROR_SUCCESS == gpTDIR->rfcomm_if.rfcomm_GetChannelAddress (gpTDIR->hRFCOMM, hConnection, &b, &c, &fLocal)) {
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Got connection request from %04x%08x 0x%02x\n", b.NAP, b.SAP, c));
        // Find address object...
        RFCOMM_ADDRESS_OBJECT *pAddr = gpTDIR->paolist;
        while (pAddr &&
            (! ((((pAddr->b.NAP == 0) && (pAddr->b.SAP == 0)) || (pAddr->b == b)) && pAddr->fListenOn && pAddr->pEventConnect && (pAddr->ch == c))))
            pAddr = pAddr->pNext;

        if (pAddr) {
            IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Found address object 0x%08x\n", pAddr));
            if (pAddr->fAuthenticate || pAddr->fEncrypt) {
                IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Address object 0x%08x requires authentication\n", pAddr));
                SVSCookie cookie = btutil_ScheduleEvent (AuthenticateConnectionRequest, hConnection, 0);
                if (cookie)
                    fSkipReply = TRUE;
                else {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] Could not create authentication thread in tdir_connect_request_ind\n"));
                }
            } else {
                cPinLength = pAddr->cPinLength;
                memcpy (caPinData, pAddr->caPinData, sizeof(caPinData));

                SVSUTIL_ASSERT ((cPinLength >= 0) && (cPinLength <= PIN_SIZE));

                fAcceptConnection = AcceptIncomingCall (pAddr, hConnection);
            }
        } else
            IFDBG(DebugOut (DEBUG_ERROR, L"Address translation failed for connection 0x%08x\n", hConnection));
    } else
        IFDBG(DebugOut (DEBUG_ERROR, L"Address translation failed for connection 0x%08x\n", hConnection));

    if (gpTDIR->fIsConnected && (! fSkipReply)) {
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"tdir_connect_request_ind : %s connection\n", fAcceptConnection ? L"Accepting" : L"Declining"));
        HANDLE h = gpTDIR->hRFCOMM;
        RFCOMM_ConnectResponse_In pCallback = gpTDIR->rfcomm_if.rfcomm_ConnectResponse_In;

        gpTDIR->AddRef ();
        gpTDIR->Unlock ();

        __try {
            if (fAcceptConnection && cPinLength) {
                bt_addr bt = SET_NAP_SAP (b.NAP, b.SAP);
                BthSetPIN (&bt, cPinLength, caPinData);
            }

            pCallback (h, NULL, hConnection, fAcceptConnection);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_connect_request_ind Exception in rfcomm_ConnectResponse_In\n"));
        }
        gpTDIR->Lock ();
        gpTDIR->DelRef ();
    }

    if (gpTDIR->fIsConnected && (! fSkipReply) && fAcceptConnection) {
        RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionByHandle (hConnection);
        if (pConn)
            SendMSC (pConn, TDIR_V24_RTC|TDIR_V24_RTR, 0xff);
    }

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_connect_request_ind ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int ConnectEntrySDP(RFCOMM_CONNECTION_OBJECT *pC) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"[TDIR] ConnectEntrySDP : 0x%08x\n", pC));

    int iErr = ERROR_INTERNAL_ERROR;

    SVSUTIL_ASSERT (pC->hCallContext != SVSUTIL_HANDLE_INVALID);

    BD_ADDR b;
    b.NAP = GET_NAP(pC->BTAddr.btAddr);
    b.SAP = GET_SAP(pC->BTAddr.btAddr);

    HANDLE h = gpTDIR->hSDP;
    SVSHandle hContext = pC->hCallContext;
    SDP_Connect_In pCallback = gpTDIR->sdp_if.sdp_Connect_In;
    gpTDIR->AddRef ();
    gpTDIR->Unlock ();

    __try {
        iErr = pCallback (h, (LPVOID)hContext, &b);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] : exception in ConnectEntrySDP!\n"));
    }

    gpTDIR->Lock ();
    gpTDIR->DelRef ();

    if (!GetConnectionObject ((HANDLE)pC) || (!gpTDIR->fIsConnected && (iErr == ERROR_SUCCESS)))
        iErr = ERROR_OPERATION_ABORTED;

    return iErr;
}

static int ConnectEntryRFCOMM(RFCOMM_CONNECTION_OBJECT *pC) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"[TDIR] ConnectEntryRFCOMM : 0x%08x\n", pC));

    SVSUTIL_ASSERT (! pC->hConnect);

    int iErr = ERROR_INTERNAL_ERROR;

    HANDLE hConnect = NULL;
    HANDLE h = gpTDIR->hRFCOMM;
    RFCOMM_CreateChannel pCallback = gpTDIR->rfcomm_if.rfcomm_CreateChannel;

    unsigned char chPort = (unsigned char)pC->BTAddr.port;
    bt_addr bt = pC->BTAddr.btAddr;

    int cPinLength = pC->cPinLength;
    unsigned char caPinData[PIN_SIZE];
    memcpy (caPinData, pC->caPinData, sizeof(caPinData));

    SVSUTIL_ASSERT ((cPinLength >= 0) && (cPinLength <= PIN_SIZE));

    gpTDIR->AddRef ();
    gpTDIR->Unlock ();

    __try {
        if (cPinLength)
            BthSetPIN (&bt, cPinLength, caPinData);

        BD_ADDR b;

        b.NAP = GET_NAP(bt);
        b.SAP = GET_SAP(bt);

        iErr = pCallback (h, &b, chPort, &hConnect);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] : exception in ConnectEntryRFCOMM!\n"));
    }

    gpTDIR->Lock ();
    gpTDIR->DelRef ();

#if defined (DEBUG) || defined (_DEBUG)
    if (hConnect) {
        RFCOMM_CONNECTION_OBJECT *pC2 = gpTDIR->pcolist;
        while (pC2) {
            SVSUTIL_ASSERT ((pC2 == pC) || (pC2->hConnect != hConnect));
            pC2 = pC2->pNext;
        }
    }
#endif

    pC = GetConnectionObject ((HANDLE)pC);

    if (hConnect && gpTDIR->fIsConnected && pC && (iErr == ERROR_SUCCESS)) {
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"[TDIR] ConnectEntryRFCOMM : object 0x%08x handle 0x%08x channel = %d\n", pC, hConnect, chPort));

        SVSUTIL_ASSERT (pC->hCallContext != SVSUTIL_HANDLE_INVALID);
        SVSHandle hContext = pC->hCallContext;

        pC->hConnect = hConnect;

        int n1 = pC->mtu;

        RFCOMM_PNREQ_In pCallback2 = gpTDIR->rfcomm_if.rfcomm_PNREQ_In;
        gpTDIR->AddRef ();
        gpTDIR->Unlock ();

        iErr = ERROR_INTERNAL_ERROR;

        __try {
            iErr = pCallback2 (h, (LPVOID)hContext, hConnect, TDIR_PRI, n1, RFCOMM_PN_CREDIT_IN, RFCOMM_PN_CREDIT_MAX);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] : exception in rfcomm_CreateChannel!\n"));
        }

        gpTDIR->Lock ();
        gpTDIR->DelRef ();
        if (gpTDIR->fIsInitialized)
            pC = GetConnectionObject ((HANDLE)pC);
        else
            pC = NULL;
    }

    if (! pC)
        iErr = ERROR_OPERATION_ABORTED;

    return iErr;
}


void SDPDisconnect(unsigned short cid)  {
    HANDLE h = gpTDIR->hSDP;
    SDP_Disconnect_In  pCallback = gpTDIR->sdp_if.sdp_Disconnect_In;

    SVSUTIL_ASSERT(gpTDIR->IsLocked());

    gpTDIR->AddRef ();
    gpTDIR->Unlock ();

    __try {
        pCallback (h,NULL,cid);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] : exception in sdp_disconnect_in!\n"));
    }

    gpTDIR->Lock ();
    gpTDIR->DelRef ();
}



static int tdir_data_up_ind (void *pUserContext, HANDLE hConnection, BD_BUFFER *pba, int additional_credits) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_data_up_ind, data = %d, extra credits = %d\n", BufferTotal (pba), additional_credits));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_data_up_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_data_up_ind : ERROR_SERVICE_NOT_ACTIVE\n"));

        if ((! pba->fMustCopy) && pba->pFree)
            pba->pFree (pba);

        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

#if defined (DEBUG_PEER_SYNCHRONIZATION)
    PeerDebugData pd;
    PeerDebugData *ppd = NULL;
    if (BufferTotal (pba) >= sizeof(pd)) {
        memcpy (&pd, pba->pBuffer + pba->cEnd - sizeof(pd), sizeof(pd));
        if (pd.magic == DEBUG_PEER_MAGIC) {
            pba->cEnd -= sizeof(pd);
            ppd = &pd;
        }
    }

    if (! ppd) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_data_up_ind : mismatch of peer debug builds!\n"));
        SVSUTIL_ASSERT (0);
    }
#endif

    int fDataAccepted = FALSE;

    //    Try to satisfy outstanding RECVs first...

    int cBytes = BufferTotal (pba);
    RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionByHandle (hConnection);

    if ((cBytes > 0) && pConn && pConn->fc_credit)
        --pConn->iGaveCredits;

#if defined (DEBUG_CREDIT_FLOW)
    if (pConn && (cBytes > 0))
        ++pConn->iPacketsRecv;

    if (pConn && pConn->fc_credit) {
        pConn->iCreditsRecv += additional_credits;
        // Credit Invariants

        if (pConn->iGaveCredits != (pConn->iCreditsSent - pConn->iPacketsRecv)) {
            SVSUTIL_ASSERT (0);
            RETAILMSG(1, (L"Bluetooth TDI Credit mismatch: GaveCredits = %d Total SentCredits = %d Total PacketsRecv = %d\n", pConn->iGaveCredits, pConn->iCreditsSent, pConn->iPacketsRecv));
        }

        if (pConn->iHaveCredits + additional_credits != (pConn->iCreditsRecv - pConn->iPacketsSent)) {
            RETAILMSG(1, (L"Bluetooth TDI Credit mismatch: HaveCredits = %d (incl %d addtl) Total CreditsRecv = %d Total PacketsSent = %d\n", pConn->iHaveCredits + additional_credits, additional_credits, pConn->iCreditsRecv, pConn->iPacketsSent));
            SVSUTIL_ASSERT (0);
        }
    }
#endif

#if defined (DEBUG_PEER_SYNCHRONIZATION) && defined (DEBUG_CREDIT_FLOW)
    if (ppd && pConn) {
        SVSUTIL_ASSERT (ppd->iPacketsSent == pConn->iPacketsRecv);
        SVSUTIL_ASSERT (ppd->iCreditsSent == pConn->iCreditsRecv);
    }
#endif

    while (gpTDIR->fIsConnected && (cBytes > 0) && pConn && pConn->pRecvList && pConn->pRecvList->pNdisBuffer) {
        SVSUTIL_ASSERT (! pConn->pRecvList->pBuffer);

        NDIS_BUFFER *pNdis = pConn->pRecvList->pNdisBuffer;

        DWORD dwPerms = SetProcPermissions (pConn->pRecvList->dwPerms);
        BOOL  bkm = SetKMode (TRUE);

        int cBytesToTransfer = QueryNdisSize (pNdis);
        if (cBytes < cBytesToTransfer)
            cBytesToTransfer = cBytes;

        SVSUTIL_ASSERT (pConn->pRecvList->iNdisUsed == 0);

        uint StartOffset = 0;
        TDI_STATUS tdiStatus = TDI_SUCCESS;        

        __try {
            CopyFlatToNdis (pNdis, pba->pBuffer + pba->cStart, cBytesToTransfer, &StartOffset);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_data_up_ind : Exception copying to TDI buffer\n"));
            tdiStatus = TDI_BUFFER_TOO_SMALL;
        }

        SetKMode (bkm);
        SetProcPermissions (dwPerms);

        pba->cStart += cBytesToTransfer;
        pConn->pRecvList->iNdisUsed = cBytesToTransfer;

        cBytes = BufferTotal (pba);

        RFCOMM_RECEIVE_OBJECT *pThis = pConn->pRecvList;
        pConn->pRecvList = pConn->pRecvList->pNext;

        PBT_RECV_COMPLETE   pRecvEvent = pThis->pRecvEvent;
        void                *pRecvContext = pThis->pRecvContext;
        int                 iNdisUsed = pThis->iNdisUsed;

        delete pThis;

        IFDBG(DebugOut (DEBUG_TDI_PACKETS, L"tdir_data_up_ind 0x%08x :: satisfied read request for %d bytes\n", pConn, iNdisUsed));

        gpTDIR->AddRef ();
        gpTDIR->Unlock ();

        pRecvEvent (pRecvContext, tdiStatus, iNdisUsed);

        gpTDIR->Lock ();
        gpTDIR->DelRef ();

        if (gpTDIR->fIsInitialized)
            pConn = GetConnectionByHandle (hConnection);
        else
            pConn = NULL;
    }

    SVSUTIL_ASSERT (cBytes >= 0);

    if (gpTDIR->fIsConnected && (cBytes > 0) && pConn && pConn->pAddr &&
                                ((pConn->irecvd + cBytes) <= GetQuota (pConn))) {
        SVSUTIL_ASSERT ((! pConn->pRecvList) || pConn->pRecvList->pBuffer);

        RFCOMM_RECEIVE_OBJECT *pRecv = new RFCOMM_RECEIVE_OBJECT;
        if (pRecv) {
            memset (pRecv, 0, sizeof(*pRecv));
            pRecv->pBuffer = pba->fMustCopy ? BufferCopy (pba) : pba;
            if (! pConn->pRecvList)
                pConn->pRecvList = pRecv;
            else {
                RFCOMM_RECEIVE_OBJECT *pRP = pConn->pRecvList;
                while (pRP->pNext)
                    pRP = pRP->pNext;
                pRP->pNext = pRecv;
            }

            fDataAccepted = TRUE;
            pConn->irecvd += cBytes;

            if (pConn->pAddr && (! pConn->cbDataIndicated)) {
                // If we need to queue any data, we indicate it first.
                // Only indicate the first time since we are using the same data.

                pConn->cbDataIndicated = cBytes;
                EventRcvBuffer     EventRecvCompleteInfo;    
                int                BytesTaken = 0;

                void *pEventReceiveContext = pConn->pAddr->pEventReceiveContext;
                void *pConnectionContext = pConn->pConnectionContext;
                PRcvEvent pEventReceive = pConn->pAddr->pEventReceive;

                gpTDIR->AddRef ();
                gpTDIR->Unlock ();

                TDI_STATUS TdiStatus = pEventReceive (pEventReceiveContext, pConnectionContext,
                    0, cBytes, cBytes, (unsigned int *)&BytesTaken, NULL, &EventRecvCompleteInfo);

                gpTDIR->Lock ();
                gpTDIR->DelRef ();

                IFDBG(DebugOut (DEBUG_TDI_PACKETS, L"tdir_data_up_ind 0x%08x :: indicated %d bytes\n", pConn, cBytes));

                SVSUTIL_ASSERT (TdiStatus == TDI_NOT_ACCEPTED);
                SVSUTIL_ASSERT (BytesTaken == 0);

                if (gpTDIR->fIsInitialized)
                    pConn = GetConnectionByHandle (hConnection);
                else
                    pConn = NULL;
            } else {
                IFDBG(DebugOut (DEBUG_TDI_PACKETS, L"tdir_data_up_ind 0x%08x :: data already indicated, stored %d bytes\n", pConn, cBytes));
            }

            if (pConn && (pConn->irecvd > pConn->xon_lim) && (! pConn->fc_local) && (! pConn->fc_credit)) {
                IFDBG(DebugOut (DEBUG_TDI_PACKETS, L"Switching flow off for connection 0x%08x\n", pConn));

                pConn->fc_local = TRUE;
                SendMSC (pConn, TDIR_V24_RTC|TDIR_V24_RTR, 0xff);
            }
        } else {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] tdir_data_up_ind :: OOM !\n"));
        }
    }
#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
    else if (cBytes != 0) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] Data came to nonexistent connection 0x%08x or overflow!\n", hConnection));
        if (gpTDIR->fIsConnected && pConn)
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] Overflow details: received so far: %d, quota %d, bytes to queue %d\n", pConn->irecvd, GetQuota (pConn), cBytes));
    }
#endif

    if (! fDataAccepted) {
        if ((! pba->fMustCopy) && pba->pFree)
            pba->pFree (pba);
    }

    if (pConn && (pConn == GetConnectionByHandle (hConnection)) && pConn->fc_credit) {
        pConn->iHaveCredits += additional_credits;
        if (pConn->iHaveCredits - additional_credits <= 0)
            SendData (pConn);

        if (gpTDIR->fIsInitialized)
            pConn = GetConnectionByHandle (hConnection);
        else
            pConn = NULL;
    }

    if (pConn && pConn->fc_credit)
        SendCredits (pConn);

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_data_up_ind ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int tdir_disconnect_ind (void *pUserContext, HANDLE hConnection) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_disconnect_ind\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_disconnect_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_disconnect_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    ExtractPN (hConnection, NULL, NULL, NULL);

    RFCOMM_CONNECTION_OBJECT *pC = GetConnectionByHandle (hConnection);

    if (pC) {
        pC->hConnect = NULL;
        SignalConnectionLoss (pC, TDI_GRACEFUL_DISC, FALSE);
    } else
        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] tdir_disconnect_ind : connection for handle 0x%08x not found or in process of being closed!\n", hConnection));

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_disconnect_ind 0x%08x ERROR_SUCCESS\n", pC));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int tdir_msc_ind (void *pUserContext, HANDLE hConnection, unsigned char v24, unsigned char bs) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_msc_ind\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_msc_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_msc_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionByHandle (hConnection);
    if (pConn) {
        pConn->v24in = v24;
        pConn->brin  = bs;

        if (! pConn->fc_credit) {
            if (v24 & TDIR_V24_FC) // fc
                pConn->fc_remote = TRUE;
            else {
                pConn->fc_remote = FALSE;
                SendData (pConn);
            }
        }
    } else
        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] tdir_msc_ind : Connection not found for handle 0x%08x\n", hConnection));

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_msc_ind ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int tdir_rls_ind (void *pUserContext, HANDLE hConnection, unsigned char rls) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_rls_ind\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_rls_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_rls_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionByHandle (hConnection);
    if (pConn) {
        pConn->rlsin = rls;
    } else
        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] tdir_rls_ind : Connection not found for handle 0x%08x\n", hConnection));

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_rls_ind ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int tdir_fc_ind (void *pUserContext, HANDLE hConnect, unsigned char fcOn) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_fc_ind\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_fc_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_fc_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONNECTION_OBJECT *pC = GetConnectionByHandle (hConnect);

    if (pC) {
        if (fcOn) {    // Initiate data sends...
            pC->fc_aggregate = FALSE;
            SendData (pC);
        } else
            pC->fc_aggregate = TRUE;
    } else
        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] tdir_fc_ind : Connection not found!\n"));

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_fc_ind ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int tdir_pnreq_ind (void *pUserContext, HANDLE hConnection, unsigned char priority, unsigned short n1, int use_credit_fc, int initial_credits) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_pnreq_ind, pri = %d, mtu = %d, credit fc = %x, initial credits = %d\n", priority, n1, use_credit_fc, initial_credits));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_pnreq_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_pnreq_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONNECTION_OBJECT *pC = GetConnectionByHandle (hConnection);
    if (pC) {
        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] tdir_pnreq_ind : unusual: PNREQ for existing connection, 0x%08x handle 0x%08x\n", pC, hConnection));
        if (n1 > pC->imax_mtu)
            n1 = pC->imax_mtu;
        if (n1 < pC->imin_mtu)
            n1 = pC->imin_mtu;
        else
            pC->mtu = n1;
    } else {
        if (n1 > TDIR_MTUMAX)
            n1 = TDIR_MTUMAX;
        if (n1 < TDIR_MTUMIN)
            n1 = TDIR_MTUMIN;

        RFCOMM_PN_OBJECT *pPNO = gpTDIR->pnlist;
        while (pPNO && (pPNO->hConnection != hConnection))
            pPNO = pPNO->pNext;

        if (! pPNO) {
            pPNO = new RFCOMM_PN_OBJECT;
            if (pPNO) {
                pPNO->pNext       = gpTDIR->pnlist;
                gpTDIR->pnlist    = pPNO;
            }
        }

        if (pPNO) {
            pPNO->hConnection = hConnection;
            pPNO->imtu        = n1;
            if (use_credit_fc == RFCOMM_PN_CREDIT_IN) {
                pPNO->fCreditFlow = TRUE;
                pPNO->iCredits    = initial_credits;
            } else {
                pPNO->fCreditFlow = FALSE;
                pPNO->iCredits    = 0;
            }                
        }
    }

    HANDLE h = gpTDIR->hRFCOMM;
    RFCOMM_PNRSP_In pCallback = gpTDIR->rfcomm_if.rfcomm_PNRSP_In;

    gpTDIR->Unlock ();

    int iRes = ERROR_INTERNAL_ERROR;
    __try {
        iRes = pCallback (h, NULL, hConnection, priority, n1,
                        use_credit_fc ? RFCOMM_PN_CREDIT_OUT : 0,
                        use_credit_fc ? RFCOMM_PN_CREDIT_MAX : 0);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] tdir_pnreq_ind Exception in rfcomm_PNRSP_In\n"));
    }

#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
    if (iRes != ERROR_SUCCESS)
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] tdir_pnreq_ind rfcomm_PNRSP_In returns %d\n", iRes));
#endif

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_pnreq_ind ERROR_SUCCESS\n"));
    return ERROR_SUCCESS;
}

static int tdir_rpnreq_ind (void *pUserContext, HANDLE hConnection, unsigned short mask, int baud, int data, int stop, int parity, int parity_type, unsigned char flow, unsigned char xon, unsigned char xoff) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_rpnreq_ind\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_rpnreq_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_rpnreq_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONNECTION_OBJECT *pC = GetConnectionByHandle (hConnection);
    if (pC) {
        if (mask & RFCOMM_RPN_HAVE_BAUD)
            pC->BaudRate = baud;

        if (mask & RFCOMM_RPN_HAVE_DATA)
            pC->ByteSize = data;

        if (mask & RFCOMM_RPN_HAVE_STOP)
            pC->StopBits = stop;

        if (mask & RFCOMM_RPN_HAVE_PARITY)
            pC->fParity = parity;

        if (mask & RFCOMM_RPN_HAVE_PT)
            pC->Parity = parity_type;

        if (! mask) {
            mask = RFCOMM_RPN_HAVE_BAUD | RFCOMM_RPN_HAVE_DATA | RFCOMM_RPN_HAVE_STOP | 
                RFCOMM_RPN_HAVE_PARITY | RFCOMM_RPN_HAVE_PT;
            baud = pC->BaudRate;
            data = pC->ByteSize;
            stop = pC->StopBits;
            parity = pC->fParity;
            parity_type = pC->Parity;
            xoff = 0;
            xon = 0;
            flow = 0;
        }
    }

    HANDLE h = gpTDIR->hRFCOMM;
    RFCOMM_RPNRSP_In pCallback = gpTDIR->rfcomm_if.rfcomm_RPNRSP_In;

    gpTDIR->AddRef ();
    gpTDIR->Unlock ();

    __try {
        //    Accept everything...
        pCallback (h, NULL, hConnection, mask, baud, data, stop, parity, parity_type, flow, xon, xoff);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_rpnreq_ind : Exception in rfcomm_RPNRSP_In\n"));
    }

    gpTDIR->Lock ();
    gpTDIR->DelRef ();

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_rpnreq_ind ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int tdir_stack_event (void *pUserContext, int iEvent, void *pEventContext) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_stack_event\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_stack_event : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_stack_event : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    switch (iEvent) {
    case BTH_STACK_RESET:
        btutil_ScheduleEvent (StackXXX, (LPVOID)3);
        break;

    case BTH_STACK_DOWN:
        btutil_ScheduleEvent (StackXXX, (LPVOID)2);
        break;

    case BTH_STACK_UP:
        btutil_ScheduleEvent (StackXXX, (LPVOID)1);
        break;

    case BTH_STACK_DISCONNECT:
        btutil_ScheduleEvent (StackDisconnect, NULL);
        break;
    }

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_stack_event ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int tdir_call_aborted (void *pCallContext, int iError) {
    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] +tdir_call_aborted\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_call_aborted : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_call_aborted : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    HANDLE hObject = (HANDLE) btutil_TranslateHandle ((SVSHandle)pCallContext);
    if (hObject) {
        RFCOMM_CONNECTION_OBJECT *pC = NULL;
        pC = GetConnectionObject (hObject);
        if (! pC) {
            RFCOMM_PACKET_OBJECT *pP = GetPacketObject (hObject);
            if (pP)
                pC = pP->pConn;
        }

        SVSUTIL_ASSERT (pC);

        if (pC) {
            HANDLE hConn = pC->hConnect;
            pC->hConnect = NULL;

            IFDBG(DebugOut (DEBUG_WARN, L"[TDI] +tdir_call_aborted : aborting connection 0x%08x handle 0x%08x\n", pC, hConn));

            SignalConnectionLoss (pC, TDI_CONNECTION_ABORTED, TRUE);
            if (gpTDIR->fIsConnected && hConn)
                DisconnectConnection (NULL, hConn, NULL);
        }
    }

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_call_aborted ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int tdir_connect_request_out    (void *pCallContext, int iError, HANDLE hConnection) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_connect_request_out\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_connect_request_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_connect_request_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionByHandle (hConnection);

    if (pConn) {
        SVSUTIL_ASSERT (pConn->pAddr);    // This must be an associated connection!
        SVSUTIL_ASSERT (! pConn->pPacketList);
        SVSUTIL_ASSERT (! pConn->pSendList);
        SVSUTIL_ASSERT (! pConn->pRecvList);
        SVSUTIL_ASSERT (pConn->hConnect == hConnection);

        SVSUTIL_ASSERT (pConn->eStage == OPENING);

        int TdiStatus = TDI_SUCCESS;
        if (iError == ERROR_SUCCESS)
            ResetConnection (pConn, FALSE);        // Leave credits be - they were set in PN process
        else
            TdiStatus = TDI_CONN_REFUSED;

        if ((TdiStatus == ERROR_SUCCESS) && (pConn->fAuthenticate || pConn->fEncrypt)) {
            IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Connection object 0x%08x requires authentication\n", pConn));
            SVSCookie c = btutil_ScheduleEvent (AuthenticateOutgoingConnection, hConnection, 0);
            if (c == 0) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] Could not create authentication thread in tdir_connect_request_ind\n"));
                SignalConnectionCompleteUp (pConn, TDI_CONN_REFUSED);
            }
        } else
            SignalConnectionCompleteUp (pConn, TdiStatus);

        if (gpTDIR->fIsConnected && (pConn == GetConnectionByHandle (hConnection)) && (pConn->eStage == OPENED))
            SendMSC (pConn, TDIR_V24_RTC|TDIR_V24_RTR, 0xff);
    } else
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_connect_request_out : no connection for handle 0x%08x\n", hConnection));

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_connect_request_out 0x%08x ERROR_SUCCESS\n", pConn));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int tdir_disconnect_out    (void *pCallContext, int iError) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_disconnect_out\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_disconnect_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_disconnect_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }
    
    RFCOMM_CONNECTION_OBJECT *pConn = (RFCOMM_CONNECTION_OBJECT *) btutil_TranslateHandle ((SVSHandle)pCallContext);

    if (pConn) {
        SVSUTIL_ASSERT (pConn == GetConnectionObject ((HANDLE)pConn));
        pConn->hConnect = NULL;
        SignalConnectionLoss (pConn, TDI_GRACEFUL_DISC, FALSE);
    } else
        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] tdir_disconnect_out : no closing connection 0x%08x\n", pCallContext));

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_disconnect_out 0x%08x ERROR_SUCCESS\n", pConn));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int tdir_pnreq_out (void *pCallContext, int iError, unsigned char priority, unsigned short n1, int use_credit_fc, int initial_credits) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_pnreq_out, pri = %d, mtu = %d, credit fc = %x, initial credits = %d\n", priority, n1, use_credit_fc, initial_credits));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_pnreq_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_pnreq_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONNECTION_OBJECT *pConn = (RFCOMM_CONNECTION_OBJECT *) btutil_TranslateHandle ((SVSHandle)pCallContext);

    if (pConn) {
        int iErr = ERROR_SUCCESS;

        SVSUTIL_ASSERT (pConn == GetConnectionObject ((HANDLE)pConn));
        SVSUTIL_ASSERT (pConn->hCallContext != SVSUTIL_HANDLE_INVALID);

        HANDLE hConnect = pConn->hConnect;
        SVSHandle hContext = pConn->hCallContext;
        HANDLE h = gpTDIR->hRFCOMM;
        if ((n1 >= pConn->imin_mtu) && (n1 <= pConn->imax_mtu)) {
            IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Accepted MTU %d for connection 0x%08x\n", n1, pConn));

            if (n1) 
                pConn->mtu = n1;
            else {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] tdir_pnreq_out : warning - n1 should be non-zero, MTU might be too large.\n"));
                pConn->mtu = TDIR_MTU_DESIRED;
            }
            
            if (use_credit_fc) {
                pConn->fc_credit    = TRUE;
                pConn->iGaveCredits = RFCOMM_PN_CREDIT_MAX;
                pConn->iHaveCredits = initial_credits;
#if defined (DEBUG_CREDIT_FLOW)
                pConn->iCreditsRecv = initial_credits;
                pConn->iCreditsSent = RFCOMM_PN_CREDIT_MAX;
#endif
            } else {
                pConn->fc_credit    = FALSE;
                pConn->iGaveCredits = 0;
                pConn->iHaveCredits = 0;
#if defined (DEBUG_CREDIT_FLOW)
                pConn->iCreditsRecv = 0;
                pConn->iCreditsSent = 0;
#endif
            }

            IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_pnreq_out : negotiated parms mtu = %d, credit fc = %s, gave credits = %d, have credits = %d\n", pConn->mtu, pConn->fc_credit ? L"on" : L"off", pConn->iGaveCredits, pConn->iHaveCredits));
            
            RFCOMM_ConnectRequest_In pCallback2 = gpTDIR->rfcomm_if.rfcomm_ConnectRequest_In;

            gpTDIR->AddRef ();
            gpTDIR->Unlock ();

            iErr = ERROR_INTERNAL_ERROR;

            __try {
                iErr = pCallback2 (h, (LPVOID)hContext, hConnect);
            } __except (1) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] : exception in rfcomm_CreateChannel!\n"));
            }

            gpTDIR->Lock ();
            gpTDIR->DelRef ();
        } else {
            IFDBG(DebugOut (DEBUG_WARN, L"[TDI] The other side has supplied unacceptable mtu %d for connection 0x%08x\n", n1, pConn));
            iErr = ERROR_PROTOCOL_UNREACHABLE;
        }

        if (iErr != ERROR_SUCCESS)
            SignalConnectionCompleteUp (pConn, TDI_CONN_REFUSED);
    } else
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_pnreq_out : no connection for handle 0x%08x\n", pCallContext));

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_pnreq_out ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

static int tdir_data_down_out (void *pCallContext, int iError) {
    if (! pCallContext) {
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_data_down_out : ignored credit-flow grant\n"));
        return ERROR_SUCCESS;
    }

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdir_data_down_out\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_data_down_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdir_data_down_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_PACKET_OBJECT *pP = (RFCOMM_PACKET_OBJECT *) btutil_TranslateHandle ((SVSHandle)pCallContext);

    if (pP) {
        SVSUTIL_ASSERT (pP == GetPacketObject ((HANDLE)pP));
        if (pP == pP->pConn->pPacketList)
            pP->pConn->pPacketList = pP->pConn->pPacketList->pNext;
        else {
            RFCOMM_PACKET_OBJECT *pPP = pP->pConn->pPacketList;
            while (pPP->pNext != pP)
                pPP = pPP->pNext;
            pPP->pNext = pP->pNext;
        }

        int cBytes = pP->cBytesSent;
        RFCOMM_CONNECTION_OBJECT *pC = pP->pConn;
        delete pP;

        RegisterTransferCompletion (pC, cBytes);
    }

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-tdir_data_down_out ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

//
// SDP Callback functions
//

int tdisdp_connect_out(void *pCallContext, unsigned long status, unsigned short cid) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdisdp_connect_out\n"));

    int iErr = ERROR_INTERNAL_ERROR;
    RFCOMM_CONNECTION_OBJECT      *pC = NULL;
    SDP_ServiceAttributeSearch_In pCallback;
    SdpQueryUuid                  uuid[2];
    SdpAttributeRange             attribRange;
    HANDLE                        h;

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdisdp_connect_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }
    gpTDIR->Lock ();

    // Past this point we use goto done; because we need to call sdp_Disconnect on error.
    if (! gpTDIR->fIsConnected) {
        iErr = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }

    if (NULL == (pC = (RFCOMM_CONNECTION_OBJECT *) btutil_TranslateHandle ((SVSHandle)pCallContext))) {
        iErr = ERROR_INVALID_DATA;
        goto done;
    }

    SVSUTIL_ASSERT (pC == GetConnectionObject ((HANDLE)pC));

    if (status != ERROR_SUCCESS) {
        SVSUTIL_ASSERT(cid == 0);
        iErr = ERROR_CONNECTION_UNAVAIL;
        goto done;
    }
    pC->sdpCid = cid;

    // Call ServiceAttribSearch to get protocol headers.
    pCallback = gpTDIR->sdp_if.sdp_ServiceAttributeSearch_In;
    h = gpTDIR->hSDP;

    uuid[0].uuidType = SDP_ST_UUID128;
    memcpy(&uuid[0].u.uuid128,&pC->BTAddr.serviceClassId,sizeof(GUID));
    memset(&uuid[1],0,sizeof(uuid[1]));

    attribRange.minAttribute = attribRange.maxAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;

    gpTDIR->AddRef ();
    gpTDIR->Unlock ();

    __try {
        iErr = pCallback (h, pCallContext, cid, uuid,&attribRange,1);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] : exception in sdp_ServiceAttributeSearch_In!\n"));
    }

    gpTDIR->Lock ();
    gpTDIR->DelRef ();

done:
    SVSUTIL_ASSERT(gpTDIR->IsLocked());

    if (iErr != ERROR_SUCCESS) {
        IFDBG(DebugOut(DEBUG_ERROR,L"tdisdp_connect_out: fails, err = 0x%08x\r\n",iErr));

        if (status == ERROR_SUCCESS)
            SDPDisconnect(cid);

        pC = (RFCOMM_CONNECTION_OBJECT *) btutil_TranslateHandle ((SVSHandle)pCallContext);
        if (pC && gpTDIR->fIsConnected)
            SignalConnectionCompleteUp (pC, TDI_CONN_REFUSED);
    }
    gpTDIR->Unlock ();
    return ERROR_SUCCESS;
}

int tdisdp_service_attribute_search_out(void *pCallContext, unsigned long status, unsigned char *pOutBuf, unsigned long cOutBuf) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdisdp_service_attribute_search_out\n"));

    int iErr = ERROR_INTERNAL_ERROR;
    RFCOMM_CONNECTION_OBJECT *pC = NULL;
    BOOL fFreeLock = FALSE;

    if (! gpTDIR) {
        iErr = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }
    gpTDIR->Lock ();
    fFreeLock = TRUE;

    if (! gpTDIR->fIsConnected) {
        iErr = ERROR_SERVICE_NOT_ACTIVE;
        goto done;
    }

    pC = (RFCOMM_CONNECTION_OBJECT *) btutil_TranslateHandle ((SVSHandle)pCallContext);
    if ((! pC) || (status != ERROR_SUCCESS) || (! pOutBuf) || (! cOutBuf)) {
        iErr = ERROR_INVALID_DATA;
        goto done;
    }

    SVSUTIL_ASSERT (pC == GetConnectionObject ((HANDLE)pC));

    // Grep through tree here, fill in pC->BTAddr.bthChannel = 1st found item.
    if (ERROR_SUCCESS != (iErr = GetProtocolCidFromResponse(pOutBuf,cOutBuf,&RFCOMM_PROTOCOL_UUID,&pC->BTAddr.port)))
        goto done;

    IFDBG(DebugOut(DEBUG_TDI_TRACE,L"tdisdp_service_attribute_search_out has got valid response, attempting to connect to channel 0x%08x\r\n",pC->BTAddr.port));

    iErr = ConnectEntryRFCOMM(pC);
done:

#if defined (DEBUG) || defined (_DEBUG)
    if (iErr != ERROR_SUCCESS)
        IFDBG(DebugOut(DEBUG_ERROR,L"tdisdp_service_attribute_search_out: err=0x%08x\r\n",iErr));
#endif

    if (pOutBuf)
        g_funcFree(pOutBuf,g_pvAllocData);

    if (!fFreeLock)
        return iErr;

    SVSUTIL_ASSERT(gpTDIR && gpTDIR->IsLocked());

    if (gpTDIR->fIsConnected && pC) {
        if (pC == GetConnectionObject ((HANDLE)pC)) {
            SDPDisconnect(pC->sdpCid);
            if (iErr != ERROR_SUCCESS) {
                SignalConnectionCompleteUp (pC, TDI_CONN_REFUSED);
            }
        }
    }

    gpTDIR->Unlock();
    return ERROR_SUCCESS;
}

// These do nothing currently
int    tdisdp_disconnect_out(void *pCallContext, unsigned long status) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdisdp_disconnect_out\n"));
    return ERROR_SUCCESS;
}

int tdisdp_service_search_out(void *pCallContext, unsigned long status, unsigned short cReturnedHandles, unsigned long *pHandles) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdisdp_service_search_out\n"));
    SVSUTIL_ASSERT(0);
    return ERROR_SUCCESS;
}

int tdisdp_attribute_search_out(void *pCallContext, unsigned long status, unsigned char *pOutBuf, unsigned long cOutBuf) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+tdisdp_attribute_search_out\n");)    
    SVSUTIL_ASSERT(0);
    return ERROR_SUCCESS;
}

//static int tdir_RPNREQ_Out            (void *pCallContext, int iError, unsigned char mask, unsigned char b, unsigned char d, unsigned char flc, unsigned char xon, unsigned char xoff, unsigned short pm);

//HCI Callback functions
static int tdih_read_bdaddr_out(void *pCallContext, unsigned char status, BD_ADDR *pba) {
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdih_read_btaddr_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpTDIR->Lock ();

    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] tdih_read_btaddr_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpTDIR->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    if (status) {
        int iErr = StatusToError (status, ERROR_INTERNAL_ERROR);
        IFDBG(DebugOut(DEBUG_ERROR, L"tdih_read_btaddr_out failed. (Status=%d, Err=0x%08x)", status, iErr));
        gpTDIR->Unlock ();
        return iErr;
    }

    SVSUTIL_ASSERT(pba);

    gpTDIR->btAddr = SET_NAP_SAP(pba->NAP, pba->SAP);

    gpTDIR->Unlock ();
    return ERROR_SUCCESS;
}

//
//    Initialization code
//
int RFCOMM_TDI::Init (void) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+Init\n"));

    SVSUTIL_ASSERT (! fIsConnected);
    SVSUTIL_ASSERT (! fIsInitialized);
    SVSUTIL_ASSERT (! hRFCOMM);
    SVSUTIL_ASSERT (! hSDP);

    reset ();

    if ((! pfmdAO) && (! (pfmdAO = svsutil_AllocFixedMemDescr (sizeof(RFCOMM_ADDRESS_OBJECT), 10)))) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI_RFCOMM] -Init :: ERROR_OUTOFMEMORY - 1\n"));
        return ERROR_OUTOFMEMORY;
    }

    if ((! pfmdCO) && (! (pfmdCO = svsutil_AllocFixedMemDescr (sizeof(RFCOMM_CONNECTION_OBJECT), 10)))) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI_RFCOMM] -Init :: ERROR_OUTOFMEMORY - 2\n"));
        return ERROR_OUTOFMEMORY;
    }

    if ((! pfmdSO) && (! (pfmdSO = svsutil_AllocFixedMemDescr (sizeof(RFCOMM_SOCKET_OBJECT), 10)))) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI_RFCOMM] -Init :: ERROR_OUTOFMEMORY - 3\n"));
        return ERROR_OUTOFMEMORY;
    }

    if ((! pfmdSEND) && (! (pfmdSEND = svsutil_AllocFixedMemDescr (sizeof(RFCOMM_SEND_OBJECT), 10)))) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI_RFCOMM] -Init :: ERROR_OUTOFMEMORY - 4\n"));
        return ERROR_OUTOFMEMORY;
    }

    if ((! pfmdRECV) && (! (pfmdRECV = svsutil_AllocFixedMemDescr (sizeof(RFCOMM_RECEIVE_OBJECT), 10)))) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI_RFCOMM] -Init :: ERROR_OUTOFMEMORY - 5\n"));
        return ERROR_OUTOFMEMORY;
    }

    if ((! pfmdPL) && (! (pfmdPL = svsutil_AllocFixedMemDescr (sizeof(RFCOMM_PACKET_OBJECT), 10)))) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI_RFCOMM] -Init :: ERROR_OUTOFMEMORY - 6\n"));
        return ERROR_OUTOFMEMORY;
    }

    if ((! pfmdPN) && (! (pfmdPN = svsutil_AllocFixedMemDescr (sizeof(RFCOMM_PN_OBJECT), 10)))) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI_RFCOMM] -Init :: ERROR_OUTOFMEMORY - 7\n"));
        return ERROR_OUTOFMEMORY;
    }

    RFCOMM_EVENT_INDICATION    ei;
    memset (&ei, 0, sizeof(ei));

    ei.rfcomm_ConnectRequest_Ind = tdir_connect_request_ind;
    ei.rfcomm_DataUp_Ind         = tdir_data_up_ind;
    ei.rfcomm_Disconnect_Ind     = tdir_disconnect_ind;
    ei.rfcomm_FC_Ind             = tdir_fc_ind;
    ei.rfcomm_MSC_Ind            = tdir_msc_ind;
    ei.rfcomm_PNREQ_Ind          = tdir_pnreq_ind;
    ei.rfcomm_RLS_Ind            = tdir_rls_ind;
    ei.rfcomm_RPNREQ_Ind         = tdir_rpnreq_ind;
    ei.rfcomm_StackEvent         = tdir_stack_event;

    RFCOMM_CALLBACKS c;
    memset (&c, 0, sizeof(c));

    c.rfcomm_CallAborted        = tdir_call_aborted;
    c.rfcomm_ConnectRequest_Out = tdir_connect_request_out;
    c.rfcomm_Disconnect_Out     = tdir_disconnect_out;
    c.rfcomm_DataDown_Out       = tdir_data_down_out;
    c.rfcomm_PNREQ_Out          = tdir_pnreq_out;
    c.rfcomm_RPNREQ_Out         = NULL;    //#error Implement me!

    int iErr = RFCOMM_EstablishDeviceContext (this, 0, &ei, &c, &rfcomm_if, &cDeviceHeader,
                                                    &cDeviceTrailer, &hRFCOMM);

    if (iErr != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI_RFCOMM] -Init :: %d\n", iErr));
        return iErr;
    }

    SVSUTIL_ASSERT (hRFCOMM);

    SDP_EVENT_INDICATION sdpEI;
    sdpEI.sdp_StackEvent       = NULL;

    SDP_CALLBACKS sdpC;
    sdpC.sdp_Connect_Out                 = tdisdp_connect_out; 
    sdpC.sdp_Disconnect_Out              = tdisdp_disconnect_out;    
    sdpC.sdp_ServiceSearch_Out           = tdisdp_service_search_out;
    sdpC.sdp_AttributeSearch_Out         = tdisdp_attribute_search_out;
    sdpC.sdp_ServiceAttributeSearch_Out  = tdisdp_service_attribute_search_out;
    sdpC.sdp_CallAborted                 = tdir_call_aborted;

    iErr = SDP_EstablishDeviceContext(this, &sdpEI, &sdpC, &sdp_if,&hSDP);

    if (iErr != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI_RFCOMM] SDP -Init :: %d\n", iErr));
        RFCOMM_CloseDeviceContext(hRFCOMM);
        hRFCOMM = NULL;
        return iErr;
    }

    SVSUTIL_ASSERT(hSDP);

    HCI_EVENT_INDICATION hei;
    memset (&hei, 0, sizeof (hei));

    HCI_CALLBACKS hc;
    memset (&hc, 0, sizeof (hc));
    hc.hci_ReadBDADDR_Out = tdih_read_bdaddr_out;

    int cHciHeaders = 0;
    int cHciTrailers = 0;
    
    iErr = HCI_EstablishDeviceContext (this, BTH_CONTROL_DEVICEONLY, NULL, 0, 0, &hei, &hc, &hci_if, &cHciHeaders, &cHciTrailers, &hHCI);
    if (iErr != ERROR_SUCCESS) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI_RFCOMM] HCI -Init :: %d\n", iErr));
        
        SDP_CloseDeviceContext(hSDP);
        hSDP = NULL;

        RFCOMM_CloseDeviceContext(hRFCOMM);
        hRFCOMM = NULL;
        return iErr;
    }

    SVSUTIL_ASSERT(hHCI);

    fIsInitialized = TRUE;
    GetConnectionState ();

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-Init :: ERROR_SUCCESS\n"));

    return ERROR_SUCCESS;
}

int tdiR_InitializeOnce (void) {
    IFDBG(DebugOut (DEBUG_TDI_INIT, L"+tdiR_InitializeOnce\n"));
    ASSERT (! gpTDIR);

    if (!LoadNdisProtocolVtable())    
        return ERROR_INTERNAL_ERROR;

    gpTDIR = new RFCOMM_TDI;

    if (gpTDIR)
        sdp_LoadWSANameSpaceProvider();

    IFDBG(DebugOut (gpTDIR == NULL ? DEBUG_ERROR : DEBUG_TDI_INIT, L"-tdiR_InitializeOnce (%s)\n", gpTDIR == NULL ? L"failed" : L"success"));
    return (gpTDIR != NULL) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

int tdiR_UninitializeOnce (void) {
    IFDBG(DebugOut (DEBUG_TDI_INIT, L"+tdiR_UninitializeOnce\n"));
    IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] ATTENTION! TDI interface is being uninitialized.\n"));
    IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] If the library is now freed, AFD WILL CRASH.\n"));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"-tdiR_UninitializeOnce :: ERROR_INTERNAL_ERROR\n"));
        return ERROR_INTERNAL_ERROR;
    }

    gpTDIR->Lock ();
    RFCOMM_TDI *pTDIR = gpTDIR;
    gpTDIR = NULL;
    pTDIR->Unlock ();

    SVSUTIL_ASSERT (! pTDIR->fIsInitialized);

    sdp_UnLoadWSANameSpaceProvider();

    delete pTDIR;

    IFDBG(DebugOut (DEBUG_TDI_INIT, L"-tdiR_UninitializeOnce\n"));
    return ERROR_SUCCESS;
}

int tdiR_CreateDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_TDI_INIT, L"+tdiR_CreateDriverInstance\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"-tdiR_CreateDriverInstance :: ERROR_INTERNAL_ERROR\n"));
        return ERROR_INTERNAL_ERROR;
    }

    for ( ; ; ) {
        gpTDIR->Lock ();

        if (gpTDIR->fIsInitialized) {
            IFDBG(DebugOut (DEBUG_ERROR, L"-tdiR_CreateDriverInstance :: ERROR_SERVICE_ALREADY_RUNNING\n"));
            gpTDIR->Unlock ();
            return ERROR_SERVICE_ALREADY_RUNNING;
        }

        if (gpTDIR->GetRefCount () == 1)
            break;

        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Waiting for ref count in tdiR_CreateDriverInstance\n"));
        gpTDIR->Unlock ();

        Sleep (100);
    }

    int iRes = gpTDIR->Init ();

    IFDBG(DebugOut (iRes == ERROR_SUCCESS ? DEBUG_TDI_INIT : DEBUG_ERROR, L"-tdiR_CreateDriverInstance :: %d\n", iRes));
    gpTDIR->Unlock ();
    return iRes;
}

int tdiR_CloseDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_TDI_INIT, L"+tdiR_CloseDriverInstance\n"));
    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"-tdiR_CloseDriverInstance :: ERROR_INTERNAL_ERROR\n"));
        return ERROR_INTERNAL_ERROR;
    }

    for ( ; ; ) {
        gpTDIR->Lock ();

        if (! gpTDIR->fIsInitialized) {
            IFDBG(DebugOut (DEBUG_ERROR, L"-tdiR_CloseDriverInstance :: ERROR_SERVICE_NOT_ACTIVE\n"));
            gpTDIR->Unlock ();
            return ERROR_SERVICE_NOT_ACTIVE;
        }

        if (gpTDIR->GetRefCount() == 1)
            break;

        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Waiting for ref count in tdiR_CloseDriverInstance\n"));
        gpTDIR->Unlock ();
        Sleep (100);
    }

    gpTDIR->fIsInitialized = FALSE;
    gpTDIR->fIsConnected   = FALSE;
    gpTDIR->AddRef ();

    while (gpTDIR->pcolist) {
        RFCOMM_CONNECTION_OBJECT *pC = gpTDIR->pcolist;
        SignalConnectionLoss (pC, TDI_CONNECTION_ABORTED, TRUE);
        if (pC == gpTDIR->pcolist)
            CloseConnectionObject (pC);
    }

    while (gpTDIR->paolist)
        CloseAddressObject (gpTDIR->paolist);

    while (gpTDIR->psolist)
        CloseSocketObject (gpTDIR->psolist);

    while (gpTDIR->pnlist) {
        RFCOMM_PN_OBJECT *pNext = gpTDIR->pnlist->pNext;
        delete gpTDIR->pnlist;
        gpTDIR->pnlist = pNext;
    }

    gpTDIR->Unlock ();
    if (gpTDIR->hRFCOMM)
        RFCOMM_CloseDeviceContext (gpTDIR->hRFCOMM);
    if (gpTDIR->hSDP)
        SDP_CloseDeviceContext(gpTDIR->hSDP);
    gpTDIR->Lock ();
    gpTDIR->DelRef ();
    gpTDIR->reset ();
    IFDBG(DebugOut (DEBUG_TDI_INIT, L"-tdiR_CloseDriverInstance :: ERROR_SUCCESS\n"));
    gpTDIR->Unlock ();

    return ERROR_SUCCESS;
}

//
//    TDI interface
//
////////////////////////////////////////////////////////////////////////////////
//    BTR_OpenAddressEntry()
//        
//    Description:
//        Called after bind(). Allocates a new BT_ADDR_OBJ structure.
//
//    Arguments:
//        pTDIRequest::
//            The address of the new BT_ADDR_OBJ is returned in
//            pTDIReq->Handle.AddressHandle.
//
//        pTransportAddr::
//            Pointer to input TRANSPORT_ADDRESS struct. 
//            We assume one TA_ADDRESS entry with the Address field containing 
//            the remaining bytes of a SOCKADDR_BT struct minus the first two
//            bytes.
//
//        uiProtocol::
//            Ignored/Not used.
//
//        pvOptions::
//            Ignored/Not used.
//
//    Returns:
//        TDI_SUCCESS on success, or tdistat.h
//
//    Comments:
//        Called after winsock's bind() or implicit bind().
//
static TDI_STATUS BTR_OpenAddressEntry
(
PTDI_REQUEST        pTDIRequest,
PTRANSPORT_ADDRESS    pTransportAddr,
uint                uiProtocol,
PVOID                pvOptions
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_OpenAddressEntry\n"));

    SOCKADDR_BTH    *pBTAddr = (SOCKADDR_BTH *)&pTransportAddr->Address[0].AddressType;    
    if (((pBTAddr->port < 0) || (pBTAddr->port > 31)) && (pBTAddr->port != CLIENTPORTSIG)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_OpenAddressEntry : Bluetooth channel %d invalid\n", pBTAddr->port));
        return TDI_BAD_ADDR;
    }

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_OpenAddressEntry : TDI_INVALID_STATE\n"));
        return TDI_INVALID_STATE;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_OpenAddressEntry : TDI_INVALID_STATE\n"));
        gpTDIR->Unlock ();
        return TDI_INVALID_STATE;
    }

    BT_ADDR bt;
    memcpy (&bt, &pBTAddr->btAddr, sizeof(bt));

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_OpenAddressObject %04x%08x 0x%02x\n", GET_NAP(bt), GET_SAP(bt), pBTAddr->port));

    unsigned char cPort = (unsigned char)pBTAddr->port;
    BD_ADDR       b = { GET_NAP(bt), GET_SAP(bt) };
    unsigned int  fAssigned = 0;

    TDI_STATUS TdiStatus = TDI_SUCCESS;

    int fListenOn = FALSE;

    if (pBTAddr->port != CLIENTPORTSIG) {
        fListenOn = TRUE;

        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"[TDI] BTR_OpenAddressEntry : [semi]wildcard address supplied for server, requesting the port %d...\n", cPort));
        //    Reserve/Allocate the port first...

        HANDLE h = gpTDIR->hRFCOMM;
        BT_LAYER_IO_CONTROL pCallback = gpTDIR->rfcomm_if.rfcomm_ioctl;
        gpTDIR->AddRef ();
        gpTDIR->Unlock ();

        int iRes = ERROR_INTERNAL_ERROR;
        TdiStatus = TDI_ADDR_INVALID;

        int iSize = 0;

        __try {
            unsigned char cPortIn = cPort;
            iRes = pCallback (h, BTH_STACK_IOCTL_RESERVE_PORT, sizeof(cPortIn), (char *)&cPortIn, sizeof(cPort), (char *)&cPort, &iSize);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] BTR_OpenAddressEntry Exception in BTH_STACK_IOCTL_RESERVE_PORT\n"));
        }

        gpTDIR->Lock ();
        gpTDIR->DelRef ();

        if (iRes == ERROR_SUCCESS) {
            fAssigned = 1;
            TdiStatus = TDI_SUCCESS;
        } else if (iRes == ERROR_NO_SYSTEM_RESOURCES)    // already assigned
            TdiStatus = TDI_NO_FREE_ADDR;
        else if (iRes == ERROR_ALREADY_ASSIGNED)
            TdiStatus = TDI_ADDR_IN_USE;
        else if (iRes == ERROR_OUTOFMEMORY)
            TdiStatus = TDI_NO_RESOURCES;
        
        if (iRes != ERROR_SUCCESS) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDIR] BTR_OpenAddressEntry returns %d\n", iRes));
            gpTDIR->Unlock ();
            return TdiStatus;
        }
    } else
        cPort = 0;

    RFCOMM_ADDRESS_OBJECT *pA = NULL;
    if (cPort != 0) {    // If it's a wildcard, just create it!
        pA = gpTDIR->paolist;
        while (pA && ((pA->b != b) || (pA->ch != cPort)))
            pA = pA->pNext;
    }

    if (pA) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_OpenAddressEntry Address duplicated!\n"));
        gpTDIR->Unlock ();
        return TDI_ADDR_IN_USE;
    }

    pA = new RFCOMM_ADDRESS_OBJECT;
    if (pA) {
        pA->pNext = gpTDIR->paolist;
        gpTDIR->paolist = pA;

        pA->b          = b;
        pA->ch         = cPort;
        pA->fAssigned  = fAssigned;
        pA->AddRef ();

        pA->fListenOn = fListenOn;

        pTDIRequest->Handle.AddressHandle = pA;
    } else
        TdiStatus = TDI_NO_RESOURCES;


    IFDBG(DebugOut (((TdiStatus == TDI_SUCCESS) || (TdiStatus == TDI_PENDING)) ? DEBUG_TDI_TRACE : DEBUG_ERROR,
                                                                    L"-BTR_OpenAddressEntry :: %d (0x%08x)\n", TdiStatus, pTDIRequest->Handle.AddressHandle));
    gpTDIR->Unlock ();
    return TdiStatus;
}

////////////////////////////////////////////////////////////////////////////////
//    BTR_CloseAddressEntry()
//        
//    Description:
//        Called at socket close time after BT_DisconnectEntry() and 
//        BT_CloseConnectionEntry(). Frees the BT_ADDR_OBJ.
//        
//    Arguments:
//            pTDIRequest->Handle.AddressHandle is a handle to the
//            BT_ADDR_OBJ.   BT_OpenAddressEntry() assigns this value to be
//            a pointer to the BT_ADDR_OBJ.
//
//    Returns:
//        TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
//        TDI_ADDR_INVALID on Bad address object.
//
//    Comments:
//
//
static TDI_STATUS BTR_CloseAddressEntry
(
PTDI_REQUEST        pTDIRequest
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_CloseAddressEntry 0x%08x\n", pTDIRequest->Handle.AddressHandle));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_CloseAddressEntry : TDI_INVALID_STATE\n"));
        return TDI_INVALID_STATE;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_CloseAddressEntry : TDI_INVALID_STATE\n"));
        gpTDIR->Unlock ();
        return TDI_INVALID_STATE;
    }

    TDI_STATUS TdiStatus = TDI_SUCCESS;

    RFCOMM_ADDRESS_OBJECT *pA = GetAddressObject (pTDIRequest->Handle.AddressHandle);
    if (pA) {
        pA->DelRef ();
        pA->fListenOn = FALSE;

        if (pA->GetRefCount () == 1)
            CloseAddressObject (pA);
    } else
        TdiStatus = TDI_NO_RESOURCES;

    IFDBG(DebugOut (((TdiStatus == TDI_SUCCESS) || (TdiStatus == TDI_PENDING)) ? DEBUG_TDI_TRACE : DEBUG_ERROR,
                                                                    L"-BTR_CloseAddressEntry :: %d (0x%08x)\n", TdiStatus, pTDIRequest->Handle.AddressHandle));
    gpTDIR->Unlock ();
    return TdiStatus;
}

////////////////////////////////////////////////////////////////////////////////
//    BTR_OpenConnectionEntry()
//        
//    Description:
//        Called after connect() and backlog times after listen(). Allocates 
//        a new BT_CONN_OBJ.
//        
//    Arguments:
//        pTDIRequest::
//            pTDIRequest->Handle.ConnectionContext returns the address of
//            the new BT_CONN_OBJ.
//
//        pvContext::
//            To be used later in the future calls to:
//            o  pEventDisconnect()
//            o  pEventReceive()
//
//    Returns:
//        TDI_SUCCESS on success, or error in tdistat.h
//
//    Comments:
//        Called after connect()
//
static TDI_STATUS BTR_OpenConnectionEntry
(
PTDI_REQUEST        pTDIRequest,
PVOID                pvContext
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_OpenConnectionEntry\n"));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_OpenConnectionEntry : TDI_INVALID_STATE\n"));
        return TDI_INVALID_STATE;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_OpenConnectionEntry : TDI_INVALID_STATE\n"));
        gpTDIR->Unlock ();
        return TDI_INVALID_STATE;
    }

    TDI_STATUS TdiStatus = TDI_SUCCESS;

    RFCOMM_CONNECTION_OBJECT *pC = new RFCOMM_CONNECTION_OBJECT;
    if (pC) {
        pC->hCallContext = btutil_AllocHandle ((LPVOID)pC);
        if (SVSUTIL_HANDLE_INVALID != pC->hCallContext) {
            pC->pConnectionContext = pvContext;
            pC->eStage = INITED;
            pC->pNext = gpTDIR->pcolist;
            gpTDIR->pcolist = pC;
            pTDIRequest->Handle.ConnectionContext = pC;

            RFCOMM_SOCKET_OBJECT *pSock = GetSocketByHandle (pvContext);

            if (pSock)
                TransferOptions (pC, pSock);
        }
        else {
            TdiStatus = TDI_NO_RESOURCES;        
        }
    } else {
        TdiStatus = TDI_NO_RESOURCES;
    }

    IFDBG(DebugOut (((TdiStatus == TDI_SUCCESS) || (TdiStatus == TDI_PENDING)) ? DEBUG_TDI_TRACE : DEBUG_ERROR,
                                                                    L"-BTR_OpenConnectionEntry :: %d (0x%08x)\n", TdiStatus, pTDIRequest->Handle.ConnectionContext));
    gpTDIR->Unlock ();
    return TdiStatus;
}

////////////////////////////////////////////////////////////////////////////////
//    BT_CloseConnectionEntry()
//        
//    Description:
//        Called at socket close time. If the connection is in a connected
//        state, this call implies an abortive close - force close the 
//        connection and free the PBT_CONN_OBJ. 
//        If the connection is not in the connected state, the DisconnectEntry()
//        been called and we simply free PBT_CONN_OBJ.
//        
//    Arguments:
//        pTDIRequest::
//            pTDIRequest->Handle.ConnectionContext contains the address to
//            the BT_CONN_OBJ structure.
//    Returns:
//        TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
//        TDI_INVALID_CONNECTION for bad connection.
//
//    Comments: This will always complete immediately, which means that the next call
//   to connect may fail because channel has not disconnected at the RFCOMM layer.
//   We might want to think about returning TDI_PENDING here and really closing connection
//   later.
//
static TDI_STATUS BTR_CloseConnectionEntry
(
PTDI_REQUEST        pTDIRequest
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_CloseConnectionEntry 0x%08x\n", pTDIRequest->Handle.ConnectionContext));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_CloseConnectionEntry : TDI_INVALID_STATE\n"));
        return TDI_INVALID_STATE;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_CloseConnectionEntry : TDI_INVALID_STATE\n"));
        gpTDIR->Unlock ();
        return TDI_INVALID_STATE;
    }

    TDI_STATUS TdiStatus = TDI_SUCCESS;

    RFCOMM_CONNECTION_OBJECT *pC = GetConnectionObject (pTDIRequest->Handle.ConnectionContext);
    if (pC) {
        DisassociateConnection (pC);    // This precludes signal up the stack.

        if (gpTDIR->fIsInitialized && (pC == GetConnectionObject ((HANDLE)pC)))
            SignalConnectionLoss (pC, TDI_GRACEFUL_DISC, TRUE);

        if (gpTDIR->fIsInitialized && (pC == GetConnectionObject ((HANDLE)pC)))
            CloseConnectionObject (pC);
    } else
        TdiStatus = TDI_INVALID_CONNECTION;

    IFDBG(DebugOut (((TdiStatus == TDI_SUCCESS) || (TdiStatus == TDI_PENDING)) ? DEBUG_TDI_TRACE : DEBUG_ERROR,
                                                                    L"-BTR_CloseConnectionEntry :: %d (0x%08x)\n", TdiStatus, pTDIRequest->Handle.ConnectionContext));
    gpTDIR->Unlock ();
    return TdiStatus;
}

////////////////////////////////////////////////////////////////////////////////
//    BT_AssociateAddressEntry()
//        
//    Description:
//        Called after connect() and backlog times after listen(). 
//        Assigns pAddrObj in a newly created BT_CONN_OBJ to point
//        to an BT_ADDR_OBJ and adds the BT_CONN_OBJ to pConnList
//        anchored on BT_ADDR_OBJ. 
//        SockAddrLocal is copied from the BT_ADDR_OBJ to the BT_CONN_OBJ
//        to handle the the IsConn option of BT_QueryInformation().
//
//    Arguments:
//        pTDIRequest::
//            pTDIRequest->Handle.ConnectionContext contains the 
//            CONNECTION_CONTEXT of the BT_CONN_OBJ. 
//            BT_OpenConnection() assigns this value to be a pointer to
//            the BT_CONN_OBJ.
//
//        hAddrHandle::
//            Extract the BT_ADDR_OBJ from this pointer.
//
//    Returns:
//        TDI_SUCCESS on success, or error in tdistat.h
//
//    Comments:
//        Called after connect() and backlog times after listen()
//
static TDI_STATUS BTR_AssociateAddressEntry
(
PTDI_REQUEST        pTDIRequest,
HANDLE                hAddrHandle
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_AssociateAddressEntry addr: 0x%08x conn: 0x%08x\n", hAddrHandle, pTDIRequest->Handle.ConnectionContext));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_AssociateAddressEntry : TDI_INVALID_STATE\n"));
        return TDI_INVALID_STATE;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_AssociateAddressEntry : TDI_INVALID_STATE\n"));
        gpTDIR->Unlock ();
        return TDI_INVALID_STATE;
    }

    TDI_STATUS TdiStatus = TDI_SUCCESS;

    RFCOMM_ADDRESS_OBJECT    *pA = GetAddressObject    (hAddrHandle);
    RFCOMM_CONNECTION_OBJECT *pC = GetConnectionObject (pTDIRequest->Handle.ConnectionContext);
    if (! pA)
        TdiStatus = TDI_ADDR_INVALID;
    else if (! pC)
        TdiStatus = TDI_INVALID_CONNECTION;
    else {
        pA->AddRef ();
        pC->pAddr = pA;

        ResetConnection (pC, TRUE);
        TransferOptions (pC, pC->pAddr);
    }

    IFDBG(DebugOut (((TdiStatus == TDI_SUCCESS) || (TdiStatus == TDI_PENDING)) ? DEBUG_TDI_TRACE : DEBUG_ERROR,
                                                                    L"-BTR_AssociateAddressEntry :: %d\n", TdiStatus));
    gpTDIR->Unlock ();
    return TdiStatus;
}

////////////////////////////////////////////////////////////////////////////////
//    BT_DisAssociateAddressEntry()
//        
//    Description:
//
//
//        
//    Arguments:
//
//    Returns:
//
//    Comments:
//
//
static TDI_STATUS BTR_DisAssociateAddressEntry
(
PTDI_REQUEST        pTDIRequest
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_DisAssociateAddressEntry 0x%08x\n", pTDIRequest->Handle.ConnectionContext));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_DisAssociateAddressEntry : TDI_INVALID_STATE\n"));
        return TDI_INVALID_STATE;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_DisAssociateAddressEntry : TDI_INVALID_STATE\n"));
        gpTDIR->Unlock ();
        return TDI_INVALID_STATE;
    }

    TDI_STATUS TdiStatus = TDI_SUCCESS;

    RFCOMM_CONNECTION_OBJECT *pC = GetConnectionObject (pTDIRequest->Handle.ConnectionContext);
    if (pC)
        DisassociateConnection (pC);
    else
        TdiStatus = TDI_INVALID_CONNECTION;

    IFDBG(DebugOut (((TdiStatus == TDI_SUCCESS) || (TdiStatus == TDI_PENDING)) ? DEBUG_TDI_TRACE : DEBUG_ERROR,
                                                                    L"-BTR_DisAssociateAddressEntry :: %d (0x%08x)\n", TdiStatus, pTDIRequest->Handle.ConnectionContext));
    gpTDIR->Unlock ();
    return TdiStatus;
}

////////////////////////////////////////////////////////////////////////////////
//    BT_ConnectEntry()
//        
//    Description:
//        Called after connect().   
//        Initiates an BT connection. 
//        BT should call ConnectComplete() when the connection is established.
//        
//    Arguments:
//        pTDIRequest::
//            pTDIRequest->Handle.ConnectionContext contains the 
//            CONNECTION_CONTEXT (tdi.h, PVOID) of the BT_CONN_OBJ. 
//            BT_OpenConnection() assigns this value to be a pointer to
//            the BT_CONN_OBJ.
//
//            pTDIReq->RequestNotifyObject is a pointer 
//            (void (*)(PVOID Context, TDI_STATUS FinalStatus, 
//            unsigned long ByteCount)) 
//            to a Winsock routine to call if this function returns TDI_PENDING.
//
//            pTDIReq->RequestNotifyContext is a Winsock context to return in
//            the callback.
//
//        pvTimeOut::
//            sockutil.c appears to always set this -1.   We will ignore it.
//
//        pReqAddr::
//            Pointer to input TDI_CONNECTION_INFORMATION (tdi.h) struct.
//            BT assumes RemoteAddress points to a TRANSPORT_ADDRESS (tdi.h)
//            struct. BT assumes one TA_ADDRESS entry with the Address field
//            containing the remaining bytes of a SOCKADDR_BT struct minus the
//            first two bytes.
//
//        pRetAddr::
//            Pointer to output TDI_CONNECTION_INFORMATION (tdi.h) struct.
//
//
//    Returns:
//        TDI_SUCCESS on success, or tdistat.h
//
//    Comments:
//
//
static TDI_STATUS BTR_ConnectEntry
(
PTDI_REQUEST                    pTDIRequest,
PVOID                            pvTimeOut,
PTDI_CONNECTION_INFORMATION        pReqAddr,
PTDI_CONNECTION_INFORMATION        pRetAddr
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_ConnectEntry 0x%08x\n", pTDIRequest->Handle.ConnectionContext));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_ConnectEntry : TDI_INVALID_STATE\n"));
        return TDI_INVALID_STATE;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_ConnectEntry : TDI_INVALID_STATE\n"));
        gpTDIR->Unlock ();
        return TDI_INVALID_STATE;
    }

    TDI_STATUS TdiStatus = TDI_SUCCESS;

    RFCOMM_CONNECTION_OBJECT *pC = GetConnectionObject (pTDIRequest->Handle.ConnectionContext);
    if (pC) {
        SVSUTIL_ASSERT (pC->pAddr);
        SVSUTIL_ASSERT (pC->eStage == INITED);
        if (pC->pAddr->fListenOn) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_ConnectEntry : Cannot connect on a bound socket\n"));
            gpTDIR->Unlock ();
            return TDI_INVALID_STATE;
        }

        // Address MUST be wildcard at this point, otherwise we are using server address
        // or existing connection.
        SVSUTIL_ASSERT ((pC->pAddr->b.NAP == 0) && (pC->pAddr->b.SAP == 0) && (pC->pAddr->ch == 0)); // Wildcard

        SOCKADDR_BTH *pBTAddr = (SOCKADDR_BTH *) &(((PTRANSPORT_ADDRESS) pReqAddr->RemoteAddress)->Address[0].AddressType);

        pC->eStage = OPENING;
        pC->pConnectCompleteContext = pTDIRequest->RequestContext;
        pC->pConnectCompleteEvent   = (PBT_CONNECT_COMPLETE)pTDIRequest->RequestNotifyObject;
        pC->fNoDisconnect           = TRUE;
        memcpy (&pC->BTAddr, pBTAddr, sizeof(pC->BTAddr));

        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Connect to %04x%08x 0x%02d\n", GET_NAP(pC->BTAddr.btAddr), GET_SAP(pC->BTAddr.btAddr), pBTAddr->port));

        int iErr;
        if (pBTAddr->port)
            iErr = ConnectEntryRFCOMM(pC);
        else
            iErr = ConnectEntrySDP(pC);

        if (iErr != ERROR_SUCCESS) {
            switch (iErr) {
            case ERROR_OPERATION_ABORTED:
                TdiStatus = TDI_REQ_ABORTED;
                break;
            case ERROR_INTERNAL_ERROR:
                TdiStatus = TDI_NOT_ACCEPTED;
                break;
            case ERROR_ALREADY_EXISTS:
                TdiStatus = TDI_LINK_BUSY;
                break;
            default:
                SVSUTIL_ASSERT (0);    // invent the case above for this error!
                TdiStatus = TDI_CONNECTION_ABORTED;
            };
        } else {
            //Refcount=1 in constructor, =2 in OpenAddressEntry, =3 in AssociateAddressEntry
            //If it is three, AssociateAddressEntry has not been called more than once.
            SVSUTIL_ASSERT(3 == pC->pAddr->GetRefCount());
            // In high-contention environments the connection might complete before we got here.
            // We need to signal connection then. However, before we returned "pending" we don't
            // know if we can call that event. Instead of signalling with pConnectComplete event,
            // we now have separate flag.
            //pC->pConnectCompleteEvent   = (PBT_CONNECT_COMPLETE)pTDIRequest->RequestNotifyObject;

            pC->fNoDisconnect = FALSE;
            pC->pAddr->ch = (unsigned char)pBTAddr->port;
            TdiStatus = TDI_PENDING;
        }
    } else
        TdiStatus = TDI_INVALID_CONNECTION;

    IFDBG(DebugOut (((TdiStatus == TDI_SUCCESS) || (TdiStatus == TDI_PENDING)) ? DEBUG_TDI_TRACE : DEBUG_ERROR,
                                                                    L"-BTR_ConnectEntry :: %d (0x%08x)\n", TdiStatus, pTDIRequest->Handle.ConnectionContext));
    gpTDIR->Unlock ();
    return TdiStatus;
}


////////////////////////////////////////////////////////////////////////////////
//    BT_DisconnectEntry()
//        
//    Description:
//        Called at socket close time. 
//        We will call BTDisconnect() to complete the disconnect. 
//        It appears that only graceful closes use this mechanism. 
//        For abortive closes, Winsock calls only BT_CloseConnectionEntry().
//
//    Arguments:
//        pTdiRequest::
//            pTDIRequest->Handle.ConnectionContext contains the 
//            CONNECTION_CONTEXT (tdi.h, PVOID) of the BT_CONN_OBJ. 
//            BT_OpenConnection() assigns this value to be a pointer to
//            the BT_CONN_OBJ.
//
//        pvTimeOut::
//            Ignored.
//
//        usFlags::
//            Ignored.
//
//        pDiscInfo::
//            Ignored.
//
//        pRetInfo::
//            Ignored.
//
//
//
//    Returns:
//        TDI_SUCCESS, TDI_INVALID_CONNECTION, TDI_INVALID_STATE
//
//    Comments:
//
//    OmarM says signaling unless TDI_PENDING is unnecessary:
//    PBT_DISCONNECT_COMPLETE pDisconnectCompleteCallback = NULL;
//    void                    *pDisconnectCompleteContext = NULL;
//        pDisconnectCompleteCallback = (PBT_DISCONNECT_COMPLETE)pTDIRequest->RequestNotifyObject;
//        pDisconnectCompleteContext = pTDIRequest->RequestContext;    
//    if (pDisconnectCompleteCallback) {
//        pDisconnectCompleteCallback (pDisconnectCompleteContext, TdiStatus, 0);
//        TdiStatus = TDI_SUCCESS;
//    }
//
static TDI_STATUS BTR_DisconnectEntry
(
PTDI_REQUEST                    pTDIRequest,
PVOID                            pvTimeOut,
ushort                            usFlags,
PTDI_CONNECTION_INFORMATION        pDiscInfo,
PTDI_CONNECTION_INFORMATION        pRetInfo
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_DisconnectEntry 0x%08x\n", pTDIRequest->Handle.ConnectionContext));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_DisconnectEntry : TDI_INVALID_STATE\n"));
        return TDI_INVALID_STATE;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_DisconnectEntry : TDI_INVALID_STATE\n"));
        gpTDIR->Unlock ();
        return TDI_SUCCESS;
    }

    TDI_STATUS TdiStatus = TDI_SUCCESS;

    RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionObject (pTDIRequest->Handle.ConnectionContext);
    if (pConn && (pConn->eStage == OPENED)) {
        if ((usFlags & TDI_DISCONNECT_ABORT) || (pConn->pSendList == NULL)) {
            if (ERROR_SUCCESS != DisconnectConnection (NULL, pConn->hConnect, pConn)) {
                if (gpTDIR->fIsConnected && (pConn == GetConnectionObject (pTDIRequest->Handle.ConnectionContext)))
                    SignalConnectionLoss (pConn, TDI_GRACEFUL_DISC, TRUE);
            }
        } else
            pConn->fCloseHaveSend = TRUE;
    } else
        TdiStatus = TDI_INVALID_STATE;

    IFDBG(DebugOut (((TdiStatus == TDI_SUCCESS) || (TdiStatus == TDI_PENDING)) ? DEBUG_TDI_TRACE : DEBUG_ERROR,
                                                                    L"-BTR_DisconnectEntry :: %d (0x%08x)\n", TdiStatus, pTDIRequest->Handle.ConnectionContext));
    gpTDIR->Unlock ();

    return TdiStatus;
}


////////////////////////////////////////////////////////////////////////////////
//    BTR_ListenEntry()
//        
//    Description:
//
//
//        
//    Arguments:
//
//    Returns:
//
//    Comments: NEVER CALLED
//
//
static TDI_STATUS BTR_ListenEntry
(
PTDI_REQUEST                    pTDIRequest,
ushort                            usFlags,
PTDI_CONNECTION_INFORMATION        pAcceptableInfo,
PTDI_CONNECTION_INFORMATION        pConnectedInfo
) {
    SVSUTIL_ASSERT (0);
    return TDI_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
//    BTR_AcceptEntry()
//        
//    Description:
//
//
//        
//    Arguments:
//
//    Returns:
//
//    Comments: NEVER CALLED
//
//
static TDI_STATUS BTR_AcceptEntry
(
PTDI_REQUEST                    pTDIRequest,
PTDI_CONNECTION_INFORMATION        pAcceptInfo,
PTDI_CONNECTION_INFORMATION        pConnectedInfo
) {
    SVSUTIL_ASSERT (0);
    return TDI_SUCCESS;
}


////////////////////////////////////////////////////////////////////////////////
//    BTR_ReceiveEntry()
//        
//    Description:
//        Called to recv data driectly into a user buffer or to signal
//        a flow off condition.
//        
//    Arguments:
//        pTDIRequest::
//            pTDIReq->Handle.ConnectionContext contains the CONNECTION_CONTEXT 
//            of the BT_CONN_OBJ.   BT_OpenConnectionEntry() assigns this 
//            value to be a pointer to the BT_CONN_OBJ.
//    
//            pTDIReq->RequestNotifyObject is a pointer 
//            (void (*)(PVOID Context, TDI_STATUS TdiStatus, DWORD BytesSent))
//            to a Winsock routine to call if this function returns TDI_PENDING.
//            TDIReq->RequestNotifyContext is a Winsock context to return in
//            the callback.
//
//    Returns:
//        TDI_SUCCESS on success, or tdistat.h
//
//    Comments:
//
static TDI_STATUS BTR_ReceiveEntry
(
PTDI_REQUEST        pTDIRequest,
ushort                *pusFlags,
uint                *puiRecvLen, 
PNDIS_BUFFER        pNdisBuffer
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_ReceiveEntry 0x%08x\n", pTDIRequest->Handle.ConnectionContext));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_ReceiveEntry : WSANOTINITIALISED\n"));
        return TDI_INVALID_STATE;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_ReceiveEntry : WSANOTINITIALISED\n"));
        gpTDIR->Unlock ();
        return TDI_INVALID_STATE;
    }

    TDI_STATUS        TdiStatus = TDI_SUCCESS;
    *puiRecvLen = 0;

    RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionObject (pTDIRequest->Handle.ConnectionContext);
    if (pConn && ((pConn->eStage == OPENED) || pConn->fCloseHaveRecv)) {
        SVSUTIL_ASSERT (pConn->pAddr);

        if (pConn->pRecvList && pConn->pRecvList->pBuffer) {
            PVOID    VirtualAddress;
            UINT    BufferLength;

            //    Have data, will travel.

            int cBufferSize = QueryNdisSize (pNdisBuffer);
            int cNdisCurrentOffset = 0;
            for ( ; ; ) {
                int cDataSize = BufferTotal (pConn->pRecvList->pBuffer);
                NdisQueryBuffer(pNdisBuffer, &VirtualAddress, &BufferLength);
                if ((int)BufferLength - cNdisCurrentOffset < cDataSize)
                    cDataSize = (int)BufferLength - cNdisCurrentOffset;

                __try {
                    BufferGetChunk (pConn->pRecvList->pBuffer, cDataSize,
                            ((unsigned char *)VirtualAddress) + cNdisCurrentOffset);
                } __except (1) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_ReceiveEntry : Exception copying to TDI buffer\n"));
                    *puiRecvLen = 0;
                    gpTDIR->Unlock ();
                    return TDI_BUFFER_TOO_SMALL;
                }
                cNdisCurrentOffset += cDataSize;
                *puiRecvLen += cDataSize;

                SVSUTIL_ASSERT (cNdisCurrentOffset <= (int)BufferLength);
                if (cNdisCurrentOffset == (int)BufferLength) {
                    NdisGetNextBuffer(pNdisBuffer, &pNdisBuffer);
                    cNdisCurrentOffset = 0;
                }

                if (BufferTotal (pConn->pRecvList->pBuffer) == 0) {
                    if (pConn->pRecvList->pBuffer->pFree)
                        pConn->pRecvList->pBuffer->pFree (pConn->pRecvList->pBuffer);
                    RFCOMM_RECEIVE_OBJECT *pNext = pConn->pRecvList->pNext;
                    delete pConn->pRecvList;
                    pConn->pRecvList = pNext;
                }

                if ((! pConn->pRecvList) || (! pNdisBuffer))
                    break;
            }

            IFDBG(DebugOut (DEBUG_TDI_TRACE, L"BTR_ReceiveEntry :: read %d bytes\n", *puiRecvLen));

            pConn->cbDataIndicated -= (int)*puiRecvLen;
            if (pConn->cbDataIndicated < 0)
                pConn->cbDataIndicated = 0;

            //    Now, we need to indicate the data left...
            int cDataLeft = 0;
            RFCOMM_RECEIVE_OBJECT *pRecvList = pConn->pRecvList;
            while (pRecvList) {
                SVSUTIL_ASSERT (pRecvList->pBuffer);
                cDataLeft += BufferTotal (pRecvList->pBuffer);
                pRecvList = pRecvList->pNext;
            }

            SVSUTIL_ASSERT (cDataLeft >= 0);

            pConn->irecvd = cDataLeft;

            if (cDataLeft && (pConn->cbDataIndicated == 0)) {
                pConn->cbDataIndicated = cDataLeft;
                EventRcvBuffer     EventRecvCompleteInfo;    
                int                BytesTaken = 0;

                void *pEventReceiveContext = pConn->pAddr->pEventReceiveContext;
                void *pConnectionContext = pConn->pConnectionContext;
                PRcvEvent pEventReceive = pConn->pAddr->pEventReceive;

                gpTDIR->AddRef ();
                gpTDIR->Unlock ();

                TDI_STATUS TdiStatus2 = pEventReceive (pEventReceiveContext, pConnectionContext,
                    0, cDataLeft, cDataLeft, (unsigned int *)&BytesTaken, NULL, &EventRecvCompleteInfo);

                gpTDIR->Lock ();
                gpTDIR->DelRef ();

                SVSUTIL_ASSERT (TdiStatus2 == TDI_NOT_ACCEPTED);
                SVSUTIL_ASSERT (BytesTaken == 0);
                IFDBG(DebugOut (DEBUG_TDI_TRACE, L"BTR_ReceiveEntry :: still have %d bytes, indicated\n", cDataLeft));
            } else if (! cDataLeft) {
                SVSUTIL_ASSERT (pConn->cbDataIndicated == 0);
                IFDBG(DebugOut (DEBUG_TDI_TRACE, L"BTR_ReceiveEntry :: all read, nothing left\n"));

                    // No more data to be read and socket has been closed.

                if (pConn->fCloseHaveRecv)
                    SignalConnectionLoss(pConn, TDI_GRACEFUL_DISC, TRUE);
            }

            if (pConn == GetConnectionObject (pTDIRequest->Handle.ConnectionContext)) {
                if ((pConn->irecvd < pConn->xoff_lim) && pConn->fc_local && (! pConn->fc_credit)) {
                    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"Switching flow back on for connection 0x%08x\n", pConn));

                    pConn->fc_local = FALSE;
                    SendMSC (pConn, TDIR_V24_RTC|TDIR_V24_RTR, 0xff);
                } else if (pConn->fc_credit)
                    SendCredits (pConn);
            }
        } else if (pConn->fCloseHaveRecv) {
            // No more data to be read and socket has been closed.
            SignalConnectionLoss(pConn, TDI_GRACEFUL_DISC, TRUE);
        } else {
            RFCOMM_RECEIVE_OBJECT *pRecv = new RFCOMM_RECEIVE_OBJECT;
            IFDBG(DebugOut (DEBUG_TDI_TRACE, L"BTR_ReceiveEntry queueing request...\n"));
            if (pRecv) {
                memset (pRecv, 0, sizeof(*pRecv));
                pRecv->pNdisBuffer  = pNdisBuffer;
                pRecv->dwPerms      = GetCurrentPermissions ();
                pRecv->pRecvEvent   = (PBT_RECV_COMPLETE)pTDIRequest->RequestNotifyObject;
                pRecv->pRecvContext = pTDIRequest->RequestContext;

                if (pConn->pRecvList) {
                    RFCOMM_RECEIVE_OBJECT *pROP = pConn->pRecvList;

                    while (pROP->pNext)
                        pROP = pROP->pNext;

                    pROP->pNext = pRecv;
                } else
                    pConn->pRecvList = pRecv;
                TdiStatus = TDI_PENDING;
            } else {
                IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_ReceiveEntry No memory!\n"));
                TdiStatus = TDI_NO_RESOURCES;
            }
        }

    } else
        TdiStatus = TDI_INVALID_STATE;

    IFDBG(DebugOut (((TdiStatus == TDI_SUCCESS) || (TdiStatus == TDI_PENDING)) ? DEBUG_TDI_TRACE : DEBUG_ERROR,
                                                                    L"-BTR_ReceiveEntry :: %d (0x%08x)\n", TdiStatus, pTDIRequest->Handle.ConnectionContext));
    gpTDIR->Unlock ();
    return TdiStatus;
}


////////////////////////////////////////////////////////////////////////////////
//    BTR_SendEntry()
//        
//    Description:
//        Called to send data.
//        
//    Arguments:
//        pTDIRequest::
//            pTDIReq->Handle.ConnectionContext contains the CONNECTION_CONTEXT 
//            of the BT_CONN_OBJ.   BT_OpenConnectionEntry() assigns this 
//            value to be a pointer to the BT_CONN_OBJ.
//    
//            pTDIReq->RequestNotifyObject is a pointer 
//            (void (*)(PVOID Context, TDI_STATUS TdiStatus, DWORD BytesSent))
//            to a Winsock routine to call if this function returns TDI_PENDING.
//            TDIReq->RequestNotifyContext is a Winsock context to return in
//            the callback.
//
//        usFlags::
//            One of the:
//            TDI_SEND_EXPEDITED
//            TDI_SEND_PARTIAL
//            TDI_SEND_NO_RESPONSE_EXPECTED
//            TDI_SEND_NON_BLOCKING
//
//        uiSendLen::
//
//        pNdisBuff::
//            Pointer to input NDIS_BUFFER
//
//    Returns:
//        TDI_SUCCESS on success, or tdistat.h
//
//    Comments:
//
//
static TDI_STATUS BTR_SendEntry
(
PTDI_REQUEST        pTDIRequest,
ushort                usFlags,
uint                uiSendLen,
PNDIS_BUFFER        pNdisBuff
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_SendEntry 0x%08x\n", pTDIRequest->Handle.ConnectionContext));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_SendEntry : WSANOTINITIALISED\n"));
        return TDI_INVALID_STATE;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_SendEntry : WSANOTINITIALISED\n"));
        gpTDIR->Unlock ();
        return TDI_INVALID_STATE;
    }

    TDI_STATUS        TdiStatus = TDI_SUCCESS;

    RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionObject (pTDIRequest->Handle.ConnectionContext);
    if (pConn && (pConn->eStage == OPENED)) {
        if (! pConn->fCloseHaveSend) {
            RFCOMM_SEND_OBJECT *pS = new RFCOMM_SEND_OBJECT;
            if (pS) {
                SVSUTIL_ASSERT (pS->pNext == NULL);
                SVSUTIL_ASSERT (pS->cBytesUsed == 0);

                pS->pConnect = pConn;
                pS->pNdisBuffer = pNdisBuff;
                pS->pSendCallback = (PBT_SEND_COMPLETE)pTDIRequest->RequestNotifyObject;
                pS->pSendContext = pTDIRequest->RequestContext;

                if (pConn->pSendList) {
                    RFCOMM_SEND_OBJECT *pP = pConn->pSendList;
                    while (pP->pNext)
                        pP = pP->pNext;

                    pP->pNext = pS;
                } else
                    pConn->pSendList = pS;

                SendData (pConn);
                TdiStatus = TDI_PENDING;
            } else
                TdiStatus = TDI_NO_RESOURCES;
        } else
            TdiStatus = TDI_INVALID_STATE;
    } else
        TdiStatus = TDI_INVALID_STATE;

    IFDBG(DebugOut (((TdiStatus == TDI_SUCCESS) || (TdiStatus == TDI_PENDING)) ? DEBUG_TDI_TRACE : DEBUG_ERROR,
                                                                    L"-BTR_SendEntry :: %d (0x%08x)\n", TdiStatus, pTDIRequest->Handle.ConnectionContext));
    gpTDIR->Unlock ();
    return TdiStatus;
}


////////////////////////////////////////////////////////////////////////////////
//    BTR_SendDatagramEntry()
//        
//    Description:
//
//
//        
//    Arguments:
//
//    Returns:
//
//    Comments: NEVER CALLED
//
//
static TDI_STATUS BTR_SendDatagramEntry
(
PTDI_REQUEST                    pTDIRequest,
PTDI_CONNECTION_INFORMATION        pConnInfo,
uint                            uiSendLen,
ULONG                            *puiBytesSent,
PNDIS_BUFFER                    pNdisBuff
) {
    SVSUTIL_ASSERT (0);
    return TDI_SUCCESS;
}


////////////////////////////////////////////////////////////////////////////////
//    BTR_ReceiveDatagramEntry()
//        
//    Description:
//
//
//        
//    Arguments:
//
//    Returns:
//
//    Comments: NEVER CALLED
//
//
static TDI_STATUS BTR_ReceiveDatagramEntry
(
PTDI_REQUEST                    pTDIRequest,
PTDI_CONNECTION_INFORMATION        pConnInfo,
PTDI_CONNECTION_INFORMATION        pRetInfo,
uint                            uiRecvLen,
uint                            *puiBytesRecvd,
PNDIS_BUFFER                    pNdisBuff
) {
    SVSUTIL_ASSERT (0);
    return TDI_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
//    BTR_SetEventEntry()
//        
//    Description:
//        Informs the stack of each Winsock entry point (event handler).
//        Called after bind() for each event handler except
//        TDI_EVENT_CONNECT. Called for TDI_EVENT_CONNECT after listen().
//        Each Address Object can have unique event handlers. 
//        
//    Arguments:
//        pvHandle::
//            Pointer to the BT_ADDR_OBJ struct that will be associated with
//            this event handler. 
//
//        iType::
//
//        pvHandler::
//
//        pvContext::
//
//    Returns:
//        TDI_SUCCESS  on success, or tdistat.h
//
//    Comments:
//
//
static TDI_STATUS BTR_SetEventEntry
(
PVOID                pvHandle,
int                    iType,
PVOID                pvHandler,
PVOID                pvContext
) {
    TDI_STATUS      TdiStatus = TDI_SUCCESS;

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"BTR_SetEventEntry 0x%08x :: for [%s] ptr 0x%08x, ctx 0x%08x\n", pvHandle,
        (iType == TDI_EVENT_CONNECT)            ?    L"EventConnect" :
        (iType == TDI_EVENT_DISCONNECT)            ?    L"EventDisconnect" :
        (iType == TDI_EVENT_ERROR)                ?    L"EventError" :
        (iType == TDI_EVENT_RECEIVE)            ?    L"EventReceive" :
        (iType == TDI_EVENT_RECEIVE_DATAGRAM)    ?    L"EventRxDatagram" :
        (iType == TDI_EVENT_RECEIVE_EXPEDITED)    ?    L"EventRxEpd" :
        L"Unsupported!", pvHandler, pvContext));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_SetEventEntry : WSANOTINITIALISED\n"));
        return TDI_INVALID_STATE;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_SetEventEntry : WSANOTINITIALISED\n"));
        gpTDIR->Unlock ();
        return TDI_INVALID_STATE;
    }

    RFCOMM_ADDRESS_OBJECT *pAddrObj = GetAddressObject (pvHandle);

    if (pAddrObj) {
        switch (iType)
        {
          case TDI_EVENT_CONNECT:        
            pAddrObj->pEventConnect                 = (PConnectEvent)pvHandler;
            pAddrObj->pEventConnectContext          = pvContext;
            break;

          case TDI_EVENT_DISCONNECT:
            pAddrObj->pEventDisconnect              = (PDisconnectEvent)pvHandler;
            pAddrObj->pEventDisconnectContext       = pvContext;
            break;

          case TDI_EVENT_ERROR:
            pAddrObj->pEventError                   = (PErrorEvent)pvHandler;
            pAddrObj->pEventDisconnectContext       = pvContext;
            break;

          case TDI_EVENT_RECEIVE:
            pAddrObj->pEventReceive                 = (PRcvEvent)pvHandler;
            pAddrObj->pEventReceiveContext          = pvContext;
            break;

          case TDI_EVENT_RECEIVE_DATAGRAM:
            pAddrObj->pEventReceiveDatagram         = (PRcvDGEvent)pvHandler;
            pAddrObj->pEventReceiveDatagramContext  = pvContext;
            break;

          case TDI_EVENT_RECEIVE_EXPEDITED:
            pAddrObj->pEventReceiveExpedited        = (PRcvExpEvent)pvHandler;
            pAddrObj->pEventReceiveExpeditedContext = pvContext;
            break;
        
          default:
            TdiStatus = TDI_BAD_EVENT_TYPE;
            break;
        }
    } else
        TdiStatus = TDI_ADDR_INVALID;

    
    IFDBG(DebugOut (TdiStatus == TDI_SUCCESS ? DEBUG_TDI_TRACE : DEBUG_ERROR, L"-BTR_SetEventEntry :: %d\n", TdiStatus));
    gpTDIR->Unlock ();

    return TdiStatus;
}


////////////////////////////////////////////////////////////////////////////////
//    BTR_QueryInformationEntry()
//        
//    Description:
//        pTDIRequest::
//            If (IsConn), pTDIReq->Handle.ConnectionContext contains
//            the CONNECTION_CONTEXT of the BT_CONN_OBJ
//            BT_OpenConnection() assigns this value to be a pointer to
//            the BT_CONN_OBJ.
//            If (~IsConn), pTDIReq->Handle.AddressHandle is a handle to
//            the BT_ADDR_OBJ. BT_OpenAddress() assigns this value to be
//            a pointer to the BT_ADDR_OBJ.
//
//        uiQueryType::
//            TDI_QUERY_BROADCAST_ADDRESS
//            TDI_QUERY_PROVIDER_INFO
//            TDI_QUERY_ADDRESS_INFO
//            TDI_QUERY_CONNECTION_INFO
//            TDI_QUERY_PROVIDER_STATISTICS
//
//        pNdisBuff::
//            Pointer to output NDIS_BUFFER.
//
//        puiBytesReturned::
//            On entry, the data size of the NDIS buffer chain. On return, 
//            sizeof() information struct copied into NDIS_BUFFER.
//
//        uiIsConn::
//            Indicates a query for connection information rather than Address
//            Object information (TDI_QUERY_ADDRESS_INFO only).
//        
//    Arguments:
//
//    Returns:
//        TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
//
//    Comments:
//
//
static TDI_STATUS BTR_QueryInformationEntry
(
PTDI_REQUEST        pTDIRequest,
uint                uiQueryType,
PNDIS_BUFFER        pNdisBuff,
uint *                puiBytesReturned,
uint                uiIsConn
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_QueryInformationEntry 0x%08x\n", uiQueryType));

    ////////////////////////////////////////////////////////////////////////////
    //    This is large enough for TDI_QUERY_ADDRESS_INFO because
    //    of the inclusion of TDI_PROVIDER_STATISTICS.
    //
    union 
    {
        TDI_CONNECTION_INFO        ConnInfo;
        TDI_ADDRESS_INFO        AddrInfo;
        TDI_PROVIDER_INFO        ProviderInfo;
        TDI_PROVIDER_STATISTICS    ProviderStats;
    } InfoBuf;

    TDI_STATUS  TdiStatus = TDI_SUCCESS;
    int         InfoSize  = 0;
    int         Offset;
    
    switch (uiQueryType)
    {
        case TDI_QUERY_PROVIDER_INFO:
            InfoSize = sizeof(TDI_PROVIDER_INFO);
            InfoBuf.ProviderInfo.Version                 = 0x0100;
            InfoBuf.ProviderInfo.MaxSendSize            = 2048;
            InfoBuf.ProviderInfo.MaxConnectionUserData    = 0;
            InfoBuf.ProviderInfo.MaxDatagramSize        = 0;    
            InfoBuf.ProviderInfo.ServiceFlags            = 0;
            InfoBuf.ProviderInfo.MinimumLookaheadData    = 0;
            InfoBuf.ProviderInfo.MaximumLookaheadData    = 0;
            InfoBuf.ProviderInfo.NumberOfResources        = 0;
            InfoBuf.ProviderInfo.StartTime.LowPart        = 0;
            InfoBuf.ProviderInfo.StartTime.HighPart        = 0;
            break;
    
        case TDI_QUERY_ADDRESS_INFO: {
            InfoSize = TAI_SIZE;
    
            if (! gpTDIR) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_QueryInformationEntry : TDI_INVALID_STATE\n"));
                TdiStatus = TDI_INVALID_STATE;
                break;
            }

            gpTDIR->Lock ();
            if (! gpTDIR->fIsInitialized) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_QueryInformationEntry : WSANOTINITIALISED\n"));
                gpTDIR->Unlock ();
                TdiStatus = TDI_INVALID_STATE;
                break;
            }

            InfoBuf.AddrInfo.ActivityCount          = 1;    // No one knows!
            InfoBuf.AddrInfo.Address.TAAddressCount = 1;    // # addr follow
            InfoBuf.AddrInfo.Address.Address[0].AddressLength = sizeof(SOCKADDR_BTH) - 2;

            SOCKADDR_BTH *pAddr = (SOCKADDR_BTH *)&InfoBuf.AddrInfo.Address.Address[0].AddressType;
            memset (pAddr, 0, sizeof(*pAddr));
            pAddr->addressFamily = AF_BT;
                
            if (uiIsConn) {
                ////////////////////////////////////////////////////////////////
                //    Extract the local address from the Connection
                //
                RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionObject (pTDIRequest->Handle.ConnectionContext);
                if (pConn && pConn->pAddr) {
                    pAddr->port = pConn->pAddr->ch;
                    if (pConn->pAddr->b.NAP == 0 && pConn->pAddr->b.SAP == 0)
                        memcpy (&pAddr->btAddr, &gpTDIR->btAddr, sizeof(pAddr->btAddr));
                    else {
                        BT_ADDR bt = SET_NAP_SAP(pConn->pAddr->b.NAP, pConn->pAddr->b.SAP);
                        memcpy (&pAddr->btAddr, &bt, sizeof(pAddr->btAddr));
                    }
                } else
                    TdiStatus = TDI_INVALID_CONNECTION;
            } else {
                ////////////////////////////////////////////////////////////////
                //    Extract the local address from the Address Object
                //
                RFCOMM_ADDRESS_OBJECT *pAO = GetAddressObject (pTDIRequest->Handle.AddressHandle);
                if (pAO) {
                    pAddr->port = pAO->ch;
                    if (pAO->b.NAP == 0 && pAO->b.SAP == 0)
                        memcpy (&pAddr->btAddr, &gpTDIR->btAddr, sizeof(pAddr->btAddr));
                    else {
                        BT_ADDR bt = SET_NAP_SAP(pAO->b.NAP, pAO->b.SAP);
                        memcpy (&pAddr->btAddr, &bt, sizeof(pAddr->btAddr));
                    }
                } else
                    TdiStatus = TDI_ADDR_INVALID;
            }
    
            gpTDIR->Unlock ();
            break;
        }
    
        case TDI_QUERY_BROADCAST_ADDRESS:        
        case TDI_QUERY_PROVIDER_STATISTICS:
        case TDI_QUERY_DATAGRAM_INFO:
        case TDI_QUERY_DATA_LINK_ADDRESS:
        case TDI_QUERY_NETWORK_ADDRESS:
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_QueryInformationEntry : query 0x%08x not supported\n", uiQueryType));
            break;
            
        default:
            TdiStatus = TDI_INVALID_QUERY;
    }

    if ((TdiStatus == TDI_SUCCESS) && (*puiBytesReturned < (ULONG) InfoSize))
        TdiStatus = TDI_BUFFER_OVERFLOW;

    if (TdiStatus == TDI_SUCCESS) {
        Offset = 0;
        CopyFlatToNdis(pNdisBuff, (uchar *) &InfoBuf, InfoSize, (unsigned int *)&Offset);
        *puiBytesReturned = InfoSize;
    }

    IFDBG(DebugOut (TdiStatus == TDI_SUCCESS ? DEBUG_TDI_TRACE : DEBUG_ERROR, L"-BTR_QueryInformationEntry : %d\n", TdiStatus);   ) 
    return TdiStatus;
    
}

////////////////////////////////////////////////////////////////////////////////
//    BTR_SetInformationEntry()
//        
//    Description:
//
//
//        
//    Arguments:
//
//    Returns:
//
//    Comments: NEVER CALLED
//
//
static TDI_STATUS BTR_SetInformationEntry
(
PTDI_REQUEST        pTDIRequest,
uint                uiSetType, 
PNDIS_BUFFER        pNdisBuff,
uint                uiBufLen,
uint                uiIsConn
) {
    SVSUTIL_ASSERT (0);
    return TDI_SUCCESS;
}


////////////////////////////////////////////////////////////////////////////////
//    BTR_ActionEntry()
//        
//    Description:
//
//
//        
//    Arguments:
//
//    Returns:
//
//    Comments: NEVER CALLED
//
//
static TDI_STATUS BTR_ActionEntry
(
PTDI_REQUEST        pTDIRequest,
uint                uiActionType, 
PNDIS_BUFFER        pNdisBuff,
uint                uiBuffLen
) {
    SVSUTIL_ASSERT (0);
    return TDI_SUCCESS;
}


////////////////////////////////////////////////////////////////////////////////
//    BTR_ActionEntry()
//        
//    Description:
//
//
//        
//    Arguments:
//
//    Returns:
//
//    Comments: NEVER CALLED
//
//
static TDI_STATUS BTR_QueryInformationExEntry
(
PTDI_REQUEST        pTDIRequest, 
struct TDIObjectID    *psObjId, 
PNDIS_BUFFER        pNdisBuff,
uint *              puiBufLen,
void *              pvContext
) {
    SVSUTIL_ASSERT (0);
    return TDI_INVALID_REQUEST;
}

////////////////////////////////////////////////////////////////////////////////
//    BTR_SetInformationExEntry()
//        
//    Description:
//        Set info associated with ADDRESS_OBJECTS or CONNECTIONS.
//
//    Arguments:
//        pTDIRequest::
//            pTDIReq->Handle.ConnectionContext contains the CONNECTION_CONTEXT
//            of the BT_CONN_OBJ.   BT_OpenConnection() assigns this 
//            value to be a pointer to the BT_CONN_OBJ.
//            
//        pObjID::
//            Pointer to input TDIObjectID struct. IrLMP ignores the 
//            TCP_SOCKET_WINDOW (tcpinfo.h, ipexport.h) option that occurs on a 
//            connect()
//
//        pvBuf::
//            Pointer to input buffer.
//
//        uiBuffLen::
//            Input buffer len.
//        
//
//    Returns:
//        TDI_SUCCESS on success, or tdistat.h
//
//    Comments:
//        Ported from IR TDI.
//        Doesn't realy do anything on BTh
//
static TDI_STATUS BTR_SetInformationExEntry
(
PTDI_REQUEST        pTDIRequest, 
struct TDIObjectID    *pObjId,
void                *pvBuf,
uint                uiBuffLen
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+/-BTR_SetInformationExEntry\n"));

    if (pObjId->toi_entity.tei_entity != CO_TL_ENTITY &&
        pObjId->toi_entity.tei_entity != CL_TL_ENTITY)
        return TDI_INVALID_REQUEST;

    if (pObjId->toi_entity.tei_instance != 0)
        return TDI_INVALID_REQUEST;

    switch(pObjId->toi_class) {
        case INFO_CLASS_PROTOCOL:
        {
            switch(pObjId->toi_type) {
                case INFO_TYPE_ADDRESS_OBJECT:
                    //switch(pObjId->toi_id) {
                    //    default:
                            return TDI_INVALID_PARAMETER;
                    //        break;
                    //}
                    //break;


                case INFO_TYPE_CONNECTION:
                    switch(pObjId->toi_id) {
                        case TCP_SOCKET_WINDOW:
                            return TDI_SUCCESS;
                            break;

                        default:
                            return TDI_INVALID_PARAMETER;
                            break;
                    }
                    break;
            
          
                case INFO_TYPE_PROVIDER:
                    default:
                        return TDI_INVALID_PARAMETER;
                        break;
            }
            break;
        }
        
        case INFO_CLASS_GENERIC:
            return TDI_INVALID_PARAMETER;
            break;
        
        case INFO_CLASS_IMPLEMENTATION:
        default:
            return TDI_INVALID_REQUEST;
            break;
    }

    return TDI_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
//    BTR_GetSockaddrType()    
//        
//    Description:
//        Called after bind(), this function returns address information.
//
//    Arguments:
//        pSockAddr::
//            Pointer to winsock's SOCKADDR structure.
//            
//        dwSockAddrLen::
//            Length of the input SOCKADDR structure.
//
//        pSockAddrInfo::
//            Pointer to output SOCKADDR_INFO structure.
//
//    Returns:
//        NO_ERROR on success, or winsock.h error.
//
//    Comments:
//        Called after winsock's bind().
//
static TDI_STATUS BTR_GetSockaddrType
(
PSOCKADDR Sockaddr,
DWORD SockaddrLength,
PSOCKADDR_INFO SockaddrInfo
) {
    SOCKADDR_BTH *pAddr = (SOCKADDR_BTH *)Sockaddr;

    if (SockaddrLength < sizeof(SOCKADDR_BTH))
        return WSAEFAULT;

    if (pAddr->addressFamily != AF_BT)
        return WSAEAFNOSUPPORT;
    BT_ADDR bt;
    memcpy (&bt, &pAddr->btAddr, sizeof(pAddr->btAddr));

    if (GET_NAP(bt) == 0x00 &&    GET_SAP(bt) == 0x00) {
        SockaddrInfo->AddressInfo  = SockaddrAddressInfoWildcard;
        SockaddrInfo->EndpointInfo = SockaddrEndpointInfoWildcard;
    } else {
        SockaddrInfo->AddressInfo  = SockaddrAddressInfoNormal;
        SockaddrInfo->EndpointInfo = SockaddrEndpointInfoNormal;
    }
    
    return NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
//    BTR_GetWildcardSockaddr()
//        
//    Description:
//        Called after connect() without previous bind(). 
//        Returns a SOCKADDR_BT suitable for an implicit bind().
//        
//    Arguments:
//        pvHelperDllSocketContext::
//
//        pSockAddr::
//
//        piSockaddrLen::
//
//
//    Returns:
//        NO_ERROR on success, or winsock.h error.
//
//    Comments:
//        Called after connect() without previous bind()
//
//
static TDI_STATUS BTR_GetWildcardSockaddr
(
PVOID HelperDllSocketContext,
PSOCKADDR Sockaddr,
PINT SockaddrLength
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_GetSockaddrType\n"));

    if (*SockaddrLength < sizeof(SOCKADDR_BTH)) {
        IFDBG(DebugOut (DEBUG_WARN, L"-BTR_GetSockaddrType WSAEFAULT\n"));

        return WSAEFAULT;
    }

    *SockaddrLength = sizeof(SOCKADDR_BTH);

    memset(Sockaddr, sizeof(SOCKADDR_BTH), 0);
    
    ((SOCKADDR_BTH *) Sockaddr)->addressFamily = AF_BT;
    ((SOCKADDR_BTH *) Sockaddr)->port = CLIENTPORTSIG;

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_GetSockaddrType NO_ERROR\n"));
    return NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
//    BTR_GetSocketInformation()
//        
//    Description:
//        Called by the Winsock DLL when a level/option name combination
//        is passed to getsockopt() that the winsock DLL does not understand.
//        
//    Arguments:
//        pvHelperDllSocketContext::
//            Pointer to the BT_SOCKET_CONTEXT returned by BT_OpenSocket().
//
//        SocketHandle::
//            Handle of the socket for which we're getting information.
//
//        hTdiAddressObjectHandle::
//            TDI address object of the socket, or NULL.
//
//        iLevel::
//            level, from getsockopt().
//
//        iOptionName::
//            optname, from getsockopt().
//
//        pcOptionValue::
//            optval, from getsockopt().
//
//        piOptionLen::
//            optlen, from getsockopt().
//
//    Returns:
//        NO_ERROR (winerror.h) on success, or (winsock.h)
//
//    Comments:
//
static TDI_STATUS BTR_GetSocketInformation
(
PVOID HelperDllSocketContext,
SOCKET SocketHandle,
HANDLE TdiAddressObjectHandle,
HANDLE TdiConnectionObjectHandle,
INT Level,
INT OptionName,
PCHAR OptionValue,
PINT OptionLength
) {
    if (Level != SOL_RFCOMM)
        return WSAEINVAL;

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_GetSocketInformation : WSANOTINITIALISED\n"));
        return WSANOTINITIALISED;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_GetSocketInformation : WSANOTINITIALISED\n"));
        gpTDIR->Unlock ();
        return WSANOTINITIALISED;
    }

    //
    //    Process getsockopt data here
    //
    RFCOMM_SOCKET_OBJECT *pSocket = GetSocketObject (HelperDllSocketContext);
    if (! pSocket) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_GetSocketInformation : WSANOTINITIALISED\n"));
        gpTDIR->Unlock ();
        return WSAENOTSOCK;
    }

    RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionObject (TdiConnectionObjectHandle);
    RFCOMM_ADDRESS_OBJECT *pao = GetAddressObject (TdiAddressObjectHandle);

    int iRet = NO_ERROR;

    __try {
        switch (OptionName) {
        case SO_BTH_GET_PAGE_TO:            // no restrictions. optlen=sizeof(unsigned int), optval = &page timeout
            {
                if (*OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : invalid SO_BTH_GET_PAGE_TO : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                gpTDIR->Unlock ();

                unsigned short page = 0;
                int iRes = BthReadPageTimeout (&page);
                if (iRes == ERROR_SUCCESS)
                    *(unsigned int *)OptionValue = page;
                return iRes;
            }

        case SO_BTH_GET_SCAN:            // no restrictions. optlen=sizeof(unsigned int), optval = &scan
            {
                if (*OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : invalid SO_BTH_GET_SCAN : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                gpTDIR->Unlock ();

                unsigned char mask = 0;
                int iRes = BthReadScanEnableMask (&mask);
                if (iRes == ERROR_SUCCESS)
                    *(unsigned int *)OptionValue = mask;
                return iRes;
            }

        case SO_BTH_GET_AUTHN_ENABLE:    // no restrictions. optlen=sizeof(unsigned int), optval = &ae
            {
                if (*OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : invalid SO_BTH_GET_AUTHN_ENABLE : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                gpTDIR->Unlock ();

                unsigned char ae = 0;
                int iRes = BthReadAuthenticationEnable (&ae);
                if (iRes == ERROR_SUCCESS)
                    *(unsigned int *)OptionValue = ae;
                return iRes;
            }

        case SO_BTH_GET_LINK_POLICY:    // connected only. optlen=sizeof(int), optval = &int
            {
                if (*OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : invalid SO_BTH_GET_LINK_POLICY : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                if (pConn && (pConn->eStage == OPENED)) {
                    BT_ADDR bt = pConn->BTAddr.btAddr;
                    unsigned short settings;

                    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_SetSocketInformation :: querying settings for %04x%08x\n", GET_NAP(bt), GET_SAP(bt)));
                    gpTDIR->Unlock ();
                    int iRes = BthReadLinkPolicySettings (&bt, &settings);
                    if (iRes == ERROR_SUCCESS)
                        *(unsigned int *)OptionValue = settings;

                    return iRes;
                }

                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_GET_LINK_POLICY : NOT CONNECTED\n"));
                iRet = WSAENOTCONN;
                break;
            }

        case SO_BTH_GET_MODE:    // connected only. optlen=sizeof(int), optval = &int
            {
                if (*OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : invalid SO_BTH_GET_MODE : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                if (pConn && (pConn->eStage == OPENED)) {
                    BT_ADDR bt = pConn->BTAddr.btAddr;
                    unsigned char mode;

                    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_SetSocketInformation :: querying mode for %04x%08x\n", GET_NAP(bt), GET_SAP(bt)));
                    gpTDIR->Unlock ();
                    int iRes = BthGetCurrentMode (&bt, &mode);
                    if (iRes == ERROR_SUCCESS)
                        *(unsigned int *)OptionValue = mode;

                    return iRes;
                }

                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_GET_MODE : NOT CONNECTED\n"));
                iRet = WSAENOTCONN;
                break;
            }

        case SO_BTH_GET_COD:            // no restrictions. optlen=sizeof(unsigned int), optval = &cod
            {
                if (*OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : invalid SO_BTH_GET_COD : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                gpTDIR->Unlock ();

                unsigned int cod = 0;
                int iRes = BthReadCOD (&cod);
                if (iRes == ERROR_SUCCESS)
                    *(unsigned int *)OptionValue = cod;
                return iRes;
            }

        case SO_BTH_GET_LOCAL_VER:            // no restrictions. optlen=sizeof(unsigned int), optval = &BTH_LOCAL_VERSION
            {
                if (*OptionLength != sizeof(BTH_LOCAL_VERSION)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : invalid SO_BTH_GET_LOCAL_VER : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                gpTDIR->Unlock ();

                unsigned char hci_version, lmp_version;
                unsigned short hci_revision, lmp_revision, manufacturer;
                unsigned char lmp_features[8];
                int iRes = BthReadLocalVersion (&hci_version, &hci_revision, &lmp_version, &lmp_revision, &manufacturer, lmp_features);
                if (iRes == ERROR_SUCCESS) {
                    BTH_LOCAL_VERSION *pv = (BTH_LOCAL_VERSION *)OptionValue;
                    pv->hci_revision = hci_revision;
                    pv->hci_version = hci_version;
                    pv->lmp_subversion = lmp_revision;
                    pv->lmp_version = lmp_version;
                    pv->manufacturer = manufacturer;
                    memcpy (pv->lmp_features, lmp_features, sizeof(pv->lmp_features));
                }
                return iRes;
            }

        case SO_BTH_GET_REMOTE_VER:            // no restrictions. optlen=sizeof(unsigned int), optval = &BTH_REMOTE_VERSION
            {
                if (*OptionLength != sizeof(BTH_REMOTE_VERSION)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : invalid SO_BTH_GET_REMOTE_VER : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                if (! (pConn && pConn->pAddr && (pConn->eStage == OPENED))) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : connection not open SO_BTH_GET_REMOTE_VER\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                BT_ADDR bt = pConn->BTAddr.btAddr;
                gpTDIR->Unlock ();

                unsigned char lmp_version;
                unsigned short lmp_revision, manufacturer;
                unsigned char lmp_features[8];
                int iRes = BthReadRemoteVersion (&bt, &lmp_version, &lmp_revision, &manufacturer, lmp_features);
                if (iRes == ERROR_SUCCESS) {
                    BTH_REMOTE_VERSION *pv = (BTH_REMOTE_VERSION *)OptionValue;
                    pv->lmp_subversion = lmp_revision;
                    pv->lmp_version = lmp_version;
                    pv->manufacturer = manufacturer;
                    memcpy (pv->lmp_features, lmp_features, sizeof(pv->lmp_features));
                }
                return iRes;
            }

        case SO_BTH_GET_LINK:            // bound only! optlen=sizeof(BTH_SOCKOPT_SECURITY), optval=BTH_SOCKOPT_SECURITY
            {
                if (*OptionLength != sizeof(BTH_SOCKOPT_SECURITY)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : invalid SO_BTH_GET_LINK : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                BTH_SOCKOPT_SECURITY *pbs = (BTH_SOCKOPT_SECURITY *)OptionValue;
                pbs->iLength = 16;

                BT_ADDR bt = pbs->btAddr;
                if ((bt == 0) && pConn)
                    bt = pConn->BTAddr.btAddr;

                if (bt == 0) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_LINK: not bound\n"));
                    iRet = WSAEADDRNOTAVAIL;
                    break;
                }

                gpTDIR->Unlock ();

                return BthGetLinkKey (&bt, pbs->caData);
            }

        case SO_BTH_GET_MTU:            // optlen=sizeof(unsigned int), optval = &mtu
            if (*OptionLength != sizeof(unsigned int)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_MTU: bad length\n"));
                iRet = WSAEINVAL;
            } else if (pConn)
                *(unsigned int *)OptionValue = pConn->mtu;
            else if (pao)
                *(unsigned int *)OptionValue = pao->mtu;
            else {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_MTU: not bound\n"));
                iRet = WSAEADDRNOTAVAIL;
            }
            break;

        case SO_BTH_GET_MTU_MAX:        // optlen=sizeof(unsigned int), optval = &max. mtu
            if (*OptionLength != sizeof(unsigned int)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_MTU_MAX: bad length\n"));
                iRet = WSAEINVAL;
            } else if (pConn)
                *(unsigned int *)OptionValue = pConn->imax_mtu;
            else if (pao)
                *(unsigned int *)OptionValue = pao->imax_mtu;
            else {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_MTU_MAX: not bound\n"));
                iRet = WSAEADDRNOTAVAIL;
            }
            break;

        case SO_BTH_GET_MTU_MIN:        // optlen=sizeof(unsigned int), optval = &min. mtu
            if (*OptionLength != sizeof(unsigned int)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_MTU_MIN: bad length\n"));
                iRet = WSAEINVAL;
            } else if (pConn)
                *(unsigned int *)OptionValue = pConn->imin_mtu;
            else if (pao)
                *(unsigned int *)OptionValue = pao->imin_mtu;
            else {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_MTU_MIN: not bound\n"));
                iRet = WSAEADDRNOTAVAIL;
            }
            break;

        case SO_BTH_GET_XON_LIM:        // optlen=sizeof(unsigned int), optval = &xon
            if (*OptionLength != sizeof(unsigned int)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_XON_LIM: bad length\n"));
                iRet = WSAEINVAL;
            } else if (pConn)
                *(unsigned int *)OptionValue = pConn->xon_lim;
            else if (pao)
                *(unsigned int *)OptionValue = pao->xon_lim;
            else {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_XON_LIM: not bound\n"));
                iRet = WSAEADDRNOTAVAIL;
            }
            break;

        case SO_BTH_GET_XOFF_LIM:        // optlen=sizeof(unsigned int), optval = &xoff
            if (*OptionLength != sizeof(unsigned int)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_XOFF_LIM: bad length\n"));
                iRet = WSAEINVAL;
            } else if (pConn)
                *(unsigned int *)OptionValue = pConn->xoff_lim;
            else if (pao)
                *(unsigned int *)OptionValue = pao->xoff_lim;
            else {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_XOFF_LIM: not bound\n"));
                iRet = WSAEADDRNOTAVAIL;
            }
            break;

        case SO_BTH_GET_SEND_BUFFER:    // optlen=sizeof(unsigned int), optval = &max buffered size for send
            if (*OptionLength != sizeof(unsigned int)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_SEND_BUFFER: bad length\n"));
                iRet = WSAEINVAL;
            } else if (pConn)
                *(unsigned int *)OptionValue = pConn->imax_send;
            else if (pao)
                *(unsigned int *)OptionValue = pao->imax_send;
            else {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_SEND_BUFFER: not bound\n"));
                iRet = WSAEADDRNOTAVAIL;
            }
            break;

        case SO_BTH_GET_RECV_BUFFER:    // optlen=sizeof(unsigned int), optval = &max buffered size for recv
            if (*OptionLength != sizeof(unsigned int)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_RECV_BUFFER: bad length\n"));
                iRet = WSAEINVAL;
            } else if (pConn)
                *(unsigned int *)OptionValue = pConn->imax_recv;
            else if (pao)
                *(unsigned int *)OptionValue = pao->imax_recv;
            else {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_RECV_BUFFER: not bound\n"));
                iRet = WSAEADDRNOTAVAIL;
            }
            break;

        case SO_BTH_GET_V24_BR:            // connected only! optlen=2*sizeof(unsigned char), optval = &{v24 , br}
            if (*OptionLength != 2*sizeof(int)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_V24_BR: bad length\n"));
                iRet = WSAEINVAL;
            } else if (pConn && (pConn->eStage == OPENED)) {
                ((int *)OptionValue)[0] = pConn->v24in;
                ((int *)OptionValue)[1] = pConn->brin;
            } else {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_V24_BR: not connected\n"));
                iRet = WSAENOTCONN;
            }
            break;

        case SO_BTH_GET_RLS:            // connected only! optlen=sizeof(unsigned char), optval = &rls
            if (*OptionLength != sizeof(int)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_RLS: bad length\n"));
                iRet = WSAEINVAL;
            } else if (pConn && (pConn->eStage == OPENED)) {
                ((int *)OptionValue)[0] = pConn->rlsin;
            } else {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_RLS: not connected\n"));
                iRet = WSAENOTCONN;
            }
            break;

        case SO_BTH_GET_FLOW_TYPE:        // connected only! optlen=sizeof(unsigned int), optval=&1=credit-based, 0=legacy
            if (*OptionLength != sizeof(int)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_RLS: bad length\n"));
                iRet = WSAEINVAL;
            } else if (pConn && (pConn->eStage == OPENED)) {
                *(int *)OptionValue = pConn->fc_credit ? 1 : 0;
            } else {
                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : SO_BTH_GET_RLS: not connected\n"));
                iRet = WSAENOTCONN;
            }
            break;

        default:
            IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : invalid socket option %d\n", OptionName));
            iRet = WSAENOPROTOOPT;
        }
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_GetSocketInformation : Exception!\n"));
        iRet = WSAEFAULT;
    }

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_GetSocketInformation :: %d\n", iRet));
    gpTDIR->Unlock ();
    return iRet;
}

////////////////////////////////////////////////////////////////////////////////
//    BTR_GetWinsockMapping()
//        
//    Description:        
//        Called at stack load time. Returns a WINSOCK_MAPPING 
//        struct containing the AddressFamily, SocketType and Protocol 
//        triple socket() parms supported by this stack.
//
//        Required WINSOCK_MAPPING struct size. The function is called again 
//        with a larger buffer if needed.
//        
//    Arguments:
//        pMapping :: 
//            structure determining the address family/socket
//            type/protocol supported by this stack driver.
//    
//        dwMappingLength :: 
//            Length of the given buffer.
//
//    Returns:
//        Size of the WINSOCK_MAPPING this driver support.
//
//    Comments:
//        Called during stack load time.
//
static DWORD BTR_GetWinsockMapping
(
PWINSOCK_MAPPING Mapping,
DWORD MappingLength
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_GetWinsockMapping\n"));

    if (MappingLength >= sizeof(BT_MAPPING))
        memcpy(Mapping, &BT_MAPPING, sizeof(BT_MAPPING));

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_GetWinsockMapping\n"));
    return sizeof(BT_MAPPING);    
}

////////////////////////////////////////////////////////////////////////////////
//    BTR_Notify()
//        
//    Description:
//        Called after the socket event(s) specified in IRLMP_OpenSocket()
//        occur.
//        
//    Arguments:
//        pvHelperDllSocketContext::
//            Pointer to the BT_SOCKET_CONTEXT returned by BT_OpenSocket().
//
//        SocketHandle::
//            TDI Address Object handle if the socket is bound, otherwise NULL.
//
//        hTdiAddressObjectHandle::
//            TDI Address Object handle if the socket is bound, otherwise NULL.
//
//        hTdiConnectionObjectHandle::
//            TDI Connection Object handle if socket is connected, otherwise NULL.
//
//        dwNotifyEvent::
//            One of the:
//                WSH_NOTIFY_BIND
//                WSH_NOTIFY_LISTEN
//                WSH_NOTIFY_CONNECT
//                WSH_NOTIFY_ACCEPT
//                WSH_NOTIFY_SHUTDOWN_RECEIVE
//                WSH_NOTIFY_SHUTDOWN_SEND
//                WSH_NOTIFY_SHUTDOWN_ALL
//                WSH_NOTIFY_CLOSE
//                WSH_NOTIFY_CONNECT_ERROR
//
//    Returns:
//        NO_ERROR (winerror.h) on success, or (winsock.h)
//
//    Comments:
//
//
static TDI_STATUS BTR_Notify
(
PVOID pvHelperDllSocketContext,
SOCKET SocketHandle,
HANDLE hTdiAddressObjectHandle,
HANDLE hTdiConnectionObjectHandle,
DWORD dwNotifyEvent
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"BTR_Notify [%s] [0x%x]\r\n",
        dwNotifyEvent == WSH_NOTIFY_BIND             ? L"WSH_NOTIFY_BIND" :
        dwNotifyEvent == WSH_NOTIFY_LISTEN             ? L"WSH_NOTIFY_LISTEN" :        
        dwNotifyEvent == WSH_NOTIFY_CONNECT             ? L"WSH_NOTIFY_CONNECT" :        
        dwNotifyEvent == WSH_NOTIFY_ACCEPT             ? L"WSH_NOTIFY_ACCEPT" :
        dwNotifyEvent == WSH_NOTIFY_SHUTDOWN_RECEIVE ? L"WSH_NOTIFY_SHUTDOWN_RECEIVE" :
        dwNotifyEvent == WSH_NOTIFY_SHUTDOWN_SEND    ? L"WSH_NOTIFY_SHUTDOWN_SEND" :
        dwNotifyEvent == WSH_NOTIFY_SHUTDOWN_ALL     ? L"WSH_NOTIFY_SHUTDOWN_ALL" :
        dwNotifyEvent == WSH_NOTIFY_CLOSE             ? L"WSH_NOTIFY_CLOSE" :
        dwNotifyEvent == WSH_NOTIFY_CONNECT_ERROR     ? L"WSH_NOTIFY_CONNECT_ERROR" :
        L"unexpected WSH_NOTIFY",
        dwNotifyEvent));

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_Notify : WSANOTINITIALISED\n"));
        return WSANOTINITIALISED;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_Notify : WSANOTINITIALISED\n"));
        gpTDIR->Unlock ();
        return WSANOTINITIALISED;
    }

    int status = NO_ERROR;

    if (dwNotifyEvent == WSH_NOTIFY_LISTEN) {
        ////////////////////////////////////////////////////////////////////////
        //    When we are notified to LISTEN, AFD should have informed us on 
        //    the connection event to call back via SetEventEntry() for
        //    TDI_EVENT_CONNECT.
        //    We will now trigger L2CAP to start listening to the port that
        //    this socket bind to.
        //
        RFCOMM_ADDRESS_OBJECT *pao = GetAddressObject (hTdiAddressObjectHandle);
        if (pao) {
            if (!pao->fListenOn)
                status = WSAEINVAL;
        } else
            status = WSAESTALE;
    } else if (dwNotifyEvent == WSH_NOTIFY_BIND) {    
        RFCOMM_ADDRESS_OBJECT *pao = GetAddressObject (hTdiAddressObjectHandle);
        RFCOMM_SOCKET_OBJECT *pSocket = GetSocketObject (pvHelperDllSocketContext);
        if (pao && pSocket)
            pSocket->pao = pao;
        else
            status = WSAESTALE;
    } else if (dwNotifyEvent == WSH_NOTIFY_CLOSE) {    
        RFCOMM_SOCKET_OBJECT *pSocket = GetSocketObject (pvHelperDllSocketContext);
        if (pSocket)
            CloseSocketObject (pSocket);
        else
            status = WSAENOTSOCK;
    }

    IFDBG(DebugOut (status == NO_ERROR ? DEBUG_TDI_TRACE : DEBUG_ERROR, L"-BTR_Notify :: %d\n", status));
    gpTDIR->Unlock ();
    return status;
}

////////////////////////////////////////////////////////////////////////////////
//    BT_OpenSocket()
//        
//    Description:
//        Called after winsock's socket() call.
//        Validates the parms and allocates internal data structure for new
//        socket connection.
//
//    Arguments:
//        piAddressFamily::
//            From socket(AF_BT, ,).   
//            Must be AF_BT.
//
//        piSocketType::
//            From socket( , SOCK_SEQPACKET, ).   
//            Must be SOCK_STREAM
//
//        piProtocol::
//            From socket( , , 0)
//            Must be 0x00.
//
//        pwszTransportDeviceName::
//            Will copy the BLUETOOTH_TRANSPORT_NAME to this buffer.
//
//        pvHelperSocketContext::
//            Returns the address of a new BT_SOCKET_CONTEXT. 
//            This context will be returned in 
//            BT_Notify(), 
//            BT_OpenConnection(), 
//            BT_GetWildcardSockaddr(), 
//            BT_SetSocketInformation(),
//            BT_GetSocketInformation(). 
//
//        pdwNotificationEvents::
//            Bitmask indicating which Winsock events should result
//            in a call to BT_Notify().   
//
//    Returns:
//        NO_ERROR on success, or winsock.h error.
//
//    Comments:
//        Called after winsock's socket() call.
//    
static TDI_STATUS BTR_OpenSocket
(
PINT piAddressFamily,
PINT piSocketType,
PINT piProtocol,
PWSTR pwszTransportDeviceName,
PVOID *pvHelperDllSocketContext,
PDWORD pdwNotificationEvents
) {
    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_OpenSocket\n"));
    ////////////////////////////////////////////////////////////////////////////
    //    Reject it immediately if it's not our supported protocol.
    //
    if ((*piAddressFamily != (int) BT_MAPPING.Mapping[0].AddressFamily) ||
        (*piSocketType    != (int) BT_MAPPING.Mapping[0].SocketType)    ||
        (*piProtocol      != (int) BT_MAPPING.Mapping[0].Protocol))    {
        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] Unsupported protocol in BTR_OpenSocket\n"));

        return WSAEINVAL;
    }

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_OpenSocket : WSANOTINITIALISED\n"));
        return WSANOTINITIALISED;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_OpenSocket : WSANOTINITIALISED\n"));
        gpTDIR->Unlock ();
        return WSANOTINITIALISED;
    }

    wcscpy(pwszTransportDeviceName, RFCOMM_TRANSPORT_NAME);

    RFCOMM_SOCKET_OBJECT *pSock = new RFCOMM_SOCKET_OBJECT;

    if (! pSock) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_OpenSocket : WSAENOBUFS\n"));
        gpTDIR->Unlock ();
        return WSAENOBUFS;
    }

    pSock->pNext = gpTDIR->psolist;
    gpTDIR->psolist = pSock;

    ////////////////////////////////////////////////////////////////////////////
    //    Here are the goodies we return..
    //
    *pvHelperDllSocketContext = pSock;    
    *pdwNotificationEvents = 
        WSH_NOTIFY_BIND                    | 
        WSH_NOTIFY_LISTEN                |    
        WSH_NOTIFY_CLOSE                | 
        WSH_NOTIFY_CONNECT                |
        WSH_NOTIFY_ACCEPT                |
        WSH_NOTIFY_SHUTDOWN_RECEIVE        |
        WSH_NOTIFY_SHUTDOWN_SEND        |
        WSH_NOTIFY_SHUTDOWN_ALL            |
        WSH_NOTIFY_CLOSE                |
        WSH_NOTIFY_CONNECT_ERROR;       


    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_OpenSocket :: NO_ERROR\n"));
    gpTDIR->Unlock ();
    return NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
//    BT_SetSocketInformation()
//        
//    Description:
//        Called by the Winsock DLL when a level/option name combination
//        is passed to setsockopt() that the winsock DLL does not understand.
//        
//    Arguments:
//        NO_ERROR (winerror.h) on success, or (winsock.h)
//
//    Returns:
//        pvHelperDllSocketContext::
//            Pointer to the BT_SOCKET_CONTEXT returned by BT_OpenSocket().
//
//        SocketHandle::
//            Handle of the socket for which we're getting information.
//    
//        hTdiAddressObjectHandle::
//            TDI address object of the socket, or NULL.
//
//        hTdiConnectionObjectHandle::
//            TDI connection object of the socket, or NULL.
//
//        iLevel::
//            level, from setsockopt().            
//
//        iOptionName::
//            optname, from setsockopt().
//
//        pcOptionValue::
//            optval, from setsockopt().
//
//        iOptionLength::
//            optlen, from setsockopt().
//
//    Comments:
//
//
static TDI_STATUS BTR_SetSocketInformation
(
PVOID HelperDllSocketContext,
SOCKET SocketHandle,
HANDLE TdiAddressObjectHandle,
HANDLE TdiConnectionObjectHandle,
INT Level,
INT OptionName,
PCHAR OptionValue,
INT OptionLength
) {
    if ((Level != SOL_RFCOMM) || (OptionLength < 4))
        return WSAEINVAL;

    if (! gpTDIR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_SetSocketInformation : WSANOTINITIALISED\n"));
        return WSANOTINITIALISED;
    }

    gpTDIR->Lock ();
    if (! gpTDIR->fIsInitialized) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_SetSocketInformation : WSANOTINITIALISED\n"));
        gpTDIR->Unlock ();
        return WSANOTINITIALISED;
    }

    //
    //    Process setsockopt data here
    //
    RFCOMM_SOCKET_OBJECT *pSocket = GetSocketObject (HelperDllSocketContext);
    if (! pSocket) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_SetSocketInformation : WSANOTINITIALISED\n"));
        gpTDIR->Unlock ();
        return WSAENOTSOCK;
    }

    if (! pSocket->hSocketHandle)
        pSocket->hSocketHandle = (HANDLE)SocketHandle;

    SVSUTIL_ASSERT (pSocket->hSocketHandle == (HANDLE)SocketHandle);

    RFCOMM_CONNECTION_OBJECT *pConn = GetConnectionObject (TdiConnectionObjectHandle);
    RFCOMM_ADDRESS_OBJECT *pao = GetAddressObject (TdiAddressObjectHandle);

    OPT *pOpts = NULL;

    if (pao)
        pOpts = pao;
    else
        pOpts = pSocket;

    int iRet = NO_ERROR;

    __try {
        switch (OptionName) {
        case SO_BTH_SET_READ_REMOTE_NAME:    // no restrictions. optlen=sizeof(BTH_REMOTE_NAME), optval = &BTH_REMOTE_NAME
            {
                if (OptionLength != sizeof(BTH_REMOTE_NAME)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_GetSocketInformation : invalid SO_BTH_GET_REMOTE_NAME : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                gpTDIR->Unlock ();

                BTH_REMOTE_NAME *prn = (BTH_REMOTE_NAME *)OptionValue;
                unsigned int uiRetSize = 0;
                return BthRemoteNameQuery (&prn->bt, 248, &uiRetSize, prn->szNameBuffer);
            }

        case SO_BTH_SET_PAGE_TO:    // no restrictions. optlen=sizeof(unsigned int), optval = &page timeout
            {
                if (OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_PAGE_TO : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                unsigned int page = *(unsigned int *)OptionValue;
                gpTDIR->Unlock ();
                return BthWritePageTimeout ((unsigned short)page);
            }

        case SO_BTH_SET_SCAN:    // no restrictions. optlen=sizeof(unsigned int), optval = &scan
            {
                if (OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_SCAN : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                unsigned int mask = *(unsigned int *)OptionValue;
                gpTDIR->Unlock ();
                return BthWriteScanEnableMask ((unsigned char)mask);
            }

        case SO_BTH_SET_AUTHN_ENABLE:    // no restrictions. optlen=sizeof(unsigned int), optval = &authn_enable
            {
                if (OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_AUTHN_ENABLE : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                unsigned int ae = *(unsigned int *)OptionValue;
                gpTDIR->Unlock ();
                return BthWriteAuthenticationEnable ((unsigned char)ae);
            }

        case SO_BTH_SET_LINK_POLICY:    // connected only. optlen=sizeof(int), optval = &int
            {
                if (OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_LINK_POLICY : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                if (pConn && (pConn->eStage == OPENED)) {
                    BT_ADDR bt = pConn->BTAddr.btAddr;
                    unsigned short settings = (short)*(int *)OptionValue;

                    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_SetSocketInformation :: settings for %04x%08x = 0x%04x\n", GET_NAP(bt), GET_SAP(bt), settings));
                    gpTDIR->Unlock ();
                    return BthWriteLinkPolicySettings (&bt, settings);
                }

                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_LINK_POLICY : NOT CONNECTED\n"));
                iRet = WSAENOTCONN;
                break;
            }

        case SO_BTH_ENTER_HOLD_MODE:    // connected only. optlen=sizeof(BTH_HOLD_MODE), optval = &BTH_HOLD_MODE
            {
                if (OptionLength != sizeof(BTH_HOLD_MODE)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_ENTER_HOLD_MODE : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                if (pConn && (pConn->eStage == OPENED)) {
                    BT_ADDR bt = pConn->BTAddr.btAddr;
                    BTH_HOLD_MODE *phm = (BTH_HOLD_MODE *)OptionValue;

                    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_SetSocketInformation :: entering hold mode for %04x%08x\n", GET_NAP(bt), GET_SAP(bt)));
                    gpTDIR->Unlock ();
                    return BthEnterHoldMode (&bt, phm->hold_mode_max, phm->hold_mode_min, &phm->interval);
                }

                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_ENTER_HOLD_MODE : NOT CONNECTED\n"));
                iRet = WSAENOTCONN;
                break;
            }

        case SO_BTH_ENTER_SNIFF_MODE:    // connected only. optlen=sizeof(BTH_SNIFF_MODE), optval = &BTH_SNIFF_MODE
            {
                if (OptionLength != sizeof(BTH_SNIFF_MODE)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_ENTER_SNIFF_MODE : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                if (pConn && (pConn->eStage == OPENED)) {
                    BT_ADDR bt = pConn->BTAddr.btAddr;
                    BTH_SNIFF_MODE *psm = (BTH_SNIFF_MODE *)OptionValue;

                    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_SetSocketInformation :: entering sniff mode for %04x%08x\n", GET_NAP(bt), GET_SAP(bt)));
                    gpTDIR->Unlock ();
                    return BthEnterSniffMode (&bt, psm->sniff_mode_max, psm->sniff_mode_min, psm->sniff_attempt, psm->sniff_timeout, &psm->interval);
                }

                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_ENTER_SNIFF_MODE : NOT CONNECTED\n"));
                iRet = WSAENOTCONN;
                break;
            }

        case SO_BTH_EXIT_SNIFF_MODE:    // connected only. optlen, optval ignored
            {
                if (pConn && (pConn->eStage == OPENED)) {
                    BT_ADDR bt = pConn->BTAddr.btAddr;

                    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_SetSocketInformation :: exiting sniff mode for %04x%08x\n", GET_NAP(bt), GET_SAP(bt)));
                    gpTDIR->Unlock ();
                    return BthExitSniffMode (&bt);
                }

                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_EXIT_SNIFF_MODE : NOT CONNECTED\n"));
                iRet = WSAENOTCONN;
                break;
            }

        case SO_BTH_ENTER_PARK_MODE:    // connected only. optlen=sizeof(BTH_PARK_MODE), optval = &BTH_PARK_MODE
            {
                if (OptionLength != sizeof(BTH_PARK_MODE)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_ENTER_PARK_MODE : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                if (pConn && (pConn->eStage == OPENED)) {
                    BT_ADDR bt = pConn->BTAddr.btAddr;
                    BTH_PARK_MODE *ppm = (BTH_PARK_MODE *)OptionValue;

                    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_SetSocketInformation :: entering park mode for %04x%08x\n", GET_NAP(bt), GET_SAP(bt)));
                    gpTDIR->Unlock ();
                    return BthEnterParkMode (&bt, ppm->beacon_max, ppm->beacon_min, &ppm->interval);
                }

                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_ENTER_PARK_MODE : NOT CONNECTED\n"));
                iRet = WSAENOTCONN;
                break;
            }


        case SO_BTH_EXIT_PARK_MODE:    // connected only. optlen, optval ignored
            {
                if (pConn && (pConn->eStage == OPENED)) {
                    BT_ADDR bt = pConn->BTAddr.btAddr;

                    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_SetSocketInformation :: exiting park mode for %04x%08x\n", GET_NAP(bt), GET_SAP(bt)));
                    gpTDIR->Unlock ();
                    return BthExitParkMode (&bt);
                }

                IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_EXIT_PARK_MODE : NOT CONNECTED\n"));
                iRet = WSAENOTCONN;
                break;
            }

        case SO_BTH_SET_COD:    // no restrictions. optlen=sizeof(unsigned int), optval = &cod
            {
                if (OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_COD : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                unsigned int cod = *(unsigned int *)OptionValue;
                gpTDIR->Unlock ();
                return BthWriteCOD (cod);
            }

        case SO_BTH_AUTHENTICATE:    // optlen, optval ignored
            {
                if (pConn) {
                    pConn->fAuthenticate = TRUE;
                    if (pConn->pAddr && (pConn->eStage == OPENED)) {
                        BT_ADDR bt = pConn->BTAddr.btAddr;

                        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_SetSocketInformation :: authenticating live connection\n"));
                        gpTDIR->Unlock ();

                        return BthAuthenticate (&bt);
                    }
                    break;
                }

                pOpts->fAuthenticate = TRUE;
                pOpts->uiConfigMask |= OPT_CONFIG_AUTH;

                break;
            }

        case SO_BTH_ENCRYPT:    // optlen=sizeof(unsigned int), optval = &(unsigned int)TRUE/FALSE
            {
                if (OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_ENCRYPT : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                int flag = *(int *)OptionValue ? TRUE : FALSE;

                if (pConn) {
                    pConn->fEncrypt = flag;
                    if (pConn->pAddr && (pConn->eStage == OPENED)) {
                        BT_ADDR bt = pConn->BTAddr.btAddr;

                        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_SetSocketInformation :: encryption %s on live connection\n", flag ? L"on" : L"off"));
                        gpTDIR->Unlock ();

                        return BthSetEncryption (&bt, flag);
                    }
                    break;
                }

                pOpts->fEncrypt = flag;
                pOpts->uiConfigMask |= OPT_CONFIG_CRYPT;

                break;
            }

        case SO_BTH_SET_PIN:    // bound only! survives socket! optlen=sizeof(BTH_SOCKOPT_SECURITY), optval=&BTH_SOCKOPT_SECURITY
            {
                if (OptionLength != sizeof(BTH_SOCKOPT_SECURITY)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_PIN : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                BTH_SOCKOPT_SECURITY *pbs = (BTH_SOCKOPT_SECURITY *)OptionValue;
                if ((pbs->iLength < 0) || (pbs->iLength > PIN_SIZE)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_PIN : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }


                BT_ADDR bt = pbs->btAddr;
                if ((bt == 0) && pConn)
                    bt = pConn->BTAddr.btAddr;

                if (bt == 0) {
                    if (pConn) {
                        pConn->cPinLength = pbs->iLength;
                        memcpy (pConn->caPinData, pbs->caData, pbs->iLength);
                        break;
                    }

                    pOpts->cPinLength = pbs->iLength;
                    memcpy (pOpts->caPinData, pbs->caData, pbs->iLength);
                    pOpts->uiConfigMask |= OPT_CONFIG_PIN;

                    break;
                }

                gpTDIR->Unlock ();

                return pbs->iLength ? BthSetPIN (&bt, pbs->iLength, pbs->caData) : BthRevokePIN (&bt);
            }

        case SO_BTH_SET_LINK:    // optlen=sizeof(BTH_SOCKOPT_SECURITY), optval=&BTH_SOCKOPT_SECURITY
            {
                if (OptionLength != sizeof(BTH_SOCKOPT_SECURITY)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_LINK : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                BTH_SOCKOPT_SECURITY *pbs = (BTH_SOCKOPT_SECURITY *)OptionValue;
                if ((pbs->iLength != 0) && (pbs->iLength != KEY_SIZE)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_LINK : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                BT_ADDR bt = pbs->btAddr;
                if ((bt == 0) && pConn)
                    bt = pConn->BTAddr.btAddr;

                if (bt != 0) {
                    gpTDIR->Unlock ();

                    return pbs->iLength ? BthSetLinkKey (&bt, pbs->caData) : BthRevokeLinkKey (&bt);
                } else {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : SO_BTH_SET_LINK : WSAEADDRNOTAVAIL\n"));
                    iRet = WSAEADDRNOTAVAIL;
                }

                break;
            }

        case SO_BTH_SET_MTU:    // unconnected only! optlen=sizeof(unsigned int), optval = &mtu
            {
                if (OptionLength != sizeof(unsigned int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_MTU : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                unsigned short mtu = (unsigned short)*(unsigned int *)OptionValue;
                if ((mtu < TDIR_MTUMIN) || (mtu > TDIR_MTUMAX)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_MTU : out of bounds\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                if (pConn) {
                    if (pConn->eStage == INITED) {
                        pConn->mtu = mtu;
                        if (mtu > pConn->imax_mtu)
                            pConn->imax_mtu = mtu;
                        if (mtu < pConn->imin_mtu)
                            pConn->imin_mtu = mtu;
                    } else {
                        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_MTU : already connected\n"));
                        iRet = WSAEISCONN;
                    }

                    break;
                }

                pOpts->mtu = mtu;
                if (mtu > pOpts->imax_mtu)
                    pOpts->imax_mtu = mtu;
                if (mtu < pOpts->imin_mtu)
                    pOpts->imin_mtu = mtu;

                pOpts->uiConfigMask |= OPT_CONFIG_MTU;
                break;
            }

        case SO_BTH_SET_MTU_MAX:    // unconnected only! optlen=sizeof(unsigned int), optval = &max. mtu
            {
                if (OptionLength != sizeof(unsigned int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_MTU_MAX : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                unsigned short mtumax = (unsigned short)*(unsigned int *)OptionValue;
                if ((mtumax < TDIR_MTUMIN) || (mtumax > TDIR_MTUMAX)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_MTU_MAX : out of bounds\n"));
                    iRet = WSAEINVAL;
                    break;
                }
                
                if (pConn) {
                    if (pConn->eStage == INITED) {
                        pConn->imax_mtu = mtumax;
                        if (mtumax < pConn->mtu)
                            pConn->mtu = mtumax;
                        if (mtumax < pConn->imin_mtu)
                            pConn->imin_mtu = mtumax;
                    } else {
                        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_MTU_MAX : already connected\n"));
                        iRet = WSAEISCONN;
                    }

                    break;
                }

                pOpts->imax_mtu = mtumax;
                if (mtumax < pOpts->mtu)
                    pOpts->mtu = mtumax;
                if (mtumax < pOpts->imin_mtu)
                    pOpts->imin_mtu = mtumax;

                pOpts->uiConfigMask |= OPT_CONFIG_MTU_MAX;
                break;
            }

        case SO_BTH_SET_MTU_MIN:    // unconnected only! optlen=sizeof(unsigned int), optval = &min. mtu
            {
                if (OptionLength != sizeof(unsigned int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_MTU_MIN : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                unsigned short mtumin = (unsigned short)*(unsigned int *)OptionValue;
                if ((mtumin < TDIR_MTUMIN) || (mtumin > TDIR_MTUMAX)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_MTU_MIN : out of bounds\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                if (pConn) {
                    if (pConn->eStage == INITED) {
                        pConn->imax_mtu = mtumin;
                        if (mtumin > pConn->mtu)
                            pConn->mtu = mtumin;
                        if (mtumin > pConn->imax_mtu)
                            pConn->imax_mtu = mtumin;
                    } else {
                        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_MTU_MIN : already connected\n"));
                        iRet = WSAEISCONN;
                    }

                    break;
                }

                pOpts->imin_mtu = mtumin;
                if (mtumin > pOpts->mtu)
                    pOpts->mtu = mtumin;
                if (mtumin > pOpts->imax_mtu)
                    pOpts->imax_mtu = mtumin;

                pOpts->uiConfigMask |= OPT_CONFIG_MTU_MIN;
                break;
            }

        case SO_BTH_SET_XON_LIM:    // optlen=sizeof(unsigned int), optval = &xon limit (set flow off)
            {
                if (OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_XON_LIM : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                int xon = *(int *)OptionValue;

                if (pConn) {
                    if (pConn->eStage == INITED) {
                        if ((xon < 0) || (xon > pConn->imax_recv) || (xon < pConn->xoff_lim)) {
                            IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_XON_LIM : out of bounds\n"));
                            iRet = WSAEINVAL;
                        } else
                            pConn->xon_lim = xon;
                    } else {
                        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_XON_LIM : already connected\n"));
                        iRet = WSAEISCONN;
                    }

                    break;
                }

                if ((xon < 0) || (xon > pOpts->imax_recv) || (xon < pOpts->xoff_lim)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_XON_LIM : out of bounds\n"));
                    iRet = WSAEINVAL;
                } else {
                    pOpts->xon_lim = xon;
                    pOpts->uiConfigMask |= OPT_CONFIG_XON;
                }

                break;
            }

        case SO_BTH_SET_XOFF_LIM:    // optlen=sizeof(unsigned int), optval = &xoff limit (set flow on)
            {
                if (OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_XOFF_LIM : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                int xoff = *(int *)OptionValue;
                if (pConn) {
                    if (pConn->eStage == INITED) {
                        if ((xoff < 0) || (xoff > pConn->imax_recv) || (xoff > pConn->xon_lim)) {
                            IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_XOFF_LIM : out of bounds\n"));
                            iRet = WSAEINVAL;
                        } else
                            pConn->xoff_lim = xoff;
                    } else {
                        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_XOFF_LIM : already connected\n"));
                        iRet = WSAEISCONN;
                    }

                    break;
                }

                if ((xoff < 0) || (xoff > pOpts->imax_recv) || (xoff > pOpts->xon_lim)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_XOFF_LIM : out of bounds\n"));
                    iRet = WSAEINVAL;
                } else {
                    pOpts->xoff_lim = xoff;
                    pOpts->uiConfigMask |= OPT_CONFIG_XOFF;
                }
                break;
            }

        case SO_BTH_SET_SEND_BUFFER:    // optlen=sizeof(unsigned int), optval = &max buffered size for send
            {
                if (OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_SEND_BUFFER : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                int isend = *(int *)OptionValue;
                if (isend <= 0) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_SEND_BUFFER : out of bounds\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                if (pConn) {
                    if (pConn->eStage == INITED)
                        pConn->imax_send = isend;
                    else {
                        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_SEND_BUFFER : already connected\n"));
                        iRet = WSAEISCONN;
                    }

                    break;
                }

                pOpts->imax_send = isend;
                pOpts->uiConfigMask |= OPT_CONFIG_MAX_SEND;

                break;
            }

        case SO_BTH_SET_RECV_BUFFER:    // optlen=sizeof(unsigned int), optval = &max buffered size for recv
            {
                if (OptionLength != sizeof(int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_RECV_BUFFER : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                int irecv = *(int *)OptionValue;
                if (irecv <= 0) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_RECV_BUFFER : out of bounds\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                if (pConn) {
                    if (pConn->eStage == INITED) {
                        pConn->imax_recv = irecv;
                        if ((pConn->xoff_lim > irecv) || (pConn->xon_lim > irecv)) {
                            pConn->xoff_lim = irecv / 5;
                            pConn->xon_lim  = (irecv / 5) * 4;
                        }
                    } else {
                        IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SET_RECV_BUFFER : already connected\n"));
                        iRet = WSAEISCONN;
                    }

                    break;
                }

                pOpts->imax_recv = irecv;
                if ((pOpts->xoff_lim > irecv) || (pOpts->xon_lim > irecv)) {
                    pOpts->xoff_lim = irecv / 5;
                    pOpts->xon_lim  = (irecv / 5) * 4;
                }

                pOpts->uiConfigMask |= OPT_CONFIG_MAX_RECV;
                break;
            }

        case SO_BTH_SEND_MSC:    // connected only! optlen=2*sizeof(unsigned int), optval = &{v24, br}
            {
                if (OptionLength != 2*sizeof(unsigned int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SEND_MSC : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                unsigned char v24 = (unsigned char)(((int *)OptionValue)[0] & (TDIR_V24_RTC | TDIR_V24_RTR | TDIR_V24_IC));
                unsigned char br  = (unsigned char)(((int *)OptionValue)[1]);

                if (pConn && (pConn->eStage == OPENED)) {
                    iRet = SendMSC (pConn, v24, br);
                } else {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SEND_MSC : NOT CONNECTED\n"));
                    iRet = WSAENOTCONN;
                }
                break;
            }

        case SO_BTH_SEND_RLS:    // connected only! optlen=sizeof(unsigned int), optval = &rls
            {
                if (OptionLength != sizeof(unsigned int)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SEND_RLS : length\n"));
                    iRet = WSAEINVAL;
                    break;
                }

                unsigned char rls = (*(unsigned int *)OptionValue) & 0xf;

                if (pConn && (pConn->eStage == OPENED)) {
                    iRet = SendRLS (pConn, rls);
                } else {
                    IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid SO_BTH_SEND_RLS : NOT CONNECTED\n"));
                    iRet = WSAENOTCONN;
                }
                break;
            }

        default:
            IFDBG(DebugOut (DEBUG_WARN, L"[TDI] BTR_SetSocketInformation : invalid socket option %d\n", OptionName));
            iRet = WSAENOPROTOOPT;
        }
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] BTR_SetSocketInformation : Exception!\n"));
        iRet = WSAEFAULT;
    }

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_SetSocketInformation :: %d\n", iRet));
    gpTDIR->Unlock ();
    return iRet;
}

////////////////////////////////////////////////////////////////////////////////
//    BT_EnumProtocols()
//        
//    Description:
//        Called at stack load time.
//        Fill up the PROTOCOL_INFO structure supported by this stack driver.
//        
//    Arguments:
//        piProtocols    ::
//            Ignored/Not used.
//
//        pszTransportKeyName ::
//            Ignored/Not used.
//
//        lpvProtocolBuffer::
//            The buffer to return the PROTOCOL_INFO structure.
//
//        lpdwBufferLength::
//            Pointer to the length of the buffer pointed to be lpvProtocolBuffer.
//
//    Returns:
//
//    Comments:
//        Called during stack load time.
//
//
static TDI_STATUS BTR_EnumProtocols
(
LPINT lpiProtocols,
LPTSTR lpTransportKeyName,
LPVOID lpProtocolBuffer,
LPDWORD lpdwBufferLength
) {
    DWORD            dwBytesRequired;
    PPROTOCOL_INFO    pProtocolInfo;

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_EnumProtocols\n"));
    
    dwBytesRequired = sizeof(PROTOCOL_INFO) + sizeof(RFCOMM_TRANSPORT_NAME);

    if (dwBytesRequired > *lpdwBufferLength) {
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"-BTR_EnumProtocols : not enough space\n"));
        *lpdwBufferLength = dwBytesRequired;
        return -1;
    }


    ////////////////////////////////////////////////////////////////////////////
    //    Start filling the protocol info to be returned to the AFD.
    //
    pProtocolInfo = (PROTOCOL_INFO *)lpProtocolBuffer;    
    
    pProtocolInfo->iAddressFamily = AF_BT;
    pProtocolInfo->iMaxSockAddr   = sizeof (SOCKADDR_BTH);
    pProtocolInfo->iMinSockAddr   = sizeof (SOCKADDR_BTH);
    pProtocolInfo->iSocketType    = SOCK_STREAM;
    pProtocolInfo->iProtocol      = BTHPROTO_RFCOMM;
    pProtocolInfo->dwMessageSize  = 0;   
    pProtocolInfo->dwServiceFlags = XP_GUARANTEED_DELIVERY | XP_GUARANTEED_ORDER;


    ////////////////////////////////////////////////////////////////////////////
    //    Use the end of the passed in buffer to store the name
    //
    pProtocolInfo->lpProtocol = (LPWSTR) ((LPBYTE)pProtocolInfo + sizeof(PROTOCOL_INFO));    
    wcscpy (pProtocolInfo->lpProtocol, RFCOMM_TRANSPORT_NAME);
        
    ////////////////////////////////////////////////////////////////////////////
    //    Indicate one protocol info structure returned.
    //
    *lpdwBufferLength = dwBytesRequired;

    IFDBG(DebugOut (DEBUG_TDI_TRACE, L"+BTR_EnumProtocols\n"));
    return 1;    
}

////////////////////////////////////////////////////////////////////////////////
//    TDI dispatch table returned to AFD.
//
const 
TDIDispatchTable BTR_DispatchTable =
{
    BTR_OpenAddressEntry,
    BTR_CloseAddressEntry,
    BTR_OpenConnectionEntry,
    BTR_CloseConnectionEntry,
    BTR_AssociateAddressEntry,
    BTR_DisAssociateAddressEntry,
    BTR_ConnectEntry,
    BTR_DisconnectEntry,
    BTR_ListenEntry,
    BTR_AcceptEntry,
    BTR_ReceiveEntry,
    BTR_SendEntry,
    BTR_SendDatagramEntry,
    BTR_ReceiveDatagramEntry,
    BTR_SetEventEntry,
    BTR_QueryInformationEntry,
    BTR_SetInformationEntry,
    BTR_ActionEntry,
    BTR_QueryInformationExEntry,
    BTR_SetInformationExEntry,
    BTR_GetSockaddrType,
    BTR_GetWildcardSockaddr,
    BTR_GetSocketInformation,
    BTR_GetWinsockMapping,
    BTR_Notify,
    BTR_OpenSocket,
    BTR_SetSocketInformation,
    BTR_EnumProtocols
};    

////////////////////////////////////////////////////////////////////////////////
//    Register()
//        
//    Description:
//        AFD entry point to register protocol.
//        
//    Arguments:
//        pRegister    ::    Function pointer to register TDI dispatch table 
//                        with AFD.
//        pAfdCS        ::    Must be NULL. Not required anymore by AFD.
//
//    Returns:
//
//    Comments:
//
//
extern "C" void
Register(
    PWSRegister            pRegister, 
    CRITICAL_SECTION    *pAfdCS)
{
    DebugInitialize ();

    IFDBG(DebugOut (DEBUG_TDI_INIT, L"+Register\n"));

    ////////////////////////////////////////////////////////////////////////////
    //    Initialize the TDI.
    //

#if defined (SDK_BUILD)
    int CheckTime (void);

    if (! CheckTime ())
        return;
#endif

    ////////////////////////////////////////////////////////////////////////////
    //    Early initialization the BT stack..
    //        

    ////////////////////////////////////////////////////////////////////////////
    //    Register the stack driver with the AFD.
    //
    if (pRegister(BLUETOOTH_TRANSPORT_NAME, 
            (TDIDispatchTable *)&BTR_DispatchTable) != TDI_SUCCESS)
    {
        IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] failed to register with AFD\n"));
    } else {
        HANDLE hDevice = RegisterDevice (L"BTD", 0, L"btd.dll", 0);
        IFDBG(DebugOut (DEBUG_TDI_TRACE, L"hDevice = 0x%08x GetLastError = 0x%08x (%d)\n", hDevice, GetLastError (), GetLastError ()));

        HANDLE hEvent = OpenEvent (EVENT_ALL_ACCESS, FALSE, BTH_NAMEDEVENT_STACK_INITED);
        if (! hEvent) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[TDI] Failed to open event.\n"));
            return;
        }

        WaitForSingleObject (hEvent, INFINITE);
        CloseHandle (hEvent);
        
        if (ERROR_SUCCESS == tdiR_InitializeOnce ())
            tdiR_CreateDriverInstance ();
    }
}    //    Register()

//
//    Memory allocation
//
void *RFCOMM_ADDRESS_OBJECT::operator new (size_t size) {
    SVSUTIL_ASSERT (size == sizeof(RFCOMM_ADDRESS_OBJECT));

    return svsutil_GetFixed (gpTDIR->pfmdAO);
}

void RFCOMM_ADDRESS_OBJECT::operator delete (void *ptr) {
    svsutil_FreeFixed (ptr, gpTDIR->pfmdAO);
}

void *RFCOMM_CONNECTION_OBJECT::operator new (size_t size) {
    SVSUTIL_ASSERT (size == sizeof(RFCOMM_CONNECTION_OBJECT));

    return svsutil_GetFixed (gpTDIR->pfmdCO);
}

void RFCOMM_CONNECTION_OBJECT::operator delete (void *ptr) {
    svsutil_FreeFixed (ptr, gpTDIR->pfmdCO);
}

void *RFCOMM_SOCKET_OBJECT::operator new (size_t size) {
    SVSUTIL_ASSERT (size == sizeof(RFCOMM_SOCKET_OBJECT));

    return svsutil_GetFixed (gpTDIR->pfmdSO);
}

void RFCOMM_SOCKET_OBJECT::operator delete (void *ptr) {
    svsutil_FreeFixed (ptr, gpTDIR->pfmdSO);
}

void *RFCOMM_SEND_OBJECT::operator new (size_t size) {
    SVSUTIL_ASSERT (size == sizeof(RFCOMM_SEND_OBJECT));

    return svsutil_GetFixed (gpTDIR->pfmdSEND);
}

void RFCOMM_SEND_OBJECT::operator delete (void *ptr) {
    svsutil_FreeFixed (ptr, gpTDIR->pfmdSEND);
}

void *RFCOMM_RECEIVE_OBJECT::operator new (size_t size) {
    SVSUTIL_ASSERT (size == sizeof(RFCOMM_RECEIVE_OBJECT));

    return svsutil_GetFixed (gpTDIR->pfmdRECV);
}

void RFCOMM_RECEIVE_OBJECT::operator delete (void *ptr) {
    svsutil_FreeFixed (ptr, gpTDIR->pfmdRECV);
}

void *RFCOMM_PACKET_OBJECT::operator new (size_t size) {
    SVSUTIL_ASSERT (size == sizeof(RFCOMM_PACKET_OBJECT));

    return svsutil_GetFixed (gpTDIR->pfmdPL);
}

void RFCOMM_PACKET_OBJECT::operator delete (void *ptr) {
    svsutil_FreeFixed (ptr, gpTDIR->pfmdPL);
}

void *RFCOMM_PN_OBJECT::operator new (size_t size) {
    SVSUTIL_ASSERT (size == sizeof(RFCOMM_PN_OBJECT));

    return svsutil_GetFixed (gpTDIR->pfmdPN);
}

void RFCOMM_PN_OBJECT::operator delete (void *ptr) {
    svsutil_FreeFixed (ptr, gpTDIR->pfmdPN);
}

//
//    Console output
//
#if defined (BTH_CONSOLE)

int tdiR_ProcessConsoleCommand (WCHAR *pszCommand) {
    if (! gpTDIR) {
        DebugOut (DEBUG_OUTPUT, L"TDI help : service not initialized\n");
        return ERROR_SUCCESS;
    }

    gpTDIR->Lock ();
    int iRes = ERROR_SUCCESS;
    __try {
        if (wcsicmp (pszCommand, L"help") == 0) {
            DebugOut (DEBUG_OUTPUT, L"TDI Commands:\n");
            DebugOut (DEBUG_OUTPUT, L"    help        prints this text\n");
            DebugOut (DEBUG_OUTPUT, L"    global      dumps global state\n");
            DebugOut (DEBUG_OUTPUT, L"    conn        dumps currently installed connection objects\n");
            DebugOut (DEBUG_OUTPUT, L"    addr        dumps currently installed address objects\n");
            DebugOut (DEBUG_OUTPUT, L"    socket      dumps currently installed socket objects\n");
        } else if (wcsicmp (pszCommand, L"global") == 0) {
            DebugOut (DEBUG_OUTPUT, L"Global state : %s Initialized,  %s Connected\n", gpTDIR->fIsInitialized ? L"" : L"Not", gpTDIR->fIsConnected ? L"" : L"Not");
            DebugOut (DEBUG_OUTPUT, L"RFCOMM Handle   : 0x%08x\n", gpTDIR->hRFCOMM);
            DebugOut (DEBUG_OUTPUT, L"SDP Handle      : 0x%08x\n", gpTDIR->hSDP);
            DebugOut (DEBUG_OUTPUT, L"Device headers  : %d\n", gpTDIR->cDeviceHeader);
            DebugOut (DEBUG_OUTPUT, L"Device trailers : %d\n", gpTDIR->cDeviceTrailer);
            int iCount = 0;
            RFCOMM_ADDRESS_OBJECT *pAddr = gpTDIR->paolist;
            while (pAddr) {
                iCount++;
                pAddr = pAddr->pNext;
            }
            DebugOut (DEBUG_OUTPUT, L"Address objects : %d\n", iCount);
            iCount = 0;
            RFCOMM_CONNECTION_OBJECT *pConn = gpTDIR->pcolist;
            while (pConn) {
                iCount++;
                pConn = pConn->pNext;
            }
            DebugOut (DEBUG_OUTPUT, L"Connect objects : %d\n", iCount);
            iCount = 0;
            RFCOMM_SOCKET_OBJECT *pSock = gpTDIR->psolist;
            while (pSock) {
                ++iCount;
                pSock = pSock->pNext;
            }
            DebugOut (DEBUG_OUTPUT, L"Socket objects  : %d\n", iCount);
            DebugOut (DEBUG_OUTPUT, L"Address descr   : 0x%08x\n", gpTDIR->pfmdAO);
            DebugOut (DEBUG_OUTPUT, L"Conn descr      : 0x%08x\n", gpTDIR->pfmdCO);
            DebugOut (DEBUG_OUTPUT, L"Socket descr    : 0x%08x\n", gpTDIR->pfmdSO);
            DebugOut (DEBUG_OUTPUT, L"Send descr      : 0x%08x\n", gpTDIR->pfmdSEND);
            DebugOut (DEBUG_OUTPUT, L"Recv descr      : 0x%08x\n", gpTDIR->pfmdRECV);
            DebugOut (DEBUG_OUTPUT, L"Packet descr    : 0x%08x\n", gpTDIR->pfmdPL);
        } else if (wcsicmp (pszCommand, L"conn") == 0) {
            RFCOMM_CONNECTION_OBJECT *pConn = gpTDIR->pcolist;
            while (pConn) {
                DebugOut (DEBUG_OUTPUT, L"Connection object :         0x%08x\n", pConn);
                DebugOut (DEBUG_OUTPUT, L"Connect ctx:                0x%08x\n", pConn->pConnectionContext);
                DebugOut (DEBUG_OUTPUT, L"Addr :                      0x%08x %04x%08x 0x%02x\n", pConn->pAddr,
                    pConn->pAddr ? pConn->pAddr->b.NAP : 0, pConn->pAddr ? pConn->pAddr->b.SAP : 0,
                    pConn->pAddr ? pConn->pAddr->ch : 0);
                DebugOut (DEBUG_OUTPUT, L"Stage :                     %s\n",
                    (pConn->eStage == ALLOCATED ? L"ALLOCATED" : 
                    (pConn->eStage == INITED ? L"INITED" : 
                    (pConn->eStage == OPENING ? L"OPENING" : 
                    (pConn->eStage == OPENED ? L"OPENED" : 
                    (pConn->eStage == CLOSED ? L"CLOSED" : L"Error -- Undefined!\n"))))));

                DebugOut (DEBUG_OUTPUT, L"Connection handle :         0x%08x\n", pConn->hConnect);
                DebugOut (DEBUG_OUTPUT, L"MTU       :                 %d\n", pConn->mtu);
                DebugOut (DEBUG_OUTPUT, L"MaxMTU    :                 %d\n", pConn->imax_mtu);
                DebugOut (DEBUG_OUTPUT, L"MinMTU    :                 %d\n", pConn->imin_mtu);
                DebugOut (DEBUG_OUTPUT, L"xon_lim   :                 %d\n", pConn->xon_lim);
                DebugOut (DEBUG_OUTPUT, L"xoff_lim  :                 %d\n", pConn->xoff_lim);
                DebugOut (DEBUG_OUTPUT, L"imax_send :                 %d\n", pConn->imax_send);
                DebugOut (DEBUG_OUTPUT, L"imax_recv :                 %d\n", pConn->imax_recv);
                DebugOut (DEBUG_OUTPUT, L"isent :                     %d\n", pConn->isent);
                DebugOut (DEBUG_OUTPUT, L"irecvd :                    %d\n", pConn->irecvd);
                DebugOut (DEBUG_OUTPUT, L"v24 :                       0x%02x\n", pConn->v24in);
                DebugOut (DEBUG_OUTPUT, L"br :                        0x%02x\n", pConn->brin);
                DebugOut (DEBUG_OUTPUT, L"rls :                       0x%02x\n", pConn->rlsin);
                DebugOut (DEBUG_OUTPUT, L"Connect complete evt:       0x%08x\n", pConn->pConnectCompleteEvent);
                DebugOut (DEBUG_OUTPUT, L"Connect complete ctx:       0x%08x\n", pConn->pConnectCompleteContext);
                DebugOut (DEBUG_OUTPUT, L"Local FC :                  %s\n", pConn->fc_local ? L"yes" : L"no");
                DebugOut (DEBUG_OUTPUT, L"Remote FC :                 %s\n", pConn->fc_remote ? L"yes" : L"no");
                DebugOut (DEBUG_OUTPUT, L"Aggregate :                 %s\n", pConn->fc_aggregate ? L"yes" : L"no");
                DebugOut (DEBUG_OUTPUT, L"Data Indicated :            %d\n", pConn->cbDataIndicated);
                DebugOut (DEBUG_OUTPUT, L"Send List:\n");

                RFCOMM_SEND_OBJECT            *pSendList = pConn->pSendList;
                while (pSendList) {
                    DebugOut (DEBUG_OUTPUT, L"    NDIS 0x%08x size %d cfm %d used %d callback %d ctx %d\n", pSendList->pNdisBuffer, QueryNdisSize (pSendList->pNdisBuffer), pSendList->cBytesConfirmed, pSendList->cBytesUsed, pSendList->pSendCallback, pSendList->pSendContext);
                    pSendList = pSendList->pNext;
                }

                DebugOut (DEBUG_OUTPUT, L"Packets:\n");
                RFCOMM_PACKET_OBJECT        *pPacketList = pConn->pPacketList;
                while (pPacketList) {
                    DebugOut (DEBUG_OUTPUT, L"Pkt 0x%08x bytes %d\n", pPacketList, pPacketList->cBytesSent);
                    pPacketList = pPacketList->pNext;
                }

                DebugOut (DEBUG_OUTPUT, L"Recv blocks:\n");
                RFCOMM_RECEIVE_OBJECT        *pRecvList = pConn->pRecvList;
                while (pRecvList) {
                    if (pRecvList->pNdisBuffer) {
                        DWORD dwPerms = SetProcPermissions (pRecvList->dwPerms);
                        BOOL  bkm = SetKMode (TRUE);

                        int iSize = QueryNdisSize (pRecvList->pNdisBuffer);

                        SetKMode (bkm);
                        SetProcPermissions (pRecvList->dwPerms);

                        DebugOut (DEBUG_OUTPUT, L"    RECV 0x%08x NDIS 0x%08x Size %d Used %d PERMS 0x%08x Ctx 0x%08x cb 0x%08x\n",
                            pRecvList, pRecvList->pNdisBuffer, iSize, pRecvList->iNdisUsed, pRecvList->dwPerms, pRecvList->pRecvContext, pRecvList->pRecvEvent);
                    } else
                        DebugOut (DEBUG_OUTPUT, L"    RECV 0x%08x BUFF 0x%08x bytes %d size %d head %d tail %d\n",
                            pRecvList, pRecvList->pBuffer, BufferTotal (pRecvList->pBuffer), pRecvList->pBuffer->cSize,
                            pRecvList->pBuffer->cStart, pRecvList->pBuffer->cEnd);

                    pRecvList = pRecvList->pNext;
                }

                pConn = pConn->pNext;
            }

        } else if (wcsicmp (pszCommand, L"addr") == 0) {
            RFCOMM_ADDRESS_OBJECT *pAddr = gpTDIR->paolist;
            while (pAddr) {
                DebugOut (DEBUG_OUTPUT, L"Address   : 0x%08x\n", pAddr);
                DebugOut (DEBUG_OUTPUT, L"BD_ADDR   : %04x%08x\n", pAddr->b.NAP, pAddr->b.SAP);
                DebugOut (DEBUG_OUTPUT, L"Channel   : 0x%02x\n", pAddr->ch);
                DebugOut (DEBUG_OUTPUT, L"Listening : %s\n", pAddr->fListenOn ? L"yes" : L"no");
                DebugOut (DEBUG_OUTPUT, L"MTU       : %d\n", pAddr->mtu);
                DebugOut (DEBUG_OUTPUT, L"MaxMTU    : %d\n", pAddr->imax_mtu);
                DebugOut (DEBUG_OUTPUT, L"MinMTU    : %d\n", pAddr->imin_mtu);
                DebugOut (DEBUG_OUTPUT, L"xon_lim   : %d\n", pAddr->xon_lim);
                DebugOut (DEBUG_OUTPUT, L"xoff_lim  : %d\n", pAddr->xoff_lim);
                DebugOut (DEBUG_OUTPUT, L"imax_send : %d\n", pAddr->imax_send);
                DebugOut (DEBUG_OUTPUT, L"imax_recv : %d\n", pAddr->imax_recv);
                DebugOut (DEBUG_OUTPUT, L"pEventConnect            : 0x%08x\n", pAddr->pEventConnect);
                DebugOut (DEBUG_OUTPUT, L"pEventConnectContext     : 0x%08x\n", pAddr->pEventConnectContext);
                DebugOut (DEBUG_OUTPUT, L"pEventDisconnect         : 0x%08x\n", pAddr->pEventDisconnect);
                DebugOut (DEBUG_OUTPUT, L"pEventDisconnectContext  : 0x%08x\n", pAddr->pEventDisconnectContext);
                DebugOut (DEBUG_OUTPUT, L"pEventError              : 0x%08x\n", pAddr->pEventError);
                DebugOut (DEBUG_OUTPUT, L"pEventErrorContext       : 0x%08x\n", pAddr->pEventErrorContext);
                DebugOut (DEBUG_OUTPUT, L"pEventReceive            : 0x%08x\n", pAddr->pEventReceive);
                DebugOut (DEBUG_OUTPUT, L"pEventReceiveContext     : 0x%08x\n", pAddr->pEventReceiveContext);
                DebugOut (DEBUG_OUTPUT, L"pEventReceiveDgm         : 0x%08x\n", pAddr->pEventReceiveDatagram);
                DebugOut (DEBUG_OUTPUT, L"pEventReceiveDgmContext  : 0x%08x\n", pAddr->pEventReceiveDatagramContext);
                DebugOut (DEBUG_OUTPUT, L"pEventReceiveExp         : 0x%08x\n", pAddr->pEventReceiveExpedited);
                DebugOut (DEBUG_OUTPUT, L"pEventReceiveExpContext  : 0x%08x\n", pAddr->pEventReceiveExpeditedContext);

                DebugOut (DEBUG_OUTPUT, L"\n\n");
                pAddr = pAddr->pNext;
            }
        } else if (wcsicmp (pszCommand, L"socket") == 0) {
            RFCOMM_SOCKET_OBJECT *pSock = gpTDIR->psolist;
            while (pSock) {
                DebugOut (DEBUG_OUTPUT, L"Socket 0x%08x Addr 0x%08x\n", pSock, pSock->pao);
                pSock = pSock->pNext;
            }
        } else
            iRes = ERROR_UNKNOWN_FEATURE;
    } __except (1) {
        DebugOut (DEBUG_ERROR, L"Exception in TDI dump!\n");
    }

    gpTDIR->Unlock ();

    return iRes;
}

#endif // BTH_CONSOLE

#else
#include <windows.h>

extern "C" void
Register
(
void *pVoid1,
void *pVoid2
){
}

int tdiR_InitializeOnce (void) {
    return TRUE;
}

int tdiR_UninitializeOnce (void) {
    return TRUE;
}

int tdiR_CreateDriverInstance (void) {
    return ERROR_SUCCESS;
}

int tdiR_CloseDriverInstance (void) {
    return ERROR_SUCCESS;
}

int tdiR_ProcessConsoleCommand (WCHAR *pszCommand) { return 0; }

#endif
