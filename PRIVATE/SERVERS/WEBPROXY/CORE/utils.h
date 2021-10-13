//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __UTILS_H__
#define __UTILS_H__

#include "global.h"
#include "string.hxx"

class CBuffer {
public:
	CBuffer(void);
	~CBuffer(void);
	PBYTE GetBuffer(int iSize, BOOL fPreserveData = FALSE);
	int GetSize(void);
	
private:
	CBuffer(const CBuffer&){} // don't want to copy this object
	BYTE m_Buffer[DEFAULT_BUFFER_SIZE];
	PBYTE m_pBuffer;
	int m_iSize;
};


// char_traits for char
template<class T>
struct case_insensitive_traits
{
    static int __cdecl compare(const char* _U, const char* _V)
        {return _U ? (_V ? _stricmp(_U, _V) : 1) : -1; }

    static size_t __cdecl length(const char* _U)
        {return _U ? strlen(_U) : 0; }

    static char* __cdecl copy(char* _U, const char* _V, size_t _N)
        {return (char *)memcpy(_U, _V, _N); }

    static char* __cdecl assign(char* _U, size_t _N, const char& _C)
        {return ((char *)memset(_U, _C, _N)); }

    static const char* __cdecl find(const char* _U, const char& _C)
        {return strchr(_U, _C); }
};

typedef ce::_string_t<char, 128, case_insensitive_traits<char> >  stringi;
typedef ce::_string_t<char, 128> string;

BOOL MyInternetCanonicalizeUrlA(IN LPCSTR lpszUrl,OUT LPSTR lpszBuffer);


#endif // __UTILS_H__
