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
*  File:   irda.h
*
*  Description: Definitions used across the IRDA stack
*
*  Date:   4/15/95
*
*  This file primarily defines the IRDA message (IRDA_MSG) used for 
*  communicating with the stack and communication between the layers
*  of the stack. IRDA_MSG provides the following services: 
*       MAC_CONTROL_SERVICE
*       IRLAP_DISCOVERY_SERVICE   
*       IRDA_DISCONNECT_SERVICE   
*       IRDA_CONNECT_SERVICE      
*       IRDA_DATA_SERVICE
*       IRLMP_ACCESSMODE_SERVICE
*       IRLMP_IAS_SERVICE
*
*  IRDA_MSG usage:
*
*  +-------+
*  | IRLAP |
*  +-------+
*      | 
*      |  IrmacDown(IRDA_MSG)
*     \|/
*  +-------+
*  | IRMAC |
*  +-------+
*  |*************************************************************************|
*  | Prim                     | MsgType and parameters                       |
*  |=========================================================================|
*  | MAC_DATA_REQ             | IRDA_DATA_SERVICE                            |
*  |                          |   o IRDA_MSG_pHdrRead = start of IRDA headers|
*  |                          |   o IRDA_MSG_pHdrWrite = end of header       |
*  |                          |   o IRDA_MSG_pRead = start of data           |
*  |                          |   o IRDA_MSG_pWrite = end of data            |
*  |--------------------------+----------------------------------------------|
*  | MAC_DATA_RESP            | no parms (completetion of DATA_IND)          |
*  |--------------------------+----------------------------------------------|
*  | MAC_CONTROL_REQ          | MAC_CONTROL_SERVICE                          |
*  |                          |   o IRDA_MSG_Op = MAC_INITIALIZIE_LINK       |
*  |                          |     - IRDA_MSG_Port                          |
*  |                          |     - IRDA_MSG_Baud                          |
*  |                          |     - IRDA_MSG_MinTat = min turn time        |
*  |                          |     - IRDA_MSG_NumBOFs = # added when tx'ing |
*  |                          |     - IRDA_MSG_DataSize = max rx frame       |
*  |                          |     - IRDA_MSG_SetIR = TRUE/FALSE (does an   |
*  |                          |         EscapeComm(SETIR) to select int/ext  |
*  |                          |         dongle)                              |
*  |                          |   o IRDA_MSG_Op = MAC_MEDIA_SENSE            |
*  |                          |     - IRDA_MSG_SenseTime (in ms)             |
*  |                          |   o IRDA_MSG_Op = MAC_RECONFIG_LINK          |
*  |                          |     - IRDA_MSG_Baud                          |
*  |                          |     - IRDA_MSG_NumBOFs = # added when tx'ing |
*  |                          |     - IRDA_MSG_DataSize = max rx frame       |
*  |                          |     - IRDA_MSG_MinTat = min turn time        |
*  |                          |   o IRDA_MSG_OP = MAC_CLOSE_LINK             |
*  |                          |   o IRDA_MSG_OP = MAC_ACQUIRE_RESOURCES      |
*  |                          |     - IRDA_MSG_pMacConfig = config struct    |
*  |                          |   o IRDA_MSG_OP = MAC_RELEASE_RESOURCES      |
*  |                          |     - IRDA_MSG_pMacConfig = config struct    |
*  |-------------------------------------------------------------------------|
*
*  +-------+
*  | IRLAP |
*  +-------+
*     /|\
*      |  IrlapUp(IRDA_MSG)
*      | 
*  +-------+
*  | IRMAC |
*  +-------+
*  |*************************************************************************|
*  | Prim                     | MsgType and parameters                       |
*  |=========================================================================|
*  | MAC_DATA_IND             | IRDA_DATA_SERVICE                            |
*  |                          |   o IRDA_MSG_pRead  = start of frame         |
*  |                          |                       (includes IRLAP header)|
*  |                          |   o IRDA_MSG_pWrite = end of frame           |
*  |                          |                       (excludes FCS)         |
*  |--------------------------+----------------------------------------------|
*  | MAC_CONTROL_CONF         | MAC_CONTROL_SERVICE                          |
*  |                          |   o IRDA_MSG_Op = MAC_MEDIA_SENSE            |
*  |                          |     - IRDA_MSG_OpStatus = MAC_MEDIA_BUSY     |
*  |                          |                           MAC_MEDIA_CLEAR    |
*  |--------------------------+----------------------------------------------|
*  | MAC_DATA_CONF            | no parms                                     |
*  |-------------------------------------------------------------------------|
*
*  +-------+
*  | IRLMP |
*  +-------+
*      | 
*      |  IrlapDown(IRDA_MSG)
*     \|/
*  +-------+
*  | IRLAP |
*  +-------+
*  |*************************************************************************|
*  | Prim                     | MsgType and parameters                       |
*  |=========================================================================|
*  | IRLAP_DISCOVERY_REQ      | IRLAP_DISCOVERY_SERVICE                      |
*  |  IRLAP_Down() returns    |   o IRDA_MSG_SenseMedia = TRUE/FALSE         |
*  |  IRLAP_REMOTE_DISCOVERY_IN_PROGRESS_ERR or                              |
*  |  IRLAP_REMOTE_CONNECT_IN_PROGRESS_ERR when indicated                    |
*  |--------------------------+----------------------------------------------|
*  | IRLAP_CONNECT_REQ        | IRDA_CONNECT_SERVICE                         |
*  |                          |   o IRDA_MSG_RemoteDevAddr                   |
*  |  IRLAP_Down() returns    |                                              |
*  |  IRLAP_REMOTE_DISCOVERY_IN_PROGRESS_ERR when indicated                  |
*  |--------------------------+----------------------------------------------|
*  | IRLAP_CONNECT_RESP       | no parms                                     |
*  |--------------------------+----------------------------------------------|
*  | IRLAP_DISCONNECT_REQ     | no parms                                     |
*  |--------------------------+----------------------------------------------|
*  | IRLAP_DATA_REQ           | IRDA_DATA_SERVICE                            |
*  | IRLAP_UDATA_REQ          |   o IRDA_MSG_pHdrRead = start of IRLMP header|
*  |  IRLAP_Down() returns    |   o IRDA_MSG_pHdrWrite = end of header       |
*  |  IRLAP_REMOTE_BUSY to    |   o IRDA_MSG_pRead = start of data           |
*  |  to flow off LMP.        |   o IRDA_MSG_pWrite = end of data            |
*  |-------------------------------------------------------------------------|
*  | IRLAP_FLOWON_REQ         | no parms                                     |
*  |-------------------------------------------------------------------------|
*  | IRLAP_LINKCONTROL_REQ    | IRDA_LINKCONTROL_SERVICE                     |
*  |                          |   o IRDA_MSG_pLinkContext - pIrdaLinkCb      |
*  |                          |   o IRDA_MSG_pLinkConfig - config struct     |
*  |                          |   o IRDA_MSG_AcquireResources =              |
*  |                          |                      TRUE to acquire         |
*  |                          |                      FALSE to release        |
*  |-------------------------------------------------------------------------|
*
*  +-------+
*  | IRLMP |
*  +-------+
*     /|\
*      |  IrlmpUp(IRDA_MSG)
*      | 
*  +-------+
*  | IRLAP |
*  +-------+
*  |*************************************************************************|
*  | Prim                     | MsgType and parameters                       |
*  |=========================================================================|
*  | IRLAP_DISCOVERY_IND      | IRLAP_DISCOVERY_SERVICE                      |
*  |                          |   o pDevList = Discovery info of device that |
*  |                          |                initiated discovery           |
*  |--------------------------+----------------------------------------------|
*  | IRLAP_DISCOVERY_CONF     | IRLAP_DISCOVERY_SERVICE                      |
*  |                          |   o IRDA_MSG_pDevList = list of discovered   |
*  |                          |       devices, NULL when                     |
*  |                          |       status != IRLAP_DISCOVERY_COMPLETED    |
*  |                          |   o IRDA_MSG_DscvStatus =                    |
*  |                          |       MAC_MEDIA_BUSY                         |
*  |                          |       IRLAP_REMOTE_DISCOVERY_IN_PROGRESS     |
*  |                          |       IRLAP_DISCOVERY_COLLISION              |
*  |                          |       IRLAP_REMOTE_CONNECTION_IN_PROGRESS    |
*  |                          |       IRLAP_DISCOVERY_COMPLETED              |
*  |--------------------------+----------------------------------------------|
*  | IRLAP_CONNECT_IND        | IRDA_CONNECT_SERVICE                         |
*  |                          |   o IRDA_MSG_RemoteDevAddr                   |
*  |                          |   o IRDA_MSG_pQos = Negotiated QOS           |
*  |-------------------------------------------------------------------------|
*  | IRLAP_CONNECT_CONF       | IRDA_CONNECT_SERVICE                         |
*  |                          |   o IRDA_MSG_pQos = Negotiated QOS, only when|
*  |                          |                     successful               |
*  |                          |   o IRDA_MSG_ConnStatus =                    |
*  |                          |                     IRLAP_CONNECTION_COMPLETE|
*  |--------------------------+----------------------------------------------|
*  | IRLAP_DISCONNECT_IND     | IRDA_DISCONNECT_SERVICE                      |
*  |                          |   o IRDA_MSG_DiscStatus =                    |
*  |                          |       IRLAP_DISCONNECT_COMPLETED             |
*  |                          |       IRLAP_REMOTED_INITIATED                |
*  |                          |       IRLAP_PRIMARY_CONFLICT                 |
*  |                          |       IRLAP_REMOTE_DISCOVERY_IN_PROGRESS     |
*  |                          |       IRLAP_NO_RESPONSE                      |
*  |                          |       IRLAP_DECLINE_RESET                    |
*  |                          |       MAC_MEDIA_BUSY                         |
*  |--------------------------+----------------------------------------------|
*  | IRLAP_DATA_IND           | IRDA_DATA_SERVICE                            |
*  | IRLAP_UDATA_IND          |   o IRDA_MSG_pRead  = start of IRLMP packet  |
*  |                          |   o IRDA_MSG_pWrite = end of IRLMP packet    |
*  |--------------------------+----------------------------------------------|
*  | IRLAP_DATA_CONF          | IRDA_DATA_SERVICE                            |
*  | IRLAP_UDATA_CONF         |   o IRDA_MSG_DataStatus =                    |
*  |                          |       ILAP_DATA_REQUEST_COMPLETED            |
*  |                          |       IRLAP_DATA_REQUEST_FAILED_LINK_RESET   |
*  |--------------------------+----------------------------------------------|
*  | IRLAP_STATUS_IND         | IRDA_MSG_LinkStatus                          |
*  |-------------------------------------------------------------------------|
*
*      +-----+
*      | Tdi |
*      +-----+
*         |   
*         |  IrlmpDown(IRLMPContext, IRDA_MSG)
*        \|/
*     +-------+
*     | IRLMP |
*     +-------+
*  |*************************************************************************|
*  | Prim                     | MsgType and parameters                       |
*  |=========================================================================|
*  | IRLMP_DISCOVERY_REQ      | IRLAP_DISCOVERY_SERVICE                      |
*  |                          |   o IRDA_MSG_SenseMedia = TRUE/FALSE         |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_CONNECT_REQ        | IRDA_CONNECT_SERVICE                         |
*  |   IRLMP_Down() returns   |   o IRDA_MSG_RemoteDevAddr                   |
*  |   IRLMP_LINK_IN_USE      |   o IRDA_MSG_RemoteLsapSel                   |
*  |   when the requested     |   o IRDA_MSG_pQos (may be NULL)              |
*  |   connection is to a     |   o IRDA_MSG_pConnData                       |
*  |   remote device other    |   o IRDA_MSG_ConnDataLen                     |
*  |   than the one the link  |   o IRDA_MSG_LocalLsapSel                    |
*  |   is currently connected |   o IRDA_MSG_pContext                        |
*  |   or connecting to.      |   o IRDA_MSG_UseTtp                          |
*  |                          | * o IRDA_MSG_TtpCredits                      |
*  |                          |   o IRDA_MSG_MaxSDUSize - Max size that this |
*  |                          |        IRLMP client can receive.             |
*  |                          |                                              |
*  |                          | * even if not using TTP, TtpCredits are spec-|
*  |                          |   ified for flow control in exclusive mode   |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_CONNECT_RESP       | IRDA_CONNECT_SERVICE                         |
*  |                          |   o IRDA_MSG_pConnData                       |
*  |                          |   o IRDA_MSG_ConnDataLen                     |
*  |                          |   o IRDA_MSG_pContext                        |
*  |                          |   o IRDA_MSG_MaxSDUSize - Max size that this |
*  |                          |        IRLMP client can receive.             |
*  |                          | * o IRDA_MSG_TtpCredits                      |
*  |                          | * see above                                  |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_DISCONNECT_REQ     | IRDA_DISCONNECT_SERVICE                      |
*  |                          |   o IRDA_MSG_pDiscData                       |
*  |                          |   o IRDA_MSG_DiscDataLen                     |
*  |                          |   o IRDA_MSG_pDiscContext                    |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_DATA/UDATA_REQ     | IRDA_DATA_SERVICE                            |
*  |   IRLMP_Down() may return|   o IRDA_MSG_pDataContext =ptr to NDIS_BUFFER|
*  |   IRLMP_REMOTE_BUSY,     |   o IRDA_MSG_IrCOMM_9Wire = TRUE/FALSE       |
*  |    when tx cred exhausted|                                              |
*  |    in multiplexed mode.  |                                              |
*  |   IRLAP_REMOTE_BUSY,     |                                              |
*  |    when remote IRLAP     |                                              |
*  |    flowed off in exclMode|                                              |
*  |   In either case the req |                                              |
*  |   was successful.        |                                              |
*  |-------------------------------------------------------------------------|
*  | IRLMP_ACCESSMODE_REQ     | IRLMP_ACCESSMODE_SERVICE                     |
*  |   IRLMP_Down() may return|   o IRDA_MSG_AccessMode = IRLMP_MULTIPLEXED  |
*  |   IRLMP_IN_EXCLUSIVE_MODE|                           IRLMP_EXCLUSIVE    |
*  |   if already in excl-mode|   o IRDA_MSG_IrLPTMode - TRUE, doesn't send  |
*  |   IRLMP_IN_MULTIPLEXED...|                          the Access PDU      |
*  |   if other LSAPs exist or|                                              |
*  |   requesting trans to this state when already in it.                    |
*  |-------------------------------------------------------------------------|
*  | IRLMP_MORECREDIT_REQ     | IRDA_CONNECT_SERVICE                         |
*  |                          |   o IRDA_MSG_TtpCredits                      |
*  |-------------------------------------------------------------------------|
*  | IRLMP_GETVALUEBYCLASS_REQ| IRDA_IAS_SERVICE                             |
*  |                          |   o IRDA_MSG_pIasQuery                       |
*  |                          |   o IRDA_MSG_AttribLen                       |
*  |-------------------------------------------------------------------------|
*  | IRLMP_REGISTERLSAP_REQ   | IRDA_CONNECT_SERVICE                         |
*  |                          |   o IRDA_MSG_LocalLsapSel                    |
*  |                          |   o IRDA_MSG_UseTtp                          |
*  |-------------------------------------------------------------------------|
*  | IRLMP_DEREGISTERLSAP_REQ | IRDA_CONNECT_SERVICE                         |
*  |                          |   o IRDA_MSG_LocalLsapSel                    |
*  |-------------------------------------------------------------------------|
*  | IRLMP_ADDATTRIBUTE_REQ   | IRDA_IAS_SERVICE                             |
*  |                          |   o IRDA_MSG_pIasSet                         |
*  |                          |   o IRDA_MSG_pAttribHandle (return value)    |
*  |-------------------------------------------------------------------------|
*  | IRLMP_DELATTRIBUTE_REQ   | IRDA_IAS_SERVICE                             |
*  |                          |   o IRDA_MSG_AttribHandle                    |
*  |-------------------------------------------------------------------------|
*  | IRLMP_LINKCONTROL_REQ    | IRDA_LINKCONTROL_SERVICE                     |
*  |                          |   o IRDA_MSG_AcquireResources =              |
*  |                          |                      TRUE to acquire         |
*  |                          |                      FALSE to release        |
*  |-------------------------------------------------------------------------|
*
*
*      +-----+
*      | Tdi |
*      +-----+
*        /|\   
*         |  TdiUp(TransportAPIContext, IRDA_MSG)
*         | 
*     +-------+
*     | IRLMP |
*     +-------+
*  |*************************************************************************|
*  | Prim                     | MsgType and parameters                       |
*  |=========================================================================|
*  | IRLMP_DISCOVERY_IND      | IRLAP_DISCOVERY_SERVICE                      |
*  |                          |   o pDevList = aged Discovery list           |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_DISCOVERY_CONF     | same as IRLAP_DISCOVERY_CONF. The device list|
*  |                          | however is the one maintained in IRLMP       |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_DISCONNECT_IND     | IRDA_DISCONNECT_SERVICE                      |
*  |                          |   o IRDA_MSG_DiscReason =                    |
*  |                          |       see IRLMP_DISC_REASON below            |
*  |                          |   o IRDA_MSG_pDiscData - may be NULL         |
*  |                          |   o IRDA_MSG_DiscDataLen                     |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_CONNECT_IND        | IRDA_CONNECT_SERVICE                         |
*  |                          |   o IRDA_MSG_RemoteDevAddr                   |
*  |                          |   o IRDA_MSG_RemoteLsapSel;                  |
*  |                          |   o IRDA_MSG_LocalLsapSel;                   |
*  |                          |   o IRDA_MSG_pQos                            |
*  |                          |   o IRDA_MSG_pConnData                       |
*  |                          |   o IRDA_MSG_ConnDataLen                     |
*  |                          |   o IRDA_MSG_pContext                        |
*  |                          |   o IRDA_MSG_MaxSDUSize - Max size that this |
*  |                          |        IRLMP client can send to peer         |
*  |                          |   o IRDA_MSG_MaxPDUSize                      |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_CONNECT_CONF       | IRDA_CONNECT_SERVICE                         |
*  |                          |   o IRDA_MSG_pQos                            |
*  |                          |   o IRDA_MSG_pConnData                       |
*  |                          |   o IRDA_MSG_ConnDataLen                     |
*  |                          |   o IRDA_MSG_pContext                        |
*  |                          |   o IRDA_MSG_MaxSDUSize - Max size that this |
*  |                          |        IRLMP client can send to peer         |
*  |                          |   o IRDA_MSG_MaxPDUSize                      |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_DATA_IND           | IRDA_DATA_SERVICE                            |
*  |                          |   o IRDA_MSG_pRead  = start of User Data     |
*  |                          |   o IRDA_MSG_pWrite = end of User Data       |
*  |                          |   o IRDA_MSG_SegFlags = 0 or SEG_FINAL       |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_DATA_CONF          | IRDA_DATA_SERVICE                            |
*  |                          |   o IRDA_MSG_pDataContext =ptr to NDIS_BUFFER|
*  |                          |   o IRDA_MSG_DataStatus =                    |
*  |                          |       IRLMP_DATA_REQUEST_COMPLETED           |
*  |                          |       IRLMP_DATA_REQUEST_FAILED              |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_ACCESSMODE_IND     | IRLMP_ACCESSMODE_SERVICE                     |
*  |                          |   o IRDA_MSG_AccessMode =                    |
*  |                          |         IRLMP_EXCLUSIVE                      |
*  |                          |         IRLMP_MULTIPLEXED                    |
*  |--------------------------+----------------------------------------------|
*  | IRLMP_ACCESSMODE_CONF    | IRLMP_ACCESSMODE_SERVICE                     |
*  |                          |   o IRDA_MSG_AccessMode =                    |
*  |                          |         IRLMP_EXCLUSIVE                      |
*  |                          |         IRLMP_MULTIPLEXED                    |
*  |                          |   o IRDA_MSG_ModeStatus =                    |
*  |                          |         IRLMP_ACCESSMODE_SUCCESS             |
*  |                          |         IRLMP_ACCESSMODE_FAILURE             |
*  |--------------------------+----------------------------------------------|
*  |IRLMP_GETVALUEBYCLASS_CONF| IRDA_DATA_SERVICE                            |
*  |                          |   o IRDA_MSG_pIasQuery                       |
*  |                          |   o IRDA_MSG_IASStatus = An IRLMP_DISC_REASON|
*  |                          |                          (see below)         |
*  |-------------------------------------------------------------------------|
*/

