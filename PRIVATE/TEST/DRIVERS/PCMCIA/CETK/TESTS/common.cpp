//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
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

UINT16   rgWinAttr[MAX_WIN_ATTR] =
{
	WIN_ATTR_IO_SPACE      			,
	WIN_ATTR_ATTRIBUTE     			,
	WIN_ATTR_ENABLED       			,
	WIN_ATTR_16BIT         			,
	WIN_ATTR_PAGED         			,
	WIN_ATTR_SHARED        			,
	WIN_ATTR_FIRST_SHARED  			,
	WIN_ATTR_OFFSETS_SIZED 		,
	WIN_ATTR_ACCESS_SPEED_VALID	,

// OR with WIN_ATTR_ACCESS_SPEED_VALID	
	0x203,
	0x205,
	0x206,
	0x207,
	0x209,
	0x20A,
	0x20B,
	0x20C,
	0x20D,
	0x20E,
	0x20F,
	0x210,
	0x218,
	0x22F,
	0x238,
	0x248,
	0x348,
	0x368,
	0x370,
	0x378,
	0x36F
};

#define STREAM_ECHO_LEN       	10
#define STREAM_DIAL_IN_LEN    	12
#define STREAM_DIAL_OUT1_LEN  	17
#define STREAM_DIAL_OUT2_LEN  	17
#define STREAM_DIAL_OUT3_LEN  	20

