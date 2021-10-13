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
TEST_FUNCTION(TestSusRes);
TEST_FUNCTION(TestStressSusRes);
TEST_FUNCTION(TestResetOnResume);
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
