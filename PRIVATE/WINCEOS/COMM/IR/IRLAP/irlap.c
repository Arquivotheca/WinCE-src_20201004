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
*       @doc
*       @module irlap.c | Provides IrLAP API
*
*       Date: 4/15/95
*
*       @comm
*
*  This module exports the following API's:
*
*       IrlapDown(Message)
*           Receives from LMP:
*               - Discovery request
*               - Connect request/response
*               - Disconnect request
*               - Data/UData request
*
*       IrlapUp(Message)
*           Receives from MAC:
*               - Data indications
*               - Control confirmations
*
*       IrlapTimerExp(Timer)
*           Receives from timer thread timer expiration notifications
*
*       IrlapCloseLink()
*           Shut down IRLAP and IRMAC.
*
*       IrlapGetQosParmVal()
*           Allows IRLMP to decode Qos.
*
*                |---------|
*                |  IRLMP  |
*                |---------|
*                  /|\  |
*                   |   |
*        IrlmpUp()  |   | IrlapDown()
*                   |   |
*                   |  \|/
*                |---------|  IRDA_TimerStart/Stop()   |-------|
*                |         |-------------------------->|       |
*                |  IRLAP  |                           | TIMER |
*                |         |<--------------------------|       |
*                |---------|      XTimerExp()          |-------|
*                  /|\  |
*                   |   |
*        IrlapUp()  |   |IrmacDown()
*                   |   |
*                   |  \|/
*                |---------|
*                |  IRMAC  |
*                |---------|
*
*
*  Discovery Request
*
*  |-------|  IRLAP_DISCOVERY_REQ                                |-------|
*  |       |---------------------------------------------------->|       |
*  | IRLMP |                                                     | IRLAP |
*  |       |<----------------------------------------------------|       |
*  |-------|   IRLAP_DISCOVERY_CONF                              |-------|
*                  DscvStatus = IRLAP_DISCOVERY_COMPLETE
*                               IRLAP_DISCOVERY_COLLISION
*                               MAC_MEDIA_BUSY
*
*  Connect Request
*
*  |-------|  IRLAP_CONNECT_REQ                                  |-------|
*  |       |---------------------------------------------------->|       |
*  | IRLMP |                                                     | IRLAP |
*  |       |<----------------------------------------------------|       |
*  |-------|   IRLAP_CONNECT_CONF                                |-------|
*                  ConnStatus = IRLAP_CONNECTION_COMPLETE
*              IRLAP_DISCONNECT_IND
*                  DiscStatus = IRLAP_NO_RESPONSE
*                               MAC_MEDIA_BUSY
*
*  Disconnect Request
*
*  |-------|  IRLAP_DISCONNECT_REQ                               |-------|
*  |       |---------------------------------------------------->|       |
*  | IRLMP |                                                     | IRLAP |
*  |       |<----------------------------------------------------|       |
*  |-------|   IRLAP_DISCONNECT_IND                              |-------|
*                  DiscStatus = IRLAP_DISCONNECT_COMPLETE
*                               IRLAP_NO_RESPONSE
*
*  UData/Data Request
*
*  |-------|  IRLAP_DATA/UDATA_REQ                               |-------|
*  |       |---------------------------------------------------->|       |
*  | IRLMP |                                                     | IRLAP |
*  |       |<----------------------------------------------------|       |
*  |-------|   IRLAP_DATA_CONF                                   |-------|
*                  DataStatus =  IRLAP_DATA_REQUEST_COMPLETED
*                                IRLAP_DATA_REQUEST_FAILED_LINK_RESET
*
* See irda.h for complete message definitions
*/

#include <irda.h>
#ifdef UNDER_CE
#include <linklist.h>

// defined in NT irioctl.h
#define LINK_STATUS_IDLE            0
#define LINK_STATUS_DISCOVERING     1
#define LINK_STATUS_CONNECTED       2
#define LINK_STATUS_INTERRUPTED     3
#else // UNDER_CE
#include <irioctl.h>
#endif // !UNDER_CE
#include <irlap.h>
#include <irlmp.h>
#include <irlapp.h>
#include <irlapio.h>
#include <irlaplog.h>


// In IrNDIS.c.
extern UINT SlotTimeout;
extern UINT DiscoverTimeout;

#ifdef TEMPERAMENTAL_SERIAL_DRIVER
int TossedDups;
#endif

UINT    IrlapSlotTable[] = {1, 6, 8, 16};

UCHAR   IrlapBroadcastDevAddr[IRDA_DEV_ADDR_LEN] = {0xFF,0xFF,0xFF,0xFF};

// Parameter Value (PV) tables used for negotation
//                      bit0        1       2        3      4
//                         5        6       7        8
//                     --------------------------------------------
UINT vBaudTable[]     = {2400,   9600,   19200,   38400,  57600,\
                         115200, 576000, 1152000, 4000000,16000000};
UINT vMaxTATTable[]   = {500,    250,    100,     50,     25,   \
                         10,     5,      0,       0,      0     };
UINT vMinTATTable[]   = {10000,  5000,   1000,    500,    100,  \
                         50,     10,     0,       0,      0     };
UINT vDataSizeTable[] = {64,     128,    256,     512,    1024, \
                         2048,   0,      0,       0,      0     };
UINT vWinSizeTable[]  = {1,      2,      3,       4,      5,    \
                         6,      7,      0,       0,      0     };
UINT vBOFSTable[]     = {48,     24,     12,      5,      3,    \
                         2,      1,      0,       0,      0     };
UINT vDiscTable[]     = {3,      8,      12,      16,    20,    \
                         25,     30,     40,      0,     0      };
UINT vThreshTable[]   = {0,      3,      3,       3,     3,     \
                         3,      3,      3,       0,     0      };
UINT vBOFSDivTable[]  = {48,     12,     6,       3,     2,     \
                         1,      1,      1,       1,     1      };

// Tables for determining maximum line capacity for baud, max turn time
//      max turn time - 500ms   250ms   100ms  50ms  25ms  10ms   5ms
//      -------------------------------------------------------------
UINT MAXCAP_9600[]    = {400,       200,    80,     0,      0,      0,      0,      0,      0,      0};
UINT MAXCAP_19200[]   = {800,       400,    160,    0,      0,      0,      0,      0,      0,      0};
UINT MAXCAP_38400[]   = {1600,      800,    320,    0,      0,      0,      0,      0,      0,      0};
UINT MAXCAP_57600[]   = {2360,      1180,   472,    0,      0,      0,      0,      0,      0,      0};
UINT MAXCAP_115200[]  = {4800,      2400,   960,    480,    240,    96,     48,     24,     12,     6};
UINT MAXCAP_576000[]  = {28800,     11520,  5760,   2880,   1440,   720,    360,    180,    90,     45};
UINT MAXCAP_1152000[] = {57600,     28800,  11520,  5760,   2880,   1440,   720,    360,    180,    90};
UINT MAXCAP_4000000[] = {200000,    100000, 40000,  20000,  10000,  5000,   2500,   1200,   600,    300};
UINT MAXCAP_16000000[] ={800000,    400000, 160000, 80000,  40000,  20000,  10000,  5000,   2500,   1200};

// prototypes
STATIC VOID InitializeState(PIRLAP_CB, IRLAP_STN_TYPE);
STATIC VOID ReturnRxTxWinMsgs(PIRLAP_CB);
STATIC UINT ProcessLinkControlReq(PIRLAP_CB, PIRDA_MSG);
STATIC UINT ProcessConnectReq(PIRLAP_CB, PIRDA_MSG);
STATIC VOID ProcessConnectResp(PIRLAP_CB, PIRDA_MSG);
STATIC UINT ProcessDiscoveryReq(PIRLAP_CB, PIRDA_MSG);
STATIC VOID ProcessDisconnectReq(PIRLAP_CB);
STATIC UINT ProcessDataAndUDataReq(PIRLAP_CB, PIRDA_MSG);
STATIC VOID XmitTxMsgList(PIRLAP_CB, BOOLEAN, BOOLEAN *);
STATIC VOID GotoPCloseState(PIRLAP_CB);
STATIC VOID GotoNDMThenDscvOrConn(PIRLAP_CB);
STATIC VOID ProcessMACControlConf(PIRLAP_CB, PIRDA_MSG);
STATIC VOID ProcessMACDataInd(PIRLAP_CB, PIRDA_MSG , BOOLEAN *);
STATIC VOID ProcessDscvXIDCmd(PIRLAP_CB, IRLAP_XID_DSCV_FORMAT *, UCHAR *);
STATIC VOID ProcessDscvXIDRsp(PIRLAP_CB, IRLAP_XID_DSCV_FORMAT *, UCHAR *);
STATIC VOID ExtractQosParms(IRDA_QOS_PARMS *, UCHAR *, UCHAR *);
STATIC VOID InitDscvCmdProcessing(PIRLAP_CB, IRLAP_XID_DSCV_FORMAT *);
STATIC VOID ExtractDeviceInfo(IRDA_DEVICE *, IRLAP_XID_DSCV_FORMAT *, UCHAR *);
STATIC BOOLEAN DevInDevList(UCHAR[], LIST_ENTRY *);
STATIC VOID AddDevToList(PIRLAP_CB, IRLAP_XID_DSCV_FORMAT *, UCHAR *);
STATIC VOID FreeDevList(LIST_ENTRY *);
STATIC VOID ProcessSNRM(PIRLAP_CB, IRLAP_SNRM_FORMAT *, UCHAR *);
STATIC VOID ProcessUA(PIRLAP_CB, IRLAP_UA_FORMAT *, UCHAR *);
STATIC VOID ProcessDISC(PIRLAP_CB);
STATIC VOID ProcessRD(PIRLAP_CB);
STATIC VOID ProcessRNRM(PIRLAP_CB);
STATIC VOID ProcessDM(PIRLAP_CB);
STATIC VOID ProcessFRMR(PIRLAP_CB);
STATIC VOID ProcessTEST(PIRLAP_CB, PIRDA_MSG, IRLAP_UA_FORMAT *, int, int);
STATIC VOID ProcessUI(PIRLAP_CB, PIRDA_MSG, int, int);
STATIC VOID ProcessREJ_SREJ(PIRLAP_CB, int, PIRDA_MSG, int, int, UINT);
STATIC VOID ProcessRR_RNR(PIRLAP_CB, int, PIRDA_MSG, int, int, UINT);
STATIC VOID ProcessIFrame(PIRLAP_CB, PIRDA_MSG, int, int, UINT, UINT, BOOLEAN *);
STATIC BOOLEAN InvalidNs(PIRLAP_CB, UINT);
STATIC BOOLEAN InvalidNr(PIRLAP_CB, UINT);
STATIC BOOLEAN InWindow(UINT, UINT, UINT);
STATIC VOID ProcessInvalidNsOrNr(PIRLAP_CB, int);
STATIC VOID ProcessInvalidNr(PIRLAP_CB, int);
STATIC VOID InsertRxWinAndForward(PIRLAP_CB, PIRDA_MSG, UINT, BOOLEAN *);
STATIC VOID ResendRejects(PIRLAP_CB, UINT);
STATIC VOID ConfirmAckedTxMsgs(PIRLAP_CB, UINT);
STATIC VOID MissingRxFrames(PIRLAP_CB);
STATIC VOID IFrameOtherStates(PIRLAP_CB, int, int);
STATIC UINT NegotiateQosParms(PIRLAP_CB, IRDA_QOS_PARMS *);
STATIC VOID ApplyQosParms(PIRLAP_CB);
STATIC VOID StationConflict(PIRLAP_CB);
STATIC VOID ApplyDefaultParms(PIRLAP_CB);
STATIC VOID ResendDISC(PIRLAP_CB);
STATIC BOOLEAN IgnoreState(PIRLAP_CB);
STATIC BOOLEAN MyDevAddr(PIRLAP_CB, UCHAR []);
STATIC VOID SlotTimerExp(PVOID);
STATIC VOID FinalTimerExp(PVOID);
STATIC VOID PollTimerExp(PVOID);
STATIC VOID BackoffTimerExp(PVOID);
STATIC VOID WDogTimerExp(PVOID);
STATIC VOID QueryTimerExp(PVOID);

#if DBG
void _inline IrlapTimerStart(
    TCHAR *lpszFunc, 
    PIRLAP_CB pIrlapCb, 
    PIRDA_TIMER pTmr)
{
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Start %hs timer for %dms"), pTmr->pName,
                      pTmr->Timeout));

    DEBUGMSG(DBG_TIMER,
             (TEXT("%hs timer started @ %d (%s)\r\n"),
              pTmr->pName, 
              GetTickCount(),
              lpszFunc));
    
    IrdaTimerStart(pTmr);
}

void _inline IrlapTimerStop(
    TCHAR *lpszFunc, 
    PIRLAP_CB pIrlapCb, 
    PIRDA_TIMER pTmr)
{
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Stop %hs timer"), pTmr->pName));

    IrdaTimerStop(pTmr);

    DEBUGMSG(DBG_TIMER,
             (TEXT("%hs timer stop @ %d (%s) - %hs\r\n"),
              pTmr->pName, 
              GetTickCount(),
              lpszFunc,
              pTmr->Late ? "LATE" : "not late"));
}
#else
#define IrlapTimerStart(f,c,t)   IrdaTimerStart(t)
#define IrlapTimerStop(f,c,t)    IrdaTimerStop(t)
#endif

VOID
IrlapOpenLink(OUT PNTSTATUS           Status,
              IN  PIRDA_LINK_CB       pIrdaLinkCb,
              IN  IRDA_QOS_PARMS      *pQos,
              IN  UCHAR               *pDscvInfo,
              IN  int                 DscvInfoLen,
              IN  UINT                MaxSlot,
              IN  UCHAR               *pDeviceName,
              IN  int                 DeviceNameLen,
              IN  UCHAR               CharSet)
{
    UINT        rc = SUCCESS;
    int         i;
    IRDA_MSG    IMsg;
    PIRLAP_CB   pIrlapCb;
    NDIS_STRING AStr = NDIS_STRING_CONST("InfraredTransceiverType");
    
    DEBUGMSG(DBG_IRLAP, (TEXT("IrlapOpenLink\n")));
    
    if (IRDA_ALLOC_MEM(pIrlapCb, sizeof(IRLAP_CB), MT_IRLAPCB) == NULL)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("Alloc failed\n")));
        *Status = STATUS_INSUFFICIENT_RESOURCES;
        return;
    }
                
    IrlmpOpenLink(Status,
                  pIrdaLinkCb,
                  pDeviceName,
                  DeviceNameLen,
                  CharSet);

    if (*Status != STATUS_SUCCESS)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IrlmpOpenLink failed\n")));
        IRDA_FREE_MEM(pIrlapCb);
        return;
    }
        
    pIrdaLinkCb->IrlapContext = pIrlapCb;

    DscvInfoLen = DscvInfoLen > IRLAP_DSCV_INFO_LEN ?
        IRLAP_DSCV_INFO_LEN : DscvInfoLen;
    
    CTEMemCopy(pIrlapCb->LocalDevice.DscvInfo, pDscvInfo, DscvInfoLen);

    pIrlapCb->LocalDevice.DscvInfoLen = DscvInfoLen;

    CTEMemCopy(&pIrlapCb->LocalQos, pQos, sizeof(IRDA_QOS_PARMS));

    pIrlapCb->MaxSlot       = MaxSlot;
    pIrlapCb->pIrdaLinkCb   = pIrdaLinkCb;
    
    InitMsgList(&pIrlapCb->TxMsgList);
    
    InitializeListHead(&pIrlapCb->DevList);
    
    for (i = 0; i < IRLAP_MOD; i++)
    {
        pIrlapCb->TxWin.pMsg[i] = NULL;
        pIrlapCb->RxWin.pMsg[i] = NULL;
    }

    // Get the local MAX TAT (for final timeout)
    if ((pIrlapCb->LocalMaxTAT =
         IrlapGetQosParmVal(vMaxTATTable,
                            pIrlapCb->LocalQos.bfMaxTurnTime, NULL)) == -1)
    {
        *Status = STATUS_UNSUCCESSFUL;
        return /*IRLAP_BAD_QOS*/;
    }

    if ((pIrlapCb->LocalWinSize =
         IrlapGetQosParmVal(vWinSizeTable,
                            pIrlapCb->LocalQos.bfWindowSize, NULL)) == -1)
    {
        *Status = STATUS_UNSUCCESSFUL;
        return /*IRLAP_BAD_QOS*/;
    }
        
    // initialize as PRIMARY so UI frames in contention
    // state sends CRBit = cmd
    InitializeState(pIrlapCb, PRIMARY);
    
    pIrlapCb->State = NDM;

    // Generate random local address
    StoreULAddr(pIrlapCb->LocalDevice.DevAddr, (ULONG) GetMyDevAddr(FALSE));

    pIrlapCb->LocalDevice.IRLAP_Version = 0; 

    pIrlapCb->Baud              = IRLAP_CONTENTION_BAUD;
    pIrlapCb->RemoteMaxTAT      = IRLAP_CONTENTION_MAX_TAT;
    pIrlapCb->RemoteDataSize    = IRLAP_CONTENTION_DATA_SIZE;
    pIrlapCb->RemoteWinSize     = IRLAP_CONTENTION_WIN_SIZE; 
    pIrlapCb->RemoteNumBOFS     = IRLAP_CONTENTION_BOFS;

    pIrlapCb->ConnAddr = IRLAP_BROADCAST_CONN_ADDR;

    pIrlapCb->N1 = 0;  // calculated at negotiation
    pIrlapCb->N2 = 0;
    pIrlapCb->N3 = 5;  // recalculated after negotiation ??

#if DBG
    pIrlapCb->PollTimer.pName       = "Poll";
    pIrlapCb->FinalTimer.pName      = "Final" ;
    pIrlapCb->SlotTimer.pName       = "Slot";
    pIrlapCb->QueryTimer.pName      = "Query";
    pIrlapCb->WDogTimer.pName       = "WatchDog";
    pIrlapCb->BackoffTimer.pName    = "Backoff";    
