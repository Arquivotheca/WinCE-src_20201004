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

    init.h

Abstract:

    This module contains all of the code prototypes to initialize the
    variables for the DhcpV6 Service.



    FrancisD

Environment

    User Level: Windows

Revision History:


--*/


#ifdef __cplusplus
extern "C" {
#endif


DWORD
InitDHCPV6ThruRegistry(
    );

DWORD
InitDHCPV6Globals(
    );


#ifdef UNDER_CE
DWORD DhcpV6InitTdiNotifications();
#endif


#ifdef __cplusplus
}
#endif

