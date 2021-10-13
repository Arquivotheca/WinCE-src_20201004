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
// HeaderCollection.h: interface for the CHeaderCollection class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(HEADERCOLLECTION_H)
#define HEADERCOLLECTION_H


class CHeaderCollection : public IHeaderCollection
{
public:
	CHeaderCollection();
	~CHeaderCollection();
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

    HRESULT STDMETHODCALLTYPE AddByteArray (byte Id, unsigned long ulSize, byte *pData);
    HRESULT STDMETHODCALLTYPE AddLong (byte Id,unsigned long ulData);
    HRESULT STDMETHODCALLTYPE AddByte (byte Id, byte pData);
    HRESULT STDMETHODCALLTYPE AddUnicodeString (byte Id, LPCWSTR pszData);
    HRESULT STDMETHODCALLTYPE Remove (byte Id);
    HRESULT STDMETHODCALLTYPE RemoveAll ();
    HRESULT STDMETHODCALLTYPE AddCount( unsigned long ulCount);
    HRESULT STDMETHODCALLTYPE AddName( LPCWSTR pszName);
    HRESULT STDMETHODCALLTYPE AddType( unsigned long ulSize,  byte *pData);
    HRESULT STDMETHODCALLTYPE AddLength( unsigned long ulLength);
    HRESULT STDMETHODCALLTYPE AddTimeOld( unsigned long ulTime);
    HRESULT STDMETHODCALLTYPE AddTime(FILETIME * pFiletime);   
    HRESULT STDMETHODCALLTYPE AddDescription( LPCWSTR pszDescription);
    HRESULT STDMETHODCALLTYPE AddTarget( unsigned long ulSize, byte *pData); 
    HRESULT STDMETHODCALLTYPE AddHTTP( unsigned long ulSize, byte *pData);
    HRESULT STDMETHODCALLTYPE AddBody( unsigned long ulSize, byte *pData);
    HRESULT STDMETHODCALLTYPE AddEndOfBody( unsigned long ulSize, byte *pData);
    HRESULT STDMETHODCALLTYPE AddWho( unsigned long ulSize, byte *pData);
    HRESULT STDMETHODCALLTYPE AddConnectionId(unsigned long ulConnectionId);
    HRESULT STDMETHODCALLTYPE AddAppParams(unsigned long ulSize, byte *pData);
    HRESULT STDMETHODCALLTYPE AddObjectClass(unsigned long ulSize, byte *pData);   
    HRESULT STDMETHODCALLTYPE EnumHeaders(LPHEADERENUM *pHeaderEnum);

    HRESULT STDMETHODCALLTYPE InsertHeader(OBEX_HEADER *pHeader);
    HRESULT STDMETHODCALLTYPE InsertHeaderEnd(OBEX_HEADER *pHeader);


   	static void CopyHeader(OBEX_HEADER *pOrig, OBEX_HEADER *pNew);
   	
 private:
    ULONG _refCount;
    CHeaderNode *pHeaders;     
};

#endif // !defined(HEADERCOLLECTION_H)
