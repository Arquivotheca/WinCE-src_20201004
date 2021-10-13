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
#include "WinTest.h"

BOOL
WinTest::Init(){
	LPWINTESTPARAMS  lparm = &winarr[dwCaseID-1];

	//initialize test parameters
	nTestFunc		= lparm->nTestFunc;
	nClients      		= lparm->nClients      ;
	fAttri        		= lparm->fAttri        ;
	fAccessSpeed  	= lparm->fAccessSpeed  ;
	uWindowSize   	= lparm->uWindowSize   ;

	nHandle       		= lparm->nHandle       ;	
	uCardAddr     		= lparm->uCardAddr     ;
	uSize         		= lparm->uSize         ;	
	nPGranularity 		= lparm->nPGranularity ;
	nExpectRet	  	= lparm->nExpectRet	 ;
	dwExpectEr	  	= lparm->dwExpectEr	 ;
				  	  			
	nHandle2      		= lparm->nHandle2      ;
	fAttri2       		= lparm->fAttri2       ;
	fAccessSpeed2 	= lparm->fAccessSpeed2 ;
	uExpectRet    		= lparm->uExpectRet    ;
	gfModemCard 		= 0;

	//setup client data
	client_data.hClient= NULL;
	client_data.hEvent = NULL;
	client_data.pWin   = NULL;
	client_data.uGranularity     = 0;	  // for Manaul test use
	client_data.fCardReqExclDone = 0;
	client_data.uCardReqStatus = UNDEFINED_VALUE;	// undefined value
	client_data.ThreadNo = dwThreadID;

	return TRUE;
}

//constant defines only used in the following ThreadRun() API.
#define	WINTST_BASE_ADDR		0x3f8
#define	WINTST_WIN_SIZE		8
#define	WINTST_WIN_INTERVAL	0x10

