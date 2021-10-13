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

   SdpDB.H

Abstract:

    This module contains the BTh SDP database structures.



   Doron J. Holan 

   Notes:

   Environment:

   Kernel mode only


Revision History:
*/

#ifndef  __SDPDB_H__
#define  __SDPDB_H__


struct HandleEntry {

    SELF_DESTRUCT

    HandleEntry(ULONG h) : handle(h) { InitializeListHead(&link); }

    LIST_ENTRY link;
    ULONG handle;
};

typedef CList<HandleEntry, FIELD_OFFSET(HandleEntry, link)> HandleList;

struct SdpAttribSearch;
struct SdpUuidSearch;

class SdpDatabase {
public:

    SELF_DESTRUCT

    SdpDatabase();
    ~SdpDatabase();

    //
    // pool tag for this class
    //
    static const ULONG SDP_TAG;

    ////////////////////////////////////////////////////////////////////////////
    // Setup and management 
    ////////////////////////////////////////////////////////////////////////////

    //
    // Initialize the object.  This must be the first method called on this
    // object after instantiation
    //
    NTSTATUS Init();

    //
    // Query Interface handler (add or remove various direct call items)
    //
    NTSTATUS HandleQI(PIRP Irp);

    void AddDevice(PDEVICE_OBJECT DeviceObject);
    void RemoveDevice(PDEVICE_OBJECT DeviceObject);

    ////////////////////////////////////////////////////////////////////////////
    // Local client functions
    ////////////////////////////////////////////////////////////////////////////

    NTSTATUS AddProtMux(GUID *protocol);
    NTSTATUS RemoveProtMux(GUID *protocol);
    BOOLEAN  IsProtocolMuxed(const GUID &protocol);

    //
    // return values for GetProtocol and GetServiceClass
    // STATUS_SUCCESS - successfully found
    // STATUS_PENDING - PSM is multiplexed, multiplexer must requery
    // STATUS_NOT_FOUND - no record of the PSM in the service record list
    //
    NTSTATUS GetPsmInfo(USHORT psm, GUID *protocol, ULONG *serviceHandle);
    NTSTATUS GetServiceClass(USHORT psm, GUID *serviceClass);

    //
    // Makes sure that the stream is well formed.  It does NOT syntax check the
    // stream for any kind of element arrangement
    //
    static NTSTATUS ValidateStream(UCHAR *pStream, ULONG streamSize, PULONG_PTR err);

    //
    // Adds a new record to the database, if all the verification tests pass
    //
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    NTSTATUS AddRecord(PIRP pIrp, PHANDLE pHandle);
#else
protected:
    struct ServiceRecord {
        SELF_DESTRUCT

        ServiceRecord();
        ~ServiceRecord();
    
        LIST_ENTRY  link;
//      ULONG       fService;       // COD service bits associated with this record
//      ULONG       fOptions;       // SERVICE_OPTIONS_XXX
        HANDLE      handle;         // handle for the creator
        ULONG       recordHandle;   // the SDP attribute handle value, for searches
        ULONG       streamSize;
        PUCHAR      pStream;
        GUID        serviceClass;
        PSM_LIST    psmList;

        HANDLE      hCallerProc;     // Process that created this record, used when deleting the record on shutdown.
    };

public:
    NTSTATUS AddRecord(PUCHAR pStream, ULONG streamSize, PHANDLE pHandle, ServiceRecord *pRecord=NULL);
    void RemoveRecordsAssociatedWithProcess(HANDLE hDyingProc);

#endif

    //
    // Can update or remove (pStream == NULL) a record from the database
    //
    NTSTATUS UpdateRecord(UCHAR *pStream, ULONG streamSize, HANDLE handle);

    NTSTATUS RemoveRecord(HANDLE handle, ULONG *pfService);

    //
    // Whenever a record is added or removed, alter the database state attribute 
    // in the SDP record representing this SDP server
    //
    void AlterDatabaseState();

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    //
    // Need cleanup based on PDOs when they go away ??
    //
    void Cleanup(PFILE_OBJECT pFileObject);
#endif

    
    ////////////////////////////////////////////////////////////////////////////
    // Remote SDP query handlers 
    ////////////////////////////////////////////////////////////////////////////
    NTSTATUS ServiceSearchRequestResponseLocal(
        SdpQueryUuid *pUuid,
        UCHAR numUuid,
        IN USHORT maxRecordsRequested,
        OUT ULONG **ppResultStream,         // must be freed when xmit is finished
        OUT USHORT *pRecordCount);

    NTSTATUS ServiceSearchRequestResponseRemote(
        IN UCHAR *pStream,
        IN ULONG streamSize,
        IN USHORT maxRecordsRequested,
        OUT ULONG **ppResultStream,         // must be freed when xmit is finished
        OUT USHORT *pRecordCount,
        OUT PSDP_ERROR pSdpError);

    NTSTATUS ServiceSearchRequestResponseCached(
        HANDLE hDeviceCache,
        SdpQueryUuid *pUuid,
        UCHAR numUUid,
        IN USHORT maxRecordsRequested,
        OUT ULONG **ppResultStream,
        OUT USHORT *pRecordCount);

    NTSTATUS ServiceSearchRequestResponse(
        IN SdpUuidSearch *pUuidSearch,
        IN USHORT maxRecordsRequested,
        OUT ULONG **ppResultStream,         // must be freed when xmit is finished
        OUT USHORT *pRecordCount,
        OUT PSDP_ERROR pSdpError,
        IN BOOLEAN local);

    NTSTATUS ServiceAttributeRequestResponseRemote(
        IN ULONG serviceHandle,
        IN UCHAR *pAttribIdListStream,
        IN ULONG streamSize,
        OUT UCHAR **ppAttributeListStream,      // free *ppOriginalAlloc, not this
        OUT ULONG *pAttributeListSize,
        OUT PVOID * ppOriginalAlloc,            // must be freed when xmit is finished
        OUT PSDP_ERROR pSdpError);

