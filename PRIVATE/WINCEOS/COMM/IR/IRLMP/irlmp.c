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
*       @module irlmp.c | Provides IrLMP API
*
*       Date: 4/15/95
*
*       @comm
*
*  This module exports the following API's:
*
*       IrlmpOpenLink()
*       IrlmpCloseLink()
*       IrlmpDown()
*       IrlmpUp()
*
*
*                |---------|
*                |   Tdi   |
*                |---------|
*                  /|\  |
*                   |   |
*           TdiUp() |   | IrlmpDown()
*                   |   |
*                   |  \|/
*                |---------|      IrdaTimerStart()     |-------|
*                |         |-------------------------->|       |
*                |  IRLMP  |                           | TIMER |
*                |         |<--------------------------|       |
*                |---------|      ExpFunc()            |-------|
*                  /|\  |
*                   |   |
*         IrlmpUp() |   | IrlapDown()
*                   |   |
*                   |  \|/
*                |---------|
*                |  IRLAP  |
*                |---------|
*
* See irda.h for complete message definitions
*
* Connection context for IRLMP and Tdi are exchanged
* during connection establishment:
*
*   Active connection:
*      +------------+ IRLMP_CONNECT_REQ(TdiContext)           +-------+
*      |            |---------------------------------------->|       |
*      |    Tdi     | IRLMP_CONNECT_CONF(IrlmpContext)        | IRMLP |
*      |            |<----------------------------------------|       |
*      +------------+                                         +-------+
*
*   Passive connection:
*      +------------+ IRLMP_CONNECT_IND(IrlmpContext)         +-------+
*      |            |<----------------------------------------|       |
*      |    Tdi     | IRLMP_CONNECT_RESP(TdiContext)          | IRMLP |
*      |            |---------------------------------------->|       |
*      +------------+                                         +-------+
*
*   
*   Tdi calling IrlmpDown(void *pIrlmpContext, IRDA_MSG *pMsg)
*       pIrlmpContext = NULL for the following:
*       pMsg->Prim = IRLMP_DISCOVERY_REQ,
*                    IRLMP_CONNECT_REQ,
*                    IRLMP_FLOWON_REQ,
*                    IRLMP_GETVALUEBYCLASS_REQ.
*       In all other cases, the pIRLMPContext must be a valid context.
*
*   IRLMP calling TdiUp(void *pTdiContext, IRDA_MSG *pMsg)
*       pTdiContext = NULL for the following:
*       pMsg->Prim = IRLAP_STATUS_IND,
*                    IRLMP_DISCOVERY_CONF,
*                    IRLMP_CONNECT_IND,
*                    IRLMP_GETVALUEBYCLASS_CONF.
*       In all other cases, the pTdiContext will have a valid context.
*/
#include <irda.h>
#include <irlap.h>
#include <irlmp.h>
#include <irlmpp.h>

// IAS
UCHAR IAS_IrLMPSupport[] = {IAS_IRLMP_VERSION, IAS_SUPPORT_BIT_FIELD, 
                           IAS_LMMUX_SUPPORT_BIT_FIELD};

const CHAR IasClassName_Device[]         = "Device";
const CHAR IasAttribName_DeviceName[]    = "DeviceName";
const CHAR IasAttribName_IrLMPSupport[]  = "IrLMPSupport";
const CHAR IasAttribName_TTPLsapSel[]    = "IrDA:TinyTP:LsapSel";
const CHAR IasAttribName_IrLMPLsapSel[]  = "IrDA:IrLMP:LsapSel";
const CHAR IasAttribName_IrLMPLsapSel2[] = "IrDA:IrLMP:LSAPSel"; // jeez

const UCHAR IasClassNameLen_Device        = sizeof(IasClassName_Device)-1;
const UCHAR IasAttribNameLen_DeviceName   = sizeof(IasAttribName_DeviceName)-1;
const UCHAR IasAttribNameLen_IrLMPSupport = sizeof(IasAttribName_IrLMPSupport)-1;
const UCHAR IasAttribNameLen_TTPLsapSel   = sizeof(IasAttribName_TTPLsapSel)-1;
const UCHAR IasAttribNameLen_IrLMPLsapSel = sizeof(IasAttribName_IrLMPLsapSel)-1;

// Globals
LIST_ENTRY      RegisteredLsaps;
LIST_ENTRY      DeviceList;
LIST_ENTRY      IasObjects;
IRDA_EVENT      EvDiscoveryReq;
IRDA_EVENT      EvConnectReq;
IRDA_EVENT      EvConnectResp;
IRDA_EVENT      EvLmConnectReq;
IRDA_EVENT      EvIrlmpCloseLink;
BOOLEAN         DscvReqScheduled;
LIST_ENTRY      IrdaLinkCbList;
NDIS_SPIN_LOCK  SpinLock;

// Prototypes
STATIC UINT CreateLsap(PIRLMP_LINK_CB, IRLMP_LSAP_CB **);
STATIC VOID DeleteLsap(IRLMP_LSAP_CB *);
STATIC VOID CleanupLsap(IRLMP_LSAP_CB *pLsapCb);
STATIC VOID TearDownConnections(PIRLMP_LINK_CB, IRLMP_DISC_REASON);
STATIC VOID IrlmpMoreCreditReq(IRLMP_LSAP_CB *, IRDA_MSG *);
STATIC VOID IrlmpDiscoveryReq(IRDA_MSG *pMsg);
STATIC UINT IrlmpLinkControlReq(IRDA_MSG *);
STATIC UINT IrlmpConnectReq(IRDA_MSG *);
STATIC UINT IrlmpConnectResp(IRLMP_LSAP_CB *, IRDA_MSG *);
STATIC UINT IrlmpDisconnectReq(IRLMP_LSAP_CB *, IRDA_MSG *);
STATIC UINT IrlmpDataReqExclusive(IRLMP_LSAP_CB *, IRDA_MSG *);
STATIC UINT IrlmpDataReqMultiplexed(IRLMP_LSAP_CB *, IRDA_MSG *);
STATIC VOID FormatAndSendDataReq(IRLMP_LSAP_CB *, IRDA_MSG *, BOOLEAN);
STATIC UINT IrlmpAccessModeReq(IRLMP_LSAP_CB *, IRDA_MSG *);
STATIC void SetupTtp(IRLMP_LSAP_CB *);
STATIC VOID SendCntlPdu(IRLMP_LSAP_CB *, int, int, int, int);
STATIC VOID LsapResponseTimerExp(PVOID);
STATIC VOID IrlapDiscoveryConf(PIRLMP_LINK_CB, IRDA_MSG *);
STATIC void UpdateDeviceList(PIRDA_LINK_CB, LIST_ENTRY *);
STATIC VOID IrlapConnectInd(PIRLMP_LINK_CB, IRDA_MSG *pMsg);
STATIC VOID IrlapConnectConf(PIRLMP_LINK_CB, IRDA_MSG *pMsg);
STATIC VOID IrlapDisconnectInd(PIRLMP_LINK_CB, IRDA_MSG *pMsg);
STATIC IRLMP_LSAP_CB *GetLsapInState(PIRLMP_LINK_CB, int, int, BOOLEAN);
STATIC IRLMP_LINK_CB *GetIrlmpCb(PUCHAR);
STATIC VOID DiscDelayTimerFunc(PVOID);
STATIC VOID IrlapDataConf(IRDA_MSG *pMsg);
STATIC VOID IrlapDataInd(PIRLMP_LINK_CB, IRDA_MSG *pMsg);
STATIC VOID LmPduConnectReq(PIRLMP_LINK_CB, IRDA_MSG *, int, int, UCHAR *);
STATIC VOID LmPduConnectConf(PIRLMP_LINK_CB, IRDA_MSG *, int, int, UCHAR *);
STATIC VOID LmPduDisconnectReq(PIRLMP_LINK_CB, IRDA_MSG *, int, int, UCHAR *);
STATIC VOID SendCreditPdu(IRLMP_LSAP_CB *);
STATIC VOID LmPduData(PIRLMP_LINK_CB, IRDA_MSG *, int, int);
STATIC VOID SetupTtpAndStoreConnData(IRLMP_LSAP_CB *, IRDA_MSG *);
STATIC VOID LmPduAccessModeReq(PIRLMP_LINK_CB, int, int, UCHAR *, UCHAR *);
STATIC VOID LmPduAccessModeConf(PIRLMP_LINK_CB, int, int, UCHAR *, UCHAR *);
STATIC IRLMP_LSAP_CB *GetLsap(PIRLMP_LINK_CB, int, int);
STATIC VOID UnroutableSendLMDisc(PIRLMP_LINK_CB, int, int);
STATIC VOID ScheduleConnectReq(PIRLMP_LINK_CB);
STATIC void InitiateCloseLink(PVOID Context);
STATIC void InitiateConnectReq(PVOID Context);
STATIC void InitiateDiscoveryReq(PVOID Context);
STATIC void InitiateConnectResp(PVOID Context);
STATIC void InitiateLMConnectReq(PVOID Context);
STATIC UINT IrlmpGetValueByClassReq(IRDA_MSG *);
STATIC IAS_OBJECT *IasGetObject(CHAR *pClassName);
STATIC IasGetValueByClass(const CHAR *, int, const CHAR *, int, void **,
                           int *, UCHAR *);
STATIC VOID IasConnectReq(PIRLMP_LINK_CB, int);
STATIC VOID IasServerDisconnectReq(IRLMP_LSAP_CB *pLsapCb);
STATIC VOID IasClientDisconnectReq(IRLMP_LSAP_CB *pLsapCb, IRLMP_DISC_REASON);
STATIC VOID IasSendQueryResp(IRLMP_LSAP_CB *, IRDA_MSG *);
STATIC VOID IasProcessQueryResp(PIRLMP_LINK_CB, IRLMP_LSAP_CB *, IRDA_MSG *);
STATIC VOID SendGetValueByClassReq(IRLMP_LSAP_CB *);
STATIC VOID SendGetValueByClassResp(IRLMP_LSAP_CB *, IRDA_MSG *);
STATIC VOID RegisterLsapProtocol(int Lsap, BOOLEAN UseTTP);
STATIC UINT IasAddAttribute(IAS_SET *pIASSet, PVOID *pAttribHandle);
STATIC VOID IasDelAttribute(PVOID AttribHandle);

STATIC VOID UpdateIasDeviceName(PIRLMP_LINK_CB pIrlmpCb);

/*****************************************************************************
*
*/
VOID
IrlmpInitialize()
{
    InitializeListHead(&RegisteredLsaps);
    InitializeListHead(&DeviceList);
    InitializeListHead(&IasObjects);
    InitializeListHead(&IrdaLinkCbList);

    NdisAllocateSpinLock(&SpinLock);
        
    DscvReqScheduled = FALSE;
    IrdaEventInitialize(&EvDiscoveryReq, InitiateDiscoveryReq);        
    IrdaEventInitialize(&EvConnectReq, InitiateConnectReq);
    IrdaEventInitialize(&EvConnectResp,InitiateConnectResp);
    IrdaEventInitialize(&EvLmConnectReq, InitiateLMConnectReq);
    IrdaEventInitialize(&EvIrlmpCloseLink, InitiateCloseLink);
}    

/*****************************************************************************
*
*/
VOID
IrdaShutdown()
{
    PIRDA_LINK_CB   pIrdaLinkCb, pIrdaLinkCbNext;
#ifndef UNDER_CE
    LARGE_INTEGER   SleepMs;
#endif 
    NTSTATUS        Status;
    UINT            Seconds;


    // This needs to be fixed

#ifndef UNDER_CE
    SleepMs.QuadPart = -(10*1000*1000); // 1 second
#endif 

    NdisAcquireSpinLock(&SpinLock);

    for (pIrdaLinkCb = (PIRDA_LINK_CB) IrdaLinkCbList.Flink;
         (LIST_ENTRY *) pIrdaLinkCb != &IrdaLinkCbList;
         pIrdaLinkCb = pIrdaLinkCbNext)
    {
        pIrdaLinkCbNext = (PIRDA_LINK_CB) pIrdaLinkCb->Linkage.Flink;

        NdisReleaseSpinLock(&SpinLock);

        IrlmpCloseLink(pIrdaLinkCb);

        NdisAcquireSpinLock(&SpinLock);
    }

    NdisReleaseSpinLock(&SpinLock);

    Seconds = 0;
    while (Seconds < 30)
    {
        if (IsListEmpty(&IrdaLinkCbList))
            break;
    #ifdef UNDER_CE
        Sleep(1000);
    #else 
        KeDelayExecutionThread(KernelMode, FALSE, &SleepMs);
    #endif 

        Seconds++;
    }

#ifdef UNDER_CE
    Sleep(1000);
    ASSERT(Seconds < 30);
#else 

#if DBG
    if (Seconds >= 30)
    {
        DbgPrint("Link left open at shutdown!\n");

        for (pIrdaLinkCb = (PIRDA_LINK_CB) IrdaLinkCbList.Flink;
             (LIST_ENTRY *) pIrdaLinkCb != &IrdaLinkCbList;
            pIrdaLinkCb = pIrdaLinkCbNext)
        {
            DbgPrint("pIrdaLinkCb: %X\n", pIrdaLinkCb);
            DbgPrint("   pIrlmpCb: %X\n", pIrdaLinkCb->IrlmpContext);
            DbgPrint("   pIrlapCb: %X\n", pIrdaLinkCb->IrlapContext);
        }
        ASSERT(0);
    }
    else
    {
        DbgPrint("Irda shutdown complete\n");
    }
#endif 

    KeDelayExecutionThread(KernelMode, FALSE, &SleepMs);
#endif 

    NdisFreeSpinLock(&SpinLock);
    NdisDeregisterProtocol(&Status, NdisIrdaHandle);
}
/*****************************************************************************
*
*/
VOID
IrlmpOpenLink(OUT PNTSTATUS       Status,
              IN  PIRDA_LINK_CB   pIrdaLinkCb,  
              IN  UCHAR           *pDeviceName,
              IN  int             DeviceNameLen,
              IN  UCHAR           CharSet)
{
    PIRLMP_LINK_CB   pIrlmpCb;
    UINT             rc;
    IAS_SET          *pIASSet;

    *Status = STATUS_SUCCESS;

    if ((pIrlmpCb = CTEAllocMem(sizeof(IRLMP_LINK_CB))) == NULL)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("Alloc failed\n")));
        *Status = STATUS_INSUFFICIENT_RESOURCES;
        return;
    }

    pIASSet = (IAS_SET *)IrdaAlloc(sizeof(IAS_SET) + 128, 0);
    if (NULL == pIASSet) {
        CTEFreeMem(pIrlmpCb);
        DEBUGMSG(DBG_ERROR, (TEXT("IAS_SET Alloc failed\n")));
        *Status = STATUS_INSUFFICIENT_RESOURCES;
        return;
    }

    pIrdaLinkCb->IrlmpContext = pIrlmpCb;
    
#if DBG
    pIrlmpCb->DiscDelayTimer.pName = "DiscDelay";
#endif
    IrdaTimerInitialize(&pIrlmpCb->DiscDelayTimer,
                        DiscDelayTimerFunc,
                        IRLMP_DISCONNECT_DELAY_TIMEOUT,
                        pIrlmpCb,
                        pIrdaLinkCb);
    
    InitializeListHead(&pIrlmpCb->LsapCbList);

    pIrlmpCb->pIrdaLinkCb       = pIrdaLinkCb;
    pIrlmpCb->ConnReqScheduled  = FALSE;
    pIrlmpCb->LinkState         = LINK_DISCONNECTED;
    pIrlmpCb->pExclLsapCb       = NULL; 
    pIrlmpCb->pIasQuery         = NULL;

    // ConnDevAddrSet is set to true if LINK_IN_DISCOVERY or
    // LINK_DISCONNECTING and an LSAP requests a connection. Subsequent
    // LSAP connection requests check to see if this flag is set. If so
    // the requested device address must match that contained in the
    // IRLMP control block (set by the first connect request)
    pIrlmpCb->ConnDevAddrSet = FALSE;

    // Add device info to IAS        
    RtlCopyMemory(pIASSet->irdaClassName, IasClassName_Device,
                  IasClassNameLen_Device+1);
    RtlCopyMemory(pIASSet->irdaAttribName, IasAttribName_DeviceName,
                  IasAttribNameLen_DeviceName+1);
    pIASSet->irdaAttribType = IAS_ATTRIB_VAL_STRING;

    DeviceNameLen = min(DeviceNameLen, sizeof(pIASSet->irdaAttribute.irdaAttribUsrStr.UsrStr));
    RtlCopyMemory(pIASSet->irdaAttribute.irdaAttribUsrStr.UsrStr,
                  pDeviceName, 
                  DeviceNameLen + 1);
    pIASSet->irdaAttribute.irdaAttribUsrStr.CharSet = CharSet;
    pIASSet->irdaAttribute.irdaAttribUsrStr.Len = DeviceNameLen;        
    rc = IasAddAttribute(pIASSet, &pIrlmpCb->hAttribDeviceName);
    ASSERT(rc == 0);

    RtlCopyMemory(pIASSet->irdaClassName, IasClassName_Device,
                  IasClassNameLen_Device+1);
    RtlCopyMemory(pIASSet->irdaAttribName, IasAttribName_IrLMPSupport, 
                  IasAttribNameLen_IrLMPSupport+1);
    pIASSet->irdaAttribType = IAS_ATTRIB_VAL_BINARY;
    RtlCopyMemory(pIASSet->irdaAttribute.irdaAttribOctetSeq.OctetSeq, 
           IAS_IrLMPSupport, sizeof(IAS_IrLMPSupport));
    pIASSet->irdaAttribute.irdaAttribOctetSeq.Len =  
        sizeof(IAS_IrLMPSupport);
        
    rc = IasAddAttribute(pIASSet, &pIrlmpCb->hAttribIrlmpSupport);
    ASSERT(rc == 0);

    if (*Status != STATUS_SUCCESS)
    {
        CTEFreeMem(pIrlmpCb);
    }
    else
    {
        NdisInterlockedInsertTailList(&IrdaLinkCbList,
                                      &pIrdaLinkCb->Linkage,
                                      &SpinLock);
    }

    IrdaFree(pIASSet);
    
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP initialized, status %x\n"), *Status));
    
    return;
}

/*****************************************************************************
*
*
*
*/
VOID
IrlmpDeleteInstance(PVOID Context)
{
    PIRLMP_LINK_CB  pIrlmpCb = (PIRLMP_LINK_CB) Context;
    PVOID           LinkContext = pIrlmpCb->pIrdaLinkCb;
    IRDA_DEVICE     *pDevice, *pNextDevice;
    
    NdisAcquireSpinLock(&SpinLock);
    
    for (pDevice = (IRDA_DEVICE *) DeviceList.Flink;
         (LIST_ENTRY *) pDevice != &DeviceList;
         pDevice = pNextDevice)
    {
        pNextDevice = (IRDA_DEVICE *) pDevice->Linkage.Flink;
        
        if (LinkContext == pDevice->LinkContext)
        {
            RemoveEntryList(&pDevice->Linkage);
            IRDA_FREE_MEM(pDevice);
        }
    }    

    // remove IrdaLinkCb from List

    RemoveEntryList(&pIrlmpCb->pIrdaLinkCb->Linkage);

    ASSERT(IsListEmpty(&pIrlmpCb->LsapCbList));
    
    NdisReleaseSpinLock(&SpinLock);

    IasDelAttribute(pIrlmpCb->hAttribDeviceName);
    IasDelAttribute(pIrlmpCb->hAttribIrlmpSupport);

    CTEFreeMem(pIrlmpCb);
    
/*        
    // Cleanup registered LSAP
    while (!IsListEmpty(&RegisteredLsaps))
    {
        pPtr = RemoveHeadList(&RegisteredLsaps);
        IRDA_FREE_MEM(pPtr);
    }
    // And the device list
    while (!IsListEmpty(&DeviceList))
    {
        pPtr = RemoveHeadList(&DeviceList);
        IRDA_FREE_MEM(pPtr);
    }    

    // And IAS entries
    for (pObject = (IAS_OBJECT *) IasObjects.Flink;
         (LIST_ENTRY *) pObject != &IasObjects;
         pObject = pNextObject)    
    {
        DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Deleting object %s\n"),
                              pObject->pClassName));
        
        // Get the next cuz this ones being deleted
        pNextObject = (IAS_OBJECT *) pObject->Linkage.Flink;

        IasDeleteObject(pObject->pClassName);
    }
 */   
}

/*****************************************************************************
*
*   @func   UINT | IrlmpCloseLink | Shuts down IRDA stack
*
*   @rdesc  SUCCESS or error
*
*/
VOID
IrlmpCloseLink(PIRDA_LINK_CB pIrdaLinkCb)
{
    DEBUGMSG(1, (TEXT("IRLMP: CloseLink request\n")));

    IrdaEventSchedule(&EvIrlmpCloseLink, pIrdaLinkCb);

    return;
}
/*****************************************************************************
*
*   @func   UINT | IrlmpRegisterLSAPProtocol | Bag to let IRLMP know if
*                                               a connect ind is using TTP
*   @rdesc  SUCCESS or error
*
*   @parm   int | LSAP | LSAP being registered
*   @parm   BOOLEAN | UseTtp | 
*/
VOID
RegisterLsapProtocol(int Lsap, BOOLEAN UseTtp)
{
    PIRLMP_REGISTERED_LSAP     pRegLsap;
    
    NdisAcquireSpinLock(&SpinLock);

    for (pRegLsap = (PIRLMP_REGISTERED_LSAP) RegisteredLsaps.Flink;
         (LIST_ENTRY *) pRegLsap != &RegisteredLsaps;
         pRegLsap = (PIRLMP_REGISTERED_LSAP) pRegLsap->Linkage.Flink)
    {
        if (pRegLsap->Lsap == Lsap)
        {
            pRegLsap->UseTtp = UseTtp;
            goto done;
        }
    }

    if (IRDA_ALLOC_MEM(pRegLsap, sizeof(IRLMP_REGISTERED_LSAP), 
                       MT_IRLMP_REGLSAP) == NULL)
    {
        ASSERT(0);
        goto done;
    }
    pRegLsap->Lsap = Lsap;
    pRegLsap->UseTtp = UseTtp;
    InsertTailList(&RegisteredLsaps, &pRegLsap->Linkage);    

done:

    NdisReleaseSpinLock(&SpinLock);

    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: LSAP %x registered, %s\n"), Lsap,
                          UseTtp ? TEXT("use TTP") : TEXT("no TTP")));
}

