//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <tapi.h>
#include <tchar.h>
#include "TapiInfo.h"

// Maximum length of all internal string buffers.
#define MAXOUTPUTSTRINGLENGTH 4096 

// define to make accessing arrays easy.
#define sizeofArray(pArray) (sizeof(pArray) / sizeof((pArray)[0]))

//
// Internal prototypes.
//
static long strBinaryArrayAppend(LPTSTR pszOutputBuffer, DWORD dwFlags,
     LPTSTR szStringArray[], DWORD dwSizeofStringArray);


//
// Global arrays for interpreting TAPI constants.
//
LPTSTR pszLineError[] = 
{
	TEXT("***"),
	TEXT("LINEERR_ALLOCATED"),
	TEXT("LINEERR_BADDEVICEID"),
	TEXT("LINEERR_BEARERMODEUNAVAIL"),
	TEXT("LINEERR Unused constant, ERROR!!"),
	TEXT("LINEERR_CALLUNAVAIL"),
	TEXT("LINEERR_COMPLETIONOVERRUN"),
	TEXT("LINEERR_CONFERENCEFULL"),
	TEXT("LINEERR_DIALBILLING"),
	TEXT("LINEERR_DIALDIALTONE"),
	TEXT("LINEERR_DIALPROMPT"),
	TEXT("LINEERR_DIALQUIET"),
	TEXT("LINEERR_INCOMPATIBLEAPIVERSION"),
	TEXT("LINEERR_INCOMPATIBLEEXTVERSION"),
	TEXT("LINEERR_INIFILECORRUPT"),
	TEXT("LINEERR_INUSE"),
	TEXT("LINEERR_INVALADDRESS"),
	TEXT("LINEERR_INVALADDRESSID"),
	TEXT("LINEERR_INVALADDRESSMODE"),
	TEXT("LINEERR_INVALADDRESSSTATE"),
	TEXT("LINEERR_INVALAPPHANDLE"),
	TEXT("LINEERR_INVALAPPNAME"),
	TEXT("LINEERR_INVALBEARERMODE"),
	TEXT("LINEERR_INVALCALLCOMPLMODE"),
	TEXT("LINEERR_INVALCALLHANDLE"),
	TEXT("LINEERR_INVALCALLPARAMS"),
	TEXT("LINEERR_INVALCALLPRIVILEGE"),
	TEXT("LINEERR_INVALCALLSELECT"),
	TEXT("LINEERR_INVALCALLSTATE"),
	TEXT("LINEERR_INVALCALLSTATELIST"),
	TEXT("LINEERR_INVALCARD"),
	TEXT("LINEERR_INVALCOMPLETIONID"),
	TEXT("LINEERR_INVALCONFCALLHANDLE"),
	TEXT("LINEERR_INVALCONSULTCALLHANDLE"),
	TEXT("LINEERR_INVALCOUNTRYCODE"),
	TEXT("LINEERR_INVALDEVICECLASS"),
	TEXT("LINEERR_INVALDEVICEHANDLE"),
	TEXT("LINEERR_INVALDIALPARAMS"),
	TEXT("LINEERR_INVALDIGITLIST"),
	TEXT("LINEERR_INVALDIGITMODE"),
	TEXT("LINEERR_INVALDIGITS"),
	TEXT("LINEERR_INVALEXTVERSION"),
	TEXT("LINEERR_INVALGROUPID"),
	TEXT("LINEERR_INVALLINEHANDLE"),
	TEXT("LINEERR_INVALLINESTATE"),
	TEXT("LINEERR_INVALLOCATION"),
	TEXT("LINEERR_INVALMEDIALIST"),
	TEXT("LINEERR_INVALMEDIAMODE"),
	TEXT("LINEERR_INVALMESSAGEID"),
	TEXT("LINEERR Unused constant, ERROR!!"),
	TEXT("LINEERR_INVALPARAM"),
	TEXT("LINEERR_INVALPARKID"),
	TEXT("LINEERR_INVALPARKMODE"),
	TEXT("LINEERR_INVALPOINTER"),
	TEXT("LINEERR_INVALPRIVSELECT"),
	TEXT("LINEERR_INVALRATE"),
	TEXT("LINEERR_INVALREQUESTMODE"),
	TEXT("LINEERR_INVALTERMINALID"),
	TEXT("LINEERR_INVALTERMINALMODE"),
	TEXT("LINEERR_INVALTIMEOUT"),
	TEXT("LINEERR_INVALTONE"),
	TEXT("LINEERR_INVALTONELIST"),
	TEXT("LINEERR_INVALTONEMODE"),
	TEXT("LINEERR_INVALTRANSFERMODE"),
	TEXT("LINEERR_LINEMAPPERFAILED"),
	TEXT("LINEERR_NOCONFERENCE"),
	TEXT("LINEERR_NODEVICE"),
	TEXT("LINEERR_NODRIVER"),
	TEXT("LINEERR_NOMEM"),
	TEXT("LINEERR_NOREQUEST"),
	TEXT("LINEERR_NOTOWNER"),
	TEXT("LINEERR_NOTREGISTERED"),
	TEXT("LINEERR_OPERATIONFAILED"),
	TEXT("LINEERR_OPERATIONUNAVAIL"),
	TEXT("LINEERR_RATEUNAVAIL"),
	TEXT("LINEERR_RESOURCEUNAVAIL"),
	TEXT("LINEERR_REQUESTOVERRUN"),
	TEXT("LINEERR_STRUCTURETOOSMALL"),
	TEXT("LINEERR_TARGETNOTFOUND"),
	TEXT("LINEERR_TARGETSELF"),
	TEXT("LINEERR_UNINITIALIZED"),
	TEXT("LINEERR_USERUSERINFOTOOBIG"),
	TEXT("LINEERR_REINIT"),
	TEXT("LINEERR_ADDRESSBLOCKED"),
	TEXT("LINEERR_BILLINGREJECTED"),
	TEXT("LINEERR_INVALFEATURE"),
	TEXT("LINEERR_NOMULTIPLEINSTANCE")
};