DWORD WinTest::ThreadRun() {

	CARD_CLIENT_HANDLE   	hClient = NULL;
	CARD_WINDOW_HANDLE   	hWindow = NULL, hWindowTest = NULL;
	BOOL	  fDeregisterClientDone = FALSE;
	BOOL	  fReleaseWindowDone    = FALSE;
	PVOID   pWin;
	UINT32  uGranularity;
	UINT    uRet;
	DWORD   dwError;
	UINT32  uCardAddrTemp, uSizeTemp;
	PCLIENT_CONTEXT 		pData = &client_data;


	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ WinTest::ThreadRun() enterted\r\n")));

	//Three special cases
   	if(nTestFunc == 0x10){//some additional test case
   		Test_SpecialCase();
   		return GetLastError();
   	}

   	if(nTestFunc == 0x11){//test using invalid parameters
   		Test_InvalidParas();
   		return GetLastError();
   	}
	
   	if(nTestFunc == 0x12){//test colliding windows' scenario
   		Test_CollideWindow();
   		return GetLastError();
   	}
	
   	//normal test scenario
 	if (!RegisterClientAndWindow(CLIENT_ATTR_IO_DRIVER|CLIENT_ATTR_NOTIFY_SHARED, &hClient, &hWindow, pData)){
		SetResult(FALSE);
		return GetLastError(); 
 	}
	// Account for the case when we justifiably run out of resources
	// which means that above function returned TRUE, but
	// both handles are NULL.
	// We skip the test, returning OK.
	if ( (NULL==hClient) && (NULL==hWindow) )
		return TRUE;

	//===========================================================================
	//  Now test CardMapWindow()
	//===========================================================================

	switch (nHandle){
		case 0:  
			hWindowTest = NULL;        
			break;
		case 1:  
			hWindowTest = hWindow;  
			break;
		case 2:  //hWindowTest =  invalid handle
			hWindowTest = hWindow;
			CardReleaseWindow(hWindow);
			fReleaseWindowDone   =  TRUE;
			break;
		case 3:  //hClient =  invalid variable
			CardDeregisterClient(hClient);
			fDeregisterClientDone = TRUE;
			break;
		default:
			g_pKato->Log(LOG_FAIL,
				(LPTSTR)TEXT("  Thread %u Socket %u: nHandle =%d : Don't know how to handle.\r\n"),
					dwThreadID, uLocalSock,  nHandle);
			SetResult(FALSE);
			goto LDeregisterClient;
	}

	uCardAddrTemp = WINTST_BASE_ADDR;
	uSizeTemp =  WINTST_WIN_SIZE;
	if (dwTotalThread > 1){
		uCardAddrTemp +=  dwThreadID * (uSize+WINTST_WIN_INTERVAL);
	}

	//map window
  	SetLastError(0);
  	uGranularity = 1;  // default
  	pWin = CardMapWindow(hWindowTest, uCardAddrTemp, uSizeTemp, nPGranularity == 0 ? NULL : &uGranularity);
	dwError = GetLastError();

	g_pKato->Log(LOG_DETAIL,
		 (LPTSTR)TEXT(" Thread %u Socket %u: CardMapWindow() return pWin=0x%lX  uGranularity=%u  dwError=0x%lx\r\n"),
		  dwThreadID, uLocalSock, pWin, uGranularity, dwError);

	if ((nExpectRet && !pWin) || (!nExpectRet && pWin) ){
		if (!pWin){
		 	 #ifdef x86
		  	if(dwError==CERR_BAD_BASE){
			  	g_pKato->Log(LOG_DETAIL,  (LPTSTR)TEXT("  Thread %u Socket %u: CardMapWindow() FAIL as Expected X86 platform:  uCardAddr=0x%lx, uSize=0x%lx   dwError=0x%lx\r\n"),
			  		 dwThreadID, uLocalSock,  uCardAddrTemp, uSizeTemp, dwError);
			       goto LReleaseWindow;
		  	}
		  	#endif
		  	if ((uSizeTemp == 0 && dwError == CERR_BAD_SIZE)  || (dwError == CERR_OUT_OF_RESOURCE)){
				 g_pKato->Log(LOG_DETAIL,
				   (LPTSTR)TEXT("  Thread %u Socket %u: CardMapWindow() FAIL as Expected:  uCardAddr=0x%lx, uSize=0x%lx   dwError=0x%lx\r\n"),
						 dwThreadID, uLocalSock,  uCardAddrTemp, uSizeTemp, dwError);
			}
		 	else if (uCardAddrTemp == 0 && dwError == 0){
			  	g_pKato->Log(LOG_DETAIL,
				  (LPTSTR)TEXT("  Thread %u Socket %u: CardMapWindow() Succeed: uCardAddr=0x%lx uSize=0x%lx dwError=%ld\r\n"),
						 dwThreadID, uLocalSock,  uCardAddrTemp, uSizeTemp, dwError);
			}
		 	//Adding CERR_IN_USE check back in with new IOR check-in
			//This is normally caused by mapping to a already-mapped address, or I/O address is not avalialbe
			//in this system. So it will be ignored.
		 	else if (dwError == CERR_IN_USE ){
				g_pKato->Log(LOG_DETAIL,
						(LPTSTR)TEXT("CardMapWindow() FAIL as Expected: uCardAddr=0x%lx uSize=0x%lx dwError=0x%lx\r\n"),
								uCardAddrTemp, uSizeTemp, dwError);
			}
		 	else{
				 g_pKato->Log(LOG_FAIL,
						(LPTSTR)TEXT("  Thread %u Socket %u: CardMapWindow() FAIL:  uCardAddr=0x%lx, uSize=0x%lx   dwError=0x%lx\r\n"),
						dwThreadID, uLocalSock,  uCardAddrTemp, uSizeTemp, dwError);
				 SetResult(FALSE);
			}

			goto LReleaseWindow;
		}
		else{
	   		g_pKato->Log(LOG_FAIL,
		      		(LPTSTR)TEXT("  Thread %u Socket %u: CardMapWindow() FAIL: nExpectRet=%d  pWin=0x%lx   uCardAddr=0x%lx, uSize=0x%lx\r\n"),
							dwThreadID, uLocalSock,  nExpectRet, pWin, uCardAddrTemp, uSizeTemp);
			SetResult(FALSE);
		}
	}
 	else{
		if (!pWin){
			 if (dwError != dwExpectEr) {
				g_pKato->Log(LOG_FAIL,TEXT("  Thread %u Socket %u: dwError (0x%lx)  !=  dwExpectEr (0x%lx)\r\n"),
						 dwThreadID, uLocalSock,  dwError, dwExpectEr);
				SetResult(FALSE);
			}
			goto LReleaseWindow;
		}
	}

	 g_pKato->Log(LOG_DETAIL,
		 (LPTSTR)TEXT("  Thread %u Socket %u: CardMapWindow() PASS: pWin=0x%lx   uCardAddr=0x%lx, uSize=0x%lx\r\n"),
		 dwThreadID, uLocalSock,  pWin, uCardAddrTemp, uSizeTemp);

	//===========================================================================
	//
	//  Now test CardModifyWindow()
	//
	//===========================================================================
	switch (nHandle2){
		case 0:  
			hWindowTest = NULL;        
			break;
		case 1:  
			hWindowTest = hWindow;  
			break;
		case 2:  //hWindowTest =  invalid handle
			hWindowTest = hWindow;
			CardReleaseWindow(hWindow);
			fReleaseWindowDone   =  TRUE;
			break;
		case 3:  //hClient =  invalid variable
			CardDeregisterClient(hClient);
			fDeregisterClientDone = TRUE;
			break;
		default:
			g_pKato->Log(LOG_FAIL,
				(LPTSTR)TEXT("  Thread %u Socket %u: nHandel2 =%d : Don't know how to handle.\r\n"),
					dwThreadID, uLocalSock,  nHandle2);
			SetResult(FALSE);
			goto LDeregisterClient;

	}

	uRet =  CardModifyWindow(hWindowTest, fAttri2, fAccessSpeed2);
	if (uRet != uExpectRet){
		if ((dwTotalThread > 1) &&  uRet == CERR_IN_USE){
   		 		g_pKato->Log(LOG_DETAIL,
		      			(LPTSTR)TEXT("  Thread %u Socket %u: CardModifyWindow() FAIL as Expected in Multi threads testing: uRet=0x%lX  uExpectRet=0x%lX fAttr=0x%lx, fSpeed=0x%lx\r\n"),
							dwThreadID, uLocalSock,  uRet, uExpectRet, fAttri2, fAccessSpeed2);
		}
		else{
			 if((!uRet) &&(uExpectRet==CERR_BAD_ARGS)){
			  	g_pKato->Log(LOG_FAIL,
      		  			(LPTSTR)TEXT("  Thread %u Socket %u: CardModifyWindow() FAIL:  uRet=0x%lX   uExpectRet=0x%lX fAttr=0x%lx, fSpeed=0x%lx\r\n"),
					dwThreadID, uLocalSock,  uRet, uExpectRet, fAttri2, fAccessSpeed2);
			 }
			else{
	   		 	 g_pKato->Log(LOG_FAIL,
      		 			 (LPTSTR)TEXT("  Thread %u Socket %u: CardModifyWindow() FAIL:  uRet=0x%lX   uExpectRet=0x%lX fAttr=0x%lx, fSpeed=0x%lx\r\n"),
			 		dwThreadID, uLocalSock,  uRet, uExpectRet, fAttri2, fAccessSpeed2);
			 }
			SetResult(FALSE);
		}
	}
	else{
	   	g_pKato->Log(LOG_DETAIL,
		      	(LPTSTR)TEXT("  Thread %u Socket %u: CardModifyWindow() PASS:  uRet=0x%lX   uExpectRet=0x%lX fAttr=0x%lx, fSpeed=0x%lx\r\n"),
					dwThreadID, uLocalSock,  uRet, uExpectRet, fAttri2, fAccessSpeed2);
	}


LReleaseWindow:
	uRet = CardReleaseWindow(hWindow);

	if (uRet){  // Failed
		if (!fReleaseWindowDone && !fDeregisterClientDone){
	   		g_pKato->Log(LOG_FAIL,
		      		(LPTSTR)TEXT("  Thread %u Socket %u: CardReleaseWindow() FAILed: uRet=0x%lx  fReleaseWindowDone=%d fDeregisterClientDone=%d\r\n"),
						dwThreadID, uLocalSock,  uRet, fReleaseWindowDone, fDeregisterClientDone);
			SetResult(FALSE);
		}
	}
	else{
		if (fReleaseWindowDone || fDeregisterClientDone){
	   		g_pKato->Log(LOG_FAIL,
	      			(LPTSTR)TEXT("  Thread %u Socket %u: CardReleaseWindow() Should have Failed: but uRet=0x%lx:  fReleaseWindowDone=%d fDeregisterClientDone=%d\r\n"),
						dwThreadID, uLocalSock,  uRet, fReleaseWindowDone, fDeregisterClientDone);
			SetResult(FALSE);
		}
	}



LDeregisterClient:

	uRet = CardDeregisterClient(hClient);

	if (uRet)	{					// fail
		if (!fDeregisterClientDone){
	   		g_pKato->Log(LOG_FAIL,
	      			(LPTSTR)TEXT("  Thread %u Socket %u: CardDeregisterClient() FAILed: uRet=0x%lx  fDeregisterClient =FALSE\r\n"),
				dwThreadID, uLocalSock,  uRet);
			SetResult(FALSE);
		}
	}
	else{
		if (fDeregisterClientDone){
			// hClient has been Deregistered
   			g_pKato->Log(LOG_FAIL,
      				(LPTSTR)TEXT("  Thread %u Socket %u: CardDeregisterClient() FAILed: uRet=0x%lx  fDeregisterClient=TRUE\r\n"),
				dwThreadID, uLocalSock,  uRet);
			SetResult(FALSE);
		}
	}

	CloseHandle( pData->hEvent);
	pData->hEvent = NULL;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- WinTest::ThreadRun()\r\n")));

	return GetLastError();
}


