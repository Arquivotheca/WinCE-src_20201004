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

    rand.h

Abstract:

    Random Generator for DHCPv6 client.



    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/

DWORD
DhcpV6GenerateRandom(
    PUCHAR pucBuffer,
    ULONG uLength
    );

//
// Create uniform distribution random number between -0.1 and 0.1
//
DOUBLE
DhcpV6UniformRandom(
    );