#ifndef UNDER_CE
#include <ntos.h>
#else
#include <windows.h>
#endif 

#include <cxport.h>
#include <ndis.h>
#include <af_irda.h>
#include <irerr.h>

#ifdef UNDER_CE
// #include <ntddndis.h>
#include "mem.h"
#else
#include <dbgmsg.h>
#endif 

#include <refcnt.h>

#define IRDA_MSG_DATA_SIZE_INTERNAL sizeof(IRDA_MSG)+128 // can this be smaller?
#define IRDA_MAX_DATA_SIZE          2048

//#define TEMPERAMENTAL_SERIAL_DRIVER // drivers busted. intercharacter delays cause
                                    // IrLAP to reset.

#if DBG
// Prototypes for Debugging Output
void IRDA_DebugOut (TCHAR *pFormat, ...);
void IRDA_DebugStartLog (void);
void IRDA_DebugEndLog (void *, void *);
#endif

#ifndef UNDER_CE
#define DBG_NDIS        0x00000002 // keep in sync with test\irdakdx
#define DBG_TIMER       0x00000004
#define DBG_IRMAC       0x00000008

#define DBG_IRLAP       0x00000010
#define DBG_IRLAPLOG    0x00000020
#define DBG_RXFRAME     0x00000040
#define DBG_TXFRAME     0x00000080

