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

void SdpByteSwapUuid128(GUID *uuid128From, GUID *uuid128To)
{
    uuid128To->Data1 = RtlUlongByteSwap(uuid128From->Data1);
    uuid128To->Data2 = RtlUshortByteSwap(uuid128From->Data2);
    uuid128To->Data3 = RtlUshortByteSwap(uuid128From->Data3);
    memcpy(uuid128To->Data4, uuid128From->Data4, sizeof(uuid128From->Data4)); 
}

void SdpByteSwapUint128(PSDP_ULARGE_INTEGER_16 pInUint128, PSDP_ULARGE_INTEGER_16 pOutUint128)
{
    pOutUint128->HighPart = SdpUlonglongByteSwap(pInUint128->HighPart);    
    pOutUint128->LowPart = SdpUlonglongByteSwap(pInUint128->LowPart);    
}

ULONGLONG SdpUlonglongByteSwap(IN ULONGLONG Source)

/*++

Routine Description:

    The SdpUlonglongByteSwap function exchanges byte pairs 0:7, 1:6, 2:5, and
    3:4 of Source and returns the resulting ULONGLONG.

    We do not use RtlUlonglongByteSwap because on Windows 2000, only the first 4
    bytes of the ULONGLONG are based in because of a linker issue
    
Arguments:

    Source - 64-bit value to byteswap.

Return Value:

    Swapped 64-bit value.

--*/
{
    ULONGLONG swapped;

    swapped = ((Source)                      << (8 * 7)) |
              ((Source & 0x000000000000FF00) << (8 * 5)) |
              ((Source & 0x0000000000FF0000) << (8 * 3)) |
              ((Source & 0x00000000FF000000) << (8 * 1)) |
              ((Source & 0x000000FF00000000) >> (8 * 1)) |
              ((Source & 0x0000FF0000000000) >> (8 * 3)) |
              ((Source & 0x00FF000000000000) >> (8 * 5)) |
              ((Source)                      >> (8 * 7));

    return swapped;
}

ULONGLONG SdpByteSwapUint64(ULONGLONG uint64)
{
    return SdpUlonglongByteSwap(uint64);
}

ULONG SdpByteSwapUint32(ULONG uint32)
{
    return RtlUlongByteSwap(uint32);
}

USHORT SdpByteSwapUint16(USHORT uint16)
{
    return RtlUshortByteSwap(uint16);
}

void SdpRetrieveUuid128(PUCHAR Stream, GUID *uuidVal)
{
    PUCHAR tmpStream = Stream;

    RtlRetrieveUlong(&uuidVal->Data1, tmpStream);

    tmpStream = Stream + FIELD_OFFSET(GUID, Data2); 
    RtlRetrieveUshort(&uuidVal->Data2, tmpStream); 

    tmpStream = Stream + FIELD_OFFSET(GUID, Data3);
    RtlRetrieveUshort(&uuidVal->Data3, tmpStream ); 

    tmpStream = Stream + FIELD_OFFSET(GUID, Data4); 
    memcpy(uuidVal->Data4, tmpStream, sizeof(uuidVal->Data4)); 
}

void SdpRetrieveUint128(PUCHAR Stream, PSDP_ULARGE_INTEGER_16 pUint128)
{
    PUCHAR tmpStream;

    SdpRetrieveUlonglong(&pUint128->HighPart, Stream);
    tmpStream = Stream + sizeof(pUint128->HighPart);
    SdpRetrieveUlonglong(&pUint128->LowPart, tmpStream);
}

void SdpRetrieveUint64(PUCHAR Stream, PULONGLONG pUint64)
{
    SdpRetrieveUlonglong(pUint64, Stream);
}

void SdpRetrieveUint32(PUCHAR Stream, PULONG pUint32)
{
    RtlRetrieveUlong(pUint32, Stream);
}

void SdpRetrieveUint16(PUCHAR Stream, PUSHORT pUint16)
{
    RtlRetrieveUshort(pUint16, Stream);
}

