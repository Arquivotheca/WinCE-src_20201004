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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// WEP (Wired Equivalent Privacy) keys.
//
struct WEPKeys_t
{
    BYTE m_KeyIndex;
    BYTE m_Size[4];
    BYTE m_Material[4][32];

    WEPKeys_t(void) { Clear(); }

    // Clears the key data:
    void
    Clear(void);
    
    // Determines whether the keys have been set:
    bool
    IsValid(void) const;

    // Determines whether the specified key size is valid:
    static bool
    ValidKeySize(int KeySize);

    // Translates the specified key value to text:
    void
    ToString(
                                int    KeyIndex,
      __out_ecount(BufferChars) char *pBuffer,
                                int    BufferChars) const;
    void
    ToString(
                                int     KeyIndex,
      __out_ecount(BufferChars) WCHAR *pBuffer,
                                int     BufferChars) const;

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
};

};
};
#endif /* _DEFINED_WEPKeys_t_ */
// ----------------------------------------------------------------------------
