/*
--------------------------------------------------------------------
                                                                    
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
PARTICULAR PURPOSE.                                                 
Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
                                                                    
--------------------------------------------------------------------

*/
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:
    TapiClient.cpp

Abstract:
    "lineMakeCall" TAPI Test Program V1.21
	This program tests the behavior of the TAPI "lineMakeCall" and "lineDrop"
	asynchronous functions, as well as other TAPI APIs needed to implement outbound
	calls.

Notes:
	This version is a stand-alone application and outputs to the CE debugger by
	default; output may be redirected using command-line options.

Source files:
	"tapiinfo.c"
	"TapiLog.cpp"
	"BufDump.cpp"

-------------------------------------------------------------*/

#include <windows.h>
#include <winbase.h>												/* for "Sleep" */
#include <tchar.h>
#include <tapi.h>
#include <katoex.h>
#include <tux.h>
#include "tuxmain.h"
#include "tapiinfo.h"

//------------------------------------------------------------------------
//	Program functions - prototypes:
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

LPLINECALLPARAMS InitCallParameters(void);
DWORD WaitForCallback(DWORD);
void CALLBACK lineCallbackFunc(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);

extern CKato* g_pKato;

HINSTANCE hInst;									 /* handle to current instance */

BOOL bCallbackDone = FALSE;					   /* set to TRUE in callback function */
DWORD dwCallbackErr;								 /* error in callback function */
DWORD dwNumCalls = 1;				  /* default values of command-line parameters */
DWORD dwDeviceID = 0;										  /* default device ID */
DWORD dwBearerMode = LINEBEARERMODE_VOICE;
DWORD dwMediaMode = LINEMEDIAMODE_DATAMODEM;
DWORD dwBaudrate = 19200;

#ifdef UNDER_CE
	 /* CE Unimodem dials the call before sending the LINE_REPLY to "lineMakeCall" */
 DWORD dwDialingTimeout = 30000;			/* timeout before "lineMakeCall" reply */
 DWORD dwConnectionDelay = 10000;			 /* timeout after "lineMakeCall" reply */
#else
					 /* NT Unimodem sends LINE_REPLY first and then dials the call */
 DWORD dwDialingTimeout = 5000;				/* timeout before "lineMakeCall" reply */
 DWORD dwConnectionDelay = 30000;			 /* timeout after "lineMakeCall" reply */
#endif

//DWORD dwHangupTimeout = 5000;			  /* timeout before/after "lineDrop" reply */
DWORD dwHangupTimeout = 10000;			  /* timeout before/after "lineDrop" reply */
DWORD dwDeallocateDelay = 1000;			  /* timeout before/after "lineDrop" reply */

BOOL bEditDevConfig = FALSE;		  /* TRUE if device configuration to be edited */
BOOL bListAllDevices = FALSE;				   /* TRUE if devices to be enumerated */

TCHAR szPhoneNumber[120];							/* buffer for telephone number */

const TCHAR szDeviceClass[] = TEXT("tapi/line");
const DWORD dwCallbackData = 25;		 /* data returned within callback function */

DWORD dwCallsMade = 0;								  /* number of calls completed */
DWORD dwSuccessfulCalls = 0;						 /* number of successful calls */
TCHAR szBuff[1024];                               /* buffer for error messages */


//**********************************************************************************
DWORD RunTests(void);

