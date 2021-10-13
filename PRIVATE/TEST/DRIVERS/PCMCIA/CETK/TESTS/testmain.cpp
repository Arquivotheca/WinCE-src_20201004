//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#include "testmain.h"
#include "resource.h"
#include "common.h"
#include "TestManager.h"

//defines for retry times of get status, which is used to detect whether a card is inserted or not
#define MAX_CONNECT_TRY	100

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
	DWORD	dwTestSocket, dwTestGroup, dwTestCase;
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
	dwTestSocket= dwTestID / TESTSOCKET_DIV;
	if(5 < dwTestSocket) {//invalid test slot number
		g_pKato->Log(LOG_FAIL,TEXT("Test socket number: %u is invalid! \n"), dwTestSocket);
		return TPR_ABORT;
	}
	dwTestGroup = (dwTestID - TESTSOCKET_DIV* dwTestSocket) / TESTGROUP_DIV;
	if(0 >= dwTestGroup || 7 <= dwTestGroup) {//invalid test group number
		g_pKato->Log(LOG_FAIL,TEXT("Test group ID: %u is invalid! \n"), dwTestGroup);
		return TPR_ABORT;
	}
	dwTestCase = (dwTestID - TESTSOCKET_DIV* dwTestSocket) % TESTGROUP_DIV;

	//--setup socket and function info --
	switch(dwTestSocket){
		case 1:	//single function card test, using socket 0
				uSock = 0;
				uFunc = 0;
				break;
		case 2:	//single function card test, using socket 1
				uSock = 1;
				uFunc = 0;
				break;
		case 3:	//multiple-function card test, using socket 0
				uSock = 0;
				uFunc = 1; //we only test function 1here
				break;
		case 4:	//multiple-function card test, using socket 1
				uSock = 1;
				uFunc = 1; //we only test function 1 here
				break;
		case 5:	// two cards in two sockets at the same time
				uSock = 2;
				uFunc = 0;
				break;
	}

	//-- if we need to switch to another slot, then setup registry, and load dummy driver --
	if(uSock != uSocket || uFunc != uFunction) {
		uSocket = uSock;
		uFunction = uFunc;
		if(FALSE == TestSetup(uSocket, uFunction)){
			g_pKato->Log(LOG_DETAIL,TEXT("Test setup had failed! test will be skipped\n"));
			bCardNotInSlot = TRUE; //can not detect the connection
			return TPR_SKIP;
		}
		else
			bCardNotInSlot = FALSE;
	}
	else{
		if(bCardNotInSlot == TRUE){//if card is not detected in previous test, then ignore all other tests
			g_pKato->Log(LOG_DETAIL,TEXT("Can not detect the card, skipped\n"));
			return TPR_SKIP;
		}
	}

	//-- Let test manager to handle the test case execution --
	TestManager *TSMan = new  TestManager(dwTestGroup, dwTestCase, dwThreadNum);
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
TestSetup(USHORT uSocketID, USHORT uFuncID){

    	PCARD_STATUS 	pStatus = new CARD_STATUS ;
	int 				i, iTry;
	HWND			hDiagWnd = NULL;
	TCHAR			lpstrNotification[MAX_STRINGLEN];

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+TestSetup\r\n")));

	for(i = 0; i < MAX_SOCKETS; i++){
		//ignore this socket if we are not going to test it
		if(uSocketID != i && uSocketID != MAX_SOCKETS)
			continue;

		(pStatus->hSocket).uSocket = i ;
	    	(pStatus->hSocket).uFunction = (UCHAR)uFuncID ;

		NKMSG(_T("Socket is: %u, function is:%u \r\n"), (pStatus->hSocket).uSocket, (pStatus->hSocket).uSocket);
		pStatus->fCardState = 0;
		pStatus->fSocketState = 0;
		if (CERR_SUCCESS == CardGetStatus(pStatus)){
	          	if ((pStatus->fCardState & CARD_DETECTED_MASK) == 0){
				NKMSG(_T("NODETECTIONSocket is: %u, function is:%u\r\n "), (pStatus->hSocket).uSocket, (pStatus->hSocket).uSocket);
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
				wsprintf(lpstrNotification, TEXT("Please REMOVE the card in socket %1u now"), i);
	          		SetDlgItemText(hDiagWnd, IDC_TEXT1, lpstrNotification);
	          		UpdateWindow(hDiagWnd);
	          		
				//debug output msg for headless tests.
	          		NKMSG(TEXT("Please REMOVE the card in the socket %1u!\r\n"), i);
	          		//wait until user removed that card
				while ((pStatus->fCardState & CARD_DETECTED_MASK) != 0){
						pStatus->fCardState = 0;
						pStatus->fSocketState = 0;
		    				if(CERR_SUCCESS != CardGetStatus(pStatus)){
							g_pKato->Log(LOG_DETAIL,TEXT("Can not get card status! "));
							delete pStatus;
							return FALSE;
		    				}
	          				NKMSG(TEXT("cardstate = 0x%x, socketstatus = 0x%x\r\n "), pStatus->fCardState, pStatus->fSocketState);
		    				Sleep(RETRY_IDLETIME);
				}
				//destroy the dialogbox
				if(NULL != hDiagWnd)
					DestroyWindow(hDiagWnd);
				hDiagWnd = NULL;
	          	}
		}
		else {//can not get card status, have to exit
	      		g_pKato->Log(LOG_DETAIL,TEXT("CardGetStatus() call failed! "));
	      		delete pStatus;
			return FALSE;
		}
	}

	//update registry, only need to do it once during test
	if(FALSE == bRegUpdated){
		if(FALSE == UpdateRegistry()){
	      		g_pKato->Log(LOG_DETAIL,TEXT("update registry failed! "));
	      		delete pStatus;
			return FALSE;
		}
		else
			bRegUpdated = TRUE;
	}
	
	for(i = 0; i < MAX_SOCKETS; i++){

		//ignore this socket if we are not going to test it
		if(uSocketID != i && uSocketID != 2)
			continue;
		
		//now let user to re-plugin the card
		hDiagWnd = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_INSERTCARD), NULL, NULL);
	       if(NULL == hDiagWnd){
			g_pKato->Log(LOG_DETAIL,TEXT("WARNING: Can not create dialog box! "));
	       }

		//create the notification string and set it to the dlg box
		if(uFuncID == 1)//MFC card
			wsprintf(lpstrNotification, TEXT("Please INSERT the MFC card in socket %1u now"), i);
		else if(uFuncID == 2)//two cards testing
			wsprintf(lpstrNotification, TEXT("Two cards test -- Please INSERT one card in socket %1u now"), i);
		else //normal card
			wsprintf(lpstrNotification, TEXT("Please INSERT the card in socket %1u now"), i);
			
	      	SetDlgItemText(hDiagWnd, IDC_TEXT2, lpstrNotification);
		ShowWindow(hDiagWnd, SW_SHOW);
		UpdateWindow(hDiagWnd);

		//debug output msg for headless tests.
	      	NKMSG(TEXT("%s"), lpstrNotification);

		(pStatus->hSocket).uSocket = i ;
	    	(pStatus->hSocket).uFunction = (UCHAR)uFuncID ;

		//waiting for the card to be plugged in
		pStatus->fCardState = 0;

	
		iTry = 0;
		while (0 == (pStatus->fCardState & CARD_DETECTED_MASK)){
			if(iTry > MAX_CONNECT_TRY){//consider this as a fail
				g_pKato->Log(LOG_DETAIL,TEXT("Can not connect to the board!"));
				delete pStatus;
				#ifndef PCMCIA_AUTOMATED
					//destroy the dialogbox
					if(NULL != hDiagWnd)
						DestroyWindow(hDiagWnd);
					hDiagWnd = NULL;
				#endif
				return FALSE;
			}
	
			pStatus->fCardState = 0;
			pStatus->fSocketState = 0;
			if(CERR_SUCCESS != CardGetStatus(pStatus)){
				g_pKato->Log(LOG_DETAIL,TEXT("Can not get card status! "));
				delete pStatus;
				return FALSE;
			}
	      		NKMSG(TEXT("cardstate = 0x%x, socketstatus = 0x%x \r\n"), pStatus->fCardState, pStatus->fSocketState);
			iTry ++;
			Sleep(RETRY_IDLETIME);
		}

		//destroy the dialogbox
		if(NULL != hDiagWnd)
			DestroyWindow(hDiagWnd);
		hDiagWnd = NULL;
	}

	//card inserted, now wait the dummy driver to be fully loaded
	Sleep(DRIVER_LOADTIME);
	
	delete pStatus;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("-TestSetup\r\n")));

	return TRUE;
}

