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
#include "IntrTest.h"


//globals

	HANDLE 	v_IsrLogEvent;
	BOOL   	gfLogIsrThread;
	HANDLE 	v_IsrLogExitEvent;
	HANDLE	hLogThread;
	int     	IsrLogIndex,IsrPrintIndex;
	UINT    	iByteStream;
	ISR_LOG_BUFFER IsrLog[ISR_LOG_EVENTS];


BOOL
IntrTest::Init(){
	LPINTRTESTPARAMS  lparm = &intrarr[dwCaseID-1];

	//initialize test parameters
	nTestFunc	= lparm->nTestFunc;
	fMemoryCard	= lparm->fMemory  ;
	uCallRR       	= lparm->uCallRR       ;
	nHCLIENT      	= lparm->nHCLIENT1      ;
	uConfigReg    	= lparm->uConfigReg    ;
	uExpectRet	= lparm->uExpectR	 ;
	nHCLIENT2	= lparm->nHCLIENT2	;
	uExpectRet2	= lparm->uExpectR2   ;
       nISRFUNC     	= lparm->nISRFUNC      ;
       uISRData      	= lparm->uISRData      ;
	nStream   	=(nTestFunc & 0xF00) >> 8;
	iByteStream 	= 0;
	IsrLogIndex 	= 0;
	IsrPrintIndex 	= 0;

	//initialize some status flags that related with test progress 
	fDeregisterClientDone = FALSE;
	fReleaseWindowDone = FALSE;
	fReleaseIRQDone = FALSE;
	fReleaseExclDone = FALSE;
	fReleaseConfigDone = FALSE;
	
	//setup client data
	client_data.hClient= NULL;
	client_data.hEvent = NULL;
	client_data.pWin   = NULL;
	client_data.uGranularity     = 0;	  // for Manaul test use
	client_data.fCardReqExclDone = 0;
	client_data.uCardReqStatus = 0xFF;	// undefined value
	client_data.ThreadNo = dwThreadID;

	memset((LPVOID)client_data.rgByteStream, 0, 64);
  	client_data.uStreamLen =  rgStreamType[nStream].uStreamLen;
	memcpy(&(client_data.rgByteStream[0]), rgStreamType[nStream].rgByteCmd, client_data.uStreamLen);

	return TRUE;
}

BOOL IntrTest::LaunchISRLogThread(){

	// only one thread can succeed in CardRequestIRQ()
     	v_IsrLogEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (v_IsrLogEvent == NULL)
	{
   	    	g_pKato->Log(LOG_COMMENT,TEXT("CreateEvent() FAILed: for v_IsrLogEvent \r\n"));
	    	g_pKato->Log(LOG_DETAIL,TEXT("GetLastError() return = %ld\r\n"), GetLastError());
		return FALSE;
	}

	gfLogIsrThread = TRUE;

	//event fired when the thread exited
   	v_IsrLogExitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (v_IsrLogExitEvent == NULL)
	{
   	    	g_pKato->Log(LOG_COMMENT,TEXT("CreateEvent() FAILed: for v_IsrLogExitEvent \r\n"));
		g_pKato->Log(LOG_DETAIL,TEXT("GetLastError() return = %ld\r\n"), GetLastError());
		CloseHandle(v_IsrLogEvent);
		return FALSE;
	}

	// Expect this thread to be closed automatically after dll is termianted
    	// This thread waits on IsrlogEvent and logs whenever output comes to the 
    	// IsrLogThread...
    	hLogThread = NULL;
    	hLogThread = ::CreateThread(NULL,
						        1024,
						        (LPTHREAD_START_ROUTINE)IntrTest::LogIsrThread,
						        NULL,
							  0,	  // immmediately run :   other flag is CREATE_SUSPENDED
						        NULL);


	if (hLogThread == NULL){
   	    	g_pKato->Log(LOG_COMMENT,TEXT("CreateThread() Failed:  for hLogThread\r\n"));
		g_pKato->Log(LOG_DETAIL,TEXT("GetLastError() return = %ld\r\n"),  GetLastError());
		CloseHandle(v_IsrLogEvent);
		CloseHandle(v_IsrLogExitEvent);
		return FALSE;
	}

	return TRUE;

}

