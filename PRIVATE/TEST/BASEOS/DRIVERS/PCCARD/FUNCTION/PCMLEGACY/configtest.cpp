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
	Configtest.cpp

Abstract:

    Tests related with PCCard configuration 

--*/

#include "testmain.h"
#include "common.h"
#include "ConfigTest.h"

BOOL
ConfigTest::Init(){
	return TRUE;
}

//defines used by ThreadRun()
#define CONFIGTST_WINSIZE	0x100

DWORD ConfigTest::ThreadRun() {
       if(IsTerminated())
           return GetLastError();
           
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::ThreadRun() enterted\r\n")));

	SetLastError(0);
	
	switch(m_dwCaseID){
		case 1: //test on CardRequestConfiguration
			Test_CardRequestConfiguration();
			break;
		case 2: //normal test on CardModifyConfiguration
			Test_CardModifyConfiguration();
			break;
		case 3: //normal test on CardAccessConfigurationRegister()
			Test_AccessConfigurationRegisters();
			break;
		default:	
   			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u Func %u: Invalid test case\r\n"), 
   			                                        m_dwThreadID, m_uLocalSock, m_uLocalFunc);
			break;
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- ConfigTest::ThreadRun() exit\r\n")));
	
	return GetLastError();
}


BOOL ConfigTest::RetrieveRegisterValues(PCARD_CONFIG_INFO pConfInfo){

	STATUS status = CERR_SUCCESS;
	CARD_SOCKET_HANDLE hSocket = {m_uLocalSock, m_uLocalFunc};
	
	if(pConfInfo == NULL)
		return FALSE;

	//read ConfigReg
	status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 0, &(pConfInfo->uConfigReg));
	if(status != CERR_SUCCESS){
		DEBUGMSG(ZONE_WARNING, (TEXT("Can not access ConfigReg\r\n")));
	}

	//read StatusReg
	status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 1, &(pConfInfo->uStatusReg));
	if(status != CERR_SUCCESS){
		DEBUGMSG(ZONE_WARNING, (TEXT("Can not access StatusReg\r\n")));
	}

	//read PinReg
	status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 2, &(pConfInfo->uPinReg));
	if(status != CERR_SUCCESS){
		DEBUGMSG(ZONE_WARNING, (TEXT("Can not access PinReg\r\n")));
	}

	//read uCopyReg
	status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 3, &(pConfInfo->uCopyReg));
	if(status != CERR_SUCCESS){
		DEBUGMSG(ZONE_WARNING, (TEXT("Can not access CopyReg\r\n")));
	}

	//read uExtendedStatusReg
	status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 4, &(pConfInfo->uExtendedStatus));
	if(status != CERR_SUCCESS){
		DEBUGMSG(ZONE_WARNING, (TEXT("Can not access ExtendedStatusReg\r\n")));
	}

	//read IOBases
	for(int i = 0; i < NUM_EXT_REGISTERS; i++){
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, NUM_REGISTERS+i, &(pConfInfo->IOBase[i]));
		if(status != CERR_SUCCESS){
			DEBUGMSG(ZONE_WARNING, (TEXT("Can not access IOBase no. %d\r\n"), i));
		}
	}

	//read IOLimit
	status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 9, &(pConfInfo->IOLimit));
	if(status != CERR_SUCCESS){
		DEBUGMSG(ZONE_WARNING, (TEXT("Can not access IOLimit\r\n")));
	}

	return TRUE;
	
}

