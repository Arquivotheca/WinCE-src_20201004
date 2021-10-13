//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#include "testmain.h"
#include "common.h"
#include "ExclTest.h"

BOOL
ExclTest::Init(){
	LPEXCLTESTPARAMS  lparm = NULL;

	if(dwCaseID >= 30){
		lparm = &exclarr[0];
	}
	else {
		lparm = &exclarr[dwCaseID-1];
	}
	
	//initialize test parameters
	nTestFunc	= lparm->nTestFunc;
	fManualAction = lparm->fManualAction;
	uCallRR       	= lparm->uCallRR;
	nHCLIENT      	= lparm->nHCLIENT;
	fAttriC    		= lparm->fAttriC    	 ;
	uExpectRet	= lparm->uExpectRet	 ;
	nHCLIENT2	= lparm->nHCLIENT2	;
	uExpectRet2	= lparm->uExpectRet2   ;
     	uEventGotExpected	= lparm->uEventGotExpected     ;
     	uEventGotExpected1    	= lparm->uEventGotExpected1    ;
     	uEventGotExpected2    	= lparm->uEventGotExpected2    ;

     	uSocketForWindow = nTestFunc & 0x1;   // 0 use Socket;   1 use socket 1
	
	//setup client data
	client_data.hClient= NULL;
	client_data.hEvent = NULL;
	client_data.pWin   = NULL;
	client_data.uGranularity     = 0;	  // for Manaul test use
	client_data.fCardReqExclDone = 0;
	client_data.uCardReqStatus = 0xFF;	// undefined value
	client_data.ThreadNo = dwThreadID;
    	if (nTestFunc == 100 || nTestFunc == 101  )
		  client_data.uReqReturn = CERR_IN_USE;	  // Reject the request, so no one gets the exclusive

	return TRUE;
}

//defines used in ThreadRun()
#define	EXCLTST_WINSIZE	0x100

