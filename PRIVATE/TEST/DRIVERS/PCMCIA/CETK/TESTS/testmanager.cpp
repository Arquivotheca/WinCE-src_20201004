//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#include "TestMain.h"
#include "common.h"
#include "minithread.h"
#include "configtest.h"
#include "excltest.h"
#include "IntrTest.h"
#include "MaskTest.h"
#include "TplTest.h"
#include "WinTest.h"
#include "TestManager.h"


BOOL 
TestManager::Init(VOID){
	DWORD 		i = 0;
	ConfigTest* 	ConfigTC = NULL;
	ConfigTest* 	ConfigTC1 = NULL;
	IntrTest*     	IntrTC = NULL;
	IntrTest*	IntrTC1 = NULL;
	ExclTest*     	ExclTC = NULL;
	ExclTest*	ExclTC1 = NULL;
	MaskTest*     MaskTC = NULL;
	MaskTest*	MaskTC1 = NULL;
	TplTest*     	TplTC = NULL;
	TplTest*		TplTC1 = NULL;
	WinTest* 	WinTC;
	WinTest* 	WinTC1;
	
	ppTstThread = NULL;
	ppTstThread1 = NULL;

	ppTstThread = new pMiniThread[dwTstThreadNum];
	if(uSocket == 2)//two cards in two sockets
		ppTstThread1 = new pMiniThread[dwTstThreadNum];
		
	for(i = 0; i < dwTstThreadNum; i ++){
		ppTstThread[i] = NULL;
		if(ppTstThread1 != NULL)
			ppTstThread1[i] = NULL;
	}

	for(i = 0; i < dwTstThreadNum; i ++){
		switch(dwTstGroup){
			case TSTGROUP_CONFIG:	//configuration tests
				if(uSocket == 2) //two cards
					ConfigTC = new ConfigTest(dwTstCase, i, 0, 0);
				else 
					ConfigTC = new ConfigTest(dwTstCase, i, uSocket, uFunction);
				if(FALSE == ConfigTC->Init())//error happened, return immediately
					return FALSE;
				ppTstThread[i] = ConfigTC;

				if(uSocket == 2){//two cards
					ConfigTC1 = new ConfigTest(dwTstCase, i, 1, 0);
					if(FALSE == ConfigTC1->Init())//error happened, return immediately
						return FALSE;
					ppTstThread1[i] = ConfigTC1;
				}
				break;

			case TSTGROUP_EXECLUSIVE:	//exclusive access tests
				if(uSocket == 2) //two cards
					ExclTC = new ExclTest(dwTstCase, i, 0, 0);
				else 
					ExclTC = new ExclTest(dwTstCase, i, uSocket, uFunction);
				if(FALSE == ExclTC->Init())//error happened, return immediately
					return FALSE;
				ppTstThread[i] = ExclTC;

				if(uSocket == 2){//two cards
					ExclTC1 = new ExclTest(dwTstCase, i, 1, 0);
					if(FALSE == ExclTC1->Init())//error happened, return immediately
						return FALSE;
					ppTstThread1[i] = ExclTC1;
				}
				break;

			case TSTGROUP_INTERRUPT:	//exclusive access tests
				if(uSocket == 2) //two cards
					IntrTC = new IntrTest(dwTstCase, i, 0, 0);
				else 
					IntrTC = new IntrTest(dwTstCase, i, uSocket, uFunction);
				if(FALSE == IntrTC->Init())//error happened, return immediately
					return FALSE;
				ppTstThread[i] = IntrTC;

				if(uSocket == 2){//two cards
					IntrTC1 = new IntrTest(dwTstCase, i, 1, 0);
					if(FALSE == IntrTC1->Init())//error happened, return immediately
						return FALSE;
					ppTstThread1[i] = IntrTC1;
				}
				break;

			case TSTGROUP_MASK:	//exclusive access tests
				if(uSocket == 2) //two cards
					MaskTC = new MaskTest(dwTstCase, i, 0, 0);
				else 
					MaskTC = new MaskTest(dwTstCase, i, uSocket, uFunction);
				if(FALSE == MaskTC->Init())//error happened, return immediately
					return FALSE;
				ppTstThread[i] = MaskTC;

				if(uSocket == 2){//two cards
					MaskTC1 = new MaskTest(dwTstCase, i, 1, 0);
					if(FALSE == MaskTC1->Init())//error happened, return immediately
						return FALSE;
					ppTstThread1[i] = MaskTC1;
				}
				break;

			case TSTGROUP_TUPLE:	//exclusive access tests
				if(uSocket == 2) //two cards
					TplTC = new TplTest(dwTstCase, i, 0, 0);
				else 
					TplTC = new TplTest(dwTstCase, i, uSocket, uFunction);
				if(FALSE == TplTC->Init())//error happened, return immediately
					return FALSE;
				ppTstThread[i] = TplTC;

				if(uSocket == 2){//two cards
					TplTC1 = new TplTest(dwTstCase, i, 1, 0);
					if(FALSE == TplTC1->Init())//error happened, return immediately
						return FALSE;
					ppTstThread1[i] = TplTC1;
				}
				break;
			case TSTGROUP_WINDOW:	//configuration tests
				if(uSocket == 2) //two cards
					WinTC = new WinTest(dwTstCase, i, 0, 0);
				else 
					WinTC = new WinTest(dwTstCase, i, uSocket, uFunction);
				if(FALSE == WinTC->Init())//error happened, return immediately
					return FALSE;
				ppTstThread[i] = WinTC;

				if(uSocket == 2){//two cards
					WinTC1 = new WinTest(dwTstCase, i, 1, 0);
					if(FALSE == WinTC1->Init())//error happened, return immediately
						return FALSE;
					ppTstThread1[i] = WinTC1;
				}
				break;
		}
	}

	return TRUE;
}

