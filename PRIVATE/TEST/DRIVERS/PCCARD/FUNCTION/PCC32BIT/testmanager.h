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

