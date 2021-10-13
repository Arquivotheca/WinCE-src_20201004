//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "pch.h"
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#include "bt_100.h"

#pragma code_seg("PAGE", "CODE")
#endif

UCHAR IdxToSize[] = {
    sizeof(UCHAR),
    sizeof(USHORT),
    sizeof(ULONG),
    sizeof(ULONGLONG),
    sizeof(ULONGLONG) * 2
};

#define INC_STREAM(_inc)        \
{                               \
    pStream += (_inc);           \
    size -= (_inc);             \
}

NTSTATUS
SdpVerifySequenceOf(
    UCHAR *pStream,
    ULONG size,
    UCHAR ofType,
    UCHAR *pSpecSizes,
    ULONG *pNumFound,
    PSDP_STREAM_WALK_FUNC pFunc,
    PVOID pContext
    )
{

    UCHAR type, sizeIndex;

    SdpRetrieveHeader(pStream, type, sizeIndex);

    if (type != SDP_TYPE_SEQUENCE) {
        return STATUS_INVALID_PARAMETER;
    }

    INC_STREAM(1);

    ULONG elementSize, storageSize;
    SdpRetrieveVariableSize(pStream, sizeIndex, &elementSize, &storageSize);

    INC_STREAM(storageSize);
    
    //
    // Make sure the top level seqeunce is the only element in the stream
    //
    if (size != elementSize) {
        return STATUS_INVALID_PARAMETER;
    }

    while (size) {
        SdpRetrieveHeader(pStream, type, sizeIndex);

        if (type != ofType) {
            return STATUS_INVALID_PARAMETER;
        }         

        INC_STREAM(1);

        if (pSpecSizes) {
            BOOLEAN matched = FALSE;

            for (ULONG idx = 0; pSpecSizes[idx] != 0x00; idx++) {
                if (IdxToSize[sizeIndex] == pSpecSizes[idx]) {
                    matched = TRUE;
                    break;
                }
            }

            if (matched == FALSE) {
                return STATUS_INVALID_PARAMETER;
            }
        }

        if (ARGUMENT_PRESENT(pNumFound)) {
            (*pNumFound)++;
        }

        ULONG dataSize;
        switch (sizeIndex) {
        case 0: 
        case 1: 
        case 2: 
        case 3:
        case 4:
            dataSize = IdxToSize[sizeIndex];
            break;

        default:
            SdpRetrieveVariableSize(pStream, sizeIndex, &dataSize, &storageSize);
            INC_STREAM(storageSize);
            break;
        }

        if (pFunc != NULL) {
            NTSTATUS status = pFunc(pContext,
                                    type,
                                    dataSize,
                                    pStream,
                                    0);

            if (!NT_SUCCESS(status) && status != STATUS_REPARSE_POINT_NOT_RESOLVED) {
                return status;
            }
        }

        INC_STREAM(dataSize);
    }
    
    return STATUS_SUCCESS;
}

typedef NTSTATUS (*PFNVERIFY)(PUCHAR pStream, ULONG size, ULONG flags);

struct ProtocolListInfo {
    ProtocolListInfo(ULONG f) : firstSequence(TRUE), havePsm(FALSE), psmFromUuid(FALSE), flags(f) {}

    ULONG flags;
    BOOLEAN firstSequence;
    BOOLEAN psmFromUuid;
    BOOLEAN havePsm;
};

