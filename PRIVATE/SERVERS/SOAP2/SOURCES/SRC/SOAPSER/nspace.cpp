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
// File:    nspace.cpp
// 
// Contents:
//
//  implementation file 
//
//
//	
//
//-----------------------------------------------------------------------------
#include "headers.h"
#include "soapser.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CNamespaceHelper::~CNamespaceHelper()
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// destructor removes all namespaces from the list
CNamespaceHelper::~CNamespaceHelper(void)
{
	HRESULT hr;

	hr = reset();

	ASSERT (hr == S_OK);

	return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CNamespaceHelper::reset(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CNamespaceHelper::reset(void)
{
	// get the first level
	while (!dlNamespaceList.IsEmpty())
	{
		CNamespaceListEntry * pEntry; 

		pEntry = dlNamespaceList.RemoveHead();
		delete pEntry;
	}	
	uNamespaceNumber = 1;
	lNamespaceLevel = 0;

	return S_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CNamespaceHelper::AddNamespace(const WCHAR *pcPrefix, const WCHAR * pcURI)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CNamespaceHelper::AddNamespace(const WCHAR *pcPrefix, const WCHAR * pcURI)
{
	HRESULT 						hr = S_OK;						
	WCHAR *							prefix;
	CAutoP<CNamespaceListEntry>		pnsle(NULL);
	WCHAR							acBuffer[64];

	if ( (pcPrefix) && wcslen(pcPrefix) )
	{
		// in case we have a prefix, we are going to use it
		prefix = (WCHAR *) pcPrefix;
	}
	else
	{
		// in this case we are going to create a prefix name of 
		// the form SOAPSDK####
		swprintf(acBuffer,L"SOAPSDK%lu",uNamespaceNumber++);
		prefix = acBuffer;
	}
	

	pnsle = new CNamespaceListEntry();
	CHK_BOOL (pnsle != NULL, E_OUTOFMEMORY);


	CHK (pnsle->set(prefix, pcURI, lNamespaceLevel)) ;
	dlNamespaceList.InsertHead(pnsle.PvReturn());
	
Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CNamespaceHelper::FindURI(const WCHAR * pcPrefix)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CNamespaceListEntry * CNamespaceHelper::FindURI(
		const WCHAR * 			pcPrefix)
{
	CNamespaceListEntry *			pEntry;

	pEntry = dlNamespaceList.getHead();
	
	while (pEntry)
	{
		// we are on a legal entry
		if (wcsicmp(pcPrefix, pEntry->getPrefix()) == NULL)
		{
			// and we found a match
			return pEntry;
		}
		pEntry = dlNamespaceList.next(pEntry);
	}
	return (NULL);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CNamespaceHelper::FindNamespace( const WCHAR * pcURI, CNamespaceListEntry * 	pPos)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CNamespaceListEntry * CNamespaceHelper::FindNamespace(
		const WCHAR * 			pcURI, 
		CNamespaceListEntry * 	pPos)
{
	CNamespaceListEntry *			pEntry;

	if (pPos)
	{
		// we got a starting position to start the search from
		pEntry = dlNamespaceList.next(pPos);
	}
	else
	{
		// the first Entry
		pEntry = dlNamespaceList.getHead();
	}
	
	while (pEntry)
	{
		// we are on a legal entry
		if (wcsicmp(pcURI, pEntry->getURI()) == NULL)
		{
			// and we found a match
			return pEntry;
		}
		pEntry = dlNamespaceList.next(pEntry);
	}
	return (NULL);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CNamespaceHelper::NonSerialized( CNamespaceListEntry * 	pPos)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
CNamespaceListEntry * CNamespaceHelper::NonSerialized(
		CNamespaceListEntry * 	pPos)
{
	CNamespaceListEntry *			pEntry;

	if (pPos)
	{
		// we got a starting position to start the search from
		pEntry = dlNamespaceList.next(pPos);
	}
	else
	{
		// the first Entry
		pEntry = dlNamespaceList.getHead();
	}
	
	while (pEntry)
	{
		// we are on a legal entry
		if (pEntry->getSerialized() == FALSE)
		{
			return pEntry;
		}
		pEntry = dlNamespaceList.next(pEntry);
	}
	return NULL;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CNamespaceHelper::PopLevel(void)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CNamespaceHelper::PopLevel(void)
{
	CNamespaceListEntry *			pEntry;
	
	if (lNamespaceLevel == 0)		// the first level we created was 1
		return E_FAIL;
		
	pEntry = dlNamespaceList.getHead();

	// get the first level
	while (pEntry)
	{
		// first figure out the next entry in the list
		CNamespaceListEntry *	pnext = dlNamespaceList.next(pEntry);
		
		if (pEntry->getLevel() == lNamespaceLevel)
		{ 
			// we have to remove this one
			dlNamespaceList.RemoveEntry(pEntry);
			delete pEntry;
		}
		pEntry = pnext;
	}	

	// don't forget to actually decrement the level ...
	lNamespaceLevel --;
	return S_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CNamespaceListEntry::set(const WCHAR * pcPrefix, const WCHAR * pcURI, long lLevel)
//
//  parameters:
//
//  description:
//
//  returns: 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CNamespaceListEntry::set(const WCHAR * pcPrefix, const WCHAR * pcURI, long lLevel)
{
	HRESULT hr = S_OK;
	ASSERT (pcURI);

	
	CHK ( allocateAndCopy(&m_pcURI, pcURI) );

	CHK ( allocateAndCopy(&m_pcPrefix, pcPrefix) );

	lCurrentLevel = lLevel;
	
Cleanup:
	ASSERT(hr==S_OK);
	return (hr);
};





// End Of File


