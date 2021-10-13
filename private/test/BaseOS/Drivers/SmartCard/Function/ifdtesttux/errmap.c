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
//+-------------------------------------------------------------------------
//
//  File:       errmap.c
//
//--------------------------------------------------------------------------

typedef long LONG;
typedef LONG NTSTATUS;
#include <ntstatus.h>
#include <winerror.h>

//#define WIN32_NO_STATUS
#include "errmap.h"       

LONG                               
MapWinErrorToNtStatus(
    LONG in_uErrorCode
    )
{
    LONG i;

    for (i = 0; i < sizeof(CodePairs) / sizeof(CodePairs[0]); i += 2) {

        if (CodePairs[i + 1] == in_uErrorCode) {

            return CodePairs[i];
             
        }
    }

    return -1;
}

