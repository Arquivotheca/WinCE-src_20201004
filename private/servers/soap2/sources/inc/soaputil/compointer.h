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
//+----------------------------------------------------------------------------
//
//
// File:
//      ComPointer.h
//
// Contents:
//
//      CComPointer template class
//
//-----------------------------------------------------------------------------


#ifndef __COMPOINTER_H_INCLUDED__
#define __COMPOINTER_H_INCLUDED__

template<class T> class CComPointer
{
    T *m_pT;
public:
    CComPointer();
    CComPointer(T *pT);
    CComPointer(CComPointer<T> &p);
    ~CComPointer();

    operator T*();
    T& operator *();
    T **operator &();

    T *operator->();

    T *operator=(T *pT);
    T *operator=(const CComPointer<T> &p);

    void Acquire(T *pT);

    ULONG AddRef();
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline CComPointer<T>::CComPointer()
//
//  parameters:
//          
//  description:
//          CComPointer default constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline CComPointer<T>::CComPointer()
: m_pT(0)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline CComPointer<T>::CComPointer(T *pT)
//
//  parameters:
//          pT - interface pointer to initialize CComPointer object with
//  description:
//          CComPointer constructor - initializes from interface pointer
//  returns:
//          nothing
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline CComPointer<T>::CComPointer(T *pT)
{
    if((m_pT = pT) != 0) m_pT->AddRef();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline CComPointer<T>::CComPointer(CComPointer<T> &p)
//
//  parameters:
//          
//  description:
//          CComPointer copy constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline CComPointer<T>::CComPointer(CComPointer<T> &p)
{
    if((m_pT = p.m_pT) != 0) m_pT->AddRef();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline CComPointer<T>::~CComPointer()
//
//  parameters:
//          
//  description:
//          CComPointer destructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline CComPointer<T>::~CComPointer()
{
    ReleaseInterface((const T*&)m_pT);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline CComPointer<T>::operator T*()
//
//  parameters:
//          
//  description:
//          Cast operator
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline CComPointer<T>::operator T*()
{
    return m_pT;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline T& CComPointer<T>::operator *()
//
//  parameters:
//          
//  description:
//          Indirection operator
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline T& CComPointer<T>::operator *()
{
    ASSERT(m_pT);
    return *m_pT;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline T *CComPointer<T>::operator ->()
//
//  parameters:
//          
//  description:
//          Selection operator
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline T *CComPointer<T>::operator ->()
{
    ASSERT(m_pT);
    return m_pT;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline T **CComPointer<T>::operator &()
//
//  parameters:
//          
//  description:
//          Address operator
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline T **CComPointer<T>::operator &()
{
    ASSERT(!m_pT);
    return &m_pT;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline T *CComPointer<T>::operator =(T *pT)
//
//  parameters:
//          
//  description:
//          Assignment operator
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline T *CComPointer<T>::operator =(T *pT)
{
    return AssignInterface<T>(&m_pT, pT);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline T *CComPointer<T>::operator =(const CComPointer<T> &p)
//
//  parameters:
//          
//  description:
//          Assignment operator
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline T *CComPointer<T>::operator =(const CComPointer<T> &p)
{
    return AssignInterface<T>(&m_pT, p.m_pT);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline void CComPointer<T>::Acquire(T *pT)
//
//  parameters:
//          
//  description:
//          Acquires interface pointer - doesn't call AddRef
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline void CComPointer<T>::Acquire(T *pT)
{
    ReleaseInterface(m_pT);
    m_pT = pT;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class T> inline ULONG CComPointer<T>::AddRef()
//
//  parameters:
//          
//  description:
//          AddRefs embedded interface pointer (used before returning pointer out to the caller)
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T> inline ULONG CComPointer<T>::AddRef()
{
    if (m_pT)
    {
        return m_pT->AddRef();
    }

    return 0;
}

#endif //__COMPOINTER_H_INCLUDED__
