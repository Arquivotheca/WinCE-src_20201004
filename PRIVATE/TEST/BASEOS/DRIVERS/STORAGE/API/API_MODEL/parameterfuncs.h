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

#include <windows.h>
#include <diskio.h>
#include <atapi.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include <storemgr.h>
#include <devutils.h>
#include <clparse.h>

extern DISK_INFO         	g_diskInfo;
extern CKato *			g_pKato;
#define MAX_BUF 		524288
#define MIN_BUF			255
#define MAX_SECTORS	2048
#define SECTOR_SIZE		512

BOOL MakeJunkBuffer(TCHAR*, DWORD);
BOOL MakeAlphaBuffer(TCHAR*, DWORD);
BOOL MakeOneBuffer(TCHAR*, DWORD);
BOOL MakeZeroBuffer(TCHAR*, DWORD);
TCHAR* GetBuffer(const TCHAR* value,const TCHAR* BufferType,DWORD & dwBufferSize);
DWORD GetDWORD(const TCHAR* value,DWORD dwBufferSize);