TESTPROCAPI DefaultTest   (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	const DWORD DEVCAPSSIZE = 1024;            /* buffer size for "lineGetDevCaps" */
	long lRet;										 /* return value from function */
	LPLINEDEVCAPS lpDevCaps = NULL;    /* buffer pointer for LINEDEVCAPS structure */    
	DWORD dwErrors = 0;
	HLINEAPP hLineApp;								    /* line application handle */
	DWORD dwAPIVersion;       /* version of TAPI used; negotiated with application */
	DWORD dwNumDevs;
	LINEEXTENSIONID ExtensionID;                /* dummy structure - needed for NT */

    // Check our message value to see why we have been called
    if (uMsg == TPM_QUERY_THREAD_COUNT) 
	{
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = DEFAULT_THREAD_COUNT;
		return TPR_HANDLED;
    } 
	else if (uMsg != TPM_EXECUTE) 
	{
		return TPR_NOT_HANDLED;
    }

    lRet = lineInitialize(&hLineApp, hInst, lineCallbackFunc, NULL, &dwNumDevs);
	if (lRet)
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineInitialize\" failed: %hs"), FormatLineError(lRet, szBuff));
		return 1;
    }
			/* Negotiate API version: */
	g_pKato->Log(LOG_COMMENT, TEXT("Negotiating API version for device %lu:"), dwDeviceID);
	if (lRet = lineNegotiateAPIVersion(hLineApp, dwDeviceID, 0x00010000,
	  0x00020001, &dwAPIVersion, &ExtensionID))
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineNegotiateAPIVersion\" failed: %s"), FormatLineError(lRet, szBuff));
		++dwErrors;
		goto TapiCleanup;
	}

			/* Read device capabilities: */
	g_pKato->Log(LOG_DETAIL, TEXT("Allocating memory for \"DevCaps\" structure"));
	lpDevCaps = (LPLINEDEVCAPS)(LocalAlloc(LPTR, DEVCAPSSIZE));
	if (lpDevCaps == NULL)
	{
		g_pKato->Log(LOG_FAIL, TEXT("Unable to allocate memory for \"DevCaps\" structure"));
		++dwErrors;
	}
	else
	{
		g_pKato->Log(LOG_DETAIL, TEXT("\n\tReading device capabilities for device %lu:"), dwDeviceID);
		lpDevCaps->dwTotalSize = DEVCAPSSIZE;
		if (lRet = lineGetDevCaps(hLineApp, dwDeviceID, dwAPIVersion, 0, lpDevCaps))
		{
			g_pKato->Log(LOG_FAIL, TEXT("\"lineGetDevCaps\" failed: %s"), FormatLineError(lRet, szBuff));
			++dwErrors;
		}
		else
		{
			g_pKato->Log(LOG_COMMENT, TEXT("Modem name - %s"),(LPTSTR)lpDevCaps + (lpDevCaps->dwLineNameOffset/sizeof(TCHAR)));

			DWORD* Modes;
			Modes = (DWORD*)(lpFTE->dwUserData);
			dwBearerMode = Modes[0];
			dwMediaMode = Modes[1];
			g_pKato->Log(LOG_COMMENT, TEXT("BearerMode: %d"), dwBearerMode);
			g_pKato->Log(LOG_COMMENT, TEXT("MediaMode: %d"), dwMediaMode);
			dwErrors += RunTests();
		}
		LocalFree(lpDevCaps);
	}

TapiCleanup:
	if (lRet = lineShutdown(hLineApp))
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineShutdown\" failed: %s"), FormatLineError(lRet, szBuff));
		++dwErrors;
	}
	if (dwErrors)
		return TPR_FAIL;
	else
		return TPR_PASS;
}

TESTPROCAPI ListDevices   (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	const DWORD DEVCAPSSIZE = 1024;            /* buffer size for "lineGetDevCaps" */
	long lRet;										 /* return value from function */
	LPLINEDEVCAPS lpDevCaps = NULL;    /* buffer pointer for LINEDEVCAPS structure */    
	DWORD dwErrors = 0;
	HLINEAPP hLineApp;								    /* line application handle */
	DWORD dwAPIVersion;       /* version of TAPI used; negotiated with application */
	DWORD dwNumDevs;
	LINEEXTENSIONID ExtensionID;                /* dummy structure - needed for NT */
	TCHAR szBuff[1024];                               /* buffer for error messages */
	DWORD dwDeviceID;

    // Check our message value to see why we have been called
    if (uMsg == TPM_QUERY_THREAD_COUNT) 
	{
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = DEFAULT_THREAD_COUNT;
		return TPR_HANDLED;
    } 
	else if (uMsg != TPM_EXECUTE) 
	{
		return TPR_NOT_HANDLED;
    }

	/* Initialize TAPI: */
	g_pKato->Log(LOG_DETAIL, TEXT("Calling \"lineInitialize\":"));    
    lRet = lineInitialize(&hLineApp, hInst, lineCallbackFunc, NULL, &dwNumDevs);
	if (lRet)
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineInitialize\" failed: %hs"), FormatLineError(lRet, szBuff));
		return 1;
    }
    g_pKato->Log(LOG_COMMENT, TEXT("\"lineInitialize\" succeeded: %lu devices available"), dwNumDevs);

	/* List devices if enabled: */
	for (dwDeviceID = 0; dwDeviceID < dwNumDevs; ++dwDeviceID)
	{
		lpDevCaps = (LPLINEDEVCAPS)(LocalAlloc(LPTR, DEVCAPSSIZE));	
		lpDevCaps->dwTotalSize = DEVCAPSSIZE;
		if (lRet = lineNegotiateAPIVersion(hLineApp, dwDeviceID, 0x00010000,
		  0x00020001, &dwAPIVersion, &ExtensionID))
		{
			g_pKato->Log(LOG_FAIL, TEXT("\"lineNegotiateAPIVersion\" failed: %s"), FormatLineError(lRet, szBuff));
			++dwErrors;
			goto TapiCleanup;
		}
		if (lRet = lineGetDevCaps(hLineApp, dwDeviceID, dwAPIVersion, 0, lpDevCaps))
		{
			g_pKato->Log(LOG_FAIL, TEXT("\"lineGetDevCaps\" failed: %s"), FormatLineError(lRet, szBuff));
			++dwErrors;
			goto TapiCleanup;
		}
		g_pKato->Log(LOG_COMMENT, TEXT("TAPI device [%lu]: %s"), dwDeviceID, (LPTSTR)lpDevCaps + (lpDevCaps->dwLineNameOffset/sizeof(TCHAR)));
		LocalFree(lpDevCaps);
	}

