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
#endif

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#pragma code_seg("PAGE", "CODE")
#endif

#define RtlUlonglongByteSwap do_not_use_RtlUlonglongByteSwap

#define WRITE_STREAM(_data, _size)          \
    {                                       \
        memcpy(Stream, (_data), (_size));   \
        Stream += (_size);                  \
    }
    
PUCHAR WriteVariableSizeToStream(UCHAR Type, ULONG DataSize, PUCHAR Stream)
{
    ULONG uint32;
    USHORT uint16;
    UCHAR byte;

    if (DataSize <= SIZE_8_BITS) {
        byte = FMT_TYPE(Type) | SIZE_INDEX_NEXT_8_BITS;
        WRITE_STREAM(&byte, sizeof(byte));

        byte = (UCHAR) DataSize;
        WRITE_STREAM(&byte, sizeof(byte));
    }
    else if (DataSize <= SIZE_16_BITS) {
        byte = FMT_TYPE(Type) | SIZE_INDEX_NEXT_16_BITS;
        WRITE_STREAM(&byte, sizeof(byte));

        uint16 = (USHORT) DataSize;
        uint16 = RtlUshortByteSwap(uint16);
        WRITE_STREAM(&uint16, sizeof(uint16));
    }
    else {
        byte = FMT_TYPE(Type) | SIZE_INDEX_NEXT_32_BITS;
        WRITE_STREAM(&byte, sizeof(byte));

        uint32 = RtlUlongByteSwap(DataSize);
        WRITE_STREAM(&uint32, sizeof(uint32));
    }

    return Stream;
}

PUCHAR WriteLeafToStream(PSDP_NODE Node, PUCHAR Stream)
{
    GUID uuid128;
    ULONGLONG uint64;
    ULONG uint32, uuid32;
    USHORT uint16;

    UCHAR byte;

    switch (Node->hdr.Type) {
    case SDP_TYPE_NIL:
        // write the header and that's it
        byte = 0x00;
        WRITE_STREAM(&byte, sizeof(byte));
        break;

    case SDP_TYPE_UINT:
    case SDP_TYPE_INT: 

        byte = FMT_TYPE(Node->hdr.Type) | FMT_SIZE_INDEX_FROM_ST(Node->hdr.SpecificType);
        WRITE_STREAM(&byte, sizeof(byte));

        switch (Node->hdr.SpecificType) {
        case SDP_ST_UINT8:
            WRITE_STREAM(&Node->u.uint8, Node->DataSize);
            break;

        case SDP_ST_INT8:  
            WRITE_STREAM(&Node->u.int8, Node->DataSize);
            break;

        case SDP_ST_UINT16: 
            uint16 = RtlUshortByteSwap(Node->u.uint16);
            WRITE_STREAM(&uint16, Node->DataSize);
            break;

        case SDP_ST_INT16: 
            uint16 = RtlUshortByteSwap((USHORT) Node->u.int16);
            WRITE_STREAM(&uint16, Node->DataSize);
            break;

        case SDP_ST_UINT32:  
            uint32 = RtlUlongByteSwap(Node->u.uint32);
            WRITE_STREAM(&uint32, Node->DataSize);
            break;

        case SDP_ST_INT32:
            uint32 = RtlUlongByteSwap((ULONG) Node->u.int32);
            WRITE_STREAM(&uint32, Node->DataSize);
            break;

        case SDP_ST_UINT64:
            uint64 = SdpUlonglongByteSwap(Node->u.uint64);
            WRITE_STREAM(&uint64, Node->DataSize);
            break;

        case SDP_ST_INT64:   
            uint64 = SdpUlonglongByteSwap((ULONGLONG) Node->u.int64);
            WRITE_STREAM(&uint64, Node->DataSize);
            break;

        case SDP_ST_UINT128: 
            uint64 = SdpUlonglongByteSwap(Node->u.uint128.HighPart);
            WRITE_STREAM(&uint64, sizeof(uint64));

            uint64 = SdpUlonglongByteSwap(Node->u.uint128.LowPart);
            WRITE_STREAM(&uint64, sizeof(uint64));
            break;

        case SDP_ST_INT128:  
            uint64 = SdpUlonglongByteSwap((LONGLONG) Node->u.int128.HighPart);
            WRITE_STREAM(&uint64, sizeof(uint64));

            uint64 = SdpUlonglongByteSwap(Node->u.int128.LowPart);
            WRITE_STREAM(&uint64, sizeof(uint64));
            break;
        }
        break;

    case SDP_TYPE_UUID:
        byte = FMT_TYPE(Node->hdr.Type) | FMT_SIZE_INDEX_FROM_ST(Node->hdr.SpecificType);
        WRITE_STREAM(&byte, sizeof(byte));

        switch (Node->hdr.SpecificType) {
        case SDP_ST_UUID16:
            uint16 = RtlUshortByteSwap(Node->u.uuid16);
            WRITE_STREAM(&uint16, Node->DataSize);
            break;

        case SDP_ST_UUID32:  
            uuid32 = RtlUlongByteSwap(Node->u.uuid32);
            WRITE_STREAM(&uuid32, Node->DataSize);
            break;

        case SDP_ST_UUID128: 
            SdpByteSwapUuid128(&Node->u.uuid128, &uuid128);
            WRITE_STREAM(&uuid128, Node->DataSize);
            break;
        }
        break;

    case SDP_TYPE_BOOLEAN:
        byte = FMT_TYPE(Node->hdr.Type) | SIZE_INDEX_ZERO;
        WRITE_STREAM(&byte, sizeof(byte));

        WRITE_STREAM(&Node->u.boolean, sizeof(Node->u.boolean));
        break;

    case SDP_TYPE_STRING:
    case SDP_TYPE_URL:
        Stream = WriteVariableSizeToStream((UCHAR) Node->hdr.Type, Node->DataSize, Stream);

        if (Node->hdr.Type == SDP_TYPE_STRING) {
            WRITE_STREAM(Node->u.string, Node->DataSize);
        }
        else if (Node->hdr.Type == SDP_TYPE_URL) {
            WRITE_STREAM(Node->u.url, Node->DataSize);
        }
		break;

    default:
        ASSERT(FALSE);
        break;
    }

    return Stream;
}

