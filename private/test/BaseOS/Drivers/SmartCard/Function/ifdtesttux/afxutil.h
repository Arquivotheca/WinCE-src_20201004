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
/* Since we dont have MFC in winceos, here are (abridged)
 definitions for the MFC classes used by the IFDTEST program
 Significant change is that it is not affected by the UNICODE setting
 It uses the TCHAR type instead of the TCHAR type and right now
 the TCHAR is unilaterally defined to CHAR */

#ifndef AFXUTIL_H
#define AFXUTIL_H

#include <stdarg.h> 
#include <stdio.h>
#include <string.h>
#include <windows.h>

#define AFXAPI __cdecl
#define AFX_CDECL __cdecl
#define _AFX_INLINE inline

#define ASSERT_VALID(x) (0)

// the default character set is 8-bit
// if you want to change the default to Unicode, 
// define TCHAR_IS_WIDE
#define TCHAR_IS_WIDE

// conversion helpers
int AFX_CDECL _wcstombs(char* mbstr, const wchar_t* wcstr, size_t count);
int AFX_CDECL _mbstowcs(wchar_t* wcstr, const char* mbstr, size_t count);

#define TRACE(x)
#define TRACE1(x,y)
#define TRACE2(x,y,z)
#define AfxIsValidString(x,y) (TRUE)

#define TSTRlen _tcslen


struct CStringData
{
    long nRefs;     // reference count
    int nDataLength;
    int nAllocLength;
    // TCHAR data[nAllocLength]

    TCHAR* data()
        { return (TCHAR*)(this+1); }
};



class CString
{
public:
// Constructors
    CString();
    CString(const CString& stringSrc);
    CString(TCHAR ch, int nRepeat = 1);
    CString(LPCTSTR lpch, int nLength);
    CString(LPCTSTR psz);

// Attributes & Operations
    // as an array of characters
    int GetLength() const;
    BOOL IsEmpty() const;
    void Empty();                       // free up the data

    TCHAR GetAt(int nIndex) const;      // 0 based
    TCHAR operator[](int nIndex) const; // same as GetAt
    void SetAt(int nIndex, TCHAR ch);
    operator LPCTSTR() const;           // as a C string
    // overloaded assignment
    const CString& operator=(const CString& stringSrc);
    const CString& operator=(TCHAR ch);


    const CString& operator=(LPCTSTR lpsz);

    // string concatenation
    const CString& operator+=(const CString& string);
    const CString& operator+=(TCHAR ch);

    const CString& operator+=(LPCTSTR lpsz);

    friend CString AFXAPI operator+(const CString& string1,
            const CString& string2);
    friend CString AFXAPI operator+(const CString& string, TCHAR ch);
    friend CString AFXAPI operator+(TCHAR ch, const CString& string);

    friend CString AFXAPI operator+(const CString& string, LPCTSTR lpsz);
    friend CString AFXAPI operator+(LPCTSTR lpsz, const CString& string);

    // string comparison
    int Compare(LPCTSTR lpsz) const;         // straight character

    // simple sub-string extraction
    CString Mid(int nFirst, int nCount) const;
    CString Mid(int nFirst) const;
    CString Left(int nCount) const;
    CString Right(int nCount) const;
    
    // lower conversion
    void MakeLower();

    CString SpanIncluding(LPCTSTR lpszCharSet) const;
    CString SpanExcluding(LPCTSTR lpszCharSet) const;

    // trimming whitespace (either side)
    void TrimRight();
    void TrimLeft();


    // searching (return starting index, or -1 if not found)
    // look for a single character match
    int Find(TCHAR ch) const;               // like "C" strchr
    int FindOneOf(LPCTSTR lpszCharSet) const;

    // look for a specific sub-string
    int Find(LPCTSTR lpszSub) const;        // like "C" strstr


                                        // 255 chars max
#ifndef _UNICODE
    // ANSI <-> OEM support (convert string in place)
    void AnsiToOem();
    void OemToAnsi();
#endif


