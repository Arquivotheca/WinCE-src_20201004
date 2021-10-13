//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef _MP_DRVSTK_H
#define _MP_DRVSTK_H

typedef uint VMPVersion;
#define NMPCL51m	0
#define NMPCL60		1
#define VNWIFI		2

#include <ndis.h>

NDIS_STATUS GetDriverStack(VMPVersion version, LPTSTR szAdapterName, PVOID *ppDriverStack, PULONG pulDriverStackDataLength);
BOOL BuildDriverStack(LPTSTR szFileName, PNDIS_ENUM_FILTERS *ppExpectedDriverStack);
BOOL CompareDriverStacks(PNDIS_ENUM_FILTERS pGeneratedDriverStack, PNDIS_ENUM_FILTERS pExpectedDriverStack);
#endif