void
SdpRetrieveVariableSize(
    UCHAR *Stream,
    UCHAR SizeIndex,
    ULONG *ElementSize,
    ULONG *StorageSize
    )
{
    ULONG uint32;
    USHORT uint16;

    switch (SizeIndex) {
    case 5:
        *ElementSize = ((ULONG) *Stream);  
        *StorageSize = 1;
        break;

    case 6:
        RtlRetrieveUshort(&uint16, Stream);
        *ElementSize = (ULONG) RtlUshortByteSwap(uint16);
        *StorageSize = 2;
        break;

    case 7:
        RtlRetrieveUlong(&uint32, Stream);
        *ElementSize = RtlUlongByteSwap(uint32);
        *StorageSize = 4;
        break;
    }
}

void SdpRetrieveUuidFromStream(PUCHAR Stream, ULONG DataSize, GUID *pUuid, UCHAR bigEndian)
{
//  PUCHAR tmpStream;
    ULONG ulVal;
    USHORT usVal;

    switch (DataSize) {
    case 2:
        //
        // Retrieve the value and convert it to little endian
        //
        RtlRetrieveUshort(&usVal, Stream);
        usVal = RtlUshortByteSwap(usVal);

        //
        // Add the value
        //
        memcpy(pUuid, (PVOID) &Bluetooth_Base_UUID, sizeof(GUID));
        pUuid->Data1 += usVal;

        if (bigEndian) {
            //
            // convert the UUID to big endian
            //
            SdpByteSwapUuid128(pUuid, pUuid);
        }

        break;

    case 4:
        //
        // Retrieve the value and convert it to little endian
        //
        RtlRetrieveUlong(&ulVal, Stream);
        ulVal = RtlUlongByteSwap(ulVal);

        //
        // Add the value
        //
        memcpy(pUuid, (PVOID) &Bluetooth_Base_UUID, sizeof(GUID));
        pUuid->Data1 += ulVal;

        if (bigEndian) {
            //
            // convert the UUID to big endian
            //
            SdpByteSwapUuid128(pUuid, pUuid);
        }
        break;

    case 16:
        //
        // Just retrieve the UUID, already in big endian form
        //
        SdpRetrieveUuid128(Stream, pUuid);

        //
        // convert to little endian if the caller requires
        //
        if (!bigEndian) {
            SdpByteSwapUuid128(pUuid, pUuid);
        }
        break;
    }
}



