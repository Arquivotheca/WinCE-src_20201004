//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
