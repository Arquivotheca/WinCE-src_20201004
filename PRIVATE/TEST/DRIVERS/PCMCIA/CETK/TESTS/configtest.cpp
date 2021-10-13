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
#include "ConfigTest.h"

BOOL
ConfigTest::Init(){
	LPCONFIGTESTPARAMS  lparm = &configpara[dwCaseID-1];

	//initialize test parameters
	nTestFunc	= lparm->nTestFunc;
	fMemory		= lparm->fMemory  ;
	uCallRR       	= lparm->uCallRR       ;
	nHCLIENT      	= lparm->nHCLIENT      ;
	nConfigInfo	= lparm->nCONFIGINFO   ;
	fAttrib    		= lparm->fAttrib    	 ;
	fInterfaceType	= lparm->fInterfaceType;
	uVcc          	= (UCHAR)g_Vcc          ;
	uVpp1        	= lparm->uVpp1         ;
	uVpp2         	= lparm->uVpp2         ;
	fRegisters    	= lparm->fRegisters    ;
	uConfigReg    	= lparm->uConfigReg    ;
	uStatusReg    = lparm->uStatusReg    ;
	uPinReg       	= lparm->uPinReg       ;
	uCopyReg      = lparm->uCopyReg      ;
	uExtStatus    	= lparm->uExtStatus    ;
	uExpectRet	= lparm->uExpectRet	 ;
	nHCLIENT2	= lparm->nHCLIENT2	;
	uExpectRet2	= lparm->uExpectRet2   ;
	nIRQStream   	= lparm->nIRQStream    ;
	iByteStream 	= 0;
	IsrLogIndex 	= 0;
	IsrPrintIndex 	= 0;

	ASSERT(nIRQStream < MAX_MY_STREAM);
	
	//initialize some status flags that related with test progress 
	fDeregisterClientDone = FALSE;
	fReleaseWindowDone = FALSE;
	fReleaseIRQDone = FALSE;
	fReleaseExclDone = FALSE;
	
	//setup client data
	client_data.hClient= NULL;
	client_data.hEvent = NULL;
	client_data.pWin   = NULL;
	client_data.uGranularity     = 0;	  // for Manaul test use
	client_data.fCardReqExclDone = 0;
	client_data.uCardReqStatus = 0xFF;	// undefined value
	client_data.ThreadNo = dwThreadID;

	memset((LPVOID)client_data.rgByteStream, 0, 64);
  	client_data.uStreamLen =  rgStreamType[nIRQStream].uStreamLen;
	memcpy(&(client_data.rgByteStream[0]), rgStreamType[nIRQStream].rgByteCmd, client_data.uStreamLen);

	return TRUE;
}

//defines used by ThreadRun()
#define CONFIGTST_WINSIZE	0x100

DWORD ConfigTest::ThreadRun() {

	CARD_REGISTER_PARMS	crParam;
	CARD_WINDOW_PARMS    	cwParam;
	CARD_CLIENT_HANDLE   	hClient = NULL;
	CARD_WINDOW_HANDLE   	hWindow = NULL;
  	CARD_SOCKET_HANDLE   	hSocket;
	UINT                 			uRet;
	DWORD                		dwError, dw, dwLogStatus;
	CARD_CONFIG_INFO     		confInfo;
	PCLIENT_CONTEXT 		pData = &client_data;
	BOOL					f = TRUE;
	

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::ThreadRun() enterted\r\n")));
   	
	//create event:
	pData->hEvent = CreateEvent(0L,  FALSE,  FALSE /*fInitState*/, NULL);
	if (NULL == pData->hEvent){
   		g_pKato->Log(LOG_DETAIL,TEXT("***Thead %u for Socket %u: CreateEvent() FAILed:\r\n"), dwThreadID, uLocalSock);
		dwError = GetLastError();
		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: GetLastError() return = 0x%lx\r\n"), dwThreadID, uLocalFunc, dwError);
		SetResult(FALSE);
		return dwError;
	}


	if (fMemory )
		crParam.fAttributes = CLIENT_ATTR_MEM_DRIVER;
	else
		crParam.fAttributes = CLIENT_ATTR_IO_DRIVER;

	crParam.fAttributes |= (CLIENT_ATTR_NOTIFY_SHARED|CLIENT_ATTR_NOTIFY_EXCLUSIVE) ;
	crParam.fEventMask   = 0xFFFF;
	crParam.uClientData = (UINT32) pData;

	hSocket.uSocket = (UCHAR)uLocalSock;
	hSocket.uFunction = (UCHAR)uLocalFunc;
	
	//now register client
	SetLastError(0);
	switch(dwCaseID){
		case 32: //null params
			hClient = CardRegisterClient(CallBackFn_Client, NULL);
			if(hClient != NULL){
				g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRegisterClient() should fail!"), dwThreadID, uLocalSock);
				SetResult(FALSE);
			}
			goto LExit;
		case 33: //invalid params
			crParam.fAttributes = 0;
			hClient = CardRegisterClient(CallBackFn_Client, &crParam);
			if(hClient != NULL){
				g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRegisterClient() should fail!"), dwThreadID, uLocalSock);
				SetResult(FALSE);
			}
			goto LExit;
		default:	
			hClient = CardRegisterClient(CallBackFn_Client, &crParam);
			break;
	}
	
   	dw = WaitForSingleObject( pData->hEvent, TEST_WAIT_TIME);
	if (!hClient || dw == WAIT_TIMEOUT){
   		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRegisterClient() FAILed\r\n"), dwThreadID, uLocalSock);
		dwError = GetLastError();
   		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: GetLastError() return =0x%lx\r\n"), dwThreadID, uLocalSock, dwError);
		SetResult(FALSE);
		goto LDeregisterClient;
	}
	pData->hClient = hClient;

	//reset function
	CardResetFunction(hClient, hSocket);
//	Sleep (3000);

	cwParam.hSocket.uSocket   = (UCHAR)uLocalSock  ;
	cwParam.hSocket.uFunction = (UCHAR)uLocalFunc;

	if (fMemory)
		cwParam.fAttributes  = 0;    // Memory Window
	else
		cwParam.fAttributes  = WIN_ATTR_IO_SPACE;   //0x01

	cwParam.uWindowSize = CONFIGTST_WINSIZE;
	cwParam.fAccessSpeed= WIN_SPEED_USE_WAIT;	

	//request window 
	SetLastError(0);
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

	//request exclusive access
 	SetLastError(0);
 	uRet =  CardRequestExclusive(hClient, hSocket);
	dw = WaitForSingleObject( pData->hEvent, TEST_WAIT_TIME);
	if (uRet != CERR_SUCCESS || dw == WAIT_TIMEOUT){
		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRequestExclusive() FAILed:\r\n"), dwThreadID, uLocalSock);
		dwError = GetLastError();
		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: GetLastError() return = 0x%lx\r\n"),
					dwThreadID, uLocalSock, dwError);
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	if (pData->uCardReqStatus != uRet) {//request exclusive failed
		if (dwTotalThread > 1){//if it is multi-threaded test, then this is acceptable 
			dwLogStatus = LOG_DETAIL;
		}
		else{//othrewise it indicates a real failure
			dwLogStatus = LOG_FAIL;
			SetResult(FALSE);
		}

   		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRequestExclusive() FAILed:  uCardReqStaus (0x%lx) != uRet (0x%lx)\r\n"),
						dwThreadID, uLocalSock, pData->uCardReqStatus, uRet);
	}

	//===========================================================================
	//
	//  Now test CardRequest/ReleaseConfiguration()
	//
	//===========================================================================


	confInfo.hSocket.uSocket        	= hSocket.uSocket   ;
	confInfo.hSocket.uFunction      	= hSocket.uFunction ;
	confInfo.fAttributes    			= fAttrib   ;
	confInfo.fInterfaceType			= fInterfaceType;
	confInfo.uVcc           			= uVcc      ;
	confInfo.uVpp1          			= uVpp1     ;
	confInfo.uVpp2          			= uVpp2     ;
	confInfo.fRegisters     			= fRegisters;
	confInfo.uConfigReg     			= uConfigReg & 0x7F;
	confInfo.uStatusReg     		= uStatusReg;
	confInfo.uPinReg        			= uPinReg   ;
	confInfo.uCopyReg       		= uCopyReg  ;
	confInfo.uExtendedStatus		= uExtStatus;


	switch (nTestFunc){//we can have different function call orders
		default:
			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: nTestFunc =%d : Don't know how to handle.\r\n"),
							dwThreadID, uLocalSock, nTestFunc);
			break;

 		case 0x1:
		case 0x101:
			f = Test_Sub1(pData, hClient, hWindow, &confInfo);
			break;

		case 0x2:
		case 0x102:
			f = Test_Sub2(pData, hClient, hWindow, &confInfo);
			break;

		case 0x5:
		case 0x105:
			f = Test_AccessRegister(pData, hClient, hWindow, &confInfo);
			break;

		case 0x201:
			f = Test_CardModifyConfig(pData, hClient, hWindow, &confInfo);
			break;

		case 0x301:
			f = Test_CardPowerOnOff(pData, hClient, hWindow, &confInfo);
			break;


	}

	if(f == FALSE)
		SetResult(FALSE);
	