VOID
DeregisterLsapProtocol(int Lsap)
{
    PIRLMP_REGISTERED_LSAP     pRegLsap;

    NdisAcquireSpinLock(&SpinLock);

    for (pRegLsap = (PIRLMP_REGISTERED_LSAP) RegisteredLsaps.Flink;
         (LIST_ENTRY *) pRegLsap != &RegisteredLsaps;
         pRegLsap = (PIRLMP_REGISTERED_LSAP) pRegLsap->Linkage.Flink)
    {
        if (pRegLsap->Lsap == Lsap)
        {
            DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: LSAP %x deregistered\n"),
                        Lsap));

            RemoveEntryList(&pRegLsap->Linkage);

            IRDA_FREE_MEM(pRegLsap);
            break;
        }
    }

    NdisReleaseSpinLock(&SpinLock);
}

/*****************************************************************************
*
*   @func   UINT | DeleteLsap | Delete an Lsap control context and
*
*   @rdesc  pointer to Lsap context or 0 on error
*
*   @parm   void | pLsapCb | pointer to an Lsap control block
*/
void
DeleteLsap(IRLMP_LSAP_CB *pLsapCb)
{
    VALIDLSAP(pLsapCb);

    ASSERT(pLsapCb->State == LSAP_DISCONNECTED);

    ASSERT(IsListEmpty(&pLsapCb->SegTxMsgList));

    ASSERT(IsListEmpty(&pLsapCb->TxMsgList));

    LOCK_LINK(pLsapCb->pIrlmpCb->pIrdaLinkCb);

#ifdef DBG 
    pLsapCb->Sig = 0xdaeddead;
#endif 

    RemoveEntryList(&pLsapCb->Linkage);

    UNLOCK_LINK(pLsapCb->pIrlmpCb->pIrdaLinkCb);
    REFDEL(&pLsapCb->pIrlmpCb->pIrdaLinkCb->RefCnt, 'PASL');
    
    IrdaTimerStop(&pLsapCb->ResponseTimer);
    
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Deleting LsapCb:%X (%d,%d)\n"),
             pLsapCb, pLsapCb->LocalLsapSel, pLsapCb->RemoteLsapSel));

    memset(pLsapCb, 0, sizeof(IRLMP_LSAP_CB));

    IRDA_FREE_MEM(pLsapCb);
}

/*****************************************************************************
*
*   @func   UINT | CreateLsap | Create an LSAP control context and
*/
UINT
CreateLsap(PIRLMP_LINK_CB pIrlmpCb, IRLMP_LSAP_CB **ppLsapCb)
{
    IRLMP_LSAP_CB * pLsapCb;
    *ppLsapCb = NULL;

    IRDA_ALLOC_MEM(pLsapCb, sizeof(IRLMP_LSAP_CB), MT_IRLMP_LSAP_CB);

    if (pLsapCb == NULL)
    {
        return IRLMP_ALLOC_FAILED;
    }

    *ppLsapCb = pLsapCb;

    memset(pLsapCb, 0, sizeof(IRLMP_LSAP_CB));

    pLsapCb->pIrlmpCb = pIrlmpCb;
    pLsapCb->State = LSAP_CREATED;
    pLsapCb->UserDataLen = 0;

    InitializeListHead(&pLsapCb->TxMsgList);
    InitializeListHead(&pLsapCb->SegTxMsgList);

    ReferenceInit(&pLsapCb->RefCnt, *ppLsapCb, DeleteLsap);
    REFADD(&pLsapCb->RefCnt, ' TS1');

#if DBG
    pLsapCb->ResponseTimer.pName = "ResponseTimer";
    pLsapCb->Sig = LSAPSIG;
#endif
    IrdaTimerInitialize(&pLsapCb->ResponseTimer,
                        LsapResponseTimerExp,
                        LSAP_RESPONSE_TIMEOUT,
                        pLsapCb,
                        pIrlmpCb->pIrdaLinkCb);

    // Insert into list in the link control block
    NdisAcquireSpinLock(&SpinLock);

    InsertTailList(&pIrlmpCb->LsapCbList, &pLsapCb->Linkage);

    NdisReleaseSpinLock(&SpinLock);
    
    REFADD(&pIrlmpCb->pIrdaLinkCb->RefCnt, 'PASL');

    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: New LsapCb:%X\n"),
             pLsapCb));

    return SUCCESS;
}

void
CleanupLsap(IRLMP_LSAP_CB *pLsapCb)
{
    IRDA_MSG *pMsg, *pNextMsg, *pSegParentMsg;
    
    VALIDLSAP(pLsapCb);

    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: CleanupLsap:%X\n"), pLsapCb));

    if (pLsapCb->State == LSAP_DISCONNECTED)
    {
        ASSERT(0);
        return;
    }

    if (pLsapCb == pLsapCb->pIrlmpCb->pExclLsapCb)
    {
        pLsapCb->pIrlmpCb->pExclLsapCb = NULL;
    }
    
    pLsapCb->State = LSAP_DISCONNECTED;

    // Clean up the segmented tx msg list
    while (!IsListEmpty(&pLsapCb->SegTxMsgList))
    {
        pMsg = (IRDA_MSG *) RemoveHeadList(&pLsapCb->SegTxMsgList);
        
        // Decrement the segment counter contained in the parent data request
        pSegParentMsg = pMsg->DataContext;
        pSegParentMsg->IRDA_MSG_SegCount -= 1;
        // IRLMP owns these
        FreeIrdaBuf(IrdaMsgPool, pMsg);
    }

    // return any outstanding data requests (unless there are outstanding segments)
    for (pMsg = (IRDA_MSG *) pLsapCb->TxMsgList.Flink;
         (LIST_ENTRY *) pMsg != &(pLsapCb->TxMsgList);
         pMsg = pNextMsg)
    {
        pNextMsg = (IRDA_MSG *) pMsg->Linkage.Flink;

        if (pMsg->IRDA_MSG_SegCount == 0)
        {
            RemoveEntryList(&pMsg->Linkage);
            pMsg->Prim = IRLMP_DATA_CONF;
            pMsg->IRDA_MSG_DataStatus = IRLMP_DATA_REQUEST_FAILED;
            TdiUp(pLsapCb->pTdiContext, pMsg);
        }
    }
    REFDEL(&pLsapCb->RefCnt, ' TS1');
}

/*****************************************************************************
*
*   @func   void | TearDownConnections | Tears down and cleans up connections
*
*   @parm   IRLMP_DISC_REASONE| DiscReason | The reason connections are being
*                                            torn down. Passed to IRLMP clients
*                                            in IRLMP_DISCONNECT_IND
*/
void
TearDownConnections(PIRLMP_LINK_CB pIrlmpCb, IRLMP_DISC_REASON DiscReason)
{
    IRLMP_LSAP_CB       *pLsapCb, *pLsapCbNext;
    IRDA_MSG            IMsg;
    
    pIrlmpCb->pExclLsapCb = NULL; 
   
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Tearing down connections\r\n")));
    
    // Clean up each LSAP
    for (pLsapCb = (IRLMP_LSAP_CB *) pIrlmpCb->LsapCbList.Flink;
         (LIST_ENTRY *) pLsapCb != &pIrlmpCb->LsapCbList;
         pLsapCb = pLsapCbNext)
    {
        pLsapCbNext = (IRLMP_LSAP_CB *) pLsapCb->Linkage.Flink;

        DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Teardown LsapCb:%X\n"), pLsapCb));
        VALIDLSAP(pLsapCb);

        if (pLsapCb->LocalLsapSel == IAS_LSAP_SEL)
        {
            IasServerDisconnectReq(pLsapCb);
            continue;
        }
        
        if (pLsapCb->LocalLsapSel == IAS_LOCAL_LSAP_SEL && 
            pLsapCb->RemoteLsapSel == IAS_LSAP_SEL)
        {
            IasClientDisconnectReq(pLsapCb, DiscReason);
        }
        else
        {
            IrdaTimerStop(&pLsapCb->ResponseTimer);

            if (pLsapCb->State != LSAP_DISCONNECTED)
            {
                DEBUGMSG(DBG_IRLMP, 
                         (TEXT("IRLMP: Sending IRLMP Disconnect Ind\r\n")));

                // Now tell client this LSAP is history
        
                IMsg.Prim = IRLMP_DISCONNECT_IND;
                IMsg.IRDA_MSG_DiscReason = DiscReason;
                IMsg.IRDA_MSG_pDiscData = NULL;
                IMsg.IRDA_MSG_DiscDataLen = 0;

                TdiUp(pLsapCb->pTdiContext, &IMsg);
                CleanupLsap(pLsapCb);
            }
        }
    }
}

/*****************************************************************************
*
*   @func   UINT | IrlmpDown | Message from Upper layer to LMP
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   void * | void_pLsapCb | void pointer to an LSAP_CB. Can be NULL
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*/
UINT
IrlmpDown(void *void_pLsapCb, IRDA_MSG *pMsg)
{
    UINT rc = SUCCESS;

    IRLMP_LSAP_CB *pLsapCb =
            (IRLMP_LSAP_CB *) void_pLsapCb;

    if (pLsapCb != NULL && pLsapCb->State == LSAP_DISCONNECTED)
    {
        return IRLMP_INVALID_LSAP_CB;
    }

    if (pLsapCb)
    {
#ifdef DEBUG
        if (pLsapCb->Sig != LSAPSIG) {
            DEBUGMSG(DBG_ERROR, (L"IRLMP:IrlmpDown IRLMP_LSAP_SIG==0x%x vs. 0x%x\n", pLsapCb->Sig, LSAPSIG));
            ASSERT(0);
            return IRLMP_INVALID_LSAP_CB;
        }
#endif
        VALIDLSAP(pLsapCb);

        REFADD(&pLsapCb->RefCnt, 'NWOD');
        if (pLsapCb->pIrlmpCb == NULL) {
            REFDEL(&pLsapCb->RefCnt, 'NWOD');
            DEBUGMSG(DBG_ERROR, (L"IRLMP:IrlmpDown pIrlmpCb==NULL\n"));
            return IRLMP_INVALID_LSAP_CB;
        }

        LOCK_LINK(pLsapCb->pIrlmpCb->pIrdaLinkCb);
    } else {
        //
        // These requests cannot have a NULL LsapCb
        //
        switch (pMsg->Prim) {
        case IRLMP_CONNECT_RESP:
        case IRLMP_DATA_REQ:
        case IRLMP_ACCESSMODE_REQ:
        case IRLMP_MORECREDIT_REQ:
            return IRLMP_INVALID_LSAP_CB;
        }
    }
    
    switch (pMsg->Prim)
    {
      case IRLMP_DISCOVERY_REQ:
        IrlmpDiscoveryReq(pMsg);
        break;

      case IRLMP_CONNECT_REQ:
        rc = IrlmpConnectReq(pMsg);
        break;

      case IRLMP_CONNECT_RESP:
        rc = IrlmpConnectResp(pLsapCb, pMsg);
        break;

      case IRLMP_DISCONNECT_REQ:
        rc = IrlmpDisconnectReq(pLsapCb, pMsg);
        break;

      case IRLMP_DATA_REQ:
        if (pLsapCb->pIrlmpCb->pExclLsapCb != NULL)
        {
            rc = IrlmpDataReqExclusive(pLsapCb, pMsg);
        }
        else
        {
            rc = IrlmpDataReqMultiplexed(pLsapCb, pMsg);
        }
        break;

      case IRLMP_ACCESSMODE_REQ:
        rc = IrlmpAccessModeReq(pLsapCb, pMsg);
        break;
        
      case IRLMP_MORECREDIT_REQ:
        IrlmpMoreCreditReq(pLsapCb, pMsg);
        break;

      case IRLMP_GETVALUEBYCLASS_REQ:
        rc = IrlmpGetValueByClassReq(pMsg);
        break;

      case IRLMP_REGISTERLSAP_REQ:
        RegisterLsapProtocol(pMsg->IRDA_MSG_LocalLsapSel,
                             pMsg->IRDA_MSG_UseTtp);
        break;

      case IRLMP_DEREGISTERLSAP_REQ:
        DeregisterLsapProtocol(pMsg->IRDA_MSG_LocalLsapSel);
        break;

      case IRLMP_ADDATTRIBUTE_REQ:
        rc = IasAddAttribute(pMsg->IRDA_MSG_pIasSet, pMsg->IRDA_MSG_pAttribHandle);
        break;

      case IRLMP_DELATTRIBUTE_REQ:
        IasDelAttribute(pMsg->IRDA_MSG_AttribHandle);
        break;
        
      case IRLMP_LINKCONTROL_REQ:
        rc = IrlmpLinkControlReq(pMsg);
        break;

      default:
        ASSERT(0);
    }

    if (pLsapCb)
    {
        UNLOCK_LINK(pLsapCb->pIrlmpCb->pIrdaLinkCb);
        REFDEL(&pLsapCb->RefCnt, 'NWOD');
    }
    
    return rc;
}

UINT
IrlmpLinkControlReq(IRDA_MSG *pMsg)
{
    PIRDA_LINK_CB pIrdaLinkCb, pIrdaLinkCbNext;
    UINT          rc = SUCCESS;
    PIRLMP_LINK_CB pIrlmpCb;

    NdisAcquireSpinLock(&SpinLock);

    for (pIrdaLinkCb = (PIRDA_LINK_CB) IrdaLinkCbList.Flink;
         (LIST_ENTRY *) pIrdaLinkCb != &IrdaLinkCbList;
         pIrdaLinkCb = pIrdaLinkCbNext)
    {
        pIrdaLinkCbNext = (PIRDA_LINK_CB) pIrdaLinkCb->Linkage.Flink;

        pIrlmpCb = pIrdaLinkCb->IrlmpContext;
    
        while (pIrlmpCb->LinkState != LINK_DISCONNECTED)
        {
            NdisReleaseSpinLock(&SpinLock);
            // TDI should ensure that we never get a connect request while
            // we are trying to release the link.
            DEBUGMSG(ZONE_WARN,
                (TEXT("Irlmp: waiting for LMP to disconnect to release link.\r\n"))
                );
            Sleep(500); 
            NdisAcquireSpinLock(&SpinLock);
        }
    
        NdisReleaseSpinLock(&SpinLock);

        // If we are re-acquiring resources, we need to reload the device
        // name and change the IAS database. 
        if (pMsg->IRDA_MSG_AcquireResources == TRUE)
        {
            UpdateIasDeviceName(pIrlmpCb);
        }

        pMsg->Prim = IRLAP_LINKCONTROL_REQ;
        // AcquireResources is already set.
        // Currently we will not use the link configuration block.
        pMsg->IRDA_MSG_pLinkConfig  = NULL;
        pMsg->IRDA_MSG_pLinkContext = pIrdaLinkCb;
        rc = IrlapDown(pIrdaLinkCb->IrlapContext, pMsg);

        NdisAcquireSpinLock(&SpinLock);

        if (rc != SUCCESS)
        {
            DEBUGMSG(DBG_ERROR,
                     (TEXT("IRLAP_LINKCONTROL_REQ failure - 0x%x\r\n"), rc));
            

            break; 
        }
    }

    NdisReleaseSpinLock(&SpinLock);

    return (rc);
}

VOID
IrlmpMoreCreditReq(IRLMP_LSAP_CB *pLsapCb, IRDA_MSG *pMsg)
{
    int CurrentAvail = pLsapCb->AvailableCredit;

    pLsapCb->AvailableCredit += pMsg->IRDA_MSG_TtpCredits;
    
    if (pLsapCb->UseTtp)
    {
        if ((CurrentAvail == 0) || (pLsapCb->RemoteTxCredit == 0))
        {
            // remote peer completely out of credit, send'm some
            SendCreditPdu(pLsapCb);
        }
    }
    else
    {
        if (pLsapCb == pLsapCb->pIrlmpCb->pExclLsapCb)
        {
            pLsapCb->RemoteTxCredit += pMsg->IRDA_MSG_TtpCredits;
            pMsg->Prim = IRLAP_FLOWON_REQ;
            IrlapDown(pLsapCb->pIrlmpCb->pIrdaLinkCb->IrlapContext, pMsg);
        }
    }
}