TapiCleanup:		
		LocalFree(lpDevCaps);

	if (lRet = lineShutdown(hLineApp))
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineShutdown\" failed: %s"), FormatLineError(lRet, szBuff));
		++dwErrors;
	}
	if (dwErrors)
		return TPR_FAIL;
	else
		return TPR_PASS;
}
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:
    RunTests

Description:
    The current testing sequence is:
		 1) Initialize TAPI ("lineInitialize")
		 2) If selected from command line, list all device names and exit
		 3) If device listing not selected, negotiate an API version
			  ("lineNegotiateAPIVersion") on the selected line
		 4) List the device parameters ("lineGetDevCaps")
		 5) If device configuration/current location to be edited, call dialog boxes
			  ("lineGetDevConfig", "lineConfigDialogEdit", "lineSetDevConfig",
			  "lineTranslateDialog")
		 6) If phone number in canonical format, translate it
			  ("lineTranslateAddress")
		 7) Open the line ("lineOpen")
		 8) Enable status messages on the line ("lineSetStatusMessages")
		 9) Make a call ("lineMakeCall")
		10) If call successful, hold for a while to let it connect; then
			  drop it ("lineDrop")
		11) Close the line ("lineClose")
	    12) Repeat steps 7-11 for number of requested iterations
		13) Shut down TAPI ("lineShutdown")

Return value:
	Zero if all tests successful; the number of errors encountered if not.

-------------------------------------------------------------------*/

