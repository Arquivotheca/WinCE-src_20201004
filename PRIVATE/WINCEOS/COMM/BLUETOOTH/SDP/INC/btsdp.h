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
/*++


Module Name:

   BTSDP.H

Abstract:

    This module contains the BTh SDP structures.



    Doron Holan

   Notes:

   Environment:

   Kernel mode only


Revision History:
*/

#ifndef __BTSDP_H__
#define __BTSDP_H__

///////////////////////////////////////////////////////
// typedefs
///////////////////////////////////////////////////////


///////////////////////////////////////////////////////
// Defines
//////////////////////////////////////////////////////
//
// PDU ID
//
#define SdpPDU_Error                            1
#define SdpPDU_ServiceSearchRequest             2
#define SdpPDU_ServiceSearchResponse            3
#define SdpPDU_ServiceAttributeRequest          4
#define SdpPDU_ServiceAttributeResponse         5
#define SdpPDU_ServiceSearchAttributeRequest    6
#define SdpPDU_ServiceSearchAttributeResponse   7

//
// Values used for the L2CAP configuration process 
//
#define SDP_MTU                             672 
#define SDP_QOS                             0x01 
#define SDP_FLUSH_TIMEOUT                   0xffff     // best effort

///////////////////////////////////////////////////////
//  structures
/////////////////////////////////////////////////////

#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
typedef struct _SdpServiceSearch {
    unsigned short cid;              // in
    SdpQueryUuid   *uuids;           // in
    unsigned short cUUIDs;           // in (max 12)
    unsigned short cMaxHandles;      // in
} SdpServiceSearch;

typedef struct _SdpAttributeSearch {
    unsigned short    cid;           // in
    unsigned long     recordHandle;  // in
    SdpAttributeRange *range;        // in
    unsigned long     numAttributes; // in
} SdpAttributeSearch;

typedef struct _SdpServiceAttributeSearch {
    unsigned short    cid;           // in
    SdpQueryUuid      *uuids;        // in
    unsigned short    cUUIDs;        // in
    SdpAttributeRange *range;        // in
    unsigned long     numAttributes; // in
} SdpServiceAttributeSearch;
typedef struct _BUFFER_OFFSET {
    //
    // Offset into the buffer
    //
    ULONG_PTR Offset;
    //
    // Length of memory to compare
    //
    ULONG_PTR Length;
} BUFFER_OFFSET, *PBUFFER_OFFSET;
#endif

////////////////////////////////////////////////
// Main SDP interface structure
///////////////////////////////////////////////
#include <pshpack1.h>

struct SdpPDU {
    UCHAR pduId;
    USHORT transactionId;
    USHORT parameterLength;

    PVOID Read(PVOID pBuffer);
    PUCHAR Write(PUCHAR pBuffer);
};

struct ContinuationState {
    ContinuationState() { ZERO_THIS(); }
    ContinuationState(PVOID p) { ZERO_THIS(); Init(p); }

    void Init(PVOID p);

    inline UCHAR   GetTotalSize() { return sizeof(size) + size; }

    BOOLEAN Read (UCHAR *pStream, ULONG streamSize);
    void    Write(UCHAR *pStream);

    inline ULONG   IsEqual(const ContinuationState& rhs) {
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
        return (rhs.size == size) && 
               (RtlCompareMemory(rhs.state, state, size) == size);
#else
        return (rhs.size == size) && 
               (memcmp(rhs.state, state, size) == 0);
#endif
    }

    UCHAR size;
    UCHAR state[16];
};

#include <poppack.h>

enum SdpChannelState{
    SdpChannelStateConnecting = 0,
    SdpChannelStateOpen,
    SdpChannelStateClosing,
    SdpChannelStateClosed,
};

enum ConnectionAcquireReason {
    AcquireCreate = 0x100,
    AcquireConfigInd,
    AcquireConnectInd,
    AcquireConnect,
};