/*****************************************************************************
*
*   @func   UINT | IrlmpDiscoveryReq | initiates a discovery request
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*/
VOID
IrlmpDiscoveryReq(IRDA_MSG *pMsg)
{
    PIRDA_LINK_CB   pIrdaLinkCb;
    PIRLMP_LINK_CB  pIrlmpCb;
    
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: IRLMP_DISCOVERY_REQ\n")));

    NdisAcquireSpinLock(&SpinLock);

    if (DscvReqScheduled)
    {
        DEBUGMSG(DBG_IRLMP, (TEXT("Discovery already schedule\n")));
        NdisReleaseSpinLock(&SpinLock);
    }
    else
    {
        // Flag each link for discovery
        for (pIrdaLinkCb = (PIRDA_LINK_CB) IrdaLinkCbList.Flink;
             (LIST_ENTRY *) pIrdaLinkCb != &IrdaLinkCbList;
             pIrdaLinkCb = (PIRDA_LINK_CB) pIrdaLinkCb->Linkage.Flink)    
        {
            pIrlmpCb = (PIRLMP_LINK_CB) pIrdaLinkCb->IrlmpContext;
            pIrlmpCb->DiscoveryPending = TRUE;
        }
        
        DscvReqScheduled = TRUE;
        
        NdisReleaseSpinLock(&SpinLock);

        // Schedule the first link
    
        IrdaEventSchedule(&EvDiscoveryReq, NULL);
    }
}
/*****************************************************************************
*
*   @func   UINT | IrlmpConnectReq | Process IRLMP connect request
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*/
UINT
IrlmpConnectReq(IRDA_MSG *pMsg)
{
    PIRLMP_LSAP_CB  pLsapCb;
    PIRLMP_LINK_CB  pIrlmpCb = GetIrlmpCb(pMsg->IRDA_MSG_RemoteDevAddr);
    UINT            rc = SUCCESS;

    if (pIrlmpCb == NULL)
        return IRLMP_BAD_DEV_ADDR;
        
    LOCK_LINK(pIrlmpCb->pIrdaLinkCb);        
    
    if (pIrlmpCb->pExclLsapCb != NULL)
    {
        rc = IRLMP_IN_EXCLUSIVE_MODE;
    } 
    else if (GetLsap(pIrlmpCb, pMsg->IRDA_MSG_LocalLsapSel, 
            pMsg->IRDA_MSG_RemoteLsapSel) != NULL)
    {        
        rc = IRLMP_LSAP_SEL_IN_USE;
    } 
    else if ((UINT)pMsg->IRDA_MSG_ConnDataLen > IRLMP_MAX_USER_DATA_LEN)
    {
        rc = IRLMP_USER_DATA_LEN_EXCEEDED;
    }    
    else if (CreateLsap(pIrlmpCb, &pLsapCb) != SUCCESS)
    {
        rc = 1;
    }    
    
    if (rc != SUCCESS)
        goto exit;


    // Initialize the LSAP endpoint
    pLsapCb->LocalLsapSel          = pMsg->IRDA_MSG_LocalLsapSel;
    pLsapCb->RemoteLsapSel         = pMsg->IRDA_MSG_RemoteLsapSel;
    pLsapCb->pTdiContext           = pMsg->IRDA_MSG_pContext;
    pLsapCb->UseTtp                = pMsg->IRDA_MSG_UseTtp;
    pLsapCb->RxMaxSDUSize          = pMsg->IRDA_MSG_MaxSDUSize;
    pLsapCb->AvailableCredit       = pMsg->IRDA_MSG_TtpCredits;
    pLsapCb->UserDataLen           = pMsg->IRDA_MSG_ConnDataLen;    
    RtlCopyMemory(pLsapCb->UserData, pMsg->IRDA_MSG_pConnData,
           pMsg->IRDA_MSG_ConnDataLen);
    
    // TDI can abort this connection before the confirm
    // from peer is received. TDI will call into LMP to
    // do this so we must return the Lsap context now.
    // This is the only time we actually return something
    // in an Irda Message.
    pMsg->IRDA_MSG_pContext = pLsapCb;

    DEBUGMSG((DBG_IRLMP | DBG_IRLMP_CONN), 
            (TEXT("IRLMP: IRLMP_CONNECT_REQ (l=%d,r=%d), Tdi:%X LinkState=%s\r\n"),
            pLsapCb->LocalLsapSel, pLsapCb->RemoteLsapSel, pLsapCb->pTdiContext,
            pIrlmpCb->LinkState == LINK_DISCONNECTED ? TEXT("DISCONNECTED") :
            pIrlmpCb->LinkState == LINK_IN_DISCOVERY ? TEXT("IN_DISCOVERY") :
            pIrlmpCb->LinkState == LINK_DISCONNECTING? TEXT("DISCONNECTING"):
            pIrlmpCb->LinkState == LINK_READY ? TEXT("READY") : TEXT("oh!")));

    switch (pIrlmpCb->LinkState)
    {
      case LINK_DISCONNECTED:
        RtlCopyMemory(pIrlmpCb->ConnDevAddr, pMsg->IRDA_MSG_RemoteDevAddr,
               IRDA_DEV_ADDR_LEN);

        pLsapCb->State = LSAP_CONN_REQ_PEND;
        SetupTtp(pLsapCb);

        // Initiate a connection on another thrd cuz this might be the rx thread
        ScheduleConnectReq(pIrlmpCb);
        break;

      case LINK_IN_DISCOVERY:
      case LINK_DISCONNECTING:
        if (pIrlmpCb->ConnDevAddrSet == FALSE)
        {
            // Ensure that only the first device to request a connection
            // sets the device address of the remote to be connected to.
            RtlCopyMemory(pIrlmpCb->ConnDevAddr, pMsg->IRDA_MSG_RemoteDevAddr,
                    IRDA_DEV_ADDR_LEN);
            pIrlmpCb->ConnDevAddrSet = TRUE;
        }
        else
        {
            DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Link in use!\r\n")));
            
        #ifdef UNDER_CE
            if (memcmp(pMsg->IRDA_MSG_RemoteDevAddr,
                       pIrlmpCb->ConnDevAddr,
                       IRDA_DEV_ADDR_LEN) != 0)
        #else
            if (RtlCompareMemory(pMsg->IRDA_MSG_RemoteDevAddr,
                                 pIrlmpCb->ConnDevAddr,
                                 IRDA_DEV_ADDR_LEN) != IRDA_DEV_ADDR_LEN)
        #endif // !UNDER_CE
            {
                // This LSAP is requesting a connection to another device
                CleanupLsap(pLsapCb);
                rc = IRLMP_LINK_IN_USE;
                break;
            }
        }

        pLsapCb->State = LSAP_CONN_REQ_PEND;
        SetupTtp(pLsapCb);

        // This request will complete when discovery/disconnect ends
        break;

      case LINK_CONNECTING:

    #ifdef UNDER_CE
        if (memcmp(pMsg->IRDA_MSG_RemoteDevAddr,
                   pIrlmpCb->ConnDevAddr,
                   IRDA_DEV_ADDR_LEN) != 0)
    #else
        if (RtlCompareMemory(pMsg->IRDA_MSG_RemoteDevAddr,
                             pIrlmpCb->ConnDevAddr,
                             IRDA_DEV_ADDR_LEN) != IRDA_DEV_ADDR_LEN)
    #endif // !UNDER_CE
        {
            DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Link in use!\r\n")));
            // This LSAP is requesting a connection to another device,
            // not the one IRLAP is currently connected to
            CleanupLsap(pLsapCb);
            rc = IRLMP_LINK_IN_USE;
            break;
        }

        // The LSAP will be notified when the IRLAP connection that is
        // underway has completed (see IRLAP_ConnectConf)
        pLsapCb->State = LSAP_IRLAP_CONN_PEND; 

        SetupTtp(pLsapCb);

        break;

      case LINK_READY:
    #ifdef UNDER_CE
        if (memcmp(pMsg->IRDA_MSG_RemoteDevAddr,
                   pIrlmpCb->ConnDevAddr,
                   IRDA_DEV_ADDR_LEN) != 0)
    #else
        if (RtlCompareMemory(pMsg->IRDA_MSG_RemoteDevAddr,
                             pIrlmpCb->ConnDevAddr,
                             IRDA_DEV_ADDR_LEN) != IRDA_DEV_ADDR_LEN)
    #endif // !UNDER_CE
        {
            DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Link in use!\r\n")));
            // This LSAP is requesting a connection to another device
            CleanupLsap(pLsapCb);
            rc = IRLMP_LINK_IN_USE;
            break;
        }
        IrdaTimerRestart(&pLsapCb->ResponseTimer);

        pLsapCb->State = LSAP_LMCONN_CONF_PEND;
        SetupTtp(pLsapCb);

        // Ask remote LSAP for a connection
        SendCntlPdu(pLsapCb, IRLMP_CONNECT_PDU,
                    IRLMP_ABIT_REQUEST, IRLMP_RSVD_PARM, 0);
        break;
    }
    
exit:

    UNLOCK_LINK(pIrlmpCb->pIrdaLinkCb);    

    return rc;
}
/*****************************************************************************
*
*   @func   void | SetupTtp | if using TTP, calculate initial credits
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRLMP_LSAP_CB * | pLsapCb | pointer LSAP control block
*/
void
SetupTtp(IRLMP_LSAP_CB *pLsapCb)
{
    VALIDLSAP(pLsapCb);

    if (pLsapCb->AvailableCredit > 127)
    {
        pLsapCb->RemoteTxCredit = 127;
        pLsapCb->AvailableCredit -= 127;
    }
    else
    {
        pLsapCb->RemoteTxCredit = pLsapCb->AvailableCredit;
        pLsapCb->AvailableCredit = 0;
    }
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: RemoteTxCredit %d\n"),
                         pLsapCb->RemoteTxCredit));
}
/*****************************************************************************
*
*   @func   UINT | IrlmpConnectResp | Process IRLMP connect response
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRLMP_LSAP_CB * | pLsapCb | pointer LSAP control block
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*/
UINT
IrlmpConnectResp(IRLMP_LSAP_CB *pLsapCb, IRDA_MSG *pMsg)
{
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Connect response\n")));
    
    if (pLsapCb->pIrlmpCb->LinkState != LINK_READY)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: Bad link state\n")));
        ASSERT(0);
        return IRLMP_LINK_BAD_STATE;
    }

    if (pLsapCb->State != LSAP_CONN_RESP_PEND)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: Bad LSAP state\n")));
        ASSERT(0);
        return IRLMP_LSAP_BAD_STATE;
    }

    IrdaTimerStop(&pLsapCb->ResponseTimer);

    if (pMsg->IRDA_MSG_ConnDataLen > IRLMP_MAX_USER_DATA_LEN)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: User data len exceeded\n")));
        return IRLMP_USER_DATA_LEN_EXCEEDED;
    }

    pLsapCb->RxMaxSDUSize       = pMsg->IRDA_MSG_MaxSDUSize;
    pLsapCb->UserDataLen        = pMsg->IRDA_MSG_ConnDataLen;
    pLsapCb->AvailableCredit    = pMsg->IRDA_MSG_TtpCredits;
    RtlCopyMemory(pLsapCb->UserData, pMsg->IRDA_MSG_pConnData,
           pMsg->IRDA_MSG_ConnDataLen);

    pLsapCb->pTdiContext = pMsg->IRDA_MSG_pContext;

    SetupTtp(pLsapCb);

    pLsapCb->State = LSAP_READY;

    SendCntlPdu(pLsapCb, IRLMP_CONNECT_PDU, IRLMP_ABIT_CONFIRM,
                IRLMP_RSVD_PARM, 0);

    return SUCCESS;
}
/*****************************************************************************
*
*   @func   UINT | IrlmpDisconnectReq | Process IRLMP disconnect request
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRLMP_LSAP_CB * | pLsapCb | pointer LSAP control block
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*/
UINT
IrlmpDisconnectReq(IRLMP_LSAP_CB *pLsapCb, IRDA_MSG *pMsg)
{
    //
    // If the peer initiated a disconnect while TDI was requesting a disconnect,
    // then the IRLMP_LSAP_CB gets destroyed.
    //
    if (pLsapCb == NULL) {
        return IRLMP_INVALID_LSAP_CB;
    }

    DEBUGMSG((DBG_IRLMP | DBG_IRLMP_CONN), 
            (TEXT("IRLMP: IRLMP_DISCONNECT_REQ (l=%d,r=%d)\r\n"),
             pLsapCb->LocalLsapSel, pLsapCb->RemoteLsapSel));
    
    if (pLsapCb->State == LSAP_DISCONNECTED)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: Disconnect in bad state\n")));
        return IRLMP_LSAP_BAD_STATE;
    }

    if (pLsapCb->State == LSAP_LMCONN_CONF_PEND ||
        pLsapCb->State == LSAP_CONN_RESP_PEND)
    {
        IrdaTimerStop(&pLsapCb->ResponseTimer);
    }

    if (pLsapCb->State == LSAP_CONN_RESP_PEND || pLsapCb->State >= LSAP_READY)
    {
        // Either the LSAP is connected or the peer is waiting for a
        // response from our client

        if (pMsg->IRDA_MSG_DiscDataLen > IRLMP_MAX_USER_DATA_LEN)
        {
            DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: User data len exceeded\n")));
            return IRLMP_USER_DATA_LEN_EXCEEDED;
        }

        pLsapCb->UserDataLen = pMsg->IRDA_MSG_DiscDataLen;
        RtlCopyMemory(pLsapCb->UserData, pMsg->IRDA_MSG_pDiscData,
               pMsg->IRDA_MSG_DiscDataLen);

        // Notify peer of the disconnect request, reason: user request
        // Send on different thread in case TranportAPI calls this on rx thread
        SendCntlPdu(pLsapCb,IRLMP_DISCONNECT_PDU,IRLMP_ABIT_REQUEST,
                         IRLMP_USER_REQUEST, 0);
    }

    IrdaTimerRestart(&pLsapCb->pIrlmpCb->DiscDelayTimer);

    CleanupLsap(pLsapCb);
    
    return 0;
}
/*****************************************************************************
*
*   @func   UINT | IrlmpDataReqExclusive | Process IRLMP data request
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRLMP_LSAP_CB * | pLsapCb | pointer LSAP control block
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*
*   WinCE can pass down multiple NDIS bufs. We need to segment -
*             just call IrlmpDataReqMultiplexed.
*/
UINT
IrlmpDataReqExclusive(IRLMP_LSAP_CB *pLsapCb, IRDA_MSG *pMsg)
{
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Exclusive mode data request\n")));

    if (pLsapCb->pIrlmpCb->LinkState != LINK_READY)
    {
        return IRLMP_LINK_BAD_STATE;
    }

    if (pLsapCb != pLsapCb->pIrlmpCb->pExclLsapCb)
    {
        return IRLMP_INVALID_LSAP_CB;
    }
    
    return (IrlmpDataReqMultiplexed(pLsapCb, pMsg));
}
/*****************************************************************************
*
*   @func   UINT | IrlmpDataReqMultiplexed | Process IRLMP data request
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRLMP_LSAP_CB * | pLsapCb | pointer LSAP control block
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*/
UINT
IrlmpDataReqMultiplexed(IRLMP_LSAP_CB *pLsapCb, IRDA_MSG *pMsg)
{
    NDIS_BUFFER         *pNBuf = (NDIS_BUFFER *) pMsg->DataContext;
    NDIS_BUFFER         *pNextNBuf;
    UCHAR                *pData;
    int                 DataLen;
    int                 SegLen;
    IRDA_MSG            *pSegMsg;

    if (pLsapCb->State < LSAP_READY)
    {
        return IRLMP_LSAP_BAD_STATE;
    }
    
    // Place this message on the LSAP's TxMsgList. The message remains
    // here until all segments for it are sent and confirmed
    InsertTailList(&pLsapCb->TxMsgList, &pMsg->Linkage);

    pMsg->IRDA_MSG_SegCount = 0;
    // If it fails, this will be changed
    pMsg->IRDA_MSG_DataStatus = IRLMP_DATA_REQUEST_COMPLETED;

    // Segment the message into PDUs. The segment will either be:
    //  1. Sent immediately to IRLAP if the link is not busy
    //  2. If link is busy, placed on TxMsgList contained in IRLMP_LCB
    //  3. If no credit, placed onto this LSAPS SegTxMsgList

    while (pNBuf != NULL)
    {
        NdisQueryBuffer(pNBuf, &pData, &DataLen);
        
         // Get the next one now so I know when to set SegFlag to final
        NdisGetNextBuffer(pNBuf, &pNextNBuf);

        while (DataLen != 0)
        {
            if ((pSegMsg = AllocIrdaBuf(IrdaMsgPool))
                == NULL)
            {
                ASSERT(0);
                return IRLMP_ALLOC_FAILED;
            }
            pSegMsg->IRDA_MSG_pOwner = pLsapCb; // MUX routing
            pSegMsg->DataContext = pMsg; // Parent of segment
            pSegMsg->IRDA_MSG_IrCOMM_9Wire = pMsg->IRDA_MSG_IrCOMM_9Wire;

            // Increment segment count contained in original messages
            pMsg->IRDA_MSG_SegCount++;
            
            if (DataLen > pLsapCb->pIrlmpCb->MaxPDUSize)
            {
                SegLen = pLsapCb->pIrlmpCb->MaxPDUSize;
            }
            else
            {
                SegLen = DataLen;
            }
            
            DEBUGMSG(DBG_IRLMP, (TEXT("IRMLP: Sending SegLen %d\n"),
                                  SegLen));
            
            pSegMsg->IRDA_MSG_pRead = pData;
            pSegMsg->IRDA_MSG_pWrite = pData + SegLen;

            // Indicate this message is part of a segmented message
            pSegMsg->IRDA_MSG_SegCount = pMsg->IRDA_MSG_SegCount;
            
            pData += SegLen;
            DataLen -= SegLen;
            
            if (DataLen == 0 && pNextNBuf == NULL)
            {
                pSegMsg->IRDA_MSG_SegFlags = SEG_FINAL;
            }
            else
            {
                pSegMsg->IRDA_MSG_SegFlags = 0;
            }

            if (pLsapCb->State == LSAP_NO_TX_CREDIT)
            {
                DEBUGMSG(DBG_IRLMP, 
                        (TEXT("IRLMP: Out of credit, placing on SegList\n")));
                InsertTailList(&pLsapCb->SegTxMsgList, &pSegMsg->Linkage);
            }
            else
            {
                FormatAndSendDataReq(pLsapCb, pSegMsg, FALSE);
            }
        }
        pNBuf = pNextNBuf;
    }

    return SUCCESS;
}

VOID
FormatAndSendDataReq(IRLMP_LSAP_CB *pLsapCb, IRDA_MSG *pMsg,
                     BOOLEAN LocallyGenerated)
{
    IRLMP_HEADER        *pLMHeader;
    TTP_DATA_HEADER     *pTTPHeader;
    int                 AdditionalCredit;
    UCHAR                *pLastHdrByte;

    VALIDLSAP(pLsapCb);

    // Initialize the header pointers to the end of the header block
    pMsg->IRDA_MSG_pHdrRead  =
    pMsg->IRDA_MSG_pHdrWrite = pMsg->IRDA_MSG_Header + IRDA_HEADER_LEN;

    // Back up the read pointer for the LMP header
    pMsg->IRDA_MSG_pHdrRead -= sizeof(IRLMP_HEADER);

    // Back up header read pointer for TTP
    if (pLsapCb->UseTtp)
    {
        pMsg->IRDA_MSG_pHdrRead -= (sizeof(TTP_DATA_HEADER));
    }

    
    if (pMsg->IRDA_MSG_IrCOMM_9Wire == TRUE)
    {
        pMsg->IRDA_MSG_pHdrRead -= 1;
    }

    ASSERT(pMsg->IRDA_MSG_pHdrRead >= pMsg->IRDA_MSG_Header);

    // Build the LMP Header
    pLMHeader = (IRLMP_HEADER *) pMsg->IRDA_MSG_pHdrRead;
    pLMHeader->DstLsapSel = pLsapCb->RemoteLsapSel;
    pLMHeader->SrcLsapSel = pLsapCb->LocalLsapSel;
    pLMHeader->CntlBit = IRLMP_DATA_PDU;
    pLMHeader->RsvrdBit = 0;

    pLastHdrByte = (UCHAR *) (pLMHeader + 1);
    
    // Build the TTP Header
    if (pLsapCb->UseTtp)
    {
        pTTPHeader = (TTP_DATA_HEADER *) (pLMHeader + 1);

        // Credit
        if (pLsapCb->AvailableCredit > 127)
        {
            AdditionalCredit = 127;
            pLsapCb->AvailableCredit -= 127;
        }
        else
        {
            AdditionalCredit = pLsapCb->AvailableCredit;
            pLsapCb->AvailableCredit = 0;
        }

        pTTPHeader->AdditionalCredit = AdditionalCredit;
        pLsapCb->RemoteTxCredit += AdditionalCredit;

        if (pMsg->IRDA_MSG_pRead != pMsg->IRDA_MSG_pWrite)
        {
            // Only decrement my TxCredit if I'm sending data
            // (may be sending dataless PDU to extend credit to peer)
            pLsapCb->LocalTxCredit -= 1;
        
            if (pLsapCb->LocalTxCredit == 0)
            {
                DEBUGMSG(DBG_IRLMP, 
                         (TEXT("IRLMP: l%d,r%d No credit\n"), pLsapCb->LocalLsapSel,
                          pLsapCb->RemoteLsapSel));
                pLsapCb->State = LSAP_NO_TX_CREDIT;
            }
        }
        
        // SAR
        if (pMsg->IRDA_MSG_SegFlags & SEG_FINAL)
        {
            pTTPHeader->MoreBit = TTP_MBIT_FINAL;
        }
        else
        {
            pTTPHeader->MoreBit = TTP_MBIT_NOT_FINAL;
        }

        pLastHdrByte = (UCHAR *) (pTTPHeader + 1);
    }
    
    
    if (pMsg->IRDA_MSG_IrCOMM_9Wire == TRUE)
    {
        *pLastHdrByte = 0;
    }

    pMsg->Prim = IRLAP_DATA_REQ;
    if (LocallyGenerated)
    {
        pMsg->IRDA_MSG_SegFlags = SEG_LOCAL | SEG_FINAL;
        pMsg->IRDA_MSG_pOwner = 0;
    }
    else
    {
        pMsg->IRDA_MSG_pOwner = pLsapCb;
        REFADD(&pLsapCb->RefCnt, 'ATAD');
    }

    
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Sending Data request pMsg:%X LsapCb:%X\n"),
             pMsg, pMsg->IRDA_MSG_pOwner));
    IrlapDown(pLsapCb->pIrlmpCb->pIrdaLinkCb->IrlapContext, pMsg);
    
    DEBUGMSG(DBG_IRLMP_CRED,
             (TEXT("IRLMP(l%d,r%d): Tx LocTxCredit %d,RemoteTxCredit %d\n"),
              pLsapCb->LocalLsapSel, pLsapCb->RemoteLsapSel,
              pLsapCb->LocalTxCredit, pLsapCb->RemoteTxCredit));
}
/*****************************************************************************
*
*   @func   UINT | IrlmpAccessModeReq | Processes the IRLMP_ACCESSMODE_REQ
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRLMP_LSAP_CB * | pLsapCb | pointer LSAP control block
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*/
UINT
IrlmpAccessModeReq(IRLMP_LSAP_CB *pRequestingLsapCb, IRDA_MSG *pMsg)
{
    IRLMP_LSAP_CB   *pLsapCb;
    PIRLMP_LINK_CB  pIrlmpCb = pRequestingLsapCb->pIrlmpCb;
    
    if (pIrlmpCb->LinkState != LINK_READY)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: Link bad state %x\n"), 
                              pIrlmpCb->LinkState));        
        return IRLMP_LINK_BAD_STATE;
    }
    if (pRequestingLsapCb->State != LSAP_READY)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: LSAP bad state %x\n"), 
                              pRequestingLsapCb->State));        
        return IRLMP_LSAP_BAD_STATE;
    }
    switch (pMsg->IRDA_MSG_AccessMode)
    {
      case IRLMP_EXCLUSIVE:
        if (pIrlmpCb->pExclLsapCb != NULL)
        {
            // Another LSAP has it already
            return IRLMP_IN_EXCLUSIVE_MODE;
        }
        for (pLsapCb = (IRLMP_LSAP_CB *) pIrlmpCb->LsapCbList.Flink;
             (LIST_ENTRY *) pLsapCb != &pIrlmpCb->LsapCbList;
             pLsapCb = (IRLMP_LSAP_CB *) pLsapCb->Linkage.Flink)
        {
            VALIDLSAP(pLsapCb);

            if (pLsapCb->State != LSAP_DISCONNECTED && 
                pLsapCb != pRequestingLsapCb)
            {
                return IRLMP_IN_MULTIPLEXED_MODE;
            }
        }
        
        // OK to request exclusive mode from peer
        pIrlmpCb->pExclLsapCb = pRequestingLsapCb;  
        
        if (pMsg->IRDA_MSG_IrLPTMode == TRUE)
        {
            pMsg->Prim = IRLMP_ACCESSMODE_CONF;
            pMsg->IRDA_MSG_AccessMode = IRLMP_EXCLUSIVE;
            pMsg->IRDA_MSG_ModeStatus = IRLMP_ACCESSMODE_SUCCESS;
            
            TdiUp(pRequestingLsapCb->pTdiContext, pMsg);
            return SUCCESS;
        }
        else
        {
            pRequestingLsapCb->State = LSAP_EXCLUSIVEMODE_PEND;

            SendCntlPdu(pRequestingLsapCb, IRLMP_ACCESSMODE_PDU,
                        IRLMP_ABIT_REQUEST, IRLMP_RSVD_PARM,
                        IRLMP_EXCLUSIVE);

            IrdaTimerRestart(&pRequestingLsapCb->ResponseTimer);

        }        
        break;
        
      case IRLMP_MULTIPLEXED:
        if (pIrlmpCb->pExclLsapCb == NULL)
        {
            return IRLMP_IN_MULTIPLEXED_MODE;
        }
        if (pIrlmpCb->pExclLsapCb != pRequestingLsapCb)
        {
            return IRLMP_NOT_LSAP_IN_EXCLUSIVE_MODE;
        }

        if (pMsg->IRDA_MSG_IrLPTMode == TRUE)
        {
            pIrlmpCb->pExclLsapCb = NULL;
            pMsg->Prim = IRLMP_ACCESSMODE_CONF;
            pMsg->IRDA_MSG_AccessMode = IRLMP_MULTIPLEXED;
            pMsg->IRDA_MSG_ModeStatus = IRLMP_ACCESSMODE_SUCCESS;
            return TdiUp(pRequestingLsapCb->pTdiContext,
                                   pMsg);
        }
        else
        {
            pRequestingLsapCb->State = LSAP_MULTIPLEXEDMODE_PEND;
        
            SendCntlPdu(pRequestingLsapCb, IRLMP_ACCESSMODE_PDU,
                        IRLMP_ABIT_REQUEST, IRLMP_RSVD_PARM,
                        IRLMP_MULTIPLEXED);     

            IrdaTimerRestart(&pRequestingLsapCb->ResponseTimer);            
        }
        break;
      default:
        return IRLMP_BAD_ACCESSMODE;
    }
    return SUCCESS;
}
/*****************************************************************************
*
*   @func   UINT | SendCntlPdu | Sends connect request to IRLAP
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRLMP_LSAP_CB * | pLsapCb | pointer LSAP control block
*/
VOID
SendCntlPdu(IRLMP_LSAP_CB *pLsapCb, int OpCode, int ABit,
            int Parm1, int Parm2)
{
    IRDA_MSG            *pMsg = AllocIrdaBuf(IrdaMsgPool);
    IRLMP_HEADER        *pLMHeader;
    IRLMP_CNTL_FORMAT   *pCntlFormat;
    TTP_CONN_HEADER     *pTTPHeader;
    TTP_CONN_PARM       *pTTPParm;
    UINT                rc = SUCCESS;
    
    VALIDLSAP(pLsapCb);

    if (pMsg == NULL)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: Alloc failed\n")));
        ASSERT(0);
        return;// IRLMP_ALLOC_FAILED;
    }

    pMsg->IRDA_MSG_SegFlags = SEG_LOCAL | SEG_FINAL;

    // Initialize the header pointers to the end of the header block
    pMsg->IRDA_MSG_pHdrRead =
    pMsg->IRDA_MSG_pHdrWrite = pMsg->IRDA_MSG_Header + IRDA_HEADER_LEN;

    // Back up the read pointer for the LMP header
    pMsg->IRDA_MSG_pHdrRead -= (sizeof(IRLMP_HEADER) + \
                             sizeof(IRLMP_CNTL_FORMAT));
    
    // move it forward for non access mode control requests
    // (connect and disconnect don't have a Parm2)
    if (OpCode != IRLMP_ACCESSMODE_PDU)
    {
        pMsg->IRDA_MSG_pHdrRead++;
    }

    // If using Tiny TPP back up the read pointer for its header
    // From LMPs point of view this is where the user data begins.
    // We are sticking it in the header because TTP is now part of IRLMP.

    // TTP connect PDU's are only used for connection establishment
    if (pLsapCb->UseTtp && OpCode == IRLMP_CONNECT_PDU)
    {
        pMsg->IRDA_MSG_pHdrRead -= sizeof(TTP_CONN_HEADER);

        if (pLsapCb->RxMaxSDUSize > 0)
        {
            pMsg->IRDA_MSG_pHdrRead -= sizeof(TTP_CONN_PARM);
        }
    }

    // Build the IRLMP header
    pLMHeader = (IRLMP_HEADER *) pMsg->IRDA_MSG_pHdrRead;
    pLMHeader->DstLsapSel = pLsapCb->RemoteLsapSel;
    pLMHeader->SrcLsapSel = pLsapCb->LocalLsapSel;
    pLMHeader->CntlBit = IRLMP_CNTL_PDU;
    pLMHeader->RsvrdBit = 0;
    // Control portion of header
    pCntlFormat = (IRLMP_CNTL_FORMAT *) (pLMHeader + 1);
    pCntlFormat->OpCode = OpCode; 
    pCntlFormat->ABit = ABit;
    pCntlFormat->Parm1 = Parm1;
    if (OpCode == IRLMP_ACCESSMODE_PDU)
    {
        pCntlFormat->Parm2 = Parm2; // Access mode
    }
    
    // Build the TTP header if needed (we are using TTP and this is a
    // connection request or confirmation not).
    if (pLsapCb->UseTtp && OpCode == IRLMP_CONNECT_PDU)
    {
        // Always using the MaxSDUSize parameter. If the client wishes
        // to disable, MaxSDUSize = 0

        pTTPHeader = (TTP_CONN_HEADER *) (pCntlFormat + 1) - 1;
        // -1, LM-Connect-PDU doesn't use parm2

        /*
           #define TTP_PFLAG_NO_PARMS      0
           #define TTP_PFLAG_PARMS         1
        */

        pTTPHeader->ParmFlag = (pLsapCb->RxMaxSDUSize > 0);
        
        pTTPHeader->InitialCredit = pLsapCb->RemoteTxCredit;
        
        pTTPParm = (TTP_CONN_PARM *) (pTTPHeader + 1);

        if (pLsapCb->RxMaxSDUSize > 0)
        {
            // HARDCODE-O-RAMA
            pTTPParm->PLen = 6;
            pTTPParm->PI = TTP_MAX_SDU_SIZE_PI;
            pTTPParm->PL = TTP_MAX_SDU_SIZE_PL;
            pTTPParm->PV[3] = (UCHAR) (pLsapCb->RxMaxSDUSize & 0xFF);
            pTTPParm->PV[2] = (UCHAR) ((pLsapCb->RxMaxSDUSize & 0xFF00)
                                      >> 8);
            pTTPParm->PV[1] = (UCHAR) ((pLsapCb->RxMaxSDUSize & 0xFF0000)
                                      >> 16);
            pTTPParm->PV[0] = (UCHAR) ((pLsapCb->RxMaxSDUSize & 0xFF000000)
                                      >> 24);
        }
        
    }

    // Client connection data, Access mode does not include client data
    if (pLsapCb->UserDataLen == 0 || OpCode == IRLMP_ACCESSMODE_PDU) 
    {
        pMsg->IRDA_MSG_pBase =
        pMsg->IRDA_MSG_pRead =
        pMsg->IRDA_MSG_pWrite =
        pMsg->IRDA_MSG_pLimit = NULL;
    }
    else
    {
        pMsg->IRDA_MSG_pBase = pLsapCb->UserData;
        pMsg->IRDA_MSG_pRead = pLsapCb->UserData;
        pMsg->IRDA_MSG_pWrite = pLsapCb->UserData + pLsapCb->UserDataLen;
        pMsg->IRDA_MSG_pLimit = pMsg->IRDA_MSG_pWrite;
    }

    // Message built, send data request to IRLAP
    pMsg->Prim = IRLAP_DATA_REQ;

    pMsg->IRDA_MSG_pOwner = 0;

    DEBUGMSG(DBG_IRLMP,(TEXT("IRLMP: Sending LM_%s_%s for l=%d,r=%d pMsg:%X LsapCb:%X\n"), 
                         (OpCode == IRLMP_CONNECT_PDU ? TEXT("CONNECT") :
                         OpCode == IRLMP_DISCONNECT_PDU ? TEXT("DISCONNECT") :
                         OpCode == IRLMP_ACCESSMODE_PDU ? TEXT("ACCESSMODE") :
                         TEXT("!!oops!!")), 
                         (ABit==IRLMP_ABIT_REQUEST?TEXT("REQ"):TEXT("CONF")),
                         pLsapCb->LocalLsapSel,
                         pLsapCb->RemoteLsapSel,
                         pMsg, pLsapCb));

    IrlapDown(pLsapCb->pIrlmpCb->pIrdaLinkCb->IrlapContext, pMsg);
}
/*****************************************************************************
*
*   @func   UINT | LsapResponseTimerExp | Timer expiration callback
*
*   @rdesc  SUCCESS or an error code
*
*/
VOID
LsapResponseTimerExp(PVOID Context)
{
    IRLMP_LSAP_CB   *pLsapCb = (IRLMP_LSAP_CB *) Context;
    IRDA_MSG        IMsg;
    UINT            rc = SUCCESS;
    PIRLMP_LINK_CB  pIrlmpCb = pLsapCb->pIrlmpCb;
    
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: LSAP timer expired\n")));

    VALIDLSAP(pLsapCb);

    switch (pLsapCb->State)
    {
      case LSAP_LMCONN_CONF_PEND:
        if (pLsapCb->LocalLsapSel == IAS_LSAP_SEL)
        {
            IasServerDisconnectReq(pLsapCb);
            break;
        }
        
        if (pLsapCb->LocalLsapSel == IAS_LOCAL_LSAP_SEL && 
            pLsapCb->RemoteLsapSel == IAS_LSAP_SEL)
        {
            IasClientDisconnectReq(pLsapCb, IRLMP_NO_RESPONSE_LSAP) ;
        }
        else
        {
            // Tell upper layer the connect request failed
            IMsg.Prim = IRLMP_DISCONNECT_IND;
            IMsg.IRDA_MSG_DiscReason = IRLMP_NO_RESPONSE_LSAP;
            TdiUp(pLsapCb->pTdiContext, &IMsg);
            IrdaTimerRestart(&pIrlmpCb->DiscDelayTimer);
            CleanupLsap(pLsapCb);
        }
        break;

      case LSAP_CONN_RESP_PEND:
        pLsapCb->UserDataLen = 0; // This will ensure no client data sent in
                                   // Disconnect PDU below

        // Tell remote LSAP that its peer did not respond
        SendCntlPdu(pLsapCb,IRLMP_DISCONNECT_PDU,IRLMP_ABIT_REQUEST,
                    IRLMP_NO_RESPONSE_LSAP, 0);
        IrdaTimerRestart(&pIrlmpCb->DiscDelayTimer);

        CleanupLsap(pLsapCb);
        break;

      case LSAP_MULTIPLEXEDMODE_PEND:
        // Spec says peer can't refuse request to return to multiplex mode
        // but if no answer, go into multiplexed anyway?
      case LSAP_EXCLUSIVEMODE_PEND:
        pIrlmpCb->pExclLsapCb = NULL;
        // Peer didn't respond, maybe we are not connected anymore ???

        pLsapCb->State = LSAP_READY;
        
        IMsg.Prim = IRLMP_ACCESSMODE_CONF;
        IMsg.IRDA_MSG_AccessMode = IRLMP_MULTIPLEXED;
        IMsg.IRDA_MSG_ModeStatus = IRLMP_ACCESSMODE_FAILURE;
        TdiUp(pLsapCb->pTdiContext, &IMsg);
        break;
         
      default:
        DEBUGMSG(DBG_IRLMP, (TEXT("Ignoring timer expiry in this state\n")));
        ; // ignore
    }
}
/*****************************************************************************
*
*   @func   UINT | IrlmpUp | Bottom of IRLMP, called by IRLAP with
*                             IRLAP messages. This is the MUX
*
*   @rdesc  SUCCESS or an error code
*
*/
UINT
IrlmpUp(PIRDA_LINK_CB pIrdaLinkCb, IRDA_MSG *pMsg)
{
    PIRLMP_LINK_CB  pIrlmpCb;

    if (NULL == pIrdaLinkCb) {
        return IRLMP_LINK_BAD_STATE;
    }

    pIrlmpCb = (PIRLMP_LINK_CB) pIrdaLinkCb->IrlmpContext;

    switch (pMsg->Prim)
    {
      case IRLAP_DISCOVERY_IND:
        DEBUGMSG(ZONE_DSCV, (TEXT("IRLAP_DISCOVERY_IND\r\n")));  
        UpdateDeviceList(pIrdaLinkCb, pMsg->IRDA_MSG_pDevList);
        pMsg->Prim = IRLMP_DISCOVERY_IND;
        pMsg->IRDA_MSG_pDevList = &DeviceList;
        TdiUp(NULL, pMsg);
        return SUCCESS;

      case IRLAP_DISCOVERY_CONF:
        IrlapDiscoveryConf(pIrlmpCb, pMsg);
        return SUCCESS;

      case IRLAP_CONNECT_IND:
        IrlapConnectInd(pIrlmpCb, pMsg);
        return SUCCESS;

      case IRLAP_CONNECT_CONF:
        IrlapConnectConf(pIrlmpCb, pMsg);
        return SUCCESS;

      case IRLAP_DISCONNECT_IND:
        IrlapDisconnectInd(pIrlmpCb, pMsg);
        return SUCCESS;

      case IRLAP_DATA_CONF:
        IrlapDataConf(pMsg);
        return SUCCESS;

      case IRLAP_DATA_IND:
        IrlapDataInd(pIrlmpCb, pMsg);
        if (pIrlmpCb->pExclLsapCb &&
            pIrlmpCb->pExclLsapCb->RemoteTxCredit <=0)
            return IRLMP_LOCAL_BUSY;
        else
            return SUCCESS;

      case IRLAP_UDATA_IND:
        ASSERT(0);
        return SUCCESS;
        
      case IRLAP_STATUS_IND:
        TdiUp(NULL, pMsg);
        return SUCCESS;
    }
    return (SUCCESS);
}