#define DBG_IRLMP       0x00000100
#define DBG_IRLMP_CONN  0x00000200
#define DBG_IRLMP_CRED  0x00000400
#define DBG_IRLMP_IAS   0x00000800

#define DBG_DISCOVERY   0x00001000
#define DBG_PRINT       0x00002000
#define DBG_ADDR        0x00004000

#define DBG_REF         0x00010000

#define DBG_TDI         0x00020000
#define DBG_TDI_IRP     0x00040000

#define DBG_ALLOC       0x10000000
#define DBG_FUNCTION    0x20000000
#define DBG_WARN        0x40000000
#define DBG_ERROR       0x80000000
#else // !UNDER_CE
// Debug zone definitions.
#define ZONE_INIT       DEBUGZONE(0)
#define ZONE_WSHIRDA    DEBUGZONE(1)
#define ZONE_TDI        DEBUGZONE(2)
#define ZONE_IRLMP      DEBUGZONE(3)
#define ZONE_IRLAP      DEBUGZONE(4)
#define ZONE_IRNDIS     DEBUGZONE(5)
#define ZONE_DSCV       DEBUGZONE(6)
#define ZONE_SOCKETCNT  DEBUGZONE(7)
#define ZONE_TIMER      DEBUGZONE(8)
#define ZONE_REF        DEBUGZONE(9)
#define ZONE_LOCK       DEBUGZONE(10)
#define ZONE_MISC       DEBUGZONE(11)
#define ZONE_ALLOC      DEBUGZONE(12)
#define ZONE_FUNCTION   DEBUGZONE(13)
#define ZONE_WARN       DEBUGZONE(14)
#define ZONE_ERROR      DEBUGZONE(15)

