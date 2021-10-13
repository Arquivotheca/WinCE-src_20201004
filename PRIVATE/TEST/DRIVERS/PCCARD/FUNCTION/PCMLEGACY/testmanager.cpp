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
#include "MaskTest.h"
#include "TplTest.h"
#include "WinTest.h"
#include "TestManager.h"


BOOL 
TestManager::Init(VOID){
	DWORD 		i, dwIndex;
	ConfigTest* 	ConfigTC = NULL;
	IntrTest*     	IntrTC = NULL;
	MaskTest*     MaskTC = NULL;
	TplTest*     	TplTC = NULL;
	WinTest* 	WinTC;
	pMiniThread* ppTstThread;

      bInitSucceed = TRUE;
	//create threads pool
	if(pThreadsArrs == NULL){
		g_pKato->Log(LOG_FAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
             bInitSucceed = FALSE;
		return FALSE;
	}

	for(dwIndex = 0; dwIndex < g_dwTotalCards; dwIndex++){

		ppTstThread = new pMiniThread[dwTstThreadNum];
		if(ppTstThread == NULL){
			g_pKato->Log(LOG_FAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
                    bInitSucceed = FALSE;
			return FALSE;
		}
		memset(ppTstThread, 0, sizeof(pMiniThread)*dwTstThreadNum);
		pThreadsArrs[dwIndex] = ppTstThread;

	}
	
	for(dwIndex = 0; dwIndex < g_dwTotalCards; dwIndex++){
		ppTstThread = (pMiniThread *)(pThreadsArrs[dwIndex]);
		UINT8	uSocket = g_CardDescs[dwIndex].hCardHandle.uSocket;
		UINT8	uFunction = g_CardDescs[dwIndex].hCardHandle.uFunction;
		for(i = 0; i < dwTstThreadNum; i ++){
		      ppTstThread[i] = NULL;
			switch(dwTstGroup){
				case TSTGROUP_CONFIG:	//configuration tests
					ConfigTC = new ConfigTest(dwTstCase, i, uSocket, uFunction);
					ppTstThread[i] = ConfigTC;
					if(ConfigTC == NULL){
						g_pKato->Log(LOG_FAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
    						return FALSE;
					}
					if(FALSE == ConfigTC->Init()){//error happened, return immediately
                                      bInitSucceed = FALSE;
						return FALSE;
				       }
					break;

				case TSTGROUP_INTERRUPT:	//exclusive access tests
					IntrTC = new IntrTest(dwTstCase, i, uSocket, uFunction);
					ppTstThread[i] = IntrTC;
					if(IntrTC == NULL){
						g_pKato->Log(LOG_DETAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
                                         bInitSucceed = FALSE;
						return FALSE;
					}
					if(FALSE == IntrTC->Init()){//error happened, return immediately
                                         bInitSucceed = FALSE;
						return FALSE;
					}
					break;

				case TSTGROUP_MASK:	//exclusive access tests
					MaskTC = new MaskTest(dwTstCase, i, uSocket, uFunction);
					ppTstThread[i] = MaskTC;
					if(MaskTC == NULL){
						g_pKato->Log(LOG_DETAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
                                         bInitSucceed = FALSE;
						return FALSE;
					}
					if(FALSE == MaskTC->Init()){//error happened, return immediately
                                         bInitSucceed = FALSE;
						return FALSE;
					}
					break;

				case TSTGROUP_TUPLE:	//exclusive access tests
					TplTC = new TplTest(dwTstCase, i, uSocket, uFunction);
					ppTstThread[i] = TplTC;
					if(TplTC == NULL){
						g_pKato->Log(LOG_DETAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
						return FALSE;
					}
					if(FALSE == TplTC->Init()){//error happened, return immediately
                                         bInitSucceed = FALSE;
						return FALSE;
					}
					break;

				case TSTGROUP_WINDOW:	//configuration tests
					WinTC = new WinTest(dwTstCase, i, uSocket, uFunction);
					ppTstThread[i] = WinTC;
					if(WinTC == NULL){
						g_pKato->Log(LOG_DETAIL,TEXT("-->TestManager::Init<--   FAIL -- out of memory\r\n")); 
                                         bInitSucceed = FALSE;
						return FALSE;
					}
					if(FALSE == WinTC->Init()){//error happened, return immediately
                                         bInitSucceed = FALSE;
						return FALSE;
					}
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
		ppTstThread = (pMiniThread *)(pThreadsArrs[dwIndex]);
		for(i = 0; i < dwTstThreadNum; i++){
		    if(ppTstThread[i] == NULL)
		        continue;
		       if(bInitSucceed == FALSE)//let the main thread to exit immeidately
		           ppTstThread[i]->ThreadTerminated(1);
			ppTstThread[i]->ThreadStart();
		}
	}

	//wait them to be finished
	for(dwIndex = 0; dwIndex < g_dwTotalCards; dwIndex++){
		ppTstThread = (pMiniThread *)(pThreadsArrs[dwIndex]);
		for(i = 0; i < dwTstThreadNum; i++){
		    if(ppTstThread[i] == NULL)
		        continue;
			if (FALSE == ppTstThread[i]->WaitThreadComplete(600*1000)) {//wait for 10 minutes
				g_pKato->Log(LOG_FAIL, TEXT("Test thread %u for card %u Time Out"), i, g_CardDescs[dwIndex].hCardHandle.uSocket);
				bRet=FALSE;
			}
		}
	}

	if(TRUE == bRet){//if no time out error, then retrieve test results now
		for(dwIndex = 0; dwIndex < g_dwTotalCards; dwIndex++){
			ppTstThread = (pMiniThread *)(pThreadsArrs[dwIndex]);
			for(i = 0; i < dwTstThreadNum; i++){
        		    if(ppTstThread[i] == NULL)
        		        continue;
				if(FALSE == ppTstThread[i]->GetResult()){
					g_pKato->Log(LOG_FAIL, TEXT("Test thread %u for socket %u FAILED"), i, g_CardDescs[dwIndex].hCardHandle.uSocket);
					bRet = FALSE;
				}	
				else 
					g_pKato->Log(LOG_DETAIL, TEXT("Test thread %u for socket %u SUCCEED"), i, g_CardDescs[dwIndex].hCardHandle.uSocket);
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
		ppTstThread = (pMiniThread *)(pThreadsArrs[dwIndex]);
		for(i = 0; i < dwTstThreadNum; i++){
	           pMiniThread pTemp = ppTstThread[i];
                 if(ppTstThread[i] != NULL)
                    delete ppTstThread[i];
		}
		delete[] ppTstThread;
	}

}
