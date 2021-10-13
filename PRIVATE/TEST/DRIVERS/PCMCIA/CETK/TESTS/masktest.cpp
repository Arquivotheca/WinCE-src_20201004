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
#include "MaskTest.h"


//callback function, doing nothing
STATUS XYZCallback (CARD_EVENT,CARD_SOCKET_HANDLE,PCARD_EVENT_PARMS) {return 0;}

VOID MaskTest::SetGlobal () {fAttr = fAttr & ~(UINT16)1 ;}
VOID MaskTest::SetLocal ()  {fAttr = fAttr | (UINT16)1 ;}
int  MaskTest::IsLocal () {return fAttr & 1 ;}
int  MaskTest::IsGlobal () {return !IsLocal() ;}

VOID MaskTest::RequestMask (UINT16 mask){

    	if (NULL == hClient) 
    		return ;

    	if (IsGlobal()){ // Try to get a local socket mask
    		CARD_SOCKET_HANDLE hSocket ;
        	hSocket.uSocket = (UCHAR)uLocalSock ;
        	hSocket.uFunction = (UCHAR)uLocalFunc ;
        	g_pKato->Log(LOG_DETAIL,TEXT("Thread %u: before CardRequestSocketMask()  Socket#= %d\r\n"),
                       dwThreadID, uLocalSock);
        	stat = CardRequestSocketMask(hClient, hSocket, mask);
      }
    	else SetEventMask (mask) ;

    	// Now see if we got a socket mask.
    	if (stat == CERR_SUCCESS){
        	g_pKato->Log(LOG_DETAIL,TEXT("CardRequestSocketMask()  PASS -- Client =0x%lx -- ID%d, Sock%d\r\n"),
                       hClient, dwThreadID, uLocalSock);
      }
    	else {
        	g_pKato->Log(LOG_DETAIL,TEXT("CardRequestSocketMask()  FAIL Error code: %s -- ID%d, Sock%d\r\n"),
        	               FindStatusName(stat), dwThreadID, uLocalSock) ;
        	updateError (TST_SEV2) ;
        	SetResult(FALSE);
      }

    	if(stat == CERR_SUCCESS){
        	fMask = mask ; 
        	SetLocal () ;
        	// now set fLocalExpMask
        	PCARD_EVENT_MASK_PARMS pMaskParms = new CARD_EVENT_MASK_PARMS ;
        	(pMaskParms->hSocket).uSocket = (UCHAR)uLocalSock ;
        	(pMaskParms->hSocket).uFunction = (UCHAR)uLocalFunc ;
        	pMaskParms->fAttributes = fAttr ;
        	
        	stat = CardGetEventMask(hClient, pMaskParms);
        	fLocalExpMask = pMaskParms->fEventMask ;
        	delete pMaskParms ;
      }
  }

