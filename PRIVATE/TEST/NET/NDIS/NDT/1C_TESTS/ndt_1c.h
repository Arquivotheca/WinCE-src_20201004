//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __TEST_1C_H
#define __TEST_1C_H

//----------------------------------------------------------------------------------------------------------------------

BOOL PrepareToRun();
BOOL FinishRun();

//----------------------------------------------------------------------------------------------------------------------

TEST_FUNCTION(TestWork);

TEST_FUNCTION(TestOpenClose);
TEST_FUNCTION(TestSend);
TEST_FUNCTION(TestLoopbackSend);
TEST_FUNCTION(TestLoopbackStress);
TEST_FUNCTION(TestSetMulticast);
TEST_FUNCTION(TestReset);
TEST_FUNCTION(TestCancelSend);
TEST_FUNCTION(TestFaultHandling);
TEST_FUNCTION(TestOids);
TEST_FUNCTION(Test64BitOids);

//----------------------------------------------------------------------------------------------------------------------

extern TCHAR g_szTestAdapter[256];
extern NDIS_MEDIUM g_ndisMedium;
extern DWORD g_dwStressDelay;
extern BOOL g_bNoUnbind;
extern BOOL g_bFaultHandling;
extern BOOL g_bForceCancel;
extern BOOL g_bNoStress;
extern BOOL g_bFullMulti;

//----------------------------------------------------------------------------------------------------------------------

#endif
