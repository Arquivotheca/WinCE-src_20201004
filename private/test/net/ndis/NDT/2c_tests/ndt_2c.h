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
#ifndef __TEST_2C_H
#define __TEST_2C_H

//----------------------------------------------------------------------------------------------------------------------

BOOL PrepareToRun();
BOOL FinishRun();

//----------------------------------------------------------------------------------------------------------------------

TEST_FUNCTION(TestSendPackets);
TEST_FUNCTION(TestReceivePackets);
TEST_FUNCTION(TestFilterReceive);
TEST_FUNCTION(TestMulticastReceive);
TEST_FUNCTION(TestStressSend);
TEST_FUNCTION(TestStressReceive);

//----------------------------------------------------------------------------------------------------------------------

extern TCHAR g_szTestAdapter[256];
extern TCHAR g_szHelpAdapter[256];
extern NDIS_MEDIUM g_ndisMedium;
extern DWORD g_dwStressDelay;
extern BOOL g_bNoStress;

//----------------------------------------------------------------------------------------------------------------------

#endif