/*****************************************************************************
*
*   @func   UINT | IrlapDiscoveryConf | Process the discovery confirm
*/
VOID
IrlapDiscoveryConf(PIRLMP_LINK_CB pIrlmpCb, IRDA_MSG *pMsg)
{
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: IRLAP_DISCOVERY_CONF\n")));
    
    if (pIrlmpCb->LinkState != LINK_IN_DISCOVERY)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: Link bad state\n")));

        ASSERT(0);
        
        return;// IRLMP_LINK_BAD_STATE;
    }

    pIrlmpCb->LinkState = LINK_DISCONNECTED;

    if (pMsg->IRDA_MSG_DscvStatus == IRLAP_DISCOVERY_COMPLETED)
    {
        DEBUGMSG(ZONE_DSCV, (TEXT("IRLAP_DISCOVERY_CONF\r\n")));
        UpdateDeviceList(pIrlmpCb->pIrdaLinkCb, pMsg->IRDA_MSG_pDevList);
    }
    
    // Initiate discovery on next link
    IrdaEventSchedule(&EvDiscoveryReq, NULL);

    // Initiate a connection if one was requested while in discovery
    ScheduleConnectReq(pIrlmpCb);    
}
/*****************************************************************************
*
*   @func   void | UpdateDeviceList | Determines if new devices need to be
*                                  added or old ones removed from the device
*                                  list maintained by IRLMP
*
*   @parm   LIST_ENTRY * | pDevList | pointer to a list of devices
*/
void
UpdateDeviceList(PIRDA_LINK_CB pIrdaLinkCb, LIST_ENTRY *pNewDevList)
{
    IRDA_DEVICE     *pNewDevice;
    IRDA_DEVICE     *pOldDevice;
    IRDA_DEVICE     *pDevice;
    BOOLEAN         DeviceInList;
    
    DEBUGMSG(ZONE_DSCV, (TEXT("+UpdateDeviceList(0x%x, 0x%x)\r\n"),
                         pIrdaLinkCb, pNewDevList));
    
    NdisAcquireSpinLock(&SpinLock);
    
    // Add new devices, set not seen count to zero if devices is
    // seen again
    for (pNewDevice = (IRDA_DEVICE *) pNewDevList->Flink;
         (LIST_ENTRY *) pNewDevice != pNewDevList;
         pNewDevice = (IRDA_DEVICE *) pNewDevice->Linkage.Flink)
    {
        DeviceInList = FALSE;

        for (pOldDevice = (IRDA_DEVICE *) DeviceList.Flink;
             (LIST_ENTRY *) pOldDevice != &DeviceList;
             pOldDevice = (IRDA_DEVICE *) pOldDevice->Linkage.Flink)
        {
            if (pNewDevice->DscvInfoLen == pOldDevice->DscvInfoLen &&
            #ifdef UNDER_CE
                memcmp(pNewDevice->DevAddr, pOldDevice->DevAddr,
                       IRDA_DEV_ADDR_LEN) == 0 &&
                memcmp(pNewDevice->DscvInfo, pOldDevice->DscvInfo,
                       pNewDevice->DscvInfoLen) == 0)
            #else
                RtlCompareMemory(pNewDevice->DevAddr, pOldDevice->DevAddr,
                                 IRDA_DEV_ADDR_LEN) == IRDA_DEV_ADDR_LEN &&
                RtlCompareMemory(pNewDevice->DscvInfo, pOldDevice->DscvInfo,
                                 pNewDevice->DscvInfoLen) ==
                                (ULONG) pNewDevice->DscvInfoLen)
            #endif // !UNDER_CE
            {
                DeviceInList = TRUE;
                pOldDevice->NotSeenCnt = -1; // reset not seen count
                                             // will be ++'d to 0 below
                pOldDevice->LinkContext = pIrdaLinkCb;
                break;
            }
        }
        if (!DeviceInList)
        {
            // Create an new entry in the list maintained by IRLMP
            IRDA_ALLOC_MEM(pDevice, sizeof(IRDA_DEVICE), MT_IRLMP_DEVICE);
            if (pDevice != NULL) {
                RtlCopyMemory(pDevice, pNewDevice, sizeof(IRDA_DEVICE));
                pDevice->NotSeenCnt = -1; // will be ++'d to 0 below
                pDevice->LinkContext = pIrdaLinkCb;
                InsertHeadList(&DeviceList, &pDevice->Linkage);
            } else {
                DEBUGMSG(ZONE_DSCV, (TEXT("-UpdateDeviceList: alloc failed!\r\n")));
                NdisReleaseSpinLock(&SpinLock);
                return;
            }
        }
    }

    // Run through the list and remove devices that haven't
    // been seen for awhile

    pOldDevice = (IRDA_DEVICE *) DeviceList.Flink;

    while ((LIST_ENTRY *) pOldDevice != &DeviceList)
    {
        pDevice = (IRDA_DEVICE *) pOldDevice->Linkage.Flink;

        DEBUGMSG(ZONE_DSCV, 
            (TEXT("    DevAddr: 0x%02x%02x%02x%02x NotSeenCnt: %d\r\n"),
            pOldDevice->DevAddr[0], pOldDevice->DevAddr[1],
            pOldDevice->DevAddr[2], pOldDevice->DevAddr[3],
            pOldDevice->NotSeenCnt));

        if (++(pOldDevice->NotSeenCnt) >= IRLMP_NOT_SEEN_THRESHOLD)
        {
            RemoveEntryList(&pOldDevice->Linkage);
            IRDA_FREE_MEM(pOldDevice);
        }
        pOldDevice = pDevice; // next
    }
    NdisReleaseSpinLock(&SpinLock);

    DEBUGMSG(ZONE_DSCV, (TEXT("-UpdateDeviceList\r\n")));
}
/*****************************************************************************
*
*   @func   UINT | IrlapConnectInd | Process the connect indication from LAP
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*/
VOID
IrlapConnectInd(PIRLMP_LINK_CB pIrlmpCb, IRDA_MSG *pMsg)
{
    if (pIrlmpCb->LinkState != LINK_DISCONNECTED)
    {
        ASSERT(0);
        return;
    }
    
    RtlCopyMemory(pIrlmpCb->ConnDevAddr, pMsg->IRDA_MSG_RemoteDevAddr,
           IRDA_DEV_ADDR_LEN);
    RtlCopyMemory(&pIrlmpCb->NegotiatedQOS, pMsg->IRDA_MSG_pQos,
           sizeof(IRDA_QOS_PARMS));
    pIrlmpCb->MaxPDUSize = IrlapGetQosParmVal(vDataSizeTable,
                               pMsg->IRDA_MSG_pQos->bfDataSize, NULL)
                             - sizeof(IRLMP_HEADER) 
                             - sizeof(TTP_DATA_HEADER);

    pIrlmpCb->WindowSize = IrlapGetQosParmVal(vWinSizeTable,
                              pMsg->IRDA_MSG_pQos->bfWindowSize, NULL);

    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Connect indication, MaxPDU = %d\n"),
                          pIrlmpCb->MaxPDUSize));
    
    // schedule the response to occur on a differnt thread
    pIrlmpCb->AcceptConnection = TRUE;            
    IrdaEventSchedule(&EvConnectResp, pIrlmpCb->pIrdaLinkCb);
}
/*****************************************************************************
*
*   @func   UINT | IrlapConnectConf | Processes the connect confirm
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*/
VOID
IrlapConnectConf(PIRLMP_LINK_CB pIrlmpCb, IRDA_MSG *pMsg)
{
    ASSERT(pIrlmpCb->LinkState == LINK_CONNECTING);

    // Currently, the connect confirm always returns a successful status
    ASSERT(pMsg->IRDA_MSG_ConnStatus == IRLAP_CONNECTION_COMPLETED);

    // Update Link
    pIrlmpCb->LinkState = LINK_READY;
    RtlCopyMemory(&pIrlmpCb->NegotiatedQOS, pMsg->IRDA_MSG_pQos,
            sizeof(IRDA_QOS_PARMS));
    pIrlmpCb->MaxPDUSize =  IrlapGetQosParmVal(vDataSizeTable,
                                      pMsg->IRDA_MSG_pQos->bfDataSize, NULL)
                             - sizeof(IRLMP_HEADER) 
                             - sizeof(TTP_DATA_HEADER);

    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: IRLAP_CONNECT_CONF, TxMaxPDU = %d\n"),
                          pIrlmpCb->MaxPDUSize));
    
    IrdaEventSchedule(&EvLmConnectReq, pIrlmpCb->pIrdaLinkCb);
}
/*****************************************************************************
*
*   @func   UINT | IrlapDisconnectInd | Processes the disconnect indication
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*/
VOID
IrlapDisconnectInd(PIRLMP_LINK_CB pIrlmpCb, IRDA_MSG *pMsg)
{
    IRLMP_DISC_REASON   DiscReason = IRLMP_UNEXPECTED_IRLAP_DISC;

    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: IRLAP Disconnect Ind, status = %d\n"),
                          pMsg->IRDA_MSG_DiscStatus));
    
    switch (pIrlmpCb->LinkState)
    {
      case LINK_CONNECTING:
        if (pMsg->IRDA_MSG_DiscStatus == MAC_MEDIA_BUSY)
        {
            DiscReason = IRLMP_MAC_MEDIA_BUSY;
        }
        else
        {
            DiscReason = IRLMP_IRLAP_CONN_FAILED;
        }
        
        // Fall through
      case LINK_READY:
        pIrlmpCb->LinkState = LINK_DISCONNECTED;
        TearDownConnections(pIrlmpCb, DiscReason);
        break;

      case LINK_DISCONNECTING:
        pIrlmpCb->LinkState = LINK_DISCONNECTED;
        // Initiate a connection if one was requested while disconnecting
        ScheduleConnectReq(pIrlmpCb);
        break;

      default:
        DEBUGMSG(1, (TEXT("Link STATE %d\n"), pIrlmpCb->LinkState));
        
        //ASSERT(0);
    }
}
/*****************************************************************************
*
*   @func   IRLMP_LSAP_CB * | GetLsapInState | returns the first occurance of
*                                              an LSAP in the specified state
*                                              as long as the link is in the
*                                              specified state.
*
*   @parm   int | LinkState | get LSAP only as long as link is in this state
*
*   @parm   int | LSAP | Return the LSAP that is in this state if InThisState
*                        is TRUE. Else return LSAP if it is not in this state
*                        if InThisState = FALSE
*
*   @parm   BOOLEAN | InThisState | TRUE return LSAP if in this state
*                                FALSE return LSAP if not in this state
*
*   @rdesc  pointer to an LSAP control block or NULL, if an LSAP is returned
*
*/
IRLMP_LSAP_CB *
GetLsapInState(PIRLMP_LINK_CB pIrlmpCb,
               int LinkState,
               int LSAPState,
               BOOLEAN InThisState)
{
    IRLMP_LSAP_CB *pLsapCb;

    // Only want to find an LSAP if the link is in the specified state
    if (pIrlmpCb->LinkState != LinkState)
    {
        return NULL;
    }

    for (pLsapCb = (IRLMP_LSAP_CB *) pIrlmpCb->LsapCbList.Flink;
         (LIST_ENTRY *) pLsapCb != &pIrlmpCb->LsapCbList;
         pLsapCb = (IRLMP_LSAP_CB *) pLsapCb->Linkage.Flink)
    {
        VALIDLSAP(pLsapCb);

        if ((pLsapCb->State == LSAPState && InThisState == TRUE) ||
            (pLsapCb->State != LSAPState && InThisState == FALSE))
        {
            return pLsapCb;
        }
    }

    return NULL;
}
/*****************************************************************************
*
*/
IRLMP_LINK_CB *
GetIrlmpCb(PUCHAR RemoteDevAddr)
{
    IRDA_DEVICE     *pDevice;
    PIRLMP_LINK_CB  pIrlmpCb = NULL;
    
    NdisAcquireSpinLock(&SpinLock);
    
    for (pDevice = (IRDA_DEVICE *) DeviceList.Flink;
         (LIST_ENTRY *) pDevice != &DeviceList;
         pDevice = (IRDA_DEVICE *) pDevice->Linkage.Flink)
    {
    #ifdef UNDER_CE
        if (memcmp(pDevice->DevAddr, RemoteDevAddr, 
                   IRDA_DEV_ADDR_LEN) == 0)
    #else
        if (RtlCompareMemory(pDevice->DevAddr, RemoteDevAddr,
                             IRDA_DEV_ADDR_LEN) == IRDA_DEV_ADDR_LEN)
    #endif // !UNDER_CE
        {
            pIrlmpCb = (PIRLMP_LINK_CB)
                         ((PIRDA_LINK_CB) pDevice->LinkContext)->IrlmpContext;
            break;
        }
    }
    
    NdisReleaseSpinLock(&SpinLock);
    
    return pIrlmpCb;
}
/*****************************************************************************
*
*   @func   UINT | DiscDelayTimerFunc | Timer expiration callback
*
*   @rdesc  SUCCESS or an error code
*
*/
VOID
DiscDelayTimerFunc(PVOID Context)
{
    IRLMP_LSAP_CB               *pLsapCb;
    IRDA_MSG                    IMsg;
    UINT                        rc = SUCCESS;
    PIRLMP_LINK_CB              pIrlmpCb = (PIRLMP_LINK_CB) Context;

    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Link timer expired\n")));

    // The timer that expired is the disconnect delay timer. Bring
    // down link if no LSAP connection exists
    if (pIrlmpCb->LinkState == LINK_DISCONNECTED)
    {
        // already disconnected
        return;
    }
    
    // Search for an LSAP that is connected or coming up
    pLsapCb = (IRLMP_LSAP_CB *) pIrlmpCb->LsapCbList.Flink;
    while (&pIrlmpCb->LsapCbList != (LIST_ENTRY *) pLsapCb)
    {
        VALIDLSAP(pLsapCb);

        if (pLsapCb->State != LSAP_DISCONNECTED)
        {
            // Don't bring down link, an LSAP is connected or connecting
            DEBUGMSG(ZONE_WARN, (L"IRLMP:DiscDelayTimerFunc - Lsap %x state = %d\n", pLsapCb, pLsapCb->State));
            return;
        }
        pLsapCb = (IRLMP_LSAP_CB *) pLsapCb->Linkage.Flink;
    }

    DEBUGMSG(DBG_IRLMP, (TEXT(
       "IRLMP: No LSAP connections, disconnecting link\n")));
    // No LSAP connections, bring it down if it is up
    if (pIrlmpCb->LinkState == LINK_READY)
    {
        pIrlmpCb->LinkState = LINK_DISCONNECTING;

        // Request IRLAP to disconnect the link
        IMsg.Prim = IRLAP_DISCONNECT_REQ;
        IrlapDown(pIrlmpCb->pIrdaLinkCb->IrlapContext, &IMsg);
    }

    return;
}
/*****************************************************************************
*
*   @func   UINT | IrlapDataConf | Processes the data confirm
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*/
VOID
IrlapDataConf(IRDA_MSG *pMsg)
{
    IRLMP_LSAP_CB   *pLsapCb = pMsg->IRDA_MSG_pOwner;
    IRDA_MSG        *pSegParentMsg;
    BOOLEAN         RequestFailed = FALSE;
    UINT            rc = SUCCESS;

    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Received IRLAP_DATA_CONF pMsg:%X LsapCb:%X\n"),
             pMsg, pMsg->IRDA_MSG_pOwner));

    if (pMsg->IRDA_MSG_DataStatus != IRLAP_DATA_REQUEST_COMPLETED)
    {
        RequestFailed = TRUE;
    }
        
    if (pMsg->IRDA_MSG_SegFlags & SEG_LOCAL)
    {
        // Locally generated data request
        FreeIrdaBuf(IrdaMsgPool, pMsg);

        if (RequestFailed)
        {
            ; // LOG ERROR
        }

        return;
    }
    else
    {
        VALIDLSAP(pLsapCb);

        if (pMsg->IRDA_MSG_SegCount == 0)
        {
            if (!RequestFailed)
            {
                pMsg->IRDA_MSG_DataStatus = IRLMP_DATA_REQUEST_COMPLETED;
            }
        }
        else
        {
            // A segmented message, get its Parent 
            pSegParentMsg = pMsg->DataContext;
            
            // Free the segment
            FreeIrdaBuf(IrdaMsgPool, pMsg);

            if (RequestFailed)
            {
                pSegParentMsg->IRDA_MSG_DataStatus = IRLMP_DATA_REQUEST_FAILED;
            }
            
            if (--(pSegParentMsg->IRDA_MSG_SegCount) != 0)
            {
                // Still outstanding segments
                goto done;
            }
            // No more segments, send DATA_CONF to client
            // First remove it from the LSAPs TxMsgList 
            RemoveEntryList(&pSegParentMsg->Linkage);

            pMsg = pSegParentMsg;
        }

        // If request fails for non-segmented messages, the IRLAP error is
        // returned            
        pMsg->Prim = IRLMP_DATA_CONF;

        TdiUp(pLsapCb->pTdiContext, pMsg);

    done:
        REFDEL(&pLsapCb->RefCnt, 'ATAD');
    }
}
/*****************************************************************************
*
*   @func   UINT | IrlapDataInd | process the data indication
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRDA_MSG * | pMsg | Pointer to an IRDA Message
*
*/
VOID
IrlapDataInd(PIRLMP_LINK_CB pIrlmpCb, IRDA_MSG *pMsg)
{
    IRLMP_HEADER        *pLMHeader;
    IRLMP_CNTL_FORMAT   *pCntlFormat;
    UCHAR                *pCntlParm1;
    UCHAR                *pCntlParm2;

    if ((pMsg->IRDA_MSG_pWrite - pMsg->IRDA_MSG_pRead) < sizeof(IRLMP_HEADER))
    {
        ASSERT(0);
        return; // IRLMP_DATA_IND_BAD_FRAME;
    }

    pLMHeader = (IRLMP_HEADER *) pMsg->IRDA_MSG_pRead;

    pMsg->IRDA_MSG_pRead += sizeof(IRLMP_HEADER);

    if (pLMHeader->CntlBit != IRLMP_CNTL_PDU)
    {
        LmPduData(pIrlmpCb, pMsg, (int) pLMHeader->DstLsapSel,
                  (int) pLMHeader->SrcLsapSel);        
    }
    else
    {
        pCntlFormat = (IRLMP_CNTL_FORMAT *) pMsg->IRDA_MSG_pRead;

        // Ensure the control format is included. As per errate, it is
        // valid to exclude the parameter (for LM-connects and only if
        // no user data)        
        if ((UCHAR *) pCntlFormat >= pMsg->IRDA_MSG_pWrite)
        {
            ASSERT(0);
            // Need at least the OpCode portion
            return;// IRLMP_DATA_IND_BAD_FRAME;
        }
        else
        {
            // Initialize control parameters (if exists) and point
            // to beginning of user data
            if (&(pCntlFormat->Parm1) >= pMsg->IRDA_MSG_pWrite)
            {
                pCntlParm1 = NULL;
                pCntlParm2 = NULL;
                pMsg->IRDA_MSG_pRead = &(pCntlFormat->Parm1); // ie none
            }
            else
            {
                pCntlParm1 = &(pCntlFormat->Parm1);
                pCntlParm2 = &(pCntlFormat->Parm2); // Access mode only
                pMsg->IRDA_MSG_pRead = &(pCntlFormat->Parm2); 
            }                
        }        

        switch (pCntlFormat->OpCode)
        {
          case IRLMP_CONNECT_PDU:
            if (pCntlFormat->ABit == IRLMP_ABIT_REQUEST)
            {
                // Connection Request LM-PDU
                LmPduConnectReq(pIrlmpCb, pMsg, 
                                (int) pLMHeader->DstLsapSel,
                                (int) pLMHeader->SrcLsapSel,
                                pCntlParm1);
            }
            else 
            {
                // Connection Confirm LM-PDU
                LmPduConnectConf(pIrlmpCb, pMsg, 
                                 (int) pLMHeader->DstLsapSel,
                                 (int) pLMHeader->SrcLsapSel,
                                 pCntlParm1);
            }
            break;

        case IRLMP_DISCONNECT_PDU:
            if (pCntlFormat->ABit != IRLMP_ABIT_REQUEST)
            {
                ; // LOG ERROR !!!
            }
            else
            {
                LmPduDisconnectReq(pIrlmpCb, pMsg,
                                   (int) pLMHeader->DstLsapSel,
                                   (int) pLMHeader->SrcLsapSel,
                                   pCntlParm1);
            }
            break;
            
          case IRLMP_ACCESSMODE_PDU:
            if (pCntlFormat->ABit == IRLMP_ABIT_REQUEST)
            {
                LmPduAccessModeReq(pIrlmpCb,
                                   (int) pLMHeader->DstLsapSel,
                                   (int) pLMHeader->SrcLsapSel,
                                   pCntlParm1, pCntlParm2);
            }
            else
            {
                LmPduAccessModeConf(pIrlmpCb,
                                    (int) pLMHeader->DstLsapSel,
                                    (int) pLMHeader->SrcLsapSel,
                                    pCntlParm1, pCntlParm2);
            }
            break;
        }
    }
}
/*****************************************************************************
*
*   @func   UINT | LmPduConnectReq | Process the received connect
*                                        request LM-PDU
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRDA_MSG * | pMsg | pointer to an IRDA message
*           int | LocalLsapSel | The local LSAP selector, 
*                                 (destination LSAP-SEL in message)
*           int | RemoteLsapSel | The remote LSAP selector,
*                                  (source LSAP-SEL in message)
*           UCHAR * | pRsvdByte | pointer to the reserved parameter
*/
VOID
LmPduConnectReq(PIRLMP_LINK_CB pIrlmpCb, IRDA_MSG *pMsg,
                int LocalLsapSel, int RemoteLsapSel, UCHAR *pRsvdByte)
{
    IRDA_MSG                IMsg;
    IRLMP_LSAP_CB           *pLsapCb = GetLsap(pIrlmpCb,
                                               LocalLsapSel, RemoteLsapSel);
    IRLMP_REGISTERED_LSAP   *pRegLsap;
    BOOLEAN                 LsapRegistered = FALSE;
    
    DEBUGMSG((DBG_IRLMP | DBG_IRLMP_CONN), 
             (TEXT("IRLMP: Received LM_CONNECT_REQ for l=%d,r=%d\n"),
              LocalLsapSel, RemoteLsapSel));
    
    if (pRsvdByte != NULL && *pRsvdByte != 0x00)
    {
        // LOG ERROR (bad parm value)
        ASSERT(0);
        return;
    }       

    if (LocalLsapSel == IAS_LSAP_SEL)
    {
        IasConnectReq(pIrlmpCb, RemoteLsapSel);
        return;
    }

    if (pLsapCb == NULL) // Usually NULL, unless receiving 2nd ConnReq
    {
        // No reason to except connection if an LSAP hasn't been registered
        for (pRegLsap = (IRLMP_REGISTERED_LSAP *)
             RegisteredLsaps.Flink;
             (LIST_ENTRY *) pRegLsap != &RegisteredLsaps;
             pRegLsap = (IRLMP_REGISTERED_LSAP *) pRegLsap->Linkage.Flink)
        {
            if (pRegLsap->Lsap == LocalLsapSel)
            {
                LsapRegistered = TRUE;
                break;
            }
        }
        if (!LsapRegistered)
        {
            // No LSAP exists which matches the requested LSAP in the connect
            // packet. IRLMP will decline this connection
            UnroutableSendLMDisc(pIrlmpCb, LocalLsapSel, RemoteLsapSel);
            return;
        }
        else
        {
            // Create a new one
            if (CreateLsap(pIrlmpCb, &pLsapCb) != SUCCESS)
            {
                ASSERT(0);
                return;
            }
            pLsapCb->UseTtp = pRegLsap->UseTtp;
            pLsapCb->pTdiContext = NULL;
        }

        // very soon this LSAP will be waiting for a connect response
        // from the upper layer
        pLsapCb->State = LSAP_CONN_RESP_PEND;

        pLsapCb->LocalLsapSel = LocalLsapSel;
        pLsapCb->RemoteLsapSel = RemoteLsapSel;
        pLsapCb->UserDataLen = 0;

        SetupTtpAndStoreConnData(pLsapCb, pMsg);

        // Now setup the message to send to the client notifying him
        // of a incoming connection indication
        IMsg.Prim = IRLMP_CONNECT_IND;

        RtlCopyMemory(IMsg.IRDA_MSG_RemoteDevAddr, pIrlmpCb->ConnDevAddr,
               IRDA_DEV_ADDR_LEN);
        IMsg.IRDA_MSG_LocalLsapSel = LocalLsapSel;
        IMsg.IRDA_MSG_RemoteLsapSel = RemoteLsapSel;
        IMsg.IRDA_MSG_pQos = &pIrlmpCb->NegotiatedQOS;
        if (pLsapCb->UserDataLen != 0)
        {
            IMsg.IRDA_MSG_pConnData = pLsapCb->UserData;
            IMsg.IRDA_MSG_ConnDataLen = pLsapCb->UserDataLen;
        }
        else
        {
            IMsg.IRDA_MSG_pConnData = NULL;
            IMsg.IRDA_MSG_ConnDataLen = 0;
        }
    
        IMsg.IRDA_MSG_pContext = pLsapCb;
        IMsg.IRDA_MSG_MaxSDUSize = pLsapCb->TxMaxSDUSize;
        IMsg.IRDA_MSG_MaxPDUSize = pIrlmpCb->MaxPDUSize;

        // The LSAP response timer is the time that we give the Client
        // to respond to this connect indication. If it expires before
        // the client responds, then IRLMP will decline the connect
        IrdaTimerRestart(&pLsapCb->ResponseTimer);
    
        TdiUp(pLsapCb->pTdiContext, &IMsg);
        return;
    }
    else
    {
        ASSERT(0);
    }
    // Ignoring if LSAP already exists
}
/*****************************************************************************
*
*   @func   UINT | LmPduConnectConf | Process the received connect
*                                         confirm LM-PDU
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRDA_MSG * | pMsg | pointer to an IRDA message
*           int | LocalLsapSel | The local LSAP selector, 
*                                 (destination LSAP-SEL in message)
*           int | RemoteLsapSel | The remote LSAP selector,
*                                  (source LSAP-SEL in message)
*           BYTE * | pRsvdByte | pointer to the reserved byte parameter
*/
VOID
LmPduConnectConf(PIRLMP_LINK_CB pIrlmpCb,
                 IRDA_MSG *pMsg, int LocalLsapSel, int RemoteLsapSel,
                 UCHAR *pRsvdByte)
{
    IRLMP_LSAP_CB   *pLsapCb = GetLsap(pIrlmpCb,
                                       LocalLsapSel, RemoteLsapSel);

    DEBUGMSG((DBG_IRLMP | DBG_IRLMP_CONN), 
             (TEXT("IRLMP: Received LM_CONNECT_CONF for l=%d,r=%d\n"),
              LocalLsapSel, RemoteLsapSel));

    if (pRsvdByte != NULL && *pRsvdByte != 0x00)
    {
        // LOG ERROR, indicate bad parm
        return;
    }

    if (pLsapCb == NULL)
    {
        // This is a connect confirm to a non-existant LSAP
        // LOG SOMETHING HERE !!!
        return; 
    }

    if (pLsapCb->State != LSAP_LMCONN_CONF_PEND)
    {
        // received unsolicited confirm
        // probably timed out
        return;
    }

    IrdaTimerStop(&pLsapCb->ResponseTimer);
    
    pLsapCb->State = LSAP_READY;
    
    if (LocalLsapSel == IAS_LOCAL_LSAP_SEL && RemoteLsapSel == IAS_LSAP_SEL)
    {
        SendGetValueByClassReq(pLsapCb);
        return;
    }
    else
    {
        SetupTtpAndStoreConnData(pLsapCb, pMsg);
    
        pMsg->Prim = IRLMP_CONNECT_CONF;
            
        pMsg->IRDA_MSG_pQos = &pIrlmpCb->NegotiatedQOS;
        if (pLsapCb->UserDataLen != 0)
        {
            pMsg->IRDA_MSG_pConnData = pLsapCb->UserData;
            pMsg->IRDA_MSG_ConnDataLen = pLsapCb->UserDataLen;
        }
        else
        {
            pMsg->IRDA_MSG_pConnData = NULL;
            pMsg->IRDA_MSG_ConnDataLen = 0;
        }
        
        pMsg->IRDA_MSG_pContext = pLsapCb;
        pMsg->IRDA_MSG_MaxSDUSize = pLsapCb->TxMaxSDUSize;
        pMsg->IRDA_MSG_MaxPDUSize = pIrlmpCb->MaxPDUSize;

        TdiUp(pLsapCb->pTdiContext, pMsg);
    }
}
/*****************************************************************************
*/
VOID
SetupTtpAndStoreConnData(IRLMP_LSAP_CB *pLsapCb, IRDA_MSG *pMsg)
{
    TTP_CONN_HEADER *pTTPHeader;
    UCHAR            PLen, *pEndParms, PI, PL;

    VALIDLSAP(pLsapCb);

    // Upon entering this function, the pRead pointer points to the
    // TTP header or the beginning of client data

    if (pLsapCb->UseTtp == FALSE)
    {
        pLsapCb->TxMaxSDUSize = pLsapCb->pIrlmpCb->MaxPDUSize;
    }
    else
    {
        if (pMsg->IRDA_MSG_pRead >= pMsg->IRDA_MSG_pWrite)
        {
            // THIS IS AN ERROR, WE ARE USING TTP. There is no more
            // data in the frame, but we need the TTP header
            // SOME KIND OF WARNING SHOULD BE LOGGED
            return;
        }
        pTTPHeader = (TTP_CONN_HEADER *) pMsg->IRDA_MSG_pRead;
        pLsapCb->LocalTxCredit = (int) pTTPHeader->InitialCredit;

        DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: LocalTxCredit %d\n"),
                              pLsapCb->LocalTxCredit));
                              
        // advance the pointer to the first byte of data
        pMsg->IRDA_MSG_pRead += sizeof(TTP_CONN_HEADER);
        
        pLsapCb->TxMaxSDUSize = 0;
        if (pTTPHeader->ParmFlag == TTP_PFLAG_PARMS)
        {
            // Parameter carrying Connect TTP-PDU
            PLen = *pMsg->IRDA_MSG_pRead++;
            pEndParms = pMsg->IRDA_MSG_pRead + PLen;
        
            // NOTE: This breaks if PI other than MaxSDUSize!!!

            if (PLen < 3 || pEndParms > pMsg->IRDA_MSG_pWrite)
            {
                // LOG ERROR !!!
                return;
            }
            
            PI = *pMsg->IRDA_MSG_pRead++;
            PL = *pMsg->IRDA_MSG_pRead++;
            
            if (PI != TTP_MAX_SDU_SIZE_PI)
            {
                // LOG ERROR !!!
                return;
            }

            for ( ; PL != 0 ; PL--)
            {
                pLsapCb->TxMaxSDUSize <<= 8;
                pLsapCb->TxMaxSDUSize += (int) (*pMsg->IRDA_MSG_pRead);
                pMsg->IRDA_MSG_pRead++;
            }
        }
    }
    
    // if there is any user data with this connection request/conf, place
    // it in the control block. This is just a place to store this
    // information while upper layer is looking at it. It may be over-
    // written by connection data in the response from the client.
    if (pMsg->IRDA_MSG_pRead < pMsg->IRDA_MSG_pWrite)
    {
        pLsapCb->UserDataLen = pMsg->IRDA_MSG_pWrite - pMsg->IRDA_MSG_pRead;
        RtlCopyMemory(pLsapCb->UserData, pMsg->IRDA_MSG_pRead,
               pLsapCb->UserDataLen);
    }

    return;
}
/*****************************************************************************
*
*   @func   UINT | LmPduDisconnectReq | Process the received discconnect
*                                           request LM-PDU
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRDA_MSG * | pMsg | pointer to an IRDA message
*           int | LocalLsapSel | The local LSAP selector, 
*                                 (destination LSAP-SEL in message)
*           int | RemoteLsapSel | The remote LSAP selector,
*                                  (source LSAP-SEL in message)
*           BYTE * | pReason | pointer to the reason parameter
*/
VOID
LmPduDisconnectReq(PIRLMP_LINK_CB pIrlmpCb, IRDA_MSG *pMsg,
                   int LocalLsapSel, int RemoteLsapSel, UCHAR *pReason)
{
    IRLMP_LSAP_CB       *pLsapCb;
    UINT                rc = SUCCESS;

    if (pReason == NULL)
    {
        ASSERT(0);
        return; // LOG ERROR !!! need reason code
    }

    pLsapCb = GetLsap(pIrlmpCb, LocalLsapSel, RemoteLsapSel);
    
    DEBUGMSG((DBG_IRLMP | DBG_IRLMP_CONN), 
             (TEXT("IRLMP: Received LM_DISCONNECT_REQ LsapCb:%x for l=%d,r=%d\n"),
              pLsapCb, LocalLsapSel, RemoteLsapSel));
    
    if (pLsapCb == NULL)
    {
        return;
    }

    if (pLsapCb->State == LSAP_LMCONN_CONF_PEND)
    {
        IrdaTimerStop(&pLsapCb->ResponseTimer);
    }

    IrdaTimerRestart(&pIrlmpCb->DiscDelayTimer);

    if (LocalLsapSel == IAS_LSAP_SEL)
    {
        IasServerDisconnectReq(pLsapCb);
        return;
    }
    if (LocalLsapSel == IAS_LOCAL_LSAP_SEL && RemoteLsapSel == IAS_LSAP_SEL)
    {
        IasClientDisconnectReq(pLsapCb, *pReason);
        return;
    }

    if (pLsapCb->State != LSAP_DISCONNECTED)
    {
        if (pMsg->IRDA_MSG_pRead < pMsg->IRDA_MSG_pWrite)
        {
            // Disconnect User data
            pLsapCb->UserDataLen = pMsg->IRDA_MSG_pWrite -
            pMsg->IRDA_MSG_pRead;
            RtlCopyMemory(pLsapCb->UserData, pMsg->IRDA_MSG_pRead,
               pLsapCb->UserDataLen);
        }   
        else
        {
            pLsapCb->UserDataLen = 0;
        }
            
        // Notify upper layer
        pMsg->Prim = IRLMP_DISCONNECT_IND;
        pMsg->IRDA_MSG_DiscReason = *pReason;
        if (pLsapCb->UserDataLen != 0)
        {
            pMsg->IRDA_MSG_pDiscData = pLsapCb->UserData;
            pMsg->IRDA_MSG_DiscDataLen = pLsapCb->UserDataLen;
        }
        else
        {
            pMsg->IRDA_MSG_pDiscData = NULL;
            pMsg->IRDA_MSG_DiscDataLen = 0;
        }
        TdiUp(pLsapCb->pTdiContext, pMsg);

        CleanupLsap(pLsapCb);
    }
}
/*****************************************************************************
*
*   @func   IRLMP_LSAP_CB *| GetLsap | For the LSAP selector pair, return the
*                                      LSAP control block they map to. NULL
*                                      if one does not exist
*
*   @rdesc  pointer to an LSAP control block or NULL
*
*   @parm   int | LocalLsapSel | local LSAP selector
*   @parm   int | RemoteLsapSel | Remote LSAP selector
*
*   if an LSAP is found, its critical section is acquired
*/
IRLMP_LSAP_CB *
GetLsap(PIRLMP_LINK_CB pIrlmpCb, int LocalLsapSel, int RemoteLsapSel)
{
    IRLMP_LSAP_CB *pLsapCb;

    for (pLsapCb = (IRLMP_LSAP_CB *) pIrlmpCb->LsapCbList.Flink;
         (LIST_ENTRY *) pLsapCb != &pIrlmpCb->LsapCbList;
         pLsapCb = (IRLMP_LSAP_CB *) pLsapCb->Linkage.Flink)
    {
        VALIDLSAP(pLsapCb);

        if (pLsapCb->LocalLsapSel == LocalLsapSel &&
            pLsapCb->RemoteLsapSel == RemoteLsapSel)
        {
            return pLsapCb;
        }
    }

    return NULL;
}
/*****************************************************************************
*
*   @func   UINT | SendCreditPdu | Send a dataless PDU to extend credit
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRLMP_LSAP_CB * | pLsapCb | pointer to an LSAP control block
*/
VOID
SendCreditPdu(IRLMP_LSAP_CB *pLsapCb)
{
    IRDA_MSG    *pMsg;
    
    VALIDLSAP(pLsapCb);

    if (pLsapCb->AvailableCredit == 0)
    {
        // No credit to give
        return;
    }
    
    if ((pMsg = AllocIrdaBuf(IrdaMsgPool)) == NULL)
    {
        ASSERT(0);
        return;
    }

    // No Data
    pMsg->IRDA_MSG_pBase =
    pMsg->IRDA_MSG_pLimit =
    pMsg->IRDA_MSG_pRead = 
    pMsg->IRDA_MSG_pWrite = pMsg->IRDA_MSG_Header + IRDA_HEADER_LEN;

    pMsg->IRDA_MSG_IrCOMM_9Wire = FALSE;
    
    pMsg->IRDA_MSG_SegFlags = SEG_FINAL;
    
    FormatAndSendDataReq(pLsapCb, pMsg, TRUE);
}
/*****************************************************************************
*
*   @func   VOID | LmPduData | Process the received data (indication)
*                                  LM-PDU
*
*   @rdesc  SUCCESS or an error code
*
*   @parm   IRDA_MSG * | pMsg | pointer to an IRDA message
*           int | LocalLsapSel | The local LSAP selector, 
*                                 (destination LSAP-SEL in message)
*           int | RemoteLsapSel | The remote LSAP selector,
*                                  (source LSAP-SEL in message)
*/
VOID
LmPduData(PIRLMP_LINK_CB pIrlmpCb, IRDA_MSG *pMsg,
          int LocalLsapSel, int RemoteLsapSel)
{
    IRLMP_LSAP_CB       *pLsapCb = GetLsap(pIrlmpCb,
                                           LocalLsapSel, RemoteLsapSel);
    TTP_DATA_HEADER     *pTTPHeader;
    // UINT                rc;
    BOOLEAN             DataPDUSent = FALSE;
    BOOLEAN             FinalSeg = TRUE;
   
    if (pLsapCb == NULL)
    {
        // Unroutable, send disconnect
        DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Data sent to bad Lsap (%d,%d\n"),
                 LocalLsapSel, RemoteLsapSel));
        //UnroutableSendLMDisc(pIrlmpCb, LocalLsapSel, RemoteLsapSel);
        return;
    }

    if (LocalLsapSel == IAS_LSAP_SEL)
    {
        IasSendQueryResp(pLsapCb, pMsg);
        return;
    }
    if (LocalLsapSel == IAS_LOCAL_LSAP_SEL && RemoteLsapSel == IAS_LSAP_SEL)
    {
        IasProcessQueryResp(pIrlmpCb, pLsapCb, pMsg);
        return;
    }
    
    if (pLsapCb->UseTtp)
    {
        if (pMsg->IRDA_MSG_pRead >= pMsg->IRDA_MSG_pWrite)
        {
            // NEED TTP HEADER, LOG ERROR !!!
            return;
        }
            
        pTTPHeader = (TTP_DATA_HEADER *) pMsg->IRDA_MSG_pRead;
        pMsg->IRDA_MSG_pRead += sizeof(TTP_DATA_HEADER);

        pLsapCb->LocalTxCredit += (int) pTTPHeader->AdditionalCredit;

#if DBG
        if (pTTPHeader->AdditionalCredit)
        {
            DEBUGMSG(DBG_IRLMP_CRED,
                     (TEXT("IRLMP(l%d,r%d): Rx LocalTxCredit:+%d=%d\n"),
                      pLsapCb->LocalLsapSel, pLsapCb->RemoteLsapSel,
                      pTTPHeader->AdditionalCredit, pLsapCb->LocalTxCredit));
        }
#endif 
        if (pTTPHeader->MoreBit == TTP_MBIT_NOT_FINAL)
        {
            FinalSeg = FALSE;
        }
    }

    REFADD(&pLsapCb->RefCnt, ' DNI');

    if (pMsg->IRDA_MSG_pRead < pMsg->IRDA_MSG_pWrite)
    {
        // PDU containing data. Decrement remotes Tx Credit
        pLsapCb->RemoteTxCredit--;
        
        if (pLsapCb->State >= LSAP_READY)
        {
            pMsg->Prim = IRLMP_DATA_IND;
            pMsg->IRDA_MSG_SegFlags = FinalSeg ? SEG_FINAL : 0;

            TdiUp(pLsapCb->pTdiContext, pMsg);
        }
    }
    // else no user data, this was a dataless TTP-PDU to extend credit.
      
    if (pLsapCb->State != LSAP_DISCONNECTED)
    {
        // Did we get some credit?
        if (pLsapCb->UseTtp && pLsapCb->LocalTxCredit > 0 && 
            pLsapCb->State == LSAP_NO_TX_CREDIT)
        {
            DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: l%d,r%d flow on\n"),
                                  pLsapCb->LocalLsapSel, pLsapCb->RemoteLsapSel));
            
            pLsapCb->State = LSAP_READY;
        }
        DEBUGMSG(DBG_IRLMP,
                 (TEXT("IRLMP: Rx LocTxCredit %d,RemoteTxCredit %d\n"),
                  pLsapCb->LocalTxCredit, pLsapCb->RemoteTxCredit));
    
        while (!IsListEmpty(&pLsapCb->SegTxMsgList) && 
               pLsapCb->State == LSAP_READY)
        {
            pMsg = (IRDA_MSG *) RemoveHeadList(&pLsapCb->SegTxMsgList);
            
            FormatAndSendDataReq(pLsapCb, pMsg, FALSE);
    
            DataPDUSent = TRUE;
        }   
        
        // Do I need to extend credit to peer in a dataless PDU?
        if (pLsapCb->UseTtp && !DataPDUSent && 
            pLsapCb->RemoteTxCredit <= pIrlmpCb->WindowSize + 1)
        {   
            SendCreditPdu(pLsapCb);
        }
    }
    REFDEL(&pLsapCb->RefCnt, ' DNI');
}
/*****************************************************************************
*
*   @func   UINT | LmPduAccessModeReq | process access mode request 
*                                           from peer
*
*   @rdesc  SUCCESS
*
*   @parm   int | LocalLsapSel | Local LSAP selector
*   @parm   int | LocalLsapSel | Local LSAP selector
*   @parm   BYTE * | pRsvdByte | Reserved byte in the Access mode PDU
*   @parm   BYTE * | pMode | Mode byte in Access mode PDU
*/
VOID
LmPduAccessModeReq(PIRLMP_LINK_CB pIrlmpCb,
                   int LocalLsapSel, int RemoteLsapSel, 
                   UCHAR *pRsvdByte, UCHAR *pMode)
{
    IRLMP_LSAP_CB   *pRequestedLsapCb = GetLsap(pIrlmpCb,
                                                 LocalLsapSel,RemoteLsapSel);
    IRLMP_LSAP_CB   *pLsapCb;
    IRDA_MSG        IMsg;
    
    if (pRequestedLsapCb==NULL || pRequestedLsapCb->State != LSAP_READY)
    {
        UnroutableSendLMDisc(pIrlmpCb, LocalLsapSel, RemoteLsapSel);
        return;
    }
    
    if (pRsvdByte == NULL || *pRsvdByte != 0x00 || pMode == NULL)
    {
        // LOG ERROR, indicate bad parm
        return;
    }
    
    switch (*pMode)
    {
      case IRLMP_EXCLUSIVE:
        if (pIrlmpCb->pExclLsapCb != NULL)
        {
            if (pIrlmpCb->pExclLsapCb == pRequestedLsapCb)
            {
                // Already has exclusive mode, confirm it again I guess
                // but I'm not telling my client again
                SendCntlPdu(pRequestedLsapCb, IRLMP_ACCESSMODE_PDU,
                            IRLMP_ABIT_CONFIRM, IRLMP_STATUS_SUCCESS,
                            IRLMP_EXCLUSIVE);
                return;
            }
            else
            {
                // This is what spec says...
                SendCntlPdu(pRequestedLsapCb, IRLMP_ACCESSMODE_PDU,
                            IRLMP_ABIT_CONFIRM, IRLMP_STATUS_FAILURE,
                            IRLMP_MULTIPLEXED);
                return;
            }
        }

        // Are there any other LSAPs connections? If so, NACK peer
        for (pLsapCb = (IRLMP_LSAP_CB *) pIrlmpCb->LsapCbList.Flink;
             (LIST_ENTRY *) pLsapCb != &pIrlmpCb->LsapCbList;
             pLsapCb = (IRLMP_LSAP_CB *) pLsapCb->Linkage.Flink)
        {
            if (pLsapCb->State != LSAP_DISCONNECTED && 
                pLsapCb != pRequestedLsapCb)
            {
                SendCntlPdu(pRequestedLsapCb, IRLMP_ACCESSMODE_PDU,
                            IRLMP_ABIT_CONFIRM, IRLMP_STATUS_FAILURE,
                            IRLMP_MULTIPLEXED);
                return;
            }
        }      
        // OK to go into exclusive mode
        pIrlmpCb->pExclLsapCb = pRequestedLsapCb;
        // Send confirmation to peer
        SendCntlPdu(pRequestedLsapCb, IRLMP_ACCESSMODE_PDU,
                    IRLMP_ABIT_CONFIRM, IRLMP_STATUS_SUCCESS,
                    IRLMP_EXCLUSIVE);
        // Notify client
        IMsg.Prim = IRLMP_ACCESSMODE_IND;
        IMsg.IRDA_MSG_AccessMode = IRLMP_EXCLUSIVE;
        TdiUp(pRequestedLsapCb->pTdiContext, &IMsg);
        return;
        
      case IRLMP_MULTIPLEXED:
        if (pRequestedLsapCb != pIrlmpCb->pExclLsapCb)
        {
            // Log Error here
            return;
        }
        pIrlmpCb->pExclLsapCb = NULL;
        // Send confirmation to peer
        SendCntlPdu(pRequestedLsapCb, IRLMP_ACCESSMODE_PDU,
                    IRLMP_ABIT_CONFIRM, IRLMP_STATUS_SUCCESS,
                    IRLMP_MULTIPLEXED);
        // Notify client
        IMsg.Prim = IRLMP_ACCESSMODE_IND;
        IMsg.IRDA_MSG_AccessMode = IRLMP_MULTIPLEXED;
        TdiUp(pRequestedLsapCb->pTdiContext, &IMsg);
        return;
        
      default:
        ASSERT(0);
    }
}
/*****************************************************************************
*
*   @func   UINT | LmPduAccessModeReq | process access mode request 
*                                           from peer
*
*   @rdesc  SUCCESS
*
*   @parm   int | LocalLsapSel | Local LSAP selector
*   @parm   int | LocalLsapSel | Local LSAP selector
*   @parm   BYTE * | pStatus | Status byte in the Access mode PDU
*   @parm   BYTE * | pMode | Mode byte in Access mode PDU
*/
VOID
LmPduAccessModeConf(PIRLMP_LINK_CB pIrlmpCb,
                    int LocalLsapSel, int RemoteLsapSel, 
                    UCHAR *pStatus, UCHAR *pMode)
{
    IRLMP_LSAP_CB   *pRequestedLsapCb = GetLsap(pIrlmpCb,
                                                 LocalLsapSel,RemoteLsapSel);
    IRDA_MSG        IMsg;

    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: ACCESSMODE_CONF\r\n")));
    
    if (pRequestedLsapCb==NULL)
    {
        UnroutableSendLMDisc(pIrlmpCb, LocalLsapSel, RemoteLsapSel);
        return;
    }
    if (pStatus == NULL || pMode == NULL)
    {
        // LOG ERROR
        return;
    }
    
    switch (*pMode)
    {
      case IRLMP_EXCLUSIVE:
        if (pRequestedLsapCb != pIrlmpCb->pExclLsapCb || 
            pRequestedLsapCb->State != LSAP_EXCLUSIVEMODE_PEND)
        {
            // LOG ERROR
            return;
        }
        if (*pStatus != IRLMP_STATUS_SUCCESS)
        {
            pIrlmpCb->pExclLsapCb = NULL;
            return; // protocol error, 
                    // wouldn't have Exclusive mode != SUCCESS
        }
        else
        {
            pRequestedLsapCb->State = LSAP_READY;

            IMsg.Prim = IRLMP_ACCESSMODE_CONF;
            IMsg.IRDA_MSG_AccessMode = IRLMP_EXCLUSIVE;
            IMsg.IRDA_MSG_ModeStatus = IRLMP_ACCESSMODE_SUCCESS;

            TdiUp(pRequestedLsapCb->pTdiContext, &IMsg);
            return;
        }
        
      case IRLMP_MULTIPLEXED:
        if (pRequestedLsapCb != pIrlmpCb->pExclLsapCb || 
            (pRequestedLsapCb->State != LSAP_EXCLUSIVEMODE_PEND &&
             pRequestedLsapCb->State != LSAP_MULTIPLEXEDMODE_PEND))
        {
            return;
        }

        pIrlmpCb->pExclLsapCb = NULL;
        pRequestedLsapCb->State = LSAP_READY;
            
        IMsg.Prim = IRLMP_ACCESSMODE_CONF;
        IMsg.IRDA_MSG_AccessMode = *pMode;
        if (*pStatus == IRLMP_STATUS_SUCCESS)
        {
            IMsg.IRDA_MSG_ModeStatus = IRLMP_ACCESSMODE_SUCCESS;
        }
        else
        {
            IMsg.IRDA_MSG_ModeStatus = IRLMP_ACCESSMODE_FAILURE;
        }            
        TdiUp(pRequestedLsapCb->pTdiContext, &IMsg);
        return;
        
      default:
        ASSERT(0);
    }
}
/*****************************************************************************
*
*   @func   UINT | UnroutableSendLMDisc | Sends an LM-Disconnect to peer with
*                                         reason = "received LM packet on 
*                                         disconnected LSAP"
*   @parm   int | LocalLsapSel | the local LSAP selector in LM-PDU
*   @parm   int | RemoteLsapSel | the remote LSAP selector in LM-PDU
*/
VOID
UnroutableSendLMDisc(PIRLMP_LINK_CB pIrlmpCb, int LocalLsapSel, int RemoteLsapSel)
{
    IRLMP_LSAP_CB   FakeLsapCb;
    
    DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: received unroutable Pdu LocalLsap:%d RemoteLsap:%d\n"),
                        LocalLsapSel, RemoteLsapSel));
    FakeLsapCb.UseTtp           = FALSE;
    FakeLsapCb.LocalLsapSel     = LocalLsapSel;
    FakeLsapCb.RemoteLsapSel    = RemoteLsapSel;
    FakeLsapCb.UserDataLen      = 0;
    FakeLsapCb.pIrlmpCb         = pIrlmpCb;
