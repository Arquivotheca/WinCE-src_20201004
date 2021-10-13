//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __SDPLIB_H__                               
#define __SDPLIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sdpnode.h"


#ifndef NTSTATUS
typedef LONG NTSTATUS;                
#endif

#define SDP_TYPE_LISTHEAD     0x0021                   

#define SDP_ST_CONTAINER_STREAM    0x2001 
#define SDP_ST_CONTAINER_INTERFACE 0x2002

typedef struct _SDP_STREAM_ENTRY {
    LIST_ENTRY link;
    ULONG streamSize;
    UCHAR stream[1];
} SDP_STREAM_ENTRY, *PSDP_STREAM_ENTRY;

typedef struct _PSM_PROTOCOL_PAIR {
    GUID protocol;
    USHORT psm;
} PSM_PROTOCOL_PAIR, *PPSM_PROTOCOL_PAIR;

typedef struct _PSM_LIST {
    ULONG count;
    PSM_PROTOCOL_PAIR list[1];
} PSM_LIST, *PPSM_LIST;

#define SIZE_8_BITS        0xFF
#define SIZE_16_BITS     0xFFFF
#define SIZE_32_BITS 0xFFFFFFFF

#define TYPE_BIT_SIZE  5
#define TYPE_SHIFT_VAL (8 - TYPE_BIT_SIZE)
#define TYPE_MASK      ((UCHAR) 0x1F)

#define SPECIFIC_TYPE_MASK  0x07
#define SIZE_INDEX_MASK     SPECIFIC_TYPE_MASK
#define SPECIFIC_TYPE_SHIFT 8

#define SIZE_INDEX_ZERO           0
#define SIZE_INDEX_NEXT_8_BITS    5
#define SIZE_INDEX_NEXT_16_BITS   6 
#define SIZE_INDEX_NEXT_32_BITS   7

#define FMT_TYPE(_type) ((((_type) & TYPE_MASK) << TYPE_SHIFT_VAL))
#define FMT_SIZE_INDEX_FROM_ST(_spectype) ((UCHAR) \
   (((_spectype) & (SPECIFIC_TYPE_MASK << SPECIFIC_TYPE_SHIFT)) >> SPECIFIC_TYPE_SHIFT))

void      SdpInitializeNodeHeader(PSDP_NODE_HEADER Header);
NTSTATUS  SdpAddAttributeToNodeHeader(PSDP_NODE_HEADER Header, USHORT AttribId, PSDP_NODE AttribValue);

PSDP_NODE SdpCreateNodeTree();
NTSTATUS  SdpFreeTree(PSDP_NODE Tree);
NTSTATUS  SdpFreeNode(PSDP_NODE Node);
NTSTATUS  SdpFreeOrphanedNode(PSDP_NODE Node);
void      SdpReleaseContainer(ISdpNodeContainer *container);

PSDP_NODE SdpCreateNode();

PSDP_NODE SdpCreateNodeNil(void);

PSDP_NODE SdpCreateNodeUInt128(PSDP_ULARGE_INTEGER_16 puli16Val);
PSDP_NODE SdpCreateNodeUInt64(ULONGLONG ullVal);
PSDP_NODE SdpCreateNodeUInt32(ULONG ulVal);
PSDP_NODE SdpCreateNodeUInt16(USHORT usVal);
PSDP_NODE SdpCreateNodeUInt8(UCHAR ucVal);

PSDP_NODE SdpCreateNodeInt128(PSDP_LARGE_INTEGER_16 uil16Val);
PSDP_NODE SdpCreateNodeInt64(LONGLONG llVal);
PSDP_NODE SdpCreateNodeInt32(LONG lVal);
PSDP_NODE SdpCreateNodeInt16(SHORT sVal);
PSDP_NODE SdpCreateNodeInt8(CHAR cVal);

#define   SdpCreateNodeUUID SdpCreateNodeUUID128
PSDP_NODE SdpCreateNodeUUID128(const GUID *uuid);
PSDP_NODE SdpCreateNodeUUID32(ULONG uuidVal4);
PSDP_NODE SdpCreateNodeUUID16(USHORT uuidVal2);

PSDP_NODE SdpCreateNodeString(PCHAR string, ULONG stringLength);

PSDP_NODE SdpCreateNodeBoolean(UCHAR  bVal);

PSDP_NODE SdpCreateNodeSequence(void);

PSDP_NODE SdpCreateNodeAlternative(void);

PSDP_NODE SdpCreateNodeUrl(PCHAR url, ULONG urlLength);

NTSTATUS  SdpAppendNodeToContainerNode(PSDP_NODE Parent, PSDP_NODE Node);
NTSTATUS  SdpAddAttributeToTree(PSDP_NODE Tree, USHORT AttribId, PSDP_NODE AttribValue); 

typedef struct _SDP_ATTRIBUTE_RANGE {
    USHORT MinValue;
    USHORT MaxValue;
} SDP_ATTRIBUTE_RANGE, *PSDP_ATTRIBUTE_RANGE;

NTSTATUS SdpFindAttributeInTree(PSDP_NODE Root, USHORT AttribId, PSDP_NODE *Attribute);

NTSTATUS SdpFindAttributeInStream(PUCHAR Stream, ULONG Size, USHORT Attrib, PUCHAR *ppStream, ULONG *pSize);

NTSTATUS SdpFindAttributeSequenceInStream(PUCHAR Stream,
                                          ULONG Size,
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
                                          PSDP_ATTRIBUTE_RANGE AttributeRange,
#else
                                          SdpAttributeRange *AttributeRange,
#endif
                                          ULONG AttributeRangeCount,
                                          PSDP_STREAM_ENTRY *ppEntry,
                                          PSDP_ERROR SdpError);

SDP_ERROR MapNtStatusToSdpError(NTSTATUS Status);