LPTSTR psz_dwMsg[] = {
	TEXT("LINE_ADDRESSSTATE"),
	TEXT("LINE_CALLINFO"),
	TEXT("LINE_CALLSTATE"),
	TEXT("LINE_CLOSE"),
	TEXT("LINE_DEVSPECIFIC"),
	TEXT("LINE_DEVSPECIFICFEATURE"),
	TEXT("LINE_GATHERDIGITS"),
	TEXT("LINE_GENERATE"),
	TEXT("LINE_LINEDEVSTATE"),
	TEXT("LINE_MONITORDIGITS"),
	TEXT("LINE_MONITORMEDIA"),
	TEXT("LINE_MONITORTONE"),
	TEXT("LINE_REPLY"),
	TEXT("LINE_REQUEST"),
	TEXT("PHONE_BUTTON"),
	TEXT("PHONE_CLOSE"),
	TEXT("PHONE_DEVSPECIFIC"),
	TEXT("PHONE_REPLY"),
	TEXT("PHONE_STATE"),
	TEXT("LINE_CREATE"),
	TEXT("PHONE_CREATE"),
	TEXT("LINE_AGENTSPECIFIC"),
	TEXT("LINE_AGENTSTATUS"),
	TEXT("LINE_APPNEWCALL"),
	TEXT("LINE_PROXYREQUEST"),
	TEXT("LINE_REMOVE"),
	TEXT("PHONE_REMOVE")
};


LPTSTR pszfLINEADDRESSSTATE[] = 
{
	TEXT("Unknown LINEADDRESSSTATE information"),
	TEXT("LINEADDRESSSTATE_OTHER"),
	TEXT("LINEADDRESSSTATE_DEVSPECIFIC"),
	TEXT("LINEADDRESSSTATE_INUSEZERO"),
	TEXT("LINEADDRESSSTATE_INUSEONE"),
	TEXT("LINEADDRESSSTATE_INUSEMANY"),
	TEXT("LINEADDRESSSTATE_NUMCALLS"),
	TEXT("LINEADDRESSSTATE_FORWARD"),
	TEXT("LINEADDRESSSTATE_TERMINALS"),
	TEXT("LINEADDRESSSTATE_CAPSCHANGE")
};


