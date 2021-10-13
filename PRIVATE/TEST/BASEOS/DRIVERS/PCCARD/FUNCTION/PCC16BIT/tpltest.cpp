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
	Tpltest.cpp

Abstract:

    Tests related with tuple-related functions

--*/

#include "testmain.h"
#include "common.h"
#include "scardtpl.h"
#include "TplTest.h"

BOOL
TplTest::Init(){
	LPTPLTESTPARAMS  lparm = NULL;

	if(m_dwCaseID > 100){
		lparm = &tplarr[0];
	}
	else {
		lparm = &tplarr[m_dwCaseID-1];
	}
	
	m_nSeed = lparm->nSeed;
	m_nTrials = lparm->nTrials;

	m_pMatchedCard = NULL;

	m_pTplContents = NULL;
	m_pTplContents = new TPLCONTENT[TOTAL_VALID_TUPLECODES+1];
	if(m_pTplContents == NULL){
        	g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> out fo memory  -- Thread%u, Socket %u Funciton %u\r\n"), 
        							m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		return FALSE;
	}

	memset(m_pTplContents, 0, sizeof(TPLCONTENT)*(TOTAL_VALID_TUPLECODES+1));
	if(ReadAllTuples(m_pTplContents, NULL) == FALSE){
        	g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> Failed to read out all tuples on this card  -- Thread%d, Socket %d Funciton %u\r\n"), 
        							m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		return FALSE;
	}
	
	return TRUE;
}

DWORD TplTest::ThreadRun() {

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::ThreadRun() enterted\r\n")));

	//Some specail test cases are designed in order to cover some code which can not be covered 
	//by those nomral test cases.
	switch(m_dwCaseID){
		case 1:
			if(Test_ReadAllTuples() == FALSE){
				SetResult(FALSE);
			}
			goto CLEANUP;
		case 2:
			if(Test_MT_ReadAllTuples() == FALSE){
				SetResult(FALSE);
			}
			goto CLEANUP;
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
	
CLEANUP:

	if(m_pTplContents)
		CleanupTupleContents(m_pTplContents);
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::ThreadRun()\r\n")));
	
	return 0;
}

BOOL TplTest::ReadAllTuples(PTPLCONTENT pTpls, INT *pOrder){

	
	if(pTpls == NULL)
		return FALSE;

	for(int i = 1; i <= TOTAL_VALID_TUPLECODES; i++){
		if(GetOneTypeTuples(pTpls, (pOrder == NULL)?i:pOrder[i]) == FALSE){
			return FALSE;
		}
	}

	return TRUE;
}

BOOL TplTest::GetOneTypeTuples(PTPLCONTENT pTpls, INT loc){

	CARD_TUPLE_PARMS	TupleParms;
	STATUS	status;
		
	if(pTpls == NULL)
		return FALSE;

	UINT8 uNextTplCode = TupleCodes[loc].index;
	pTpls[loc].uTplCode = uNextTplCode;
	
	//find the first tuple
	TupleParms.fAttributes = 0xFF;
	TupleParms.uDesiredTuple = uNextTplCode;
	TupleParms.hSocket.uFunction = (UINT8)m_uLocalFunc;
	TupleParms.hSocket.uSocket = (UINT8)m_uLocalSock;

	status = ::CardGetFirstTuple(&TupleParms);
	if(status != CERR_SUCCESS){
		if(status == CERR_NO_MORE_ITEMS){//no items found
			return TRUE;
		}
		else{
	        	g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u Funciton %u--CardGetFirstTuple returned %s\r\n"),
	          							m_dwThreadID, m_uLocalSock, m_uLocalFunc, FindStatusName(status)) ;
	        	return FALSE;
		}
	}

	do{
		//get data from this tuple
	    	UINT16 size = TupleParms.uTupleLink + sizeof (CARD_DATA_PARMS) ;
	    	if (!size || (size < sizeof(CARD_TUPLE_PARMS))) 
	    		return FALSE ;

	    	PCARD_DATA_PARMS q = (PCARD_DATA_PARMS) new UCHAR [size] ;
	    	if (!q) 
	    		return FALSE;
			
	    	memset (q, 0, size) ;
	    	memcpy (q, &TupleParms, sizeof (CARD_TUPLE_PARMS)) ;
	    	q->uBufLen = TupleParms.uTupleLink ;
	    	q->uDataLen = 0 ;
	    	q->uTupleOffset = 0 ; 
		if(::CardGetTupleData(q) != CERR_SUCCESS){
	        	g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u Funciton %u--CardGetTupleData returned %s\r\n"),
	          							m_dwThreadID, m_uLocalSock, m_uLocalFunc, FindStatusName(status)) ;
			return FALSE;
		}

		//create instance to hold the data
		SCardTuple *pNode = new SCardTuple(TupleParms.uTupleCode, TupleParms.uTupleLink, q); 
		if(pNode == NULL){
			return FALSE;
		}

		//add it to the link list
		SCardTuple* sCurrent = pTpls[loc].sTplHead;
		if(sCurrent == NULL){//this is the head
			pTpls[loc].sTplHead = pNode;
		}
		else{
			while(sCurrent->link != NULL){
				sCurrent = sCurrent->link;
			}
			sCurrent->link = pNode;		
		}
		pTpls[loc].uNumofTpls ++;

		DEBUGMSG(ZONE_VERBOSE, (TEXT("Tuple %s, add one tuple.\r\n"), TupleCodes[loc].text));
		
		status = ::CardGetNextTuple(&TupleParms);
		if(status == CERR_NO_MORE_ITEMS)//done
			return TRUE;
		else if(status != CERR_SUCCESS){
	        	g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u Funciton %u--CardGetNextTuple returned %s\r\n"),
	          							m_dwThreadID, m_uLocalSock, m_uLocalFunc, FindStatusName(status)) ;
			return FALSE;
		}
	}while(TRUE);

	return TRUE;
	
}

