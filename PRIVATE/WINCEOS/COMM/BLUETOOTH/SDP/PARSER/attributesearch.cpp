//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "pch.h"
#include "bt_debug.h"
#include <svsutil.hxx>

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#pragma code_seg("PAGE", "CODE")
#endif // UNDER_CE

// extern "C" ULONG g_Support10B;
// extern "C" USHORT g_MaxAttribCount;

struct ATTRIBUTE_INFO {
    LIST_ENTRY Link;
    PUCHAR Stream;
    ULONG StreamSize;
    USHORT AttributeId;
};
typedef ATTRIBUTE_INFO* PATTRIBUTE_INFO;

struct FindStreamInfo {
    FindStreamInfo(UCHAR findOne = FALSE) 
    {
        RtlZeroMemory(this, sizeof(*this));

        findOnlyOne = findOne;
        if (findOnlyOne) {
            pStream = NULL;
            streamSize = 0;
        }
        else {
            InitializeListHead(&ListHead);
        }
        IsAttribId = TRUE;
    }

    NTSTATUS Walk(SdpAttributeRange * pRange, ULONG count, UCHAR *stream, ULONG size)
    {
        AttributeRange = pRange;
        AttributeRangeCount = count;

        return SdpWalkStream(stream,
                             size,
                             (PSDP_STREAM_WALK_FUNC) FindAttribInStreamWalk,
                             (PVOID) this);
    }

    ULONG IsAttributeInRange(USHORT Attribute);

    static NTSTATUS FindAttribInStreamWalk(FindStreamInfo *Info,
                                           UCHAR DataType,
                                           ULONG DataSize,
                                           PVOID Data,
                                           ULONG DataStorageSize);

    union {
        LIST_ENTRY ListHead;
        struct {
            UCHAR *pStream;
            ULONG streamSize;
        };
    };

    SdpAttributeRange * AttributeRange;
    ULONG AttributeRangeCount;
    
    ULONG Size;
    ULONG Depth;

    USHORT AttributeId;

    UCHAR IsAttribId;
    UCHAR IsDesiredStream;    

    UCHAR findOnlyOne;

#if DBG
    UCHAR Done;
#endif
};

ULONG FindStreamInfo::IsAttributeInRange(USHORT Attribute)
{
    ULONG i;

    for (i = 0; i < AttributeRangeCount; i++) {
        if (Attribute >= AttributeRange[i].minAttribute &&
            Attribute <= AttributeRange[i].maxAttribute) {
            return TRUE;
        }
        else if (Attribute < AttributeRange[i].minAttribute) {
            //
            // The range is sorted in ascending order.  If the Attribute is less
            // than the current minimum, there is no match.
            // 
            return FALSE;
        }
    }

    return FALSE;
}
            
NTSTATUS
FindStreamInfo::FindAttribInStreamWalk(
    FindStreamInfo *Info,
    UCHAR DataType,
    ULONG DataSize,
    PVOID Data,
    ULONG DataStorageSize
    )
{
// DataStorageSize is for sequences, alternatives, URLs and Strings only.  
// It contains the number of bytes used to hold the size of the data. 

    NTSTATUS status = STATUS_SUCCESS;
    PATTRIBUTE_INFO ai;
    
    USHORT attributeId;

    // per spec, must be 1, 2, or 4 bytes on SEQ/Alternative/URL/String.  It's 0 bytes if we're not using one of those types.
    SVSUTIL_ASSERT((DataStorageSize == 1) || (DataStorageSize == 2) || (DataStorageSize == 4) || (DataStorageSize == 0));

#if DBG
    ASSERT(Info->Done != TRUE);
#endif

    if (Info->Depth == 0) {
        ASSERT(DataType == SDP_TYPE_SEQUENCE);
        //
        // We are at the top level sequence for the record
        //
        Info->Depth++;
    }
    else {
        if (Info->IsAttribId) {

            if (DataType == SDP_TYPE_SEQUENCE) {
                //
                // we reached the end of the record sequence
                //
                ASSERT(Data == NULL);
#if DBG
                Info->Done = TRUE;
#endif
                return STATUS_SUCCESS;
            }
            else if (DataType == SDP_TYPE_UINT && DataSize == sizeof(USHORT)) {
                RtlRetrieveUshort(&attributeId, Data);
                attributeId = RtlUshortByteSwap(attributeId);
                
                if (Info->IsAttributeInRange(attributeId)) {
                    Info->AttributeId = attributeId;
                    Info->IsDesiredStream = TRUE;
                }
            }
            else {
                ASSERT(FALSE);
                return STATUS_INVALID_PARAMETER;
            }
        }
        else if (Info->IsDesiredStream) {
            PUCHAR stream = (PUCHAR) Data;

            Info->IsDesiredStream = FALSE;

            //
            // Right now, stream points to the data of the attribute, but 
            // we need the header for this element, so we backtrack a bit
            //
            switch (DataType) {
            case SDP_TYPE_NIL:
            case SDP_TYPE_BOOLEAN:
            case SDP_TYPE_UUID:
            case SDP_TYPE_UINT:
            case SDP_TYPE_INT:
                //
                // No adjustment necessary b/c the size isn't stored in the stream
                //
                break;

            case SDP_TYPE_URL:
            case SDP_TYPE_STRING:
            case SDP_TYPE_SEQUENCE:
            case SDP_TYPE_ALTERNATIVE:
                //
                // Each of these data type embed their size into the stream, 
                // compensate for it.
                //
                stream   -= DataStorageSize;
                DataSize += DataStorageSize;
                break;

            default:
                ASSERT(FALSE);
            }                

            //
            // Adjust for the header (which contains the type and subtype)
            //
            stream--;
            DataSize++;

            if (Info->findOnlyOne) {
                Info->pStream = stream;
                Info->streamSize = DataSize;
            }
            else {
                ai = (PATTRIBUTE_INFO) SdpAllocatePool(sizeof(ATTRIBUTE_INFO));      
                if (ai) {
                    InitializeListHead(&ai->Link);

                    ai->AttributeId = Info->AttributeId;
                    ai->Stream = stream;
                    ai->StreamSize = DataSize;

                    InsertTailList(&Info->ListHead, &ai->Link);
                }
                else {
                    return STATUS_INSUFFICIENT_RESOURCES;
                }
            }
        }

        if (DataType == SDP_TYPE_SEQUENCE || DataType == SDP_TYPE_ALTERNATIVE) {
            status = STATUS_REPARSE_POINT_NOT_RESOLVED;
        }

        Info->IsAttribId = !Info->IsAttribId;
    }

    return status;
}

