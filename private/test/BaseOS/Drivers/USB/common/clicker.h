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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef BOOL (* LPUN_CLICKER_INIT)(TCHAR *);
typedef BOOL (* LPUN_CLICKER_CONNECT)(void);
typedef BOOL (* LPUN_CLICKER_DISCONNECT)(void);
typedef VOID (* LPUN_CLICKER_DEINIT)(void);

#define CLICKER_INIT_FN _T("ClickerInit")
#define CLICKER_CONNECT_FN _T("ClickerConnect")
#define CLICKER_DISCONNECT_FN _T("ClickerDisconnect")
#define CLICKER_DEINIT_FN _T("ClickerDeInit")
#ifdef __cplusplus
}
#endif






