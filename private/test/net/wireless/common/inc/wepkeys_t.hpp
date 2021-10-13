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
// Definitions and declarations for the WEPKeys_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WEPKeys_t_
#define _DEFINED_WEPKeys_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>

namespace litexml
{
    class XmlBaseElement_t;
    class XmlElement_t;
};

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// WEP (Wired Equivalent Privacy) keys.
//
struct WEPKeys_t
{
    static const int KeyCount = 4;

    BYTE m_KeyIndex;
    BYTE m_Size[KeyCount];
    BYTE m_Material[KeyCount][32];

    WEPKeys_t(void) { Clear(); }

    // Clears the key data:
    void
    Clear(void);
    
    // Determines whether the keys have been set:
    bool
    IsValid() const;

    // Determines if all keys are empty:
    bool
    AreAllEmpty() const;

    // Determines whether the specified key size is valid:
    static bool
    ValidKeySize(int KeySize);

    // Translates the specified key value to text:
    HRESULT
    ToString(
                                int    KeyIndex,
      __out_ecount(BufferChars) char  *pBuffer,
                                int    BufferChars,
                                BOOL   addDot = TRUE) const;
    HRESULT
    ToString(
                                int     KeyIndex,
      __out_ecount(BufferChars) WCHAR  *pBuffer,
                                int     BufferChars,
                                BOOL    addDot = TRUE) const;

    // Translates the specified text to key value:
    // Also sets the current key-index.
    HRESULT
    FromString(
        int         KeyIndex,
        const char *pString);
    HRESULT
    FromString(
        int          KeyIndex,
        const WCHAR *pString);

    // Translates the value to and from XML:
    HRESULT
    AddXmlElement(
        litexml::XmlElement_t  *pRoot,
        const WCHAR            *pName,
        litexml::XmlElement_t **ppElem = NULL) const;   
    HRESULT
    GetXmlElement(
        const litexml::XmlElement_t  &Root,
        const WCHAR                  *pName,
        const litexml::XmlElement_t **ppElem = NULL);

    // Compare to a supplied WEPKeys_t:
    int
    Compare(
        const WEPKeys_t& keys) const;

    // Compare one key to a supplied WEPKeys_t:
    int
    CompareKey(
        const WEPKeys_t& keys,
        int              KeyIndex) const;

    // Copy one key from a supplied WEPKeys_t:
    HRESULT
    CopyKey(
        const WEPKeys_t& keys,
        int              KeyIndex);
};

};
};
#endif /* _DEFINED_WEPKeys_t_ */
// ----------------------------------------------------------------------------
