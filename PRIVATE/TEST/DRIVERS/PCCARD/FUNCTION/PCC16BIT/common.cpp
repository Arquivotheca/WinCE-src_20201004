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
    {TEXT("CISTPL_NULL"),                       CISTPL_NULL                  , 0                  },
    {TEXT("CISTPL_DEVICE"),                    CISTPL_DEVICE                , CARD_IO|CARD_MEMORY},
    {TEXT("CISTPL_LONGLINK_CB"),            CISTPL_LONGLINK_CB           , 0                  },
    {TEXT("CISTPL_CONFIG_CB"),               CISTPL_CONFIG_CB             , 0                  },
    {TEXT("CISTPL_CFTABLE_ENTRY_CB"), CISTPL_CFTABLE_ENTRY_CB      , 0                  },
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
STATUS 
CallBackFn_Client(
    CARD_EVENT EventCode,
    CARD_SOCKET_HANDLE hSocket,
    PCARD_EVENT_PARMS pParms
    )
{

    PCLIENT_CONTEXT   pData;
    TCHAR             	lpClient[25] ;
    LPTSTR			pEventName = NULL;
    STATUS 			status = CERR_SUCCESS;
    STATUS 			uRet;
    CARD_STATUS         CardStatus;             // For CardGetStatus

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
	
    uParsedSize = uParsedItems * sizeof (PARSED_PCMCFTABLE) ;
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
    PARSED_PCMCFTABLE* pTable = NULL;
    BOOL	fConfigured = FALSE;
    for(UINT i = 0; i < uParsedItems; i++){
        pTable = (PARSED_PCMCFTABLE *)pCur;
        if(pTable->IFaceType != 1){// not I/O config
            pCur = (PBYTE)pCur + sizeof(PARSED_PCMCFTABLE);
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
        
        pCur = (PBYTE)pCur + sizeof(PARSED_PCMCFTABLE);
    }

    delete[] parsedBuf;
    return fConfigured;
}

BOOL 
RetrieveRegisterValues(PCARD_CONFIG_INFO pConfInfo, CARD_SOCKET_HANDLE hSocket){

    if(pConfInfo == NULL)
        return FALSE;

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
	
    DEBUGMSG(ZONE_VERBOSE, (TEXT("CardGetParsedTuple entered %d\r\n"), DesiredTuple));

    if (pnItems != NULL) {
        __try {
            switch (DesiredTuple) {
            case CISTPL_CONFIG:
                status = ParseConfig(hSocket, pBuf, pnItems);
                break;

            case CISTPL_CFTABLE_ENTRY:
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
        DEBUGMSG(ZONE_ERROR,(TEXT("Socket %u, Func %u: CardGetParsedTuple for %d failed, error= %d\r\n"), 
                                             hSocket.uSocket, hSocket.uFunction, DesiredTuple, status));
    } else {
        DEBUGMSG(ZONE_VERBOSE, (TEXT("CardGetParsedTuple succeeded\r\n")));
    }
    return status;
}

//
// ParseConfig - Read the CISTPL_CONFIG tuple from the CIS and format it into a
// PARSED_CONFIG structure.
//
#define BUFFER_SIZE 0x200
STATUS 
ParseConfig(CARD_SOCKET_HANDLE hSocket, PVOID pBuf, PUINT   pnItems )
{

    if(pBuf == NULL || pnItems == NULL)
        return CERR_BAD_ARGS;
        
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
    if(pBytes == NULL)
        return CERR_BAD_ARGS;
        
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

    if(pnItems == NULL)
        return CERR_BAD_ARGS;
        
    STATUS status;
    UCHAR buf[BUFFER_SIZE + sizeof(CARD_DATA_PARMS)];
    PCARD_DATA_PARMS pData;
    PCARD_TUPLE_PARMS pTuple;
    PUCHAR  pCIS;
    PUCHAR  pTmp;
    PPARSED_PCMCFTABLE pCfTable = (PPARSED_PCMCFTABLE)pBuf;
    PPARSED_PCMCFTABLE pCfDefault = NULL;
    BOOL    TimingPresent;  // This tuple contains a description of the card's timing
    BOOL    IOPresent;      // This tuple contains I/O descriptor bytes
    DWORD   bmPowerPresent; // This tuple has some power descriptors
    int     lastCFI = -1;   // Configuration Index of the previous CFTABLE_ENTRY tuple
    INT    i;
    UINT    nItems;
    DWORD dwLen;
    BYTE IrqPresent, MemPresent;	

    nItems = 0;

    //
    // Find the first CISTPL_CFTABLE_ENTRY tuple
    //
    pTuple = (PCARD_TUPLE_PARMS)buf;
    pTuple->hSocket = hSocket;
    pTuple->uDesiredTuple = CISTPL_CFTABLE_ENTRY;
    pTuple->fAttributes = 0;         // Not interested in links
    status = CardGetFirstTuple(pTuple);
    if (status) {
        DEBUGMSG(ZONE_WARNING,
            (TEXT("ParseCfTable: CardGetFirstTuple returned %d\r\n"), status));
        goto pcft_exit;
    }

    if (pBuf != NULL) {
        if (*pnItems == 0) {
            status = CERR_BAD_ARG_LENGTH;
        } else {
            memset(pBuf, 0, *pnItems * sizeof(PARSED_CFTABLE));
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

        //
        // Parse the interface type byte if present
        //
        if (*pCIS & 0x80) {
            pCIS++;
            if (dwLen--==0)  goto pcft_continue;
            pCfTable->IFacePresent = TRUE;
            pCfTable->IFaceType     = *pCIS & 0x0F;
            pCfTable->BVDActive     = *pCIS & 0x10;
            pCfTable->WPActive      = *pCIS & 0x20;
            pCfTable->ReadyActive   = *pCIS & 0x40;
            pCfTable->WaitRequired  = *pCIS & 0x80;
        } else {
            //pCfTable->IFacePresent = FALSE;
        }
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
        // Timing description present
        //
        TimingPresent = *pCIS & 0x04;

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
        MemPresent = *pCIS & 0x60;


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
        // Skip the timing information
        //
        if ( TimingPresent ) {
            pTmp = pCIS;

            pCIS++;
            if (dwLen--==0)  goto pcft_continue;
            if ( (*pTmp & 0x03) != 0x03 ) {
                while ( *pCIS++ & TPCE_PD_EXT ){
			        if (dwLen--==0)  goto pcft_continue;
                }
            }
            if ( (*pTmp & 0x1C) != 0x1C ) {
                while ( *pCIS++ & TPCE_PD_EXT ){
			        if (dwLen--==0)  goto pcft_continue;
                }
            }
        }


        //
        // Process the I/O address ranges (up to MAX_IO_RANGES)
        //
        if (IOPresent) {
            pCfTable->NumIOAddrLines = *pCIS & 0x1F;
            pCfTable->IOAccess = (*pCIS & 0x60) >> 5;   // type of access allowed
            if (*pCIS & 0x80) {  // range present bit
                UINT8   AddrSize, LengthSize;
                pCIS++;
	          if (dwLen--==0)  goto pcft_continue;
                pCfTable->NumIOEntries = (*pCIS & 0x0F) + 1;
                if (pCfTable->NumIOEntries > MAX_IO_RANGES) {
                    DEBUGMSG(ZONE_WARNING,
                             (TEXT("PCMCIA ParseCfTable: too many I/O ranges; ignoring some.\n")));
                    pCfTable->NumIOEntries = MAX_IO_RANGES;
                }
                AddrSize = (*pCIS & 0x30) >> 4;
                if (AddrSize == 3) {
                    AddrSize = 4;
                }
                LengthSize = (*pCIS & 0xC0) >> 6;
                if (LengthSize == 3) {
                    LengthSize = 4;
                }
                pCIS++;
	          if (dwLen--==0)  goto pcft_continue;

                for (i = 0; i < pCfTable->NumIOEntries; i++) {
                    pCfTable->IOBase[i] = VarToFixed(AddrSize, pCIS);
                    pCIS += AddrSize;
			 if((dwLen - AddrSize) <= 0) goto pcft_continue;
                    pCfTable->IOLength[i] = VarToFixed(LengthSize, pCIS);
                    pCIS += LengthSize;
			 if((dwLen - AddrSize) <= 0) goto pcft_continue;
                }
		
            } else {
                // NumIOEntries is a misnomer; it's really the number of IO ranges.
                pCfTable->NumIOEntries = 0;
            }
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
            if (MemPresent == 0x20) {//Signle 2 byte length present
                if (dwLen<2) goto pcft_continue;
                UINT32 uSize = *pCIS + (*(pCIS+1)<<8);
                ASSERT(uSize!=0);
                pCIS += 2;
                pCfTable->NumMemEntries= 1;
                pCfTable->MemBase[0] = 0;
                pCfTable->MemLength[0] = uSize*0x100;
                dwLen -=2;
            }
            else
            if (MemPresent == 0x40) {//Signle 2 byte length & base present
                if (dwLen<4) goto pcft_continue;
                UINT32 uSize = *pCIS + (*(pCIS+1)<<8);
                ASSERT(uSize!=0);
                pCIS +=2;
                UINT32 uBase =*pCIS + (*(pCIS+1)<<8);
                pCIS +=2;
                pCfTable->NumMemEntries = 1;
                pCfTable->MemLength[0] = uSize*0x100;
                pCfTable->MemBase[0] = uBase*0x100;
            }
            else { // Memory Descriptor structure present.
                UINT8   AddrSize, LengthSize,bHostAddress, NumOfEntries;
                pCfTable->NumMemEntries = NumOfEntries= (*pCIS & 0x07) + 1;
                LengthSize = ((*pCIS & 0x18)>>3);
                AddrSize =  ((*pCIS & 0x60)>> 5);
                bHostAddress = (*pCIS & 0x80);
                if (pCfTable->NumMemEntries > MAX_WINDOWS_RANGES) {
                    DEBUGMSG(ZONE_WARNING, (TEXT("PCMCIA ParseCfTable: too many I/O ranges; ignoring some.\n")));
                    pCfTable->NumMemEntries = MAX_IO_RANGES;
                }
                pCIS++;
                if (dwLen--==0)  goto pcft_continue;
                for (i =0 ; i< NumOfEntries; i ++) {
                    UINT32 uSize = 0;
                    UINT32 uBase = 0;
                    UINT32 uShift = 0;
                    for (int NumOfBytes =0;  NumOfBytes < LengthSize; NumOfBytes++) {
                        uSize += ((*pCIS)<<uShift);
                        pCIS++;
                        uShift +=8;
                        if (dwLen--==0) goto pcft_continue;
                    }
                    for (NumOfBytes =0;  NumOfBytes < AddrSize; NumOfBytes++) {
                        uBase += ((*pCIS)<<uShift);
                        pCIS++;
                        uShift +=8;
                        if (dwLen--==0) goto pcft_continue;
                    }
                    // We ignore host address. but we have to parsed it.
                    for (NumOfBytes =0;  NumOfBytes < AddrSize && bHostAddress!=0; NumOfBytes++) {
                        pCIS++;
                        if (dwLen--==0)  goto pcft_continue;
                    }
                    ASSERT(uSize!=0 );
                    if (i < MAX_WINDOWS_RANGES) {
                        pCfTable->MemBase[i] = uBase*0x100;
                        pCfTable->MemLength[i] = uSize*0x100;
                    }
    
                }
            }
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
    if(pCIS == NULL || pDescr == NULL)
        return 0;
        
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
    if(pVolt == NULL || pDescr == NULL)
        return 0;
        
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

