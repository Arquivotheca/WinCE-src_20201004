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
#ifndef __VERSION_H__
#define __VERSION_H__

#include <bldver.h>

#define _CEVERSION_BUILDNUMBER(x) CEVERSION_BUILDNUMBER(x)
#define CEVERSION_BUILDNUMBER(x) #x
#pragma comment(linker, "/VERSION:" _CEVERSION_BUILDNUMBER(CE_BUILD_VER))

#endif // __VERSION_H__
