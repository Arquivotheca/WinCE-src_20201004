//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      UrlUtil.h
//
// Contents:
//
//      Url escaping utility functions
//
//----------------------------------------------------------------------------------

#ifndef __URLUTIL_H_INCLUDED__
#define __URLUTIL_H_INCLUDED__

#define CHAR_PERCENT            0x25
#define ESCAPE_MASK             0xFFFFFF80

extern CHAR s_ToHex[];

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class F> inline bool IsEscapeCharacter(F ch)
//
//  parameters:
//
//  description:
//        Decides whether character needs escaping or not. Character is escaped iff its value is
//        greater than 0x7F (that is if (ch & ESCAPE_MASK) != 0) or it is '%' character
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class F> inline bool IsEscapeCharacter(F ch)
{
    return (((F)ESCAPE_MASK & ch) != 0) || (ch == (F)CHAR_PERCENT);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class F, class T> T HexDigitToValue(F digit)
//
//  parameters:
//
//  description:
//        Converts Hexadecimal digit character into corresponding value
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class F, class T> T HexDigitToValue(F digit)
{
    if (digit >= (F)'0' && digit <= (F)'9')
        return (T)(digit - (F)'0');
    else if (digit >= (F)'A' && digit <= (F)'F')
        return (T)(digit - (F)'A' + 10);
    else if (digit >= (F)'a' && digit <= (F)'f')
        return (T)(digit - (F)'a' + 10);
    else return (T)(-1);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class F> DWORD SizeToEscape(const F *unescaped)
//
//  parameters:
//
//  description:
//        Returns number of characters (including terminating zero) needed to escape given URL.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class F> DWORD SizeToEscape(const F *unescaped)
{
    if (unescaped)
    {
        DWORD dwSize = 1;   // null terminator
        for ( ; *unescaped; unescaped ++)
        {
            static DWORD s_addens[] = { 1, 3 };
            dwSize += s_addens[IsEscapeCharacter(*unescaped)];
        }
        return dwSize;
    }
    else
    {
        return 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class F> DWORD SizeToUnescape(const F *escaped)
//
//  parameters:
//
//  description:
//        Returns number of characters (including terminating zero) needed to unescape given URL.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class F> DWORD SizeToUnescape(const F *escaped)
{
    if (escaped)
    {
        DWORD dwSize = 1; // null terminator;
        for ( ; *escaped; )
        {
            if ( *escaped ++ == (F)CHAR_PERCENT)
            {
                // 2 following characters are the encoding
                if (*escaped) escaped ++;
                if (*escaped) escaped ++;
            }

            dwSize ++;
        }

        return dwSize;
    }
    else
    {
        return 0;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class F, class T> HRESULT DoEscapeUrl(const F *unescaped, T *escaped)
//
//  parameters:
//
//  description:
//        Performs URL escape work (assumes buffers are of correct sizes)
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class F, class T> HRESULT DoEscapeUrl(const F *unescaped, T *escaped)
{
    while (*unescaped)
    {
        F unesc = (F) *unescaped ++;
        if (IsEscapeCharacter(unesc))
        {
            *escaped ++ = (T)'%';
            *escaped ++ = (T)s_ToHex[((unesc) >> 4) & 0xF];
            *escaped ++ = (T)s_ToHex[(unesc) & 0xF];
        }
        else
        {
            *escaped ++ = (T) unesc;
        }
    }

    ASSERT(*unescaped == 0);
    *escaped = (T)0;       // null terminator
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class F, class T> HRESULT EscapeUrl(const F *unescaped, T *escaped, DWORD *size)
//
//  parameters:
//
//  description:
//        Escapes URL. If escaped is null, returns only the buffer size (in characters) needed to
//        escape the url. This size includes the null string terminator.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class F, class T> HRESULT EscapeUrl(const F *unescaped, T *escaped, DWORD *size)
{
    HRESULT hr     = S_OK;
    DWORD   dwSize = SizeToEscape<F>(unescaped);

    if (unescaped && dwSize && escaped && size && *size >= dwSize)
    {
        CHK((DoEscapeUrl<F, T>(unescaped, escaped)));
    }

    if (size)
    {
        *size = dwSize;
    }

Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class F, class T> HRESULT DoUnescapeUrl(const F *escaped, T*unescaped)
//
//  parameters:
//
//  description:
//        Performs URL unescape work (assumes buffers are of correct sizes)
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class F, class T> HRESULT DoUnescapeUrl(const F *escaped, T*unescaped)
{
    while (*escaped)
    {
        F esc = *escaped ++;

        if ( esc == (F)CHAR_PERCENT )
        {
            // 2 following characters are the encoding
            T unesc = 0;
            T u     = 0;

            if (*escaped && (u = HexDigitToValue<F, T>(*escaped ++)) != (T)(-1))
            {
                unesc = u << 4;
            }
            else
                return E_INVALIDARG;

            if (*escaped && (u = HexDigitToValue<F, T>(*escaped ++)) != (T)(-1))
            {
                unesc |= u;
            }
            else
                return E_INVALIDARG;

            *unescaped++ = unesc;
        }
        else
        {
            *unescaped++ = (T)esc;
        }
    }

    ASSERT(*escaped == 0);
    *unescaped = (T)0;     // null terminator

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: template<class F, class T> HRESULT UnescapeUrl(const F *escaped, T *unescaped, DWORD *size)
//
//  parameters:
//
//  description:
//        Unescapes URL. If unescaped is null, returns only the buffer size (in characters) needed to
//        unescape the url. This size includes the null string terminator.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
template<class F, class T> HRESULT UnescapeUrl(const F *escaped, T *unescaped, DWORD *size)
{
    HRESULT hr     = S_OK;
    DWORD   dwSize = SizeToUnescape<F>(escaped);

    if (escaped && dwSize && unescaped && size && *size > dwSize)
    {
        CHK((DoUnescapeUrl<F, T>(escaped, unescaped)));
    }

    if (size)
    {
        *size = dwSize;
    }

Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline DWORD SizeToEscapeA(const CHAR *unescaped)
//
//  parameters:
//
//  description:
//        Calculates the size (in characters, including string null terminator) to escape given url.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline DWORD SizeToEscapeA(const CHAR *unescaped)
{
    return SizeToEscape<CHAR>(unescaped);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline DWORD SizeToEscapeW(const WCHAR *unescaped)
//
//  parameters:
//
//  description:
//        Calculates the size (in characters, including string null terminator) to escape given url.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline DWORD SizeToEscapeW(const WCHAR *unescaped)
{
    return SizeToEscape<WCHAR>(unescaped);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline DWORD SizeToUnescapeA(const CHAR *escaped)
//
//  parameters:
//
//  description:
//        Calculates the size (in characters, including string null terminator) to unescape given url.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline DWORD SizeToUnescapeA(const CHAR *escaped)
{
    return SizeToUnescape<CHAR>(escaped);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline DWORD SizeToUnescapeW(const WCHAR *escaped)
//
//  parameters:
//
//  description:
//        Calculates the size (in characters, including string null terminator) to unescape given url.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline DWORD SizeToUnescapeW(const WCHAR *escaped)
{
    return SizeToUnescape<WCHAR>(escaped);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT DoEscapeUrlA(const CHAR *unescaped, CHAR *escaped)
//
//  parameters:
//
//  description:
//        Performs URL escape work (assumes buffers are of correct sizes)
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT DoEscapeUrlA(const CHAR *unescaped, CHAR *escaped)
{
    return DoEscapeUrl<CHAR>(unescaped, escaped);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT DoEscapeUrlW(const WCHAR *unescaped, WCHAR *escaped)
//
//  parameters:
//
//  description:
//        Performs URL escape work (assumes buffers are of correct sizes)
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT DoEscapeUrlW(const WCHAR *unescaped, WCHAR *escaped)
{
    return DoEscapeUrl<WCHAR>(unescaped, escaped);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT EscapeUrlA(const CHAR *unescaped, CHAR *escaped, DWORD *size)
//
//  parameters:
//
//  description:
//        Escapes URL. If escaped is null, returns only the buffer size (in characters) needed to
//        escape the url. This size includes the null string terminator.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT EscapeUrlA(const CHAR *unescaped, CHAR *escaped, DWORD *size)
{
    return EscapeUrl<CHAR>(unescaped, escaped, size);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT EscapeUrlW(const WCHAR *unescaped, WCHAR *escaped, DWORD *size)
//
//  parameters:
//
//  description:
//        Escapes URL. If escaped is null, returns only the buffer size (in characters) needed to
//        escape the url. This size includes the null string terminator.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT EscapeUrlW(const WCHAR *unescaped, WCHAR *escaped, DWORD *size)
{
    return EscapeUrl<WCHAR>(unescaped, escaped, size);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT DoUnescapeUrlA(const CHAR *escaped, CHAR *unescaped)
//
//  parameters:
//
//  description:
//        Performs URL unescape work (assumes buffers are of correct sizes)
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT DoUnescapeUrlA(const CHAR *escaped, CHAR *unescaped)
{
    return DoUnescapeUrl<CHAR>(escaped, unescaped);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT DoUnescapeUrlW(const WCHAR *escaped, WCHAR *unescaped)
//
//  parameters:
//
//  description:
//        Performs URL unescape work (assumes buffers are of correct sizes)
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT DoUnescapeUrlW(const WCHAR *escaped, WCHAR *unescaped)
{
    return DoUnescapeUrl<WCHAR>(escaped, unescaped);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT UnescapeUrlA(const CHAR *escaped, CHAR *unescaped, DWORD *size)
//
//  parameters:
//
//  description:
//        Unescapes URL. If unescaped is null, returns only the buffer size (in characters) needed to
//        unescape the url. This size includes the null string terminator.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT UnescapeUrlA(const CHAR *escaped, CHAR *unescaped, DWORD *size)
{
    return UnescapeUrl<CHAR>(escaped, unescaped, size);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: inline HRESULT UnescapeUrlW(const WCHAR *escaped, WCHAR *unescaped, DWORD *size)
//
//  parameters:
//
//  description:
//        Unescapes URL. If unescaped is null, returns only the buffer size (in characters) needed to
//        unescape the url. This size includes the null string terminator.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
inline HRESULT UnescapeUrlW(const WCHAR *escaped, WCHAR *unescaped, DWORD *size)
{
    return UnescapeUrl<WCHAR>(escaped, unescaped, size);
}


HRESULT EscapeUrl(const BSTR unescaped, BSTR *escaped);
HRESULT UnescapeUrl(const BSTR escaped, BSTR *unescaped);


#endif  // __URLUTIL_H_INCLUDED__