VOID MaskTest::ReleaseMask (){

    	if (IsGlobal()){
     		RequestMask (0xFFFF) ;
    		DEBUGMSG(ZONE_WARNING, (TEXT("MaskTest::ReleaseMask could not be called")));
        	return ;
      }

    	CARD_SOCKET_HANDLE hSocket ;
    	hSocket.uSocket = (UCHAR)uLocalSock ;
    	hSocket.uFunction = (UCHAR)uLocalFunc ;
    	g_pKato->Log(LOG_DETAIL,TEXT("Thread %u: before CardReleaseSocketMask()  Socket#= %d\r\n"), 
    							dwThreadID, uLocalSock);
    	stat = CardReleaseSocketMask(hClient, hSocket);
      	DEBUGMSG(ZONE_VERBOSE, (TEXT("CardReleaseSocketMask -- hClient: "),   (DWORD)hClient)) ;
     	DEBUGMSG (ZONE_VERBOSE, (TEXT("CardReleaseSocketMask -- uSocket: "),   (DWORD)hSocket.uSocket)) ;
    	DEBUGMSG(ZONE_VERBOSE, (TEXT("CardReleaseSocketMask -- uFunction: "), (DWORD)hSocket.uFunction)) ;

    	if (stat == CERR_SUCCESS)
        	g_pKato->Log(LOG_DETAIL,TEXT("CardReleaseSocketMask()  PASS: Error code: CERR_SUCCESS -- ID%d, Sock%d\r\n"), dwThreadID, uLocalSock);
    	else{
      		if(stat == CERR_BAD_SOCKET){
      			if (uLocalFunc == 1)
      				g_pKato->Log(LOG_DETAIL,TEXT("CardReleaseSocketMask()  FAIL: Card is not MFC compliant: Error code: CERR_BAD_SOCKET -- ID%d, Sock%d\r\n"), dwThreadID, uLocalSock);
      			else{
       	 		g_pKato->Log(LOG_DETAIL,TEXT("CardReleaseSocketMask()  FAIL: Error code: %s -- ID%d, Sock%d\r\n"), FindStatusName(stat), dwThreadID, uLocalSock);
        			updateError (TST_SEV2) ;
        			SetResult(FALSE);
      			}
      		}
      }

    	if (stat == CERR_SUCCESS){
        	SetGlobal () ;
        	// now set fGlobalExpMask
        	PCARD_EVENT_MASK_PARMS pMaskParms = new CARD_EVENT_MASK_PARMS ;
        	(pMaskParms->hSocket).uSocket = (UCHAR)uLocalSock;
        	(pMaskParms->hSocket).uFunction = (UCHAR)uLocalFunc;
        	pMaskParms->fAttributes = fAttr ;
        	stat = CardGetEventMask(hClient, pMaskParms);
        	fGlobalExpMask = pMaskParms->fEventMask ;
        	delete pMaskParms ;
      }
  }

VOID MaskTest::GetEventMask (){
    	PCARD_EVENT_MASK_PARMS pMaskParms = new CARD_EVENT_MASK_PARMS ;
    	(pMaskParms->hSocket).uSocket = (UCHAR)uLocalSock ;
    	(pMaskParms->hSocket).uFunction = (UCHAR)uLocalFunc ;
    	pMaskParms->fAttributes = fAttr ;
    	stat = CardGetEventMask(hClient, pMaskParms);
    	fMask = pMaskParms->fEventMask ;
    	delete pMaskParms ;

    	UINT16 fExpectedMask = IsLocal() ? fLocalExpMask : fGlobalExpMask ;

    	if (stat != CERR_SUCCESS){ // Good socket, call failed
      		if(stat == CERR_BAD_SOCKET){
      			if(uLocalFunc == 1)
      				g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetEventMask<--   FAIL -- Card is not MFC compliant: Error return: %s -- ID%d, Sock%d\r\n"), 
      										FindStatusName(stat), dwThreadID, uLocalSock) ;
      			else{
      				g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetEventMask<--   FAIL -- Expected return: CERR_SUCCESS, Actual return: %s -- ID%d, Sock%d\r\n"),
            									FindStatusName(stat), dwThreadID, uLocalSock) ;
          			g_pKato->Log(LOG_DETAIL,TEXT("                                 fAttr: 0x%x -- fEventMask: 0x%x -- ID%d, Sock%d\r\n"), 
          									fAttr, fMask, dwThreadID, uLocalSock) ;
          			updateError (TST_SEV2) ;
          			SetResult(FALSE);
      			}
      			
      		}
      }
    	else if (fMask != fExpectedMask) {// Good socket, call succeeded, got wrong mask
        	g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetEventMask<--   FAIL -- Expected mask: 0x%x -- Actual mask: 0x%x -- ID%d, Sock%d\r\n"),
          							fExpectedMask, fMask, dwThreadID, uLocalSock) ;
        	g_pKato->Log(LOG_DETAIL,TEXT("                                 fAttr: 0x%x -- fEventMask: 0x%x -- ID%d, Sock%d\r\n"),
          							fAttr, fMask, dwThreadID, uLocalSock) ;
        	updateError (TST_SEV2);
        	SetResult(FALSE);
      }
    	else{ // Good socket, call succeeded, got expected mask
        	g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetEventMask<--   PASS -- Returned mask: 0x%x -- ID%d, Sock%d\r\n"),
          							fMask, dwThreadID, uLocalSock) ;
      }
  }