DWORD RunTests(void)
{
	const TCHAR szAppName[] = TEXT(_FILENAM);
#ifndef UNDER_CE
	const char szAsciiName[] = _FILENAM;       /* required for NT "lineInitialize" */
#endif
	const DWORD TRANSLATESIZE = 1024;     /* buffer size for "lineTranslateOutput" */
	const DWORD DEVCAPSSIZE = 1024;            /* buffer size for "lineGetDevCaps" */
	const DWORD DEVCONFIGSIZE = 1024; /* memory allocated for device configuration */

	long lRet;										 /* return value from function */
	HLINEAPP hLineApp;								    /* line application handle */
	HLINE hLine;													/* line handle */
	HCALL hCall;													/* call handle */
	DWORD dwAPIVersion;       /* version of TAPI used; negotiated with application */
	DWORD dwNumDevs;						   /* number of line devices available */
	DWORD dwRequestID;					/* return value from asynchronous function */
	DWORD dwCallTime;	   /* asynchronous function completion time (milliseconds) */
	LPTSTR lpszDialableAddr;      /* pointer to telephone number after translation */

	TCHAR szBuff[1024];                               /* buffer for error messages */
	LINEEXTENSIONID ExtensionID;                /* dummy structure - needed for NT */
	LPLINEDEVCAPS lpDevCaps = NULL;    /* buffer pointer for LINEDEVCAPS structure */    
	LPLINETRANSLATEOUTPUT lpTranslateOutput = NULL; /* ptr for address translation */
	LPLINECALLPARAMS lpCallParams = NULL;  /* pointer for call parameter structure */
	LPVARSTRING lpDeviceConfig;      /* pointer for device configuration structure */
	DWORD dwErrors = 0;

		/* Initialize TAPI: */
	g_pKato->Log(LOG_DETAIL, TEXT("Calling \"lineInitialize\":"));    
    lRet = lineInitialize(&hLineApp, hInst, lineCallbackFunc, NULL, &dwNumDevs);
	if (lRet)
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineInitialize\" failed: %hs"), FormatLineError(lRet, szBuff));
		return 1;
    }
    g_pKato->Log(LOG_COMMENT, TEXT("\"lineInitialize\" succeeded: %lu devices available"), dwNumDevs);

	if (dwDeviceID >= dwNumDevs)
	{
		g_pKato->Log(LOG_FAIL, TEXT("Device ID (%lu) is outside permitted range (0 - %lu) - aborting"), dwDeviceID, dwNumDevs - 1);
		++dwErrors;
		goto TapiCleanup;
	}

		/* Negotiate API version: */
	g_pKato->Log(LOG_COMMENT, TEXT("Negotiating API version for device %lu:"), dwDeviceID);
	if (lRet = lineNegotiateAPIVersion(hLineApp, dwDeviceID, 0x00010000,
	  0x00020001, &dwAPIVersion, &ExtensionID))
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineNegotiateAPIVersion\" failed: %s"), FormatLineError(lRet, szBuff));
		++dwErrors;
		goto TapiCleanup;
	}
	g_pKato->Log(LOG_COMMENT, TEXT("API version used: %.8X"), dwAPIVersion);    


	   /* If phone number in canonical format, translate: */
	if (szPhoneNumber[0] == '+')
	{
		g_pKato->Log(LOG_DETAIL, TEXT("Allocating memory for \"TranslateOutput\" structure"));
		lpTranslateOutput = (LPLINETRANSLATEOUTPUT)(LocalAlloc(LPTR, TRANSLATESIZE));
		if (lpTranslateOutput == NULL)
		{
			g_pKato->Log(LOG_FAIL, TEXT("Unable to allocate memory for \"TranslateOutput\" structure - aborting"));
			++dwErrors;
			goto TapiCleanup;
		}
		g_pKato->Log(LOG_COMMENT, TEXT("Translating telephone number:"));
		lpTranslateOutput->dwTotalSize = TRANSLATESIZE;
		if (lRet = lineTranslateAddress(hLineApp, dwDeviceID, dwAPIVersion,
		  szPhoneNumber, 0, 0, lpTranslateOutput))
		{
			g_pKato->Log(LOG_FAIL, TEXT("\"lineTranslateAddress\" failed: %s"), FormatLineError(lRet, szBuff));
			++dwErrors;
			goto TapiCleanup;
		}
		//ListTranslateOutput(lpTranslateOutput, QAVerbose);
		lpszDialableAddr = (LPTSTR)lpTranslateOutput +
		  lpTranslateOutput->dwDialableStringOffset/sizeof(TCHAR);
	}
	else if (szPhoneNumber[0] == '#') /* If address begins with "#", dial using NULL */
		lpszDialableAddr = NULL;
	else				/* If address not in canonical format, assume it is dialable */
		lpszDialableAddr = szPhoneNumber;

	g_pKato->Log(LOG_COMMENT, TEXT("Telephone number to be dialed = %s"), lpszDialableAddr);

		/* Edit device configuration and location if requested: */
