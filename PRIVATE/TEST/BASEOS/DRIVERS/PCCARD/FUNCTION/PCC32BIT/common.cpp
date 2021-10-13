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
	Common.cpp

Abstract:

    Data structuctures and functions that used by all other files

--*/

#include "TestMain.h"
#include "common.h"

//******************************************************************************
//   Globals that will be initially used in this file
//******************************************************************************

BOOL	gfSomeoneGotExclusive = FALSE;
int		nThreadsDone;
UINT 	LastError;
PCB_BUS_INFO  g_pCBBusInfoHead = NULL;
CBCARD_INFO	   g_CBCardInfo;

UINT EventMaskIndex = 0;
UINT EventMasks[MAX_EVENT_MASK] = {
	BASE_EVENT_MASK,
	BASE_EVENT_MASK & (~EVENT_MASK_WRITE_PROTECT),
	BASE_EVENT_MASK & (~EVENT_MASK_CARD_LOCK),
	BASE_EVENT_MASK & (~EVENT_MASK_EJECT_REQ),
	BASE_EVENT_MASK & (~EVENT_MASK_INSERT_REQ),
	BASE_EVENT_MASK & (~EVENT_MASK_BATTERY_DEAD),
	BASE_EVENT_MASK & (~EVENT_MASK_BATTERY_LOW),
	BASE_EVENT_MASK & (~EVENT_MASK_CARD_READY),
	BASE_EVENT_MASK & (~EVENT_MASK_CARD_DETECT),
	BASE_EVENT_MASK & (~EVENT_MASK_POWER_MGMT),
	BASE_EVENT_MASK & (~EVENT_MASK_RESET),
};

#define MAX_EVENT_MASK 11
#define BASE_EVENT_MASK 0x02FF  // all of them.
#define LAST_EVENT_CODE ((CARD_EVENT) -1)

//
// Table of callback event codes and their names.
// NOTE: The names with ! at the end are not expected.
//
EVENT_NAME_TBL v_EventNames[] = {
    { CE_BATTERY_DEAD,          		TEXT("CE_BATTERY_DEAD") },
    { CE_BATTERY_LOW,           		TEXT("CE_BATTERY_LOW") },
    { CE_CARD_LOCK,             		TEXT("CE_CARD_LOCK") },
    { CE_CARD_READY,            		TEXT("CE_CARD_READY") },
    { CE_CARD_REMOVAL,          		TEXT("CE_CARD_REMOVAL") },
    { CE_CARD_UNLOCK,           		TEXT("CE_CARD_UNLOCK") },
    { CE_EJECTION_COMPLETE,     	TEXT("CE_EJECTION_COMPLETE!") },
    { CE_EJECTION_REQUEST,      	TEXT("CE_EJECTION_REQUEST!") },
    { CE_INSERTION_COMPLETE,    	TEXT("CE_INSERTION_COMPLETE!") },
    { CE_INSERTION_REQUEST,     	TEXT("CE_INSERTION_REQUEST!") },
    { CE_PM_RESUME,             		TEXT("CE_PM_RESUME!") },
    { CE_PM_SUSPEND,            		TEXT("CE_PM_SUSPEND!") },
    { CE_EXCLUSIVE_COMPLETE,    	TEXT("CE_EXCLUSIVE_COMPLETE") },
    { CE_EXCLUSIVE_REQUEST,     	TEXT("CE_EXCLUSIVE_REQUEST") },
    { CE_RESET_PHYSICAL,        		TEXT("CE_RESET_PHYSICAL") },
    { CE_RESET_REQUEST,         		TEXT("CE_RESET_REQUEST") },
    { CE_CARD_RESET,            		TEXT("CE_CARD_RESET") },
    { CE_MTD_REQUEST,           		TEXT("CE_MTD_REQUEST!") },
    { CE_CLIENT_INFO,           		TEXT("CE_CLIENT_INFO!") },
    { CE_TIMER_EXPIRED,         		TEXT("CE_TIMER_EXPIRED!") },
    { CE_SS_UPDATED,            		TEXT("CE_SS_UPDATED!") },
    { CE_WRITE_PROTECT,         		TEXT("CE_WRITE_PROTECT") },
    { CE_CARD_INSERTION,        		TEXT("CE_CARD_INSERTION") },
    { CE_RESET_COMPLETE,        		TEXT("CE_RESET_COMPLETE") },
    { CE_ERASE_COMPLETE,        		TEXT("CE_ERASE_COMPLETE!") },
    { CE_REGISTRATION_COMPLETE, 	TEXT("CE_REGISTRATION_COMPLETE") },
    { LAST_EVENT_CODE,          		TEXT("Unknown Event!") },
};

UINT8   rgWinSpeed[MAX_WIN_SPEED] =
{
	WIN_SPEED_EXP_1NS   ,
	WIN_SPEED_EXP_10NS  ,
	WIN_SPEED_EXP_100NS ,
	WIN_SPEED_EXP_1US   ,
	WIN_SPEED_EXP_10US  ,
	WIN_SPEED_EXP_100US ,
	WIN_SPEED_EXP_1MS   ,
	WIN_SPEED_EXP_10MS  ,

	WIN_SPEED_MANT_10   ,
	WIN_SPEED_MANT_12   ,
	WIN_SPEED_MANT_13   ,
	WIN_SPEED_MANT_15   ,
	WIN_SPEED_MANT_20   ,
	WIN_SPEED_MANT_25   ,
	WIN_SPEED_MANT_30   ,
	WIN_SPEED_MANT_35   ,
	WIN_SPEED_MANT_40   ,
	WIN_SPEED_MANT_45   ,
	WIN_SPEED_MANT_50   ,
	WIN_SPEED_MANT_55   ,
	WIN_SPEED_MANT_60   ,
	WIN_SPEED_MANT_70   ,
	WIN_SPEED_MANT_80   ,

	WIN_SPEED_USE_WAIT  ,

	0x55,						  // undefined values
	0x66,
	0x77,
	0x88,
	0x99,
	0xAA,
	0xBB,
	0xCC,
	0xDD,
	0xEE,
	0xFF

};


