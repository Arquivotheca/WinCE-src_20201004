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
/*--
Module Name: mfcrepl.h
--*/


#ifndef PTRMAPPER
#define PTRMAPPER


typedef struct {
	WCHAR *wszKey;		// how we index into this
	PVOID pbData;		// Generic variable
}  MAPINFO, *PMAPINFO;


class CPtrMapper
{
protected:
	int m_nEntries;
	int m_iModAlloc;		// When we need to realloc, do this many blocks
	PMAPINFO m_pMapInfo;	// data itself

public:
	CPtrMapper(int iInitial=10);
	~CPtrMapper();    

	BOOL Set(WCHAR *wszKey, PVOID pbInData);
	BOOL Lookup(WCHAR *wszKey, PVOID *ppbOutData);
	BOOL Lookup(int iIndex, PVOID *ppbOutData);

};
#endif // PTRMAPPER
