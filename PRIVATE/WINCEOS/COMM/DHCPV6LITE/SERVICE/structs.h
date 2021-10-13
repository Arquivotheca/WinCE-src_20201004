//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++



Module Name:

    structs.h

Abstract:

    This module contains all of the internal structures
    for DHCPv6 Service.



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