VOID TplTest::CleanupTupleContents(PTPLCONTENT pTplContents){

	if(pTplContents == NULL)
		return;

	//delete each link list
	for(int i = 0; i <= TOTAL_VALID_TUPLECODES; i++){
		if(&(pTplContents[i]) == NULL)//ah, end here
			return;
		SCardTuple* sCurNode = pTplContents[i].sTplHead;
		if(sCurNode == NULL)
			continue;
		SCardTuple* sNextNode = sCurNode->link;
		while(sNextNode != NULL){
			delete sCurNode;
			sCurNode = sNextNode;
			sNextNode = sNextNode->link;
		}
	}

	delete[] pTplContents;
	pTplContents = NULL;
}


BOOL TplTest::AreTwoTupleContentsIdentical(PTPLCONTENT pTpl1, PTPLCONTENT pTpl2){

	if((pTpl1 == NULL) || (pTpl2 == NULL)){
		g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u, Function %u--Bad args for AreTwoTupleContentsEqual() call\r\n"),
		                                                m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
		return FALSE;
	}

	for(int i = 0; i <=TOTAL_VALID_TUPLECODES; i++){
		if(&(pTpl1[i])  == NULL || &(pTpl2[i]) == NULL){
			g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u, Function %u--Bad args for AreTwoTupleContentsEqual() call\r\n"),
			                    m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
			return FALSE;
		}

		//two tuplecode should be the same in any cases
		ASSERT(pTpl1[i].uTplCode == pTpl2[i].uTplCode);

		// the number of tuples of the same tuple code should be the same
		if(pTpl1[i].uNumofTpls != pTpl2[i].uNumofTpls){
			g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u, Function %u--number of tuples for %s: %d != %d\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc, TupleCodes[i].text, pTpl1[i].uNumofTpls, pTpl2[i].uNumofTpls) ;
			return FALSE;
		}

		SCardTuple *p = pTpl1[i].sTplHead;
		SCardTuple *q = pTpl2[i].sTplHead;
		for(int j = 0; j < pTpl1[i].uNumofTpls; j++){
			if(p == NULL || q == NULL){
				g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u, Function %u--some tuple for %s is missing!\r\n"),
								m_dwThreadID, m_uLocalSock, m_uLocalFunc, TupleCodes[i].text) ;
				return FALSE;
			}

			//compare tuple code
			if(p->uTupleCode != q->uTupleCode){
				g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u, Function %u-- tuple code %d != %d!\r\n"),
								m_dwThreadID, m_uLocalSock, m_uLocalFunc, p->uTupleCode, q->uTupleCode) ;
				return FALSE;
			}
			//compare data length
			if(p->uTupleLink != q->uTupleLink){
				g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u, Function %u-- date len %d != %d!\r\n"),
								m_dwThreadID, m_uLocalSock, m_uLocalFunc, p->uTupleLink, q->uTupleLink) ;
				return FALSE;
			}
			//compare data
			PBYTE x = (PBYTE)(p->pTupleData); 
			PBYTE y = (PBYTE)(q->pTupleData);
			if(memcmp(x, y, p->uTupleLink)){
				g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u, Function %u-- two tuples' data is not identical\r\n"),\
				                                                m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
				p->dump();
				q->dump();
				return FALSE;
			}
			//compare parsed data, if it exists
			if(p->nParsedItems == 0 && q->nParsedItems == 0){//no parsed data, goto next tuple
				p = p->link;
				q = q->link;
				continue;
			}
			//compare number of parsed items
			if(p->nParsedItems != q->nParsedItems){
				g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u, Function %u-- number of parsed items %d != %dl\r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, p->nParsedItems, q->nParsedItems) ;
				p->dump();
				q->dump();
				return FALSE;
			}
			//compare parsed buffer length
			if(p->parsedSize != q->parsedSize){
				g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u, Function %u-- length of parsed buffer %d != %dl\r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, p->parsedSize, q->parsedSize) ;
				p->dump();
				q->dump();
				return FALSE;
			}
			//compare parsed buffer
			x = (PBYTE)p->parsedBuf;
			y = (PBYTE)q->parsedBuf;
			if(memcmp(x, y, p->parsedSize)){
				g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u, Function %u-- two tuples' data is not identical\r\n"),
				                                       m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
				p->dump();
				q->dump();
				return FALSE;
			}

			p = p->link;
			q = q->link;
		}

	}
	
	return TRUE;
}

