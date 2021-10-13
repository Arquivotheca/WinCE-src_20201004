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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the HtmlForms_t class.
//
// ----------------------------------------------------------------------------

#include "HtmlForms_t.hpp"

#include <assert.h>
#include <intsafe.h>

#ifndef _PREFAST_
#pragma warning(disable:4068)
#endif

// Define this if you want LOTS of debug output:
//#define EXTRA_DEBUG 1

using namespace ce::qa;

/* =============================== HtmlUtils =============================== */

// ----------------------------------------------------------------------------
//
// Retrieves the string between the specified start and end markers
// following the specified HTML header. If all the "marks" are located,
// increments the current web-page "cursor".
//
const char *
HtmlUtils::
FindString(
    const char *pCursor,
    const char *pHeadMark,
    const char *pStartMark,
    const char *pEndMark,
    ce::string *pValue)
{
    pValue->clear();
    if (pCursor)
    {
        const char *pStart = strstr(pCursor, pHeadMark);
        if (pStart)
        {
            pStart += strlen(pHeadMark);
            pStart = strstr(pStart, pStartMark);
            if (pStart)
            {
                pStart += strlen(pStartMark);
                const char *pEnd = strstr(pStart, pEndMark);
                if (pEnd)
                {
                    pCursor = pEnd + strlen(pEndMark);
                    pEnd--;
                    while (pStart < pEnd && isspace((unsigned char)*pStart))
                         ++pStart;
                    while (pEnd > pStart && isspace((unsigned char)*pEnd))
                         --pEnd;
                    pEnd++;
                    if (pEnd > pStart)
                    {
                        size_t length = pEnd - pStart;
                        pValue->assign(pStart, length);
                    }
                }
            }
        }
    }
    return pCursor;
}

