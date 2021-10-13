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
	Intrtest.cpp

Abstract:

    Tests related with Request/Release IRQs 

--*/

#include "testmain.h"
#include "common.h"
#include "IntrTest.h"



BOOL
IntrTest::Init(){

	return TRUE;
}


DWORD IntrTest::ThreadRun() {


	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ IntrTest::ThreadRun() enterted\r\n")));
	
	switch(m_dwCaseID){
		case 1: //test on CardRequestConfiguration
			Test_CardRequestIRQ();
			break;
		default:	
   			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u Function %u: Invalid test case\r\n"), 
   			                                        m_dwThreadID, m_uLocalSock, m_uLocalFunc);
			break;
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- IntrTest::ThreadRun()\r\n")));

	return GetLastError();
}


/* 
@Func ISRFunction is installed by the call to function CardRequestIRQ...
CardServices calls this ISRFunction when....
*/
VOID ISRFunction(UINT32 uContext)
{
	return;
}   // LogDisplayThread

#define TUPLEBUFFER_SIZE  512

VOID IntrTest::Test_CardRequestIRQ(){
	STATUS status;
	CARD_SOCKET_HANDLE hSocket = {m_uLocalSock, m_uLocalFunc};
      CARD_CONFIG_INFO     		confInfo = {0};
	UINT32	uParsedItems = 0;
	UINT32	uParsedSize = 0;
	PVOID	parsedBuf;
	PARSED_PCMCFTABLE* pTable = NULL;
	UINT16 uIrqMask = 0xFFFF;
	DWORD dwIrq = -1;
	DWORD dwSysIrq = -1;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ IntrTest::Test_CardRequestIRQ() enter\r\n")));
	
        // Get Function ID from tuple.
        CARD_TUPLE_PARMS TupleParms;
        UCHAR dataBuffer[TUPLEBUFFER_SIZE + sizeof( CARD_DATA_PARMS )];
        BYTE bFuncID = 0;
        BOOL bFirst = TRUE;
        while( bFuncID == 0 )
        {
            if( FindTuple( hSocket, &TupleParms, bFirst, CISTPL_FUNCID ) &&
                GetTupleData( &TupleParms,
                              dataBuffer,
                              TUPLEBUFFER_SIZE +
                              sizeof( CARD_DATA_PARMS ) ) )
            {
                PCARD_DATA_PARMS pData = ( PCARD_DATA_PARMS ) dataBuffer;
                if( pData->uDataLen >= 1 ) // At lest 4 byte.
                    bFuncID = dataBuffer[sizeof( CARD_DATA_PARMS )];
                bFirst = FALSE;
            }
            else
            {
                DEBUGCHK( FALSE );
                break;
            }
        }

	if(CardGetParsedTuple(hSocket, CISTPL_CFTABLE_ENTRY, 0, &uParsedItems) != CERR_SUCCESS){
		DEBUGMSG(ZONE_ERROR, (TEXT("CardGetParsedTuple call failed")));
		SetResult(FALSE);
		return;
		
	}
	
      uParsedSize = uParsedItems * sizeof (PARSED_PCMCFTABLE) ;
	if (uParsedSize){
             parsedBuf = new UCHAR [uParsedSize] ;
             if (parsedBuf){
              	CardGetParsedTuple(hSocket, CISTPL_CFTABLE_ENTRY, parsedBuf, &uParsedItems);
		}
		else{ 
			SetResult(FALSE);
			return;
              }
	}
	else{
		DEBUGMSG(ZONE_ERROR, (TEXT("no config table entry found")));
		SetResult(FALSE);
		return;
	}

      //if the card is a CF card, we will have to skip.
	if(bFuncID == 4){
	    PVOID pCur = parsedBuf;
	    BOOL fIsCFCard = FALSE;
          for(UINT32 index = 0; index < uParsedItems; index ++){
                  pTable = (PARSED_PCMCFTABLE *)pCur;
                  for(int j =0 ; j < MAX_WINDOWS_RANGES; j++){
                    DWORD dwIOBase = pTable->IOBase[j];
                    if(dwIOBase == 0x1f0 || dwIOBase == 0x3f6 || dwIOBase == 0x170 || dwIOBase == 0x376){
                        g_pKato->Log(LOG_SKIP, TEXT(":Test_CardReleaseIRQ(),  Thread %u for Socket %u Function %u: FunctionID=4, IOBase =0x%x, it is a CF Card, test will skip!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc, dwIOBase);
	                 delete[] parsedBuf;
	                 SetResult(TRUE);
	                 return;
                    }
                  }
                  pCur = (PBYTE)pCur + sizeof(PARSED_PCMCFTABLE);
          }
	}

	PVOID pCur = parsedBuf;
	BOOL fSucceeded = FALSE;
	//normal request IRQ
	for(UINT i = 0; i < uParsedItems; i++){
		pTable = (PARSED_PCMCFTABLE *)pCur;
		if((pTable->IFaceType == 1) && (pTable->wIrqMask != 0)){// I/O config with valid irq mask

                //calling CardRequestCOnfiguration using normal parameters
                confInfo.hSocket.uFunction = m_uLocalFunc;
                confInfo.hSocket.uSocket = m_uLocalSock;
                confInfo.fInterfaceType = CFG_IFACE_MEMORY_IO;
                confInfo.fAttributes = CFG_ATTR_IRQ_STEERING;
                confInfo.fRegisters = 0xFF;
                confInfo.fExtRegisters = 0xFF;
                //don't change power-related parameters
                confInfo.uVcc = (UINT8)(pTable->VccDescr.NominalV);
                confInfo.uVpp1 = (UINT8)(pTable->Vpp1Descr.NominalV);
                confInfo.uVpp2 = (UINT8)(pTable->Vpp2Descr.NominalV);
                //retrieve current registry values
                if(RetrieveRegisterValues(&confInfo, hSocket) == FALSE){
                    g_pKato->Log(LOG_FAIL,TEXT("Socket %u: Function %u: can not retreive configuration register values.\r\n"), 
                                                        m_uLocalSock, m_uLocalFunc);
                    delete[] parsedBuf;
                    SetResult(FALSE);
                    return;
                }

                //now request configuration
                status = CardRequestConfiguration(g_hClient, &confInfo);
                if(status != CERR_SUCCESS){//if it failed, try the next configuation table
                    g_pKato->Log(LOG_DETAIL,TEXT("Socket %u: Function %u: Request config on No. %u config table failed\r\n"), 
                                                        m_uLocalSock, m_uLocalFunc, i);
                    g_pKato->Log(LOG_DETAIL,TEXT("uVcc = %u, uVpp1 = %u, uVpp2 = %u\r\n"), 
                                                       confInfo.uVcc, confInfo.uVpp1, confInfo.uVpp2);
         		pCur = (PBYTE)pCur + sizeof(PARSED_PCMCFTABLE);
         		continue;
                                                      
                }
                else{
                    g_pKato->Log(LOG_DETAIL,TEXT("Socket %u: Function %u: RequestConfig on No. %u config table succeed!\r\n"), 
                                                        m_uLocalSock, m_uLocalFunc, i);
                    g_pKato->Log(LOG_DETAIL,TEXT("uVcc = %u, uVpp1 = %u, uVpp2 = %u\r\n"), 
                                                       confInfo.uVcc, confInfo.uVpp1, confInfo.uVpp2);
                }

                //if Request config succeeded, now try request IRQ
                status = CardRequestIRQLine(g_hClient, hSocket, pTable->wIrqMask, &dwIrq, &dwSysIrq);
                if(status != CERR_SUCCESS){//if failed, try next configuration table
                	g_pKato->Log(LOG_DETAIL,TEXT(":Test_CardRequestIRQLine(),  Thread %u for Socket %u Function %u: request IRQ failed!\r\n"),
                								m_dwThreadID, m_uLocalSock, m_uLocalFunc);
         		pCur = (PBYTE)pCur + sizeof(PARSED_PCMCFTABLE);
         		continue;

                }
                else{
                    uIrqMask = pTable->wIrqMask;
                    g_pKato->Log(LOG_DETAIL, _T("Socket %u: Function %u: Request IRQ line succeeded, IrqMask = 0x%x, IRQ = %u"), 
                                                            m_uLocalSock, m_uLocalFunc, uIrqMask, dwIrq);
                    fSucceeded = TRUE;
                    break;
                }
             }
        	pCur = (PBYTE)pCur + sizeof(PARSED_PCMCFTABLE);
	}

	delete[] parsedBuf;

      if(fSucceeded == FALSE){//failed
            g_pKato->Log(LOG_FAIL,TEXT("Socket %u: Function %u: Can not Request IRQ based on any config table entry.\r\n"), 
                                                m_uLocalSock, m_uLocalFunc);
            SetResult(FALSE);
            return;
      }
      
	//wait for a while, to see whether something unexpected will happen or not
	Sleep(TEST_IDLE_TIME);
	
	//request again, should fail
	DWORD dwOldIrq = dwIrq;
	status = CardRequestIRQLine(g_hClient, hSocket, uIrqMask, &dwIrq, &dwSysIrq);
	if(status == CERR_SUCCESS && (dwOldIrq != dwIrq)){
		g_pKato->Log(LOG_FAIL, _T("Request IRQ line AGAIN, succeeded, IrqMask = 0x%x, IRQ = %u"), uIrqMask, dwIrq);
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Function %u: The secound request IRQLine call should at least return the same IRQ!\r\n"),
									m_dwThreadID, m_uLocalSock);
		SetResult(FALSE);
		//sth wrong
		CardReleaseIRQ(g_hClient, hSocket);
		return;
	}

	//release IRQ using invalid socket number, should fail
	hSocket.uSocket= m_uLocalSock + TEST_MAX_CARDS;
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardReleaseIRQ(),  Thread %u for Socket %u Function %u: Release IRQ using invalid socket number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uSocket = m_uLocalSock;
	
	//relase IRQ using invalid function number, should fail
	hSocket.uFunction = m_uLocalFunc + TEST_MAX_CARDS;
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardReleaseIRQ(),  Thread %u for Socket %u Function %u: Release IRQ using invalid function number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uFunction = m_uLocalFunc;

	//release IRQ
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Function %u: Release IRQ failed!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}

	
	//use invalid socket number, should fail
	hSocket.uSocket= m_uLocalSock + TEST_MAX_CARDS;
	status = CardRequestIRQLine(g_hClient, hSocket, uIrqMask, &dwIrq,  &dwSysIrq);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Funciton %u: Request IRQ using invalid socket number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uSocket = m_uLocalSock;
	
	//use invalid function number, should fail
	hSocket.uFunction = m_uLocalFunc + TEST_MAX_CARDS;
	status = CardRequestIRQLine(g_hClient, hSocket, uIrqMask, &dwIrq, &dwSysIrq);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Funciton %u: Request IRQ using invalid function number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uFunction = m_uLocalFunc;

	//null client, should fail
	status = CardRequestIRQLine(NULL, hSocket, uIrqMask, &dwIrq, &dwSysIrq);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Funciton %u: Request IRQ using NULL clientshould fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}

	//use 0 irq mask, should fail
	hSocket.uFunction = m_uLocalFunc + TEST_MAX_CARDS;
	status = CardRequestIRQLine(g_hClient, hSocket, 0, &dwIrq, &dwSysIrq);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Funciton %u: Request IRQ using zero irq mask should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	
	//release again, should fail
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardReleaseIRQ(),  Thread %u for Socket %u Funciton %u: The secound release IRQ call should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
	}

}