#define DBG_TDI         ZONE_TDI
#define DBG_IRLMP       ZONE_IRLMP
#define DBG_IRLMP_CONN  ZONE_IRLMP
#define DBG_IRLMP_CRED  ZONE_IRLMP
#define DBG_IRLMP_IAS   ZONE_IRLMP
#define DBG_IRLAP       ZONE_IRLAP
#define DBG_IRLAPLOG    ZONE_IRLAP
#define DBG_DISCOVERY   ZONE_DSCV
#define DBG_PRINT       (0)
#define DBG_ADDR        (0)
#define DBG_NDIS        ZONE_IRNDIS
#define DBG_TIMER       ZONE_TIMER
#define DBG_REF         ZONE_REF
#define DBG_MISC        ZONE_MISC
#define DBG_ALLOC       ZONE_ALLOC
#define DBG_FUNCTION    ZONE_FUNCTION
#define DBG_WARN        ZONE_WARN
#define DBG_ERROR       ZONE_ERROR

// This is zone for debug logging
#define DBG_RXFRAME     0x1
#define DBG_TXFRAME     0x2
#endif // UNDER_CE

#define IRDA_ALLOC_MEM(ptr, sz, id) ((ptr) = CTEAllocMem(sz))
#define IRDA_FREE_MEM(ptr)          CTEFreeMem((ptr))

#if DBG
#define STATIC
#else
#define STATIC static
#endif 

typedef struct
{
    CTETimer        CteTimer;
    VOID            (*ExpFunc)(PVOID Context);
    PVOID           Context;
    UINT            Timeout;
    BOOLEAN         Late;
    struct IrdaLinkCb *pIrdaLinkCb;
#if DBG    
    char            *pName;
#endif    
} IRDA_TIMER, *PIRDA_TIMER;

typedef struct
{
    CTEEvent        CteEvent; // Must be first field
    VOID            (*Callback)(PVOID Context);
} IRDA_EVENT, *PIRDA_EVENT;

#define IRMAC_CONTEXT(ilcb)     ((ilcb)->IrmacContext)
#define IRLAP_CONTEXT(ilcb)     ((ilcb)->IrlapContext)
#define IRLMP_CONTEXT(ilcb)     ((ilcb)->IrlmpContext)

// Device/Discovery Information
#define IRLAP_DSCV_INFO_LEN       32
#define IRDA_DEV_ADDR_LEN         4

typedef struct
{
    LIST_ENTRY      Linkage;
    UCHAR           DevAddr[IRDA_DEV_ADDR_LEN];
    int             DscvMethod;
    int             IRLAP_Version;
    UCHAR           DscvInfo[IRLAP_DSCV_INFO_LEN];
    int             DscvInfoLen;
    int             NotSeenCnt;  // used by IRLMP to determine when to remove
                                 // the device from its list
    PVOID           LinkContext; // Link on which device was discovered
} IRDA_DEVICE;

#define IRDA_NDIS_BUFFER_POOL_SIZE   14
#define IRDA_NDIS_PACKET_POOL_SIZE   7
#define IRDA_MSG_LIST_LEN            2
#define IRDA_MSG_DATA_SIZE           64

typedef struct
{
    LIST_ENTRY                  Linkage;
    PVOID                       Owner;
    struct irda_msg            *pMsg;
    MEDIA_SPECIFIC_INFORMATION  MediaInfo;
} IRDA_PROTOCOL_RESERVED, *PIRDA_PROTOCOL_RESERVED;

typedef struct IrdaLinkCb
{
    LIST_ENTRY      Linkage;
    PVOID           IrlapContext;
    PVOID           IrlmpContext;
    NDIS_SPIN_LOCK  SpinLock;
#ifdef UNDER_CE
    CRITICAL_SECTION Mutex;
#else
    KMUTEX          Mutex;
#endif // !UNDER_CE
    NDIS_HANDLE     UnbindContext;
    NDIS_HANDLE     NdisBindingHandle;
    NDIS_EVENT      SyncEvent;
    NDIS_STATUS     SyncStatus;
    IRDA_TIMER      MediaSenseTimer;
    int             MediaBusy;
    NDIS_HANDLE     BufferPool;
    NDIS_HANDLE     PacketPool;
    LIST_ENTRY      PacketList;
    LIST_ENTRY      TxMsgFreeList;
    int             TxMsgFreeListLen;
    LIST_ENTRY      RxMsgFreeList;
    int             RxMsgFreeListLen;
    LIST_ENTRY      RxMsgList;
    IRDA_EVENT      EvRxMsgReady;
    int             RxMsgDataSize;
    UINT            ExtraBofs;   // These should be per connection for
    UINT            MinTat;      // multipoint
    BOOLEAN         WaitMinTat;
    REF_CNT         RefCnt;
} IRDA_LINK_CB, *PIRDA_LINK_CB;    

// IRLAP Quality of Service
#define BIT_0       1
#define BIT_1       2
#define BIT_2       4
#define BIT_3       8
#define BIT_4       16
#define BIT_5       32
#define BIT_6       64
#define BIT_7       128
#define BIT_8       256

#define BPS_2400            BIT_0   // Baud Rates
#define BPS_9600            BIT_1
#define BPS_19200           BIT_2
#define BPS_38400           BIT_3
#define BPS_57600           BIT_4
#define BPS_115200          BIT_5
#define BPS_4000000         BIT_8

#define MAX_TAT_500         BIT_0   // Maximum Turnaround Time (millisecs)
#define MAX_TAT_250         BIT_1
#define MAX_TAT_100         BIT_2
#define MAX_TAT_50          BIT_3
#define MAX_TAT_25          BIT_4
#define MAX_TAT_10          BIT_5
#define MAX_TAT_5           BIT_6

#define DATA_SIZE_64        BIT_0   // Data Size (bytes)
#define DATA_SIZE_128       BIT_1
#define DATA_SIZE_256       BIT_2
#define DATA_SIZE_512       BIT_3
#define DATA_SIZE_1024      BIT_4
#define DATA_SIZE_2048      BIT_5

