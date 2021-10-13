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
/*++

Module Name:

     afxutil.cpp

Abstract:
Functions:
Notes:
--*/

//
// This source code is only intended as a supplement to the
// Microsoft Foundation Classes Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the
// Microsoft Foundation Classes product.

// This contains abridged implementations of CString and CByteArray
// specifically for WinCE

#include "afxutil.h"


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define _isxspace(x) ((x) == ' ' || (x) == '\t')
/////////////////////////////////////////////////////////////////////////////
// static class data, special inlines

// afxChNil is left for backward compatibility
TCHAR afxChNil = '\0';

// For an empty string, m_pchData will point here
// (note: avoids special case of checking for NULL m_pchData)
// empty string data (and locked)
static long rgInitData[5] = { -1, 0,0,0,0 };
static CStringData* afxDataNil = (CStringData *)rgInitData;
static LPCTSTR afxPchNil = (LPCTSTR)(afxDataNil+1);

// special function to make afxEmptyString work even during initialization
const CString& AFXAPI AfxGetEmptyString()
    { return *(CString*)&afxPchNil; }

//////////////////////////////////////////////////////////////////////////////
// Construction/Destruction

CString::CString()
{
    Init();
}

CString::CString(const CString& stringSrc)
{
    ASSERT(stringSrc.GetData()->nRefs != 0);
    if (stringSrc.GetData()->nRefs >= 0)
    {
        ASSERT(stringSrc.GetData() != afxDataNil);
        m_pchData = stringSrc.m_pchData;
        InterlockedIncrement(&GetData()->nRefs);
        m_pchData2=NULL;
    }
    else
    {
        Init();
        *this = stringSrc.m_pchData;// BUG it could exist on NT as well.
    }
}

void CString::AllocBuffer(UINT nLen)
// always allocate one extra character for '\0' termination
// assumes [optimistically] that data length will equal allocation length
{
    ASSERT(nLen >= 0);
    ASSERT(nLen <= INT_MAX-1);    // max size (enough room for 1 extra)

    if (nLen == 0)
        Init();
    else
    {
        CStringData* pData =
            (CStringData*)new BYTE[sizeof(CStringData) + (nLen+1)*sizeof(TCHAR)];
        if(pData) {
            pData->nRefs = 1;
            pData->data()[nLen] = _T('\0');
            pData->nDataLength = nLen;
            pData->nAllocLength = nLen;
            m_pchData = pData->data();
        }
    }
}

void CString::Release()
{
    if (GetData() != afxDataNil)
    {
        ASSERT(GetData()->nRefs != 0);
        if (InterlockedDecrement(&GetData()->nRefs) <= 0)
            delete[] (BYTE*)GetData();
        Init();
    }
}

void PASCAL CString::Release(CStringData* pData)
{
    if (pData != afxDataNil)
    {
        ASSERT(pData->nRefs != 0);
        if (InterlockedDecrement(&pData->nRefs) <= 0)
            delete[] (BYTE*)pData;
    }
}

void CString::Empty()
{
    if (GetData()->nDataLength == 0)
        return;
    if (GetData()->nRefs >= 0)
        Release();
    else
        *this = &afxChNil;
    ASSERT(GetData()->nDataLength == 0);
    ASSERT(GetData()->nRefs < 0 || GetData()->nAllocLength == 0);
}

void CString::CopyBeforeWrite()
{
    if (GetData()->nRefs > 1)
    {
        CStringData* pData = GetData();
        Release();
        AllocBuffer(pData->nDataLength);
        memcpy(m_pchData, pData->data(), (pData->nDataLength+1)*sizeof(TCHAR));
    }
    ASSERT(GetData()->nRefs <= 1);
}

void CString::AllocBeforeWrite(int nLen)
{
    if (GetData()->nRefs > 1 || nLen > GetData()->nAllocLength)
    {
        Release();
        AllocBuffer(nLen);
    }
    ASSERT(GetData()->nRefs <= 1);
}

CString::~CString()
//  free any attached data
{
    if (GetData() != afxDataNil)
    {
        if (InterlockedDecrement(&GetData()->nRefs) <= 0)
            delete[] (BYTE*)GetData();
    }
    if (m_pchData2)
        delete[] m_pchData2;
}

//////////////////////////////////////////////////////////////////////////////
// Helpers for the rest of the implementation