#define CERR_PASSED_BY_DEFAULT 0x23

TCHAR *RtnCodes [] = // These are card return codes
  {
    TEXT("CERR_SUCCESS")             , // 0x00
    TEXT("CERR_BAD_ADAPTER")         , // 0x01
    TEXT("CERR_BAD_ATTRIBUTE")       , // 0x02
    TEXT("CERR_BAD_BASE")            , // 0x03
    TEXT("CERR_BAD_EDC")             , // 0x04
    TEXT("Undefined:0x05")           , // 0x05
    TEXT("CERR_BAD_IRQ")             , // 0x06
    TEXT("CERR_BAD_OFFSET")          , // 0x07
    TEXT("CERR_BAD_PAGE")            , // 0x08
    TEXT("CERR_READ_FAlLURE")        , // 0x09
    TEXT("CERR_BAD_SIZE")            , // 0x0A
    TEXT("CERR_BAD_SOCKET")          , // 0x0B
    TEXT("Undefined:0x0C")           , // 0x0C
    TEXT("CERR_BAD_TYPE")            , // 0x0D
    TEXT("CERR_BAD_VCC")             , // 0x0E
    TEXT("CERR_BAD_VPP")             , // 0x0F
    TEXT("Undefined:0x10")           , // 0x10
    TEXT("CERR_BAD_WINDOW")          , // 0x11
    TEXT("CERR_WRITE_FAlLURE")       , // 0x12
    TEXT("Undefined:0x13")           , // 0x13
    TEXT("CERR_NO_CARD")             , // 0x14
    TEXT("CERR_UNSUPPORTED_SERVICE") , // 0x15
    TEXT("CERR_UNSUPPORTED_MODE")    , // 0x16
    TEXT("CERR_BAD_SPEED")           , // 0x17
    TEXT("CERR_BUSY")                , // 0x18
    TEXT("CERR_GENERAL_FAlLURE")     , // 0x19
    TEXT("CERR_WRITE_PROTECTED")     , // 0x1A
    TEXT("CERR_BAD_ARG_LENGTH")      , // 0x1B
    TEXT("CERR_BAD_ARGS")            , // 0x1C
    TEXT("CERR_CONFIGURATION_LOCKED"), // 0x1D
    TEXT("CERR_IN_USE")              , // 0x1E
    TEXT("CERR_NO_MORE_ITEMS")       , // 0x1F
    TEXT("CERR_OUT_OF_RESOURCE")     , // 0x20
    TEXT("CERR_BAD_HANDLE")          , // 0x21
    TEXT("CERR_BAD_VERSION")         , // 0x22
    TEXT("CERR_PASSED_BY_DEFAULT"),     // 0x23
  } ;


CISStrings TupleCodes [] =
  {
    {TEXT("CISTPL_NULL")              , CISTPL_NULL                  , 0                  },
    {TEXT("CISTPL_DEVICE")            , CISTPL_DEVICE                , CARD_IO|CARD_MEMORY},
    {TEXT("CISTPL_LONGLINK_CB")       , CISTPL_LONGLINK_CB           , 0                  },
    {TEXT("CISTPL_CONFIG_CB")         , CISTPL_CONFIG_CB             , 0                  },
    {TEXT("CISTPL_CFTABLE_ENTRY_CB")  , CISTPL_CFTABLE_ENTRY_CB      , 0                  },
    {TEXT("CISTPL_LONG_LINK_MFC")     , CISTPL_LONG_LINK_MFC         , 0                  },
    {TEXT("CISTPL_CHECKSUM")          , CISTPL_CHECKSUM              , 0                  },
    {TEXT("CISTPL_LONGLINK_A")        , CISTPL_LONGLINK_A            , 0                  },
    {TEXT("CISTPL_LONGLINK_C")        , CISTPL_LONGLINK_C            , 0                  },
    {TEXT("CISTPL_LINKTARGET")        , CISTPL_LINKTARGET            , 0                  },
    {TEXT("CISTPL_NO_LINK")           , CISTPL_NO_LINK               , 0                  },
    {TEXT("CISTPL_VERS_1")            , CISTPL_VERS_1                , CARD_IO            },
    {TEXT("CISTPL_ALTSTR")            , CISTPL_ALTSTR                , 0                  },
    {TEXT("CISTPL_DEVICE_A")          , CISTPL_DEVICE_A              , 0                  },
    {TEXT("CISTPL_JEDEC_C")           , CISTPL_JEDEC_C               , CARD_MEMORY        },
    {TEXT("CISTPL_JEDEC_A")           , CISTPL_JEDEC_A               , 0                  },
    {TEXT("CISTPL_CONFIG")            , CISTPL_CONFIG                , CARD_IO            },
    {TEXT("CISTPL_CFTABLE_ENTRY")     , CISTPL_CFTABLE_ENTRY         , CARD_IO            },
    {TEXT("CISTPL_DEVICE_OC")         , CISTPL_DEVICE_OC             , 0                  },
    {TEXT("CISTPL_DEVICE_OA")         , CISTPL_DEVICE_OA             , 0                  },
    {TEXT("CISTPL_GEODEVICE")         , CISTPL_GEODEVICE             , CARD_MEMORY        },
    {TEXT("CISTPL_GEODEVICE_A")       , CISTPL_GEODEVICE_A           , 0                  },
    {TEXT("CISTPL_MANFID")            , CISTPL_MANFID                , CARD_IO_RECOMMENDED},
    {TEXT("CISTPL_FUNCID")            , CISTPL_FUNCID                , CARD_IO            },
    {TEXT("CISTPL_FUNCE")             , CISTPL_FUNCE                 , 0                  },
    {TEXT("CISTPL_VERS_2")            , CISTPL_VERS_2                , 0                  },
    {TEXT("CISTPL_FORMAT")            , CISTPL_FORMAT                , CARD_MEMORY        },
    {TEXT("CISTPL_GEOMETRY")          , CISTPL_GEOMETRY              , 0                  },
    {TEXT("CISTPL_BYTEORDER")         , CISTPL_BYTEORDER             , 0                  },
    {TEXT("CISTPL_DATE")              , CISTPL_DATE                  , 0                  },
    {TEXT("CISTPL_BATTERY")           , CISTPL_BATTERY               , 0                  },
    {TEXT("CISTPL_ORG")               , CISTPL_ORG                   , CARD_MEMORY        },
    {TEXT("CISTPL_END")               , CISTPL_END                   , CARD_IO|CARD_MEMORY}
  } ;