#endif
    
    IrdaTimerInitialize(&pIrlapCb->PollTimer,
                        PollTimerExp,
                        pIrlapCb->RemoteMaxTAT,
                        pIrlapCb,
                        pIrdaLinkCb);

    IrdaTimerInitialize(&pIrlapCb->FinalTimer,
                        FinalTimerExp,
                        pIrlapCb->LocalMaxTAT,
                        pIrlapCb,
                        pIrdaLinkCb);

    IrdaTimerInitialize(&pIrlapCb->SlotTimer,
                        SlotTimerExp,
                        SlotTimeout,
                        pIrlapCb,
                        pIrdaLinkCb);

    IrdaTimerInitialize(&pIrlapCb->QueryTimer,
                        QueryTimerExp,
                        (IRLAP_MAX_SLOTS + 4) * SlotTimeout*2,
                        pIrlapCb,
                        pIrdaLinkCb);

    IrdaTimerInitialize(&pIrlapCb->WDogTimer,
                        WDogTimerExp,
                        3000,
                        pIrlapCb,
                        pIrdaLinkCb);

    IrdaTimerInitialize(&pIrlapCb->BackoffTimer,
                        BackoffTimerExp,
                        0,
                        pIrlapCb,
                        pIrdaLinkCb);

    // Initialize Link
    IMsg.Prim               = MAC_CONTROL_REQ;
    IMsg.IRDA_MSG_Op        = MAC_INITIALIZE_LINK;
    IMsg.IRDA_MSG_Baud      = IRLAP_CONTENTION_BAUD;
    IMsg.IRDA_MSG_NumBOFs   = IRLAP_CONTENTION_BOFS;
    IMsg.IRDA_MSG_DataSize  = IRLAP_CONTENTION_DATA_SIZE;
    IMsg.IRDA_MSG_MinTat    = 0;
    
    IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);

    *Status = STATUS_SUCCESS;
}

/*****************************************************************************
*/
VOID
IrlapCloseLink(PIRDA_LINK_CB pIrdaLinkCb)
{
    IRDA_MSG    IMsg;
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) pIrdaLinkCb->IrlapContext;

    IRLAP_LOG_START((pIrlapCb, TEXT("IRLAP: CloseLink")));

    ReturnRxTxWinMsgs(pIrlapCb);    

    IrlapTimerStop(TEXT("IrlapCloseLink"), pIrlapCb, &pIrlapCb->SlotTimer);
    IrlapTimerStop(TEXT("IrlapCloseLink"), pIrlapCb, &pIrlapCb->QueryTimer);
    IrlapTimerStop(TEXT("IrlapCloseLink"), pIrlapCb, &pIrlapCb->PollTimer);
    IrlapTimerStop(TEXT("IrlapCloseLink"), pIrlapCb, &pIrlapCb->FinalTimer);
    IrlapTimerStop(TEXT("IrlapCloseLink"), pIrlapCb, &pIrlapCb->WDogTimer);
    IrlapTimerStop(TEXT("IrlapCloseLink"), pIrlapCb, &pIrlapCb->BackoffTimer);
    
    IRLAP_LOG_COMPLETE(pIrlapCb);
    
    IMsg.Prim = MAC_CONTROL_REQ;
    IMsg.IRDA_MSG_Op = MAC_CLOSE_LINK;
    IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);    

    return;
}

/*****************************************************************************
*
* Delete memory associated with an Irlap instance
*
*/
VOID
IrlapDeleteInstance(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;
        
#if DBG
    int     i;
    for (i = 0; i < IRLAP_MOD; i++)
    {
        ASSERT(pIrlapCb->TxWin.pMsg[i] == NULL);
        ASSERT(pIrlapCb->RxWin.pMsg[i] == NULL);
    }    
#endif

    FreeDevList(&pIrlapCb->DevList);
    
    IRDA_FREE_MEM(pIrlapCb);    
}

/*****************************************************************************
*
* InitializeState - resets link control block
*
*/
VOID
InitializeState(PIRLAP_CB pIrlapCb,
                IRLAP_STN_TYPE StationType)
{
//    IRDA_MSG    *pMsg;
//    int         i;

    pIrlapCb->StationType = StationType;

    if (StationType == PRIMARY)
        pIrlapCb->CRBit = IRLAP_CMD;
    else
        pIrlapCb->CRBit = IRLAP_RSP;

    pIrlapCb->RemoteBusy        = FALSE;
    pIrlapCb->LocalBusy         = FALSE;
    pIrlapCb->ClrLocalBusy      = FALSE;
    pIrlapCb->NoResponse        = FALSE;
    pIrlapCb->LocalDiscReq      = FALSE;
    pIrlapCb->ConnAfterClose    = FALSE;
    pIrlapCb->DscvAfterClose    = FALSE;
    pIrlapCb->GenNewAddr        = FALSE;
    pIrlapCb->StatusSent        = FALSE;    
    pIrlapCb->Vs                = 0;
    pIrlapCb->Vr                = 0;
    pIrlapCb->WDogExpCnt        = 0;    
    pIrlapCb->FastPollCount     = 0;

    FreeDevList(&pIrlapCb->DevList);

    memset(&pIrlapCb->RemoteQos, 0, sizeof(IRDA_QOS_PARMS));
    memset(&pIrlapCb->NegotiatedQos, 0, sizeof(IRDA_QOS_PARMS));

    // Return msgs on tx list and in tx/rx windows
    ReturnRxTxWinMsgs(pIrlapCb);
}

/*****************************************************************************
*
*  IrlapDown - Entry point into IRLAP for LMP
*
*/
UINT
IrlapDown(PVOID     Context,
          PIRDA_MSG pMsg)
{
    PIRLAP_CB   pIrlapCb    = (PIRLAP_CB) Context;
    UINT        rc          = SUCCESS;

    IRLAP_LOG_START((pIrlapCb, IRDA_PrimStr[pMsg->Prim]));

    switch (pMsg->Prim)
    {
      case IRLAP_DISCOVERY_REQ:
        rc = ProcessDiscoveryReq(pIrlapCb, pMsg);
        break;

      case IRLAP_CONNECT_REQ:
        rc = ProcessConnectReq(pIrlapCb, pMsg);
        break;

      case IRLAP_CONNECT_RESP:
        ProcessConnectResp(pIrlapCb, pMsg);
        break;

      case IRLAP_DISCONNECT_REQ:
        ProcessDisconnectReq(pIrlapCb);
        break;

      case IRLAP_DATA_REQ:
      case IRLAP_UDATA_REQ:
        rc = ProcessDataAndUDataReq(pIrlapCb, pMsg);
        break;

      case IRLAP_FLOWON_REQ:
        if (pIrlapCb->LocalBusy)
        {
            IRLAP_LOG_ACTION((pIrlapCb,TEXT("Local busy condition cleared")));
            pIrlapCb->LocalBusy = FALSE;
            pIrlapCb->ClrLocalBusy = TRUE;
        }
        break;

      case IRLAP_LINKCONTROL_REQ:
        rc = ProcessLinkControlReq(pIrlapCb, pMsg);
        break;

      default:
        ASSERT(0);
        rc = IRLAP_BAD_PRIM;

    }

    IRLAP_LOG_COMPLETE(pIrlapCb);

    return rc;
}
/*****************************************************************************
*
*  IrlapUp - Entry point into IRLAP for MAC
*
*/
VOID
IrlapUp(PVOID Context, PIRDA_MSG pMsg)
{
    BOOLEAN     FreeMsg     = TRUE;
    PIRLAP_CB   pIrlapCb    = (PIRLAP_CB) Context;


    switch (pMsg->Prim)
    {
      case MAC_DATA_IND:
//        IRLAP_LOG_START((pIrlapCb, TEXT("MAC_DATA_IND: %s"), FrameToStr(pMsg)));
        IRLAP_LOG_START((pIrlapCb, TEXT("MAC_DATA_IND")));

        ProcessMACDataInd(pIrlapCb, pMsg, &FreeMsg);

        if (FreeMsg)
        {
            pMsg->Prim = MAC_DATA_RESP;
            IrmacDown(pIrlapCb->pIrdaLinkCb, pMsg);
        }

        break;

      case MAC_DATA_CONF:
        
        IRLAP_LOG_START((pIrlapCb, TEXT("MAC_DATA_CONF")));
        // IrNDIS will only confirm if the ref count is 0. this will ensure
        // that we are not messing up message when retranmitting.
        // Proper way may be to incorporate send confirmations into the LAP state
        // machine and not retransmit until the message is confirmed.
        ASSERT(pMsg->IRDA_MSG_SendRefCnt == 0);
        pMsg->Prim = IRLAP_DATA_CONF;
        pMsg->IRDA_MSG_DataStatus = IRLAP_DATA_REQUEST_COMPLETED;

        IrlmpUp(pIrlapCb->pIrdaLinkCb, pMsg);
        break;

      case MAC_CONTROL_CONF:
        IRLAP_LOG_START((pIrlapCb, IRDA_PrimStr[pMsg->Prim]));
        ProcessMACControlConf(pIrlapCb, pMsg);
        break;

      default:
        IRLAP_LOG_START((pIrlapCb, IRDA_PrimStr[pMsg->Prim]));
        ASSERT(0); //rc = IRLAP_BAD_PRIM;

    }

    IRLAP_LOG_COMPLETE(pIrlapCb);
}

/*****************************************************************************
*
*/
VOID
ReturnRxTxWinMsgs(PIRLAP_CB pIrlapCb)
{
    int         i;
    IRDA_MSG   *pMsg;

    // Return messages on TxMsgList to LMP
    while (DequeMsgList(&pIrlapCb->TxMsgList, &pMsg) == SUCCESS)
    {
        pMsg->Prim += 2; // make it a confirm
        pMsg->IRDA_MSG_DataStatus = IRLAP_DATA_REQUEST_FAILED_LINK_RESET;
        IrlmpUp(pIrlapCb->pIrdaLinkCb, pMsg); 
    }

    pIrlapCb->TxWin.Start = 0;
    pIrlapCb->TxWin.End = 0;
    // Transmit window
    for (i = 0; i < IRLAP_MOD; i++)
    {
        if (pIrlapCb->TxWin.pMsg[i] != NULL)
        {
            pIrlapCb->TxWin.pMsg[i]->Prim = IRLAP_DATA_CONF;
            pIrlapCb->TxWin.pMsg[i]->IRDA_MSG_DataStatus =
            IRLAP_DATA_REQUEST_FAILED_LINK_RESET;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, pIrlapCb->TxWin.pMsg[i]); // rc !!!

            pIrlapCb->TxWin.pMsg[i] = NULL;
        }
    }

    // Cleanup RxWin
    pIrlapCb->RxWin.Start = 0;
    pIrlapCb->RxWin.End = 0;
    for (i = 0; i < IRLAP_MOD; i++)
    {
        // Receive window
        if ((pMsg = pIrlapCb->RxWin.pMsg[i]) != NULL)
        {
            pMsg->Prim = MAC_DATA_RESP;
            IrmacDown(pIrlapCb->pIrdaLinkCb, pMsg);
            pIrlapCb->RxWin.pMsg[i] = NULL;
        }
    }

    return;
}

/*****************************************************************************
*
* MyDevAddr - Determines if DevAddr matches the local
*             device address or is the broadcast
*
* TRUE if address is mine or broadcast else FALS
*
*
*/
BOOLEAN
MyDevAddr(PIRLAP_CB pIrlapCb,
          UCHAR       DevAddr[])
{
    if (CTEMemCmp(DevAddr, IrlapBroadcastDevAddr,
                  IRDA_DEV_ADDR_LEN) &&
        CTEMemCmp(DevAddr, pIrlapCb->LocalDevice.DevAddr,
                  IRDA_DEV_ADDR_LEN))
    {
        return FALSE;
    }
    return TRUE;
}

/*****************************************************************************
*
*    ProcessConnectReq - Process connect request from LMP
*
*/
UINT
ProcessConnectReq(PIRLAP_CB pIrlapCb,
                  PIRDA_MSG pMsg)
{
    IRDA_MSG IMsg;
    
    switch (pIrlapCb->State)
    {
      case NDM:
        // Save Remote Address for later use
        CTEMemCopy(pIrlapCb->RemoteDevice.DevAddr,
                      pMsg->IRDA_MSG_RemoteDevAddr,
                      IRDA_DEV_ADDR_LEN);

        IMsg.Prim = MAC_CONTROL_REQ;
        IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
        IMsg.IRDA_MSG_SenseTime = IRLAP_MEDIA_SENSE_TIME;
        
        IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);
        pIrlapCb->State = CONN_MEDIA_SENSE;
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (media sense)")));
        break;

      case P_CLOSE:
        CTEMemCopy(pIrlapCb->RemoteDevice.DevAddr,
                       pMsg->IRDA_MSG_RemoteDevAddr, IRDA_DEV_ADDR_LEN);
        pIrlapCb->ConnAfterClose = TRUE;
        break;
        
       case DSCV_REPLY:
       default:
          return IRLAP_REMOTE_DISCOVERY_IN_PROGRESS_ERR;
    }

    return SUCCESS;
}
/*****************************************************************************
*
*   ProcessConnectResp - Process connect response from LMP
*
*/
VOID
ProcessConnectResp(PIRLAP_CB pIrlapCb,
                   PIRDA_MSG pMsg)
{

    if (pIrlapCb->State != SNRM_RECEIVED)
    {
        ASSERT(0);
        return;
    }

    pIrlapCb->ConnAddr = pIrlapCb->SNRMConnAddr;
    SendUA(pIrlapCb, TRUE);
    ApplyQosParms(pIrlapCb);

    InitializeState(pIrlapCb, SECONDARY);
    // start watchdog timer with poll timeout
    IrlapTimerStart(TEXT("ProcessConnectResp"), pIrlapCb, &pIrlapCb->WDogTimer);
    pIrlapCb->State = S_NRM;
}
/*****************************************************************************
*
*   ProcessLinkControlReq - Process Link control request from LMP
*
*/
UINT
ProcessLinkControlReq(PIRLAP_CB pIrlapCb,
                      PIRDA_MSG pMsg)
{
    IRDA_MSG    IMsg;

    IMsg.Prim = MAC_CONTROL_REQ;

    while (pIrlapCb->State != NDM)
    {
        DEBUGMSG(1, (TEXT("Irlap link control waiting for LAP NDM.\r\n")));
        Sleep(200);
    }
    if (pMsg->IRDA_MSG_AcquireResources == TRUE)
    {
        UCHAR  DscvInfoBuf[64];
        int    DscvInfoLen;
        ULONG  Val, Mask;    
        int    i;
        
        // Update the device discovery info. IrLMP has already updated
        // NickName & NickNameLen.
        // Build the discovery info    
        Val = Hints;
        DscvInfoLen = 0;
        for (i = 0, Mask = 0xFF000000; i < 4; i++, Mask >>= 8)
        {
            if (Mask & Val || DscvInfoLen > 0)
            {
                DscvInfoBuf[DscvInfoLen++] = (UCHAR)
                    ((Mask & Val) >> (8 * (3-i)));
            }
        }
        DscvInfoBuf[DscvInfoLen++] = CharSet;
        RtlCopyMemory(DscvInfoBuf+DscvInfoLen, NickName, NickNameLen);
        DscvInfoLen += NickNameLen;    

        DscvInfoLen = DscvInfoLen > IRLAP_DSCV_INFO_LEN ?
            IRLAP_DSCV_INFO_LEN : DscvInfoLen;
        
        CTEMemCopy(pIrlapCb->LocalDevice.DscvInfo, DscvInfoBuf, DscvInfoLen);
    
        pIrlapCb->LocalDevice.DscvInfoLen = DscvInfoLen;
    
        IMsg.IRDA_MSG_Op = MAC_ACQUIRE_RESOURCES;
    }
    else
    {
        IMsg.IRDA_MSG_Op = MAC_RELEASE_RESOURCES;
    }
    IMsg.IRDA_MSG_pMacConfig = pMsg->IRDA_MSG_pLinkConfig;

    IrmacDown(pMsg->IRDA_MSG_pLinkContext, &IMsg);

    return (IMsg.IRDA_MSG_MacConfigStatus);
}
/*****************************************************************************
*
*   ProcessDiscoveryReq - Process Discovery request from LMP
*
*/
UINT
ProcessDiscoveryReq(PIRLAP_CB pIrlapCb,
                    PIRDA_MSG pMsg)
{
    IRDA_MSG    IMsg;
    
    switch (pIrlapCb->State)
    {
      case NDM:
        if (pMsg->IRDA_MSG_SenseMedia == TRUE)
        {
            IMsg.Prim = MAC_CONTROL_REQ;
            IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
            IMsg.IRDA_MSG_SenseTime = IRLAP_MEDIA_SENSE_TIME;            
            IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);
            pIrlapCb->State = DSCV_MEDIA_SENSE;
            IRLAP_LOG_ACTION((pIrlapCb,TEXT("MAC_CONTROL_REQ (mediasense)")));
        }
        else
        {
            pIrlapCb->SlotCnt = 0;
            pIrlapCb->GenNewAddr = FALSE;

            FreeDevList(&pIrlapCb->DevList);

            SendDscvXIDCmd(pIrlapCb);

            IMsg.Prim = MAC_CONTROL_REQ;
            IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
            IMsg.IRDA_MSG_SenseTime = DiscoverTimeout;
            IRLAP_LOG_ACTION((pIrlapCb,TEXT("MAC_CONTROL_REQ (dscvsense)")));
            IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);

            pIrlapCb->State = DSCV_QUERY;
        }
        break;

      case DSCV_REPLY:
        return IRLAP_REMOTE_DISCOVERY_IN_PROGRESS_ERR;

      case SNRM_RECEIVED:
        return IRLAP_REMOTE_CONNECTION_IN_PROGRESS_ERR;

      case P_CLOSE:
        pIrlapCb->DscvAfterClose = TRUE;
        break;
        
      default:
        ASSERT(0);
        return IRLAP_BAD_STATE;
    }
    return SUCCESS;
}
/*****************************************************************************
*
*   ProcessDisconnectReq - Process disconnect request from LMP
*
*/
VOID
ProcessDisconnectReq(PIRLAP_CB pIrlapCb)
{
    ReturnRxTxWinMsgs(pIrlapCb);

    switch (pIrlapCb->State)
    {
      case NDM:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        break;

      case SNRM_SENT:
        IrlapTimerStop(TEXT("ProcessDisconnectResp: SNRM_SENT"), pIrlapCb, &pIrlapCb->FinalTimer);
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
      case DSCV_REPLY:
      case CONN_MEDIA_SENSE:
        pIrlapCb->State = NDM;
        break;

      case BACKOFF_WAIT:
        IrlapTimerStop(TEXT("ProcessDisconnectResp: BACKOFF_WAIT"), pIrlapCb, &pIrlapCb->BackoffTimer);
        pIrlapCb->State = NDM;
        break;

      case SNRM_RECEIVED:
        pIrlapCb->ConnAddr = pIrlapCb->SNRMConnAddr;
        SendDM(pIrlapCb);
        pIrlapCb->ConnAddr = IRLAP_BROADCAST_CONN_ADDR;
        pIrlapCb->State = NDM;
        break;

      case P_XMIT:
        pIrlapCb->LocalDiscReq = TRUE;
        IrlapTimerStop(TEXT("ProcessDisconnectResp: P_XMIT"), pIrlapCb, &pIrlapCb->PollTimer);
        SendDISC(pIrlapCb);
        IrlapTimerStart(TEXT("ProcessDisconnectResp: P_XMIT"), pIrlapCb, &pIrlapCb->FinalTimer);
        pIrlapCb->RetryCnt = 0;
        pIrlapCb->State = P_CLOSE;
        break;

      case P_RECV:
        pIrlapCb->LocalDiscReq = TRUE;
        pIrlapCb->State = P_DISCONNECT_PEND;
        break;

      case S_NRM:
        pIrlapCb->LocalDiscReq = TRUE;
        pIrlapCb->State = S_DISCONNECT_PEND;
        break;

      default:
        ASSERT(0);
        // return IRLAP_BAD_STATE;
    }
}
/*****************************************************************************
*
*   ProcessDataReq - Process data request from LMP
*
*/
UINT
ProcessDataAndUDataReq(PIRLAP_CB pIrlapCb,
                       PIRDA_MSG pMsg)
{
    BOOLEAN LinkTurned;
    int  DataSize = (pMsg->IRDA_MSG_pHdrWrite - pMsg->IRDA_MSG_pHdrRead) +
                    (pMsg->IRDA_MSG_pWrite - pMsg->IRDA_MSG_pRead);
    UINT rc;

    if (DataSize > pIrlapCb->RemoteDataSize)
    {
        rc = IRLAP_BAD_DATA_REQUEST;
        goto pdaudr_fail;
    }

    switch (pIrlapCb->State)
    {
      case P_XMIT:
        // Enque message, then drain the message list. If the link
        // was turned in the process of draining messages stop Poll Timer,
        // start Final Timer and enter P_RECV. Otherwise we'll stay in P_XMIT
        // waiting for more data requests from LMP or Poll Timer expiration
        EnqueMsgList(&pIrlapCb->TxMsgList, pMsg, -1);

        XmitTxMsgList(pIrlapCb, FALSE, &LinkTurned);

        if (LinkTurned)
        {
           IrlapTimerStop(TEXT("ProcessDataAndUDataReq: P_XMIT"), pIrlapCb, &pIrlapCb->PollTimer);
           IrlapTimerStart(TEXT("ProcessDataAndUDataReq: P_XMIT"), pIrlapCb, &pIrlapCb->FinalTimer);
           pIrlapCb->State = P_RECV;
        }
        return SUCCESS;

      case P_DISCONNECT_PEND: //For pending disc states, take the message.
      case S_DISCONNECT_PEND: // They will be returned when the link discs
      case P_RECV:
      case S_NRM:
        // Que the message for later transmission

        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Queueing request")));

        EnqueMsgList(&pIrlapCb->TxMsgList, pMsg, -1);
        
        return SUCCESS;

      default:
        if (pMsg->Prim == IRLAP_DATA_REQ)
        {
            ASSERT(0);
            rc = IRLAP_BAD_STATE;
            goto pdaudr_fail;
        }
        else
        {
            if (pIrlapCb->State == NDM)
            {
                SendUIFrame(pIrlapCb, pMsg);
            }
            else
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
                rc = SUCCESS;
                goto pdaudr_fail;
            }
        }
    }
    return SUCCESS;