// ----------------------------------------------------------------------------
//
// Translates the specified HTML string from UTF-8 to Unicode and/or
// interprets HTML character-entities.
//
DWORD
HtmlUtils::
DecodeHtml(
    const WCHAR *pInput,
    long         Length,
    ce::wstring *pOutput)
{
    if (0 >= Length)
    {
        pOutput->clear();
        return ERROR_SUCCESS;
    }

    // Generate the map of known character enties the first time it's needed.
    static ce::hash_map<ce::wstring, WCHAR> Entities;
    static bool                             EntitiesBuilt = false;
    if (!EntitiesBuilt)
    {
        // C0 Controls and Basic Latin:
        Entities.insert(L"quot",    (WCHAR)0x22);
        Entities.insert(L"amp",     (WCHAR)0x26);
        Entities.insert(L"apos",    (WCHAR)0x27);
        Entities.insert(L"lt",      (WCHAR)0x3C);
        Entities.insert(L"gt",      (WCHAR)0x3E);
        // ISO 8859-1 (Latin-1) characters:
        Entities.insert(L"nbsp",    (WCHAR)0xA0);
        Entities.insert(L"iexcl",   (WCHAR)0xA1);
        Entities.insert(L"cent",    (WCHAR)0xA2);
        Entities.insert(L"pound",   (WCHAR)0xA3);
        Entities.insert(L"current", (WCHAR)0xA4);
        Entities.insert(L"yen",     (WCHAR)0xA5);
        Entities.insert(L"brvbar",  (WCHAR)0xA6);
        Entities.insert(L"sect",    (WCHAR)0xA7);
        Entities.insert(L"uml",     (WCHAR)0xA8);
        Entities.insert(L"copy",    (WCHAR)0xA9);
        Entities.insert(L"ordf",    (WCHAR)0xAA);
        Entities.insert(L"laquo",   (WCHAR)0xAB);
        Entities.insert(L"not",     (WCHAR)0xAC);
        Entities.insert(L"shy",     (WCHAR)0xAD);
        Entities.insert(L"reg",     (WCHAR)0xAE);
        Entities.insert(L"macr",    (WCHAR)0xAF);
        Entities.insert(L"deg",     (WCHAR)0xB0);
        Entities.insert(L"plusmn",  (WCHAR)0xB1);
        Entities.insert(L"sup2",    (WCHAR)0xB2);
        Entities.insert(L"sup3",    (WCHAR)0xB3);
        Entities.insert(L"acute",   (WCHAR)0xB4);
        Entities.insert(L"micro",   (WCHAR)0xB5);
        Entities.insert(L"para",    (WCHAR)0xB6);
        Entities.insert(L"middot",  (WCHAR)0xB7);
        Entities.insert(L"cedil",   (WCHAR)0xB8);
        Entities.insert(L"sup1",    (WCHAR)0xB9);
        Entities.insert(L"ordm",    (WCHAR)0xBA);
        Entities.insert(L"raquo",   (WCHAR)0xBB);
        Entities.insert(L"frac14",  (WCHAR)0xBC);
        Entities.insert(L"frac12",  (WCHAR)0xBD);
        Entities.insert(L"frac34",  (WCHAR)0xBE);
        Entities.insert(L"iquest",  (WCHAR)0xBF);
        Entities.insert(L"Agrave",  (WCHAR)0xC0);
        Entities.insert(L"Aacute",  (WCHAR)0xC1);
        Entities.insert(L"Acirc",   (WCHAR)0xC2);
        Entities.insert(L"Atilde",  (WCHAR)0xC3);
        Entities.insert(L"Auml",    (WCHAR)0xC4);
        Entities.insert(L"Aring",   (WCHAR)0xC5);
        Entities.insert(L"AElig",   (WCHAR)0xC6);
        Entities.insert(L"Ccedil",  (WCHAR)0xC7);
        Entities.insert(L"Egrave",  (WCHAR)0xC8);
        Entities.insert(L"Eacute",  (WCHAR)0xC9);
        Entities.insert(L"Ecirc",   (WCHAR)0xCA);
        Entities.insert(L"Euml",    (WCHAR)0xCB);
        Entities.insert(L"Igrave",  (WCHAR)0xCC);
        Entities.insert(L"Iacute",  (WCHAR)0xCD);
        Entities.insert(L"Icirc",   (WCHAR)0xCE);
        Entities.insert(L"Iuml",    (WCHAR)0xCF);
        Entities.insert(L"ETH",     (WCHAR)0xD0);
        Entities.insert(L"Ntilde",  (WCHAR)0xD1);
        Entities.insert(L"Ograve",  (WCHAR)0xD2);
        Entities.insert(L"Oacute",  (WCHAR)0xD3);
        Entities.insert(L"Ocirc",   (WCHAR)0xD4);
        Entities.insert(L"Otilde",  (WCHAR)0xD5);
        Entities.insert(L"Ouml",    (WCHAR)0xD6);
        Entities.insert(L"times",   (WCHAR)0xD7);
        Entities.insert(L"Oslash",  (WCHAR)0xD8);
        Entities.insert(L"Ugrave",  (WCHAR)0xD9);
        Entities.insert(L"Uacute",  (WCHAR)0xDA);
        Entities.insert(L"Ucirc",   (WCHAR)0xDB);
        Entities.insert(L"Uuml",    (WCHAR)0xDC);
        Entities.insert(L"Yacute",  (WCHAR)0xDD);
        Entities.insert(L"THORN",   (WCHAR)0xDE);
        Entities.insert(L"szlig",   (WCHAR)0xDF);
        Entities.insert(L"agrave",  (WCHAR)0xE0);
        Entities.insert(L"aacute",  (WCHAR)0xE1);
        Entities.insert(L"acirc",   (WCHAR)0xE2);
        Entities.insert(L"atilde",  (WCHAR)0xE3);
        Entities.insert(L"auml",    (WCHAR)0xE4);
        Entities.insert(L"aring",   (WCHAR)0xE5);
        Entities.insert(L"aelig",   (WCHAR)0xE6);
        Entities.insert(L"ccedil",  (WCHAR)0xE7);
        Entities.insert(L"egrave",  (WCHAR)0xE8);
        Entities.insert(L"eacute",  (WCHAR)0xE9);
        Entities.insert(L"ecirc",   (WCHAR)0xEA);
        Entities.insert(L"euml",    (WCHAR)0xEB);
        Entities.insert(L"igrave",  (WCHAR)0xEC);
        Entities.insert(L"iacute",  (WCHAR)0xED);
        Entities.insert(L"icirc",   (WCHAR)0xEE);
        Entities.insert(L"iuml",    (WCHAR)0xEF);
        Entities.insert(L"eth",     (WCHAR)0xF0);
        Entities.insert(L"ntilde",  (WCHAR)0xF1);
        Entities.insert(L"ograve",  (WCHAR)0xF2);
        Entities.insert(L"oacute",  (WCHAR)0xF3);
        Entities.insert(L"ocirc",   (WCHAR)0xF4);
        Entities.insert(L"otilde",  (WCHAR)0xF5);
        Entities.insert(L"ouml",    (WCHAR)0xF6);
        Entities.insert(L"divide",  (WCHAR)0xF7);
        Entities.insert(L"oslash",  (WCHAR)0xF8);
        Entities.insert(L"ugrave",  (WCHAR)0xF9);
        Entities.insert(L"uacute",  (WCHAR)0xFA);
        Entities.insert(L"ucirc",   (WCHAR)0xFB);
        Entities.insert(L"uuml",    (WCHAR)0xFC);
        Entities.insert(L"yacute",  (WCHAR)0xFD);
        Entities.insert(L"thorn",   (WCHAR)0xFE);
        Entities.insert(L"yuml",    (WCHAR)0xFF);
        EntitiesBuilt = true;
    }

    // Make room in the output buffer.
    if (!pOutput->reserve(Length))
    {
        return ERROR_OUTOFMEMORY;
    }

    // Copy to the output buffer and resolve the character-entities.
    const WCHAR *bufPtr = &pInput[0];
    const WCHAR *bufEnd = &pInput[Length];
    WCHAR *outPtr = &(*pOutput)[0];
    for (; bufPtr < bufEnd ; ++outPtr)
    {
        WCHAR ch = *(bufPtr++);
       *outPtr = ch;
        if (L'&' != ch)
            continue;

        const WCHAR *enStart, *enEnd;
        enStart = enEnd = bufPtr;

        // Numeric entities:
        if (L'#' == enStart[0])
        {
            ++enEnd;
            unsigned int chValue = 0;
            switch (enEnd[0])
            {
                // Not a valid entity.
                default:
                    continue;

                // Decimal entity.
                case L'0': case L'1': case L'2': case L'3': case L'4':
                case L'5': case L'6': case L'7': case L'8': case L'9':
                    chValue = static_cast<unsigned int>(enEnd[0]) - L'0';
                    while (++enEnd < &enStart[7] && enEnd < bufEnd)
                    {
                        unsigned int newValue = chValue * 10;
                        switch (enEnd[0])
                        {
                            default:
                                goto deciLoopEnd;
                            case L';':
                                ++enEnd;
                                goto deciLoopEnd;
                            case L'0': case L'1': case L'2': case L'3': case L'4':
                            case L'5': case L'6': case L'7': case L'8': case L'9':
                                newValue += static_cast<unsigned int>(enEnd[0]) - L'0';
                                break;
                        }
                        if (newValue > 0xFFFF)
                            goto deciLoopEnd;
                        chValue = newValue;
                    }
deciLoopEnd:
                    break;

                // Hexadecimal entity.
                case L'x': case L'X':
                    while (++enEnd < &enStart[6] && enEnd < bufEnd)
                    {
                        unsigned int newValue = chValue << 4;
                        switch (enEnd[0])
                        {
                            default:
                                goto hexiLoopEnd;
                            case L';':
                                ++enEnd;
                                goto hexiLoopEnd;
                            case L'0': case L'1': case L'2': case L'3': case L'4':
                            case L'5': case L'6': case L'7': case L'8': case L'9':
                                newValue += static_cast<unsigned int>(enEnd[0]) - L'0';
                                break;
                            case L'a': case L'b': case L'c':
                            case L'd': case L'e': case L'f':
                                newValue += static_cast<unsigned int>(enEnd[0]) - (L'a' - 10);
                                break;
                            case L'A': case L'B': case L'C':
                            case L'D': case L'E': case L'F':
                                newValue += static_cast<unsigned int>(enEnd[0]) - (L'A' - 10);
                                break;
                        }
                        if (newValue > 0xFFFF)
                            goto hexiLoopEnd;
                        chValue = newValue;
                    }
hexiLoopEnd:
                    break;
            }

           *outPtr = static_cast<WCHAR>(chValue);
            bufPtr = enEnd;
        }

        // Character entities:
        else
        {
            // Find the terminating semi-colon (if any).
            while (enEnd < &enStart[10] && L';'  != enEnd[0]
                                        && L'&'  != enEnd[0]
                                        && L'\0' != enEnd[0] && enEnd < bufEnd)
                ++enEnd;
            if (enEnd < &enStart[3] || (L';'  != enEnd[0]
                                     && L'&'  != enEnd[0]
                                     && L'\0' != enEnd[0]))                 
                continue;

            // Convert the entity to lower case.
            WCHAR entity[12], *op;
            const WCHAR *ip;
            for (ip = enStart, op = entity ; ip < enEnd ; ++ip, ++op)
                *op = towlower(*ip);
            *op = L'\0';
	
            // Some character entity references are case-sensitive, so
            // we fix them manually before doing the hash-table lookup.
			if (0 == wcscmp(entity, L"eth")
             || 0 == wcscmp(entity, L"thorn"))
			{
				if (iswupper(enStart[0]))
                {
                    for (ip = enStart, op = entity ; ip < enEnd ; ++ip, ++op)
                        *op = towupper(*ip);
                }
			}
			else
            if (0 == wcscmp(&entity[0], L"oslash")
             || 0 == wcscmp(&entity[1], L"grave")
             || 0 == wcscmp(&entity[1], L"acute")
             || 0 == wcscmp(&entity[1], L"circ")
             || 0 == wcscmp(&entity[1], L"uml")
             || 0 == wcscmp(&entity[1], L"tilde")
             || 0 == wcscmp(&entity[1], L"cedil")
             || 0 == wcscmp(&entity[1], L"ring"))
			{
                entity[0] = enStart[0];
			}
			else
            if (0 == wcscmp(entity, L"aelig"))
			{
                if (iswupper(enStart[0]))
                {
				    entity[0] = L'A';
				    entity[1] = L'E';
                }
			}

            // Look it up in the hash-table.
            ce::hash_map<ce::wstring, WCHAR>::const_iterator it;
            it = Entities.find(entity);
            if (it != Entities.end())
            {
               *outPtr = it->second;
                bufPtr = enEnd;
                if (L';' == bufPtr[0])
                    bufPtr++;
            }
        }
    }

   *outPtr = L'\0';
    pOutput->resize(outPtr - &(*pOutput)[0]);
    return ERROR_SUCCESS;
}