PSDP_NODE
StreamToNode(
    PUCHAR Stream,
    ULONG Size,
    PSDP_NODE_HEADER Header,
    PSDP_NODE Nodes,
    PCHAR ExtraPool,
    UCHAR FullParse
    )
{
#define LINK_IN_NODE(_list, _node)                \
{                                                   \
    InsertTailList((_list), &(_node)->hdr.Link);      \
    (_node)++;                                     \
}

#define INC_STREAM(_inc)        \
{                               \
    Stream += (_inc);           \
    Size -= (_inc);             \
}

    SdpStack                stack;
    SD_STACK_ENTRY          *stackEntry;
	PLIST_ENTRY				head, newHead;
    PSDP_NODE               node;
    PUCHAR                  tmpStream;
    ULONG                   dataSize, storageSize, elementSize;
    NTSTATUS                status;

    UCHAR                   type, sizeIndex; // , byte

    SDP_LARGE_INTEGER_16    int128;
    SDP_ULARGE_INTEGER_16   uint128;
    GUID                    uuid128, uuid128Tmp;
    ULONGLONG               uint64;
    ULONG                   uint32;
    USHORT                  uint16;
//  UCHAR                   uint8;

    if (Header) {
        node = Nodes;
        head = &Header->Link;
    }
    else {
        SdpInitializeNodeTree(Nodes);
        head = &Nodes->hdr.Link;
        node = Nodes + 1;
    }

    while (1) {
        SdpRetrieveHeader(Stream, type, sizeIndex);
        INC_STREAM(1);

        switch (type) {
        case SDP_TYPE_NIL:
            SdpInitializeNodeNil(node);
            LINK_IN_NODE(head, node);
            break;

        case SDP_TYPE_BOOLEAN:
            //
            // Do we need to validate the value?
            //
            SdpInitializeNodeBoolean(node, *Stream);
            LINK_IN_NODE(head, node);
            INC_STREAM(1);
            break;

        case SDP_TYPE_UINT:
            //
            // valid size indecies are 0 - 4.
            //
            switch (sizeIndex) {
            case 0:  
                dataSize = 1; 
                SdpInitializeNodeUInt8(node, *Stream);
                break;

            case 1:  
                dataSize = 2; 
                RtlRetrieveUshort(&uint16, Stream);
                SdpInitializeNodeUInt16(node, RtlUshortByteSwap(uint16));
                break;

            case 2:  
                dataSize = 4; 
                RtlRetrieveUlong(&uint32, Stream);
                SdpInitializeNodeUInt32(node, RtlUlongByteSwap(uint32));
                break;

            case 3:  
                dataSize = 8; 
                SdpRetrieveUlonglong(&uint64, Stream);
                SdpInitializeNodeUInt64(node, SdpUlonglongByteSwap(uint64));
                break;

            case 4:  
                dataSize = 16; 

                // high quad word first, then the low part
                tmpStream = Stream;
                SdpRetrieveUlonglong(&uint128.HighPart, tmpStream);
                
                tmpStream = Stream + sizeof(uint128.HighPart);
                SdpRetrieveUlonglong(&uint128.LowPart, tmpStream);
                
                uint128.HighPart = SdpUlonglongByteSwap(uint128.HighPart);
                uint128.LowPart = SdpUlonglongByteSwap(uint128.LowPart);
                SdpInitializeNodeUInt128(node, &uint128);
                break;

            default:
                ASSERT(FALSE); 
                return NULL;
            }

            LINK_IN_NODE(head, node);
            INC_STREAM(dataSize);
            break;

        case SDP_TYPE_INT:
            //
            // valid size indecies are 0 - 4.
            //
            switch (sizeIndex) {
            case 0:  
                dataSize = 1; 
                SdpInitializeNodeInt8(node, (CHAR) *Stream);
                break;

            case 1:  
                dataSize = 2; 
                RtlRetrieveUshort(&uint16, Stream);
                SdpInitializeNodeInt16(node, (SHORT) RtlUshortByteSwap(uint16));
                break;

            case 2:  
                dataSize = 4; 
                RtlRetrieveUlong(&uint32, Stream);
                SdpInitializeNodeInt32(node, (LONG) RtlUlongByteSwap(uint32));
                break;

            case 3:  
                dataSize = 8; 
                SdpRetrieveUlonglong(&uint64, Stream);
                SdpInitializeNodeInt64(node, (LONGLONG) SdpUlonglongByteSwap(uint64));
                break;

            case 4:  
                dataSize = 16; 

                SdpRetrieveUint128(Stream, (PSDP_ULARGE_INTEGER_16) &int128);

                int128.HighPart =
                    (LONGLONG) SdpUlonglongByteSwap((LONGLONG) int128.HighPart);
                int128.LowPart = SdpUlonglongByteSwap(int128.LowPart);

                SdpInitializeNodeInt128(node, &int128);
                break;

            default:
                ASSERT(FALSE); 
                return NULL;
            }

            LINK_IN_NODE(head, node);
            INC_STREAM(dataSize);
            break;

        case SDP_TYPE_UUID:
            //
            // valid indecies are 1,2,4
            //
            switch (sizeIndex) {
            case 1:  
                dataSize = 2; 
                RtlRetrieveUshort(&uint16, Stream);
                SdpInitializeNodeUUID16(node, RtlUshortByteSwap(uint16));
                break;

            case 2:  
                dataSize = 4; 
                RtlRetrieveUlong(&uint32, Stream);
                SdpInitializeNodeUUID32(node, RtlUlongByteSwap(uint32));
                break;

            case 4:  
                dataSize = 16; 

                //
                // Retrieve the whole structure ...
                //
                SdpRetrieveUuid128(Stream, &uuid128Tmp);

                //
                // ... and then convert into little endian
                //
                SdpByteSwapUuid128(&uuid128Tmp, &uuid128);

                SdpInitializeNodeUUID128(node, &uuid128);
                break;

            default: 
                ASSERT(FALSE);
                return NULL;
            }

            LINK_IN_NODE(head, node);
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
            SdpRetrieveVariableSize(Stream, sizeIndex, &elementSize, &storageSize);

            switch (type) {
            case SDP_TYPE_URL:
                SdpInitializeNodeUrl(node,
                                     (PCHAR) Stream + storageSize,
                                      elementSize,
                                      ExtraPool);
                ExtraPool += ROUND_SIZE(elementSize);
                break;

            case SDP_TYPE_STRING:
                SdpInitializeNodeString(node,
                                        (PCHAR) Stream + storageSize,
                                         elementSize,
                                         ExtraPool);
                ExtraPool += ROUND_SIZE(elementSize);
                break;

            case SDP_TYPE_SEQUENCE:
            case SDP_TYPE_ALTERNATIVE:
                if (FullParse) {
                    if (type == SDP_TYPE_SEQUENCE) {
                        SdpInitializeNodeSequence(node);
                        newHead = &node->u.sequence.Link;
                    }
                    else {
                        SdpInitializeNodeAlternative(node);
                        newHead = &node->u.alternative.Link;
                    }

                    node->DataSize = elementSize;

                    if (elementSize > 0) {
                        INC_STREAM(storageSize);
                        status = stack.Push(node, Size, head);
                        if (!NT_SUCCESS(status)) {
                            return NULL;
                        }

                        LINK_IN_NODE(head, node);

                        //
                        // LINK_IN_NODE incremented node, so it is now pointing to a
                        // new and unused slot in the array
                        //
                        head = newHead;
                        Size = elementSize;
                        continue;
                    }
                }
                else {
                    //
                    // don't recurse on the container, just store the stream for
                    // it, including the element header (which we just incremented
                    // past)
                    //
                    SdpInitializeNodeContainer(
                        node,
                        Stream - 1,
                        elementSize + 1 // we subtracted 1 for element header above
                                    + storageSize
                        );
                }
                break;
            }

            LINK_IN_NODE(head, node);
            INC_STREAM(storageSize + elementSize);
            break;

        default:
            // there is an error in the stream
            ASSERT(FALSE);
            return NULL;
        }

        if ((int) Size < 0) {
        	return NULL;
        }

        while (Size == 0) {
            //
            // Check to see if we have pushed any items.  If not, then we are 
            // done
            //
            if (stack.Depth() == 0) {
                return Nodes;
            }

            stackEntry = stack.Pop();            

            Size = stackEntry->Size - stackEntry->Node->DataSize;
            head = stackEntry->Head;
        }
    }

    return Nodes;
#undef INC_STREAM
#undef LINK_IN_NODE
}


