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
#include <upnpdevapi.h>
#include <service.h>
#include <upnpdcall.h>
#include "string.hxx"

#define celems(_x)          (sizeof(_x) / sizeof(_x[0]))

#ifdef __cplusplus
extern "C" {
#endif

static UPNPApi *g_pUPNPApi = NULL;

void SetUPNPApi(UPNPApi *pApi)
{
    g_pUPNPApi = pApi;
}

UPNPApi *GetUPNPApi()
{
    return g_pUPNPApi;
}

#ifdef __cplusplus
}
#endif