VOID TplTest::ResetCardTupleParms (){
    	// Initialize the data in pTuple.
    	(m_pTuple->hSocket).uSocket = (UCHAR)m_uLocalSock ;
    	(m_pTuple->hSocket).uFunction = (UCHAR)m_uLocalFunc ;
    	m_pTuple->fAttributes = 0xFF ;
    	m_pTuple->uDesiredTuple = CISTPL_CONFIG; //0xFF ;
    	m_pTuple->uReserved = 0 ;
    	m_pTuple->fFlags = 0 ;
    	m_pTuple->uLinkOffset = 0 ;
    	m_pTuple->uCISOffset = 0 ;
    	m_pTuple->uTupleCode = 0 ;
    	m_pTuple->uTupleLink = 0 ;
    
    	// Finish initializing pMatchedCard.
    	STATUS status  = CERR_SUCCESS;
	UINT   iTry;

	for (iTry = 0; iTry < MAX_TRY; iTry++){
		status = ::CardGetFirstTuple(m_pTuple);
		if (status != CERR_READ_FAILURE)  		           // In multi threads situation
			break;
		Sleep (1000);
	}

	m_lastCalled = (status == CERR_SUCCESS) ? LAST_FIRST : LAST_NONE ;


    	// out the unreported return from this ::CardGetFirstTuple call.
    	if (m_lastCalled == LAST_NONE){
        	if (status < 0) status = 0 ;
        	if (status > 0x23) status = 0x23 ;
        	g_pKato->Log(LOG_DETAIL,TEXT("Thread %u, Socket %u Funciton %u--CardGetFirstTuple returned %s\r\n"),
          							m_dwThreadID, m_uLocalSock, m_uLocalFunc, FindStatusName(status)) ;
        	SetResult(FALSE);
      }
}

VOID TplTest::MakeACall (){

    	if (m_lastCalled == LAST_NONE)
    		ResetCardTupleParms () ;
    	if (m_lastCalled == LAST_NONE){//failed
        	g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> Could not reset CARD_TUPLE_PARMS  -- Thread%u, Socket %u\r\n"), 
        							m_dwThreadID, m_uLocalSock, m_uLocalFunc);
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
	m_pMatchedCard->getMismatchedParams (&w, &x, &y) ;

    	switch (w){
        	case 0: // STATUS
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> Soft status: %s  --  Hard status: %s -- Thread%d, Sock %d, Func %d\r\n"),
              							FindStatusName(x), FindStatusName(y), m_dwThreadID, m_uLocalSock, m_uLocalFunc);
          		SetResult(FALSE);
          		break;

        	case 1: // Tuple code
          		softStr = findTupleNameStrEx (x) ;
          		hardStr = findTupleNameStrEx (y) ;
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> Soft card code: %s  --  Hard card code: %s -- Thread%d, Sock %d, Func %d\r\n"),
              							softStr, hardStr, m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
          		SetResult(FALSE);
          		break ;

        	case 2: // Tuple Link
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED  --> Soft tuple link: %ld  --  Hard tuple link: %ld -- Thread%d, Sock %d, Func %d\r\n"),
              							x, y, m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
          		SetResult(FALSE);
          		break ; 

        	case 3: // Data Length
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> Soft data length: %ld  --  Hard data length: %ld -- Thread%d, Sock %d, Func %d\r\n"),
              							x, y, m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
          		SetResult(FALSE);
          		break ; 

        	case 4: // Data mismatch
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> Tuple data does not match -- Both lengths: %ld -- Thread%d, Sock %d, Func %d\r\n"),
              							x, m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
          		SetResult(FALSE);
          		break ; 

        	case 5: // Extra data bytes set.
          		g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> The tuple has %ld extra bytes -- Thread%d, Sock %d, Func %d\r\n"),
              							y, m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
          		SetResult(FALSE);
          		break ; 

        	default: // No errors
          		g_pKato->Log(LOG_DETAIL,TEXT("PASSED -- Thread%d, Sock %d Func %d\r\n"),
              							m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
          		break ; 
      }

}