VOID MaskTest::SetEventMask (UINT16 mask){
    	PCARD_EVENT_MASK_PARMS pMaskParms = new CARD_EVENT_MASK_PARMS ;
    	(pMaskParms->hSocket).uSocket = (UCHAR)uLocalSock ;
    	(pMaskParms->hSocket).uFunction = (UCHAR)uLocalFunc ;
    	pMaskParms->fAttributes = fAttr ;
    	pMaskParms->fEventMask  = mask ;
    	if (IsLocal()) 
    		fLocalExpMask = mask ; 
    	else 
    		fGlobalExpMask = mask ;
    	stat = CardSetEventMask(hClient, pMaskParms);
    	delete pMaskParms ;

    	if (stat == CERR_SUCCESS){
        	g_pKato->Log(LOG_DETAIL,TEXT("-->CardSetEventMask<--   PASS -- fAttr: 0x%x -- fEventMask: 0x%x -- ID%d, Sock%d\r\n"),
          							fAttr, fMask, dwThreadID, uLocalSock) ;
      }
    else {
        	g_pKato->Log(LOG_DETAIL,TEXT("-->CardSetEventMask<--   FAIL -- Expected return: CERR_SUCCESS -- Actual return: %s -- ID%d, Sock%d\r\n"),
          							FindStatusName(stat), dwThreadID, uLocalSock) ;
        	g_pKato->Log(LOG_DETAIL,TEXT("                                 fAttr: 0x%x -- fEventMask: 0x%x -- ID%d, Sock%d\r\n"),
          							fAttr, fMask, dwThreadID, uLocalSock) ;
        	updateError (TST_SEV2) ;
        	SetResult(FALSE);
      }
}

VOID MaskTest::GetStatus (){
     	UINT32 rtnCode; 

   	PCARD_STATUS pStatus = new CARD_STATUS ;
    	(pStatus->hSocket).uSocket = (UCHAR)uLocalSock ;
    	(pStatus->hSocket).uFunction = (UCHAR)uLocalFunc ;
	rtnCode = CardGetStatus(pStatus);

    	if (rtnCode != CERR_SUCCESS){
        	g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetStatus<--      FAIL -- Expected return: CERR_SUCCESS -- Actual return: %s  uSoc: %d uFunc %d -- ID%d, Sock%d\r\n"),
          							 FindStatusName(rtnCode), uLocalSock, uLocalFunc, dwThreadID, uLocalSock) ;
        	updateError (TST_SEV2) ;
        	SetResult(FALSE);
      }
    	else{
        	g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetStatus<--      PASS -- Return: CERR_SUCCESS -- ID%d, Sock%d\r\n"),
          							dwThreadID, uLocalSock) ;
      }

    	rtnCode = CardGetStatus(0);

    	if (rtnCode == CERR_BAD_ARGS){
        	g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetStatus<--      PASS -- Return: CERR_BAD_ARGS -- ID%d, Sock%d\r\n"),
          							dwThreadID, uLocalSock) ;
      }
    	else{
        	g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetStatus<--      FAIL -- Expected return: CERR_BAD_ARGS -- Actual return: %s -- ID%d, Sock%d\r\n"),
          							FindStatusName(rtnCode), dwThreadID, uLocalSock) ;
        	updateError (TST_SEV2) ;
        	SetResult(FALSE);
      }

    delete pStatus ;
  }