BOOL IntrTest::EndISRLogThread(){
	BOOL	bWait = TRUE;
	DWORD	dwExitCode =(DWORD) -1;
	
	gfLogIsrThread = FALSE;
	//wait for the thread to exit
  	WaitForSingleObject(v_IsrLogExitEvent, INFINITE);

	while (bWait){
		if (!::GetExitCodeThread(hLogThread,  &dwExitCode)){
   		     	g_pKato->Log(LOG_DETAIL,TEXT("GetExitCodeThread() Failed:. thread 0x%lx:   dwError=%ld\r\n"),
					hLogThread, GetLastError());
			 break;
		}
		if (dwExitCode !=  STILL_ACTIVE)
			break;

	  Sleep(1000);
	}

	if (dwExitCode != 1){
   	     	g_pKato->Log(LOG_DETAIL,TEXT("GetExitCodeThread() Failed: dwExitCode (0x%lx) != 1\r\n"),dwExitCode);
		return FALSE;
	}

	CloseHandle(v_IsrLogEvent);
	v_IsrLogEvent = NULL;
	CloseHandle(v_IsrLogExitEvent);
	v_IsrLogExitEvent = NULL;
	CloseHandle(hLogThread);
	hLogThread = NULL;
	
	return TRUE;
}

//defines used by ThreadRun() 
#define	INTRTST_WINSIZE	0x100

