//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************
* 
*
*       irdatdi.h
*
*       (mikezin)
*/

#ifndef _IRDATDI_H_
#define _IRDATDI_H_

#include "windows.h"
#include "memory.h"
#include "winsock.h"
#include "nspapi.h"
#include "cxport.h"
#include "ndis.h"
#include "tdi.h"
#include "tdistat.h"
#include "tdice.h"
#include "tdiinfo.h"
#include "ipexport.h"
#include "tcpinfo.h"
#include "linklist.h"
#include "cclib.h"
#include "af_irda.h"
#include "irda.h"
#include "irlmp.h"
#include "irerr.h"

#define IRDATDI_RECVQ_LEN 4096

// #undef MIN
// #define MIN(a, b) ((a) < (b) ? (a) : (b))

// Defined in irlaplog.c.
extern TCHAR *IRDA_PrimStr[];

#define IRLMP_TRANSPORT_NAME    TEXT("MSIrDA")

#define IRDA_PORT               1
#define IRDA_INTERNAL_IR        FALSE

#define DISCOVERY_HINT_CHARSET  0x820400
#define DISCOVERY_NICKNAME      "Aoxomoxoa"
#define DISCOVERY_NICKNAME_LEN  9
#define DISCOVERY_SLOTS         8

#define IAS_DEVICE_NAME         "Aoxomoxoa (IAS)"
#define IAS_DEVICE_NAME_LEN     16

#define LOCAL_QOS_BPS           BPS_9600       | \
                                BPS_19200      | \
                                BPS_115200

#define LOCAL_QOS_MAX_TURN      MAX_TAT_500

#define LOCAL_QOS_FRAME_LEN     DATA_SIZE_64   | \
                                DATA_SIZE_128  | \
                                DATA_SIZE_256

#define LOCAL_QOS_WINDOW        FRAMES_1       | \
                                FRAMES_2       | \
                                FRAMES_3

#define LOCAL_QOS_BOFS          BOFS_3

#define LOCAL_QOS_MIN_TAT       MIN_TAT_10

#define LOCAL_QOS_DISC_TIME     DISC_TIME_12   | \
								DISC_TIME_8    | \
								DISC_TIME_3

#define HIGHEST_LSAP_SEL        255

#define TINY_TP_RECV_MAX_SDU    0
#define TINY_TP_RECV_CREDITS    4

#define DISCOVERY_REPEATS       6

#define LSAPSEL_TXT     "LSAP-SEL"
#define LSAPSEL_TXTLEN  8

typedef enum 
{
    IRLMP_CONN_CREATED,
    IRLMP_CONN_CLOSING,
    IRLMP_CONN_OPENING,
    IRLMP_CONN_OPEN
} IRLMP_CONN_STATES;

typedef void (* PIRLMP_CONNECT_COMPLETE)(PVOID          Context, 
                                         TDI_STATUS     FinalStatus, 
                                         unsigned long  ByteCount);

typedef void (* PIRLMP_SEND_COMPLETE)   (PVOID          Context,
                                         TDI_STATUS     TdiStatus,
                                         DWORD          BytesSent);

typedef void (* PIRLMP_RECV_COMPLETE)   (PVOID          Context,
                                         TDI_STATUS     TdiStatus,
                                         DWORD          BytesRecvd);

typedef struct _IRDATDI_RECV_BUFF
{
    LIST_ENTRY  Linkage;
    int         DataLen;
    BYTE       *pRead;
    BYTE        FinalSeg;
    BYTE        Data[1];
} IRDATDI_RECV_BUFF, *PIRDATDI_RECV_BUFF;

typedef struct _WSHIRDA_IAS_ATTRIB
{
    PVOID AttributeHandle;
    struct _WSHIRDA_IAS_ATTRIB *pNext;
} WSHIRDA_IAS_ATTRIB, *PWSHIRDA_IAS_ATTRIB;

typedef struct _IRLMP_SOCKET_CONTEXT
{
    LIST_ENTRY                  Linkage;
    BOOL                        UseExclusiveMode;
    BOOL                        UseIrLPTMode;
    BOOL                        Use9WireMode;
	BOOL						UseSharpMode;
    PWSHIRDA_IAS_ATTRIB         pIasAttribs;
} IRLMP_SOCKET_CONTEXT, *PIRLMP_SOCKET_CONTEXT;