STREAMTYPE   rgStreamType[MAX_MY_STREAM] =
{
	{STREAM_ECHO_LEN,      	{"ATE1\r\nAT\r\n"}},
	{STREAM_DIAL_IN_LEN,   	{"ATSO=1\r\nAT\r\n"}        }, // must have ""AT\r\n"
	{STREAM_DIAL_OUT1_LEN, 	{"ATS0=0 DT 66991\r\n"}     },
	{STREAM_DIAL_OUT2_LEN, 	{"ATS0=0 DT 66991\r\n"}     },
	{STREAM_DIAL_OUT3_LEN, 	{"ATS0=0 DT 99366991\r\n"}  },
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
	UINT16 				fState;

	//get the event name string
    	pEventName = FindEventName(EventCode);

    	// Make sure everything comes in as expected.
    	if (pParms == NULL){
   		g_pKato->Log(LOG_DETAIL,TEXT("CallBackFn: (EventCode=0x%lx=%s): pParms == NULL!!!\r\n"),
				 EventCode,	(LPTSTR) pEventName);
      		goto CBF_END;
	}

    	pData = (PCLIENT_CONTEXT)(pParms->uClientData);
	if (pData == NULL){
		g_pKato->Log(LOG_DETAIL,TEXT("\r\n**********: CallBackFn: (Event=0x%lx=%s): return right away !!!\r\n"),
				EventCode, (LPTSTR)pEventName);
      		goto CBF_END;
	}
	else{
		wsprintf(lpClient, TEXT("Thread %u"), pData->ThreadNo);
    		g_pKato->Log(LOG_DETAIL,TEXT("\r\n**********: %s: CallBackFn: (Event=0x%lx=%s): uSocket=0x%x uFun=0x%lx Parm1=0x%lx  Parm2=0x%lx\r\n"),
				(LPTSTR)lpClient, EventCode, (LPTSTR)pEventName, hSocket.uSocket, hSocket.uFunction, pParms->Parm1, pParms->Parm2);
	}

	switch (EventCode){
		
		case CE_EXCLUSIVE_COMPLETE:
			pData->fCardReqExclDone = TRUE;
			pData->uCardReqStatus =  pParms->Parm1; // Exclusive Status
			pData->uEventGot +=  CE_EXCLUSIVE_COMPLETE;
			if (pParms->Parm1 == CERR_SUCCESS){
                   		gfSomeoneGotExclusive = TRUE;     // Someone has get the exclusive access
				g_pKato->Log(LOG_DETAIL,TEXT("%s: CE_EXCLUSIVE_COMPLETE: pData->uEvent=0x%lx:  Request succeed\r\n"),
							(LPTSTR)lpClient, pData->uEventGot);
			}
			else{
				g_pKato->Log(LOG_DETAIL,TEXT("%s: CE_EXCLUSIVE_COMPLETE: pData->uEvent=0x%lx:  Request Fail: status=0x%lX\r\n"),
							 (LPTSTR)lpClient, pData->uEventGot, pParms->Parm1 );
					}
			 if (pData->hEvent)
      				SetEvent(pData->hEvent);

			if (dwTotalThread > 0)
				InterlockedIncrement((long*)&nThreadsDone);
			break;
			
		case CE_REGISTRATION_COMPLETE:
			pData->uCardReqStatus =  pParms->Parm1; //@comm  contains the registered handle
      			if (pParms->Parm1 == 0) { //Parm1=Client hClient
				g_pKato->Log(LOG_DETAIL,TEXT("Failed:  %s: CallBackFn: CE_REGISTRATION_COMPLETE: pParm->Parm1=NULL\r\n"),
              								(LPTSTR)lpClient,  pParms->Parm1);
			}
	      		pData->fEventGot |=  CE_REGISTRATION_COMPLETE; //for wintst
			if (pData->hEvent)
				SetEvent(pData->hEvent);
      			break;
      			
    		case CE_CARD_INSERTION:
      			if (pParms->Parm1 == 0) { // expect: Parm1 == PNP device id string
				g_pKato->Log(LOG_DETAIL,TEXT("FAILed:  %s: CallBackFn: CE_CARD_INSERTION: pParm->Parm1 = NULL\r\n"),
         	  								(LPTSTR)lpClient);
			}
      			if (pParms->Parm2 == 0) {  //Parm2=Client hClient
				g_pKato->Log(LOG_DETAIL,TEXT("Failed:  %s: CallBackFn: CE_CARD_INSERTION: pParm->Parm2=0x%lx: != expected Handle=0x%lx\r\n"),
              								(LPTSTR)lpClient,  pParms->Parm2, pData->hClient);
			}
  			 pData->uEventGot +=  CE_CARD_INSERTION;
  			 pData->fEventGot |=  CE_CARD_INSERTION;
			 g_pKato->Log(LOG_DETAIL,TEXT("%s: CE_CARD_INSERTION: pData->uEvent=0x%lx: pData->hClient=0x%lX\r\n"),
							 (LPTSTR)lpClient, pData->uEventGot, pData->hClient );
        		// Determine if this is an artificial insertion notice
        		CardStatus.hSocket = hSocket;
        		uRet = CardGetStatus(&CardStatus);
      			if (uRet)
				 {
				   g_pKato->Log(LOG_DETAIL,TEXT("FAIL %s: CallBackFn: CE_CARD_INSERTION: CardGetStatus returned 0x%lx\r\n"),
									 (LPTSTR)lpClient, uRet);
         		}
      			break;
      			
    		case CE_CARD_REMOVAL:    
			 pData->uEventGot +=  CE_CARD_REMOVAL;
			 g_pKato->Log(LOG_DETAIL,TEXT("%s: CE_CARD_REMOVAL: pData->uEvent=0x%lx\r\n"),
						(LPTSTR)lpClient, pData->uEventGot );
        		// Determine whether this is an artificial removal notice.
        		CardStatus.hSocket = hSocket;
        		uRet = CardGetStatus(&CardStatus);
    			if (uRet){
              		g_pKato->Log(LOG_DETAIL,TEXT("FAIL:  CallBackFn: CE_CARD_REMOVAL: CardGetStatus returned 0x%lx\r\n"), uRet);
      			}
			else	 {// CERR_SUCCESS = 0
              		fState = CardStatus.fSocketState & EVENT_MASK_CARD_DETECT;
				if (fState != EVENT_MASK_CARD_DETECT){
				//    g_pKato->Log(LOG_DETAIL,TEXT("FAIled:  CallBackFn: CE_CARD_REMOVAL: real remove notice NOT detect: fState=0x%lx\r\n"),
      	           	      //                       fState);
				}
				else 
					pData->fCardRemoved = TRUE;
      			}
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