//defines for Test_SpecialCase() API
#define	WINTST_MAX_WINSIZE	0x4000000    // 64 MB


VOID WinTest::Test_SpecialCase (){
	CARD_CLIENT_HANDLE   hClient = NULL;
	CARD_WINDOW_HANDLE   hWindow = NULL;
	BOOL	  fReleaseWindowDone    = FALSE;
	PVOID   pWin;
	UINT    uRet;
	DWORD   dwError;
	UINT32  uCardAddrTemp, uTotalSpace, uGranularity;
	UINT    i, uLoop;
	PCLIENT_CONTEXT pData =   &(client_data);

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ WinTest::Test_SpecialCase() enterted\r\n")));

	// register for memory window
	if (!RegisterClientAndWindow(CLIENT_ATTR_MEM_DRIVER|CLIENT_ATTR_NOTIFY_SHARED,
			&hClient, &hWindow, pData)){
		SetResult(FALSE);
		return;
	}
	
	// Account for the case when we justifiably run out of resources
	// which means that above function returned TRUE, but
	// both handles are NULL.
	// We skip the test, returning OK.
	if ( (NULL==hClient) && (NULL==hWindow) ){
	   	g_pKato->Log(LOG_DETAIL,
	     		(LPTSTR)TEXT("  Thread %u Socket %u:  Test_SpecialCase():  not enough resources\r\n"));
		return;
	}

	//===========================================================================
	//
	//  Now test CardMapWindow()
	//
	//===========================================================================

	uTotalSpace = WINTST_MAX_WINSIZE;    // 64 MB

	uCardAddrTemp = uCardAddr ;   // uCardAddress = 0

	// 64 Mb % 32 K  =  0x800  ( == 2048 decimal)
	uLoop = uTotalSpace / uSize ;

   	g_pKato->Log(LOG_DETAIL,
     		(LPTSTR)TEXT("  Thread %u Socket %u: uTotalSpace=0x%lx uSize=0x%lx  uLoop=%lu\r\n"),
		dwThreadID, uLocalSock,  uTotalSpace, uSize, uLoop);


	for (i = 0; i <= uLoop; i++){
	    	pWin = CardMapWindow(hWindow, uCardAddrTemp , uSize, &uGranularity);
		if (!pWin){
			dwError = GetLastError();
			g_pKato->Log(LOG_DETAIL,
				(LPTSTR)TEXT("  Thread %u Socket %u: GetLastError() return = 0x%lx\r\n"),
				dwThreadID, uLocalSock,  dwError);

			goto LReleaseWindow;
		}
		else{
			if ((i % 100) == 0){
	   			g_pKato->Log(LOG_DETAIL,
  					(LPTSTR)TEXT("CardMapWindow()    PASS: i=%d  uCardAddr=0x%lx, uSize=0x%lx\r\n"),
			 		i, uCardAddrTemp, uSize);
			}
		}

		uCardAddrTemp  +=  uSize ;

	   	uRet = CardModifyWindow(hWindow, 0x0, WIN_SPEED_USE_WAIT);
		if (uRet){
	   		g_pKato->Log(LOG_FAIL,
	      			(LPTSTR)TEXT("CardModifyWindow() FAIL: i=%d  fAttri=0x%lx Speed=0x%x : err=%ld\r\n"),
				i,  0x0, WIN_SPEED_USE_WAIT, GetLastError());
			SetResult( FALSE);
		}
		else{
			if ((i % 100) == 0){
	   			g_pKato->Log(LOG_DETAIL,
      					(LPTSTR)TEXT("CardModifyWindow() PASS: i=%d  fAttri=0x%lx Speed=0x%x\r\n"),
					i,  0x0, WIN_SPEED_USE_WAIT);
			}
		}
	}  // for Loop


	g_pKato->Log(LOG_DETAIL,
  		(LPTSTR)TEXT("CardMapWindow()    DONE: i=%d  uCardAddr=0x%lx, uSize=0x%lx\r\n"),
		i, uCardAddrTemp, uSize);
	g_pKato->Log(LOG_DETAIL,
   		(LPTSTR)TEXT("CardModifyWindow() DONE: i=%d  fAttri=0x%lx Speed=0x%x\r\n"),
		i,  0x0, WIN_SPEED_USE_WAIT);


LReleaseWindow:
	uRet = CardReleaseWindow(hWindow);

	if (uRet) { // Failed						
		if (!fReleaseWindowDone){
	   		g_pKato->Log(LOG_FAIL,
	      			(LPTSTR)TEXT("  Thread %u Socket %u: CardReleaseWindow() FAILed: uRet=0x%lx  fReleaseWindowDone=FALSE\r\n"),
				dwThreadID, uLocalSock,  uRet);
			SetResult(FALSE);
		}
	}
	else{
		if (fReleaseWindowDone){
   			g_pKato->Log(LOG_FAIL,
      				(LPTSTR)TEXT("  Thread %u Socket %u: CardReleaseWindow() SHOULD FAILed: uRet=0x%lx fReleaseWindowDone=TRUE\r\n"),
				(LPTSTR) pData->lpClient);
			SetResult( FALSE);
		}
	}


	uRet = CardDeregisterClient(hClient);
	if (uRet){						// faile
   		g_pKato->Log(LOG_FAIL,
      			(LPTSTR)TEXT("  Thread %u Socket %u: CardDeregisterClient() FAILed: uRet=0x%lx  fDeregisterClient =FALSE\r\n"),
			dwThreadID, uLocalSock,  uRet);
		SetResult(FALSE);
	}

	CloseHandle( pData->hEvent);
	pData->hEvent = NULL;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- WinTest::Test_SpecialCase()\r\n")));

}