NTSTATUS ComputeNodeListSize(PSDP_NODE Node, PULONG Size)
{
    SdpStack stack;
    SD_STACK_ENTRY *stackEntry;
    NTSTATUS status;

    PLIST_ENTRY current;
    PSDP_NODE node;
    PSDP_NODE_HEADER header;
    ULONG size = 0;

    status = STATUS_SUCCESS;

    current = &Node->hdr.Link;

    while (1) {
        node = CONTAINING_RECORD(current, SDP_NODE, hdr.Link);

        // every node has a one byte header to describe it
        size++;

        switch (node->hdr.Type) {
        case SDP_TYPE_NIL:
            // nothing more to do, no storage required for nil
            break;

        case SDP_TYPE_BOOLEAN:
            size++; 
            break;

        case SDP_TYPE_UINT:
        case SDP_TYPE_INT:
        case SDP_TYPE_UUID:
            // data immediately follows the header
            size +=  node->DataSize;
            break;

        case SDP_TYPE_URL:
        case SDP_TYPE_STRING:
            // the length of the string (w/out a NULL)
            size += node->DataSize;

            // add on the storage for the length of the string
            if (node->DataSize <= SIZE_8_BITS) {
                size += sizeof(UCHAR);
            }
            else if (node->DataSize <= SIZE_16_BITS) {
                size += sizeof(USHORT);
            }
            else {
                size += sizeof(ULONG);
            }
            break;

        case SDP_TYPE_SEQUENCE:
        case SDP_TYPE_ALTERNATIVE:
            // Assumes node->u.Sequence == node->u.Alternative
            if (IsListEmpty(&node->u.sequence.Link)) {
                // still need to store the length even if it is 0
                size += sizeof(UCHAR);
                node->DataSize = 0;
            }
            else {
                status = stack.Push(node, size);
                if (!NT_SUCCESS(status)) {
                    return status;
                }

                size = 0;
                node = CONTAINING_RECORD(node->u.sequence.Link.Flink,
										 SDP_NODE,
										 hdr.Link);
                current = &node->hdr.Link;
                continue;
            }
            break;

        case SDP_TYPE_CONTAINER:
            //
            // We let the container take care of everything, compensate for the 
            // header
            //
            size--;
            break;
        }

        //
        // Advance to the next link in the list
        //
        current = current->Flink;
        header = CONTAINING_RECORD(current, SDP_NODE_HEADER, Link);

        //
        // Check to see if we are at the end of the list
        //
        while (header->Type == SDP_TYPE_LISTHEAD) {
            //
            // Check to see if we have pushed any items.  If not, then we are 
            // done
            //
            if (stack.Depth() == 0) {
                *Size = size;
                return STATUS_SUCCESS;
            }

            stackEntry = stack.Pop();            

            node = stackEntry->Node;
            node->DataSize = size;

            //
            // add storage for size of the sequence / alternative we just
            // computed and add the amount of additional storage we need to
            // store this value
            //
            if (size <= SIZE_8_BITS) {
                size += sizeof(UCHAR);
            }
            else if (size <= SIZE_16_BITS) {
                size += sizeof(USHORT);
            }
            else {
                size += sizeof(ULONG);
            }

            //
            // Add up the size and get the next item in the list
            //
            size = stackEntry->Size + size;
            current = node->hdr.Link.Flink;
            header = CONTAINING_RECORD(current, SDP_NODE_HEADER, Link);
        }
    } 

    *Size = size;
    return STATUS_SUCCESS;
}

#define INC_STREAM(_inc)        \
{                               \
    Stream += (_inc);           \
    Size -= (_inc);             \
}

