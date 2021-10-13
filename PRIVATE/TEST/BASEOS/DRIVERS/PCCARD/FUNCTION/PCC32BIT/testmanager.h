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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name:  
	TestManager.h

Abstract:

    Definition for TestManager class

--*/

#ifndef _TESTMANAGER_H_
#define _TESTMANAGER_H_
#include "miniThread.h"


typedef  MiniThread*  pMiniThread;

class TestManager  {

public :
	TestManager (DWORD dwTestGroup, DWORD dwTestCase, DWORD dwThreadNum) :
		dwTstGroup(dwTestGroup), dwTstCase(dwTestCase), dwTstThreadNum(dwThreadNum){
		
		NKDMSG(TEXT(" Test group = %u; Test case ID= %u; Threads number = %u"), dwTstGroup, dwTstCase, dwTstThreadNum);

	};
	~TestManager();
	BOOL Init();
	BOOL Exec();

private:

	DWORD	dwTstGroup;
	DWORD	dwTstCase;
	DWORD   dwTstThreadNum;
	pMiniThread*	pThreadsArrs[TEST_MAX_CARDS];
};

#endif

