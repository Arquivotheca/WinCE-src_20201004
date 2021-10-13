//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#ifndef _MASKTEST_H_
#define _MASKTEST_H_

#include "minithread.h"


class MaskTest : public MiniThread {
public:
	MaskTest(DWORD dwTestCase, DWORD dwThreadNo, UINT8	uSock, UINT8 uFunc) :
			MiniThread (0,THREAD_PRIORITY_NORMAL,TRUE),
			m_dwCaseID(dwTestCase),
			m_dwThreadID(dwThreadNo),
			m_uLocalSock(uSock),
			m_uLocalFunc(uFunc){
		NKDMSG(TEXT("ConfigTest: CaseID: %u; Thread No. %u; Socket %u; Function %u"), 
					m_dwCaseID, m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(TRUE);
	};

	BOOL Init();
	~MaskTest(){;};

private:
	virtual DWORD ThreadRun();
	VOID CardEventMasksTest ();
	
	DWORD	m_dwCaseID;
	DWORD 	m_dwThreadID;
	UINT8	m_uLocalSock, m_uLocalFunc;

};

#endif