BOOL
TestManager::Exec(VOID){
	BOOL	bRet = TRUE;
	BOOL	bRet1 = TRUE;
	DWORD		i = 0;

	//start each test thread
	for(i = 0; i < dwTstThreadNum; i++){
		ppTstThread[i]->ThreadStart();
		if(ppTstThread1 != NULL)//the second card
			ppTstThread1[i]->ThreadStart();
			
	}
	//wait them to be finished
	for(i = 0; i < dwTstThreadNum; i++){
		if (FALSE == ppTstThread[i]->WaitThreadComplete(600*1000)) {//wait for 10 minutes
			g_pKato->Log(LOG_FAIL, TEXT("Test thread %u for socket %u Time Out"), i, (uSocket == 2)?0:uSocket);
			bRet=FALSE;
		}

		if(ppTstThread1 != NULL){
			if (FALSE == ppTstThread1[i]->WaitThreadComplete(600*1000)) {//wait for 10 minutes
				g_pKato->Log(LOG_FAIL, TEXT("Test thread %u for socket 1 Time Out"), i);
				bRet=FALSE;
			}
		}
			
	}

	if(TRUE == bRet){//if no time out error, then retrieve test results now
		for(i = 0; i < dwTstThreadNum; i++){
			bRet1 = ppTstThread[i]->GetResult();
			if(FALSE == bRet1){
				g_pKato->Log(LOG_FAIL, TEXT("Test thread %u for socket %u FAILED"), i, (uSocket == 2)?0:uSocket);
				bRet = FALSE;
			}	
			else 
				g_pKato->Log(LOG_DETAIL, TEXT("Test thread %u for socket %u SUCCEED"), i, (uSocket == 2)?0:uSocket);

			if(ppTstThread1 != NULL){
				bRet1 = ppTstThread1[i]->GetResult();
				if(FALSE == bRet1){
					g_pKato->Log(LOG_FAIL, TEXT("Test thread %u for socket 1 FAILED"), i);
					bRet = FALSE;
				}	
				else 
					g_pKato->Log(LOG_DETAIL, TEXT("Test thread %u for socket 1 SUCCEED"), i);
			}
		}
	}

	return bRet;
}

TestManager::~TestManager(VOID){

	//destroy test thread objects
	for(DWORD i = 0; i < dwTstThreadNum; i ++){
		delete ppTstThread[i];
		if(ppTstThread1 != NULL)
			delete ppTstThread1[i];
	}
	
	delete []ppTstThread;
	if(ppTstThread1 != NULL)
		delete []ppTstThread1;
}
