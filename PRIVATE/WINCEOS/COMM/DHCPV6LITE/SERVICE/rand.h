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

    rand.h

Abstract:

    Random Generator for DHCPv6 client.

Author:

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

