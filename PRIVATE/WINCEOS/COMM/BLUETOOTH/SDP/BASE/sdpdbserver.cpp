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

   SdpDBUuidSearch.CPP

Abstract:

    This module implemnts BTh SDP core spec.



   Doron J. Holan

   Notes:

   Environment:

   Kernel mode only


Revision History:
*/
#include "common.h"






extern ULONG g_Support10B;

struct QueryUuidData {
    QueryUuidData() { RtlZeroMemory(this, sizeof(*this)); }

	GUID uuid;
	UCHAR found;
}; 

struct SdpUuidSearch {
    SdpUuidSearch() { ZERO_THIS(); }

    NTSTATUS RetrieveUuids(PUCHAR pStream, ULONG streamSize)
    {
        PAGED_CODE();

        NTSTATUS status =
            SdpVerifySequenceOf(pStream,
                                streamSize,
                                SDP_TYPE_UUID,
                                NULL,
                                NULL,
                                (PSDP_STREAM_WALK_FUNC) FindUuidsInSearchStreamWalk,
                                (PVOID) this);

        if (NT_SUCCESS(status) && max == 0) {
            SdpPrint(SDP_DBG_UUID_ERROR, ("valid UUID search seq, but no UUIDs in it!\n"));
            return STATUS_INVALID_PARAMETER;
        }

        SdpPrint(SDP_DBG_UUID_TRACE,
                 ("UUID search stream has %d elements\n", (ULONG) max+1));

        return status;
    }

    BOOLEAN SearchStream(PUCHAR pStream, ULONG streamSize);

    void SetUuids(SdpQueryUuid *pUuid, UCHAR maxUuid);

    static NTSTATUS FindUuidsInSearchStreamWalk(SdpUuidSearch * pUuidSrch,
                                                UCHAR DataType,
                                                ULONG DataSize,
                                                PUCHAR Data,
                                                ULONG DataStorageSize);

    static NTSTATUS FindUuidsInRecordStreamWalk(SdpUuidSearch *pUuidSrch,
                                                UCHAR DataType,
                                                ULONG DataSize,
                                                PUCHAR Data);

	ULONG current;
	ULONG max;
	QueryUuidData queryData[MAX_UUIDS_IN_QUERY];
}; 

BOOLEAN SdpUuidSearch::SearchStream(PUCHAR pStream, ULONG streamSize)
{
    PAGED_CODE();

	ULONG i;

    //
    // reset the search results
    //
    current = 0;
    for (i = 0; i < max; i++) {
        queryData[i].found = FALSE;
    }

    //
    // now search the record stream
    //
    SdpWalkStream(pStream,
                  streamSize, 
                  (PSDP_STREAM_WALK_FUNC) FindUuidsInRecordStreamWalk,
                  (PVOID) this);

    for (i = 0; i < max; i++) {
        if (queryData[i].found != TRUE) {
            SdpPrint(SDP_DBG_UUID_INFO | SDP_DBG_UUID_WARNING,
                     ("did not find UUID #%d in stream\n", i));
            return FALSE;
        }
    }

    SdpPrint(SDP_DBG_UUID_INFO, ("found all UUIDs in the stream\n"));

    return TRUE;
}

void
SdpUuidSearch::SetUuids(
    SdpQueryUuid *pUuid,
    UCHAR maxUuid
    )
{
//  GUID uuid;

    max = maxUuid;

    for (ULONG i = 0; i < max; i++) {
        if (pUuid[i].uuidType == SDP_ST_UUID16) {
            RtlCopyMemory(&queryData[i].uuid, &Bluetooth_Base_UUID, sizeof(GUID));
            queryData[i].uuid.Data1 += pUuid[i].u.uuid16;
        }
        else if (pUuid[i].uuidType == SDP_ST_UUID32) {
            RtlCopyMemory(&queryData[i].uuid, &Bluetooth_Base_UUID, sizeof(GUID));
            queryData[i].uuid.Data1 += pUuid[i].u.uuid32;
        }
        else {
            RtlCopyMemory(&queryData[i].uuid, &pUuid[i].u.uuid128, sizeof(GUID));
        }

        //
        // Convert to network byte order so that we can search for it
        //
        SdpByteSwapUuid128(&queryData[i].uuid, &queryData[i].uuid);
    }
}

