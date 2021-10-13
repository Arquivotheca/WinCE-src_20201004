//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/*++

Module Name:
    lckmgrdbg.hpp

Abstract:
    Lock Manager Debug Definition Set.

Revision History:

--*/

#ifndef __LOCKMGRDBG_HPP_
#define __LOCKMGRDBG_HPP_

#include <windows.h>

#ifndef SHIP_BUILD
    #define STR_MODULE _T("LockMgr!")
    #define SETFNAME(name) LPCTSTR pszFname = STR_MODULE name _T(":")
#else
    #define SETFNAME(name)
#endif // SHIP_BUILD

#endif // __LOCKMGRDBG_HPP_

