//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//


#include <windows.h>
#include <Control.h>



//
//  A Key.  It has a name and a value (equivalent to struct {BSTR, BSTR})
//
typedef struct 
{
    BSTR KeyName;
    BSTR Value;
} KeyVal, *PKeyVal;


//
//  CMediaMetadata.  It has a CHive and implements IMetaInfo
//

class CMediaMetadata : public IMediaMetadata
{
public:
    CMediaMetadata(void);
    ~CMediaMetadata(void);

    // IMetaInfo
    HRESULT STDMETHODCALLTYPE Get(BSTR KeyIn, BSTR *pValOut);
    
    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
    ULONG STDMETHODCALLTYPE AddRef(void);
    ULONG STDMETHODCALLTYPE Release(void);

    // Local method for filling the bag
    HRESULT Add(LPCWSTR KeyIn, LPCWSTR ValIn);

private:
    ULONG   m_dwRefCount;
    ULONG   m_nKeysAlloc;
    ULONG   m_nKeysInuse;
    PKeyVal m_pKeys;
};