//stole from reg_update.exe, this function will add an entry for the dummy driver,
//so whne any card plug in, the dummy driver will be loaded. 
BOOL 
UpdateRegistry(VOID){

	TCHAR    strDll[20], strEntry[20];
	HKEY hNewKey=NULL;
	HKEY hNewKey2=NULL;
	DWORD dwDisposition=0;

	memset(strDll,0x0,20*sizeof(TCHAR));
	memset(strEntry,0x0,20*sizeof(TCHAR));

	wsprintf(strDll,TEXT("dummydr.dll"));
	wsprintf(strEntry,TEXT("DetectIntr"));

	SetLastError(0);

	NKMSG(TEXT("Updating Registery... "));
	
	// Create registry entry at top of Detect List
 	if(ERROR_SUCCESS != RegCreateKeyEx( HKEY_LOCAL_MACHINE, TEXT("Drivers\\PCMCIA\\Detect\\01"), 
							0, TEXT(""), 0, 
							0, NULL, 
							&hNewKey, &dwDisposition ))
		return FALSE;

  	if(ERROR_SUCCESS != RegSetValueEx(hNewKey, TEXT("Dll"), 
 						0, REG_SZ, (const BYTE *)strDll, 20*sizeof(TCHAR) )){
 		RegCloseKey(hNewKey);
		ResetRegistry();
 		return FALSE;
  	}

	if(ERROR_SUCCESS != RegSetValueEx(hNewKey, TEXT("Entry"), 
 						0, REG_SZ, (const BYTE *)strEntry, 20*sizeof(TCHAR) )){
		RegCloseKey(  hNewKey ); 
		ResetRegistry();
 		return FALSE;
	}
	
	RegCloseKey(  hNewKey ); 

 	//Create equivalent driver registry entry
	memset(strDll,0x0,20*sizeof(TCHAR));
	memset(strEntry,0x0,20*sizeof(TCHAR));

	wsprintf(strDll,TEXT("dummydr.dll"));
	wsprintf(strEntry,TEXT("tst"));

	if(ERROR_SUCCESS != RegCreateKeyEx( HKEY_LOCAL_MACHINE, TEXT("Drivers\\PCMCIA\\PCMTEST"), 
							0, TEXT(""), 0, 
							0, NULL, 
							&hNewKey2, &dwDisposition )){
		ResetRegistry();
		return FALSE;
	}

  	if(ERROR_SUCCESS != RegSetValueEx(hNewKey2, TEXT("Dll"), 
 						0, REG_SZ, (const BYTE *)strDll, 20*sizeof(TCHAR) )){
 		RegCloseKey(hNewKey2);
		ResetRegistry();
 		return FALSE;
  	}
 		

 	if(ERROR_SUCCESS != RegSetValueEx(hNewKey2, TEXT("Prefix"), 
 						0, REG_SZ, (const BYTE *)strEntry, 20*sizeof(TCHAR) )){
 		RegCloseKey(hNewKey2);
		ResetRegistry();
 		return FALSE;
  	}
 
	RegCloseKey(  hNewKey2 ); 

	return TRUE;

}


//stole from reg_update.exe, clear the dummy driver entry 

BOOL ResetRegistry() {

	HKEY hNewKey=NULL;

	SetLastError(0);
	NKMSG(TEXT("Resetting Registery \r\n"));

	// delete registry entry at top of Detect List

	if(ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, TEXT("Drivers\\PCMCIA\\Detect"), 
							0, 0, 
							&hNewKey))
		goto DEL_DRIVER_ENTRY;
	
	RegDeleteKey( hNewKey, TEXT("01") );
	RegCloseKey(  hNewKey ); 

DEL_DRIVER_ENTRY:
 	//delete equivalent driver registry entry
	if(ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, TEXT("Drivers\\PCMCIA"), 
							0,0, 
							&hNewKey))
		return FALSE;

	RegDeleteKey( hNewKey, TEXT("PCMTEST") );
	RegCloseKey(  hNewKey ); 

	return TRUE;
	
}

