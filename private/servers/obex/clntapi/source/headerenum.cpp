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
#include "common.h"
#include "HeaderEnum.h"
#include "obexpacketinfo.h"

CHeaderEnum::CHeaderEnum() :
    _refCount(1),
    pListHead(0),
    pListTail(0),
    pListCurrent(0)
{
    DEBUGMSG(OBEX_HEADERENUM_ZONE,(L"CHeaderEnum::CHeaderEnum()\n"));
}

CHeaderEnum::~CHeaderEnum()
{
    DEBUGMSG(OBEX_HEADERENUM_ZONE,(L"CHeaderEnum::~CHeaderEnum()\n"));
    
    while(pListHead)
    {
        CHeaderNode *pTemp = pListHead;
        pListHead = pListHead->pNext;
        delete pTemp;
    }  
}
   
//return 'celt' items
HRESULT STDMETHODCALLTYPE
CHeaderEnum::Next (ULONG celt, OBEX_HEADER **rgelt, ULONG *pceltFetched)
{
    DEBUGMSG(OBEX_HEADERENUM_ZONE,(L"CHeaderEnum::Next()\n"));
    ULONG i;
    HRESULT hRes = S_OK;
    
    for(i=0; i<celt; i++)
    {
        if(pListCurrent)
        {
            rgelt[i] = pListCurrent->pItem;            
            pListCurrent = pListCurrent->pNext;
        }
        else
            break;
    }

    if(i)    
        *pceltFetched = i;
    else
        hRes = E_FAIL;

   
    return hRes;
}



//Skip: skip 'celt' items
HRESULT STDMETHODCALLTYPE
CHeaderEnum::Skip(ULONG celt)
{
    return E_FAIL;
}

//Reset: reset enumeration to beginning
HRESULT STDMETHODCALLTYPE
CHeaderEnum::Reset()
{
    DEBUGMSG(OBEX_HEADERENUM_ZONE,(L"CHeaderEnum::Reset()\n"));
    pListCurrent = pListHead;
    return S_OK;
}

//Clone: clone the enumeration, including location
HRESULT STDMETHODCALLTYPE
CHeaderEnum::Clone(IHeaderEnum **ppenum)
{
    return E_FAIL;
}

//insert a device into the list
HRESULT STDMETHODCALLTYPE
CHeaderEnum::InsertBack(OBEX_HEADER *pDevice)
{
    DEBUGMSG(OBEX_HEADERENUM_ZONE,(L"CHeaderEnum::InsertBack()\n"));
   
    //if there is nothing in the list, just chain
    //  everything together
    if(!pListHead)
    {
        if(!(pListHead = new CHeaderNode())) {
            return E_OUTOFMEMORY;
        }
        pListHead->pItem = pDevice;
        pListHead->pNext = 0;

        pListTail = pListHead;
        pListCurrent = pListHead;
    }

    else
    {
        if(!(pListTail->pNext = new CHeaderNode())) {
            return E_OUTOFMEMORY;
        }
        pListTail = pListTail->pNext;

        pListTail->pItem = pDevice;
        pListTail->pNext = 0;
    }
   
    return S_OK;
}

//insert a device into the list
HRESULT STDMETHODCALLTYPE
CHeaderEnum::InsertFront(OBEX_HEADER *pDevice)
{
    DEBUGMSG(OBEX_HEADERENUM_ZONE,(L"CHeaderEnum::InsertFront()\n"));
   
    //if there is nothing in the list, just chain
    //  everything together
    if(!pListHead)
    {
        pListHead = new CHeaderNode();
        
        if(!pListHead)
            return E_OUTOFMEMORY;
            
        pListHead->pItem = pDevice;
        pListHead->pNext = 0;

        pListTail = pListHead;
        pListCurrent = pListHead;
    }

    else
    {
        CHeaderNode *pTemp = new CHeaderNode();
        
        if(!pTemp)
            return E_OUTOFMEMORY;
            
        pTemp->pNext = pListHead;
        pTemp->pItem = pDevice;
        pListHead = pTemp;
        pListCurrent = pListHead;
    }
   
    return S_OK;
}

void CHeaderEnum::RemoveAll()
{
 	while(pListHead)
    {
        CHeaderNode *pTemp = pListHead;

        //clean up memory in the header collection
        if((pTemp->pItem->bId & OBEX_TYPE_MASK) == OBEX_TYPE_BYTESEQ)
            g_funcFree(pTemp->pItem->value.ba.pbaData,g_pvFreeData);
        else if((pTemp->pItem->bId & OBEX_TYPE_MASK) == OBEX_TYPE_UNICODE) {
            PREFAST_SUPPRESS(307, "this is BSTR must call SysFreeString");
            g_funcFree(pTemp->pItem->value.pszData,g_pvFreeData);
        }

        delete pTemp->pItem;

        pListHead = pListHead->pNext;
        delete pTemp;
    }
    
    return;
}



HRESULT STDMETHODCALLTYPE 
CHeaderEnum::QueryInterface(REFIID riid, void** ppv) 
{
    DEBUGMSG(OBEX_HEADERENUM_ZONE,(L"CHeaderEnum::QueryInterface()\n"));
 	if(!ppv) 
		return E_POINTER;
	if(riid == IID_IUnknown) 
		*ppv = this;
	else if(riid == IID_IHeaderEnum) 
		*ppv = static_cast<IHeaderEnum*>(this);
	else 
		return *ppv = 0, E_NOINTERFACE;

	return AddRef(), S_OK;	
}



ULONG STDMETHODCALLTYPE 
CHeaderEnum::AddRef() 
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CHeaderEnum::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE 
CHeaderEnum::Release() 
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CHeaderEnum::Release()\n"));
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount); 
    if(!ret) 
        delete this; 
    return ret;
}
