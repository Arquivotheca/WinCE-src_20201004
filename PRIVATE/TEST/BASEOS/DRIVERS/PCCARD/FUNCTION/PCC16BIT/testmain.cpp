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
	TestMain.cpp

Abstract:

    implementation for test main entry and setup/cleanup functions

--*/

#include "testmain.h"
#include "resource.h"
#include "common.h"
#include "TestManager.h"

//defines for retry times of get status, which is used to detect whether a card is inserted or not
#define MAX_CONNECT_TRY	50

#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("PCMCIATESTS"), 
    {
    TEXT("Errors"),TEXT("Warnings"),TEXT("Functions"),TEXT("Initialization"),
    TEXT("Verbose"),TEXT("Ioctl"),TEXT("Undefined"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),
    TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined"),TEXT("Undefined")
     },
    DBG_ERROR | DBG_WARNING |DBG_FUNCTION | DBG_INIT|DBG_VERBOSE 
};
#endif


//some defines used only in the following TestDispatchEntry API
#define 	TESTGROUP_DIV		1000
#define	TESTSOCKET_DIV	 	10000
#define	MAX_TESTTHREADS	32

//=================================================================================================================
//
//	   Main entrance for all PCMCIA tests
//
//
//=================================================================================================================

TESTPROCAPI TestDispatchEntry(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	DWORD	dwTestID = lpFTE->dwUniqueID;
	DWORD	dwThreadNum = lpFTE->dwUserData;
	DWORD	dwTestCat, dwTestGroup, dwTestCase;
 	USHORT	uSock = 0xFF;
 	USHORT  uFunc = 0xFF;
 	BOOL	bRet = TRUE;

	if (uMsg != TPM_EXECUTE) {
      		return TPR_NOT_HANDLED;
   	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+TestDispatchEntry\r\n")));

	ASSERT(dwThreadNum > 0 && dwThreadNum <= MAX_TESTTHREADS);
	dwTotalThread = dwThreadNum;

	//-- parse test case id, and retrieve needed test info --
	dwTestCat= dwTestID / TESTSOCKET_DIV;
	if(4 < dwTestCat) {//invalid test slot number
		g_pKato->Log(LOG_FAIL,TEXT("Test Category: %u is invalid! \n"), dwTestCat);
		return TPR_ABORT;
	}
	dwTestGroup = (dwTestID - TESTSOCKET_DIV* dwTestCat) / TESTGROUP_DIV;
	if(0 >= dwTestGroup || 7 <= dwTestGroup) {//invalid test group number
		g_pKato->Log(LOG_FAIL,TEXT("Test group ID: %u is invalid! \n"), dwTestGroup);
		return TPR_ABORT;
	}
	dwTestCase = (dwTestID - TESTSOCKET_DIV* dwTestCat) % TESTGROUP_DIV;


	//-- if we need to switch to another test category --
	if((USHORT)dwTestCat != uTestCategory) {
		uTestCategory = (USHORT)dwTestCat;
		if(FALSE == TestSetup(uTestCategory)){
			g_pKato->Log(LOG_DETAIL,TEXT("Test setup had failed! test will be skipped\n"));
			bCardNotInSlot = TRUE; //can not detect the connection
			return TPR_SKIP;
		}
		else{
			bCardNotInSlot = FALSE;
			
			//enumerate cards now
			DWORD	dwCardsNum = MAX_SOCKETS*2;
			DWORD	dwCardsAvail = 0;
			if(EnumCard(&dwCardsNum, g_CardDescs, &dwCardsAvail) != CERR_SUCCESS){
				g_pKato->Log(LOG_DETAIL,TEXT("ERROR: Can not enum cards in the system!"));
				return TPR_ABORT;		
			}
			else{
			       NKMSG(_T("There are total %d cards in the system"), dwCardsAvail);
				if((uTestCategory == 2) && (dwCardsAvail< 2)){
					g_pKato->Log(LOG_DETAIL,TEXT("ERROR: Please plug in at least one Multiple fucntion card!!"));
					return TPR_ABORT;		
				}
				
				if(dwCardsNum > dwCardsAvail){
					g_pKato->Log(LOG_DETAIL,TEXT("WARNING: System has more cards that test can handle!"));
				}
				g_dwTotalCards = dwCardsAvail;
				g_pKato->Log(LOG_DETAIL,TEXT("There are total %d cards in the system"), dwCardsAvail);
			}
		}
	}
	else{
		if(bCardNotInSlot == TRUE){//if card is not detected in previous test, then ignore all other tests
			g_pKato->Log(LOG_DETAIL,TEXT("Can not detect the card, skipped\n"));
			return TPR_SKIP;
		}
	}

	//-- Let test manager to handle the test case execution --
	TestManager *TSMan = new  TestManager(dwTestGroup, dwTestCase, dwThreadNum);
	if(TSMan == NULL)
	    return TPR_FAIL;
	if(TRUE == (bRet = TSMan->Init()))
		bRet = TSMan->Exec();
	delete TSMan;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("-TestDispatchEntry\r\n")));
	return  (TRUE == bRet)?TPR_PASS:TPR_FAIL;

}


