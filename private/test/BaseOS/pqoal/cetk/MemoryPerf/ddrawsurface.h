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

// ***************************************************************************************************************
//
// Create and set up DDraw surfaces for testing their performance.
//
// ***************************************************************************************************************

#pragma once
#include <ddraw.h>

typedef HRESULT (WINAPI *PFN_DirectDrawCreate)( GUID FAR* lpGUID, LPDIRECTDRAW FAR* lplpDD, IUnknown FAR* pUnkOuter );

#ifdef BUILD_WITH_DDRAW

bool bDDSurfaceInitApp(int nCmdShow);
void DDSurfaceClose();
enum { eLockReadWrite, eLockReadOnly, eLockWriteOnly, eLockWriteDiscard };  // DDrawLockStyle
bool bDDSurfaceLock( const int eLockStyle, DWORD** ppdwDDSRead, DWORD* pdwDDSReadSize );
void DDSurfaceUnLock(void);

#endif // BUILD_WITH_DDRAW