void CString::AllocCopy(CString& dest, UINT nCopyLen, UINT nCopyIndex,
     UINT nExtraLen) const
{
    // will clone the data attached to this string
    // allocating 'nExtraLen' characters
    // Places results in uninitialized string 'dest'
    // Will copy the part or all of original data to start of new string

    UINT nNewLen = nCopyLen + nExtraLen;

    if (nNewLen < nCopyLen || nNewLen < nExtraLen)
    {
        ASSERT(!L"CString::AllocCopy has overflow");
    }
        
    if (nNewLen == 0 || nNewLen < nCopyLen || nNewLen < nExtraLen)
    {
        dest.Init();
    }
    else
    {
        dest.AllocBuffer(nNewLen);
        if (nCopyLen) 
            memcpy(dest.m_pchData, m_pchData+nCopyIndex, nCopyLen*sizeof(TCHAR));
    }
}

//////////////////////////////////////////////////////////////////////////////
// More sophisticated construction

CString::CString(LPCTSTR lpsz)
{
    Init();
    int nLen = SafeStrlen(lpsz);
    if (nLen != 0)
    {
        AllocBuffer(nLen);
        memcpy(m_pchData, lpsz, nLen*sizeof(TCHAR));
    }
}

///////////////////////////////////////////////////////////////////////////////
// CString conversion helpers (these use the current system locale)

int AFX_CDECL _wcstombs(char* mbstr, const wchar_t* wcstr, size_t count)
{
    if (mbstr == NULL || count == 0)
        return 0;

    int result = ::WideCharToMultiByte(CP_ACP, 0, wcstr, -1,
        mbstr, count, NULL, NULL);
    ASSERT(mbstr == NULL || result <= (int)count);
    if (result > 0)
        mbstr[result-1] = 0;
    return result;
}

int AFX_CDECL _mbstowcs(wchar_t* wcstr, const char* mbstr, size_t count)
{
    if (wcstr == NULL || count == 0)
        return 0;

    int result = ::MultiByteToWideChar(CP_ACP, 0, mbstr, -1,
        wcstr, count);
    ASSERT(wcstr == NULL || result <= (int)count);
    if (result > 0)
        wcstr[result-1] = 0;
    return result;
}
/////////////////////////////////////////////////////////////////////////////
// Special conversion constructors

#if _UNICODE // unicode
CString::CString(LPCSTR lpsz) 
{
    Init();
    int nSrcLen = lpsz != NULL ? strlen(lpsz) : 0;
    if (nSrcLen != 0)
    {
        AllocBuffer(nSrcLen);
        _mbstowcs(m_pchData, lpsz, nSrcLen+1);
    }
}
const CString& CString::operator=(LPCSTR lpsz)
{
    int nSrcLen = lpsz != NULL ? strlen(lpsz) : 0;
    if (nSrcLen != 0)
    {
        AllocBeforeWrite(nSrcLen);
        int rLength=_mbstowcs(m_pchData, lpsz, nSrcLen+1);
    }
    return *this;
}
CString::operator LPCSTR()            // convert to WIDE string
{
    if (m_pchData2)
        delete m_pchData2;
    m_pchData2 = new  CHAR[2*GetLength()+1];
    if (m_pchData2)
    {
        _wcstombs(m_pchData2,m_pchData, GetLength()+1);
    }
    return m_pchData2;
}
const CString& CString::operator+=(char ch)
{
    return (*this+=(TCHAR)ch);
}
#else 
CString::CString(LPCWSTR lpsz) 
{
    Init();
    int nSrcLen = lpsz != NULL ? wcslen(lpsz) : 0;
    if (nSrcLen != 0)
    {
        AllocBuffer(nSrcLen*2);
        _wcstombsz(m_pchData, lpsz, (nSrcLen*2)+1);
    }
}
const CString& CString::operator=(LPCWSTR lpsz)
{
    int nSrcLen = lpsz != NULL ? wcslen(lpsz) : 0;
    if (nSrcLen != 0)
    {
        AllocBeforeWrite(nSrcLen*2);
        _wcstombsz(m_pchData, lpsz, (nSrcLen*2)+1);
    };
    return *this;
}
CString::operator LPCWSTR()            // convert to WIDE string
{
    if (m_pchData2)
        delete m_pchData2;
    m_pchData2 = new  WCHAR[GetLength()+1];
    if (m_pchData2)
    {
        mbstowcs(m_pchData2,m_pchData, GetLength()+1);
    }
    return m_pchData2;
}
#endif //!_UNICODE