pdaudr_fail:
    //
    // Check if the msg should be freed
    //
    if ((NULL == pMsg->IRDA_MSG_pOwner) && (pMsg->IRDA_MSG_SegFlags & SEG_LOCAL)) {
        FreeIrdaBuf(IrdaMsgPool, pMsg);
    }
    return rc;

}
/*****************************************************************************
*
*/
VOID
XmitTxMsgList(PIRLAP_CB pIrlapCb, BOOLEAN AlwaysTurnLink,
              BOOLEAN *pLinkTurned)
{
    IRDA_MSG    *pMsg;
    UINT        LinkTurned;

    LinkTurned = FALSE;

    // If the remote is not busy send data
    // If we need to clear the local busy condition, don't send data send RR
    if (!pIrlapCb->RemoteBusy && !pIrlapCb->ClrLocalBusy)
    {
        while (!LinkTurned &&
               (DequeMsgList(&pIrlapCb->TxMsgList, &pMsg) == SUCCESS))
        {
            pIrlapCb->FastPollCount = IRLAP_FAST_POLL_COUNT;
            pIrlapCb->PollTimer.Timeout = IRLAP_FAST_POLL_TIME > 
                pIrlapCb->RemoteMaxTAT ? pIrlapCb->RemoteMaxTAT : IRLAP_FAST_POLL_TIME;

            if (pMsg->Prim == IRLAP_DATA_REQ)
            {
                // Insert message into transmit window
                pIrlapCb->TxWin.pMsg[pIrlapCb->Vs] = pMsg;

                pMsg->IRDA_MSG_SendRefCnt = 1;

                // Send message. If full window or there are no
                // more data requests, send with PF Set (turns link).
                if ((pIrlapCb->Vs == (pIrlapCb->TxWin.Start +
                      pIrlapCb->RemoteWinSize-1) % IRLAP_MOD) ||
                      (0 == pIrlapCb->TxMsgList.Len /*AlwaysTurnLink*/))
                {
                    SendIFrame(pIrlapCb,
                               pMsg,
                               pIrlapCb->Vs,
                               IRLAP_PFBIT_SET);
                    LinkTurned = TRUE;
                }
                else
                {
                    SendIFrame(pIrlapCb,
                               pMsg,
                               pIrlapCb->Vs,
                               IRLAP_PFBIT_CLEAR);
                }
                pIrlapCb->Vs = (pIrlapCb->Vs + 1) % IRLAP_MOD;
            }
            else // IRLAP_UDATA_REQUEST
            {
                // For now, always turn link
                SendUIFrame(pIrlapCb, pMsg);
                pMsg->Prim = IRLAP_UDATA_CONF;
                pMsg->IRDA_MSG_DataStatus = IRLAP_DATA_REQUEST_COMPLETED;
                IrlmpUp(pIrlapCb->pIrdaLinkCb, pMsg);
                LinkTurned = TRUE;
            }
        }
        pIrlapCb->TxWin.End = pIrlapCb->Vs;
    }

    if ((AlwaysTurnLink && !LinkTurned) || pIrlapCb->ClrLocalBusy)
    {
        SendRR_RNR(pIrlapCb);
        LinkTurned = TRUE;
        if (pIrlapCb->ClrLocalBusy)
        {
            pIrlapCb->ClrLocalBusy = FALSE;
        }
    }

    if (pLinkTurned != NULL)
    {
        *pLinkTurned = LinkTurned;
    }
}

VOID
GotoPCloseState(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG IMsg;
    
    if (!pIrlapCb->LocalDiscReq)
    {
        IMsg.Prim = IRLAP_DISCONNECT_IND;
        IMsg.IRDA_MSG_DiscStatus = IRLAP_REMOTE_INITIATED;
        IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
    }

    ReturnRxTxWinMsgs(pIrlapCb);
    pIrlapCb->State = P_CLOSE;
}

VOID
GotoNDMThenDscvOrConn(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG IMsg;
    
    if (pIrlapCb->ConnAfterClose)
    {
        pIrlapCb->ConnAfterClose = FALSE;
        IMsg.Prim = MAC_CONTROL_REQ;
        IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
        IMsg.IRDA_MSG_SenseTime = IRLAP_MEDIA_SENSE_TIME;
        
        IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);
        pIrlapCb->State = CONN_MEDIA_SENSE;
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (media sense)")));
        return;
    }

    if (pIrlapCb->DscvAfterClose)
    {
        pIrlapCb->DscvAfterClose = FALSE;
        IMsg.Prim = MAC_CONTROL_REQ;
        IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
        IMsg.IRDA_MSG_SenseTime = IRLAP_MEDIA_SENSE_TIME;        
        IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);
        pIrlapCb->State = DSCV_MEDIA_SENSE;
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (media sense)")));
        return;
    }
    pIrlapCb->State = NDM;
    return;
}

