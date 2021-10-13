//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef _SDPNODE_H__
#define _SDPNODE_H__

#ifdef __cplusplus
extern "C" {
#endif

struct ISdpNodeContainer;

typedef struct _SDP_NODE_HEADER {
    LIST_ENTRY Link;
    USHORT Type;
    USHORT SpecificType;
} SDP_NODE_HEADER, *PSDP_NODE_HEADER;

typedef union _SDP_NODE_DATA {
    // the nil type contains no data, so no storage is necessary

    // 16 byte integers
    //
    // ISSUE is there a better way to represent a 16 byte int???
    //
    SDP_LARGE_INTEGER_16 int128;
    SDP_ULARGE_INTEGER_16 uint128;

    // UUID 
    GUID uuid128;
    ULONG uuid32;
    USHORT uuid16;

    // 8 byte integers
    LONGLONG int64;
    ULONGLONG uint64;

    // 4 byte integers
    LONG int32;
    ULONG uint32;

    // 2 byte integers
    SHORT int16;
    USHORT uint16;

    // 1 bytes integers
    CHAR int8;
    UCHAR uint8;

    // Boolean
    UCHAR boolean;

    // string 
    PCHAR string;

    // URL
    PCHAR url;

    // Sequence 
	SDP_NODE_HEADER sequence;

    // Alt list
    SDP_NODE_HEADER alternative;

    ISdpNodeContainer *container;

    struct {
        PUCHAR stream;
        ULONG streamLength;
    };

} SDP_NODE_DATA, *PSDP_NODE_DATA;

typedef struct _SDP_NODE {
	SDP_NODE_HEADER  hdr;

    ULONG DataSize;
    
	SDP_NODE_DATA u;
    
	PVOID Reserved;
} SDP_NODE, *PSDP_NODE;

#define SdpNode_GetType(pNode) ((pNode)->hdr.Type)
#define SdpNode_GetSpecificType(pNode) ((pNode)->hdr.SpecificType)

#define ListEntryToSdpNode(pListEntry) CONTAINING_RECORD((pListEntry), SDP_NODE, hdr.Link)

#define SdpNode_GetNextEntry(pNode) ((pNode)->hdr.Link.Flink)
#define SdpNode_GetPrevEntry(pNode) ((pNode)->hdr.Link.Blink)

#define SdpNode_SequenceGetListHead(pNode) (&(pNode)->u.sequence.Link)
#define SdpNode_SequenceGetFirstNode(pNode) ((pNode)->u.sequence.Link.Flink)

#define SdpNode_AlternativeGetListHead(pNode) (&(pNode)->u.alternative.Link)
#define SdpNode_AlternativeGetFirstNode(pNode) ((pNode)->u.alternative.Link.Flink)

#ifdef __cplusplus
}
#endif

#endif // _SDPNODE_H__