DWORD ExclTest::ThreadRun() {

	CARD_REGISTER_PARMS	crParam;
	CARD_WINDOW_PARMS    	cwParam;
	CARD_CLIENT_HANDLE   	hClient = NULL;
	CARD_WINDOW_HANDLE   	hWindow = NULL;
  	CARD_SOCKET_HANDLE   	hSocket;
	UINT                 			uRet;
	DWORD                		dwError, dw;
	PCLIENT_CONTEXT 		pData = &client_data;
	CARD_CLIENT_HANDLE   	hClientTest = NULL;
	BOOL	  				fDeregisterClientDone = FALSE;
	BOOL	  				fReleaseWindowDone = FALSE;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ExclTest::ThreadRun() enterted\r\n")));
   	
	//create event:
	pData->hEvent = CreateEvent(0L,  FALSE,  FALSE /*fInitState*/, NULL);
	if (NULL == pData->hEvent){
   		g_pKato->Log(LOG_DETAIL,TEXT("***Thead %u for Socket %u: CreateEvent() FAILed:\r\n"), dwThreadID, uLocalSock);
		dwError = GetLastError();
		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: GetLastError() return = 0x%lx\r\n"), dwThreadID, uLocalSock, dwError);
		SetResult(FALSE);
		return dwError;
	}

	crParam.fAttributes	= fAttriC;
	crParam.fEventMask   	= 0xFFFF;
	crParam.uClientData 	= (UINT32) pData;

	hSocket.uSocket = (UCHAR)uLocalSock;
	hSocket.uFunction = (UCHAR)uLocalFunc;
	
	//now register client
	SetLastError(0);
	hClient = CardRegisterClient(CallBackFn_Client, &crParam);
   	dw = WaitForSingleObject( pData->hEvent, TEST_WAIT_TIME);
	if (!hClient || dw == WAIT_TIMEOUT){
   		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRegisterClient() FAILed\r\n"), dwThreadID, uLocalSock);
		dwError = GetLastError();
   		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: GetLastError() return =0x%lx\r\n"), dwThreadID, uLocalSock, dwError);
   		SetResult(FALSE);
	 	goto LCLoseEventHandle;
	}
	if (pData->uCardReqStatus != (UINT) hClient){
		 g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: pData->uCardReqStatus (0x%lX) != hClient (0x%lX)\r\n"),
						dwThreadID, pData->uCardReqStatus, hClient);
		SetResult(FALSE);
		 goto LDeregisterClient;
	}

	// reset	this uEventGot for later CardRequestExclusive().
	pData->uEventGot = 0;
	pData->uCardReqStatus = 0xFF;
	pData->hClient = hClient;

	CardResetFunction(hClient, hSocket);

	cwParam.hSocket.uSocket   = (UCHAR)uLocalSock  ;
	cwParam.hSocket.uFunction = (UCHAR)uLocalFunc;
	cwParam.fAttributes  = 0x5;                // for IO Window
	cwParam.uWindowSize  = EXCLTST_WINSIZE;
	cwParam.fAccessSpeed= WIN_SPEED_EXP_10MS;	 // 

	//request window 
	SetLastError(0);
	pData->uEventGot = 0;
	hWindow = CardRequestWindow(hClient, &cwParam);
	if (!hWindow){
		dwError = GetLastError();
		if(dwError == CERR_BAD_SOCKET && uLocalFunc == 1){//MFC testing
			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRequestWindow() FAILed. Card is not MFC compliant: socket=%u:  err=0x%lX\r\n"),
				 	   	dwThreadID, uLocalSock, cwParam.hSocket.uSocket, dwError);
			goto LDeregisterClient;
		}
   	       else {
   	       	g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRequestWindow() FAILed: socket=%u:  err=0x%lX\r\n"),
				 		dwThreadID, uLocalSock, cwParam.hSocket.uSocket, dwError);
   	       }
		SetResult(FALSE);
		goto LDeregisterClient;
	}


	if (!(uCallRR & CALL_REQUEST))   // don't call CardRequestExclusive()
		goto LTestReleaseExcl;

	//request exclusive access
      g_pKato->Log(LOG_DETAIL,TEXT("================Thread %u for Socket %u:  BEFORE test RequestExclusive =============\r\n\r\n"),
			dwThreadID, uLocalSock);
      
 	 // reset	this uEventGot for later CardRequestExclusive().
	 // especially in threads situation. Need to cleare early
	 pData->uEventGot = 0;
	 pData->uCardReqStatus = 0xFF;
	SetLastError(0);

	//===========================================================================
	//
	//  Now test CardRequestExclusive()
	//
	//===========================================================================

	hSocket.uSocket   = (UCHAR)uLocalSock;
	hSocket.uFunction = (UCHAR)uLocalFunc;
	
	switch (nHCLIENT){
		case 0:  
			hClientTest = NULL;        
			break;
		case 1:  
			hClientTest = hClient;  
			break;
		case 2:  //hClient =  invalid handle
			hClientTest = hClient;
			CardReleaseWindow(hWindow);
			CardDeregisterClient(hClient);
			fReleaseWindowDone = TRUE;
			fDeregisterClientDone = TRUE;
			break;
		case 3:	 // window released is OK. It won't affect to Get Exclusive
			hClientTest = hClient;
			CardReleaseWindow(hWindow);
			fReleaseWindowDone = TRUE;
			break;
		case 4: //use invalid socket and function number
			hClientTest = hClient;
			hSocket.uSocket   = 0xFF;
			hSocket.uFunction = 0xFF;
			break;
		default:
			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: nHCLIENT =%d : Don't know how to handle.\r\n"),
				dwThreadID, uLocalSock, nHCLIENT);
			SetResult(FALSE);
			goto LReleaseWindow;
	}



	uRet =  CardRequestExclusive(hClientTest, hSocket);
	if (uExpectRet != CERR_SUCCESS){
		 if ( uRet == uExpectRet){
			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u:  CardRequestExclusive()  FAIL as expected.  return 0x%lX\r\n"),
						 dwThreadID, uLocalSock,  uRet);
		}
		else{
			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u:  CardRequestExclusive() expect to fail.  BUT return 0x%lX != expected 0x%lX\r\n"),
					 	dwThreadID, uLocalSock, uRet, uExpectRet);
		}
		 goto LReleaseWindow;
	 }
	else  if (uRet != CERR_SUCCESS) {// Expect succeed
		 g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u:  CardRequestExclusive() FAIL.  return 0x%lX != expected 0x%lX\r\n"),
					dwThreadID, uLocalSock, uRet, uExpectRet);
		 goto LReleaseWindow;
	}

	 dw = WaitForSingleObject(pData->hEvent, INFINITE);
	
	 // check the event we got:  This might be the second thread
	 //
	 if ( pData->uCardReqStatus == CERR_SUCCESS){
		 if ((int) (pData->uEventGot - uEventGotExpected) < 0){
			 g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: GardRequestExclusive() FAIL:  uEventGot=0x%lx, expect uEvent=0x%lx uEvent1=0x%lx\r\n"),
					 dwThreadID, uLocalSock, pData->uEventGot, uEventGotExpected, uEventGotExpected1);
			 SetResult(FALSE);
		}
		else{
			 g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRequestExclusive() SUCCEEDed:  uEventGot=0x%lx, expect uEvent=0x%lx uEvent1=0x%lx\r\n"),
					 dwThreadID, uLocalSock, pData->uEventGot, uEventGotExpected, uEventGotExpected1);
		}
	}

	pData->uEventGot = 0;

	 //  Need waliting here to  test interface with second client
	 if (dwTotalThread > 0){
	 	BOOL	bInLoop = TRUE;
		while (bInLoop){
			if (nThreadsDone >= (int)dwTotalThread)
				bInLoop = FALSE;
			Sleep(TEST_WAIT_TIME);
		}
	}

	//===========================================================================
	//
	//  Now test CardReleaseExclusive()
	//
	//===========================================================================