//=================================================================================================================
//
//	   Client callback function
//
//
//=================================================================================================================
STATUS CallBackFn_Client(
    CARD_EVENT EventCode,
    CARD_SOCKET_HANDLE hSocket,
    PCARD_EVENT_PARMS pParms
    )
{

    	PCLIENT_CONTEXT    	pData;
    	TCHAR             		lpClient[25] ;
    	LPTSTR				pEventName = NULL;
    	STATUS 				status = CERR_SUCCESS;
    	STATUS 				uRet;
	CARD_STATUS 		CardStatus;             // For CardGetStatus
	DWORD BusNumber;
    	INTERFACE_TYPE InterfaceType;
    	PCI_SLOT_NUMBER SlotNumber;

	//get the event name string
    	pEventName = FindEventName(EventCode);

    	// Make sure everything comes in as expected.
    	if (pParms == NULL){
   		DEBUGMSG(ZONE_ERROR, (TEXT("CallBackFn: (EventCode=0x%lx=%s): pParms == NULL!!!\r\n"), EventCode, (LPTSTR) pEventName));
      		goto CBF_END;
	}

    	pData = (PCLIENT_CONTEXT)(pParms->uClientData);
	if (pData == NULL){
		DEBUGMSG(ZONE_WARNING, (TEXT("CallBackFn: (Event=0x%lx=%s): return right away !!!\r\n"), EventCode, (LPTSTR)pEventName));
      		goto CBF_END;
	}
	else{
		wsprintf(lpClient, TEXT("Client %u"), pData->uClientID);
    		DEBUGMSG(ZONE_VERBOSE, (TEXT("%s: CallBackFn: (Event=0x%lx=%s): uSocket=0x%x uFun=0x%lx Parm1=0x%lx  Parm2=0x%lx\r\n"),
				(LPTSTR)lpClient, EventCode, (LPTSTR)pEventName, hSocket.uSocket, hSocket.uFunction, pParms->Parm1, pParms->Parm2));
	}

	switch (EventCode){
		
		case CE_EXCLUSIVE_COMPLETE:
			pData->uCardReqStatus =  pParms->Parm1; // Exclusive Status
			pData->uEventGot +=  CE_EXCLUSIVE_COMPLETE;
			if (pParms->Parm1 == CERR_SUCCESS){
                   		gfSomeoneGotExclusive = TRUE;     // Someone has get the exclusive access
				DEBUGMSG(ZONE_VERBOSE, (TEXT("%s: CE_EXCLUSIVE_COMPLETE: pData->uEvent=0x%lx:  Request succeed\r\n"),
							(LPTSTR)lpClient, pData->uEventGot));
			}
			else{
				DEBUGMSG(ZONE_VERBOSE, (TEXT("%s: CE_EXCLUSIVE_COMPLETE: pData->uEvent=0x%lx:  Request Fail: status=0x%lX\r\n"),
							 (LPTSTR)lpClient, pData->uEventGot, pParms->Parm1));
					}
			 if (pData->hEvent)
      				SetEvent(pData->hEvent);
			break;
			
		case CE_REGISTRATION_COMPLETE:
			pData->uCardReqStatus =  pParms->Parm1; //@comm  contains the registered handle
      			if (pParms->Parm1 == 0) { //Parm1=Client hClient
				DEBUGMSG(ZONE_VERBOSE, (TEXT("Failed:  %s: CallBackFn: CE_REGISTRATION_COMPLETE: pParm->Parm1=NULL\r\n"),
              								(LPTSTR)lpClient,  pParms->Parm1));
			}
	      		pData->fEventGot |=  CE_REGISTRATION_COMPLETE; //for wintst
			if (pData->hEvent)
				SetEvent(pData->hEvent);
      			break;
      			
    		case CE_CARD_INSERTION:
			//request exclusive access immediately
			if(CardRequestExclusive(pData->hClient, hSocket) != CERR_SUCCESS){
				g_pKato->Log(LOG_DETAIL,TEXT("FAILed:  %s: CallBackFn: CE_CARD_INSERTION: Request exclusive access failed\r\n"),
         	  								(LPTSTR)lpClient);
			}
			
            		 BusNumber=pParms->Parm1 & 0xffff;
                    InterfaceType = (INTERFACE_TYPE )((pParms->Parm1>>16 ) & 0xffff);
			 SlotNumber.u.AsULONG=pParms->Parm2;
			 Add_CBCardBusInfo(hSocket, BusNumber, InterfaceType, SlotNumber);
						
  			 pData->uEventGot +=  CE_CARD_INSERTION;
  			 pData->fEventGot |=  CE_CARD_INSERTION;
			 g_pKato->Log(LOG_DETAIL,TEXT("%s: CE_CARD_INSERTION: pData->uEvent=0x%lx: pData->hClient=0x%lX\r\n"),
							 (LPTSTR)lpClient, pData->uEventGot, pData->hClient );
        		// Determine if this is an artificial insertion notice
        		CardStatus.hSocket = hSocket;
        		uRet = CardGetStatus(&CardStatus);
      			if (uRet){
				   g_pKato->Log(LOG_DETAIL,TEXT("FAIL %s: CallBackFn: CE_CARD_INSERTION: CardGetStatus returned 0x%lx\r\n"),
									 (LPTSTR)lpClient, uRet);
         		}
      			break;
      			
    		case CE_CARD_REMOVAL:    
			 pData->uEventGot +=  CE_CARD_REMOVAL;
			 Delete_CBCardBusInfo(hSocket);
			 g_pKato->Log(LOG_DETAIL,TEXT("%s: CE_CARD_REMOVAL: pData->uEvent=0x%lx\r\n"), (LPTSTR)lpClient, pData->uEventGot );
			 break;

		case CE_EXCLUSIVE_REQUEST:
			CardStatus.hSocket = hSocket;
			uRet = CardGetStatus(&CardStatus);
			if (uRet){
				g_pKato->Log(LOG_DETAIL,TEXT("FAIL: %s:CallBackFn: CE_EXCLUSIVE_REQUEST: CardGetStatus returned 0x%lx\r\n"),
         			(LPTSTR)lpClient, uRet);
   			}
			pData->uEventGot +=  CE_EXCLUSIVE_REQUEST;
			g_pKato->Log(LOG_DETAIL,TEXT("%s: CE_EXCLUSIVE_REQUEST: pData->uEvent=0x%lx: return 0x%lX\r\n"),
							(LPTSTR)lpClient, pData->uEventGot, pData->uReqReturn );


			break;
			
		case CE_RESET_COMPLETE:
			pData->uCardReqStatus =  pParms->Parm1; // Reset Status
	    		if(pData->hEvent)
	    			SetEvent(pData->hEvent);
			break;
			
      	//no action on the following events
    		case CE_CARD_UNLOCK:
 		case CE_EJECTION_COMPLETE:
		case CE_INSERTION_COMPLETE:
    		case CE_EJECTION_REQUEST:
    		case CE_INSERTION_REQUEST:
    		case CE_RESET_REQUEST:
    		case CE_BATTERY_DEAD:
    		case CE_BATTERY_LOW:
    		case CE_CARD_LOCK:
    		case CE_CARD_READY:
    		case CE_PM_RESUME:
    		case CE_PM_SUSPEND:
    		case CE_RESET_PHYSICAL:
    		case CE_CARD_RESET:
    		case CE_WRITE_PROTECT:
        		break;
    }

CBF_END:
    	return status;
} // CallBackFn