DWORD
HtmlUtils::
DecodeHtml(
    const char  *pInput,
    long         Length,
    ce::wstring *pOutput)
{
    if (0 >= Length)
    {
        pOutput->clear();
        return ERROR_SUCCESS;
    }
    else
    {
        ce::wstring buffer;
        return ce::MultiByteToWideChar(CP_UTF8, pInput, Length, &buffer)
             ? DecodeHtml(&buffer[0], buffer.length(), pOutput)
             : GetLastError();
    }
}

// ----------------------------------------------------------------------------
//
// Looks up the specified form-parameter and complains if it can't be found.
//
ValueMapIter
HtmlUtils::
LookupParam(
    ValueMap    &Parameters,
    const char  *pParamName,
    const char  *pParamDesc,
    const WCHAR *pPageName)
{
    ValueMapIter it = Parameters.find(pParamName);
    if (Parameters.end() == it && pParamDesc && pPageName)
    {
        LogError(TEXT("%hs element \"%hs\" missing from \"%ls\""),
                 pParamDesc, pParamName, pPageName);
    }
    return it;
}
ValueMapCIter
HtmlUtils::
LookupParam(
    const ValueMap &Parameters,
    const char     *pParamName,
    const char     *pParamDesc,
    const WCHAR    *pPageName)
{
    // 9/30/05 - This ugliness is required by a bug in the Bowmore version
    // of hash.hxx which disables "find" on const hash-list objects...
    ValueMapCIter it = const_cast<ValueMap &>(Parameters).find(pParamName);
    if (Parameters.end() == it && pParamDesc && pPageName)
    {
        LogError(TEXT("%hs element \"%hs\" missing from \"%ls\""),
                 pParamDesc, pParamName, pPageName);
    }
    return it;
}

