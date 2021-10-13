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

   SdpDB.CPP

Abstract:

    This module implemnts BTh SDP core spec.



   Doron J. Holan

   Notes:

   Environment:

   Kernel mode only


Revision History:
*/

#include "common.h"

#if defined (UNDER_CE)
#include <bt_os.h>
#endif






///////////////////////////////////////////////////////////////////////////
// INTERNAL Defines
///////////////////////////////////////////////////////////////////////////

const ULONG SdpDatabase::SDP_TAG = 'CpdS';
const ULONG PM_TAG = 'xMtP';
const ULONG RV_TAG = 'VceR';

#if SDP_VERBOSE
ULONG SDP_DBG_FLAGS = DEFAULT_SDP_DBG_FLAGS;
#endif // SDP_VERBOSE

//
// Service record stream whose form is (Attrib ID, AttibValue)
// SDP_ATTRIB_RECORD_HANDLE
//      UINT32 0x0
//
// SDP_ATTRIB_CLASS_ID_LIST,
//      SEQ 
//          UUID16 ServiceDiscoveryServerServiceClassID_UUID16
//
// SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST
//      SEQ 
//          SEQ
//              UUID16 L2CAP_PROTOCOL_UUID16
//              UINT 16 0x00001
//          SEQ
//              UUID16 SDP_PROTOCOL_UUID16
//
// (SDP_ATTRIB_BROWSE_GROUP_LIST has been removed for WinCE)
// SDP_ATTRIB_BROWSE_GROUP_LIST
//  SEQ
//      UUID16 PublicBrowseGroupServiceClassID_UUID16 
//
// SDP_ATTRIB_LANG_BASE_ATTIB_ID_LIST
//  SEQ
//      UINT16      0x656e      (language, english "en")
//      UINT16      0x0061      (encoding, utf-8)
//      UINT16      0x0100      (offset)
//
// 0x100 + STRING_NAME_OFFSET
//  STRING "Service Discovery"
//
// 0x100 + STRING_DESCRIPTION_OFFSET
//  STRING "Publishes services to remote devices"
//
// 0x100 + STIRNG_PROVIDER_NAME_OFFSET
//  STRING "Microsoft"
//
// SDP_ATTRIB_PRIV_VERSION_NUMBER_LIST
//  SEQ
//      UINT16 0x0100
//
// SDP_ATTRIB_SDP_DATABASE_STATE
//      UINT32 0x00 (this is only an initial value and changes every time a 
//                   record is added or removed)
// 

#if defined (UNDER_CE)
// WinCE doesn't include SDP_ATTRIB_BROWSE_GROUP_LIST or PublicBrowseGroupServiceClassID_UUID16.
const UCHAR g_ServiceStream[] = {
    0x35, 0x90, 0x09, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00,
    0x01, 0x35, 0x03, 0x19, 0x10, 0x00, 0x09, 0x00, 0x04, 0x35, 0x0d, 0x35,
    0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x00, 0x01,
    0x09, 0x00, 0x06, 0x35,
    0x09, 0x09, 0x65, 0x6e, 0x09, 0x00, 0x6a, 0x09, 0x01, 0x00, 0x09, 0x01,
    0x00, 0x25, 0x12, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x20, 0x44,
    0x69, 0x73, 0x63, 0x6f, 0x76, 0x65, 0x72, 0x79, 0x00, 0x09, 0x01, 0x01,
    0x25, 0x25, 0x50, 0x75, 0x62, 0x6c, 0x69, 0x73, 0x68, 0x65, 0x73, 0x20,
    0x73, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x73, 0x20, 0x74, 0x6f, 0x20,
    0x72, 0x65, 0x6d, 0x6f, 0x74, 0x65, 0x20, 0x64, 0x65, 0x76, 0x69, 0x63,
    0x65, 0x73, 0x00, 0x09, 0x01, 0x02, 0x25, 0x0a, 0x4d, 0x69, 0x63, 0x72,
    0x6f, 0x73, 0x6f, 0x66, 0x74, 0x00, 0x09, 0x02, 0x00, 0x35, 0x03, 0x09,
    0x01, 0x00, 0x09, 0x02, 0x01, 0x0a, 0x00, 0x00, 0x00, 0x00,
};
#else
const UCHAR g_ServiceStream[] = {
    0x35, 0x98, 0x09, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00,
    0x01, 0x35, 0x03, 0x19, 0x10, 0x00, 0x09, 0x00, 0x04, 0x35, 0x0d, 0x35,
    0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x00, 0x01,
    0x09, 0x00, 0x05, 0x35, 0x03, 0x19, 0x10, 0x02, 0x09, 0x00, 0x06, 0x35,
    0x09, 0x09, 0x65, 0x6e, 0x09, 0x00, 0x6a, 0x09, 0x01, 0x00, 0x09, 0x01,
    0x00, 0x25, 0x12, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x20, 0x44,
    0x69, 0x73, 0x63, 0x6f, 0x76, 0x65, 0x72, 0x79, 0x00, 0x09, 0x01, 0x01,
    0x25, 0x25, 0x50, 0x75, 0x62, 0x6c, 0x69, 0x73, 0x68, 0x65, 0x73, 0x20,
    0x73, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x73, 0x20, 0x74, 0x6f, 0x20,
    0x72, 0x65, 0x6d, 0x6f, 0x74, 0x65, 0x20, 0x64, 0x65, 0x76, 0x69, 0x63,
    0x65, 0x73, 0x00, 0x09, 0x01, 0x02, 0x25, 0x0a, 0x4d, 0x69, 0x63, 0x72,
    0x6f, 0x73, 0x6f, 0x66, 0x74, 0x00, 0x09, 0x02, 0x00, 0x35, 0x03, 0x09,
    0x01, 0x00, 0x09, 0x02, 0x01, 0x0a, 0x00, 0x00, 0x00, 0x00,
};
#endif // UNDER_CE

ULONG g_ServiceStreamSize = sizeof(g_ServiceStream)/sizeof(g_ServiceStream[0]); 

#define INVALID_RECORD_HANDLE  1
#define INVALID_ID             ((PVOID) 1)
#define INIT_HANDLE_VALUE      0xFFFF

