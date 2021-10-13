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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
#include <sdcommon.h>
#include <sd_tst_cmn.h>
#include <sddrv.h>

void SDDummyTst1(PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO /*pClientInfo*/)
{
    LogDebug(TEXT("SDDummyTst1 is called. %d"),99);
    pTestParams->iResult =TPR_PASS;
}

void SDDummyTst2(PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO /*pClientInfo*/)
{
    LogDebug(TEXT("SDDummyTst2 is called. %s, %f"),TEXT("ninety nine"), 99.9);
    pTestParams->iResult =TPR_FAIL;
}
