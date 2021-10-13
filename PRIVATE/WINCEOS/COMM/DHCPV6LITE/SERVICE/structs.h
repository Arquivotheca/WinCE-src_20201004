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
/*++



Module Name:

    structs.h

Abstract:

    This module contains all of the internal structures
    for DHCPv6 Service.

Author:

    FrancisD

Environment

    User Level: Windows

Revision History:


--*/


#ifdef __cplusplus
extern "C" {
#endif


typedef struct _INI_INTERFACE_HANDLE {
    struct _INI_INTERFACE_HANDLE * pNext;
    DWORD dwInterfaceID;
} INI_INTERFACE_HANDLE, * PINI_INTERFACE_HANDLE;


#ifdef __cplusplus
}
#endif

