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
//=--------------------------------------------------------------------------=
// StandardEnum.H
//=--------------------------------------------------------------------------=
//
// object definition for a generic enumerator object.
//
#ifndef _STANDARDENUM_H_

#include "Unknown.H"

// to support a generic Enumerator object, we'll just define this
// interface.  it can be safely cast to any other enumerator, since all
// they differ in is their pointer type in Next().
//
class IEnumGeneric: public IUnknown {

  public:
    virtual HRESULT __stdcall Next(ULONG celt, LPVOID rgelt, ULONG *pceltFetched) = 0;
    virtual HRESULT __stdcall Skip(ULONG celt) = 0;
    virtual HRESULT __stdcall Reset(void) = 0;
    virtual HRESULT __stdcall Clone(IEnumGeneric **ppenum) = 0;
};

//=--------------------------------------------------------------------------=
// StandardEnum
//=--------------------------------------------------------------------------=
// a generic enumerator object.  given a pointer to generic data, some
// information about the elements, and a function to copy the elements,
// we can implement a generic enumerator.
//
// NOTE: this class assumes that rgElements is HeapAlloc'd, and will free it
//       in it's destructor [although it IS valid for this to be NULL if there
//       are no elements to enumerate over.]
//
class CStandardEnum: public CUnknownObject, public IEnumGeneric {

public:
    // IUnknown methods
    //
    DECLARE_STANDARD_UNKNOWN();

    // IEnumVariant methods
    //
    STDMETHOD(Next)(unsigned long celt, void * rgvar, unsigned long * pceltFetched); 
    STDMETHOD(Skip)(unsigned long celt); 
    STDMETHOD(Reset)(); 
    STDMETHOD(Clone)(IEnumGeneric **ppEnumOut); 

    CStandardEnum(REFIID riid, int cElement, int cbElement, void *rgElements,
                 void (WINAPI * pfnCopyElement)(void *, const void *, DWORD));
    ~CStandardEnum();

private:
    virtual HRESULT InternalQueryInterface(REFIID riid, void **ppvObjOut);

    IID m_iid;                        // type of enumerator that we are
    int m_cElements;                  // Total number of elements
    int m_cbElementSize;              // Size of each element
    int m_iCurrent;                   // Current position: 0 = front, m_cElt = end
    VOID * m_rgElements;              // Array of elements  
    CStandardEnum *m_pEnumClonedFrom; // If we were cloned, from whom?
    void  (WINAPI * m_pfnCopyElement)(void *, const void *, DWORD);
};



#define _STANDARDENUM_H_
#endif // _STANDARDENUM_H_

