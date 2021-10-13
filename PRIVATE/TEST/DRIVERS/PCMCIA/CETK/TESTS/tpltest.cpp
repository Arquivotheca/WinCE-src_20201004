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
#include "scardtpl.h"
#include "TplTest.h"

BOOL
TplTest::Init(){
	LPTPLTESTPARAMS  lparm = NULL;

	if(dwCaseID > 100){
		lparm = &tplarr[0];
	}
	else {
		lparm = &tplarr[dwCaseID-1];
	}
	
	nSeed = lparm->nSeed;
	nTrials = lparm->nTrials;

	pMatchedCard = NULL;
	pData = NULL;
	
	return TRUE;
}

DWORD TplTest::ThreadRun() {

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::ThreadRun() enterted\r\n")));

	//register client first
    	CARD_REGISTER_PARMS *pcrParm = new CARD_REGISTER_PARMS ;
    	pcrParm->fAttributes  = CLIENT_ATTR_IO_DRIVER|CLIENT_ATTR_NOTIFY_SHARED;
    	pcrParm->fEventMask   = 0 ;
    	pcrParm->uClientData  = 0 ;
    	hClient = CardRegisterClient(0, pcrParm);
    	delete pcrParm ;

    	g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u --CardRegisterClient got the hClient 0x%lx\r\n"),
  									dwThreadID, uLocalSock, (long)hClient) ;
    	if (hClient == NULL){
		g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u --CardRegisterClient Failed!\r\n"),
  									dwThreadID, uLocalSock) ;
		SetResult(FALSE);
		return 0;
	}

	//Some specail test cases are designed in order to cover some code which can not be covered 
	//by those nomral test cases.
	switch(dwCaseID){
		case 101://additional test for "CardGetFirstTuple"
			if(AddiTst_GetFirstTuple() == FALSE){
				SetResult(FALSE);
			}
			goto CLEANUP;
		case 102://additional test for "CardGetNextTuple"
			if(AddiTst_GetNextTuple() == FALSE){
				SetResult(FALSE);
			}
			goto CLEANUP;
		case 103://additional test for "CardGetTupleData"
			if(AddiTst_GetTupleData() == FALSE){
				SetResult(FALSE);
			}
			goto CLEANUP;
		default: //normal test cases
			break;
	}
	
	//run those normal tests
	pMatchedCard = new MatchedCard ((UCHAR)uLocalSock, (UCHAR)uLocalFunc) ;
	pData = (PCARD_DATA_PARMS) new UINT [MAX_PCARD_DATA_PARMS] ;
	pTuple = (PCARD_TUPLE_PARMS) pData ;
	ResetCardTupleParms () ;
    	lastCalled = LAST_NONE ; // Force a reset.
	while(nTrials){
    		MakeACall () ; //run one trial
    		DumpCurrentErrors () ;
    		nTrials --;
	}

#ifdef DEBUG
	Dump();
#endif

CLEANUP:
	//cleanup
	 if (pMatchedCard) 
	 	delete pMatchedCard ;
	 if (pData)        
	 	delete pData ;
	 if (hClient) 
	 	CardDeregisterClient(hClient);

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::ThreadRun()\r\n")));
	
	return 0;
}

VOID TplTest::ResetCardTupleParms (){
    	// Initialize the data in pTuple.
    	(pTuple->hSocket).uSocket = (UCHAR)uLocalSock ;
    	(pTuple->hSocket).uFunction = (UCHAR)uLocalFunc ;
    	pTuple->fAttributes = 0xFF ;
    	pTuple->uDesiredTuple = CISTPL_CONFIG; //0xFF ;
    	pTuple->uReserved = 0 ;
    	pTuple->fFlags = 0 ;
    	pTuple->uLinkOffset = 0 ;
    	pTuple->uCISOffset = 0 ;
    	pTuple->uTupleCode = 0 ;
    	pTuple->uTupleLink = 0 ;
    
    	// Finish initializing pMatchedCard.
    	STATUS status  = CERR_SUCCESS;
	UINT   iTry;

	for (iTry = 0; iTry < MAX_TRY; iTry++){
		status = CallCardServices(CardGetFirstTuple)(pTuple);
		if (status != CERR_READ_FAILURE)  		           // In multi threads situation
			break;
		Sleep (1000);
	}

	lastCalled = (status == CERR_SUCCESS) ? LAST_FIRST : LAST_NONE ;


    	// out the unreported return from this ::CardGetFirstTuple call.
    	if (lastCalled == LAST_NONE){
        	if (status < 0) status = 0 ;
        	if (status > 0x23) status = 0x23 ;
        	g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u--CardGetFirstTuple returned %s\r\n"),
          							dwThreadID, uLocalSock, FindStatusName(status)) ;
        	SetResult(FALSE);
      }
}

