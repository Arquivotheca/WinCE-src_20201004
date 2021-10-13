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

#define SDPC_TAG 'CpdS'

#define SDP_DEFAULT_TIMEOUT 30      // in seconds
#define SDP_SCAN_INTERVAL 1

#if  ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
LIST_ENTRY SdpConnection::_TimeoutListHead;
KSPIN_LOCK SdpConnection::_TimeoutListLock;
KTIMER SdpConnection::_TimeoutTimer;
KDPC SdpConnection::_TimeoutTimerDpc;
#endif

#if UPF
USHORT GetMaxByteCount(USHORT inMtu)
{
    HANDLE hKey;
    USHORT rVal = 0;
    if (NT_SUCCESS(BthPort_OpenKey(NULL,
                                   STR_REG_MACHINE STR_SOFTWARE_KEYW,
                                   &hKey))) {
        ULONG tmp = 0;
        if (NT_SUCCESS(QueryKeyValue(hKey,
                                     L"OverrideMaxByteCount",
                                     &tmp,
                                     sizeof(ULONG))) && tmp != 0) {
            rVal = (USHORT) tmp;
        }

        ZwClose(hKey);
    }

    if (rVal == 0) {
        //
        // Compute max byte count by taking the maximum buffer the server can send me
        // and taking out all the fixed fields
        //
        rVal = (USHORT) inMtu -
                                (sizeof(SdpPDU)    +
                                 sizeof(USHORT)    + // AttribyteListsByteCount
                                 sizeof(UCHAR));     // cont state of zero
    }
    else {
        SdpPrint(0, ("***UPF*** overrding max byte count with %d\n", (ULONG) rVal));
    }

    return rVal;
}
#else
//
// USHORT =  AttribyteListsByteCount
// UCHAR  =  cont state of zero
//
#define GetMaxByteCount(mbc) ((USHORT) (mbc) - (sizeof(SdpPDU) + sizeof(USHORT) + sizeof(UCHAR)))
#endif // UPF



NTSTATUS
SendBrbCompletion(
    PDEVICE_OBJECT DeviceObject, 
    PIRP Irp,
    PVOID Context
    );

#if SDP_VERBOSE
NTSTATUS TestWalkStream(PVOID Context, UCHAR DataType, ULONG DataSize, PUCHAR Stream);
#endif


// On WinCE we malloc blocks of these structs at a time, so the constructor is never called.
SdpConnection::SdpConnection(BThDevice *pDev, BOOLEAN isClient)  
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    : fileObjectList(), clientQueue(), channelState(SdpChannelStateClosed),
    pDevice(pDev), client(isClient)
#endif
{
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    ZERO_THIS();
    _DbgPrintF(DBG_SDP_TRACE, ("SdpConnection created 0x%x", this));

    InitializeListHead(&entry);
    KeInitializeSpinLock(&stateSpinLock);
    IoInitializeRemoveLock(&remLock, 0, 0, 0);
    
    clientQueue.SetQueueCancelRoutine(_QueueCancelRoutine, this);

    Acquire(REASON(AcquireCreate));
#endif  // UNDER_CE
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
void
SdpConnection::ReleaseAndProcessNextRequest(
    PIRP pIrp
    )
{
    PIRP pNextRequest = NULL;
     
    clientQueue.RemoveIfAvailable(&pNextRequest);

    //
    // pNextRequest already has a reference taken out on this connection, so 
    // this object is still valid if there is another request
    //
    Release(pIrp);

    if (pNextRequest) {
        PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pNextRequest);
        switch (stack->Parameters.DeviceIoControl.IoControlCode) {
        case IOCTL_BTH_SDP_SERVICE_SEARCH:
            SendServiceSearchRequest(pIrp);
            break;

        case IOCTL_BTH_SDP_ATTRIBUTE_SEARCH:
            SendAttributeSearchRequest(pIrp);
            break;

        case IOCTL_BTH_SDP_SERVICE_ATTRIBUTE_SEARCH:
            SendServiceSearchAttributeRequest(pIrp);
            break;

        default:
            ASSERT(FALSE);
        }
    }
}

void SdpConnection::ReleaseAndWait()
{
    IoReleaseRemoveLockAndWait(&remLock, REASON(AcquireCreate));
}
#endif

