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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
