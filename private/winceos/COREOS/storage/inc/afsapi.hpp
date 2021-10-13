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

EXTERN_C void FS_ProcessCleanupVolumes (HANDLE hProcess);
EXTERN_C int FSINT_RegisterAFSName (const WCHAR* pName);
EXTERN_C int FSEXT_RegisterAFSName (const WCHAR* pName);

EXTERN_C BOOL FSINT_RegisterAFSEx (int Index, HANDLE hAPISet, DWORD Context, 
    DWORD Version, DWORD Flags);
EXTERN_C BOOL FSEXT_RegisterAFSEx (int Index, HANDLE hAPISet, DWORD Context, 
    DWORD Version, DWORD Flags);

EXTERN_C BOOL FSINT_DeregisterAFS (int Index);
EXTERN_C BOOL FSEXT_DeregisterAFS (int Index);

EXTERN_C BOOL FSINT_DeregisterAFSName (int Index);
EXTERN_C BOOL FSEXT_DeregisterAFSName (int Index);