VOID MaskTest::ResetFunction (){
    	CARD_SOCKET_HANDLE hSocket ;
    	hSocket.uSocket = (UCHAR)uLocalSock ;
    	hSocket.uFunction = (UCHAR)uLocalFunc ;
    	UINT32 rtnCode = CardResetFunction(hClient, hSocket);
    	if(rtnCode == CERR_NO_CARD){
    		g_pKato->Log(LOG_DETAIL,TEXT("-->CardResetFunction<--  FAIL -- CERR_NO_CARD,  uSoc: %d uFunc %d -- ID%d, Sock%d\r\n"),
    						 		uLocalSock, uLocalFunc, dwThreadID, uLocalSock) ;
        	updateError (TST_SEV2);
        	SetResult(FALSE);
    	}
    	if (rtnCode != CERR_SUCCESS){
      		if(rtnCode == CERR_BAD_SOCKET){
      			if(uLocalFunc == 1)
      				g_pKato->Log(LOG_DETAIL,TEXT("-->CardResetFunction<--  FAIL: Card is not MFC compliant: -- Expected return: CERR_SUCCESS -- Actual return: %s  uSoc: %d uFunc %d -- ID%d, Sock%d\r\n"),
                      							 FindStatusName(rtnCode), uLocalSock, uLocalFunc, dwThreadID, uLocalSock) ;
      			else
      				g_pKato->Log(LOG_DETAIL,TEXT("-->CardResetFunction<--  FAIL -- Expected return: CERR_SUCCESS -- Actual return: %s  uSoc: %d uFunc %d -- ID%d, Sock%d\r\n"),
                      							 FindStatusName(rtnCode), uLocalSock, uLocalFunc, dwThreadID, uLocalSock) ;
      		}
        	updateError (TST_SEV2);
        	SetResult(FALSE);	
      }
    	else{
        	g_pKato->Log(LOG_DETAIL,TEXT("-->CardResetFunction<--  PASS -- Return: %s -- ID%d, Sock%d\r\n"),
          							FindStatusName(rtnCode), dwThreadID, uLocalSock) ;
      }

    	rtnCode = CardResetFunction((PVOID)(~(UINT32)hClient), hSocket);

    	if (rtnCode == CERR_BAD_HANDLE){
        	g_pKato->Log(LOG_DETAIL,TEXT("-->CardResetFunction<--  PASS -- Return: %s -- ID%d, Sock%d\r\n"),
          							FindStatusName(rtnCode), dwThreadID, uLocalSock) ;
      }
    	else{
        	g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetStatus<--      FAIL -- Expected return: CERR_BAD_HANDLE -- Actual return: %s -- ID%d, Sock%d\r\n"),
          							FindStatusName(rtnCode), dwThreadID, uLocalSock) ;
        	updateError (TST_SEV2) ;
        	SetResult(FALSE);
      }

}

STATUS MaskTest::GetCurrentStatus () {return stat ;}
UINT16 MaskTest::GetCurrentMask () {return fMask ;}

VOID MaskTest::Generic (UINT16){
    	if (IsLocal())
    		ReleaseMask () ;

    	CARD_SOCKET_HANDLE hSocket ;
    	hSocket.uSocket = (UCHAR)uLocalSock ;
    	hSocket.uFunction = (UCHAR)uLocalFunc ;

    	STATUS stat ;
    	stat = CardRequestSocketMask(hClient, hSocket, fLocalExpMask);
    	DEBUGMSG (ZONE_VERBOSE, (TEXT("generic return: "),FindStatusName(stat))) ;
}


BOOL
MaskTest::Init(){
	LPMASKTESTPARAMS  lparm = &maskarr[dwCaseID-1];

	nWhich = lparm->nWhich;
	nTrials = lparm->nTrials;

	return TRUE;
}

DWORD MaskTest::ThreadRun() {

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ MaskTest::ThreadRun() enterted\r\n")));

	switch(nWhich){
        	case 1:  
        		CardGetStatusTest();
        		break;
        	case 2:  
        		CardEventMasksTest () ;    
        		break ;
        	case 3:  
        		CardResetFunctionTest () ; 
        		break ;
        	case 4:  
        		CardFunctionsTest () ;   
        		break ;
		case 5:
			EventMask_InvalidParaTest();
			break;
		case 6:
			SocketMask_InvalidParaTest();
			break;
        default:
        		g_pKato->Log(LOG_DETAIL,TEXT("Don't know how to handle test group %d\r\n"), nWhich);
        		SetResult(FALSE);
        		break ;
      }

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- MaskTest::ThreadRun() \r\n")));
	
	return 0;
}

