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
#pragma code_seg("PAGE", "CODE")

// Removed for optimization
static ULONG IndexToDataSize[5] = { 1, 2, 4, 8, 16 };
#endif

NTSTATUS SdpWalkStream(PUCHAR Stream, ULONG Size, PSDP_STREAM_WALK_FUNC WalkFunc, PVOID WalkContext)
{
#define INC_STREAM(_inc)        \
{                               \
    Stream += (_inc);           \
    Size -= (_inc);             \
}
    NTSTATUS status;

    SdpStack stack;
    SD_STACK_ENTRY *stackEntry;

    ULONG dataSize, elementSize, storageSize;  // ulVal
//  USHORT usVal;
//  UCHAR ucVal;

    UCHAR type, sizeIndex;

    status = ValidateStream(Stream,Size, NULL, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    while (Size) {
        SdpRetrieveHeader(Stream, type, sizeIndex);
        INC_STREAM(1);

        switch (type) {
        case SDP_TYPE_NIL:
            status = (*WalkFunc)(WalkContext, type, 0, Stream,0);
            if (!NT_SUCCESS(status)) {
                return status;
            }
            break;

        case SDP_TYPE_BOOLEAN:
            status = (*WalkFunc)(WalkContext, type, sizeof(UCHAR), Stream,0); 
            if (!NT_SUCCESS(status)) {
                return status;
            }
            INC_STREAM(1);
            break;

        case SDP_TYPE_UUID: // valid indecies are 1,2,4
        case SDP_TYPE_UINT: // valid indecies are 0-4
        case SDP_TYPE_INT:  // valid indecies are 0-4
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
            dataSize = IndexToDataSize[sizeIndex];
#else
            // Optimize
            dataSize = 1 << sizeIndex;
#endif
            status = (*WalkFunc)(WalkContext, type, dataSize, Stream,0);
            if (!NT_SUCCESS(status)) {
                return status;
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
            SdpRetrieveVariableSize(Stream, sizeIndex, &elementSize, &storageSize);
            INC_STREAM(storageSize);

            status = (*WalkFunc)(WalkContext, type, elementSize, Stream,storageSize);

            //
            // don't walk into this container, just skip over its contents
            //
            if (status == STATUS_REPARSE_POINT_NOT_RESOLVED &&
                (type == SDP_TYPE_ALTERNATIVE || type == SDP_TYPE_SEQUENCE)) {
                //
                // since STATUS_REPARSE_POINT_NOT_RESOLVED is a special value,
                // we coerce the result to success
                //
                status = STATUS_SUCCESS;
                INC_STREAM(elementSize);
                break;
            }
            else if (!NT_SUCCESS(status)) {
                return status;
            }

            if (type == SDP_TYPE_STRING || type == SDP_TYPE_URL) {
                INC_STREAM(elementSize);
            }
            else {
                if (elementSize > 0) {
                    status = stack.Push((PSDP_NODE) type, Size - elementSize);
                    if (!NT_SUCCESS(status)) {
                        return status;
                    }

                    //
                    // skip past the storage for the size and move on
                    //
                    Size = elementSize;
                    continue;
                }
                else {
                    status = (*WalkFunc)(WalkContext, type, elementSize, NULL,0);
                    if (!NT_SUCCESS(status)) {
                        return status;
                    }
                }
            }
            break;
        }


        while (Size == 0) {
            //
            // Check to see if we have pushed any items.  If not, then we are 
            // done
            //
            if (stack.Depth() == 0) {
                return STATUS_SUCCESS;
            }

            stackEntry = stack.Pop();            

            status = (*WalkFunc)(WalkContext, (UCHAR) stackEntry->Node, 0, NULL,0);
            if (!NT_SUCCESS(status)) {
                return status;
            }

            Size = stackEntry->Size;
        }
    }

    return STATUS_SUCCESS;
#undef INC_STREAM
}