/*****************************************************************************
*
*   ProcessMACControlConf - Process a control confirm from MAC
*
*/
VOID
ProcessMACControlConf(PIRLAP_CB pIrlapCb, PIRDA_MSG pMsg)
{
    IRDA_MSG    IMsg;
    
    if (pMsg->IRDA_MSG_Op != MAC_MEDIA_SENSE)
    {
        ASSERT(0);
        return; //IRLAP_BAD_OP;
    }

    switch (pIrlapCb->State)
    {
      case DSCV_MEDIA_SENSE:
        switch (pMsg->IRDA_MSG_OpStatus)
        {
          case MAC_MEDIA_CLEAR:
               
            //IndicateLinkStatus(pIrlapCb, LINK_STATUS_DISCOVERING);

            pIrlapCb->SlotCnt = 0;
            pIrlapCb->GenNewAddr = FALSE;

            FreeDevList(&pIrlapCb->DevList);

            SendDscvXIDCmd(pIrlapCb);

            IMsg.Prim = MAC_CONTROL_REQ;
            IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
            IMsg.IRDA_MSG_SenseTime = DiscoverTimeout;
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (dscvsense)")));
            IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);
            pIrlapCb->State = DSCV_QUERY;
            break;

          case MAC_MEDIA_BUSY:
            pIrlapCb->State = NDM;
            IMsg.Prim = IRLAP_DISCOVERY_CONF;
            IMsg.IRDA_MSG_pDevList = NULL;
            IMsg.IRDA_MSG_DscvStatus = MAC_MEDIA_BUSY;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            break;

          default:
            ASSERT(0);
            return;// IRLAP_BAD_OPSTATUS;
        }
        break;

      case CONN_MEDIA_SENSE:
        switch (pMsg->IRDA_MSG_OpStatus)
        {
          case MAC_MEDIA_CLEAR:

            // Generate a random connection address
            pIrlapCb->ConnAddr = IRLAP_RAND(1, 0x7e);

            pIrlapCb->RetryCnt = 0;

            SendSNRM(pIrlapCb, TRUE);
            IrlapTimerStart(TEXT("ProcessMACControlConf: CONN_MEDIA_SENSE"), 
                            pIrlapCb, &pIrlapCb->FinalTimer);
            pIrlapCb->State = SNRM_SENT;
            break;

          case MAC_MEDIA_BUSY:
            pIrlapCb->State = NDM;
            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = MAC_MEDIA_BUSY;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            break;

          default:
            ASSERT(0);
            return;// IRLAP_BAD_OPSTATUS;
        }
        break;

      case DSCV_QUERY:
        switch (pMsg->IRDA_MSG_OpStatus)
        {
          case MAC_MEDIA_CLEAR:
            // Nobody responded, procede as if the slot timer expired

            IRLAP_LOG_ACTION((pIrlapCb,
                              TEXT("Media clear, making fake slot exp")));
              
            SlotTimerExp(pIrlapCb);
            break;

          case MAC_MEDIA_BUSY:
            // Some responding, give'm more time

            IRLAP_LOG_ACTION((pIrlapCb,
                              TEXT("Media busy, starting slot timer")));
            
            IrlapTimerStart(TEXT("ProcessMACControlConf: DSCV_QUERY"), 
                            pIrlapCb, &pIrlapCb->SlotTimer);
            break;
        }
        break;
      
      default:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
    }
}
/*****************************************************************************
*
*   ProcessMACDataInd - Processes MAC Data
*
*
*/
VOID
ProcessMACDataInd(PIRLAP_CB pIrlapCb, PIRDA_MSG pMsg, BOOLEAN *pFreeMsg)
{
    int Addr        = (int) IRLAP_GET_ADDR(*(pMsg->IRDA_MSG_pRead));
    int CRBit       = (int) IRLAP_GET_CRBIT(*(pMsg->IRDA_MSG_pRead));
    int Cntl        = (int) *(pMsg->IRDA_MSG_pRead + 1);
    int PFBit       = IRLAP_GET_PFBIT(Cntl);
    UINT Ns         = IRLAP_GET_NS(Cntl);
    UINT Nr         = IRLAP_GET_NR(Cntl);
    int XIDFormatID = (int) *(pMsg->IRDA_MSG_pRead+2);
    IRLAP_XID_DSCV_FORMAT *pXIDFormat  = (IRLAP_XID_DSCV_FORMAT *)
                                          (pMsg->IRDA_MSG_pRead + 3);
    IRLAP_SNRM_FORMAT     *pSNRMFormat = (IRLAP_SNRM_FORMAT *)
                                          (pMsg->IRDA_MSG_pRead + 2);
    IRLAP_UA_FORMAT       *pUAFormat   = (IRLAP_UA_FORMAT *)
                                          (pMsg->IRDA_MSG_pRead + 2);

    if (Addr != pIrlapCb->ConnAddr && Addr != IRLAP_BROADCAST_CONN_ADDR)
    {
        IRLAP_LOG_ACTION((pIrlapCb,
                          TEXT("Ignoring, connection address %02X"), Addr));
        return;
    }

    pIrlapCb->StatusSent = FALSE;

    pIrlapCb->Frmr.CntlField = Cntl; // for later maybe

    // Peer has sent a frame so clear the NoResponse condition
    if (pIrlapCb->NoResponse)
    {
        pIrlapCb->NoResponse = FALSE;
        pIrlapCb->RetryCnt = 0;
        pIrlapCb->WDogExpCnt = 0;

        //IndicateLinkStatus(pIrlapCb, LINK_STATUS_CONNECTED);
    }

    switch (IRLAP_FRAME_TYPE(Cntl))
    {
      /*****************/
      case IRLAP_I_FRAME:
      /*****************/
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("I-frame")));
        ProcessIFrame(pIrlapCb, pMsg, CRBit, PFBit, Ns, Nr, pFreeMsg);
        return;

      /*****************/
      case IRLAP_S_FRAME:
      /*****************/
        switch (IRLAP_GET_SCNTL(Cntl))
        {
          /*-----------*/
          case IRLAP_RR:
          case IRLAP_RNR:
          /*-----------*/
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("RR/RNR-frame")));
            ProcessRR_RNR(pIrlapCb, IRLAP_GET_SCNTL(Cntl),
                          pMsg, CRBit, PFBit, Nr);
            return;
            
          /*------------*/
          case IRLAP_SREJ:
          case IRLAP_REJ:
          /*------------*/
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("SJREJ/REJ-frame")));
            ProcessREJ_SREJ(pIrlapCb, IRLAP_GET_SCNTL(Cntl),
                            pMsg, CRBit, PFBit, Nr);
            return;
        }
        break;

      /*****************/
      case IRLAP_U_FRAME:
      /*****************/
        switch (IRLAP_GET_UCNTL(Cntl))
        {
          /*---------------*/
          case IRLAP_XID_CMD:
          /*---------------*/
            // Should always be a command
            if (CRBit != IRLAP_CMD)
            {
                IRLAP_LOG_ACTION((pIrlapCb,
                                  TEXT("Received XID cmd with CRBit = rsp")));
                ASSERT(0);
                return; // IRLAP_XID_CMD_RSP;
            }
            // Poll bit should always be set
            if (PFBit != IRLAP_PFBIT_SET)
            {
                IRLAP_LOG_ACTION((pIrlapCb, 
                   TEXT("Received XID command without Poll set")));
                ASSERT(0);
                return; // IRLAP_XID_CMD_NOT_P;
            }

            if (XIDFormatID == IRLAP_XID_DSCV_FORMAT_ID)
            {
                // Slot No is less than max slot or 0xff
                if (pXIDFormat->SlotNo>IrlapSlotTable[pXIDFormat->NoOfSlots]
                    && pXIDFormat->SlotNo != IRLAP_END_DSCV_SLOT_NO)
                {
                    IRLAP_LOG_ACTION((pIrlapCb,
                                      TEXT("Invalid slot number %d"),
                                      pXIDFormat->SlotNo));
                    ASSERT(0);
                    return;// IRLAP_BAD_SLOTNO;
                }
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("DscvXIDCmd")));
                ProcessDscvXIDCmd(pIrlapCb, pXIDFormat,
                                  pMsg->IRDA_MSG_pWrite);
                return;
            }
            else
            {
                return; // ignore per errata
            }

          /*---------------*/
          case IRLAP_XID_RSP:
          /*---------------*/
            if (XIDFormatID == IRLAP_XID_DSCV_FORMAT_ID)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("DscvXIDRsp")));
                ProcessDscvXIDRsp(pIrlapCb, pXIDFormat,pMsg->IRDA_MSG_pWrite);
                return;
            }
            else
            {
                return; // ignore per errata
            }

          /*------------*/
          case IRLAP_SNRM: // or IRLAP_RNRM
          /*------------*/
            if (IRLAP_PFBIT_SET != PFBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb,
                                  TEXT("Received SNRM/RNRM without P set")));
                return;// IRLAP_SNRM_NOT_P;
            }
            if (IRLAP_CMD == CRBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("SNRM")));
                ProcessSNRM(pIrlapCb, pSNRMFormat, pMsg->IRDA_MSG_pWrite);
                return;
            }
            else
            {
                ProcessRNRM(pIrlapCb);
                return;
            }

          /*----------*/
          case IRLAP_UA:
          /*----------*/
            if (CRBit != IRLAP_RSP)
            {
                IRLAP_LOG_ACTION((pIrlapCb,
                                  TEXT("Received UA as a command")));
                return;// IRLAP_UA_NOT_RSP;
            }
            if (PFBit != IRLAP_PFBIT_SET)
            {
                IRLAP_LOG_ACTION((pIrlapCb,
                                  TEXT("Received UA without F set")));
                return;// IRLAP_UA_NOT_F;
            }
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("UA")));
            ProcessUA(pIrlapCb, pUAFormat, pMsg->IRDA_MSG_pWrite);
            return;

          /*------------*/
          case IRLAP_DISC: // or IRLAP_RD
          /*------------*/
            if (IRLAP_PFBIT_SET != PFBit)
            {
              IRLAP_LOG_ACTION((pIrlapCb, 
                   TEXT("Received DISC/RD command without Poll set")));
              return;// IRLAP_DISC_CMD_NOT_P;
            }
            if (IRLAP_CMD == CRBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("DISC")));
                ProcessDISC(pIrlapCb);
                return;
            }
            else
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("RD")));
                ProcessRD(pIrlapCb);
                return;
            }

          /*----------*/
          case IRLAP_UI:
          /*----------*/
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("UI")));
            ProcessUI(pIrlapCb, pMsg, CRBit, PFBit);
            return;

          /*------------*/
          case IRLAP_TEST:
          /*------------*/
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("TEST")));
            ProcessTEST(pIrlapCb, pMsg, pUAFormat, CRBit, PFBit);
            return;

          /*------------*/
          case IRLAP_FRMR:
          /*------------*/
            if (IRLAP_RSP != CRBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb,
                                  TEXT("Received FRMR cmd (must be resp)")));
                return;// IRLAP_FRMR_RSP_CMD;
            }
            if (IRLAP_PFBIT_SET != PFBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, 
                     TEXT("Received FRMR resp without Final set")));
                return;// IRLAP_FRMR_RSP_NOT_F;
            }
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("FRMR")));
            ProcessFRMR(pIrlapCb);
            return;

          /*----------*/
          case IRLAP_DM:
          /*----------*/
            if (IRLAP_RSP != CRBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, 
                     TEXT("Received DM command (must be response)")));
                return;// IRLAP_DM_RSP_CMD;
            }
            if (IRLAP_PFBIT_SET != PFBit)
            {
                IRLAP_LOG_ACTION((pIrlapCb, 
                      TEXT("Received DM response without Final set")));
                return;// IRLAP_DM_RSP_NOT_F;
            }
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("DM")));
            ProcessDM(pIrlapCb);
            return;
        }
        break;
    }
}
/*****************************************************************************
*
*  ProcessDscvXIDCmd - Process received XID Discovery command
*
*/
VOID
ProcessDscvXIDCmd(PIRLAP_CB pIrlapCb,
                  IRLAP_XID_DSCV_FORMAT *pXidFormat,
                  UCHAR *pEndDscvInfoUCHAR)
{
    IRDA_MSG    IMsg;
    
    if (!MyDevAddr(pIrlapCb, pXidFormat->DestAddr))
    {
/*        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring XID addressed to:%02X%02X%02X%02X"),
                          EXPAND_ADDR(pXidFormat->DestAddr)));*/
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring XID addressed to %X"),
                          pXidFormat->DestAddr));        
        return;
    }

    if (pXidFormat->SlotNo == IRLAP_END_DSCV_SLOT_NO)
    {
        pIrlapCb->GenNewAddr = FALSE;
        switch (pIrlapCb->State)
        {
          case DSCV_QUERY:
            IrlapTimerStop(TEXT("ProcessDscvXIDCmd: DSCV_QUERY"), 
                           pIrlapCb, &pIrlapCb->SlotTimer);

            IMsg.Prim = IRLAP_DISCOVERY_CONF;
            IMsg.IRDA_MSG_pDevList = NULL;
            IMsg.IRDA_MSG_DscvStatus =
                IRLAP_REMOTE_DISCOVERY_IN_PROGRESS;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            // fall through. Send indication to LMP

          case DSCV_REPLY:
            if (pIrlapCb->State == DSCV_REPLY)
            {
                IrlapTimerStop(TEXT("ProcessDscvXIDCmd: DSCV_REPLY"),
                               pIrlapCb, &pIrlapCb->QueryTimer);
            }

            // Place the device information in the control block
            ExtractDeviceInfo(&pIrlapCb->RemoteDevice, pXidFormat,
                              pEndDscvInfoUCHAR);

            if (!DevInDevList(pXidFormat->SrcAddr, &pIrlapCb->DevList))
            {
                AddDevToList(pIrlapCb, pXidFormat, pEndDscvInfoUCHAR);
            }

            // Notifiy LMP
            pIrlapCb->State = NDM;
            IMsg.Prim = IRLAP_DISCOVERY_IND;
            IMsg.IRDA_MSG_pDevList = &pIrlapCb->DevList;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            break;

          default:
            IRLAP_LOG_ACTION((pIrlapCb,
                              TEXT("Ignoring End XID in this state")));
        }
    }
    else // in middle of discovery process
    {
        switch (pIrlapCb->State)
        {
          case DSCV_MEDIA_SENSE:
            IMsg.Prim = IRLAP_DISCOVERY_CONF;
            IMsg.IRDA_MSG_pDevList = NULL;
            IMsg.IRDA_MSG_DscvStatus =
                IRLAP_REMOTE_DISCOVERY_IN_PROGRESS;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            // fall through

          case NDM:
            InitDscvCmdProcessing(pIrlapCb, pXidFormat);
            pIrlapCb->State = DSCV_REPLY;
            break;

          case DSCV_QUERY:
            pIrlapCb->State = NDM;
            IMsg.Prim = IRLAP_DISCOVERY_CONF;
            IMsg.IRDA_MSG_pDevList = NULL;
            IMsg.IRDA_MSG_DscvStatus = IRLAP_DISCOVERY_COLLISION;
            IrlapTimerStop(TEXT("ProcessDscvXIDCmd: DSCV_QUERY2"), 
                           pIrlapCb, &pIrlapCb->SlotTimer);
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            break;

          case DSCV_REPLY:
            if (pXidFormat->GenNewAddr)
            {
                pIrlapCb->GenNewAddr = TRUE;
                IrlapTimerStop(TEXT("ProcessDscvXIDCmd: DSCV_REPLY2"),
                               pIrlapCb, &pIrlapCb->QueryTimer);
                InitDscvCmdProcessing(pIrlapCb, pXidFormat);
            }
            else
            {
                if (pIrlapCb->RespSlot <= pXidFormat->SlotNo &&
                    !pIrlapCb->DscvRespSent)
                {
                    SendDscvXIDRsp(pIrlapCb);
                    pIrlapCb->DscvRespSent = TRUE;
                }
            }
            break;

          default:
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        }
    }
    return;
}
/*****************************************************************************
*
*
*/
void
ExtractDeviceInfo(IRDA_DEVICE *pDevice, IRLAP_XID_DSCV_FORMAT *pXidFormat,
                  UCHAR *pEndDscvInfoUCHAR)
{
    CTEMemCopy(pDevice->DevAddr, pXidFormat->SrcAddr, IRDA_DEV_ADDR_LEN);
    pDevice->IRLAP_Version = pXidFormat->Version;

    // ??? what about DscvMethod

    pDevice->DscvInfoLen =
        pEndDscvInfoUCHAR > &pXidFormat->FirstDscvInfoByte ?
        pEndDscvInfoUCHAR-&pXidFormat->FirstDscvInfoByte : 0;
    
    memset(pDevice->DscvInfo, 0, sizeof(pDevice->DscvInfo));
    CTEMemCopy(pDevice->DscvInfo, &pXidFormat->FirstDscvInfoByte,
           pDevice->DscvInfoLen);
}
/*****************************************************************************
*
*
*/
VOID
InitDscvCmdProcessing(PIRLAP_CB pIrlapCb,
                      IRLAP_XID_DSCV_FORMAT *pXidFormat)
{
    pIrlapCb->RemoteMaxSlot = IrlapSlotTable[pXidFormat->NoOfSlots];

    pIrlapCb->RespSlot = IRLAP_RAND(pXidFormat->SlotNo,
                                   pIrlapCb->RemoteMaxSlot - 1);

    CTEMemCopy(pIrlapCb->RemoteDevice.DevAddr, pXidFormat->SrcAddr,
                  IRDA_DEV_ADDR_LEN);

    IRLAP_LOG_ACTION((pIrlapCb,
                      TEXT("Responding in slot %d to dev %02X%02X%02X%02X"),
                      pIrlapCb->RespSlot,
                      pIrlapCb->RemoteDevice.DevAddr[0],
                      pIrlapCb->RemoteDevice.DevAddr[1],
                      pIrlapCb->RemoteDevice.DevAddr[2],
                      pIrlapCb->RemoteDevice.DevAddr[3]));

    if (pIrlapCb->RespSlot == pXidFormat->SlotNo)
    {
        SendDscvXIDRsp(pIrlapCb);
        pIrlapCb->DscvRespSent = TRUE;
    }
    else
    {
        pIrlapCb->DscvRespSent = FALSE;
    }

    IrlapTimerStart(TEXT("InitDscvCmdProcessing"),
                    pIrlapCb, &pIrlapCb->QueryTimer);
}
/*****************************************************************************
*
*/
VOID
ProcessDscvXIDRsp(PIRLAP_CB pIrlapCb,
                  IRLAP_XID_DSCV_FORMAT *pXidFormat,
                  UCHAR *pEndDscvInfoUCHAR)
{
    IRDA_MSG    IMsg;
    
    if (pIrlapCb->State == DSCV_QUERY)
    {

        if (DevInDevList(pXidFormat->SrcAddr, &pIrlapCb->DevList))
        {
            IrlapTimerStop(TEXT("ProcessDscvXIDRsp"),
                           pIrlapCb, &pIrlapCb->SlotTimer);
            pIrlapCb->SlotCnt = 0;
            pIrlapCb->GenNewAddr = TRUE;
            FreeDevList(&pIrlapCb->DevList);
            SendDscvXIDCmd(pIrlapCb);

            IMsg.Prim = MAC_CONTROL_REQ;
            IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
            IMsg.IRDA_MSG_SenseTime = DiscoverTimeout;
            IRLAP_LOG_ACTION((pIrlapCb,
                              TEXT("MAC_CONTROL_REQ (dscv sense)")));
            IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);            
        }
        else
        {
            AddDevToList(pIrlapCb, pXidFormat, pEndDscvInfoUCHAR);
        }
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
    }
}
/*****************************************************************************
*
*   DevInDevList - Determines if given device is already in list
*
*/
BOOLEAN
DevInDevList(UCHAR DevAddr[], LIST_ENTRY *pDevList)
{
    IRDA_DEVICE *pDevice;

    pDevice = (IRDA_DEVICE *) pDevList->Flink;

    while (pDevList != (LIST_ENTRY *) pDevice)
    {
        if (CTEMemCmp(pDevice->DevAddr, DevAddr,
                      IRDA_DEV_ADDR_LEN) == 0)
            return (TRUE);

        pDevice = (IRDA_DEVICE *) pDevice->Linkage.Flink;
    }
    return (FALSE);
}
/*****************************************************************************
*
*   AddDevToList - Adds elements in a device list
*
*/
VOID
AddDevToList(PIRLAP_CB pIrlapCb,
             IRLAP_XID_DSCV_FORMAT *pXidFormat,
             UCHAR *pEndDscvInfoUCHAR)
{
    IRDA_DEVICE *pDevice;

    if (IRDA_ALLOC_MEM(pDevice, sizeof(IRDA_DEVICE), MT_IRLAP_DEVICE) == NULL)
    {
        ASSERT(0);
        
        return;// (IRLAP_MALLOC_FAILED);
    }
    else
    {
        ExtractDeviceInfo(pDevice, pXidFormat, pEndDscvInfoUCHAR);

        InsertTailList(&pIrlapCb->DevList, &(pDevice->Linkage));

        IRLAP_LOG_ACTION((pIrlapCb,
                          TEXT("%02X%02X%02X%02X added to Device List"),
                          EXPAND_ADDR(pDevice->DevAddr)));
    }
}
/*****************************************************************************
*
*/
void
FreeDevList(LIST_ENTRY *pDevList)
{
    IRDA_DEVICE *pDevice;

    while (IsListEmpty(pDevList) == FALSE)
    {
        pDevice = (IRDA_DEVICE *) RemoveHeadList(pDevList);
        IRDA_FREE_MEM(pDevice);
    }

    //IRLAP_LOG_ACTION((pIrlapCb, TEXT("Device list cleared")));
}

/*****************************************************************************
*
*/
int
AddressGreaterThan(UCHAR A1[], UCHAR A2[])
{
    int i;
    
    for (i = 0; i < IRDA_DEV_ADDR_LEN; i++)
    {
        if (A1[i] > A2[i])
            return TRUE;
        if (A1[i] != A2[1])
            return FALSE;
    }
    return FALSE;
}
/*****************************************************************************
*
*/
VOID
ProcessSNRM(PIRLAP_CB pIrlapCb,
            IRLAP_SNRM_FORMAT *pSnrmFormat,
            UCHAR *pEndQosUCHAR)
{
    IRDA_MSG    IMsg;
    BOOLEAN     QosInSNRM = &pSnrmFormat->FirstQosByte < pEndQosUCHAR;
    BOOLEAN     AddrsInSNRM = (UCHAR *)pSnrmFormat < pEndQosUCHAR;
    UINT        rc;

    if (AddrsInSNRM)
    {
        if (!MyDevAddr(pIrlapCb, pSnrmFormat->DestAddr))
        {
            IRLAP_LOG_ACTION((pIrlapCb, 
                       TEXT("Ignoring SNRM addressed to:%02X%02X%02X%02X"),
                              EXPAND_ADDR(pSnrmFormat->DestAddr)));
            return;
        }
        CTEMemCopy(pIrlapCb->RemoteDevice.DevAddr,
                  pSnrmFormat->SrcAddr, IRDA_DEV_ADDR_LEN);
    }

    switch (pIrlapCb->State)
    {
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
        // In the middle of discovery... End discovery and reply to SNRM
        IMsg.Prim = IRLAP_DISCOVERY_CONF;
        IMsg.IRDA_MSG_pDevList = NULL;
        IMsg.IRDA_MSG_DscvStatus = IRLAP_REMOTE_CONNECTION_IN_PROGRESS;
        IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
        // fall through and send connect indication
      case DSCV_REPLY:
      case NDM:
        if (AddrsInSNRM)
        {
            pIrlapCb->SNRMConnAddr =
                (int)IRLAP_GET_ADDR(pSnrmFormat->ConnAddr);
        }
        if (QosInSNRM)
        {
            ExtractQosParms(&pIrlapCb->RemoteQos, &pSnrmFormat->FirstQosByte,
                        pEndQosUCHAR);

            if ((rc = NegotiateQosParms(pIrlapCb, &pIrlapCb->RemoteQos)))
            {
                DEBUGMSG(1, (TEXT("IRLAP: SNRM/UA negotiation failed, rc=%\n"), rc));
                ASSERT(0);
                return;
            }
        }

        CTEMemCopy(IMsg.IRDA_MSG_RemoteDevAddr,
               pIrlapCb->RemoteDevice.DevAddr, IRDA_DEV_ADDR_LEN);
        IMsg.IRDA_MSG_pQos = &pIrlapCb->NegotiatedQos;
        IMsg.Prim = IRLAP_CONNECT_IND;
        IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
        pIrlapCb->State = SNRM_RECEIVED;
        break;

      case BACKOFF_WAIT:   // CROSSED SNRM
        // if Remote address greater than mine we'll respond to SNRM
        if (AddrsInSNRM)
        {
            if (AddressGreaterThan(pSnrmFormat->SrcAddr,
                                   pIrlapCb->LocalDevice.DevAddr))
            {
                IrlapTimerStop(TEXT("ProcessSNRM: BACKOFF_WAIT"), 
                               pIrlapCb, &pIrlapCb->BackoffTimer);
            }
        }
        // fall through
      case CONN_MEDIA_SENSE:   // CROSSED SNRM
      case SNRM_SENT:
        // if Remote address greater than mine we'll respond to SNRM
        if (AddrsInSNRM && AddressGreaterThan(pSnrmFormat->SrcAddr,
                                              pIrlapCb->LocalDevice.DevAddr))
        {
            if (pIrlapCb->State != BACKOFF_WAIT)
            {
                IrlapTimerStop(TEXT("ProcessSNRM: SNRM_SENT/CONN_MEDIA_SENSE"), 
                               pIrlapCb, &pIrlapCb->FinalTimer);
            }
            InitializeState(pIrlapCb, SECONDARY);

            if (QosInSNRM)
            {
                ExtractQosParms(&pIrlapCb->RemoteQos,
                                &pSnrmFormat->FirstQosByte, pEndQosUCHAR);
                if ((rc = NegotiateQosParms(pIrlapCb, &pIrlapCb->RemoteQos)))
                {
                    DEBUGMSG(1, (TEXT("Negotiation failed, rc=%\n"), rc));
                    ASSERT(0);
                    pIrlapCb->State = NDM;
                    return;
                }
            }

            if (AddrsInSNRM)
            {
                pIrlapCb->ConnAddr =
                    (int)IRLAP_GET_ADDR(pSnrmFormat->ConnAddr);
            }

            SendUA(pIrlapCb, TRUE);
            
            if (QosInSNRM)
            {
                ApplyQosParms(pIrlapCb);
            }
            
            IMsg.IRDA_MSG_pQos = &pIrlapCb->NegotiatedQos;
            IMsg.Prim = IRLAP_CONNECT_CONF;
            IMsg.IRDA_MSG_ConnStatus = IRLAP_CONNECTION_COMPLETED;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);

            IrlapTimerStart(TEXT("ProcessSNRM: SNRM_SENT/CONN_MEDIA_SENSE/BACKOFF_WAIT"), 
                            pIrlapCb, &pIrlapCb->WDogTimer);
            pIrlapCb->State = S_NRM;
        }
        break;

      case P_RECV:
      case P_DISCONNECT_PEND:
      case P_CLOSE:
        IrlapTimerStop(TEXT("ProcessSNRM: P_RECV/P_DISCONNECT_PEND/P_CLOSE"), 
                       pIrlapCb, &pIrlapCb->FinalTimer);
        pIrlapCb->State = NDM;
        StationConflict(pIrlapCb);
        ReturnRxTxWinMsgs(pIrlapCb);
        if (pIrlapCb->State == P_CLOSE)
        {
            GotoNDMThenDscvOrConn(pIrlapCb);
        }
        break;

      case S_NRM:
      case S_CLOSE:
      case S_DISCONNECT_PEND:
        IrlapTimerStop(TEXT("ProcessSNRM: S_DISCONNECT_PEND"), 
                       pIrlapCb, &pIrlapCb->WDogTimer);
        SendDM(pIrlapCb);
        ApplyDefaultParms(pIrlapCb);
        IMsg.Prim = IRLAP_DISCONNECT_IND;
        if (pIrlapCb->State == S_NRM)
        {
            IMsg.IRDA_MSG_DiscStatus = IRLAP_DECLINE_RESET;
        }
        else
        {
            IMsg.IRDA_MSG_DiscStatus = IRLAP_DISCONNECT_COMPLETED;
        }
        pIrlapCb->State = NDM;
        IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
        break;

      case S_ERROR:
        IrlapTimerStop(TEXT("ProcessSNRM: S_ERROR"), 
                       pIrlapCb, &pIrlapCb->WDogTimer);
        SendFRMR(pIrlapCb, &pIrlapCb->Frmr);
        IrlapTimerStart(TEXT("ProcessSNRM: S_ERROR"), 
                        pIrlapCb, &pIrlapCb->WDogTimer);
        pIrlapCb->State = S_NRM;
        break;

      default:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("SNRM ignored in this state")));
    }

    return;
}
/*****************************************************************************
*
*/
VOID
ProcessUA(PIRLAP_CB pIrlapCb,
          IRLAP_UA_FORMAT *pUAFormat,
          UCHAR *pEndQosUCHAR)
{
    BOOLEAN        QosInUA = &pUAFormat->FirstQosByte < pEndQosUCHAR;
    BOOLEAN        AddrsInUA = (UCHAR *)pUAFormat < pEndQosUCHAR;
    int         Tmp;
    IRDA_MSG    IMsg;
    UINT        rc;

    if (AddrsInUA && !MyDevAddr(pIrlapCb, pUAFormat->DestAddr))
    {
        IRLAP_LOG_ACTION((pIrlapCb,
                          TEXT("Ignoring UA addressed to:%02X%02X%02X%02X"),
                          EXPAND_ADDR(pUAFormat->DestAddr)));
        return;
    }

    switch (pIrlapCb->State)
    {
      case BACKOFF_WAIT:
        IrlapTimerStop(TEXT("ProcessUA: BACKOFF_WAIT"),
                       pIrlapCb, &pIrlapCb->BackoffTimer);
        // fall through
      case SNRM_SENT:
        if (pIrlapCb->State != BACKOFF_WAIT)
        {
            IrlapTimerStop(TEXT("ProcessUA: SNRM_SENT"),
                           pIrlapCb, &pIrlapCb->FinalTimer);
        }

        InitializeState(pIrlapCb, PRIMARY);

        if (QosInUA)
        {
            ExtractQosParms(&pIrlapCb->RemoteQos, &pUAFormat->FirstQosByte,
                            pEndQosUCHAR);

            if ((rc = NegotiateQosParms(pIrlapCb, &pIrlapCb->RemoteQos)))
            {
                DEBUGMSG(1, (TEXT("Negotiation failed, rc=%\n"), rc));
                ASSERT(0);
                pIrlapCb->State = NDM;
                return;
            }
            ApplyQosParms(pIrlapCb);
        }

        IMsg.IRDA_MSG_pQos = &pIrlapCb->NegotiatedQos;

        IMsg.Prim = IRLAP_CONNECT_CONF;
        IMsg.IRDA_MSG_ConnStatus = IRLAP_CONNECTION_COMPLETED;

        // notify LMP of connection
        IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
                                    
        // send RR (turn link), start FinalTimer/2
        SendRR_RNR(pIrlapCb);
        
        Tmp = pIrlapCb->FinalTimer.Timeout;
        pIrlapCb->FinalTimer.Timeout = pIrlapCb->FinalTimer.Timeout/2;
        IrlapTimerStart(TEXT("ProcessUA: SNRM_SENT/BACKOFF_WAIT"),
                        pIrlapCb, &pIrlapCb->FinalTimer);
        pIrlapCb->FinalTimer.Timeout = Tmp;
        
        pIrlapCb->State = P_RECV;
        break;

      case P_RECV: // Unsolicited UA, may want to do something else ???
        IrlapTimerStop(TEXT("ProcessUA: P_RECV"),
                       pIrlapCb, &pIrlapCb->FinalTimer);
        IrlapTimerStart(TEXT("ProcessUA: P_RECV"),
                        pIrlapCb, &pIrlapCb->PollTimer);
        pIrlapCb->State = P_XMIT;
        break;

      case P_DISCONNECT_PEND:
        IrlapTimerStop(TEXT("ProcessUA: P_DISCONNECT_PENDING"),
                       pIrlapCb, &pIrlapCb->FinalTimer);
        SendDISC(pIrlapCb);
        IrlapTimerStart(TEXT("ProcessUA: P_DISCONNECT_PENDING"),
                        pIrlapCb, &pIrlapCb->FinalTimer);
        pIrlapCb->RetryCnt = 0;
        GotoPCloseState(pIrlapCb);
        break;

      case P_CLOSE:
        IrlapTimerStop(TEXT("ProcessUA: P_CLOSE"), 
                       pIrlapCb, &pIrlapCb->FinalTimer);
        ApplyDefaultParms(pIrlapCb);
        if (pIrlapCb->LocalDiscReq == TRUE)
        {
            pIrlapCb->LocalDiscReq = FALSE;
            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = IRLAP_DISCONNECT_COMPLETED;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
        }
        GotoNDMThenDscvOrConn(pIrlapCb);
        break;

      case S_NRM:
      case S_DISCONNECT_PEND:
      case S_ERROR:
      case S_CLOSE:
        IrlapTimerStop(TEXT("ProcessUA: S_NRM/S_DISCONNECT_PENDING/S_ERROR/S_CLOSE"),
                       pIrlapCb, &pIrlapCb->WDogTimer);
        StationConflict(pIrlapCb);
        pIrlapCb->State = NDM;
        break;

      default:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("UA ignored in this state")));
    }

}

