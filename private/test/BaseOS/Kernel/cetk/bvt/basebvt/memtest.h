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

#define             SIGNATURE_BYTE     0x54

BOOL BaseVirtualAllocateMemory(DWORD dwSize, LPBYTE *lpMem);
BOOL BaseVirtualUseMemory(DWORD dwSize, LPBYTE lpMem);
BOOL BaseVirtualFreeMemory(LPBYTE lpMem);
BOOL DoWriteReadMem(LPBYTE lpMem,DWORD dwSize,BYTE SignatureByte);
BOOL BaseLocalAllocateMemory( DWORD dwSize, LPBYTE *lpMem);
BOOL BaseLocalUseMemory(DWORD dwSize, LPBYTE lpMem);
BOOL BaseLocalFreeMemory(LPBYTE lpMem);