#define REASON(r) ((PVOID) r)

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
struct FileObjectEntry {

    SELF_DESTRUCT

    FileObjectEntry(PFILE_OBJECT pFO) : pFileObject(pFO) { InitializeListHead(&entry); }

    LIST_ENTRY entry;
    PFILE_OBJECT pFileObject;
};
#endif

struct SdpConnection{
#if defined (UNDER_CE) || defined (WINCE_EMULATION)
	void *pCallContext;  // points to (Call *) in sdpbase.cxx, back pointer

	
    NTSTATUS ReadAsync(BOOLEAN shortOk = TRUE)   { return STATUS_SUCCESS; }
    // PVOID    ReleaseeAndIdle(PVOID tag) { ; }
	NTSTATUS SdpConnection::ReadWorkItemWorker(PUCHAR pResponse, USHORT usSize);
    ULONG    SendOrQueueRequest(void);
    unsigned long SendInitialRequestToServer(void);

	ULONG    ConnInformation;   // forges pRequestorIrp->IoStatus.Information
	PUCHAR   pClientBuf;
	ULONG    cClientBuf;
#if defined (DEBUG) || defined (_DEBUG)
    BOOL     fClientNotified;
#endif
	
#endif
    SELF_DESTRUCT

    SdpConnection(BThDevice *pDev, BOOLEAN isClient);
    ~SdpConnection();

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    //
    // Initial setup
    //
    BOOLEAN Init();
    NTSTATUS Config(USHORT mtu, USHORT *pApprovedMtu);

    BOOLEAN InMtuDone(USHORT psm);
    BOOLEAN OutMtuDone(USHORT psm);

    BOOLEAN FinalizeConnection();

    void CompletePendingConnectRequests(NTSTATUS Status);

    //
    // raw I/O wrappers
    //
    BOOLEAN SetupReadBuffer();

    NTSTATUS ReadAsync(BOOLEAN shortOk = TRUE);
    static NTSTATUS _ReadAsyncCompletion(PDEVICE_OBJECT pDeviceObject,PIRP pIrp, PVOID pContext);
    void ReadAsyncComplete();

    static void _ReadWorkItemCallback(IN PVOID Parameter);
    void ReadWorkItemWorker();
#endif    

    NTSTATUS Write(PVOID pBuffer, ULONG length);
    NTSTATUS WriteSdpError(USHORT transactionId, SDP_ERROR sdpErrorValue);

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    static NTSTATUS _TimeoutDisconnectComplete(PDEVICE_OBJECT pDeviceObject,PIRP pIrp, PVOID pContext);

    //
    // State management
    //
    void LockState(KIRQL *pIrql) { KeAcquireSpinLock(&stateSpinLock, pIrql); }
    void UnlockState(KIRQL irql) { KeReleaseSpinLock(&stateSpinLock, irql); }

    //
    // For I/O related requests
    //
    NTSTATUS Acquire(PVOID tag = NULL) { return IoAcquireRemoveLock(&remLock, tag); }
    void     Release(PVOID tag = NULL) { IoReleaseRemoveLock(&remLock, tag); }
    void     ReleaseAndWait();
    void     ReleaseAndProcessNextRequest(PIRP pIrp);

    //
    // File object maintance
    //
    BOOLEAN     AddFileObject(PFILE_OBJECT pFileObject);
    FileObjectEntry * FindFileObject(PFILE_OBJECT pFileObject);
    LONG        RemoveFileObject(PFILE_OBJECT pFileObject);
    LONG        RemoveFileObject(FileObjectEntry *pEntry);
    void        Cleanup(PFILE_OBJECT pFileObject);
#else
    void     ReleaseAndProcessNextRequest(PIRP pIrp)  { ; }
#endif

    void FireWmiEvent(SDP_SERVER_LOG_TYPE Type, PVOID Buffer, ULONG Length)	{ ;	}
    void FireWmiEvent(SDP_SERVER_LOG_TYPE Type, const BD_ADDR& Address, USHORT Mtu) { ;	}