UCHAR *
GetPv(UCHAR *pQosUCHAR,
      UINT *pBitField)
{
    int     Pl = (int) *pQosUCHAR++;

    *pBitField = 0;
    
    if (Pl == 1)
    {
        *pBitField = (UINT) *pQosUCHAR;
    }
    else
    {
        *pBitField = ((UINT) *(pQosUCHAR+1))<<8;
        *pBitField |= (UINT) *(pQosUCHAR);
    }
    
    return pQosUCHAR + Pl;
}
/*****************************************************************************
*
*           THIS WILL BREAK IF PARAMETER LENGTH (PL) IS GREATER THAN 2
*/
void
ExtractQosParms(IRDA_QOS_PARMS *pQos,
                UCHAR *pQosUCHAR,
                UCHAR *pEndQosUCHAR)
{
    while (pQosUCHAR + 2 < pEndQosUCHAR)
    {
        switch (*pQosUCHAR)
        {
          case QOS_PI_BAUD:
            pQosUCHAR = GetPv(pQosUCHAR+1, &pQos->bfBaud);
            break;

          case QOS_PI_MAX_TAT:
            pQosUCHAR = GetPv(pQosUCHAR+1, &pQos->bfMaxTurnTime);
            break;

          case QOS_PI_DATA_SZ:
            pQosUCHAR = GetPv(pQosUCHAR+1, &pQos->bfDataSize);
            break;

          case QOS_PI_WIN_SZ:
            pQosUCHAR = GetPv(pQosUCHAR+1, &pQos->bfWindowSize);
            break;

          case QOS_PI_BOFS:
            pQosUCHAR = GetPv(pQosUCHAR+1, &pQos->bfBofs);
            break;

          case QOS_PI_MIN_TAT:
            pQosUCHAR = GetPv(pQosUCHAR+1, &pQos->bfMinTurnTime);
            break;

          case QOS_PI_DISC_THRESH:
            pQosUCHAR = GetPv(pQosUCHAR+1, &pQos->bfDisconnectTime);
            break;

          default:
            pQosUCHAR += (*(pQosUCHAR+1)); 
        }
    }
}
/*****************************************************************************
*
*   @func   UINT | NegotiateQosParms | Take the received Qos build
*                                      negotiated Qos.
*
*   @rdesc  SUCCESS, otherwise one of the folowing:
*   @flag   IRLAP_BAUD_NEG_ERR     | Failed to negotiate baud
*   @flag   IRLAP_DISC_NEG_ERR     | Failed to negotiate disconnect time
*   @flag   IRLAP_MAXTAT_NEG_ERR   | Failed to negotiate max turn time
*   @flag   IRLAP_DATASIZE_NEG_ERR | Failed to negotiate data size
*   @flag   IRLAP_WINSIZE_NEG_ERR  | Failed to negotiate window size
*   @flag   IRLAP_BOFS_NEG_ERR     | Failed to negotiate number of BOFS
*   @flag   IRLAP_WINSIZE_NEG_ERR  | Failed to window size
*   @flag   IRLAP_LINECAP_ERR      | Failed to determine valid line capacity
*
*   @parm   IRDA_QOS_PARMS * | pRemoteQos | Pointer to QOS parm struct
*/
UINT
NegotiateQosParms(PIRLAP_CB         pIrlapCb,
                  IRDA_QOS_PARMS    *pRemoteQos)
{
    UINT BitSet = 0;
    BOOLEAN ParmSet = FALSE;
    UINT BOFSDivisor = 1;
    UINT MaxLineCap = 0;
    UINT LineCapacity;
    UINT DataSizeBit = 0;
    UINT WinSizeBit = 0;
    UINT WSBit;
    int  RemoteDataSize = 0;
    int  RemoteWinSize = 0;

    // Baud rate is Type 0 parm
    pIrlapCb->Baud = IrlapGetQosParmVal(vBaudTable,
                        pIrlapCb->LocalQos.bfBaud & pRemoteQos->bfBaud,
                        &BitSet);
    BOFSDivisor = IrlapGetQosParmVal(vBOFSDivTable,
                        pIrlapCb->LocalQos.bfBaud & pRemoteQos->bfBaud,
                        &BitSet);
    pIrlapCb->NegotiatedQos.bfBaud = BitSet;

    if (-1 == pIrlapCb->Baud)
    {
        return IRLAP_BAUD_NEG_ERR;
    }
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Negotiated Baud:%d"), pIrlapCb->Baud));

    // Disconnect/Threshold time is Type 0 parm
    pIrlapCb->DisconnectTime = IrlapGetQosParmVal(vDiscTable,
             pIrlapCb->LocalQos.bfDisconnectTime & pRemoteQos->bfDisconnectTime, 
             &BitSet);
    pIrlapCb->ThresholdTime = IrlapGetQosParmVal(vThreshTable,
             pIrlapCb->LocalQos.bfDisconnectTime & pRemoteQos->bfDisconnectTime,
             &BitSet);
    pIrlapCb->NegotiatedQos.bfDisconnectTime = BitSet;

    if (-1 == pIrlapCb->DisconnectTime)
    {
        return IRLAP_DISC_NEG_ERR;
    }
    IRLAP_LOG_ACTION((pIrlapCb,
                      TEXT("Negotiated Disconnect/Threshold time:%d/%d"),
                      pIrlapCb->DisconnectTime, pIrlapCb->ThresholdTime));

    pIrlapCb->RemoteMaxTAT = IrlapGetQosParmVal(vMaxTATTable,
                                          pRemoteQos->bfMaxTurnTime,
                                          &BitSet);
    pIrlapCb->NegotiatedQos.bfMaxTurnTime = BitSet;
    if (-1 == pIrlapCb->RemoteMaxTAT)
    {
        return IRLAP_MAXTAT_NEG_ERR;
    }
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Remote max turnaround time:%d"),
                      pIrlapCb->RemoteMaxTAT));

    pIrlapCb->RemoteMinTAT = IrlapGetQosParmVal(vMinTATTable,
                                          pRemoteQos->bfMinTurnTime,
                                          &BitSet);
    pIrlapCb->NegotiatedQos.bfMinTurnTime = BitSet;
    if (-1 == pIrlapCb->RemoteMinTAT)
    {
        return IRLAP_MINTAT_NEG_ERR;
    }
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Remote min turnaround time:%d"),
                      pIrlapCb->RemoteMinTAT));

    pIrlapCb->RemoteDataSize = IrlapGetQosParmVal(
                                    vDataSizeTable,
                                    (UCHAR) (pIrlapCb->LocalQos.bfDataSize & pRemoteQos->bfDataSize),
                                    &BitSet);
    DataSizeBit = BitSet;
    pIrlapCb->NegotiatedQos.bfDataSize = BitSet;
    if (-1 == pIrlapCb->RemoteDataSize)
    {
        return IRLAP_DATASIZE_NEG_ERR;
    }
    IRLAP_LOG_ACTION((pIrlapCb,
                      TEXT("Remote data size:%d"), pIrlapCb->RemoteDataSize));

    pIrlapCb->RemoteWinSize = IrlapGetQosParmVal(vWinSizeTable,
                                          pRemoteQos->bfWindowSize, &BitSet);
    WinSizeBit = BitSet;
    pIrlapCb->NegotiatedQos.bfWindowSize = BitSet;
    if (-1 == pIrlapCb->RemoteWinSize)
    {
        return IRLAP_WINSIZE_NEG_ERR;
    }
    IRLAP_LOG_ACTION((pIrlapCb,
                      TEXT("Remote window size:%d"),
                      pIrlapCb->RemoteWinSize));

    pIrlapCb->RemoteNumBOFS=(IrlapGetQosParmVal(vBOFSTable,
                                          pRemoteQos->bfBofs, &BitSet)
                                    / BOFSDivisor)+1;
    pIrlapCb->NegotiatedQos.bfBofs = BitSet;
    if (-1 == pIrlapCb->RemoteNumBOFS)
    {
        return IRLAP_BOFS_NEG_ERR;
    }
    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Remote number of BOFS:%d"),
                      pIrlapCb->RemoteNumBOFS));

    // The maximum line capacity is in UCHARs and comes from a table in spec.
    // (can't calc because table isn't linear). It is determined by the
    // maximum line capacity and baud rate.
    //
    // Later note: Errata corrected table so values could be calculated.
    // Could get rid of tables
    switch (pIrlapCb->Baud)
    {
      case 9600:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_9600,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 19200:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_19200,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 38400:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_38400,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 57600:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_57600,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 115200:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_115200,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 576000:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_576000,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 1152000:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_1152000,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 4000000:
        MaxLineCap = IrlapGetQosParmVal(MAXCAP_4000000,
                                   pRemoteQos->bfMaxTurnTime, &BitSet);
        break;

      case 16000000:
         MaxLineCap = IrlapGetQosParmVal(MAXCAP_16000000,
                                    pRemoteQos->bfMaxTurnTime, &BitSet);
         break;
         
      default:
         ASSERT(0);
    }

    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Maximum line capacity:%d"), MaxLineCap));
    LineCapacity = LINE_CAPACITY(pIrlapCb);
    IRLAP_LOG_ACTION((pIrlapCb,
                      TEXT("Requested line capacity:%d"), LineCapacity));

    if (LineCapacity > MaxLineCap)
    {
        ParmSet = FALSE;
        // Adjust data and window size to fit within the line capacity.
        // Get largest possible datasize
        for (; DataSizeBit != 0 && !ParmSet; DataSizeBit >>= 1)
        {
            pIrlapCb->RemoteDataSize = IrlapGetQosParmVal(vDataSizeTable,
                                                          DataSizeBit, NULL);
            // Start with smallest window
            for (WSBit=1; WSBit <= WinSizeBit; WSBit <<=1)
            {
                pIrlapCb->RemoteWinSize = IrlapGetQosParmVal(vWinSizeTable,
                                                             WSBit, NULL);
                LineCapacity = LINE_CAPACITY(pIrlapCb);

                IRLAP_LOG_ACTION((pIrlapCb, 
                  TEXT("adjusted data size=%d, window size= %d, line cap=%d"),
                        pIrlapCb->RemoteDataSize, pIrlapCb->RemoteWinSize, 
                        LineCapacity));

                if (LineCapacity > MaxLineCap)
                {
                    // Get a smaller data size (only if ParmSet is false)
                    break; 
                }
                ParmSet = TRUE;
                // Save the last good one,then loop and try a larger window
                RemoteDataSize = pIrlapCb->RemoteDataSize;
                RemoteWinSize  = pIrlapCb->RemoteWinSize;
                pIrlapCb->NegotiatedQos.bfWindowSize = WSBit;
                pIrlapCb->NegotiatedQos.bfDataSize = DataSizeBit;
            }
        }
        if (!ParmSet)
        {
            return IRLAP_LINECAP_ERR;
        }

        pIrlapCb->RemoteDataSize = RemoteDataSize;
        pIrlapCb->RemoteWinSize = RemoteWinSize;

        IRLAP_LOG_ACTION((pIrlapCb,
                   TEXT("final data size=%d, window size= %d, line cap=%d"),
                          pIrlapCb->RemoteDataSize, pIrlapCb->RemoteWinSize, 
                          LINE_CAPACITY(pIrlapCb)));
    }

    return SUCCESS;
}
/*****************************************************************************
*
*
*/
VOID
ApplyQosParms(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG    IMsg;
    
    pIrlapCb->PollTimer.Timeout     = pIrlapCb->RemoteMaxTAT;
    pIrlapCb->FinalTimer.Timeout    = pIrlapCb->LocalMaxTAT;

    if (pIrlapCb->Baud <= 115200)
    {
        pIrlapCb->FinalTimer.Timeout += 150; // fudge factor for SIR
    }

    // convert disconnect/threshold time to ms and divide by final timer
    // to get number of retries
    pIrlapCb->N1 = pIrlapCb->ThresholdTime * 1000 / pIrlapCb->FinalTimer.Timeout;
    pIrlapCb->N2 = pIrlapCb->DisconnectTime * 1000 / pIrlapCb->FinalTimer.Timeout;


    IMsg.Prim              = MAC_CONTROL_REQ;
    IMsg.IRDA_MSG_Op       = MAC_RECONFIG_LINK;
    IMsg.IRDA_MSG_Baud     = pIrlapCb->Baud;
    IMsg.IRDA_MSG_NumBOFs  = pIrlapCb->RemoteNumBOFS;  // Number of BOFS
                                                          // to add to tx
    IMsg.IRDA_MSG_DataSize = pIrlapCb->RemoteDataSize; // Max rx size packet
                                                      // causes major heap
                                                      // problems later
    IMsg.IRDA_MSG_MinTat   = pIrlapCb->RemoteMinTAT;
    IRLAP_LOG_ACTION((pIrlapCb,
        TEXT("Reconfig link for Baud:%d, Remote BOFS:%d"),
                      pIrlapCb->Baud, 
                      pIrlapCb->RemoteNumBOFS));

    IRLAP_LOG_ACTION((pIrlapCb, TEXT("Retry counts N1=%d, N2=%d"),
                      pIrlapCb->N1, pIrlapCb->N2));
    
    IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);
}
/*****************************************************************************
*
*   @func   UINT | IrlapGetQosParmVal |    
*                   retrieves the parameters value from table
*
*   @rdesc  value contained in parmeter value table, 0 if not found
*           (0 is a valid parameter in some tables though)
*
*   @parm   UINT [] | PVTable | table containing parm values
*           USHORT  | BitField | contains bit indicating which parm to select
*
*   @comm
*/
UINT
IrlapGetQosParmVal(UINT PVTable[], UINT BitField, UINT *pBitSet)
{
    int     i;
    UINT    Mask;

    for (i = PV_TABLE_MAX_BIT, Mask = (1<<PV_TABLE_MAX_BIT);
         Mask > 0; i--, Mask = Mask >> 1)
    {
        if (Mask & BitField)
        {
            if (pBitSet != NULL)
            {
                *pBitSet = Mask;
            }
            return (PVTable[i]);
        }
    }
    return (UINT) -1;
}
/*****************************************************************************
*
*/
VOID
ProcessTEST(PIRLAP_CB       pIrlapCb,
            PIRDA_MSG       pMsg,
            IRLAP_UA_FORMAT *pTestFormat,
            int             CRBit,
            int             PFBit)
{
    if (IRLAP_CMD == CRBit && IRLAP_PFBIT_SET == PFBit)
    {
        // bounce it back
        
        *(pMsg->IRDA_MSG_pRead) ^= 1; // swap cr bit    
    
        // copy the header to the header portion for an outbound message
        
        pMsg->IRDA_MSG_pHdrRead = pMsg->IRDA_MSG_Header;
        *pMsg->IRDA_MSG_pHdrRead = *pMsg->IRDA_MSG_pRead;
        *(pMsg->IRDA_MSG_pHdrRead+1) = *(pMsg->IRDA_MSG_pRead+1);
        pMsg->IRDA_MSG_pHdrWrite = pMsg->IRDA_MSG_Header + 2;
        pMsg->IRDA_MSG_pRead+=2;

        pMsg->Prim = MAC_DATA_REQ;

        IrmacDown(pIrlapCb->pIrdaLinkCb, pMsg);
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring")));
    }
}
/*****************************************************************************
*
*/
VOID
ProcessUI(PIRLAP_CB pIrlapCb,
          PIRDA_MSG pMsg,
          int       CRBit,
          int       PFBit)
{
    BOOLEAN LinkTurned = TRUE;

    pMsg->IRDA_MSG_pRead += 2; // chop the IRLAP header

    switch (pIrlapCb->State)
    {
      case NDM:
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
      case DSCV_REPLY:
      case CONN_MEDIA_SENSE:
      case SNRM_SENT:
      case BACKOFF_WAIT:
      case SNRM_RECEIVED:
        pMsg->Prim = IRLAP_UDATA_IND;
        IrlmpUp(pIrlapCb->pIrdaLinkCb, pMsg);
        return;
        
      case P_XMIT:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        return;
    }

    if (PRIMARY == pIrlapCb->StationType)
    {
        // stop timers if PF bit set or invalid CRBit (matches mine)
        if (IRLAP_PFBIT_SET == PFBit || pIrlapCb->CRBit == CRBit)
        {
            IrlapTimerStop(TEXT("ProcessUI: PRIMARY"),
                           pIrlapCb, &pIrlapCb->FinalTimer);
        }
    }
    else
    {
        IrlapTimerStop(TEXT("ProcessUI: SECONDARY"),
                       pIrlapCb, &pIrlapCb->WDogTimer);
    }

    if (pIrlapCb->CRBit == CRBit)
    {
        StationConflict(pIrlapCb);
        pIrlapCb->State = NDM;
        return;
    }

    // Send the Unnumber information to LMP
    pMsg->Prim = IRLAP_UDATA_IND;
    IrlmpUp(pIrlapCb->pIrdaLinkCb, pMsg);

    if (IRLAP_PFBIT_SET == PFBit)
    {
        switch (pIrlapCb->State)
        {
          case P_RECV:
            XmitTxMsgList(pIrlapCb, FALSE, &LinkTurned);
            break;

          case P_DISCONNECT_PEND:
            SendDISC(pIrlapCb);
            pIrlapCb->RetryCnt = 0;
            GotoPCloseState(pIrlapCb);
            break;

          case P_CLOSE:
            ResendDISC(pIrlapCb);
            break;

          case S_NRM:
            XmitTxMsgList(pIrlapCb, TRUE, NULL);
            break;

          case S_DISCONNECT_PEND:
            SendRD(pIrlapCb);
            pIrlapCb->State = S_CLOSE;
            break;

          case S_ERROR:
            SendFRMR(pIrlapCb, &pIrlapCb->Frmr);
            pIrlapCb->State = S_NRM;
            break;

          case S_CLOSE:
            SendRD(pIrlapCb);
        }
    }

    if (PRIMARY == pIrlapCb->StationType)
    {
        if (IRLAP_PFBIT_SET == PFBit && pIrlapCb->State != NDM)
        {
            if (LinkTurned)
            {
                IrlapTimerStart(TEXT("ProcessUI: PRIMARY1"),
                                pIrlapCb, &pIrlapCb->FinalTimer);
            }
            else
            {
                IrlapTimerStart(TEXT("ProcessUI: PRIMARY2"),
                                pIrlapCb, &pIrlapCb->PollTimer);
                pIrlapCb->State = P_XMIT;
            }
        }
    }
    else
    {
        IrlapTimerStart(TEXT("ProcessUI: SECONDARY"),
                        pIrlapCb, &pIrlapCb->WDogTimer);
    }

    return;
}
/*****************************************************************************
*
*/
VOID
ProcessDM(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG    IMsg;
    BOOLEAN        LinkTurned;

    switch (pIrlapCb->State)
    {
      case NDM:
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
      case DSCV_REPLY:
      case CONN_MEDIA_SENSE:
      case BACKOFF_WAIT:
      case SNRM_RECEIVED:
      case P_XMIT:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        return;
    }

    if (PRIMARY != pIrlapCb->StationType)
    {
        IrlapTimerStop(TEXT("ProcessDM: SECONDARY"),
                       pIrlapCb, &pIrlapCb->WDogTimer);
        StationConflict(pIrlapCb);
        pIrlapCb->State = NDM;
        return;
    }

    IrlapTimerStop(TEXT("ProcessDM"),
                   pIrlapCb, &pIrlapCb->FinalTimer);

    switch (pIrlapCb->State)
    {
      case P_RECV: // I'm not sure why I am doing this ???
        XmitTxMsgList(pIrlapCb, FALSE, &LinkTurned);
        if (LinkTurned)
        {
            IrlapTimerStart(TEXT("ProcessDM: P_RECV1"),
                            pIrlapCb, &pIrlapCb->FinalTimer);
        }
        else
        {
            IrlapTimerStart(TEXT("ProcessDM: P_RECV2"),
                            pIrlapCb, &pIrlapCb->PollTimer);
            pIrlapCb->State = P_XMIT;
        }
        break;

      case P_DISCONNECT_PEND:
        pIrlapCb->RetryCnt = 0;
        SendDISC(pIrlapCb);
        IrlapTimerStart(TEXT("ProcessDM: P_DISCONNECT_PENDING"),
                        pIrlapCb, &pIrlapCb->FinalTimer);
        GotoPCloseState(pIrlapCb);
        break;

      case SNRM_SENT:
        ApplyDefaultParms(pIrlapCb);

        pIrlapCb->State = NDM;

        IMsg.Prim = IRLAP_DISCONNECT_IND;
        IMsg.IRDA_MSG_DiscStatus = IRLAP_REMOTE_INITIATED;

        pIrlapCb->LocalDiscReq = FALSE;
        IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
        break;                

      case P_CLOSE:

        pIrlapCb->State = NDM;

        ApplyDefaultParms(pIrlapCb);

        if (pIrlapCb->LocalDiscReq || pIrlapCb->State == SNRM_SENT)
        {
            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = IRLAP_DISCONNECT_COMPLETED;

            pIrlapCb->LocalDiscReq = FALSE;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
        }

        GotoNDMThenDscvOrConn(pIrlapCb);
        break;
    }
}
/*****************************************************************************
*
*
*/
VOID
ProcessDISC(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG    IMsg;
    
    if (IgnoreState(pIrlapCb))
    {
        return;
    }

    if (SECONDARY != pIrlapCb->StationType)
    {
        IrlapTimerStop(TEXT("ProcessDISC"),
                       pIrlapCb, &pIrlapCb->FinalTimer);
        StationConflict(pIrlapCb);
        pIrlapCb->State = NDM;
        return;
    }

    IrlapTimerStop(TEXT("ProcessDISC"),
                   pIrlapCb, &pIrlapCb->WDogTimer);

    // Acknowledge primary's disconnect request
    SendUA(pIrlapCb, FALSE /* No Qos */);

    // notify LMP of disconnect
    IMsg.Prim = IRLAP_DISCONNECT_IND;
    if (pIrlapCb->LocalDiscReq)
    {
        IMsg.IRDA_MSG_DiscStatus = IRLAP_DISCONNECT_COMPLETED;
        pIrlapCb->LocalDiscReq = FALSE;
    }
    else
    {
        IMsg.IRDA_MSG_DiscStatus = IRLAP_REMOTE_INITIATED;
    }

    pIrlapCb->State = NDM;

    IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);

    ReturnRxTxWinMsgs(pIrlapCb);
    ApplyDefaultParms(pIrlapCb);

    return;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  SUCCESS, otherwise one of the following errors:
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
VOID
ProcessRD(PIRLAP_CB pIrlapCb)
{
    if (IgnoreState(pIrlapCb))
    {
        return;
    }

    if (PRIMARY != pIrlapCb->StationType)
    {
        IrlapTimerStop(TEXT("ProcessRD"), pIrlapCb, &pIrlapCb->WDogTimer);
        StationConflict(pIrlapCb);
        pIrlapCb->State = NDM;
        return;
    }

    IrlapTimerStop(TEXT("ProcessRD"), pIrlapCb, &pIrlapCb->FinalTimer);

    if (pIrlapCb->State == P_CLOSE)
    {
        ResendDISC(pIrlapCb);
    }
    else
    {
        ReturnRxTxWinMsgs(pIrlapCb);
        pIrlapCb->RetryCnt = 0;
        SendDISC(pIrlapCb);
        GotoPCloseState(pIrlapCb);
    }
    if (pIrlapCb->State != NDM)
    {
        IrlapTimerStart(TEXT("ProcessRD"), pIrlapCb, &pIrlapCb->FinalTimer);
    }
}
/*****************************************************************************
*
*/
VOID
ProcessFRMR(PIRLAP_CB pIrlapCb)
{
    if (IgnoreState(pIrlapCb))
    {
        return;
    }

    if (PRIMARY != pIrlapCb->StationType)
    {
        IrlapTimerStop(TEXT("ProcessFRMR"), pIrlapCb, &pIrlapCb->WDogTimer);
        StationConflict(pIrlapCb);
        pIrlapCb->State = NDM;
        return;
    }

    IrlapTimerStop(TEXT("ProcessFRMR"), pIrlapCb, &pIrlapCb->FinalTimer);

    switch (pIrlapCb->State)
    {
      case P_RECV:
        ReturnRxTxWinMsgs(pIrlapCb);
        // fall through

      case P_DISCONNECT_PEND:
        pIrlapCb->RetryCnt = 0;
        SendDISC(pIrlapCb);
        GotoPCloseState(pIrlapCb);
        break;

      case P_CLOSE:
        ResendDISC(pIrlapCb);
        break;
    }

    if (pIrlapCb->State != NDM)
    {
        IrlapTimerStart(TEXT("ProcessFRMR"), pIrlapCb, &pIrlapCb->FinalTimer);
    }
}
/*****************************************************************************
*
*/
VOID
ProcessRNRM(PIRLAP_CB pIrlapCb)
{
    if (IgnoreState(pIrlapCb))
    {
        return;
    }

    if (PRIMARY != pIrlapCb->StationType)
    {
        IrlapTimerStop(TEXT("ProcessRNRM"), pIrlapCb, &pIrlapCb->WDogTimer);
        StationConflict(pIrlapCb);
        pIrlapCb->State = NDM;
        return;
    }

    IrlapTimerStop(TEXT("ProcessRNRM"), pIrlapCb, &pIrlapCb->FinalTimer);

    switch (pIrlapCb->State)
    {
      case P_RECV:
      case P_DISCONNECT_PEND:
        pIrlapCb->RetryCnt = 0;
        SendDISC(pIrlapCb);
        GotoPCloseState(pIrlapCb);
        break;

      case P_CLOSE:
        ResendDISC(pIrlapCb);
        break;
    }

    if (pIrlapCb->State != NDM)
    {
        IrlapTimerStart(TEXT("ProcessRNRM"), pIrlapCb, &pIrlapCb->FinalTimer);
    }

    return;
}
/*****************************************************************************
*
*/
VOID
ProcessREJ_SREJ(PIRLAP_CB   pIrlapCb,
                int         FrameType,
                PIRDA_MSG   pMsg,
                int         CRBit,
                int         PFBit,
                UINT        Nr)
{
    if (IgnoreState(pIrlapCb))
    {
        return;
    }

    if (PRIMARY == pIrlapCb->StationType)
    {
        // stop timers if PF bit set or invalid CRBit (matches mine)
        if (IRLAP_PFBIT_SET == PFBit || pIrlapCb->CRBit == CRBit)
        {
            IrlapTimerStop(TEXT("ProcessREJ_SREJ"), pIrlapCb, &pIrlapCb->FinalTimer);
        }
    }
    else
    {
        IrlapTimerStop(TEXT("ProcessREJ_SREJ"), pIrlapCb, &pIrlapCb->WDogTimer);
    }

    if (pIrlapCb->CRBit == CRBit)
    {
        StationConflict(pIrlapCb);
        pIrlapCb->State = NDM;
        return;
    }

    switch (pIrlapCb->State)
    {
      case P_RECV:
      case S_NRM:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            if (InvalidNr(pIrlapCb,Nr) || Nr == pIrlapCb->TxWin.End)
            {
                ProcessInvalidNr(pIrlapCb, PFBit);
            }
            else
            {
                ConfirmAckedTxMsgs(pIrlapCb, Nr);
                if (FrameType == IRLAP_REJ)
                {
                    ResendRejects(pIrlapCb, Nr); // link turned here
                }
                else // selective reject
                {
                    IRLAP_LOG_ACTION((pIrlapCb, TEXT("RETRANSMISSION:")));
                    SendIFrame(pIrlapCb,
                               pIrlapCb->TxWin.pMsg[Nr],
                               Nr, IRLAP_PFBIT_SET);
                }
            }
        }
        break;

      case P_DISCONNECT_PEND:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            pIrlapCb->RetryCnt = 0;
            SendDISC(pIrlapCb);
            GotoPCloseState(pIrlapCb);
        }
        break;

      case P_CLOSE:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            ResendDISC(pIrlapCb);
        }
        break;

      case S_DISCONNECT_PEND:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            SendRD(pIrlapCb);
            pIrlapCb->State = S_CLOSE;
        }
        break;

      case S_ERROR:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            SendFRMR(pIrlapCb, &pIrlapCb->Frmr);
            pIrlapCb->State = S_NRM;
        }
        break;

      case S_CLOSE:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            SendRD(pIrlapCb);
        }
        break;

    }
    if (PRIMARY == pIrlapCb->StationType)
    {
        if (IRLAP_PFBIT_SET == PFBit && pIrlapCb->State != NDM)
        {
            IrlapTimerStart(TEXT("ProcessREJ_SREJ"), pIrlapCb, &pIrlapCb->FinalTimer);
        }
    }
    else
    {
        IrlapTimerStart(TEXT("ProcessREJ_SREJ"), pIrlapCb, &pIrlapCb->WDogTimer);
    }
}
/*****************************************************************************
*
*/
VOID
ProcessRR_RNR(PIRLAP_CB pIrlapCb,
              int       FrameType,
              PIRDA_MSG pMsg,
              int       CRBit,
              int       PFBit,
              UINT      Nr)
{
    BOOLEAN LinkTurned = TRUE;

    if (IgnoreState(pIrlapCb))
    {
        return;
    }

    if (pIrlapCb->FastPollCount ==0)
    {
        pIrlapCb->PollTimer.Timeout = pIrlapCb->RemoteMaxTAT;
    }
    else
    {
        pIrlapCb->FastPollCount--;
    }

    if (PRIMARY == pIrlapCb->StationType)
    {
        // stop timers if PF bit set or invalid CRBit (matches mine)
        if (IRLAP_PFBIT_SET == PFBit || pIrlapCb->CRBit == CRBit)
        {
            IrlapTimerStop(TEXT("ProcessRR_RNR"), pIrlapCb, &pIrlapCb->FinalTimer);
        }
    }
    else // SECONDARY, restart WDog
    {
        IrlapTimerStop(TEXT("ProcessRR_RNR"), pIrlapCb, &pIrlapCb->WDogTimer);
        if (pIrlapCb->CRBit != CRBit)
        {
            IrlapTimerStart(TEXT("ProcessRR_RNR: SECONDARY"), pIrlapCb, &pIrlapCb->WDogTimer);
        }
    }

    if (pIrlapCb->CRBit == CRBit)
    {
        StationConflict(pIrlapCb);
        pIrlapCb->State = NDM;
        return;
    }

    if (FrameType == IRLAP_RR)
    {
        pIrlapCb->RemoteBusy = FALSE;
    }
    else // RNR
    {
        pIrlapCb->RemoteBusy = TRUE;
    }

    switch (pIrlapCb->State)
    {
      case P_RECV:
      case S_NRM:
        if (PFBit == IRLAP_PFBIT_SET)
        {
            if (InvalidNr(pIrlapCb, Nr))
            {
                ProcessInvalidNr(pIrlapCb, PFBit);
            }
            else
            {
                ConfirmAckedTxMsgs(pIrlapCb,Nr);

                if (Nr != pIrlapCb->Vs) // Implicit reject
                {
                    if (PRIMARY == pIrlapCb->StationType &&
                        IRLAP_RNR == FrameType)
                    {
                        LinkTurned = FALSE;
                    }
                    else
                    {
                        ResendRejects(pIrlapCb, Nr); // always turns link
                    }
                }
                else
                {
                    if (pIrlapCb->Vr != pIrlapCb->RxWin.End)
                    {
                        MissingRxFrames(pIrlapCb); // Send SREJ or REJ
                    }
                    else
                    {
                        if (PRIMARY == pIrlapCb->StationType)
                        {
                            LinkTurned = FALSE;
                            if (IRLAP_RR == FrameType)
                            {
                                XmitTxMsgList(pIrlapCb, FALSE, &LinkTurned);
                            }
                        }
                        else
                        {
                            // Always turn link if secondary
                            // with data or an RR if remote is busy
                            if (IRLAP_RR == FrameType)
                            {
                                XmitTxMsgList(pIrlapCb, TRUE, NULL);
                            }
                            else
                            {
                                SendRR_RNR(pIrlapCb);
                            }
                        }
                    }
                }
            }
            // If the link was turned, restart Final timer,
            // else start the Poll timer and enter the transmit state
            if (PRIMARY == pIrlapCb->StationType)
            {
                if (LinkTurned)
                {
                    IrlapTimerStart(TEXT("ProcessRR_RNR: PRIMARY1"), 
                                    pIrlapCb, &pIrlapCb->FinalTimer);
                }
                else
                {
                    IrlapTimerStart(TEXT("ProcessRR_RNR: PRIMARY2"), 
                                    pIrlapCb, &pIrlapCb->PollTimer);
                    pIrlapCb->State = P_XMIT;
                }
            }
        }
        break;

      case P_DISCONNECT_PEND:
        SendDISC(pIrlapCb);
        pIrlapCb->RetryCnt = 0;
        IrlapTimerStart(TEXT("ProcessRR_RNR: P_DISCONNECT_PENDING"), 
                        pIrlapCb, &pIrlapCb->FinalTimer);
        GotoPCloseState(pIrlapCb);
        break;

      case P_CLOSE:
        ResendDISC(pIrlapCb);
        if (pIrlapCb->State != NDM)
        {
            IrlapTimerStart(TEXT("ProcessRR_RNR: P_CLOSE"), 
                            pIrlapCb, &pIrlapCb->FinalTimer);
        }
        break;

      case S_DISCONNECT_PEND:
      case S_CLOSE:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            SendRD(pIrlapCb);
            if (pIrlapCb->State != S_CLOSE)
                pIrlapCb->State = S_CLOSE;
        }
        break;

      case S_ERROR:
        if (IRLAP_PFBIT_SET == PFBit)
        {
            SendFRMR(pIrlapCb, &pIrlapCb->Frmr);
            pIrlapCb->State = S_NRM;
        }
        break;

      default:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));

    }
}
/*****************************************************************************
*
*/
VOID
ProcessInvalidNr(PIRLAP_CB pIrlapCb,
                 int PFBit)
{
    DEBUGMSG(DBG_ERROR, (TEXT("IRLAP: ERROR, Invalid Nr\r\n")));
    
    ReturnRxTxWinMsgs(pIrlapCb);

    if (PRIMARY == pIrlapCb->StationType)
    {
        if (PFBit == IRLAP_PFBIT_SET)
        {
            SendDISC(pIrlapCb);
            pIrlapCb->RetryCnt = 0;
            // F-timer will be started by caller
            GotoPCloseState(pIrlapCb);
        }
        else
        {
            pIrlapCb->State = P_DISCONNECT_PEND;
        }
    }
    else // SECONDARY
    {
        if (PFBit == IRLAP_PFBIT_SET)
        {
            pIrlapCb->Frmr.Vs = pIrlapCb->Vs;
            pIrlapCb->Frmr.Vr = pIrlapCb->Vr;
            pIrlapCb->Frmr.W = 0;
            pIrlapCb->Frmr.X = 0;
            pIrlapCb->Frmr.Y = 0;
            pIrlapCb->Frmr.Z = 1; // bad NR
            SendFRMR(pIrlapCb, &pIrlapCb->Frmr);
        }
    }
}
/*****************************************************************************
*
*/
VOID
ProcessIFrame(PIRLAP_CB pIrlapCb,
              PIRDA_MSG pMsg,
              int       CRBit,
              int       PFBit,
              UINT      Ns,
              UINT      Nr,
              BOOLEAN      *pFreeMsg)
{
#if DBG
    UCHAR    *p1, *p2;
#endif

    pMsg->IRDA_MSG_pRead += 2; // chop the IRLAP header

    switch (pIrlapCb->State)
    {
      case S_NRM:
      case P_RECV:
        // Stop Timers: if PFSet stop Final (I frame from secondary)
        // Always stop WDog (I from primary)
        if (PRIMARY == pIrlapCb->StationType)
        {
            if (PFBit == IRLAP_PFBIT_SET)
            {
                IrlapTimerStop(TEXT("ProcessIFrame"), 
                               pIrlapCb, &pIrlapCb->FinalTimer);
            }
        }
        else
        {
            IrlapTimerStop(TEXT("ProcessIFrame"), 
                           pIrlapCb, &pIrlapCb->WDogTimer);
        }

        if (pIrlapCb->CRBit == CRBit)
        {
            StationConflict(pIrlapCb);
            pIrlapCb->State = NDM;
            return;
        }

        if (InvalidNs(pIrlapCb, Ns))
        {
            DEBUGMSG(DBG_ERROR, (TEXT("IRLAP: ignoring invalid NS frame\r\n")));
        }
        else if (InvalidNr(pIrlapCb, Nr))
        {
            DEBUGMSG(DBG_ERROR, (TEXT("IRLAP: invalid NR frame\r\n")));
#if DBG
            p1 = pMsg->IRDA_MSG_pRead - 2; // Get header back
            p2 = pMsg->IRDA_MSG_pWrite + 2; // and FCS
            
            while (p1 < p2)
            DEBUGMSG(DBG_ERROR, (TEXT("%02X "), *p1++));
            DEBUGMSG(DBG_ERROR, (TEXT("\n")));
#endif

#ifdef TEMPERAMENTAL_SERIAL_DRIVER
            if (pIrlapCb->RxWin.FCS[Ns] == pMsg->IRDA_MSG_FCS)
                TossedDups++;
            else
                ProcessInvalidNsOrNr(pIrlapCb, PFBit);
#else
            ProcessInvalidNsOrNr(pIrlapCb, PFBit);
#endif            
        }
        else
        {
            ConfirmAckedTxMsgs(pIrlapCb, Nr);

            if (PFBit == IRLAP_PFBIT_SET)
            {
                InsertRxWinAndForward(pIrlapCb, pMsg, Ns, pFreeMsg);

                if (Nr != pIrlapCb->Vs)
                {
                    ResendRejects(pIrlapCb, Nr); // always turns link
                }
                else // Nr == Vs, Good Nr
                {            
                    // Link will always be turned here
                    if (pIrlapCb->Vr != pIrlapCb->RxWin.End)
                    {
                        MissingRxFrames(pIrlapCb);
                    }
                    else
                    {
                        XmitTxMsgList(pIrlapCb, TRUE, NULL);
                    }
                }
            }
            else // PF Bit not set
            {
                InsertRxWinAndForward(pIrlapCb, pMsg, Ns, pFreeMsg);
            }
        }
        // Start Timers: If PFBit set, link was turned so start final
        //               WDog is always stopped, so restart
        if (PRIMARY == pIrlapCb->StationType)
        {
            if (PFBit == IRLAP_PFBIT_SET)
            {
                IrlapTimerStart(TEXT("ProcessIFrame"),
                                pIrlapCb, &pIrlapCb->FinalTimer);
            }
        }
        else // command from primary
        {
            IrlapTimerStart(TEXT("ProcessIFrame"),
                            pIrlapCb, &pIrlapCb->WDogTimer);
        }
        break;

      default:
        IFrameOtherStates(pIrlapCb, CRBit, PFBit);
    }
}
/*****************************************************************************
*
*/
BOOLEAN
InvalidNs(PIRLAP_CB pIrlapCb,
              UINT      Ns)
{
    // Valididate ns
    if (!InWindow(pIrlapCb->Vr,
       (pIrlapCb->RxWin.Start + pIrlapCb->LocalWinSize-1) % IRLAP_MOD, Ns)
        || !InWindow(pIrlapCb->RxWin.Start,
       (pIrlapCb->RxWin.Start + pIrlapCb->LocalWinSize-1) % IRLAP_MOD, Ns))
    {
        DEBUGMSG(DBG_ERROR, 
           (TEXT("IRLAP: ERROR, Invalid Ns=%d! Vr=%d, RxStrt=%d Win=%d\r\n"),
                Ns, pIrlapCb->Vr, pIrlapCb->RxWin.Start,
            pIrlapCb->LocalWinSize));
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("** INVALID Ns **")));
        return TRUE;
    }
    return FALSE;
}
/*****************************************************************************
*
*/
BOOLEAN
InvalidNr(PIRLAP_CB pIrlapCb,
          UINT Nr)
{
    if (!InWindow(pIrlapCb->TxWin.Start, pIrlapCb->Vs, Nr))
    {
        DEBUGMSG(DBG_ERROR, 
                 (TEXT("IRLAP: ERROR, Invalid Nr=%d! Vs=%d, TxStrt=%d\r\n"),
                  Nr, pIrlapCb->Vs, pIrlapCb->TxWin.Start));
        return TRUE; // Invalid Nr
    }
    return FALSE;
}
/*****************************************************************************
*
*/
BOOLEAN
InWindow(UINT Start, UINT End, UINT i)
{
    if (Start <= End)
    {
        if (i >= Start && i <= End)
            return TRUE;
    }
    else
    {
        if (i >= Start || i <= End)
            return TRUE;
    }
    return FALSE;
}
/*****************************************************************************
*
*/
VOID
ProcessInvalidNsOrNr(PIRLAP_CB pIrlapCb,
                     int PFBit)
{
    ReturnRxTxWinMsgs(pIrlapCb);

    if (PRIMARY == pIrlapCb->StationType)
    {
        if (PFBit == IRLAP_PFBIT_SET)
        {
            SendDISC(pIrlapCb);
            pIrlapCb->RetryCnt = 0;
            // F-timer will be started by caller
            GotoPCloseState(pIrlapCb);
        }
        else
        {
            pIrlapCb->State = P_DISCONNECT_PEND;
        }
    }
    else // SECONDARY
    {
        pIrlapCb->Frmr.Vs = pIrlapCb->Vs;
        pIrlapCb->Frmr.Vr = pIrlapCb->Vr;
        pIrlapCb->Frmr.W = 0;
        pIrlapCb->Frmr.X = 0;
        pIrlapCb->Frmr.Y = 0;
        pIrlapCb->Frmr.Z = 1; // bad NR
        if (PFBit == IRLAP_PFBIT_SET)
        {
            SendFRMR(pIrlapCb, &pIrlapCb->Frmr);
        }
        else
        {
            pIrlapCb->State = S_ERROR;
        }
    }
}
/*****************************************************************************
*
*/
VOID
InsertRxWinAndForward(PIRLAP_CB pIrlapCb,
                      PIRDA_MSG pIrdaMsg,
                      UINT      Ns,
                      BOOLEAN      *pFreeMsg)
{
    UINT        rc = SUCCESS;
    PIRDA_MSG   pMsg;

    // insert message into receive window
    if (pIrlapCb->RxWin.pMsg[Ns] != NULL)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("IRLAP: RxFrame Ns:%d alread in RxWin\r\n"), Ns));
        
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ns:%d already in window\n"), Ns));
        return;
    }

    pIrlapCb->RxWin.pMsg[Ns] = pIrdaMsg;
