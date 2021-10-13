//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __COLLECTION_HXX__
#define __COLLECTION_HXX__

#include "DispatchImpl.hxx"
#include "list.hxx"
#include "string.hxx"
#include "com_macros.h"
#include "assert.h"
#include <combook.h>

#pragma warning(push)
#pragma warning(disable : 4786)

namespace ce
{

// EnumVARIANT_Unknown
template <typename iterator>
class EnumVARIANT_Unknown : public IEnumUnknown,
							public IEnumVARIANT
{
	typedef EnumVARIANT_Unknown<iterator> _Myt;
public:
	EnumVARIANT_Unknown(IUnknown* punkColl, iterator itBegin, iterator itEnd)
		: m_punkColl(punkColl),
		  m_it(itBegin),
		  m_itBegin(itBegin),
		  m_itEnd(itEnd)
	{
		assert(m_punkColl);
		
		// hold reference to underlying collection object
		m_punkColl->AddRef();
	}


	~EnumVARIANT_Unknown()
	{
		// release underlying collection
		m_punkColl->Release();
	}

	// implement GetInterfaceTable
    BEGIN_INTERFACE_TABLE(EnumVARIANT_Unknown)
        IMPLEMENTS_INTERFACE(IEnumVARIANT)
		IMPLEMENTS_INTERFACE(IEnumUnknown)
    END_INTERFACE_TABLE()

    // implement QueryInterface/AddRef/Release
    IMPLEMENT_UNKNOWN(EnumVARIANT_Unknown)

// IEnumUnknown methods
public:
	// Next
	STDMETHOD(Next)(unsigned long celt, IUnknown** rgelt, unsigned long FAR* pceltFetched)
	{
		CHECK_POINTER(rgelt);

		if(pceltFetched)
			*pceltFetched = 0;

		for(; celt > 0 && m_it != m_itEnd; ++m_it, --celt)
		{
			m_it->get_item(rgelt++);

			if(pceltFetched)
				++(*pceltFetched);
		}

		return celt ? S_FALSE : S_OK;
	}


	// Skip
	STDMETHOD(Skip)(unsigned long celt)
	{
		for(; celt > 0 && m_it != m_itEnd; ++m_it, --celt);

		return celt ? S_FALSE : S_OK;
	}


	// Reset
	STDMETHOD(Reset)()
	{
		m_it = m_itBegin;

		return S_OK;
	}


	// Clone
	STDMETHOD(Clone)(IEnumUnknown **ppenum)
	{
		_Myt* pEnum = new _Myt(m_punkColl, m_itBegin, m_itEnd);

		if(!pEnum)
			return E_OUTOFMEMORY;

		return pEnum->QueryInterface(IID_IEnumUnknown, (void**)ppenum);
	}

// IEnumVARIANT methods
public:
	// Next
	STDMETHOD(Next)(unsigned long celt, VARIANT* rgelt, unsigned long FAR* pceltFetched)
	{
		CHECK_POINTER(rgelt);

		if(pceltFetched)
			*pceltFetched = 0;

		for(; celt > 0 && m_it != m_itEnd; ++m_it, --celt)
		{
			m_it->get_item(rgelt++);

			if(pceltFetched)
				++(*pceltFetched);
		}

		return celt ? S_FALSE : S_OK;
	}

	// Clone
	STDMETHOD(Clone)(IEnumVARIANT **ppenum)
	{
		_Myt* pEnum = new _Myt(m_punkColl, m_itBegin, m_itEnd);

		if(!pEnum)
			return E_OUTOFMEMORY;

		return pEnum->QueryInterface(IID_IEnumVARIANT, (void**)ppenum);
	}

protected:
	IUnknown*	m_punkColl;
	iterator	m_itBegin;
	iterator	m_itEnd;
	iterator	m_it;
};


// com_ptr
template <typename T>
class com_ptr
{
public:
	com_ptr(T* p)
		: m_p(p)
	{
		m_p->AddRef();
	}

