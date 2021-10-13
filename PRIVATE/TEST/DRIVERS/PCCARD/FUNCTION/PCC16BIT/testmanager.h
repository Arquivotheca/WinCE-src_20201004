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
		m_dwTstGroup(dwTestGroup), m_dwTstCase(dwTestCase), m_dwTstThreadNum(dwThreadNum){
		
		NKDMSG(TEXT(" Test group = %u; Test case ID= %u; Threads number = %u"), m_dwTstGroup, m_dwTstCase, m_dwTstThreadNum);

	};
	~TestManager();
	BOOL Init();
	BOOL Exec();

private:

	DWORD	m_dwTstGroup;
	DWORD	m_dwTstCase;
	DWORD   m_dwTstThreadNum;
	pMiniThread*	m_pThreadsArrs[TEST_MAX_CARDS];
};

#endif

