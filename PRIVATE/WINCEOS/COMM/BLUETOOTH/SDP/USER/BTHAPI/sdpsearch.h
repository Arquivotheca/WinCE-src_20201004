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
    
// SdpSearch.h : Declaration of the CSdpSearch

#ifndef __SDPSEARCH_H_
#define __SDPSEARCH_H_

#include "resource.h"       // main symbols
#if ! (defined (UNDER_CE) || defined (WINCE_EMULATION))
#include "BluetoothEvents.h"
#endif

/////////////////////////////////////////////////////////////////////////////
// CSdpSearch
class CSdpSearch : 
    public ISdpSearch
{
public:
    CSdpSearch()  {refCount = 0;}
    ~CSdpSearch() {;}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();


// ISdpSearch
public:
    STDMETHOD(Begin)(ULONGLONG *pAddress, ULONG fConnect);
    STDMETHOD(End)();
    STDMETHOD(ServiceSearch)(SdpQueryUuid *pUuidList,
                             ULONG listSize,
                             ULONG *pHandles,
                             USHORT *pNumHandles);
    STDMETHOD(AttributeSearch)(ULONG handle,
                               SdpAttributeRange *pRangeList,
                               ULONG numRanges,
                               ISdpRecord **ppSdpRecord);
    STDMETHOD(ServiceAndAttributeSearch)(SdpQueryUuid *pUuidList,
                                         ULONG listSize,
                                         SdpAttributeRange *pRangeList,
                                         ULONG numRanges,
                                         ISdpRecord ***pppSdpRecords,
                                         ULONG *pNumRecords);
// BluetoothEvents
public:
    void LocalRadioInsertion(TCHAR *pszDeviceName, BOOL Enumerated);

protected:
    LONG refCount;
};

#endif //__SDPSEARCH_H_