ULONG GetContainerHeaderSize(ULONG ContainerSize)
{
    ULONG size;

    //
    // All of the streams are contained in a top level sequence, we must 
    // account for this sequence as well
    //
    size = sizeof(UCHAR); // element header

    //
    // compute the amount of space needed to scribble the size
    //
    if (ContainerSize <= SIZE_8_BITS) {
        size += sizeof(UCHAR);
    }
    else if (ContainerSize <= SIZE_16_BITS) {
        size += sizeof(USHORT);
    }
    else {
        size += sizeof(ULONG);
    }

    return size;
}

NTSTATUS
CreateAttributeSearchReply(
    FindStreamInfo *Info,
    ULONG SequenceDataSize,
    PSDP_STREAM_ENTRY *ppEntry
    )
{
    PUCHAR stream;
    PLIST_ENTRY entry;
    PATTRIBUTE_INFO ai;

    ULONG totalSize;
//  ULONG ulVal;
    USHORT usVal;
    UCHAR byte;

    //
    // Get the header size again b/c the size might have crossed an integer
    // size boundary
    //
    totalSize = GetContainerHeaderSize(SequenceDataSize) + SequenceDataSize;
    *ppEntry = (PSDP_STREAM_ENTRY) SdpAllocatePool(sizeof(SDP_STREAM_ENTRY) + totalSize - 1);
    if (*ppEntry == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    InitializeListHead(&(*ppEntry)->link);
    stream = (*ppEntry)->stream;
    (*ppEntry)->streamSize = totalSize; 

#define WRITE_STREAM(_data, _size)          \
{                                       \
    memcpy(stream, (_data), (_size));   \
    stream += (_size);                  \
}

    //
    // Write out the element header and size for the top level sequence.
    // The size field in the header is the size of the contents, not
    // the element header + contents
    //
    stream = WriteVariableSizeToStream(SDP_TYPE_SEQUENCE, SequenceDataSize, stream);

    //
    // Write out the contents of the top level sequence (ie the results)
    //
    while (!IsListEmpty(&Info->ListHead)) {
        entry = RemoveHeadList(&Info->ListHead);
        ai = CONTAINING_RECORD(entry, ATTRIBUTE_INFO, Link);

        byte = FMT_TYPE(SDP_TYPE_UINT) | FMT_SIZE_INDEX_FROM_ST(SDP_ST_UINT16);
        WRITE_STREAM(&byte, sizeof(byte));

        usVal = RtlUshortByteSwap(ai->AttributeId);
        WRITE_STREAM(&usVal, sizeof(USHORT));

        WRITE_STREAM(ai->Stream, ai->StreamSize);

        SdpFreePool(ai);
    }
#undef WRITE_STREAM

    return STATUS_SUCCESS;
}

NTSTATUS
SdpFindAttributeInStreamInternal(
    PUCHAR Stream,
    ULONG Size,
    SdpAttributeRange * AttributeRange,
    ULONG AttributeRangeCount, 
    PSDP_STREAM_ENTRY *ppEntry,
    PSDP_ERROR SdpError
    )
{
    PLIST_ENTRY entry;
    PATTRIBUTE_INFO ai;
//  PUCHAR stream;

    FindStreamInfo info;

    NTSTATUS status;

    ULONG sequenceSize;

 // UCHAR byte;

    status = ValidateStream(Stream, Size, NULL, NULL, NULL);

    if (!NT_SUCCESS(status)) {
        *SdpError = MapNtStatusToSdpError(status);
        return status;
    }

    status = info.Walk(AttributeRange, AttributeRangeCount, Stream, Size);

    if (!NT_SUCCESS(status)) {
        //
        // Clean up and leave
        //
        while (!IsListEmpty(&info.ListHead)) {
            entry = RemoveHeadList(&info.ListHead);
            ai = CONTAINING_RECORD(entry, ATTRIBUTE_INFO, Link);
            SdpFreePool(ai);
        }

        *SdpError = MapNtStatusToSdpError(status);
        return status;
    }

    sequenceSize = 0;

    ULONG hdrSize = sizeof(UCHAR) + sizeof(USHORT);

    for (entry = info.ListHead.Flink;
         entry != &info.ListHead;
         entry = entry->Flink) {
        ai = CONTAINING_RECORD(entry, ATTRIBUTE_INFO, Link);

        // if (g_Support10B) {
        //     if ((ai->StreamSize + sequenceSize) <= (g_MaxAttribCount + hdrSize)) {
        //         sequenceSize += ai->StreamSize;
        //     }
        // }
        // else {
            //
            // Add the size of the attribute identifier (including element
            // header)
            //
        //     sequenceSize += sizeof(UCHAR)  + // element header;
        //                     sizeof(USHORT) + // attibute ID
        //                     ai->StreamSize;  // attribute value (already includes header)
        // }

        //
        // Add the size of the attribute identifier (including element
        // header)
        //
        sequenceSize += sizeof(UCHAR)  + // element header;
                        sizeof(USHORT) + // attibute ID
                        ai->StreamSize;  // attribute value (already includes header)
    }

    if (IsListEmpty(&info.ListHead)) {
        *ppEntry = NULL;
        return STATUS_SUCCESS;
    }

    status = CreateAttributeSearchReply(&info,
                                        sequenceSize,
                                        ppEntry);

    if (!NT_SUCCESS(status)) {
        *SdpError = MapNtStatusToSdpError(status);
    }

    return status;
}

NTSTATUS
SdpFindAttributeSequenceInStream(
    PUCHAR Stream,
    ULONG Size,
    SdpAttributeRange * AttributeRange,
    ULONG AttributeRangeCount,
    PSDP_STREAM_ENTRY *ppEntry,
    PSDP_ERROR SdpError
    )
{
    return SdpFindAttributeInStreamInternal(Stream, 
                                            Size,
                                            AttributeRange,
                                            AttributeRangeCount,
                                            ppEntry,
                                            SdpError
                                            );
}

NTSTATUS SdpFindAttributeInStream(PUCHAR Stream, ULONG Size, USHORT Attrib, PUCHAR *ppStream, ULONG *pSize)
{
    NTSTATUS status;

    status = ValidateStream(Stream, Size, NULL, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    SdpAttributeRange sar;
    sar.minAttribute = sar.maxAttribute = Attrib;

    FindStreamInfo info(TRUE);

    status = info.Walk(&sar, 1, Stream, Size);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    if (info.pStream != NULL) {
        *ppStream = info.pStream;
        *pSize = info.streamSize;

        status = STATUS_SUCCESS;
    }
    else {
        status = STATUS_NOT_FOUND;
    }

    return status;
}

NTSTATUS SdpFindAttributeInTree(PSDP_NODE Parent, USHORT AttribId, PSDP_NODE *Attribute)
{
    PLIST_ENTRY current;
    PSDP_NODE node, root;

	*Attribute = NULL;
    if (IsListEmpty(&Parent->hdr.Link)) {
        return STATUS_NOT_FOUND; 
    }
    root = CONTAINING_RECORD(Parent->hdr.Link.Flink, SDP_NODE, hdr.Link);

    if (root->hdr.Type != SDP_TYPE_SEQUENCE) {
        return STATUS_INVALID_PARAMETER;
    }

    if (IsListEmpty(&root->u.sequence.Link)) {
        return STATUS_NOT_FOUND;
    }

	if ((GetListLength(&root->u.sequence.Link) % 2) != 0) {
		//
		// there is an odd number of links in the sequence, it is malformed
		//
		return STATUS_INVALID_PARAMETER;
	}

	//
	// node is the attribute id, current is its value
	//
    node = CONTAINING_RECORD(root->u.sequence.Link.Flink, SDP_NODE, hdr.Link);
    current = node->hdr.Link.Flink;

    while (1) {
        if (node->hdr.Type != SDP_TYPE_UINT ||
            node->hdr.SpecificType != SDP_ST_UINT16 ||
            node->DataSize != sizeof(AttribId)) {
            return STATUS_INVALID_PARAMETER;
        }
        else if (node->u.uint16 == AttribId) {
            *Attribute = CONTAINING_RECORD(current, SDP_NODE, hdr.Link);
			return STATUS_SUCCESS;
        }
        else {
			PSDP_NODE_HEADER header;

            current = current->Flink;

			header = CONTAINING_RECORD(current, SDP_NODE_HEADER, Link);

            //
            // Check to see if we got to the end of the list
            //
            if (header->Type == SDP_TYPE_LISTHEAD) {
                return STATUS_NOT_FOUND;
            }

            node = CONTAINING_RECORD(current, SDP_NODE, hdr.Link);
            current = current->Flink;
        }
    }
}

