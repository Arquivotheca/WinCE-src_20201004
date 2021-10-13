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
