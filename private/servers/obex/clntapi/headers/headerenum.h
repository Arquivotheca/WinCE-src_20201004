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
// HeaderEnum.h: interface for the CHeaderEnum class.
//
#if !defined(HEADERENUM_H)
#define HEADERENUM_H


struct CHeaderNode
{
    OBEX_HEADER *pItem;
    CHeaderNode *pNext;
};


class CHeaderEnum : public IHeaderEnum
{
public:
	CHeaderEnum();
	~CHeaderEnum();

    //Next:  return 'celt' items
    HRESULT STDMETHODCALLTYPE Next (ULONG celt, OBEX_HEADER **rgelt, ULONG *pceltFetched);

    //Skip: skip 'celt' items
    HRESULT STDMETHODCALLTYPE Skip(ULONG celt);

    //Reset: reset enumeration to beginning
    HRESULT STDMETHODCALLTYPE Reset();

    //Clone: clone the enumeration, including location
    HRESULT STDMETHODCALLTYPE Clone(IHeaderEnum **ppenum);

    //insert a device into the list
    HRESULT STDMETHODCALLTYPE InsertFront(OBEX_HEADER *pHeader);
    HRESULT STDMETHODCALLTYPE InsertBack(OBEX_HEADER *pHeader);

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

    void RemoveAll();

private:
    CHeaderNode *pListHead;
    CHeaderNode *pListCurrent;
    CHeaderNode *pListTail; //used for O(1) inserts

    ULONG _refCount; 

};

#endif // !defined(HEADERENUM_H)