NTSTATUS 
SdpUuidSearch::FindUuidsInSearchStreamWalk(
    SdpUuidSearch *pUuidSrch,
    UCHAR DataType,
    ULONG DataSize,
    PUCHAR Data,
    ULONG DataStorageSize
    )
{
    PAGED_CODE();

    if (pUuidSrch->current >= MAX_UUIDS_IN_QUERY) {
        return STATUS_TOO_MANY_GUIDS_REQUESTED;
    }
    
    SdpRetrieveUuidFromStream(Data,
                              DataSize,
                              &(pUuidSrch->queryData + pUuidSrch->current)->uuid,
                              TRUE);
        
    pUuidSrch->current++;
    pUuidSrch->max++;

    return STATUS_SUCCESS;
}

NTSTATUS
SdpUuidSearch::FindUuidsInRecordStreamWalk(
    SdpUuidSearch *pUuidSrch,
    UCHAR DataType,
    ULONG DataSize,
    PUCHAR Data
    )
{
    PAGED_CODE();

    if (DataType != SDP_TYPE_UUID) {
        //
        // not interested in anything but UUIDs 
        //
        return STATUS_SUCCESS;
    }

    QueryUuidData query;
    SdpRetrieveUuidFromStream(Data, DataSize, &query.uuid, TRUE);

    for (ULONG i = 0; i < pUuidSrch->max; i++) {
        //
        // Since we normalize all UUIDs to 128 bits, just to a memcmp on the 2
        // values
        //
        if (IsEqualUuid(&pUuidSrch->queryData[i].uuid, &query.uuid)) {
            pUuidSrch->queryData[i].found = TRUE;

            //
            // Do not break out of the loop.  If the client repeated any UUIDs, 
            // than we should mark those as found as well.
            //
            // break;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
SdpDatabase::ServiceSearchRequestResponseRemote(
    UCHAR *pStream,
    ULONG streamSize,
    USHORT maxRecordsRequested,
    ULONG **ppResultStream,         // must be freed when xmit is finished
    USHORT *pRecordCount,
    PSDP_ERROR pSdpError
    )
{
    SdpUuidSearch uuidSrch;

    //
    // Suck all of the UUIDs out of the search list
    //
    NTSTATUS status = uuidSrch.RetrieveUuids(pStream, streamSize);
    if (!NT_SUCCESS(status)) {
        SdpPrint(SDP_DBG_UUID_ERROR,
                 ("could not retrieve the uuids from the request, 0x%x\n",
                  status));

        *pSdpError = MapNtStatusToSdpError(status);
        return status;
    }

    return ServiceSearchRequestResponse(&uuidSrch,
                                        maxRecordsRequested,
                                        ppResultStream,
                                        pRecordCount,
                                        pSdpError,
                                        FALSE);
}

NTSTATUS
SdpDatabase::ServiceSearchRequestResponseLocal(
    SdpQueryUuid *pUuid,
    UCHAR numUuid,
    USHORT maxRecordsRequested,
    ULONG **ppResultStream,         // must be freed when xmit is finished
    USHORT *pRecordCount
    )
{
    SdpUuidSearch uuidSrch;
    SDP_ERROR sdpError;

    uuidSrch.SetUuids(pUuid, numUuid);

    return ServiceSearchRequestResponse(&uuidSrch,
                                        maxRecordsRequested,
                                        ppResultStream,
                                        pRecordCount,
                                        &sdpError,
                                        TRUE);
}

NTSTATUS
SdpDatabase::ServiceSearchRequestResponse(
    SdpUuidSearch *pUuidSearch,
    USHORT maxRecordsRequested,
    ULONG **ppResultStream,
    USHORT *pRecordCount,
    PSDP_ERROR pSdpError,
    BOOLEAN local
    )
{
    PAGED_CODE();

    NTSTATUS status;

    SdpPrint(SDP_DBG_UUID_TRACE, ("ServiceSearchRequestResponse enter\n"));

    *ppResultStream = NULL;
    *pRecordCount = 0;

    USHORT numFound;

    numFound = 0;

    status = STATUS_SUCCESS;

    //
    // Iterate over each local record.  If ALL the UUIDs in the search are found
    // then report the search record back to the requestor
    //
    m_ServiceRecordList.Lock();

    HandleList hl;

    HandleEntry *phe;
    ServiceRecord *pRecord;

    for (pRecord = m_ServiceRecordList.GetHead();
         pRecord != NULL;
         pRecord = m_ServiceRecordList.GetNext(pRecord)) {

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
        //
        // We do not hide records from local browsing
        //
        if (local == FALSE &&
            pRecord->fOptions & SERVICE_OPTION_DO_NOT_PUBLISH) {
            //
            // Record sumbitter did not want this SDP record published to the 
            // world.  Skip it.
            //
            continue;
        }
#endif

        if (pUuidSearch->SearchStream(pRecord->pStream, pRecord->streamSize)) {
            SdpPrint(SDP_DBG_UUID_INFO,
                     ("uuid search, FOUND match in record 0x%x\n",
                      pRecord->recordHandle));

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
            phe = new (PagedPool, SDP_TAG) HandleEntry(pRecord->recordHandle);
#else
            phe = new HandleEntry(pRecord->recordHandle);
#endif
            if (phe) {
                hl.AddTail(phe);
                numFound++;
            }

            //
            // Found as many as the requestor asked for
            //
            if (numFound == maxRecordsRequested) {
                break;
            }
        }
        else {
            SdpPrint(SDP_DBG_UUID_INFO,
                     ("did not find any matches in record 0x%x\n",
                     pRecord->recordHandle));
        }
    }

    m_ServiceRecordList.Unlock();

    if (numFound > 0) {
        PULONG pResult;

        //
        // TODO:  I can optimize this by just returning the linked list and then
        //      pulling values off of that instead of allocating pool again here
        //
        pResult = (ULONG *) ExAllocatePoolWithTag(NonPagedPool, 
                                                  numFound * sizeof(ULONG),
                                                  SDP_TAG);
        *ppResultStream = pResult;

        if (*ppResultStream == NULL) {
            SdpPrint(SDP_DBG_UUID_ERROR,
                     ("could not alloc pool for a ServiceSearch response\n"));

            status = STATUS_INSUFFICIENT_RESOURCES;
            *pSdpError = MapNtStatusToSdpError(status);
        }
        else {
            *pRecordCount = numFound;
            phe = hl.GetHead();

            ASSERT(hl.GetCount() == numFound);

            for (USHORT i = 0; i < numFound; i++, phe = hl.GetNext(phe)) {
                if (local) {
                    pResult[i] = phe->handle;
                }
                else {
                    pResult[i] = RtlUlongByteSwap(phe->handle);
                }
            }
        }
    }

    //
    // clean up
    //
    while (!hl.IsEmpty()) {
        phe = hl.RemoveHead();
        delete phe;
    }

    return status;
}

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
NTSTATUS
SdpDatabase::ServiceSearchRequestResponseCached(
    HANDLE hDeviceCache,
    SdpQueryUuid *pUuid,
    UCHAR numUuid,
    USHORT maxRecordsRequested,
    ULONG **ppResultStream,         // must be freed when xmit is finished
    USHORT *pRecordCount
    )
{
    NTSTATUS status;
    SdpUuidSearch uuidSrch;
    SDP_ERROR sdpError;
    ULONG sizeFull, sizePartial;
    ULONG numValues;
    USHORT numFound;
    PKEY_FULL_INFORMATION pKeyFullInfo;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyPartialInfo;
    PUCHAR pFull, pPartial;


    PAGED_CODE();

    uuidSrch.SetUuids(pUuid, numUuid);

    SdpPrint(SDP_DBG_UUID_TRACE, ("ServiceSearchRequestResponseCached enter\n"));

    *ppResultStream = NULL;
    *pRecordCount = 0;

    numFound = 0;

    status = STATUS_SUCCESS;

    sizeFull = 0;
    status = ZwQueryKey(hDeviceCache, KeyFullInformation, NULL, 0, &sizeFull);
    if (!NT_SUCCESS(status)) {
    }

    pFull = new  (PagedPool) UCHAR[sizeFull];
    pKeyFullInfo = (PKEY_FULL_INFORMATION) pFull;
    if (pKeyFullInfo == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = ZwQueryKey(hDeviceCache, KeyFullInformation, pKeyFullInfo, sizeFull, &sizeFull);
    if (!NT_SUCCESS(status)) {
        delete[] pFull;
        return status;
    }

    // MaxValueDataLen 
    sizePartial = pKeyFullInfo->MaxValueDataLen +
                  sizeof (KEY_VALUE_PARTIAL_INFORMATION);

    pPartial = new (PagedPool) UCHAR[sizePartial];
    pKeyPartialInfo = (PKEY_VALUE_PARTIAL_INFORMATION) pPartial;
                       
    numValues = pKeyFullInfo->Values;
        
    //
    // Iterate over each cached record.  If ALL the UUIDs in the search are found
    // then report the search record back to the requestor
    //
    HandleList hl;
    HandleEntry *phe;

    for (ULONG i = 0; i < numValues; i++) {
        ULONG size;

        status = ZwEnumerateValueKey(hDeviceCache,
                                     i,
                                     KeyValuePartialInformation,
                                     pKeyPartialInfo,
                                     sizePartial,
                                     &size);

        if (NT_SUCCESS(status)) {
            PUCHAR pStream = pKeyPartialInfo->Data;
            ULONG sizeStream = pKeyPartialInfo->DataLength;
            USHORT id;

            status = ValidateStream(pStream, sizeStream, NULL);
            if (!NT_SUCCESS(status)) {
                continue;
            }

            status = SdpVerifyServiceRecord(pStream,
                                            sizeStream,
                                            VERIFY_CHECK_MANDATORY_REMOTE,
                                            &id);
            if (!NT_SUCCESS(status)) {
                continue;
            }

            if (uuidSrch.SearchStream(pStream, sizeStream)) {
                PUCHAR pStreamHandle;
                ULONG sizeHandle;

                status = SdpFindAttributeInStream(pStream, 
                                                  sizeStream,
                                                  SDP_ATTRIB_RECORD_HANDLE,
                                                  &pStreamHandle,
                                                  &sizeHandle);

                if (NT_SUCCESS(status)) {
                    ASSERT(sizeHandle == sizeof(ULONG) + sizeof(UCHAR));

                    pStreamHandle++;
                    sizeHandle--;

                    ULONG recordHandle;

                    RtlRetrieveUlong(&recordHandle, pStreamHandle);
                    recordHandle = RtlUlongByteSwap(recordHandle);

                    SdpPrint(SDP_DBG_UUID_INFO,
                             ("uuid search, FOUND match in cached record %08x\n",
                              recordHandle));

                    phe = new (PagedPool, SDP_TAG) HandleEntry(recordHandle);
                    if (phe) {
                        hl.AddTail(phe);
                        numFound++;
                    }

                    //
                    // Found as many as the requestor asked for
                    //
                    if (numFound == maxRecordsRequested) {
                        break;
                    }
                }

            }
        }
    }

    if (numFound > 0) {
        PULONG pResult;

        //
        // TODO:  I can optimize this by just returning the linked list and then
        //      pulling values off of that instead of allocating pool again here
        //
        pResult = (ULONG *) ExAllocatePoolWithTag(NonPagedPool, 
                                                  numFound * sizeof(ULONG),
                                                  SDP_TAG);
        *ppResultStream = pResult;

        if (*ppResultStream == NULL) {
            SdpPrint(SDP_DBG_UUID_ERROR,
                     ("could not alloc pool for a cached ServiceSearch response\n"));

            status = STATUS_INSUFFICIENT_RESOURCES;
        }
        else {
            *pRecordCount = numFound;
            phe = hl.GetHead();

            ASSERT(hl.GetCount() == numFound);

            for (USHORT i = 0; i < numFound; i++, phe = hl.GetNext(phe)) {
                pResult[i] = phe->handle;
            }
        }
    }

    //
    // clean up
    //
    while (!hl.IsEmpty()) {
        phe = hl.RemoveHead();
        delete phe;
    }

    if (pFull) {
        delete[] pFull;
        pFull = NULL;
        pKeyFullInfo = NULL;
    }
    if (pPartial) {
        delete[] pPartial;
        pPartial = NULL;
        pKeyPartialInfo = NULL;
    }

    return status;
}
#endif  // UNDER_CE

struct SdpAttribSearch {
    SdpAttribSearch() { ZERO_THIS(); }

    ~SdpAttribSearch()
    {
        if (pRange && ownRange) {
            delete[] pRange;
            pRange = NULL;
        }
    }

    void SetAttributes(SdpAttributeRange *pOrigRange, ULONG numRange)
    {
        ownRange = FALSE;
        pRange = pOrigRange;
        numAttributes = numRange;
    }

    NTSTATUS RetrieveAttributes(PUCHAR pStream, ULONG streamSize);

    NTSTATUS FindElements(PUCHAR pStream,
                          ULONG streamSize,
                          PSDP_STREAM_ENTRY *ppEntry,
                          PSDP_ERROR pSdpError)
    {
        PAGED_CODE();

        return SdpFindAttributeSequenceInStream(
                pStream,
                streamSize,
                pRange,
                numAttributes,
                ppEntry,
                pSdpError);
    }

    static NTSTATUS CountAttributesInSearchStreamWalk(
        SdpAttribSearch * pAttribSrch,
        UCHAR DataType,
        ULONG DataSize,
        PUCHAR Data,
        ULONG DataStorageSize);

    static NTSTATUS RetrieveAttributesInSearchStreamWalk(
        SdpAttribSearch *pAttribSrch,
        UCHAR DataType,
        ULONG DataSize,
        PUCHAR Data,
        ULONG DataStorageSize);

    SdpAttributeRange *pRange;

    ULONG numAttributes;
    ULONG currentAttribute;
    USHORT minAttrib;

    BOOLEAN ownRange;
};

NTSTATUS SdpAttribSearch::RetrieveAttributes(PUCHAR pStream, ULONG streamSize)
{
    PAGED_CODE();

    NTSTATUS status;

    status = SdpVerifySequenceOf(
        pStream,
        streamSize,
        SDP_TYPE_UINT,
        NULL,
        NULL,
        (PSDP_STREAM_WALK_FUNC) CountAttributesInSearchStreamWalk,
        (PVOID) this);

    if (!NT_SUCCESS(status)) {
        return status;
    }
    else if (numAttributes == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    ownRange = TRUE;

    







#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
    pRange = new (NonPagedPool, 'ApdS') SdpAttributeRange[numAttributes];
#else
    pRange = new SdpAttributeRange[numAttributes];
    if (pRange)
        memset(pRange,0,sizeof(SdpAttributeRange)*numAttributes);
#endif
    if (pRange == NULL) {
        SdpPrint(SDP_DBG_ATTRIB_ERROR, ("could not alloc pool for a range list\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return SdpVerifySequenceOf(
        pStream,
        streamSize,
        SDP_TYPE_UINT,
        NULL,
        NULL,
        (PSDP_STREAM_WALK_FUNC) RetrieveAttributesInSearchStreamWalk,
        (PVOID) this);
}

NTSTATUS 
SdpAttribSearch::CountAttributesInSearchStreamWalk(
    SdpAttribSearch *pAttribSrch,
    UCHAR DataType,
    ULONG DataSize,
    PUCHAR Data,
    ULONG DataStorageSize
    )
{
    PAGED_CODE();

    if (DataSize == sizeof(USHORT)) {
        USHORT uint16;

        RtlRetrieveUshort(&uint16, Data);
        uint16 = RtlUshortByteSwap(uint16);

        if (pAttribSrch->numAttributes != 0 && uint16 <= pAttribSrch->minAttrib) {
            SdpPrint(SDP_DBG_ATTRIB_ERROR, ("out of order attrib search (single attrib)\n"));
            return STATUS_INVALID_PARAMETER;
        }

        pAttribSrch->minAttrib = uint16;
        pAttribSrch->numAttributes++;
    }
    else if (DataSize == sizeof(ULONG)) {
        ULONG uint32;
        USHORT minAttrib, maxAttrib;

        RtlRetrieveUlong(&uint32, Data);
        uint32 = RtlUlongByteSwap(uint32);

        minAttrib = (USHORT) ((uint32 & 0xFFFF0000) >> 16);
        maxAttrib = (USHORT) (uint32 & 0xFFFF);

        if (maxAttrib <= minAttrib) {
            //
            // max is less than min???
            //
            SdpPrint(SDP_DBG_ATTRIB_ERROR, ("out of order attrib search (range attrib)\n"));
            return STATUS_INVALID_PARAMETER;
        }
        else if (pAttribSrch->numAttributes != 0 &&
                 minAttrib <= pAttribSrch->minAttrib) {
            //
            // ranges are overlapping
            //
            SdpPrint(SDP_DBG_ATTRIB_ERROR, ("overlapping attrib search\n"));
            return STATUS_INVALID_PARAMETER;
        }

        pAttribSrch->minAttrib = maxAttrib;
        pAttribSrch->numAttributes++;
    }
    else {
        SdpPrint(SDP_DBG_ATTRIB_ERROR, ("wrong size int (%d)\n", DataSize));
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
SdpAttribSearch::RetrieveAttributesInSearchStreamWalk(
    SdpAttribSearch *pAttribSrch,
    UCHAR DataType,
    ULONG DataSize,
    PUCHAR Data,
    ULONG DataStorageSize
    )
{
    PAGED_CODE();

	if (DataType != SDP_TYPE_UINT) {
		return STATUS_SUCCESS;
	}

    SdpAttributeRange * pAttr =
        pAttribSrch->pRange + pAttribSrch->currentAttribute;

    if (DataSize == sizeof(USHORT)) {
        USHORT uint16;

        RtlRetrieveUshort(&uint16, Data);
        pAttr->minAttribute = pAttr->maxAttribute = RtlUshortByteSwap(uint16);
    }
    else {
        ULONG uint32;

        RtlRetrieveUlong(&uint32, Data);
        uint32 = RtlUlongByteSwap(uint32);

        pAttr->minAttribute = (USHORT) ((uint32 & 0xFFFF0000) >> 16);
        pAttr->maxAttribute = (USHORT) (uint32 & 0xFFFF);
    }

    pAttribSrch->currentAttribute++;

    return STATUS_SUCCESS;
}


NTSTATUS 
SdpDatabase::ServiceAttributeRequestResponseRemote(
    IN ULONG serviceHandle,
    IN UCHAR *pAttribIdListStream,
    IN ULONG streamSize,
    OUT UCHAR **ppAttributeListStream,
    OUT ULONG *pAttributeListSize,
    OUT PVOID * ppOriginalAlloc,
    OUT PSDP_ERROR pSdpError
    )
{
    SdpAttribSearch attribSrch;
    NTSTATUS status;

    PAGED_CODE();

    status = attribSrch.RetrieveAttributes(pAttribIdListStream, streamSize);
    if (!NT_SUCCESS(status)) {
        *pSdpError = MapNtStatusToSdpError(status);
        return status;
    }

    return ServiceAttributeRequestResponse(serviceHandle,
                                           &attribSrch,
                                           ppAttributeListStream,
                                           pAttributeListSize,
                                           ppOriginalAlloc,
                                           pSdpError);
}

NTSTATUS 
SdpDatabase::ServiceAttributeRequestResponseLocal(
    IN ULONG serviceHandle,
    IN SdpAttributeRange *pRange,
    IN ULONG numRange,
    OUT UCHAR **ppAttributeListStream,
    OUT ULONG *pAttributeListSize,
    OUT PVOID * ppOriginalAlloc
    )
{
    SdpAttribSearch attribSrch;
    SDP_ERROR sdpError;
    
    attribSrch.SetAttributes(pRange, numRange);

    return ServiceAttributeRequestResponse(serviceHandle,
                                           &attribSrch,
                                           ppAttributeListStream,
                                           pAttributeListSize,
                                           ppOriginalAlloc,
                                           &sdpError);
}

NTSTATUS 
SdpDatabase::ServiceAttributeRequestResponse(
    IN ULONG serviceHandle,
    SdpAttribSearch *pAttribSearch,
    OUT UCHAR **ppAttributeListStream,
    OUT ULONG *pAttributeListSize,
    OUT PVOID * ppOriginalAlloc,
    OUT PSDP_ERROR pSdpError
    )
{
    ServiceRecord *pRecord;
    PSDP_STREAM_ENTRY pStreamEntry;
    NTSTATUS status = STATUS_SUCCESS;

    BOOLEAN foundRecord = FALSE;

    pStreamEntry = NULL;
    m_ServiceRecordList.Lock();

    status = STATUS_UNSUCCESSFUL;

    for (pRecord = m_ServiceRecordList.GetHead();
         pRecord != NULL;
         pRecord = m_ServiceRecordList.GetNext(pRecord)) {

        if (pRecord->recordHandle == serviceHandle) {
            _DbgPrintF(DBG_SDP_INFO,
                       ("AttributeRequest found record handle 0x%08x", serviceHandle));

            foundRecord = TRUE;

            status = pAttribSearch->FindElements(
                pRecord->pStream,
                pRecord->streamSize,
                &pStreamEntry,
                pSdpError);

            //
            // FindElements will return success even if there was no match.  If
            // it returns an error, than that means there was either an invalid
            // search or there was a problem allocating pool
            //
            if (!NT_SUCCESS(status)) {
                _DbgPrintF(DBG_SDP_WARNING, ("search was not successful, 0x%x", status));
                *pSdpError = MapNtStatusToSdpError(status);
            }
            break;
        }
    }

    m_ServiceRecordList.Unlock();

    if (foundRecord == TRUE) {
        if (!NT_SUCCESS(status)) {

            _DbgPrintF(DBG_SDP_WARNING, ("search was successful, but an error occurred, 0x%x", status));
            if (pStreamEntry) {
                ExFreePool(pStreamEntry);
                pStreamEntry = NULL;
            }

            *ppAttributeListStream = NULL;
            *pAttributeListSize = 0x0;
        }
        else if (pStreamEntry != NULL){
            //
            // there are search results
            //
            _DbgPrintF(DBG_SDP_INFO,
                       ("search was successful, result is %d long", pStreamEntry->streamSize));

            *ppAttributeListStream = pStreamEntry->stream;
            *pAttributeListSize = pStreamEntry->streamSize;
            *ppOriginalAlloc = pStreamEntry;
        }
        else {
            //
            // We didn't find anything so we need to create an empty sequence
            // and send it back
            //
            _DbgPrintF(DBG_SDP_INFO, ("search was successful, no match though"));

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
            UCHAR *pStream = new (PagedPool, SDP_TAG) UCHAR[2];
#else
            UCHAR *pStream = (UCHAR*) ExAllocatePoolWithTag(0,2*sizeof(UCHAR),0);
            if (pStream)
                memset(pStream,0,sizeof(UCHAR[2]));
#endif
            if (*ppAttributeListStream) {
                *ppOriginalAlloc = pStream; 
                *ppAttributeListStream = pStream;
                *pAttributeListSize = 2;

                WriteVariableSizeToStream(SDP_TYPE_SEQUENCE, 0, pStream);

                ASSERT(NT_SUCCESS(status));
            }
            else {
                status = STATUS_INSUFFICIENT_RESOURCES;
                *pSdpError = MapNtStatusToSdpError(status);
            }
        }
    }
    else {
        _DbgPrintF(DBG_SDP_INFO,
                   ("did not find the specified record handle (0x%08x) in the DB",
                    serviceHandle));
        *pSdpError = SDP_ERROR_INVALID_RECORD_HANDLE;
        status = STATUS_UNSUCCESSFUL;
    }

    return status;
}

NTSTATUS 
SdpDatabase::ServiceSearchAttributeResponseLocal(
    IN SdpQueryUuid *pUuid,
    IN UCHAR numUuid,
    IN SdpAttributeRange *pRange,
    IN ULONG numRange,
    OUT UCHAR **ppAttributeLists,           
    OUT ULONG *pAttributeListsByteCount
    )
{
    SdpUuidSearch uuidSrch;
    SdpAttribSearch attribSrch;
    SDP_ERROR sdpError;

    uuidSrch.SetUuids(pUuid, numUuid);
    attribSrch.SetAttributes(pRange, numRange);

    return ServiceSearchAttributeResponse(&uuidSrch,
                                          &attribSrch,
                                          ppAttributeLists,
                                          pAttributeListsByteCount,
                                          &sdpError,
                                          TRUE);
}

NTSTATUS
SdpDatabase::ServiceSearchAttributeResponseRemote(
    IN UCHAR *pServiceSearchStream,
    IN ULONG serviceSearchStreamSize,
    IN UCHAR *pAttributeIdStream,
    IN ULONG attributeIdStreamSize,
    OUT UCHAR **ppAttributeLists,
    OUT ULONG *pAttributeListsByteCount,
    OUT PSDP_ERROR pSdpError
    )
{
    SdpUuidSearch uuidSrch;
    SdpAttribSearch attribSrch;

    NTSTATUS status;

    status = uuidSrch.RetrieveUuids(pServiceSearchStream, serviceSearchStreamSize);
    if (!NT_SUCCESS(status)) {
        *pSdpError = MapNtStatusToSdpError(status);
        return status;
    }

    status = attribSrch.RetrieveAttributes(pAttributeIdStream, attributeIdStreamSize);
    if (!NT_SUCCESS(status)) {
        *pSdpError = MapNtStatusToSdpError(status);
        return status;
    }

    return ServiceSearchAttributeResponse(&uuidSrch,
                                          &attribSrch,
                                          ppAttributeLists,
                                          pAttributeListsByteCount,
                                          pSdpError,
                                          FALSE);
}

NTSTATUS
SdpDatabase::ServiceSearchAttributeResponse(
    IN SdpUuidSearch *pUuidSearch,
    IN SdpAttribSearch *pAttribSearch,
    OUT UCHAR **ppAttributeLists,           // must be freed when xmit is finished
    OUT ULONG *pAttributeListsByteCount,
    OUT PSDP_ERROR pSdpError,
    IN BOOLEAN Local
    )
{
    PAGED_CODE();

    NTSTATUS status = STATUS_SUCCESS;
    LIST_ENTRY head;

    PLIST_ENTRY pListEntry;
    PSDP_STREAM_ENTRY pStreamEntry;

    *ppAttributeLists = NULL;
    *pAttributeListsByteCount = 0;

    InitializeListHead(&head);
    ServiceRecord *pRecord;
    ULONG streamSize = 0;

    m_ServiceRecordList.Lock();

    for (pRecord = m_ServiceRecordList.GetHead();
         pRecord != NULL;
         pRecord = m_ServiceRecordList.GetNext(pRecord)) {

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
        if (Local == FALSE &&
            pRecord->fOptions & SERVICE_OPTION_DO_NOT_PUBLISH) {
            //
            // Record sumbitter did not want this SDP record published to the 
            // world.  Skip it.
            //
            continue;
        }
#endif

        if (pUuidSearch->SearchStream(pRecord->pStream, pRecord->streamSize)) {
            pStreamEntry = NULL;

            status = pAttribSearch->FindElements(
                pRecord->pStream,
                pRecord->streamSize,
                &pStreamEntry,
                pSdpError);

            if (NT_SUCCESS(status)) {
                if (pStreamEntry != NULL) {
                    //
                    // pStreamEntry will be freed when we exit this function
                    //
                    InsertTailList(&head, &pStreamEntry->link);
                    streamSize += pStreamEntry->streamSize;
                }
                else {
                    //
                    // success will be returned even if there is no match
                    //
                    ;
                }
            }
            else {
                //
                // if there was an error allocating pool or there was something
                // incorrectly formatted, we will drop into here
                //
                break;
            }
        }
    }

    m_ServiceRecordList.Unlock();

    if (NT_SUCCESS(status)) {
        ULONG totalSize;

        //
        // compute the total amout of stream required
        //
        totalSize = streamSize + (USHORT) GetContainerHeaderSize(streamSize);

        




#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
        UCHAR *pStream = new(NonPagedPool, SDP_TAG) UCHAR[totalSize];
#else
        UCHAR *pStream = (PUCHAR) ExAllocatePoolWithTag(0,sizeof(UCHAR)*totalSize,0);
        if (pStream)
            memset(pStream,0,sizeof(UCHAR)*totalSize);
#endif

        if (pStream) {
            *ppAttributeLists = pStream;
            *pAttributeListsByteCount = totalSize;

            pStream  = WriteVariableSizeToStream(SDP_TYPE_SEQUENCE, streamSize, pStream);

            ASSERT(streamSize == 0 || !IsListEmpty(&head));

            for (pListEntry = head.Flink;
                 pListEntry != &head;
                 pListEntry = pListEntry->Flink) {

                pStreamEntry =
                    CONTAINING_RECORD(pListEntry, SDP_STREAM_ENTRY, link);

                RtlCopyMemory(pStream,
                              pStreamEntry->stream,
                              pStreamEntry->streamSize);

                pStream += pStreamEntry->streamSize;
            }

            ASSERT((ULONG) (pStream - *ppAttributeLists) == totalSize);
        }
        else {
            status = STATUS_INSUFFICIENT_RESOURCES;
            *pSdpError = MapNtStatusToSdpError(status);
        }
    }
    else {
        *pSdpError = MapNtStatusToSdpError(status);
    }

    while (!IsListEmpty(&head)) {
        pListEntry = RemoveHeadList(&head);
        ExFreePool( CONTAINING_RECORD(pListEntry, SDP_STREAM_ENTRY, link) );
    }

    return status;
}
