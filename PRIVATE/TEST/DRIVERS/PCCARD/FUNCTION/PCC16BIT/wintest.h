//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name:  
    wintest.h

Abstract:

    Definition of window-related test class and other needed functions/data structures

--*/

#ifndef _WINTEST_H_
#define _WINTEST_H_

#include "minithread.h"

class WinTest : public MiniThread {
public:
	WinTest(DWORD dwTestCase, DWORD dwThreadNo, UINT8 uSock, UINT8 uFunc) :
			MiniThread (0,THREAD_PRIORITY_NORMAL,TRUE),
			m_dwCaseID(dwTestCase),
			m_dwThreadID(dwThreadNo),
			m_uLocalSock(uSock),
			m_uLocalFunc(uFunc){
		NKDMSG(TEXT("WinTest: CaseID: %u; Thread No. %u; Socket %u; Function %u"), 
					m_dwCaseID, m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(TRUE);
	};
      static UINT32  m_uIOBaseAddr;
	BOOL Init();
	~WinTest(){DeleteCriticalSection(&m_cs);};
private:
	virtual DWORD ThreadRun();
	VOID Test_CardRequestandMapWindow ();
	VOID Test_InvalidParas ();
	
	DWORD	m_dwCaseID;
	DWORD 	m_dwThreadID;
	UINT8	m_uLocalSock, m_uLocalFunc;
      CRITICAL_SECTION  m_cs;

};


#endif

