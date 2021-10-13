//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _TLS_H
#define _TLS_H

class CTls
{
public:
	CTls();
	~CTls();

	void* GetValue();
	BOOL SetValue( void* pVoid );
	LONG AddRef();
	LONG Release();

private:
	static DWORD m_dwTlsIndex;
	static LONG m_lRefCount;	
};


#endif//_TLS_H