VOID TplTest::CardGetFirstTuple (){
	UINT uRandNo = ((UINT)rand()) %2;
    	m_pTuple->fAttributes = uRandNo;
    	uRandNo = ((UINT)rand()) % numberOfTupleCodes();
    	m_pTuple->uDesiredTuple = TupleCodes[uRandNo].index ;

    	TCHAR *desStr = findTupleNameStrEx (m_pTuple->uDesiredTuple) ;
    	TCHAR *linkMsg = (m_pTuple->fAttributes & 1) ? TEXT("accept links") : TEXT("skip links") ;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::CardGetFirstTuple() enterted\r\n")));
    	g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: %s  --  Desired: %s -- Thread%d Sock %d Func %d\r\n"),
    							linkMsg, desStr, m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;

    	STATUS status = m_pMatchedCard->CardGetFirstTuple (m_pTuple) ;

	m_lastCalled = (status == CERR_SUCCESS) ? LAST_FIRST : LAST_NONE ;
	if(m_pTuple->uDesiredTuple == CISTPL_NULL || m_pTuple->uDesiredTuple == CISTPL_LONGLINK_A ||m_pTuple->uDesiredTuple == CISTPL_LONGLINK_C){
		m_lastCalled = LAST_NONE; //force to reset
		DEBUGMSG(ZONE_VERBOSE, (TEXT("Reset tuple\r\n")));

	}
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::CardGetFirstTuple()\r\n")));

}

VOID TplTest::CardGetNextTuple (){
	UINT uRandNo = ((UINT)rand()) %2;
    	m_pTuple->fAttributes = uRandNo;
    	uRandNo = ((UINT)rand()) % numberOfTupleCodes();
    	m_pTuple->uDesiredTuple = TupleCodes[uRandNo].index ;

    	TCHAR *desStr = findTupleNameStrEx (m_pTuple->uDesiredTuple) ;
    	TCHAR *linkMsg = (m_pTuple->fAttributes & 1) ? TEXT("accept links") : TEXT("skip links") ;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::CardGetNextTuple() enterted\r\n")));
    	g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetNextTuple: %s  --  Desired: %s -- Thread%d Sock %d Func %d\r\n"),
    							linkMsg, desStr, m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;

    	STATUS status  = CERR_SUCCESS;

	for (UINT iTry = 0; iTry < MAX_TRY; iTry++){
		status = m_pMatchedCard->CardGetNextTuple (m_pTuple);
		if (status != CERR_READ_FAILURE)  		           // In multi threads situation
				break;
		Sleep (1000);
	}

    	m_lastCalled = (status == CERR_SUCCESS) ? LAST_NEXT  : LAST_NONE ;
	if(m_pTuple->uDesiredTuple == CISTPL_NULL || m_pTuple->uDesiredTuple == CISTPL_LONGLINK_A ||m_pTuple->uDesiredTuple == CISTPL_LONGLINK_C){
		m_lastCalled = LAST_NONE; //force to reset
		DEBUGMSG(ZONE_VERBOSE, (TEXT("Reset tuple\r\n")));
	}
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::CardGetNextTuple()\r\n")));

  }