    NTSTATUS    SendServiceSearchRequest(PIRP pIrp, SdpServiceSearch *pSearch);
    NTSTATUS    SendAttributeSearchRequest(PIRP pIrp, SdpAttributeSearch *pSearch);
    NTSTATUS    SendServiceSearchAttributeRequest(PIRP pIrp, SdpServiceAttributeSearch *pSearch);

    //
    // Send out a request to the server
    //
    NTSTATUS SendInitialRequestToServer(PIRP pClientIrp,
                                        PVOID pRequest,
                                        USHORT requestLength,
                                        UCHAR pduId);

    //
    // Ask for more from the server
    //
    NTSTATUS SendContinuationRequestToServer(ContinuationState *pContState);

    //
    // Handle what the server sent back in a response to this client
    //
    NTSTATUS OnServiceSearchResponse(SdpPDU *pPdu, PUCHAR pBuffer, ULONG_PTR* pInfo);
    NTSTATUS OnServiceAttributeResponse(SdpPDU *pPdu, PUCHAR pBuffer, ULONG_PTR* pInfo);
    NTSTATUS OnServiceSearchAttributeResponse(SdpPDU *pPdu, PUCHAR pBuffer, ULONG_PTR* pInfo);

    //
    // Handle what the client is requesting from this server
    //
    NTSTATUS OnServiceSearchRequest(SdpPDU *pPdu,
                                    PUCHAR pBuffer, 
                                    PSDP_ERROR pSdpError);
    NTSTATUS OnServiceAttributeRequest(SdpPDU *pPdu,
                                       PUCHAR pBuffer, 
                                       PSDP_ERROR pSdpError);
    NTSTATUS OnServiceSearchAttributeRequest(SdpPDU *pPdu,
                                             PUCHAR pBuffer, 
                                             PSDP_ERROR pSdpError);

    //
    // Clear out the cached query  
    //
    void FlushClientRequestCache(NTSTATUS status);

    //
    // Check to see if the current request matches the request we just cached.
    // If so, and there is ample room, complete
    NTSTATUS IsPreviousRequest(PIRP pIrp,
                               UCHAR pduId,
                               SdpQueryUuid *pUuid,
                               SdpAttributeRange *pRange,
                               ULONG numRange,
                               ULONG_PTR* pInfo);
    
    //
    // Server side 
    //
    NTSTATUS SaveQueryInfo(SdpPDU *pPdu, PVOID pQuery, PSDP_ERROR pSdpError);
    void SendResponseToClient(SdpPDU *pPdu,
                              UCHAR pduId,
                              PUCHAR pResponse,
                              ULONG responseSize);
    NTSTATUS SendResponseToClient(NTSTATUS createPduStatus,
                                  const ContinuationState& cs,
                                  SdpPDU *pPdu,
                                  PUCHAR pClientRequest,
                                  PUCHAR pResponse,
                                  ULONG responseSize,
                                  UCHAR pduId,
                                  PSDP_ERROR pSdpError);

#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    BOOL IsClient();
    BOOL IsServer();
#else
    BOOLEAN IsClient() { return client; }
    BOOLEAN IsServer() { return !client; }

    BOOLEAN Disconnect(SdpChannelState *pPreviousState = NULL);

    //
    // Lock to guard I/O  so we don't delete the object before all I/O completes
    //
    IO_REMOVE_LOCK  remLock;

    //
    // Number of creates for this connection and a list of each file object for
    // each create (to tear down connections on close if the user fails to do so)
    //
    LONG            createCount;
    CList<FileObjectEntry, FIELD_OFFSET(FileObjectEntry, entry)>  fileObjectList; 

    //
    // List of pending connect irps waiting on the real l2cap connection
    // completing.  This list is guarded by SdpConnection::LockState
    //
    CList<IRP, IRP_LE_OFFSET>  connectList;