BOOL IntrTest::GetTupleData(PCARD_TUPLE_PARMS pTupleParams,
                                  PVOID pTupleData,
                                  DWORD dwLength,
                                  DWORD dwOffset )
{
    if( pTupleParams && pTupleData && dwLength > sizeof( CARD_DATA_PARMS ))
    {
        memcpy( pTupleData, pTupleParams, sizeof( CARD_TUPLE_PARMS ) );
        PCARD_DATA_PARMS pData = ( PCARD_DATA_PARMS ) pTupleData;
        pData->uBufLen = ( UINT16 ) ( dwLength - sizeof( CARD_DATA_PARMS ) );
        pData->uDataLen = 0;
        pData->uTupleOffset = ( UINT8 ) dwOffset;
        if( CardGetTupleData( pData ) == CERR_SUCCESS )
            return TRUE;
    }
    return FALSE;
}

BOOL IntrTest::FindTuple( CARD_SOCKET_HANDLE hSocket,
                               PCARD_TUPLE_PARMS pTupleParams,
                               BOOL bFirst,
                               UINT8 uDesiredTuple,
                               UINT16 attributes )
{
    if( pTupleParams )
    {
        if( bFirst )
        {
            // Intialize 
            memset( pTupleParams, 0, sizeof( CARD_TUPLE_PARMS ) );
            pTupleParams->hSocket = hSocket;
            pTupleParams->fAttributes = attributes;
            pTupleParams->uDesiredTuple = uDesiredTuple;
            if( CardGetFirstTuple( pTupleParams ) == CERR_SUCCESS )
                return TRUE;
        }
        else if( CardGetNextTuple( pTupleParams ) == CERR_SUCCESS )
        {
            return TRUE;
        }
    }
    return FALSE;
}


