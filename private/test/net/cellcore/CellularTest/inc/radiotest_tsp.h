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

#ifndef _radiotest_tsp_h_
#define _radiotest_tsp_h_

#include <tapi.h>
#include "radiotest.h"

// Initialize threads etc
bool TSP_Init(VOID);
bool TSP_DeInit(VOID);

// Utils
HLINE TSP_GetLineHandle(VOID);
BOOL TSP_DialIncoming(VOID);

// Tests
BOOL TSP_OutNormal(DWORD dwUserData);
BOOL TSP_OutBusy(DWORD dwUserData);
BOOL TSP_OutNoAnswer(DWORD dwUserData);
BOOL TSP_OutReject(DWORD dwUserData);
BOOL TSP_InNormal(DWORD dwUserData);
BOOL TSP_InNoAnswer(DWORD dwUserData);
BOOL TSP_InReject(DWORD dwUserData);
BOOL TSP_LongCall(DWORD dwUserData);
BOOL TSP_HoldUnhold(DWORD dwUserData);
BOOL TSP_Conference(DWORD dwUserData);
BOOL TSP_HoldConference(DWORD dwUserData);
BOOL TSP_2WayCall(DWORD dwUserData);
BOOL TSP_Flash(DWORD dwUserData);

BOOL StartUser2(DWORD dwUserData);

BOOL TSP_SetEquipmentState(DWORD dwUserData);

// Attempts to extract own number using TSP
BOOL TSP_RecoverOwnNumber(LPTSTR ptszOwnNumber, LPTSTR ptszIMSI, LPTSTR ptszSerialNumber);
BOOL TSP_RetrieveRadioInfo(LPTSTR ptszManufacturer, LPTSTR ptszModel, LPTSTR ptszRevision);

#endif