    // Access to string implementation buffer as "C" character array
    LPTSTR GetBuffer(int nMinBufLength);
    void ReleaseBuffer(int nNewLength = -1);
    LPTSTR GetBufferSetLength(int nNewLength);
    void FreeExtra();

    // Use LockBuffer/UnlockBuffer to turn refcounting off
    LPTSTR LockBuffer();
    void UnlockBuffer();

// Implementation
public:
    ~CString();
    int GetAllocLength() const;

protected:
    LPTSTR m_pchData;   // pointer to master ref counted string data

    // implementation helpers
    CStringData* GetData() const;
    void Init();
    void AllocCopy(CString& dest, UINT nCopyLen, UINT nCopyIndex, UINT nExtraLen) const;
    void AllocBuffer(UINT nLen);
    void AssignCopy(int nSrcLen, LPCTSTR lpszSrcData);
    void ConcatCopy(UINT nSrc1Len, LPCTSTR lpszSrc1Data, UINT nSrc2Len, LPCTSTR lpszSrc2Data);
    void ConcatInPlace(UINT nSrcLen, LPCTSTR lpszSrcData);
    void FormatV(LPCTSTR lpszFormat, va_list argList);
    void CopyBeforeWrite();
    void AllocBeforeWrite(int nLen);
    void Release();
    static void PASCAL Release(CStringData* pData);
    static int PASCAL SafeStrlen(LPCTSTR lpsz);
public:
#if _UNICODE
    LPSTR m_pchData2; 
    CString(LPCSTR lpsz);
    operator LPCSTR();           // convert to CSTR
    const CString& operator=(char ch);
    const CString& operator=(LPCSTR lpsz);
    const CString& operator+=(char ch);
#else
    LPWSTR m_pchData2;   // alternate character set copy
    CString(LPCWSTR lpsz);
    operator LPCWSTR();           // convert to WIDE string
    const CString& operator=(wchar ch);
    const CString& operator=(LPCWSTR lpsz);
    const CString& operator+=(wchar ch);
#endif


};

// Globals
extern TCHAR afxChNil;
const CString& AFXAPI AfxGetEmptyString();
#define afxEmptyString AfxGetEmptyString()

// Compare helpers
bool AFXAPI operator==(const CString& s1, const CString& s2);
bool AFXAPI operator==(const CString& s1, LPCTSTR s2);
bool AFXAPI operator==(LPCTSTR s1, const CString& s2);

// CString
_AFX_INLINE CStringData* CString::GetData() const
    { ASSERT(m_pchData != NULL); return ((CStringData*)m_pchData)-1; }
_AFX_INLINE void CString::Init()
    { m_pchData = afxEmptyString.m_pchData; m_pchData2 = NULL; }
//_AFX_INLINE CString::CString(LPCTSTR lpsz)
//    { Init(); *this = lpsz; }
//_AFX_INLINE const CString& CString::operator=(LPCTSTR lpsz)
//    { *this = (LPCSTR)lpsz; return *this; }

#ifdef TCHAR_IS_WIDE
//_AFX_INLINE const CString& CString::operator+=(char ch)
//    { *this += (TCHAR)ch; return *this; }
_AFX_INLINE const CString& CString::operator=(char ch)
    { *this = (TCHAR)ch; return *this; }
_AFX_INLINE CString AFXAPI operator+(const CString& string, char ch)
    { return string + (TCHAR)ch; }
_AFX_INLINE CString AFXAPI operator+(char ch, const CString& string)
    { return (TCHAR)ch + string; }
#endif

_AFX_INLINE int CString::GetLength() const
    { return GetData()->nDataLength; }
_AFX_INLINE int CString::GetAllocLength() const
    { return GetData()->nAllocLength; }
_AFX_INLINE BOOL CString::IsEmpty() const
    { return GetData()->nDataLength == 0; }
_AFX_INLINE CString::operator LPCTSTR() const
    { return m_pchData; }

// CString support (windows specific)

_AFX_INLINE int PASCAL CString::SafeStrlen(LPCTSTR lpsz)
    { return (lpsz == NULL) ? 0 : TSTRlen(lpsz); }