VOID TplTest::MakeACall (){

    	if (lastCalled == LAST_NONE)
    		ResetCardTupleParms () ;
    	if (lastCalled == LAST_NONE){//failed
        	g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> Could not reset CARD_TUPLE_PARMS  -- Thread%d, Socket %d\r\n"), 
        							dwThreadID, uLocalSock);
        	updateError (TST_SEV2);
        	SetResult(FALSE);
        	return;
      }

	UINT uRandNo =	2;// ((UINT) rand()) % 4;
    	switch (uRandNo){//exercise one of the following functionality
        	case 0: 
        		CardGetFirstTuple () ;  
        		break ;
        	case 1: 
        		CardGetNextTuple () ;   
        		break ;
        	case 2: 
        		CardGetTupleData () ;   
        		break ;
        	case 3: 
        		CardGetParsedTuple () ; 
        		break ;
        	default: break ;
      	}
}

VOID TplTest::DumpCurrentErrors (){

	TCHAR *softStr;
	TCHAR *hardStr;

	UINT32 w, x, y ; 
	pMatchedCard->getMismatchedParams (&w, &x, &y) ;

    	switch (w){
        	case 0: // STATUS
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> Soft status: %s  --  Hard status: %s -- Thread%d, Sock %d\r\n"),
              							FindStatusName(x), FindStatusName(y), dwThreadID, uLocalSock) ;
          		updateError (TST_SEV2) ;
          		SetResult(FALSE);
          		break;

        	case 1: // Tuple code
          		softStr = findTupleNameStrEx (x) ;
          		hardStr = findTupleNameStrEx (y) ;
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> Soft card code: %s  --  Hard card code: %s -- Thread%d, Sock %d\r\n"),
              							softStr, hardStr, dwThreadID, uLocalSock) ;
          		updateError (TST_SEV2) ;
          		SetResult(FALSE);
          		break ;

        	case 2: // Tuple Link
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED  --> Soft tuple link: %ld  --  Hard tuple link: %ld -- Thread%d, Sock %d\r\n"),
              							x, y, dwThreadID, uLocalSock) ;
          		updateError (TST_SEV2) ;
          		SetResult(FALSE);
          		break ; 

        	case 3: // Data Length
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> Soft data length: %ld  --  Hard data length: %ld -- Thread%d, Sock %d\r\n"),
              							x, y, dwThreadID, uLocalSock) ;
          		updateError (TST_SEV2) ;
          		SetResult(FALSE);
          		break ; 

        	case 4: // Data mismatch
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> Tuple data does not match -- Both lengths: %ld -- Thread%d, Sock %d\r\n"),
              							x, dwThreadID, uLocalSock) ;
          		updateError (TST_SEV2) ;
          		SetResult(FALSE);
          		break ; 

        	case 5: // Extra data bytes set.
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> The tuple has %ld extra bytes -- Thread%d, Sock %d\r\n"),
              							y, dwThreadID, uLocalSock) ;
          		updateError (TST_SEV2) ;
          		SetResult(FALSE);
          		break ; 

        	default: // No errors
          		g_pKato->Log(LOG_DETAIL,TEXT("PASSED -- Thread%d, Sock %d\r\n"),
              							dwThreadID, uLocalSock) ;
          		break ; 
      }

}