	com_ptr(const com_ptr& x)
		: m_p(x.m_p)
	{
		m_p->AddRef();
	}

	~com_ptr()
	{
		m_p->Release();
	}

	T* operator->()
	{
		return m_p;
	}

private:
	T* m_p;
};


// owned_ptr
template <typename T>
class owned_ptr
{
public:
	owned_ptr(T* p)
		: m_p(p),
		  m_bOwn(true)
	{
	}

	owned_ptr(const owned_ptr& x)
		: m_p(x.m_p),
		  m_bOwn(true)
	{
		// take over ownership
		x.m_bOwn = false;
	}

	~owned_ptr()
	{
		if(m_bOwn)
			delete m_p;
	}

	T* operator->()
	{
		return m_p;
	}

private:
	T*				m_p;
	mutable bool	m_bOwn;
};


// com_element
template<typename T, const IID* piid>
class com_element
{
public:
	typedef LPCWSTR key_type;
	typedef T		item_type;

	com_element(key_type key, item_type item)
		: m_key(key),
		  m_item(item)
	{}

	// compare keys
	bool compare_key(key_type key)
	{
		return m_key == key;
	}

	// get item into a VARIANT
	HRESULT get_item(VARIANT* pItem)
	{
		IDispatch* pdisp;

        if(SUCCEEDED(m_item->QueryInterface(IID_IDispatch, (void**)&pdisp)))
        {
            pItem->vt = VT_DISPATCH;
            pdisp->Release();
        }
        else
            pItem->vt = VT_UNKNOWN;

		return m_item->QueryInterface(*piid, (void**)&pItem->punkVal);
	}

	// get item into an interface pointer
	HRESULT get_item(void* pItem)
	{
		return m_item->QueryInterface(*piid, (void**)pItem);
	}

private:
	wstring			m_key;
	T				m_item;
};


// Collection template
template <typename item_type,				// type of items in the collection
		  typename collection_interface,	// interface of collection object
		  const IID* pcollection_iid,		// IID of collection interface
		  typename element_trait>			// class specifying behavior of elements
class Collection : public DispatchImpl<collection_interface, pcollection_iid>
{
public:
	typedef list<element_trait>				collection;
	typedef collection::iterator			iterator;

	// Note: it would be nice to be able to specify enumerator as template argument for the
	// collection but it can't be elegantly done w\o support for template template arguments
	// For now hard code to EnumVARIANT_Unknown
	typedef EnumVARIANT_Unknown<iterator>	enumerator;

	HRESULT AddItem(const element_trait::key_type key, element_trait::item_type item)
	{
		m_Elements.push_back(element_trait(key, item));

		return S_OK;
	}

// collection_interface methods
public:	
	// get_Count
	STDMETHOD(get_Count)(/*[retval][out]*/ LONG* plCount)
	{
		CHECK_POINTER(plCount);

		*plCount = m_Elements.size();

		return S_OK;
	}

	// get_Item
    STDMETHOD(get_Item)(/*[in]*/ BSTR index, /*[retval][out]*/ item_type* ppItem)
	{
		for(iterator it = m_Elements.begin(), itEnd = m_Elements.end(); it != itEnd; ++it)
			if(it->compare_key(index))
				return it->get_item(ppItem);

		return E_FAIL;
	}

    // get__NewEnum
	STDMETHOD(get__NewEnum)(/*[retval][out] */ LPUNKNOWN* ppunk)
	{
		enumerator *pEnum = new enumerator(this, m_Elements.begin(), m_Elements.end());

		if(!pEnum)
			return E_OUTOFMEMORY;
		
		return pEnum->QueryInterface(IID_IUnknown, (void**)ppunk);
	}

protected:
	collection m_Elements;
};

}; // namespace ce

#pragma warning(pop)

#endif // __COLLECTION_HXX__