LPTSTR pszfLINECALLINFOSTATE[] = 
{
	TEXT("Unknown LINECALLINFOSTATE state"),
	TEXT("LINECALLINFOSTATE_OTHER"),
	TEXT("LINECALLINFOSTATE_DEVSPECIFIC"),
	TEXT("LINECALLINFOSTATE_BEARERMODE"),
	TEXT("LINECALLINFOSTATE_RATE"),
	TEXT("LINECALLINFOSTATE_MEDIAMODE"),
	TEXT("LINECALLINFOSTATE_APPSPECIFIC"),
	TEXT("LINECALLINFOSTATE_CALLID"),
	TEXT("LINECALLINFOSTATE_RELATEDCALLID"),
	TEXT("LINECALLINFOSTATE_ORIGIN"),
	TEXT("LINECALLINFOSTATE_REASON"),
	TEXT("LINECALLINFOSTATE_COMPLETIONID"),
	TEXT("LINECALLINFOSTATE_NUMOWNERINCR"),
	TEXT("LINECALLINFOSTATE_NUMOWNERDECR"),
	TEXT("LINECALLINFOSTATE_NUMMONITORS"),
	TEXT("LINECALLINFOSTATE_TRUNK"),
	TEXT("LINECALLINFOSTATE_CALLERID"),
	TEXT("LINECALLINFOSTATE_CALLEDID"),
	TEXT("LINECALLINFOSTATE_CONNECTEDID"),
	TEXT("LINECALLINFOSTATE_REDIRECTIONID"),
	TEXT("LINECALLINFOSTATE_REDIRECTINGID"),
	TEXT("LINECALLINFOSTATE_DISPLAY"),
	TEXT("LINECALLINFOSTATE_USERUSERINFO"),
	TEXT("LINECALLINFOSTATE_HIGHLEVELCOMP"),
	TEXT("LINECALLINFOSTATE_LOWLEVELCOMP"),
	TEXT("LINECALLINFOSTATE_CHARGINGINFO"),
	TEXT("LINECALLINFOSTATE_TERMINAL"),
	TEXT("LINECALLINFOSTATE_DIALPARAMS"),
	TEXT("LINECALLINFOSTATE_MONITORMODES")
};


LPTSTR pszfLINECALLSTATE[] = 
{
	TEXT("Unknown LINECALLSTATE state"),
	TEXT("LINECALLSTATE_IDLE"),
	TEXT("LINECALLSTATE_OFFERING"),
	TEXT("LINECALLSTATE_ACCEPTED"),
	TEXT("LINECALLSTATE_DIALTONE"),
	TEXT("LINECALLSTATE_DIALING"),
	TEXT("LINECALLSTATE_RINGBACK"),
	TEXT("LINECALLSTATE_BUSY"),
	TEXT("LINECALLSTATE_SPECIALINFO"),
	TEXT("LINECALLSTATE_CONNECTED"),
	TEXT("LINECALLSTATE_PROCEEDING"),
	TEXT("LINECALLSTATE_ONHOLD"),
	TEXT("LINECALLSTATE_CONFERENCED"),
	TEXT("LINECALLSTATE_ONHOLDPENDCONF"),
	TEXT("LINECALLSTATE_ONHOLDPENDTRANSFER"),
	TEXT("LINECALLSTATE_DISCONNECTED"),
	TEXT("LINECALLSTATE_UNKNOWN")
};


LPTSTR pszfLINEDIALTONEMODE[] =
{
	TEXT("Unknown LINEDIALTONE information"),
	TEXT("LINEDIALTONEMODE_NORMAL"),
	TEXT("LINEDIALTONEMODE_SPECIAL"),
	TEXT("LINEDIALTONEMODE_INTERNAL"),
	TEXT("LINEDIALTONEMODE_EXTERNAL"),
	TEXT("LINEDIALTONEMODE_UNKNOWN"),
	TEXT("LINEDIALTONEMODE_UNAVAIL")
};


LPTSTR pszfLINEBUSYMODE[] =
{
	TEXT("Unknown LINEBUSYMODE information"),
	TEXT("LINEBUSYMODE_STATION"),
	TEXT("LINEBUSYMODE_TRUNK"),
	TEXT("LINEBUSYMODE_UNKNOWN"),
	TEXT("LINEBUSYMODE_UNAVAIL")
};


LPTSTR pszfLINECALLPRIVILEGE[] =
{
	TEXT("No change to LINECALLPRIVILEGE"),
	TEXT("LINECALLPRIVILEGE_NONE"),
	TEXT("LINECALLPRIVILEGE_MONITOR"),
	TEXT("LINECALLPRIVILEGE_OWNER")
};