VOID TplTest::CardGetFirstTuple (){
	UINT uRandNo = ((UINT)rand()) %2;
    	pTuple->fAttributes = uRandNo;
    	uRandNo = ((UINT)rand()) % numberOfTupleCodes();
    	pTuple->uDesiredTuple = TupleCodes[uRandNo].index ;

    	TCHAR *desStr = findTupleNameStrEx (pTuple->uDesiredTuple) ;
    	TCHAR *linkMsg = (pTuple->fAttributes & 1) ? TEXT("accept links") : TEXT("skip links") ;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::CardGetFirstTuple() enterted\r\n")));
    	g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: %s  --  Desired: %s -- Thread%d Sock %d\r\n"),
    							linkMsg, desStr, dwThreadID, uLocalSock) ;

    	STATUS status = pMatchedCard->CardGetFirstTuple (pTuple) ;

	lastCalled = (status == CERR_SUCCESS) ? LAST_FIRST : LAST_NONE ;
	if(pTuple->uDesiredTuple == CISTPL_NULL || pTuple->uDesiredTuple == CISTPL_LONGLINK_A ||pTuple->uDesiredTuple == CISTPL_LONGLINK_C){
		lastCalled = LAST_NONE; //force to reset
		DEBUGMSG(ZONE_VERBOSE, (TEXT("Reset tuple\r\n")));

	}
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::CardGetFirstTuple()\r\n")));

}

VOID TplTest::CardGetNextTuple (){
	UINT uRandNo = ((UINT)rand()) %2;
    	pTuple->fAttributes = uRandNo;
    	uRandNo = ((UINT)rand()) % numberOfTupleCodes();
    	pTuple->uDesiredTuple = TupleCodes[uRandNo].index ;

    	TCHAR *desStr = findTupleNameStrEx (pTuple->uDesiredTuple) ;
    	TCHAR *linkMsg = (pTuple->fAttributes & 1) ? TEXT("accept links") : TEXT("skip links") ;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::CardGetNextTuple() enterted\r\n")));
    	g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetNextTuple: %s  --  Desired: %s -- Thread%d Sock %d\r\n"),
    							linkMsg, desStr, dwThreadID, uLocalSock) ;

    	STATUS status  = CERR_SUCCESS;

	for (UINT iTry = 0; iTry < MAX_TRY; iTry++){
		status = pMatchedCard->CardGetNextTuple (pTuple);
		if (status != CERR_READ_FAILURE)  		           // In multi threads situation
				break;
		Sleep (1000);
	}

    	lastCalled = (status == CERR_SUCCESS) ? LAST_NEXT  : LAST_NONE ;
	if(pTuple->uDesiredTuple == CISTPL_NULL || pTuple->uDesiredTuple == CISTPL_LONGLINK_A ||pTuple->uDesiredTuple == CISTPL_LONGLINK_C){
		lastCalled = LAST_NONE; //force to reset
		DEBUGMSG(ZONE_VERBOSE, (TEXT("Reset tuple\r\n")));
	}
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::CardGetNextTuple()\r\n")));

  }

VOID TplTest::CardGetTupleData (){

    	// Because of resetCardTupleParms() in makeACall()
    	// pTuple->uTupleCode and pTuple->uTupleLink are set. We need to
    	// use these values before guaranteePresence (pTuple)

    	UINT8 tplLength       = pTuple->uTupleLink ;
    	UINT8 uDesiredTuple   = pTuple->uTupleCode ;
    	pData->uDesiredTuple  = uDesiredTuple ;
    	TCHAR *desStr         = findTupleNameStrEx (pData->uDesiredTuple) ;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::CardGetTupleData() enterted\r\n")));

    	// This might destroy pTuple->uTupleCode and pTuple->uTupleLink. (should not)
    	pMatchedCard->guaranteePresence (pTuple) ;

    	// uTupleOffset is the amount of the tuple to skip at the start.
    	pData->uTupleOffset = ((UINT)rand()) % (1+tplLength) ; // 0 <= offset <= tplLength
    	pData->uBufLen      = ((UINT)rand()) % (1+2*tplLength) ; // Maximum = 512
	pData->uCISOffset = pTuple->uCISOffset;
	pData->uLinkOffset = pTuple->uLinkOffset;
	DEBUGMSG(ZONE_VERBOSE, (TEXT("cisoff: %d, linkoff:%d"), pData->uCISOffset, pData->uLinkOffset));

    	// We left pTuple->fFlags and pTuple->uCISOffest alone because
    	// we do not wish to change places. Since we got here,

    	g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetTupleData: Tuple type: %s  -- Offset: %d -- BufLen: %d -- Thread%d, Sock %d\r\n"),
        						desStr, pData->uTupleOffset, pData->uBufLen, dwThreadID, uLocalSock) ;

    	pMatchedCard->CardGetTupleData (pData, MAX_PCARD_DATA_PARMS) ;
    	lastCalled = LAST_NONE ;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::CardGetTupleData()\r\n")));

}