SdpDatabase::ServiceRecord::ServiceRecord() : handle(INVALID_ID), streamSize(0),
    pStream(NULL), recordHandle(INVALID_RECORD_HANDLE)
{
    InitializeListHead(&link);
    RtlZeroMemory(&psmList, sizeof(psmList));

#if defined (UNDER_CE)
    hCallerProc = GetCallerProcess();
#endif
}



SdpDatabase::ServiceRecord::~ServiceRecord()
{
    if (pStream) {
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
        delete[] pStream;
#else
        SdpFreePool(pStream);
#endif
    }
    pStream = NULL;
}

SdpDatabase::ServiceRecord * SdpDatabase::GetServiceRecord(HANDLE handle)
{
    ServiceRecord *pRecord;
//  PLIST_ENTRY entry;

	for (pRecord = m_ServiceRecordList.GetHead();
         pRecord != NULL;
         pRecord = m_ServiceRecordList.GetNext(pRecord)) {

	    if (pRecord->handle == handle) {
		    return pRecord; 
        }
    }

	return NULL;
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
SdpDatabase::RecordValidator::RecordValidator(PBTHDDI_SDP_VALIDATE_INTERFACE pValInterface) 
{
    flags = pValInterface->Flags;

    pValidator = pValInterface->Validator;
    pRemoteValidator = pValInterface->RemoteValidator;
    pUpdateValidator = pValInterface->UpdateValidator;

    pContext = pValInterface->ValidatorContext;

    pValInterface->Cookie = (PVOID) this;

    InitializeListHead(&link);
}
#endif // UNDER_CE

SdpDatabase::ProtMux::ProtMux(GUID *prot) : protocol(*prot)
{
    InitializeListHead(&link);
}


SdpDatabase::SdpDatabase() :  m_nextRecordHandle(INIT_HANDLE_VALUE)
{
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    KeInitializeSemaphore(&m_dbStateSemaphore, 1, 1);
#else
    #define INITIAL_DB_STATE 0
    m_dbState = INITIAL_DB_STATE;
#endif
}

SdpDatabase::~SdpDatabase()
{
    while (!m_ServiceRecordList.IsEmpty()) {
        ServiceRecord *pRecord = m_ServiceRecordList.RemoveHead();
        delete pRecord;
    }

    while (!m_ProtMuxList.IsEmpty()) {
        ProtMux *ppm = m_ProtMuxList.RemoveHead();
        delete ppm;
    }
}

NTSTATUS SdpDatabase::Init()
{
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    NTSTATUS status;
#endif

    




#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    m_pSdpRecord = new (NonPagedPool, SDP_TAG) ServiceRecord; 
#else
    m_pSdpRecord = new ServiceRecord;
#endif
    if (m_pSdpRecord == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    m_pSdpRecord->pStream  = new (NonPagedPool, SDP_TAG) UCHAR[g_ServiceStreamSize];
#else
    m_pSdpRecord->pStream  = (UCHAR*) SdpAllocatePool(g_ServiceStreamSize * sizeof(UCHAR));
    m_pSdpRecord->handle   = 0;
#endif
    if (m_pSdpRecord->pStream == NULL) {
        delete m_pSdpRecord;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(&m_pSdpRecord->serviceClass,
                  &ServiceDiscoveryServerServiceClassID_UUID,
                  sizeof(GUID));

    m_pSdpRecord->psmList.count = 1;
    m_pSdpRecord->psmList.list[0].psm = PSM_SDP;
    RtlCopyMemory(&m_pSdpRecord->psmList.list[0].protocol,
                  &SDP_PROTOCOL_UUID,
                  sizeof(GUID));

    m_pSdpRecord->streamSize = g_ServiceStreamSize;
    RtlCopyMemory(m_pSdpRecord->pStream, g_ServiceStream, g_ServiceStreamSize);
    m_pSdpRecord->recordHandle = 0x0;
    
    m_ServiceRecordList.AddHead(m_pSdpRecord);

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    //
    // Add the service to the security database, we do not want to prompt the
    // user when a remote device connects
    //
    status = Globals.pSecDB->AddService(ServiceDiscoveryServerServiceClassID_UUID,
                                        SERVICE_SECURITY_NONE);
#endif // UNDER_CE


    


    AddProtMux((LPGUID) &RFCOMM_PROTOCOL_UUID);

    return STATUS_SUCCESS;
}

NTSTATUS SdpDatabase::ValidateStream(UCHAR *pStream, ULONG streamSize, PULONG_PTR err)
{
    return ::ValidateStream(pStream, streamSize, NULL, NULL, err);
}


#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
NTSTATUS
SdpDatabase::ValidateRecordExternal(
    PUCHAR pStream,
    ULONG streamSize,
    PSDP_TREE_ROOT_NODE pTreeRoot,
    PPSM_LIST pPsmList,
    ULONG flags,
    GUID* pServiceClassUuid
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    m_RecordValidatorList.Lock();

    RecordValidator *prv;
    RECORD_VALIDATOR_INFO rvi;

    RtlZeroMemory(&rvi, sizeof(rvi));

    rvi.pPsmList = pPsmList;
    RtlCopyMemory(&rvi.serviceClass, pServiceClassUuid, sizeof(GUID));

    for (prv = m_RecordValidatorList.GetHead();
         prv != NULL;
         prv = m_RecordValidatorList.GetNext(prv)) {

        if (prv->flags & RVFI_STREAM) {
            rvi.pStream = pStream;
            rvi.streamSize = streamSize;
        }
        else {
            rvi.pStream = NULL;
            rvi.streamSize = 0;
        }

        if (prv->flags & RVFI_TREE) {
            rvi.pTreeRoot = pTreeRoot;
        }
        else {
            rvi.pTreeRoot = NULL;
        }

        status = prv->pValidator(prv->pContext,
                                 &rvi,
                                 flags);

        if (!NT_SUCCESS(status)) {
            break;
        }
    }

    m_RecordValidatorList.Unlock();

    return status;
}
#endif // UNDER_CE

#define SERVICE_SECURITY_ENCRYPT_BOTH (SERVICE_SECURITY_ENCRYPT_REQUIRED | SERVICE_SECURITY_ENCRYPT_OPTIONAL)
#define SERVICE_SECURITY_AUTH_TYPES (SERVICE_SECURITY_NONE | SERVICE_SECURITY_AUTHORIZE | SERVICE_SECURITY_AUTHENTICATE)


// NTSTATUS SdpDatabase::AddRecord(PIRP pIrp, PHANDLE pHandle, ULONG *pfService)
NTSTATUS SdpDatabase::AddRecord(PUCHAR pStream, ULONG streamSize, PHANDLE pHandle, ServiceRecord *pRecordOriginal)  // UNDER_CE
{
    NTSTATUS        status;
    PUCHAR          pNewStream = NULL;
    ULONG           newStreamSize = 0;
    ServiceRecord*  pRecord = NULL;
    ULONG           count, size;
    PSDP_NODE       pCont = NULL;
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    BOOLEAN         protIsMux;
    stack = IoGetCurrentIrpStackLocation(pIrp);
    inLen = stack->Parameters.DeviceIoControl.InputBufferLength;
    
    if (stack->Parameters.DeviceIoControl.IoControlCode ==
        IOCTL_BTH_SDP_SUBMIT_RECORD_WITH_INFO) {
        BthSdpRecord*       pBthSdpRecord;

        if (inLen < sizeof(BthSdpRecord)) {
            return STATUS_INVALID_BUFFER_SIZE;
        }

        pBthSdpRecord = (BthSdpRecord *) pIrp->AssociatedIrp.SystemBuffer;
        pStream = pBthSdpRecord->record;
        streamSize = pBthSdpRecord->recordLength;

        //
        // Validity check for all IN params
        //
        // Make sure only the supported flags are being sent
        //
        if ((pBthSdpRecord->fCodService & ~COD_SERVICE_VALID_MASK) != 0             ||
            (pBthSdpRecord->fSecurity & ~SERVICE_SECURITY_VALID_MASK) != 0   ||
            (pBthSdpRecord->fOptions & ~SERVICE_OPTION_VALID_MASK) != 0) {
            return STATUS_INVALID_PARAMETER;
        }

        fSecurity = pBthSdpRecord->fSecurity;

        //
        // Only bthport controls the limited discovery bit
        //
        if (fSecurity & COD_SERVICE_LIMITED) {
            _DbgPrintF(DBG_SDP_ERROR,
                       ("clients are not allowed to set limited COD service"));
            return STATUS_INVALID_PARAMETER;
        }

        //
        // Make sure exclusive flags are not being sent together
        //
        if ((fSecurity & SERVICE_SECURITY_ENCRYPT_BOTH) ==
            SERVICE_SECURITY_ENCRYPT_BOTH) {
            _DbgPrintF(
                DBG_SDP_ERROR,
                ("encryption can only be required or optional, not both!"));
            return STATUS_INVALID_PARAMETER;
        }

        //              
        // To have an encrypted channel, you must be authenticated first
        //
        if ((fSecurity & SERVICE_SECURITY_ENCRYPT_BOTH) != 0 && 
            (fSecurity & SERVICE_SECURITY_AUTHENTICATE) == 0) {
            _DbgPrintF(DBG_SDP_ERROR, ("encryption requires authentication"));
            return STATUS_INVALID_PARAMETER;
        }

        //
        // Check to see that only one of the AUTH security flags is set.  If more
        // than one flag is set, then subtracting one will not create the logical
        // and inverse of itself.  For instance:
        //  0x101 & (0x101-1) == 0x100 ... invalid
        //  0x100 & (0x100-1) == 0x000 ... valid
        //
        if (((fSecurity & SERVICE_SECURITY_AUTH_TYPES) &
             ((fSecurity & SERVICE_SECURITY_AUTH_TYPES) - 1)) != 0) {
            _DbgPrintF(DBG_SDP_ERROR,
                       ("more than one authentication type was specified"));
            return STATUS_INVALID_PARAMETER;
        }

        //
        // Make sure the size reported in the structure is the size of the
        // stream buffer
        //
        if (inLen - (sizeof(BthSdpRecord) - sizeof(pBthSdpRecord->record)) !=
            streamSize) {
            _DbgPrintF(
                DBG_SDP_ERROR,
                ("BthSdpRecord::recordLength (%d) does not match buffer"
                 " size (%d)", streamSize,
                 inLen - (sizeof(BthSdpRecord) - sizeof(pBthSdpRecord->record))
                 ));
            return STATUS_INVALID_PARAMETER;
        }

        fService = pBthSdpRecord->fCodService;
        fOptions = pBthSdpRecord->fOptions;

        


        if (fOptions & SERVICE_OPTION_PERMANENT) {
            _DbgPrintF(DBG_SDP_ALL,
                       ("%c%cpermanent records are not yet supported!", 7, 7));
        }
    }
    else {
        pStream = (UCHAR *) pIrp->AssociatedIrp.SystemBuffer;
        streamSize = inLen;
        fSecurity = SERVICE_SECURITY_USE_DEFAULTS; 
        fService = 0;
        fOptions = 0;
    }
#endif

    //
    // Make sure it is well formed (ie a sequence of ushort (attrib id) + one
    // element (attrib value)
    //
    status = SdpIsStreamRecord(pStream, streamSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Make sure all of the universal attributes have the correct syntax and 
    // that all of the attributes we require locally are present
    //
    USHORT err = 0xFFFF;
    status = SdpVerifyServiceRecord(pStream,
                                    streamSize, 
                                    VERIFY_CHECK_MANDATORY_LOCAL,
                                    &err);

    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    PSDP_NODE tree = NULL;
    PSDP_NODE uint32;
    PVOID pv = NULL;

    status = SdpTreeFromStream(pStream, streamSize, NULL, &tree, TRUE);
    if (!NT_SUCCESS(status)) {
        return status; 
    }
    ASSERT(tree != NULL);

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    // WinCE has added ability to update record handle (pRecord != NULL)
    ULONG recordHandle = GetNextRecordHandle();
#else
    ULONG recordHandle = pRecordOriginal ? pRecordOriginal->recordHandle : GetNextRecordHandle();
#endif

    uint32 = SdpCreateNodeUInt32(recordHandle);
    if (uint32 == NULL) {
        //
        // see if we can roll back the increment.  This will not recover the 
        // "acquired" recordHandle if another caller has tried to add a record in
        // between our call for GetNextRecordHandle and here
        //
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }

    SdpAddAttributeToTree(tree, SDP_ATTRIB_RECORD_HANDLE, uint32);

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    //
    // Only automatically add the public browse group list if the user does NOT
    // disable this feature.
    //
    if ((fOptions & SERVICE_OPTION_NO_PUBLIC_BROWSE) == 0 &&
        (fOptions & SERVICE_OPTION_DO_NOT_PUBLISH) == 0) {
        pCont = NULL;
        status = SdpFindAttributeInTree(tree,
                                        SDP_ATTRIB_BROWSE_GROUP_LIST,
                                        &pCont);

        if (NT_SUCCESS(status)) {
            // 
            // Group was present
            //
            /* do nothing */;
        }
        else {
            PSDP_NODE pSeq, pUuid16;
             
            ASSERT(pCont == NULL);

            //
            // Group was not present, add it
            //
            // Keep status as a failure code until we successfully add the
            // attribute
            //
            pSeq = SdpCreateNodeSequence();

            if (pSeq) {
                pUuid16 =
                    SdpCreateNodeUUID16(PublicBrowseGroupServiceClassID_UUID16);

                if (pUuid16) {
                    //
                    // Add in the node and set our status to success
                    //
                    SdpAppendNodeToContainerNode(pSeq, pUuid16);
                    SdpAddAttributeToTree(tree,
                                          SDP_ATTRIB_BROWSE_GROUP_LIST,
                                          pSeq);
                    status = STATUS_SUCCESS;
                }
                else {
                    SdpFreeOrphanedNode(pSeq);
                }
            }
        }
    }

    if (!NT_SUCCESS(status)) {
        goto Done;
    }   
#endif // UNDER_CE

    //
    // See if there is a protocol list.  Some records, such as the BrowseGroupInfo
    // will not have any protocols associated with them
    //
    status = SdpFindAttributeInTree(tree, SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, &pCont);
    if (NT_SUCCESS(status)) {
        ASSERT(pCont != NULL);

        if (pCont->hdr.Type == SDP_TYPE_ALTERNATIVE) {
            count = 0;
            PLIST_ENTRY entry = pCont->u.alternative.Link.Flink;
            for ( ; entry != &pCont->u.alternative.Link; entry = entry->Flink, count++) 
                ;

            ASSERT(count != 0);
        }
        else {
            ASSERT(pCont->hdr.Type == SDP_TYPE_SEQUENCE);
            count = 1;
        }
    }
    else {
        status = STATUS_SUCCESS;
        count = 0;
    }

    size = sizeof(ServiceRecord);
    if (count > 1) {
        size += sizeof(PSM_PROTOCOL_PAIR) * (count - 1);
    }

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))    
    
    pv = ExAllocatePoolWithTag(NonPagedPool, size, SDP_TAG);

    if (pv) {
        pRecord = new(pv) ServiceRecord;
    }
    else {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }

    if (count && pCont) {
        //
        // This will also initialize the psmList
        //
        status = SdpValidateProtocolContainer(pCont, &pRecord->psmList);
        if (!NT_SUCCESS(status)) {
            goto Done;
        }
    }
#else
    pRecord = new ServiceRecord();
    if (!pRecord) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Done;
    }
#endif

    //
    // Get the service class ID.  This is a sequence of UUIDs, where the 1st
    // item in the list is the most specific.  SdpVerifyServiceRecord has made
    // sure that this attrib exists
    //
    SdpFindAttributeInTree(tree, SDP_ATTRIB_CLASS_ID_LIST, &pCont);
    ASSERT(pCont != NULL);
    ASSERT(pCont->hdr.Type == SDP_TYPE_SEQUENCE);
    ASSERT(!IsListEmpty(&pCont->u.sequence.Link));

    //
    // Store the UUID as a 128 bit UUID to make comparison easier
    //
    SdpNormalizeUuid(
        CONTAINING_RECORD(pCont->u.sequence.Link.Flink, SDP_NODE, hdr.Link),
        &pRecord->serviceClass);

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    serviceClass = pRecord->serviceClass;

    //
    //  Allow each validator to validate stream
    //
    status = ValidateRecordExternal(pStream,
                                    streamSize,
                                    tree,
                                    &pRecord->psmList,
                                    RVF_ADD_RECORD,
                                    &pRecord->serviceClass
                                    );
#endif // UNDER_CE                                    

    if (!NT_SUCCESS(status)) {
        goto Done;
    }


    status = SdpStreamFromTree(tree, &pNewStream, &newStreamSize);
    if (!NT_SUCCESS(status)) {
        goto Done; 
    }


//
// don't know if this chunk of code is necessary or not
//
#if 0
    //
    // Revalidate in case one of the validators screwed up
    //
    status = SdpIsStreamRecord(pNewStream, newStreamSize);
    if (!NT_SUCCESS(status)) {
         goto Done;
    }

    //
    // Make sure all of the universal attributes have the correct syntax and 
    // that all of the attributes we require locally are present
    //
    err = 0xFFFF;
    status = SdpVerifyServiceRecord(pNewStream,
                                    newStreamSize, 
                                    VERIFY_CHECK_MANDATORY_LOCAL,
                                    &err);
    if (!NT_SUCCESS(status)) {
        goto Done;
    }
#endif

    m_ServiceRecordList.Lock();

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    ProtMux *ppm;
    ULONG i; // ,j
    protIsMux = FALSE;

    for (i = 0; i < pRecord->psmList.count; i++) {
        if ((protIsMux = IsProtocolMuxed(pRecord->psmList.list[i].protocol))) {
            break;
        }
    }

    if (protIsMux == FALSE) {
        //
        // Make sure the protocol isn't duplicated across any other record
        //
        ServiceRecord *psr;

        for (psr = m_ServiceRecordList.GetHead();
             psr != NULL;
             psr = m_ServiceRecordList.GetNext(psr)) {

            for (i = 0; i < psr->psmList.count; i++) {
                for (j = 0; j < pRecord->psmList.count; j++) {
                    //
                    // We only care about potential conflicts on the PSM
                    //
                    if (psr->psmList.list[i].psm == pRecord->psmList.list[j].psm) {
                        if (IsEqualUuid(&psr->psmList.list[i].protocol,
                                        &pRecord->psmList.list[j].protocol)) {
                            //
                            // protocol is being muxed w/out a registered mux handler
                            //
                            status = STATUS_DEVICE_PROTOCOL_ERROR;
                        }
                        else {
                            //
                            // different protocols on top of the same PSM
                            //
                            status = STATUS_NO_MATCH;
                        }

                        break;
                    }
                }

                if (!NT_SUCCESS(status)) {
                    break;
                }
            }

            if (!NT_SUCCESS(status)) {
                break;
            }
        }
    }
#endif // UNDER_CE

    if (NT_SUCCESS(status)) {
        //
        // Make sure the psmList in this record is either unique, or we have a
        // registered multiplexor for it
        //

#if defined (UNDER_CE) || defined (WINCE_EMULATION)
        if (pRecordOriginal)  {
            if (pRecordOriginal->pStream)
                SdpFreePool(pRecordOriginal->pStream);

            memcpy(&pRecordOriginal->serviceClass,&pRecord->serviceClass,sizeof(GUID));
            ASSERT(pRecordOriginal == pRecordOriginal->handle);
            ASSERT(pRecordOriginal->recordHandle == recordHandle);

            delete pRecord;
            pRecord = pRecordOriginal;
        }

        pRecord->recordHandle = recordHandle;
        pRecord->streamSize = newStreamSize;
        pRecord->pStream = pNewStream;
        pRecord->psmList.count = 0;

        AlterDatabaseState();

        if (!pRecordOriginal)
            m_ServiceRecordList.AddTail(pRecord);
#if defined (UNDER_CE)
        *pHandle = pRecord->handle = (HANDLE) pRecord;
#endif

#else   // UNDER_CE
        pRecord->recordHandle = recordHandle;
        pRecord->streamSize = newStreamSize;
        pRecord->fService = fService;
        pRecord->fOptions = fOptions;
        pRecord->pFileObject = stack->FileObject;
        pRecord->pStream = pNewStream;
        pRecord->handle = (HANDLE) pRecord;

        AlterDatabaseState();

        m_ServiceRecordList.AddTail(pRecord);
#endif  // UNDER_CE
    }

    m_ServiceRecordList.Unlock();

Done:
    if (tree) {
        SdpFreeTree(tree);
    }

    if (NT_SUCCESS(status)) {
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
        FireWmiEvent(*pHandle, SdpDatabaseEventNewRecord);
        Globals.pSecDB->AddService(serviceClass, fSecurity);
        *pfService = fService;
#endif
    }
    else {
        if (pRecord) {
            delete pRecord;
        }

        if (pNewStream) {
            SdpFreePool(pNewStream);
        }

        //
        // this will revert the next record handle count if there have been 
        // no new records in between the increment and here.  Otherwise, it will
        // be a gap
        //
#if defined (UNDER_CE)
        InterlockedCompareExchange(&m_nextRecordHandle,
                                   (LONG)(recordHandle-1),
                                   (LONG) recordHandle);
#else
        InterlockedCompareExchange((void **)&m_nextRecordHandle,
                                   (void *)(recordHandle-1),
                                   (void *) recordHandle);
#endif
    }

    return status;
}

NTSTATUS SdpDatabase::UpdateRecord(UCHAR *pStream, ULONG stremaSize, HANDLE handle)
{
#if (defined (UNDER_CE) || defined (WINCE_EMULATION))
    ServiceRecord *pRecordOriginal;
    NTSTATUS status;

    if (handle == 0) {
    	ASSERT(0); // handle == 0 only for SDP specific database entry.  Should never get in this state
		return STATUS_NOT_FOUND;
    }
 
    // make sure record is in the data base.
    m_ServiceRecordList.Lock();
    for (pRecordOriginal = m_ServiceRecordList.GetHead();
         pRecordOriginal != NULL;
         pRecordOriginal = m_ServiceRecordList.GetNext(pRecordOriginal)) {
        if (pRecordOriginal->handle == handle) {
            break;
        }
    }

    if (!pRecordOriginal) {
        m_ServiceRecordList.Unlock();
        return STATUS_NOT_FOUND;
    }

    status = AddRecord(pStream,stremaSize,&handle,pRecordOriginal);
    m_ServiceRecordList.Unlock();
    return status;

#else // UNDER_CE
    // This code isn't fully implemented on NT yet.
    if (handle == INVALID_ID) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Find the record in the list, then remove it.  If the update
    // goes well, it will be reinserted into the list.  I remove it so
    // I can make calls outside the driver without holding onto a lock
    //
    ServiceRecord *pRecord;
    NTSTATUS status = STATUS_NOT_FOUND;

#if 0
    m_ServiceRecordList.Lock();
    for (pRecord = m_ServiceRecordList.GetHead();
         pRecord != NULL;
         pRecord = m_ServiceRecordList.GetNext(pRecord)) {
        
        if (pRecord->handle == handle) {
            m_ServiceRecordList.RemoveAt(pRecord);
            status = STATUS_SUCCESS;
            break;
        }
    }

    m_ServiceRecordList.Unlock();
#endif

    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // TBD:  of the core attributes, what do we allow to change?
    // o RecordHandle ? defnitely not
    // o ServiceClass ?  if so, then the record is more "new" than updated
    // o 
    //
    FireWmiEvent(handle, SdpDatabaseEventUpdateRecord);

    return status;
#endif 
}



NTSTATUS
SdpDatabase::RemoveRecord(
    HANDLE handle,
    ULONG *pfService
    )
{
    if (handle == INVALID_ID) {
        return STATUS_INVALID_PARAMETER;
    }

    ServiceRecord *pRecord;
    NTSTATUS status = STATUS_NOT_FOUND;

    m_ServiceRecordList.Lock();
    for (pRecord = m_ServiceRecordList.GetHead();
         pRecord != NULL;
         pRecord = m_ServiceRecordList.GetNext(pRecord)) {
        
        if (pRecord->handle == handle) {
            m_ServiceRecordList.RemoveAt(pRecord);
            AlterDatabaseState();
            status = STATUS_SUCCESS;
            break;
        }
    }

    m_ServiceRecordList.Unlock();

    if (NT_SUCCESS(status)) {
        FireWmiEvent(handle, SdpDatabaseEventRemoveRecord);
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
        *pfService = pRecord->fService;
#endif
        delete pRecord;
    }

    return status;
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
void SdpDatabase::Cleanup(PFILE_OBJECT pFileObject)
{
    ServiceRecordList sdl;

    ServiceRecord *pRecord;

    m_ServiceRecordList.Lock();

    pRecord = m_ServiceRecordList.GetHead();
    while (pRecord != NULL) {
        
        if (pRecord->pFileObject == pFileObject) {
            //
            // Retrieve the next record in the list b/c we will lose the link
            // once it pRecord is removed from the list
            //
            ServiceRecord *pNext = m_ServiceRecordList.GetNext(pRecord);
            m_ServiceRecordList.RemoveAt(pRecord);
            sdl.AddTail(pRecord);
            pRecord = pNext;
        }
        else {
            pRecord = m_ServiceRecordList.GetNext(pRecord);
        }
    }

    if (!sdl.IsEmpty()) {
        AlterDatabaseState();
    }

    m_ServiceRecordList.Unlock();

    while (!sdl.IsEmpty()) {
        pRecord = sdl.RemoveHead();
        delete pRecord;
    }
}
#endif  // defined (UNDER_CE) || defined (WINCE_EMULATION)

void SdpDatabase::AlterDatabaseState()
{
    NTSTATUS status; 
    ULONG val, size;
    PUCHAR pStream;

    val = RtlUlongByteSwap(InterlockedIncrement((PLONG) &m_dbState)); 

    //
    // DB lock is held while this function is called
    //
    status = SdpFindAttributeInStream(m_pSdpRecord->pStream,
                                      m_pSdpRecord->streamSize,
                                      SDP_ATTRIB_SDP_DATABASE_STATE,
                                      &pStream,
                                      &size
                                      );

    //
    // The stream returned is the entire element, not just the element value, so 
    // we need to increment beyond the header
    //
    ASSERT(NT_SUCCESS(status));
    ASSERT(size == sizeof(val) + sizeof(UCHAR));

#if DBG
    UCHAR type, sizeIndex;
    SdpRetrieveHeader(pStream, type, sizeIndex);
    
    ASSERT(type == SDP_TYPE_UINT);
    ASSERT(sizeIndex == 2); 
#endif // DBG

    pStream++;

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    KeWaitForSingleObject((PVOID) &m_dbStateSemaphore,
                          Executive,
                          KernelMode,
                          FALSE,
                          NULL);

    RtlCopyMemory(pStream, &val, sizeof(val));

    KeReleaseSemaphore(&m_dbStateSemaphore, (KPRIORITY) 0, 1, FALSE); 
#else
    RtlCopyMemory(pStream, &val, sizeof(val));
#endif
}

void SdpDatabase::FireWmiEvent(
    HANDLE recordHandle,
    SDP_DATABASE_EVENT_TYPE type
    )
{
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    PSDP_DATABASE_EVENT pEvent = (PSDP_DATABASE_EVENT)
        ExAllocatePoolWithTag(PagedPool, sizeof(*pEvent), SDP_TAG); 

    if (pEvent) {
        DeviceLE *ple;

        pEvent->type = type;
        pEvent->handle = recordHandle;

        m_DeviceList.Lock();
        ple = m_DeviceList.GetHead();
        if (ple) {
            WmiFireEvent(ple->device,
                         (LPGUID) &GUID_BTHPORT_WMI_SDP_DATABASE_EVENT,
                         0,
                         sizeof(*pEvent),
                         pEvent);
        }
        else {
            ExFreePool(pEvent);
        }
        m_DeviceList.Unlock();
    }
#endif    // defined (UNDER_CE) || defined (WINCE_EMULATION)
}

NTSTATUS SdpDatabase::AddProtMux(GUID *protocol)
{
    ProtMux * ppm, *ppmIter;
    NTSTATUS status = STATUS_SUCCESS;


    
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    ppm = new(NonPagedPool, PM_TAG) ProtMux(protocol);
#else
    ppm = new ProtMux(protocol);
#endif

    if (ppm) {
        m_ServiceRecordList.Lock();

        for (ppmIter = m_ProtMuxList.GetHead();
             ppmIter != NULL;
             ppmIter = m_ProtMuxList.GetNext(ppmIter)) {
            if (IsEqualUuid(protocol, &ppmIter->protocol)) {
                status = STATUS_OBJECT_NAME_COLLISION; 
                break;
            }
        }

        if (NT_SUCCESS(status)) {
            m_ProtMuxList.AddTail(ppm);
            m_ServiceRecordList.Unlock();
        }
        else {
            m_ServiceRecordList.Unlock();
            delete ppm;
        }
    }
    else {
        status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return status;
}

NTSTATUS SdpDatabase::RemoveProtMux(GUID *protocol)
{
    ProtMux *ppm;
    NTSTATUS status = STATUS_NOT_FOUND;

    m_ServiceRecordList.Lock();
    for (ppm = m_ProtMuxList.GetHead();
         ppm != NULL;
         ppm = m_ProtMuxList.GetNext(ppm)) {
        if (IsEqualUuid(protocol, &ppm->protocol)) {
            m_ProtMuxList.RemoveAt(ppm);
            break;
        }
    }
    m_ServiceRecordList.Unlock();

    if (NT_SUCCESS(status)) {
        delete ppm;
    }

    return status;
}

BOOLEAN SdpDatabase::IsProtocolMuxed(const GUID &protocol)
{
    ProtMux *ppm;

    for (ppm = m_ProtMuxList.GetHead();
         ppm != NULL;
         ppm = m_ProtMuxList.GetNext(ppm)) {

        if (IsEqualUuid(&ppm->protocol, &protocol)) {
            return TRUE;
        }
    }

    return FALSE;
}

NTSTATUS SdpDatabase::GetPsmInfo(USHORT psm, GUID *protocol, ULONG *serviceHandle)
{
    ServiceRecord *pRecord;
    NTSTATUS status = STATUS_NOT_FOUND;

    m_ServiceRecordList.Lock();

    for (pRecord = m_ServiceRecordList.GetHead();
         pRecord != NULL;
         pRecord = m_ServiceRecordList.GetNext(pRecord)) {

        for (ULONG i = 0; i < pRecord->psmList.count; i++) {
            if (psm == pRecord->psmList.list[i].psm) {
                *serviceHandle = pRecord->recordHandle;
                RtlCopyMemory(protocol,
                              &pRecord->psmList.list[i].protocol,
                              sizeof(GUID));
                status = STATUS_SUCCESS;
                break;
            }
        }

        if (NT_SUCCESS(status)) {
            break;
        }
    }
    
    m_ServiceRecordList.Unlock();

    return status;
}

NTSTATUS SdpDatabase::GetServiceClass(USHORT psm, GUID *serviceClass)
{
    // NOTE: If this function is ever called for WinCE, be sure to make sure
    // gpSdp (in sdpbase.cxx) has lock held.
    ServiceRecord *pRecord;
    NTSTATUS status = STATUS_NOT_FOUND;

    m_ServiceRecordList.Lock();

    for (pRecord = m_ServiceRecordList.GetHead();
         pRecord != NULL;
         pRecord = m_ServiceRecordList.GetNext(pRecord)) {

        for (ULONG i = 0; i < pRecord->psmList.count; i++) {
            if (psm == pRecord->psmList.list[i].psm) {
                if (IsProtocolMuxed(pRecord->psmList.list[i].protocol)) {
                    status = STATUS_PENDING;
                }
                else {
                    RtlCopyMemory(serviceClass,
                                  &pRecord->serviceClass,
                                  sizeof(GUID));

                    status = STATUS_SUCCESS;
                }
                break;
            }
        }

        if (NT_SUCCESS(status)) {
            break;
        }
    }
    
    m_ServiceRecordList.Unlock();

    return status;
}

ULONG SdpDatabase::GetNextRecordHandle()
{
    ULONG handle = (ULONG) InterlockedIncrement(&m_nextRecordHandle);
    if (handle == 0) {
        //
        // we have wrapped, goto the start, then increment
        //
        // ISSUE:  we should really find a hole in the records list, but we 
        //         worry about that when the time comes
        //
#if defined (UNDER_CE)
        InterlockedCompareExchange((LONG*)&m_nextRecordHandle,
                                   INIT_HANDLE_VALUE,
                                   0);
#else
        InterlockedCompareExchange((void **)&m_nextRecordHandle,
                                   (void *)INIT_HANDLE_VALUE,
                                   (void *)0);
#endif
        handle = (ULONG) InterlockedIncrement(&m_nextRecordHandle);
    }

    return handle;
}


#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
extern "C"
NTSTATUS pSdpTreeFromStream(
    PUCHAR Stream,
    ULONG Size,
    PSDP_TREE_ROOT_NODE *Node
    )
{
    return SdpTreeFromStream(Stream, Size, Node, TRUE);
}

NTSTATUS SdpDatabase::HandleQI(PIRP Irp)
{
    PIO_STACK_LOCATION stack;
    NTSTATUS status;
    const GUID *pInterfaceType;

    status = Irp->IoStatus.Status;
    stack = IoGetCurrentIrpStackLocation(Irp);

    pInterfaceType = stack->Parameters.QueryInterface.InterfaceType; 

    if (IsEqualUuid(pInterfaceType, &GUID_BTHDDI_SDP_VALIDATE_INTERFACE)) {
        PBTHDDI_SDP_VALIDATE_INTERFACE pValidate = (PBTHDDI_SDP_VALIDATE_INTERFACE)
            stack->Parameters.QueryInterface.Interface;

        if (pValidate->Interface.Size != sizeof(*pValidate) ||
            pValidate->Interface.Version < 1                ||
            (pValidate->Validator != NULL &&
                 (pValidate->Flags & RVFI_VALID_FLAGS) == 0)) {
            status = STATUS_INVALID_PARAMETER; 
        }
        else if (pValidate->Validator == NULL) {
            //
            // removal
            //
            if (pValidate->Cookie == NULL) {
                status = STATUS_INVALID_PARAMETER; 
            }
            else {
                RecordValidator *prv;

                m_RecordValidatorList.Lock();
                for (prv = m_RecordValidatorList.GetHead();
                     prv != NULL;
                     prv = m_RecordValidatorList.GetNext(prv)) {
                    if ((PVOID) prv == pValidate->Cookie) {
                        m_RecordValidatorList.RemoveAt(prv);
                        break;
                    }
                }
                m_RecordValidatorList.Unlock();

                if (prv) {
                    status = STATUS_SUCCESS;
                    delete prv;
                }
                else {
                    status = STATUS_NOT_FOUND;
                }
            }
        }
        else {
            
            RecordValidator *prv =
                new(NonPagedPool, RV_TAG) RecordValidator(pValidate);

            if (prv == NULL) {
                status = STATUS_INSUFFICIENT_RESOURCES;
            }
            else {
                m_RecordValidatorList.Lock();
                m_RecordValidatorList.AddTail(prv);
                m_RecordValidatorList.Unlock();

                status = STATUS_SUCCESS;
            }
        }
    }
    else if (IsEqualUuid(pInterfaceType, &GUID_BTHDDI_SDP_PARSE_INTERFACE)) {
        PBTHDDI_SDP_PARSE_INTERFACE pParse = (PBTHDDI_SDP_PARSE_INTERFACE)
            stack->Parameters.QueryInterface.Interface;

        if (pParse->Interface.Size != sizeof(*pParse) ||
            pParse->Interface.Version < 1) {
            status = STATUS_INVALID_PARAMETER;
        }
        else {
            status = STATUS_SUCCESS;

            pParse->SdpValidateStream = SdpDatabase::ValidateStream;
            pParse->SdpWalkStream = SdpWalkStream;

            pParse->SdpConvertStreamToTree = pSdpTreeFromStream;
            pParse->SdpConvertTreeToStream = SdpStreamFromTree;

            pParse->SdpByteSwapUuid128 = SdpByteSwapUuid128;
            pParse->SdpByteSwapUint128 = SdpByteSwapUint128;
            pParse->SdpByteSwapUint64 =  SdpByteSwapUint64;

            pParse->SdpRetrieveUuid128 = SdpRetrieveUuid128;
            pParse->SdpRetrieveUint128 = SdpRetrieveUint128;
            pParse->SdpRetrieveUint64 =  SdpRetrieveUint64;

            pParse->SdpFindAttribute =         SdpFindAttributeInStream;
            pParse->SdpFindAttributeSequence = SdpFindAttributeSequenceInStream;
            pParse->SdpFindAttributeInTree =   SdpFindAttributeInTree;
        }
    }
    else if (IsEqualUuid(pInterfaceType, &GUID_BTHDDI_SDP_NODE_INTERFACE)) {
        PBTHDDI_SDP_NODE_INTERFACE pNode = (PBTHDDI_SDP_NODE_INTERFACE)
            stack->Parameters.QueryInterface.Interface;

        if (pNode->Interface.Size != sizeof(*pNode) ||
            pNode->Interface.Version < 1) {
            status = STATUS_INVALID_PARAMETER;
        }
        else {
            pNode->SdpCreateNodeTree = SdpCreateNodeTree;
            pNode->SdpFreeTree = SdpFreeTree;

            pNode->SdpCreateNodeNil = SdpCreateNodeNil;

            pNode->SdpCreateNodeBoolean = SdpCreateNodeBoolean;

            pNode->SdpCreateNodeUint8 = SdpCreateNodeUInt8;
            pNode->SdpCreateNodeUint16 = SdpCreateNodeUInt16;
            pNode->SdpCreateNodeUint32 = SdpCreateNodeUInt32;
            pNode->SdpCreateNodeUint64 = SdpCreateNodeUInt64;
            pNode->SdpCreateNodeUint128 = SdpCreateNodeUInt128;

            pNode->SdpCreateNodeInt8 = SdpCreateNodeInt8;
            pNode->SdpCreateNodeInt16 = SdpCreateNodeInt16;
            pNode->SdpCreateNodeInt32 = SdpCreateNodeInt32;
            pNode->SdpCreateNodeInt64 = SdpCreateNodeInt64;
            pNode->SdpCreateNodeInt128 = SdpCreateNodeInt128;

            pNode->SdpCreateNodeUuid16 = SdpCreateNodeUUID16;
            pNode->SdpCreateNodeUuid32 = SdpCreateNodeUUID32;
            pNode->SdpCreateNodeUuid128 = SdpCreateNodeUUID128;

            pNode->SdpCreateNodeString = SdpCreateNodeString;
            pNode->SdpCreateNodeUrl = SdpCreateNodeUrl;

            pNode->SdpCreateNodeAlternative = SdpCreateNodeAlternative;
            pNode->SdpCreateNodeSequence = SdpCreateNodeSequence;

            pNode->SdpAddAttributeToTree = SdpAddAttributeToTree;
            pNode->SdpAppendNodeToContainerNode = SdpAppendNodeToContainerNode;


            status = STATUS_SUCCESS;
        }
    }
    else if (IsEqualUuid(pInterfaceType, &GUID_BTHDDI_MUX_PROTOCOL_INTERFACE)) {
        PBTH_MUX_PROTOCOL_INTERFACE pMux =  (PBTH_MUX_PROTOCOL_INTERFACE)
            stack->Parameters.QueryInterface.Interface;

        if (pMux->Interface.Size != sizeof(*pMux) ||
            pMux->Interface.Version < 1) {
            status = STATUS_INVALID_PARAMETER;
        }
        else {
            status = AddProtMux(&pMux->Protocol);

            if (NT_SUCCESS(status)) {
                pMux->GetSecurityInfoContext = (PVOID) Globals.pSecDB;
                pMux->GetServiceConnectRequirements =
                    SecurityDatabase::_GetServiceConnectRequirements;
            }
        }
    }

    Irp->IoStatus.Status = status;
    return status;
}


void
SdpDatabase::AddDevice(
    PDEVICE_OBJECT DeviceObject
    )
{
    DeviceLE *ple = new (PagedPool, SDP_TAG) DeviceLE(DeviceObject);
    if (ple) {
    }
}

void
SdpDatabase::RemoveDevice(
    PDEVICE_OBJECT DeviceObject
    )
{
    DeviceLE *ple = NULL;

    m_DeviceList.Lock();

    for (ple = m_DeviceList.GetHead();
         ple != NULL;
         ple = m_DeviceList.GetNext(ple)) {
        if (ple->device == DeviceObject) {
            m_DeviceList.RemoveAt(ple);
            break;
        }
    }

    m_DeviceList.Unlock();

    if (ple) {
        delete ple;
    }
}
#endif // UNDER_CE

#if defined (UNDER_CE)


// Remove all entries associated with a process when it's being terminated.
void SdpDatabase::RemoveRecordsAssociatedWithProcess(HANDLE hDyingProc) {
    ServiceRecord *pRecord = m_ServiceRecordList.GetHead();

    m_ServiceRecordList.Lock();

    while (pRecord) {
         if (hDyingProc == pRecord->hCallerProc) {
             ServiceRecord *pNext = m_ServiceRecordList.GetNext(pRecord);

             m_ServiceRecordList.RemoveAt(pRecord);
             AlterDatabaseState();
             delete pRecord;

             pRecord = pNext;
         }
         else
             pRecord = m_ServiceRecordList.GetNext(pRecord);
    }

    m_ServiceRecordList.Unlock();
}
#endif
