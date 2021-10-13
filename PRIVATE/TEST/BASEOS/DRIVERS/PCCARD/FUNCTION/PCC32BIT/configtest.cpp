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

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::ThreadRun() enterted\r\n")));

	SetLastError(0);
	
	switch(m_dwCaseID){
		case 1: //test on CardRequestConfiguration
			Test_CardRequestConfiguration();
			break;
		case 2: //normal test on CardModifyConfiguration-- NOT SUPPORTED IN 32 BIT DRIVER
			//Test_CardModifyConfiguration();
			break;
		case 3: //normal test on CardAccessConfigurationRegister()
			Test_AccessConfigurationRegisters();
			break;
		case 4://test on CardResetFunction(), CardRequestDisable().
			Test_CardDisableandReset();
			break;
		default:	
   			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u: Invalid test case\r\n"), m_dwThreadID, m_uLocalSock);
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
	
	STATUS					status = CERR_SUCCESS;
	UINT32					uParsedItems = 0;
	UINT32					uParsedSize = 0;
	CARD_SOCKET_HANDLE		hSocket = {m_uLocalSock, m_uLocalFunc};
	PVOID					parsedBuf;
	
	if(CardGetParsedTuple(hSocket, CISTPL_CFTABLE_ENTRY_CB, 0, &uParsedItems) != CERR_SUCCESS){
		DEBUGMSG(ZONE_ERROR, (TEXT("CardGetParsedTuple call failed")));
		return FALSE;
		
	}
	
      uParsedSize = uParsedItems * sizeof (PARSED_CBCFTABLE) ;
	if (uParsedSize){
             parsedBuf = new UCHAR [uParsedSize] ;
             if (parsedBuf){
              	CardGetParsedTuple(hSocket, CISTPL_CFTABLE_ENTRY_CB, parsedBuf, &uParsedItems);
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
	PARSED_CBCFTABLE* pTable = NULL;
	BOOL	fconfigured = FALSE;
	for(UINT i = 0; i < uParsedItems; i++){
		pTable = (PARSED_CBCFTABLE *)pCur;
		if(pTable->VccDescr.ValidMask &1){
		    	pConInfo->uVcc=((pTable->VccDescr.ValidMask &1)?(UINT8)pTable->VccDescr.NominalV:(UINT)-1);
		    	pConInfo->uVpp1=((pTable->Vpp1Descr.ValidMask &1)?(UINT8)pTable->Vpp1Descr.NominalV:(UINT)-1);
		    	pConInfo->uVpp2=((pTable->Vpp2Descr.ValidMask &1)?(UINT8)pTable->Vpp2Descr.NominalV:(UINT)-1);
			pConInfo->uConfigReg = pConInfo->uConfigReg | pTable->ConfigIndex;

                  	//now request configuration
                	status = CardRequestConfiguration(g_hClient, pConInfo);
                	if(status != CERR_SUCCESS){//configuratino failed,try next one
                		g_pKato->Log(LOG_DETAIL,TEXT("Thread %u for Socket %u Function %u: RequestConfig failed with uVcc=%u, uVpp1=%u,  uConfigReg = 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc, pConInfo->uVcc, pConInfo->uVpp1,pConInfo->uConfigReg);
                		pCur = (PBYTE)pCur + sizeof(PARSED_CBCFTABLE);
                		continue;
                	}
		       else{
                		g_pKato->Log(LOG_DETAIL,TEXT("Thread %u for Socket %u Function %u: RequestConfig succeeded with uVcc=%u, uVpp1=%u,  uConfigReg = 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc, pConInfo->uVcc, pConInfo->uVpp1, pConInfo->uConfigReg);
			      fconfigured = TRUE;
			      break;
			}
		}
		pCur = (PBYTE)pCur + sizeof(PARSED_CBCFTABLE);
	}

	delete[] parsedBuf;

	return fconfigured;
	
}
BOOL ConfigTest::CompareConfRegValues(CARD_CONFIG_INFO ConfInfo, CARD_CONFIG_INFO NewConfInfo){

	//compare ConfigReg
	if(ConfInfo.uConfigReg != NewConfInfo.uConfigReg){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u Function %u: ConfigReg  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc, ConfInfo.uConfigReg, NewConfInfo.uConfigReg);
	}
	
	//compare StatusReg
	if(ConfInfo.uStatusReg != NewConfInfo.uStatusReg){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u Function %u: StatusReg  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc, ConfInfo.uStatusReg, NewConfInfo.uStatusReg);
	}

	//compare PinReg
	if(ConfInfo.uPinReg != NewConfInfo.uPinReg){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u Function %u: PinReg  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc, ConfInfo.uPinReg, NewConfInfo.uPinReg);
	}
	
	//compare CopyReg
	if(ConfInfo.uCopyReg != NewConfInfo.uCopyReg){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u Function %u: CopyReg  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc, ConfInfo.uCopyReg, NewConfInfo.uCopyReg);
	}
	
	//compare ExtendedStatus
	if(ConfInfo.uExtendedStatus != NewConfInfo.uExtendedStatus){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u Function %u: ExtendedStatus  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc, ConfInfo.uExtendedStatus, NewConfInfo.uExtendedStatus);
	}

	for(int i = 0; i < NUM_REGISTERS; i++){
		//compare IOBase
		if(ConfInfo.IOBase[i] != NewConfInfo.IOBase[i]){
			g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u Function %u: IOBase[%d]  (1st) 0x%x != (2nd) 0x%x\r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, i, ConfInfo.IOBase[i], NewConfInfo.IOBase[i]);
		}
	}

	//compare IOLimit
	if(ConfInfo.IOLimit != NewConfInfo.IOLimit){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u Function %u: IOLimit  (1st) 0x%x != (2nd) 0x%x\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc, ConfInfo.IOLimit, NewConfInfo.IOLimit);
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
	//don't touch power-related parameters
	confInfo.uVcc = 0xFF;
	confInfo.uVpp1 = 0xFF;
	confInfo.uVpp2 = 0xFF;
	//try requesting configuration without setting too much details
	if(TryRequestConfig(&confInfo) == FALSE){
		g_pKato->Log(LOG_DETAIL,TEXT("WARNING:::Test_CardRequestConfiguration(),  Thread %u for Socket %u Func %u:requestconfiguration failed!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		g_pKato->Log(LOG_DETAIL,TEXT("Please verify that this card does not use tuples to store card-specific information (use PCI registers instead), otherwise it could be a driver bug!!!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		return;
	}
		
	//use wrong socket number, should return failure
	confInfo.hSocket.uSocket = m_uLocalSock + TEST_MAX_CARDS;
	status = CardRequestConfiguration(g_hClient, &confInfo);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestConfiguration(),  Thread %u for Socket %u Func %u:requestconfiguration (uSocket=0xFF) should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	confInfo.hSocket.uSocket = m_uLocalSock;
	
	//use wrong function number, should return failure
	confInfo.hSocket.uFunction= m_uLocalFunc + TEST_MAX_CARDS;
	status = CardRequestConfiguration(g_hClient, &confInfo);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestConfiguration(),  Thread %u for Socket %u Func %u:requestconfiguration (uFunction=0xFF) should fail!\r\n"),
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
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: can not retreive configuration register values.\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}

	for(int iRepeat = 0; iRepeat < 1000; iRepeat++){
		//read ConfigReg
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 0, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.uConfigReg){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: ConfigReg = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.uConfigReg);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.uConfigReg != 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: ConfigReg should be valid as 0x%x \r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, confInfo.uConfigReg);
			SetResult(FALSE);
			return;
		}

		//read StatusReg
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 1, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.uStatusReg){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: StatusReg = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.uStatusReg);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.uStatusReg != 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: StatusReg should be valid as 0x%x \r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, confInfo.uStatusReg);
			SetResult(FALSE);
			return;
		}

		//read PinReg
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 2, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.uPinReg){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: PinReg = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.uPinReg);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.uPinReg != 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: PinReg should be valid as 0x%x \r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, confInfo.uPinReg);
			SetResult(FALSE);
			return;
		}
		
		//read CopyReg
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 3, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.uCopyReg){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: CopyReg = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.uCopyReg);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.uCopyReg != 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: CopyReg should be valid as 0x%x \r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, confInfo.uCopyReg);
			SetResult(FALSE);
			return;
		}

		//read uExtendedStatusReg
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 4, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.uExtendedStatus){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: ExtendedStatus = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.uExtendedStatus);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.uExtendedStatus!= 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: ExtendedStatus should be valid as 0x%x \r\n"),
										m_dwThreadID, m_uLocalSock, m_uLocalFunc, confInfo.uExtendedStatus);
			SetResult(FALSE);
			return;
		}

		//read IOBases
		for(int i = 0; i < NUM_EXT_REGISTERS; i++){
			status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, NUM_REGISTERS+i, &uRegVal);
			if(status == CERR_SUCCESS){
				if(uRegVal != confInfo.IOBase[i]){//not the same value as the first read
					g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: IOBase[%d] = 0x%x != 0x%x (orig) \r\n"),
												m_dwThreadID, m_uLocalSock, m_uLocalFunc, i, uRegVal, confInfo.IOBase[i]);
					SetResult(FALSE);
					return;
				}
			}
			else if(confInfo.IOBase[i]!= 0){//it should be a valid value, not zero
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: IOBase[%d] should be valid as 0x%x \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, i, confInfo.IOBase[i]);
				SetResult(FALSE);
				return;
			}
		}

		//read IOLimit
		status = CardAccessConfigurationRegister(g_hClient, hSocket, CARD_FCR_READ, 9, &uRegVal);
		if(status == CERR_SUCCESS){
			if(uRegVal != confInfo.IOLimit){//not the same value as the first read
				g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: IOLimit = 0x%x != 0x%x (orig) \r\n"),
											m_dwThreadID, m_uLocalSock, m_uLocalFunc, uRegVal, confInfo.IOLimit);
				SetResult(FALSE);
				return;
			}
		}
		else if(confInfo.IOLimit != 0){//it should be a valid value, not zero
			g_pKato->Log(LOG_FAIL,TEXT(":Test_CardAccessConfigurationRegisters(),  Thread %u for Socket %u Func %u: ExtendedStatus should be valid as 0x%x \r\n"),
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
									m_dwThreadID, m_uLocalSock);
		SetResult(FALSE);
		return;
	}
	
	//now request configuration
	status = CardRequestConfiguration(g_hClient, &confInfo);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestConfiguration(),  Thread %u for Socket %u: requestconfiguration failed!\r\n"),
									m_dwThreadID, m_uLocalSock);
		SetResult(FALSE);
		return;
	}
		
	confInfo.fAttributes = CFG_ATTR_IRQ_STEERING;
	status = CardModifyConfiguration(g_hClient, confInfo.hSocket, &confInfo.fAttributes);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardModifyConfiguration(),  Thread %u for Socket %u: ModifyConfiguration failed!\r\n"),
									m_dwThreadID, m_uLocalSock);
		SetResult(FALSE);
		return;
	}
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_CardModifyConfiguration() exit\r\n")));
}