#ifdef TEMPERAMENTAL_SERIAL_DRIVER
    pIrlapCb->RxWin.FCS[Ns] = pIrdaMsg->IRDA_MSG_FCS;
#endif    

    IRLAP_LOGMSG(("lap %d", 
        pIrdaMsg->IRDA_MSG_pWrite - pIrdaMsg->IRDA_MSG_pRead));

#if DBG_OUT
    // What if the MAC driver modifies the buffer that we may hold onto?
    {
        UINT    CheckVal = 0;
        UCHAR   *pByte = pIrdaMsg->IRDA_MSG_pRead;

        while (pByte != pIrdaMsg->IRDA_MSG_pWrite)
        {
            CheckVal += *pByte++;
        }

        *(UINT *)pIrdaMsg->IRDA_MSG_Header = CheckVal;
    }
#endif 

    // Advance RxWin.End to Ns+1 if Ns is at or beyond RxWin.End
    if (!InWindow(pIrlapCb->RxWin.Start, pIrlapCb->RxWin.End, Ns) ||
        Ns == pIrlapCb->RxWin.End)
    {
        pIrlapCb->RxWin.End = (Ns + 1) % IRLAP_MOD;
    }

    // Forward in sequence frames starting from Vr
    pMsg = pIrlapCb->RxWin.pMsg[pIrlapCb->Vr];
    while (pMsg != NULL && !pIrlapCb->LocalBusy)
    {

#if DBG_OUT
    // What if the MAC driver modifies the buffer that we may hold onto?
    {
        UINT    CheckVal = 0;
        UCHAR   *pByte = pMsg->IRDA_MSG_pRead;

        while (pByte != pMsg->IRDA_MSG_pWrite)
        {
            CheckVal += *pByte++;
        }

        if (CheckVal != *(UINT *)pMsg->IRDA_MSG_Header)
        {
            DEBUGMSG(1, (TEXT("MAC driver has modified buffer owned by IrLAP!!!!\n")));
            ASSERT(0);
        }
    }
#endif 

        pMsg->Prim = IRLAP_DATA_IND;

        rc = IrlmpUp(pIrlapCb->pIrdaLinkCb, pMsg);
        
        if (rc == SUCCESS || rc == IRLMP_LOCAL_BUSY)
        {
            // Delivered successfully. Done with this message. Remove it from
            // the RxWin and return msg to MAC. Update Vr
            pIrlapCb->RxWin.pMsg[pIrlapCb->Vr] = NULL;
            
            pMsg->Prim = MAC_DATA_RESP;
            IrmacDown(pIrlapCb->pIrdaLinkCb, pMsg);

            pIrlapCb->Vr = (pIrlapCb->Vr + 1) % IRLAP_MOD;
            
            pMsg = pIrlapCb->RxWin.pMsg[pIrlapCb->Vr];
            
            // LMP doesn't want anymore messages
            if (rc == IRLMP_LOCAL_BUSY)
            {
                // The receive window will be cleaned out when RNR is sent
                pIrlapCb->LocalBusy = TRUE;
            }
        }
        else
        {
            ASSERT(0);
            return;  
        }
    }
    *pFreeMsg = FALSE; // we either already freed it or placed it in the window
}
/*****************************************************************************
*
*/
VOID
ResendRejects(PIRLAP_CB pIrlapCb, UINT Nr)
{
    if (!pIrlapCb->RemoteBusy)
    {
        // Set Vs back

        for (pIrlapCb->Vs=Nr; pIrlapCb->Vs !=
                 (pIrlapCb->TxWin.End-1)%IRLAP_MOD;
             pIrlapCb->Vs = (pIrlapCb->Vs + 1) % IRLAP_MOD)
        {
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("RETRANSMISSION:")));
            SendIFrame(pIrlapCb,
                       pIrlapCb->TxWin.pMsg[pIrlapCb->Vs],
                       pIrlapCb->Vs,
                       IRLAP_PFBIT_CLEAR);
        }

        IRLAP_LOG_ACTION((pIrlapCb, TEXT("RETRANSMISSION:")));
        // Send last one with PFBit set
        SendIFrame(pIrlapCb, pIrlapCb->TxWin.pMsg[pIrlapCb->Vs],
                   pIrlapCb->Vs, IRLAP_PFBIT_SET);

        pIrlapCb->Vs = (pIrlapCb->Vs + 1) % IRLAP_MOD; // Vs == TxWin.End
    }
    else
    {
        SendRR_RNR(pIrlapCb);
    }
}
/*****************************************************************************
*
*/
VOID
ConfirmAckedTxMsgs(PIRLAP_CB pIrlapCb,
                UINT Nr)
{
    UINT i = pIrlapCb->TxWin.Start;
    IRDA_MSG *pMsg;

    while (i != Nr)
    {
        pMsg = pIrlapCb->TxWin.pMsg[i];
        pIrlapCb->TxWin.pMsg[i] = NULL;
        
        if (pMsg != NULL)
        {
            ASSERT(pMsg->IRDA_MSG_SendRefCnt);

            if (InterlockedDecrement(&pMsg->IRDA_MSG_SendRefCnt) == 0)
            {
                pMsg->Prim = IRLAP_DATA_CONF;
                pMsg->IRDA_MSG_DataStatus = IRLAP_DATA_REQUEST_COMPLETED;
    
                IrlmpUp(pIrlapCb->pIrdaLinkCb, pMsg);
            }
        }
        i = (i + 1) % IRLAP_MOD;
    }
    pIrlapCb->TxWin.Start = i;
}
/*****************************************************************************
*
*/
VOID
MissingRxFrames(PIRLAP_CB pIrlapCb)
{
    int MissingFrameCnt = 0;
    int MissingFrame = -1;
    UINT i;

    i = pIrlapCb->Vr;

    // Count missing frame, determine first missing frame

    for (i = pIrlapCb->Vr; (i + 1) % IRLAP_MOD != pIrlapCb->RxWin.End;
         i = (i+1) % IRLAP_MOD)
    {
        if (pIrlapCb->RxWin.pMsg[i] == NULL)
        {
            MissingFrameCnt++;
            if (MissingFrame == -1)
            {
                MissingFrame = i;
            }
        }
    }

    // if there are missing frames send SREJ (1) or RR (more than 1)
    // and turn link around
    if (MissingFrameCnt == 1 && !pIrlapCb->LocalBusy)
    {
        // we don't want to send the SREJ when local is busy because
        // peer *MAY* interpret it as a clearing of the local busy condition
        SendSREJ(pIrlapCb, MissingFrame);
    }
    else
    {
        // The RR/RNR will serve as an implicit REJ
        SendRR_RNR(pIrlapCb); 
    }
}
/*****************************************************************************
*
*/
VOID
IFrameOtherStates(PIRLAP_CB pIrlapCb,
                  int       CRBit,
                  int       PFBit)
{
    switch (pIrlapCb->State)
    {
      case NDM:
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
      case DSCV_REPLY:
      case CONN_MEDIA_SENSE:
      case SNRM_SENT:
      case BACKOFF_WAIT:
      case SNRM_RECEIVED:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        return;
    }

    if (pIrlapCb->CRBit == CRBit) // should be opposite of mine
    {
        if (pIrlapCb->StationType == PRIMARY)
        {
            if (pIrlapCb->State == P_XMIT)
            {
                IrlapTimerStop(TEXT("IFrameOtherStates"), pIrlapCb, &pIrlapCb->PollTimer);
            }
            else
            {
                IrlapTimerStop(TEXT("IFrameOtherStates"), pIrlapCb, &pIrlapCb->FinalTimer);
            }
        }
        else
        {
            IrlapTimerStop(TEXT("IFrameOtherStates"), pIrlapCb, &pIrlapCb->WDogTimer);
        }
        StationConflict(pIrlapCb);
        pIrlapCb->State = NDM;

        return;
    }

    if (pIrlapCb->StationType == PRIMARY) // I'm PRIMARY, this is a
    {                                    // response from secondary
        switch (pIrlapCb->State)
        {
          case P_DISCONNECT_PEND:
            if (PFBit == IRLAP_PFBIT_CLEAR)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
            }
            else
            {
                IrlapTimerStop(TEXT("IFrameOtherStates: P_DISCONNECT_PENDING"), 
                               pIrlapCb, &pIrlapCb->FinalTimer);
                SendDISC(pIrlapCb);
                pIrlapCb->RetryCnt = 0;
                IrlapTimerStart(TEXT("IFrameOtherStates: P_DISCONNECT_PENDING"), 
                                pIrlapCb, &pIrlapCb->FinalTimer);
                GotoPCloseState(pIrlapCb);
            }
            break;

          case P_CLOSE:
            if (PFBit == IRLAP_PFBIT_CLEAR)
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
            }
            else
            {
                IrlapTimerStop(TEXT("IFrameOtherStates: P_CLOSE"), 
                               pIrlapCb, &pIrlapCb->FinalTimer);
                ResendDISC(pIrlapCb);
                if (pIrlapCb->State != NDM)
                {
                    IrlapTimerStart(TEXT("IFrameOtherStates: P_CLOSE"), 
                                    pIrlapCb, &pIrlapCb->FinalTimer);
                }
            }
            break;

          case S_CLOSE:
            IrlapTimerStop(TEXT("IFrameOtherStates: S_CLOSE"), 
                           pIrlapCb, &pIrlapCb->WDogTimer);
            break;

          default:
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        }
    }
    else
    {
        switch (pIrlapCb->State)
        {
          case S_DISCONNECT_PEND:
            if (IRLAP_PFBIT_SET == PFBit)
            {
                IrlapTimerStop(TEXT("IFrameOtherStates: S_DISCONNECT_PEND"), 
                               pIrlapCb, &pIrlapCb->WDogTimer);
                SendRD(pIrlapCb);
                IrlapTimerStart(TEXT("IFrameOtherStates: S_DISCONNECT_PEND"), 
                                pIrlapCb, &pIrlapCb->WDogTimer);
                pIrlapCb->State = S_CLOSE;
            }
            else
            {
                IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
            }
            break;

          case S_ERROR:
            if (IRLAP_PFBIT_SET == PFBit)
            {
                SendFRMR(pIrlapCb, &pIrlapCb->Frmr);
                pIrlapCb->State = S_NRM;
            }
            else
            {
                IrlapTimerStop(TEXT("IFrameOtherStates: S_ERROR"), 
                               pIrlapCb, &pIrlapCb->WDogTimer);
                IrlapTimerStart(TEXT("IFrameOtherStates: S_ERROR"), 
                                pIrlapCb, &pIrlapCb->WDogTimer);
            }
            break;

          case S_CLOSE:
            if (IRLAP_PFBIT_SET == PFBit)
            {
                IrlapTimerStop(TEXT("IFrameOtherStates: S_CLOSE"), 
                               pIrlapCb, &pIrlapCb->WDogTimer);
                SendRD(pIrlapCb);
                IrlapTimerStart(TEXT("IFrameOtherStates: S_CLOSE"), 
                                pIrlapCb, &pIrlapCb->WDogTimer);
            }
            else
            {
                IrlapTimerStop(TEXT("IFrameOtherStates: S_CLOSE2"), 
                               pIrlapCb, &pIrlapCb->WDogTimer);
                IrlapTimerStart(TEXT("IFrameOtherStates: S_CLOSE2"), 
                                pIrlapCb, &pIrlapCb->WDogTimer);
            }
          default:
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignore in this state")));
        }
    }
}
/*****************************************************************************
*
*/
VOID
StationConflict(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG    IMsg;
    
    InitializeState(pIrlapCb, PRIMARY); // Primary doesn't mean anything here

    ApplyDefaultParms(pIrlapCb);
    IMsg.Prim = IRLAP_DISCONNECT_IND;
    IMsg.IRDA_MSG_DiscStatus = IRLAP_PRIMARY_CONFLICT;
    IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
}
/*****************************************************************************
*
*/
VOID
ApplyDefaultParms(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG    IMsg;
    
    //IndicateLinkStatus(pIrlapCb, LINK_STATUS_IDLE);

    pIrlapCb->Baud              = IRLAP_CONTENTION_BAUD;
    pIrlapCb->RemoteMaxTAT      = IRLAP_CONTENTION_MAX_TAT;
    pIrlapCb->RemoteDataSize    = IRLAP_CONTENTION_DATA_SIZE;
    pIrlapCb->RemoteWinSize     = IRLAP_CONTENTION_WIN_SIZE;
    pIrlapCb->RemoteNumBOFS     = IRLAP_CONTENTION_BOFS;
    pIrlapCb->ConnAddr          = IRLAP_BROADCAST_CONN_ADDR;

    IMsg.Prim               = MAC_CONTROL_REQ;
    IMsg.IRDA_MSG_Op        = MAC_RECONFIG_LINK;
    IMsg.IRDA_MSG_Baud      = IRLAP_CONTENTION_BAUD;
    IMsg.IRDA_MSG_NumBOFs   = IRLAP_CONTENTION_BOFS;
    IMsg.IRDA_MSG_DataSize  = IRLAP_CONTENTION_DATA_SIZE;
    IMsg.IRDA_MSG_MinTat    = 0;

    IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ - reconfig link")));

    IrmacDown(pIrlapCb->pIrdaLinkCb, &IMsg);
}
/*****************************************************************************
*
*/
VOID
ResendDISC(PIRLAP_CB pIrlapCb)
{
    IRDA_MSG    IMsg;
    
    if (pIrlapCb->RetryCnt >= pIrlapCb->N3)
    {
        ApplyDefaultParms(pIrlapCb);
        pIrlapCb->RetryCnt = 0;
        IMsg.Prim = IRLAP_DISCONNECT_IND;
        IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
        pIrlapCb->State = NDM;
        IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
    }
    else
    {
        SendDISC(pIrlapCb);
        pIrlapCb->RetryCnt++;
    }
}