SdpConnection::~SdpConnection()
{
    SdpPrint(SDP_DBG_CON_TRACE, ("SdpConnection destroyed %p\n", this));

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    CompletePendingConnectRequests(STATUS_UNSUCCESSFUL);

    


    if (pReadBuffer) {
        ExFreePool(pReadBuffer);
        pReadBuffer = NULL;
    }

    if (pReadIrp) {
        IoFreeIrp(pReadIrp);
        pReadIrp = NULL;
    }

    if (pWriteIrp) {
        IoFreeIrp(pWriteIrp);
        pWriteIrp = NULL;
    }

    if (pReadBrb) {
        delete pReadBrb;
        pReadBrb = NULL;
    }
#endif    
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
BOOLEAN SdpConnection::Init()
{
    pReadIrp = IoAllocateIrp(pDevice->m_Struct.FunctionalDeviceObject->StackSize+1, FALSE);
    pWriteIrp = IoAllocateIrp(pDevice->m_Struct.FunctionalDeviceObject->StackSize+1, FALSE);

    pReadBrb = new (NonPagedPool, SDPC_TAG) BRB;

    return (pReadIrp != NULL) && (pWriteIrp != NULL);
}

void
SdpConnection::FireWmiEvent(
    SDP_SERVER_LOG_TYPE Type,
    PVOID Buffer,
    ULONG Length
    )
{
#ifdef SDP_LOGGING
    PSDP_SERVER_LOG_INFO pLog;
    ULONG size = sizeof(*pLog) + Length -1;

    pLog = (PSDP_SERVER_LOG_INFO)
        ExAllocatePoolWithTag(PagedPool, size, SDPC_TAG);

    if (pLog) {
        RtlZeroMemory(pLog, size);

        pLog->type = Type;
        pLog->info.btAddress = btAddress;
        pLog->mtu = outMtu;
        pLog->dataLength = Length;
        RtlCopyMemory(pLog->data, Buffer, Length);

        WmiFireEvent(pDevice->m_Struct.FunctionalDeviceObject,
                     (LPGUID) &GUID_BTHPORT_WMI_SDP_SERVER_LOG_INFO,
                     0,
                     size,
                     pLog);
    }
#endif // SDP_LOGGING
}

void
SdpConnection::FireWmiEvent(
    SDP_SERVER_LOG_TYPE Type,
    const bt_addr& Address,
    USHORT Mtu
    )
{
#ifdef SDP_LOGGING
    PSDP_SERVER_LOG_INFO pLog;

    pLog = (PSDP_SERVER_LOG_INFO)
        ExAllocatePoolWithTag(PagedPool, sizeof(*pLog), SDPI_TAG);

    if (pLog) {
        RtlZeroMemory(pLog, sizeof(*pLog));

        pLog->type = Type;
        pLog->info.btAddress = Address;
        pLog->mtu = Mtu;
        pLog->dataLength = 0;

        WmiFireEvent(pDevice->m_Struct.FunctionalDeviceObject,
                     (LPGUID) &GUID_BTHPORT_WMI_SDP_SERVER_LOG_INFO,
                     0,
                     sizeof(*pLog),
                     pLog);
    }
#endif SDP_LOGGING
}

BOOLEAN
SdpConnection::AddFileObject(PFILE_OBJECT pFileObject)
{
    FileObjectEntry *pEntry = new(PagedPool, SDPC_TAG) FileObjectEntry(pFileObject);

    if (pEntry == NULL) {
        return FALSE;
    }

    ASSERT(closing == FALSE);

    _DbgPrintF(DBG_SDP_INFO, ("Adding file object %p\n", pEntry));

    fileObjectList.AddTail(pEntry);

    InterlockedIncrement(&createCount);
    ASSERT(createCount > 0);

    return TRUE;
}

LONG
SdpConnection::RemoveFileObject(PFILE_OBJECT pFileObject)
{
    FileObjectEntry *pEntry = FindFileObject(pFileObject);
    if (pEntry == NULL) {
        //
        // Somebody send a disconnect for a connection they did not own
        //
        return -1;
    }

    return RemoveFileObject(pEntry);
}


LONG
SdpConnection::RemoveFileObject(FileObjectEntry *pEntry)
{
    ULONG c;

    ASSERT(closing == FALSE);

    _DbgPrintF(DBG_SDP_INFO, ("Removing file object %p\n", pEntry));

    fileObjectList.RemoveAt(pEntry);
    delete pEntry;

    c = InterlockedDecrement(&createCount);

    if (c == 0) {
        _DbgPrintF(DBG_SDP_INFO, ("Closing on %p is TRUE\n", this));
        closing = TRUE;
        ASSERT(fileObjectList.IsEmpty());
    }

    return c;
}

FileObjectEntry *
SdpConnection::FindFileObject(
    PFILE_OBJECT pFileObject
    )
{
    FileObjectEntry *pEntry;
    for (pEntry = fileObjectList.GetHead();
         pEntry != NULL;
         pEntry = fileObjectList.GetNext(pEntry)) {
        if (pEntry->pFileObject == pFileObject) {
            return pEntry;
        }
    }

    return NULL;
}

void
SdpConnection::Cleanup(
    PFILE_OBJECT pFileObject
    )
{
    PIRP irp;
    CIRP_LIST list;

    clientQueue.Cleanup(&list, pFileObject);

    while (!list.IsEmpty()) {
        irp = list.RemoveHead();

        Release(irp);

        irp->IoStatus.Information = 0;
        irp->IoStatus.Status = STATUS_CANCELLED;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }
}

BOOLEAN
SdpConnection::SetupReadBuffer()
{
    pReadBuffer = ExAllocatePoolWithTag(NonPagedPool, inMtu, SDPC_TAG);
    return pReadBuffer != NULL;
}

BOOLEAN
SdpConnection::InMtuDone(
    USHORT mtu
    )
{
    KIRQL irql;
    BOOLEAN resp = FALSE;

    LockState(&irql);

    SdpPrint(SDP_DBG_CONFIG_INFO, ("InMtuDone, %d\n", mtu));

    inMtu = mtu;
    configInDone = TRUE;
    if (configOutDone) {
        SdpPrint(SDP_DBG_CONFIG_INFO, ("all done!\n"));
        channelState = SdpChannelStateOpen;
        configInDone = configOutDone = FALSE;
        resp = TRUE;
    }
    else {
        SdpPrint(SDP_DBG_CONFIG_INFO, ("waiting for out!\n"));
    }

    UnlockState(irql);

    return resp;
}

BOOLEAN
SdpConnection::OutMtuDone(
    USHORT mtu
    )
{
    KIRQL irql;
    BOOLEAN resp = FALSE;

    LockState(&irql);

    SdpPrint(SDP_DBG_CONFIG_INFO, ("OutMtuDone, %d\n", mtu));

    if (mtu == 0) {
        mtu = L2CAP_DEFAULT_MTU;
        SdpPrint(SDP_DBG_CONFIG_INFO, ("Converting 0 to default mtu (%d)\n", (ULONG) mtu));
    }

    outMtu = mtu;
    configOutDone = TRUE;
    if (configInDone) {
        SdpPrint(SDP_DBG_CONFIG_INFO, ("all done!\n"));
        channelState = SdpChannelStateOpen;
        configInDone = configOutDone = FALSE;
        resp = TRUE;
    }
    else {
        SdpPrint(SDP_DBG_CONFIG_INFO, ("waiting for in!\n"));
    }

    UnlockState(irql);

    return resp;
}

BOOLEAN SdpConnection::FinalizeConnection()
{
    NTSTATUS status;
    BOOLEAN disconnect = FALSE;

    if (SetupReadBuffer() == FALSE) {
        SdpPrint(SDP_DBG_CONFIG_ERROR | SDP_DBG_CONFIG_WARNING,
                 ("FinalizeConnection, config done, no buffers!\n"));
        disconnect = TRUE;
    }
    else if (IsClient()) {
        SdpPrint(SDP_DBG_CONFIG_INFO,
                 ("FinalizeConnection, completing all pended connect requests\n"));

        CompletePendingConnectRequests(STATUS_SUCCESS);
    }
    else {
        //
        // We are the server in this connection, send down a read to
        // retrieve the client's search request
        //
        SdpPrint(SDP_DBG_CONFIG_INFO,
                 ("L2CA_ConfigInd, we are a server, acquire and read\n"));

        status = Acquire(pReadIrp);
        if (NT_SUCCESS(status)) {
            SdpPrint(SDP_DBG_CONFIG_INFO,
                     ("L2CA_ConfigInd, acquire and read succes\n"));

            FireWmiEvent(SdpServerLogTypeConnect, btAddress, outMtu);
            ReadAsync();
        }
        else {
            SdpPrint(SDP_DBG_CONFIG_ERROR | SDP_DBG_CONFIG_WARNING,
                     ("L2CA_ConfigInd, acquire and read not successful, 0x%x\n",
                      status));
            disconnect = TRUE;
        }
    }

    return disconnect;
}

void SdpConnection::CompletePendingConnectRequests(NTSTATUS status)
{
    CList<IRP, IRP_LE_OFFSET> irps;
    KIRQL irql;

    LockState(&irql);
    while (!connectList.IsEmpty()) {
        irps.AddTail(connectList.RemoveHead());
    }
    UnlockState(irql);

    while (!irps.IsEmpty()) {
        PIRP irp = irps.RemoveHead();

        irp->IoStatus.Status = status;
        if (NT_SUCCESS(status)) {
            BthSdpConnect *pConnect;
            BthConnect *pBthConnect;

            pConnect = NULL;
            pBthConnect = NULL;

            // DJH:  remove me later
            if (IoGetCurrentIrpStackLocation(irp)->
                    Parameters.DeviceIoControl.OutputBufferLength ==
                sizeof(BthSdpConnect)) {
                pConnect = (BthSdpConnect*) irp->AssociatedIrp.SystemBuffer;

                RtlZeroMemory(pConnect, sizeof(BthSdpConnect));
                pConnect->hConnection = hConnection;
                irp->IoStatus.Information = sizeof(BthSdpConnect);
            }
            else {
                pBthConnect = (BthConnect*) irp->AssociatedIrp.SystemBuffer;

                RtlZeroMemory(pBthConnect, sizeof(BthConnect));
                pBthConnect->hConnection = hConnection;
                irp->IoStatus.Information = sizeof(BthConnect);
            }

            AddFileObject(IoGetCurrentIrpStackLocation(irp)->FileObject);
        }
        else {
            irp->IoStatus.Information = 0;
        }

        SdpPrint(SDP_DBG_CONFIG_INFO,
                 ("Completing connect request %p with status 0x%x\n",
                  irp, status));

        Release(irp);

        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }
}

VOID
SdpConnection::_QueueCancelRoutine(
    PVOID Context,
    PIRP Irp
    )
{
    ((SdpConnection *) Context)->Release(Irp);
}

#endif // UNDER_CE


NTSTATUS
SdpConnection::SendServiceSearchRequest(
    PIRP pIrp
#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    , SdpServiceSearch *pSearch
#endif
    )
{
//  BthSdpServiceSearchRequest *pSearch;
//  PIO_STACK_LOCATION stack;
    PSDP_NODE pTree = NULL;
    PUCHAR stream = NULL, tmpStream;
    ULONG streamSize = 0;
    NTSTATUS status;
//  ULONG outlen;
    USHORT maxHandles, totalSize;
    UCHAR maxUuids;

    ContinuationState cs;
    SdpPDU pdu;

#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    maxHandles = pSearch->cMaxHandles;
    maxUuids   = (UCHAR) pSearch->cUUIDs;
    pTree = SdpInterface::CreateServiceSearchTree(pSearch->uuids,maxUuids);
#else
    stack = IoGetCurrentIrpStackLocation(pIrp);

    maxHandles = (USHORT)
        (stack->Parameters.DeviceIoControl.OutputBufferLength / sizeof(ULONG));

    maxUuids = GetMaxUuidsFromIrp(pIrp);

    pSearch = (BthSdpServiceSearchRequest *) pIrp->AssociatedIrp.SystemBuffer;
    pTree = SdpInterface::CreateServiceSearchTree(pSearch->uuids, maxUuids);
#endif
    if (pTree == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Error;
    }

    status = SdpStreamFromTreeEx(pTree,
                                 &stream,
                                 &streamSize,
                                 sizeof(SdpPDU),
                                 sizeof(maxHandles) + sizeof(cs));

    //
    // Our initial request does not have a continuation state, BUT if we get
    // a partial response, we need to send  request w/a cont state, so just
    // alloc room for it now
    //
    totalSize = sizeof(SdpPDU) +
                (USHORT) streamSize +
                sizeof(maxHandles) +
                sizeof(cs.size);

    SdpFreeTree(pTree);
    pTree = NULL;

    if (!NT_SUCCESS(status)) {
        goto Error;
    }

    if (totalSize > outMtu) {
        //
        // Can't xmit the request, sigh
        //
        status = STATUS_UNSUCCESSFUL;
        goto Error;
    }

    pdu.pduId = SdpPDU_ServiceSearchRequest; 
    pdu.transactionId = transactionId++;
    pdu.parameterLength = totalSize - sizeof(SdpPDU); 

    //
    // Write the PDU, then skip over the already written stream
    //
    tmpStream = pdu.Write(stream);

    //
    // increment past already writen search pattern
    //
    tmpStream += streamSize;

    maxHandles = RtlUshortByteSwap(maxHandles);
    RtlCopyMemory(tmpStream, &maxHandles, sizeof(maxHandles));
    tmpStream += sizeof(maxHandles);
    
    ASSERT(cs.size == 0);
    cs.Write(tmpStream);

#if DBG
    tmpStream += cs.GetTotalSize();
    ASSERT((tmpStream - stream) == totalSize);
#endif 

    SdpPrint(SDP_DBG_CLIENT_INFO, ("sending initial request for service search\n"));
             
    return SendInitialRequestToServer(pIrp, stream, totalSize, pdu.pduId);

Error:
    if (pTree) {
        SdpFreeTree(pTree);
    }

    if (stream) {
        ExFreePool(stream);
    }

    ReleaseAndProcessNextRequest(pIrp);

    return status;
}

NTSTATUS
SdpConnection::SendAttributeSearchRequest(
    PIRP pIrp
#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    , SdpAttributeSearch *pSearch
#endif
    )
{
//  BthSdpAttributeSearchRequest *pSearch;
    PSDP_NODE pTree = NULL;
    PUCHAR stream = NULL, tmpStream;
    NTSTATUS status;
    ULONG streamSize = 0, recordHandle; // maxRange
    USHORT totalSize;
    
    SdpPDU pdu;

    ContinuationState cs;
    USHORT maxByteCount;

#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    recordHandle = RtlUlongByteSwap(pSearch->recordHandle);

    pTree = SdpInterface::CreateAttributeSearchTree(pSearch->range,
                           pSearch->numAttributes);
#else
    maxRange = GetMaxRangeFromIrp(pIrp);
    pSearch = (BthSdpAttributeSearchRequest *) pIrp->AssociatedIrp.SystemBuffer;

    pTree = SdpInterface::CreateAttributeSearchTree(pSearch->range, maxRange);
#endif
    if (pTree == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Error;
    }

    //
    // Our initial request does not have a continuation state, BUT if we get
    // a partial response, we need to send  request w/a cont state, so just
    // alloc room for it now by allocating sizeof(cs) instead of sizeof(cs.size)
    //
    status = SdpStreamFromTreeEx(pTree,
                                 &stream,
                                 &streamSize,
                                 sizeof(SdpPDU) + sizeof(recordHandle) + sizeof(maxByteCount), 
                                 sizeof(cs));

    totalSize = sizeof(SdpPDU) +
                sizeof(recordHandle) +
                sizeof(maxByteCount) +
                (USHORT) streamSize +
                sizeof(cs.size);

    SdpFreeTree(pTree);
    pTree = NULL;

    if (!NT_SUCCESS(status)) {
        goto Error;
    }

    if (totalSize > outMtu) {
        //
        // Can't xmit the request
        //
        status =  STATUS_UNSUCCESSFUL;
        goto Error;
    }

    // if (g_Support10B) {
    //     maxByteCount = 0xFFFF; 
    // }
    // else {
    //     maxByteCount = GetMaxByteCount(inMtu);
    // }
    maxByteCount = GetMaxByteCount(inMtu);

    pdu.pduId = SdpPDU_ServiceAttributeRequest;
    pdu.transactionId = transactionId++;
    pdu.parameterLength = totalSize - sizeof(SdpPDU);

    //
    // Write the PDU, then skip over the already written stream
    //
    tmpStream = pdu.Write(stream);


#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    // This is done at top of fcn for WinCE
    recordHandle = RtlUlongByteSwap(pSearch->recordHandle);
#endif

    RtlCopyMemory(tmpStream, &recordHandle, sizeof(recordHandle));
    tmpStream += sizeof(recordHandle);
    
    maxByteCount = RtlUshortByteSwap(maxByteCount);
    RtlCopyMemory(tmpStream, &maxByteCount, sizeof(maxByteCount));
    tmpStream += sizeof(maxByteCount);

    tmpStream += streamSize;

    ASSERT(cs.size == 0);
    cs.Write(tmpStream);

#if DBG
    tmpStream += cs.GetTotalSize();
    ASSERT((tmpStream - stream) == totalSize);
#endif // DBG

    SdpPrint(SDP_DBG_CLIENT_INFO, ("sending initial request for attribute search\n"));

    return SendInitialRequestToServer(pIrp, stream, totalSize, pdu.pduId);

Error:

    if (pTree) {
        SdpFreeTree(pTree);
    }

    if (stream) {
        ExFreePool(stream);
    }

    ReleaseAndProcessNextRequest(pIrp);

    return status;
}

NTSTATUS
SdpConnection::SendServiceSearchAttributeRequest(
    PIRP pIrp
#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    , SdpServiceAttributeSearch *pSearch
#endif
    )
{
//  BthSdpServiceAttributeSearchRequest *pSearch;
    PSDP_NODE pTreeService = NULL, pTreeAttrib = NULL;
    PUCHAR stream = NULL, tmpStream;
    NTSTATUS status;
    ULONG maxRange, streamSize, serviceSize, attribSize;
    int totalSize;
    USHORT maxByteCount;
    UCHAR maxUuids;

    SdpPDU pdu;
    ContinuationState cs;

#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    ASSERT(pSearch->cUUIDs <= MAX_UUIDS_IN_QUERY);
    maxUuids = (UCHAR) pSearch->cUUIDs;
    maxRange = pSearch->numAttributes;
#else
    maxUuids = GetMaxUuidsFromIrp(pIrp);
    maxRange = GetMaxRangeFromIrp(pIrp);

    pSearch = (BthSdpServiceAttributeSearchRequest *)
        pIrp->AssociatedIrp.SystemBuffer;
#endif

    pTreeService =
        SdpInterface::CreateServiceSearchTree(pSearch->uuids, maxUuids);

    if (pTreeService == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Error;
    }

    pTreeAttrib = SdpInterface::CreateAttributeSearchTree(pSearch->range, maxRange);
    if (pTreeAttrib == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Error;
    }

    status = ComputeNodeListSize(
        ListEntryToSdpNode(SdpNode_GetNextEntry(pTreeService)),
        &serviceSize);

    if (!NT_SUCCESS(status)) {
        goto Error;
    }

    status = ComputeNodeListSize(
        ListEntryToSdpNode(SdpNode_GetNextEntry(pTreeAttrib)),
        &attribSize);

    if (!NT_SUCCESS(status)) {
        goto Error;
    }

    status = SdpStreamFromTreeEx(pTreeService,
                                 &stream,
                                 &streamSize,
                                 sizeof(SdpPDU),    
                                 sizeof(maxByteCount) + attribSize + sizeof(cs));

    if (!NT_SUCCESS(status)) {
        goto Error;
    }

    status = 
        NodeToStream(
            ListEntryToSdpNode(SdpNode_GetNextEntry(pTreeAttrib)),
            stream + sizeof(SdpPDU) + serviceSize + sizeof(maxByteCount) );

    if (!NT_SUCCESS(status)) {
        goto Error;
    }

    SdpFreeTree(pTreeService);
    SdpFreeTree(pTreeAttrib);

    pTreeService = NULL;
    pTreeAttrib = NULL;

    totalSize = sizeof(SdpPDU)        +
                serviceSize           +
                sizeof(maxByteCount)  +
                attribSize            +
                sizeof(cs.size);

    if (totalSize > outMtu) {
        status = STATUS_UNSUCCESSFUL;
        goto Error;
    }

    // if (g_Support10B) {
    //     maxByteCount = 0xFFFF;
    // }
    // else {
    //     maxByteCount = GetMaxByteCount(inMtu);
    // }
    maxByteCount = GetMaxByteCount(inMtu);

    pdu.pduId = SdpPDU_ServiceSearchAttributeRequest;
    pdu.transactionId = transactionId++;
    pdu.parameterLength = (USHORT) (totalSize - sizeof(SdpPDU));

    tmpStream = pdu.Write(stream);

    //
    // service search pattern was written out by the call to SdpStreamFromTreeEx
    //
    tmpStream += serviceSize;

    maxByteCount = RtlUshortByteSwap(maxByteCount);
    RtlCopyMemory(tmpStream, &maxByteCount, sizeof(maxByteCount));
    tmpStream += sizeof(maxByteCount);

    //
    // AttributeIDList was written out by the call to NodeToStream 
    //
    tmpStream += attribSize;

    ASSERT(cs.size == 0);
    cs.Write(tmpStream);

#if DBG
    tmpStream += cs.GetTotalSize();
    ASSERT((tmpStream - stream) == totalSize);
#endif // DBG

    SdpPrint(SDP_DBG_CLIENT_INFO, ("sending initial request for service attribute search\n"));

    return SendInitialRequestToServer(pIrp, stream, (USHORT) totalSize, pdu.pduId);

Error:
    if (stream) {
        ExFreePool(stream);
    }

    if (pTreeService) {
        SdpFreeTree(pTreeService);
    }

    if (pTreeAttrib) {
        SdpFreeTree(pTreeAttrib);
    }

    ReleaseAndProcessNextRequest(pIrp);

    return status;
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
BOOLEAN
SdpConnection::_RegisterConnectionOnTimer(
    SdpConnection *pConnection,
    BOOLEAN Register
    )
{
    BOOLEAN empty;
    BOOLEAN res = TRUE;
    static BOOLEAN firstCall = TRUE;

    if (firstCall) {
        firstCall = FALSE;
        InitializeListHead(&_TimeoutListHead);
        KeInitializeSpinLock(&_TimeoutListLock);
        KeInitializeTimerEx(&_TimeoutTimer, NotificationTimer);
        KeInitializeDpc(&_TimeoutTimerDpc,
                        _TimeoutTimerDpcProc,
                        NULL);
    }

    KIRQL irql;
    KeAcquireSpinLock(&_TimeoutListLock, &irql);

    if (Register == FALSE) {
        SdpConnection *pConFind = NULL;
        PLIST_ENTRY pEntry;

        res = FALSE;

        SdpPrint(SDP_DBG_TO_INFO,
                 ("trying to unregister connection %p\n", pConnection));
        pEntry = _TimeoutListHead.Flink;
        while (pEntry != &_TimeoutListHead) {
            pConFind = CONTAINING_RECORD(pEntry, SdpConnection, timerEntry);

            if (pConFind == pConnection) {
                res = TRUE;
                RemoveEntryList(&pConnection->timerEntry);
                pConnection->Release(&_TimeoutTimerDpc);

                SdpPrint(SDP_DBG_TO_INFO, ("found it in list, removing\n"));
                break;
            }

            pEntry = pEntry->Flink;
        }

        if (IsListEmpty(&_TimeoutListHead)) {
            SdpPrint(SDP_DBG_TO_TRACE, ("stopping timeout timer\n"));
            KeCancelTimer(&_TimeoutTimer);
        }
    }
    else {
        empty = IsListEmpty(&_TimeoutListHead);

        InsertTailList(&_TimeoutListHead, &pConnection->timerEntry);

        pConnection->timeoutTime = 0x0;
        pConnection->Acquire(&_TimeoutTimerDpc);

        SdpPrint(SDP_DBG_TO_INFO, ("adding connection %p to timeout list\n", pConnection));

        if (empty) {
            LARGE_INTEGER scanTime;

            SdpPrint(SDP_DBG_TO_TRACE, ("starting timeout timer\n"));

            scanTime = RtlConvertLongToLargeInteger(-10*1000*1000 * SDP_SCAN_INTERVAL);

            KeSetTimerEx(&_TimeoutTimer,
                         scanTime,
                         SDP_SCAN_INTERVAL * 1000, // in ms
                         &_TimeoutTimerDpc);
        }
    }

    KeReleaseSpinLock(&_TimeoutListLock, irql);

    return res;
}

void
SdpConnection::_TimeoutTimerDpcProc(
    IN PKDPC Dpc,
    IN PVOID DeviceObject,
    IN PVOID Context1,
    IN PVOID Context2
    )
{
    CList<SdpConnection, FIELD_OFFSET(SdpConnection, timerEntry)> list;

    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Context1);
    UNREFERENCED_PARAMETER(Context2);

    KIRQL irql;

    KeAcquireSpinLock(&_TimeoutListLock, &irql);

    SdpPrint(SDP_DBG_TO_TRACE, ("entering timeout timer DPC\n"))

    SdpConnection *pCon = NULL;
    PLIST_ENTRY pEntry;

    pEntry = _TimeoutListHead.Flink;

    while (pEntry != &_TimeoutListHead) {
        LONG count;

        pCon = CONTAINING_RECORD(pEntry, SdpConnection, timerEntry);
        count = InterlockedIncrement(&pCon->timeoutTime);

        SdpPrint(SDP_DBG_TO_INFO,
                 ("connection %p has elapsed %d s, will timeout at %d s\n",
                  pCon, count, pCon->maxTimeout));
        //
        // Advance to the next element first and foremost
        //
        pEntry = pEntry->Flink;

        //
        // If we have timed out or the requestor has cancelled their request
        // we will yank it out of the timer list and cancel the underlying
        // read request.  In the read completion routine we will process accordingly
        //
        // NOTE:  letting the user cancel the request before we get a response
        //        is tricky b/c we are only allowed on pended request to th server
        //        and cancelling the underlying read means we need to shut down
        //        entire connection and that is not fair for all the other applications
        //        that are piggybacking this connection
        //
        if (count == pCon->maxTimeout /* || pCon->pRequestorIrp->Cancel*/) {
            //
            // Then remove this element from the list and then add it to our
            // local list
            //
            RemoveEntryList(&pCon->timerEntry);
            list.AddTail(pCon);
        }
    }

    KeReleaseSpinLock(&_TimeoutListLock, irql);

    while (!list.IsEmpty()) {
        pCon = list.RemoveHead();

        SdpPrint(SDP_DBG_TO_INFO,
                 ("cancelling irp %p on connection %p\n", pCon->pReadIrp, pCon));

        IoCancelIrp(pCon->pReadIrp);
        pCon->Release(&_TimeoutTimerDpc);
    }
}

NTSTATUS
SdpConnection::_TimeoutDisconnectComplete(
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp,
    PVOID pContext
    )
{
    SdpConnection *pCon = (SdpConnection *) pContext;

    SdpPrint(SDP_DBG_TO_TRACE, ("timeout disconnection complete\n"));

    //
    // By setting the address to zero, we are allowing new connections to be
    // created to this device (b/c we will not find an active connection in the
    // SdpInterface list)
    //
    pCon->btAddress = 0x0;
    pCon->Release(pIrp);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
SdpConnection::_ReadAsyncCompletion(
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp,
    PVOID pContext
    )
{
    SdpPrint(SDP_DBG_BRB_TRACE, ("read async complete, status = 0x%x\n",
                                pIrp->IoStatus.Status));

    UNREFERENCED_PARAMETER(pDeviceObject);
    UNREFERENCED_PARAMETER(pIrp);

    SdpConnection *pCon = (SdpConnection*) pContext;

    BOOLEAN removed;

    //
    // Will only be in the timer queue if the read is from a client connection
    //
    if (pCon->IsClient()) {
        removed = _RegisterConnectionOnTimer(pCon, FALSE);
    }
    else {
        removed = TRUE;
    }

    //
    // If we have successfully removed the conneciton from the timer, then the
    // timer did not have a change to timeout our I/O and we can proceed.  If
    // the remove failed, then the timer has fired and cancelled our I/O.  There
    // is an edge condition where the I/O completes and have removed the connection
    // from the timer list in the timer (to cancel it) but have not yet actually
    // cancelled the irp.  Because of this, we must allocate a new irp to do the
    // disconnect (but we can still reuse our read brb).
    //
    if (removed == FALSE) {
        SdpPrint(SDP_DBG_TO_INFO,
                 ("read async complete, connection NOT in timer queue\n"));

        //
        // The user must close the connection up receiving this error code.  We
        // shall disconnect from the device but keep the SdpConnection in the
        // SdpInterface list until all clients close down their handles.
        //
        KIRQL irql;

        




        //
        // By setting the channel state to closed, SdpInterface::AcquireConnection
        // will gracefully fail, not allowing any new queries to go out on the
        // wire.
        //
        pCon->LockState(&irql);
        pCon->channelState = SdpChannelStateClosed;
        pCon->UnlockState(irql);

        //
        // Can't complete the client requests here b/c we free pool when we do so
        // and we could be at DPC freeing paged pool.  Just drop into the worker
        // item and do it there.
        //
    }

    pCon->ReadAsyncComplete();

    return STATUS_MORE_PROCESSING_REQUIRED;
}

void SdpConnection::ReadAsyncComplete()
{
    ExInitializeWorkItem(&readWorkItem, _ReadWorkItemCallback, (PVOID) this);
    ExQueueWorkItem(&readWorkItem, DelayedWorkQueue);
}

void SdpConnection::_ReadWorkItemCallback(IN PVOID Parameter)
{
    ((SdpConnection*) Parameter)->ReadWorkItemWorker();
}

void SdpConnection::ReadWorkItemWorker()
{
    if (GetChannelState() == SdpChannelStateClosed) {
        //
        // Channel is closed, clean up
        //
        // Passing STATIS_IO_TIMEOUT will complete all of the pended irps as
        // well
        //
        CompleteClientRequest(0, STATUS_IO_TIMEOUT);

        //
        // The timer could have this connection in its local list and cancel cancel
        // this read irp....so we can use pCon->pReadIrp...BUT we can use
        // pCon->pWriteIrp b/c it is in no timer queue and thus cannot be
        // cancelled
        //
        BRB *pBrb = pReadBrb;
        RtlZeroMemory(pBrb, sizeof(BRB));

        pBrb->BrbHeader.Length = sizeof (_BRB_L2CA_DISCONNECT_REQ);
        pBrb->BrbHeader.Type = BRB_L2CA_DISCONNECT_REQ;
        pBrb->BrbHeader.Status = STATUS_SUCCESS;
        pBrb->BrbL2caDisconnectReq.hConnection = hConnection;

        IoReuseIrp(pWriteIrp, STATUS_SUCCESS);

        Acquire(pWriteIrp);
        SendBrbAsync(pBrb, _TimeoutDisconnectComplete, pWriteIrp, (PVOID) this);
    }
    else if (!NT_SUCCESS(pReadIrp->IoStatus.Status)) {
        if (IsClient()) {
            CompleteClientRequest(0, pReadIrp->IoStatus.Status);
        }
        else {
            






            Release(pReadIrp);
        }
    }
    else if (pReadBrb->BrbL2caAclTransfer.BufferSize < sizeof(SdpPDU)) {
        if (IsClient()) {
            CompleteClientRequest(0, STATUS_INVALID_NETWORK_RESPONSE);
        }
        else {
            USHORT transactionId;

            if (pReadBrb->BrbL2caAclTransfer.BufferSize >=
                 (sizeof(UCHAR) + sizeof(USHORT))) {

                RtlRetrieveUshort(&transactionId,
                                  (((PUCHAR) pReadBrb->BrbL2caAclTransfer.Buffer)+1));
                transactionId = RtlUshortByteSwap(transactionId);
            }
            else {
                //
                // Is there a good value for this?
                //
                transactionId = 0x00;
            }

            WriteSdpError(transactionId , SDP_ERROR_INVALID_PDU_SIZE);
        }
    }
    else {
        PUCHAR pResponse;
        SdpPDU pdu;

        pResponse = (PUCHAR) pdu.Read(pReadBrb->BrbL2caAclTransfer.Buffer);

        ULONG_PTR info = 0;
        NTSTATUS status = STATUS_SUCCESS;
        SDP_ERROR sdpError;

        if (IsClient()) {

            if (pRequestorIrp->Cancel) {
                CompleteClientRequest(0, STATUS_CANCELLED);
                return;
            }

            switch (pdu.pduId) {
            case SdpPDU_ServiceSearchResponse:
                status = OnServiceSearchResponse(&pdu, pResponse, &info);
                break;

            case SdpPDU_ServiceAttributeResponse:
                status = OnServiceAttributeResponse(&pdu, pResponse, &info);
                break;

            case SdpPDU_ServiceSearchAttributeResponse:
                status = OnServiceSearchAttributeResponse(&pdu, pResponse, &info);
                break;

            case SdpPDU_Error:
            default:
                status = STATUS_INVALID_NETWORK_RESPONSE;
                break;
            }

            if (status != STATUS_PENDING) {
                CompleteClientRequest(info, status);
            }
        }
        else {
            switch (pdu.pduId) {
            case SdpPDU_ServiceSearchRequest:
                ASSERT(!IsClient());
                status = OnServiceSearchRequest(&pdu, pResponse, &sdpError);
                break;

            case SdpPDU_ServiceAttributeRequest:
                ASSERT(!IsClient());
                status = OnServiceAttributeRequest(&pdu, pResponse, &sdpError);
                break;

            case SdpPDU_ServiceSearchAttributeRequest:
                ASSERT(!IsClient());
                status = OnServiceSearchAttributeRequest(&pdu, pResponse, &sdpError);
                break;

            default:
                sdpError = SDP_ERROR_INVALID_REQUEST_SYNTAX;
                status = STATUS_UNSUCCESSFUL;
            }

            if (!NT_SUCCESS(status)) {
                ReadAsync();
                WriteSdpError(pdu.transactionId, sdpError);
            }
        }
    }
}
#else  // UNDER_CE

NTSTATUS SdpConnection::ReadWorkItemWorker(PUCHAR pResponse, USHORT usSize)  {
    SdpPDU pdu;
    ULONG_PTR info = 0;  // ignored by WinCE.
    NTSTATUS status = STATUS_SUCCESS;
    SDP_ERROR sdpError;

    if (usSize < sizeof(SdpPDU))  {
        if (IsClient())  {
            return ERROR_INVALID_PARAMETER;
        }
        else {
            USHORT transactionId;
            if (usSize >= sizeof(UCHAR) + sizeof(USHORT))  {
                RtlRetrieveUshort(&transactionId,pResponse+1);
                transactionId = RtlUshortByteSwap(transactionId);
            }
            else {
                transactionId = 0x00;
            }
            WriteSdpError(transactionId , SDP_ERROR_INVALID_PDU_SIZE);
        }
    }

    pResponse = (PUCHAR) pdu.Read(pResponse); 

    if (IsClient()) {
        switch (pdu.pduId) {
        case SdpPDU_ServiceSearchResponse:
            status = OnServiceSearchResponse(&pdu, pResponse, &info);
            break;

        case SdpPDU_ServiceAttributeResponse:
            status = OnServiceAttributeResponse(&pdu, pResponse, &info);
            break;

        case SdpPDU_ServiceSearchAttributeResponse:
            status = OnServiceSearchAttributeResponse(&pdu, pResponse, &info);
            break;

        case SdpPDU_Error:
        default:
            status = STATUS_INVALID_NETWORK_RESPONSE;
            break;
        }
    }
    else {
        switch (pdu.pduId) {
        case SdpPDU_ServiceSearchRequest:
            ASSERT(!IsClient());
            status = OnServiceSearchRequest(&pdu, pResponse, &sdpError);
            break;

        case SdpPDU_ServiceAttributeRequest:
            ASSERT(!IsClient());
            status = OnServiceAttributeRequest(&pdu, pResponse, &sdpError);
            break;

        case SdpPDU_ServiceSearchAttributeRequest:
            ASSERT(!IsClient());
            status = OnServiceSearchAttributeRequest(&pdu, pResponse, &sdpError);
            break;

        default:
            sdpError = SDP_ERROR_INVALID_REQUEST_SYNTAX;
            status = STATUS_UNSUCCESSFUL;
        }

        if (!NT_SUCCESS(status)) {
            ReadAsync();
            WriteSdpError(pdu.transactionId, sdpError);
        }
    }
    return status;
}
#endif  // UNDER_CE


NTSTATUS
SdpConnection::SendInitialRequestToServer(
    PIRP pClientIrp,
    PVOID pRequest,
    USHORT requestLength,
    UCHAR pduId
    )
{
    u.Client.lastPduRequest = pduId;

    ASSERT (!u.Client.pRequest); // non-NULL here indicates mem leak
    //
    // raw request length is the guts of the request w/out the PDU header and
    // without the ContinuationState of zero
    //
    u.Client.rawParametersLength = requestLength - sizeof(SdpPDU) - sizeof(UCHAR);
    u.Client.pRequest = pRequest;

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
	// In WinCE on initial request we queue it up.
    IoMarkIrpPending(pClientIrp);
    pRequestorIrp = pClientIrp;

    //
    // Send the read down before we send out the request in case the response 
    // is super fast
    //
    ReadAsync();

    _RegisterConnectionOnTimer(this, TRUE);

    //
    // Store the request buffer so that we can resend if the response needs
    // to be segmented
    //
    Write(pRequest, requestLength);

    return STATUS_PENDING;
#else // UNDER_CE
    return SendOrQueueRequest();
#endif
}

void
SdpConnection::CompleteClientRequest(
    ULONG_PTR info,
    NTSTATUS status
    )
{
    if (u.Client.pRequest)
        ExFreePool(u.Client.pRequest);
    
//  PIRP pIrp = pRequestorIrp;

    FlushClientRequestCache(status);

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    //
    // If the buffer is too small, we must conver this to success so that we
    // can send the requestor the correct amount of data
    //
    if (status == STATUS_BUFFER_TOO_SMALL) {
        status = STATUS_SUCCESS;
    }

    if (status == STATUS_IO_TIMEOUT) {
        CIRP_LIST list;

        Release(pIrp);
        clientQueue.Cleanup(&list);

        PIRP irp;

        while (!list.IsEmpty()) {
            irp = list.RemoveHead();

            Release(irp);

            irp->IoStatus.Information = 0;
            irp->IoStatus.Status = status;
            IoCompleteRequest(irp, IO_NO_INCREMENT);
        }
    }
    else {
        ReleaseAndProcessNextRequest(pIrp);
    }

    pIrp->IoStatus.Information = info;
    pIrp->IoStatus.Status = status;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
#endif // UNDER_CE
}

void 
SdpConnection::CleanUpServerSearchResults()
{
    if (u.Server.pOriginalAlloc) {
        ExFreePool(u.Server.pOriginalAlloc);
    }

    if (u.Server.pClientRequest) {
        ExFreePool(u.Server.pClientRequest);
    }

    RtlZeroMemory(&u.Server, sizeof(u.Server));
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
NTSTATUS
SdpConnection::ReadAsync(
    BOOLEAN shortOk
    )
{
    IoReuseIrp(pReadIrp, STATUS_SUCCESS);

    RtlZeroMemory(pReadBrb, sizeof(BRB));

    pReadBrb->BrbHeader.Length = sizeof (_BRB_L2CA_ACL_TRANSFER);
    pReadBrb->BrbHeader.Type = BRB_L2CA_ACL_TRANSFER;
    pReadBrb->BrbL2caAclTransfer.BufferMDL = NULL;
    pReadBrb->BrbL2caAclTransfer.Buffer = pReadBuffer;
    pReadBrb->BrbL2caAclTransfer.BufferSize = inMtu;
    pReadBrb->BrbL2caAclTransfer.hConnection = hConnection;
    pReadBrb->BrbL2caAclTransfer.TransferFlags = BTHPORT_TRANSFER_DIRECTION_IN;
    if (shortOk) {
        pReadBrb->BrbL2caAclTransfer.TransferFlags |= BTHPORT_SHORT_TRANSFER_OK;    
    }
    pReadBrb->BrbHeader.Status = STATUS_SUCCESS;

    return SendBrbAsync(pReadBrb,
                        _ReadAsyncCompletion,
                        pReadIrp, 
                        (PVOID) this);
}

NTSTATUS
SdpConnection::SendBrbAsync(
    PBRB pBrb, 
    PIO_COMPLETION_ROUTINE Routine,
    PIRP pIrp,
    PVOID pContext,
    PVOID pArg1,
    PVOID pArg2,
    PVOID pArg3,
    PVOID pArg4)
{
    PIO_STACK_LOCATION stack;
    KEVENT event;
    NTSTATUS status;

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

    return IoCallDriver(pDevice->m_Struct.FunctionalDeviceObject, pIrp);
}

NTSTATUS SdpConnection::SendBrb(PBRB pBrb, PIRP pIrp)
{
    NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;
    BOOLEAN ownIrp = FALSE;
        
    PIO_STACK_LOCATION stack;
    KEVENT event;

    if (pIrp == NULL) {
        pIrp = IoAllocateIrp(pDevice->m_Struct.FunctionalDeviceObject->StackSize, 
                             FALSE);
        ownIrp = TRUE;

        if (pIrp == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    else {
        IoReuseIrp(pIrp, STATUS_SUCCESS);
    }

    stack = IoGetNextIrpStackLocation(pIrp);
    stack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    stack->Parameters.DeviceIoControl.IoControlCode =
        IOCTL_INTERNAL_BTH_SUBMIT_BRB;
    stack->Parameters.Others.Argument1 = pBrb;

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

    IoSetCompletionRoutine(pIrp,
                           SendBrbCompletion,
                           (PVOID) &event,
                           TRUE, 
                           TRUE,
                           TRUE);

    status = IoCallDriver(pDevice->m_Struct.FunctionalDeviceObject, pIrp);
    
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event,
                              Executive,
                              KernelMode,
                              FALSE, 
                              NULL);

        status = pIrp->IoStatus.Status;
    }

    if (ownIrp) {
        IoFreeIrp(pIrp);
    }

    return status;
}

NTSTATUS 
SdpConnection::Write(
    PVOID pBuffer,
    ULONG length
    )
{
    PIO_STACK_LOCATION stack;

    RtlZeroMemory(&writeBrb, sizeof(writeBrb));

    writeBrb.BrbHeader.Length = sizeof (_BRB_L2CA_ACL_TRANSFER);
    writeBrb.BrbHeader.Type = BRB_L2CA_ACL_TRANSFER;

    writeBrb.BrbL2caAclTransfer.BufferMDL = NULL;
    writeBrb.BrbL2caAclTransfer.Buffer = pBuffer;
    writeBrb.BrbL2caAclTransfer.BufferSize = length;
    writeBrb.BrbL2caAclTransfer.hConnection = hConnection;
    writeBrb.BrbL2caAclTransfer.TransferFlags =  0;

    writeBrb.BrbHeader.Status = STATUS_SUCCESS;

    IoReuseIrp(pWriteIrp, STATUS_SUCCESS);

    // return SendBrb(&writeBrb, pWriteIrp);
    stack = IoGetNextIrpStackLocation(pWriteIrp);
    stack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    stack->Parameters.DeviceIoControl.IoControlCode =
        IOCTL_INTERNAL_BTH_SUBMIT_BRB;
    stack->Parameters.Others.Argument1 = &writeBrb;

    IoSetCompletionRoutine(pWriteIrp,
                           SendBrbCompletion,
                           NULL,  
                           TRUE, 
                           TRUE,
                           TRUE);

    return IoCallDriver(pDevice->m_Struct.FunctionalDeviceObject, pWriteIrp);
}
#endif // UNDER_CE

NTSTATUS
SdpConnection::WriteSdpError(
    USHORT transactionId,
    SDP_ERROR sdpErrorValue
    )
{
    SdpPDU pdu;
    UCHAR buffer[sizeof(SdpPDU) + sizeof(USHORT)];
    USHORT param;

    //
    // write an error back
    // 
    pdu.pduId = SdpPDU_Error;
    pdu.transactionId = transactionId;
    pdu.parameterLength = sizeof(USHORT);
    pdu.Write(buffer);

    param = sdpErrorValue;
    param = RtlUshortByteSwap(param);
    RtlCopyMemory(buffer + sizeof(SdpPDU), &param, sizeof(param));

    return Write(buffer, sizeof(buffer)); 
}

void 
SdpConnection::FlushClientRequestCache(
    NTSTATUS status
    )
{
    if (status != STATUS_BUFFER_TOO_SMALL) {
        if (u.Client.pLastRange) {
            delete[] u.Client.pLastRange;
        }

        if (u.Client.AttributeSearch.pResponse) {
            delete[] u.Client.AttributeSearch.pResponse; 
        }
    }

    RtlZeroMemory(&u.Client, sizeof(u.Client));
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
NTSTATUS
SdpConnection::IsPreviousRequest(
    PIRP pIrp,
    UCHAR pduId,
    SdpQueryUuid *pUuid,
    SdpAttributeRange *pRange,
    ULONG numRange,
    ULONG_PTR* pInfo
    )
/*++

    Right now, only the last query is saved.  If more than one application is 
    trying to query the server, the following will occur:
    
    App A runs query, output buffer too small, result is cached
    App B runs query, blows away cached result
    App A resends query, instead of getting cached version, must go over the wire
    
    One solution to this is to store the cached results in the file object.  
    That way, the results are stored on an app by app basis.  This limits the 
    caching to one per file handle.
    
  --*/
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if (u.Client.lastPduRequest == pduId && pduId != SdpPDU_ServiceSearchRequest) {
        ASSERT(u.Client.pLastRange != NULL);

        ULONG rangeSize = numRange * sizeof(SdpAttributeRange);

        //
        // Always compare ranges b/c the 2 response we cache both have ranges
        // in their requests
        //
        if (numRange == u.Client.numRange &&
            (rangeSize ==RtlCompareMemory(pRange, u.Client.pLastRange, rangeSize))) {
            //
            // If this is a service + attribute search, check the UUIDS 
            // otherwise just drop in b/c the attribs already matched
            //
            if ((pduId == SdpPDU_ServiceSearchAttributeRequest &&
                 sizeof(u.Client.lastUuid) !=
                 RtlCompareMemory(pUuid, u.Client.lastUuid, sizeof(u.Client.lastUuid)))
                ||
                pduId == SdpPDU_ServiceAttributeRequest) {

                BthSdpStreamResponse *pResponse;
                PIO_STACK_LOCATION stack;
                ULONG outLen, responseSize, toCopy;

                stack = IoGetCurrentIrpStackLocation(pRequestorIrp);
                pResponse =  (BthSdpStreamResponse *)
                    pRequestorIrp->AssociatedIrp.SystemBuffer;

                outLen = stack->Parameters.DeviceIoControl.OutputBufferLength;
                responseSize =
                    outLen - (sizeof(*pResponse) - sizeof(pResponse->response));

                *pInfo = sizeof(pResponse) - sizeof(pResponse->response);
                pResponse->requiredSize = u.Client.AttributeSearch.responseLength;

                if (u.Client.AttributeSearch.responseLength > responseSize) {
                    toCopy = responseSize;
                    status = STATUS_BUFFER_TOO_SMALL;
                }
                else {
                    toCopy = u.Client.AttributeSearch.responseLength;
                    status = STATUS_SUCCESS;
                }

                pResponse->responseSize = toCopy;

                *pInfo += toCopy;
                RtlCopyMemory(pResponse->response,
                              u.Client.AttributeSearch.pResponse,
                              toCopy);
            }
        }
    }

    FlushClientRequestCache(status);

    //
    // We want to give the request the answer, but we must return success for 
    // any of the data to propagate back up.  We don't do this before the flush
    // cache call b/c otherwise we would blow away the cached data.
    //
    if (status == STATUS_BUFFER_TOO_SMALL) {
        status = STATUS_SUCCESS;
    }

    return status;
}

#endif // UNDER_CE

BOOLEAN
SdpConnection::DoRequestsMatch(
    PBUFFER_OFFSET Offsets,
    PUCHAR ContinuedRequest
    )
/*++

Compares different offsets and lengths into a pair of buffers to see if
these offset / length pairs are the same.

ISSUE:  Technically speaking, the buffers do not have to be the same as long as
sequences they describe are functionally the same (if we are comparing
sequences), ie:

initial request
SEQ, SIZE is in 1 byte
    UUID16 <0x1234>
    UUID32 <0x12345678>

continued request
SEQ, SIZE is in 2 bytes
    UUID32 <0x00001234>
    UUID32 <0x12345678>

The 2 buffers that describe these sequences are different sizes, but they
describe the same query.  Currently we don't support this type of requery where
the continued query buffer is byte for byte different than the original.

To perform this type of compare, both sequences could be turned into SDP trees
and then compare the leaf types for equality.  This seems like too much work for
a very minor technical point.

  --*/
{
    for ( ; Offsets->Length != 0; Offsets++) {
        if (0 != memcmp((PUCHAR)u.Server.pClientRequest + Offsets->Offset,
                             ContinuedRequest + Offsets->Offset,
                             Offsets->Length)) {
            return FALSE;
        }
    }

    return TRUE;
}

NTSTATUS 
SdpConnection::SendContinuationRequestToServer(
    ContinuationState *pContState
    )
{
    if (sizeof(SdpPDU) + u.Client.rawParametersLength + pContState->GetTotalSize() >
        outMtu) {
        DbgPrint("----: could not fit request in a packet!!!\n");
        return STATUS_INVALID_NETWORK_RESPONSE;
    }

    SdpPDU pdu;

    pdu.Read(u.Client.pRequest);
    pdu.transactionId = transactionId++;
    pdu.parameterLength = u.Client.rawParametersLength + pContState->GetTotalSize();

    PUCHAR pStream = pdu.Write((PUCHAR) u.Client.pRequest);
    pStream += u.Client.rawParametersLength;

    pContState->Write(pStream);

#if DBG
    pStream += pContState->GetTotalSize();
    DbgPrint("%d = %d\n", ((PUCHAR) u.Client.pRequest) - pStream, 
        sizeof(SdpPDU) + pdu.parameterLength);

    ASSERT(((ULONG_PTR) (pStream - ((PUCHAR) u.Client.pRequest))) ==
           sizeof(SdpPDU) + pdu.parameterLength);
#endif 

#if ! ((defined (UNDER_CE) || defined (WINCE_EMULATION)))
    ReadAsync();

    _RegisterConnectionOnTimer(this, SDP_DEFAULT_TIMEOUT);

    Write(u.Client.pRequest, sizeof(SdpPDU) + pdu.parameterLength);

    return STATUS_PENDING;
#else

    int iRet = Write(u.Client.pRequest, sizeof(SdpPDU) + pdu.parameterLength);
    return (iRet==ERROR_SUCCESS) ? STATUS_PENDING : iRet;
#endif
}

NTSTATUS
SdpConnection::OnServiceSearchResponse(
    SdpPDU *pPdu,
    PUCHAR pBuffer,
    ULONG_PTR* pInfo
    )
{
    SDP_ERROR sdpError;

    if (pPdu->transactionId != transactionId-1) {
        //
        // Server error, ergh
        //
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
        *pInfo = pRequestorIrp->IoStatus.Information * sizeof(ULONG);
#endif        
        return STATUS_INVALID_NETWORK_RESPONSE;
    }

    ContinuationState cs;

    USHORT totalCount, currentCount;
    ULONG *pHandleList;
    USHORT handlesSoFar;

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    handlesSoFar = (USHORT) pRequestorIrp->IoStatus.Information;
#else
    handlesSoFar = (USHORT) ConnInformation;
#endif

    NTSTATUS status = SdpInterface::CrackServerServiceSearchResponse(
        pBuffer,
        pPdu->parameterLength,
        &totalCount,
        &currentCount,
        &pHandleList,
        &cs,
        &sdpError
        );

    if (!NT_SUCCESS(status)) {
        //
        // Server is giving back garbage
        //
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
        *pInfo = pRequestorIrp->IoStatus.Information * sizeof(ULONG);
#endif        
        return STATUS_INVALID_NETWORK_RESPONSE;
    }

    if (u.Client.ServiceSearch.totalRecordCount == 0) {
        //
        // First response
        //
        u.Client.ServiceSearch.totalRecordCount = totalCount;  

#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
        cClientBuf = totalCount*sizeof(unsigned long);
        if (cClientBuf) {
            pClientBuf = (unsigned char*) ExAllocatePoolWithTag(0,cClientBuf,0);
            if (!pClientBuf)  {
                return ERROR_OUTOFMEMORY;
            }
        }
#endif // UNDER_CE
    }
    else if (u.Client.ServiceSearch.totalRecordCount != totalCount) {
        //
        // Server is giving back inconsistent results, stop sending requests
        //
        *pInfo = handlesSoFar * sizeof(ULONG);
        return STATUS_INVALID_NETWORK_RESPONSE;
    }
        
    if (handlesSoFar + currentCount > totalCount) {
        //
        // Well, the server gave us too many handles.  
        //
        // TBD:  do we notify the user of this?  if so, how? 
        //
        // just copy over what we can for right now
        //
        currentCount = totalCount - handlesSoFar;
    }

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    PULONG pResult = (PULONG) pRequestorIrp->AssociatedIrp.SystemBuffer;
#else
    PULONG pResult = (PULONG) pClientBuf;
#endif
    for (ULONG i = 0; i < currentCount; i++) {
        ULONG handle;
        
        //
        // pHandleList may not be correctly aligned
        //
        RtlRetrieveUlong(&handle, (pHandleList+i));
        pResult[handlesSoFar + i] = RtlUlongByteSwap(handle);
    }

    handlesSoFar += currentCount;

    if (handlesSoFar == totalCount) {
        //
        // All done
        //
        *pInfo = handlesSoFar * sizeof(ULONG);
        return STATUS_SUCCESS;
    }
    else {
        //
        // Not all done, must ask for more
        //
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
        pRequestorIrp->IoStatus.Information = handlesSoFar;
#else
        ConnInformation = handlesSoFar;
#endif
        return SendContinuationRequestToServer(&cs);
    }
}

NTSTATUS
SdpConnection::OnServiceAttributeResponse(
    SdpPDU *pPdu,
    PUCHAR pBuffer,
    ULONG_PTR* pInfo
    )
{
    SDP_ERROR sdpError;

    if (pPdu->transactionId != transactionId-1) {
        //
        // Server erorr, ergh
        //
        SdpPrint(SDP_DBG_CLIENT_WARNING, ("invalid transaction id!"));
        return STATUS_INVALID_NETWORK_RESPONSE;
    }

    ContinuationState cs;

    UCHAR *pAttribList;
    USHORT listByteCount;

    NTSTATUS status = SdpInterface::CrackServerServiceAttributeResponse(
        pBuffer,
        pPdu->parameterLength,
        &listByteCount,
        &pAttribList,
        &cs,
        &sdpError);

    if (!NT_SUCCESS(status)) {
        return STATUS_INVALID_NETWORK_RESPONSE;
    }

#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
	ULONG bytesSoFar;

    ULONG storageSize;
    UCHAR type, sizeIndex;

    bytesSoFar = ConnInformation;

	if (bytesSoFar == 0) {
	    SdpRetrieveHeader(pAttribList, type, sizeIndex);
	    
	    if (type != (UCHAR) SDP_TYPE_SEQUENCE) {
	        return STATUS_INVALID_PARAMETER;
	    }

	    if (sizeIndex < 5 || sizeIndex > 7) {
	        return STATUS_INVALID_PARAMETER; 
	    }
	    SdpRetrieveVariableSize(pAttribList+1,
	                            sizeIndex,
	                            &u.Client.AttributeSearch.responseLength,
	                            &storageSize);	    
		u.Client.AttributeSearch.responseLength += (storageSize + sizeof(UCHAR));
        pClientBuf = (unsigned char*) ExAllocatePoolWithTag(0,u.Client.AttributeSearch.responseLength,0);
        cClientBuf = u.Client.AttributeSearch.responseLength;
        if (!pClientBuf) {
        	return ERROR_OUTOFMEMORY;
        }
    }

    if (bytesSoFar + listByteCount > u.Client.AttributeSearch.responseLength) {
        // server sent us too much data
        return STATUS_INVALID_NETWORK_RESPONSE;
    }
    memcpy(pClientBuf+bytesSoFar,pAttribList,listByteCount);
    ConnInformation += listByteCount;

    if (cs.size == 0) {
        return ERROR_SUCCESS;
    }
    else {
        return SendContinuationRequestToServer(&cs);
    }

#else // UNDER_CE
    BthSdpStreamResponse *pResponse;
    PIO_STACK_LOCATION stack;
    ULONG outLen, responseSize;
    ULONG_PTR bytesSoFar;

    stack = IoGetCurrentIrpStackLocation(pRequestorIrp);
    pResponse =  (BthSdpStreamResponse *) pRequestorIrp->AssociatedIrp.SystemBuffer;

    outLen = stack->Parameters.DeviceIoControl.OutputBufferLength;
    responseSize = outLen - (sizeof(*pResponse) - sizeof(pResponse->response));

    bytesSoFar = pRequestorIrp->IoStatus.Information;

    if (bytesSoFar == 0) {
        //
        // Retrieve the size from first part of the response
        //
        ULONG respTotalSize;
        ULONG storageSize;
        UCHAR type, sizeIndex;

        



        SdpRetrieveHeader(pAttribList, type, sizeIndex);
    
        if (type != (UCHAR) SDP_TYPE_SEQUENCE) {
            return STATUS_INVALID_PARAMETER;
        }

        if (sizeIndex < 5 || sizeIndex > 7) {
            return STATUS_INVALID_PARAMETER; 
        }

        SdpRetrieveVariableSize(pAttribList+1,
                                sizeIndex,
                                &u.Client.AttributeSearch.responseLength,
                                &storageSize);

        //
        // Take header and the storage space needed for the size into account
        //
        u.Client.AttributeSearch.responseLength += (storageSize + sizeof(UCHAR));

        DbgPrint("----: total response length is %d\n", u.Client.AttributeSearch.responseLength);

        if (u.Client.AttributeSearch.responseLength > responseSize) {
            //
            // We must cache off the request before we start copying the response
            // because they share the same buffer
            //
            SdpAttributeRange *pRange;
            ULONG numRange; 

            if (stack->Parameters.DeviceIoControl.IoControlCode ==
                IOCTL_BTH_SDP_ATTRIBUTE_SEARCH) {

                BthSdpAttributeSearchRequest *pSearch =  
                    (BthSdpAttributeSearchRequest *)
                    pRequestorIrp->AssociatedIrp.SystemBuffer;

                numRange = (stack->Parameters.DeviceIoControl.InputBufferLength -
                                (sizeof(*pSearch) - sizeof(pSearch->range))) /
                            sizeof(SdpAttributeRange);
                pRange = pSearch->range;

                u.Client.lastPduRequest = SdpPDU_ServiceAttributeRequest; 
            }
            else {
                //
                // ServiceSearchAttributeRequest
                //
                BthSdpServiceAttributeSearchRequest *pSearch =
                    (BthSdpServiceAttributeSearchRequest *)
                    pRequestorIrp->AssociatedIrp.SystemBuffer;

                numRange = (stack->Parameters.DeviceIoControl.InputBufferLength -
                                (sizeof(*pSearch) - sizeof(pSearch->range))) /
                            sizeof(SdpAttributeRange);

                pRange = pSearch->range;

                // Save off the uuids as well
                RtlCopyMemory(u.Client.lastUuid,
                              pSearch->uuids,
                              sizeof(u.Client.lastUuid));

                u.Client.lastPduRequest = SdpPDU_ServiceSearchAttributeRequest; 
            }

            u.Client.pLastRange = new(PagedPool, SDPC_TAG) SdpAttributeRange[numRange];

            if (u.Client.pLastRange) {
                u.Client.AttributeSearch.pResponse =
                    new(PagedPool, SDPC_TAG) UCHAR[u.Client.AttributeSearch.responseLength];

                if (u.Client.AttributeSearch.pResponse) {
                    RtlCopyMemory(u.Client.pLastRange,
                                  pRange,
                                  numRange * sizeof(SdpAttributeRange));
                    u.Client.numRange = numRange;
                }
                else {
                    return STATUS_INSUFFICIENT_RESOURCES;
                }
            }
            else {
                return STATUS_INSUFFICIENT_RESOURCES;
            }
        }
    }

    DbgPrint("----: bytes so far (%d), current PDU (%d)\n",  bytesSoFar, listByteCount);

    if (bytesSoFar + listByteCount > u.Client.AttributeSearch.responseLength) {
        //
        // server sent us too much data
        //
        return STATUS_INVALID_NETWORK_RESPONSE;
    }

    if (u.Client.AttributeSearch.pResponse) {
        // copy to cache
        RtlCopyMemory(u.Client.AttributeSearch.pResponse + bytesSoFar,
                      pAttribList,
                      listByteCount);
    }
    else {
        // copy to requestor buffer
        RtlCopyMemory(pResponse->response + bytesSoFar,
                      pAttribList,
                      listByteCount);
    }

    pRequestorIrp->IoStatus.Information = bytesSoFar + listByteCount;

    if (cs.size == 0) {
        DbgPrint("----: we are done!\n");

        //
        // We are done, give the user his response back
        //
        ASSERT(pRequestorIrp->IoStatus.Information == 
               u.Client.AttributeSearch.responseLength);

        //
        // Everything but the response buffer for now
        //
        *pInfo = sizeof(*pResponse) - sizeof(pResponse->response);

        if (u.Client.AttributeSearch.pResponse) {
            DbgPrint("----:  not enough client space to fill in, caching\n");

            //
            // Not enough room in the user buffer to give the entire response
            // back, so copy as much as possible
            //
            RtlCopyMemory(pResponse->response,
                          u.Client.AttributeSearch.pResponse,
                          responseSize);

            pResponse->responseSize = responseSize; 

            pResponse->requiredSize = u.Client.AttributeSearch.responseLength;
            status = STATUS_BUFFER_TOO_SMALL;
        }
        else {
            DbgPrint("----:  enough room!!!\n");

            //
            // There is enough room for all
            //
            pResponse->requiredSize = (ULONG) pRequestorIrp->IoStatus.Information;
            pResponse->responseSize = pResponse->requiredSize;  

            status = STATUS_SUCCESS;
        }

        *pInfo += pResponse->responseSize;

        return status;
    }
    else {
        DbgPrint("----: get more from the server!\n");

        //
        // get more info from the server
        //
        return SendContinuationRequestToServer(&cs);
    }
#endif // UNDER_CE
}

NTSTATUS
SdpConnection::OnServiceSearchAttributeResponse(
    SdpPDU *pPdu,
    PUCHAR pBuffer,
    ULONG_PTR* pInfo
    )
{
    return OnServiceAttributeResponse(pPdu, pBuffer, pInfo);
}

BOOLEAN
SdpConnection::IsRequestContinuation(
    PBUFFER_OFFSET Offsets,
    SdpPDU *pPdu,
    PUCHAR pRequest, 
    ContinuationState *pConStateReq,
    BOOLEAN &cleanUpRequired,
    BOOLEAN &contError
    )
{
    contError = FALSE;

    if (u.Server.pClientRequest == NULL) {
        cleanUpRequired = FALSE;
        if (pConStateReq->size != 0) {
            contError = TRUE;
        }
        return FALSE;
    }

    cleanUpRequired = TRUE;

    if (pPdu->pduId != u.Server.lastPduRequest) {
        return FALSE;
    }

    ContinuationState cs(this);

    ULONG length = pPdu->parameterLength;
    BOOLEAN continuation = FALSE;

    if (u.Server.clientRequestLength == (length - cs.GetTotalSize()) &&
        DoRequestsMatch(Offsets, pRequest)) {
        if (cs.IsEqual(*pConStateReq)) {
            cleanUpRequired = FALSE;
            continuation = TRUE;
        }
        else {
            contError = TRUE;
        }
    }
    else {
        continuation = FALSE;
    }

    return continuation;
}

void
SdpConnection::SendResponseToClient(
    SdpPDU *pPdu,
    UCHAR pduId,
    PUCHAR pResponse,
    ULONG responseSize
    )
{
    SdpPDU pdu;

    pdu.pduId = pduId;
    pdu.transactionId = pPdu->transactionId;
    pdu.parameterLength = (USHORT) (responseSize - sizeof(SdpPDU));

    pdu.Write(pResponse);

    ReadAsync();

    Write(pResponse, responseSize);

    //
    // Always free the response we generated
    //
    ExFreePool(pResponse);
}

NTSTATUS
SdpConnection::SendResponseToClient(
    NTSTATUS createPduStatus,
    const ContinuationState& cs,
    SdpPDU *pPdu,
    PUCHAR pClientRequest,
    PUCHAR pResponse,
    ULONG responseSize,
    UCHAR pduId,
    PSDP_ERROR pSdpError
    )
{
    NTSTATUS status = createPduStatus;

    if (NT_SUCCESS(status)) {
        if (cs.size != 0) {
            status = SaveQueryInfo(pPdu, pClientRequest, pSdpError);
    
            if (!NT_SUCCESS(status)) {
                ExFreePool(pResponse);
                CleanUpServerSearchResults();
                return status;
            }
        }

        //
        // If we can send the response in one PDU, then free our search results
        //
        if (cs.size == 0) {
            CleanUpServerSearchResults();
        }

        SendResponseToClient(pPdu, pduId, pResponse, responseSize);
    }
    else {
        //
        // Could not allocate a response buffer, free the search result and send
        // back an error
        //
        CleanUpServerSearchResults();

        status = STATUS_INSUFFICIENT_RESOURCES;
        *pSdpError = MapNtStatusToSdpError(status);
    }

    return status;
}

NTSTATUS
SdpConnection::SaveQueryInfo(
    SdpPDU *pPdu,
    PVOID pQuery,
    PSDP_ERROR pSdpError
    )
{
    USHORT len = pPdu->parameterLength - sizeof(UCHAR);
    PVOID pBuf = ExAllocatePoolWithTag(PagedPool, len, SDPC_TAG);

    if (pBuf == NULL) {
        *pSdpError = MapNtStatusToSdpError(STATUS_INSUFFICIENT_RESOURCES);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(pBuf, pQuery, len); 

    //
    // Save off request info
    //
    u.Server.pClientRequest = pBuf;
    u.Server.clientRequestLength = len; 
    u.Server.lastPduRequest = pPdu->pduId;
    
    return STATUS_SUCCESS;
}
    
NTSTATUS
SdpConnection::OnServiceSearchRequest(
    SdpPDU *pPdu,
    PUCHAR pBuffer,
    PSDP_ERROR pSdpError
    )
{
    PUCHAR pSequence;
    ULONG sequenceSize;
    ULONG length;
    USHORT maxRecordCount = 0;
    ContinuationState csReq;
    BUFFER_OFFSET offsets[2];

    // Last element needs to be zero for end marker
    RtlZeroMemory(&offsets[0], sizeof(offsets));

    length = pPdu->parameterLength;

    SdpPrint(SDP_DBG_SERVER_TRACE, ("OnServiceSearchRequest"));

    NTSTATUS status = SdpInterface::CrackClientServiceSearchRequest(
        pBuffer,
        length, 
        &pSequence,
        &sequenceSize,
        &maxRecordCount,
        &csReq,
        pSdpError
        );

    if (!NT_SUCCESS(status)) {
        SdpPrint(SDP_DBG_SERVER_WARNING,
                 ("cracking ServiceSearch request failed, 0x%x, %d",
                  status, (ULONG) *pSdpError));
        return status;
    }
    else {
        FireWmiEvent(SdpServerLogTypeServiceSearch,
                     pBuffer,
                     length);
    }

    PUCHAR pResponse = NULL;
    ULONG responseSize = 0;

    // The buffer to compare is the UUIDs sequence.
    offsets[0].Offset = 0;
    offsets[0].Length = sequenceSize;

    ContinuationState cs(this);

    BOOLEAN cleanUpReq = FALSE, contStateError = FALSE;

    if (IsRequestContinuation(&offsets[0], pPdu, pBuffer, &csReq, cleanUpReq, contStateError)) {
        _DbgPrintF(DBG_SDP_INFO, ("request is a continuation"));

        status = SdpInterface::CreateServiceSearchResponsePDU(
            u.Server.ServiceSearch.pRecordsNext,
            u.Server.ServiceSearch.recordCount,
            u.Server.ServiceSearch.totalRecordCount,
            &cs,
            outMtu,
            &pResponse,
            &responseSize,
            &u.Server.ServiceSearch.pRecordsNext,
            &u.Server.ServiceSearch.recordCount,
            pSdpError 
            );

        if (NT_SUCCESS(status)) {

            if (cs.size == 0) {
                //
                // Cleanup before sending down another read so there is no
                // contention issues
                //
                ASSERT(u.Server.ServiceSearch.pRecordsNext == NULL);
                CleanUpServerSearchResults();
            }

            SendResponseToClient(pPdu,
                                 SdpPDU_ServiceSearchResponse,
                                 pResponse,
                                 responseSize);
        }
        else {
            CleanUpServerSearchResults();
        }

        return status; 
    }
    else {
        SdpPrint(SDP_DBG_SERVER_INFO, ("request is not a continuation"));

        if (cleanUpReq) {
            SdpPrint(SDP_DBG_SERVER_WARNING, ("cleaning up previous results"));
            CleanUpServerSearchResults();
        }

        //
        // One of the following occurred:
        // 1 client sent a cs of non zero in the initial request
        // 2 sent the wrong cs state back after a partial response
        //
        if (contStateError) {
            SdpPrint(SDP_DBG_SERVER_WARNING, ("cont state error!"));
            *pSdpError = SDP_ERROR_INVALID_CONTINUATION_STATE;
            return STATUS_INVALID_PARAMETER;
        }
    }

    ULONG *pSearchResult;

#if SDP_VERBOSE
    ULONG indent = 0;
    SdpWalkStream(pSequence, sequenceSize, TestWalkStream, &indent);
#endif

    status = pSdpDB->ServiceSearchRequestResponseRemote(
        pSequence,
        sequenceSize,
        maxRecordCount,
        &pSearchResult,
        &u.Server.ServiceSearch.totalRecordCount,
        pSdpError);

    SdpPrint(SDP_DBG_SERVER_INFO, ("service search returned 0x%x (%d)",
             status, (ULONG) *pSdpError)); 

    if (!NT_SUCCESS(status)) {
        return status;
    }
    else {
        FireWmiEvent(SdpServerLogTypeServiceSearchResponse,
                     pSearchResult,
                     u.Server.ServiceSearch.totalRecordCount * sizeof(ULONG));
    }

    u.Server.pOriginalAlloc = pSearchResult;

    status = SdpInterface::CreateServiceSearchResponsePDU(
        pSearchResult,
        u.Server.ServiceSearch.totalRecordCount,
        u.Server.ServiceSearch.totalRecordCount, 
        &cs,
        outMtu,
        &pResponse,
        &responseSize,
        &u.Server.ServiceSearch.pRecordsNext,
        &u.Server.ServiceSearch.recordCount,
        pSdpError);

    SdpPrint(SDP_DBG_SERVER_INFO, ("create response PDU returned 0x%x (sdp err %d)\n",
             status, (ULONG) *pSdpError));

    return SendResponseToClient(status,
                                cs,
                                pPdu,
                                pBuffer,
                                pResponse,
                                responseSize,
                                SdpPDU_ServiceSearchResponse,
                                pSdpError);
}

NTSTATUS
SdpConnection::OnServiceAttributeRequest(
    SdpPDU *pPdu,
    PUCHAR pBuffer,
    PSDP_ERROR pSdpError
    )
{
    ULONG handle;
    USHORT maxByteCount;
    UCHAR *pAttribIds;
    ULONG attribIdsSize;
    ContinuationState csReq;
    BUFFER_OFFSET offsets[3];

    // Last element needs to be zero for end marker
    RtlZeroMemory(&offsets[0], sizeof(offsets));

    NTSTATUS status = SdpInterface::CrackClientServiceAttributeRequest(
        pBuffer,
        pPdu->parameterLength,
        &handle,
        &maxByteCount,
        &pAttribIds,
        &attribIdsSize,
        &csReq,
        pSdpError);

    if (!NT_SUCCESS(status)) {
        SdpPrint(SDP_DBG_SERVER_WARNING,
                 ("cracking AttributeSearch request failed, 0x%x, %d",
                 status, (ULONG) *pSdpError));

        return status;
    }
    else {
        FireWmiEvent(SdpServerLogTypeAttributeSearch,
                     pBuffer,
                     pPdu->parameterLength);
    }

    UCHAR *pSearchResult, *pResponse;
    ULONG searchSize, responseSize;

    // First buffer to compare is the ServiceRecordHandle.
    offsets[0].Offset = 0;
    offsets[0].Length = sizeof(handle);

    // Second buffer to compare is the attribute contents and header.
    // We need to skip over the MaxAttributeByteCount.
    offsets[1].Offset = sizeof(handle) + sizeof(maxByteCount);
    offsets[1].Length = attribIdsSize;

    ContinuationState cs(this);

    BOOLEAN cleanUpReq = FALSE, contStateError = FALSE;

    if (IsRequestContinuation(&offsets[0], pPdu, pBuffer, &csReq, cleanUpReq, contStateError)) {
        status = SdpInterface::CreateServiceAttributePDU(
            u.Server.StreamSearch.pSearchResults,
            u.Server.StreamSearch.resultsSize,
            &cs,
            maxByteCount,
            outMtu,
            &pResponse,
            &responseSize,
            &u.Server.StreamSearch.pSearchResults,
            &u.Server.StreamSearch.resultsSize,
            pSdpError);

        if (NT_SUCCESS(status)) {
            if (cs.size == 0) {
                CleanUpServerSearchResults();
            }

            SendResponseToClient(pPdu,
                                 SdpPDU_ServiceAttributeResponse,
                                 pResponse,
                                 responseSize);
        }
        else {
            CleanUpServerSearchResults();
        }

        return status;
    }
    else {
        SdpPrint(SDP_DBG_SERVER_INFO, ("request is not a continuation"));

        if (cleanUpReq) {
            //
            // There is no error here.  The client sent one request, go a partial
            // response and then asked for something else
            //
            SdpPrint(SDP_DBG_SERVER_WARNING, ("cleaning up previous results"));
            CleanUpServerSearchResults();
        }

        if (contStateError) {
            SdpPrint(SDP_DBG_SERVER_WARNING, ("cont state error!"));
            *pSdpError = SDP_ERROR_INVALID_CONTINUATION_STATE;
            return STATUS_INVALID_PARAMETER;
        }
    }

#if SDP_VERBOSE
    ULONG indent = 0;
    SdpPrint(SDP_DBG_SERVER_TRACE, ("Service Attrribute Request sequence:"));
    SdpWalkStream(pAttribIds, attribIdsSize, TestWalkStream, &indent);
#endif

    // if (g_Support10B) {
    //     g_MaxAttribCount = maxByteCount;
    // }

    status = pSdpDB->ServiceAttributeRequestResponseRemote(
        handle,
        pAttribIds,
        attribIdsSize,
        &pSearchResult,
        &searchSize,
        &u.Server.pOriginalAlloc,
        pSdpError
        );

    SdpPrint(SDP_DBG_SERVER_INFO, ("Attribute search returned 0x%x (%d)",
             status, (ULONG) *pSdpError)); 

    if (!NT_SUCCESS(status)) {
        return status;
    }
    else {
        FireWmiEvent(SdpServerLogTypeAttributeSearchResponse,
                     pSearchResult,
                     searchSize);
    }

    status = SdpInterface::CreateServiceAttributePDU(
        pSearchResult,
        searchSize,
        &cs,
        maxByteCount,
        outMtu,
        &pResponse,
        &responseSize,
        &u.Server.StreamSearch.pSearchResults,
        &u.Server.StreamSearch.resultsSize,
        pSdpError);

    return SendResponseToClient(status,
                                cs,
                                pPdu,
                                pBuffer,
                                pResponse,
                                responseSize,
                                SdpPDU_ServiceAttributeResponse,
                                pSdpError);
}

NTSTATUS
SdpConnection::OnServiceSearchAttributeRequest(
    SdpPDU *pPdu,
    PUCHAR pBuffer,
    PSDP_ERROR pSdpError
    )
{
    PUCHAR pServiceSearch, pAttribId;
    ULONG serviceSize, attribSize;
    USHORT maxByteCount;
    BUFFER_OFFSET offsets[3];

    // Last element needs to be zero for end marker
    RtlZeroMemory(&offsets[0], sizeof(offsets));

    ContinuationState csReq;

    NTSTATUS status = SdpInterface::CrackClientServiceSearchAttributeRequest(
        pBuffer,
        pPdu->parameterLength,
        &pServiceSearch,
        &serviceSize,
        &maxByteCount,
        &pAttribId,
        &attribSize,
        &csReq,
        pSdpError);

    if (!NT_SUCCESS(status)) {
        return status;
    }
    else {
        FireWmiEvent(SdpServerLogTypeServiceSearchAttribute,
                     pBuffer,
                     pPdu->parameterLength);
    }

    UCHAR *pSearchResult, *pResponse;
    ULONG searchSize, responseSize;

    // First buffer to compare is sequence of UUIDs being queried.
    offsets[0].Offset = 0;
    offsets[0].Length = serviceSize;

    // Second buffer to compare is the attribute contents and header.
    // We need to skip over the MaxAttributeByteCount.
    offsets[1].Offset = serviceSize + sizeof(maxByteCount);
    offsets[1].Length = attribSize;

    ContinuationState cs(this);

    BOOLEAN cleanUpReq = FALSE, contStateError = FALSE;

    if (IsRequestContinuation(&offsets[0], pPdu, pBuffer, &csReq, cleanUpReq, contStateError)) {
        SdpPrint(SDP_DBG_SERVER_INFO, ("request is a continuation"));

        status = SdpInterface::CreateServiceAttributePDU(
            u.Server.StreamSearch.pSearchResults,
            u.Server.StreamSearch.resultsSize,
            &cs,
            maxByteCount,
            outMtu,
            &pResponse,
            &responseSize,
            &u.Server.StreamSearch.pSearchResults,
            &u.Server.StreamSearch.resultsSize,
            pSdpError);

        if (NT_SUCCESS(status)) {
            SdpPrint(SDP_DBG_SERVER_INFO, ("response size is %d", responseSize));

            if (cs.size == 0) {
                SdpPrint(SDP_DBG_SERVER_INFO, ("total query finished!"));
                CleanUpServerSearchResults();
            }
            else {
                SdpPrint(SDP_DBG_SERVER_INFO, ("remaining result size is %d", u.Server.StreamSearch.resultsSize));
            }

            SendResponseToClient(pPdu,
                                 SdpPDU_ServiceSearchAttributeResponse,
                                 pResponse,
                                 responseSize);
        }
        else {
            SdpPrint(SDP_DBG_SERVER_WARNING,
                     ("continued request failed, cleaning up search"));

            CleanUpServerSearchResults();
        }

        return status;
    }
    else {
        if (cleanUpReq) {
            //
            // There is no error here.  The client sent one request, go a partial
            // response and then asked for something else
            //
            CleanUpServerSearchResults();
        }

        if (contStateError) {
            *pSdpError = SDP_ERROR_INVALID_CONTINUATION_STATE;
            return STATUS_INVALID_PARAMETER;
        }
    }

#if SDP_VERBOSE
    ULONG indent = 0;
    SdpPrint(SDP_DBG_SERVER_TRACE, ("Service Search Attrribute Request sequence\n"));
    SdpPrint(DBG_SDP_INFO, ("Service Search:\n"));
    SdpWalkStream(pServiceSearch, serviceSize, TestWalkStream, &indent);

    indent = 0;
    SdpPrint(SDP_DBG_SERVER_TRACE, ("\nAttribute Search:\n"));
    SdpWalkStream(pAttribId, attribSize, TestWalkStream, &indent);
#endif

    // if (g_Support10B) {
    //     g_MaxAttribCount = maxByteCount;
    // }

    status = pSdpDB->ServiceSearchAttributeResponseRemote(
        pServiceSearch,
        serviceSize,
        pAttribId,
        attribSize,
        &pSearchResult,
        &searchSize,
        pSdpError);

    if (!NT_SUCCESS(status)) {
        return status;
    }
    else {
        FireWmiEvent(SdpServerLogTypeServiceSearchAttributeResponse,
                     pSearchResult,
                     searchSize);
    }

    u.Server.pOriginalAlloc = pSearchResult;

    status = SdpInterface::CreateServiceAttributePDU(
        pSearchResult,
        searchSize,
        &cs,
        maxByteCount,
        outMtu,
        &pResponse,
        &responseSize,
        &u.Server.StreamSearch.pSearchResults,
        &u.Server.StreamSearch.resultsSize,
        pSdpError);

    return SendResponseToClient(status,
                                cs,
                                pPdu,
                                pBuffer,
                                pResponse,
                                responseSize,
                                SdpPDU_ServiceSearchAttributeResponse,
                                pSdpError);
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
BOOLEAN SdpConnection::Disconnect(SdpChannelState *pPreviousState)
{
    SdpChannelState previousState;
    KIRQL irql;
    BOOLEAN disconnect = FALSE;

    SdpPrint(SDP_DBG_CONFIG_TRACE, ("Connection:  Enter Disconnect (%p)\n", this));

    LockState(&irql);
    if (channelState == SdpChannelStateConnecting ||
        channelState == SdpChannelStateOpen) {

        SdpPrint(SDP_DBG_CONFIG_INFO,
                 ("Connection:  Moving from %d to SdpChannelStateClosing state\n",
                  channelState));

        previousState = channelState;
        channelState = SdpChannelStateClosing;
        disconnect = TRUE;
    }
    UnlockState(irql);

    SdpPrint(SDP_DBG_CONFIG_TRACE, ("Connection:  Exit Disconnect (%p), %d\n",
                                    this, (ULONG) disconnect));

    if (disconnect) {
        if (ARGUMENT_PRESENT(pPreviousState)) {
            *pPreviousState = previousState;
        }

        if (IsClient()) {
            //
            // Complete any pending connects
            //
            CompletePendingConnectRequests(STATUS_UNSUCCESSFUL);

            



        }
    }

    return disconnect;
}
#endif // UNDER_CE

#if SDP_VERBOSE
NTSTATUS TestWalkStream(PVOID Context, UCHAR DataType, ULONG DataSize, PUCHAR Stream)
{
    PULONG pIndent = (PULONG) Context;

    if (Stream != NULL) {
        for (ULONG j=0; j < *pIndent; j++)
            DbgPrint(" ");
    }

    CHAR int8;
    UCHAR uint8;
    SHORT int16;
    USHORT uint16;
    LONG int32;
    ULONG uint32;
    LONGLONG int64;
    ULONGLONG uint64;
    SDP_LARGE_INTEGER_16 int128;
    SDP_ULARGE_INTEGER_16 uint128;
    GUID uuid128;

    switch (DataType) {
    case SDP_TYPE_NIL:
        DbgPrint("NIL\n");
        break;

    case SDP_TYPE_BOOLEAN:
        DbgPrint("BOOLEAN (%d)\n", (ULONG) *Stream);
        break;

    case SDP_TYPE_UINT:
        DbgPrint("UINT (size %d) ", DataSize);
        switch (DataSize) {
        case sizeof(UCHAR):
            uint8 = (UCHAR) *Stream;
            DbgPrint("%d (0x%02x)\n", (ULONG) uint8, (ULONG) uint8);
            break;

        case sizeof(USHORT):
            SdpRetrieveUint16(Stream, &uint16);
            uint16 = SdpByteSwapUint16(uint16);
            DbgPrint("%hd (0x%04hx)\n", uint16, uint16);
            break;

        case sizeof(ULONG):
            SdpRetrieveUint32(Stream, &uint32);
            uint32 = SdpByteSwapUint32(uint32);
            DbgPrint("%d (0x%08x)\n", uint32, uint32);
            break;

        case sizeof(ULONGLONG):
            SdpRetrieveUint64(Stream, &uint64);
            uint64 = SdpByteSwapUint64(uint64);
            DbgPrint("%I64d (0x%016I64x)\n", uint64, uint64);
            break;

        case sizeof(uint128):
            SdpRetrieveUint128(Stream, &uint128);
            SdpByteSwapUint128(&uint128, &uint128);
            DbgPrint("high part:  %I64d (0x%016I64x), ", uint128.HighPart, uint128.HighPart);
            DbgPrint("low part:  %I64d (0x%016I64x)\n", uint128.LowPart, uint128.LowPart);
            break;
        }
        break;

    case SDP_TYPE_INT:
        DbgPrint("INT (size %d) \n", DataSize);
        switch (DataSize) {
        case sizeof(CHAR):
            int8 = (CHAR) *Stream;
            DbgPrint("%d (0x%02x)\n", (ULONG) int8, (ULONG) int8);
            break;

        case sizeof(SHORT):
            SdpRetrieveUint16(Stream, (USHORT*) &int16);
            int16 = SdpByteSwapUint16((USHORT) int16);
            DbgPrint("%hd (0x%04hx)\n", int16, int16);
            break;

        case sizeof(LONG):
            SdpRetrieveUint32(Stream, (ULONG*) &int32);
            int32 = SdpByteSwapUint32((ULONG) int32);
            DbgPrint("%d (0x%08x)\n", int32, int32);
            break;

        case sizeof(LONGLONG):
            SdpRetrieveUint64(Stream, (ULONGLONG*) &int64);
            int64 = SdpByteSwapUint64((ULONGLONG) int64);
            DbgPrint("%I64d (0x%016I64x)\n", int64, int64);
            break;

        case sizeof(int128):
            SdpRetrieveUint128(Stream, (PSDP_ULARGE_INTEGER_16) &int128);
            SdpByteSwapUint128((PSDP_ULARGE_INTEGER_16) &int128, (PSDP_ULARGE_INTEGER_16) &int128);
            DbgPrint("high part:  %I64d (0x%016I64x), ", int128.HighPart, int128.HighPart);
            DbgPrint("low part:  %I64d (0x%016I64x)\n", int128.LowPart, int128.LowPart);
            break;
        }
        break;

    case SDP_TYPE_UUID:
        DbgPrint("UUID (size %d) ", DataSize);
        switch (DataSize) {
        case sizeof(USHORT):
            SdpRetrieveUint16(Stream, &uint16);
            uint16 = SdpByteSwapUint16(uint16);
            DbgPrint("%u (0x%04x)\n", (ULONG) uint16, (ULONG) uint16);
            break;

        case sizeof(ULONG):
            SdpRetrieveUint32(Stream, &uint32);
            uint32 = SdpByteSwapUint32(uint32);
            DbgPrint("%u (0x%08x)\n", uint32, uint32);
            break;

        case sizeof(GUID):
            SdpRetrieveUuid128(Stream, &uuid128);
            SdpByteSwapUuid128(&uuid128, &uuid128);
            DbgPrint("{%08x-%04hx-%04hx-",
                   uuid128.Data1, uuid128.Data2, uuid128.Data3);
            for (int idx = 0; idx < 8; idx++) DbgPrint("%02x", uuid128.Data4[idx]);
            DbgPrint("}\n");
            break;
        }
        break;

    case SDP_TYPE_URL:
        DbgPrint("URL (%.*s)\n", DataSize, Stream);
        break;

    case SDP_TYPE_STRING:
        DbgPrint("STRING (%.*s)\n", DataSize, Stream);
        break;

    case SDP_TYPE_ALTERNATIVE:
    case SDP_TYPE_SEQUENCE:
        if (Stream) {
            if (DataType == SDP_TYPE_SEQUENCE) 
                DbgPrint("SEQUENCE\n");
            else
                DbgPrint("ALTERNATIVE\n");
            
            (*pIndent) += 4;
        }
        else {
            (*pIndent) -= 4;
        }
        break;
    }

    return 0;
}
#endif