//////////////////////////////////////////////////////////////////////////////
// Assignment operators
//  All assign a new value to the string
//      (a) first see if the buffer is big enough
//      (b) if enough room, copy on top of old buffer, set size and type
//      (c) otherwise free old string data, and create a new one
//
//  All routines return the new string (but as a 'const CString&' so that
//      assigning it again will cause a copy, eg: s1 = s2 = "hi there".
//

void CString::AssignCopy(int nSrcLen, LPCTSTR lpszSrcData)
{
    AllocBeforeWrite(nSrcLen);
    memcpy(m_pchData, lpszSrcData, nSrcLen*sizeof(TCHAR));
    GetData()->nDataLength = nSrcLen;
    m_pchData[nSrcLen] = '\0';
}

const CString& CString::operator=(const CString& stringSrc)
{
    if (m_pchData != stringSrc.m_pchData)
    {
        if ((GetData()->nRefs < 0 && GetData() != afxDataNil) ||
            stringSrc.GetData()->nRefs < 0)
        {
            // actual copy necessary since one of the strings is locked
            AssignCopy(stringSrc.GetData()->nDataLength, stringSrc.m_pchData);
        }
        else
        {
            // can just copy references around
            Release();
            ASSERT(stringSrc.GetData() != afxDataNil);
            m_pchData = stringSrc.m_pchData;
            InterlockedIncrement(&GetData()->nRefs);
        }
    }
    return *this;
}

const CString& CString::operator=(LPCTSTR lpsz)
{
    ASSERT(lpsz == NULL || AfxIsValidString(lpsz, FALSE));
    AssignCopy(SafeStrlen(lpsz), lpsz);
    return *this;
}


//////////////////////////////////////////////////////////////////////////////
// concatenation

// NOTE: "operator+" is done as friend functions for simplicity
//      There are three variants:
//          CString + CString
// and for ? = TCHAR, LPCTSTR
//          CString + ?
//          ? + CString

void CString::ConcatCopy(UINT nSrc1Len, LPCTSTR lpszSrc1Data,
    UINT nSrc2Len, LPCTSTR lpszSrc2Data)
{
  // -- master concatenation routine
  // Concatenate two sources
  // -- assume that 'this' is a new CString object

    UINT nNewLen = nSrc1Len + nSrc2Len;

    if (nNewLen < nSrc1Len || nNewLen < nSrc2Len)
    {
        ASSERT(!L"CString::ConcatCopy calculation overflowed!");
    }

    if (nNewLen != 0 && nNewLen >= nSrc1Len && nNewLen >= nSrc2Len)
    {
        AllocBuffer(nNewLen);
        memcpy(m_pchData, lpszSrc1Data, nSrc1Len*sizeof(TCHAR));
        memcpy(m_pchData+nSrc1Len, lpszSrc2Data, nSrc2Len*sizeof(TCHAR));
    }
}

CString AFXAPI operator+(const CString& string1, const CString& string2)
{
    CString s;
    s.ConcatCopy(string1.GetData()->nDataLength, string1.m_pchData,
        string2.GetData()->nDataLength, string2.m_pchData);
    return s;
}

CString AFXAPI operator+(const CString& string, LPCTSTR lpsz)
{
    ASSERT(lpsz == NULL || AfxIsValidString(lpsz, FALSE));
    CString s;
    s.ConcatCopy(string.GetData()->nDataLength, string.m_pchData,
        CString::SafeStrlen(lpsz), lpsz);
    return s;
}

CString AFXAPI operator+(LPCTSTR lpsz, const CString& string)
{
    ASSERT(lpsz == NULL || AfxIsValidString(lpsz, FALSE));
    CString s;
    s.ConcatCopy(CString::SafeStrlen(lpsz), lpsz, string.GetData()->nDataLength,
        string.m_pchData);
    return s;
}

//////////////////////////////////////////////////////////////////////////////
// concatenate in place