#ifdef getwc											 /* Don't include in mincomm */
	if (bEditDevConfig)
	{
		g_pKato->Log(LOG_DETAIL, TEXT("Allocating memory for device configuration structure"));
		lpDeviceConfig = (LPVARSTRING)(LocalAlloc(LPTR, DEVCONFIGSIZE));
		if (lpDeviceConfig == NULL)
		{
			g_pKato->Log(LOG_FAIL, TEXT("Unable to allocate memory for device configuration input structure"));
			++dwErrors;
		}
		else
		{
			lpDeviceConfig->dwTotalSize = DEVCONFIGSIZE;

			g_pKato->Log(LOG_COMMENT, TEXT("\n\tRetrieving device configuration"));
			if (lRet = lineGetDevConfig(dwDeviceID, lpDeviceConfig, szDeviceClass))
			{
				g_pKato->Log(LOG_FAIL, TEXT("\"lineGetDevConfig\" failed: %s"), FormatLineError(lRet, szBuff));
				++dwErrors;
			}
			else
			{
				g_pKato->Log(LOG_COMMENT, TEXT("Device configuration structure before editing:"));
				//ListVarstring(lpDeviceConfig, QAVerbose);

				g_pKato->Log(LOG_COMMENT, TEXT("Opening device configuration dialog box"));
				if (lRet = lineConfigDialogEdit(dwDeviceID, NULL, szDeviceClass,
				  (LPVOID)((LPBYTE)lpDeviceConfig + lpDeviceConfig->dwStringOffset),
				  lpDeviceConfig->dwStringSize, lpDeviceConfig))
				{
					g_pKato->Log(LOG_FAIL, TEXT("\"lineConfigDialogEdit\" failed: %s"), FormatLineError(lRet, szBuff));
					++dwErrors;
				}
				else
				{
					g_pKato->Log(LOG_COMMENT, TEXT("\n\tDevice configuration structure after editing:"));
					//ListVarstring(lpDeviceConfig, QAVerbose);

					g_pKato->Log(LOG_COMMENT, TEXT("Saving device configuration"));
					if (lRet = lineSetDevConfig(dwDeviceID,
					  (LPVOID)((LPBYTE)lpDeviceConfig + lpDeviceConfig->dwStringOffset),
					  lpDeviceConfig->dwStringSize, szDeviceClass))
					{
						g_pKato->Log(LOG_FAIL, TEXT("\"lineSetDevConfig\" failed: %s"), FormatLineError(lRet, szBuff));
						++dwErrors;
					}
					else
						g_pKato->Log(LOG_COMMENT, TEXT("Device configuration successfully set"));
				}
			}
		}

		g_pKato->Log(LOG_COMMENT, TEXT("Opening dialog box to edit location:"));
		if (lRet = lineTranslateDialog(hLineApp, dwDeviceID, dwAPIVersion,
		  NULL, szPhoneNumber))
		{
			g_pKato->Log(LOG_FAIL, TEXT("\"lineTranslateDialog\" failed: %s"), FormatLineError(lRet, szBuff));
			++dwErrors;
		}
		else
			g_pKato->Log(LOG_COMMENT, TEXT("\"lineTranslateDialog\" successful"));
	}
