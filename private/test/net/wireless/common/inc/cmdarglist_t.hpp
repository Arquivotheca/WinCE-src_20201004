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
// Definitions and declarations for the CmdArgList_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_CmdArgList_t_
#define _DEFINED_CmdArgList_t_
#pragma once


#include <CmdArg_t.hpp>
#include <MemBuffer_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Represents a list of CmdArg objects.
//
class CmdArgList_t
{
private:

    // Argument list:
    MemBuffer_t m_ArgList;
    MemBuffer_t m_ArgFlags;
    int         m_ArgListSize;

    // Argument flags:
    static const DWORD sm_fDeleteOnClose = 0x0001; // Delete object when finished

    // List of sub-list names:
    MemBuffer_t m_ListNames;
    MemBuffer_t m_NameOffsets;
    DWORD       m_LastNameOffset;

    // Copy and assignment is deliberately disabled:
    CmdArgList_t(const CmdArgList_t &rhs);
    CmdArgList_t &operator = (const CmdArgList_t &rhs);

public:

    // Overloaded constructor
    CmdArgList_t(const TCHAR *pListName = NULL);  
   ~CmdArgList_t(void);

 // Accessors:

    int Length(void) const { return m_ArgListSize; }
          CmdArg_t &operator[](int Index);
    const CmdArg_t &operator[](int Index) const;

    // Defines a name for the current sub-section of the list:
    // This is used during PrintUsage to display a header at the beginning
    // of each potion of the list.
    DWORD
    SetSubListName(
        const TCHAR *pSubListName);
    
    // Adds the specified object to the list:
    // Handles a NULL pointer with an out-of-memory indication.
    // Note that if fDeleteOnClose is true the object will be deleted by
    // this object when it cleans up. Hence, in that case it cannot be passed
    // the address of a local object on the stack. Instead, you must allocate
    // the object on the heap.
    DWORD
    Add(
        CmdArg_t *pCmdArg,
        bool      fDeleteOnClose = true);

    // Merges two lists:
    // Handles a NULL pointer with an out-of-memory indication.
    DWORD
    Add(
        CmdArgList_t *pCmdArgList);

    // Displays the command-argument instructions:
    // Logs the list-name(s) before each sub-list of objects.
    void
    PrintUsage(
        void (*LogFunc)(const TCHAR *,...)) const;

    // Parses the specified Node based configuraton tree:
    DWORD
    ParseCommandLine(
        int    argc,
        TCHAR *argv[]);

    // Opens the specified registry and reads, writes or deletes 
    // the argument values:
    DWORD
    ReadRegistry(
        HKEY         RootKey,      // HKLM, HKCU, etc.
        const TCHAR *pSubKeyName);
    DWORD
    WriteRegistry(
        HKEY         RootKey,      // HKLM, HKCU, etc.
        const TCHAR *pSubKeyName,
        bool         Persistent = true) const; // keep new reg entries across unloads/reboots
    DWORD
    DeleteRegistry(
        HKEY         RootKey,      // HKLM, HKCU, etc.
        const TCHAR *pSubKeyName,
        bool         DeleteSubKeyToo = false) const; // delete sub-key as well as values

};

};
};
#endif /* _DEFINED_CmdArgList_t_ */
// ----------------------------------------------------------------------------
