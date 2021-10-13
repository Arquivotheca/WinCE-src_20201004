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

    ping.h

Abstract:

  Falcon private ping


--*/

#if ! defined (__ping_HXX__)
#define __ping_HXX__	1


BOOL ping(unsigned long ip);
BOOL StartPingServer();
void StopPingServer();

#endif /* __ping_HXX__ */


