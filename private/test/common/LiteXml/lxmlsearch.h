//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#include "lxmlElement.h"

namespace litexml
{

class XmlSearch_t;

struct SearchContext
{
	SearchContext(DWORD _dwTypeMask=XmlBaseElement_t::EtElement,
		const XmlString_t& _Name=_X(""));

	DWORD		dwTypeMask;
	XmlString_t	Name;
};

typedef SearchContext SearchParams;

class XmlSearch_t
{
public:
	// Search for all elements matching the specified search parameters
	XmlSearch_t(const XmlBaseElement_t* _pRoot, const SearchParams& _Sp)
		{ DoSearch(_pRoot, _Sp); }

	// Search for all elements of the specified type
	XmlSearch_t(const XmlBaseElement_t* _pRoot,
		DWORD _dwTypeMask)
		{ DoSearch(_pRoot, SearchContext(_dwTypeMask)); }

	// Search for all standard elements with the specified name. If no
	// name is specified, all standard elements are retrieved. To search
	// for non-standard elements by name, use the SeachParams version of
	// the search constructor.
	XmlSearch_t(const XmlBaseElement_t* _pRoot,
		const XmlString_t& _strName=_X(""))
		{ DoSearch(_pRoot, SearchContext(XmlBaseElement_t::EtElement, _strName)); }

public:
	// STL-style iteration over the search results. These methods
	// are provided mainly as a means of interfacing with STL 
	// container operations, and in most cases the 'walker'-style
	// Next/End operations will provide an adequate result enumeration.
	XmlElementList_t::const_iterator GetBeginIter()
		{ return m_Results.begin(); }
	XmlElementList_t::const_iterator GetEndIter()
		{ return m_Results.end(); }

public:
	size_t size() const
		{ return m_Results.size(); }
	bool empty() const
		{ return m_Results.empty(); }

public:
	// Moves to the next result, returning FALSE if there are none remaining
	bool Next();

	// Returns non-zero if there are no more results
	bool End()
		{ return (m_Iter == m_Results.end()); }

	XmlBaseElement_t *Get();

private:
	VOID DoSearch(const XmlBaseElement_t* _pRoot,
		const SearchContext& _Sc);

private:
	static BOOL AddResultProc(const XmlBaseElement_t *_pElem,
		LPVOID _lpThis);

private:
	XmlElementList_t			m_Results;
	XmlElementList_t::iterator	m_Iter;
};

}	// namespace litexml