typedef struct _IRLMP_CONNECTION *PIRLMP_CONNECTION;

typedef struct _IRLMP_ADDR_OBJ
{
    // Linkage must be first.
    LIST_ENTRY                  Linkage;
    CTELock                     Lock;
    LIST_ENTRY                  ConnList;
    uint                        Protocol;
    BOOL                        IsServer;
    SOCKADDR_IRDA		        SockAddrLocal;
    int                         LSAPSelLocal;
    BOOL                        UseExclusiveMode;
    BOOL                        UseIrLPTMode;
    BOOL                        Use9WireMode;
	BOOL						UseSharpMode;
    PVOID                       IasAttribHandle;
    PConnectEvent               pEventConnect;
    PVOID                       pEventConnectContext;
    PDisconnectEvent            pEventDisconnect;
    PVOID                       pEventDisconnectContext;
    PErrorEvent                 pEventError;
    PVOID                       pEventErrorContext;
    PRcvEvent                   pEventReceive;
    PVOID                       pEventReceiveContext;
    PRcvDGEvent                 pEventReceiveDatagram;
    PVOID                       pEventReceiveDatagramContext;
    PRcvExpEvent                pEventReceiveExpedited;
    PVOID                       pEventReceiveExpeditedContext;
#ifdef DEBUG
    DWORD                       Sig;
    DWORD                       dwId;
#endif 
    DWORD                       cRefs;
} IRLMP_ADDR_OBJ, *PIRLMP_ADDR_OBJ;

typedef struct _IRLMP_CONNECTION
{
    // Linkage must be first.
    LIST_ENTRY                  Linkage;
    CTELock                     Lock;
    PIRLMP_ADDR_OBJ             pAddrObj;
    LIST_ENTRY                  RecvBuffList;
    NDIS_BUFFER                *pUsrNDISBuff;
    int                         UsrBuffLen;
    DWORD                       UsrBuffPerm;
    int                         RecvQBytes;
    IRLMP_CONN_STATES           ConnState;
    BOOL                        fConnPending;
    BOOL                        IsServer;
	SOCKADDR_IRDA		        SockAddrLocal;
    int                         LSAPSelLocal;
	SOCKADDR_IRDA		        SockAddrRemote;
    int                         LSAPSelRemote;
    BOOL                        UseExclusiveMode;
    BOOL                        UseIrLPTMode;
    BOOL                        Use9WireMode;
	BOOL						UseSharpMode;
    int                         SendMaxSDU;
    int                         SendMaxPDU;
    int                         TinyTPRecvCreditsLeft;
    BOOL                        RecvBusy;
    int                         IndicatedNotAccepted;
    int                         NotIndicated;
    TDI_STATUS                  TdiStatus;
    PVOID                       pIrLMPContext;
	PVOID                       pConnectionContext;
    PIRLMP_CONNECT_COMPLETE     pConnectComplete;
    PVOID                       pConnectCompleteContext;
    PIRLMP_RECV_COMPLETE        pRecvComplete;
    PVOID                       pRecvCompleteContext;
    CTEEvent                    LingerTimer;
    DWORD                       LingerTime;
    DWORD                       SendsRemaining;
    DWORD                       LastSendsRemaining; // for graceful close
#ifdef DEBUG
    DWORD                       Sig;       
    DWORD                       dwId;
#endif 
    DWORD                       cRefs;
} IRLMP_CONNECTION, *PIRLMP_CONNECTION;

#ifdef DEBUG
// future - can put debug tracing for locks.
#define GET_CONN_LOCK(pConn)    CTEGetLock(&pConn->Lock, NULL)
#define FREE_CONN_LOCK(pConn)   CTEFreeLock(&pConn->Lock, NULL)
#define GET_ADDR_LOCK(pAddr)    CTEGetLock(&pAddr->Lock, NULL)
#define FREE_ADDR_LOCK(pAddr)   CTEFreeLock(&pAddr->Lock, NULL)
#else
#define GET_CONN_LOCK(pConn)    CTEGetLock(&pConn->Lock, NULL)
#define FREE_CONN_LOCK(pConn)   CTEFreeLock(&pConn->Lock, NULL)
#define GET_ADDR_LOCK(pAddr)    CTEGetLock(&pAddr->Lock, NULL)
#define FREE_ADDR_LOCK(pAddr)   CTEFreeLock(&pAddr->Lock, NULL)
#endif // DEBUG