void CString::ConcatInPlace(UINT nSrcLen, LPCTSTR lpszSrcData)
{
    //  -- the main routine for += operators

    // concatenating an empty string is a no-op!
    if (nSrcLen == 0)
        return;

    // if the buffer is too small, or we have a width mis-match, just
    //   allocate a new buffer (slow but sure)
    if (GetData()->nRefs > 1 || GetData()->nDataLength + nSrcLen > (UINT)GetData()->nAllocLength)
    {
        // we have to grow the buffer, use the ConcatCopy routine
        CStringData* pOldData = GetData();
        ConcatCopy(GetData()->nDataLength, m_pchData, nSrcLen, lpszSrcData);
        ASSERT(pOldData != NULL);
        CString::Release(pOldData);
    }
    else
    {
        // fast concatenation when buffer big enough
        memcpy(m_pchData+GetData()->nDataLength, lpszSrcData, nSrcLen*sizeof(TCHAR));
        GetData()->nDataLength += nSrcLen;
        ASSERT(GetData()->nDataLength <= GetData()->nAllocLength);
        m_pchData[GetData()->nDataLength] = '\0';
    }
}

const CString& CString::operator+=(LPCTSTR lpsz)
{
    ASSERT(lpsz == NULL || AfxIsValidString(lpsz, FALSE));
    ConcatInPlace(SafeStrlen(lpsz), lpsz);
    return *this;
}

const CString& CString::operator+=(TCHAR ch)
{
    ConcatInPlace(1, &ch);
    return *this;
}

const CString& CString::operator+=(const CString& string)
{
    ConcatInPlace(string.GetData()->nDataLength, string.m_pchData);
    return *this;
}

CString AFXAPI operator+(const CString& string1, TCHAR ch)
{
    CString s;
    s.ConcatCopy(string1.GetData()->nDataLength, string1.m_pchData, 1, &ch);
    return s;
}

CString AFXAPI operator+(TCHAR ch, const CString& string)
{
    CString s;
    s.ConcatCopy(1, &ch, string.GetData()->nDataLength, string.m_pchData);
    return s;
}

///////////////////////////////////////////////////////////////////////////////
// Advanced direct buffer access

LPTSTR CString::GetBuffer(int nMinBufLength)
{
    ASSERT(nMinBufLength >= 0);

    if (GetData()->nRefs > 1 || nMinBufLength > GetData()->nAllocLength)
    {
        // we have to grow the buffer
        CStringData* pOldData = GetData();
        int nOldLen = GetData()->nDataLength;   // AllocBuffer will tromp it
        if (nMinBufLength < nOldLen)
            nMinBufLength = nOldLen;
        AllocBuffer(nMinBufLength);
        memcpy(m_pchData, pOldData->data(), (nOldLen+1)*sizeof(TCHAR));
        GetData()->nDataLength = nOldLen;
        CString::Release(pOldData);
    }
    ASSERT(GetData()->nRefs <= 1);

    // return a pointer to the character storage for this string
    ASSERT(m_pchData != NULL);
    return m_pchData;
}

void CString::ReleaseBuffer(int nNewLength)
{
    CopyBeforeWrite();  // just in case GetBuffer was not called

    if (nNewLength == -1)
        nNewLength = TSTRlen(m_pchData); // zero terminated

    ASSERT(nNewLength <= GetData()->nAllocLength);
    GetData()->nDataLength = nNewLength;
    m_pchData[nNewLength] = '\0';
}

LPTSTR CString::GetBufferSetLength(int nNewLength)
{
    ASSERT(nNewLength >= 0);

    GetBuffer(nNewLength);
    GetData()->nDataLength = nNewLength;
    m_pchData[nNewLength] = '\0';
    return m_pchData;
}

void CString::FreeExtra()
{
    ASSERT(GetData()->nDataLength <= GetData()->nAllocLength);
    if (GetData()->nDataLength != GetData()->nAllocLength)
    {
        CStringData* pOldData = GetData();
        AllocBuffer(GetData()->nDataLength);
        memcpy(m_pchData, pOldData->data(), pOldData->nDataLength*sizeof(TCHAR));
        ASSERT(m_pchData[GetData()->nDataLength] == '\0');
        CString::Release(pOldData);
    }
    ASSERT(GetData() != NULL);
}

LPTSTR CString::LockBuffer()
{
    LPTSTR lpsz = GetBuffer(0);
    GetData()->nRefs = -1;
    return lpsz;
}

void CString::UnlockBuffer()
{
    ASSERT(GetData()->nRefs == -1);
    if (GetData() != afxDataNil)
        GetData()->nRefs = 1;
}

///////////////////////////////////////////////////////////////////////////////
// Commonly used routines (rarely used routines in STREX.CPP)

int CString::Find(TCHAR ch) const
{
    // find first single character
    LPTSTR lpsz = _tcschr(m_pchData, ch);

    // return -1 if not found and index otherwise
    return (lpsz == NULL) ? -1 : (int)(lpsz - m_pchData);
}

