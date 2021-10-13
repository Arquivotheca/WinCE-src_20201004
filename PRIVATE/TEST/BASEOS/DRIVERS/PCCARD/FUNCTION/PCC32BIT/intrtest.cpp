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

VOID IntrTest::Test_CardRequestIRQ(){
	STATUS status;
	CARD_SOCKET_HANDLE hSocket = {m_uLocalSock, m_uLocalFunc};
	UINT32	uParsedItems = 0;
	UINT32	uParsedSize = 0;
	PVOID	parsedBuf;
	PARSED_CBCFTABLE* pTable = NULL;
	UINT16 uIrqMask = 0xFFFF;
	DWORD	dwIrq = -1, dwSysIrq = -1;
	
	if(CardGetParsedTuple(hSocket, CISTPL_CFTABLE_ENTRY_CB, 0, &uParsedItems) != CERR_SUCCESS){
		DEBUGMSG(ZONE_ERROR, (TEXT("CardGetParsedTuple call failed")));
		SetResult(FALSE);
		return;
		
	}
	
      uParsedSize = uParsedItems * sizeof (PARSED_CBCFTABLE) ;
	if (uParsedSize){
             parsedBuf = new UCHAR [uParsedSize] ;
             if (parsedBuf){
              	CardGetParsedTuple(hSocket, CISTPL_CFTABLE_ENTRY_CB, parsedBuf, &uParsedItems);
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

	PVOID pCur = parsedBuf;
	BOOL fSucceeded = FALSE;
	//normal request IRQ
	for(UINT i = 0; i < uParsedItems; i++){
		pTable = (PARSED_CBCFTABLE *)pCur;
		if(pTable->wIrqMask != 0){// I/O config with valid irq mask
                CARD_CONFIG_INFO     		confInfo = {0};
                //calling CardRequestCOnfiguration using normal parameters
                confInfo.hSocket.uFunction = m_uLocalFunc;
                confInfo.hSocket.uSocket = m_uLocalSock;
                confInfo.fInterfaceType = CFG_IFACE_MEMORY_IO;
                confInfo.fAttributes = CFG_ATTR_IRQ_STEERING;
                confInfo.fRegisters = 0xFF;
                confInfo.fExtRegisters = 0xFF;
            	   confInfo.uVcc=((pTable->VccDescr.ValidMask &1)?(UINT8)pTable->VccDescr.NominalV:(UINT)-1);
        	   confInfo.uVpp1=((pTable->Vpp1Descr.ValidMask &1)?(UINT8)pTable->Vpp1Descr.NominalV:(UINT)-1);
        	   confInfo.uVpp2=((pTable->Vpp2Descr.ValidMask &1)?(UINT8)pTable->Vpp2Descr.NominalV:(UINT)-1);
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
         		pCur = (PBYTE)pCur + sizeof(PARSED_CBCFTABLE);
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
         		pCur = (PBYTE)pCur + sizeof(PARSED_CBCFTABLE);
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
        	pCur = (PBYTE)pCur + sizeof(PARSED_CBCFTABLE);
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
		NKDbgPrintfW(_T("Request IRQ line succeeded, IrqMask = 0x%x, IRQ = %u, Old IRQ= %d"), uIrqMask, dwIrq, dwOldIrq);
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: The secound request IRQLine call should at least return the same IRQ!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		//sth wrong
		CardReleaseIRQ(g_hClient, hSocket);
		return;
	}

	//release IRQ using invalid socket number, should fail
	hSocket.uSocket= m_uLocalSock + TEST_MAX_CARDS;
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardReleaseIRQ(),  Thread %u for Socket %u Func %u: Release IRQ using invalid socket number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uSocket = m_uLocalSock;
	
	//relase IRQ using invalid function number, should fail
	hSocket.uFunction = m_uLocalFunc + TEST_MAX_CARDS;
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardReleaseIRQ(),  Thread %u for Socket %u Func %u: Release IRQ using invalid function number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uFunction = m_uLocalFunc;

	//release IRQ
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: Release IRQ failed!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}

	
	//use invalid socket number, should fail
	hSocket.uSocket= m_uLocalSock + TEST_MAX_CARDS;
	status = CardRequestIRQLine(g_hClient, hSocket, uIrqMask, &dwIrq, &dwSysIrq);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: Request IRQ using invalid socket number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uSocket = m_uLocalSock;
	
	//use invalid function number, should fail
	hSocket.uFunction = m_uLocalFunc + TEST_MAX_CARDS;
	status = CardRequestIRQLine(g_hClient, hSocket, uIrqMask, &dwIrq, &dwSysIrq);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: Request IRQ using invalid function number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uFunction = m_uLocalFunc;

	//null client, should fail
	status = CardRequestIRQLine(NULL, hSocket, uIrqMask, &dwIrq, &dwSysIrq);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: Request IRQ using NULL clientshould fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}

	//use 0 irq mask, should fail
	hSocket.uFunction = m_uLocalFunc + TEST_MAX_CARDS;
	status = CardRequestIRQLine(g_hClient, hSocket, 0, &dwIrq, &dwSysIrq);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: Request IRQ using zero irq mask should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	
	//release again, should fail
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardReleaseIRQ(),  Thread %u for Socket %u Func %u: The secound release IRQ call should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
	}

}