//================================================================
//
// Clean up
//
//================================================================


	uRet =  CardReleaseExclusive(hClient, hSocket);
	if (uRet) { // Failed
		if (!fReleaseExclDone){
			if (dwTotalThread > 1)
				dwLogStatus = LOG_DETAIL;
			else{
				dwLogStatus = LOG_FAIL;
				SetResult(FALSE);
			}
   			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardReleaseExclusive() FAILed: uRet=0x%lx: uSocket=%d\n"),
										dwThreadID, uLocalSock,   uRet, hSocket.uSocket);
		}
	}

LReleaseWindow:

	uRet = CardReleaseWindow(hWindow);
	if (uRet){  // Failed
		if (!fReleaseWindowDone){//failured may caused by some un-released resources
			if (!(uCallRR & CALL_RELEASE_CONF) || !(uCallRR & CALL_RELEASE_IRQ)){
				if (!(uCallRR & CALL_RELEASE_CONF)){//release config now
					uRet =  CardReleaseConfiguration(hClient, hSocket);
					if (uRet) { // Failed
   						g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardReleaseConfiguration() FAILed: uRet=0x%lx\r\n"),
												dwThreadID, uLocalSock,   uRet);
						SetResult(FALSE);
					}
				}

				if (!(uCallRR & CALL_RELEASE_IRQ)){//release IRQ now
					uRet = CardReleaseIRQ(hClient, hSocket);
					if (uRet){  // FAIL
						if (dwTotalThread > 1)
							dwLogStatus = LOG_DETAIL;
						else{
							dwLogStatus = LOG_FAIL;
							SetResult(FALSE);
						}

   						g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardReleaseIRQ() FAILed: uRet=0x%lx\r\n"),
												dwThreadID, uLocalSock,   uRet);
						}
					}

				goto LReleaseWindow;
			}
			else{//otherwise it's a real failure
  				g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardReleaseWindow() FAILed: uRet=0x%lx\r\n"),
										dwThreadID, uLocalSock,   uRet);
				SetResult(FALSE);
			}
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
LExit:
	//free event
	CloseHandle( pData->hEvent);
	pData->hEvent = NULL;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- ConfigTest::ThreadRun() \r\n")));

	return GetLastError();
}