LPTSTR pszfLINEDEVSTATE[] =
{    
	TEXT("Unknown LINEDEVESTATE state"),
	TEXT("LINEDEVSTATE_OTHER"),
	TEXT("LINEDEVSTATE_RINGING"),
	TEXT("LINEDEVSTATE_CONNECTED"),
	TEXT("LINEDEVSTATE_DISCONNECTED"),
	TEXT("LINEDEVSTATE_MSGWAITON"),
	TEXT("LINEDEVSTATE_MSGWAITOFF"),
	TEXT("LINEDEVSTATE_INSERVICE"),
	TEXT("LINEDEVSTATE_OUTOFSERVICE"),
	TEXT("LINEDEVSTATE_MAINTENANCE"),
	TEXT("LINEDEVSTATE_OPEN"),
	TEXT("LINEDEVSTATE_CLOSE"),
	TEXT("LINEDEVSTATE_NUMCALLS"),
	TEXT("LINEDEVSTATE_NUMCOMPLETIONS"),
	TEXT("LINEDEVSTATE_TERMINALS"),
	TEXT("LINEDEVSTATE_ROAMMODE"),
	TEXT("LINEDEVSTATE_BATTERY"),
	TEXT("LINEDEVSTATE_SIGNAL"),
	TEXT("LINEDEVSTATE_DEVSPECIFIC"),
	TEXT("LINEDEVSTATE_REINIT"),
	TEXT("LINEDEVSTATE_LOCK"),
	TEXT("LINEDEVSTATE_CAPSCHANGE"),
	TEXT("LINEDEVSTATE_CONFIGCHANGE"),
	TEXT("LINEDEVSTATE_TRANSLATECHANGE"),
	TEXT("LINEDEVSTATE_COMPLCANCEL"),
	TEXT("LINEDEVSTATE_REMOVED")
};
    

LPTSTR pszfLINEDISCONNECTED[] =
{
	TEXT("Unknown LINEDISCONNECTED information"),
	TEXT("LINEDISCONNECTMODE_NORMAL"),
	TEXT("LINEDISCONNECTMODE_UNKNOWN"),
	TEXT("LINEDISCONNECTMODE_REJECT"),
	TEXT("LINEDISCONNECTMODE_PICKUP"),
	TEXT("LINEDISCONNECTMODE_FORWARDED"),
	TEXT("LINEDISCONNECTMODE_BUSY"),
	TEXT("LINEDISCONNECTMODE_NOANSWER"),
	TEXT("LINEDISCONNECTMODE_BADADDRESS"),
	TEXT("LINEDISCONNECTMODE_UNREACHABLE"),
	TEXT("LINEDISCONNECTMODE_CONGESTION"),
	TEXT("LINEDISCONNECTMODE_INCOMPATIBLE"),
	TEXT("LINEDISCONNECTMODE_UNAVAIL"),
	TEXT("LINEDISCONNECTMODE_NODIALTONE")
};


LPTSTR pszfLINEMEDIAMODE[] =
{
	TEXT("Unknown LINEMEDIAMODE mode"),
	TEXT("Unused LINEMEDIAMODE mode, ERROR!!"),
	TEXT("LINEMEDIAMODE_UNKNOWN"),
	TEXT("LINEMEDIAMODE_INTERACTIVEVOICE"),
	TEXT("LINEMEDIAMODE_AUTOMATEDVOICE"),
	TEXT("LINEMEDIAMODE_DATAMODEM"),
	TEXT("LINEMEDIAMODE_G3FAX"),
	TEXT("LINEMEDIAMODE_TDD"),
	TEXT("LINEMEDIAMODE_G4FAX"),
	TEXT("LINEMEDIAMODE_DIGITALDATA"),
	TEXT("LINEMEDIAMODE_TELETEX"),
	TEXT("LINEMEDIAMODE_VIDEOTEX"),
	TEXT("LINEMEDIAMODE_TELEX"),
	TEXT("LINEMEDIAMODE_MIXED"),
	TEXT("LINEMEDIAMODE_ADSI"),
	TEXT("LINEMEDIAMODE_VOICEVIEW")
};


#if (TAPI_CURRENT_VERSION >= 0x00020000)

LPTSTR pszfLINEDIGITMODE[] =
{
	TEXT("Unknown LINEDIGITMODE mode"),
	TEXT("LINEDIGITMODE_PULSE"),
	TEXT("LINEDIGITMODE_DTMF"),
	TEXT("LINEDIGITMODE_DTMFEND")
};