VOID TplTest::CardGetParsedTuple (){
	UINT  iTry;
	UINT  status;
    	CARD_SOCKET_HANDLE hSocket = {(UCHAR)uLocalSock, (UCHAR)uLocalFunc} ;

    	UINT8 uDesiredTuple = (((UINT)rand()) % 2) ? CISTPL_CONFIG : CISTPL_CFTABLE_ENTRY ;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::CardGetParsedTuple() enterted\r\n")));

    	// Guarantee presence if it is needed.
    	(pTuple->hSocket).uSocket = (UCHAR)uLocalSock ;
    	(pTuple->hSocket).uFunction = (UCHAR)uLocalFunc ;
    	pTuple->fAttributes = 0xFF ;
    	pTuple->uDesiredTuple = uDesiredTuple ;
    	pTuple->uReserved = 0 ;
    	pTuple->fFlags = 0 ;
    	pTuple->uLinkOffset = 0 ;
    	pTuple->uCISOffset = 0 ;
    	pTuple->uTupleCode = 0 ;
    	pTuple->uTupleLink = 0 ;
	for (iTry = 0; iTry < MAX_TRY; iTry++){
       	status = CallCardServices(CardGetFirstTuple)(pTuple);
		if (status == CERR_SUCCESS ){
			pMatchedCard->guaranteePresence (pTuple) ;
			break;
		}
		//delay for 1 sec and then retry
		Sleep (1000);
	}

    	TCHAR *desStr = findTupleNameStrEx (uDesiredTuple) ;
    	UINT nItems = pMatchedCard->getMaxParsedItems (hSocket, uDesiredTuple) ;

    	nItems =  ((UINT)rand()) % (1+nItems) ; // 0 <= result <= nItems.

    	if (nItems){ // Slight protective measure -- maybe decrease nItems.
        	switch (uDesiredTuple){
            		case CISTPL_CONFIG:
              		if ((nItems * sizeof (PARSED_CONFIG)) > MAX_PARSE_BUFFER)
                			nItems = (MAX_PARSE_BUFFER/sizeof(PARSED_CONFIG)) / 2 ;
              		break ;
            		case CISTPL_CFTABLE_ENTRY:
              		if ((nItems * sizeof (PARSED_CFTABLE)) > MAX_PARSE_BUFFER)
                			nItems = (MAX_PARSE_BUFFER/sizeof(PARSED_CFTABLE)) / 2 ;
              		break ;
            		default: // This of course never happens.
              		nItems = 0 ;
              		break ;
          }
      }

    	g_pKato->Log(LOG_DETAIL, TEXT("--> CardGetParsedTuple: Tuple type: %s, nItems: %d -- Thread%d, Sock %d  "),
        						desStr, nItems, dwThreadID, uLocalSock) ;

    	PVOID pBuf = (PVOID) new UCHAR [MAX_PARSE_BUFFER] ;
	switch(dwCaseID){
		case 104: // null pointer for nItems
			status = ::CardGetParsedTuple (hSocket, uDesiredTuple, pBuf, NULL) ;
			if(status == CERR_SUCCESS){
				g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetParsedTuple: Thread%d, Sock %d, null pointer for nItems, should NOT return CERR_SUCCESS\r\n"),
									 		dwThreadID, uLocalSock) ;
				status = CERR_BAD_ARGS;
			}
			else
				status = CERR_SUCCESS;
			break;
		case 105: //invalid desired tuple
			uDesiredTuple = CISTPL_END;
			status = ::CardGetParsedTuple (hSocket, uDesiredTuple, pBuf, &nItems) ;
			if(status == CERR_SUCCESS){
				g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetParsedTuple: Thread%d, Sock %d, invalid desired tuple,  should NOT return CERR_SUCCESS\r\n"),
											dwThreadID, uLocalSock, status) ;
				status = CERR_BAD_ARGS;
			}
			else 
				status = CERR_SUCCESS;
			break;
		default:
			status = pMatchedCard->CardGetParsedTuple (hSocket, uDesiredTuple, pBuf, &nItems, MAX_PARSE_BUFFER) ;
			break;
	}
    	delete[] pBuf ;

    	lastCalled = LAST_NONE ;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::CardGetParsedTuple()\r\n")));

}

