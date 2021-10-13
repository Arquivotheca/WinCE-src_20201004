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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <windows.h>
#include "string.hxx"

// Use this buffer size since most servers return size of 1460 (2922 = 1460*2 + 1 byte
// since proxy appends '\0' to packets)
#define DEFAULT_BUFFER_SIZE            2921


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
        {return (char *)memmove(_U, _V, _N); }

    static char* __cdecl assign(char* _U, size_t _N, const char& _C)
        {return ((char *)memset(_U, _C, _N)); }

    static const char* __cdecl find(const char* _U, const char& _C)
        {return strchr(_U, _C); }
};

typedef ce::_string_t<char, 128, case_insensitive_traits<char> >  stringi;
typedef ce::_string_t<char, 128> string;

#endif // __UTILS_H__
