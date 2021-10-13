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
			
      			if (pParms->Parm1 == 0) { // expect: Parm1 == PNP device id string
				g_pKato->Log(LOG_DETAIL,TEXT("FAILed:  %s: CallBackFn: CE_CARD_INSERTION: pParm->Parm1 = NULL\r\n"),
         	  								(LPTSTR)lpClient);
			}
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

BOOL 
NormalRequestConfig(CARD_CLIENT_HANDLE hClient, UINT8 uSock, UINT8 uFunc){

    if(hClient == NULL)
        return FALSE;

    CARD_CONFIG_INFO     		confInfo = {0};
    STATUS					status = CERR_SUCCESS;
    CARD_SOCKET_HANDLE		hSocket = {uSock, uFunc};
    UINT32					uParsedItems = 0;
    UINT32					uParsedSize = 0;
    PVOID					      parsedBuf;
	
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

        //calling CardRequestCOnfiguration using normal parameters
        confInfo.hSocket.uFunction = uFunc;
        confInfo.hSocket.uSocket = uSock;
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
                                                uSock, uFunc);
            delete[] parsedBuf;
            return FALSE;
        }

        //now request configuration
        status = CardRequestConfiguration(hClient, &confInfo);
        if(status != CERR_SUCCESS){//if it failed, try the next configuation table
            g_pKato->Log(LOG_FAIL,TEXT("Socket %u: Function %u: Request config on No. %u config table failed\r\n"), 
                                                uSock, uFunc, i);
            g_pKato->Log(LOG_FAIL,TEXT("uVcc = %u, uVpp1 = %u, uVpp2 = %u\r\n"), 
                                               confInfo.uVcc, confInfo.uVpp1, confInfo.uVpp2);
                                               
        }
        else{
            g_pKato->Log(LOG_DETAIL,TEXT("Socket %u: Function %u: RequestConfig on No. %u config table succeed!\r\n"), 
                                                uSock, uFunc, i);
            g_pKato->Log(LOG_DETAIL,TEXT("uVcc = %u, uVpp1 = %u, uVpp2 = %u\r\n"), 
                                               confInfo.uVcc, confInfo.uVpp1, confInfo.uVpp2);
            fConfigured = TRUE;
            break;
        }
        
        pCur = (PBYTE)pCur + sizeof(PARSED_CFTABLE);
    }

    delete[] parsedBuf;
    return fConfigured;
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