LPTSTR 
FindEventName( CARD_EVENT EventCode)
{
    PEVENT_NAME_TBL pEvent = v_EventNames;

    while (pEvent->EventCode != LAST_EVENT_CODE) {
        if (pEvent->EventCode == EventCode) {
            return pEvent->pEventName;
        }
        pEvent++;
    }
    return pEvent->pEventName;
}

LPTSTR
FindStatusName( STATUS StatusCode)
{
	if(StatusCode < 0 || StatusCode > 0x23) //invalid code
		return NULL;
	return RtnCodes[StatusCode];
}

// --------------------- update error code -----------
void updateError (UINT n)
{                                                              // n arbitrary
    if (n > 5) return ;                                          // 0 <= n <= 5
    if (n == TST_NO_ERROR) {return ;}                            // 1 <= n <= 5
    if (LastError == TST_NO_ERROR) {LastError = n ; return ;}    // First error
    if (n < LastError) {LastError = n ; return ;}                // n no worse
}

BOOL NormalRequestConfig(CARD_CLIENT_HANDLE hClient, UINT8 uSock, UINT8 uFunc){

	CARD_CONFIG_INFO     		confInfo = {0};
	STATUS					status = CERR_SUCCESS;
	CARD_SOCKET_HANDLE		hSocket = {uSock, uFunc};
	UINT32					uParsedItems = 0;
	UINT32					uParsedSize = 0;
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
	BOOL	bConfigured = FALSE;
	for(UINT i = 0; i < uParsedItems; i++){
		pTable = (PARSED_CBCFTABLE *)pCur;

		confInfo.hSocket.uFunction = uFunc;
		confInfo.hSocket.uSocket = uSock;
		//calling CardRequestCOnfiguration using normal parameters
	    	confInfo.uVcc=((pTable->VccDescr.ValidMask &1)?(UINT8)pTable->VccDescr.NominalV:(UINT)-1);
	    	confInfo.uVpp1=((pTable->Vpp1Descr.ValidMask &1)?(UINT8)pTable->Vpp1Descr.NominalV:(UINT)-1);
	    	confInfo.uVpp2=((pTable->Vpp2Descr.ValidMask &1)?(UINT8)pTable->Vpp2Descr.NominalV:(UINT)-1);
		//now request configuration
		status = CardRequestConfiguration(hClient, &confInfo);
		if(status == CERR_SUCCESS){//configure succeeded
			bConfigured = TRUE;
			break;
		}

		pCur = (PBYTE)pCur + sizeof(PARSED_CBCFTABLE);
	}
	if(bConfigured == FALSE){
		g_pKato->Log(LOG_FAIL,TEXT("Socket %u: requestconfiguration failed!\r\n"), uSock);
	}

	delete[] parsedBuf;
	return bConfigured;
}