LPTSTR pszfLINEGATHERTERM[] =
{
    TEXT("Unknown LINEGATHERTERM message"),
    TEXT("LINEGATHERTERM_BUFFERFULL"),
    TEXT("LINEGATHERTERM_TERMDIGIT"),
    TEXT("LINEGATHERTERM_FIRSTTIMEOUT"),
    TEXT("LINEGATHERTERM_INTERTIMEOUT"),
    TEXT("LINEGATHERTERM_CANCEL")
};


LPTSTR pszfLINEGENERATETERM[] = 
{
    TEXT("Unknown LINEGENERATETERM message"),
    TEXT("LINEGENERATETERM_DONE"),
    TEXT("LINEGENERATETERM_CANCEL")
};


LPTSTR pszfLINEREQUESTMODE[] =
{
    TEXT("Unknown LINEREQUESTMODE message"),
    TEXT("LINEREQUESTMODE_MAKECALL"),
    TEXT("LINEREQUESTMODE_MEDIACALL"),
    TEXT("LINEREQUESTMODE_DROP")
};


LPTSTR pszfLINESPECIALINFO[] =
{
    TEXT("Unknown LINESPECIALINFO information"),
    TEXT("LINESPECIALINFO_NOCIRCUIT"),
    TEXT("LINESPECIALINFO_CUSTIRREG"),
    TEXT("LINESPECIALINFO_REORDER"),
    TEXT("LINESPECIALINFO_UNKNOWN"),
    TEXT("LINESPECIALINFO_UNAVAIL")
};

#endif

//
//
//
LPTSTR FormatLineError(long lLineError, LPTSTR lpszOutputBuffer) {
   DWORD dwError;

   if (lpszOutputBuffer == NULL)
      return NULL;
   
   // Strip off the high bit to make the error code positive.
   dwError = (DWORD)(lLineError & 0x7FFFFFFF);

   if ((lLineError > 0) || (dwError > sizeof(pszLineError)))
      wsprintf(lpszOutputBuffer, TEXT("Unknown LINEERR_ code: 0x%lx"), lLineError);
   else
      _tcscpy(lpszOutputBuffer, pszLineError[dwError]);

   return lpszOutputBuffer;
}

//
//
//
LPTSTR FormatLineCallback(LPTSTR szOutputBuffer,
    DWORD dwDevice, DWORD dwMsg, DWORD dwCallbackInstance, 
    DWORD dwParam1, DWORD dwParam2, DWORD dwParam3) {
	long lBufferIndex = 0;

	if (szOutputBuffer == NULL)
	  return NULL;

	// Is this a known message?
	if (dwMsg >= sizeofArray(psz_dwMsg)) {
		wsprintf(szOutputBuffer, TEXT("Unknown dwMsg: '0x%.8X', ")
		  TEXT("dwDevice: '0x%.8X', dwCallbackInstance: '0x%.8X', ")
		  TEXT("dwParam1: '0x%.8X', dwParam2: '0x%.8X', dwParam3: '0x%.8X'"), dwMsg, 
			dwDevice, dwCallbackInstance, dwParam1, dwParam2, dwParam3);
		return szOutputBuffer;
	}

	// Let's start pretty printing.
	lBufferIndex += wsprintf(szOutputBuffer, TEXT("%s; "), psz_dwMsg[dwMsg]);

	// Which message was it?  And start decoding it!
	// How the message is decoded depends entirely on the message.
	// READ THE HELP FILES if you need more information on this.
	switch(dwMsg) {
		case LINE_ADDRESSSTATE:
		{
			lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
			  TEXT("Address ID: %lu, Address State: "), dwParam1);
			lBufferIndex += strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
			  dwParam2, pszfLINEADDRESSSTATE, sizeofArray(pszfLINEADDRESSSTATE));
			break;
		}

		case LINE_CALLINFO:
		{
			lBufferIndex += strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
			  dwParam1, pszfLINECALLINFOSTATE, sizeofArray(pszfLINECALLINFOSTATE));
			break;
		}

		case LINE_CALLSTATE:
		{
			if (dwParam3)
			{
			   lBufferIndex += strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
				 dwParam3, pszfLINECALLPRIVILEGE,
				 sizeofArray(pszfLINECALLPRIVILEGE));
			   lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
				 TEXT("; "));
			}
			lBufferIndex += strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
			  dwParam1, pszfLINECALLSTATE, sizeofArray(pszfLINECALLSTATE));

			switch(dwParam1) {
				case LINECALLSTATE_DIALTONE:
				{
					lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
					  TEXT(": "));
					lBufferIndex +=
					  strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
					  dwParam2, pszfLINEDIALTONEMODE,
					  sizeofArray(pszfLINEDIALTONEMODE));
					break;
				}

				case LINECALLSTATE_BUSY:
				{
					lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
							TEXT(": "));
					lBufferIndex += 
					  strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
					  dwParam2, pszfLINEBUSYMODE, sizeofArray(pszfLINEBUSYMODE));
					break;
				}
