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
/*++


Module Name:

    scping.hxx

Abstract:

  Falcon private ping


--*/

#if ! defined (__ping_HXX__)
#define __ping_HXX__	1


BOOL ping(unsigned long ip);
BOOL StartPingServer();
void StopPingServer();

#endif /* __ping_HXX__ */