//Defines that only used in the following TestSetup API

#define	RETRY_IDLETIME		1000
#define	DRIVER_LOADTIME	5000

BOOL 
TestSetup(USHORT uTestCat){

	int 				i, iTry;
	HWND			hDiagWnd = NULL;
	TCHAR			lpstrNotification[MAX_STRINGLEN];
	DWORD			dwSockets, dwSocketsAvail;
	DWORD			dwSocketStatus;
	SOCKET_DESCRIPTOR SocketInfo;
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+TestSetup\r\n")));

	//enumerate sockets
	dwSockets = MAX_SOCKETS;
	if(EnumSocket(&dwSockets, g_SocketDescs, &dwSocketsAvail) != CERR_SUCCESS){
		g_pKato->Log(LOG_DETAIL,TEXT("ERROR: Can not enum sockets in the system!"));
		return FALSE;		
	}
	else{
		if(dwSockets > dwSocketsAvail){
			g_pKato->Log(LOG_DETAIL,TEXT("WARNING: System has more sockets that test can handle!"));
		}
		g_dwTotalSockets = dwSocketsAvail;
		g_pKato->Log(LOG_DETAIL, TEXT("There are total %d sockets in the system"), dwSocketsAvail);
	}

	for(i = 0; i < (int)dwSocketsAvail; i++){//set up 

		SocketInfo= g_SocketDescs[i];
		NKMSG(_T("Now checking socket %s, socket index %d \r\n"), SocketInfo.sSocketName, SocketInfo.dwSocketIndex);

		//get socket status
		dwSocketStatus = 0;
		if (CERR_SUCCESS == GetSocketStatus(SocketInfo.dwSocketIndex, &dwSocketStatus)){
	          	if ((dwSocketStatus & CARD_DETECTED_MASK) == 0){
				NKMSG(_T("NO DETECTION on socket %d \r\n "), SocketInfo.dwSocketIndex);
	          		continue;
	          	}
	          	else {//there's already a card in that slot, need to remove it first
				//create a dialog box to prompt user to remove that card
	          		hDiagWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_REMOVECARD), NULL, NULL);
	          		if(NULL == hDiagWnd){
					g_pKato->Log(LOG_DETAIL,TEXT("WARNING: Can not create dialog box! "));
	          		}
	            		//show dialog   		
	          		ShowWindow(hDiagWnd, SW_SHOW);

	     			//create the notification string and set it to the dlg box
				wsprintf(lpstrNotification, TEXT("Please REMOVE the card in socket %s, index %d now"), SocketInfo.sSocketName, SocketInfo.dwSocketIndex );
	          		SetDlgItemText(hDiagWnd, IDC_TEXT1, lpstrNotification);
	          		UpdateWindow(hDiagWnd);

				//debug output msg for headless tests.
	          		NKMSG(TEXT("Please REMOVE the card in the socket %s, index %d!\r\n"), SocketInfo.sSocketName, SocketInfo.dwSocketIndex);
	          		//wait until user removed that card
				while ((dwSocketStatus & CARD_DETECTED_MASK) != 0){
					dwSocketStatus = 0;
	    				if(CERR_SUCCESS != GetSocketStatus(SocketInfo.dwSocketIndex, &dwSocketStatus)){
						g_pKato->Log(LOG_DETAIL,TEXT("Can not get socket status! "));
						return FALSE;
	    				}
          				NKMSG(TEXT("Socket status = 0x%x\r\n "), dwSocketStatus);
	    				Sleep(RETRY_IDLETIME);
				}
				//destroy the dialogbox
				if(NULL != hDiagWnd)
					DestroyWindow(hDiagWnd);
				hDiagWnd = NULL;
	          	}
		}
		else {//can not get card status, have to exit
	      		g_pKato->Log(LOG_DETAIL,TEXT("GetSocketStatus() call failed!"));
			return FALSE;
		}
	}

	PUCHAR pSocketStatus = new UCHAR[dwSocketsAvail];
	if(pSocketStatus == NULL){//should not happen
            g_pKato->Log(LOG_DETAIL,TEXT("Out Of Memory!"));
            return FALSE;
	}
	memset(pSocketStatus, 0, sizeof(UCHAR)*dwSocketsAvail);
	
	//now insert cards 
	DWORD dwInsertedSlots = 0;
	while(dwInsertedSlots < dwSocketsAvail){//set up 

             //prompt user to insert card
		hDiagWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_INSERTCARD), NULL, NULL);
	       if(NULL == hDiagWnd){
			g_pKato->Log(LOG_DETAIL,TEXT("WARNING: Can not create dialog box! "));
	       }

		//create the notification string and set it to the dlg box
		if(uTestCategory == 2)//MFC card
			wsprintf(lpstrNotification, TEXT("Please INSERT MFC card No. %d in avaliable slot now"), dwInsertedSlots+1);
		else //normal card
			wsprintf(lpstrNotification, TEXT("Please INSERT the card No. %d in avaliable slot now"), dwInsertedSlots+1);
			
	      	SetDlgItemText(hDiagWnd, IDC_TEXT2, lpstrNotification);
		ShowWindow(hDiagWnd, SW_SHOW);
		UpdateWindow(hDiagWnd);

		//debug output msg for headless tests.
	      	NKMSG(TEXT("%s"), lpstrNotification);

		iTry = 0;
             while (iTry < MAX_CONNECT_TRY){
        		for(int j = 0; j < (int)dwSocketsAvail; j++){
                		SocketInfo= g_SocketDescs[j];
                		dwSocketStatus = 0;
        			if(CERR_SUCCESS != GetSocketStatus(SocketInfo.dwSocketIndex, &dwSocketStatus)){
        				g_pKato->Log(LOG_DETAIL,TEXT("Can not get card status! "));
        				delete[] pSocketStatus;
        				return FALSE;
        			}
                		if((0 != (dwSocketStatus & CARD_DETECTED_MASK)) && pSocketStatus[j] == 0){
                                //a new card is inserted
                	    		NKMSG(TEXT("Soket %d: Socket status = 0x%x\r\n "), j, dwSocketStatus);
                                pSocketStatus[j] = 1;
                		}
        		}

                   DWORD dwInsertedThisRound = 0;
                   for(int k = 0; k < (int)dwSocketsAvail; k++){
                       if(pSocketStatus[k] == 1)
                           dwInsertedThisRound ++;
                   }

                   if(dwInsertedThisRound > dwInsertedSlots){//new card inserted
        			//destroy the dialogbox
        			if(NULL != hDiagWnd){
        				DestroyWindow(hDiagWnd);
                			hDiagWnd = NULL;
        			}

        			dwInsertedSlots = dwInsertedThisRound;
                         break;
                   }
                   iTry ++ ;
			Sleep(RETRY_IDLETIME);
        	      	NKMSG(TEXT("%s"), lpstrNotification);
             }

		if(uTestCategory < 3 && dwInsertedSlots > 0){
                 break; //we had enough card for test category 1 and 2.
		}
	}

	//destroy the dialogbox if it still exists
	if(NULL != hDiagWnd){
		DestroyWindow(hDiagWnd);
	      hDiagWnd = NULL;
	}

      delete[] pSocketStatus;
      
      if((uTestCategory < 3 && dwInsertedSlots == 0) || (uTestCategory == 3 && (dwInsertedSlots < dwSocketsAvail))){
            g_pKato->Log(LOG_DETAIL,TEXT("Not enough card(s) being inserted!"));
            return FALSE;
      }
	//card inserted, now wait the dummy driver to be fully loaded
	Sleep(DRIVER_LOADTIME);
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("-TestSetup\r\n")));

	return TRUE;
}

