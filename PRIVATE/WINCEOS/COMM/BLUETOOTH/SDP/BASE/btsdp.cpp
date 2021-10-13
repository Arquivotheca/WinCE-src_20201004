//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

   BTSDP.CPP

Abstract:

    This module implemnts BTh SDP core spec.



   Doron Holan 

   Notes:

   Environment:

   Kernel mode only


Revision History:

*/

#include "common.h"
#include "sdplib.h"

#define SDPI_TAG 'IpdS'

extern SdpDatabase *pSdpDB;
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
SdpInterface* pSdp = NULL;
#endif

USHORT SDP_DEFAULT_MTU = 1024;

#define CONFIG_RETRY_DEFAULT    (5)

BOOLEAN ContinuationState::Read(UCHAR *pStream, ULONG streamSize)
{
    // PAGED_CODE();

    size = *pStream;

    // check for out of range size
    if (size > 16) {
        return FALSE; 
    }

    // cont state always at end, so size must match
    if (size != (streamSize - 1)) {
        return FALSE; 
    }

    if (size) {
        memcpy(state, pStream + 1, size);
    }

    return TRUE;
}

void ContinuationState::Write(UCHAR *pStream)
{
    // PAGED_CODE();

    *pStream  = size;
    if (size) {
        memcpy(pStream + 1, state, size); 
    }
}

void ContinuationState::Init(PVOID p)
{ 
    size = sizeof(p);
    RtlCopyMemory(state, &p, size); 
}

PVOID SdpPDU::Read(PVOID pBuffer)
/*++
 
    Reads an SDP PDU header from the stream and returns an unaligned pointer
    to the beginning of the data in the packet
   
    PDU format is
    byte 1:  PDU id
    byte 2-3:  PDU transaction id
    byte 4-5:  PDU parameter length (the remaining size of the buffer)
   
    PDU is in network byte order so we must byte swap the >1 byte values so that
    they make sense
     
  --*/
{
    PUCHAR stream = (PUCHAR) pBuffer;

    pduId = *stream;
    stream++;

    RtlRetrieveUshort(&transactionId, stream);
    transactionId = RtlUshortByteSwap(transactionId);
    stream += sizeof(transactionId);

    RtlRetrieveUshort(&parameterLength, stream);
    parameterLength = RtlUshortByteSwap(parameterLength);
    stream += sizeof(parameterLength);

    return (PVOID) stream;
}