#define FRAMES_1            BIT_0   // Window Size
#define FRAMES_2            BIT_1
#define FRAMES_3            BIT_2
#define FRAMES_4            BIT_3
#define FRAMES_5            BIT_4
#define FRAMES_6            BIT_5
#define FRAMES_7            BIT_6

#define BOFS_48             BIT_0   // Additional Beginning of Frame Flags
#define BOFS_24             BIT_1
#define BOFS_12             BIT_2
#define BOFS_5              BIT_3
#define BOFS_3              BIT_4
#define BOFS_2              BIT_5
#define BOFS_1              BIT_6
#define BOFS_0              BIT_7

#define MIN_TAT_10          BIT_0   // Minumum Turnaround Time (millisecs)
#define MIN_TAT_5           BIT_1
#define MIN_TAT_1           BIT_2
#define MIN_TAT_0_5         BIT_3
#define MIN_TAT_0_1         BIT_4
#define MIN_TAT_0_05        BIT_5
#define MIN_TAT_0_01        BIT_6
#define MIN_TAT_0           BIT_7

#define DISC_TIME_3         BIT_0   // Link Disconnect/Threshold Time (seconds)
#define DISC_TIME_8         BIT_1
#define DISC_TIME_12        BIT_2
#define DISC_TIME_16        BIT_3
#define DISC_TIME_20        BIT_4
#define DISC_TIME_25        BIT_5
#define DISC_TIME_30        BIT_6
#define DISC_TIME_40        BIT_7

typedef struct
{
    UINT        bfBaud;
    UINT        bfMaxTurnTime;
    UINT        bfDataSize;
    UINT        bfWindowSize;
    UINT        bfBofs;
    UINT        bfMinTurnTime;
    UINT        bfDisconnectTime; // holds threshold time also
} IRDA_QOS_PARMS;


// IrDA Message Primitives
typedef enum
{
    MAC_DATA_REQ = 0,  // Keep in sync with table in irlaplog.c
    MAC_DATA_IND,
    MAC_DATA_RESP,
    MAC_DATA_CONF,
    MAC_CONTROL_REQ,
    MAC_CONTROL_CONF,
    IRLAP_DISCOVERY_REQ,
    IRLAP_DISCOVERY_IND,
    IRLAP_DISCOVERY_CONF,
    IRLAP_CONNECT_REQ,
    IRLAP_CONNECT_IND,
    IRLAP_CONNECT_RESP,
    IRLAP_CONNECT_CONF,
    IRLAP_DISCONNECT_REQ,
    IRLAP_DISCONNECT_IND,
    IRLAP_DATA_REQ,    // Don't fuss with the order, CONF must be 2 from REQ
    IRLAP_DATA_IND,
    IRLAP_DATA_CONF,
    IRLAP_UDATA_REQ,
    IRLAP_UDATA_IND,
    IRLAP_UDATA_CONF,
    IRLAP_STATUS_IND,
    IRLAP_FLOWON_REQ,
    IRLMP_DISCOVERY_REQ,
    IRLMP_DISCOVERY_IND,
    IRLMP_DISCOVERY_CONF,
    IRLMP_CONNECT_REQ,
    IRLMP_CONNECT_IND,
    IRLMP_CONNECT_RESP,
    IRLMP_CONNECT_CONF,
    IRLMP_DISCONNECT_REQ,
    IRLMP_DISCONNECT_IND,
    IRLMP_DATA_REQ,
    IRLMP_DATA_IND,
    IRLMP_DATA_CONF,
    IRLMP_UDATA_REQ,
    IRLMP_UDATA_IND,
    IRLMP_UDATA_CONF,
    IRLMP_ACCESSMODE_REQ,
    IRLMP_ACCESSMODE_IND,
    IRLMP_ACCESSMODE_CONF,
    IRLMP_MORECREDIT_REQ,
    IRLMP_GETVALUEBYCLASS_REQ,
    IRLMP_GETVALUEBYCLASS_CONF,
    IRLMP_REGISTERLSAP_REQ,
    IRLMP_DEREGISTERLSAP_REQ,
    IRLMP_ADDATTRIBUTE_REQ,
    IRLMP_DELATTRIBUTE_REQ,
    IRLMP_LINKCONTROL_REQ,
    IRLAP_LINKCONTROL_REQ
} IRDA_SERVICE_PRIM;

typedef enum
{
    MAC_MEDIA_BUSY,         // keep in sync with IRDA_StatStr in irlaplog.c
    MAC_MEDIA_CLEAR,
    IRLAP_DISCOVERY_COLLISION,
    IRLAP_REMOTE_DISCOVERY_IN_PROGRESS,
    IRLAP_REMOTE_CONNECT_IN_PROGRSS,
    IRLAP_DISCOVERY_COMPLETED,
    IRLAP_REMOTE_CONNECTION_IN_PROGRESS,
    IRLAP_CONNECTION_COMPLETED,
    IRLAP_REMOTE_INITIATED,
    IRLAP_PRIMARY_CONFLICT,
    IRLAP_DISCONNECT_COMPLETED,
    IRLAP_NO_RESPONSE,
    IRLAP_DECLINE_RESET,
    IRLAP_DATA_REQUEST_COMPLETED,
    IRLAP_DATA_REQUEST_FAILED_LINK_RESET,
    IRLAP_DATA_REQUEST_FAILED_REMOTE_BUSY,
    IRLMP_NO_RESPONSE,
    IRLMP_ACCESSMODE_SUCCESS,
    IRLMP_ACCESSMODE_FAILURE,
    IRLMP_DATA_REQUEST_COMPLETED,
    IRLMP_DATA_REQUEST_FAILED
} IRDA_SERVICE_STATUS;

// MAC Control Service Request Message - MAC_CONTROL_REQ/CONF
typedef enum
{
    MAC_INITIALIZE_LINK,  // keep in sync with MAC_OpStr in irlaplog.c
    MAC_CLOSE_LINK,
    MAC_RECONFIG_LINK,
    MAC_MEDIA_SENSE,
    MAC_ACQUIRE_RESOURCES,
    MAC_RELEASE_RESOURCES
} MAC_CONTROL_OPERATION;

typedef struct
{
    MAC_CONTROL_OPERATION   Op;
    int                     Port;
    int                     Baud;
    int                     NumBOFs;
    int                     MinTat;
    int                     DataSize;
    int                     SenseTime;
    IRDA_SERVICE_STATUS     OpStatus;
    BOOLEAN                 SetIR;
    void                    *pMacConfig;
    UINT                    MacConfigStatus;
} MAC_CONTROL_SERVICE;

// IRDA Link Control Service Request Message - IRLMP_LINKCONTROL_REQ
//                                             IRLAP_LINKCONTROL_REQ
typedef struct
{
    BOOLEAN             AcquireResources;
    PIRDA_LINK_CB       pIrdaLinkCb;
    void                *pLinkConfig;
} IRDA_LINKCONTROL_SERVICE;

// IRLAP Discovery Service Request Message - IRLAP_DISCOVERY_IND/CONF
typedef struct
{
    LIST_ENTRY             *pDevList;
    IRDA_SERVICE_STATUS     DscvStatus;
    BOOLEAN                 SenseMedia;
} IRLAP_DISCOVERY_SERVICE;