VOID TplTest::CardGetTupleData (){

    	// Because of resetCardTupleParms() in makeACall()
    	// m_pTuple->uTupleCode and m_pTuple->uTupleLink are set. We need to
    	// use these values before guaranteePresence (m_pTuple)

    	UINT8 tplLength       = m_pTuple->uTupleLink ;
    	UINT8 uDesiredTuple   = m_pTuple->uTupleCode ;
    	m_pData->uDesiredTuple  = uDesiredTuple ;
    	TCHAR *desStr         = findTupleNameStrEx (m_pData->uDesiredTuple) ;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::CardGetTupleData() enterted\r\n")));

    	// This might destroy m_pTuple->uTupleCode and m_pTuple->uTupleLink. (should not)
    	m_pMatchedCard->guaranteePresence (m_pTuple) ;

    	// uTupleOffset is the amount of the tuple to skip at the start.
    	m_pData->uTupleOffset = ((UINT)rand()) % (1+tplLength) ; // 0 <= offset <= tplLength
    	m_pData->uBufLen      = ((UINT)rand()) % (1+2*tplLength) ; // Maximum = 512
	m_pData->uCISOffset = m_pTuple->uCISOffset;
	m_pData->uLinkOffset = m_pTuple->uLinkOffset;
	DEBUGMSG(ZONE_VERBOSE, (TEXT("cisoff: %d, linkoff:%d"), m_pData->uCISOffset, m_pData->uLinkOffset));

    	// We left m_pTuple->fFlags and m_pTuple->uCISOffest alone because
    	// we do not wish to change places. Since we got here,

    	g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetTupleData: Tuple type: %s  -- Offset: %d -- BufLen: %d -- Thread%d, Sock %d, Func %d\r\n"),
        						desStr, m_pData->uTupleOffset, m_pData->uBufLen, m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;

    	m_pMatchedCard->CardGetTupleData (m_pData, MAX_PCARD_DATA_PARMS) ;
    	m_lastCalled = LAST_NONE ;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::CardGetTupleData()\r\n")));

}

VOID TplTest::CardGetParsedTuple (){
	UINT  iTry;
	UINT  status;
    	CARD_SOCKET_HANDLE hSocket = {(UCHAR)m_uLocalSock, (UCHAR)m_uLocalFunc} ;

    	UINT8 uDesiredTuple = (((UINT)rand()) % 2) ? CISTPL_CONFIG : CISTPL_CFTABLE_ENTRY ;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::CardGetParsedTuple() enterted\r\n")));

    	// Guarantee presence if it is needed.
    	(m_pTuple->hSocket).uSocket = (UCHAR)m_uLocalSock ;
    	(m_pTuple->hSocket).uFunction = (UCHAR)m_uLocalFunc ;
    	m_pTuple->fAttributes = 0xFF ;
    	m_pTuple->uDesiredTuple = uDesiredTuple ;
    	m_pTuple->uReserved = 0 ;
    	m_pTuple->fFlags = 0 ;
    	m_pTuple->uLinkOffset = 0 ;
    	m_pTuple->uCISOffset = 0 ;
    	m_pTuple->uTupleCode = 0 ;
    	m_pTuple->uTupleLink = 0 ;
	for (iTry = 0; iTry < MAX_TRY; iTry++){
       	status = ::CardGetFirstTuple(m_pTuple);
		if (status == CERR_SUCCESS ){
			m_pMatchedCard->guaranteePresence (m_pTuple) ;
			break;
		}
		//delay for 1 sec and then retry
		Sleep (1000);
	}

    	TCHAR *desStr = findTupleNameStrEx (uDesiredTuple) ;
    	UINT nItems = m_pMatchedCard->getMaxParsedItems (hSocket, uDesiredTuple) ;

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

    	g_pKato->Log(LOG_DETAIL, TEXT("--> CardGetParsedTuple: Tuple type: %s, nItems: %d -- Thread%d, Sock %d Func %d"),
        						desStr, nItems, m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;

    	PVOID pBuf = (PVOID) new UCHAR [MAX_PARSE_BUFFER] ;
	switch(m_dwCaseID){
		case 104: // null pointer for nItems
			status = ::CardGetParsedTuple (hSocket, uDesiredTuple, pBuf, NULL) ;
			if(status == CERR_SUCCESS){
				g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetParsedTuple: Thread%d, Sock %d Func %d, null pointer for nItems, should NOT return CERR_SUCCESS\r\n"),
									 		m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
				status = CERR_BAD_ARGS;
			}
			else
				status = CERR_SUCCESS;
			break;
		case 105: //invalid desired tuple
			uDesiredTuple = CISTPL_END;
			status = ::CardGetParsedTuple (hSocket, uDesiredTuple, pBuf, &nItems) ;
			if(status == CERR_SUCCESS){
				g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetParsedTuple: Thread%d, Sock %d Func %d, invalid desired tuple,  should NOT return CERR_SUCCESS\r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, status) ;
				status = CERR_BAD_ARGS;
			}
			else 
				status = CERR_SUCCESS;
			break;
		default:
			status = m_pMatchedCard->CardGetParsedTuple (hSocket, uDesiredTuple, pBuf, &nItems, MAX_PARSE_BUFFER) ;
			break;
	}
    	delete[] pBuf ;

    	m_lastCalled = LAST_NONE ;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::CardGetParsedTuple()\r\n")));

}