BOOL RetrieveRegisterValues(PCARD_CONFIG_INFO pConfInfo, CARD_SOCKET_HANDLE hSocket){

	STATUS status = CERR_SUCCESS;
	
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

STATUS 
CardGetParsedTuple(CARD_SOCKET_HANDLE hSocket, UINT8  DesiredTuple, PVOID pBuf,PUINT pnItems){
    STATUS status = CERR_BAD_ARGS;

    if(pnItems == NULL)
        return status;
	
    DEBUGMSG(ZONE_FUNCTION, (TEXT("CardGetParsedTuple entered %d\r\n"), DesiredTuple));

    if (pnItems != NULL) {
        __try {
            switch (DesiredTuple) {
            case CISTPL_CONFIG:
                status = ParseConfig(hSocket, pBuf, pnItems);
                break;

            case CISTPL_CFTABLE_ENTRY_CB:
                status = ParseCfTable(hSocket, pBuf, pnItems);
                break;

            default:
                status = CERR_BAD_ARGS;
                break;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            status=CERR_BAD_HANDLE;
        }        
    }
    if (status) {
        DEBUGMSG(ZONE_FUNCTION|ZONE_ERROR,(TEXT("CardGetParsedTuple failed %d\r\n"), status));
    } else {
        DEBUGMSG(ZONE_FUNCTION, (TEXT("CardGetParsedTuple succeeded\r\n")));
    }
    return status;
}

//
// ParseConfig - Read the CISTPL_CONFIG tuple from the CIS and format it into a
// PARSED_CONFIG structure.
//
#define BUFFER_SIZE 0x200
#define PCI_CONFIG_CARDBUS_CIS  (10 << 2)
#define PCI_CONFIG_BASE0            (4 << 2)

STATUS ParseConfig(CARD_SOCKET_HANDLE hSocket, PVOID pBuf, PUINT   pnItems )
{
    STATUS status;
    PPARSED_CONFIG pCfg = (PPARSED_CONFIG)pBuf;
    UCHAR buf[BUFFER_SIZE + sizeof(CARD_DATA_PARMS)];
    PCARD_DATA_PARMS pData;
    PCARD_TUPLE_PARMS pTuple;
    PUCHAR pCIS;
    UINT   AddrSz;


    //
    // Find and read the second CISTPL_CONFIG tuple if possible.
    // (Some MFC cards contain a CISTPL_CONFIG in the CISTPL_LONGLINK_MFC chain)
    //
    pTuple = (PCARD_TUPLE_PARMS)buf;
    pTuple->hSocket = hSocket;
    pTuple->uDesiredTuple = CISTPL_CONFIG;
    pTuple->fAttributes = 0;         // Not interested in links
    status = CardGetFirstTuple(pTuple);
    if (status) {
        DEBUGMSG(ZONE_WARNING, (TEXT("ParseConfig: CardGetFirstTuple returned %d\r\n"), status));
        *pnItems = 0;
        return status;
    }

    status = CardGetNextTuple(pTuple);
    if (status) {
        status = CardGetFirstTuple(pTuple);
        if (status) {
            DEBUGMSG(ZONE_WARNING, (TEXT("ParseConfig: CardGetFirstTuple returned %d\r\n"), status));
            *pnItems = 0;
            return status;
        }
    }

    pData = (PCARD_DATA_PARMS)buf;
    pData->uBufLen = BUFFER_SIZE;
    pData->uTupleOffset = 0;
    status = CardGetTupleData(pData);
    if (status) {
        DEBUGMSG(ZONE_WARNING, (TEXT("ParseConfig: CardGetTupleData returned %d\r\n"), status));
        *pnItems = 0;
        return status;
    }

    if (pBuf == NULL) {
        *pnItems = 1;
        return CERR_SUCCESS;
    }

    if (*pnItems == 0) {
        return CERR_BAD_ARG_LENGTH;
    }

    *pnItems = 1;
    pCfg->ConfigBase = 0;
    pCfg->RegMask = 0;

    pCIS = buf + sizeof(CARD_DATA_PARMS);
    AddrSz = (*pCIS & 0x03) + 1;
    pCIS++;
    pCfg->LastConfigIndex = *pCIS & 0x3f;
    pCIS++;

    //
    // Read the configuration register offset address and leave it in real
    // physical address units.
    //
    pCfg->ConfigBase = VarToFixed(
                            AddrSz,
                            pCIS);          // pointer

    //
    // Now get the register presence mask (only the first byte)
    //
    pCfg->RegMask = pCIS[AddrSz];

    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("ParseConfig: Last configuration index = 0x%x\r\n"), pCfg->LastConfigIndex));
    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("ParseConfig: Config registers base = 0x%x\r\n"), pCfg->ConfigBase));
    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("ParseConfig: Register presence mask = 0x%x\r\n"), pCfg->RegMask));

    return CERR_SUCCESS;
}   // ParseConfig