    //
    // Link into the SdpInterface list of connections
    //
    LIST_ENTRY      entry;   

    //
    // Link into the timer list of connections
    //
    LIST_ENTRY      timerEntry;
    LONG   timeoutTime, maxTimeout;

    //
    // Spinlock to guard the state of the object and associated state variables
    //
    KSPIN_LOCK      stateSpinLock;
    SdpChannelState channelState;
    ULONG           configRetry;
    BOOLEAN         configInDone, configOutDone, client, closing;

    //
    // We one read and write request which are sent down to the bus so we do not
    // have to worry about allocating them every time.  We also have a read buffer
    // preallocated for the same reason.
    //
    // Every time a read completes, queue a work item and process the results at
    // PASSIVE
    //
    PIRP  pWriteIrp, pReadIrp;
    PBRB  pReadBrb;
    PVOID pReadBuffer;

    WORK_QUEUE_ITEM readWorkItem;

    //
    // BTHPORT provided value, used for BRBs
    //
    PVOID           hConnection;

    //
    // Address of the device we are talking to
    //
    BD_ADDR         btAddress;

    //
    // The BTHPORT object for this radio
    BThDevice *pDevice;

    //
    // The client's request in IRP form.   It does not have a cancel routine set
    // even though it has been pended.  I do this b/c if I complete the cancelled
    // irp before I receive a server response, the client will not know when he
    // can send another request.
    //
    PIRP pRequestorIrp;
#endif

    //
    // The next transactionId we will use
    //
    USHORT transactionId;

    USHORT          inMtu, outMtu;


#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    //
    // The destructor for this object is behaving oddly, removing for now
    //
    CancelableQue clientQueue;
    static VOID _QueueCancelRoutine(PVOID Context, PIRP Irp);
#endif

    union {
        struct {
            //
            // Entire request sent to the server, including the PDU.  It saved
            // off in case we multiple PDUs are required for the entire response.
            //
            PVOID pRequest;

            //
            // Size of all the parameters in the reuqest except for the 
            // continuation state
            //
            USHORT rawParametersLength;

            UCHAR lastPduRequest;

            //
            // Last client request.  If the response was too large for the 
            // requestor's buffer, we will save off the result and if the 
            // requestor sends down the same request w/the appropriately sized
            // buffer, the response will be sent back up.  For service searches,
            // we do not store off any results b/c we send the server the max amount
            // of data we can handle
            //
            SdpQueryUuid lastUuid[MAX_UUIDS_IN_QUERY];
            SdpAttributeRange *pLastRange;  
            ULONG numRange;

            struct {
                //
                // Total number of records in the response
                //
                USHORT totalRecordCount;
            } ServiceSearch;

            struct {
                //
                // The response from either an attribute of service + attribute
                // search
                //
                PUCHAR pResponse;
                ULONG responseLength;
            } AttributeSearch;
        } Client;

        struct {
            //
            // The original request sent by the client.  We store this off so that
            // when we receive another request w/a continuation state, we can match
            // up the requests
            //
            PVOID pClientRequest;
            ULONG clientRequestLength;     
            UCHAR lastPduRequest;

            //
            // Some search responses have a response pointer plus a pointer which
            // should be free (and not the response), but for simplicity, always put
            // the pointer that needs to be freed here.
            //
            PVOID pOriginalAlloc;

            //
            // Cached search results
            //
            union {
                struct {
                    PUCHAR pSearchResults;
                    ULONG resultsSize;
                } StreamSearch;

                struct {
                    PULONG pRecordsNext;
                    USHORT recordCount;
                    USHORT totalRecordCount;
                } ServiceSearch;
            };
        } Server;
    } u;

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
protected:

    SdpChannelState GetChannelState()
    {
        KIRQL irql;
        LockState(&irql);
        SdpChannelState state = channelState;
        UnlockState(irql);
        return state;
    }
#endif
    void CompleteClientRequest(ULONG_PTR info, NTSTATUS status);
    void CleanUpServerSearchResults();