// IRDA Connection Service Request Message - IRLAP_CONNECT_REQ/IND/CONF
//                                           IRLMP_CONNECT_REQ/CONF
typedef struct
{
    UCHAR                   RemoteDevAddr[IRDA_DEV_ADDR_LEN];
    IRDA_QOS_PARMS          *pQos;
    int                     LocalLsapSel; 
    int                     RemoteLsapSel;
    UCHAR                   *pConnData;   
    int                     ConnDataLen;  
    void                    *pContext; 
    int                     MaxPDUSize;
    int                     MaxSDUSize;
    int                     TtpCredits;
    IRDA_SERVICE_STATUS     ConnStatus;
    BOOLEAN                 UseTtp;
} IRDA_CONNECT_SERVICE;

// IRDA Disconnection Service Request Message - IRLAP_DISCONNECT_REQ/IND
//                                              IRLMP_DISCONNECT_REQ/IND
typedef enum
{
    IRLMP_USER_REQUEST = 1,
    IRLMP_UNEXPECTED_IRLAP_DISC,
    IRLMP_IRLAP_CONN_FAILED,
    IRLMP_IRLAP_RESET,
    IRLMP_LM_INITIATED_DISC,
    IRLMP_DISC_LSAP,
    IRLMP_NO_RESPONSE_LSAP,
    IRLMP_NO_AVAILABLE_LSAP,
    IRLMP_MAC_MEDIA_BUSY,
    IRLMP_IRLAP_REMOTE_DISCOVERY_IN_PROGRESS,

    IRLMP_IAS_NO_SUCH_OBJECT, // these are added for the IAS_GetValueByClass.Conf
    IRLMP_IAS_NO_SUCH_ATTRIB,
    IRLMP_IAS_SUCCESS,
    IRLMP_IAS_SUCCESS_LISTLEN_GREATER_THAN_ONE,
   
    IRLMP_UNSPECIFIED_DISC = 0xFF
} IRLMP_DISC_REASON;

typedef struct
{
    UCHAR                   *pDiscData;     // IRLMP_DISCONNECT_REQ/IND only
    int                     DiscDataLen;    // IRLMP_DISCONNECT_REQ/IND only
    IRLMP_DISC_REASON       DiscReason;     // IRLMP_DISCONNECT_REQ/IND only
    IRDA_SERVICE_STATUS     DiscStatus;     // Indication only
    void                    *pDiscContext;
} IRDA_DISCONNECT_SERVICE;

// IRDA Data Service Request Message
#define IRLAP_HEADER_LEN       2
#define IRLMP_HEADER_LEN       6
#define TTP_HEADER_LEN         8
#define IRDA_HEADER_LEN        IRLAP_HEADER_LEN+IRLMP_HEADER_LEN+TTP_HEADER_LEN+1
                                             // + 1 IRComm WACK!!
#define SEG_FINAL              1
#define SEG_LOCAL              2

typedef struct
{
    void                    *pOwner;
    int                     SegCount;      // Number of segments
    UINT                    SegFlags;
    UCHAR                   *pBase;
    UCHAR                   *pLimit;
    UCHAR                   *pRead;
    UCHAR                   *pWrite;
    void                    *pTdiSendComp;
    void                    *pTdiSendCompCnxt;
    UINT                    SendRefCnt;
    UINT                    SendLen;
    BOOLEAN                 IrCOMM_9Wire;
#ifdef TEMPERAMENTAL_SERIAL_DRIVER
    int                     FCS;
#endif    
    IRDA_SERVICE_STATUS     DataStatus; // for CONF
    //                      |------------------------|
    //                      | pRead                o-------------
    //                      |------------------------|           |
    //                      | pWrite               o----------   |
    //                      |------------------------|        |  |
    //                      | pBase                o-------   |  |
    //                      |------------------------|     |  |  |
    //                      | pLimit               o----   |  |  |
    //                      |------------------------|  |  |  |  |
    //                                               |  |  |  |  |
    //                       ------------------------   |  |  |  |
    //                      |                        |<----   |  |
    //                      |                        |  |     |  |
    //                      |                        |<--------<-
    //                      |                        |  |
    //                      |                        |<-
    //                       ------------------------
    UCHAR                   *pHdrRead;
    UCHAR                   *pHdrWrite;
    UCHAR                   Header[IRDA_HEADER_LEN];
    //                      |------------------------|
    //                      | pHdrRead              o-------------
    //                      |------------------------|           |
    //                      | pHdrWrite            o----------   |
    //                      |------------------------|        |  |
    //            Header--->|                        |        |  |
    //                      |                        |        |  |
    //                      |                        |<--------<-
    //                      |                        |        |
    //                      |                        |<-------
    //                       ------------------------
    //
    //                      On the receive side, all headers are contained
    //                      at pRead, not in the above Header array
    //
} IRDA_DATA_SERVICE;

typedef enum
{
    IRLMP_MULTIPLEXED,
    IRLMP_EXCLUSIVE
} IRLMP_ACCESSMODE;

typedef struct
{
    IRLMP_ACCESSMODE    AccessMode;
    IRDA_SERVICE_STATUS ModeStatus;     
    BOOLEAN             IrLPTMode;  // if true don't send PDU
} IRLMP_ACCESSMODE_SERVICE;

typedef struct
{
    IAS_SET             *pIasSet;
    CHAR                *pClassName;
    IAS_QUERY           *pIasQuery;
    int                 AttribLen;      // OctetSeq or UsrStr len
    IRLMP_DISC_REASON   IASStatus;
    PVOID               AttribHandle;
    PVOID               *pAttribHandle;
} IRLMP_IAS_SERVICE;

typedef struct irda_msg
{
    LIST_ENTRY          Linkage;
    IRDA_SERVICE_PRIM   Prim;
    PVOID               DataContext;
    union
    {
        MAC_CONTROL_SERVICE             MAC_ControlService;
        IRDA_LINKCONTROL_SERVICE        IRDA_LinkControlService;
        IRLAP_DISCOVERY_SERVICE         IRLAP_DiscoveryService;
        IRDA_DISCONNECT_SERVICE         IRDA_DisconnectService;
        IRDA_CONNECT_SERVICE            IRDA_ConnectService;
        IRDA_DATA_SERVICE               IRDA_DataService;
        IRLMP_ACCESSMODE_SERVICE        IRLMP_AccessModeService;
        IRLMP_IAS_SERVICE               IRLMP_IASService;
        UINT                            LinkStatus;
    } MsgType;

} IRDA_MSG, *PIRDA_MSG;

#define IRDA_MSG_Op                 MsgType.MAC_ControlService.Op
#define IRDA_MSG_Port               MsgType.MAC_ControlService.Port
#define IRDA_MSG_Baud               MsgType.MAC_ControlService.Baud
#define IRDA_MSG_NumBOFs            MsgType.MAC_ControlService.NumBOFs
#define IRDA_MSG_MinTat             MsgType.MAC_ControlService.MinTat 
#define IRDA_MSG_DataSize           MsgType.MAC_ControlService.DataSize
#define IRDA_MSG_OpStatus           MsgType.MAC_ControlService.OpStatus
#define IRDA_MSG_SetIR              MsgType.MAC_ControlService.SetIR
#define IRDA_MSG_SenseTime          MsgType.MAC_ControlService.SenseTime
#define IRDA_MSG_pMacConfig         MsgType.MAC_ControlService.pMacConfig
#define IRDA_MSG_MacConfigStatus    MsgType.MAC_ControlService.MacConfigStatus