BOOL TplTest::Test_ReadAllTuples(){
	INT* pOrder = NULL;
	PTPLCONTENT pNewTplContent = NULL;

	pOrder = new INT[TOTAL_VALID_TUPLECODES+1];
	pNewTplContent = new TPLCONTENT[TOTAL_VALID_TUPLECODES+1];
	if(pNewTplContent == NULL || pOrder == NULL){
        	g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> out fo memory  -- Thread%d, Socket %d, Func %d\r\n"), 
        							m_dwThreadID, m_uLocalSock, m_uLocalFunc);
             if(pOrder != NULL)
                 delete[] pOrder;
             if(pNewTplContent != NULL)
        	    delete[] pNewTplContent;
		return FALSE;
	}
	memset(pNewTplContent, 0, sizeof(TPLCONTENT)*(TOTAL_VALID_TUPLECODES+1));


	INT j = 1;
	//generate an access order
	for(INT i = 1; i<=TOTAL_VALID_TUPLECODES/2; i++){
		pOrder[i] = TOTAL_VALID_TUPLECODES -j +1;
		pOrder[TOTAL_VALID_TUPLECODES-i+1] = TOTAL_VALID_TUPLECODES - j;
		j += 2;
	}
	if(TOTAL_VALID_TUPLECODES % 2 )
		pOrder[i] = 1;

	//read tuples out
	if(ReadAllTuples(pNewTplContent, pOrder) == FALSE){
        	g_pKato->Log(LOG_DETAIL,TEXT("Thread%d, Socket %d Func %d, Read all tuples failed using alternative order\r\n"), 
        							m_dwThreadID, m_uLocalSock, m_uLocalFunc);
             delete[] pOrder;
    	        delete[] pNewTplContent;
		return FALSE;
	}

	//compare with the default content structure
	if(AreTwoTupleContentsIdentical(m_pTplContents, pNewTplContent) == FALSE){
        	g_pKato->Log(LOG_DETAIL,TEXT("Thread%d, Socket %d, Func %d, compare results of two reads failed\r\n"), 
        							m_dwThreadID, m_uLocalSock, m_uLocalFunc);
             delete[] pOrder;
    	       delete[] pNewTplContent;
		return FALSE;
	}

	CleanupTupleContents(pNewTplContent);
	delete[] pOrder;

	return TRUE;
}