BOOL ConfigTest::TryRequestConfig(PCARD_CONFIG_INFO pConInfo){

    if(pConInfo == NULL)
        return FALSE;
	
    STATUS				status = CERR_SUCCESS;
    UINT32				uParsedItems = 0;
    UINT32				uParsedSize = 0;
    CARD_SOCKET_HANDLE	hSocket = {m_uLocalSock, m_uLocalFunc};
    PVOID					parsedBuf;
	
    if(CardGetParsedTuple(hSocket, CISTPL_CFTABLE_ENTRY, 0, &uParsedItems) != CERR_SUCCESS){
        DEBUGMSG(ZONE_ERROR, (TEXT("CardGetParsedTuple call failed")));
        return FALSE;
    }
	
    uParsedSize = uParsedItems * sizeof (PARSED_CFTABLE) ;
    if (uParsedSize){
        parsedBuf = new UCHAR [uParsedSize] ;
        if (parsedBuf){
            CardGetParsedTuple(hSocket, CISTPL_CFTABLE_ENTRY, parsedBuf, &uParsedItems);
        }
        else{ 
            return FALSE;
        }
    }
    else{
        DEBUGMSG(ZONE_ERROR, (TEXT("no config table entry found")));
        return FALSE;
    }
	
    PVOID pCur = parsedBuf;
    PARSED_CFTABLE* pTable = NULL;
    BOOL	fConfigured = FALSE;
    for(UINT i = 0; i < uParsedItems; i++){
        pTable = (PARSED_CFTABLE *)pCur;
        if(pTable->IFaceType != 1){// not I/O config
            pCur = (PBYTE)pCur + sizeof(PARSED_CFTABLE);
            continue;
        }
	
        pConInfo->uVcc = (UINT8)(pTable->VccDescr.NominalV);
        pConInfo->uVpp1 = (UINT8)(pTable->Vpp1Descr.NominalV);
        pConInfo->uVpp2 =(UINT8)( pTable->Vpp2Descr.NominalV);

        //now request configuration
        status = CardRequestConfiguration(g_hClient, pConInfo);
        if(status != CERR_SUCCESS){//if it failed, try the next configuation table
            g_pKato->Log(LOG_FAIL,TEXT("Socket %u: Function %u: Request config on No. %u config table failed\r\n"), 
                                                m_uLocalSock, m_uLocalFunc, i);
            g_pKato->Log(LOG_FAIL,TEXT("uVcc = %u, uVpp1 = %u, uVpp2 = %u\r\n"), 
                                               pConInfo->uVcc, pConInfo->uVpp1, pConInfo->uVpp2);
                                               
        }
        else{
            g_pKato->Log(LOG_DETAIL,TEXT("Socket %u: Function %u: RequestConfig on No. %u config table succeed!\r\n"), 
                                                m_uLocalSock, m_uLocalFunc, i);
            g_pKato->Log(LOG_DETAIL,TEXT("uVcc = %u, uVpp1 = %u, uVpp2 = %u\r\n"), 
                                               pConInfo->uVcc, pConInfo->uVpp1, pConInfo->uVpp2);
            fConfigured = TRUE;
            break;
        }
        
        pCur = (PBYTE)pCur + sizeof(PARSED_CFTABLE);
    }

    delete[] parsedBuf;

    return fConfigured;
}
BOOL ConfigTest::CompareConfRegValues(CARD_CONFIG_INFO ConfInfo, CARD_CONFIG_INFO NewConfInfo){

	//compare ConfigReg
	if(ConfInfo.uConfigReg != NewConfInfo.uConfigReg){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u: ConfigReg  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, ConfInfo.uConfigReg, NewConfInfo.uConfigReg);
		return FALSE;
	}
	
	//compare StatusReg
	if(ConfInfo.uStatusReg != NewConfInfo.uStatusReg){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u: StatusReg  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, ConfInfo.uStatusReg, NewConfInfo.uStatusReg);
		return FALSE;
	}

	//compare PinReg
	if(ConfInfo.uPinReg != NewConfInfo.uPinReg){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u: PinReg  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, ConfInfo.uPinReg, NewConfInfo.uPinReg);
		return FALSE;
	}
	
	//compare CopyReg
	if(ConfInfo.uCopyReg != NewConfInfo.uCopyReg){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u: CopyReg  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, ConfInfo.uCopyReg, NewConfInfo.uCopyReg);
		return FALSE;
	}
	
	//compare ExtendedStatus
	if(ConfInfo.uExtendedStatus != NewConfInfo.uExtendedStatus){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u: ExtendedStatus  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, ConfInfo.uExtendedStatus, NewConfInfo.uExtendedStatus);
		return FALSE;
	}

	for(int i = 0; i < NUM_REGISTERS; i++){
		//compare IOBase
		if(ConfInfo.IOBase[i] != NewConfInfo.IOBase[i]){
			g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u: IOBase[%d]  (1st) 0x%x != (2nd) 0x%x\r\n"),
										m_dwThreadID, m_uLocalSock, i, ConfInfo.IOBase[i], NewConfInfo.IOBase[i]);
			return FALSE;
		}
	}

	//compare IOLimit
	if(ConfInfo.IOLimit != NewConfInfo.IOLimit){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u: IOLimit  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, ConfInfo.IOLimit, NewConfInfo.IOLimit);
		return FALSE;
	}
	
	return TRUE;

}


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
VOID ConfigTest::Test_CardRequestConfiguration()
{
	CARD_CONFIG_INFO     		confInfo = {0};
	CARD_CONFIG_INFO		newconfInfo = {0};
	STATUS					status = CERR_SUCCESS;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_CardRequestConfiguration() enterted\r\n")));

	//calling CardRequestCOnfiguration using normal parameters
	confInfo.hSocket.uFunction = m_uLocalFunc;
	confInfo.hSocket.uSocket = m_uLocalSock;
	confInfo.fInterfaceType = CFG_IFACE_MEMORY_IO;
	confInfo.fAttributes = CFG_ATTR_IRQ_STEERING;
	confInfo.fRegisters = 0xFF;
	confInfo.fExtRegisters = 0xFF;
	//don't touch power-related parameters
	confInfo.uVcc = 0xFF;
	confInfo.uVpp1 = 0xFF;
	confInfo.uVpp2 = 0xFF;
	//retrieve current registry values
	if(RetrieveRegisterValues(&confInfo) == FALSE){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestConfiguration(),  Thread %u for Socket %u Function %u: can not retreive configuration register values.\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}

	//now request configuration
	if(TryRequestConfig(&confInfo) == FALSE){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u Function %u: requestconfiguration failed!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
		
		
	//now read out registers and compare the values 
	if(RetrieveRegisterValues(&newconfInfo) == FALSE){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestConfiguration(),  Thread %u for Socket %u Function %u: can not retreive configuration register values.\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	if(CompareConfRegValues(confInfo, newconfInfo) == FALSE){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestConfiguration(),  Thread %u for Socket %u Function %u: configuration register values changed unexpected!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}


	//use wrong socket number, should return failure
	confInfo.hSocket.uSocket = m_uLocalSock + TEST_MAX_CARDS;
	status = CardRequestConfiguration(g_hClient, &confInfo);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestConfiguration(),  Thread %u for Socket %u Function %u: requestconfiguration (uSocket=0xFF) should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	confInfo.hSocket.uSocket = m_uLocalSock;
	
	//use wrong function number, should return failure
	confInfo.hSocket.uFunction= m_uLocalFunc + TEST_MAX_CARDS;
	status = CardRequestConfiguration(g_hClient, &confInfo);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestConfiguration(),  Thread %u for Socket %u Function %u: requestconfiguration (uFunction=0xFF) should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
	}
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_CardRequestConfiguration() exit\r\n")));
}




VOID ConfigTest::Test_AccessConfigurationRegisters(){

	STATUS status = CERR_SUCCESS;
	CARD_SOCKET_HANDLE hSocket = {m_uLocalSock, m_uLocalFunc};
	CARD_CONFIG_INFO     		confInfo = {0};
	UINT8					uRegVal;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_AccessConfigurationRegisters() enterted\r\n")));
	
	//retrieve current registry values
	if(RetrieveRegisterValues(&confInfo) == FALSE){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u: can not retreive configuration register values.\r\n"),
									m_dwThreadID, m_uLocalSock);
		SetResult(FALSE);
		return;
	}

	for(int iRepeat = 0; iRepeat < 1000; iRepeat++){
		//read ConfigReg
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 0, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.uConfigReg){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: ConfigReg = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.uConfigReg);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.uConfigReg != 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: ConfigReg should be valid as 0x%x \r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, confInfo.uConfigReg);
			SetResult(FALSE);
			return;
		}

		//read StatusReg
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 1, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.uStatusReg){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: StatusReg = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.uStatusReg);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.uStatusReg != 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: StatusReg should be valid as 0x%x \r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, confInfo.uStatusReg);
			SetResult(FALSE);
			return;
		}

		//read PinReg
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 2, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.uPinReg){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: PinReg = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.uPinReg);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.uPinReg != 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: PinReg should be valid as 0x%x \r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, confInfo.uPinReg);
			SetResult(FALSE);
			return;
		}
		
		//read CopyReg
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 3, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.uCopyReg){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: CopyReg = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.uCopyReg);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.uCopyReg != 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: CopyReg should be valid as 0x%x \r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, confInfo.uCopyReg);
			SetResult(FALSE);
			return;
		}

		//read uExtendedStatusReg
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 4, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.uExtendedStatus){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: ExtendedStatus = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.uExtendedStatus);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.uExtendedStatus!= 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: ExtendedStatus should be valid as 0x%x \r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, confInfo.uExtendedStatus);
			SetResult(FALSE);
			return;
		}

		//read IOBases
		for(int i = 0; i < NUM_EXT_REGISTERS; i++){
			status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, NUM_REGISTERS+i, &uRegVal);
			if(status == CERR_SUCCESS){
				if(uRegVal != confInfo.IOBase[i]){//not the same value as the first read
					g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: IOBase[%d] = 0x%x != 0x%x (orig) \r\n"),
												m_dwThreadID, m_uLocalSock, m_uLocalFunc, i, uRegVal, confInfo.IOBase[i]);
					SetResult(FALSE);
					return;
				}
			}
			else if(confInfo.IOBase[i]!= 0){//it should be a valid value, not zero
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: IOBase[%d] should be valid as 0x%x \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, i, confInfo.IOBase[i]);
				SetResult(FALSE);
				return;
			}
		}

		//read IOLimit
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 9, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.IOLimit){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: IOLimit = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.IOLimit);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.IOLimit != 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Function %u: ExtendedStatus should be valid as 0x%x \r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, confInfo.IOLimit);
			SetResult(FALSE);
			return;
		}
	}

	//try some invalid offset
	//read IOLimit
	status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 10, &uRegVal);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: no one at offset 10! \r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal);
		SetResult(FALSE);
	}
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- ConfigTest::Test_AccessConfigurationRegisters() exit\r\n")));


}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
VOID ConfigTest::Test_CardModifyConfiguration()
{
	CARD_CONFIG_INFO     		confInfo = {0};
	CARD_CONFIG_INFO		newconfInfo = {0};
	STATUS					status = CERR_SUCCESS;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_CardModifyConfiguration() enterted\r\n")));

	//calling CardRequestCOnfiguration using normal parameters
	confInfo.hSocket.uFunction = m_uLocalFunc;
	confInfo.hSocket.uSocket = m_uLocalSock;
	confInfo.fInterfaceType = CFG_IFACE_MEMORY_IO;
	confInfo.fAttributes = 0; // CFG_ATTR_IRQ_STEERING;
	confInfo.fRegisters = 0xFF;
	confInfo.fExtRegisters = 0xFF;
	//don't touch power-related parameters
	confInfo.uVcc = 0xFF;
	confInfo.uVpp1 = 0xFF;
	confInfo.uVpp2 = 0xFF;
	//retrieve current registry values
	if(RetrieveRegisterValues(&confInfo) == FALSE){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestConfiguration(),  Thread %u for Socket %u: can not retreive configuration register values.\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	
	//now request configuration
	status = CardRequestConfiguration(g_hClient, &confInfo);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestConfiguration(),  Thread %u for Socket %u: requestconfiguration failed!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
		
	confInfo.fAttributes = CFG_ATTR_IRQ_STEERING;
	status = CardModifyConfiguration(g_hClient, confInfo.hSocket, &confInfo.fAttributes);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardModifyConfiguration(),  Thread %u for Socket %u: ModifyConfiguration failed!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_CardModifyConfiguration() exit\r\n")));
}