//
// VarToFixed - convert the bytes of a variable length field into a UINT32
//              and return fixed length value.
//
UINT32 VarToFixed(UINT32 VarSize, PUCHAR pBytes)
{
    UINT32 Fixed = 0;

    //
    // Parse the bytes starting from the MSB and shift them into place.
    //
    while (VarSize) {
        VarSize--;
        Fixed <<= 8;
        Fixed |= (UINT32) pBytes[VarSize];
    }
    return Fixed;
}

STATUS ParseCfTable(CARD_SOCKET_HANDLE hSocket, PVOID pBuf,PUINT   pnItems )
{
    STATUS status;
    UCHAR buf[BUFFER_SIZE + sizeof(CARD_DATA_PARMS)];
    PCARD_DATA_PARMS pData;
    PCARD_TUPLE_PARMS pTuple;
    PUCHAR  pCIS;
    PPARSED_CBCFTABLE pCfTable = (PPARSED_CBCFTABLE)pBuf;
    PPARSED_CBCFTABLE pCfDefault = NULL;
    BOOL    IOPresent;      // This tuple contains I/O descriptor bytes
    DWORD   bmPowerPresent; // This tuple has some power descriptors
    int     lastCFI = -1;   // Configuration Index of the previous CFTABLE_ENTRY tuple
    INT    i;
    UINT    nItems;
    DWORD dwLen;
    BYTE IrqPresent, MemPresent, MiscPresent;	

    nItems = 0;

    //
    // Find the first CISTPL_CFTABLE_ENTRY tuple
    //
    pTuple = (PCARD_TUPLE_PARMS)buf;
    pTuple->hSocket = hSocket;
    pTuple->uDesiredTuple = CISTPL_CFTABLE_ENTRY_CB;
    pTuple->fAttributes = 0;         // Not interested in links
    status = CardGetFirstTuple(pTuple);
    if (status) {//use default instead
        nItems = 1;
	 if(pBuf){
		CreateDefaultInfo(hSocket, (PPARSED_CBCFTABLE)pBuf);
	 }
	 status = CERR_SUCCESS;
        goto pcft_exit;
    }

    if (pBuf != NULL) {
        if (*pnItems == 0) {
            status = CERR_BAD_ARG_LENGTH;
        } else {
            memset(pBuf, 0, *pnItems * sizeof(PARSED_CBCFTABLE));
        }
    }

    //
    // Parse all the CISTPL_CFTABLE_ENTRYs or as many as fit in the user's buffer
    //
    while (status == CERR_SUCCESS) {
        nItems++;
        if (pBuf == NULL) {
            goto pcft_next;
        }

        pData = (PCARD_DATA_PARMS)buf;
        pData->uBufLen = BUFFER_SIZE;
        pData->uTupleOffset = 0;
        status = CardGetTupleData(pData);
        if (status) {
            DEBUGMSG(ZONE_WARNING,
            (TEXT("ParseCfTable: CardGetTupleData returned %d\r\n"), status));
            goto pcft_exit;
        }
	 dwLen= min(BUFFER_SIZE-sizeof(CARD_DATA_PARMS),pData->uDataLen);
        //
        // Parse this CISTPL_CFTABLE_ENTRY
        //
        pCIS = buf + sizeof(CARD_DATA_PARMS);

        // Default all the fields of the current parsed entry if necessary
        //
        // !! The ContainsDefaults field is now deprecated and should be
        // !! set to non-zero in every parsed entry.
        //
        pCfTable->ContainsDefaults = *pCIS & 0x40;
        pCfTable->ConfigIndex = *pCIS & 0x3F;
        if (pCfTable->ContainsDefaults)
            pCfDefault = pCfTable;
        else if (pCfTable->ConfigIndex == lastCFI) {
            ASSERT(pCfTable != pBuf && lastCFI >= 0);
            *pCfTable = *(pCfTable - 1);
            pCfTable->ContainsDefaults = TRUE;
        } else if (pCfDefault)
            *pCfTable = *pCfDefault;

        // Reset the ConfigIndex after defaulting
        pCfTable->ConfigIndex = *pCIS & 0x3F;
        lastCFI = pCfTable->ConfigIndex;
        // lastCFI is not used beyond this point
        pCIS++;
        if (dwLen--==0)  goto pcft_continue;


        //
        // Parse the feature select byte
        //

        //
        // Power requirements present
        //
        bmPowerPresent = *pCIS & 0x03;
        //
        // I/O space descriptions present
        //
        IOPresent = *pCIS & 0x08;
        //
        // Irq description present
        //
        IrqPresent = *pCIS & 0x10;
        //
        // Memory description present
        //
        MemPresent = *pCIS & 0x20;
        //
        // Misc description present
        //
        MiscPresent = *pCIS & 0x80;


        pCIS++;
        if (dwLen--==0)  goto pcft_continue;

        //
        // Parse the power description structures if present
        //
	    DWORD dwParsedLength;
	    if (bmPowerPresent >= 1) {
	        pCfTable->VccDescr.ValidMask = 0xFF;
	        pCIS +=(dwParsedLength=ParseVoltageDescr(pCIS, &pCfTable->VccDescr));
	        if (dwParsedLength >dwLen)  goto pcft_continue;
	        dwLen -=dwParsedLength;
	        for (i = 0; i < 3; i++) {
	            if ((PWR_DESCR_NOMINALV << i) & pCfTable->VccDescr.ValidMask )
	                pCfTable->VccDescr.ValidMask |= (PWR_AVAIL_NOMINALV << i);
	        }
	    }
	    if (bmPowerPresent >= 2) {
	        pCfTable->Vpp1Descr.ValidMask = 0xFF;
	        pCIS += (dwParsedLength=ParseVoltageDescr(pCIS, &pCfTable->Vpp1Descr));
	        if (dwParsedLength >dwLen)  goto pcft_continue;
	        dwLen -=dwParsedLength;
	        
	        for (i = 0; i < 3; i++) {
	            if ((PWR_DESCR_NOMINALV << i) & pCfTable->Vpp1Descr.ValidMask )
	                pCfTable->Vpp1Descr.ValidMask |= (PWR_AVAIL_NOMINALV << i);
	        }
	        
	        pCfTable->Vpp2Descr.ValidMask = 0xFF;
	        if (bmPowerPresent == 3) {
	            pCIS += (dwParsedLength=ParseVoltageDescr(pCIS, &pCfTable->Vpp2Descr));
		        if (dwParsedLength >dwLen)  goto pcft_continue;
		        dwLen -=dwParsedLength;
	            for (i = 0; i < 3; i++) {
	                if ((PWR_DESCR_NOMINALV << i) & pCfTable->Vpp1Descr.ValidMask )
	                    pCfTable->Vpp2Descr.ValidMask |= (PWR_AVAIL_NOMINALV << i);
	            }
	        } else
	            pCfTable->Vpp2Descr = pCfTable->Vpp1Descr;
	    }
	    

        //
        // Process the I/O address ranges (up to MAX_IO_RANGES)
        //
        if (IOPresent) {
            pCfTable->bUsedBarForIo = ((*pCIS)>>1) & 0x1f;
            pCIS++;
            if (dwLen--==0)  goto pcft_continue;
        }

        // Irq description present
        //
        if ( IrqPresent) {
            if ((*pCIS & 0x10)==0) { // Mask Bit is set
                pCfTable->wIrqMask =(1<< (*pCIS & 0xf));
                pCIS++;
	          if (dwLen--==0)  goto pcft_continue;
            }
            else {
	          if (dwLen <3)  goto pcft_continue;
                pCIS++;
                pCfTable->wIrqMask=*pCIS+(((WORD)*(pCIS+1))<<8);
                pCIS+=2;
                dwLen -= 3;
            }
        }

        if (MemPresent) {
            pCfTable->bUsedBarForMem  = ((*pCIS)>>1) & 0x7f; // 7 Bit. INCLUDE ROM.
            pCIS++;
            if (dwLen--==0)  goto pcft_continue;
        }

        // else it has already been either defaulted or memset to zeros.

pcft_continue:
        pCfTable++;

pcft_next:
        if ((pBuf != NULL) && (nItems == *pnItems)) {
            status = CERR_NO_MORE_ITEMS;    // no more room in caller's buffer
        } else {
            //
            // Get the next CISTPL_CFTABLE_ENTRY
            //
            status = CardGetNextTuple(pTuple);
        }
    }   // while (status == CERR_SUCCESS)

pcft_exit:
    if(pnItems != NULL)
        *pnItems = nItems;

    //
    // If there were problems and no CFTABLE_ENTRYs found then return an error.
    //
    if (nItems) {
        return CERR_SUCCESS;
    }
    return status;

}
USHORT VoltageConversionTable[16] = {
    10, 12, 13, 15, 20, 25, 30, 35,
    40, 45, 50, 55, 60, 70, 80, 90
};

