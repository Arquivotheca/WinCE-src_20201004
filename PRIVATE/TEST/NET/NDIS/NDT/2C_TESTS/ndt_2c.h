//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
