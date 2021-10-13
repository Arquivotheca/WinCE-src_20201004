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
#ifndef _VIDCAPTESTS_H
#define _VIDCAPTESTS_H

#include <tux.h>

enum VidCapTestID
{
    IDQueryInterface = 100,
    IDQueryPinInterface,
    IDDriverLoad,
    IDGetCLSID,
    IDPinFlowingTests,
    IDPinFlowingGraphSTateTests,
    IDStillPinTriggerTests,
    IDVCapMetadataTests
};

TESTPROCAPI VidCapQITest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI VidCapPinQITest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI VidCapDriverLoadTests(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI GetCLSIDTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI PinFlowingTests(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI StillPinTriggerTests(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI PinFlowingGraphStateTests(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI VCapMetadataTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

#endif

