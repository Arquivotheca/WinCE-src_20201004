//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// Unknown.Cpp
//=--------------------------------------------------------------------------=
//
// implementation for various things in the unknown object that supports
// aggregation.
//
#include "IPServer.H"
#include "Unknown.H"

#if defined (winNT)
#include <stddef.h>
#endif


//=--------------------------------------------------------------------------=
// CUnknownObject::CPrivateUnknownObject::m_pMainUnknown
//=--------------------------------------------------------------------------=
// this method is used when we're sitting in the private unknown object,
// and we need to get at the pointer for the main unknown.  basically, it's
// a little better to do this pointer arithmetic than have to store a pointer
// to the parent, etc.
//
inline CUnknownObject *CUnknownObject::CPrivateUnknownObject::m_pMainUnknown
(
    void
)
{
    return (CUnknownObject *)((LPBYTE)this - offsetof(CUnknownObject, m_UnkPrivate));
}

//=--------------------------------------------------------------------------=
// CUnknownObject::CPrivateUnknownObject::QueryInterface
//=--------------------------------------------------------------------------=
// this is the non-delegating internal QI routine.
//
// Parameters:
//    REFIID        - [in]  interface they want
//    void **       - [out] where they want to put the resulting object ptr.
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
STDMETHODIMP CUnknownObject::CPrivateUnknownObject::QueryInterface
(
    REFIID riid,
    void **ppvObjOut
)
{
    CHECK_POINTER(ppvObjOut);

    // if they're asking for IUnknown, then we have to pass them ourselves.
    // otherwise defer to the inheriting object's InternalQueryInterface
    //
    if (DO_GUIDS_MATCH(riid, IID_IUnknown)) {
        InterlockedIncrement((long *)&m_cRef);
        *ppvObjOut = (IUnknown *)this;
        return S_OK;
    } else
        return m_pMainUnknown()->InternalQueryInterface(riid, ppvObjOut);

    // dead code    
}

//=--------------------------------------------------------------------------=
// CUnknownObject::CPrivateUnknownObject::AddRef
//=--------------------------------------------------------------------------=
// adds a tick to the current reference count.
//
// Output:
//    ULONG        - the new reference count
//
// Notes:
//
ULONG CUnknownObject::CPrivateUnknownObject::AddRef
(
    void
)
{
    return InterlockedIncrement((long *)&m_cRef);
}

//=--------------------------------------------------------------------------=
// CUnknownObject::CPrivateUnknownObject::Release
//=--------------------------------------------------------------------------=
// removes a tick from the count, and delets the object if necessary
//
// Output:
//    ULONG         - remaining refs
//
// Notes:
//
ULONG CUnknownObject::CPrivateUnknownObject::Release
(
    void
)
{
    ULONG cRef = InterlockedDecrement((long *)&m_cRef);

    if (!m_cRef)
        delete m_pMainUnknown();

    return cRef;
}


//=--------------------------------------------------------------------------=
// CUnknownObject::InternalQueryInterface
//=--------------------------------------------------------------------------=
// objects that are aggregated use this to support additional interfaces.
// they should call this method on their parent so that any of it's interfaces
// are queried.
//
// Parameters:
//    REFIID        - [in]  interface they want
//    void **       - [out] where they want to put the resulting object ptr.
//
// Output:
//    HRESULT       - S_OK, E_NOINTERFACE
//
// Notes:
//
HRESULT CUnknownObject::InternalQueryInterface
(
    REFIID  riid,
    void  **ppvObjOut
)
{
    *ppvObjOut = NULL;

    return E_NOINTERFACE;
}

