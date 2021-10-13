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
    
// SdpWalk.h : Declaration of the CSdpWalk

#ifndef __SDPWALK_H_
#define __SDPWALK_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CSdpWalk
class CSdpWalk : 
    public ISdpWalk
{
public:
    CSdpWalk()
    {
        refCount = 0;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    HRESULT FinalConstruct()
    {
        return S_OK;
    }

    void FinalRelease()
    {
        ;
    }
// ISdpWalk
public:
    STDMETHOD(WalkStream)(USHORT type, USHORT specificType, UCHAR *pStream,  ULONG streamSize);
    STDMETHOD(WalkNode)(NodeData *pData, ULONG state);

protected:
    LONG refCount;
};

#endif //__SDPWALK_H_