int CString::FindOneOf(LPCTSTR lpszCharSet) const
{
    ASSERT(AfxIsValidString(lpszCharSet, FALSE));
    LPTSTR lpsz = _tcspbrk(m_pchData, lpszCharSet);
    return (lpsz == NULL) ? -1 : (int)(lpsz - m_pchData);
}

// find a sub-string (like strstr)
int CString::Find(LPCTSTR lpszSub) const
{
    ASSERT(AfxIsValidString(lpszSub, FALSE));

    // find first matching substring
    LPTSTR lpsz = _tcsstr(m_pchData, lpszSub);

    // return -1 for not found, distance from beginning otherwise
    return (lpsz == NULL) ? -1 : (int)(lpsz - m_pchData);
}

void CString::MakeLower()
{
    CopyBeforeWrite();
    _tcslwr(m_pchData);
}


void CString::SetAt(int nIndex, TCHAR ch)
{
    ASSERT(nIndex >= 0);
    ASSERT(nIndex < GetData()->nDataLength);

    CopyBeforeWrite();
    m_pchData[nIndex] = ch;
}

#ifndef _UNICODE
void CString::AnsiToOem()
{
    CopyBeforeWrite();
    ::AnsiToOem(m_pchData, m_pchData);
}
void CString::OemToAnsi()
{
    CopyBeforeWrite();
    ::OemToAnsi(m_pchData, m_pchData);
}
#endif

CString CString::Right(int nCount) const
{
    if (nCount < 0)
        nCount = 0;
    else if (nCount > GetData()->nDataLength)
        nCount = GetData()->nDataLength;

    CString dest;
    AllocCopy(dest, nCount, GetData()->nDataLength-nCount, 0);
    return dest;
}

CString CString::Left(int nCount) const
{
    if (nCount < 0)
        nCount = 0;
    else if (nCount > GetData()->nDataLength)
        nCount = GetData()->nDataLength;

    CString dest;
    AllocCopy(dest, nCount, 0, 0);
    return dest;
}

CString CString::Mid(int nFirst) const
{
    return Mid(nFirst, GetData()->nDataLength - nFirst);
}

CString CString::Mid(int nFirst, int nCount) const
{
    // out-of-bounds requests return sensible things
    if (nFirst < 0)
        nFirst = 0;
    if (nCount < 0)
        nCount = 0;

    if (nFirst + nCount > GetData()->nDataLength)
        nCount = GetData()->nDataLength - nFirst;
    if (nFirst > GetData()->nDataLength)
        nCount = 0;

    CString dest;
    AllocCopy(dest, nCount, nFirst, 0);
    return dest;
}

void CString::TrimRight()
{
    CopyBeforeWrite();

    // find beginning of trailing spaces by starting at beginning (DBCS aware)
    LPTSTR lpsz = m_pchData;
    LPTSTR lpszLast = NULL;
    while (*lpsz != '\0')
    {
        if (_isxspace(*lpsz))
        {
            if (lpszLast == NULL)
                lpszLast = lpsz;
        }
        else
            lpszLast = NULL;
        //lpsz = _tcsinc(lpsz);
        lpsz++;
    }

    if (lpszLast != NULL)
    {
        // truncate at trailing space start
        *lpszLast = '\0';
        GetData()->nDataLength = lpszLast - m_pchData;
    }
}

void CString::TrimLeft()
{
    CopyBeforeWrite();

    // find first non-space character
    LPCTSTR lpsz = m_pchData;
    while (_isxspace(*lpsz))
        lpsz++;

    // fix up data and length
    int nDataLength = GetData()->nDataLength - (lpsz - m_pchData);
    memmove(m_pchData, lpsz, (nDataLength+1)*sizeof(TCHAR));
    GetData()->nDataLength = nDataLength;
}

// strspn equivalent
CString CString::SpanIncluding(LPCTSTR lpszCharSet) const
{
    ASSERT(AfxIsValidString(lpszCharSet, FALSE));
    return Left(_tcsspn(m_pchData, lpszCharSet));
}

// strcspn equivalent
CString CString::SpanExcluding(LPCTSTR lpszCharSet) const
{
    ASSERT(AfxIsValidString(lpszCharSet, FALSE));
    return Left(_tcscspn(m_pchData, lpszCharSet));
}