NTSTATUS ValidateStream(PUCHAR Stream, ULONG Size, PULONG NumEntries, PULONG ExtraPool, PULONG_PTR ErrorByte)
{
    PUCHAR origStream;
    ULONG ulVal, dataSize, elementSize;
    USHORT usVal;
//  UCHAR ucVal;

    UCHAR type, sizeIndex;

    origStream = Stream;

    while (Size) {

        SdpRetrieveHeader(Stream, type, sizeIndex);

        INC_STREAM(1);

        if (ARGUMENT_PRESENT(NumEntries)) {
            (*NumEntries)++;
        }

        if (type == SDP_TYPE_NIL) {
            //
            // no storage necessary for nil, just continue
            //
            continue;
        }

        if (Size == 0) {
            //
            // Error! A type value, but no storage?
            //
            if (ARGUMENT_PRESENT(ErrorByte)) {
                *ErrorByte = Stream - origStream;
            }
            return STATUS_INVALID_PARAMETER;
        }

        switch (type) {
        case SDP_TYPE_BOOLEAN:
            INC_STREAM(1);
            break;

        case SDP_TYPE_UINT:
        case SDP_TYPE_INT:
            //
            // valid size indicides are 0 - 4.
            //
            switch (sizeIndex) {
            case 0:  dataSize = 1; break;
            case 1:  dataSize = 2; break;
            case 2:  dataSize = 4; break;
            case 3:  dataSize = 8; break;
            case 4:  dataSize = 16; break;
            default: 
                if (ARGUMENT_PRESENT(ErrorByte)) {
                    *ErrorByte = Stream - origStream;
                }
                return STATUS_INVALID_PARAMETER;
            }

            if (dataSize > Size) {
                if (ARGUMENT_PRESENT(ErrorByte)) {
                    *ErrorByte = Stream - origStream;
                }
                return STATUS_INVALID_PARAMETER;
            }

            INC_STREAM(dataSize);
            break;

        case SDP_TYPE_UUID:
            //
            // valid indecies are 1,2,4
            //
            switch (sizeIndex) {
            case 1:  dataSize = 2; break;
            case 2:  dataSize = 4; break;
            case 4:  dataSize = 16; break;
            default: 
                if (ARGUMENT_PRESENT(ErrorByte)) {
                    *ErrorByte = Stream - origStream;
                }
                return STATUS_INVALID_PARAMETER;
            }

            if (dataSize > Size) {
                if (ARGUMENT_PRESENT(ErrorByte)) {
                    *ErrorByte = Stream - origStream;
                }
                return STATUS_INVALID_PARAMETER;
            }

            INC_STREAM(dataSize);
            break;

        case SDP_TYPE_URL:
        case SDP_TYPE_STRING:
        case SDP_TYPE_SEQUENCE:
        case SDP_TYPE_ALTERNATIVE:
            //
            // valid indecies are 5, 6 ,7
            //
            // Retrieve the size, convert it to little endian, and then compute
            // the total size for this element (amount of storage for the size
            // + the size itself).  
            //
            // Furthermore, make sure we have enough storage for the stored size.
            //
            switch (sizeIndex) {
            case 5:
                elementSize = ((ULONG) *Stream);  
                dataSize = 1 + elementSize;
                break;

            case 6:
                if (Size < 2) {
                    if (ARGUMENT_PRESENT(ErrorByte)) {
                        *ErrorByte = Stream - origStream;
                    }
                    return STATUS_INVALID_PARAMETER;
                }
                RtlRetrieveUshort(&usVal, Stream);
                elementSize = (ULONG) RtlUshortByteSwap(usVal);
                dataSize = 2 + elementSize;
                break;

            case 7:
                if (Size < 4) {
                    if (ARGUMENT_PRESENT(ErrorByte)) {
                        *ErrorByte = Stream - origStream;
                    }
                    return STATUS_INVALID_PARAMETER;
                }
                RtlRetrieveUlong(&ulVal, Stream);
                elementSize = RtlUlongByteSwap(ulVal);
                dataSize = 4 + elementSize;
                break;

            default:
                if (ARGUMENT_PRESENT(ErrorByte)) {
                    *ErrorByte = Stream - origStream;
                }
                return STATUS_INVALID_PARAMETER;
            }

            //
            // Finally, check to see if there is enough storage for the storage
            // for the size + the actual amount of data.
            // 
            if (dataSize > Size) {
                if (ARGUMENT_PRESENT(ErrorByte)) {
                    *ErrorByte = Stream - origStream;
                }
                return STATUS_INVALID_PARAMETER;
            }

            if (type == SDP_TYPE_STRING || type == SDP_TYPE_URL) {
                //
                // Increment past the string  / URL data
                //
                if (ARGUMENT_PRESENT(ExtraPool)) {
                    (*ExtraPool) += ROUND_SIZE(elementSize);
                }
                INC_STREAM(dataSize);
            }
            else {
                //
                // Validate the sequence / alternative list, increment only past 
                // the storage that holds the size
                //
                INC_STREAM((dataSize - elementSize));
            }
            break;

        default:
            // there is an error in the stream
            if (ARGUMENT_PRESENT(ErrorByte)) {
                *ErrorByte = Stream - origStream;
            }
            return STATUS_INVALID_PARAMETER;
        }
    }

    return STATUS_SUCCESS;
}