VOID WinTest::Test_InvalidParas (){
	CARD_CLIENT_HANDLE   hClient = NULL;
	CARD_WINDOW_HANDLE   hWindow = NULL;
	PVOID   pWin;
	UINT    uRet;
	DWORD   dwError;
	PCLIENT_CONTEXT pData =   &(client_data);
	CARD_WINDOW_PARMS   cwParam;
	CARD_WINDOW_HANDLE	hTempWin = NULL;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ WinTest::Test_InvalidParas()\r\n")));

	if (!RegisterClientAndWindow(CLIENT_ATTR_MEM_DRIVER|CLIENT_ATTR_NOTIFY_SHARED,
			&hClient, &hWindow, pData)){
		SetResult(FALSE);
		return;
	}
	
	// FOR CODE REVIEW -- Justify returning OK for all cases.
	if ( (NULL==hClient) && (NULL==hWindow) )
		return;

	//===========================================================================
	//
	//  Now test CardRequestWindow()
	//
	//===========================================================================

	cwParam.hSocket.uSocket    = (UCHAR)uLocalSock;
	cwParam.hSocket.uFunction  = (UCHAR)uLocalFunc;
	cwParam.fAttributes  = fAttri;
	cwParam.uWindowSize  = uWindowSize;
	cwParam.fAccessSpeed = fAccessSpeed;

	//try null client
	SetLastError(0);
	hTempWin = CardRequestWindow(NULL, &cwParam);
	dwError = GetLastError();
	if(dwError != CERR_BAD_HANDLE){
		g_pKato->Log(LOG_FAIL,TEXT("CardRequestWindow: Thread %u for Socket %u: Null Client, expected: 0x%x, ret = 0x%x"),
				 dwThreadID, uLocalSock,  CERR_BAD_HANDLE,  dwError);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try null WinParam.
	SetLastError(0);
	hTempWin = CardRequestWindow(hClient, NULL);
	dwError = GetLastError();
	if(dwError == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardRequestWindow: Thread %u for Socket %u: Null WinParam, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}
	
	//try wrong socket number.
	cwParam.hSocket.uSocket = 3;
	SetLastError(0);
	hTempWin = CardRequestWindow(hClient, &cwParam);
	dwError = GetLastError();
	if(dwError == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardRequestWindow: Thread %u for Socket %u: wrong socket number,should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}
	
	//try wrong function number.
	cwParam.hSocket.uSocket    = (UCHAR)uLocalSock;
	cwParam.hSocket.uFunction  = 3;
	SetLastError(0);
	hTempWin = CardRequestWindow(hClient, &cwParam);
	dwError = GetLastError();
	if(dwError == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardRequestWindow: Thread %u for Socket %u: wrong function number, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	
	//===========================================================================
	//
	//  Now test CardModifyWindow()
	//
	//===========================================================================


	//try 0 winsize
	hTempWin = hWindow;
	SetLastError(0);
	pWin = CardMapWindow(hTempWin, uCardAddr, 0, (PUINT)&nPGranularity);
	dwError = GetLastError();
	if(dwError == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardMapWindow: Thread %u for Socket %u: 0 WinSize, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

CLEANUP:
	uRet = CardReleaseWindow(hWindow);
	if (uRet) {//failed 
		g_pKato->Log(LOG_FAIL,
	      			(LPTSTR)TEXT("  Thread %u Socket %u: CardReleaseWindow() FAILed: uRet=0x%lx "),
				dwThreadID, uLocalSock,  uRet);
		SetResult(FALSE);
	}

	uRet = CardDeregisterClient(hClient);
	if (uRet){// failed
   		g_pKato->Log(LOG_FAIL,
      			(LPTSTR)TEXT("  Thread %u Socket %u: CardDeregisterClient() FAILed: uRet=0x%lx "),
			dwThreadID, uLocalSock,  uRet);
		SetResult(FALSE);
	}

	CloseHandle( pData->hEvent);
	pData->hEvent = NULL;
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- WinTest::Test_InvalidParas()\r\n")));

}


VOID WinTest::Test_CollideWindow (){
	CARD_CLIENT_HANDLE   hClient = NULL, hClient1= NULL;
	CARD_WINDOW_HANDLE   hWindow = NULL, hWindow1 = NULL;
	PVOID   pWin;
	UINT    uRet;
	DWORD   dwError;
	PCLIENT_CONTEXT pData =   &(client_data);


	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ WinTest::Test_CollideWindow()\r\n")));

	if (!RegisterClientAndWindow(CLIENT_ATTR_MEM_DRIVER|CLIENT_ATTR_NOTIFY_SHARED,
			&hClient, &hWindow, pData)){
		SetResult(FALSE);
		return;
	}

	
	//register another client, and request the same window
	if (!RegisterClientAndWindow(CLIENT_ATTR_MEM_DRIVER|CLIENT_ATTR_NOTIFY_SHARED,
			&hClient1, &hWindow1, pData)){
		SetResult(FALSE);
		return;
	}

	//now try to map window for the first one, should return CERR_SUCCESS
	SetLastError(0);
	pWin = CardMapWindow(hWindow, uCardAddr, uSize, (PUINT)&nPGranularity);
	dwError = GetLastError();
	if(dwError != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardMapWindow: Thread %u for Socket %u: two client with same window, expected: 0x%x, ret = 0x%x"),
				 dwThreadID, uLocalSock,  CERR_SUCCESS,  dwError);
		SetResult(FALSE);

	}

	//now try to map window for the second one, should return CERR_IN_USE
	SetLastError(0);
	pWin = CardMapWindow(hWindow1, uCardAddr, uSize, (PUINT)&nPGranularity);
	dwError = GetLastError();
	if(dwError != CERR_IN_USE){
		g_pKato->Log(LOG_FAIL,TEXT("CardMapWindow: Thread %u for Socket %u: two client with same window, expected: 0x%x, ret = 0x%x"),
				 dwThreadID, uLocalSock,  CERR_IN_USE,  dwError);
		SetResult(FALSE);
	}

	uRet = CardReleaseWindow(hWindow);
	if (uRet) {//failed 
		g_pKato->Log(LOG_FAIL,
	      			(LPTSTR)TEXT("  Thread %u Socket %u: CardReleaseWindow() FAILed on first window: uRet=0x%lx "),
				dwThreadID, uLocalSock,  uRet);
		SetResult(FALSE);
	}

	uRet = CardReleaseWindow(hWindow1);
	if (uRet) {//failed 
		g_pKato->Log(LOG_FAIL,
	      			(LPTSTR)TEXT("  Thread %u Socket %u: CardReleaseWindow() FAILed on second window: uRet=0x%lx "),
				dwThreadID, uLocalSock,  uRet);
		SetResult(FALSE);
	}

	uRet = CardDeregisterClient(hClient);
	if (uRet){// failed
   		g_pKato->Log(LOG_FAIL,
      			(LPTSTR)TEXT("  Thread %u Socket %u: CardDeregisterClient() FAILed on first client: uRet=0x%lx "),
			dwThreadID, uLocalSock,  uRet);
		SetResult(FALSE);
	}
	
	uRet = CardDeregisterClient(hClient1);
	if (uRet){// failed
   		g_pKato->Log(LOG_FAIL,
      			(LPTSTR)TEXT("  Thread %u Socket %u: CardDeregisterClient() FAILed on second client: uRet=0x%lx "),
			dwThreadID, uLocalSock,  uRet);
		SetResult(FALSE);
	}

	CloseHandle( pData->hEvent);
	pData->hEvent = NULL;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ WinTest::Test_CollideWindow()\r\n")));

}

BOOL WinTest::RegisterClientAndWindow(UINT uClientAttri, CARD_CLIENT_HANDLE * phClient, CARD_WINDOW_HANDLE * phWindow, PCLIENT_CONTEXT pData ){
	DWORD               dwError, dwLogStatus;
	CARD_WINDOW_PARMS   cwParam;

	dwLogStatus = LOG_COMMENT;

	*phClient = MyRegisterClient(uClientAttri, pData);
	if (!*phClient){
   		g_pKato->Log(LOG_FAIL,
      			(LPTSTR)TEXT("Thread %u Socket %u: RegisterClientAndWindow: CardRegisterClient() FAIL: dwError=0x%lX\r\n"),
			dwThreadID, uLocalSock, GetLastError());
		return FALSE;
	}
	cwParam.hSocket.uSocket    = (UCHAR)uLocalSock;
	cwParam.hSocket.uFunction  = (UCHAR)uLocalFunc;

	cwParam.fAttributes  = fAttri;
	cwParam.uWindowSize  = uWindowSize;
	cwParam.fAccessSpeed = fAccessSpeed;

	SetLastError(0);
	*phWindow = CardRequestWindow(*phClient, &cwParam);

	if (*phWindow)
		return TRUE;

	// Fail
	dwError = GetLastError();
	if (dwError == CERR_OUT_OF_RESOURCE)   // the window size is too large
		dwLogStatus = LOG_DETAIL;
	else{
		g_pKato->Log(dwLogStatus,
			(LPTSTR)TEXT("Thread %u Socket %u:  RegisterClientAndWindow: CardRequestWindow() FAIL:  uWindowSize=0x%lX  dwError=0x%lX\r\n"),
			dwThreadID, uLocalSock, cwParam.uWindowSize, dwError);
		dwLogStatus = LOG_FAIL;
	}
		

	CardDeregisterClient(*phClient);
   	*phClient=NULL;

	if (dwLogStatus == LOG_DETAIL)
 		return TRUE;
 	else
 		return FALSE;			
}

CARD_CLIENT_HANDLE WinTest::MyRegisterClient(UINT uClientAttri, PCLIENT_CONTEXT pData ){
   	CARD_CLIENT_HANDLE  hClient = NULL;
	CARD_REGISTER_PARMS crParam;

	pData->hEvent = CreateEvent(  0,  TRUE, FALSE, NULL);
	if (pData->hEvent == NULL){
   		g_pKato->Log(LOG_FAIL,
      			(LPTSTR)TEXT("Thread %u Socket %u:MyRegisterClient(0: CreateEvent() Failed: err=%ld\r\n"), 
      			dwThreadID, uLocalSock, GetLastError());
		return NULL;
	}

   	crParam.fAttributes  = uClientAttri;
	crParam.fEventMask   = 0xFFFF;
	crParam.uClientData = (UINT32) pData;

	hClient = CardRegisterClient(CallBackFn_Client, &crParam);

	WaitForSingleObject( pData->hEvent,  INFINITE);
	if (!hClient){
  	 	g_pKato->Log(LOG_FAIL,
      			(LPTSTR)TEXT("Thread %u Socket %u: CardRegisterClient() FAILed: dwError=%ld\r\n"),
      			dwThreadID, uLocalSock, GetLastError());
	}
	pData->hClient = hClient;
   	CloseHandle( pData->hEvent);
	return hClient;
}