LPWSTR AFXAPI AfxA2WHelper(LPWSTR lpw, LPCSTR lpa, int nChars)
{
    if (lpa == NULL)
        return NULL;
    ASSERT(lpw != NULL);
    // verify that no illegal character present
    // since lpw was allocated based on the size of lpa
    // don't worry about the number of chars
    lpw[0] = '\0';
    VERIFY(MultiByteToWideChar(CP_ACP, 0, lpa, -1, lpw, nChars));
    return lpw;
}

LPSTR AFXAPI AfxW2AHelper(LPSTR lpa, LPCWSTR lpw, int nChars)
{
    if (lpw == NULL)
        return NULL;
    ASSERT(lpa != NULL);
    // verify that no illegal character present
    // since lpa was allocated based on the size of lpw
    // don't worry about the number of chars
    lpa[0] = '\0';
    VERIFY(WideCharToMultiByte(CP_ACP, 0, lpw, -1, lpa, nChars, NULL, NULL));
    return lpa;
}

///////////////////////////////////////////////////////////////////////////////
CByteArray::CByteArray()
{
    m_pData = NULL;
    m_nSize = m_nMaxSize = m_nGrowBy = 0;
}

CByteArray::~CByteArray()
{
    ASSERT_VALID(this);

    delete[] (BYTE*)m_pData;
}

void CByteArray::Copy(const CByteArray& src)
{
    ASSERT_VALID(this);
    ASSERT(this != &src);   // cannot append to itself

    SetSize(src.m_nSize);

    memcpy(m_pData, src.m_pData, src.m_nSize * sizeof(BYTE));

}


void CByteArray::SetSize(int nNewSize, int nGrowBy)
{
    ASSERT_VALID(this);
    ASSERT(nNewSize >= 0);

    if (nGrowBy != -1)
        m_nGrowBy = nGrowBy;  // set new size

    if (nNewSize <= 0)
    {
        // shrink to nothing
        delete[] (BYTE*)m_pData;
        m_pData = NULL;
        m_nSize = m_nMaxSize = 0;
    }
    else if (m_pData == NULL)
    {
        // create one with exact size
#ifdef SIZE_T_MAX
        ASSERT(nNewSize <= SIZE_T_MAX/sizeof(BYTE));    // no overflow
#endif
        m_pData = (BYTE*) new BYTE[nNewSize * sizeof(BYTE)];

        memset(m_pData, 0, nNewSize * sizeof(BYTE));  // zero fill

        m_nSize = m_nMaxSize = nNewSize;
    }
    else if (nNewSize <= m_nMaxSize)
    {
        // it fits
        if (nNewSize > m_nSize)
        {
            // initialize the new elements

            memset(&m_pData[m_nSize], 0, (nNewSize-m_nSize) * sizeof(BYTE));

        }

        m_nSize = nNewSize;
    }
    else
    {
        // otherwise, grow array
        int nGrow = m_nGrowBy;
        if (nGrow == 0)
        {
            // heuristically determine growth when nGrowBy == 0
            //  (this avoids heap fragmentation in many situations)
            nGrow = min(1024, max(4, m_nSize / 8));
        }
        int nNewMax;
        if (nNewSize < m_nMaxSize + nGrow)
            nNewMax = m_nMaxSize + nGrow;  // granularity
        else
            nNewMax = nNewSize;  // no slush

        ASSERT(nNewMax >= m_nMaxSize);  // no wrap around
#ifdef SIZE_T_MAX
        ASSERT(nNewMax <= SIZE_T_MAX/sizeof(BYTE)); // no overflow
#endif
        BYTE* pNewData = (BYTE*) new BYTE[nNewMax * sizeof(BYTE)];

        ASSERT(pNewData);

        if(pNewData && nNewMax > m_nSize) {

            // copy new data from old
            memcpy(pNewData, m_pData, m_nSize * sizeof(BYTE));

            // construct remaining elements
            ASSERT(nNewSize > m_nSize);

            memset(&pNewData[m_nSize], 0, (nNewSize-m_nSize) * sizeof(BYTE));


            // get rid of old stuff (note: no destructors called)
            delete[] (BYTE*)m_pData;
            m_pData = pNewData;
            m_nSize = nNewSize;
            m_nMaxSize = nNewMax;
        }
    }
}

void CByteArray::SetAtGrow(int nIndex, BYTE newElement)
{
    ASSERT_VALID(this);
    ASSERT(nIndex >= 0);

    if (nIndex >= m_nSize)
        SetSize(nIndex+1);
    m_pData[nIndex] = newElement;
}