PUCHAR SdpPDU::Write(PUCHAR pBuffer)
/*++

    Writes out an SDP PDU header to the unaligned buffer.
    
  --*/
{
#if DBG
    PUCHAR pOrig = pBuffer;
#endif
    USHORT tmp;

    *pBuffer = pduId;
    pBuffer++;

    tmp = RtlUshortByteSwap(transactionId);
    RtlCopyMemory(pBuffer, &tmp, sizeof(tmp));
    pBuffer += sizeof(tmp);

#if UPF
    HANDLE hKey;
    if (NT_SUCCESS(BthPort_OpenKey(NULL,
                                   STR_REG_MACHINE STR_SOFTWARE_KEYW,
                                   &hKey))) {
        ULONG tmp = 0;
        if (NT_SUCCESS(QueryKeyValue(hKey,
                                     L"InvalidPDU",
                                     &tmp,
                                     sizeof(ULONG))) && tmp != 0) {
            SdpPrint(0, ("***UPF*** using an invalid pdu size (orig %d, override %d) for search\n",
                         (ULONG) parameterLength, (ULONG) parameterLength * 2));
            parameterLength *= 2;
        }

        ZwClose(hKey);
    }
#endif // UPF

    tmp = RtlUshortByteSwap(parameterLength);
    RtlCopyMemory(pBuffer, &tmp, sizeof(tmp));
    pBuffer += sizeof(tmp);

#if DBG
    //
    // Make sure that we did our pointer math correctly
    //
    ASSERT(pBuffer - pOrig == sizeof(*this));
#endif DBG
    
    return pBuffer;
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
NTSTATUS
SendBrbCompletion(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    )
/*++

    Simple completion routine which sets an event so we can send an irp synch

  --*/
{
    if (Context != NULL) {
        KeSetEvent((PKEVENT) Context, 0, FALSE);
    }
    return STATUS_MORE_PROCESSING_REQUIRED;
}

SdpInterface::SdpInterface()
{
    IoInitializeRemoveLock(&m_RemoveLock, 'DSTB', 0, 0);
    IoAcquireRemoveLock(&m_RemoveLock, (PVOID)-1);
}

SdpInterface::~SdpInterface()
{
    SdpConnection *pCon;
    CList<SdpConnection, FIELD_OFFSET(SdpConnection, entry)> list;

    SdpPrint(SDP_DBG_CONFIG_TRACE, ("~SdpInterface\n"));

    //
    // There should never be any connections in our list when we are destroyed
    // because they should have all been cleared out when we were Stop()'ed
    //
    m_SdpConnectionList.Lock();

    ASSERT(m_SdpConnectionList.IsEmpty());

    while (!m_SdpConnectionList.IsEmpty()) {
        pCon = m_SdpConnectionList.RemoveHead();
        list.AddTail(pCon);
    }
    m_SdpConnectionList.Unlock();

    while (!list.IsEmpty()) {
        pCon = list.RemoveHead();

        


        delete pCon;
    }
}

NTSTATUS SdpInterface::Start(BThDevice *pBthDevice)
{
    m_pDevice = pBthDevice;
    m_pDevice->AddRef();
    return STATUS_SUCCESS;
}

NTSTATUS SdpInterface::Stop()
/*++

    Iterates over the active connection list and terminates each one

  --*/
{
    


    m_pDevice->Release();

    _DbgPrintF(DBG_PNP_INFO, ("SDP: waiting on remlock %p\n", &m_RemoveLock));

    IoReleaseRemoveLockAndWait(&m_RemoveLock, (PVOID)-1);
    return STATUS_SUCCESS;
}

SdpConnection*
SdpInterface::FindConnection(
    PVOID hConnection
    )
/*++

    Assumes that the list has been locked.  Will return an SdpConnection based
    on the passed in hConnection.

  --*/
{
    SdpConnection *pCon;

    for (pCon = m_SdpConnectionList.GetHead();
         pCon != NULL;
         pCon = m_SdpConnectionList.GetNext(pCon)) {

        if (pCon->hConnection == hConnection) {
            SdpPrint(SDP_DBG_CON_INFO,
                     ("found SdpConnection %p for connection %p\n",
                      pCon, hConnection));
            return pCon;
        }
    }

    SdpPrint(SDP_DBG_CON_WARNING,
             ("found no SdpConnection for connection %p\n", hConnection));

    return NULL;
}

SdpConnection*
SdpInterface::FindConnection(
    const bt_addr& address,
    BOOLEAN onlyServer
    )
/*++

    Assumes that the list has been locked.  Will return an SdpConnection based
    on the passed in address.   If onlyServer is TRUE, then an SdpConnection
    will only be returned f

  --*/
{
    SdpConnection *pCon;

    for (pCon = m_SdpConnectionList.GetHead();
         pCon != NULL;
         pCon = m_SdpConnectionList.GetNext(pCon)) {

        if (pCon->btAddress == address &&
            ( !onlyServer || (onlyServer && pCon->IsServer()) ) ) {
            SdpPrint(SDP_DBG_CON_INFO,
                     ("found SdpConnection %p for address %04x%08x\n",
                     pCon, GET_NAP(address), GET_SAP(address)));
            return pCon;
        }
    }

    SdpPrint(SDP_DBG_CON_WARNING,
             ("found no SdpConnection for address %04x%08x\n",
             GET_NAP(address), GET_SAP(address)));

    return NULL;
}

void
SdpInterface::FireWmiEvent(
    SDP_SERVER_LOG_TYPE Type,
    bt_addr *pAddress,
    USHORT Mtu
    )
/*++

    Fires a WMI event if SDP_LOGGING is defined

  --*/
{
#ifdef SDP_LOGGING
    PSDP_SERVER_LOG_INFO pLog;

    pLog = (PSDP_SERVER_LOG_INFO)
        ExAllocatePoolWithTag(PagedPool, sizeof(*pLog), SDPI_TAG);

    if (pLog) {
        RtlZeroMemory(pLog, sizeof(*pLog));

        pLog->type = Type;
        if (pAddress) {
            pLog->info.btAddress = *pAddress;
        }
        pLog->mtu = Mtu;
        pLog->dataLength = 0;

        WmiFireEvent(m_pDevice->m_Struct.FunctionalDeviceObject,
                     (LPGUID) &GUID_BTHPORT_WMI_SDP_SERVER_LOG_INFO,
                     0,
                     sizeof(*pLog),
                     pLog);
    }
#endif SDP_LOGGING
}

NTSTATUS SdpInterface::SendBrb(PBRB pBrb)
/*++

    Sends a BRB synchronously down the stack.

  --*/
{
    NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;

    PIRP irp =
        IoAllocateIrp(m_pDevice->m_Struct.FunctionalDeviceObject->StackSize,
                      FALSE);

    if (irp) {
        PIO_STACK_LOCATION stack;
        KEVENT event;

        stack = IoGetNextIrpStackLocation(irp);
        stack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
        stack->Parameters.DeviceIoControl.IoControlCode =
            IOCTL_INTERNAL_BTH_SUBMIT_BRB;
        stack->Parameters.Others.Argument1 = pBrb;

        KeInitializeEvent(&event, SynchronizationEvent, FALSE);

        IoSetCompletionRoutine(irp,
                               SendBrbCompletion,
                               (PVOID) &event,
                               TRUE,
                               TRUE,
                               TRUE);

        status = IoCallDriver(m_pDevice->m_Struct.FunctionalDeviceObject, irp);

        if (status == STATUS_PENDING) {
            SdpPrint(SDP_DBG_BRB_INFO,
                     ("SendBrb pended, waiting on event %p for BRB %p (type %d)\n",
                      &event, pBrb, pBrb->BrbHeader.Type));

            KeWaitForSingleObject(&event,
                                  Executive,
                                  KernelMode,
                                  FALSE,
                                  NULL);

            SdpPrint(SDP_DBG_BRB_INFO,
                     ("SendBrb pended, end wait on event %p\n", &event));

            status = irp->IoStatus.Status;
        }

        IoFreeIrp(irp);
    }
    else {
        SdpPrint(SDP_DBG_BRB_ERROR, ("SendBrb could not alloc an irp\n"));
    }

    SdpPrint(SDP_DBG_BRB_INFO, ("SendBrb (%p - %d) status 0x%x\n",
             pBrb, pBrb->BrbHeader.Type, status));

    return status;
}

NTSTATUS
SdpInterface::SendBrbAsync(
    PIRP pIrp,
    PBRB pBrb,
    PIO_COMPLETION_ROUTINE Routine,
    PVOID pContext,
    PVOID pArg1,
    PVOID pArg2,
    PVOID pArg3,
    PVOID pArg4
    )
{                                                                     
    PIO_STACK_LOCATION stack;
    KEVENT event;
    NTSTATUS status;

    SdpPrint(SDP_DBG_BRB_INFO, ("SendBrbAsync, irp %p, brb %p, routine %p\n",
             pIrp, pBrb, Routine));

    IoReuseIrp(pIrp, STATUS_SUCCESS);

    stack = IoGetNextIrpStackLocation(pIrp);
    stack->Parameters.Others.Argument1 = pArg1;
    stack->Parameters.Others.Argument2 = pArg2;
    stack->Parameters.Others.Argument3 = pArg3;
    stack->Parameters.Others.Argument4 = pArg4;
    IoSetNextIrpStackLocation(pIrp);

    stack = IoGetNextIrpStackLocation(pIrp);
    stack->Parameters.Others.Argument1 = pBrb;
    stack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    stack->Parameters.DeviceIoControl.IoControlCode =
        IOCTL_INTERNAL_BTH_SUBMIT_BRB;

    IoSetCompletionRoutine(pIrp,
                           Routine,
                           pContext,
                           TRUE,
                           TRUE,
                           TRUE);

    status = IoCallDriver(m_pDevice->m_Struct.FunctionalDeviceObject, pIrp);

    SdpPrint(SDP_DBG_BRB_INFO, ("SendBrbAsync returns 0x%x\n", status));

    return status;
}
#endif // UNDER_CE

NTSTATUS
SdpInterface::VerifyServiceSearch(
    SdpQueryUuid *pUuids,
    UCHAR *pMaxUuids
    )
{
    //
    // Verify that the entries are all well formed
    //
    GUID zeroGuid;

    RtlZeroMemory(&zeroGuid, sizeof(GUID));

    UCHAR i = 0;
    for (i = 0; i < MAX_UUIDS_IN_QUERY; i++, (*pMaxUuids)++) 
    {
#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
        if (0 == memcmp(&pUuids[i].u.uuid128,&zeroGuid,sizeof(GUID)))
            break;
#else
        if (RtlCompareMemory(&pUuids[i].u.uuid128,
                             &zeroGuid,
                             sizeof(GUID)) == sizeof(GUID)) {
            break;
        }
#endif
        if (pUuids[i].uuidType != SDP_ST_UUID128 &&
            pUuids[i].uuidType != SDP_ST_UUID32  &&
            pUuids[i].uuidType != SDP_ST_UUID16) {
            return STATUS_INVALID_PARAMETER;
        }
    }

    if (*pMaxUuids == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

PSDP_NODE
SdpInterface::CreateServiceSearchTree(
    SdpQueryUuid *pUuids,
    UCHAR maxUuids
    )
/*++

    Given an array of SdpQueryUuids, create an SDP tree that represents the
    data within it so that later we can create a stream.
   
  --*/
{
    PSDP_NODE pTree;
    PSDP_NODE pNode, pSequence;

    pTree = SdpCreateNodeTree();
    if (pTree == NULL) {
        return NULL;
    }

    pSequence = SdpCreateNodeSequence();
    if (pSequence == NULL) {
        SdpFreeTree(pTree);
        return NULL;
    }

    SdpAppendNodeToContainerNode(pTree, pSequence);

    ULONG invalidUuid = 0;

#if UPF
    HANDLE hKey;

    if (NT_SUCCESS(BthPort_OpenKey(NULL,
                                   STR_REG_MACHINE STR_SOFTWARE_KEYW,
                                   &hKey))) {
        ULONG tmp = 0;
        if (NT_SUCCESS(QueryKeyValue(hKey,
                                     L"InvalidUUID",
                                     &tmp,
                                     sizeof(ULONG))) && tmp != 0) {
            invalidUuid = 1;
            SdpPrint(0, ("***UPF*** using an invalid UUID for search\n"));
        }

        ZwClose(hKey);
    }
#endif // UPF

    for (UCHAR i = 0; i < maxUuids; i++) {
        if (pUuids[i].uuidType == SDP_ST_UUID128) {
            pNode = SdpCreateNodeUUID128(&pUuids[i].u.uuid128);
        }
        else if (pUuids[i].uuidType == SDP_ST_UUID32) {
            if (invalidUuid) {
                pNode = SdpCreateNodeUInt32(pUuids[i].u.uuid32);
            }
            else {
                pNode = SdpCreateNodeUUID32(pUuids[i].u.uuid32);
            }
        }
        else {
            // SDP_ST_UUID16
            if (invalidUuid) {
                pNode = SdpCreateNodeUInt16(pUuids[i].u.uuid16);
            }
            else {
                pNode = SdpCreateNodeUUID16(pUuids[i].u.uuid16);
            }
        }

        if (pNode == NULL) {
            SdpFreeTree(pTree);
            pTree = NULL;
            break;
        }
        else {
            SdpAppendNodeToContainerNode(pSequence, pNode);
        }
    }

    return pTree;
}

NTSTATUS
SdpInterface::VerifyAttributeSearch(
    SdpAttributeRange *pRange,
    ULONG *pMaxRange,
    ULONG_PTR bufferLength
    )
{
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    *pMaxRange = 0;
#endif

    //
    // Must be in even mulitples of SdpAttributeRange
    //
    if ((bufferLength % sizeof(SdpAttributeRange)) != 0) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    *pMaxRange = (ULONG) (bufferLength / sizeof(SdpAttributeRange));
#endif

    ULONG i = 0;
    USHORT minAttrib = 0;

    for ( ; i < *pMaxRange; i++) {
        if (pRange[i].maxAttribute < pRange[i].minAttribute) {
            return STATUS_INVALID_PARAMETER;
        }

        //
        // We now know that pRange[i].minAttribute <= pRange[i].maxAttribute
        // 
        if (i != 0 && pRange[i].minAttribute <= minAttrib) {
            return STATUS_INVALID_PARAMETER;
        }

        minAttrib = pRange[i].maxAttribute; 
    }

    return STATUS_SUCCESS;
}

PSDP_NODE 
SdpInterface::CreateAttributeSearchTree(
    SdpAttributeRange *pRange,
    ULONG maxRange
    )
/*++

    Given an array of SdpAttributeRange elements, convert them into an SDP tree
    so that we can later convert the tree into a stream
    
  --*/
{
    ULONG i = 0;

    PSDP_NODE pTree;
    PSDP_NODE pNode, pSequence;

    pTree = SdpCreateNodeTree();
    if (pTree == NULL) {
        return NULL;
    }

    pSequence = SdpCreateNodeSequence();
    if (pSequence == NULL) {
        SdpFreeTree(pTree);
        return NULL;
    }

    SdpAppendNodeToContainerNode(pTree, pSequence);

    for ( ; i < maxRange; i++) {
        if (pRange[i].minAttribute == pRange[i].maxAttribute) {
            pNode = SdpCreateNodeUInt16(pRange[i].minAttribute);
        }
        else {
            ULONG tmp = pRange[i].minAttribute;

            //
            // move the value into the high word
            //
            tmp <<= 16;
            pNode = SdpCreateNodeUInt32(tmp | pRange[i].maxAttribute );
       }

        if (pNode == NULL) {
            SdpFreeTree(pTree);
            pTree = NULL;
            break;
        }

        SdpAppendNodeToContainerNode(pSequence, pNode);
    }

    return pTree;
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
NTSTATUS
SdpInterface::AcquireConnectionInternal(
    SdpConnection *pCon,
    PVOID tag
    )
{
    if (pCon) {
        NTSTATUS status = STATUS_SUCCESS;
        KIRQL irql;

        pCon->LockState(&irql);
        if (pCon->channelState == SdpChannelStateClosed) {
            status = STATUS_DEVICE_NOT_CONNECTED;
        }
        pCon->UnlockState(irql);

        if (NT_SUCCESS(status)) {
            return pCon->Acquire(tag);
        }

        return status;
    }
    else {
        return STATUS_INVALID_PARAMETER;
    }
}

NTSTATUS
SdpInterface::AcquireConnection(
    PVOID hConnection,
    SdpConnection** ppCon,
    PVOID tag
    )
/*++

    Finds an active SdpConnection based on the handle to the connection.  If
    a valid connection is returned, it will have been Acquired with tag

  --*/
{
    NTSTATUS status;

    m_SdpConnectionList.Lock();
    *ppCon = FindConnection(hConnection);
    status = AcquireConnectionInternal(*ppCon, tag);
    m_SdpConnectionList.Unlock();

    return status;
}

NTSTATUS
SdpInterface::AcquireConnection(
    const bt_addr& btAddress,
    SdpConnection** ppCon,
    PVOID tag
    )
/*++

    Finds an active SdpConnection based on the address of the connection.  If
    a valid connection is returned, it will have been Acquired with tag

  --*/
{
    NTSTATUS status;

    m_SdpConnectionList.Lock();
    *ppCon = FindConnection(btAddress);
    status = AcquireConnectionInternal(*ppCon, tag);
    m_SdpConnectionList.Unlock();

    return status;
}

NTSTATUS SdpInterface::Connect(PIRP pIrp, ULONG_PTR& info)
{
    SdpConnection *pCon, *pConFind;
    BthSdpConnect tmpConnect, *pConnect;
    BthConnect *pBthConnect;
    PIO_STACK_LOCATION stack;
    HANDLE hConnection;
    NTSTATUS status;
    ULONG inLen, outLen;
    KIRQL irql;

    hConnection = NULL;
    stack = IoGetCurrentIrpStackLocation(pIrp);
    info = 0;
    inLen =  stack->Parameters.DeviceIoControl.InputBufferLength;
    outLen =  stack->Parameters.DeviceIoControl.OutputBufferLength;


    //
    // DJH:  remove the check for BthConnect later
    //
    CAssertF(sizeof(BthSdpConnect) != sizeof(BthConnect));
    if ((inLen != sizeof(BthSdpConnect) && outLen != sizeof(BthSdpConnect)) &&
        (inLen != sizeof(BthConnect) && outLen != sizeof(BthConnect))) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (inLen == sizeof(BthSdpConnect)) {
        pConnect = (BthSdpConnect*) pIrp->AssociatedIrp.SystemBuffer;
    }
    else {
        pBthConnect = (BthConnect*) pIrp->AssociatedIrp.SystemBuffer;

        RtlZeroMemory(&tmpConnect, sizeof(tmpConnect));
        tmpConnect.btAddress = pBthConnect->btAddress;
        tmpConnect.fSdpConnect = 0;
        tmpConnect.requestTimeout = SDP_REQUEST_TO_DEFAULT;

        pConnect = &tmpConnect;
    }

    if ((pConnect->requestTimeout != SDP_REQUEST_TO_DEFAULT) &&
        (pConnect->requestTimeout < SDP_REQUEST_TO_MIN ||
         pConnect->requestTimeout > SDP_REQUEST_TO_MAX)) {
        return STATUS_INVALID_PARAMETER;
    }

    if (pConnect->fSdpConnect & ~SDP_CONNECT_VALID_FLAGS) {
        return STATUS_INVALID_PARAMETER;
    }

    if (pConnect->fSdpConnect & SDP_CONNECT_CACHE) {
        SdpPrint(SDP_DBG_CONFIG_WARNING,
                 ("Connect, cached searches not implemented yet!\n"));
    }

    pCon = new (NonPagedPool, SDPI_TAG) SdpConnection(m_pDevice, TRUE);
    if (pCon == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    else if (pCon->Init() == FALSE) {
        delete pCon;
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    m_SdpConnectionList.Lock();

    pConFind = FindConnection(pConnect->btAddress);
    if (pConFind) {
        pConFind->Acquire(pIrp);

        SdpPrint(SDP_DBG_CONFIG_WARNING, ("Connect, found a connection\n"));
        if (pConFind->IsClient()) {
            SdpPrint(SDP_DBG_CONFIG_WARNING | SDP_DBG_CONFIG_ERROR,
                     ("Connect, connection is a client\n"));
        }
        else {
            SdpPrint(SDP_DBG_CONFIG_WARNING | SDP_DBG_CONFIG_INFO,
                     ("Connect, connection is a server\n"));
        }
    }
    else {
        SdpPrint(SDP_DBG_CONFIG_INFO, ("Connect, did not find a connection\n"));
    }

    BOOLEAN useExistingConnection = FALSE;

    status = STATUS_SUCCESS;

    if (pConFind) {
        if (pConFind->IsClient()) {
            useExistingConnection  = TRUE;
            m_SdpConnectionList.Unlock();
            if (pConFind->closing == FALSE) {
                pConFind->LockState(&irql);
                if (pConFind->channelState == SdpChannelStateConnecting) {
                    IoMarkIrpPending(pIrp);
                    pCon->connectList.AddTail(pIrp);
                    status = STATUS_PENDING;
                }
                pConFind->UnlockState(irql);

                //
                // We will complete this irp when the connection is finalized or if
                // the connection dies
                //
                if (status == STATUS_PENDING) {
                    return status;
                }

                if (pConFind->AddFileObject(stack->FileObject)) {
                    hConnection = pConFind->hConnection;
                    status = STATUS_SUCCESS;
                }
                else {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                }
            }
            else {
                status = STATUS_DELETE_PENDING;
            }
        }
        else {
            pCon->btAddress = pConnect->btAddress;

            SdpPrint(SDP_DBG_CONFIG_TRACE,
                     ("Adding new server connection %p to connection list\n",
                      pCon));

            m_SdpConnectionList.AddTail(pCon);
            m_SdpConnectionList.Unlock();
        }
        pConFind->Release(pIrp);
    }
    else {
        //
        // Either there were no active connections to this address or there is a
        // client connection
        //
        pCon->btAddress = pConnect->btAddress;

        SdpPrint(SDP_DBG_CONFIG_TRACE,
                 ("Adding new client connection %p to connection list\n", pCon));

        m_SdpConnectionList.AddTail(pCon);
        m_SdpConnectionList.Unlock();
    }

    //
    // If we found a connection and it is still alive, delete our temp connection
    // object and return
    //
    if (useExistingConnection) {
        delete pCon;

        if (NT_SUCCESS(status)) {
            if (outLen == sizeof(BthSdpConnect)) {
                RtlZeroMemory(pConnect, sizeof(*pConnect));
                pConnect->hConnection = hConnection;
                info = sizeof(*pConnect);
            }
            else {
                // DJH:  remove me later
                RtlZeroMemory(pBthConnect, sizeof(*pBthConnect));
                pBthConnect->hConnection = hConnection;
                info = sizeof(*pBthConnect);
            }
        }

        SdpPrint(SDP_DBG_CONFIG_INFO,
                 ("completing piggy back connect 0x%x\n", status));

        return status;
    }

    if (pConnect->requestTimeout == SDP_REQUEST_TO_DEFAULT) {
        pCon->maxTimeout = SDP_DEFAULT_TIMEOUT;
    }
    else {
        pCon->maxTimeout = pConnect->requestTimeout;
    }

    PBRB pBrb;
    PIRP irp;

    status = STATUS_INSUFFICIENT_RESOURCES;
    irp = NULL;
    pBrb = NULL;

    irp = IoAllocateIrp(pCon->pDevice->m_Struct.FunctionalDeviceObject->StackSize,
                        FALSE);

    if (irp != NULL) {
        pBrb = new (NonPagedPool, SDPI_TAG) BRB;

        if (pBrb != NULL) {
            pBrb->BrbHeader.Length = sizeof (_BRB_L2CA_CONNECT_REQ);
            pBrb->BrbHeader.Type = BRB_L2CA_CONNECT_REQ;
            pBrb->BrbHeader.Status = STATUS_SUCCESS;

            pBrb->BrbL2caConnectReq.ContextCxn = (PVOID) pCon;
            pBrb->BrbL2caConnectReq.btAddress = pCon->btAddress;
            pBrb->BrbL2caConnectReq.PSM = PSM_SDP;

            SdpPrint(SDP_DBG_CONFIG_INFO, ("Connect to %04x%08x\n",
                     GET_NAP(pBrb->BrbL2caConnectReq.btAddress),
                     GET_SAP(pBrb->BrbL2caConnectReq.btAddress)));

            IoMarkIrpPending(pIrp);

            pCon->Acquire(pIrp);

            pCon->LockState(&irql);
            pCon->connectList.AddTail(pIrp);
            pCon->UnlockState(irql);

            SdpPrint(SDP_DBG_CONFIG_INFO,
                     ("pending connect request %p\n", irp));

            SendBrbAsync(irp,
                         pBrb,
                         ConnectRequestComplete,
                         pCon,
                         this,
                         pBrb);

            status = STATUS_PENDING;
        }
        else {
            IoFreeIrp(irp);
            irp = NULL;
        }
    }

    return status;
}
#endif // UNDER_CE

 
// !UNDER_CE NTSTATUS SdpInterface::ServiceSearch(PIRP pIrp, ULONG_PTR& info)
NTSTATUS SdpInterface::ServiceSearch(SdpConnection *pCon, SdpServiceSearch *pSearch, BOOL fLocal)
{
#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    NTSTATUS status;

	pSearch->cUUIDs = 0;
    status = VerifyServiceSearch(pSearch->uuids, (UCHAR*) &pSearch->cUUIDs);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Check to see if this is a local search
    //
    if (fLocal) {
        ULONG *pResponse;
        USHORT total = 0;

        status = pSdpDB->ServiceSearchRequestResponseLocal(
            pSearch->uuids,
            (UCHAR) pSearch->cUUIDs,
            pSearch->cMaxHandles,
            &pResponse,
            &total);

        if (NT_SUCCESS(status) && pResponse != NULL) {
            ASSERT(total != 0);
			pCon->pClientBuf = (unsigned char*) pResponse;
            pCon->u.Client.ServiceSearch.totalRecordCount = total;
        }
#if defined (DEBUG) || defined (_DEBUG)
        else {
	        ASSERT(pCon->u.Client.ServiceSearch.totalRecordCount == 0);
        }
#endif

        return status;
    }
    return pCon->SendServiceSearchRequest(NULL,pSearch);


#else // UNDER_CE
    SdpConnection *pCon;
    BthSdpServiceSearchRequest *pSearch;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
    NTSTATUS status;
    ULONG outlen;
    
    info = 0;

    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(*pSearch)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    pSearch = (BthSdpServiceSearchRequest *) pIrp->AssociatedIrp.SystemBuffer;

    outlen = stack->Parameters.DeviceIoControl.OutputBufferLength;
    if (outlen == 0 || (outlen % sizeof(ULONG) != 0)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    UCHAR maxUuids = 0;

    status = VerifyServiceSearch(pSearch->uuids, &maxUuids);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Check to see if this is a local search
    //
    if (pSearch->hConnection == HANDLE_SDP_LOCAL) {
        ULONG *pResponse;
        USHORT total = 0, maxHandles;

        maxHandles = (USHORT)
            (stack->Parameters.DeviceIoControl.OutputBufferLength / sizeof(ULONG));

        status = Globals.pSdpDB->ServiceSearchRequestResponseLocal(
            pSearch->uuids,
            maxUuids,
            maxHandles,
            &pResponse,
            &total);

        if (NT_SUCCESS(status) && pResponse != NULL) {
            ASSERT(total != 0);

            info = total * sizeof(ULONG);
            RtlCopyMemory((PULONG) pIrp->AssociatedIrp.SystemBuffer, pResponse, info);
            ExFreePool(pResponse);
        }

        return status;
    }

    pCon = NULL;
    status = AcquireConnection(pSearch->hConnection, &pCon, pIrp);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ASSERT(pCon != NULL);

    //
    // Set the max uuids before we enqueue the request because the send function
    // assumes it is set (even for a queud irp processed at a later time)
    //
    SetMaxUuidsInIrp(pIrp, maxUuids);

    BOOLEAN added = pCon->clientQueue.AddIfBusy(pIrp, &status);

    if (added) {
        //
        // The queue is busy and the request has been queued.  Don't touch the 
        // irp anymore
        //
        return STATUS_PENDING;
    }
    else if (!NT_SUCCESS(status)) {
        //
        // The request was cancelled or something went wrong.  The irp was not 
        // enqueued.  Return the error status
        //
        pCon->ReleaseAndProcessNextRequest(pIrp);
        return status;
    }
    else {
        //
        // This is the current request on the connection, see if it matches the 
        // previous request
        //

        //
        // While we do not cache ServiceSearch responses, this will clean up 
        // any cached responsed from previous queries
        //
        status = pCon->IsPreviousRequest(pIrp,
                                         SdpPDU_ServiceSearchRequest,
                                         pSearch->uuids,
                                         NULL,
                                         0,
                                         &info);

        if (NT_SUCCESS(status) || status == STATUS_BUFFER_TOO_SMALL) {
            pCon->ReleaseAndProcessNextRequest(pIrp);
            return STATUS_SUCCESS;
        }
    }


    return pCon->SendServiceSearchRequest(pIrp);
#endif // UNDER_CE 
}


// NTSTATUS SdpInterface::AttributeSearch(PIRP pIrp, ULONG_PTR& info)
NTSTATUS SdpInterface::AttributeSearch(SdpConnection *pCon, SdpAttributeSearch *pSearch, BOOL fLocal)
{
#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    NTSTATUS status; 

    status = 
        VerifyAttributeSearch(pSearch->range,
                              &pSearch->numAttributes,
                              0);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (fLocal)  {
        PVOID pOrig;
        UCHAR *pResult;

        ULONG size;

        status = pSdpDB->ServiceAttributeRequestResponseLocal(
            pSearch->recordHandle,
            pSearch->range,
            pSearch->numAttributes,
            &pResult,
            &size,
            &pOrig);

        if (NT_SUCCESS(status)) {
            ASSERT(size != 0);
            // pOrig is the base of allocated memory, pResult is what we need to pass
            // to caller, is a few bytes past pOrig.  Rather than have this info propogate up all 
            // the layers just alloc the data here.

            if (NULL == (pCon->pClientBuf = (PUCHAR) SdpAllocatePool(size)))
                 return STATUS_INSUFFICIENT_RESOURCES;
            memcpy(pCon->pClientBuf,pResult,size);         
            pCon->cClientBuf = size;
            SdpFreePool(pOrig);
        }

        return status;
    }
    return pCon->SendAttributeSearchRequest(NULL,pSearch);

#else
    SdpConnection *pCon;
    BthSdpAttributeSearchRequest *pSearch;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
    ULONG maxRange;
    NTSTATUS status;

    info = 0;
    maxRange = 0;

    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(*pSearch)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(BthSdpStreamResponse)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    pSearch = (BthSdpAttributeSearchRequest *) pIrp->AssociatedIrp.SystemBuffer;

    status =
        VerifyAttributeSearch(pSearch->range,
                              &maxRange,
                              stack->Parameters.DeviceIoControl.InputBufferLength -
                               (sizeof(*pSearch) - sizeof(pSearch->range))
                              );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Check to see if it is a local search
    //
    if (pSearch->hConnection == HANDLE_SDP_LOCAL) {
        PVOID pOrig;
        UCHAR *pResult;

        ULONG size;

        status = Globals.pSdpDB->ServiceAttributeRequestResponseLocal(
            pSearch->recordHandle,
            pSearch->range,
            maxRange,
            &pResult,
            &size,
            &pOrig);

        if (NT_SUCCESS(status)) {
            //
            // Even if there is no match, you still get an empty sequence
            //
            BthSdpStreamResponse *pResponse = (BthSdpStreamResponse *)
                pIrp->AssociatedIrp.SystemBuffer;

            ULONG responseSize;

            responseSize =
                stack->Parameters.DeviceIoControl.OutputBufferLength -
                (sizeof(*pResponse) - sizeof(pResponse->response));

            pResponse->requiredSize = size;

            if (responseSize < size) {
                info = responseSize;
            }
            else {
                info = size;
            }

            pResponse->responseSize = info;

            RtlCopyMemory(pResponse->response, pResult, info);
            info += (sizeof(*pResponse) - sizeof(pResponse->response));

            ExFreePool(pOrig);
        }

        return status;
    }

    pCon = NULL;
    status = AcquireConnection(pSearch->hConnection, &pCon, pIrp);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ASSERT(pCon != NULL);

    //
    // Set the max range before we enqueue the request because the send function
    // assumes it is set (even for a queud irp processed at a later time)
    //
    SetMaxRangeInIrp(pIrp, maxRange);

    BOOLEAN added = pCon->clientQueue.AddIfBusy(pIrp, &status);

    if (added) {
        //
        // The queue is busy and the request has been queued.  Don't touch the 
        // irp anymore
        //
        return STATUS_PENDING;
    }
    else if (!NT_SUCCESS(status)) {
        //
        // The request was cancelled or something went wrong.  The irp was not 
        // enqueued.  Return the error status
        //
        pCon->ReleaseAndProcessNextRequest(pIrp);
        return status;
    }
    else {
        //
        // This is the current request on the connection, see if it matches the 
        // previous request
        //

        //
        // While we do not cache ServiceSearch responses, this will clean up 
        // any cached responsed from previous queries
        //
        status = pCon->IsPreviousRequest(pIrp,
                                         SdpPDU_ServiceAttributeRequest,
                                         NULL,
                                         pSearch->range,
                                         maxRange,
                                         &info);

        if (NT_SUCCESS(status) || status == STATUS_BUFFER_TOO_SMALL) {
            pCon->ReleaseAndProcessNextRequest(pIrp);
            return STATUS_SUCCESS;
        }
    }

    return pCon->SendAttributeSearchRequest(pIrp);
#endif // UNDER_CE
}

// NTSTATUS SdpInterface::ServiceAndAttributeSearch(PIRP pIrp, ULONG_PTR& info)
NTSTATUS SdpInterface::ServiceAndAttributeSearch(SdpConnection *pCon, SdpServiceAttributeSearch *pSearch, BOOL fLocal)
{
#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    NTSTATUS status;

    pSearch->cUUIDs = 0;
    status = VerifyServiceSearch(pSearch->uuids, (UCHAR*) &pSearch->cUUIDs);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status =
        VerifyAttributeSearch(pSearch->range,
                              &pSearch->numAttributes,
                              0);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (fLocal) {
        ULONG size;
        UCHAR *pResult;
        
        status = pSdpDB->ServiceSearchAttributeResponseLocal(
            pSearch->uuids,
            (UCHAR) pSearch->cUUIDs,
            pSearch->range,
            pSearch->numAttributes,
            &pResult,
            &size);

        if (NT_SUCCESS(status)) {
            ASSERT(size != 0);

            pCon->pClientBuf = (PUCHAR) pResult;
            pCon->cClientBuf = size;
        }

        return status;
    }
    return pCon->SendServiceSearchAttributeRequest(NULL,pSearch);
#else // UNDER_CE
    SdpConnection *pCon;
    BthSdpServiceAttributeSearchRequest *pSearch;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
    NTSTATUS status;
    ULONG maxRange = 0;
    UCHAR maxUuids = 0;

    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(*pSearch)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(BthSdpStreamResponse)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    pSearch = (BthSdpServiceAttributeSearchRequest *) pIrp->AssociatedIrp.SystemBuffer;


    status = VerifyServiceSearch(pSearch->uuids, &maxUuids);
    if (!NT_SUCCESS(status)) {
        return status;
    }


    status =
        VerifyAttributeSearch(pSearch->range,
                              &maxRange,
                              stack->Parameters.DeviceIoControl.InputBufferLength -
                              (sizeof(*pSearch) - sizeof(pSearch->range))
                              );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Check to see if this is a local search
    //
    if (pSearch->hConnection == HANDLE_SDP_LOCAL) {
        ULONG size;
        UCHAR *pResult;

        status = Globals.pSdpDB->ServiceSearchAttributeResponseLocal(
            pSearch->uuids,
            maxUuids,
            pSearch->range,
            maxRange,
            &pResult,
            &size);

        if (NT_SUCCESS(status)) {
            BthSdpStreamResponse *pResponse = (BthSdpStreamResponse *)
                pIrp->AssociatedIrp.SystemBuffer;

            ULONG responseSize;

            responseSize =
                stack->Parameters.DeviceIoControl.OutputBufferLength -
                (sizeof(*pResponse) - sizeof(pResponse->response));

            pResponse->requiredSize = size;
            if (responseSize < size) {
                info = responseSize;
            }
            else {
                info = size;
            }

            pResponse->responseSize = info;
            RtlCopyMemory(pResponse->response, pResult, info);
            info += (sizeof(*pResponse) - sizeof(pResponse->response));

            ExFreePool(pResult);
        }

        return status;
    }

    pCon = NULL;
    status = AcquireConnection(pSearch->hConnection, &pCon, pIrp);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Set the max range and uuids before we enqueue the request because the
    // send function assumes it is set (even for a queud irp processed at a
    // later time)
    //
    SetMaxUuidsInIrp(pIrp, maxUuids);
    SetMaxRangeInIrp(pIrp, maxRange);

    BOOLEAN added = pCon->clientQueue.AddIfBusy(pIrp, &status);

    if (added) {
        //
        // The queue is busy and the request has been queued.  Don't touch the 
        // irp anymore
        //
        return STATUS_PENDING;
    }
    else if (!NT_SUCCESS(status)) {
        //
        // The request was cancelled or something went wrong.  The irp was not 
        // enqueued.  Return the error status
        //
        pCon->ReleaseAndProcessNextRequest(pIrp);
        return status;
    }
    else {
        //
        // This is the current request on the connection, see if it matches the 
        // previous request
        //

        //
        // While we do not cache ServiceSearch responses, this will clean up 
        // any cached responsed from previous queries
        //
        status = pCon->IsPreviousRequest(pIrp,
                                         SdpPDU_ServiceSearchAttributeRequest,
                                         pSearch->uuids,
                                         pSearch->range,
                                         maxRange,
                                         &info);

        if (NT_SUCCESS(status) || status == STATUS_BUFFER_TOO_SMALL) {
            pCon->ReleaseAndProcessNextRequest(pIrp);
            return STATUS_SUCCESS;
        }
    }

    //
    // The actual transmission over the wire
    //
    return pCon->SendServiceSearchAttributeRequest(pIrp);
#endif // UNDER_CE    
}


#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
NTSTATUS SdpInterface::Disconnect(PIRP pIrp, ULONG_PTR& info)
{
    SdpConnection *pCon;
    NTSTATUS status;
    PVOID hConnection;
    LONG count = -1;
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);

    info = 0;

    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(HANDLE)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    hConnection = *(HANDLE*) pIrp->AssociatedIrp.SystemBuffer;

    m_SdpConnectionList.Lock();

    pCon = FindConnection(hConnection);
    if (pCon) {
        if (pCon->IsClient()) {
            //
            // This will mark the connection as closing which will protect this
            // object from further connections until we remove it from the linked
            // list
            //
            count = pCon->RemoveFileObject(stack->FileObject);
        }
        else {
            //
            // We found a connection, but it is a server connection.  Spec says
            // that a server should not allow a disconnect except for extreme
            // conditions.
            //
            pCon = NULL;
        }
    }
    m_SdpConnectionList.Unlock();

    if (pCon) {
        if (count == -1) {
            //
            // Somehow the caller got the hConnection, but never called connect
            //
            status = STATUS_INVALID_PARAMETER;
        }
        else {
            //
            // count == 0, the object is going away
            //
            status = STATUS_SUCCESS;
        }
    }
    else {
        status = STATUS_DEVICE_NOT_CONNECTED;
    }

    if (NT_SUCCESS(status)) {
        pCon->Cleanup(stack->FileObject);

        if (count > 0) {
            //
            // There are still other clients using the underlying bth connection
            //
            return status;
        }

        BRB brb;

        RtlZeroMemory(&brb, sizeof(brb));

        brb.BrbHeader.Length = sizeof (_BRB_L2CA_DISCONNECT_REQ);
        brb.BrbHeader.Type = BRB_L2CA_DISCONNECT_REQ;
        brb.BrbHeader.Status = STATUS_SUCCESS;
        brb.BrbL2caDisconnectReq.hConnection = hConnection;

        //
        // Removed from the list in final cleanup
        //
        // m_SdpConnectionList.Lock();
        // m_SdpConnectionList.RemoveAt(pCon);
        // m_SdpConnectionList.Unlock();

        //
        // pCon will be deleted in the IndicationFinalCleanup handler
        //
        status = SendBrb(&brb);
    }

    return status;
}

void
SdpInterface::CloseConnections(
    PIRP pIrp
    )
{
    PFILE_OBJECT pFileObject = IoGetCurrentIrpStackLocation(pIrp)->FileObject;

    m_SdpConnectionList.Lock();
    m_SdpConnectionList.Unlock();
}
#endif // UNDER_CE

NTSTATUS
CrackContState(
    IN OUT PUCHAR &pStream,
    IN OUT ULONG  &streamSize,
    OUT ContinuationState *pContState,
    OUT PSDP_ERROR pSdpError
    )
{
    // PAGED_CODE();

    BOOLEAN res = pContState->Read(pStream, streamSize);

    if (res == FALSE) {
        //
        // spec says, max cont state is 16 bytes long.  We get the streamSize 
        // from the pdu parameter length field so this is actually an error in
        // that field, not an error in the continuation state.
        //
        *pSdpError = SDP_ERROR_INVALID_PDU_SIZE; 
        return STATUS_INVALID_PARAMETER;
    }
    
    //
    // one for the header + actual state info
    //
    pStream += (1 + pContState->size);
    streamSize -= (1 + pContState->size);

    ASSERT(streamSize == 0);

    return STATUS_SUCCESS;
}

NTSTATUS
CrackSequenceParameter(
    IN OUT PUCHAR &pStream,
    IN OUT ULONG  &streamSize,
    IN     ULONG  minRemainingStream,
       OUT ULONG *pSequenceSize
    )
{
    // PAGED_CODE();

    UCHAR type, sizeIndex;

    SdpRetrieveHeader(pStream, type, sizeIndex);

    if (type != (UCHAR) SDP_TYPE_SEQUENCE) {
        return STATUS_INVALID_PARAMETER; 
    }

    if (sizeIndex < 5 || sizeIndex > 7) {
        return STATUS_INVALID_PARAMETER; 
    }

    pStream++;
    streamSize--;

    ULONG elementSize, storageSize;
    SdpRetrieveVariableSize(pStream, sizeIndex, &elementSize, &storageSize);

    if (elementSize + storageSize >= streamSize + minRemainingStream) {
        return STATUS_INVALID_PARAMETER; 
    }

    *pSequenceSize = 1 + storageSize + elementSize;

    //
    // We have incremented past the header above, so decrement back
    //
    NTSTATUS status = ValidateStream(pStream-1,     
                                     *pSequenceSize,
                                     NULL,
                                     NULL,
                                     NULL);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    pStream += (storageSize + elementSize);
    streamSize -= (storageSize + elementSize);

    return STATUS_SUCCESS;
}

NTSTATUS 
SdpInterface::CrackClientServiceSearchRequest(
    IN UCHAR *pServiceSearchRequestStream,
    IN ULONG streamSize,
    OUT UCHAR **ppSequenceStream,
    OUT ULONG *pSequenceStreamSize,
    OUT USHORT *pMaxRecordCount,
    OUT ContinuationState *pContState,
    OUT PSDP_ERROR pSdpError
    )
{
    // PAGED_CODE();

    UCHAR *pStream;
     
    //
    // data elem sequence        = 2 =  1 (seq hdr) + 1 (seq size)
    //                           = 3 =  1 (elem hdr) + 2 (size of uuid16)
    // sizeof(max record count)  = 2
    // cont state               >= 1
    //
    if (streamSize < 8) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    pStream = pServiceSearchRequestStream;

    //
    // 3 = sizeof(max record count) + at least one for cont state
    // 
    NTSTATUS status;
     
    *ppSequenceStream = pStream;
    status =
        CrackSequenceParameter(pStream, streamSize, 3, pSequenceStreamSize);

    if (!NT_SUCCESS(status)) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    RtlRetrieveUshort(pMaxRecordCount, pStream);
    *pMaxRecordCount = RtlUshortByteSwap(*pMaxRecordCount);

    pStream += sizeof(USHORT); 
    streamSize -= sizeof(USHORT);

    return CrackContState(pStream, streamSize, pContState, pSdpError);
}

NTSTATUS 
SdpInterface::CrackClientServiceAttributeRequest(
    IN UCHAR *pServiceAttributeRequestStream,
    IN ULONG streamSize,
    OUT ULONG *pRecordHandle,
    OUT USHORT *pMaxAttribByteCount,
    OUT UCHAR **ppAttribIdListStream,
    OUT ULONG *pAttribIdListStreamSize,
    OUT ContinuationState *pContState,
    OUT PSDP_ERROR pSdpError
    )
{
    // PAGED_CODE();

    //
    // record handle         = 4
    // max attrib byte count = 2
    // id list (seq)         = 2 = 1 (seq header) + 1 (stream size)
    //                       = 3 = 1 (elem hdr) + 2 (size of ushort)
    // cont state            = 1
    // 
    if (streamSize < 12) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    UCHAR *pStream;

    pStream = pServiceAttributeRequestStream;

    RtlRetrieveUlong(pRecordHandle, pStream);
    *pRecordHandle = RtlUlongByteSwap(*pRecordHandle);

    pStream += sizeof(ULONG);
    streamSize -= sizeof(ULONG);

    RtlRetrieveUshort(pMaxAttribByteCount, pStream);
    *pMaxAttribByteCount = RtlUshortByteSwap(*pMaxAttribByteCount);


    pStream += sizeof(USHORT);
    streamSize -= sizeof(USHORT);

    //
    // 1 = cont state
    //
    NTSTATUS status;
    
    *ppAttribIdListStream = pStream;
    status =
        CrackSequenceParameter(pStream, streamSize, 1, pAttribIdListStreamSize);

    if (!NT_SUCCESS(status)) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    return CrackContState(pStream, streamSize, pContState, pSdpError);
}

NTSTATUS 
SdpInterface::CrackClientServiceSearchAttributeRequest(
    IN UCHAR *pServiceSearchAttributeRequestStream,
    IN ULONG streamSize,
    OUT UCHAR **ppServiceSearchSequence,
    OUT ULONG *pServiceSearchSequenceSize,
    OUT USHORT *pMaxAttributeByteCount,
    OUT UCHAR **ppAttributeIdSequence,
    OUT ULONG *pAttributeIdSequenceSize,
    OUT ContinuationState *pContState,
    OUT PSDP_ERROR pSdpError
    )
{
    // PAGED_CODE();

    //
    // uuid elem sequence        = 2 =  1 (seq hdr) + 1 (seq size)
    //                           = 3 =  1 (elem hdr) + 2 (size of uuid16)
    // max attrib byte count     = 2
    // attrib elem sequence      = 2 =  1 (seq hdr) + 1 (seq size)
    //                           = 3 =  1 (elem hdr) + 2 (size of uuid16)
    // cont state                = 1
    //
    if (streamSize < 13) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status;
    UCHAR *pStream;

    pStream = pServiceSearchAttributeRequestStream;


    //
    // max attrib byte count = 2
    // attrib elem seq       = 5 (see above)
    // cont state            = 1
    //                       = 8
    //
    *ppServiceSearchSequence = pStream;
    status =
        CrackSequenceParameter(pStream, streamSize, 8, pServiceSearchSequenceSize);
                                    
    if (!NT_SUCCESS(status)) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    RtlRetrieveUshort(pMaxAttributeByteCount, pStream);
    *pMaxAttributeByteCount = RtlUshortByteSwap(*pMaxAttributeByteCount);

    pStream += sizeof(USHORT);
    streamSize -= sizeof(USHORT);

    //
    // cont state = 1
    //
    *ppAttributeIdSequence = pStream;
    status =
        CrackSequenceParameter(pStream, streamSize, 1, pAttributeIdSequenceSize);

    if (!NT_SUCCESS(status)) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    return CrackContState(pStream, streamSize, pContState, pSdpError);
}

NTSTATUS
SdpInterface::CrackServerServiceSearchResponse(
    IN UCHAR *pResponseStream,
    IN ULONG streamSize,
    OUT USHORT *pTotalCount,
    OUT USHORT *pCurrentCount,
    OUT ULONG **ppHandleList,
    OUT ContinuationState *pContState,
    OUT PSDP_ERROR pSdpError
    )
{
    //
    // sizeof(total count)      = 2
    // sizeof(current count)    = 2
    // sizeof(0+ handles)       = 0
    // sizeof(cont state)      >= 1
    //
    if (streamSize < 5) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    USHORT tmp;

    RtlRetrieveUshort(&tmp, pResponseStream);
    *pTotalCount = RtlUshortByteSwap(tmp);
    pResponseStream += sizeof(tmp);
    streamSize -= sizeof(tmp);

    RtlRetrieveUshort(&tmp, pResponseStream);
    *pCurrentCount = RtlUshortByteSwap(tmp);
    pResponseStream += sizeof(tmp);
    streamSize -= sizeof(tmp);

    //
    // see if the described number of records plus a finishing cont state is 
    // greater than what was sent OR if the current count is greater than the 
    // reported max count (incositent results)
    // 
    if ((*pCurrentCount * sizeof(ULONG) + 1 > streamSize) ||
        (*pCurrentCount > *pTotalCount)) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    *ppHandleList = (ULONG *) pResponseStream;
    pResponseStream += sizeof(ULONG) * (*pCurrentCount);
    streamSize -= sizeof(ULONG) * (*pCurrentCount); 

    return CrackContState(pResponseStream, streamSize, pContState, pSdpError);
}

NTSTATUS 
SdpInterface::CrackServerServiceAttributeResponse(
    IN UCHAR *pResponseStream,
    IN ULONG streamSize,
    OUT USHORT *pByteCount,
    OUT UCHAR **ppAttributeList,
    OUT ContinuationState *pContState,
    OUT PSDP_ERROR pSdpError
    )
{
    //
    // sizeof(list byte count)      = 2
    // sizeof(+1 byte attrib list)  = 1
    // sizeof(cont state)          >= 1
    //
    if (streamSize < 4) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    USHORT tmp;

    RtlRetrieveUshort(&tmp, pResponseStream);
    *pByteCount = RtlUshortByteSwap(tmp);
    pResponseStream += sizeof(tmp);
    streamSize -= sizeof(tmp);

    *ppAttributeList = (UCHAR *) pResponseStream;

    ULONG count = *pByteCount;

    if (count + 1 > streamSize) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    pResponseStream += count;
    streamSize -= count;

    return CrackContState(pResponseStream, streamSize, pContState, pSdpError);
}

NTSTATUS
SdpInterface::CreateServiceAttributePDU(
    IN UCHAR *pResult,
    IN ULONG resultSize,
    IN ContinuationState *pContState,
    IN USHORT maxAttribByteCount,   
    IN ULONG mtu,
    OUT UCHAR **ppResponse,
    OUT ULONG *pResponseSize,
    OUT UCHAR **ppNextResult,
    OUT ULONG *pNextResultSize,
    OUT PSDP_ERROR pSdpError
    )
{
    NTSTATUS status;
    ULONG fixedSize, adjustedResponseSize = 0;
    USHORT byteCount = 0;
    UCHAR *pStream;

    //
    // PDU header + byte count + cont state header
    //
    fixedSize = sizeof(SdpPDU) + sizeof(byteCount) + sizeof(UCHAR);

    SdpPrint(SDP_DBG_PDU_TRACE, ("CreateServiceAttributePDU called\n"));

    SdpPrint(SDP_DBG_PDU_INFO, ("fixed %d,  result %d, mtu %d\n",
             fixedSize, resultSize, mtu));

    if (fixedSize + resultSize <= mtu) {
        //
        // We can fit the entire response in a single PDU...let's see if 
        // maxAttribByteCount allows us to do this
        //
        if (resultSize > maxAttribByteCount) {
            //
            // compute whatever amount of space is leftover
            //
            adjustedResponseSize = mtu - (fixedSize + pContState->size);

            SdpPrint(SDP_DBG_PDU_INFO,
                     ("could fit, res size > mtu, adjusted size %d\n",
                     adjustedResponseSize));

            //
            // still too big, just go smaller
            //
            if (adjustedResponseSize > maxAttribByteCount) {
                SdpPrint(SDP_DBG_PDU_INFO,
                         ("adjusted size (%d) bigger than max byte count (%d), fixing\n",
                         adjustedResponseSize, maxAttribByteCount));

                adjustedResponseSize = maxAttribByteCount;
            }
        }
        else {
            //
            // everything fits, set the cont state to reflect this
            //
            SdpPrint(SDP_DBG_PDU_INFO, ("everything fits\n"));
            pContState->size = 0;
        }
    }
    else {
        //
        // the entire response doesn't fit into the mtu, must trim it down
        //
        adjustedResponseSize = mtu - (fixedSize + pContState->size);

        SdpPrint(SDP_DBG_PDU_INFO,
                 ("total bigger than mtu, adjusted size %d\n",
                  adjustedResponseSize));

        if (adjustedResponseSize > maxAttribByteCount) {
            SdpPrint(SDP_DBG_PDU_INFO,
                     ("still too big, setting to max attrib byte count %d\n",
                     adjustedResponseSize));

            adjustedResponseSize = maxAttribByteCount;
        }
    }
                                   
    if (adjustedResponseSize > 0) {
        SdpPrint(SDP_DBG_PDU_INFO,
                 ("adjustedResponseSize is %d\n", (ULONG) adjustedResponseSize));

        ASSERT(pContState->size != 0);

        *ppNextResult = pResult + adjustedResponseSize;
        *pNextResultSize = resultSize - adjustedResponseSize;
        resultSize = adjustedResponseSize;

        SdpPrint(SDP_DBG_PDU_INFO,
                 ("remaining result size %d\n", *pNextResultSize));
    }
    else {
        SdpPrint(SDP_DBG_PDU_INFO, ("done...zeroing out continuation stuff\n"));

        *ppNextResult = NULL;
        *pNextResultSize = 0;
    }

    *pResponseSize = fixedSize + resultSize + pContState->size;
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    *ppResponse =  new (PagedPool, SDPI_TAG) UCHAR[*pResponseSize];
#else
	if (NULL != (*ppResponse = (PUCHAR) ExAllocatePoolWithTag(0,sizeof(UCHAR)*(*pResponseSize),0)))
	    memset(*ppResponse,0,*pResponseSize);
#endif
    if (*ppResponse == NULL) {
        SdpPrint(SDP_DBG_PDU_ERROR,
                 ("could not allocate stream response for response\n"));

        status = STATUS_INSUFFICIENT_RESOURCES;
        *pSdpError = MapNtStatusToSdpError(status);

        return status;
    }

    //
    // Write out the response except for the PDU
    //
    pStream  = *ppResponse;
    pStream += sizeof(SdpPDU);

    ASSERT(resultSize <= 0xFFFF);

    //
    // Easy, just one (or the last) PDU
    //
    byteCount = (USHORT) resultSize;
    byteCount = RtlUshortByteSwap(byteCount);

    RtlCopyMemory(pStream, &byteCount, sizeof(byteCount));
    pStream += sizeof(byteCount);

    RtlCopyMemory(pStream, pResult, resultSize);
    pStream += resultSize;

    pContState->Write(pStream);

    //
    // Make sure we wrote out all the byte we thought we did
    //
#if DBG
    pStream += pContState->GetTotalSize();
    ASSERT(((ULONG) (pStream  - *ppResponse)) == *pResponseSize);
#endif

    return STATUS_SUCCESS;
}

NTSTATUS
SdpInterface::CreateServiceSearchAttributePDU(
    IN UCHAR *pResult,
    IN ULONG resultSize,
    IN ContinuationState *pContState,
    IN USHORT maxAttribByteCount,
    IN USHORT mtu,
    OUT UCHAR **ppResponse,
    OUT ULONG *pResponseSize,
    OUT UCHAR **ppNextResult,
    OUT ULONG *pNextResultSize,
    OUT PSDP_ERROR pSdpError
    )
{
    return CreateServiceAttributePDU(pResult,
                                     resultSize,
                                     pContState,
                                     maxAttribByteCount,
                                     mtu,
                                     ppResponse,
                                     pResponseSize,
                                     ppNextResult,
                                     pNextResultSize,
                                     pSdpError);
}

NTSTATUS
SdpInterface::CreateServiceSearchResponsePDU(
    ULONG *pRecordHandles,
    USHORT recordCount,
    USHORT totalRecordCount,
    IN ContinuationState *pContState,
    IN USHORT mtu,
    OUT UCHAR **ppResponse,
    OUT ULONG *pResponseSize,
    OUT ULONG **ppNextRecordHandles,
    OUT USHORT *pNextRecordCount,
    OUT PSDP_ERROR pSdpError
    )
{
    NTSTATUS status;
    ULONG fixedSize;
    USHORT count;

    fixedSize = 
           sizeof(SdpPDU)      +
           sizeof(USHORT)      +    // TotalServiceRecordCount
           sizeof(USHORT)      +    // CurrentServiceRecordCount
           sizeof(UCHAR);           // continuation state of 0

    *ppNextRecordHandles = NULL; 
    *pNextRecordCount = 0; 
     
    SdpPrint(SDP_DBG_PDU_TRACE, ("CreateServiceSearchResponsePDU called\n"));

    SdpPrint(SDP_DBG_PDU_TRACE, ("mtu is %d, response size is %d\n",
             (ULONG) mtu, fixedSize + (recordCount * sizeof(ULONG)))); 

    if (fixedSize + (recordCount * sizeof(ULONG)) > ((ULONG) mtu)) {
        //
        // figure out how much space we have left for the record handles
        //
        count = (mtu - ((USHORT) (fixedSize + pContState->size))) / sizeof(ULONG);
        if (count == 0) {
            SdpPrint(SDP_DBG_PDU_WARNING,
                     ("could not fit in a single handle!!!!\n"));

            //
            // can't fit any record handles, ergh!
            //
            return STATUS_INVALID_BUFFER_SIZE;
        }

        ASSERT(recordCount > count);

        //
        // total required size
        //
        *ppNextRecordHandles = pRecordHandles + count;
        *pNextRecordCount = recordCount - count;

        recordCount = count;
    }
    else {
        pContState->size = 0;
    }

    *pResponseSize = fixedSize + recordCount * sizeof(ULONG) + pContState->size;

    SdpPrint(SDP_DBG_PDU_INFO,
             ("returning %d records (%d total), resp sizse is %d\n",
              recordCount, totalRecordCount, *pResponseSize));

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    *ppResponse = new(PagedPool, SDPI_TAG) UCHAR[*pResponseSize];
#else
    if (NULL != (*ppResponse = (PUCHAR) ExAllocatePoolWithTag(0,*pResponseSize,0)))
        memset(*ppResponse,0,*pResponseSize);
#endif

    if (*ppResponse == NULL) {
        SdpPrint(SDP_DBG_PDU_ERROR,
                 ("could not allocate a stream for a response\n"));

        status = STATUS_INSUFFICIENT_RESOURCES; 
        *pSdpError = MapNtStatusToSdpError(status);
        return status;
    }

    //
    // Write out the response except for the PDU.  Format is
    //
    //  total record count            (2 bytes)
    //  record count in this response (2 bytes)
    //  handles                       (record count * sizeof(ULONG))
    //  continuation state            (1-17 bytes)
    //
    UCHAR *pStream = *ppResponse + sizeof(SdpPDU);

    totalRecordCount = RtlUshortByteSwap(totalRecordCount);
    RtlCopyMemory(pStream, &totalRecordCount, sizeof(totalRecordCount));
    pStream += sizeof(totalRecordCount);

    count = RtlUshortByteSwap(recordCount);
    RtlCopyMemory(pStream, &count, sizeof(count));
    pStream += sizeof(count);

    if (recordCount) {
        RtlCopyMemory(pStream, pRecordHandles, recordCount * sizeof(ULONG));
        pStream +=(recordCount * sizeof(ULONG));
    }

    //
    //  Finally the cont state
    //
    pContState->Write(pStream);

    //
    // Make sure we wrote only the number bytes we said we did
    //
#if DBG
    pStream += pContState->GetTotalSize();
    ASSERT(((ULONG) (pStream - *ppResponse)) == *pResponseSize);
#endif

    return STATUS_SUCCESS;
}


#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

NTSTATUS
SdpInterface::ConnectRequestComplete(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    )
{
    PIO_STACK_LOCATION stack;
    SdpInterface *pSdpInterface;
    SdpConnection *pCon;
    PBRB pBrb;

    stack = IoGetCurrentIrpStackLocation(Irp);
    pSdpInterface = (SdpInterface *) stack->Parameters.Others.Argument1;
    pBrb = (BRB *) stack->Parameters.Others.Argument2;

    pCon = (SdpConnection *) Context;

    SdpPrint(SDP_DBG_CONFIG_TRACE,
             ("+ConnectRequestComplete: status 0x%x, hCon %p, con %p\n",
              Irp->IoStatus.Status, pBrb->BrbL2caConnectReq.hConnection, pCon));

    if (NT_SUCCESS(Irp->IoStatus.Status)) {
        //
        // Assigned when enter the config state
        //
        // pCon->hConnection = pBrb->BrbL2caConnectReq.hConnection;
    }
    else {
#if DBG
        SdpConnection *pConFind;
#endif
        //
        // Could not connect, remove from the list.  We will not receive a final
        // indication because the connect failed.  We need to clean up here.
        //
        pCon->CompletePendingConnectRequests(Irp->IoStatus.Status);

        SdpPrint(SDP_DBG_CONFIG_INFO,
                 ("connect failed with %p, removing and destroying\n", pCon));

        pSdpInterface->m_SdpConnectionList.Lock();
#if DBG
        pConFind = pSdpInterface->FindConnection(pCon->btAddress);
        ASSERT(pConFind == pCon);
#endif
        pSdpInterface->m_SdpConnectionList.RemoveAt(pCon);
        pSdpInterface->m_SdpConnectionList.Unlock();

        pCon->ReleaseAndWait();

        delete pCon;
    }

    IoFreeIrp(Irp);
    delete pBrb;

    SdpPrint(SDP_DBG_CONFIG_TRACE, ("-ConnectRequestComplete, con %p\n", pCon));

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
SdpInterface::Sdp_IndicationCallback(
    IN PVOID ContextIF,
    IN IndicationCode Indication,
    IN IndicationParameters * Parameters
    )
{
    SdpInterface *pThis = (SdpInterface*) ContextIF;
    SdpConnection *pCon = (SdpConnection*) Parameters->ContextCxn;
    NTSTATUS status = STATUS_NOT_SUPPORTED;

    SdpPrint(SDP_DBG_CONFIG_TRACE,
              ("IndicationCallback:  ind %d, pCon %p\n", Indication, pCon));

    switch (Indication) {
    case IndicationRemoteConnect:
        return pThis->RemoteConnectCallback(Parameters);

    case IndicationEnterConfigState:
        return pThis->EnterConfigStateCallback(Parameters, pCon);

    case IndicationRemoteConfig:
        return pThis->RemoteConfigCallback(Parameters, pCon);

    case IndicationConfigResponseError:
        return pThis->ConfigResponseCallback(Parameters);

    case IndicationEnterOpenState:
        return pThis->EnterOpenStateCallback(Parameters, pCon);

    case IndicationRemoteDisconnect:
        return pThis->RemoteDisconnectCallback(Parameters, pCon);

    case IndicationFinalCleanup:
        return pThis->FinalCleanupCallback(Parameters, pCon);

    default:
        return STATUS_NOT_SUPPORTED;
    }

    return status;
}

NTSTATUS SdpInterface::RemoteConnectCallback(IndicationParameters *Parameters)
{
    SdpPrint(SDP_DBG_CONFIG_TRACE,
             ("RemoteConnectCallback (0x%x)\n", Parameters->hConnection));

    ASSERT(Parameters->hConnection);

    //
    // Create a connection off the bat
    //
    SdpConnection *pCon;
    NTSTATUS status;
    USHORT response;
    KIRQL irql;

    pCon = new (NonPagedPool, SDPI_TAG) SdpConnection(m_pDevice, FALSE);

    status = STATUS_SUCCESS;
    response = CONNECT_RSP_RESULT_SUCCESS;

    ASSERT(Parameters->Parameters.Connect.Request.PSM == PSM_SDP);

    if (pCon) {
        //
        // Succesfully created a connection object, see if we already have a
        // connection to this remote device where we are the server.   If so,
        // fail this 2nd connection
        //
        SdpConnection *pConFind;

        if (pCon->Init() == FALSE) {
            //
            // Couldn't allocate some bufferes, no resources
            //
            delete pCon;
            pCon = NULL;
            status = STATUS_INSUFFICIENT_RESOURCES;
            response = CONNECT_RSP_RESULT_NO_RESOURCES;
        }
        else {
            m_SdpConnectionList.Lock();

            pConFind = FindConnection(Parameters->BtAddress, TRUE);
            if (pConFind == NULL) {
                pCon->btAddress = Parameters->BtAddress;
                pCon->hConnection = Parameters->hConnection;
                pCon->channelState = SdpChannelStateConnecting;

                SdpPrint(0, ("Adding server connection %p to connection list",
                             pCon));

                m_SdpConnectionList.AddTail(pCon);
            }

            m_SdpConnectionList.Unlock();

            //
            // We only allow one connection from the client to this server.  If the
            // client has many different applications that want to query this server,
            // it must queue them on its end.  Otherwise the client can connect to
            // the server infinite number of times until memory runs out.
            //
            if (pConFind) {
                //
                // Found another connection, kill this one
                //
                SdpPrint(SDP_DBG_CONFIG_WARNING,
                         ("remote device %04x%08x trying to connect to SDP more than once!\n",
                          GET_NAP(Parameters->BtAddress),
                          GET_SAP(Parameters->BtAddress)));

                // ASSERT(FALSE);

                delete pCon;
                pCon = NULL;

                status = STATUS_UNSUCCESSFUL;
                // use no resources for lack of a better negative value
                response = CONNECT_RSP_RESULT_NO_RESOURCES;
            }
        }
    }
    else {
        SdpPrint(SDP_DBG_CONFIG_ERROR,
                 ("RemoteConnect, not enough mem for a connection object\n"));

        status = STATUS_INSUFFICIENT_RESOURCES;
        response = CONNECT_RSP_RESULT_NO_RESOURCES;
    }

    Parameters->Parameters.Connect.Response.Result = response;
    if (NT_SUCCESS(status)) {
        SdpPrint(SDP_DBG_CONFIG_TRACE,
                 ("RemoteConnect, new connection %p\n", pCon));

        Parameters->ContextCxn = (PVOID) pCon;
    }

    return status;
}

NTSTATUS
SdpInterface::EnterConfigStateCallback(
    IndicationParameters *Parameters,
    SdpConnection *pConnection
    )
{
    PIRP irp;
    BRB *pBrb;
    NTSTATUS status;

    SdpPrint(SDP_DBG_CONFIG_TRACE,
             ("EnterConfigStateCallback (%p)\n", pConnection));

    if (pConnection->IsClient()) {
        pConnection->hConnection = Parameters->hConnection;
    }

    status = STATUS_INSUFFICIENT_RESOURCES;
    irp = NULL;
    pBrb = NULL;

    irp = IoAllocateIrp(
        pConnection->pDevice->m_Struct.FunctionalDeviceObject->StackSize,
        FALSE);

    if (irp != NULL) {
        pBrb = new (NonPagedPool, SDPI_TAG) BRB;

        if (pBrb != NULL) {
            pConnection->configRetry = CONFIG_RETRY_DEFAULT;

            SendConfigRequest(irp, pBrb,pConnection, SDP_DEFAULT_MTU);

            status = STATUS_SUCCESS;
        }
        else {
            IoFreeIrp(irp);
            irp = NULL;
        }
    }

    return status;
}

NTSTATUS
SdpInterface::RemoteConfigCallback(
    IndicationParameters *Parameters,
    SdpConnection *pConnection
    )
{
    SdpPrint(SDP_DBG_CONFIG_TRACE,
             ("RemoteConfigCallback (%p)\n", pConnection));

    pConnection->outMtu = Parameters->Parameters.Config.Request.Params.MTU;

    Parameters->Parameters.Config.Response.Result = CONFIG_STATUS_SUCCESS;
    Parameters->Parameters.Config.Response.Params.ExtraOptionsLen = 0;
    Parameters->Parameters.Config.Response.Params.Flags = 0;
    Parameters->Parameters.Config.Response.Params.MTU = 0;
    Parameters->Parameters.Config.Response.Params.pExtraOptions = NULL;
    Parameters->Parameters.Config.Response.Params.pFlowSpec = NULL;

    return STATUS_SUCCESS;
}

NTSTATUS SdpInterface::ConfigResponseCallback(IndicationParameters *Parameters)
{
    return STATUS_SUCCESS;
}

NTSTATUS
SdpInterface::EnterOpenStateCallback(
    IndicationParameters *Parameters,
    SdpConnection *pConnection
    )
{
    NTSTATUS status, foo;
    SdpPrint(SDP_DBG_CONFIG_TRACE,
             ("EnterOpenStateCallback (%p)\n", pConnection));

    //
    // SdpConnection::FinalizeConnection will return TRUE if the connection
    // needs to be destroyed
    //
    status =  pConnection->FinalizeConnection() ?  STATUS_UNSUCCESSFUL :
                                                   STATUS_SUCCESS;

    return status;
}

NTSTATUS
SdpInterface::RemoteDisconnectCallback(
    IndicationParameters *Parameters,
    SdpConnection *pConnection
    )
{
#if DBG
    SdpConnection *pConFind;
#endif

    SdpPrint(SDP_DBG_CONFIG_TRACE,
             ("RemoteDisconnectCallback (%p)\n", pConnection));

#if DBG
    m_SdpConnectionList.Lock();
    pConFind = FindConnection(Parameters->hConnection);
    if (pConFind == NULL) {
        //
        // We can have an SdpConnection in the list w/out a matching hConnection
        // when we initiate a connect request and then the next indication we
        // get is disconnect (ie no config)
        //
        SdpPrint(SDP_DBG_CONFIG_TRACE,
                 ("could not find connection %p by hConnection, use address\n",
                  pConnection));

        //
        // This should work.
        pConFind = FindConnection(Parameters->BtAddress);

        ASSERT(pConFind == pConnection);
    }
    m_SdpConnectionList.Unlock();
#endif // DBG

    //
    // Always remove the connection in final cleanup
    //
    // m_SdpConnectionList.RemoveAt(pConnection);

    FireWmiEvent(SdpServerLogTypeDisconnect, &Parameters->BtAddress);

    //
    // Only send one disconnect across the wire
    //
    pConnection->Disconnect();

    return STATUS_SUCCESS;
}

NTSTATUS
SdpInterface::FinalCleanupCallback(
    IndicationParameters *Parameters,
    SdpConnection *pConnection
    )
{
    SdpPrint(SDP_DBG_CONFIG_TRACE, ("FinalCleanupCallback (%p)\n",
                                    pConnection));

    //
    // pConnection can be NULL we failed IndicationRemoteConnect
    //
    if (pConnection != NULL) {
#if DBG
        SdpConnection *pConFind;
#endif

        m_SdpConnectionList.Lock();
#if DBG
        pConFind = FindConnection(Parameters->hConnection);
        if (pConFind == NULL) {
            //
            // We can have an SdpConnection in the list w/out a matching hConnection
            // when we initiate a connect request and then the next indication we
            // get is disconnect (ie no config)
            //
            SdpPrint(SDP_DBG_CONFIG_TRACE,
                     ("could not find connection %p by hConnection, use address\n",
                      pConnection));

            //
            // This should work.
            pConFind = FindConnection(Parameters->BtAddress);

            ASSERT(pConFind == pConnection);
        }
#endif
        SdpPrint(SDP_DBG_CONFIG_TRACE,
                 ("removing connection %p from connection list\n", pConnection));

        m_SdpConnectionList.RemoveAt(pConnection);
        m_SdpConnectionList.Unlock();

        // pConnection->ReleaseAndWait();
        //
        // Always remove the connection in final cleanup
        //
        delete pConnection;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
SdpInterface::SendConfigRequest(
    PIRP Irp,
    PBRB Brb,
    SdpConnection *Con,
    USHORT Mtu
    )
{
    SdpPrint(SDP_DBG_BRB_TRACE, ("send config request\n"));

    RtlZeroMemory(Brb, sizeof(BRB));

    Brb->BrbHeader.Length = sizeof (_BRB_L2CA_CONFIG_REQ);
    Brb->BrbHeader.Type = BRB_L2CA_CONFIG_REQ;
    Brb->BrbHeader.Status = STATUS_SUCCESS;

    Brb->BrbL2caConfigReq.hConnection = Con->hConnection;
    Brb->BrbL2caConfigReq.Params.Flags = 0;
    Brb->BrbL2caConfigReq.Params.MTU = Mtu;
    Brb->BrbL2caConfigReq.Params.ExtraOptionsLen = 0;
    Brb->BrbL2caConfigReq.Params.pExtraOptions = NULL;
    Brb->BrbL2caConfigReq.Params.pFlowSpec = NULL;

    Brb->BrbL2caConfigReq.LinkTO = 0;
    Brb->BrbL2caConfigReq.FlushTO = 0;

    Con->inMtu = Mtu;

    return SendBrbAsync(Irp,
                        Brb,
                        ConfigRequestComplete,
                        this,
                        Con,
                        Brb);
}

NTSTATUS
SdpInterface::ConfigRequestComplete(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    )
/*++

    Sent a config req, it is now complete

  --*/
{
    PIO_STACK_LOCATION pStack;
    SdpInterface *pThis;
    SdpConnection *pCon;
    BRB* pBrb;
    BOOLEAN disconnect = FALSE;

    SdpPrint(SDP_DBG_CONFIG_TRACE, ("ConfigReqComplete\n"));

    //
    // Grab our info out of the irp
    //
    pThis = (SdpInterface*) Context;
    pStack = IoGetCurrentIrpStackLocation(Irp);
    pCon = (SdpConnection*) pStack->Parameters.Others.Argument1;
    pBrb = (BRB*) pStack->Parameters.Others.Argument2;

    if (NT_SUCCESS(Irp->IoStatus.Status)) {
        if (pBrb->BrbL2caConfigReq.Params.MTU != pCon->inMtu) {
            //
            // Out in MTU was not accepted, the remote device suggested a
            // different one...use it.
            //
            pCon->inMtu = pBrb->BrbL2caConfigReq.Params.MTU;

            SdpPrint(SDP_DBG_CONFIG_INFO,
                     ("Config succeed, lowering mtu to %d\n",
                      (ULONG) pCon->inMtu));
        }
        else {
            //
            // remote client liked the config request
            //
            SdpPrint(SDP_DBG_CONFIG_INFO, ("Config succeed\n"));
        }
    }
    else if (pBrb->BrbHeader.Flags & CONFIG_STATUS_INVALID_PARAMETER) {
        //
        // Client didn't like our MTU, resend the config request with their
        // suggested value which they can support
        //
        SdpPrint(SDP_DBG_CONFIG_WARNING,
                 ("config failed, flags 0x%x...retrying (%d)\n",
                  pBrb->BrbHeader.Flags, pCon->configRetry-1));

        pCon->configRetry--;

        if (pCon->configRetry > 0) {
            pCon->inMtu = pBrb->BrbL2caConfigReq.Params.MTU;

            pThis->SendConfigRequest(Irp,
                                     pBrb,
                                     pCon,
                                     pBrb->BrbL2caConfigReq.Params.MTU);

            return STATUS_MORE_PROCESSING_REQUIRED;
        }
        else {
            //
            // Tried alot to config, didn't work
            //
            disconnect = TRUE;
        }
    }
    else {
        SdpPrint(SDP_DBG_CONFIG_ERROR,
                 ("config failed, flags 0x%x...stopping\n",
                 pBrb->BrbHeader.Flags));
        disconnect = TRUE;
    }

    if (disconnect) {
        //
        // Something went wrong, disconnect from the remote device.  Free the
        // Irp and BRB in the disconnect completion routine
        //
        SdpPrint(SDP_DBG_CONFIG_ERROR, ("config failed, disconnecting!\n"));

        pThis->SendDisconnect(pCon, Irp, pBrb);
    }
    else {
        //
        // Free the irp and brb we allocated in our remote connection callback
        //
        IoFreeIrp(Irp);
        delete pBrb;
    }


    return STATUS_MORE_PROCESSING_REQUIRED;
}

void
SdpInterface::SendDisconnect(
    SdpConnection *pCon,
    PIRP pIrp,
    PBRB pBrb
    )
{
    //
    // Only send the disconnect once.  If it returns TRUE, all the pending
    // connection requests will be failed once it returns
    //
    if (pCon->Disconnect()) {
        RtlZeroMemory(pBrb, sizeof(*pBrb));

        pBrb->BrbHeader.Length = sizeof (_BRB_L2CA_DISCONNECT_REQ);
        pBrb->BrbHeader.Type = BRB_L2CA_DISCONNECT_REQ;
        pBrb->BrbHeader.Status = STATUS_SUCCESS;
        pBrb->BrbL2caDisconnectReq.hConnection = pCon->hConnection;

        SendBrbAsync(pIrp, pBrb, DisconnectRequestComplete, pBrb);
    }
}

NTSTATUS
SdpInterface::DisconnectRequestComplete(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp,
    PVOID Context
    )
{
    BRB* pBrb;

    SdpPrint(SDP_DBG_CONFIG_TRACE, ("DisconnectRequestComplete\n"));

    pBrb = (BRB*) Context;

    delete pBrb;
    IoFreeIrp(Irp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}
#endif // UNDER_CE