#if (TAPI_CURRENT_VERSION >= 0x00020000)
				case LINECALLSTATE_SPECIALINFO:
				{
					lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
					  TEXT(": "));
					lBufferIndex +=
					  strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
					  dwParam2, pszfLINESPECIALINFO,
					  sizeofArray(pszfLINESPECIALINFO));
					break;
				}
#endif
				case LINECALLSTATE_DISCONNECTED:
				{
					lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
					  TEXT(": "));
					lBufferIndex += 
					  strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
					  dwParam2, pszfLINEDISCONNECTED,
					  sizeofArray(pszfLINEDISCONNECTED));
					break;
				}

				case LINECALLSTATE_CONFERENCED:
				{
					lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
					  TEXT(": Parent conference call handle: 0x%lx"), dwParam2);
					break;
				}
			}
			break;
		}

		case LINE_CLOSE:
			break;

		case LINE_DEVSPECIFIC:
			break;

		case LINE_DEVSPECIFICFEATURE:
			break;

		case LINE_LINEDEVSTATE:
		{
			lBufferIndex += strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
			  dwParam1, pszfLINEDEVSTATE, sizeofArray(pszfLINEDEVSTATE));

			switch(dwParam1) {
				case LINEDEVSTATE_RINGING:
					lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
					  TEXT("; Ring Mode: 0x%.8X, Ring Count: %lu"), dwParam2,
					  dwParam3);
					break;

				case LINEDEVSTATE_REINIT:
				{
					switch(dwParam2) {
						case LINE_CREATE:
							lBufferIndex +=
							  wsprintf(&(szOutputBuffer[lBufferIndex]),
							  TEXT("; ReInit reason: LINE_CREATE, ")
							  TEXT("New Line Device ID '0x%.8X'"), dwParam3);
							break;
                        
						case LINE_LINEDEVSTATE:
							lBufferIndex +=
							  wsprintf(&(szOutputBuffer[lBufferIndex]),
							  TEXT("; ReInit reason: LINE_LINEDEVSTATE, "));

							lBufferIndex +=
							  strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
							  dwParam3, pszfLINEDEVSTATE,
							  sizeofArray(pszfLINEDEVSTATE));
							break;
                    
						case 0:
							break;

						default:
							lBufferIndex +=
							  wsprintf(&(szOutputBuffer[lBufferIndex]),
							  TEXT("; ReInit reason: %s, dwParam3: 0x%.8X"),
							  psz_dwMsg[dwParam2], dwParam3);
							break;
					}
					break;
				}
				default:
					break;
			}
			break;
		}


		case LINE_MONITORMEDIA:
		{
			lBufferIndex += strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
			  dwParam1, pszfLINEMEDIAMODE, sizeofArray(pszfLINEMEDIAMODE));
			break;
		}

		case LINE_MONITORTONE:
		{
			lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
			  TEXT("AppSpecific tone '%lu'"), dwParam1);
			break;
		}

		case LINE_REPLY:
		{
			if (dwParam2 == 0) {
				lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
				  TEXT("Request ID: %lu; Successful reply!"), dwParam1);
			}
			else {
				TCHAR szTmpBuff[64];

				FormatLineError((long) dwParam2, szTmpBuff);

				lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
				  TEXT("Request ID: %lu; Unsuccessful reply; %s"), dwParam1,
				  szTmpBuff);
			}
			break;
		}

	   case LINE_CREATE:
		{
			lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
			  TEXT("New Line Device ID = %lu"), dwParam1);
			break;
		}

