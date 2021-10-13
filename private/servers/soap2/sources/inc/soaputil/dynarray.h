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
//      dynarray.h
//
// Contents:
//
//      CDynArray class declaration
//
//-----------------------------------------------------------------------------

#ifndef __DYNARRAY_H_INCLUDED__
#define __DYNARRAY_H_INCLUDED__


const int g_blockSize = 32;


class CDynArrayEntry
{

protected:
    friend class CDynArray;
};



class CDynArrayBlock
{
public:
    CDynArrayBlock() 
    {
        m_pNext = 0; 
        m_bSize = 0; 
    }
    
    UINT GetSize()
    {
        return(m_bSize);
    }
    
    CDynArrayBlock * GetNext()
    {
        return(m_pNext);
    }
    
    void Add(CDynArrayEntry *pEntry)
    {
        m_aEntries[m_bSize++] = pEntry;
    }
    
    void Shutdown(void)
    {
        m_bSize = 0; 
        if (m_pNext)
        {
            m_pNext->Shutdown();
            delete m_pNext;
            m_pNext = 0; 
        }
    }
    
    

protected:
    CDynArrayBlock      *m_pNext;
    CDynArrayEntry      *m_aEntries[g_blockSize]; 
    friend class CDynArray;
    byte                m_bSize; 
}; 


class CDynArray
{
    CDynArrayEntry     m_Head;

public:
    CDynArray();
    ~CDynArray();

    UINT   Size() const;

    HRESULT Add(CDynArrayEntry *pEntry);
    HRESULT ReplaceAt(UINT uiPos, CDynArrayEntry *pEntry);
    CDynArrayEntry * GetAt(UINT uiPos);

protected:
    CDynArrayBlock      m_aStartBlock;
    CDynArrayBlock      *m_pCurrentBlock;
    CCritSect           m_critSect;    
    UINT                m_uiSize;

};




template<class T> class CTypedDynArray : private CDynArray
{
public:

    HRESULT Add(T  *pEntry);
    HRESULT ReplaceAt(UINT uiPos, T *pEntry);
    T * GetAt(UINT uiPos);
    UINT   Size() const;    
    void  Shutdown(void);    
};


template<class T> inline UINT CTypedDynArray<T>::Size(void) const
{
    return CDynArray::Size();
}


template<class T> inline HRESULT CTypedDynArray<T>::Add(T *pEntry)
{
    return CDynArray::Add(static_cast<CDynArrayEntry *>(pEntry));
}

template<class T> inline HRESULT CTypedDynArray<T>::ReplaceAt(UINT uiPos, T *pEntry)
{
    return CDynArray::ReplaceAt(uiPos, static_cast<CDynArrayEntry *>(pEntry));
}

template<class T> inline T * CTypedDynArray<T>::GetAt(UINT uiPos)
{
   
    return (static_cast<T *> (CDynArray::GetAt(uiPos))); 
}

template<class T> 
void CTypedDynArray<T>::Shutdown()
{
	T * pEntry;    
    
    while (m_uiSize > 0)
    {
        pEntry = GetAt(m_uiSize-1);
        delete pEntry;
        m_uiSize--;
    }
    m_aStartBlock.Shutdown();
}




template<class T, class VALUE> class CKeyedDoubleList : public CTypedDynArray<T>
{
public:
    ~CKeyedDoubleList();
    VALUE Lookup(DWORD dwCookie);
    void  Remove(DWORD dwCookie);
    HRESULT Add(VALUE Value, DWORD dwCookie);
    HRESULT Replace(VALUE Value, DWORD dwCookie); 
    T*    FindEntry(DWORD dwCookie);    
};

template<class T, class VALUE> 
CKeyedDoubleList<T, VALUE>::~CKeyedDoubleList()
{
    Shutdown();
}



template<class T, class VALUE> 
VALUE CKeyedDoubleList<T, VALUE>::Lookup(DWORD dwCookie)
{
    T* pEntry = FindEntry(dwCookie);

    if (pEntry)	
        return (pEntry->GetValue());
        
    return(0);
}


template<class T, class VALUE> 
HRESULT CKeyedDoubleList<T, VALUE>::Replace(VALUE Value, DWORD dwCookie)
{
    HRESULT hr = S_OK;
    T* pEntry = FindEntry(dwCookie);
    if (pEntry)
    {
   	    hr = pEntry->SetValue(Value);
    }
    else
    {
        hr = Add(Value, dwCookie); 
    }
        
	return hr;
}

template<class T, class VALUE> 
void CKeyedDoubleList<T, VALUE>::Remove(DWORD dwCookie)
{
    UINT uiSize = Size(); 
    T * pEntry;    
    
    while (uiSize > 0)
    {
        uiSize--;
        pEntry = GetAt(uiSize);
        if (pEntry->GetKey() == dwCookie)
        {
            delete pEntry; 
            ReplaceAt(uiSize, 0); 
            return;
        }
    }
}

template<class T, class VALUE> 
T* CKeyedDoubleList<T, VALUE>::FindEntry(DWORD dwCookie)
{
    UINT uiSize = Size(); 
    T * pEntry;    
    
    while (uiSize > 0)
    {
        uiSize--;
        pEntry = GetAt(uiSize);
        if (pEntry->GetKey() == dwCookie)
        {
            return (pEntry);
        }
    }

    return(0);
}

template<class T, class VALUE> 
HRESULT CKeyedDoubleList<T, VALUE>::Add(VALUE Value, DWORD dwCookie)
{
    TRACEL((1, "Added new Entry for %d -> total %d\n", dwCookie, Size()));
    HRESULT hr = S_OK;
    T * pEntry = new T(dwCookie);
 
    CHK_BOOL(pEntry, E_OUTOFMEMORY);
    CHK (pEntry->SetValue(Value));
    CHK (CTypedDynArray<T>::Add(pEntry));

Cleanup:
    ASSERT (SUCCEEDED(hr));
    return hr;
}


template<class TYPE, class STORAGE>
class CKeyedEntry : public CDynArrayEntry
{
public:
    CKeyedEntry() 
    {
        m_dwCookie = 0; 
        memset(&m_tValue, 0, sizeof(STORAGE));
    }
    CKeyedEntry(DWORD dwCookie)
    {
        m_dwCookie = dwCookie;
    }
    DWORD GetKey(void)
    {
        return(m_dwCookie);
    }
    HRESULT SetValue(TYPE tValue)
    {
        m_tValue = tValue;
        return S_OK;
    }
    TYPE GetValue(void)
    {
        return(m_tValue);
    }
    
protected:
    DWORD           m_dwCookie;
    STORAGE         m_tValue;
};

template<class TYPE, class STORAGE>
class CKeyedObj : public CKeyedEntry<TYPE, STORAGE>
{
public:
    ~CKeyedObj()
    {
        release(&m_tValue);
    } 
    CKeyedObj(DWORD dwCookie) : CKeyedEntry<TYPE,STORAGE>(dwCookie)
    {
        m_tValue = NULL;
    }
    
    HRESULT SetValue(TYPE tValue)
    {
        release(&m_tValue);
        assign(&m_tValue,tValue); 
        return S_OK;
    }
};




#endif //__DYNARRAY_H_INCLUDED__
