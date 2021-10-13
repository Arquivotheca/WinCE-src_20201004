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

