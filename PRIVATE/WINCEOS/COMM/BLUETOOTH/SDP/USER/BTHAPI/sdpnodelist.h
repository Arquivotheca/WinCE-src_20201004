//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __SDPNODELIST_H__
#define __SDPNODELIST_H__

struct SdpNodeList : public SDP_NODE_HEADER {
    SdpNodeList();
    ~SdpNodeList();

    HRESULT CreateFromStream(UCHAR *pStream, ULONG size);
    ULONG WriteStream(PUCHAR pStream);
    ULONG GetStreamSize();

    PSDP_NODE GetNode(ULONG nodeIndex);

    HRESULT Walk(ISdpWalk *pWalk);

    void Lock(UCHAR lock);
    void Destroy();

    ULONG nodeCount;
};

#endif // __SDPNODELIST_H__