//
// ConvertVoltage - normalize a single CISTPL_CFTABLE_ENTRY voltage entry and
//                  return its length in bytes
//
UINT ConvertVoltage(PUCHAR pCIS, PUSHORT pDescr)
{
    UINT Length;
    SHORT power;
    USHORT value;
    UCHAR MantissaExponentByte;
    UCHAR ExtensionByte;

    Length = 1;
    power = 1;
    MantissaExponentByte = pCIS[0];
    ExtensionByte = pCIS[1];
    value = VoltageConversionTable[(MantissaExponentByte >> 3) & 0x0f];

    if ((MantissaExponentByte & TPCE_PD_EXT) &&
        (ExtensionByte < 100)) {
        value = (100 * value + (ExtensionByte & 0x7f));
        power += 2;
    }

    power = (MantissaExponentByte & 0x07) - 4 - power;

    while (power > 0) {
        value *= 10;
        power--;
    }

    while (power < 0) {
        value /= 10;
        power++;
    }

    *pDescr = value;

    //
    // Skip any subsequent extension bytes for now.
    //
    while (*pCIS & TPCE_PD_EXT) {
        Length++;
        pCIS++;
    }

    return Length;
}   // ConvertVoltage


//
// ParseVoltageDescr - convert the variable length power description structure
//                     from a CISTPL_CFTABLE_ENTRY to a POWER_DESCR structure
//                     and normalize the values.
// Returns the length of the variable length power description structure.
//
// A typical call would be:
//        if (pCfTable->VccDescr.ValidMask) {
//            pCIS += ParseVoltageDescr(pCIS, &pCfTable->VccDescr);
//        }
//
UINT ParseVoltageDescr(PUCHAR pVolt, PPOWER_DESCR pDescr)
{
    UINT Length;
    UINT Mask;
    UINT i;
    PUSHORT pDVolt;

    Length = 1;
    Mask = pDescr->ValidMask = (UINT)*pVolt;
    pVolt++;
    pDVolt = &(pDescr->NominalV);

    while (Mask) {
        if (Mask & 1) {
            i = ConvertVoltage(pVolt, pDVolt);
            Length += i;
            pVolt += i;
        } else {
            *pDVolt = 0;
        }
        pDVolt++;
        Mask >>= 1;
    }

    return Length;
}   // ParseVoltageDescr