    NTSTATUS ServiceAttributeRequestResponseLocal(
        IN ULONG serviceHandle,
        IN SdpAttributeRange *pRange,
        IN ULONG numRange,
        OUT UCHAR **ppAttributeListStream,      // free *ppOriginalAlloc, not this
        OUT ULONG *pAttributeListSize,
        OUT PVOID * ppOriginalAlloc);            // must be freed when xmit is finished

    NTSTATUS ServiceAttributeRequestResponse(
        IN ULONG serviceHandle,
        IN SdpAttribSearch *pAttribSearch,
        OUT UCHAR **ppAttributeListStream,      // free *ppOriginalAlloc, not this
        OUT ULONG *pAttributeListSize,
        OUT PVOID * ppOriginalAlloc,            // must be freed when xmit is finished
        OUT PSDP_ERROR pSdpError);

    NTSTATUS ServiceSearchAttributeResponseRemote(
        IN UCHAR *pServiceSearchStream,
        IN ULONG serviceSearchStreamSize,
        IN UCHAR *pAttributeIdStream,
        IN ULONG attributeIdStreamSize,
        OUT UCHAR **ppAttributeLists,           // must be freed when xmit is finished
        OUT ULONG *pAttributeListsByteCount,
        OUT PSDP_ERROR pSdpError);

    NTSTATUS ServiceSearchAttributeResponseLocal(
        IN SdpQueryUuid *pUuid,
        IN UCHAR numUuid,
        IN SdpAttributeRange *pRange,
        IN ULONG numRange,
        OUT UCHAR **ppAttributeLists,           // must be freed when xmit is finished
        OUT ULONG *pAttributeListsByteCount);

    NTSTATUS ServiceSearchAttributeResponse(
        IN SdpUuidSearch *pUuidSearch,
        IN SdpAttribSearch *pAttribSearch,
        OUT UCHAR **ppAttributeLists,           // must be freed when xmit is finished
        OUT ULONG *pAttributeListsByteCount,
        OUT PSDP_ERROR pSdpError,
        IN BOOLEAN Local);

protected:
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    struct ServiceRecord {

        SELF_DESTRUCT

        ServiceRecord();
        ~ServiceRecord();
    
        LIST_ENTRY  link;
        ULONG       fService;       // COD service bits associated with this record
        ULONG       fOptions;       // SERVICE_OPTIONS_XXX
        HANDLE      handle;         // handle for the creator
        ULONG       recordHandle;   // the SDP attribute handle value, for searches
        ULONG       streamSize;
        PUCHAR      pStream;
        PFILE_OBJECT pFileObject;
        GUID        serviceClass;
        PSM_LIST    psmList;
    };

    struct RecordValidator {

        SELF_DESTRUCT

        RecordValidator(PBTHDDI_SDP_VALIDATE_INTERFACE pValInterface);
        
        LIST_ENTRY link;

        PSDP_RECORD_VALIDATOR pValidator;
        PSDP_REMOTE_RECORD_VALIDATOR pRemoteValidator;
        PSDP_UPDATE_RECORD_VALIDATOR pUpdateValidator;

        PVOID pContext;

        ULONG flags;
    };
#endif // UNDER_CE    

    struct ProtMux {

        SELF_DESTRUCT

        ProtMux(GUID *prot);

        LIST_ENTRY link;
        GUID protocol;
    };

    struct DeviceLE {
        SELF_DESTRUCT

        DeviceLE(PDEVICE_OBJECT pDev) : device(pDev) { InitializeListHead(&link); }

        LIST_ENTRY link;
        PDEVICE_OBJECT device;
    };

    typedef CList<ServiceRecord, FIELD_OFFSET(ServiceRecord, link), CSemaphore> ServiceRecordList;  //
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    typedef CList<RecordValidator, FIELD_OFFSET(RecordValidator, link), CSemaphore> RecordValidatorList;  //
#endif
    typedef CList<ProtMux, FIELD_OFFSET(ProtMux, link)> ProtMuxList;
    typedef CList<DeviceLE, FIELD_OFFSET(DeviceLE, link), CSemaphore> DeviceList;  //

    ////////////////////////////////////////////////////////////////////////////
    // Local service record management (internal)
    ////////////////////////////////////////////////////////////////////////////
    ServiceRecordList m_ServiceRecordList;
    ServiceRecord *m_pSdpRecord;
    ProtMuxList m_ProtMuxList;
    LONG m_nextRecordHandle;

    // Assumes that the services list is locked
    ServiceRecord* GetServiceRecord(HANDLE handle);
    ULONG GetNextRecordHandle();

    ////////////////////////////////////////////////////////////////////////////
    // LIst of outside drivers that want to validate records
    ////////////////////////////////////////////////////////////////////////////
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    RecordValidatorList m_RecordValidatorList;

    NTSTATUS ValidateRecordExternal(PUCHAR pStream,
                                    ULONG streamSize,
                                    PSDP_TREE_ROOT_NODE pTreeRoot,
                                    PPSM_LIST pPsmList,
                                    ULONG flags,
                                    GUID* pServiceClassUuid);

    NTSTATUS ValidateRecordExternalUpdate(
        PRECORD_VALIDATOR_INFO pOldInfo,
        PRECORD_VALIDATOR_INFO pNewInfo);

    KSEMAPHORE m_dbStateSemaphore;

    DeviceList m_DeviceList;
#endif // UNDER_CE

    ULONG m_dbState;

    void FireWmiEvent(HANDLE recordHandle, SDP_DATABASE_EVENT_TYPE type); 
};

#endif //  __SDPDB_H__
