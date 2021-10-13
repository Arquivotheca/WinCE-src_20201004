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
// Definitions and declarations for the ConfigArgList_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_ConfigArgList_t_
#define _DEFINED_ConfigArgList_t_
#pragma once

#include <ConfigArg_t.hpp>
#include <MemBuffer_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Represents a list of ConfigArg objects.
//
class ConfigArgList_t
{
private:

    // Argument list:
    MemBuffer_t m_ArgList;
    MemBuffer_t m_ArgFlags;
    int         m_ArgListSize;

    // Argument flags:
    static const DWORD sm_fDeleteOnClose = 0x0001; // Delete object when finished
    
    // Copy and assignment are deliberately disabled:
    ConfigArgList_t(const ConfigArgList_t &rhs);
    ConfigArgList_t &operator = (const ConfigArgList_t &rhs);
    
public:

    // Constructor/Destructor:
    ConfigArgList_t(void);
   ~ConfigArgList_t(void);

 // Accessors:

    int Length(void) const { return m_ArgListSize; }
          ConfigArg_t &operator[](int Index);
    const ConfigArg_t &operator[](int Index) const;
    
    // Adds the specified object to the list:
    // Handles a NULL pointer with an out-of-memory indication.
    // Note that if fDeleteOnClose is true the object will be deleted by
    // this list when it cleans up. Hence, in that case it cannot be passed
    // the address of a local object on the stack. Instead, you must allocate
    // the object on the heap.
    DWORD
    Add(
        ConfigArg_t *pConfigArg,
        bool         fDeleteOnClose = true);

    // Merges two lists:
    // Handles a NULL pointer with an out-of-memory indication.
    DWORD
    Add(
        ConfigArgList_t *pConfidArgList);

    // Opens the specified registry and reads, writes or deletes 
    // the argument values:
    DWORD
    ReadRegistry(
        HKEY         RootKey,  // HKLM, HKCU, etc.
        const TCHAR *pSubKeyName);
    DWORD
    WriteRegistry(
        HKEY         RootKey,  // HKLM, HKCU, etc.
        const TCHAR *pSubKeyName,
        bool         Persistent = true) const; // keep new reg entries across unloads/reboots
    DWORD
    DeleteRegistry(
        HKEY         RootKey,  // HKLM, HKCU, etc.
        const TCHAR *pSubKeyName,
        bool         DeleteSubKeyToo = false) const; // delete sub-key as well as values
    
};

};
};
#endif /* _DEFINED_ConfigArgList_t_ */
// ----------------------------------------------------------------------------