#ifdef DBG
    FakeLsapCb.Sig              = LSAPSIG;
#endif 
    SendCntlPdu(&FakeLsapCb,IRLMP_DISCONNECT_PDU,
                IRLMP_ABIT_REQUEST, IRLMP_DISC_LSAP, 0);
    return;
}

/*****************************************************************************
*
*   @func   UINT | InitiateDiscovoryReq | A deferred processing routine that sends
*                                    an IRLAP discovery request
*/
void
InitiateDiscoveryReq(PVOID Context)
{
    IRDA_MSG        IMsg;
    UINT            rc;
    PIRLMP_LINK_CB  pIrlmpCb = NULL;
    PIRLMP_LINK_CB  pIrlmpCb2 = NULL;
    PIRDA_LINK_CB   pIrdaLinkCb;
    BOOLEAN         ScheduleNextLink = TRUE;

    NdisAcquireSpinLock(&SpinLock);

    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: InitDscvReq event\n")));
    
    // Find the next link to start discovery on
    for (pIrdaLinkCb = (PIRDA_LINK_CB) IrdaLinkCbList.Flink;
        (LIST_ENTRY *) pIrdaLinkCb != &IrdaLinkCbList;
         pIrdaLinkCb = (PIRDA_LINK_CB) pIrdaLinkCb->Linkage.Flink)    
    {
        pIrlmpCb2 = (PIRLMP_LINK_CB) pIrdaLinkCb->IrlmpContext;

        if (pIrlmpCb2->DiscoveryPending)
        {
            pIrlmpCb2->DiscoveryPending = FALSE;
            pIrlmpCb = pIrlmpCb2;
            break;
        }
    }

    // No more links on which to discover, send confirm up
    if (pIrlmpCb == NULL)
    {
        if (pIrlmpCb2 == NULL)
        {
            IMsg.IRDA_MSG_DscvStatus = IRLMP_NO_RESPONSE;
        }
        else 
        {
            IMsg.IRDA_MSG_DscvStatus = IRLAP_DISCOVERY_COMPLETED;
        }

        DscvReqScheduled = FALSE;
        NdisReleaseSpinLock(&SpinLock);
        
        IMsg.Prim = IRLMP_DISCOVERY_CONF;
        IMsg.IRDA_MSG_pDevList = &DeviceList;
        TdiUp(NULL, &IMsg);
        return;
    }

    NdisReleaseSpinLock(&SpinLock);

    LOCK_LINK(pIrlmpCb->pIrdaLinkCb);    

    if (pIrlmpCb->LinkState == LINK_DISCONNECTED)
    {
        IMsg.Prim = IRLAP_DISCOVERY_REQ;
        IMsg.IRDA_MSG_SenseMedia = TRUE;

        DEBUGMSG(DBG_IRLMP,
                 (TEXT
                  ("IRLMP: Sent IRLAP_DISCOVERY_REQ, New LinkState=LINK_IN_DISCOVERY\n")));
        
        if ((rc = IrlapDown(pIrlmpCb->pIrdaLinkCb->IrlapContext, &IMsg)) != SUCCESS)
        {
            if (rc != IRLAP_REMOTE_DISCOVERY_IN_PROGRESS_ERR &&
                rc != IRLAP_REMOTE_CONNECTION_IN_PROGRESS_ERR)
            {
                ASSERT(0);
            }
            else
            {                
                DEBUGMSG(DBG_IRLMP, (TEXT("IRLAP_DISCOVERY_REQ failed, link busy\n")));
            }
        }
        else
        {
            pIrlmpCb->LinkState = LINK_IN_DISCOVERY;
            // The next link will be schedule to run discovery when
            // the DISCOVERY_CONF for this link is received
            ScheduleNextLink = FALSE;
        }
    }
    
    UNLOCK_LINK(pIrlmpCb->pIrdaLinkCb);

    // Discovery failed on this link or it was not in the disconnected
    // state, schedule the next one

    if (ScheduleNextLink)
    {
        IrdaEventSchedule(&EvDiscoveryReq, NULL);
    }
}