void CreateDefaultInfo(CARD_SOCKET_HANDLE hSocket, PPARSED_CBCFTABLE pCfTable)
{
    ASSERT(pCfTable);
    PCB_BUS_INFO pBusInfo = NULL;
    if	(GetCBCardBusInfo(hSocket, &pBusInfo) == FALSE){
        return;   
    }
	
    memset(pCfTable,0,sizeof(PARSED_CBCFTABLE));
    // Scan Bar
    BYTE bShiftBit=1;
    for (DWORD dwIndex=0;dwIndex<PCI_TYPE0_ADDRESSES;dwIndex++) {
        DWORD dwPos=dwIndex*sizeof(DWORD)+PCI_CONFIG_BASE0;
        // GetValue
        DWORD dwOldValue=PCIConfig_Read(dwPos, pBusInfo);
        PCIConfig_Write(dwPos,(DWORD)-1, pBusInfo);
        DWORD dwSizeValue = PCIConfig_Read(dwPos, pBusInfo);
        PCIConfig_Write(dwPos,dwOldValue, pBusInfo);
        
        if (dwSizeValue & 1) { // IO
            dwSizeValue &=0xfffffffc;
            g_CBCardInfo.m_BARType[dwIndex]=1;// for IO;
        }
        else {
            dwSizeValue &=0xfffffff0;
            g_CBCardInfo.m_BARType[dwIndex]=0;// for Memory
        }
        g_CBCardInfo.m_BARSize[dwIndex]=(~dwSizeValue)+1;
        if (g_CBCardInfo.m_BARSize[dwIndex]!=0) {
            if (g_CBCardInfo.m_BARType[dwIndex]!=0)
                pCfTable->bUsedBarForIo = bShiftBit;
            else
                pCfTable->bUsedBarForMem = bShiftBit;
        }
        bShiftBit <<=1;
    }
    DWORD dwValue= PCIConfig_Read(15*sizeof(DWORD), pBusInfo);
    if ((dwValue & 0xff00)!=0) { // If there is interrupt PIN value, Assume it need irq
        pCfTable->wIrqMask = (WORD) -1;
    };
}
DWORD PCIConfig_Read(DWORD dwOffset, PCB_BUS_INFO pInfo)
{
    DWORD RetVal = 0;
    if(pInfo == NULL)
        return (DWORD)-1;
    HalGetBusDataByOffset(PCIConfiguration, pInfo->dwBusNumber, pInfo->SlotNumber.u.AsULONG, &RetVal, dwOffset, sizeof(RetVal));
    return RetVal;
}

void PCIConfig_Write(DWORD dwOffset,DWORD dwValue, PCB_BUS_INFO pInfo)
{
    if(pInfo == NULL)
        return;
    HalSetBusDataByOffset(PCIConfiguration, pInfo->dwBusNumber, pInfo->SlotNumber.u.AsULONG, &dwValue, dwOffset, sizeof(dwValue));
}


VOID Add_CBCardBusInfo(CARD_SOCKET_HANDLE hSocket, DWORD dwBusNumber, INTERFACE_TYPE iType, PCI_SLOT_NUMBER SlotNumber)
{
	PCB_BUS_INFO pNode = new CB_BUS_INFO;
	if(pNode == NULL)//out of memory, should not happen though
		return;
	pNode->hSocket = hSocket;
	pNode->dwBusNumber = dwBusNumber;
	pNode->InterfaceType = iType;
	pNode->SlotNumber = SlotNumber;
	pNode->pNext = NULL;

	if(g_pCBBusInfoHead == NULL){
		g_pCBBusInfoHead = pNode;
	}
	else{
		PCB_BUS_INFO pCur = g_pCBBusInfoHead;
		while(pCur->pNext!= NULL)
			pCur= pCur->pNext;
		pCur->pNext = pNode;
	}
}

VOID Delete_CBCardBusInfo(CARD_SOCKET_HANDLE hSocket){
	PCB_BUS_INFO pCur, pPrev;

	if(g_pCBBusInfoHead == NULL)
		return;

	pPrev=pCur= g_pCBBusInfoHead;
	do{
		if((pCur->hSocket.uFunction == hSocket.uFunction) || (pCur->hSocket.uSocket == hSocket.uSocket))
			break;
		pPrev = pCur;
		pCur = pCur->pNext;
	}while(pCur != NULL);

	if(pCur == NULL){//not found
		return;
	}
	else{
		if(pPrev == g_pCBBusInfoHead){//this is the head
			delete pPrev;
			g_pCBBusInfoHead = NULL;
		}
		else{
			pPrev->pNext = pCur->pNext;
			delete pCur;
		}
	}
	
}

BOOL GetCBCardBusInfo(CARD_SOCKET_HANDLE hSocket, PCB_BUS_INFO *ppInfo){
	if(ppInfo == NULL)
		return FALSE;

	PCB_BUS_INFO pCur = g_pCBBusInfoHead;

	while(pCur != NULL){
		if((pCur->hSocket.uFunction == hSocket.uFunction) || (pCur->hSocket.uSocket == hSocket.uSocket))
			break;
		pCur = pCur->pNext;
	}

	if(pCur != NULL){//found
		*ppInfo = pCur;
		return TRUE;
	}
	else
		return FALSE;

}

VOID Cleanup_CBCardBusInfo(){
	PCB_BUS_INFO pCur, pNext;

	pCur = g_pCBBusInfoHead;
	while(pCur != NULL){
		pNext = pCur->pNext;
		delete pCur;
		pCur = pNext;
	}

	g_pCBBusInfoHead = NULL;
}