BOOL TplTest::AddiTst_GetFirstTuple(){
	PCARD_TUPLE_PARMS	pTuple = NULL;
	STATUS				status;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::AddiTst_GetFirstTuple() enterted\r\n")));

	//test 1: input null parameter
	if((status =::CardGetFirstTuple(NULL)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: Thread %d, Sock %d, Null input, should NOT return CERR_SUCCESS\r\n"),
							dwThreadID, uLocalSock) ;
		return FALSE;
	}
	
	pTuple = (PCARD_TUPLE_PARMS) new CARD_TUPLE_PARMS;
	if(pTuple == NULL){
		NKDbgPrintfW(_T("not enough memory!"));
		return FALSE;
	}
	pTuple->uDesiredTuple = CISTPL_END; //that means any tuple
	pTuple->fAttributes = 1;

	//test 2:input wrong socket number
	(pTuple->hSocket).uSocket = 3;//wrong socket number
	(pTuple->hSocket).uFunction = 0;
	if((status =::CardGetFirstTuple(pTuple)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: Thread %d, Sock %d, wrong socket number, should NOT return CERR_SUCCESS\r\n"),
							dwThreadID, uLocalSock);
		delete pTuple;
		return FALSE;
	}
	(pTuple->hSocket).uSocket = (UCHAR)uLocalSock;
	(pTuple->hSocket).uFunction = 3; //wrong function number
	if((status =::CardGetFirstTuple(pTuple)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: Thread %d, Sock %d, wrong function number, should NOT return CERR_SUCCESS\r\n"),
							dwThreadID, uLocalSock);
		delete pTuple;
		return FALSE;
	}

	delete pTuple;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::AddiTst_GetFirstTuple()\r\n")));
	
	return TRUE;
}


BOOL TplTest::AddiTst_GetNextTuple(){
	PCARD_TUPLE_PARMS	pTuple = NULL;
	STATUS				status;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::AddiTst_GetNextTuple() enterted\r\n")));

	//test 1: input null parameter
	if((status =::CardGetNextTuple(NULL)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetNextTuple: Thread %d, Sock %d, Null input, should NOT return CERR_SUCCESS\r\n"),
							dwThreadID, uLocalSock) ;
		return FALSE;
	}
	
	pTuple = (PCARD_TUPLE_PARMS) new CARD_TUPLE_PARMS;
	if(pTuple == NULL){
		NKDbgPrintfW(_T("not enough memory!"));
		return FALSE;
	}
	pTuple->uDesiredTuple = CISTPL_END; //that means any tuple
	pTuple->fAttributes = 1;
	(pTuple->hSocket).uSocket = (UCHAR)uLocalSock;
	(pTuple->hSocket).uFunction = (UCHAR)uLocalFunc;
	if((status =::CardGetFirstTuple(pTuple)) != CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: Thread %d, Sock %d,  FAILed, ret = 0x%x\r\n"),
							dwThreadID, uLocalSock, status) ;
		delete pTuple;
		return FALSE;
	}

	//test 2:input with wrong hsocket info 
	(pTuple->hSocket).uSocket = 3;//wrong socket number
	if((status =::CardGetNextTuple(pTuple)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetNextTuple: Thread %d, Sock %d, wrong socket number, should NOT return CERR_SUCCESS\r\n"),
							dwThreadID, uLocalSock) ;
		delete pTuple;
		return FALSE;
	}
	(pTuple->hSocket).uSocket = (UCHAR)uLocalSock;
	(pTuple->hSocket).uFunction = 3; //wrong function number
	if((status =::CardGetNextTuple(pTuple)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetNextTuple: Thread %d, Sock %d, wrong function number, should NOT return CERR_SUCCESS\r\n"),
							dwThreadID, uLocalSock) ;
		delete pTuple;
		return FALSE;
	}

	delete pTuple;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::AddiTst_GetNextTuple()\r\n")));
	
	return TRUE;
}


