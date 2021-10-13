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
      CARD_CONFIG_INFO     		confInfo = {0};
	UINT32	uParsedItems = 0;
	UINT32	uParsedSize = 0;
	PVOID	parsedBuf;
	PARSED_PCMCFTABLE* pTable = NULL;
	UINT16 uIrqMask = 0xFFFF;
	DWORD dwIrq = -1;
	DWORD dwSysIrq = -1;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ IntrTest::Test_CardRequestIRQ() enter\r\n")));
	
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
