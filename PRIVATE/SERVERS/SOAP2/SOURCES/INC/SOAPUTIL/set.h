//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      Set.h
//
// Contents:
//
//      CSet class declaration
//
//-----------------------------------------------------------------------------

#ifndef __SET_H_INCLUDED__
#define __SET_H_INCLUDED__

template<class T, class S> class CSetEntry : public CDoubleListEntry
{
protected:
    S m_Value;

public:

    CSetEntry(T value);

    void SetValue(T value);
    T GetValue();
};

template<class T, class S> inline CSetEntry<T, S>::CSetEntry(T value)
{
    SetValue(value);
}

template<class T, class S> inline void CSetEntry<T, S>::SetValue(T value)
{
    m_Value = value;
}

template<class T, class S> inline T CSetEntry<T, S>::GetValue()
{
    return m_Value;
}

template<class T, class S> class CSet : private CTypedDoubleList< CSetEntry<T, S> >
{
    typedef CTypedDoubleList< CSetEntry<T, S> > baseType;
    typedef CSetEntry<T, S> entryType;

    CCritSect m_cs;

public:
    CSet();
    ~CSet();

    bool IsEmpty() const;

    HRESULT InsertHead(T &entry);
    HRESULT InsertTail(T &entry);

    HRESULT RemoveHead(T &entry);
    HRESULT RemoveTail(T &entry);
#ifdef CE_NO_EXCEPTIONS
private:
	BOOL m_fInitFailed;
#endif 
};


template<class T, class S> inline CSet<T, S>::CSet()
: baseType()
{
	m_fInitFailed = FALSE;
    if(FAILED(m_cs.Initialize()))
    {
#ifdef CE_NO_EXCEPTIONS
		m_fInitFailed = TRUE;
#else
        throw;
#endif
    }
}



template<class T, class S> inline CSet<T, S>::~CSet()
{
#ifdef CE_NO_EXCEPTIONS
	if(!m_fInitFailed)
	{
#endif 	
	    entryType *pEntry = 0;
	    m_cs.Enter();

	    for (;;)
	    {
	        pEntry = baseType::RemoveHead();
	        if (pEntry == 0)
	            break;

	        delete pEntry;
	    }
	    m_cs.Leave();
	    m_cs.Delete();
#ifdef CE_NO_EXCEPTIONS
	} 
#endif 
}


template<class T, class S> inline bool CSet<T, S>::IsEmpty() const
{
#ifdef CE_NO_EXCEPTIONS //<-- if we dont support expceptions and init failed
						//       the list must be empty
	if(m_fInitFailed)
		return TRUE;
#endif 
	
    bool bIsEmpty = false;
    m_cs.Enter();
    bIsEmpty = baseType::IsEmpty();
    m_cs.Leave();
    return bIsEmpty;
}


template<class T, class S> HRESULT CSet<T, S>::InsertHead(T &entry)
{
#ifdef CE_NO_EXCEPTIONS
	if(m_fInitFailed)
		return E_FAIL;
#endif 
    entryType *pEntry = new entryType(entry);
    if (! pEntry)
        return E_OUTOFMEMORY;

    m_cs.Enter();
    baseType::InsertHead(pEntry);
    m_cs.Leave();
    return S_OK;
}


template<class T, class S> HRESULT CSet<T, S>::InsertTail(T &entry)
{
#ifdef CE_NO_EXCEPTIONS
	if(m_fInitFailed)
		return E_FAIL;
#endif 
    entryType *pEntry = new entryType(entry);
    if (! pEntry)
        return E_OUTOFMEMORY;

    m_cs.Enter();
    baseType::InsertTail(pEntry);
    m_cs.Leave();
    return S_OK;
}


template<class T, class S> HRESULT CSet<T, S>::RemoveHead(T &entry)
{
    HRESULT hr = S_OK;
#ifdef CE_NO_EXCEPTIONS
	if(m_fInitFailed)
		return E_FAIL;
#endif 

    m_cs.Enter();
    entryType *pEntry = baseType::RemoveHead();
    m_cs.Leave();

    if (pEntry)
    {
        entry = pEntry->GetValue();
    }
    else
    {
        hr = S_FALSE;
    }

    delete pEntry;

    return hr;
}


template<class T, class S> HRESULT CSet<T, S>::RemoveTail(T &entry)
{
    HRESULT hr = S_OK;
    
#ifdef CE_NO_EXCEPTIONS
	if(m_fInitFailed)
		return E_FAIL;
#endif 

    m_cs.Enter();
    entryType *pEntry = baseType::RemoveTail();
    m_cs.Leave();

    if (pEntry)
    {
        entry = pEntry->GetValue();
    }
    else
    {
        hr = S_FALSE;
    }

    delete pEntry;

    return hr;
}

#endif  // __SET_H_INCLUDED__
