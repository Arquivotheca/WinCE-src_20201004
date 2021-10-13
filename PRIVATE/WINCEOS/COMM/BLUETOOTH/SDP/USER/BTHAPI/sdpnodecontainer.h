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
    
// SdpNodeContainer.h : Declaration of the CSdpNodeContainer

#ifndef __SDPNODECONTAINER_H_
#define __SDPNODECONTAINER_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CSdpNodeContainer
class CSdpNodeContainer : public ISdpNodeContainer
{
public:
    CSdpNodeContainer()
    {
        Init();
    }
    ~CSdpNodeContainer();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();


// ISdpNodeContainer
public:
    STDMETHOD(GetNodeStringData)(ULONG nodeIndex, NodeData *pData);
    STDMETHOD(CreateFromStream)(UCHAR *pStream, ULONG size);
    STDMETHOD(GetNodeCount)(ULONG *pNodeCount);
    STDMETHOD(LockContainer)(UCHAR lock);
    STDMETHOD(GetNode)(ULONG nodeIndex, NodeData *pData);
    STDMETHOD(SetNode)(ULONG nodeIndex, NodeData *pData);
    STDMETHOD(Walk)(ISdpWalk *pWalk);
    STDMETHOD(SetType)(NodeContainerType type);
    STDMETHOD(GetType)(NodeContainerType *pType);
    STDMETHOD(AppendNode)(NodeData *pData);
    STDMETHOD(WriteStream)(UCHAR *pStream, ULONG *pNumBytesWritten);
    STDMETHOD(CreateStream)(UCHAR **ppStream, ULONG *pSize);
    STDMETHOD(GetStreamSize)(ULONG *pSize);

protected:
    void Init();

    NodeContainerType containerType;
    LONG locked;
    ULONG streamSize;
    UCHAR streamSizeValid;
    LONG refCount;

    SdpNodeList nodeList;

    CRITICAL_SECTION listLock;
};

#endif //__SDPNODECONTAINER_H_