DWORD IntrTest::ThreadRun() {

	CARD_REGISTER_PARMS	crParam;
	CARD_WINDOW_PARMS    	cwParam;
	CARD_CLIENT_HANDLE   	hClient = NULL;
	CARD_CLIENT_HANDLE		hClientTest = NULL;
	CARD_WINDOW_HANDLE   	hWindow = NULL;
  	CARD_SOCKET_HANDLE   	hSocket;
	UINT                 			uRet;
	DWORD                		dwError, dw, dwLogStatus;
	CARD_CONFIG_INFO     		confInfo;
	PCLIENT_CONTEXT 		pData = &client_data;
	PVOID					pWin = NULL;
	UINT32					uGranularity;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ IntrTest::ThreadRun() enterted\r\n")));
   	
	//create event:
	pData->hEvent = CreateEvent(0L,  FALSE,  FALSE /*fInitState*/, NULL);
	if (NULL == pData->hEvent){
   		g_pKato->Log(LOG_DETAIL,TEXT("***Thead %u for Socket %u: CreateEvent() FAILed:\r\n"), dwThreadID, uLocalSock);
		dwError = GetLastError();
		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: GetLastError() return = 0x%lx\r\n"), dwThreadID, uLocalFunc, dwError);
		SetResult(FALSE);
		return dwError;
	}


	if (fMemoryCard)
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
	hClient = CardRegisterClient(CallBackFn_Client, &crParam);
   	dw = WaitForSingleObject( pData->hEvent, TEST_WAIT_TIME);
	if(dw == WAIT_TIMEOUT || !hClient ){
   		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRegisterClient() FAILed\r\n"), dwThreadID, uLocalSock);
		dwError = GetLastError();
   		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: GetLastError() return =0x%lx\r\n"), dwThreadID, uLocalSock, dwError);
		return FALSE;
	}

	pData->hClient = hClient;

	//reset function
	CardResetFunction(hClient, hSocket);
//	Sleep (2000);

	cwParam.hSocket.uSocket   = (UCHAR)uLocalSock  ;
	cwParam.hSocket.uFunction = (UCHAR)uLocalFunc;

	if (fMemoryCard)
		cwParam.fAttributes  = 0;    // Memory Window
	else
		cwParam.fAttributes  = WIN_ATTR_IO_SPACE;   //0x01

	cwParam.uWindowSize = INTRTST_WINSIZE;
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

	//if(dwTotalThread > 1)
	//	Sleep(2000);

	//map window
	switch (uConfigReg & 0x3F){
		default:
   		 	g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: (uConfigReg & 0x3F)=%d:  Don't know how to handle. Use 0x3F8\r\n"),
					dwThreadID, uLocalSock,  (uConfigReg & 0x3F));
			SetResult(FALSE);
		  	goto LReleaseConfig;
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

	SetLastError(0);
	pWin = CardMapWindow(hWindow, uCardAddress, 7, &uGranularity);
	if (pWin == NULL){
		dwError = GetLastError();
		if(dwError == CERR_OUT_OF_RESOURCE){
			g_pKato->Log(LOG_DETAIL,	
			(LPTSTR)TEXT("CardMapWindow() FAILed: CERR_OUT_OF_RESOURCE: uCardAddr=0x%lx uSize=0x%lx dwError=0x%lx\r\n"),
	              uCardAddress, 7, dwError);
			SetResult(FALSE);
			goto LReleaseWindow;
		}
		if(dwError == CERR_IN_USE){
			//This is normally caused by mapping to a already-mapped address, or I/O address is not avalialbe
			//in this system. So it will be ignored.
			g_pKato->Log(LOG_DETAIL,	
			(LPTSTR)TEXT("CardMapWindow() FAILed: CERR_IN_USE: uCardAddr=0x%lx uSize=0x%lx dwError=0x%lx\r\n"),
	              uCardAddress, 7, dwError);
			goto LReleaseWindow;
		}
		if(dwError == CERR_BAD_WINDOW){
			g_pKato->Log(LOG_DETAIL,	
			(LPTSTR)TEXT("CardMapWindow() FAILed: CERR_BAD_WINDOW: uCardAddr=0x%lx uSize=0x%lx dwError=0x%lx\r\n"),
	              uCardAddress, 7, dwError);
			SetResult(FALSE);
			goto LReleaseWindow;
		}
		if(dwError == CERR_BAD_HANDLE){
			g_pKato->Log(LOG_DETAIL,	
			(LPTSTR)TEXT("CardMapWindow() FAILed: CERR_BAD_HANDLE: uCardAddr=0x%lx uSize=0x%lx dwError=0x%lx\r\n"),
	              uCardAddress, 7, dwError);
			SetResult(FALSE);
			goto LReleaseWindow;
		}
		if(dwError == CERR_BAD_SIZE){
			g_pKato->Log(LOG_DETAIL,	
			(LPTSTR)TEXT("CardMapWindow() FAILed: CERR_BAD_SIZE: uCardAddr=0x%lx uSize=0x%lx dwError=0x%lx\r\n"),
	              uCardAddress, 7, dwError);
			SetResult(FALSE);
			goto LReleaseWindow;
		}
   	       g_pKato->Log(LOG_DETAIL,TEXT("%s: CardMapWindow() FAILed:  uSocket=%d  uCardAddress=0x%lX   dwError=0x%lx\r\n"),
				(LPTSTR) pData->lpClient, cwParam.hSocket.uSocket, uCardAddress, dwError);
		SetResult(FALSE);
		goto LReleaseWindow;	
	}
	
	pData->pWin = pWin;
	pData->uGranularity = uGranularity;

	if (!(uCallRR & CALL_REQUEST_EXCL))
		goto LRequestConfig;

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

	// Test reset again
	uRet = CardResetFunction(hClient, hSocket);
	if (uRet != CERR_SUCCESS){
	     g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardResetFunction() FAILed: uSocket=%d  uRet=0x%lx\r\n"),
				dwThreadID, uLocalSock, hSocket.uSocket, uRet);

		SetResult(FALSE);
		goto LReleaseExclusive;
	}