/* ============================= HtmlElement_t ============================= */

// ----------------------------------------------------------------------------
//
// Represents a random HTML element in a web-page.
//
namespace ce {
namespace qa {
class HtmlElement_t
{
private:

    // Web-page containing the element:
    const char *m_pPageStart;
    const char *m_pPageEnd;

    // Element type (e.g., "<form>", "<input>", "<select>"):
    char m_Type[16];
    const char *m_pTypeEnd;

    // Is it an "end" type (e.g., "</form>", "</input>", "</select>"):
    bool m_IsEndType;

    // Start and end of element within page:
    const char *m_pStart;
    const char *m_pEnd;

    // List of element attributes:
    ValueMap m_Attributes;
    bool     m_bAttribsSet;  // has map been set yet?

public:

    // Constructor and destructor:
    HtmlElement_t(
        const char *pPageStart,
        const char *pPageEnd)
        : m_pPageStart(pPageStart),
          m_pPageEnd  (pPageEnd)
    { }
   ~HtmlElement_t(void)
    { }

    // Parses the web-page beginning at the specified offset:
    // Returns ERROR_SECTOR_NOT_FOUND if there are no more elements.
    DWORD
    Parse(
        const char  *pCursor,
        const WCHAR *pPageName);

    // Gets the element type (e.g., "form", "input", "select"):
    const char *
    GetType(void) const { return m_Type; }

    // Is it an "end" type (e.g., "</form>", "</input>", "</select>"):
    bool
    IsEndType(void) const { return m_IsEndType; }

    // Gets the start and end of the element within the page:
    const char *
    GetStart(void) const { return m_pStart; }
    const char *
    GetEnd(void) const { return m_pEnd; }

    // Gets the list of element attributes:
    ValueMap &
    GetAttributes(void);
};
};
};