    BOOLEAN IsRequestContinuation(
        PBUFFER_OFFSET Offsets,
        SdpPDU *pPdu,
        PUCHAR pRequest, 
        ContinuationState *pConStateReq,
        BOOLEAN &cleanUpRequired,
        BOOLEAN &contError);

    BOOLEAN SdpConnection::DoRequestsMatch(
        PBUFFER_OFFSET Offsets,
        PUCHAR ContinuedRequest);

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    NTSTATUS SendBrb(PBRB pBrb, PIRP pIrp = NULL);

    NTSTATUS SendBrbAsync(
        PBRB pBrb, 
        PIO_COMPLETION_ROUTINE Routine,
        PIRP pIrp = NULL,
        PVOID pContext = NULL,
        PVOID pArg1 = NULL,
        PVOID pArg2 = NULL,
        PVOID pArg3 = NULL,
        PVOID pArg4 = NULL);

    //
    // Cannot have a global struct with a constructor in a kernel mode driver so
    // CList< ... > is out of the question.  Do it the old fashioned way.
    //
    static LIST_ENTRY _TimeoutListHead;
    static KSPIN_LOCK _TimeoutListLock;
    static KTIMER _TimeoutTimer;
    static KDPC _TimeoutTimerDpc;

    static BOOLEAN _RegisterConnectionOnTimer(SdpConnection *, ULONG maxTimeout);
    static void _TimeoutTimerDpcProc(IN PKDPC Dpc, IN PVOID DeviceObject, IN PVOID Context1, IN PVOID Context2);
#endif        
};

#define SetMaxUuidsInIrp(_irp, _maxuuids) (_irp)->Tail.Overlay.DriverContext[SDP_IRP_STORAGE_UUID_MAX] = UlongToPtr(_maxuuids)
#define SetMaxRangeInIrp(_irp, _maxrange) (_irp)->Tail.Overlay.DriverContext[SDP_IRP_STORAGE_RANGE_MAX] = UlongToPtr(_maxrange)

#define GetMaxUuidsFromIrp(_irp) (UCHAR) PtrToUlong((_irp)->Tail.Overlay.DriverContext[SDP_IRP_STORAGE_UUID_MAX])
#define GetMaxRangeFromIrp(_irp) PtrToUlong((_irp)->Tail.Overlay.DriverContext[SDP_IRP_STORAGE_RANGE_MAX])


class SdpInterface {
public:
    friend SdpConnection;

    SELF_DESTRUCT

    SdpInterface();
    ~SdpInterface();

#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
	static NTSTATUS ServiceSearch(SdpConnection *pCon, SdpServiceSearch *pSearch, BOOL fLocal);
	static NTSTATUS AttributeSearch(SdpConnection *pCon, SdpAttributeSearch *pSearch, BOOL fLocal);
    static NTSTATUS ServiceAndAttributeSearch(SdpConnection *pCon, SdpServiceAttributeSearch *pSearch, BOOL fLocal);

#endif // UNDER_CE

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    NTSTATUS Start(BThDevice *pBthDevice);
    NTSTATUS Stop();

    //
    // BThPort DDI interface callbacks and associated member functions
    //
    static NTSTATUS Sdp_IndicationCallback(
        IN PVOID ContextIF,
        IN IndicationCode Indication,
        IN IndicationParameters * Parameters);

    NTSTATUS RemoteConnectCallback(IndicationParameters *Parameters);
    NTSTATUS EnterConfigStateCallback(IndicationParameters *Parameters,
                                      SdpConnection *pConnection);
    NTSTATUS RemoteConfigCallback(IndicationParameters *Parameters,
                                  SdpConnection *pConnection);
    NTSTATUS ConfigResponseCallback(IndicationParameters *Parameters);
    NTSTATUS EnterOpenStateCallback(IndicationParameters *Parameters,
                                    SdpConnection *pConnection);
    NTSTATUS RemoteDisconnectCallback(IndicationParameters *Parameters,
                                      SdpConnection *pConnection);
    NTSTATUS FinalCleanupCallback(IndicationParameters *Parameters,
                                  SdpConnection *pConnection);