ULONG ComputeNumberOfEntries(PUCHAR Stream, ULONG Size, UCHAR FullParse)
{
    ULONG dataSize, elementSize, ulVal, numEntries = 0;
    USHORT usVal;
    UCHAR type, sizeIndex;

    while (Size) {
        SdpRetrieveHeader(Stream, type, sizeIndex);

        INC_STREAM(1);

        numEntries++;

        switch (type) {
        case SDP_TYPE_NIL:
            break;

        case SDP_TYPE_BOOLEAN:
            INC_STREAM(1);
            break;

        case SDP_TYPE_UINT:
        case SDP_TYPE_INT:
            //
            // valid size indicides are 0 - 4.
            //
            switch (sizeIndex) {
            case 0:  dataSize = 1; break;
            case 1:  dataSize = 2; break;
            case 2:  dataSize = 4; break;
            case 3:  dataSize = 8; break;
            case 4:  dataSize = 16; break;
            }

            INC_STREAM(dataSize);
            break;

        case SDP_TYPE_UUID:
            //
            // valid indecies are 1,2,4
            //
            switch (sizeIndex) {
            case 1:  dataSize = 2; break;
            case 2:  dataSize = 4; break;
            case 4:  dataSize = 16; break;
            }

            INC_STREAM(dataSize);
            break;

        case SDP_TYPE_URL:
        case SDP_TYPE_STRING:
        case SDP_TYPE_SEQUENCE:
        case SDP_TYPE_ALTERNATIVE:
            //
            // valid indecies are 5, 6 ,7
            //
            // Retrieve the size, convert it to little endian, and then compute
            // the total size for this element (amount of storage for the size
            // + the size itself).  
            //
            switch (sizeIndex) {
            case 5:
                elementSize = (ULONG) *Stream;  
                dataSize = 1 + elementSize;
                break;

            case 6:
                RtlRetrieveUshort(&usVal, Stream);
                elementSize = (ULONG) RtlUshortByteSwap(usVal);
                dataSize = 2 + elementSize;
                break;

            case 7:
                RtlRetrieveUlong(&ulVal, Stream);
                elementSize = RtlUlongByteSwap(ulVal);
                dataSize = 4 + elementSize;
            }

            if (type == SDP_TYPE_STRING || type == SDP_TYPE_URL || !FullParse) {
                //
                // Increment past the string  / URL data / container
                // (if we are not fully parsing)
                //
                INC_STREAM(dataSize);
            }
            else {
                //
                // Validate the sequence / alternative list, increment only past 
                // the storage that holds the size
                //
                INC_STREAM((dataSize - elementSize));
            }
            break;
        }
    }

    return numEntries;
}

SDP_ERROR MapNtStatusToSdpError(NTSTATUS Status)
{
    switch (Status) {
    case STATUS_INVALID_PARAMETER:
        return SDP_ERROR_INVALID_REQUEST_SYNTAX;

    case STATUS_INSUFFICIENT_RESOURCES:
        return SDP_ERROR_INSUFFICIENT_RESOURCES; 

    default:
        ASSERT(FALSE);
        return SDP_ERROR_INVALID_REQUEST_SYNTAX;
    }
}