// ----------------------------------------------------------------------------
//
// Parses the web-page beginning at the specified offset.
// Returns ERROR_SECTOR_NOT_FOUND if there are no more elements.
//
DWORD
HtmlElement_t::
Parse(
    const char  *pCursor,
    const WCHAR *pPageName)
{
restart:
    assert(m_pPageStart <= pCursor && pCursor <= m_pPageEnd);

    // Find the beginning of the element.
    while (pCursor < m_pPageEnd && pCursor[0] != '<')
           pCursor++;
    m_pStart = pCursor;
    while (pCursor < m_pPageEnd && isspace(static_cast<unsigned char>(pCursor[0])))
           pCursor++;
    if (pCursor++ >= m_pPageEnd)
        return ERROR_SECTOR_NOT_FOUND;

    // If it's a comment, scan to the end and start over.
    if (pCursor[0] == '!' && pCursor[1] == '-' && pCursor[2] == '-')
    {
        for (; pCursor < m_pPageEnd ; ++pCursor)
        {
            if (pCursor[0] == '-' && pCursor[1] == '-' && pCursor[2] == '>')
            {
                pCursor += 3;
                goto restart;
            }
        }
        return ERROR_SECTOR_NOT_FOUND;
    }

    // Check for an "end" type.
    if (pCursor[0] == '/')
    {
        m_IsEndType = true;
        pCursor++;
    }
    else
    {
        m_IsEndType = false;
    }

    // Get the element type.
    for (int tx = 0 ;; ++tx, ++pCursor)
    {
        char ch = pCursor[0];
        if (isalpha(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_')
        {
#pragma prefast(suppress: 394, "Potential buffer overrun is impossible") 
            m_Type[tx] = (char)tolower(ch);
            if (tx < sizeof(m_Type)-2)
                continue;
        }
        m_Type[tx] = '\0';
        m_pTypeEnd = pCursor;
        break;
    }

    // If there's no type, skip the element.
    if (m_Type[0] == '\0')
        goto restart;

    // Find the end of the element.
    while (pCursor < m_pPageEnd && pCursor[0] != '>')
           pCursor++;
    m_pEnd = pCursor;
    if (pCursor++ >= m_pPageEnd)
    {
        LogError(TEXT("Unterminated \"%hs\" element at offset %d in %ls"),
                 m_Type, m_pStart - m_pPageStart,
                 pPageName);
        return ERROR_INVALID_PARAMETER;
    }

    m_bAttribsSet = false;
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets the list of element attributes.
//
ValueMap &
HtmlElement_t::
GetAttributes(void)
{
    if (m_bAttribsSet)
        return m_Attributes;

    m_Attributes.clear();
    assert(m_pPageStart <= m_pTypeEnd && m_pTypeEnd <= m_pPageEnd);

    ce::string name;
    ce::wstring value;
    const char *pCursor = m_pTypeEnd;
    while (pCursor < m_pEnd)
    {
        // Find the attribute name.
        const char *pNameStart, *pNameEnd;
        while (pCursor < m_pEnd && !isalnum(static_cast<unsigned char>(pCursor[0])))
               pCursor++;
        if (pCursor >= m_pEnd)
            break;
        pNameStart = pCursor++;
        while (pCursor < m_pEnd && isalnum(static_cast<unsigned char>(pCursor[0])))
               pCursor++;
        pNameEnd = pCursor;

        // Translate the name to lower case.
        if (name.resize(pNameEnd - pNameStart))
        {
            const char *ip = pNameStart;
            char       *op = &name[0];
            while (ip < pNameEnd)
                *(op++) = (char)tolower(*(ip++));
        }
        
        // Find the attribute value (if any).
        const char *pValueStart, *pValueEnd;
        while (pCursor < m_pEnd && isspace(static_cast<unsigned char>(pCursor[0])))
               pCursor++;
        pValueStart = pValueEnd = pCursor;
        if (pCursor[0] == '=')
        {
            pCursor++;
            while (pCursor < m_pEnd && isspace(static_cast<unsigned char>(pCursor[0])))
                   pCursor++;
            if (pCursor < m_pEnd)
            {
                if (pCursor[0] == '"' || pCursor[0] == '\'')
                {
                    char quote = pCursor[0];
                    pValueStart = pValueEnd = ++pCursor;
                    while (pCursor < m_pEnd && pCursor[0] != quote)
                    {
                        if (*(pCursor++) == '\\')
                              pCursor++;
                    }
                    if (pCursor < m_pEnd)
                    {
                        pValueEnd = pCursor++;
                    }
#ifdef DEBUG
                    else
                    {
                        LogWarn(TEXT("Unterminated string at offset %d"),
                                pValueStart - m_pPageStart);
                    }
#endif
                }
                else
                if (isalnum(static_cast<unsigned char>(pCursor[0])))
                {
                    pValueStart = pCursor;
                    pValueEnd = ++pCursor;
                    while (pCursor < m_pEnd
                        && isalnum(static_cast<unsigned char>(pCursor[0])))
                    {
                        pCursor++;
                    }
                    if (pCursor < m_pEnd)
                    {
                        pValueEnd = pCursor++;
                    }
                }
            }
        }

        // Add the name-value pair to the map.
        if (pValueEnd <= pValueStart)
        {
            m_Attributes.insert(name, L"");
        }
        else
        {
            DWORD result = HtmlUtils::DecodeHtml(pValueStart,
                                     pValueEnd - pValueStart, &value);
            if (ERROR_SUCCESS == result)
            {
                m_Attributes.insert(name, value);
            }
            else
            {
                LogError(TEXT("Error translating form-field \"%hs\": %s"),
                         &name[0], Win32ErrorText(result));
            }
        }
    }

    return m_Attributes;
}

/* ============================== HtmlForm_t =============================== */

// ----------------------------------------------------------------------------
//
// Represents a <form> element within a web-page.
//
namespace ce {
namespace qa {
class HtmlForm_t
{
private:

    // Web-page containing the form:
    const char *m_pPageStart;
    const char *m_pPageEnd;

    // Method and action:
    ce::wstring m_Method;
    ce::wstring m_Action;

    // Start and end of form within page:
    const char *m_pStart;
    const char *m_pEnd;

    // List of input elements:
    ValueMap m_Fields;

public:

    // Constructor and destructor:
    HtmlForm_t(const ce::string &WebPage)
        : m_pPageStart(&WebPage[0]),
          m_pPageEnd  (&WebPage[WebPage.length()])
    { }
   ~HtmlForm_t(void)
    { }

    // Parses the web-page beginning at the specified offset:
    // Returns ERROR_SECTOR_NOT_FOUND if there are no more forms.
    DWORD
    Parse(
        const char  *pCursor,
        const WCHAR *pPageName);

    // Gets the method or action:
    const WCHAR *
    GetMethod(void) const { return m_Method; }
    const WCHAR *
    GetAction(void) const { return m_Action; }

    // Gets the start or end of the form within the page:
    const char *
    GetStart(void) const { return m_pStart; }
    const char *
    GetEnd(void) const { return m_pEnd; }

    // Gets the list for input fields:
    ValueMap &
    GetFields(void) { return m_Fields; }
    const ValueMap &
    GetFields(void) const { return m_Fields; }
};
};
};

// ----------------------------------------------------------------------------
//
// Inserts the specified name-value pair into the specified field-map.
// If necessary, takes care of converting the value to Unicode.
// Doesn't replace an existing value with an "empty" one.
//
static DWORD
InsertField(
    ValueMap          *pMap,
    const ce::string  &Name,
    const ce::wstring &Value,
    const WCHAR       *pPageName)
{
    ValueMapIter it = pMap->find(Name);
    if (Value.length())
    {
        if (pMap->end() != it)
            pMap->erase(it);
        pMap->insert(Name, Value);
    }
    else
    if (pMap->end() == it)
    {
        pMap->insert(Name, Value);
    }
    return ERROR_SUCCESS;
}

static DWORD
InsertField(
    ValueMap          *pMap,
    const ce::wstring &Name,
    const ce::wstring &Value,
    const WCHAR       *pPageName)
{
    ce::string mbName;
    if (ce::WideCharToMultiByte(CP_UTF8, &Name[0], Name.length(), &mbName))
    {
        return InsertField(pMap, mbName, Value, pPageName);
    }
    else
    {
        DWORD result = GetLastError();
        LogError(TEXT("Error translating field-name \"%ls\" in page %ls: %s"),
                 &Name[0], pPageName, Win32ErrorText(result));
        return result;
    }
}

// ----------------------------------------------------------------------------
//
// Inserts the specified select-list <option> value. Since this <option>
// didn't contain an explicit value attribute the value must be extracted
// from the area between the end of the <option> and the beginning of the
// next element.
//
static DWORD
InsertOptionValue(
    ValueMap          *pMap,
    const ce::wstring &Name,
    const char        *pValueStart,  // '>' at end of <option>
    const char        *pValueEnd,    // '<' at start of next element
    const WCHAR       *pPageName)
{
    while (++pValueStart < pValueEnd && isspace(static_cast<unsigned char>(pValueStart[0])))
        ;
    while (pValueStart < --pValueEnd && isspace(static_cast<unsigned char>(pValueEnd[0])))
        ;
    pValueEnd++;

    ce::wstring value;
    DWORD result = HtmlUtils::DecodeHtml(pValueStart,
                             pValueEnd - pValueStart, &value);
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Error translating field \"%ls\" value in page %ls: %s"),
                &Name[0], pPageName, Win32ErrorText(result));
        return result;
    }

    return InsertField(pMap, Name, value, pPageName);
}

