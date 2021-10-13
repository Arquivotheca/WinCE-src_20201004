//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __UTIL_H__
#define __UTIL_H__

HRESULT CreateSdpNodeFromNodeData(NodeData *pData, PSDP_NODE *pNode);
void    CreateNodeDataFromSdpNode(PSDP_NODE pNode, NodeData *pData);
HRESULT CopyStringDataToNodeData(CHAR *str, ULONG strLength, SdpString *pString);

HRESULT pNormalizeUuid(NodeData *pDataUuid, GUID* pNormalizedUuid);

HRESULT MapNtStatusToHresult(NTSTATUS Status);

struct ListLock {
    ListLock(CRITICAL_SECTION *CS);
    ~ListLock();
    
    CRITICAL_SECTION *cs;
};

#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
struct MinorDeviceInfo {
    UCHAR minorClass;
    int iImage;
    UINT stringId;
};

struct MajorDeviceInfo {
    UCHAR majorClass;
    ULONG minorMask;
    ULONG minorShift;
    int iImage;
    UINT stringId;
    MinorDeviceInfo *minorDeviceInfo;
    ULONG numMinorDeviceInfo;
};

MajorDeviceInfo* GetMajorClassInfo(bt_cod cod);
MinorDeviceInfo* GetMinorClassInfo(bt_cod cod, MajorDeviceInfo* MajorInfo);
#endif // UNDER_CE

#endif __UTIL_H__
