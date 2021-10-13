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
#include <devload.h>
#include <winsock.h>
#include <mservice.h>

#include <promgr.hpp>
#include <reflector.h>
#include <devzones.h>

// Enumerates all running services in services.exe ($services name space)
extern "C" BOOL DM_EnumServices(ServicesExeEnumServicesIntrnl *pServiceEnumInfo) {
    UNREFERENCED_PARAMETER(pServiceEnumInfo);
    // We have moved implementation of this functionality to the services enum bus driver
    // instead of this direct PSL call.
    ASSERT(0);
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}


extern "C" BOOL EX_DM_EnumServices(ServicesExeEnumServicesIntrnl *pServiceEnumInfo) {
    return DM_EnumServices(pServiceEnumInfo);
}

