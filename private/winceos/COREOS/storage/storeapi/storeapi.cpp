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
#include <windows.h>
#include <storemgr.h>

BOOL IsStorageManagerRunning()
{
    if ((GetFileAttributesW( L"\\StoreMgr") != -1) &&
        (GetFileAttributesW( L"\\StoreMgr") & FILE_ATTRIBUTE_TEMPORARY)) {
        return TRUE;
    } 
    return FALSE;
}

// NOTENOTE: The real store API has moved to thunks in coredll.