VOID
ScheduleConnectReq(PIRLMP_LINK_CB pIrlmpCb)
{
    IRLMP_LSAP_CB   *pLsapCb;
    
    // Schedule the ConnectReq event if not already scheduled and if an LSAP
    // has a connect pending
    if (pIrlmpCb->ConnReqScheduled == FALSE)
    {       
        for (pLsapCb = (IRLMP_LSAP_CB *) pIrlmpCb->LsapCbList.Flink;
             (LIST_ENTRY *) pLsapCb != &pIrlmpCb->LsapCbList;
             pLsapCb = (IRLMP_LSAP_CB *) pLsapCb->Linkage.Flink)
        {
            VALIDLSAP(pLsapCb);

            if (pLsapCb->State == LSAP_CONN_REQ_PEND)
            {
                IrdaEventSchedule(&EvConnectReq, pIrlmpCb->pIrdaLinkCb);

                pIrlmpCb->ConnReqScheduled = TRUE;
                return;
            }
        }
    }
}

/*****************************************************************************
*
*   @func   UINT | InitiateConnectReq | A deferred processing routine that sends
*                                   IRLAP a connect request
*   This is scheduled after an IRLMP discovery confirm or a disconnect 
*   indication has been received via IRLMP_Up(). This allows the possible
*   IRLAP connect request to be made in a different context.
*/
void
InitiateConnectReq(PVOID Context)
{
    IRLMP_LSAP_CB   *pLsapCb;
    BOOLEAN         ConnectIrlap = FALSE;
    IRDA_MSG        IMsg;
    UINT            rc;
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) Context;
    PIRLMP_LINK_CB  pIrlmpCb = (PIRLMP_LINK_CB) pIrdaLinkCb->IrlmpContext;

    LOCK_LINK(pIrdaLinkCb);
    
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: InitiateConnectReq()!\n")));

    pIrlmpCb->ConnReqScheduled = FALSE;

    if (pIrlmpCb->LinkState != LINK_DISCONNECTED)
    {
        UNLOCK_LINK(pIrdaLinkCb);           
        ASSERT(0);
        return;
    }

    // Check for LSAP in the connect request pending state.
    // If one or more exists, place them in IRLAP connect pending state
    // and initiate an IRLAP connection
    for (pLsapCb = (IRLMP_LSAP_CB *) pIrlmpCb->LsapCbList.Flink;
        (LIST_ENTRY *) pLsapCb != &pIrlmpCb->LsapCbList;
        pLsapCb = (IRLMP_LSAP_CB *) pLsapCb->Linkage.Flink)
    {
        VALIDLSAP(pLsapCb);

        if (pLsapCb->State == LSAP_CONN_REQ_PEND)
        {
            pLsapCb->State = LSAP_IRLAP_CONN_PEND;
            ConnectIrlap = TRUE;
        }
    }

    if (ConnectIrlap)
    {
        DEBUGMSG(DBG_IRLMP, 
           (TEXT("IRLMP: IRLAP_CONNECT_REQ, State=LINK CONNECTING\r\n")));
        
        pIrlmpCb->LinkState = LINK_CONNECTING;

        pIrlmpCb->ConnDevAddrSet = FALSE; // This was previously set by
                                          // the LSAP which set the remote
                                          // device address. This is the
                                          // first opportunity to clear
                                          // the flag

        // Get the connection address out of the IRLMP control block
        RtlCopyMemory(IMsg.IRDA_MSG_RemoteDevAddr, pIrlmpCb->ConnDevAddr,
               IRDA_DEV_ADDR_LEN);
        IMsg.Prim = IRLAP_CONNECT_REQ;
        if ((rc = IrlapDown(pIrlmpCb->pIrdaLinkCb->IrlapContext, &IMsg))
            != SUCCESS)
        {
            DEBUGMSG(DBG_IRLMP, 
             (TEXT("IRLMP: IRLAP_CONNECT_REQ failed, state = disconnect\r\n")));

            pIrlmpCb->LinkState = LINK_DISCONNECTED;

            ASSERT(rc == IRLAP_REMOTE_DISCOVERY_IN_PROGRESS_ERR);

            TearDownConnections(pIrlmpCb, IRLMP_IRLAP_REMOTE_DISCOVERY_IN_PROGRESS);
        }
    }

    UNLOCK_LINK(pIrdaLinkCb);
    
    return;
}

