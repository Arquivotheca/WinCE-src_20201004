//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef  __SDP_PRIV_H__
#define  __SDP_PRIV_H__

#include "stack.h"

#ifdef __cplusplus
extern "C" {
#endif

#if DBG
#define CAssertF(c)      { switch(0) case (c): case 0: ;}
#else
#define CAssertF(c)      
#endif // DBG

#ifdef _WIN64
    #define SHIFT_VAL (3)
#else
    #define SHIFT_VAL (2)
#endif
#define ROUND_SIZE(x) ((( (x) + (sizeof(PVOID) - 1) ) >> SHIFT_VAL) << SHIFT_VAL)

PSDP_NODE SdpInitializeNodeTree(PSDP_NODE Node);
PSDP_NODE SdpInitializeNodeNil(PSDP_NODE Node);
PSDP_NODE SdpInitializeNodeUInt128(PSDP_NODE Node, PSDP_ULARGE_INTEGER_16 puli16Val);
PSDP_NODE SdpInitializeNodeUInt64(PSDP_NODE Node, ULONGLONG ullVal);
PSDP_NODE SdpInitializeNodeUInt32(PSDP_NODE Node, ULONG ulVal);
PSDP_NODE SdpInitializeNodeUInt16(PSDP_NODE Node, USHORT usVal);
PSDP_NODE SdpInitializeNodeUInt8(PSDP_NODE Node, UCHAR cVal);
PSDP_NODE SdpInitializeNodeInt128(PSDP_NODE Node, PSDP_LARGE_INTEGER_16 uil16Val);
PSDP_NODE SdpInitializeNodeInt64(PSDP_NODE Node, LONGLONG llVal);
PSDP_NODE SdpInitializeNodeInt32(PSDP_NODE Node, LONG lVal);
PSDP_NODE SdpInitializeNodeInt16(PSDP_NODE Node, SHORT sVal);
PSDP_NODE SdpInitializeNodeInt8(PSDP_NODE Node, CHAR cVal);
PSDP_NODE SdpInitializeNodeUUID128(PSDP_NODE Node, const GUID *uuid);
PSDP_NODE SdpInitializeNodeUUID32(PSDP_NODE Node, ULONG uuidVal4);
PSDP_NODE SdpInitializeNodeUUID16(PSDP_NODE Node, USHORT uuidVal2);
PSDP_NODE SdpInitializeNodeString(PSDP_NODE Node, PCHAR string, ULONG stringLength, PCHAR nodeBuffer);
PSDP_NODE SdpInitializeNodeBoolean(PSDP_NODE Node, UCHAR  bVal);
PSDP_NODE SdpInitializeNodeSequence(PSDP_NODE Node);
PSDP_NODE SdpInitializeNodeAlternative(PSDP_NODE Node);
PSDP_NODE SdpInitializeNodeContainer(PSDP_NODE Node, PUCHAR Stream, ULONG Size);
PSDP_NODE SdpInitializeNodeUrl(PSDP_NODE Node, PCHAR url, ULONG urlLength, PCHAR entryBuffer);

ULONG     ComputeNumberOfEntries(PUCHAR Stream, ULONG Size, UCHAR FullParse);
ULONG     GetListLength(PLIST_ENTRY Head);

PVOID     SdpAllocatePool(SIZE_T Size);
PVOID     SdpAllocatePoolEx(SIZE_T Size, UCHAR PagedAllocation);

NTSTATUS SdpFreeNode(PSDP_NODE Node);

SDP_ERROR MapNtStatusToSdpError(NTSTATUS Status);

typedef struct _SDP_NODE_REF {
	PVOID Alloc;
	LONG RefCount;
} SDP_NODE_REF, *PSDP_NODE_REF;

void DecrementNodeRef(PSDP_NODE_REF NodeRef);

ULONGLONG SdpUlonglongByteSwap(IN ULONGLONG Source);

#ifndef InsertEntryList
    //
    //  VOID
    //  InsertEntryList(
    //      PLIST_ENTRY Previous,
    //      PLIST_ENTRY Entry
    //      );
    //
    
    #define InsertEntryList(Previous, Entry) {                              \
        PLIST_ENTRY _EX_Next = (Previous)->Flink;                           \
        PLIST_ENTRY _EX_Previous = (Previous);                              \
        (Entry)->Flink = _EX_Next;                                          \
        (Entry)->Blink = _EX_Previous;                                      \
        _EX_Next->Blink = (Entry);                                          \
        _EX_Previous->Flink = (Entry);                                      \
        }
#endif 

#define SdpRetrieveUlonglong(DEST_ADDRESS,SRC_ADDRESS)                  \
         if ((ULONG_PTR)SRC_ADDRESS & LONGLONG_MASK) {                  \
             ((PUCHAR) DEST_ADDRESS)[0] = ((PUCHAR) SRC_ADDRESS)[0];    \
             ((PUCHAR) DEST_ADDRESS)[1] = ((PUCHAR) SRC_ADDRESS)[1];    \
             ((PUCHAR) DEST_ADDRESS)[2] = ((PUCHAR) SRC_ADDRESS)[2];    \
             ((PUCHAR) DEST_ADDRESS)[3] = ((PUCHAR) SRC_ADDRESS)[3];    \
             ((PUCHAR) DEST_ADDRESS)[4] = ((PUCHAR) SRC_ADDRESS)[4];    \
             ((PUCHAR) DEST_ADDRESS)[5] = ((PUCHAR) SRC_ADDRESS)[5];    \
             ((PUCHAR) DEST_ADDRESS)[6] = ((PUCHAR) SRC_ADDRESS)[6];    \
             ((PUCHAR) DEST_ADDRESS)[7] = ((PUCHAR) SRC_ADDRESS)[7];    \
         }                                                              \
         else {                                                         \
             *((PULONGLONG) DEST_ADDRESS) = *((PULONGLONG) SRC_ADDRESS);\
         }


#ifdef __cplusplus
}
#endif

#endif //  __SDP_PRIV_H__