NTSTATUS  SdpStreamFromTree(PSDP_NODE Root, PUCHAR *Stream, PULONG Size);
NTSTATUS  SdpStreamFromTreeEx(PSDP_NODE Root, PUCHAR *Stream, PULONG Size, ULONG HeaderSize, ULONG TailSize);
NTSTATUS  SdpTreeFromStream(PUCHAR Stream, ULONG Size, PSDP_NODE_HEADER Root, PSDP_NODE *Node, UCHAR FullParse);

typedef NTSTATUS (*PSDP_STREAM_WALK_FUNC)(PVOID Context, UCHAR DataType, ULONG DataSize, PUCHAR Data,ULONG DataStorageSize);
NTSTATUS  SdpWalkStream(PUCHAR Stream, ULONG Size, PSDP_STREAM_WALK_FUNC WalkFunc, PVOID WalkContext);

VOID      SdpFreePool(PVOID Memory);
PVOID     SdpAllocatePool(SIZE_T Size);

void      SdpByteSwapUuid128(GUID *uuid128From, GUID *uuid128To);
void      SdpByteSwapUint128(PSDP_ULARGE_INTEGER_16 pInUint128, PSDP_ULARGE_INTEGER_16 pOutUint128);
ULONGLONG SdpByteSwapUint64(ULONGLONG uint64);
ULONG     SdpByteSwapUint32(ULONG uint32);
USHORT    SdpByteSwapUint16(USHORT uint16);

void SdpRetrieveUuid128(PUCHAR Stream, GUID *uuidVal);
void SdpRetrieveUint128(PUCHAR Stream, PSDP_ULARGE_INTEGER_16 pUint128);
void SdpRetrieveUint64(PUCHAR Stream, PULONGLONG pUint64);
void SdpRetrieveUint32(PUCHAR Stream, PULONG pUint32);
void SdpRetrieveUint16(PUCHAR Stream, PUSHORT pUint16);

void SdpRetrieveVariableSize(UCHAR *Stream, UCHAR SizeIndex, ULONG *ElementSize, ULONG *StorageSize);
void SdpRetrieveUuidFromStream(PUCHAR Stream, ULONG DataSize, GUID *pUuid, UCHAR bigEndian);
void SdpNormalizeUuid(PSDP_NODE pUuid, GUID* uuid);
NTSTATUS SdpValidateProtocolContainer(PSDP_NODE pContainer, PPSM_LIST pPsmList);

#define SdpRetrieveHeader(_stream, _type, _sizeidx)                            \
{                                                                              \
    (_type) = (UCHAR) ((*(_stream)) & (TYPE_MASK << TYPE_SHIFT_VAL)) >> TYPE_SHIFT_VAL;\
    (_sizeidx) =(UCHAR) *(_stream) & SIZE_INDEX_MASK;                               \
}

NTSTATUS ValidateStream(PUCHAR Stream, ULONG Size, PULONG NumEntries, PULONG ExtraPool, PULONG_PTR ErrorByte);
NTSTATUS SdpIsStreamRecord(PUCHAR Stream, ULONG Size);

#define VERIFY_SINGLE_ATTRIBUTE          0x00000001
#define VERIFY_CHECK_MANDATORY_LOCAL     0x00000002
#define VERIFY_CHECK_MANDATORY_REMOTE    0x00000004
#define VERIFY_STREAM_IS_ATTRIBUTE_VALUE 0x00000008

#define VERIFY_CHECK_MANDATORY_ALL      (VERIFY_CHECK_MANDATORY_LOCAL | VERIFY_CHECK_MANDATORY_REMOTE)

NTSTATUS SdpVerifyServiceRecord(UCHAR *pStream, ULONG size, ULONG flags, USHORT *pAttribId);

NTSTATUS SdpVerifySequenceOf(UCHAR *pStream,
                             ULONG size,
                             UCHAR ofType,
                             UCHAR *pSpecSizes,
                             ULONG *pNumFound,
                             PSDP_STREAM_WALK_FUNC pFunc,
                             PVOID pContext);

typedef struct _SDP_ATTRIBUTE_INFO {
    PUCHAR AttributeStream;
    ULONG AttributeStreamSize;
    USHORT AttributeId;
} SDP_ATTRIBUTE_INFO, *PSDP_ATTRIBUTE_INFO;

VOID  Sdp_InitializeListHead(PLIST_ENTRY ListHead);
UCHAR Sdp_IsListEmpty(PLIST_ENTRY ListHead);
VOID  Sdp_InsertHeadList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry);
VOID  Sdp_InsertTailList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry);
PLIST_ENTRY Sdp_RemoveHeadList(PLIST_ENTRY ListHead);
VOID  Sdp_RemoveEntryList(PLIST_ENTRY Entry);
VOID  Sdp_InsertEntryList(PLIST_ENTRY Previous, PLIST_ENTRY Entry);

NTSTATUS NodeToStream(PSDP_NODE Node, PUCHAR Stream);

NTSTATUS ComputeNodeListSize(PSDP_NODE Node, PULONG Size);

ULONG    GetContainerHeaderSize(ULONG ContainerSize);

PUCHAR WriteVariableSizeToStream(UCHAR Type, ULONG DataSize, PUCHAR Stream);
PUCHAR WriteLeafToStream(PSDP_NODE Node, PUCHAR Stream);

#define IsEqualUuid(u1, u2) (RtlEqualMemory((u1), (u2), sizeof(*u1)))

#if defined (UNDER_CE) || defined (WINCE_EMULATION)
int GetRfcommCidFromResponse(unsigned char *pInBuf, DWORD cOutBuf, unsigned long *pbtChannel);
#endif

#ifdef __cplusplus
};
#endif
            
#endif // __SDPLIB_H__