LRequestConfig:

	if (!(uCallRR & CALL_REQUEST_CONFIG))
		goto LRequestIRQ;

	//RequestConfiguration


	confInfo.hSocket.uSocket        	= hSocket.uSocket   ;
	confInfo.hSocket.uFunction      	= hSocket.uFunction ;
	confInfo.fAttributes    			= CFG_ATTR_VALID_CLIENT | CFG_ATTR_IRQ_STEERING;
	confInfo.fInterfaceType			= CFG_IFACE_MEMORY_IO;
	confInfo.uVcc           			= (UCHAR)g_Vcc      ;
	confInfo.uVpp1          			= 0     ;
	confInfo.uVpp2          			= 0     ;
	confInfo.fRegisters     			= CFG_REGISTER_CONFIG|CFG_REGISTER_STATUS;
	confInfo.uConfigReg     			= uConfigReg;
	confInfo.uStatusReg     		= FCR_FCSR_REQUIRED_BITS;
	confInfo.uPinReg        			= 0   ;
	confInfo.uCopyReg       		= 0  ;
	confInfo.uExtendedStatus		= 0;

	uRet = CardRequestConfiguration(hClient, &confInfo);
	if ( uRet != CERR_SUCCESS){
		if (dwTotalThread == 1)
			SetResult(FALSE);

   	  	 g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRequestConfig() FAILed: uSocket=%d uRet=0x%lx\r\n"),
				dwThreadID, uLocalSock, confInfo.hSocket.uSocket, uRet);
	  	goto LReleaseExclusive;
	}

	InitIRQRegister(pData);
	
LRequestIRQ:
	//===========================================================================
	//
	//  Now Test CardRequestIRQ()	
	//
	//===========================================================================
	if (!(uCallRR & CALL_REQUEST_IRQ))
		goto LTestReleaseIRQ;

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
			CardReleaseExclusive(hClient, hSocket);
			CardReleaseWindow(hWindow);
			CardDeregisterClient(hClient);
			fReleaseExclDone      = TRUE;
			fReleaseWindowDone    = TRUE;
			fDeregisterClientDone = TRUE;
			break;
		case 4: //use invalid socket number
			hClientTest = hClient;
			hSocket.uSocket   = 0xFF;
			hSocket.uFunction = 0xFF;
			break;
		default:
			hClientTest = hClient;
			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: nHCLIENT =%d : Don't know how to handle. \r\n"),
				dwThreadID, uLocalSock, nHCLIENT);
			SetResult(FALSE);
	}

	uRet = CardRequestIRQ(hClientTest, hSocket, nISRFUNC == 1 ? ISRFunction : NULL, (UINT32) pData);
	if (uRet != uExpectRet) { // FAIL
   	    	g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardRequestIRQ() FAILed: uRet=0x%lx uExpectRet=0x%lx  uSocket=%d  uFunc=%d\r\n"),
				dwThreadID, uLocalSock, uRet, uExpectRet, hSocket.uSocket, hSocket.uFunction);
		if (dwTotalThread == 1)
			SetResult(FALSE);
	}

	hSocket.uSocket   = (UCHAR)uLocalSock;
	hSocket.uFunction = (UCHAR)uLocalFunc;

	if (uRet != CERR_SUCCESS)
		goto LReleaseConfig;

	StartIRQ(pData);
	Sleep (TEST_WAIT_TIME);

	// Test: release Configuration first()
LReleaseConfig:
	if (!(uCallRR & CALL_RELEASE_CONFIG))
		goto LTestReleaseIRQ;

	// Have to call CardeleaseConfiguration () before call CardReleaseIRQ
	uRet = CardReleaseConfiguration(hClient, hSocket);
	if (uRet != CERR_SUCCESS){
   	  	g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardReleaseConfig() FAILed: uRet=0x%lx\r\n"),
				dwThreadID, uLocalSock, uRet);
		if (dwTotalThread == 1)
			SetResult(FALSE);
	}
	else
		fReleaseConfigDone = TRUE;