void
InitiateConnectResp(PVOID Context)
{
    IRDA_MSG    IMsg;
//    UINT        rc;
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) Context;
    PIRLMP_LINK_CB  pIrlmpCb = (PIRLMP_LINK_CB) pIrdaLinkCb->IrlmpContext;
    
    LOCK_LINK(pIrdaLinkCb);
    
    if (pIrlmpCb->AcceptConnection)
    {
        IMsg.Prim = IRLAP_CONNECT_RESP;

        IrlapDown(pIrdaLinkCb->IrlapContext, &IMsg);

        pIrlmpCb->LinkState = LINK_READY;
        
        // Disconnect the link if no LSAP connection after a bit
        IrdaTimerRestart(&pIrlmpCb->DiscDelayTimer);                
    }
    else
    {
        pIrlmpCb->LinkState = LINK_DISCONNECTED;
        IMsg.Prim = IRLAP_DISCONNECT_REQ;
        IrlapDown(pIrdaLinkCb->IrlapContext, &IMsg);        
    }
    
    UNLOCK_LINK(pIrdaLinkCb);    

    return;
}

void
InitiateLMConnectReq(PVOID Context)
{
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) Context;
    PIRLMP_LINK_CB  pIrlmpCb = (PIRLMP_LINK_CB) pIrdaLinkCb->IrlmpContext;
    IRLMP_LSAP_CB   *pLsapCb;
    
    LOCK_LINK(pIrdaLinkCb);

    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: InitiateLMConnectReq()!\n")));    

    // Send the connect request PDU to peer LSAPs
    while ((pLsapCb = GetLsapInState(pIrlmpCb, LINK_READY,
                                      LSAP_IRLAP_CONN_PEND, TRUE)) != NULL)
    {
        pLsapCb->State = LSAP_LMCONN_CONF_PEND;

        // Ask remote LSAP for a connection
        SendCntlPdu(pLsapCb, IRLMP_CONNECT_PDU, IRLMP_ABIT_REQUEST,
                    IRLMP_RSVD_PARM, 0);
        
        IrdaTimerRestart(&pLsapCb->ResponseTimer);
    }
    
    UNLOCK_LINK(pIrdaLinkCb);
}

void
InitiateCloseLink(PVOID Context)
{
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) Context;
    PIRLMP_LINK_CB  pIrlmpCb = (PIRLMP_LINK_CB) pIrdaLinkCb->IrlmpContext;
    // IAS_OBJECT      *pObject, *pNextObject;
    IRDA_MSG        IMsg;
    // PVOID           pPtr;
#ifndef UNDER_CE
    LARGE_INTEGER   SleepMs;    
#endif // !UNDER_CE
    
//    //Sleep(500); // This sleep allows time for LAP to send any 
                // LM_DISCONNECT_REQ's that may be sitting on its TxQue.                
    
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: InitiateCloseLink()!\n")));    
    
    LOCK_LINK(pIrdaLinkCb);    

    // Bring down the link...
    IMsg.Prim = IRLAP_DISCONNECT_REQ;
    IrlapDown(pIrdaLinkCb->IrlapContext, &IMsg);

    UNLOCK_LINK(pIrdaLinkCb);    
    
    // Allow LAP time to disconnect link
#ifdef UNDER_CE
    Sleep(1000);
#else
    SleepMs.QuadPart = -(10*1000*1000);
    KeDelayExecutionThread(KernelMode, FALSE, &SleepMs);
#endif // !UNDER_CE

    LOCK_LINK(pIrlmpCb->pIrdaLinkCb);    
    
    IrlapCloseLink(pIrdaLinkCb);
   
    TearDownConnections(pIrlmpCb, IRLMP_UNSPECIFIED_DISC);
    
    // Stop link timer
    IrdaTimerStop(&pIrlmpCb->DiscDelayTimer);
    
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP Shutdown\n")));

    UNLOCK_LINK(pIrdaLinkCb);
    
    return;
}

// IAS

// Oh my God! I'm out of time, no more function hdrs

int StringLen(char *p)
{
    int i = 0;
    
    while (*p++ != 0)
    {
        i++;
    }
    
    return i;
}

int StringCmp(char *p1, char *p2)
{
    while (1)
    {
        if (*p1 != *p2)
            break;
        
        if (*p1 == 0)
            return 0;
        p1++, p2++;
    }
    return 1;
}   


UINT
IrlmpGetValueByClassReq(IRDA_MSG *pReqMsg)
{
    UINT            rc = SUCCESS;
    PIRLMP_LINK_CB  pIrlmpCb = GetIrlmpCb(pReqMsg->IRDA_MSG_pIasQuery->irdaDeviceID);
    IRDA_MSG        IMsg;
    
    DEBUGMSG(DBG_IRLMP_IAS, (TEXT("IRLMP: IRLMP_GETVALUEBYCLASS_REQ\n")));

    if (pIrlmpCb == NULL)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: Null IrlmpCb\n")));
        return IRLMP_BAD_DEV_ADDR;
    }

    LOCK_LINK(pIrlmpCb->pIrdaLinkCb);
    
    if (pIrlmpCb->pIasQuery != NULL)
    {
        DEBUGMSG(DBG_ERROR, 
                 (TEXT("IRLMP: ERROR query already in progress\n")));
        rc = IRLMP_IAS_QUERY_IN_PROGRESS;
        
        UNLOCK_LINK(pIrlmpCb->pIrdaLinkCb);            
    }
    else
    {
        // Save the pointer to the query in the control block
        // and then request a connection to the remote IAS LSAP

        // Save it
        pIrlmpCb->pIasQuery             = pReqMsg->IRDA_MSG_pIasQuery;
        pIrlmpCb->AttribLen             = pReqMsg->IRDA_MSG_AttribLen;
        pIrlmpCb->AttribLenWritten      = 0;
        pIrlmpCb->FirstIasRespReceived  = FALSE;
        
        UNLOCK_LINK(pIrlmpCb->pIrdaLinkCb);

        // request connection
        IMsg.Prim                      = IRLMP_CONNECT_REQ;
        IMsg.IRDA_MSG_RemoteLsapSel    = IAS_LSAP_SEL;
        IMsg.IRDA_MSG_LocalLsapSel     = IAS_LOCAL_LSAP_SEL;
        IMsg.IRDA_MSG_pQos             = NULL;
        IMsg.IRDA_MSG_pConnData        = NULL;
        IMsg.IRDA_MSG_ConnDataLen      = 0;
        IMsg.IRDA_MSG_UseTtp           = FALSE;
        RtlCopyMemory(IMsg.IRDA_MSG_RemoteDevAddr,
                      pReqMsg->IRDA_MSG_pIasQuery->irdaDeviceID,
                      IRDA_DEV_ADDR_LEN);
    
        if ((rc = IrlmpConnectReq(&IMsg)) != SUCCESS)
        {
            pIrlmpCb->pIasQuery = NULL;
        }
    }

    return rc;
}

VOID
SendGetValueByClassReq(IRLMP_LSAP_CB *pLsapCb)
{
    IRDA_MSG            *pMsg;
    IAS_CONTROL_FIELD   *pControl;
    int                 ClassNameLen;
    int                 AttribNameLen;
    PIRLMP_LINK_CB      pIrlmpCb = (PIRLMP_LINK_CB) pLsapCb->pIrlmpCb;

    if (pIrlmpCb->pIasQuery == NULL)
    {
        return;
    }

    ClassNameLen = StringLen(pIrlmpCb->pIasQuery->irdaClassName);
    AttribNameLen = StringLen(pIrlmpCb->pIasQuery->irdaAttribName);

    DEBUGMSG(DBG_IRLMP_IAS, (TEXT("IRLMP: Send GetValueByClassReq(%hs, %hs)\n"),
             pIrlmpCb->pIasQuery->irdaClassName, pIrlmpCb->pIasQuery->irdaAttribName));
    
    // Alloc a message for data request that will contain the query
    if ((pMsg = AllocIrdaBuf(IrdaMsgPool)) == NULL)
    {
        ASSERT(0);
        return;
    }

    pMsg->IRDA_MSG_pHdrRead = 
    pMsg->IRDA_MSG_pHdrWrite = pMsg->IRDA_MSG_Header + IRDA_HEADER_LEN;
    
    pMsg->IRDA_MSG_pRead  = \
    pMsg->IRDA_MSG_pWrite = \
    pMsg->IRDA_MSG_pBase  = pMsg->IRDA_MSG_pHdrWrite;
    pMsg->IRDA_MSG_pLimit = pMsg->IRDA_MSG_pBase +
        IRDA_MSG_DATA_SIZE_INTERNAL - 1;    

    // Build the query and then send it in a LAP data req

    pControl = (IAS_CONTROL_FIELD *) pMsg->IRDA_MSG_pRead;

    pControl->Last   = TRUE;
    pControl->Ack    = FALSE;
    pControl->OpCode = IAS_OPCODE_GET_VALUE_BY_CLASS;
    
    *(pMsg->IRDA_MSG_pRead + 1) = (UCHAR) ClassNameLen;
    RtlCopyMemory(pMsg->IRDA_MSG_pRead + 2,
           pIrlmpCb->pIasQuery->irdaClassName, 
           ClassNameLen);
    *(pMsg->IRDA_MSG_pRead + ClassNameLen + 2) = (UCHAR) AttribNameLen;
    RtlCopyMemory(pMsg->IRDA_MSG_pRead + ClassNameLen + 3,
           pIrlmpCb->pIasQuery->irdaAttribName,
           AttribNameLen);

    pMsg->IRDA_MSG_pWrite = pMsg->IRDA_MSG_pRead + ClassNameLen + AttribNameLen + 3;

    pMsg->IRDA_MSG_IrCOMM_9Wire = FALSE;
    
    FormatAndSendDataReq(pLsapCb, pMsg, TRUE);
}

VOID
IasConnectReq(PIRLMP_LINK_CB pIrlmpCb, int RemoteLsapSel)
{
    IRLMP_LSAP_CB           *pLsapCb = GetLsap(pIrlmpCb,
                                               IAS_LSAP_SEL, RemoteLsapSel);

    DEBUGMSG(DBG_IRLMP_IAS, (TEXT("IRLMP: Received IAS connect request\n")));
    
    if (pLsapCb == NULL)
    {
        if (CreateLsap(pIrlmpCb, &pLsapCb) != SUCCESS)
            return;
        pLsapCb->UseTtp = FALSE;
        pLsapCb->pTdiContext = NULL;
        
        pLsapCb->State = LSAP_READY;
        pLsapCb->LocalLsapSel = IAS_LSAP_SEL;
        pLsapCb->RemoteLsapSel = RemoteLsapSel;
        pLsapCb->UserDataLen = 0;
    }
    
    SendCntlPdu(pLsapCb, IRLMP_CONNECT_PDU, IRLMP_ABIT_CONFIRM,
                IRLMP_RSVD_PARM, 0);
}

VOID
IasServerDisconnectReq(IRLMP_LSAP_CB *pLsapCb)
{
    DEBUGMSG(DBG_IRLMP_IAS, (TEXT("IRLMP: Received disconnect request IAS\n")));
    
    CleanupLsap(pLsapCb);
    
    return;
}

VOID
IasClientDisconnectReq(IRLMP_LSAP_CB *pLsapCb, IRLMP_DISC_REASON DiscReason)
{
    IRDA_MSG        IMsg;
    PIRLMP_LINK_CB  pIrlmpCb = (PIRLMP_LINK_CB) pLsapCb->pIrlmpCb;

    if (pIrlmpCb->pIasQuery != NULL)
    {

        pIrlmpCb->pIasQuery = NULL; 

        // Disconnect link
        IrdaTimerRestart(&pIrlmpCb->DiscDelayTimer);
        
        IMsg.Prim = IRLMP_GETVALUEBYCLASS_CONF;
        IMsg.IRDA_MSG_IASStatus = DiscReason;
    
        TdiUp(NULL, &IMsg);
    }
    CleanupLsap(pLsapCb);
}

VOID
IasSendQueryResp(IRLMP_LSAP_CB *pLsapCb, IRDA_MSG *pMsg)
{
    IAS_CONTROL_FIELD   *pCntl = (IAS_CONTROL_FIELD *) pMsg->IRDA_MSG_pRead++;

    if (pCntl->OpCode != IAS_OPCODE_GET_VALUE_BY_CLASS)
    {
        return;// IRLMP_UNSUPPORTED_IAS_OPERATION;
    }

    SendGetValueByClassResp(pLsapCb, pMsg);
}

VOID
IasProcessQueryResp(PIRLMP_LINK_CB pIrlmpCb,
                    IRLMP_LSAP_CB *pLsapCb, IRDA_MSG *pMsg)
{
    IAS_CONTROL_FIELD   *pCntl      = (IAS_CONTROL_FIELD *) pMsg->IRDA_MSG_pRead++;
    UCHAR                ReturnCode;
    int                 ObjID;

    if (pIrlmpCb->pIasQuery == NULL)
    {
        return;
        // return IRLMP_UNSOLICITED_IAS_RESPONSE;
    }

    if (pIrlmpCb->FirstIasRespReceived == FALSE)
    {
        pIrlmpCb->FirstIasRespReceived = TRUE;

        ReturnCode = *pMsg->IRDA_MSG_pRead++;

        if (ReturnCode != IAS_SUCCESS)
        {
            if (ReturnCode == IAS_NO_SUCH_OBJECT)
            {
                pMsg->IRDA_MSG_IASStatus = IRLMP_IAS_NO_SUCH_OBJECT;
            }
            else
            {
                pMsg->IRDA_MSG_IASStatus = IRLMP_IAS_NO_SUCH_ATTRIB;
            }

            // Disconnect LSAP
            SendCntlPdu(pLsapCb,IRLMP_DISCONNECT_PDU,IRLMP_ABIT_REQUEST,
                        IRLMP_USER_REQUEST, 0);

            CleanupLsap(pLsapCb);

            // Disconnect link
            IrdaTimerRestart(&pIrlmpCb->DiscDelayTimer);
            
            pMsg->Prim = IRLMP_GETVALUEBYCLASS_CONF;
            pMsg->IRDA_MSG_pIasQuery = pIrlmpCb->pIasQuery;
            pIrlmpCb->pIasQuery = NULL;

            TdiUp(NULL, pMsg);
            return;
        }

        pIrlmpCb->QueryListLen =  ((int)(*pMsg->IRDA_MSG_pRead++)) << 8;
        pIrlmpCb->QueryListLen += (int) *pMsg->IRDA_MSG_pRead++;        

        // What I am going to do with this?
        ObjID = ((int)(*pMsg->IRDA_MSG_pRead++)) << 8;
        ObjID += (int) *pMsg->IRDA_MSG_pRead++;

        pIrlmpCb->pIasQuery->irdaAttribType = (int) *pMsg->IRDA_MSG_pRead++;
    
        switch (pIrlmpCb->pIasQuery->irdaAttribType)
        {
          case IAS_ATTRIB_VAL_MISSING:
            break;
            
          case IAS_ATTRIB_VAL_INTEGER:
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribInt = 0;
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribInt += 
                ((int) (*pMsg->IRDA_MSG_pRead++) << 24) & 0xFF000000;
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribInt += 
                ((int) (*pMsg->IRDA_MSG_pRead++) << 16) & 0xFF0000;
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribInt += 
                ((int) (*pMsg->IRDA_MSG_pRead++) << 8) & 0xFF00;
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribInt += 
                (int) (*pMsg->IRDA_MSG_pRead++) & 0xFF;
            break;
        
          case IAS_ATTRIB_VAL_BINARY:
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribOctetSeq.Len = 0;
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribOctetSeq.Len += 
                         ((int )(*pMsg->IRDA_MSG_pRead++) << 8) & 0xFF00;
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribOctetSeq.Len += 
                         ((int) *pMsg->IRDA_MSG_pRead++) & 0xFF;        
            break;
            

          case IAS_ATTRIB_VAL_STRING:
            // char set
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribUsrStr.CharSet =
                *pMsg->IRDA_MSG_pRead++;                   
            
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribUsrStr.Len =
                (int) *pMsg->IRDA_MSG_pRead++;
            break;
            
        }

    }
    
    switch (pIrlmpCb->pIasQuery->irdaAttribType)
    {
      case IAS_ATTRIB_VAL_BINARY:    
        while (pMsg->IRDA_MSG_pRead < pMsg->IRDA_MSG_pWrite &&
               pIrlmpCb->AttribLenWritten < pIrlmpCb->AttribLen &&
               pIrlmpCb->AttribLenWritten < (UINT)pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribOctetSeq.Len)
        {   
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribOctetSeq.OctetSeq[pIrlmpCb->AttribLenWritten++] = *pMsg->IRDA_MSG_pRead++;
        }
        
        break;
        
      case IAS_ATTRIB_VAL_STRING:
        while (pMsg->IRDA_MSG_pRead < pMsg->IRDA_MSG_pWrite &&
               pIrlmpCb->AttribLenWritten < pIrlmpCb->AttribLen &&
               pIrlmpCb->AttribLenWritten < (UINT)pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribUsrStr.Len)
        {
            pIrlmpCb->pIasQuery->irdaAttribute.irdaAttribUsrStr.UsrStr[pIrlmpCb->AttribLenWritten++] = *pMsg->IRDA_MSG_pRead++;
        }
    }
    
    if (pCntl->Last == TRUE)
    {
        pMsg->IRDA_MSG_pIasQuery = pIrlmpCb->pIasQuery;
        
        // Done with query
        pIrlmpCb->pIasQuery = NULL;

        // Disconnect LSAP
        SendCntlPdu(pLsapCb,IRLMP_DISCONNECT_PDU,IRLMP_ABIT_REQUEST,
                    IRLMP_USER_REQUEST, 0);

        CleanupLsap(pLsapCb);

        // Disconnect link
        IrdaTimerRestart(&pIrlmpCb->DiscDelayTimer);        

        pMsg->Prim = IRLMP_GETVALUEBYCLASS_CONF;

        if (pIrlmpCb->QueryListLen > 1)
        {
            pMsg->IRDA_MSG_IASStatus = IRLMP_IAS_SUCCESS_LISTLEN_GREATER_THAN_ONE;
        }
        else
        {
            pMsg->IRDA_MSG_IASStatus = IRLMP_IAS_SUCCESS;
        }
    
        TdiUp(NULL, pMsg);
        return;
    }
}

UINT
NewQueryMsg(PIRLMP_LINK_CB pIrlmpCb, LIST_ENTRY *pList, IRDA_MSG **ppMsg)
{
    IRDA_MSG *pMsg;
    
    DEBUGMSG(DBG_IRLMP, (TEXT("+NewQueryMsg(0x%x, 0x%x, 0x%x\r\n"),
        pIrlmpCb, pList, ppMsg));

    if ((*ppMsg = AllocIrdaBuf(IrdaMsgPool)) == NULL)
    {
        pMsg = (IRDA_MSG *) RemoveHeadList(pList);
        
        while (pMsg != (IRDA_MSG *)pList)
        {
            FreeIrdaBuf(IrdaMsgPool, pMsg);
            pMsg = (IRDA_MSG *) RemoveHeadList(pList);            
        }
        return IRLMP_ALLOC_FAILED;
    }
    (*ppMsg)->IRDA_MSG_pHdrRead  = \
    (*ppMsg)->IRDA_MSG_pHdrWrite = (*ppMsg)->IRDA_MSG_Header+IRDA_HEADER_LEN;
        
    (*ppMsg)->IRDA_MSG_pRead  = \
    (*ppMsg)->IRDA_MSG_pWrite = \
    (*ppMsg)->IRDA_MSG_pBase  = (*ppMsg)->IRDA_MSG_pHdrWrite;
    (*ppMsg)->IRDA_MSG_pLimit = (*ppMsg)->IRDA_MSG_pBase +
        IRDA_MSG_DATA_SIZE_INTERNAL - sizeof(IRDA_MSG) - 1;
    
    InsertTailList(pList, &( (*ppMsg)->Linkage) );    

    // reserve space for the IAS control field.
    (*ppMsg)->IRDA_MSG_pWrite += sizeof(IAS_CONTROL_FIELD);   

    DEBUGMSG(DBG_IRLMP, (TEXT("-NewQueryMsg [new msg = 0x%x]\r\n"), *ppMsg));

    return SUCCESS;
}