#endif
		/* Initialize call parameter structure: */
	if ((lpCallParams = InitCallParameters()) == NULL)
	{
		g_pKato->Log(LOG_FAIL, TEXT("Unable to initialize call parameter structure"));
		++dwErrors;
		goto TapiCleanup;
	} 
	g_pKato->Log(LOG_COMMENT, TEXT("Call parameters set to defaults"));

		/* Make as many calls as are requested: */
	for (dwCallsMade = 1; dwCallsMade <= dwNumCalls; ++dwCallsMade)
	{
		g_pKato->Log(LOG_COMMENT, TEXT("\tMaking call %lu of %lu:"), dwCallsMade, dwNumCalls);
			/* Open selected line: */
 		g_pKato->Log(LOG_COMMENT, TEXT("Calling \"lineOpen\" - device %lu:"), dwDeviceID);
		hLine = NULL;
		if (lRet = lineOpen(hLineApp, dwDeviceID, &hLine, dwAPIVersion, 0, dwCallbackData,
		  LINECALLPRIVILEGE_NONE, dwMediaMode, NULL))
		{
			g_pKato->Log(LOG_FAIL, TEXT("\"lineOpen\" failed: %s"), FormatLineError(lRet, szBuff));
			++dwErrors;
			continue;
		} 
		g_pKato->Log(LOG_COMMENT, TEXT("Line %d successfully opened"), dwDeviceID);

			/* Enable all status messages: */
		g_pKato->Log(LOG_DETAIL, TEXT("Enabling status messages:"));

#if (!defined(TAPI_CURRENT_VERSION)) || (TAPI_CURRENT_VERSION < 0x00020000)
		if (lRet = lineSetStatusMessages(hLine, 0x000FFFFF, 0x000000FF))
#else
		if (lRet = lineSetStatusMessages(hLine, 0x01FFFFFF, 0x000000FF))
#endif
		{
			g_pKato->Log(LOG_FAIL, TEXT("\"lineSetStatusMessages\" failed: %s"), FormatLineError(lRet, szBuff));
			++dwErrors;
		}

		g_pKato->Log(LOG_COMMENT, TEXT("Placing call #%lu"), dwCallsMade);
		bCallbackDone = FALSE;
		if ((long)(dwRequestID = lineMakeCall(hLine, &hCall, lpszDialableAddr, 0,
		  lpCallParams)) < 0)
		{
			g_pKato->Log(LOG_FAIL, TEXT("lineMakeCall failed: %s"), FormatLineError(dwRequestID, szBuff));
			++dwErrors;
		}
		else
		{
			g_pKato->Log(LOG_COMMENT, TEXT("\"lineMakeCall\" successful - request ID = %ld"), dwRequestID);
											   /* Delay while waiting for callback */
			g_pKato->Log(LOG_COMMENT, TEXT("Waiting for line to reply..."));
			dwCallTime = WaitForCallback(dwDialingTimeout);

			if ((long)dwCallTime < 0)       /* If callback function returned error */
			{
				g_pKato->Log(LOG_FAIL, TEXT("Callback returned error: %s"), FormatLineError(dwCallTime, szBuff));
				++dwErrors;
			}
			else if (!bCallbackDone)			 /* If callback function timed out */
			{
				g_pKato->Log(LOG_FAIL, TEXT("Call not complete after %lu milliseconds - aborting"), dwCallTime);
				++dwErrors;
			}
			else
			{
				g_pKato->Log(LOG_COMMENT, TEXT("Call took %lu milliseconds to complete"), dwCallTime);
				++dwSuccessfulCalls;
				g_pKato->Log(LOG_COMMENT, TEXT("Waiting %lu milliseconds to allow call to connect"), dwConnectionDelay);
				bCallbackDone = FALSE;
				WaitForCallback(dwConnectionDelay);
											/* Monitor callback messages during delay */
					/* Drop the call: */
				g_pKato->Log(LOG_COMMENT, TEXT("Dropping call - call handle = 0x%.8x:"), hCall);
				bCallbackDone = FALSE;
				if ((long)(dwRequestID = lineDrop(hCall, NULL, 0)) < 0)
				{
					g_pKato->Log(LOG_FAIL, TEXT("\"lineDrop\" failed: %s"), FormatLineError(dwRequestID, szBuff));
					++dwErrors;
				}
				else if (dwRequestID == 0)
					g_pKato->Log(LOG_COMMENT, TEXT("\"lineDrop\" successful - completed synchronously"));
				else
				{
					g_pKato->Log(LOG_COMMENT, TEXT("\"lineDrop\" successful - request ID = %lu"), dwRequestID);
												   /* Delay while waiting for callback */
					g_pKato->Log(LOG_COMMENT, TEXT("Waiting for line to disconnect..."));
					dwCallTime = WaitForCallback(dwHangupTimeout);

					if (!bCallbackDone)
					{
						g_pKato->Log(LOG_FAIL, TEXT("Call not disconnected after %d ms - aborting"), dwCallTime);
						++dwErrors;
					}
					else
						g_pKato->Log(LOG_COMMENT, TEXT("Call took %d milliseconds to disconnect"), dwCallTime);
				}
				if (dwDeallocateDelay)
				{
					g_pKato->Log(LOG_DETAIL, TEXT("Waiting %lu second(s) for \"lineDrop\" to finish"), dwDeallocateDelay / 1000);
					bCallbackDone = FALSE;
					WaitForCallback(dwDeallocateDelay);
				}
				g_pKato->Log(LOG_COMMENT, TEXT("Deallocating call handle:"));
				if (lRet = lineDeallocateCall(hCall))
				{
				    g_pKato->Log(LOG_FAIL, TEXT("\"lineDeallocateCall\" failed: %s"), FormatLineError(lRet, szBuff));
					++dwErrors;
				}
			}
		}
				/* Close the line: */
		if (hLine)
		{
			if (lRet = lineClose(hLine))
			{
			    g_pKato->Log(LOG_FAIL, TEXT("\"lineClose\" failed: %s"), FormatLineError(lRet, szBuff));
				++dwErrors;
			}
		    else
				g_pKato->Log(LOG_COMMENT, TEXT("Line %d successfully closed"), dwDeviceID);
		}

		g_pKato->Log(LOG_COMMENT, TEXT("Calls made = %lu  Successful calls = %lu"), dwCallsMade, dwSuccessfulCalls);
	}		/* end for as many calls as are requested */
	--dwCallsMade;													/* clean up index */