LTestReleaseIRQ:
	//===========================================================================
	//
	//  Now Test CardReleaseIRQ()
	//
	//===========================================================================
	if (!(uCallRR & CALL_RELEASE_IRQ))
		goto LReleaseExclusive;

	switch (nHCLIENT2){
		case 0:  
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
			fReleaseExclDone      = TRUE;
			fReleaseWindowDone    = TRUE;
			fDeregisterClientDone = TRUE;
			break;
		case 4: //use invalid socket number
			hClientTest = hClient;
			hSocket.uSocket   = 0xFF;
			hSocket.uFunction = 0xFF;
			break;
		default:
			hClientTest = hClient;
			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: nHCLIENT =%d : Don't know how to handle. \r\n"),
				dwThreadID, uLocalSock, nHCLIENT);
			SetResult(FALSE);
		}


	uRet = CardReleaseIRQ(hClientTest, hSocket);
	if (uRet != uExpectRet2){  // FAIL
   		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardReleaseIRQ() FAILed: uRet=0x%lx uExpectRet2=0x%lx, uSocket=%d uFunc=%d\r\n"),
				dwThreadID, uLocalSock, uRet, uExpectRet2, hSocket.uSocket, hSocket.uFunction);
		if (dwTotalThread == 1)
			SetResult(FALSE);
	}

	if (uRet == CERR_SUCCESS)
		fReleaseIRQDone = TRUE;

	hSocket.uSocket   = (UCHAR)uLocalSock;
	hSocket.uFunction = (UCHAR)uLocalFunc;

LReleaseExclusive:
	if (!(uCallRR & CALL_RELEASE_EXCL))
		goto LReleaseWindow;
		
	uRet =  CardReleaseExclusive(hClient, hSocket);
	if (uRet) { // Failed
 		g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardReleaseExclusive() FAILed: uRet=0x%lx: uSocket=%d\n"),
												dwThreadID, uLocalSock,   uRet, hSocket.uSocket);
		SetResult(FALSE);
	}
	else
		fReleaseExclDone = TRUE;
	