VOID Sdp_InitializeListHead(PLIST_ENTRY ListHead)
{
    InitializeListHead(ListHead);
}

UCHAR Sdp_IsListEmpty(PLIST_ENTRY ListHead)
{
    return IsListEmpty(ListHead);
}

VOID Sdp_InsertHeadList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
{
    InsertHeadList(ListHead, Entry);
}

VOID Sdp_InsertTailList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
{
    InsertTailList(ListHead, Entry);
}


VOID Sdp_InsertEntryList(PLIST_ENTRY Previous, PLIST_ENTRY Entry)
{
    InsertEntryList(Previous, Entry); 
}

PLIST_ENTRY Sdp_RemoveHeadList(PLIST_ENTRY ListHead)
{
    return RemoveHeadList(ListHead);
}

VOID Sdp_RemoveEntryList(PLIST_ENTRY Entry)
{
    RemoveEntryList(Entry);
}

struct VerifyRecord {
    VerifyRecord() {
        isAttribId = TRUE;
        done = FALSE;
        seenFirstElem = FALSE;
    }

    UCHAR seenFirstElem;
    UCHAR isAttribId;
    UCHAR done;
};

NTSTATUS 
VerifyIsRecordWalk(
    VerifyRecord *verify,
    UCHAR DataType,
    ULONG DataSize,
    PVOID Data,
    ULONG DataStorageSize
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    if (verify->done) {
        //
        // if we encounter any stream elements after we are done, than this is 
        // an invalid stream
        //
        return STATUS_INVALID_PARAMETER;
    }
    else if (verify->seenFirstElem == FALSE) {
        if (DataType == SDP_TYPE_SEQUENCE) {
            verify->seenFirstElem = TRUE;
        }
        else {
            //
            // first item in the list must be a sequence
            //
            status = STATUS_INVALID_PARAMETER;
        }
    }
    else {
        if (verify->isAttribId) {
            if (DataType == SDP_TYPE_SEQUENCE && Data == NULL) {
                //
                // end of the sequence
                //
                verify->done = TRUE;
                return STATUS_SUCCESS;
            }
            else if (DataType != SDP_TYPE_UINT && DataSize != sizeof(USHORT)) {
                //
                // not a attrib ID where we expect it, bad stream!
                //
                return STATUS_INVALID_PARAMETER;
            }
        }
        else {
            if (DataType == SDP_TYPE_SEQUENCE ||
                DataType == SDP_TYPE_ALTERNATIVE) {
                status = STATUS_REPARSE_POINT_NOT_RESOLVED;
            }
        }

        verify->isAttribId = !verify->isAttribId;
    }

    return status;
}

NTSTATUS SdpIsStreamRecord(PUCHAR Stream, ULONG Size)
{
    NTSTATUS status;

    status = ValidateStream(Stream, Size, NULL, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    VerifyRecord vr;

    return SdpWalkStream(Stream,
                         Size,
                         (PSDP_STREAM_WALK_FUNC) VerifyIsRecordWalk,
                         &vr);
}

void SdpNormalizeUuid(PSDP_NODE pUuid, GUID* uuid)
{
    if (pUuid->hdr.SpecificType == SDP_ST_UUID128) {
        RtlCopyMemory(uuid, &pUuid->u.uuid128, sizeof(GUID));
    }
    else if (pUuid->hdr.SpecificType == SDP_ST_UUID32) {
        RtlCopyMemory(uuid, &Bluetooth_Base_UUID, sizeof(GUID));
        uuid->Data1 += pUuid->u.uuid32;
    }
    else {
        RtlCopyMemory(uuid, &Bluetooth_Base_UUID, sizeof(GUID));
        uuid->Data1 += pUuid->u.uuid16;
    }
}