//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
VOID ConfigTest::Test_CardDisableandReset()
{
	CARD_CONFIG_INFO     		confInfo = {0};
	CARD_CONFIG_INFO		newconfInfo = {0};
	STATUS					status = CERR_SUCCESS;
	CARD_SOCKET_HANDLE		hSocket = {m_uLocalSock, m_uLocalFunc};

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_CardDisableandReset() enterted\r\n")));
	
	//re-enumerate card in the system, make sure there is only one card
	DWORD	dwCardsNum = MAX_SOCKETS*2;
	DWORD	dwCardsAvail = 0;
	if(EnumCard(&dwCardsNum, g_CardDescs, &dwCardsAvail) != CERR_SUCCESS){
		g_pKato->Log(LOG_DETAIL,TEXT("ERROR: Can not enum cards in the system!"));
		SetResult(FALSE);
		return;
	}
	else if(dwCardsAvail != 1){
		g_pKato->Log(LOG_DETAIL,TEXT("ERROR: This test requires only one card in the system (%d cards now), test will skip!"),
			                                 dwCardsAvail);
		return;
	}

	//DisableCard can not be called during the middle of test
	/*	status = CardRequestDisable(g_hClient, hSocket);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u Func %u:disable card failed!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}

	Sleep(TEST_IDLE_TIME);
	*/
	
	//now reset this card
	status = CardResetFunction(g_hClient, hSocket);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u Func %u:disable card failed!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	Sleep(TEST_IDLE_TIME);
	
	//find out the new socket index for this card
	dwCardsNum = MAX_SOCKETS*2;
	dwCardsAvail = 0;
	if(EnumCard(&dwCardsNum, g_CardDescs, &dwCardsAvail) != CERR_SUCCESS){
		g_pKato->Log(LOG_DETAIL,TEXT("ERROR: Can not enum cards in the system!"));
		SetResult(FALSE);
		return;
	}
	else if(dwCardsAvail != 1){
		g_pKato->Log(LOG_DETAIL,TEXT("ERROR: there should be only one card in the system (%d cards now)"),
			                                 dwCardsAvail);
		SetResult(FALSE);
		return;
	}

	//calling CardRequestCOnfiguration using normal parameters
	m_uLocalSock = g_CardDescs[0].hCardHandle.uSocket;
	hSocket.uSocket = m_uLocalSock;
	confInfo.hSocket = hSocket;
	confInfo.hSocket.uFunction = m_uLocalFunc;
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
		g_pKato->Log(LOG_FAIL,TEXT("Thread %u for Socket %u Func %u:can not retreive configuration register values.\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	//Try requesting configuration
	if(TryRequestConfig(&confInfo) == FALSE){
		g_pKato->Log(LOG_FAIL,TEXT("WARNING: Thread %u for Socket %u Func %u:requestconfiguration failed!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		g_pKato->Log(LOG_DETAIL,TEXT("Please verify that this card does not use tuples to store card-specific information (use PCI registers instead), otherwise it could be a driver bug!!!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ ConfigTest::Test_CardDisableandReset() exit\r\n")));
}