#define BUF_SIZE		4096	//for holding the data in tuple

BOOL TplTest::AddiTst_GetTupleData(){

	BYTE  				buf[BUF_SIZE];
	PCARD_TUPLE_PARMS	pTuple = (PCARD_TUPLE_PARMS)buf;
	PCARD_DATA_PARMS	pData = (PCARD_DATA_PARMS)buf;
	STATUS				status;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::AddiTst_GetTupleData() enterted\r\n")));

	//test 1: input null parameter
	if((status =::CardGetTupleData(NULL)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetTupleData: Thread %d, Sock %d, Null input, should NOT return CERR_SUCCESS\r\n"),
							dwThreadID, uLocalSock) ;
		return FALSE;
	}
	
	pTuple->uDesiredTuple = CISTPL_END; //that means any tuple
	pTuple->fAttributes = 1;
	(pTuple->hSocket).uSocket = (UCHAR)uLocalSock;
	(pTuple->hSocket).uFunction = (UCHAR)uLocalFunc;
	if((status =::CardGetFirstTuple(pTuple)) != CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: Thread %d, Sock %d,  FAILed, ret = 0x%x\r\n"),
							dwThreadID, uLocalSock, status) ;
		return FALSE;
	}

	pData->uBufLen = BUF_SIZE - sizeof(CARD_DATA_PARMS);
	pData->uTupleOffset = 0;
	
	//test 2:input with wrong hsocket info 
	(pTuple->hSocket).uSocket = 3;//wrong socket number
	if((status =::CardGetTupleData(pData)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetTupleData: Thread %d, Sock %d, wrong socket number, should NOT return CERR_SUCCESS\r\n"),
							dwThreadID, uLocalSock) ;
		return FALSE;
	}
	(pTuple->hSocket).uSocket = (UCHAR)uLocalSock;
	(pTuple->hSocket).uFunction = 3; //wrong function number
	if((status =::CardGetTupleData(pData)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetTupleData: Thread %d, Sock %d, wrong function number, should NOT return CERR_SUCCESS\r\n"),
							dwThreadID, uLocalSock) ;
		return FALSE;
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::AddiTst_GetTupleData()\r\n")));

	return TRUE;
}

//--------------Helper functions -----------------------------
TCHAR *findStr (CISStrings *is, UINT32 n){
    	if (n > 0xFFU) 
    		return 0 ;
    	for (unsigned int i = 0 ; is[i].index < n ; i++) ;
    	return (n == is[i].index) ? is[i].text : 0 ;
}

TCHAR *findTupleNameStr (UINT32 n){
    	if (n > 0xFFU) return 0 ;
    	for (unsigned int i = 0 ; TupleCodes[i].index < n ; i++) ;
    	return (n == TupleCodes[i].index) ? TupleCodes[i].text : 0 ;
  }

TCHAR *findTupleNameStrEx (UINT32 n){
    	TCHAR *p = findTupleNameStr (n) ;
    	return p ? p : TEXT("Unknown") ;
}

int requiredSupport (CISStrings *is, unsigned int n){ // then & with CARD_IO or CARD_MEMORY
    	if (n > 0xFFU) return 0 ;
    	for (unsigned int i = 0 ; is[i].index < n ; i++) ;
    	return (n == is[i].index) ? is[i].cardTypesSupportingTuple : 0 ;
}

int numberOfTupleCodes () {return 33; }//sizeof(TupleCodes)/sizeof(CISStrings) ;}