VOID MaskTest::CardGetStatusTest(){
    	PCARD_STATUS pStatus = new CARD_STATUS ;
	STATUS ret;
    	UINT32 rtnCode ;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ MaskTest::CardGetStatusTest() enterted\r\n")));

    	(pStatus->hSocket).uSocket = (UCHAR)uLocalSock ;
    	(pStatus->hSocket).uFunction = (UCHAR)uLocalFunc ;

	if(dwTotalThread == 1){//if single-threaded test, then do some bad parameter checking
		ret = CardGetStatus(NULL); // nulkl param checking

		if(ret!= CERR_BAD_ARGS){//it should fail!
			g_pKato->Log(LOG_DETAIL,TEXT("FAIL ! CardGetStatus NULL parameter failure! \n"));
			updateError (TST_SEV1) ;
			SetResult(FALSE);
		}
	}

    	while (nTrials){//now try calling CardGetStatus for nTrials times

        	rtnCode = CardGetStatus(pStatus);

        	if (rtnCode != CERR_SUCCESS){//FAILED
            		g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetStatus<--      FAIL: Thread %u, Socket %u -- return: %s\r\n"), 
            								dwThreadID, uLocalSock, FindStatusName(rtnCode)) ;
            		updateError (TST_SEV2) ;
            		SetResult(FALSE);
        	}
        	else{
          		if (pStatus->fCardState == 0 && pStatus->fSocketState == 0){
	            		g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetStatus<--      FAIL: Thread %u, Socket %u: card does not exist"), 
	            								dwThreadID, uLocalSock) ;
	            		updateError (TST_SEV2) ;
	            		SetResult(FALSE);
          		}
          		else
	            		g_pKato->Log(LOG_DETAIL,TEXT("-->CardGetStatus<--    PASS: Thread %u, Socket %u"), 
	            							dwThreadID, uLocalSock) ;
          	}
        	nTrials-- ;
      }

    delete pStatus ;
    DEBUGMSG(ZONE_FUNCTION, (TEXT("- MaskTest::CardGetStatusTest()\r\n")));

}


VOID MaskTest::CardEventMasksTest (){
    	int	tries;
    	UINT16	uRandMask;

	fMask = 0xFFFF;
	callback = XYZCallback;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ MaskTest::CardEventMaskTest() enterted\r\n")));
	
	TestRegisterClient();
	ReleaseMask(); //make attribute to be Global

	//test global mask functionalities
	tries = nTrials;
	while(tries){
		GetEventMask();
		//Sleep(300);
		uRandMask = (UINT16)(rand() & 0xFFFF);
		SetEventMask(uRandMask);
		//Sleep(300);
		tries --;
	}

	RequestMask(0xFFFF); // set attribute to local

	//test global mask functionalities
	tries = nTrials;
	while(tries){
		GetEventMask();
	//	Sleep(300);
		uRandMask = (UINT16)(rand() & 0xFFFF);
		SetEventMask(uRandMask);
	//	Sleep(300);
		tries --;
	}

	CleanupSettings();
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- MaskTest::CardEventMaskTest()\r\n")));

}

VOID MaskTest::CardResetFunctionTest(){
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ MaskTest::CardResetFunctionTest() enterted\r\n")));

	fMask = 0xFFFF;
	callback = XYZCallback;

	TestRegisterClient();

	//test reset functionality
	ResetFunction();

	CleanupSettings();
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- MaskTest::CardResetFunctionTest()\r\n")));
}

VOID MaskTest::CardFunctionsTest(){
    	int	tries;
    	UINT16	uRandMask;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ MaskTest::CardFunctionsTest() enterted\r\n")));

	fMask = 0xFFFF;
	callback = XYZCallback;
	TestRegisterClient();

	//now go through mask-related functionalities
	RequestMask(0xFFFF);
	ReleaseMask(); 
	RequestMask(0xB7B7);
	RequestMask(0xCC00);
	
	//test global mask functionalities
	tries = nTrials;
	while(tries){
		uRandMask = (UINT16)(rand() & 0xFFFF);
		SetEventMask(uRandMask);
		GetEventMask();
		uRandMask = (UINT16)(rand() & 0xFFFF);
		RequestMask(uRandMask);
		ReleaseMask();
		GetStatus();
		ResetFunction();
		Generic(0);
		tries --;
	}

	CleanupSettings();
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- MaskTest::CardFunctionsTest()\r\n")));

}