LReleaseWindow:

	uRet = CardReleaseWindow(hWindow);
	if (uRet){  // Failed
		if (!fReleaseWindowDone){//failured may caused by some un-released resources
			if (!(uCallRR & CALL_RELEASE_CONFIG) || !(uCallRR & CALL_RELEASE_IRQ) ||!(uCallRR & CALL_RELEASE_EXCL) ){
					g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: CardReleaseWindow() FAILed as expected: due to not Reelase Resource\r\n"),
												dwThreadID, uLocalSock);
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

	//free event
	CloseHandle( pData->hEvent);
	pData->hEvent = NULL;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- IntrTest::ThreadRun()\r\n")));

	return GetLastError();
}


/* 
@Func ISRFunction is installed by the call to function CardRequestIRQ...
CardServices calls this ISRFunction when....
*/
VOID ISRFunction(UINT32 uContext)
{
	PCLIENT_CONTEXT		pData =  (PCLIENT_CONTEXT) uContext;
	PUCHAR          		pCardIOWin;
   	UCHAR           		iir, ier, lsr, rbr, msr;
	UINT32   		 		MemGranularity;

	pCardIOWin = (PUCHAR) pData->pWin;
	MemGranularity = pData->uGranularity;

   	 // Lets indicate which interrupts are pending
	iir =  READ_PORT_UCHAR(pCardIOWin+INTERRUPT_IDENT_REGISTER  * MemGranularity);
	ier =  READ_PORT_UCHAR(pCardIOWin+INTERRUPT_ENABLE_REGISTER * MemGranularity);

     	// Lets handle this interrupt
    	switch( iir & SERIAL_IIR_INT_MASK ){
        /*
          
          @comm The interrupts are handled here in the descending order of priority.
          i.e SERIAL_IIR_RLS comes much ahead of SERIAL_IIR_RDA etc...

        */
       	case SERIAL_IIR_RLS:   // Receiver Line Status
	             //Read the Line Status Register to Clear Interrupt
	            lsr = READ_PORT_UCHAR(pCardIOWin + LINE_STATUS_REGISTER * MemGranularity);

	            LogIsrEvent(LOG_LSR_EVENT, lsr, 0);
	            break;

	        case SERIAL_IIR_RDA:   // Receive Data Available
	        case SERIAL_IIR_CTI:   // Character Timeout Indicator
	        case SERIAL_IIR_CTI_2: // Yet Another Character Timeout Indicator
	             //Read the Receive Buffer Register to Clear Interrupt
	            rbr = READ_PORT_UCHAR(pCardIOWin+RECEIVE_BUFFER_REGISTER * MemGranularity);
	            LogIsrEvent(LOG_RBR_EVENT, rbr, 0);
	            break;

	        case SERIAL_IIR_THRE:  // Transmit Holding Register Empty
	            if( iByteStream >= pData->uStreamLen )
	            {
	               iByteStream = 0;
	               ier &= ~SERIAL_IER_THR;
	               WRITE_PORT_UCHAR(pCardIOWin + INTERRUPT_ENABLE_REGISTER * MemGranularity, ier);
	               LogIsrEvent(LOG_TXE_EVENT, ier, 0);
	            }
	            else
	            {
	                rbr = pData->rgByteStream[iByteStream++];
	                // rbr is enabled again...
	                WRITE_PORT_UCHAR(pCardIOWin+TRANSMIT_HOLDING_REGISTER * MemGranularity, rbr);
	                WRITE_PORT_UCHAR(pCardIOWin+INTERRUPT_ENABLE_REGISTER * MemGranularity, ier);

	                LogIsrEvent(LOG_TXE_TRANS_EVENT, rbr, ier);

	            }
	            break;

	        case SERIAL_IIR_MS:    // Check Modem Status Register
	        default:
	             //Read the Modem Status Register to Clear Interrupt
	            msr = READ_PORT_UCHAR(pCardIOWin+MODEM_STATUS_REGISTER * MemGranularity);

	            LogIsrEvent(LOG_MSR_EVENT, msr, iir);
	            break;
    	}

   
}

VOID LogIsrEvent( UINT8 Event, UINT8 Reg1, UINT8 Reg2 ){
    	IsrLog[IsrLogIndex].EventNum = Event;
    	IsrLog[IsrLogIndex].Reg1 = Reg1;
    	IsrLog[IsrLogIndex].Reg2 = Reg2;
    	IsrLogIndex++;
    	if (IsrLogIndex >= ISR_LOG_EVENTS) {
        	IsrLogIndex = 0;
    	}
    	SetEvent(v_IsrLogEvent);
}


DWORD WINAPI IntrTest::LogIsrThread( UINT Nothing ){
   	UINT8 reg1, reg2;
	DWORD dw;

	g_pKato->Log(LOG_DETAIL,TEXT("===============  LogIsrThread () entered ============\r\n"));

    	while (gfLogIsrThread){
      		dw = WaitForSingleObject(v_IsrLogEvent, 2000);
		if(dw != WAIT_OBJECT_0)
			continue;
       	while (IsrPrintIndex != IsrLogIndex){
		       reg1 = IsrLog[IsrPrintIndex].Reg1;
		       reg2 = IsrLog[IsrPrintIndex].Reg2;

	            switch (IsrLog[IsrPrintIndex].EventNum){
	            		case LOG_LSR_EVENT:
				   	g_pKato->Log(LOG_DETAIL,TEXT("LSR Interrupt, LSR 0x%X\r\n"), reg1 );
	                		break;

	            		case LOG_RBR_EVENT:
				   	g_pKato->Log(LOG_DETAIL,TEXT("RBR Interrupt, char %c (0x%X)\r\n"), (isprint(reg1) ? reg1 : '.'), reg1 );
	                		break;

	            		case LOG_TXE_EVENT:
				   	g_pKato->Log(LOG_DETAIL,TEXT("TX Intr: Ending Transmit Stream, ier 0x%X\r\n"), reg1 );
	                		break;

	            		case LOG_TXE_TRANS_EVENT:
				   	g_pKato->Log(LOG_DETAIL,TEXT("TX Intr: Transmitting: rbr %c (0x%x)  ier=0x%X\r\n"),
							(isprint(reg1) ? reg1 : '.'), reg1, reg2);
	                		break;

	            		case LOG_MSR_EVENT:
				   	g_pKato->Log(LOG_DETAIL,TEXT("MSR Interrupt msr 0x%X, iir 0x%X\r\n"), reg1, IsrLog[IsrPrintIndex].Reg2 );
	                		break;

	            		default:
				   	g_pKato->Log(LOG_DETAIL,TEXT("Invalid entry in IsrLog[%d]\r\n"), IsrPrintIndex);
	            	}

	            IsrPrintIndex++;
	            if (IsrPrintIndex >= ISR_LOG_EVENTS)
	                IsrPrintIndex = 0;
		}
    	}

   	SetEvent(v_IsrLogExitEvent);
   	g_pKato->Log(LOG_COMMENT,TEXT("===============  LogIsrThread () Set ExitEvent ============\r\n"));

	ExitThread(1);
	return 0;
}   // LogDisplayThread

//@Func VOID | InitIRQRegister | Initialize the UART registers to recieve and transmit interrupts...
VOID IntrTest:: InitIRQRegister(PCLIENT_CONTEXT pData){
	UCHAR     FCR, byte;
	PUCHAR    pCardIOWin = (PUCHAR)pData->pWin;
	UINT32    MemGranularity = pData->uGranularity;
	UINT16    divisor;


	iByteStream = 0;
	IsrLogIndex = 0;
	IsrPrintIndex = 0;

    	FCR = SERIAL_FCR_ENABLE | SERIAL_FCR_RCVR_RESET | SERIAL_FCR_TXMT_RESET | SERIAL_1_BYTE_HIGH_WATER;

	WRITE_PORT_UCHAR(pCardIOWin+FIFO_CONTROL_REGISTER * MemGranularity, FCR);

	// disable interrupt
	WRITE_PORT_UCHAR(pCardIOWin+INTERRUPT_ENABLE_REGISTER * MemGranularity, 0);

	 // set baud rate, then set LCR
	 byte = SERIAL_8_DATA | SERIAL_1_STOP | SERIAL_NONE_PARITY;
	 divisor = 12;

    	WRITE_PORT_UCHAR(pCardIOWin+LINE_CONTROL_REGISTER * MemGranularity, (UCHAR) (byte | SERIAL_LCR_DLAB));
    	WRITE_PORT_UCHAR(pCardIOWin+DIVISOR_LATCH_LSB     * MemGranularity, (UCHAR) (divisor & 0xff)        );
    	WRITE_PORT_UCHAR(pCardIOWin+DIVISOR_LATCH_MSB     * MemGranularity, (UCHAR) ((divisor >> 8) & 0xff) ) ;
    	WRITE_PORT_UCHAR(pCardIOWin+LINE_CONTROL_REGISTER * MemGranularity, byte);

    	// BIG TRICK.  Must set out2 bit in order to receive interrupts
    	byte = SERIAL_MCR_IRQ_ENABLE | SERIAL_MCR_DTR | SERIAL_MCR_RTS;
   	 WRITE_PORT_UCHAR(pCardIOWin+MODEM_CONTROL_REGISTER* MemGranularity, byte);

}



/*
@Func | VOID | StartIRQ | Enable the recieve and transmit bits of IER.
*/
VOID IntrTest::StartIRQ(PCLIENT_CONTEXT pData){
	PUCHAR    pCardIOWin = (PUCHAR)pData->pWin;
	UINT32    MemGranularity = pData->uGranularity;
	UCHAR		 ier;

	//==============================================
	// Now tern on all interrunpts
	//
	//==============================================
     	// Get the current value of IER, and or in bites we want
    	ier = READ_PORT_UCHAR(pCardIOWin+INTERRUPT_ENABLE_REGISTER * MemGranularity);

    	ier |= SERIAL_IER_THR | SERIAL_IER_RDA | SERIAL_IER_RLS | SERIAL_IER_MS;

     	// And finally, turn on all interrupts
    	WRITE_PORT_UCHAR(pCardIOWin+INTERRUPT_ENABLE_REGISTER * MemGranularity, ier);
}


