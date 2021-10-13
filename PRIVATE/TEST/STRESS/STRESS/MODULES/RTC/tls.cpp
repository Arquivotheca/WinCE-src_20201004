//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "tls.h"

DWORD CTls::m_dwTlsIndex = 0xFFFFFFFF;
LONG CTls::m_lRefCount = 0;

CTls::CTls()
{
	LONG lRefCount;

	lRefCount = InterlockedIncrement( &m_lRefCount );

	if( m_dwTlsIndex == 0xFFFFFFFF && lRefCount == 1 )
	{
		m_dwTlsIndex = TlsAlloc();
	}

	
}

CTls::~CTls()
{
	LONG lRefCount;

	lRefCount = InterlockedDecrement( &m_lRefCount );

	if( lRefCount == 0 && m_dwTlsIndex != 0xFFFFFFFF )
	{
		TlsFree( m_dwTlsIndex );
		m_dwTlsIndex = 0xFFFFFFFF;
	}
}

void* CTls::GetValue()
{
	if( m_dwTlsIndex != 0xFFFFFFFF )
		return TlsGetValue( m_dwTlsIndex );
	else
		return NULL;
}

BOOL CTls::SetValue( void* pVoid )
{
	if( m_dwTlsIndex != 0xFFFFFFFF )
		return TlsSetValue( m_dwTlsIndex, pVoid );
	else
		return FALSE;
}

LONG CTls::AddRef()
{
	LONG lRefCount;

	lRefCount = InterlockedIncrement( &m_lRefCount );

	return lRefCount;
}

LONG CTls::Release()
{
	LONG lRefCount;

	lRefCount = InterlockedDecrement( &m_lRefCount );

	if( lRefCount == 0 && m_dwTlsIndex != 0xFFFFFFFF )
	{
		TlsFree( m_dwTlsIndex );
		m_dwTlsIndex = 0xFFFFFFFF;
	}

	return lRefCount;
}