// ----------------------------------------------------------------------------
//
// Parses the web-page beginning at the specified offset.
// Returns ERROR_SECTOR_NOT_FOUND if there are no more forms.
//
DWORD
HtmlForm_t::
Parse(
    const char  *pCursor,
    const WCHAR *pPageName)
{
    DWORD result;
    assert(m_pPageStart <= pCursor && pCursor <= m_pPageEnd);
    HtmlElement_t element(m_pPageStart,m_pPageEnd);

    // Find the <form> element (if possible).
    for (;; pCursor = element.GetEnd())
    {
        result = element.Parse(pCursor, pPageName);
        if (ERROR_SUCCESS != result)
            return result;
        if (strcmp(element.GetType(), "form") == 0)
        {
            ValueMap &att = element.GetAttributes();
            ValueMapIter it;
            it = att.find("method");
            if (it == att.end())
                 m_Method.clear();
            else m_Method = it->second;
            it = att.find("action");
            if (it == att.end())
                 m_Action.clear();
            else m_Action = it->second;
            break;
        }

#ifdef EXTRA_DEBUG
        LogDebug(TEXT("%05d-%05d - %hs%hs (skipped):"),
                 element.GetStart() - m_pPageStart,
                 element.GetEnd()   - m_pPageStart,
                 element.GetType(),
                 element.IsEndType()? " (end)" : "");
        const ValueMap &map = element.GetAttributes();
        for (ValueMapCIter it = map.begin() ; it != map.end() ; ++it)
        {
            LogDebug(TEXT("  %hs = \"%ls\""), &it->first[0], &it->second[0]);
        }
#endif
    }

    // Parse the form contents.
    bool inSelect = false;
    ce::wstring selectName;
    const char *optionValue = NULL;
    m_pStart = element.GetStart();
    for (;;)
    {
        pCursor = element.GetEnd();
        result = element.Parse(pCursor, pPageName);
        if (ERROR_SECTOR_NOT_FOUND == result)
            break;
        if (ERROR_SUCCESS != result)
            return result;

#ifdef EXTRA_DEBUG
        LogDebug(TEXT("%05d-%05d - %hs%hs (included):"),
                 element.GetStart() - m_pPageStart,
                 element.GetEnd()   - m_pPageStart,
                 element.GetType(),
                 element.IsEndType()? " (end)" : "");
        const ValueMap &map = element.GetAttributes();
        for (ValueMapCIter it = map.begin() ; it != map.end() ; ++it)
        {
            LogDebug(L"  %hs = \"%ls\"", &it->first[0], &it->second[0]);
        }
#endif

        // Watch for the end of the form.
        if (strcmp(element.GetType(), "form") == 0)
        {
            if (element.IsEndType())
                pCursor = element.GetEnd();
            break;
        }

        // <input> elements:
        else
        if (strcmp(element.GetType(), "input") == 0 && !element.IsEndType())
        {
            // Get the input type - default to "text".
            ValueMap &att = element.GetAttributes();
            ValueMapIter it = att.find("type");
            const WCHAR *type = (it == att.end())? L"text" : &it->second[0];

            // checkbox:
            if (_wcsicmp(type, L"checkbox") == 0)
            {
                it = att.find("checked");
                if (it != att.end())
                {
                    it = att.find("name");
                    if (it == att.end())
                    {
                        LogError(TEXT("<checkbox> name missing")
                                 TEXT(" at offset %d in %ls"),
                                 element.GetStart() - m_pPageStart,
                                 pPageName);
                        return ERROR_INVALID_PARAMETER;
                    }
                    const ce::wstring &name = it->second;
                          ce::wstring  value;
                    it = att.find("value");
                    if (it != att.end())
                         value = it->second;
                    else value = L"true";
                    result = InsertField(&m_Fields, name, value, pPageName);
                    if (ERROR_SUCCESS != result)
                        return result;
                }
            }

            // radio:
            else
            if (_wcsicmp(type, L"radio") == 0)
            {
                it = att.find("name");
                if (it == att.end())
                {
                    LogError(TEXT("<radio> name missing")
                             TEXT(" at offset %d in %ls"),
                             element.GetStart() - m_pPageStart,
                             pPageName);
                    return ERROR_INVALID_PARAMETER;
                }
                const ce::wstring &name = it->second;
                if (att.find("checked") == att.end())
                {
                    result = InsertField(&m_Fields, name, L"", pPageName);
                    if (ERROR_SUCCESS != result)
                        return result;
                }
                else
                {
                    it = att.find("value");
                    if (it == att.end())
                    {
                        LogError(TEXT("\"%ls\" value missing")
                                 TEXT(" at offset %d in %ls"),
                                &name[0], element.GetStart() - m_pPageStart,
                                 pPageName);
                        return ERROR_INVALID_PARAMETER;
                    }
                    const ce::wstring &value = it->second;
                    result = InsertField(&m_Fields, name, value, pPageName);
                    if (ERROR_SUCCESS != result)
                        return result;
                }
            }

            // submit:
            else
            if (_wcsicmp(type, L"submit") == 0)
            {
                it = att.find("name");
                if (it != att.end())
                {
                    const ce::wstring &name = it->second;
                    it = att.find("value");
                    if (it != att.end())
                    {
                        const ce::wstring &value = it->second;
                        result = InsertField(&m_Fields, name, value, pPageName);
                        if (ERROR_SUCCESS != result)
                            return result;
                    }
                }
            }

            // text, hidden and password:
            else
            if (_wcsicmp(type, L"text") == 0
             || _wcsicmp(type, L"hidden") == 0
             || _wcsicmp(type, L"password") == 0)
            {
                it = att.find("name");
                if (it == att.end())
                {
                    LogError(TEXT("<%ls> name missing")
                             TEXT(" at offset %d in %ls"),
                             type, element.GetStart() - m_pPageStart,
                             pPageName);
                    return ERROR_INVALID_PARAMETER;
                }
                const ce::wstring &name = it->second;
                it = att.find("value");
                if (it == att.end())
                {
                    LogError(TEXT("\"%ls\" value missing")
                             TEXT(" at offset %d in %ls"),
                            &name[0], element.GetStart() - m_pPageStart,
                             pPageName);
                    return ERROR_INVALID_PARAMETER;
                }
                const ce::wstring &value = it->second;
                result = InsertField(&m_Fields, name, value, pPageName);
                if (ERROR_SUCCESS != result)
                    return result;
            }

            // Ignore reset:
            else
            if (_wcsicmp(type, L"reset") != 0)
            {
                LogError(TEXT("<input> type \"%ls\" unrecognized")
                         TEXT(" at offset %d in %ls"),
                         type, element.GetStart() - m_pPageStart,
                         pPageName);
                return ERROR_INVALID_PARAMETER;
            }
        }

        // <select> elements:
        else
        if (strcmp(element.GetType(), "select") == 0)
        {
            if (element.IsEndType())
            {
                if (optionValue)
                {
                    result = InsertOptionValue(&m_Fields, selectName,
                                               optionValue, element.GetStart(),
                                               pPageName);
                    if (ERROR_SUCCESS != result)
                        return result;
                    optionValue = NULL;
                }
                inSelect = false;
            }
            else
            {
                ValueMap &att = element.GetAttributes();
                ValueMapIter it = att.find("name");
                if (it == att.end())
                {
                    LogError(TEXT("<select> name missing")
                             TEXT(" at offset %d in %ls"),
                             element.GetStart() - m_pPageStart,
                             pPageName);
                    return ERROR_INVALID_PARAMETER;
                }
                selectName = it->second;
                inSelect = true;
            }
        }

        // <option> elements:
        else
        if (inSelect && strcmp(element.GetType(), "option") == 0)
        {
            if (optionValue)
            {
                result = InsertOptionValue(&m_Fields, selectName,
                                           optionValue, element.GetStart(),
                                           pPageName);
                if (ERROR_SUCCESS != result)
                    return result;
                optionValue = NULL;
            }
            if (!element.IsEndType())
            {
                ValueMap &att = element.GetAttributes();
                if (att.find("selected") != att.end())
                {
                    ValueMapIter it = att.find("value");
                    if (it == att.end())
                    {
                        optionValue = element.GetEnd();
                    }
                    else
                    {
                        result = InsertField(&m_Fields, selectName, it->second,
                                             pPageName);
                        if (ERROR_SUCCESS != result)
                            return result;
                    }
                }
            }
        }

        else
        {
#ifdef EXTRA_DEBUG
            LogDebug(TEXT("  ignored"));
#endif
            inSelect = false;
            optionValue = NULL;
        }
    }
    m_pEnd = pCursor;

    return ERROR_SUCCESS;
}

