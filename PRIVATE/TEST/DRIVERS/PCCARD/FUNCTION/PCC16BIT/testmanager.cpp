//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
/*++
Module Name:  
	TestManager.cpp

Abstract:

    Implementation of test manager class which does the test case setup/dispatch/cleanup job 

--*/

#include "TestMain.h"
#include "common.h"
#include "minithread.h"
#include "configtest.h"
#include "IntrTest.h"
#include "TplTest.h"
#include "WinTest.h"
#include "TestManager.h"


BOOL 
TestManager::Init(VOID){
	DWORD 		i, dwIndex;
	ConfigTest* 	ConfigTC = NULL;
	IntrTest*     	IntrTC = NULL;
	TplTest*     	TplTC = NULL;
	WinTest* 	WinTC;
	pMiniThread* ppTstThread;

	//create threads pool
	if(m_pThreadsArrs == NULL){
		g_pKato->Log(LOG_FAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
		return FALSE;
	}

	for(dwIndex = 0; dwIndex < g_dwTotalCards; dwIndex++){

		ppTstThread = new pMiniThread[m_dwTstThreadNum];
		if(ppTstThread == NULL){
			g_pKato->Log(LOG_FAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
			return FALSE;
		}
		memset(ppTstThread, 0, m_dwTstThreadNum*sizeof(pMiniThread));
		m_pThreadsArrs[dwIndex] = ppTstThread;

	}
	
	for(dwIndex = 0; dwIndex < g_dwTotalCards; dwIndex++){
		ppTstThread = (pMiniThread *)(m_pThreadsArrs[dwIndex]);
		UINT8	uSocket = g_CardDescs[dwIndex].hCardHandle.uSocket;
		UINT8	uFunction = g_CardDescs[dwIndex].hCardHandle.uFunction;
		for(i = 0; i < m_dwTstThreadNum; i ++){
		      ppTstThread[i] = NULL;
			switch(m_dwTstGroup){
				case TSTGROUP_CONFIG:	//configuration tests
					ConfigTC = new ConfigTest(m_dwTstCase, i, uSocket, uFunction);
					ppTstThread[i] = ConfigTC;
					if(ConfigTC == NULL){
						g_pKato->Log(LOG_FAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
						return FALSE;
					}
					if(FALSE == ConfigTC->Init())//error happened, return immediately
						return FALSE;
					break;

				case TSTGROUP_INTERRUPT:	//exclusive access tests
					IntrTC = new IntrTest(m_dwTstCase, i, uSocket, uFunction);
					ppTstThread[i] = IntrTC;
					if(IntrTC == NULL){
						g_pKato->Log(LOG_FAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
						return FALSE;
					}
					if(FALSE == IntrTC->Init())//error happened, return immediately
						return FALSE;
					break;

				case TSTGROUP_TUPLE:	//exclusive access tests
					TplTC = new TplTest(m_dwTstCase, i, uSocket, uFunction);
					ppTstThread[i] = TplTC;
					if(TplTC == NULL){
						g_pKato->Log(LOG_FAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
						return FALSE;
					}
					if(FALSE == TplTC->Init())//error happened, return immediately
						return FALSE;
					break;

				case TSTGROUP_WINDOW:	//configuration tests
					WinTC = new WinTest(m_dwTstCase, i, uSocket, uFunction);
					ppTstThread[i] = WinTC;
					if(WinTC == NULL){
						g_pKato->Log(LOG_FAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
						return FALSE;
					}
					if(FALSE == WinTC->Init())//error happened, return immediately
						return FALSE;
					break;
			}
		}
	}
	return TRUE;
}

BOOL
TestManager::Exec(VOID){
	BOOL	bRet = TRUE;
	DWORD	i, dwIndex;
	pMiniThread* ppTstThread;

	//start each test thread
	for(dwIndex = 0; dwIndex < g_dwTotalCards; dwIndex++){
		ppTstThread = (pMiniThread *)(m_pThreadsArrs[dwIndex]);
		for(i = 0; i < m_dwTstThreadNum; i++){
		    if(ppTstThread[i] == NULL)
		        continue;
			ppTstThread[i]->ThreadStart();
		}
	}

	//wait them to be finished
	for(dwIndex = 0; dwIndex < g_dwTotalCards; dwIndex++){
		ppTstThread = (pMiniThread *)(m_pThreadsArrs[dwIndex]);
		for(i = 0; i < m_dwTstThreadNum; i++){
		    if(ppTstThread[i] == NULL)
		        continue;
			if (FALSE == ppTstThread[i]->WaitThreadComplete(600*1000)) {//wait for 10 minutes
				g_pKato->Log(LOG_FAIL, TEXT("Thread %u : Socket %u : Function %u -- Test thread time Out"), 
				                                     i, g_CardDescs[dwIndex].hCardHandle.uSocket, g_CardDescs[dwIndex].hCardHandle.uFunction);
				bRet=FALSE;
			}
		}
	}

	if(TRUE == bRet){//if no time out error, then retrieve test results now
		for(dwIndex = 0; dwIndex < g_dwTotalCards; dwIndex++){
			ppTstThread = (pMiniThread *)(m_pThreadsArrs[dwIndex]);
			for(i = 0; i < m_dwTstThreadNum; i++){
        		    if(ppTstThread[i] == NULL)
        		        continue;
				if(FALSE == ppTstThread[i]->GetResult()){
					g_pKato->Log(LOG_FAIL, TEXT("Thread %u : Socket %u : Function %u -- Test FAILED"),
				                                     i, g_CardDescs[dwIndex].hCardHandle.uSocket, g_CardDescs[dwIndex].hCardHandle.uFunction);
					bRet = FALSE;
				}	
				else 
					g_pKato->Log(LOG_DETAIL, TEXT("Thread %u : Socket %u : Function %u -- Test SUCCEED"), 
				                                     i, g_CardDescs[dwIndex].hCardHandle.uSocket, g_CardDescs[dwIndex].hCardHandle.uFunction);
			}
		}
	}

	return bRet;
}

TestManager::~TestManager(VOID){
	DWORD	i, dwIndex;
	pMiniThread* ppTstThread;

	//destroy test thread objects
	for(dwIndex = 0; dwIndex < g_dwTotalCards; dwIndex++){
		ppTstThread = (pMiniThread *)(m_pThreadsArrs[dwIndex]);
		for(i = 0; i < m_dwTstThreadNum; i++){
		    pMiniThread pTemp = ppTstThread[i];
		    if(pTemp != NULL)
			delete pTemp;
		}
		delete[] ppTstThread;
	}

}
