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
// Definitions and declarations for the MACAddr_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_MACAddr_t_
#define _DEFINED_MACAddr_t_
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
// MAC (Medium Access Control) address.
//
struct MACAddr_t
{
    BYTE m_Addr[6];

    MACAddr_t(void) { Clear(); }

    // Clears the address data:
    void
    Clear(void);

    // Inserts the specified binary data:
    void
    Assign(
        const BYTE *pData,
        DWORD        DataLen);
    
    // Determines whether the address has been set:
    bool
    IsValid(void) const;

    // Translates the value to and from text:
    void
    ToString(
      __out_ecount(BufferChars) char *pBuffer,
                                int    BufferChars) const;
    void
    ToString(
      __out_ecount(BufferChars) WCHAR *pBuffer,
                                int     BufferChars) const;
    HRESULT
    FromString(
        const char *pString);
    HRESULT
    FromString(
        const WCHAR *pString);
};

};
};
#endif /* _DEFINED_MACAddr_t_ */
// ----------------------------------------------------------------------------
