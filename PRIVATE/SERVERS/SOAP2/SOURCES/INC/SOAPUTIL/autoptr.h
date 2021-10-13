//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      autoptr.h
//
// Contents:
//
//      Autopointer classes
//
//----------------------------------------------------------------------------------

#ifndef __AUTOPTR_H_INCLUDED__
#define __AUTOPTR_H_INCLUDED__

////////////////////////////////////////////////////////////////////////////////////////////////////
//  CAutoBase - base class for all AutoPointers
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef UNDER_CE
	//disable operator -> warning on templatessy
	#pragma warning(disable: 4284)
#endif


template <class T>
class CAutoBase
{
public:
    inline CAutoBase(T* pt);
    inline ~CAutoBase();

    inline T* operator= (T*);
    inline operator T* (void);
    inline operator const T* (void)const;
    inline T ** operator & (void);
    inline T* PvReturn(void);

protected:
    T* m_pt;

private:    // Never-to-use
    inline CAutoBase& operator= (CAutoBase&);
    CAutoBase(CAutoBase&);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoBase<T>::CAutoBase(T *pt)
//
//  parameters:
//
//  description:
//        Create a CAutoBase giving the array of objects pointed to by pt, allows NULL to be passed in
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoBase<T>::CAutoBase(T *pt)
{
    m_pt = pt;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoBase<T>::~CAutoBase()
//
//  parameters:
//
//  description:
//        Destructor. Asserts that the object has been free'd (set to NULL).
//        Setting to NULL does not happen in the retail build
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoBase<T>::~CAutoBase()
{
    ASSERT(NULL == m_pt);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline T* CAutoBase<T>::operator=(T* pt)
//
//  parameters:
//
//  description:
//        Assigns to variable after construction. May be dangerous so it ASSERTs that the variable is NULL
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline T* CAutoBase<T>::operator=(T* pt)
{
    ASSERT(m_pt == NULL);
    m_pt = pt;
    return pt;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoBase<T>::operator T*(void)
//
//  parameters:
//
//  description:
//        Cast operator used to "unwrap" the pointed object as if the CAutoBase variable were
//        a pointer of type T. In many situations this is enough for an autopointer to appear
//        exactly like an ordinary pointer.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoBase<T>::operator T*(void)
{
    return m_pt;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoBase<T>::operator const T*(void)const
//
//  parameters:
//
//  description:
//        Const cast operator
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoBase<T>::operator const T*(void)const
{
    return m_pt;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline T ** CAutoBase<T>::operator & ()
//
//  parameters:
//
//  description:
//        Address-of operator is used to make the autopointer even more similar to an ordinary pointer.
//        When you take an address of an autopointer, you actually get an address of the wrapped pointer.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline T ** CAutoBase<T>::operator & ()
{
    return & m_pt;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline T * CAutoBase<T>::PvReturn(void)
//
//  parameters:
//
//  description:
//        Returns the object(s) pointed to by the CAutoBase variable. In addition this method
//        'unhooks' the object(s), such that the scope of the object(s) are no longer local.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline T * CAutoBase<T>::PvReturn(void)
{
    T *ptT = m_pt;
    m_pt = NULL;
    return ptT;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  CAutoRg - autopointers to arrays
//
//    This derived class is primarily used to implement the vector deleting destructor.
//    Should only be used on objects allocated with new[]
////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T>
class CAutoRg : public CAutoBase<T>
{
public:
    inline CAutoRg(T *pt);
    inline ~CAutoRg();

    inline T* operator= (T*);
    inline void Clear(void);

private:    // Never-to-use
    inline CAutoRg& operator= (CAutoRg&);
    CAutoRg(CAutoRg&);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoRg<T>::CAutoRg(T *pt)
//
//  parameters:
//
//  description:
//        Create a CAutoRg giving the array of objects pointed to by pt, Allows NULL to be passed in.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoRg<T>::CAutoRg(T *pt)
  : CAutoBase<T>(pt)
{
}

//--------------------------------------------------------------------
// @mfunc Destructor.  When an object of class CAutoRg goes out
// of scope, free the associated object (if any).
// @side calls the vector delete method
// @rdesc None
//

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoRg<T>::~CAutoRg()
//
//  parameters:
//
//  description:
//        Destructor.  When an object of class CAutoRg goes out of scope, free the associated
//        object (if any).
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoRg<T>::~CAutoRg()
{
    delete [] m_pt;

    DBG_CODE(m_pt = NULL);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline void CAutoRg<T>::Clear(void)
//
//  parameters:
//
//  description:
//        Clear
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline void CAutoRg<T>::Clear(void)
{
    if (m_pt)
    {
        delete [] m_pt;
        m_pt = NULL;
    }
}


//--------------------------------------------------------------------
// @mfunc Assigns to variable after construction.  May be dangerous
//        so it ASSERT's that the variable is NULL
// @side None
// @rdesc None
//
// @ex Assign CAutoRg variable after construction. |
//
//        CAutoRg<lt>char<gt>    rgb;
//        /* ... */
//        rgb(NewG char[100]);
//

template <class T>
inline T* CAutoRg<T>::operator=(T* pt)
{
    return ((CAutoBase<T> *) this)->operator= (pt);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  CAutoRg - autopointers to scalars
//
//    This is analagous to CAutoRg but calls scalar delete of an object rather than arrays.
////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T>
class CAutoP : public CAutoBase<T>
{
public:
    inline CAutoP();
    inline CAutoP(T *pt);
    inline ~CAutoP();
    inline T* operator= (T*);
    inline T* operator->(void);
    inline void Clear(void);

private:    // Never-to-use
    inline CAutoP& operator= (CAutoP&);
    CAutoP(CAutoP&);
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoP<T>::CAutoP(T *pt)
//
//  parameters:
//
//  description:
//        Create a CAutoP giving the object pointed to by pt, allows NULL to be passed in
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoP<T>::CAutoP(T *pt)
: CAutoBase<T>(pt)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoP<T>::CAutoP()
//
//  parameters:
//
//  description:
//        Create an empty CAutoP
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoP<T>::CAutoP()
: CAutoBase<T>(NULL)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoP<T>::~CAutoP()
//
//  parameters:
//
//  description:
//        Destructor, delete the object pointed by CAutoP variable if any.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoP<T>::~CAutoP()
{
    delete m_pt;
    DBG_CODE(m_pt = NULL);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline void CAutoP<T>::Clear(void)
//
//  parameters:
//
//  description:
//        Clear, delete the object pointed by CAutoP variable if any.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline void CAutoP<T>::Clear(void)
{
    if (m_pt)
    {    
        delete m_pt;
        m_pt = NULL;
    }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline T* CAutoP<T>::operator=(T* pt)
//
//  parameters:
//
//  description:
//        Assigns to variable after construction.  May be dangerous so it ASSERT's that the
//        variable is NULL. Assign operator is not inherited, so it has to be written again.
//        Just calls base class assignment.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline T* CAutoP<T>::operator=(T* pt)
{
    return ((CAutoBase<T> *) this)->operator= (pt);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline T * CAutoP<T>::operator->()
//
//  parameters:
//
//  description:
//        The 'follow' operator on the CAutoP allows an CAutoP variable to act like a pointer of type T.
//        This overloading generally makes using a CAutoP simple as using a regular T pointer.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline T * CAutoP<T>::operator->()
{
    ASSERT(m_pt != NULL);
    return m_pt;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  CAutoRefc - autopointers to refcounted objects
//
//    Pointer to reference counted objects.
////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T>
class CAutoRefc
{
public:
    CAutoRefc();
    CAutoRefc(T* pt);
    CAutoRefc(CAutoRefc<T> &pt);
    CAutoRefc(REFIID riid_T, IUnknown* punk);
    ~CAutoRefc();

    T *operator=(T* pt);
    T *operator=(const CAutoRefc<T> &p);

    operator T* ();
    T ** operator & ();
    T* operator->();

    T* PvReturn(void);

    void Acquire(T *pT);
    void Clear();

    ULONG AddRef();

protected:
    T* m_pt;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoRefc<T>::CAutoRefc(T *pt)
//
//  parameters:
//
//  description:
//        Create a CAutoRefc giving the object pointed to by pt
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoRefc<T>::CAutoRefc(T *pt)
{
    m_pt = pt;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoRefc<T>::CAutoRefc()
//
//  parameters:
//
//  description:
//        Create an empty CAutoRefc.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoRefc<T>::CAutoRefc()
: m_pt(0)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoRefc<T>::CAutoRefc<T>(REFIID riid_t, IUnknown *pt)
//
//  parameters:
//
//  description:
//        Create a CAutoRefc given an IUnknown and a desired interface. QueryInterface is used on
//        pt to attempt to get the desired interface. If the QI fails, the CAutoRefc is set to null.
//        When the CAutoRefc destructs, the AddRef by the QI is released.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoRefc<T>::CAutoRefc<T>(REFIID riid_t, IUnknown *pt)
{
    ASSERT(NULL != pt);
    pt->QueryInterface(riid_t, (void **)&m_pt);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoRefc<T>::~CAutoRefc<T>()
//
//  parameters:
//
//  description:
//        CAutoRefc destructor.  When an object of class CAutoRefc goes out of scope, free the
//        associated object (if any).
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoRefc<T>::~CAutoRefc<T>()
{
    ReleaseInterface(m_pt);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline T* CAutoRefc<T>::operator=(T* pt)
//
//  parameters:
//
//  description:
//        Initialize a CAutoRefc that was not initalized by the construtor.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline T* CAutoRefc<T>::operator=(T* pt)
{
    ASSERT(m_pt == 0);
    m_pt = pt;
    return pt;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline void CAutoRefc<T>::Clear()
//
//  parameters:
//
//  description:
//        Clear
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline void CAutoRefc<T>::Clear()
{
    ReleaseInterface(m_pt);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoRefc<T>::operator T*(void)
//
//  parameters:
//
//  description:
//        Cast operator used to "unwrap" the pointed object as if the CAutoRefc variable were
//        a pointer of type T. In many situations this is enough for an autopointer to appear
//        exactly like an ordinary pointer.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline CAutoRefc<T>::operator T*(void)
{
    return m_pt;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline T ** CAutoRefc<T>::operator & ()
//
//  parameters:
//
//  description:
//        Address-of operator is used to make the autopointer even more similar to an ordinary pointer.
//        When you take an address of an autopointer, you actually get an address of the wrapped pointer.
//        This is very useful for doing QI, like this:
//
//        CAutoRefc<lt>IFoo<gt> pfoo;
//        pbar->QueryInterface (IID_IFoo, (void **) &pfoo) \| check;
//
//    This technique is different from the REFIID constructor (see above) in that it allows for an
//    explicit check of the HRESULT from QI.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline T ** CAutoRefc<T>::operator & ()
{
    return & m_pt;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline T * CAutoRefc<T>::operator->(void)
//
//  parameters:
//
//  description:
//        The 'follow' operator on the CAutoRef. Works analogously as with CAutoP.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline T * CAutoRefc<T>::operator->(void)
{
    ASSERT(m_pt != 0);
    return m_pt;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline T * CAutoRefc<T>::PvReturn()
//
//  parameters:
//
//  description:
//        Returns the object(s) pointed to by the CAutoRefc variable. In addition this method
//        'unhooks' the object(s), such that the scope of the object(s) are no longer local.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline T * CAutoRefc<T>::PvReturn()
{
    T *ptT = m_pt;
    m_pt   = 0;
    return ptT;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline ULONG CAutoRefc<T>::AddRef()
//
//  parameters:
//
//  description:
//        AddRefs embedded interface pointer.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T>
inline ULONG CAutoRefc<T>::AddRef()
{
    if (m_pt)
    {
        return m_pt->AddRef();
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  CAutoBSTR - Autopointers to BSTRs
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAutoBSTR : public CAutoBase<WCHAR>
{
public:
    inline CAutoBSTR();         // will be initialized with NULL
    inline ~CAutoBSTR();
    inline void Clear();
    inline HRESULT CopyTo(BSTR *pbstr);
    inline BOOL isEmpty(void); 
    inline UINT Len(void);
    inline HRESULT AllocBSTR(UINT len);    
    inline HRESULT Assign(BSTR bstr, bool fCopy = TRUE);
};


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoBSTR::CAutoBSTR()
//
//  parameters:
//
//  description:
//        Constructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CAutoBSTR::CAutoBSTR()
: CAutoBase<WCHAR>(NULL)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline CAutoBSTR::~CAutoBSTR()
//
//  parameters:
//
//  description:
//        Destructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline CAutoBSTR::~CAutoBSTR()
{
    Clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline void CAutoBSTR::Clear()
//
//  parameters:
//
//  description:
//        Clear
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void CAutoBSTR::Clear()
{
    ::SysFreeString(m_pt);
    m_pt = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT CAutoBSTR::Assign(BSTR bstr, bool fCopy)
//
//  parameters:
//
//  description:
//        Assignment implementation w/ optional copy of string
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT CAutoBSTR::Assign(BSTR bstr, bool fCopy)
{
    Clear();

    if (fCopy)
    {
        if (bstr != 0)
        {
            m_pt = ::SysAllocString(bstr); 
            return m_pt ? S_OK : E_OUTOFMEMORY;
        }
    }
    else
    {
        m_pt = bstr; 
    }
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT CAutoBSTR::CopyTo(BSTR *pbstr)
//
//  parameters:
//
//  description:
//        Create copy
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT CAutoBSTR::CopyTo(BSTR *pbstr)
{
    if (!pbstr)
        return E_INVALIDARG;

    *pbstr = NULL; 

    if (! isEmpty())
    {
        *pbstr = ::SysAllocString(m_pt);
        if (*pbstr == NULL)
            return E_OUTOFMEMORY;
    }    

    return S_OK;
}
    
////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline BOOL CAutoBSTR::isEmpty(void)
//
//  parameters:
//
//  description:
//        Is string empty?
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline BOOL CAutoBSTR::isEmpty(void)
{
    return (Len() == 0); 
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline UINT CAutoBSTR::Len(void)
//
//  parameters:
//
//  description:
//        Returns length of the stored string
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline UINT CAutoBSTR::Len(void)
{
    return ::SysStringLen(m_pt);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline UINT CAutoBSTR::AllocBSTR(UINT len)
//
//  parameters:
//
//  description:
//        allocates enough memory for len OLECHAR plus terminating zero
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT CAutoBSTR::AllocBSTR(UINT len)
{
    Clear();
    m_pt = ::SysAllocStringLen(0, len);

    return m_pt ? S_OK : E_OUTOFMEMORY;
}


#endif  // __AUTOPTR_H_INCLUDED__