VOID MaskTest::EventMask_InvalidParaTest (){
    	CARD_EVENT_MASK_PARMS MaskParms;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ MaskTest::EventMask_InvalidParaTest() enterted\r\n")));

	callback = XYZCallback;
	TestRegisterClient();

	//test CardGetEventMask()
	//try null MaskParms
    	stat = CardGetEventMask(hClient, NULL);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardGetEventMask: Thread %u for Socket %u: Null MaskParms, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try null client
	MaskParms.hSocket.uSocket = (UCHAR)uLocalSock ;
    	MaskParms.hSocket.uFunction = (UCHAR)uLocalFunc ;
    	MaskParms.fAttributes = 0xFFFF ;
    	stat = CardGetEventMask(NULL, &MaskParms);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardGetEventMask: Thread %u for Socket %u: Null client, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try wrong socket number
	MaskParms.hSocket.uSocket = 3;
    	MaskParms.fAttributes = 0xFFFF ;
    	stat = CardGetEventMask(hClient, &MaskParms);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardGetEventMask: Thread %u for Socket %u: wrong socket number, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try wrong function number
	MaskParms.hSocket.uSocket = (UCHAR)uLocalSock ;
    	MaskParms.hSocket.uFunction = 3 ;
    	MaskParms.fAttributes = 0xFFFF ;
    	stat = CardGetEventMask(hClient, &MaskParms);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardGetEventMask: Thread %u for Socket %u: wrong function number, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//test CardSetEventMask()
	//try null MaskParms
    	stat = CardSetEventMask(hClient, NULL);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardSetEventMask: Thread %u for Socket %u: Null MaskParms, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try null client
	MaskParms.hSocket.uSocket = (UCHAR)uLocalSock ;
    	MaskParms.hSocket.uFunction = (UCHAR)uLocalFunc ;
    	MaskParms.fAttributes = 0xFFFF ;
    	stat = CardSetEventMask(NULL, &MaskParms);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardSetEventMask: Thread %u for Socket %u: Null client, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try wrong socket number
	MaskParms.hSocket.uSocket = 3;
    	MaskParms.fAttributes = 0xFFFF ;
    	stat = CardSetEventMask(hClient, &MaskParms);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardSetEventMask: Thread %u for Socket %u: wrong socket number, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try wrong function number
	MaskParms.hSocket.uSocket = (UCHAR)uLocalSock ;
    	MaskParms.hSocket.uFunction = 3 ;
    	MaskParms.fAttributes = 0xFFFF ;
    	stat = CardSetEventMask(hClient, &MaskParms);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardSetEventMask: Thread %u for Socket %u: wrong function number, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
	}

CLEANUP:	
	CleanupSettings();
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- MaskTest::EventMask_InvalidParaTest()\r\n")));

}




VOID MaskTest::SocketMask_InvalidParaTest (){
    	UINT16	uMask;
	CARD_SOCKET_HANDLE		hLocalSocket;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ MaskTest::SocketMask_InvalidParaTest() enterted\r\n")));

	callback = XYZCallback;
	TestRegisterClient();

	//test CardRequestSocketMask()
	//try null client
	hLocalSocket.uSocket = (UCHAR)uLocalSock ;
    	hLocalSocket.uFunction = (UCHAR)uLocalFunc ;
    	uMask = 0xFFFF ;
    	stat = CardRequestSocketMask(NULL, hLocalSocket, uMask);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardRequestSocketMask: Thread %u for Socket %u: Null client, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try wrong socket number
	hLocalSocket.uSocket = 3 ;
    	hLocalSocket.uFunction = (UCHAR)uLocalFunc ;
    	uMask = 0xFFFF ;
    	stat = CardRequestSocketMask(hClient, hLocalSocket, uMask);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardRequestSocketMask: Thread %u for Socket %u: wrong socket number, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try wrong function number
	hLocalSocket.uSocket = (UCHAR)uLocalSock;
    	hLocalSocket.uFunction = 3;
    	uMask = 0xFFFF ;
    	stat = CardRequestSocketMask(hClient, hLocalSocket, uMask);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardRequestSocketMask: Thread %u for Socket %u: wrong function number, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//test CardReleaseSocketMask()
	//try null client
	hLocalSocket.uSocket = (UCHAR)uLocalSock ;
    	hLocalSocket.uFunction = (UCHAR)uLocalFunc ;
    	uMask = 0xFFFF ;
    	stat = CardReleaseSocketMask(NULL, hLocalSocket);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardReleaseSocketMask: Thread %u for Socket %u: Null client, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try wrong socket number
	hLocalSocket.uSocket = 3 ;
    	hLocalSocket.uFunction = (UCHAR)uLocalFunc ;
    	uMask = 0xFFFF ;
    	stat = CardReleaseSocketMask(hClient, hLocalSocket);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardReleaseSocketMask: Thread %u for Socket %u: wrong socket number, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try wrong function number
	hLocalSocket.uSocket = (UCHAR)uLocalSock;
    	hLocalSocket.uFunction = 3;
    	uMask = 0xFFFF ;
    	stat = CardReleaseSocketMask(hClient, hLocalSocket);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardReleaseSocketMask: Thread %u for Socket %u: wrong function number, should NOT return CERR_SUCCESS"),
				 dwThreadID, uLocalSock);
		SetResult(FALSE);
		goto CLEANUP;
	}
	
