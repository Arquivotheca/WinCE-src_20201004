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
#if ! defined (__hidconf_H__)
#define __hidconf_H__		1

#define HIDCONF_FLAGS_ACTIVE		0x00000001
#define HIDCONF_FLAGS_AUTH			0x00000002
#define HIDCONF_FLAGS_ENCRYPT		0x00000004

int GetDeviceConfig (BD_ADDR *pbt, DWORD *pdwFlags, unsigned char **ppsdp, unsigned int *pcsdp);

#endif