NTSTATUS
SdpTreeFromStream(
    PUCHAR Stream,
    ULONG Size,
    PSDP_NODE_HEADER Header,
    PSDP_NODE *Root,
    UCHAR FullParse
    )
{
    PSDP_NODE_REF ref;
    PSDP_NODE node, result;
    PCHAR extra;
    ULONG numEntries, extraPool, i, nodeSize; // valid,
    NTSTATUS status;
    ULONG_PTR errorByte;

    numEntries = 0;
    extraPool = 0;

    status = ValidateStream(Stream, Size, &numEntries, &extraPool, &errorByte);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // recompute the number of required entries if we are not parsing the entire
    // tree
    //
    if (!FullParse) {
        numEntries = ComputeNumberOfEntries(Stream, Size, FullParse);
    }

    //
    // Need to have a tree head as well if the caller doesn't provide a list head
    //
    if (Header == NULL) {
        numEntries++;
    }

    ref = (PSDP_NODE_REF) SdpAllocatePool(sizeof(SDP_NODE_REF));
    if (!ref) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    nodeSize = ROUND_SIZE(sizeof(SDP_NODE) * numEntries);
    node = (PSDP_NODE) SdpAllocatePool(nodeSize + extraPool);

    if (!node) {
        SdpFreePool(ref);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    extra = NULL;
    if (extraPool > 0) {
        extra = ((PCHAR) node) + nodeSize;
    }

    ref->Alloc = node;
    ref->RefCount = (LONG) numEntries;

    for (i = 0; i < numEntries; i++) {
        InitializeListHead(&node[i].hdr.Link);
        node[i].Reserved = ref;
    }

    result = StreamToNode(Stream, Size, Header, node, extra, FullParse);
    if (result) {
        if (Root) {
            *Root = result;
        }
        return STATUS_SUCCESS;
    }
    else {
        if (Header) {
            SdpInitializeNodeHeader(Header);
        }

        SdpFreePool(node);

        if (Root) {
            *Root = NULL;
        }

        return STATUS_INVALID_PARAMETER;
    }
}

NTSTATUS NodeToStream(PSDP_NODE Node, PUCHAR Stream)
{
    PSD_STACK_ENTRY stackEntry;
	PSDP_NODE_HEADER header;
    PLIST_ENTRY current; 
    PSDP_NODE node;

    SdpStack stack;
    NTSTATUS status;
    UCHAR byte;
    
    current = &Node->hdr.Link;

    while (1) {
        node = CONTAINING_RECORD(current, SDP_NODE, hdr.Link);
        byte = 0x00;

        switch (node->hdr.Type) {
        case SDP_TYPE_SEQUENCE:
        case SDP_TYPE_ALTERNATIVE:
            Stream = WriteVariableSizeToStream((UCHAR) node->hdr.Type,
                                               node->DataSize,
                                               Stream);

            //
            // node->DataSize == 0 if there is no sublist
            //
            if (node->DataSize) {
                status = stack.Push(node);

                if (!NT_SUCCESS(status)) {
                    return status;
                }

                if (node->hdr.Type == SDP_TYPE_SEQUENCE) {
                    node = CONTAINING_RECORD(node->u.sequence.Link.Flink,
                                             SDP_NODE, 
                                             hdr.Link);
                }
                else {
                    node = CONTAINING_RECORD(node->u.alternative.Link.Flink,
                                             SDP_NODE, 
                                             hdr.Link);
                }

                current = &node->hdr.Link;    
                continue;
            }
            break;

        default:
            Stream = WriteLeafToStream(node, Stream);
            break;
        }

        //
        // Advance to the next 
        //
        current = current->Flink;
		header = CONTAINING_RECORD(current, SDP_NODE_HEADER, Link);

        while (header->Type == SDP_TYPE_LISTHEAD) {
            //
            // Check to see if we have pushed any items.  If not, then we are 
            // done
            //
            if (stack.Depth() == 0) {
                return STATUS_SUCCESS;
            }

            stackEntry = stack.Pop();            

            node = stackEntry->Node;
            current = node->hdr.Link.Flink;
			header = CONTAINING_RECORD(current, SDP_NODE_HEADER, Link);
        }
    } 

    return STATUS_SUCCESS;
}

NTSTATUS SdpStreamFromTree(PSDP_NODE Root, PUCHAR *Stream, PULONG Size)
{
    return SdpStreamFromTreeEx(Root, Stream, Size, 0, 0);
}

NTSTATUS SdpStreamFromTreeEx(PSDP_NODE Root, PUCHAR *Stream, PULONG Size, ULONG HeaderSize, ULONG TailSize)
{
    PUCHAR stream;
    PSDP_NODE first;
    NTSTATUS status;

    *Stream = NULL;

    if (Root->hdr.Type != SDP_TYPE_LISTHEAD) {
        return STATUS_INVALID_PARAMETER;
    }

    if (IsListEmpty(&Root->hdr.Link)) {
        *Size = 0;
        return STATUS_SUCCESS;
    }

    first = CONTAINING_RECORD(Root->hdr.Link.Flink, SDP_NODE, hdr.Link);
    status = ComputeNodeListSize(first, Size);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    stream = (PUCHAR) SdpAllocatePool(HeaderSize + *Size + TailSize);  
    if (stream == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = NodeToStream(first, stream + HeaderSize);
    if (!NT_SUCCESS(status)) {
        SdpFreePool(stream);
        return status;
    }

    *Stream = stream;
    return STATUS_SUCCESS;
}