VOID
SendGetValueByClassResp(IRLMP_LSAP_CB *pLsapCb, IRDA_MSG *pReqMsg)
{  
    int                 ClassNameLen, AttribNameLen;
    CHAR                *pClassName, *pAttribName;
    IRDA_MSG            *pQMsg, *pNextMsg;
    IAS_OBJECT          *pObject;
    IAS_ATTRIBUTE       *pAttrib;
    LIST_ENTRY          QueryList;
    IAS_CONTROL_FIELD   *pControl;
    UCHAR               *pReturnCode;
    UCHAR               *pListLen;
    UCHAR               *pBPtr;
    int                 ListLen = 0;
    BOOLEAN             ObjectFound = FALSE;
    BOOLEAN             AttribFound = FALSE;
    int                 i;
#if DBG
    char                ClassStr[128];
    char                AttribStr[128];
#endif 
    
    DEBUGMSG(DBG_IRLMP, 
             (TEXT("IRLMP: Remote GetValueByClass query received\n")));

    ClassNameLen = (int) *pReqMsg->IRDA_MSG_pRead;
    pClassName = (CHAR *) (pReqMsg->IRDA_MSG_pRead + 1);

    AttribNameLen =  (int) *(pClassName + ClassNameLen);
    pAttribName = pClassName + ClassNameLen + 1;

#if DBG
    RtlCopyMemory(ClassStr, pClassName, ClassNameLen);
    ClassStr[ClassNameLen] = 0;
    RtlCopyMemory(AttribStr, pAttribName, AttribNameLen);
    AttribStr[AttribNameLen] = 0;
#endif

    if (pReqMsg->IRDA_MSG_pWrite != (UCHAR *) (pAttribName + AttribNameLen))
    {
        // The end of the message didn't point to where the end of
        // the parameters.
        
        // LOG ERROR.
        //return IRLMP_BAD_IAS_QUERY_FROM_REMOTE;
        return;
    }

    // The query may require multiple frames to transmit, build a list
    InitializeListHead(&QueryList);

    // Create the first message
    if (NewQueryMsg(pLsapCb->pIrlmpCb, &QueryList, &pQMsg) != SUCCESS)
    {
        ASSERT(0);
        return;
    }

    pReturnCode = pQMsg->IRDA_MSG_pWrite++;
    pListLen = pQMsg->IRDA_MSG_pWrite++;
    pQMsg->IRDA_MSG_pWrite++; // list len get 2 bytes
    
    for (pObject = (IAS_OBJECT *) IasObjects.Flink;
         (LIST_ENTRY *) pObject != &IasObjects;
         pObject = (IAS_OBJECT *) pObject->Linkage.Flink)    
    {
        DEBUGMSG(DBG_IRLMP_IAS, (TEXT("  compare object %hs with %hs\n"),
                 ClassStr, pObject->pClassName));
        
        if ((int)strlen(pObject->pClassName) == ClassNameLen &&
            memcmp(pClassName, pObject->pClassName, ClassNameLen) == 0)
        {
            DEBUGMSG(DBG_IRLMP_IAS, (TEXT("  Object found\n")));
            
            ObjectFound = TRUE;

            pAttrib = pObject->pAttributes;
            while (pAttrib != NULL)
            {
                DEBUGMSG(DBG_IRLMP_IAS, (TEXT(" compare attrib %hs with %hs\n"),
                         pAttrib->pAttribName, AttribStr));
                
                if ((int)strlen(pAttrib->pAttribName) == AttribNameLen &&
                    memcmp(pAttrib->pAttribName, pAttribName, AttribNameLen) == 0)
                {
                    DEBUGMSG(DBG_IRLMP_IAS, (TEXT("  Attrib found   \n")));
                    
                    AttribFound = TRUE;

                    ListLen++;
                    
                    if (pQMsg->IRDA_MSG_pWrite + 1 > pQMsg->IRDA_MSG_pLimit)
                    {
                        // I need 2 bytes for object ID, don't want to
                        // split 16 bit field up 
                        if (NewQueryMsg(pLsapCb->pIrlmpCb, &QueryList,
                                        &pQMsg) != SUCCESS)
                        {
                            ASSERT(0);
                            return;
                        }
                    }
                    *pQMsg->IRDA_MSG_pWrite++ = 
                        (UCHAR) (((1) & 0xFF00) >> 8);
                    *pQMsg->IRDA_MSG_pWrite++ = 
                        (UCHAR) ((1) & 0xFF);
                    
                    if (pQMsg->IRDA_MSG_pWrite > pQMsg->IRDA_MSG_pLimit)
                    {
                        if (NewQueryMsg(pLsapCb->pIrlmpCb, &QueryList,
                                        &pQMsg) != SUCCESS)
                        {
                            ASSERT(0);
                            return;
                        }                        
                    }
                    
                    switch (pAttrib->AttribValType)
                    {
                      case IAS_ATTRIB_VAL_INTEGER:
                        DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: integer query %d\n"),
                                              *((int *) pAttrib->pAttribVal)));
                        
                        if (pQMsg->IRDA_MSG_pWrite + 4 > pQMsg->IRDA_MSG_pLimit)
                        {
                            if (NewQueryMsg(pLsapCb->pIrlmpCb,
                                            &QueryList, &pQMsg) != SUCCESS)
                            {
                                ASSERT(0);
                                return;
                            }
                        }
                        *pQMsg->IRDA_MSG_pWrite++ = IAS_ATTRIB_VAL_INTEGER;
                        *pQMsg->IRDA_MSG_pWrite++ = (UCHAR)
                            ((*((int *) pAttrib->pAttribVal) & 0xFF000000) >> 24);
                        *pQMsg->IRDA_MSG_pWrite++ = (UCHAR)
                            ((*((int *) pAttrib->pAttribVal) & 0xFF0000) >> 16);
                        *pQMsg->IRDA_MSG_pWrite++ = (UCHAR)
                            ((*((int *) pAttrib->pAttribVal) & 0xFF00) >> 8);
                        *pQMsg->IRDA_MSG_pWrite++ = (UCHAR)
                            (*((int *) pAttrib->pAttribVal) & 0xFF); 
                        break;

                      case IAS_ATTRIB_VAL_BINARY:
                      case IAS_ATTRIB_VAL_STRING:
                        if (pQMsg->IRDA_MSG_pWrite + 2 > pQMsg->IRDA_MSG_pLimit)
                        {
                            if (NewQueryMsg(pLsapCb->pIrlmpCb,
                                            &QueryList, &pQMsg) != SUCCESS)
                            {
                                ASSERT(0);
                                return;
                            }
                        }
                        
                        *pQMsg->IRDA_MSG_pWrite++ = (UCHAR) pAttrib->AttribValType;

                        if (pAttrib->AttribValType == IAS_ATTRIB_VAL_BINARY)
                        {
                            *pQMsg->IRDA_MSG_pWrite++ = 
                                  (UCHAR) ((pAttrib->AttribValLen & 0xFF00) >> 8);
                            *pQMsg->IRDA_MSG_pWrite++ = 
                                  (UCHAR) (pAttrib->AttribValLen & 0xFF);;
                        }
                        else
                        {
                            *pQMsg->IRDA_MSG_pWrite++ = 
                                   pAttrib->CharSet;
                            *pQMsg->IRDA_MSG_pWrite++ = 
                                  (UCHAR) pAttrib->AttribValLen;
                        }

                        pBPtr = (UCHAR *) pAttrib->pAttribVal;
                        
                        for (i=0; i < pAttrib->AttribValLen; i++)
                        {
                            if (pQMsg->IRDA_MSG_pWrite > pQMsg->IRDA_MSG_pLimit)
                            {
                                if (NewQueryMsg(pLsapCb->pIrlmpCb,
                                                &QueryList, &pQMsg) != SUCCESS)
                                {
                                    ASSERT(0);
                                    return;
                                }
                            }
                            *pQMsg->IRDA_MSG_pWrite++ = *pBPtr++;
                        }
                        break;
                    }

                    break; // Break out of loop, only look for single 
                           // attrib per object (??)
                }
                pAttrib = pAttrib->pNext;
            }

            if (AttribFound) {
                break;
            }
        }
    }

    // Send the query
    if (!ObjectFound)
    {
        *pReturnCode = IAS_NO_SUCH_OBJECT;
    }
    else
    {
        if (!AttribFound)
        {
            *pReturnCode = IAS_NO_SUCH_ATTRIB;
        }
        else
        {
            *pReturnCode = IAS_SUCCESS;
            *pListLen++ =  (UCHAR) ((ListLen & 0xFF00) >> 8);
            *pListLen = (UCHAR) (ListLen & 0xFF);
        }
    }

    if (!IsListEmpty(&QueryList))
    {
        pQMsg = (IRDA_MSG *) RemoveHeadList(&QueryList);
    }
    else
    {
        pQMsg = NULL;
    }
    
    while (pQMsg)
    {
        if (!IsListEmpty(&QueryList))
        {
            pNextMsg = (IRDA_MSG *) RemoveHeadList(&QueryList);
        }
        else
        {
            pNextMsg = NULL;
        }

        // Build the control field
        pControl = (IAS_CONTROL_FIELD *) pQMsg->IRDA_MSG_pRead;
        pControl->OpCode = IAS_OPCODE_GET_VALUE_BY_CLASS;
        pControl->Ack = FALSE;
        if (pNextMsg == NULL)
        {
            pControl->Last = TRUE;
        }
        else
        {
            pControl->Last = FALSE;
        }

        pQMsg->IRDA_MSG_IrCOMM_9Wire = FALSE;               

        FormatAndSendDataReq(pLsapCb, pQMsg, TRUE);

        pQMsg = pNextMsg;
    }
}

IAS_OBJECT *
IasGetObject(CHAR *pClassName)
{
    IAS_OBJECT  *pObject;
    
    for (pObject = (IAS_OBJECT *) IasObjects.Flink;
         (LIST_ENTRY *) pObject != &IasObjects;
         pObject = (IAS_OBJECT *) pObject->Linkage.Flink)    
    {
        if (StringCmp(pObject->pClassName, pClassName) == 0)
        {
            return pObject;
        }
    }
    return NULL;
}

UINT
IasAddAttribute(IAS_SET *pIASSet, PVOID *pAttribHandle)
{
    IAS_OBJECT      *pObject;
    IAS_ATTRIBUTE   *pAttrib;
    CHAR            *pClassName;
    CHAR            ClassNameLen;
    CHAR            *pAttribName;
    CHAR            AttribNameLen;
    int             AttribValLen;
    void            *pAttribVal;
    UINT            cAttribs = 0;
    *pAttribHandle  = NULL;
    
    NdisAcquireSpinLock(&SpinLock);

    if ((pObject = IasGetObject(pIASSet->irdaClassName)) == NULL)
    {
        if (IRDA_ALLOC_MEM(pObject, sizeof(IAS_OBJECT), MT_IRLMP_IAS_OBJECT)
            == NULL)
        {
            NdisReleaseSpinLock(&SpinLock);
            return IRLMP_ALLOC_FAILED;
        }

        ClassNameLen = StringLen(pIASSet->irdaClassName) + 1;
        
        if (IRDA_ALLOC_MEM(pClassName, ClassNameLen, MT_IRLMP_IAS_CLASSNAME)
            == NULL)
        {
            NdisReleaseSpinLock(&SpinLock);
            IRDA_FREE_MEM(pObject);
            ASSERT(0);
            return IRLMP_ALLOC_FAILED;
        }

        RtlCopyMemory(pClassName, pIASSet->irdaClassName, ClassNameLen);
        
        pObject->pClassName     = pClassName;
        pObject->pAttributes    = NULL;    

        InsertTailList(&IasObjects, &pObject->Linkage);    
    }

    // Does the attribute already exist?
    for (pAttrib = pObject->pAttributes; pAttrib != NULL; 
         pAttrib = pAttrib->pNext)
    {
        if (StringCmp(pAttrib->pAttribName, pIASSet->irdaAttribName) == 0)
        {
            break;
        }
        cAttribs++;
    }
    
    if (pAttrib != NULL)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IRLMP: Attribute alreay exists\r\n")));
        NdisReleaseSpinLock(&SpinLock);
        return IRLMP_IAS_ATTRIB_ALREADY_EXISTS;
    }
    else
    {       
        // Only allowed to add 256 attributes to an object.
        if (cAttribs >= 256)
        {
            NdisReleaseSpinLock(&SpinLock);
            return IRLMP_IAS_MAX_ATTRIBS_REACHED;
        }

        if (IRDA_ALLOC_MEM(pAttrib, sizeof(IAS_ATTRIBUTE), MT_IRLMP_IAS_ATTRIB)
             == NULL)
        {
            goto iaa_fail;
        }

        AttribNameLen = StringLen(pIASSet->irdaAttribName) + 1;
        
        if (IRDA_ALLOC_MEM(pAttribName, AttribNameLen, MT_IRLMP_IAS_ATTRIBNAME)
            == NULL)
        {
            goto iaa_fail_attrib;
        }

        RtlCopyMemory(pAttribName, pIASSet->irdaAttribName, AttribNameLen);
        
    }

    switch (pIASSet->irdaAttribType)
    {
      case IAS_ATTRIB_VAL_INTEGER:
        AttribValLen = sizeof(int);
        if (IRDA_ALLOC_MEM(pAttribVal, AttribValLen, MT_IRLMP_IAS_ATTRIBVAL)
            == NULL)
        {
            goto iaa_fail_name;
        }

        *((int *) pAttribVal) = pIASSet->irdaAttribute.irdaAttribInt;
        break;
        
      case IAS_ATTRIB_VAL_BINARY:
        AttribValLen = pIASSet->irdaAttribute.irdaAttribOctetSeq.Len;
        if (IRDA_ALLOC_MEM(pAttribVal, AttribValLen, MT_IRLMP_IAS_ATTRIBVAL)
            == NULL)
        {
            goto iaa_fail_name;
        }

        RtlCopyMemory(pAttribVal, pIASSet->irdaAttribute.irdaAttribOctetSeq.OctetSeq,
               AttribValLen);
        break;
        
      case IAS_ATTRIB_VAL_STRING:
        AttribValLen = pIASSet->irdaAttribute.irdaAttribUsrStr.Len;
        if (IRDA_ALLOC_MEM(pAttribVal, AttribValLen, MT_IRLMP_IAS_ATTRIBVAL)
            == NULL)
        {
            goto iaa_fail_name;
        }

        RtlCopyMemory(pAttribVal, pIASSet->irdaAttribute.irdaAttribUsrStr.UsrStr, 
               AttribValLen);
        break;
        
      default:
        ASSERT(0);
        goto iaa_fail_name;
        break;
    }
    
    pAttrib->pAttribName        = pAttribName;
    pAttrib->pAttribVal         = pAttribVal;
    pAttrib->AttribValLen       = AttribValLen;
    pAttrib->AttribValType      = (UCHAR) pIASSet->irdaAttribType;
    pAttrib->CharSet            = pIASSet->irdaAttribute.irdaAttribUsrStr.CharSet;
    
    pAttrib->pNext       = pObject->pAttributes;
    pObject->pAttributes = pAttrib;
    *pAttribHandle = pAttrib;

    DEBUGMSG(DBG_IRLMP_IAS, (TEXT("IRLMP: Added attrib(%x) %hs to class %hs\n"),
             pAttrib, pAttrib->pAttribName, pObject->pClassName));
    
    NdisReleaseSpinLock(&SpinLock);

    return SUCCESS;

//
// Cleanup after failures
//
iaa_fail_name:
    IRDA_FREE_MEM(pAttribName);

iaa_fail_attrib:
    IRDA_FREE_MEM(pAttrib);

iaa_fail:
    NdisReleaseSpinLock(&SpinLock);
    return IRLMP_ALLOC_FAILED;
}

VOID
IasDelAttribute(PVOID AttribHandle)
{
    IAS_OBJECT      *pObject;
    IAS_ATTRIBUTE   *pAttrib, *pPrevAttrib;
    BOOLEAN         First;

    NdisAcquireSpinLock(&SpinLock);

    DEBUGMSG(DBG_IRLMP_IAS, (TEXT("IRLMP: Delete attribHandle %x\n"),
             AttribHandle));

    for (pObject = (IAS_OBJECT *) IasObjects.Flink;
         (LIST_ENTRY *) pObject != &IasObjects;
         pObject = (IAS_OBJECT *) pObject->Linkage.Flink)
    {
        First = TRUE;
        pPrevAttrib = NULL;
        for (pAttrib = pObject->pAttributes;
             pAttrib != NULL;
             pAttrib = pAttrib->pNext)
        {
            if (pAttrib == AttribHandle)
            {
                BOOLEAN DelObject = FALSE;

                DEBUGMSG(DBG_IRLMP_IAS, (TEXT("IRLMP: attrib %hs deleted\n"),
                         pAttrib->pAttribName));

                if (pAttrib->pNext == NULL && First)
                {
                    DelObject = TRUE;
                }

                if (pPrevAttrib)
                {
                    pPrevAttrib->pNext = pAttrib->pNext;
                }
                else
                {
                    pObject->pAttributes = pAttrib->pNext;
                }


                IRDA_FREE_MEM(pAttrib->pAttribName);
                IRDA_FREE_MEM(pAttrib->pAttribVal);

                IRDA_FREE_MEM(pAttrib);

                if (DelObject)
                {
                    DEBUGMSG(DBG_IRLMP_IAS, (TEXT("IRLMP: No attributes associated with class %hs, deleting\n"),
                            pObject->pClassName));

                    RemoveEntryList(&pObject->Linkage);
                    IRDA_FREE_MEM(pObject->pClassName);
                    IRDA_FREE_MEM(pObject);
                }

                goto done;
            }

            First = FALSE;
            pPrevAttrib = pAttrib;
        }
    }

done:

    NdisReleaseSpinLock(&SpinLock);
}

extern void UpdateNickName(void);

VOID
UpdateIasDeviceName(PIRLMP_LINK_CB pIrlmpCb)
{
    IAS_SET *pIASSet;
    int      rc;

    UpdateNickName();

    IasDelAttribute(pIrlmpCb->hAttribDeviceName);

    // Add device info to IAS        
    if (pIASSet = (IAS_SET *)IrdaAlloc(sizeof(IAS_SET) + 128, 0)) {
        RtlCopyMemory(pIASSet->irdaClassName, IasClassName_Device,
                      IasClassNameLen_Device+1);
        RtlCopyMemory(pIASSet->irdaAttribName, IasAttribName_DeviceName,
                      IasAttribNameLen_DeviceName+1);
        pIASSet->irdaAttribType = IAS_ATTRIB_VAL_STRING;
        RtlCopyMemory(pIASSet->irdaAttribute.irdaAttribUsrStr.UsrStr, NickName, NickNameLen);
        pIASSet->irdaAttribute.irdaAttribUsrStr.CharSet = CharSet;
        pIASSet->irdaAttribute.irdaAttribUsrStr.Len = NickNameLen;        
        rc = IasAddAttribute(pIASSet, &pIrlmpCb->hAttribDeviceName);
        ASSERT(rc == 0);

        IrdaFree(pIASSet);
    }
    
    return;
}

/*
VOID
IasDeleteObject(CHAR *pClassName)
{
    IAS_OBJECT      *pObject;
    IAS_ATTRIBUTE   *pAttrib, *pNextAttrib;
    
    NdisAcquireSpinLock(&SpinLock);
    
    if ((pObject = IasGetObject(pClassName)) == NULL)
    {
    NdisReleaseSpinLock(&SpinLock);
        return;
    }
    
    pAttrib = pObject->pAttributes;
    while(pAttrib != NULL)
    {
        pNextAttrib = pAttrib->pNext;
        
        DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Delete attrib %hs\n"),
                 pAttrib->pAttribName));
        IRDA_FREE_MEM(pAttrib->pAttribName);
        IRDA_FREE_MEM(pAttrib->pAttribVal);
        
        IRDA_FREE_MEM(pAttrib);

        pAttrib = pNextAttrib;
    }

    RemoveEntryList(&pObject->Linkage);    
    
    DEBUGMSG(DBG_IRLMP, (TEXT("IRLMP: Delete class %hs\n"),
             pObject->pClassName));
    IRDA_FREE_MEM(pObject->pClassName);
    IRDA_FREE_MEM(pObject);
    
    NdisReleaseSpinLock(&SpinLock);
}
*/

#if DBG
TCHAR *LSAPStateStr[] =
{
    TEXT("LSAP_DISCONNECTED"),
    TEXT("LSAP_IRLAP_CONN_PEND"),
    TEXT("LSAP_LMCONN_CONF_PEND"),
    TEXT("LSAP_CONN_RESP_PEND"),
    TEXT("LSAP_CONN_REQ_PEND"),
    TEXT("LSAP_EXCLUSIVEMODE_PEND"),
    TEXT("LSAP_MULTIPLEXEDMODE_PEND"),
    TEXT("LSAP_READY"),
    TEXT("LSAP_NO_TX_CREDIT")
};

TCHAR *LinkStateStr[] =
{
    TEXT("LINK_DISCONNECTED"),
    TEXT("LINK_DISCONNECTING"),
    TEXT("LINK_IN_DISCOVERY"),
    TEXT("LINK_CONNECTING"),
    TEXT("LINK_READY")
};
#endif

void
IRLMP_PrintState()
{
/*    
    IRLMP_LSAP_CB       *pLsapCb;

    // IRLMP control block
#if DBG    
    RETAILMSG(1, (TEXT("IRLMP link state %s\n"), 
                 LinkStateStr[pIrlmpCb->LinkState]));
#else    
    RETAILMSG(1, (TEXT("IRLMP link state %d\n"), 
                 pIrlmpCb->LinkState));
#endif
    for (pLsapCb = (IRLMP_LSAP_CB *) pIrlmpCb->LsapCbList.Flink;
         (LIST_ENTRY *) pLsapCb != &pIrlmpCb->LsapCbList;
         pLsapCb = (IRLMP_LSAP_CB *) pLsapCb->Linkage.Flink)
    {
#if DBG        
        RETAILMSG(1,
      (TEXT("  LSAP(l%d,r%d) state %s. Credit: avail %d, localTx %d, remoteTx %d\n"),
       pLsapCb->LocalLsapSel, pLsapCb->RemoteLsapSel, 
       LSAPStateStr[pLsapCb->State], 
       pLsapCb->AvailableCredit, 
       pLsapCb->LocalTxCredit, pLsapCb->RemoteTxCredit));
#else
        RETAILMSG(1,
      (TEXT("  LSAP(l%d,r%d) state %d. Credit: avail %d, localTx %d, remoteTx %d\n"),
       pLsapCb->LocalLsapSel, pLsapCb->RemoteLsapSel, 
       pLsapCb->State, 
       pLsapCb->AvailableCredit, 
       pLsapCb->LocalTxCredit, pLsapCb->RemoteTxCredit));
#endif        
        
    }
    
    IrlapPrintState();
    */
}
