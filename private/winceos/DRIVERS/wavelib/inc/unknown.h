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
#pragma once

template <class implementation, class derivedinterface>
class _simpleobj :  public implementation
{
public:
    _simpleobj <implementation, derivedinterface>()
    :
        m_RefCount(1)
    {
    }

    virtual ~_simpleobj <implementation, derivedinterface>()
    {
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void ** ppvObject)
    {
#ifdef SUPPORT_QUERYINTERFACE
        if (riid == IID_IUnknown)
        {
            *ppvObject = static_cast<IUnknown*>(this);
        }
        else if (riid == __uuidof(derivedinterface))
        {
            *ppvObject = static_cast<derivedinterface*>(this);
        }
        else
        {
            *ppvObject = NULL;
            return E_NOINTERFACE;
        }
        reinterpret_cast<IUnknown*>(*ppvObject)->AddRef();
        return S_OK;
#else
        return E_NOTIMPL;
#endif
    }

    virtual ULONG STDMETHODCALLTYPE AddRef(void)
    {
        return InterlockedIncrement(&m_RefCount);
    }

    virtual ULONG STDMETHODCALLTYPE Release(void)
    {
        long RefCount = InterlockedDecrement(&m_RefCount);
        if (RefCount == 0)
        {
            delete this;
        }
        return RefCount;
    }

protected:
    long m_RefCount;

};

//===========================================================================
// This template implements the IUnknown portion of a given COM interface.

template <class I> class _simpleunknown : public I
{
public:
    _simpleunknown <I>()
    :
        m_RefCount(1)
    {
    }

    virtual ~_simpleunknown <I>()
    {
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void ** ppvObject)
    {
#ifdef SUPPORT_QUERYINTERFACE
        if (riid == IID_IUnknown)
        {
            *ppvObject = static_cast<IUnknown*>(this);
        }
        else if (riid == __uuidof(I))
        {
            *ppvObject = static_cast<I*>(this);
        }
        else
        {
            *ppvObject = NULL;
            return E_NOINTERFACE;
        }
        reinterpret_cast<IUnknown*>(*ppvObject)->AddRef();
        return S_OK;
#else
        return E_NOTIMPL;
#endif
    }

    virtual ULONG STDMETHODCALLTYPE AddRef( void)
    {
        return InterlockedIncrement(&m_RefCount);
    }

    virtual ULONG STDMETHODCALLTYPE Release( void)
    {
        long RefCount = InterlockedDecrement(&m_RefCount);
        if (RefCount == 0)
        {
            delete this;
        }
        return RefCount;
    }

protected:
    long m_RefCount;

};