#define IRDA_MSG_AcquireResources   MsgType.IRDA_LinkControlService.AcquireResources
#define IRDA_MSG_pLinkContext       MsgType.IRDA_LinkControlService.pIrdaLinkCb
#define IRDA_MSG_pLinkConfig        MsgType.IRDA_LinkControlService.pLinkConfig

#define IRDA_MSG_pOwner             MsgType.IRDA_DataService.pOwner  
#define IRDA_MSG_SegCount           MsgType.IRDA_DataService.SegCount
#define IRDA_MSG_SegFlags           MsgType.IRDA_DataService.SegFlags
#define IRDA_MSG_pHdrRead           MsgType.IRDA_DataService.pHdrRead
#define IRDA_MSG_pHdrWrite          MsgType.IRDA_DataService.pHdrWrite
#define IRDA_MSG_Header             MsgType.IRDA_DataService.Header
#define IRDA_MSG_pBase              MsgType.IRDA_DataService.pBase
#define IRDA_MSG_pLimit             MsgType.IRDA_DataService.pLimit
#define IRDA_MSG_pRead              MsgType.IRDA_DataService.pRead
#define IRDA_MSG_pWrite             MsgType.IRDA_DataService.pWrite
#define IRDA_MSG_DataStatus         MsgType.IRDA_DataService.DataStatus
#define IRDA_MSG_pTdiSendComp       MsgType.IRDA_DataService.pTdiSendComp
#define IRDA_MSG_pTdiSendCompCnxt   MsgType.IRDA_DataService.pTdiSendCompCnxt
#define IRDA_MSG_SendRefCnt         MsgType.IRDA_DataService.SendRefCnt
#define IRDA_MSG_SendLen            MsgType.IRDA_DataService.SendLen
#define IRDA_MSG_IrCOMM_9Wire       MsgType.IRDA_DataService.IrCOMM_9Wire
#ifdef TEMPERAMENTAL_SERIAL_DRIVER
#define IRDA_MSG_FCS                MsgType.IRDA_DataService.FCS
#endif

#define IRDA_MSG_pDevList           MsgType.IRLAP_DiscoveryService.pDevList
#define IRDA_MSG_DscvStatus         MsgType.IRLAP_DiscoveryService.DscvStatus
#define IRDA_MSG_SenseMedia         MsgType.IRLAP_DiscoveryService.SenseMedia

#define IRDA_MSG_RemoteDevAddr      MsgType.IRDA_ConnectService.RemoteDevAddr
#define IRDA_MSG_pQos               MsgType.IRDA_ConnectService.pQos
#define IRDA_MSG_LocalLsapSel       MsgType.IRDA_ConnectService.LocalLsapSel
#define IRDA_MSG_RemoteLsapSel      MsgType.IRDA_ConnectService.RemoteLsapSel
#define IRDA_MSG_pConnData          MsgType.IRDA_ConnectService.pConnData
#define IRDA_MSG_ConnDataLen        MsgType.IRDA_ConnectService.ConnDataLen
#define IRDA_MSG_ConnStatus         MsgType.IRDA_ConnectService.ConnStatus
#define IRDA_MSG_pContext           MsgType.IRDA_ConnectService.pContext
#define IRDA_MSG_UseTtp             MsgType.IRDA_ConnectService.UseTtp
#define IRDA_MSG_MaxSDUSize         MsgType.IRDA_ConnectService.MaxSDUSize
#define IRDA_MSG_MaxPDUSize         MsgType.IRDA_ConnectService.MaxPDUSize
#define IRDA_MSG_TtpCredits         MsgType.IRDA_ConnectService.TtpCredits

#define IRDA_MSG_pDiscData          MsgType.IRDA_DisconnectService.pDiscData
#define IRDA_MSG_DiscDataLen        MsgType.IRDA_DisconnectService.DiscDataLen
#define IRDA_MSG_DiscReason         MsgType.IRDA_DisconnectService.DiscReason
#define IRDA_MSG_DiscStatus         MsgType.IRDA_DisconnectService.DiscStatus
#define IRDA_MSG_pDiscContext       MsgType.IRDA_DisconnectService.pDiscContext

#define IRDA_MSG_AccessMode         MsgType.IRLMP_AccessModeService.AccessMode
#define IRDA_MSG_ModeStatus         MsgType.IRLMP_AccessModeService.ModeStatus
#define IRDA_MSG_IrLPTMode          MsgType.IRLMP_AccessModeService.IrLPTMode

#define IRDA_MSG_pIasSet            MsgType.IRLMP_IASService.pIasSet
#define IRDA_MSG_pClassName         MsgType.IRLMP_IASService.pClassName
#define IRDA_MSG_pIasQuery          MsgType.IRLMP_IASService.pIasQuery
#define IRDA_MSG_AttribLen          MsgType.IRLMP_IASService.AttribLen
#define IRDA_MSG_IASStatus          MsgType.IRLMP_IASService.IASStatus
#define IRDA_MSG_AttribHandle       MsgType.IRLMP_IASService.AttribHandle
#define IRDA_MSG_pAttribHandle      MsgType.IRLMP_IASService.pAttribHandle

#define IRDA_MSG_LinkStatus         MsgType.LinkStatus
extern PDRIVER_OBJECT pIrdaDriverObject;

extern LIST_ENTRY   IrdaLinkCbList;
extern NDIS_HANDLE  NdisIrdaHandle;
extern PVOID        IrdaMsgPool;

VOID IrdaTimerInitialize(PIRDA_TIMER     pTimer,
                         VOID            (*ExpFunc)(PVOID Context),
                         UINT            Timeout,
                         PVOID           Context,
                         PIRDA_LINK_CB   pIrdaLinkCb);

VOID IrdaTimerStart(PIRDA_TIMER pTimer);

VOID IrdaTimerStop(PIRDA_TIMER pTimer);

VOID IrdaTimerRestart(PIRDA_TIMER pTimer);

VOID IrdaEventInitialize(PIRDA_EVENT pIrdaEvent,
                         VOID        (*Callback)(PVOID Context));
    
VOID IrdaEventSchedule(PIRDA_EVENT pIrdaEvent, PVOID Context);

NTSTATUS
IrdaInitialize(
    IN PNDIS_STRING,
    IN PUNICODE_STRING, 
    OUT PUINT);

VOID
IrdaShutdown();

UINT 
TdiUp(void *pContext, IRDA_MSG *pMsg);

IRDA_MSG *
AllocTxMsg(
    PIRDA_LINK_CB);

VOID
IrmacDown(
    IN PVOID    IrmacContext,
    PIRDA_MSG   pMsg);

_inline
VOID
INIT_LINK_LOCK(PIRDA_LINK_CB pIrdaLinkCb)
{
#ifdef UNDER_CE
    InitializeCriticalSection(&pIrdaLinkCb->Mutex);
#else
    KeInitializeMutex(&pIrdaLinkCb->Mutex, 1);
#endif 
}

__inline VOID
DELETE_LINK_LOCK(PIRDA_LINK_CB pIrdaLinkCb)
{
#ifdef UNDER_CE
    DeleteCriticalSection(&pIrdaLinkCb->Mutex);
#endif 
}

_inline
VOID
LOCK_LINK(PIRDA_LINK_CB pIrdaLinkCb)
{
#ifdef UNDER_CE
    EnterCriticalSection(&pIrdaLinkCb->Mutex);
#else
    NTSTATUS Status;

    Status = KeWaitForSingleObject(&pIrdaLinkCb->Mutex,
        Executive,
        KernelMode,
        FALSE,
        NULL);

    ASSERT(Status == STATUS_SUCCESS);
#endif 
}

