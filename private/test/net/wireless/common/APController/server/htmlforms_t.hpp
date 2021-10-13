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
// Definitions and declarations for the HtmlForms_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_HtmlForms_t_
#define _DEFINED_HtmlForms_t_
#pragma once

#include "APCUtils.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Miscellaneous utility functions to aid in working with HTML web-pages.
//
struct HtmlUtils
{
    // Retrieves the string between the specified start and end markers
    // following the specified HTML header. If all the "marks" are located,
    // increments the current web-page "cursor":
    static const char *
    FindString(
        const char  *pCursor,
        const char  *pHeadMark,
        const char  *pStartMark,
        const char  *pEndMark,
        ce::string  *pValue);

    // Translates the specified HTML string from UTF-8 to Unicode and/or
    // interprets HTML character-entities:
    static DWORD
    DecodeHtml(
        const WCHAR *pInput,
        long         Length,
        ce::wstring *pOutput);
    static DWORD
    DecodeHtml(
        const char  *pInput,
        long         Length,
        ce::wstring *pOutput);

    // Looks up the specified form-parameter and complains if it
    // can't be found:
    static ValueMapIter
    LookupParam(
        ValueMap     &Parameters,
        const char   *pParamName,
        const char   *pParamDesc = NULL,
        const WCHAR  *pPageName  = NULL);
    static ValueMapCIter
    LookupParam(
        const ValueMap &Parameters,
        const char     *pParamName,
        const char     *pParamDesc = NULL,
        const WCHAR    *pPageName  = NULL);
};

// ----------------------------------------------------------------------------
//
// Provides a simple mechanism for parsing an HTML web-page to extract
// the data-input form(s).
//
class HtmlForm_t;
class HtmlForms_t
{
private:

    // Form "implementation" objects:
    HtmlForm_t **m_pForms;
    int          m_FormsUsed;
    int          m_FormsAllocated;

    // Copy and assignment are deliberately disabled:
    HtmlForms_t(const HtmlForms_t &src);
    HtmlForms_t &operator = (const HtmlForms_t &src);

public:
    
    // Contructor and destructor:
    HtmlForms_t(void);
    virtual
   ~HtmlForms_t(void);

    // Parses the specified HTML web-page:
    DWORD
    ParseWebPage(
        const ce::string &WebPage,
        const WCHAR     *pPageName);

    // Clears the list of forms:
    void
    Clear(void)
    {
        m_FormsUsed = 0;
    }

    // Gets the count of forms on the page:
    int
    Size(void) const
    {
        return m_FormsUsed;
    }

    // Gets the method or action of the specified form:
    const WCHAR *
    GetMethod(int FormIndex) const;
    const WCHAR *
    GetAction(int FormIndex) const;

    // Gets the list of form-fields for the specified form:
    ValueMap &
    GetFields(int FormIndex);
    const ValueMap &
    GetFields(int FormIndex) const;
};

};
};
#endif /* _DEFINED_HtmlForms_t_ */
// ----------------------------------------------------------------------------
