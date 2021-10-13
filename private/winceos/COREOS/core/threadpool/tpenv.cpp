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

    tpenv.cpp

Abstract:

    Thread pool environment functions


--*/

#include "cetp.hpp"

WINBASEAPI
VOID
InitializeThreadpoolEnvironment(
    _Out_ PTP_CALLBACK_ENVIRON pcbe
    )
{
    memset (pcbe, 0, sizeof (*pcbe));
}


WINBASEAPI
VOID
SetThreadpoolCallbackPool(
    _Inout_ PTP_CALLBACK_ENVIRON pcbe,
    _In_    PTP_POOL             ptpp
    )
{
    pcbe->Pool = ptpp;
}

WINBASEAPI
VOID
SetThreadpoolCallbackRunsLong(
    _Inout_ PTP_CALLBACK_ENVIRON pcbe
    )
{
}

WINBASEAPI
VOID
SetThreadpoolCallbackLibrary(
    _Inout_ PTP_CALLBACK_ENVIRON pcbe,
    _In_    PVOID                mod
    )
{
    pcbe->RaceDll = mod;
}

WINBASEAPI
VOID
SetThreadpoolCallbackPriority(
    _Inout_ PTP_CALLBACK_ENVIRON pcbe,
    _In_    TP_CALLBACK_PRIORITY Priority
    )
{
}

WINBASEAPI
VOID
SetThreadpoolCallbackPersistent(
    _Inout_ PTP_CALLBACK_ENVIRON pcbe
    )
{
}

WINBASEAPI
VOID
DestroyThreadpoolEnvironment(
    _Inout_ PTP_CALLBACK_ENVIRON pcbe
    )
{
}