extern BOOL             DscvInProgress;
extern HANDLE           DscvConfEvent;
extern HANDLE			DscvIndEvent;
extern PDEVICELIST      pDscvDevList;
extern int              DscvNumDevices;
extern DWORD            DscvCallersPermissions;
extern int              DscvRepeats;
extern CRITICAL_SECTION csDscv;

extern BOOL             IASQueryInProgress;
extern HANDLE           IASQueryConfEvent;
extern PIAS_QUERY       pIASQuery;
extern int              IASQueryStatus;
extern CRITICAL_SECTION csIasQuery;

extern int              OpenSocketCnt;

extern IAS_QUERY        ConnectIASQuery;

extern CRITICAL_SECTION csIrObjList;
extern LIST_ENTRY       IrAddrObjList;
extern LIST_ENTRY       IrConnObjList;

// TDI entry points.
TDI_STATUS
IRLMP_OpenAddress(PTDI_REQUEST       pTDIReq,
                  PTRANSPORT_ADDRESS pAddr,
                  uint               Protocol,
                  PVOID              pOptions);

TDI_STATUS
IRLMP_CloseAddress(PTDI_REQUEST pTDIReq);

TDI_STATUS 
IRLMP_OpenConnection(PTDI_REQUEST pTDIReq,
                     PVOID        pContext);

TDI_STATUS 
IRLMP_CloseConnection(PTDI_REQUEST pTDIReq);

TDI_STATUS
IRLMP_AssociateAddress(PTDI_REQUEST pTDIReq,
                       HANDLE       AddrHandle);

TDI_STATUS
IRLMP_DisAssociateAddress(PTDI_REQUEST pTDIReq);

TDI_STATUS
IRLMP_Connect(PTDI_REQUEST                  pTDIReq,
              PVOID                         pTimeOut,
              PTDI_CONNECTION_INFORMATION   pReqAddr,
              PTDI_CONNECTION_INFORMATION   pRetAddr);

TDI_STATUS 
IRLMP_Disconnect(PTDI_REQUEST                pTDIReq,
                 PVOID                       pTimeOut,
                 ushort                      Flags,
                 PTDI_CONNECTION_INFORMATION pDiscInfo,
                 PTDI_CONNECTION_INFORMATION pRetInfo);

TDI_STATUS 
IRLMP_Listen(PTDI_REQUEST                pTDIReq,
             ushort                      Flags,
             PTDI_CONNECTION_INFORMATION pAcceptableInfo,
             PTDI_CONNECTION_INFORMATION pConnectedInfo);

TDI_STATUS 
IRLMP_Accept(PTDI_REQUEST                pTDIReq,
             PTDI_CONNECTION_INFORMATION pAcceptInfo,
             PTDI_CONNECTION_INFORMATION pConnectedInfo);

TDI_STATUS
IRLMP_Receive(PTDI_REQUEST pTDIReq,
              ushort *     pFlags,
              uint *       pRecvLen, 
              PNDIS_BUFFER pNDISBuf);

TDI_STATUS 
IRLMP_Send(PTDI_REQUEST pTDIReq,
           ushort       Flags,
           uint         SendLen,
           PNDIS_BUFFER pNDISBuf);

TDI_STATUS 
IRLMP_SendDatagram(PTDI_REQUEST                pTDIReq,
                   PTDI_CONNECTION_INFORMATION pConnInfo,
                   uint                        SendLen,
                   ULONG *                     pBytesSent,
                   PNDIS_BUFFER                pNDISBuf);

TDI_STATUS 
IRLMP_ReceiveDatagram(PTDI_REQUEST                pTDIReq,
                      PTDI_CONNECTION_INFORMATION pConnInfo,
                      PTDI_CONNECTION_INFORMATION pRetInfo,
                      uint                        RecvLen,
                      uint *                      pBytesRecvd,
                      PNDIS_BUFFER                pNDISBuf);