NTSTATUS
WalkProtocolSequence(
    ProtocolListInfo *pInfo, 
    UCHAR DataType, 
    ULONG DataSize,
    PUCHAR Data,
    ULONG DataStorageSize
    )
{
    UCHAR type, sizeIndex;

    //
    // must have content
    //
    if (DataSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    SdpRetrieveHeader(Data, type, sizeIndex);

    //
    // first element must be a UUID
    //
    if (type != SDP_TYPE_UUID) {
        return STATUS_INVALID_PARAMETER;
    }

    Data += 1;
    DataSize -= 1;

    GUID uuid;
    SdpRetrieveUuidFromStream(Data, IdxToSize[sizeIndex], &uuid, FALSE);

    DataSize -= IdxToSize[sizeIndex];

    //
    // Perform this check for local services only b/c who knows what is on
    // the remote device.
    //
    if (pInfo->flags & VERIFY_CHECK_MANDATORY_LOCAL) {
        if (pInfo->firstSequence) {
            //
            // ISSUE:  right now every profile specifies L2CAP as the lowest layer
            //         which means that if any profile comes along that doesn't specify
            //         L2CAP, this needs to be changed....but I don't see that 
            //         happening anytime soon and all we implement is l2cap anyways
            if (!IsEqualUuid(&uuid, &L2CAP_PROTOCOL_UUID)) {
                //
                // first protocol must L2CAP
                //
                return STATUS_INVALID_PARAMETER;
            }
    
            //
            // no PSM in the record, see if we can determine it from a well known
            // protocol on top of L2CAP
            //
            if (DataSize == 0) {
                pInfo->psmFromUuid = TRUE;
            }

            pInfo->firstSequence = FALSE;
        }
        else if (pInfo->psmFromUuid && pInfo->havePsm == FALSE) {
            if (IsEqualUuid(&uuid, &SDP_PROTOCOL_UUID)     ||
                IsEqualUuid(&uuid, &RFCOMM_PROTOCOL_UUID)  ||
                IsEqualUuid(&uuid, &TCSBIN_PROTOCOL_UUID)) {
                pInfo->havePsm = TRUE;
            }
            else {
                return STATUS_INVALID_PARAMETER;
            }
        }
    }

    //
    // We only know how to look at L2CAP and RFCOMM optinal elements
    //
    if (DataSize) {
        Data += IdxToSize[sizeIndex];

        SdpRetrieveHeader(Data, type, sizeIndex);

        if (IsEqualUuid(&uuid, &L2CAP_PROTOCOL_UUID)) {
            //
            // PSM
            //
            if (type != SDP_TYPE_UINT || IdxToSize[sizeIndex] != sizeof(USHORT)) {
                return STATUS_INVALID_PARAMETER;
            }
            pInfo->havePsm = TRUE;
            ASSERT(pInfo->psmFromUuid == FALSE);
        }
        else if (IsEqualUuid(&uuid, &RFCOMM_PROTOCOL_UUID)) {
            //
            // Channel number
            //
            if (type != SDP_TYPE_UINT || IdxToSize[sizeIndex] != sizeof(UCHAR)) {
                return STATUS_INVALID_PARAMETER;
            }
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS VerifyProtocolList(PUCHAR pStream, ULONG size, ULONG flags)
{
    UCHAR type, sizeIndex;
    ULONG numFound;
    NTSTATUS status;

    SdpRetrieveHeader(pStream, type, sizeIndex);

    if (type == SDP_TYPE_SEQUENCE) {
        ProtocolListInfo pli(flags);

        numFound = 0;
        status = SdpVerifySequenceOf(pStream,
                                     size,
                                     SDP_TYPE_SEQUENCE,
                                     NULL,
                                     &numFound,
                                     (PSDP_STREAM_WALK_FUNC) WalkProtocolSequence,
                                     &pli);
        if (NT_SUCCESS(status) &&
            (numFound == 0 ||
             (pli.havePsm == FALSE && (flags & VERIFY_CHECK_MANDATORY_LOCAL)))) {
            return STATUS_INVALID_PARAMETER;
        }
        
        return status;
    }
    else if (type == SDP_TYPE_ALTERNATIVE) {
        //
        // iterate over the list, making sure each one is a well formed sequence
        // of sequences
        //
        ULONG elementSize, storageSize;
    
        INC_STREAM(1);
        SdpRetrieveVariableSize(pStream, sizeIndex, &elementSize, &storageSize);
    
        //
        // must have content
        //
        if (elementSize == 0) {
            return STATUS_INVALID_PARAMETER;
        }

        INC_STREAM(storageSize);

        //
        // must be the only element in the stream
        //
        if (size != elementSize) {
            return STATUS_INVALID_PARAMETER;
        }

        while (size) {
            SdpRetrieveHeader(pStream, type, sizeIndex);

            if (type != SDP_TYPE_SEQUENCE) {
                return STATUS_INVALID_PARAMETER;
            }

            PUCHAR tmpStream = pStream;

            INC_STREAM(1);

            SdpRetrieveVariableSize(pStream, sizeIndex, &elementSize, &storageSize);

            ProtocolListInfo pli(flags);

            numFound = 0;
            status =
                SdpVerifySequenceOf(tmpStream,
                                    elementSize + storageSize + 1, // 1 == elem header
                                    SDP_TYPE_SEQUENCE,
                                    NULL,
                                    &numFound,
                                    (PSDP_STREAM_WALK_FUNC) WalkProtocolSequence,
                                    &pli);

            if (!NT_SUCCESS(status)) {
                return status;
            }
            else if (numFound == 0 ||
                     (pli.havePsm == FALSE && (flags & VERIFY_CHECK_MANDATORY_LOCAL))) {
                return STATUS_INVALID_PARAMETER;
            }

            INC_STREAM(storageSize);
            INC_STREAM(elementSize);
        }

        return STATUS_SUCCESS;
    }
    else {
        return STATUS_INVALID_PARAMETER;
    }
}

NTSTATUS VerifyLangIdList(PUCHAR pStream, ULONG size, ULONG flags)
{
    UCHAR sizes[] = { sizeof(USHORT), 0x00 };
    ULONG numFound = 0;
    NTSTATUS status;

    status = SdpVerifySequenceOf(pStream,
                                 size,
                                 SDP_TYPE_UINT, 
                                 sizes,
                                 &numFound,
                                 NULL,
                                 NULL);

    if (NT_SUCCESS(status) && ( (numFound == 0) || (numFound % 3 != 0) ) ) {
        status = STATUS_INVALID_PARAMETER; 
    }

    return status;
}

NTSTATUS
WalkProfileDescriptor(
    PVOID pContext,
    UCHAR DataType, 
    ULONG DataSize,
    PUCHAR Data,
    ULONG DataStorageSize
    )
{
    UCHAR type, sizeIndex;

    //
    // must have content
    //
    if (DataSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    SdpRetrieveHeader(Data, type, sizeIndex);

    Data += 1;
    DataSize -= 1;

    if (type != SDP_TYPE_UUID) {
        return STATUS_INVALID_PARAMETER;
    }

    Data += IdxToSize[sizeIndex];
    DataSize -= IdxToSize[sizeIndex];

    if (DataSize == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    SdpRetrieveHeader(Data, type, sizeIndex);
    
    Data += 1;
    DataSize -= 1;

    if (type != SDP_TYPE_UINT ||
        IdxToSize[sizeIndex] != sizeof(USHORT) ) {
        return STATUS_INVALID_PARAMETER;
    }

    Data += sizeof(USHORT);
    DataSize -= sizeof(USHORT);

    if (DataSize != 0) {
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

NTSTATUS VerifyProfileDescriptor(PUCHAR pStream, ULONG size, ULONG flags)
{
    NTSTATUS status;
    ULONG numFound = 0;

    status = SdpVerifySequenceOf(pStream,
                                 size,
                                 SDP_TYPE_SEQUENCE,
                                 NULL,
                                 &numFound,
                                 WalkProfileDescriptor,
                                 NULL);

    if (NT_SUCCESS(status) && numFound == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    return status;
}

#define VADF_REQ_LOCAL   0x01
#define VADF_REQ_REMOTE  0x02
#define VADF_SEQ         0x10

#define VADF_REQ_ALL     (VADF_REQ_LOCAL | VADF_REQ_REMOTE)

struct VerifyAttibuteData {
    USHORT attribute;
    USHORT flags;
    UCHAR  attributeType;
    UCHAR *pAttributeSizes;

    //
    // has precedence over any flags
    //
    PFNVERIFY pfnVerify;    
};

UCHAR HandleSizes[] =      { sizeof(ULONG),  0x00 };
UCHAR RecordStateSizes[] = { sizeof(ULONG),  0x00 };
UCHAR LangIdSizes[] =      { sizeof(USHORT), 0x00 };
UCHAR TimeToLiveSizes[] =  { sizeof(ULONG),  0x00 };
UCHAR AvailabilitySizes[] ={ sizeof(UCHAR),  0x00 };

VerifyAttibuteData VerData[] = {
#if 0
    { SDP_ATTRIB_RECORD_HANDLE,    VADF_REQ_ALL,                    SDP_TYPE_UINT, HandleSizes, NULL},
#else
    { SDP_ATTRIB_RECORD_HANDLE,    VADF_REQ_REMOTE,                 SDP_TYPE_UINT, HandleSizes, NULL},
#endif
    { SDP_ATTRIB_CLASS_ID_LIST,    VADF_REQ_ALL | VADF_SEQ,         SDP_TYPE_UUID, NULL, NULL},
    { SDP_ATTRIB_RECORD_STATE,             0x00,                    SDP_TYPE_UINT, RecordStateSizes, NULL},
    { SDP_ATTRIB_SERVICE_ID,               0x00,                    SDP_TYPE_UUID, NULL, NULL},
#if 0
    { SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, VADF_REQ_LOCAL,          0x00,          NULL, VerifyProtocolList},
#else
    { SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, 0x00,                    0x00,          NULL, VerifyProtocolList},
#endif
    { SDP_ATTRIB_BROWSE_GROUP_LIST,        VADF_SEQ,                SDP_TYPE_UUID, NULL, NULL},
    { SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST, 0x00,                    SDP_TYPE_UINT, LangIdSizes, VerifyLangIdList},
    { SDP_ATTRIB_INFO_TIME_TO_LIVE,        0x00,                    SDP_TYPE_UINT, TimeToLiveSizes, NULL},
    { SDP_ATTRIB_AVAILABILITY,             0x00,                    SDP_TYPE_UINT, AvailabilitySizes, NULL},
    { SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST,  0x00,                    0x00,          NULL, VerifyProfileDescriptor},
    { SDP_ATTRIB_DOCUMENTATION_URL,        0x00,                    SDP_TYPE_URL,  NULL, NULL},
    { SDP_ATTRIB_CLIENT_EXECUTABLE_URL,    0x00,                    SDP_TYPE_URL,  NULL, NULL},
    { SDP_ATTRIB_ICON_URL,                 0x00,                    SDP_TYPE_URL,  NULL, NULL},
};

ULONG VerDataMax = sizeof(VerData) / sizeof(VerData[0]);

NTSTATUS
SdpVerifyServiceRecord(
    UCHAR *pStream,
    ULONG size,
    ULONG flags,
    USHORT *pAttribId
    )
{
    NTSTATUS status = STATUS_NOT_FOUND;
    ULONG attributeStreamSize;
//  USHORT particularAttribute;
    UCHAR *pAttributeStream;
    
    //
    // If the stream is the attribute value itself, then make sure the single
    // attribute flag is set as well
    //
    if ((flags & VERIFY_STREAM_IS_ATTRIBUTE_VALUE)) {
        flags |= VERIFY_SINGLE_ATTRIBUTE;
    }

    for (ULONG idx = 0; idx < VerDataMax; idx++) {
        if ((flags & VERIFY_SINGLE_ATTRIBUTE) &&
            *pAttribId != VerData[idx].attribute) {
            continue;
        }

        if (flags & VERIFY_STREAM_IS_ATTRIBUTE_VALUE) {
            status = STATUS_SUCCESS;
            pAttributeStream = pStream;
            attributeStreamSize = size;
        }
        else {
            status = SdpFindAttributeInStream(pStream,
                                              size,
                                              VerData[idx].attribute,
                                              &pAttributeStream,
                                              &attributeStreamSize);
        }

        //
        // See if we have successfully found a stream which the attribute value
        //
        if (NT_SUCCESS(status)) {
            if (VerData[idx].pfnVerify != NULL) {
                //
                // Custom verifier, just call it and let it handle everything
                //
                status = VerData[idx].pfnVerify(pAttributeStream,
                                                attributeStreamSize,
                                                flags);
            }
            else if (VerData[idx].flags & VADF_SEQ) {
                ULONG numFound = 0;

                //
                // It is a sequence which is homogeneous.  It is assumed that
                // the sequence cannot be empty.
                //
                status = SdpVerifySequenceOf(pAttributeStream,
                                             attributeStreamSize,
                                             VerData[idx].attributeType,
                                             VerData[idx].pAttributeSizes,
                                             &numFound,
                                             NULL,
                                             NULL);

                if (NT_SUCCESS(status) && numFound == 0) {
                    status = STATUS_INVALID_PARAMETER;
                }
            }
            else {
                //
                // Singular type with 0 - N possible specific sizes
                //
                UCHAR type, sizeIndex;

                SdpRetrieveHeader(pAttributeStream, type, sizeIndex);

                if (type != VerData[idx].attributeType) {
                    status = STATUS_INVALID_PARAMETER;
                }
                else if (VerData[idx].pAttributeSizes) {
                    BOOLEAN matched = FALSE;

                    for (ULONG sti = 0;
                         VerData[idx].pAttributeSizes[sti] != 0x00;
                         sti++) {
                        if (IdxToSize[sizeIndex] ==
                            VerData[idx].pAttributeSizes[sti]) {
                            matched = TRUE;
                            break;
                        }
                    }

                    if (matched == FALSE) {
                        status = STATUS_INVALID_PARAMETER;
                    }
                }
                else {
                    // All sizes are valid
                    ;
                }
            }
        }
        else if (  (flags & VERIFY_SINGLE_ATTRIBUTE)            ||
                  ((flags & VERIFY_CHECK_MANDATORY_LOCAL)  &&
                   (VerData[idx].flags & VADF_REQ_LOCAL))       || 
                  ((flags & VERIFY_CHECK_MANDATORY_REMOTE) &&
                   (VerData[idx].flags & VADF_REQ_REMOTE)) ) {
            //
            // One of these have, and the attribute was not present 
            // 1  Caller has asked to verify one attribute 
            // 2  Caller wants all mandatory local attributes verified
            // 3  Caller wants all remote local attributes verified
            //
            status = STATUS_NOT_FOUND;
        }
        else {
            //
            // The attribute was not found, but it is not required, so just 
            // consider it a success
            //
            status = STATUS_SUCCESS;
        }

        //
        // Break out of the loop if the check failed or if we are only verifying
        // one attribute
        //
        if (!NT_SUCCESS(status) || (flags & VERIFY_SINGLE_ATTRIBUTE)) {
            break;
        }
    }

    if (!NT_SUCCESS(status) && idx < VerDataMax) {
        *pAttribId = VerData[idx].attribute;
    }

    return status;
}


NTSTATUS ValidateProtocolSequence(PSDP_NODE pOuterSeq, PPSM_LIST pPsmList)
{
    PPSM_PROTOCOL_PAIR pppp;
    PSDP_NODE   pSeqProt, pUuid, pParam;
    PLIST_ENTRY entry;
    GUID        uuid;
    BOOLEAN     psmFromUuid = FALSE,
                noL2Cap = FALSE;

    pppp = pPsmList->list + pPsmList->count;

    //
    // Get the first element in the sequence, which is itself a sequence
    // (which contains a UUID and optional elements)
    //
    entry = pOuterSeq->u.sequence.Link.Flink;
    pSeqProt = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);
    ASSERT(pSeqProt->hdr.Type == SDP_TYPE_SEQUENCE);

    entry = pSeqProt->u.sequence.Link.Flink; 
    pUuid = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);
    ASSERT(pUuid->hdr.Type == SDP_TYPE_UUID);

    SdpNormalizeUuid(pUuid, &uuid);

    if (IsEqualUuid(&uuid, &L2CAP_PROTOCOL_UUID)) {
        //
        // Check to see if we are at the end of the inner sequence (pSeqProt)
        //
        if (pUuid->hdr.Link.Flink == &pSeqProt->u.sequence.Link) {
            //
            // Must rely on the next protocol in the list to indicate the PSM.  
            // This is because all of the currently published profiles do not 
            // require a PSM in their records
            //
            // If this is the last sequence in the outer seq, error
            //
            if (pSeqProt->hdr.Link.Flink == &pOuterSeq->u.sequence.Link) {
                return STATUS_INVALID_PARAMETER;
            }

            psmFromUuid = TRUE;
        }
        else {
            //
            // Got a PSM!
            //
            pParam = CONTAINING_RECORD(pUuid->hdr.Link.Flink, SDP_NODE, hdr.Link);
            ASSERT(pParam->hdr.Type == SDP_TYPE_UINT);
            pppp->psm = pParam->u.uint16;
        }
    }
    else {
        noL2Cap = TRUE; 
    }

    //
    // If we need a psm or there was no l2cap and there aren't any more protocols,
    // then fail it
    //
    // ISSUE: 
    //  1 this needs to be fixed for SCO connections!
    //      (what the hell do SCO SDP records look like?)
    //  2 should SDP records w/out l2cap be failed?  in the 1.0b profile spec, 
    //      each profile requires it.  in the future, will we be bitten by this 
    //      assumption if we enforce it now???
    //
    if ((psmFromUuid || noL2Cap) && 
        pSeqProt->hdr.Link.Flink == &pOuterSeq->u.sequence.Link) {
        return STATUS_INVALID_PARAMETER;
    }

    entry = pSeqProt->hdr.Link.Flink; 
    pSeqProt = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);
    ASSERT(pSeqProt->hdr.Type == SDP_TYPE_SEQUENCE);

    entry = pSeqProt->u.sequence.Link.Flink; 
    pUuid = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);
    ASSERT(pUuid->hdr.Type == SDP_TYPE_UUID);

    SdpNormalizeUuid(pUuid, &uuid);

    if (psmFromUuid) {
        if (IsEqualUuid(&uuid, &RFCOMM_PROTOCOL_UUID)) {
            pppp->psm = PSM_RFCOMM;
        }
        else if (IsEqualUuid(&uuid, &TCSBIN_PROTOCOL_UUID)) {
            pppp->psm = PSM_TCS_BIN;
        }
        else if (IsEqualUuid(&uuid, &SDP_PROTOCOL_UUID)) {
            //
            // we don't allow anyone to submit an SDP record locally
            //
            return STATUS_INVALID_PARAMETER;
        }
        else {
            //
            // Don't know how to map the PSM from the protocol
            //
            return STATUS_INVALID_PARAMETER;
        }
    }

    RtlCopyMemory(&pppp->protocol, &uuid, sizeof(uuid));
    pPsmList->count++;

    return STATUS_SUCCESS;
}

NTSTATUS SdpValidateProtocolContainer(PSDP_NODE pContainer, PPSM_LIST pPsmList)
{
    if (pContainer->hdr.Type == SDP_TYPE_SEQUENCE) {
        return ValidateProtocolSequence(pContainer, pPsmList);
    }
    else {
        NTSTATUS status;
        PLIST_ENTRY entry;
        PSDP_NODE pSeq;

        for (entry = pContainer->u.alternative.Link.Flink;
             entry != &pContainer->u.alternative.Link;
             entry = entry->Flink) {
            pSeq = CONTAINING_RECORD(entry, SDP_NODE, hdr.Link);
            ASSERT(pSeq->hdr.Type == SDP_TYPE_SEQUENCE);
            status = ValidateProtocolSequence(pSeq, pPsmList);
            if (!NT_SUCCESS(status)) {
                return status;
            }
        }

        return STATUS_SUCCESS;
    }
}
