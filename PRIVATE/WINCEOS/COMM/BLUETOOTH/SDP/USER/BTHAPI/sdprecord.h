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
    
// SdpRecord.h : Declaration of the CSdpRecord

#ifndef __SDPRECORD_H_
#define __SDPRECORD_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CSdpRecord
class CSdpRecord : 
    public ISdpRecord, IDataObject
{
public:
    CSdpRecord()
    {
        Init();
    }
    ~CSdpRecord();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();


// ISdpRecord
public:
    STDMETHOD(GetIcon)(int cxRes, int cyRes, HICON *phIcon);
    STDMETHOD(GetString)(USHORT offset, USHORT *pLangId, WCHAR **ppString);
    STDMETHOD(GetAttributeList)(USHORT **ppList, ULONG *pListSize);
    STDMETHOD(Walk)(ISdpWalk *pWalk);
    STDMETHOD(GetAttributeAsStream)(USHORT attribute, UCHAR **ppStream, ULONG *pSize);
    STDMETHOD(GetAttribute)(USHORT attribute, NodeData *pNode);
    STDMETHOD(SetAttributeFromStream)(USHORT attribute, UCHAR *pStream, ULONG size);
    STDMETHOD(SetAttribute)(USHORT attribute, NodeData *pNode);
    STDMETHOD(WriteToStream)(UCHAR **ppStream, ULONG *pStreamSize, ULONG preSize, ULONG postSize);
    STDMETHOD(CreateFromStream)(UCHAR *pStream, ULONG size);
    STDMETHOD(GetServiceClass)(LPGUID pServiceClass);

// IDataObject
public:
    STDMETHOD(GetData)(FORMATETC *pformatetcIn, STGMEDIUM *pmedium);
    STDMETHOD(GetDataHere)(FORMATETC *pformatetc, STGMEDIUM *pmedium);
    STDMETHOD(QueryGetData)(FORMATETC *pformatetc);
    STDMETHOD(GetCanonicalFormatEtc)(FORMATETC *pformatectIn, FORMATETC *pformatetcOut);
    STDMETHOD(SetData)(FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease);
    STDMETHOD(EnumFormatEtc)(DWORD dwDirection, IEnumFORMATETC **ppenumFormatEtc);
    STDMETHOD(DAdvise)(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection);
    STDMETHOD(DUnadvise)(DWORD dwConnection);
    STDMETHOD(EnumDAdvise)(IEnumSTATDATA **ppenumAdvise);

protected:
    void Init(void);
    void DestroyRecord(void);
    void LockRecord(UCHAR lock);
    HRESULT GetLangBaseInfo(USHORT *pLangId, USHORT *pMibeNum, USHORT *pAttribId);
    PSDP_NODE GetAttributeValueNode(USHORT attribute);

    SdpNodeList nodeList;

    CRITICAL_SECTION listLock;
    LONG locked;
    LONG refCount;
};

#endif //__SDPRECORD_H_