//----------------------------------------------------------------------------
// Function:	Test_Sub1
//
// calling order:   Call
//
// CardRequestConfiguration()
// CardMapWindow()
// CardRequestIRQ()
//
//----------------------------------------------------------------------------
BOOL ConfigTest::Test_Sub1(
	PCLIENT_CONTEXT      pData,
	CARD_CLIENT_HANDLE   hClient,
	CARD_WINDOW_HANDLE   hWindow,
	PCARD_CONFIG_INFO    pconfInfo)
{
	BOOL                 			f = TRUE;
	CARD_CLIENT_HANDLE   	hClientTest;
	CARD_SOCKET_HANDLE   	hSocket;
	PVOID                			pWin;
	UINT32               			uCardAddress, uGranularity;
	STATUS               		uRet;
	STATUS               		uRetReqConf = 0xFF;  // initilize to 0xFF: undifined
	DWORD                		dwError, dwLogStatus;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_Sub1() enterted\r\n")));

	//test require ignoring conifg request
	if (!(uCallRR & CALL_REQUEST_CONF))
		goto LMapWindow;


	switch (nHCLIENT){
		case 0://deliberately set client handle to 0  
			hClientTest = 0;        
			break;
		case 1://normal test  
			hClientTest = hClient;  
			break;
		case 4://using invalid socket number
			hClientTest = hClient;
			(pconfInfo->hSocket).uSocket        	= UNDEFINED_VALUE;
			(pconfInfo->hSocket).uFunction      	= UNDEFINED_VALUE;
			break;
		default:
			g_pKato->Log(LOG_DETAIL,TEXT("Sub1: Thread %u for Socket %u: nHCLIENT =%d : Don't know how to handle.\r\n"),
									dwThreadID, uLocalSock,   nHCLIENT);
	   		return FALSE;
	}

	//config request
	uRetReqConf =  CardRequestConfiguration(hClientTest, nConfigInfo == 1 ? pconfInfo : NULL);

	if (nConfigInfo == 1)
		 g_pKato->Log(LOG_DETAIL,TEXT("Sub1: Thread %u for Socket %u: CardRequestConfig(): uRet=0x%lx: uSocket=%d uFunction=%d \r\n"),
				 dwThreadID, uLocalSock,   uRetReqConf, pconfInfo->hSocket.uSocket, pconfInfo->hSocket.uFunction);
	else
		 g_pKato->Log(LOG_DETAIL,TEXT("Sub1: Thread %u for Socket %u: CardRequestConfig(): uRet=0x%lx:   NOT passing CONFIGINFO\r\n"),
				 dwThreadID, uLocalSock,   uRetReqConf);


	if ( uRetReqConf != uExpectRet){//we should get the expected result
		if (dwTotalThread > 1){//Multi-thread tests
			if (uRetReqConf == CERR_IN_USE){//someone else is using it, acceptable
				dwLogStatus = LOG_DETAIL;
			}
			else{//otherwise this is a true failure
				f= FALSE;
				dwLogStatus = LOG_FAIL;
			}

   			g_pKato->Log(dwLogStatus,TEXT("Sub1: Thread %u for Socket %u:  CardRequestConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
									  dwThreadID, uLocalSock,   uExpectRet, uRetReqConf);
		}
		else{//failure.
   			g_pKato->Log(LOG_DETAIL,TEXT("Sub1: Thread %u for Socket %u: 1st: CardRequestConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
										dwThreadID, uLocalSock,   uExpectRet, uRetReqConf);
		}
	}

	if (dwTotalThread > 1 && uRetReqConf != CERR_SUCCESS){
		// Don't compet with other thread which has requested configuration
		return f;
	}
	
	if (dwTotalThread == 1){//if single thread test
		// Call it again:
		uRetReqConf =  CardRequestConfiguration(hClientTest, nConfigInfo == 1 ? pconfInfo : NULL);
		if ( uRetReqConf != uExpectRet){//failed
			f = FALSE;
   			g_pKato->Log(LOG_DETAIL,TEXT("Sub1: Thread %u for Socket %u: 2nd: CardRequestConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
									dwThreadID, uLocalSock,   uExpectRet, uRetReqConf);
		}
	}


LMapWindow:	

	switch ((uConfigReg & 0x3F))  {  // UINT 8: uConfig : lower 6 bits: configguratin #
		default:
   			g_pKato->Log(LOG_DETAIL,TEXT("Sub1: Thread %u for Socket %u: uConfigReg=%d:  Don't know how to handle\r\n"),
									 dwThreadID, uLocalSock,   uConfigReg);
			uCardAddress  = 0x3F8;//use default
			break;
		case 32:
			uCardAddress  = 0x3F8;
			break;
		case 33:
			uCardAddress  = 0x2F8;
			break;
		case 34:
			uCardAddress  = 0x3E8;
			break;
		case 35:
			uCardAddress  = 0x2E8;
			break;
	}

	pWin = CardMapWindow(hWindow, uCardAddress, 7, &uGranularity);
	if (pWin == NULL){
		dwError = GetLastError();
		if(dwError != CERR_SUCCESS){			
			if (dwError == CERR_IN_USE){
				//This is normally caused by mapping to a already-mapped address, or I/O address is not avalialbe
				//in this system. So it will be ignored.
				g_pKato->Log(LOG_DETAIL,TEXT("Sub1: Thread %u for Socket %u: CardMapWindow() PASSed: uAddr =0x%lx uLen=7 uGranulary=%d\r\n"), 
										dwThreadID, uLocalSock,   uCardAddress, uGranularity);
			}
			else{
				f = FALSE;
				g_pKato->Log(LOG_DETAIL,TEXT("Sub1: THread %u: CardMapWindow() FAILed: dwError=0x%lx\r\n"), 
										dwThreadID, uLocalSock,   dwError);
			}
		}
	}

	hSocket.uSocket = pconfInfo->hSocket.uSocket;
	hSocket.uFunction = pconfInfo->hSocket.uFunction;

	if (!(uCallRR & CALL_REQUEST_IRQ))
		goto LReleaseConfig;

	// REVIEW: last paramter:  uContextdata ???: current use 0
	uRet = CardRequestIRQ(hClient, hSocket, ConfISRFunction, (UINT32) pData);

	if (uRet){  // FAIL
   		g_pKato->Log(LOG_DETAIL, TEXT("Sub1: thread %u: CardRequestIRQ() FAILed: uRet=0x%lx\r\n"),
								 dwThreadID, uLocalSock,   uRet);
		if(dwTotalThread == 1)
			f = FALSE;
	}

//	Sleep(1000);

LReleaseConfig:

	//==========================================================================
	//
	//	 Now clean up:
	//
	//==========================================================================

	if (!(uCallRR & CALL_RELEASE_CONF))
		goto LReleaseIRQ;

	f &=  SubTestReleaseConfig(pData, hClient, hWindow, hSocket);

LReleaseIRQ:
	//
	// Case : test not call CardReleaseIRQ()
	if (!(uCallRR & CALL_RELEASE_IRQ))
		goto LReturn;

	uRet = CardReleaseIRQ(hClient, hSocket);
	if (uRet) { // FAIL
		if (!fReleaseIRQDone){//this is the first release, should success
			//multi-threaded tests or if IRQ service was not requested, then this failure can be ignored
			if (dwTotalThread > 1 || !(uCallRR & CALL_REQUEST_IRQ) ){
				dwLogStatus = LOG_DETAIL;
			}
			else{
				dwLogStatus = LOG_FAIL;
				f = FALSE;
			}

   			g_pKato->Log(LOG_DETAIL,TEXT("Sub1: Thread %u for Socket %u: CardReleaseIRQ() FAILed: uRet=0x%lx: %s\r\n"),
					dwThreadID, uLocalSock,   uRet,   (LPTSTR)(dwLogStatus == LOG_DETAIL ? TEXT("Expected fail") : TEXT("Error!") ));
		}
	}

LReturn:

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- ConfigTest::Test_Sub1() enterted\r\n")));

	return f;
}


//----------------------------------------------------------------------------
// Function:	Test_Sub2
//
// calling order:
//
// CardMapWindow()
// CardRequestIRQ()
// Call CardRequestConfiguration()
//
//----------------------------------------------------------------------------
BOOL ConfigTest ::Test_Sub2(
	PCLIENT_CONTEXT      pData,
	CARD_CLIENT_HANDLE   hClient,
	CARD_WINDOW_HANDLE   hWindow,
	PCARD_CONFIG_INFO    pconfInfo)
{
	BOOL                 		f = TRUE;
	CARD_CLIENT_HANDLE  hClientTest;
	CARD_SOCKET_HANDLE hSocket;
	PVOID                		pWin;
	UINT32               		uCardAddress, uGranularity;
	STATUS               	uRet = 0xFF;
	STATUS               	uRetReqConf = 0xFF;  // initilize to 0xFF: undifined
	DWORD                	dwError, dwLogStatus;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_Sub2() enterted\r\n")));
   	
	switch (uConfigReg )  {  // UINT 8: uConfig : lower 6 bits: configguratin #
		default:
   			g_pKato->Log(LOG_DETAIL,TEXT("Sub2: Thread %u for Socket %u: uConfigReg=%d:  Don't know how to handle\r\n"),
									 dwThreadID, uLocalSock,   uConfigReg);
			uCardAddress  = 0x3F8;//use default
			break;
		case 55:	  // arbitrary number
			uCardAddress  = 0x555;
			break;
		case 32:
			uCardAddress  = 0x3F8;
			break;
		case 33:
			uCardAddress  = 0x2F8;
			break;
		case 34:
			uCardAddress  = 0x3E8;
			break;
		case 35:
			uCardAddress  = 0x2E8;
			break;
	}

	pWin = CardMapWindow(hWindow, uCardAddress, 7, &uGranularity);
	if (pWin == NULL){
		dwError = GetLastError();
		if(dwError != CERR_SUCCESS){
			if (dwError == CERR_IN_USE){
				//This is normally caused by mapping to a already-mapped address, or I/O address is not avalialbe
				//in this system. So it will be ignored.
				g_pKato->Log(LOG_DETAIL,TEXT("Sub2: Thread %u for Socket %u: CardMapWindow() PASSed: uAddr =0x%lx uLen=7 uGranulary=%d\r\n"), 
										dwThreadID, uLocalSock,   uCardAddress, uGranularity);
			}
			else{
				f = FALSE;
				g_pKato->Log(LOG_DETAIL,TEXT("Sub2: Thread %u for Socket %u: CardMapWindow() FAILed: dwError=0x%lx\r\n"), 
										dwThreadID, uLocalSock,   dwError);
			}
		}
	}

	//request IRQ
	pData->pWin = pWin;
	pData->uGranularity = uGranularity;
	hSocket.uSocket   = pconfInfo->hSocket.uSocket;
	hSocket.uFunction = pconfInfo->hSocket.uFunction;
	if (!(uCallRR & CALL_REQUEST_IRQ))
		goto LRequestConfig;


	// REVIEW: last paramter:  uContextdata ???: current use 0
	uRet = CardRequestIRQ(hClient, hSocket, ConfISRFunction, (UINT32) pData);
	if (uRet){  // FAIL
   		g_pKato->Log(LOG_DETAIL, TEXT("Sub2: thread %u: CardRequestIRQ() FAILed: uRet=0x%lx\r\n"),
								 dwThreadID, uLocalSock,   uRet);
		
		if(dwTotalThread == 1)
			f = FALSE;
	}

LRequestConfig:

	if (!(uCallRR & CALL_REQUEST_CONF))
		goto LReleaseConfig;


	switch (nHCLIENT){
		case 0: //null handle  
			hClientTest = 0;        
			break;
		case 1:  
			hClientTest = hClient;  
			break;
		case 2:  //hClient =  invalid handle
			hClientTest = hClient;
			CardReleaseIRQ(hClient, hSocket);
			CardReleaseExclusive(hClient, hSocket);
			CardReleaseWindow(hWindow);
			CardDeregisterClient(hClient);
			fReleaseIRQDone       = TRUE;
			fReleaseExclDone      = TRUE;
			fReleaseWindowDone    = TRUE;
			fDeregisterClientDone = TRUE;
			break;

		case 3:	 // hWidnow is invalid
			hClientTest = hClient;
			CardReleaseWindow(hWindow);
			fReleaseWindowDone = TRUE;
			break;

		case 4://using invalid socket number
			hClientTest = hClient;
			(pconfInfo->hSocket).uSocket        	= UNDEFINED_VALUE;
			(pconfInfo->hSocket).uFunction      	= UNDEFINED_VALUE;
			break;
		default:
			hClientTest = hClient;
			g_pKato->Log(LOG_DETAIL,TEXT("Sub2: Thread %u for Socket %u: nHCLIENT =%d : Don't know how to handle.\r\n"),
									dwThreadID, uLocalSock,   nHCLIENT);
	   		f = FALSE;
	}

	
	//config request
	uRetReqConf =  CardRequestConfiguration(hClientTest, nConfigInfo == 1 ? pconfInfo : NULL);

	if (nConfigInfo == 1)
		 g_pKato->Log(LOG_DETAIL,TEXT("Sub2: Thread %u for Socket %u: CardRequestConfig(): uRet=0x%lx: uSocket=%d uFunction=%d \r\n"),
				 dwThreadID, uLocalSock,   uRetReqConf, pconfInfo->hSocket.uSocket, pconfInfo->hSocket.uFunction);
	else
		 g_pKato->Log(LOG_DETAIL,TEXT("Sub2: Thread %u for Socket %u: CardRequestConfig(): uRet=0x%lx:   NOT passing CONFIGINFO\r\n"),
				 dwThreadID, uLocalSock,   uRetReqConf);



	if ( uRetReqConf != uExpectRet){ //failed
   		g_pKato->Log(LOG_DETAIL,TEXT("Sub2: Thread %u for Socket %u: 1st: CardRequestConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
										dwThreadID, uLocalSock,   uExpectRet, uRetReqConf);
		if (dwTotalThread == 1)//single thread test, this is a true failure
			f = FALSE;
	}

	if (dwTotalThread == 1){// Call it again
		uRetReqConf =  CardRequestConfiguration(hClientTest, nConfigInfo== 1 ? pconfInfo : NULL);
		if ( uRetReqConf != uExpectRet){//Failed
   			g_pKato->Log(LOG_DETAIL,TEXT("Sub2: Thread %u for Socket %u: 2nd: CardRequestConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
									dwThreadID, uLocalSock,   uExpectRet, uRetReqConf);
			f = FALSE;
		}
	}

	if ( uRetReqConf != CERR_SUCCESS)
		goto LReleaseIRQ;

//	Sleep(1000);


LReleaseConfig:
	//==========================================================================
	//
	//	 Now clean up:
	//
	//==========================================================================

	if (!(uCallRR & CALL_RELEASE_CONF))
		goto LReleaseIRQ;


	f &= SubTestReleaseConfig(pData, hClient, hWindow, hSocket);


LReleaseIRQ:
	//
	// Case : test not call CardReleaseIRQ()
	if (!(uCallRR & CALL_RELEASE_IRQ))
		goto LReturn;

	uRet = CardReleaseIRQ(hClient, hSocket);
	if (uRet) { // FAIL
		if (!fReleaseIRQDone){//this is the first release, should success
			//multi-threaded tests or if IRQ service was not requested, then this failure can be ignored
			if (dwTotalThread > 1 || !(uCallRR & CALL_REQUEST_IRQ) ){
				dwLogStatus = LOG_DETAIL;
			}
			else{
				dwLogStatus = LOG_FAIL;
				f = FALSE;
			}

   			g_pKato->Log(LOG_DETAIL,TEXT("Sub2: Thread %u for Socket %u: CardReleaseIRQ() FAILed: uRet=0x%lx: %s\r\n"),
					dwThreadID, uLocalSock,   uRet,   (LPTSTR)(dwLogStatus == LOG_DETAIL ? TEXT("Expected fail") : TEXT("Error!") ));
		}
	}


LReturn:
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- ConfigTest::Test_Sub2() enterted\r\n")));

	return f;
}


BOOL ConfigTest::SubTestReleaseConfig(
	PCLIENT_CONTEXT    pData,
	HANDLE             hClient,
	CARD_WINDOW_HANDLE hWindow,
	CARD_SOCKET_HANDLE   hSocket
)
{
	BOOL   		f = TRUE;
	HANDLE 		hClientTest;
	STATUS    	uRet;
	DWORD     	dwLogStatus; 

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::SubTestReleaseConfig() enterted\r\n")));

	switch (nHCLIENT2){
		case 0:  //null handle
			hClientTest = 0;        
	             break;
		case 1:  
			hClientTest = hClient;  
			break;

		case 2:  //hClient =  invalid handle
			hClientTest = hClient;
			CardReleaseConfiguration(hClient, hSocket);
			CardReleaseIRQ(hClient, hSocket);
			CardReleaseExclusive(hClient, hSocket);
			CardReleaseWindow(hWindow);
			CardDeregisterClient(hClient);
			fReleaseIRQDone       = TRUE;
			fReleaseExclDone      = TRUE;
			fReleaseWindowDone    = TRUE;
			fDeregisterClientDone = TRUE;
			break;

		case 4://using invalid socket number
			hClientTest = hClient;
			hSocket.uSocket 		= UNDEFINED_VALUE;
			hSocket.uFunction      	= UNDEFINED_VALUE;
			break;
		default:
			g_pKato->Log(LOG_DETAIL,TEXT("Thread %u for Socket %u: nHCLIENT2 =%d : Don't know how to handle.\r\n"),
									dwThreadID, uLocalSock,   nHCLIENT2);
			f = FALSE;
			hClientTest = hClient;
	}

	//try releasing configuration
	uRet =  CardReleaseConfiguration(hClientTest, hSocket);
	
	if (uRet != uExpectRet2){
		if (dwTotalThread > 1){//multi-threaded situation
			if (uRet == CERR_IN_USE)//someone else is using it, that's acceptable
				dwLogStatus = LOG_DETAIL;
			else{
				f = FALSE;
				dwLogStatus = LOG_FAIL;
			}
   			g_pKato->Log(LOG_DETAIL,TEXT("Thread %u for Socket %u: In_Thread: CardReleaseConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
									dwThreadID, uLocalSock,   uExpectRet2, uRet);
			}
		else{//single-threaded situation, true failure
			dwLogStatus = LOG_FAIL;
			f = FALSE;
   			g_pKato->Log(LOG_DETAIL,TEXT("Thread %u for Socket %u: 1st: CardReleaseConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
									 dwThreadID, uLocalSock,   uExpectRet2, uRet);
		}
	}


	if (dwTotalThread == 1){
		// try again:	The second time should succeeds again
		uRet =  CardReleaseConfiguration(hClientTest, hSocket);
		if (uRet != uExpectRet2){
			f = FALSE;
   		 	g_pKato->Log(LOG_DETAIL,TEXT("%s: 2nd: CardReleaseConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
					(LPTSTR) pData->lpClient, uExpectRet2, uRet);
		}
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- ConfigTest::SubTestReleaseConfig() \r\n")));

	return f;
}

//defines used by Test_AccessRegister
#define	UNDEFINED_HANDEL_VAL		0xffff
#define	CONFIGTST_AR_TRY			18
BOOL ConfigTest::Test_AccessRegister(
	PCLIENT_CONTEXT      pData,
	CARD_CLIENT_HANDLE   hClient,
	CARD_WINDOW_HANDLE   hWindow,
	PCARD_CONFIG_INFO    pconfInfo)
{
	BOOL                 f = TRUE, fRet = TRUE;
	CARD_CLIENT_HANDLE   hClientTest;
	CARD_SOCKET_HANDLE   hSocketTest;
	UINT8     rguValue[20], i;
	STATUS    uRet;
	BOOL	bValNull = FALSE;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_AccessRegister() enterted\r\n")));

	hSocketTest.uSocket   = (UCHAR)uLocalSock;
	hSocketTest.uFunction = (UCHAR)uLocalFunc;

	switch (nHCLIENT){
		case 0://deliberately set client handle to 0  
			hClientTest = NULL;        
			break;
		case 1://normal test  
			hClientTest = hClient;  
			break;
		case 2://deliberately set client handle to be a wrong value  
			hClientTest = (CARD_CLIENT_HANDLE)UNDEFINED_HANDEL_VAL;        
			NKDbgPrintfW(_T("hClient = 0x%x, fake hClientTest = 0x%x"), hClient, hClientTest);
			break;
		case 3://null pvalue
			bValNull = TRUE;
		case 4://using invalid socket number
			hClientTest = hClient;
			hSocketTest.uSocket        	= UNDEFINED_VALUE;
			hSocketTest.uFunction      	= UNDEFINED_VALUE;
			break;
		default:
			g_pKato->Log(LOG_DETAIL,TEXT("Access Register: Thread %u for Socket %u: nHCLIENT =%d : Don't know how to handle.\r\n"),
									dwThreadID, uLocalSock,   nHCLIENT);
	   		return FALSE;
	}



//===============
// Read first
//===============

	for (i = 0; i < CONFIGTST_AR_TRY; i++){
		uRet = CardAccessConfigurationRegister(hClientTest, hSocketTest, CARD_FCR_READ, i, (bValNull == TRUE)?NULL:&rguValue[i]);
		if ( uRet != uExpectRet){
			//  multi-threaded or card dost not have this register
			if (dwTotalThread > 1){
	   			g_pKato->Log(LOG_DETAIL,TEXT("AccessRegister: Thread %u for Socket %u:  READ:  offset=%d: val=%d  uExpectRet(0x%lx) != uRet(0x%lx):  More threads\r\n"),
										dwThreadID, uLocalSock,   i, rguValue[i],  uExpectRet, uRet);
			}
		  	else if (uRet == CERR_BAD_OFFSET ){
	   			g_pKato->Log(LOG_DETAIL,TEXT("AccessRegister: Thread %u for Socket %u:  READ:  offset=%d:  uExpectRet(0x%lx) != uRet(0x%lx):  CERR_BAD_OFFSET\r\n"),
										dwThreadID, uLocalSock,   i, uExpectRet, uRet);
			}
			else{
				f = FALSE;
	   			g_pKato->Log(LOG_DETAIL,TEXT("AccessRegister: Thread %u for Socket %u:  READ:  offset=%d: val=%d  uExpectRet(0x%lx) != uRet(0x%lx)\r\n"),
										dwThreadID, uLocalSock,   i, rguValue[i], uExpectRet, uRet);
			}
		}
		else{
   			g_pKato->Log(LOG_DETAIL,TEXT("AccessRegister: Thread %u for Socket %u:  READ:  offset=%d: val=%d uExpectRet(0x%lx) == uRet(0x%lx)\r\n"),
									dwThreadID, uLocalSock,   i, rguValue[i], uExpectRet, uRet);
		}

		if (uRet == CERR_SUCCESS)
	 		g_pKato->Log(LOG_DETAIL,TEXT("READ:  offset=%d:  uValue=%x\r\n"), i, rguValue[i]);
	}

	fRet = f;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- ConfigTest::Test_AccessRegister()\r\n")));

	return fRet;
}




//----------------------------------------------------------------------------
// Function:	Test_CardModifyConfig()
//
// calling order:   Call
//
// CardRequestConfiguration()
// CardMapWindow()
// CardRequestIRQ()
//
//----------------------------------------------------------------------------
BOOL ConfigTest::Test_CardModifyConfig(
	PCLIENT_CONTEXT      pData,
	CARD_CLIENT_HANDLE   hClient,
	CARD_WINDOW_HANDLE   hWindow,
	PCARD_CONFIG_INFO    pconfInfo)
{
	BOOL                 			f = TRUE;
	CARD_SOCKET_HANDLE   	hSocket;
	STATUS               		uRet, uRetReqConf;
	DWORD                		dwLogStatus;
	UINT16					fLocalAttrib = 0;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_CardModifyConfig() enterted\r\n")));
	
	//request configuration first
	uRetReqConf =  CardRequestConfiguration(hClient, pconfInfo);

	 g_pKato->Log(LOG_DETAIL,TEXT("CardModifyConfig: Thread %u for Socket %u: CardRequestConfig(): uRet=0x%lx: uSocket=%d uFunction=%d \r\n"),
				 dwThreadID, uLocalSock,   uRetReqConf, pconfInfo->hSocket.uSocket, pconfInfo->hSocket.uFunction);

	if ( uRetReqConf != uExpectRet){//we should get the expected result
		if (dwTotalThread > 1){//Multi-thread tests
			if (uRetReqConf == CERR_IN_USE){//someone else is using it, acceptable
				dwLogStatus = LOG_DETAIL;
			}
			else{//otherwise this is a true failure
				f= FALSE;
				dwLogStatus = LOG_FAIL;
			}

   			g_pKato->Log(dwLogStatus,TEXT("CardModifyConfig: Thread %u for Socket %u:  CardRequestConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
									  dwThreadID, uLocalSock,   uExpectRet, uRetReqConf);
		}
		else{//failure.
   			g_pKato->Log(LOG_DETAIL,TEXT("CardModifyConfig: Thread %u for Socket %u: 1st: CardRequestConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
										dwThreadID, uLocalSock,   uExpectRet, uRetReqConf);
		}
	}


	hSocket.uSocket = pconfInfo->hSocket.uSocket;
	hSocket.uFunction = pconfInfo->hSocket.uFunction;

	switch(dwCaseID){
		case 34://set IRQWakeup
			fLocalAttrib |= CFG_ATTR_IRQ_WAKEUP;
			g_pKato->Log(LOG_DETAIL,TEXT("CardModifyConfig: Thread %u for Socket %u: uRet = 0x%x, Set attribute CFG_ATTR_IRQ_WAKEUP"),
				 		dwThreadID, uLocalSock);
			break;
		case 35://set KeepPowered
			fLocalAttrib |= CFG_ATTR_KEEP_POWERED;
			g_pKato->Log(LOG_DETAIL,TEXT("CardModifyConfig: Thread %u for Socket %u: uRet = 0x%x, Set attribute CFG_ATTR_KEEP_POWERED"),
				 		dwThreadID, uLocalSock);
			break;
		case 36://set no suspend unload
			fLocalAttrib |= CFG_ATTR_NO_SUSPEND_UNLOAD;
			g_pKato->Log(LOG_DETAIL,TEXT("CardModifyConfig: Thread %u for Socket %u: uRet = 0x%x, Set attribute CFG_ATTR_NO_SUSPEND_UNLOAD"),
				 		dwThreadID, uLocalSock);
			break;
		case 37://test cardModifyConfiguration()  with invalid input parameters
			f &= CardModifyConfig_InvalidParas(hClient, hSocket, fLocalAttrib);
			goto LReleaseConfig;
		default://don't care others
			break;
	}

	//set this attibute
	uRet = CardModifyConfiguration(hClient, hSocket, &fLocalAttrib);
	if(uRet != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardModifyConfig: Thread %u for Socket %u: uRet = 0x%x, CardModifyConfiguration: Set attribute Failed!"),
				 dwThreadID, uLocalSock,   uRet);
		f = FALSE;
		goto LReleaseConfig;
	}
	
	//unset this attibute
	fLocalAttrib = 0;
	uRet = CardModifyConfiguration(hClient, hSocket, &fLocalAttrib);
	if(uRet != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardModifyConfig: Thread %u for Socket %u: uRet = 0x%x, CardModifyConfiguration: Unset attribute Failed!"),
				 dwThreadID, uLocalSock,   uRet);
		f = FALSE;
	}

LReleaseConfig:

	//==========================================================================
	//
	//	 Now clean up:
	//
	//==========================================================================

	f &=  SubTestReleaseConfig(pData, hClient, hWindow, hSocket);

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- ConfigTest::Test_CardModifyConfig()\r\n")));

	return f;

}



//----------------------------------------------------------------------------
// Function:	CardModifyConfig_InvalidParas()
//----------------------------------------------------------------------------
BOOL ConfigTest::CardModifyConfig_InvalidParas(
	CARD_CLIENT_HANDLE   hClient,
	CARD_SOCKET_HANDLE	 hSocket,
	UINT16				 fAttr)
{
	UINT16					fLocalAttrib = fAttr;
	STATUS					uRet;
	CARD_SOCKET_HANDLE		hLocalSocket;
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::CardModifyConfig_InvalidParas() enterted\r\n")));


	//try null client
	uRet = CardModifyConfiguration(NULL, hSocket, &fLocalAttrib);
	if(uRet == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardModifyConfig_InvalidParas: Thread %u for Socket %u: CardModifyConfiguration with Null client!, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		return FALSE;
	}

	//try null fAttiribute pointer
	uRet = CardModifyConfiguration(hClient, hSocket, NULL);
	if(uRet == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardModifyConfig_InvalidParas: Thread %u for Socket %u: CardModifyConfiguration with Null attibutes!, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		return FALSE;
	}

	//try wrong socket number
	hLocalSocket.uFunction = hSocket.uFunction;
	hLocalSocket.uSocket = 3;
	uRet = CardModifyConfiguration(hClient, hLocalSocket, &fLocalAttrib);
	if(uRet == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardModifyConfig_InvalidParas: Thread %u for Socket %u: CardModifyConfiguration with wrong socket number!, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		return FALSE;
	}
	
	//try wrong socket number
	hLocalSocket.uSocket = hSocket.uSocket;
	hLocalSocket.uFunction = 3;
	uRet = CardModifyConfiguration(hClient, hLocalSocket, &fLocalAttrib);
	if(uRet == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardModifyConfig_InvalidParas: Thread %u for Socket %u: CardModifyConfiguration with wrong function number!, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		return FALSE;
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- ConfigTest::CardModifyConfig_InvalidParas()\r\n")));

	return TRUE;
}


//----------------------------------------------------------------------------
// Function:	Test_CardPowerOnOff()
//
// calling order:   Call
//
// CardRequestConfiguration()
// CardPowerOff()
// CardPowerOn()
//
//----------------------------------------------------------------------------
BOOL ConfigTest::Test_CardPowerOnOff(
	PCLIENT_CONTEXT      pData,
	CARD_CLIENT_HANDLE   hClient,
	CARD_WINDOW_HANDLE   hWindow,
	PCARD_CONFIG_INFO    pconfInfo)
{
	BOOL                 			f = TRUE;
	CARD_SOCKET_HANDLE   	hSocket;
	STATUS               		uRet, uRetReqConf;
	DWORD                		dwLogStatus;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_CardPowerOnOff() enterted\r\n")));

	hSocket.uSocket = pconfInfo->hSocket.uSocket;
	hSocket.uFunction = pconfInfo->hSocket.uFunction;

	//try CardPowerOff before requesting config, should return CERR_BAD_SOCKET
	uRet = CardPowerOff(hClient, hSocket);
	if(uRet == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardPowerOff: Thread %u for Socket %u: before requesting config,  should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		return FALSE;
	}

	//try CardPowerOn before requesting config, should return CERR_BAD_SOCKET
	uRet = CardPowerOn(hClient, hSocket);
	if(uRet == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardPowerOn: Thread %u for Socket %u: before requesting config,  should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		return FALSE;
	}

	//request configuration first
	uRetReqConf =  CardRequestConfiguration(hClient, pconfInfo);

	 g_pKato->Log(LOG_DETAIL,TEXT("CardPowerOnOff: Thread %u for Socket %u: CardRequestConfig(): uRet=0x%lx: uSocket=%d uFunction=%d \r\n"),
				 dwThreadID, uLocalSock,   uRetReqConf, pconfInfo->hSocket.uSocket, pconfInfo->hSocket.uFunction);

	if ( uRetReqConf != uExpectRet){//we should get the expected result
		if (dwTotalThread > 1){//Multi-thread tests
			if (uRetReqConf == CERR_IN_USE){//someone else is using it, acceptable
				dwLogStatus = LOG_DETAIL;
			}
			else{//otherwise this is a true failure
				f= FALSE;
				dwLogStatus = LOG_FAIL;
			}

   			g_pKato->Log(dwLogStatus,TEXT("CardPowerOnOff: Thread %u for Socket %u:  CardRequestConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
									  dwThreadID, uLocalSock,   uExpectRet, uRetReqConf);
		}
		else{//failure.
   			g_pKato->Log(LOG_DETAIL,TEXT("CardPowerOnOff: Thread %u for Socket %u: 1st: CardRequestConfig() FAILed: uExpectRet=0x%lx  uRet=0x%lx\r\n"),
										dwThreadID, uLocalSock,   uExpectRet, uRetReqConf);
		}
	}

	//---now test CardPowerOff---

	//try use null client
	uRet = CardPowerOff(NULL, hSocket);
	if(uRet == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardPowerOff: Thread %u for Socket %u: null client,  should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		f = FALSE;
		goto CLEANUP;
	}

	//try use wrong socket number
	hSocket.uSocket = 3;
	hSocket.uFunction = pconfInfo->hSocket.uFunction;
	uRet = CardPowerOff(hClient, hSocket);
	if(uRet == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardPowerOff: Thread %u for Socket %u: wrong socket number,  should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		f = FALSE;
		goto CLEANUP;
	}

	//try use wrong socket number
	hSocket.uSocket = pconfInfo->hSocket.uSocket;
	hSocket.uFunction = 3;
	uRet = CardPowerOff(hClient, hSocket);
	if(uRet == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardPowerOff: Thread %u for Socket %u: wrong function number,  should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		f = FALSE;
		goto CLEANUP;
	}

	//try use normal parameters
	hSocket.uSocket = pconfInfo->hSocket.uSocket;
	hSocket.uFunction = pconfInfo->hSocket.uFunction;
	uRet = CardPowerOff(hClient, hSocket);
	if(uRet != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardPowerOff: Thread %u for Socket %u: expected: 0x%x, ret = 0x%x"),
				 dwThreadID, uLocalSock,  CERR_SUCCESS,  uRet);
		f = FALSE;
		goto CLEANUP;
	}

	//wait for a while
	Sleep(TEST_WAIT_TIME);

	//---now test CardPowerOn---

	//try use null client
	uRet = CardPowerOn(NULL, hSocket);
	if(uRet != CERR_BAD_HANDLE){
		g_pKato->Log(LOG_FAIL,TEXT("CardPowerOn: Thread %u for Socket %u: null client,  expected: 0x%x, ret = 0x%x"),
				 dwThreadID, uLocalSock,  CERR_BAD_HANDLE,  uRet);
		f = FALSE;
		goto CLEANUP;
	}

	//try use wrong socket number
	hSocket.uSocket = 3;
	hSocket.uFunction = pconfInfo->hSocket.uFunction;
	uRet = CardPowerOn(hClient, hSocket);
	if(uRet == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardPowerOn: Thread %u for Socket %u: wrong socket number,  should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		f = FALSE;
		goto CLEANUP;
	}

	//try use wrong socket number
	hSocket.uSocket = pconfInfo->hSocket.uSocket;
	hSocket.uFunction = 3;
	uRet = CardPowerOn(hClient, hSocket);
	if(uRet == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardPowerOn: Thread %u for Socket %u: wrong function number,  should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		f = FALSE;
		goto CLEANUP;
	}

	//try use normal parameters
	hSocket.uSocket = pconfInfo->hSocket.uSocket;
	hSocket.uFunction = pconfInfo->hSocket.uFunction;
	uRet = CardPowerOn(hClient, hSocket);
	if(uRet != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardPowerOn: Thread %u for Socket %u: expected: 0x%x, ret = 0x%x"),
				 dwThreadID, uLocalSock,  CERR_SUCCESS,  uRet);
		f = FALSE;
	}

CLEANUP:

	//==========================================================================
	//
	//	 Now clean up:
	//
	//==========================================================================

	f &=  SubTestReleaseConfig(pData, hClient, hWindow, hSocket);

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- ConfigTest::Test_CardPowerOnOff()\r\n")));

	return f;
}

//this is a dummy ISR function
VOID ConfISRFunction(UINT32 uContext)
{
	return;
}


