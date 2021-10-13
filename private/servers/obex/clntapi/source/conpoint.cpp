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
#include "cobex.h"
#include "conpoint.h"
#include <olectl.h>

CEnumConnectionPoints::CEnumConnectionPoints(IConnectionPoint *pcnpInit)
{
        _ulRefs = 1;
        _ulIndex = 0;
    _pcnp = pcnpInit;

    if (_pcnp)
        _pcnp->AddRef();
}

CEnumConnectionPoints::~CEnumConnectionPoints()
{
        SVSUTIL_ASSERT(_ulRefs == 0);

    if (_pcnp)
        _pcnp->Release();
}

HRESULT STDMETHODCALLTYPE
CEnumConnectionPoints::Next(ULONG cConnections, LPCONNECTIONPOINT *ppCP, ULONG *pcFetched)
{
        if (!ppCP)
                return E_POINTER;

        *ppCP = NULL;

    if ((cConnections == 0) || ((cConnections > 1) && (pcFetched == NULL)))
        return E_INVALIDARG;

        ULONG cCopied = 0;
        if (pcFetched)
                *pcFetched = 0;

        if (_pcnp != NULL && _ulIndex == 0)
        {
                *ppCP = _pcnp;
                _pcnp->AddRef();
                if (pcFetched)
                        *pcFetched = 1;

                _ulIndex = 1;
                cCopied = 1;
        }
        return (cConnections == cCopied) ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE
CEnumConnectionPoints::Skip(ULONG cConnections)
{
    if (cConnections == 0)
        return E_INVALIDARG;

        if (_ulIndex > 0 || !_pcnp)
                return S_FALSE;

        _ulIndex++;

        return (cConnections == 1) ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE
CEnumConnectionPoints::Reset(void)
{
        _ulIndex = 0;
        return S_OK;
}

ULONG STDMETHODCALLTYPE
CEnumConnectionPoints::AddRef()
{
    return InterlockedIncrement((LONG*)&_ulRefs);
}

ULONG STDMETHODCALLTYPE
CEnumConnectionPoints::Release()
{
    ULONG ulRefs;

        InterlockedExchange((LONG *)&ulRefs, _ulRefs);
    SVSUTIL_ASSERT(ulRefs != 0xffffffff);

        if (InterlockedDecrement((LONG*)&_ulRefs) == 0)
    {
        delete this;
        return 0;
    }
    return ulRefs - 1;
}

HRESULT STDMETHODCALLTYPE
CEnumConnectionPoints::QueryInterface(REFIID riid, void** ppv)
{
    if ((riid == IID_IEnumConnectionPoints) || (riid == IID_IUnknown))
    {
        *ppv = (IEnumConnectionPoints *) this;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

CConnectionPoint::CConnectionPoint(IConnectionPointContainer *pCPC)
{
        _ulRefs = 1;
        _ulNextCookie = 1;
        _pSinkList = NULL;
        _ulConns = 0;
        _pCPC = pCPC;
        if (_pCPC)
                _pCPC->AddRef();
}

CConnectionPoint::~CConnectionPoint()
{
        SVSUTIL_ASSERT(_ulRefs == 0);

        if (_pCPC)
                _pCPC->Release();

        PSINKNODE pNode = _pSinkList;
        while(pNode)
        {
                SVSUTIL_ASSERT(pNode->pObSink);

                PSINKNODE pNode1 = pNode->pNext;
        PREFAST_ASSERT(pNode);
        PREFAST_ASSERT(pNode->pObSink);
                pNode->pObSink->Release();
                delete pNode;
                pNode = pNode1;
        }
}

void
CConnectionPoint::ContainerReleased()
{
        SVSUTIL_ASSERT(gpSynch->IsLocked());
        PREFAST_ASSERT(_pCPC);
        _pCPC->Release();
        _pCPC = NULL;
}


//NOTE: the *ONLY* way for this to fail is if it cant get the lock
HRESULT
CConnectionPoint::Notify(OBEX_EVENT evt, IUnknown *pOBEXDevice, IUnknown *pUnk2)
{
        SVSUTIL_ASSERT(gpSynch->IsLocked() && pOBEXDevice);
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CConnectionPoint::Notify()\n"));

        PSINKNODE pSinkNode = _pSinkList;
        PSINKNODE pPrev = NULL;
        HRESULT hr = S_OK;

        while (pSinkNode)
        {
                IObexSink *pSink = pSinkNode->pObSink;
                pSinkNode->ulFlags |= SN_LOCKED;

                PREFAST_ASSERT(pSink);

                DEBUGMSG(OBEX_COBEX_ZONE,(L"CConnectionPoint::Notify- Unlocking, going out to notify(event=%d, pSink=0x%08x)\n", evt, pSink));
                ReleaseLock();

                //run the sinks Notify
                hr = pSink->Notify(evt, pOBEXDevice, pUnk2);

                if(!GetLock())
                        return E_FAIL;

                //Notify is always called on the EnumThread, and hence is assured that g_pObex will remain initialized
                DEBUGMSG(OBEX_COBEX_ZONE,(L"CConnectionPoint::Notify- Got lock. Notify returned 0x%08x\n", hr));

                // If notify failed but we got the lock, we still return success
                hr = S_OK;

                pSinkNode->ulFlags &= ~SN_LOCKED;

                //if the sink node was marked for removal when we unlocked and went out to
                //  notify, remove it now
                if (pSinkNode->ulFlags & SN_REMOVED)
                {
                        DEBUGMSG(OBEX_COBEX_ZONE,(L"CConnectionPoint::Notify - Node 0x%08x marked for removal during Notify.Removing it now.\n", pSinkNode));

                        if (!pPrev)
                                _pSinkList = pSinkNode->pNext;
                        else
                                pPrev->pNext = pSinkNode->pNext;

                        PSINKNODE pCurrent = pSinkNode->pNext;
            PREFAST_ASSERT(pSinkNode);
            PREFAST_ASSERT(pSinkNode->pObSink);
                        pSinkNode->pObSink->Release();
                        delete pSinkNode;
                        _ulConns--;

                        pSinkNode = pCurrent;
                }
                else
                {
                        pPrev = pSinkNode;
                        pSinkNode = pSinkNode->pNext;
                }
        }

        return hr;
}



HRESULT STDMETHODCALLTYPE
CConnectionPoint::GetConnectionInterface(IID *pIID)
{
        if (!pIID)
                return E_POINTER;
        *pIID = IID_IObexSink;
        return S_OK;
}

HRESULT STDMETHODCALLTYPE
CConnectionPoint::GetConnectionPointContainer(IConnectionPointContainer **ppCPC)
{
        if (!ppCPC)
                return E_POINTER;

        *ppCPC = NULL;

        if (!GetLock())
                return E_FAIL;

        HRESULT hr = E_FAIL;

        *ppCPC = _pCPC;

        if (_pCPC)
        {
                _pCPC->AddRef();
                hr = S_OK;
        }

        ReleaseLock();

        return hr;
}

HRESULT STDMETHODCALLTYPE
CConnectionPoint::Advise(IUnknown *pUnkSink, DWORD *pdwCookie)
{
        if ((!pUnkSink) || (!pdwCookie))
                return E_POINTER;

        *pdwCookie = 0;

        //make sure this IUnknown pointer supports IObexSink
        IObexSink *pObSink = NULL;
        HRESULT hr = pUnkSink->QueryInterface(IID_IObexSink, (LPVOID *)&pObSink);
        if ((FAILED(hr)) || (!pObSink) )
        {
                DEBUGMSG(OBEX_COBEX_ZONE,(L"CConnectionPoint::Advise - Interface does not support IObexSink()\n"));
                return CONNECT_E_CANNOTCONNECT;
        }

        hr = E_FAIL;
        SINKNODE *pSink = NULL;

        if (!GetLock())
                return hr;

        SVSUTIL_ASSERT(pObSink);

        //chain in the Sink
        pSink = new SINKNODE;
        if (pSink)
        {
                pSink->ulFlags = 0;

                //ref the sink and put it in the linked list
                pObSink->AddRef();
                pSink->pObSink = pObSink;
                pSink->ulCookie = _ulNextCookie++;
                pSink->pNext = _pSinkList;

                *pdwCookie = pSink->ulCookie;
                _pSinkList = pSink;
                _ulConns++;
                hr = S_OK;

                DEBUGMSG(OBEX_COBEX_ZONE,(L"CConnectionPoint::Advise - pUnkSink=0x%08x, Cookie=%d\n", pUnkSink, _pSinkList->ulCookie));
        }
        else
        {
                hr = E_OUTOFMEMORY;
                DEBUGMSG(OBEX_COBEX_ZONE, (L"CConnectionPoint::Advise(pUnkSink=0x%08x) fails. E_OUTOFMEMORY\n", pUnkSink));
        }

        if(pObSink)
                pObSink->Release();

        ReleaseLock();

        return hr;
}

HRESULT STDMETHODCALLTYPE
CConnectionPoint::Unadvise(DWORD dwCookie)
{
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CConnectionPoint::Unadvise - Cookie=%d\n", dwCookie));

        if (!dwCookie)
                return E_INVALIDARG;

        HRESULT hr = E_FAIL;

        if (!GetLock())
                return hr;

        PSINKNODE pSinkNode = _pSinkList;
        PSINKNODE pPrev = NULL;
        while(pSinkNode)
        {
        if (pSinkNode->ulCookie == dwCookie)
        {
            pSinkNode->ulFlags |= SN_REMOVED;

                        if (!(pSinkNode->ulFlags & SN_LOCKED))
                        {
                                if (!pPrev)
                                        _pSinkList = pSinkNode->pNext;
                                else
                                        pPrev->pNext = pSinkNode->pNext;

                                PREFAST_ASSERT(pSinkNode->pObSink);
                                pSinkNode->pObSink->Release();

                                delete pSinkNode;
                                _ulConns--;
                        }

                        DEBUGMSG(OBEX_COBEX_ZONE,(L"CConnectionPoint::Unadvise : Removed Cookie=%d\n", dwCookie));
                        ReleaseLock();
                        return S_OK;
        };
                pPrev = pSinkNode;
                pSinkNode = pSinkNode->pNext;
        }

        SVSUTIL_ASSERT(pSinkNode == NULL);

        DEBUGMSG(OBEX_COBEX_ZONE,(L"CConnectionPoint::Unadvise : Missing Cookie=%d\n", dwCookie));

        ReleaseLock();

        return CONNECT_E_CANNOTCONNECT;
}

HRESULT STDMETHODCALLTYPE
CConnectionPoint::EnumConnections(IEnumConnections **ppEnum)
{
        if (!ppEnum)
                return E_POINTER;

        *ppEnum = NULL;

        HRESULT hr = E_FAIL;

        if (!GetLock())
                return hr;

        CEnumConnections *pEnum = new CEnumConnections();
        if (pEnum)
        {
                hr = pEnum->Init(_pSinkList, _ulConns);
                if (SUCCEEDED(hr))
                {
                        *ppEnum = (IEnumConnections *)pEnum;
                        (*ppEnum)->AddRef();
                        hr = S_OK;
                }
                pEnum->Release();
        }
        else
                hr = E_OUTOFMEMORY;

        ReleaseLock();

        return hr;
}


ULONG STDMETHODCALLTYPE
CConnectionPoint::AddRef()
{
    return InterlockedIncrement((LONG*)&_ulRefs);
}

ULONG STDMETHODCALLTYPE
CConnectionPoint::Release()
{
    ULONG ulRefs;
        InterlockedExchange((LONG *)&ulRefs, _ulRefs);
    SVSUTIL_ASSERT(ulRefs != 0xffffffff);

    if (InterlockedDecrement((LONG*)&_ulRefs) == 0)
    {
        delete this;
        return 0;
    }
    return ulRefs - 1;
}

HRESULT STDMETHODCALLTYPE
CConnectionPoint::QueryInterface(REFIID riid, void** ppv)
{
    if ((riid == IID_IConnectionPoint) || (riid == IID_IUnknown))
    {
        *ppv = (IConnectionPoint *) this;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
CEnumConnections::CEnumConnections()
{
        _ulRefs = 1;
        _ulIndex = 0;
        _ulConns = 0;
        _pConnData = NULL;
}

CEnumConnections::~CEnumConnections()
{
        SVSUTIL_ASSERT(_ulRefs == 0);

        if (_pConnData)
        {
                for (ULONG i=0; i<_ulConns; i++)
                {
                        PREFAST_ASSERT(_pConnData[i].pUnk);
                        _pConnData[i].pUnk->Release();
                }
        }
        delete[] _pConnData;
        _pConnData = NULL;
}


HRESULT
CEnumConnections::Init(SINKNODE *pSinkList, ULONG ulConns)
{
    //
    // 100 is arbituary, it should be safe this big but realistically
    //    never get there
        if (!ulConns || ulConns > 100)
        {
                SVSUTIL_ASSERT(!pSinkList);
                return S_FALSE;
        }

        if (!pSinkList)
                return E_INVALIDARG;

        SVSUTIL_ASSERT(gpSynch->IsLocked());

        _pConnData = new CONNECTDATA[ulConns];
        if (!_pConnData)
                return E_OUTOFMEMORY;

        for (ULONG i=0; (pSinkList) && (i<ulConns); i++)
        {
                _pConnData[i].pUnk = (IUnknown *)pSinkList->pObSink;
                _pConnData[i].pUnk->AddRef();
                _pConnData[i].dwCookie = pSinkList->ulCookie;
                pSinkList = pSinkList->pNext;
        }

        if (i==ulConns && pSinkList == NULL)
        {
                _ulConns = ulConns;
                return S_OK;
        }

        delete[] _pConnData;
        _ulConns = 0;
        return E_FAIL;
}


HRESULT STDMETHODCALLTYPE
CEnumConnections::Next(ULONG cConnections, CONNECTDATA *pCD, ULONG *pcFetched)
{
        if (!pCD)
                return E_POINTER;

    if ((cConnections == 0) || ((cConnections > 1) && (pcFetched == NULL)))
        return E_INVALIDARG;

        ULONG cCopied = 0;
        if (pcFetched)
                *pcFetched = 0;

        if (_pConnData != NULL && _ulIndex < _ulConns)
        {
                while(_ulIndex < _ulConns)
                {
                        pCD[cCopied].pUnk = _pConnData[_ulIndex].pUnk;
                        pCD[cCopied].pUnk->AddRef();
                        pCD[cCopied].dwCookie = _pConnData[_ulIndex].dwCookie;
                        cCopied++;
                        _ulIndex++;
                }
                if (pcFetched)
                        *pcFetched = cCopied;
        }
        return (cConnections == cCopied) ? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE
CEnumConnections::Skip(ULONG cConnections)
{
    if (cConnections == 0)
        return E_INVALIDARG;

        if (_ulIndex + cConnections >= _ulConns)
        {
                _ulIndex = _ulConns;
                return S_FALSE;
        }
        else
        {
                _ulIndex += cConnections;
                return S_OK;
        }
}

HRESULT STDMETHODCALLTYPE
CEnumConnections::Reset(void)
{
        _ulIndex = 0;
        return S_OK;
}

ULONG STDMETHODCALLTYPE
CEnumConnections::AddRef()
{
    return InterlockedIncrement((LONG*)&_ulRefs);
}

ULONG STDMETHODCALLTYPE
CEnumConnections::Release()
{
    ULONG ulRefs;
        InterlockedExchange((LONG *)&ulRefs, _ulRefs);
    SVSUTIL_ASSERT(ulRefs != 0xffffffff);

    if (InterlockedDecrement((LONG*)&_ulRefs) == 0)
    {
        delete this;
        return 0;
    }
    return ulRefs - 1;
}

HRESULT STDMETHODCALLTYPE
CEnumConnections::QueryInterface(REFIID riid, void** ppv)
{
    if ((riid == IID_IEnumConnections) || (riid == IID_IUnknown))
    {
        *ppv = (IEnumConnections *) this;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}