TapiCleanup:
	if (lRet = lineShutdown(hLineApp))
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineShutdown\" failed: %s"), FormatLineError(lRet, szBuff));
		++dwErrors;
	}
	else
		g_pKato->Log(LOG_COMMENT, TEXT("\"lineShutdown\" completed"));

	if (lpTranslateOutput)
		LocalFree(lpTranslateOutput);
	if (lpCallParams)
		LocalFree(lpCallParams);

	return dwErrors;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:
    InitCallParameters

Description:
    This subroutine allocates memory for and initializes a LINECALLPARAMS
structure.  Default values are used for fields except "dwBearerMode", "dwMinRate",
"dwMaxRate", and "dwMediaMode", whose values are determined from the command line.
	For TAPI V2.0, sixteen additional structure members were added.  The device class
and configuration are retrieved and stored in the LINECALLPARAMS structure.

Return value:
	Pointer to the LINECALLPARAMS structure, or NULL if unsuccessful.

-------------------------------------------------------------------*/

LPLINECALLPARAMS InitCallParameters(void)
{
	const DWORD CALLPARAMSSIZE = 1024;
	LPLINECALLPARAMS lpCallParams;
#if (TAPI_CURRENT_VERSION >= 0x00020000)
	DWORD dwOffset = sizeof(LINECALLPARAMS);
	LPVARSTRING lpDeviceConfig;
//	TCHAR szBuff[256];
	long lRet;
#endif

	lpCallParams = (LPLINECALLPARAMS)(LocalAlloc(LPTR, CALLPARAMSSIZE));
	if (lpCallParams == NULL)
	{
		g_pKato->Log(LOG_COMMENT, TEXT("Unable to allocate memory for \"LineCallParams\" structure"));
		return NULL;
	}
	lpCallParams->dwTotalSize = CALLPARAMSSIZE;
	lpCallParams->dwBearerMode = dwBearerMode;						/* bearer mode */
	lpCallParams->dwMinRate = dwBaudrate;					   /* minimum baudrate */
	lpCallParams->dwMaxRate = dwBaudrate;					   /* maximum baudrate */
	lpCallParams->dwMediaMode = dwMediaMode;						 /* media mode */
	lpCallParams->dwCallParamFlags = 0;
	lpCallParams->dwAddressMode = LINEADDRESSMODE_ADDRESSID;	   /* address mode */
	lpCallParams->dwAddressID = 0;									 /* address ID */
	lpCallParams->DialParams.dwDialPause = 0;		/* dialing pause - use default */
	lpCallParams->DialParams.dwDialSpeed = 0; /* time between digits - use default */
	lpCallParams->DialParams.dwDigitDuration = 0;  /* digit duration - use default */
	lpCallParams->DialParams.dwWaitForDialtone = 0; /* dialtone wait - use default */
	lpCallParams->dwOrigAddressSize = 0;
	lpCallParams->dwOrigAddressOffset = 0;
	lpCallParams->dwDisplayableAddressSize = 0;
	lpCallParams->dwDisplayableAddressOffset = 0;
	lpCallParams->dwCalledPartySize = 0;
	lpCallParams->dwCalledPartyOffset = 0;
	lpCallParams->dwCommentSize = 0;
	lpCallParams->dwCommentOffset = 0;
	lpCallParams->dwUserUserInfoSize = 0;
	lpCallParams->dwUserUserInfoOffset = 0;
	lpCallParams->dwHighLevelCompSize = 0;
	lpCallParams->dwHighLevelCompOffset = 0;
	lpCallParams->dwLowLevelCompSize = 0;
	lpCallParams->dwLowLevelCompOffset = 0;
	lpCallParams->dwDevSpecificSize = 0;
	lpCallParams->dwDevSpecificOffset = 0;

#if (TAPI_CURRENT_VERSION >= 0x00020000)
	lpCallParams->dwDeviceClassSize = (_tcslen(szDeviceClass) + 1) * sizeof(TCHAR);
	lpCallParams->dwDeviceClassOffset = dwOffset;
	_tcscpy((LPTSTR)((LPBYTE)lpCallParams + dwOffset), szDeviceClass);

	dwOffset += lpCallParams->dwDeviceClassSize;
	lpDeviceConfig = (LPVARSTRING)((LPBYTE)lpCallParams + dwOffset);
	lpDeviceConfig->dwTotalSize = CALLPARAMSSIZE - dwOffset;
	if (lRet = lineGetDevConfig(dwDeviceID, lpDeviceConfig, szDeviceClass))
	{
		g_pKato->Log(LOG_COMMENT, TEXT("\"lineGetDevConfig\" failed: %s"), FormatLineError(lRet, szBuff));
		g_pKato->Log(LOG_COMMENT, TEXT("Could not initialize call parameter structure"));
		LocalFree(lpCallParams);
		return NULL;
	}
	else
	{
		lpCallParams->dwDeviceConfigSize = lpDeviceConfig->dwUsedSize;
		lpCallParams->dwDeviceConfigOffset = dwOffset;
		g_pKato->Log(LOG_DETAIL, TEXT("Call parameters - device configuration: size = %lu offset = %lu"), lpCallParams->dwDeviceConfigSize, dwOffset);
	}
	lpCallParams->dwPredictiveAutoTransferStates = 0;
	lpCallParams->dwTargetAddressSize = 0;
	lpCallParams->dwTargetAddressOffset = 0;
	lpCallParams->dwSendingFlowspecSize = 0;
	lpCallParams->dwSendingFlowspecOffset = 0;
	lpCallParams->dwReceivingFlowspecSize = 0;
	lpCallParams->dwReceivingFlowspecOffset = 0;
	lpCallParams->dwCallDataSize = 0;
	lpCallParams->dwCallDataOffset = 0;
	lpCallParams->dwNoAnswerTimeout = 0;
	lpCallParams->dwCallingPartyIDSize = 0;
	lpCallParams->dwCallingPartyIDOffset = 0;
#endif
	return lpCallParams;
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:
    WaitForCallback

Description:
    After an asynchronous function has been called, this subroutine may be called
to delay for up to "dwMilliSec" milliseconds while waiting for the callback
function to complete.  In order for data to reach the callback function, a message
loop is set up and checked every fifty milliseconds.  The callback function is
responsible for setting the global variable "bCallbackDone" to TRUE when complete.
The return value is the number of milliseconds it took to complete (the same as
the input if callback never completes).

-------------------------------------------------------------------*/

DWORD WaitForCallback(DWORD dwMilliSec)
{
	MSG msg;												  /* message structure */
	DWORD dwTime;

	dwCallbackErr = 0;

	for (dwTime = 0; bCallbackDone == FALSE && dwTime < dwMilliSec; dwTime += 50)
	{
		Sleep(50);								   /* Sleep for fifty milliseconds */

		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				return FALSE;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	if (dwCallbackErr)
		return dwCallbackErr;
	else
		return dwTime;
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:
    lineCallbackFunc

Description:
    The TAPI callback function is enabled during "lineInitialize".  This
	version prints message data to the debugger; if this is a LINE_REPLY
	message, the "bCallbackDone" flag is set and the error (if any) is
	recorded.

-------------------------------------------------------------------*/


void CALLBACK lineCallbackFunc(DWORD dwDevice, DWORD dwMsg,
  DWORD dwCallbackInstance, DWORD dwParam1, DWORD dwParam2, DWORD dwParam3)
{
	TCHAR szBuff[1024];							 /* buffer for "FormatLineCallback */

	g_pKato->Log(LOG_DETAIL, TEXT("Callback received: message = %lu callback data = %lu"), dwMsg, dwCallbackInstance);
	g_pKato->Log(LOG_COMMENT, TEXT("TAPI callback function: %s"), FormatLineCallback(szBuff, dwDevice, dwMsg, dwCallbackInstance, dwParam1, dwParam2, dwParam3));

    if (dwMsg == LINE_REPLY)
	{
		bCallbackDone = TRUE;
		dwCallbackErr = dwParam2;
		if (dwCallbackInstance != dwCallbackData)
			g_pKato->Log(LOG_COMMENT, TEXT("Incorrect callback data (%lu - expected %lu)"), dwCallbackInstance, dwCallbackData);
	}
}