_AFX_INLINE int CString::Compare(LPCTSTR lpsz) const
    { return _tcscmp(m_pchData, lpsz); }    // MBCS/Unicode aware

_AFX_INLINE TCHAR CString::GetAt(int nIndex) const
{
    ASSERT(nIndex >= 0);
    ASSERT(nIndex < GetData()->nDataLength);
    return m_pchData[nIndex];
}
_AFX_INLINE TCHAR CString::operator[](int nIndex) const
{
    // same as GetAt
    ASSERT(nIndex >= 0);
    ASSERT(nIndex < GetData()->nDataLength);
    return m_pchData[nIndex];
}
_AFX_INLINE bool AFXAPI operator==(const CString& s1, const CString& s2)
    { return s1.Compare(s2) == 0; }
_AFX_INLINE bool AFXAPI operator==(const CString& s1, LPCTSTR s2)
    { return s1.Compare(s2) == 0; }
_AFX_INLINE bool AFXAPI operator==(LPCTSTR s1, const CString& s2)
    { return s2.Compare(s1) == 0; }
_AFX_INLINE bool AFXAPI operator!=(const CString& s1, const CString& s2)
    { return s1.Compare(s2) != 0; }
_AFX_INLINE bool AFXAPI operator!=(const CString& s1, LPCTSTR s2)
    { return s1.Compare(s2) != 0; }
_AFX_INLINE bool AFXAPI operator!=(LPCTSTR s1, const CString& s2)
    { return s2.Compare(s1) != 0; }


////////////////////////////////////////////////////////////////////////////

class CByteArray 
{

public:

// Construction
    CByteArray();

// Attributes
    int GetSize() const;
    int GetUpperBound() const;
    void SetSize(int nNewSize, int nGrowBy = -1);

// Operations
    // Clean up
    void FreeExtra();
    void RemoveAll();

    // Accessing elements
    BYTE GetAt(int nIndex) const;
    void SetAt(int nIndex, BYTE newElement);
    BYTE& ElementAt(int nIndex);

    // Direct Access to the element data (may return NULL)
    const BYTE* GetData() const;
    BYTE* GetData();

    // Potentially growing the array
    void SetAtGrow(int nIndex, BYTE newElement);
    int Add(BYTE newElement);
    //int Append(const CByteArray& src);
    void Copy(const CByteArray& src);

    // overloaded operator helpers
    BYTE operator[](int nIndex) const;
    BYTE& operator[](int nIndex);


// Implementation
protected:
    BYTE* m_pData;   // the actual array of data
    int m_nSize;     // # of elements (upperBound - 1)
    int m_nMaxSize;  // max allocated
    int m_nGrowBy;   // grow amount

public:
    ~CByteArray();

};
////////////////////////////////////////////////////////////////////////////

inline int CByteArray::GetSize() const
    { return m_nSize; }
inline int CByteArray::GetUpperBound() const
    { return m_nSize-1; }
inline void CByteArray::RemoveAll()
    { SetSize(0); }
inline BYTE CByteArray::GetAt(int nIndex) const
    { ASSERT(nIndex >= 0 && nIndex < m_nSize);
        return m_pData[nIndex]; }
inline void CByteArray::SetAt(int nIndex, BYTE newElement)
    { ASSERT(nIndex >= 0 && nIndex < m_nSize);
        m_pData[nIndex] = newElement; }
inline BYTE& CByteArray::ElementAt(int nIndex)
    { ASSERT(nIndex >= 0 && nIndex < m_nSize);
        return m_pData[nIndex]; }
inline const BYTE* CByteArray::GetData() const
    { return (const BYTE*)m_pData; }
inline BYTE* CByteArray::GetData()
    { return (BYTE*)m_pData; }
inline int CByteArray::Add(BYTE newElement)
    { int nIndex = m_nSize;
        SetAtGrow(nIndex, newElement);
        return nIndex; }

////////////////////////////////////////////////////////////////////////////
#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC  1000
#define clock_t time_t
#endif

#endif AFXUTIL_H