/*****************************************************************************
*
*/
BOOLEAN
IgnoreState(PIRLAP_CB pIrlapCb)
{
    switch (pIrlapCb->State)
    {
      case NDM:
      case DSCV_MEDIA_SENSE:
      case DSCV_QUERY:
      case DSCV_REPLY:
      case CONN_MEDIA_SENSE:
      case SNRM_SENT:
      case BACKOFF_WAIT:
      case SNRM_RECEIVED:
      case P_XMIT:
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring in this state")));
        return TRUE;
    }
    return FALSE;
}
/*****************************************************************************
*
*/
VOID
QueryTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;

    IRLAP_LOG_START((pIrlapCb, TEXT("Query timer expired")));
    
    if (pIrlapCb->State == DSCV_REPLY)
    {
        pIrlapCb->State = NDM;
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb, 
            TEXT("Ignoring QueryTimer Expriation in state %s"),
            IRLAP_StateStr[pIrlapCb->State]));
    }
    
    IRLAP_LOG_COMPLETE(pIrlapCb);

    return;
}
/*****************************************************************************
*
*/
VOID
SlotTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;
    IRDA_MSG    IMsg;
    
    IRLAP_LOG_START((pIrlapCb, TEXT("Slot timer expired")));

    if (pIrlapCb->State == DSCV_QUERY)
    {
        pIrlapCb->SlotCnt++;
        SendDscvXIDCmd(pIrlapCb);
        if (pIrlapCb->SlotCnt < pIrlapCb->MaxSlot)
        {
            IMsg.Prim = MAC_CONTROL_REQ;
            IMsg.IRDA_MSG_Op = MAC_MEDIA_SENSE;
            IMsg.IRDA_MSG_SenseTime = DiscoverTimeout;
            IRLAP_LOG_ACTION((pIrlapCb, TEXT("MAC_CONTROL_REQ (dscv sense)")));            
            IrmacDown(pIrlapCb->pIrdaLinkCb,&IMsg);            
        }
        else
        {
            //IndicateLinkStatus(pIrlapCb, LINK_STATUS_IDLE);

            pIrlapCb->GenNewAddr = FALSE;

            IMsg.Prim = IRLAP_DISCOVERY_CONF;
            IMsg.IRDA_MSG_pDevList = &pIrlapCb->DevList;
            IMsg.IRDA_MSG_DscvStatus = IRLAP_DISCOVERY_COMPLETED;

            // Change state now so IRLMP can do DISCOVERY_REQ on this thread
            pIrlapCb->State = NDM;

            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
        }
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb, TEXT("Ignoring SlotTimer Expriation in state %s"),
                          IRLAP_StateStr[pIrlapCb->State]));
        ; // maybe return bad state ???
    }
    IRLAP_LOG_COMPLETE(pIrlapCb);
    return;
}
/*****************************************************************************
*
*/
VOID
FinalTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;
    IRDA_MSG    IMsg;
    
    IRLAP_LOG_START((pIrlapCb, TEXT("Final timer expired")));
    
    pIrlapCb->NoResponse = TRUE;

    switch (pIrlapCb->State)
    {
      case SNRM_SENT:
        if (pIrlapCb->RetryCnt < pIrlapCb->N3)
        {
            pIrlapCb->BackoffTimer.Timeout = IRLAP_BACKOFF_TIME();
            IrlapTimerStart(TEXT("FinalTimerExp: SNRM_SENT"),
                            pIrlapCb, &pIrlapCb->BackoffTimer);
            pIrlapCb->State = BACKOFF_WAIT;
        }
        else
        {
            ApplyDefaultParms(pIrlapCb);

            pIrlapCb->RetryCnt = 0;
            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
            pIrlapCb->State = NDM;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
        }
        break;

      case P_RECV:
        if (pIrlapCb->RetryCnt == pIrlapCb->N2)
        {
            pIrlapCb->RetryCnt = 0; // Don't have to, do it for logger
            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
            pIrlapCb->State = NDM;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            ReturnRxTxWinMsgs(pIrlapCb);            
            ApplyDefaultParms(pIrlapCb);
        }
        else
        {
            pIrlapCb->RetryCnt++;
            
            IrlapTimerStart(TEXT("FinalTimerExp: P_RECV"),
                            pIrlapCb, &pIrlapCb->FinalTimer);
            SendRR_RNR(pIrlapCb);
            if (pIrlapCb->RetryCnt == pIrlapCb->N1)
            {
                //IndicateLinkStatus(pIrlapCb, LINK_STATUS_INTERRUPTED);
            }
        }
        break;

      case P_DISCONNECT_PEND:
        SendDISC(pIrlapCb);
        pIrlapCb->RetryCnt = 0;
        IrlapTimerStart(TEXT("FinalTimerExp: P_DISCONNECT_PEND"),
                        pIrlapCb, &pIrlapCb->FinalTimer);
        GotoPCloseState(pIrlapCb);
        break;

      case P_CLOSE:
        if (pIrlapCb->RetryCnt >= pIrlapCb->N3)
        {
            ApplyDefaultParms(pIrlapCb);

            pIrlapCb->RetryCnt = 0; // Don't have to, do it for logger
            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            GotoNDMThenDscvOrConn(pIrlapCb);
        }
        else
        {
            pIrlapCb->RetryCnt++;
            SendDISC(pIrlapCb);
            IrlapTimerStart(TEXT("FinalTimerExp: P_CLOSE"),
                            pIrlapCb, &pIrlapCb->FinalTimer);
        }
        break;

      default:
        IRLAP_LOG_ACTION((pIrlapCb,
                          TEXT("Ignoring Final Expriation in state %s"),
                          IRLAP_StateStr[pIrlapCb->State]));
    }
    
    IRLAP_LOG_COMPLETE(pIrlapCb);
    return;
}
/*****************************************************************************
*
*/
VOID
PollTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;

    IRLAP_LOG_START((pIrlapCb, TEXT("Poll timer expired")));
    
    if (pIrlapCb->State == P_XMIT)
    {
        SendRR_RNR(pIrlapCb);
        IrlapTimerStart(TEXT("PollTimerExp"),
                        pIrlapCb, &pIrlapCb->FinalTimer);
        pIrlapCb->State = P_RECV;
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb,
                          TEXT("Ignoring Poll Expriation in state %s"),
                          IRLAP_StateStr[pIrlapCb->State]));
    }
    
    IRLAP_LOG_COMPLETE(pIrlapCb);    
    return;
}
/*****************************************************************************
*
*/
VOID
BackoffTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;
    
    IRLAP_LOG_START((pIrlapCb, TEXT("Backoff timer expired")));

    if (pIrlapCb->State == BACKOFF_WAIT)
    {
        SendSNRM(pIrlapCb, TRUE);
        IrlapTimerStart(TEXT("BackoffTimerExp"),
                        pIrlapCb, &pIrlapCb->FinalTimer);
        pIrlapCb->RetryCnt += 1;
        pIrlapCb->State = SNRM_SENT;
    }
    else
    {
        IRLAP_LOG_ACTION((pIrlapCb, 
              TEXT("Ignoring BackoffTimer Expriation in this state ")));
    }
    IRLAP_LOG_COMPLETE(pIrlapCb);
    return;
}
/*****************************************************************************
*
*/
VOID
WDogTimerExp(PVOID Context)
{
    PIRLAP_CB   pIrlapCb = (PIRLAP_CB) Context;
    IRDA_MSG    IMsg;
    
    IRLAP_LOG_START((pIrlapCb, TEXT("WDog timer expired")));

    pIrlapCb->NoResponse = TRUE;

    switch (pIrlapCb->State)
    {
      case S_DISCONNECT_PEND:
      case S_NRM:
        pIrlapCb->WDogExpCnt++;
        // Disconnect/threshold time is in seconds
        if (pIrlapCb->WDogExpCnt * (int)pIrlapCb->WDogTimer.Timeout >=
            pIrlapCb->DisconnectTime * 1000)
        {
            pIrlapCb->State = NDM;

            IMsg.Prim = IRLAP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
            IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
            ReturnRxTxWinMsgs(pIrlapCb);            
            ApplyDefaultParms(pIrlapCb);
        }
        else
        {
            if ((pIrlapCb->WDogExpCnt * (int) pIrlapCb->WDogTimer.Timeout >=
                 pIrlapCb->ThresholdTime * 1000) && !pIrlapCb->StatusSent)
            {
                //IndicateLinkStatus(pIrlapCb, LINK_STATUS_INTERRUPTED);
                pIrlapCb->StatusSent = TRUE;
            }
            IrlapTimerStart(TEXT("WDogTimerExp"),
                            pIrlapCb, &pIrlapCb->WDogTimer);
        }
        break;

      case S_CLOSE:
        ApplyDefaultParms(pIrlapCb);

        IMsg.Prim = IRLAP_DISCONNECT_IND;
        IMsg.IRDA_MSG_DiscStatus = IRLAP_NO_RESPONSE;
        pIrlapCb->State = NDM;
        IrlmpUp(pIrlapCb->pIrdaLinkCb, &IMsg);
        break;

      default:
        IRLAP_LOG_ACTION((pIrlapCb,
                          TEXT("Ignore WDogTimer expiration in state %s"),
                          IRLAP_StateStr[pIrlapCb->State]));
    }
    IRLAP_LOG_COMPLETE(pIrlapCb);
    return;
}
/*****************************************************************************
*
*/
UINT
DequeMsgList(IRDA_MSG_LIST *pList, IRDA_MSG **ppMsg)
{
    if (pList->Len != 0)
    {
        *ppMsg = (IRDA_MSG *) RemoveHeadList(&pList->ListHead);
/**
        {
            IRDA_MSG *pAMsg = pList->ListHead.Flink;

            printf(TEXT("\nDEQUE: %x\n"), *ppMsg);

            while (pAMsg != &(pList->ListHead))
            {
                printf(TEXT("%x->"),pAMsg);
                pAMsg = pAMsg->Linkage.Flink;
            }
            printf(TEXT("\n"));
        }
**/
        pList->Len--;
        return SUCCESS;
    }
    return IRLAP_MSG_LIST_EMPTY;
}
/*****************************************************************************
*
*/
UINT
EnqueMsgList(IRDA_MSG_LIST *pList, IRDA_MSG *pMsg, int MaxLen)
{
    if (MaxLen == -1 || pList->Len < MaxLen)
    {
        InsertTailList(&pList->ListHead, &(pMsg->Linkage));
        pList->Len++;
/**
        {
            IRDA_MSG *pAMsg = pList->ListHead.Flink;

            printf(TEXT("\nENQUE: %x\n"), pMsg);
            while (pAMsg != &(pList->ListHead))
            {
                printf(TEXT("%x->"),pAMsg);
                pAMsg = pAMsg->Linkage.Flink;
            }
            printf(TEXT("\n"));
        }
**/
        return SUCCESS;
    }
    return IRLAP_MSG_LIST_FULL;
}
/*****************************************************************************
*
*/
void
InitMsgList(IRDA_MSG_LIST *pList)
{
    InitializeListHead(&pList->ListHead);
    pList->Len = 0;
}
/*****************************************************************************
*
*   @func   ret_type | func_name | funcdesc
*
*   @rdesc  return desc
*   @flag   val | desc
*
*   @parm   data_type | parm_name | description
*
*   @comm
*           comments
*
*   @ex
*           example
*/
/* !!!
void
IRLAP_PrintState()
{
#if DBG    
    DEBUGMSG(1, (TEXT("IRLAP State %s\n"), IRLAP_StateStr[pIrlapCb->State]));
#else
    DEBUGMSG(1, (TEXT("IRLAP State %d\n"), pIrlapCb->State));
#endif    
    DEBUGMSG(1,
             (TEXT("  Vs=%d Vr=%d RxWin(%d,%d) TxWin(%d,%d) TxMsgListLen=%d RxMsgFreeListLen=%d\r\n"), 
              pIrlapCb->Vs, pIrlapCb->Vr,
              pIrlapCb->RxWin.Start, pIrlapCb->RxWin.End, 
              pIrlapCb->TxWin.Start, pIrlapCb->TxWin.End,
              pIrlapCb->TxMsgList.Len, pIrlapCb->RxMsgFreeList.Len));
    
#ifdef TEMPERAMENTAL_SERIAL_DRIVER    
    DEBUGMSG(1, (TEXT("  Tossed duplicates %d\n"), TossedDups));
#endif
    
    IRMAC_PrintState();
    
    return;
}
*/

