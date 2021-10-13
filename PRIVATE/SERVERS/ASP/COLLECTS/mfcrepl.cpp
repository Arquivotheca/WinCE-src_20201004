//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Abstract:   MFC Replacement, indexes a list of void ptrs with a string
--*/


#include "aspmain.h"


CPtrMapper::CPtrMapper(int iInitial)
{
	ZEROMEM(this);
	m_iModAlloc = iInitial ? iInitial : VALUE_GROW_SIZE;		// default = 10	

	if (iInitial)
		m_pMapInfo = MyRgAllocZ(MAPINFO, iInitial);
}

CPtrMapper::~CPtrMapper()
{
	int i;
	for (i =0; i < m_nEntries; i++)
	{
		MyFree(m_pMapInfo[i].wszKey);

		// To make this inheritiable by other objects again, remove these lines
		((CRequestStrList*) m_pMapInfo[i].pbData)->Release();
	}
	MyFree(m_pMapInfo);
}


BOOL CPtrMapper::Set(WCHAR *wszKey, PVOID pbInData)
{
	// Needs to do initial allocation
	if (m_nEntries == 0)
	{
		m_pMapInfo = MyRgAllocZ(MAPINFO, m_iModAlloc);
		if (NULL == m_pMapInfo)
			return FALSE;
	}

	else if (m_nEntries % m_iModAlloc == 0)
	{
		if (NULL == (m_pMapInfo = MyRgReAlloc(MAPINFO,m_pMapInfo,m_nEntries,m_nEntries + m_iModAlloc)))
			return FALSE;
	}

	m_pMapInfo[m_nEntries].wszKey = wszKey;
	m_pMapInfo[m_nEntries].pbData = pbInData;
	m_nEntries++;
	
	return TRUE;
}


BOOL CPtrMapper::Lookup(WCHAR * wszKey, PVOID *ppbOutData)
{
	int i;

	if (NULL == m_pMapInfo)
		return FALSE;
	
	for (i = 0; i < m_nEntries; i++)
	{
		if (0 == wcsicmp(wszKey,m_pMapInfo[i].wszKey))
		{
			*ppbOutData = m_pMapInfo[i].pbData;
			return TRUE;
		}
	}
	*ppbOutData = NULL;
	return FALSE;
}


BOOL CPtrMapper::Lookup(int iIndex, PVOID *ppbOutData)
{
	if (iIndex < 0 || iIndex > m_nEntries)
	{
		return FALSE;
	}
	*ppbOutData = m_pMapInfo[iIndex].pbData;
	return TRUE;
}