LTestReleaseExcl:

	if (!(uCallRR & CALL_RELEASE))   // don't call CardReleaseExclusive()
		goto LReleaseWindow;
      g_pKato->Log(LOG_DETAIL,TEXT("================Thread %u for Socket %u:  BEFORE test ReleaseExclusive =============\r\n\r\n"),
			dwThreadID, uLocalSock);

	hSocket.uSocket   = (UCHAR)uLocalSock;
	hSocket.uFunction = (UCHAR)uLocalFunc;

	switch (nHCLIENT2){
		case 0:  
			hClientTest = NULL;        
			break;
		case 1:  
			hClientTest = hClient;  
			break;
		case 2:  //hClient =  invalid handle
			hClientTest = hClient;
			CardReleaseWindow(hWindow);
			CardDeregisterClient(hClient);
			fReleaseWindowDone = TRUE;
			fDeregisterClientDone = TRUE;
			break;
		case 4: //use invalid socket and function number
			hClientTest = hClient;
			hSocket.uSocket   = 0xFF;
			hSocket.uFunction = 0xFF;
			break;
		default:
			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: nHCLIENT2 =%d : Don't know how to handle.\r\n"),
				dwThreadID, uLocalSock, nHCLIENT2);
			SetResult(FALSE);
			goto LReleaseWindow;
	}

	uRet =  CardReleaseExclusive(hClientTest, hSocket);

	if (uRet != uExpectRet2){
		if (dwTotalThread > 1){
			if (uRet != CERR_IN_USE){
				g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardReleaseExclusive(): uExpectRet=0x%lx  uRet=0x%lx\r\n"),
									dwThreadID, uLocalSock, uExpectRet2, uRet);
				SetResult(FALSE);
			}
			// esle OK. Other clients which not got exclusive call CardReleaseExclusive() will get CERR_IN_USE
		}
		else{//single threaded
			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardReleaseExclusive() FAIL: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
							 dwThreadID, uLocalSock, uExpectRet2, uRet);
			SetResult(FALSE);
		}
		goto LReleaseWindow;
	}


	if (pData->uCardReqStatus == CERR_SUCCESS){
		// check the event we got
		if ((pData->uEventGot & uEventGotExpected2) != uEventGotExpected2){
   			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardReleaseExclusive() The expected events are not matching: uEventGet Expected=0x%lx  uEventGot=0x%lx\r\n"),
						dwThreadID, uLocalSock, uEventGotExpected2, pData->uEventGot);
		}
	}
	else if (pData->uCardReqStatus == CERR_IN_USE){
		 // In mulitiple clients situation: this client  does not get exclusive, when it calls ReleaseExclusive()
		 // the uRet is CERR_IN_USE  and it only get CE_CARD_INSERTION event.
     		if (gfSomeoneGotExclusive){
			 // Then this client, it should get CE_CARD_INSERTION event.
			 if ((pData->uEventGot & CE_CARD_INSERTION) != CE_CARD_INSERTION){
				 g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: GardReleaseExclusive() FAIL: uEventGet: Expected=0x40  acturally Got=0x%lx\r\n"),
						 dwThreadID, uLocalSock,  pData->uEventGot);
				 SetResult(FALSE);
			}
	     }
	}

LReleaseWindow:
	uRet = CardReleaseWindow(hWindow);
	if (uRet) { // FAIL
		if (!fReleaseWindowDone){
  			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: GardReleaseWindow() FAIL: uRet=0x%lx\r\n"),
					dwThreadID, uLocalSock, uRet);
			SetResult(FALSE);
		}
	}

LDeregisterClient:

	uRet = CardDeregisterClient(hClient);
	if (uRet){// failed
		if (!fDeregisterClientDone){//this is the first time to deregister, so its a real failure
   			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardDeregisterClient() FAILed: uRet=0x%lx  fDeregisterClient =FALSE\r\n"),
					                          dwThreadID, uLocalSock,   uRet);
			SetResult(FALSE);
		}
	}
	else{
		if (fDeregisterClientDone){// hClient has been Deregistered, so it SHOULD FAIL!
   			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardDeregisterClient() FAILed: uRet=0x%lx  fDeregisterClient=TRUE\r\n"),
									dwThreadID, uLocalSock,   uRet);
			SetResult(FALSE);
		}
	}

LCLoseEventHandle:
	//free event
	CloseHandle( pData->hEvent);
	pData->hEvent = NULL;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- ExclTest::ThreadRun()\r\n")));

	return GetLastError();
}