/* ============================== HtmlForms_t ============================== */

// ----------------------------------------------------------------------------
//
// Constructor.
//
HtmlForms_t::
HtmlForms_t(void)
    : m_pForms(NULL),
      m_FormsUsed(0),
      m_FormsAllocated(0)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
HtmlForms_t::
~HtmlForms_t(void)
{
    if (m_pForms)
    {
        for (int fx = m_FormsUsed ; --fx >= 0 ;)
        {
            if (m_pForms[fx])
            {
                delete m_pForms[fx];
                m_pForms[fx] = NULL;
            }
        }
        free(m_pForms);
        m_pForms = NULL;
    }
    m_FormsUsed = m_FormsAllocated = 0;
}

// ----------------------------------------------------------------------------
//
// Parses the specified HTML web-page.
//
DWORD
HtmlForms_t::
ParseWebPage(
    const ce::string &WebPage,
    const WCHAR     *pPageName)
{
#ifdef EXTRA_DEBUG
    LogDebug(TEXT("Parsing web-page \"%ls\" length=%d"),
             pPageName, WebPage.length());
#endif
    Clear();

    const char *pCursor = &WebPage[0];
    for (;;)
    {
        auto_ptr<HtmlForm_t> pForm = new HtmlForm_t(WebPage);
        if (!pForm.valid())
        {
            LogError(TEXT("Can't allocate %u bytes for HTML form"),
                     sizeof(HtmlForm_t));
            return ERROR_OUTOFMEMORY;
        }

        DWORD result = pForm->Parse(pCursor, pPageName);
        if (ERROR_SECTOR_NOT_FOUND == result)
            break;
        if (ERROR_SUCCESS != result)
            return result;
        pCursor = pForm->GetEnd();

        if (m_FormsUsed >= m_FormsAllocated)
        {
            int newAlloc = m_FormsAllocated * 2 + 16;
            size_t allocSize; HtmlForm_t **newForms;
            HRESULT hr = SizeTMult(newAlloc, sizeof(HtmlForm_t *), &allocSize);
            if (FAILED(hr))
            {
                allocSize = (size_t)-1;
                newForms = NULL;
            }
            else
            {
                newForms = static_cast<HtmlForm_t **>(malloc(allocSize));
            }
            if (NULL == newForms)
            {
                LogError(TEXT("Can't allocate %u bytes for HTML forms"),
                         allocSize);
                return ERROR_OUTOFMEMORY;
            }
            if (NULL != m_pForms)
            {
                size_t oldSize = m_FormsAllocated * sizeof(HtmlForm_t *);
                memcpy(newForms, m_pForms, oldSize);
                free(m_pForms);
            }
            m_pForms = newForms;
            m_FormsAllocated = newAlloc;
        }

#ifdef EXTRA_DEBUG
        LogDebug(TEXT("Form #%d:"), m_FormsUsed);
        LogDebug(TEXT("  method = %ls"), pForm->GetMethod());
        LogDebug(TEXT("  action = %ls"), pForm->GetAction());
        const ValueMap &fields = pForm->GetFields();
        LogDebug(TEXT("  %d form elements:"), fields.size());
        for (ValueMapCIter it = fields.begin() ; it != fields.end() ; ++it)
        {
            LogDebug(TEXT("    %hs = \"%ls\""), &it->first[0], &it->second[0]);
        }
#endif
        
#pragma prefast(suppress: 394, "Potential buffer overrun is impossible") 
        m_pForms[m_FormsUsed++] = pForm.release();
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets the name or action of the specified form.
//
const WCHAR *
HtmlForms_t::
GetMethod(int FormIndex) const
{
    assert(0 <= FormIndex && FormIndex < m_FormsUsed);
    return m_pForms[FormIndex]->GetMethod();
}
const WCHAR *
HtmlForms_t::
GetAction(int FormIndex) const
{
    assert(0 <= FormIndex && FormIndex < m_FormsUsed);
    return m_pForms[FormIndex]->GetAction();
}

// ----------------------------------------------------------------------------
//
// Gets the list of form-fields for the specified form.
//
ValueMap &
HtmlForms_t::
GetFields(int FormIndex)
{
    assert(0 <= FormIndex && FormIndex < m_FormsUsed);
    return m_pForms[FormIndex]->GetFields();
}

const ValueMap &
HtmlForms_t::
GetFields(int FormIndex) const
{
    assert(0 <= FormIndex && FormIndex < m_FormsUsed);
    return m_pForms[FormIndex]->GetFields();
}

// ----------------------------------------------------------------------------