_inline 
VOID
UNLOCK_LINK(PIRDA_LINK_CB pIrdaLinkCb)
{
#ifdef UNDER_CE
    LeaveCriticalSection(&pIrdaLinkCb->Mutex);
#else 
    KeReleaseMutex(&pIrdaLinkCb->Mutex, FALSE);
#endif 
}

#ifndef UNDER_CE
#if DBG
typedef struct
{
    NPAGED_LOOKASIDE_LIST Lookaside;
    UINT                  Allocd;
} DBG_BUF_POOL, *PDBG_BUF_POOL;
#endif 
#endif 

__inline
PVOID
CreateIrdaBufPool(ULONG Size, ULONG Tag)
{
#ifdef UNDER_CE
    return ((PVOID)Size);
#else 
#if DBG
    PDBG_BUF_POOL           pDbgBufPool;
#else
    PNPAGED_LOOKASIDE_LIST  pLookaside;
#endif 

#if DBG
    pDbgBufPool = ExAllocatePool(NonPagedPool, sizeof(DBG_BUF_POOL));
//DEBUGMSG(1, ("Created pool %x\n", pDbgBufPool));
    if (!pDbgBufPool)
        return NULL;
#else
    pLookaside = ExAllocatePool(NonPagedPool, sizeof(NPAGED_LOOKASIDE_LIST));

    if (!pLookaside)
        return NULL;
#endif 

    ExInitializeNPagedLookasideList(
#if DBG
        &pDbgBufPool->Lookaside,
#else
        pLookaside,
#endif 
        NULL,
        NULL,
        0,
        Size,
        Tag,
        4); // doesn't do anything

#if DBG
    pDbgBufPool->Allocd = 0;
    return pDbgBufPool;
#else
    return pLookaside;
#endif 
#endif
}

__inline
VOID
DeleteIrdaBufPool(PVOID pBufPool)
{
#ifdef UNDER_CE
    return;
#else 
#if DBG
    PDBG_BUF_POOL    pDbgBufPool = pBufPool;
//DEBUGMSG(1, ("Deleted pool %x\n", pDbgBufPool));

    ExDeleteNPagedLookasideList(&pDbgBufPool->Lookaside);
    ExFreePool(pDbgBufPool);
#else
    ExDeleteNPagedLookasideList((PNPAGED_LOOKASIDE_LIST) pBufPool);
    ExFreePool(pBufPool);
#endif 
#endif
}

extern HANDLE g_hIrdaHeap;

__inline
PVOID
AllocIrdaBuf(PVOID pBufPool)
{
#ifdef UNDER_CE
    return (HeapAlloc(g_hIrdaHeap, HEAP_ZERO_MEMORY, (ULONG)pBufPool));
#else
#if DBG
    PDBG_BUF_POOL    pDbgBufPool = pBufPool;

    pDbgBufPool->Allocd++;

    //DEBUGMSG(1, ("+Pool %x, cnt %d\n", pBufPool, pDbgBufPool->Allocd));

    return ExAllocateFromNPagedLookasideList(&pDbgBufPool->Lookaside);
#else
    return ExAllocateFromNPagedLookasideList(
            (PNPAGED_LOOKASIDE_LIST) pBufPool);
#endif 
#endif 
}

__inline
VOID
FreeIrdaBuf(PVOID pBufPool, PVOID Buf)
{
#ifdef UNDER_CE
    HeapFree(g_hIrdaHeap, 0, Buf);
#else
#if DBG
    PDBG_BUF_POOL    pDbgBufPool = pBufPool;

    pDbgBufPool->Allocd--;
    //DEBUGMSG(1, ("-Pool %x, cnt %d\n", pBufPool, pDbgBufPool->Allocd));
    ExFreeToNPagedLookasideList(&pDbgBufPool->Lookaside, Buf);
#else
    ExFreeToNPagedLookasideList((PNPAGED_LOOKASIDE_LIST)pBufPool, Buf);
#endif 
#endif 
}

// I'm wrapping lookaside lists so I don't have to keep track of
// which pool to free to.
/*
__inline
PVOID
CreateMsgPool(ULONG Size,
             USHORT Depth,
             ULONG Tag)
{
    PNPAGED_LOOKASIDE_LIST  pLookaside;

    pLookaside = ExAllocatePool(NonPagedPool, sizeof(NPAGED_LOOKASIDE_LIST));

    if (!pLookaside)
        return NULL;

	ExInitializeNPagedLookasideList(
		pLookaside,
		NULL,
		NULL,
		0,
		Size + sizeof(PVOID),
		Tag,
		Depth);

    DEBUGMSG(0, ("CreateMsgPool %x\n", pLookaside));
    
    return pLookaside;
}

__inline
VOID
DeleteMsgPool(PVOID pLookaside)
{
    DEBUGMSG(0, ("DeleteMsgPool %x\n", pLookaside));
    
    ExDeleteNPagedLookasideList((PNPAGED_LOOKASIDE_LIST) pLookaside);

    ExFreePool(pLookaside);
}

__inline
PVOID
AllocFromMsgPool(PVOID pLookaside)
{
    PVOID   pEntry;

    pEntry = ExAllocateFromNPagedLookasideList(pLookaside);

    if (!pEntry)
        return NULL;

    // store lookaside with entry
    
    *(PVOID *) pEntry = pLookaside;

    DEBUGMSG(0, ("Alloc %x: Total %d, AllocMiss %d, FreeTot %d, FreeMiss %d Depth%d\n",
                 pLookaside,
                 ((PNPAGED_LOOKASIDE_LIST)pLookaside)->L.TotalAllocates,
                 ((PNPAGED_LOOKASIDE_LIST)pLookaside)->L.AllocateMisses,
                 ((PNPAGED_LOOKASIDE_LIST)pLookaside)->L.TotalFrees,
                 ((PNPAGED_LOOKASIDE_LIST)pLookaside)->L.FreeMisses,
                 ((PNPAGED_LOOKASIDE_LIST)pLookaside)->L.Depth));

    return ++(PVOID *)pEntry;
}

__inline
VOID
FreeToMsgPool(PVOID pEntry)
{
    PNPAGED_LOOKASIDE_LIST pLookaside;

    pEntry = --(PVOID *) pEntry;
    
    pLookaside = *(PVOID *) pEntry;

    DEBUGMSG(1, ("Listdepth %d, depth %d\n",
                 ExQueryDepthSList(&pLookaside->L.ListHead),
                 pLookaside->L.Depth));
    
    ExFreeToNPagedLookasideList(pLookaside, pEntry);

    DEBUGMSG(0, ("FREE %x: Total %d, AllocMiss %d, FreeTot %d, FreeMiss %d Depth %d\n",
                 pLookaside,
             pLookaside->L.TotalAllocates, pLookaside->L.AllocateMisses,
             pLookaside->L.TotalFrees, pLookaside->L.FreeMisses, pLookaside->L.Depth));
}
*/
    
// sh - add extern support for Nickname and charset so that we can reload 
//      the device name whenever an irlmp control req (acquire = TRUE).

#define NICK_NAME_LEN   (IRDA_MAX_DEVICE_NAME - sizeof(WCHAR))
extern UINT    Hints;
extern BYTE    CharSet;
extern UCHAR   NickName[NICK_NAME_LEN + sizeof(WCHAR)];
extern UINT    NickNameLen;
    