#define TEST_THREAD_NUMBER	4
BOOL TplTest::Test_MT_ReadAllTuples(){

 	HANDLE	hSubThread[TEST_THREAD_NUMBER] = {0};
	UINT8	i;
	BOOL      bRet = TRUE;
	//Launch threads
	for(i = 0; i < TEST_THREAD_NUMBER; i ++){
		THREAD_PARMS TParms = {this, (UINT8)i };
		hSubThread[i] = ::CreateThread(NULL, 0, TplTest::ReadTuplesSubThread, (LPVOID)&TParms, 0, NULL);
		if (hSubThread[i] == NULL) {
			g_pKato->Log(LOG_DETAIL,TEXT("Thread %d, Sock %d Func %d, create thread failed\r\n"),
								m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
			bRet = FALSE;
			break;
		}
    	}

	//close threads
	for(i = 0; i < TEST_THREAD_NUMBER; i ++){
		if(hSubThread[i] != NULL){
        		WaitForSingleObject(hSubThread[i], INFINITE);
			CloseHandle(hSubThread[i]);
		}
    	}

	return bRet;
	
}

DWORD
WINAPI
TplTest::ReadTuplesSubThread(LPVOID dParam){
	DEBUGCHK(dParam != NULL);
	PTHREAD_PARMS pThreadPtr = (PTHREAD_PARMS)dParam;
	TplTest* pTplTestPtr = pThreadPtr->pTplTest;
	DWORD exitCode=pTplTestPtr-> RunReadTuplesSubThreadProc(pThreadPtr->uSubID);
	::ExitThread(exitCode);
	return exitCode;
}

DWORD
TplTest::RunReadTuplesSubThreadProc(UINT8 uSubID){
	INT* pOrder = NULL;
	PTPLCONTENT pNewTplContent = NULL;

	pOrder = new INT[TOTAL_VALID_TUPLECODES+1];
	pNewTplContent = new TPLCONTENT[TOTAL_VALID_TUPLECODES+1];
	if(pNewTplContent == NULL || pOrder == NULL){
        	g_pKato->Log(LOG_DETAIL,TEXT("FAILED --> out fo memory  -- Thread%d, Socket %d Func %d , SubThread %d\r\n"), 
        							m_dwThreadID, m_uLocalSock, m_uLocalFunc, uSubID);
		SetResult(FALSE);
             if(pOrder != NULL)
                 delete[] pOrder;
             if(pNewTplContent != NULL)
        	    delete[] pNewTplContent;
		return -1;
	}
	memset(pNewTplContent, 0, sizeof(TPLCONTENT)*(TOTAL_VALID_TUPLECODES+1));

	INT j = 1;
	if(GenRandomOrder(&pOrder[1], TOTAL_VALID_TUPLECODES) == FALSE){
        	g_pKato->Log(LOG_DETAIL,TEXT("Thread%d, Socket %d, SubThread %d Func %d , can not create a random order\r\n"), 
        							m_dwThreadID, m_uLocalSock, m_uLocalFunc, uSubID);
             delete[] pOrder;
    	      delete[] pNewTplContent;
		SetResult(FALSE);
		return -1;
	}
	
	//read tuples out
	if(ReadAllTuples(pNewTplContent, pOrder) == FALSE){
        	g_pKato->Log(LOG_DETAIL,TEXT("Thread%d, Socket %d Func %d , SubThread %d, Read all tuples failed using alternative order\r\n"), 
        							m_dwThreadID, m_uLocalSock, m_uLocalFunc, uSubID);
             delete[] pOrder;
    	      delete[] pNewTplContent;
		SetResult(FALSE);
		return -1;
	}

	//compare with the default content structure
	if(AreTwoTupleContentsIdentical(m_pTplContents, pNewTplContent) == FALSE){
        	g_pKato->Log(LOG_DETAIL,TEXT("Thread%d, Socket %d Func %d , SubThread %d compare results of two reads failed\r\n"), 
        							m_dwThreadID, m_uLocalSock, m_uLocalFunc, uSubID);
		SetResult(FALSE);
             delete[] pOrder;
    	      delete[] pNewTplContent;
		return -1;
	}

	CleanupTupleContents(pNewTplContent);
	delete[] pOrder;

	return 0;


}

BOOL TplTest::GenRandomOrder(INT* pArr, UINT uLen){
	if(pArr == NULL || uLen == 0)
		return FALSE;
	
	UINT* pHit = new UINT[uLen];
	if(pHit == NULL)
		return FALSE;
	memset(pHit, 0, sizeof(UINT)*uLen);
	for(UINT i = 0; i < uLen; i++){
		UINT uTemp;
		do{
			uTemp = (UINT)rand();
			uTemp = (uTemp % uLen) + 1;
		}while(pHit[uTemp-1] == 1);
		pArr[i] = uTemp;
		pHit[uTemp-1] = 1;
	}

	return TRUE;

}

BOOL TplTest::AddiTst_GetFirstTuple(){
	PCARD_TUPLE_PARMS	m_pTuple = NULL;
	STATUS				status;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::AddiTst_GetFirstTuple() enterted\r\n")));

	//test 1: input null parameter
	if((status =::CardGetFirstTuple(NULL)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: Thread %d, Sock %d Func %d , Null input, should NOT return CERR_SUCCESS\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
		return FALSE;
	}
	
	m_pTuple = (PCARD_TUPLE_PARMS) new CARD_TUPLE_PARMS;
	if(m_pTuple == NULL){
		g_pKato->Log(LOG_FAIL, _T("not enough memory!"));
		return FALSE;
	}
	m_pTuple->uDesiredTuple = CISTPL_END; //that means any tuple
	m_pTuple->fAttributes = 1;

	//test 2:input wrong socket number
	(m_pTuple->hSocket).uSocket = m_uLocalSock + TEST_MAX_CARDS;//wrong socket number
	(m_pTuple->hSocket).uFunction = 0;
	if((status =::CardGetFirstTuple(m_pTuple)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: Thread %d, Sock %d Func %d , wrong socket number, should NOT return CERR_SUCCESS\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		delete m_pTuple;
		return FALSE;
	}
	(m_pTuple->hSocket).uSocket = (UCHAR)m_uLocalSock;
	(m_pTuple->hSocket).uFunction = 0xFF; //wrong function number
	if((status =::CardGetFirstTuple(m_pTuple)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: Thread %d, Sock %d Func %d , wrong function number, should NOT return CERR_SUCCESS\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		delete m_pTuple;
		return FALSE;
	}

	delete m_pTuple;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::AddiTst_GetFirstTuple()\r\n")));
	
	return TRUE;
}


BOOL TplTest::AddiTst_GetNextTuple(){
	PCARD_TUPLE_PARMS	m_pTuple = NULL;
	STATUS				status;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::AddiTst_GetNextTuple() enterted\r\n")));

	//test 1: input null parameter
	if((status =::CardGetNextTuple(NULL)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetNextTuple: Thread %d, Sock %d Func %d , Null input, should NOT return CERR_SUCCESS\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
		return FALSE;
	}
	
	m_pTuple = (PCARD_TUPLE_PARMS) new CARD_TUPLE_PARMS;
	if(m_pTuple == NULL){
		NKDbgPrintfW(_T("not enough memory!"));
		return FALSE;
	}
	m_pTuple->uDesiredTuple = CISTPL_END; //that means any tuple
	m_pTuple->fAttributes = 1;
	(m_pTuple->hSocket).uSocket = (UCHAR)m_uLocalSock;
	(m_pTuple->hSocket).uFunction = (UCHAR)m_uLocalFunc;
	if((status =::CardGetFirstTuple(m_pTuple)) != CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: Thread %d, Sock %d Func %d ,  FAILed, ret = 0x%x\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc, status) ;
		delete m_pTuple;
		return FALSE;
	}

	//test 2:input with wrong hsocket info 
	(m_pTuple->hSocket).uSocket = m_uLocalSock + TEST_MAX_CARDS;//wrong socket number
	if((status =::CardGetNextTuple(m_pTuple)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetNextTuple: Thread %d, Sock %d Func %d , wrong socket number, should NOT return CERR_SUCCESS\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
		delete m_pTuple;
		return FALSE;
	}
	(m_pTuple->hSocket).uSocket = (UCHAR)m_uLocalSock;
	(m_pTuple->hSocket).uFunction = 0xFF; //wrong function number
	if((status =::CardGetNextTuple(m_pTuple)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetNextTuple: Thread %d, Sock %d Func %d , wrong function number, should NOT return CERR_SUCCESS\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
		delete m_pTuple;
		return FALSE;
	}

	delete m_pTuple;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- TplTest::AddiTst_GetNextTuple()\r\n")));
	
	return TRUE;
}


#define BUF_SIZE		4096	//for holding the data in tuple

BOOL TplTest::AddiTst_GetTupleData(){

	BYTE  				*buf = NULL;
	STATUS				status;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ TplTest::AddiTst_GetTupleData() enterted\r\n")));
	buf = (PBYTE)malloc(BUF_SIZE);
	if(buf == NULL){
		g_pKato->Log(LOG_DETAIL,TEXT("-->TplTest::AddiTst_GetTupleData<--   FAIL -- out of memory\r\n")); 
		return FALSE;
	}

	PCARD_TUPLE_PARMS	m_pTuple = (PCARD_TUPLE_PARMS)buf;
	PCARD_DATA_PARMS	m_pData = (PCARD_DATA_PARMS)buf;
	
	//test 1: input null parameter
	if((status =::CardGetTupleData(NULL)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetTupleData: Thread %d, Sock %d Func %d , Null input, should NOT return CERR_SUCCESS\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
		free(buf);
		return FALSE;
	}
	
	m_pTuple->uDesiredTuple = CISTPL_END; //that means any tuple
	m_pTuple->fAttributes = 1;
	(m_pTuple->hSocket).uSocket = (UCHAR)m_uLocalSock;
	(m_pTuple->hSocket).uFunction = (UCHAR)m_uLocalFunc;
	if((status =::CardGetFirstTuple(m_pTuple)) != CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetFirstTuple: Thread %d, Sock %d Func %d,  FAILed, ret = 0x%x\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc, status) ;
		free(buf);
		return FALSE;
	}

	m_pData->uBufLen = BUF_SIZE - sizeof(CARD_DATA_PARMS);
	m_pData->uTupleOffset = 0;
	
	//test 2:input with wrong hsocket info 
	(m_pTuple->hSocket).uSocket = m_uLocalSock + TEST_MAX_CARDS;//wrong socket number
	if((status =::CardGetTupleData(m_pData)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetTupleData: Thread %d, Sock %d Func %d, wrong socket number, should NOT return CERR_SUCCESS\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
		free(buf);
		return FALSE;
	}
	(m_pTuple->hSocket).uSocket = (UCHAR)m_uLocalSock;
	(m_pTuple->hSocket).uFunction = 0xFF; //wrong function number
	if((status =::CardGetTupleData(m_pData)) == CERR_SUCCESS) {
		g_pKato->Log(LOG_DETAIL,TEXT("--> CardGetTupleData: Thread %d, Sock %d Func %d, wrong function number, should NOT return CERR_SUCCESS\r\n"),
							m_dwThreadID, m_uLocalSock, m_uLocalFunc) ;
		free(buf);
		return FALSE;
	}

	free(buf);
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