TDI_STATUS
IRLMP_SetEvent(PVOID pHandle,
               int   Type,
               PVOID pHandler,
               PVOID pContext);

TDI_STATUS
IRLMP_QueryInformation(PTDI_REQUEST pTDIReq,
                       uint         QueryType,
                       PNDIS_BUFFER pNDISBuf,
                       uint *       pBytesReturned,
                       uint         IsConn);

TDI_STATUS 
IRLMP_SetInformation(PTDI_REQUEST pTDIReq,
                     uint         SetType, 
                     PNDIS_BUFFER pNDISBuf,
                     uint         BufLen,
                     uint         IsConn);

TDI_STATUS 
IRLMP_Action(PTDI_REQUEST pTDIReq,
             uint         ActionType, 
             PNDIS_BUFFER pNDISBuf,
             uint         BufLen);

TDI_STATUS
IRLMP_QueryInformationEx(PTDI_REQUEST         pTDIReq, 
                         struct TDIObjectID * pObjId, 
                         PNDIS_BUFFER         pNDISBuf,
                         uint *               pBufLen,
                         void *               pContext);

TDI_STATUS 
IRLMP_SetInformationEx(PTDI_REQUEST         pTDIReq, 
                       struct TDIObjectID * pObjId,
                       void *               pBuf,
                       uint                 BufLen);

DWORD
IRLMP_GetWinsockMapping(OUT PWINSOCK_MAPPING pMapping,
                        IN  DWORD            MappingLength);

INT
IRLMP_EnumProtocols(IN     LPINT   lpiProtocols,
                    IN     LPTSTR  lpTransportKeyName,
                    IN OUT LPVOID  lpProtocolBuffer,
                    IN OUT LPDWORD lpdwBufferLength);

INT
IRLMP_OpenSocket(IN  OUT PINT    pAddressFamily,
                 IN  OUT PINT    pSocketType,
                 IN  OUT PINT    pProtocol,
                 OUT     PWSTR   pTransportDeviceName,
                 OUT     PVOID * ppHelperDllSocketContext,
                 OUT     PDWORD  pNotificationEvents);

INT 
IRLMP_GetSockaddrType(IN  PSOCKADDR      pSockaddr,
                      IN  DWORD          SockaddrLen,
                      OUT PSOCKADDR_INFO pSockaddrInfo);

INT
IRLMP_GetWildcardSockaddr(IN  PVOID     pHelperDllSocketContext,
                          OUT PSOCKADDR pSockaddr,
                          OUT PINT      pSockaddrLen);

INT
IRLMP_GetSocketInformation(IN  PVOID  pHelperDllSocketContext,
                           IN  SOCKET SocketHandle,
                           IN  HANDLE TdiAddressObjectHandle,
                           IN  HANDLE TdiConnectionObjectHandle,
                           IN  INT    Level,
                           IN  INT    OptionName,
                           OUT PCHAR  pOptionValue,
                           OUT PINT   pOptionLen);

INT
IRLMP_SetSocketInformation(IN PVOID  pHelperDllSocketContext,
                           IN SOCKET SocketHandle,
                           IN HANDLE TdiAddressObjectHandle,
                           IN HANDLE TdiConnectionObjectHandle,
                           IN INT    Level,
                           IN INT    OptionName,
                           IN PCHAR  pOptionValue,
                           IN INT    OptionLength);
INT 
IRLMP_Notify(IN PVOID  pHelperDllSocketContext,
             IN SOCKET SocketHandle,
             IN HANDLE TdiAddressObjectHandle,
             IN HANDLE TdiConnectionObjectHandle,
             IN DWORD  NotifyEvent);

int
InitializeIRDA(void);

DWORD 
WshIrdaInit();

DWORD 
WshIrdaDeinit();

TDI_STATUS
TdiDisassociateAddress(PIRLMP_CONNECTION pConn);

void IrdaAutoSuspend(BOOL bAddConn);

#include "tdiutil.h"

#endif // _IRDATDI_H_