CLEANUP:	
	CleanupSettings();

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- MaskTest::SocketMask_InvalidParaTest()\r\n")));

}

VOID MaskTest::TestRegisterClient(){
    	CARD_REGISTER_PARMS *pcrParm = new CARD_REGISTER_PARMS ;
    	CARD_SOCKET_HANDLE hSocket ;

      	pcrParm->fAttributes  = CLIENT_ATTR_IO_DRIVER|CLIENT_ATTR_NOTIFY_SHARED;
      	pcrParm->fEventMask   = fMask ;
      	pcrParm->uClientData = 0 ;
    	hClient = CardRegisterClient(callback, pcrParm);
    	stat = GetLastError () ;
    	delete pcrParm ;

    	if (hClient){ // This can succeed even with a bad socket.
        	g_pKato->Log(LOG_DETAIL,TEXT("CardRegisterClient()     SUCCEEDed:  hClient=0x%lx -- ID%d, Sock%d\r\n"),
                       hClient, dwThreadID, uLocalSock);
      }
    	else{
        	g_pKato->Log(LOG_DETAIL,TEXT("CardRegisterClient()     FAILed: Error code: %s -- ID%d, Sock%d\r\n"),
                       					FindStatusName(stat),  dwThreadID, uLocalSock) ;
        	updateError (TST_SEV1) ;
        	SetResult(FALSE);
      }

	//setmask
    	hSocket.uSocket = (UCHAR)uLocalSock ;
    	hSocket.uFunction = (UCHAR)uLocalFunc ;
    	CardReleaseSocketMask(hClient, hSocket);
    	fAttr = 0 ; 
    	SetEventMask (fMask) ;
    	// Remain fLocalExpMask fAttr
    	CardRequestSocketMask(hClient, hSocket, fMask);
    	fAttr = 1 ; 
    	SetEventMask (fMask) ;
    	fAttr = 1;


}
VOID MaskTest::CleanupSettings(){
    	if (hClient){ // If this fails I don't care - I only want deregister to work.
        	CARD_SOCKET_HANDLE hSocket ;
        	hSocket.uSocket = (UCHAR)uLocalSock ;
        	hSocket.uFunction = (UCHAR)uLocalFunc ;
        	CardReleaseSocketMask(hClient, hSocket);
        }
    	
        stat = CardDeregisterClient(hClient);
        if (CERR_SUCCESS == stat){
            	g_pKato->Log(LOG_DETAIL,TEXT("CardDeregisterClient()   SUCCEEDed:  hClient=0x%lx -- ID%d, Sock %d\r\n"),
                           hClient, dwThreadID, uLocalSock);
        }
        else{
            g_pKato->Log(LOG_DETAIL,TEXT("CardDeregisterClient()   FAILed: Error code: %s hClient=0x%lx -- ID%d Sock %d\r\n"),
                           RtnCodes[stat], hClient, dwThreadID, uLocalSock);
            updateError (TST_SEV1) ;
            SetResult(FALSE);
        }
}
	