int
GetMyDevAddr(BOOLEAN New)
{
    int             DevAddr    = GetTickCount();

#ifdef UNDER_CE    
	HKEY	        hKey;
	LONG	        hRes;
	TCHAR	        KeyName[32];
    ULONG           RegDevAddr = 0;
    TCHAR           ValName[]  = TEXT("DevAddr");

    // Get the device address from the registry. If the key exists and the
    // value is 0, store a new random address. If no key, then return
    // a random address.
    _tcscpy (KeyName, COMM_REG_KEY);
	_tcscat (KeyName, TEXT("IrDA"));
    
	hRes = RegOpenKeyEx (HKEY_LOCAL_MACHINE, KeyName, 0, 0, &hKey);

    if (hRes == ERROR_SUCCESS &&
        GetRegDWORDValue(hKey, ValName, &RegDevAddr))
    {
        if (RegDevAddr == 0)
        {
            RegDevAddr = GetTickCount();
            SetRegDWORDValue(hKey, ValName, RegDevAddr);
        }
        RegCloseKey(hKey);

        DevAddr = (int) RegDevAddr;
    }
#else 
    LARGE_INTEGER   li;
    int             DevAddr, NewDevAddr;

    KeQueryTickCount(&li);
    NewDevAddr = (int) li.LowPart;
    DevAddr = NewDevAddr;
#endif

    return DevAddr;
}