#if (TAPI_CURRENT_VERSION >= 0x00020000)
		case LINE_GATHERDIGITS:
		{
			lBufferIndex += strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
			  dwParam1, pszfLINEGATHERTERM, sizeofArray(pszfLINEGATHERTERM));
			break;
		}

		case LINE_GENERATE:
		{
			lBufferIndex += strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
			  dwParam1, pszfLINEGENERATETERM, sizeofArray(pszfLINEGENERATETERM));
			break;
		}

		case LINE_MONITORDIGITS:
		{
			lBufferIndex += strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
			  dwParam2, pszfLINEDIGITMODE, sizeofArray(pszfLINEDIGITMODE));

			lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
			  TEXT(", Received: '%c'"), LOBYTE(LOWORD(dwParam1)));
			break;
		}

		case LINE_REQUEST:
		{
			lBufferIndex += strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
			  dwParam1, pszfLINEREQUESTMODE, sizeofArray(pszfLINEREQUESTMODE));
			break;
		}

		 case PHONE_CREATE:
		{
			lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
			  TEXT("New Phone Device ID = %lu"), dwParam1);
			break;
        }

		case LINE_APPNEWCALL:
		{
			lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
			  TEXT("Address = 0x%.8X; Handle = 0x%.8X; "), dwParam1, dwParam2);
			lBufferIndex += strBinaryArrayAppend(&(szOutputBuffer[lBufferIndex]),
			  dwParam3, pszfLINECALLPRIVILEGE, sizeofArray(pszfLINECALLPRIVILEGE));
			break;
		}
#endif

		default:
			lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
			  TEXT("dwParam1: 0x%.8X , dwParam2: 0x%.8X , dwParam3: 0x%.8X"),
			  dwParam1, dwParam2, dwParam3);
			break;

	} // End switch(dwMsg)

	// return that pointer!
	return szOutputBuffer;
}

//
//  FUNCTION: strBinaryArrayAppend(LPTSTR, DWORD, LPTSTR *, DWORD)
//
//  PURPOSE: Takes a bitmapped DWORD, an array representing that
//    binary mapping, and pretty prints it to a buffer.
//
//  PARAMETERS:
//    szOutputBuffer      - Buffer to print to.
//    dwFlags             - Binary mapped flags to interpret.
//    szStringArray       - Array of strings.
//    dwSizeofStringArray - number of elements in szStringArray.

//
//  RETURN VALUE:
//    The number of characters printed into szOutputBuffer.
//
//  COMMENTS:
//
//    This function takes dwFlags and checks each bit.  If the
//    bit is set, the appropriate string (taken from szStringArray)
//    is printed to the szOutputBuffer string buffer.  If there were
//    more bits set in the string than elements in the array, and error
//    is also tagged on the end.
//
//    This function is intended to be used only within the TapiInfo module.
//

static long strBinaryArrayAppend(LPTSTR szOutputBuffer, DWORD dwFlags,
								 LPTSTR szStringArray[], DWORD dwSizeofStringArray) {
    DWORD dwIndex = 1, dwPower = 1;
    long lBufferIndex = 0;
    BOOL bFirst = TRUE;

    // The zeroth element in every bitmapped array is the "unknown" or
    // "unchanged" message.
    if (dwFlags == 0) {
        lBufferIndex = wsprintf(szOutputBuffer, TEXT("%s"), szStringArray[0]);
        return lBufferIndex;
    }

    // iterate through the flags and check each one.
    while (dwIndex < dwSizeofStringArray) {
        // If we find one, print it.
        if (dwFlags & dwPower)
            // Seporate each printing with a ", " except the first one.
            if (bFirst) {
                lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
                  TEXT("%s"), szStringArray[dwIndex]);
                bFirst = FALSE;
            }
            else
                lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
                  TEXT(", %s"), szStringArray[dwIndex]);

        dwIndex ++;
        dwFlags &= ~dwPower;  // clear it so we know we checked it.
        dwPower *= 2;
    }

    // If there are any flags left, they were not in the array.
    if (dwFlags) {
        if (bFirst)
            lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
              TEXT("Unknown flags '0x%lx'"), dwFlags);
        else
            lBufferIndex += wsprintf(&(szOutputBuffer[lBufferIndex]),
              TEXT(", Unknown flags '0x%lx'"), dwFlags);
    }

    // how many did we print into the buffer?
    return lBufferIndex;
}