    NTSTATUS SendConfigRequest(PIRP Irp,
                               BRB* Brb,
                               SdpConnection *Con,
                               USHORT Mtu
                               );

    void SendDisconnect(SdpConnection *pCon, PIRP pIrp, PBRB pBrb);

    static NTSTATUS ConnectRequestComplete(PDEVICE_OBJECT DeviceObject,
                                       PIRP Irp,
                                       PVOID Context);

    static NTSTATUS ConfigRequestComplete(PDEVICE_OBJECT DeviceObject,
                                          PIRP Irp,
                                          PVOID Context);

    static NTSTATUS DisconnectRequestComplete(PDEVICE_OBJECT DeviceObject,
                                              PIRP Irp,
                                              PVOID Context);



    //
    // Close cleanup
    //
    void CloseConnections(PIRP pIrp);

    //
    // IOCTL handlers
    //                              
    NTSTATUS Connect(PIRP pIrp, ULONG_PTR& info);
    NTSTATUS ServiceSearch(PIRP pIrp, ULONG_PTR& info);
    NTSTATUS AttributeSearch(PIRP pIrp, ULONG_PTR& info);
    NTSTATUS ServiceAndAttributeSearch(PIRP pIrp, ULONG_PTR& info);
    NTSTATUS Disconnect(PIRP pIrp, ULONG_PTR& info);

#endif

    //
    // PDU request crackers
    //
    static NTSTATUS CrackClientServiceSearchRequest(
        IN UCHAR *pServiceSearchRequestStream,
        IN ULONG streamSize,
        OUT UCHAR **ppSequenceStream,
        OUT ULONG *pSequenceStreamSize,
        OUT USHORT *pMaxRecordCount,
        OUT ContinuationState *pContState,
        OUT PSDP_ERROR pSdpError);

    static NTSTATUS CrackClientServiceAttributeRequest(
        IN UCHAR *pServiceAttributeRequestStream,
        IN ULONG streamSize,
        OUT ULONG *pRecordHandle,
        OUT USHORT *pMaxAttribByteCount,
        OUT UCHAR **ppAttribIdListStream,
        OUT ULONG *pAttribIdListStreamSize,
        OUT ContinuationState *pContState,
        OUT PSDP_ERROR pSdpError);

    static NTSTATUS CrackClientServiceSearchAttributeRequest(
        IN UCHAR *pServiceSearchAttributeRequestStream,
        IN ULONG streamSize,
        OUT UCHAR **ppServiceSearchSequence,
        OUT ULONG *pServiceSearchSequenceSize,
        OUT USHORT *pMaxAttributeByteCount,
        OUT UCHAR **ppAttributeIdSequence,
        OUT ULONG *pAttributeIdSequenceSize,
        OUT ContinuationState *pContState,
        OUT PSDP_ERROR pSdpError);

    //
    // PDU response crackers
    //
    static NTSTATUS CrackServerServiceSearchResponse(
        IN UCHAR *pResponseStream,
        IN ULONG streamSize,
        OUT USHORT *pTotalCount,
        OUT USHORT *pCurrentCount,
        OUT ULONG **ppHandleList,
        OUT ContinuationState *pContState,
        OUT PSDP_ERROR pSdpError);

    static NTSTATUS CrackServerServiceAttributeResponse(
        IN UCHAR *pResponseStream,
        IN ULONG streamSize,
        OUT USHORT *pByteCount,
        OUT UCHAR **ppAttributeList,
        OUT ContinuationState *pContState,
        OUT PSDP_ERROR pSdpError);

    //
    // PDU creators
    //
    static NTSTATUS CreateServiceSearchResponsePDU(
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
        );