long possibleReturns [6] =
{
    CERR_SUCCESS,         // 0x00
    CERR_READ_FAILURE,    // 0x09
    CERR_BAD_SOCKET,      // 0x0B  hSocket does not name a socket
    CERR_BAD_ARGS,        // 0x1C  if pParms == 0
    CERR_NO_MORE_ITEMS,   // 0x1F
    CERR_OUT_OF_RESOURCE  // 0x20  no window is mapped to attribute space on this socket
} ;

int isPossibleReturn (long n){
    	for (int i = 0 ; i < 6 ; i++) if (n == possibleReturns[i]) return 1 ;
    	return 0 ;
}

TCHAR *MatchedString [] =
{
    TEXT("Status"),
    TEXT("Tuple Code"),
    TEXT("Tuple Link (length)"),
    TEXT("Data Length"),
    TEXT("Date")
} ;

TCHAR *findMatchedStringAt (int n){
    	if (n >= (sizeof(MatchedString)/sizeof(TCHAR*))) 
    		return TEXT("") ;
    	return MatchedString [n] ;
}

//---------------------- Debugging output functions -------------------------
/*
VOID reportGetFirstTuple (TCHAR *msg, PCARD_TUPLE_PARMS p, STATUS status){
    	TCHAR *pCode = p ? findTupleNameStrEx(p->uTupleCode) : TEXT("Unknown") ;

    	g_pKato->Log(LOG_DETAIL,TEXT(">>CardGetFirstTuple<< %s  --  Return: %s  --  Tuple: %s\r\n"),
            			msg, FindStatusName(status), pCode) ;
}

VOID reportGetNextTuple (TCHAR *msg, PCARD_TUPLE_PARMS p, STATUS status){
    	TCHAR *pCode = p ? findTupleNameStrEx(p->uTupleCode) : TEXT("Unknown") ;

    	g_pKato->Log(LOG_DETAIL,TEXT(">>CardGetNextTuple<< %s  --  Return: %s  --  Tuple: %s\r\n"),
            				msg, FindStatusName(status), pCode) ;
}

VOID reportGetTupleData (TCHAR *msg, PCARD_DATA_PARMS p, STATUS status){
    	TCHAR *pCode = p ? findTupleNameStrEx(p->uDesiredTuple) : TEXT("Unknown") ;

    	g_pKato->Log(LOG_DETAIL,TEXT(">>CardGetTupleData<< %s  --  Return: %s  --  Tuple: %s\r\n"),
            				msg, FindStatusName(status), pCode) ;
}

VOID reportGetParsedTuple (TCHAR *msg, UINT8 uDesiredTuple, UINT nItems, STATUS status){
    	TCHAR *pCode = findTupleNameStrEx(uDesiredTuple) ;

    	g_pKato->Log(LOG_DETAIL,TEXT(">>CardGetParsedTuple<< %s  --  Return: %s  --  nItems: %d -- Tuple: %s\r\n"),
            				msg, FindStatusName(status), pCode) ;
}
*/
// --------------------- Debugging tuple dump function ----------------------

#define TUPLE_FLAG_COMMON       0x0001    // Tuples are in common memory
VOID dumpTuple (UINT8 uTupleCode, UINT8 uTupleLink, UINT16 flags, UINT32 cisOffset, UINT32 nItems){
    	TCHAR *tplCode =  findTupleNameStr (uTupleCode) ;
    	if (!tplCode) tplCode = TEXT("Unknown") ;
    	TCHAR *pSpace = (flags & TUPLE_FLAG_COMMON) ? TEXT("C") : TEXT("A") ;

    	g_pKato->Log(LOG_DETAIL,TEXT("%s ---- %d bytes, ID: %s%ld"),
            tplCode, uTupleLink, pSpace, cisOffset
          ) ;

    	if ((uTupleCode == CISTPL_CONFIG) || (uTupleCode == CISTPL_CFTABLE_ENTRY))
      		g_pKato->Log(LOG_DETAIL,TEXT(" ---- Parsed items: %ld"), nItems) ;

}