    static NTSTATUS CreateServiceAttributePDU(
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
        );

    static NTSTATUS CreateServiceSearchAttributePDU(
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
        );


    static PSDP_NODE CreateAttributeSearchTree(SdpAttributeRange *pRange, ULONG maxRange);
    static PSDP_NODE CreateServiceSearchTree(SdpQueryUuid *pUuid, UCHAR maxUuids);

protected:
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    NTSTATUS SendBrb(PBRB pBrb);

    NTSTATUS SendBrbAsync(
        PIRP pIrp,
        PBRB pBrb, 
        PIO_COMPLETION_ROUTINE Routine,
        PVOID pContext = NULL,
        PVOID pArg1 = NULL,
        PVOID pArg2 = NULL,
        PVOID pArg3 = NULL,
        PVOID pArg4 = NULL);

    NTSTATUS SendConfigRequest(PIRP pIrp,
                               BRB* pBrb,
                               SdpConnection *Con,
                               USHORT Mtu,
                               PIO_COMPLETION_ROUTINE Routine = L2CA_ConnectInd_ConfigReqComplete
                               );

    void SendDisconnect(SdpConnection *pCon,
                        PIRP pIrp,
                        PBRB pBrb,
                        PIO_COMPLETION_ROUTINE pRoutine,
                        PVOID Tag
                        );


    //
    // Naming scheme for these completion routines:
    // {Indication in which the request was sent from}{type of BRB sent}Complete
    //
    // For instance, L2CA_ConnectInd_ConnectResponseComplete means:
    //
    // the brb was sent async while processing L2CA_ConnectInd
    // the brb type that is completing is a ConnectionResponse
    //

    static NTSTATUS L2CA_ConnectInd_ConnectResponseComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
    static NTSTATUS L2CA_ConnectInd_ConfigReqComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
    static NTSTATUS L2CA_ConnectInd_DisconnectComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

    static NTSTATUS L2CA_ConfigInd_ConfigComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
    static NTSTATUS L2CA_ConfigInd_DisconnectComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

    static NTSTATUS L2CA_DisconnectInd_DisconnectRspComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

    static NTSTATUS ConnectRequest_ConfigReqComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
    static NTSTATUS ConnectRequest_DisconnectComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

    BOOLEAN RemoveConnectionAtDpc(SdpConnection *pCon,
                                  BOOLEAN SendDisconnectRsp = FALSE,
                                  PVOID FinalTag = REASON(AcquireConnectInd));

    static void RemoveConnectionWorker(PDEVICE_OBJECT DeviceObject, PVOID Context);

    SdpConnection * FindConnection(PVOID hConnection);
    SdpConnection * FindConnection(const bt_addr& address, BOOLEAN onlyServer = FALSE);
    NTSTATUS AcquireConnection(PVOID hConnection,
                               SdpConnection** ppCon,
                               PVOID tag = NULL);
    NTSTATUS AcquireConnection(const bt_addr& btAddress,
                               SdpConnection** ppCon,
                               PVOID tag = NULL);
    NTSTATUS AcquireConnectionInternal(SdpConnection *pCon, PVOID tag);
#endif

    static NTSTATUS VerifyServiceSearch(SdpQueryUuid *pUuids, UCHAR *pMaxUuids);
    static NTSTATUS VerifyAttributeSearch(SdpAttributeRange *pRange, ULONG *pMaxRange, ULONG_PTR bufferLength);

    void FireWmiEvent(SDP_SERVER_LOG_TYPE Type, BD_ADDR *pAddress = NULL, USHORT Mtu = 0);

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))   
    BThDevice *m_pDevice;

    CList<SdpConnection, FIELD_OFFSET(SdpConnection, entry), CSemaphore> m_SdpConnectionList;
#endif
};

NTSTATUS VerifyProtocolList(PUCHAR pStream, ULONG size, ULONG flags);

#endif // __BTSDP_H__
