#include <windows.h>
#include <winbase.h>												/* for "Sleep" */
#include <tchar.h>
#include <tapi.h>
#include <katoex.h>
#include <tux.h>
#include "tapiinfo.h"
#include "tuxmain.h"
extern CKato* g_pKato;
void MonitorLines(void);
long DropCall(HCALL);

LineInfoStr LineInfo[MAX_OPEN_LINES];
DWORD dwMediaMode = LINEMEDIAMODE_DATAMODEM;
DWORD dwTestRunTime = RUN_FOREVER;					 /* test length (milliseconds) */
DWORD dwConnectTimeout = RUN_FOREVER;	/* time to hold call after connection made */

DWORD dwCallsReceived = 0;							   /* number of calls received */
DWORD dwCallsConnected = 0;							  /* number of calls connected */
DWORD dwTotalErrors = 0;					   /* total errors reported by program */
HINSTANCE hInst;									 /* handle to current instance */
DWORD nOpenLines = 0;
#define  DEFAULT_THREAD_COUNT 0

TESTPROCAPI DefaultTest         (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	if (uMsg == TPM_QUERY_THREAD_COUNT) 
	{
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = DEFAULT_THREAD_COUNT;
		return TPR_HANDLED;
	} 
	else if (uMsg != TPM_EXECUTE) 
	{
		return TPR_NOT_HANDLED;
	}

	MonitorLines();
	if (dwTotalErrors > 0)
		return TPR_FAIL;
	else return TPR_PASS;
}

void MonitorLines(void)
{
	long lRet;										 /* return value from function */
	HLINEAPP hLineApp;									/* line application handle */
	DWORD dwAPIVersion = TAPI_CURRENT_VERSION;   		/* negotiated TAPI version */
	DWORD dwNumDevs;						   /* number of line devices available */
	DWORD dwCurrentTime, dwFinishTime,dwStartTime;/* times read via GetTickCount() */
	DWORD i, j;
	BOOL bAcceptMoreCalls = TRUE;			   /* set to FALSE when timer runs out */
	DWORD nActiveCalls = 0;					   /* number of currently active calls */
	BOOL bOverflowOccured = FALSE; /* checks if overflow occured in GetTickCount() */

	TCHAR szBuff[512];								  /* buffer for error messages */
    LINEINITIALIZEEXPARAMS LineInitializeExParams;		/* for TAPI initialization */
	LINEEXTENSIONID ExtensionID;				  /* for "lineNegotiateAPIVersion" */
	LINEMESSAGE LineMessage;

		/* Timer initialization (Note: timers do not compensate */
		/*   for overflow in GetTickCount() every 49.7 days)    */
	dwStartTime = GetTickCount();
	dwCurrentTime = GetTickCount();
	g_pKato->Log(LOG_COMMENT, TEXT("Server started: tick count = %lu"), dwCurrentTime);
	if (dwTestRunTime != RUN_FOREVER)
	{
		dwFinishTime = dwCurrentTime + dwTestRunTime;

		if (dwFinishTime < dwCurrentTime)
			bOverflowOccured = TRUE;
	}
	else
		g_pKato->Log(LOG_COMMENT, TEXT("Server looping forever"));

		// Initialize TAPI:
	g_pKato->Log(LOG_COMMENT, TEXT("Calling \"lineInitializeEx\":"));    
	LineInitializeExParams.dwTotalSize = sizeof(LINEINITIALIZEEXPARAMS);
	LineInitializeExParams.dwOptions = LINEINITIALIZEEXOPTION_USEEVENT;
                              
	lRet = lineInitializeEx(&hLineApp, hInst, NULL, NULL, &dwNumDevs, &dwAPIVersion,
	  &LineInitializeExParams);
	if (lRet)
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineInitializeEx\" failed: %s"),
		  FormatLineError(lRet, szBuff));
		dwTotalErrors++;
		return;
    }
    g_pKato->Log(LOG_DETAIL, TEXT("\"lineInitializeEx\" succeeded: %lu devices available"),
	  dwNumDevs);

		// Negotiate API version (the version returned from "lineInitializeEx"
		//   is not necessarily valid for NT):
	g_pKato->Log(LOG_DETAIL, TEXT("Negotiating API version for device %lu:"),
	  LineInfo[0].dwDeviceID);
	if (lRet = lineNegotiateAPIVersion(hLineApp, LineInfo[0].dwDeviceID, 0x00010000,
	  TAPI_CURRENT_VERSION, &dwAPIVersion, &ExtensionID))
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineNegotiateAPIVersion\" failed: %s"),
		  FormatLineError(lRet, szBuff));
		++dwTotalErrors;
		lineShutdown(hLineApp);
		return;
	}
	g_pKato->Log(LOG_COMMENT, TEXT("API version used: %.8X"), dwAPIVersion);    

	for (i = 0; i < nOpenLines; ++i)
	{
		if (LineInfo[i].dwDeviceID >= dwNumDevs)
		{												 // skip out-of-range devices
			g_pKato->Log(LOG_COMMENT, TEXT("Device ID (%lu) is outside permitted range (0 - %lu) - skipping"),
			  LineInfo[i].dwDeviceID, dwNumDevs - 1);
			for (j = i + 1; j < nOpenLines; ++j)
				LineInfo[j - 1].dwDeviceID = LineInfo[j].dwDeviceID;
			nOpenLines--;
		}
	}
 		// TODO - This should be replaced with a call to "lineOpen" using LINEMAPPER 
		//   to select a line to be monitored.
	if (nOpenLines == 0)
	{
		g_pKato->Log(LOG_COMMENT, TEXT("No TAPI devices selected - using device 0 by default"));
		nOpenLines++;
		LineInfo[0].dwDeviceID = 0;
	}

	for (i = 0; i < nOpenLines; ++i)
	{
		g_pKato->Log(LOG_COMMENT, TEXT("Opening line %lu of %lu: TAPI device %lu"), i + 1,
		  nOpenLines, LineInfo[i].dwDeviceID);
			// Open line - pass in array index as callback data
		if (lRet = lineOpen(hLineApp, LineInfo[i].dwDeviceID, &(LineInfo[i].hLine),
		  dwAPIVersion, 0, i, LINECALLPRIVILEGE_OWNER, dwMediaMode, NULL))
		{
			g_pKato->Log(LOG_FAIL, TEXT("\"lineOpen\" failed: %s"), FormatLineError(lRet, szBuff));
			++dwTotalErrors;
			continue;
		} 
		g_pKato->Log(LOG_COMMENT, TEXT("Device %d successfully opened - handle = %lu"),
		  LineInfo[i].dwDeviceID, LineInfo[i].hLine);
	}
	
	while ((bAcceptMoreCalls) || (nActiveCalls > 0))
	{
		Sleep(50);						   // wait to let other applications catch up
		dwCurrentTime = GetTickCount();

		for (i = 0; i < nOpenLines; ++i)
		{												// Check timeout for each call
			if ((dwTestRunTime != RUN_FOREVER) &&
			  (dwCurrentTime > LineInfo[i].dwTime + dwTestRunTime) &&
			  (LineInfo[i].dwStatus == LINECALLSTATE_CONNECTED) &&
			  (((bOverflowOccured == TRUE) && (dwCurrentTime < dwStartTime)) || (bOverflowOccured == FALSE)) )
			{
				g_pKato->Log(LOG_COMMENT, TEXT("Call on line %lu expired - dropping"),
				  LineInfo[i].dwDeviceID);
				DropCall(LineInfo[i].hCall);
				LineInfo[i].dwStatus = LINECALLSTATE_DISCONNECTED;
			}
		}

		if ((dwTestRunTime != RUN_FOREVER) && (dwCurrentTime > dwFinishTime) &&
			(((bOverflowOccured == TRUE) && (dwCurrentTime < dwStartTime)) || (bOverflowOccured == FALSE)) )
		{														 // Check program timeout
			if (bAcceptMoreCalls)
			{
				g_pKato->Log(LOG_COMMENT, TEXT("Time to quit - dropping any active calls"));
				bAcceptMoreCalls = FALSE;
				for (i = 0; i < nOpenLines; ++i)
				{
					if ((LineInfo[i].dwStatus != LINECALLSTATE_DISCONNECTED) &&
					  (LineInfo[i].dwStatus != LINECALLSTATE_IDLE) &&
					  (LineInfo[i].dwStatus != 0))
					{
						g_pKato->Log(LOG_COMMENT, TEXT("Dropping call on line %lu"),
						  LineInfo[i].dwDeviceID);
						DropCall(LineInfo[i].hCall);
						LineInfo[i].dwStatus = LINECALLSTATE_DISCONNECTED;
					}
				}
			}

			if ((dwCurrentTime - dwFinishTime > 15000) && 
				(((bOverflowOccured == TRUE) && (dwCurrentTime < dwStartTime)) || (bOverflowOccured == FALSE)))
			{
				g_pKato->Log(LOG_COMMENT, TEXT("Time to quit application"));
				nActiveCalls = 0;		  // Force to shutdown if hung for 15 seconds
			}
		}

		if (lRet = lineGetMessage(hLineApp, &LineMessage, 0))
		{
			//  "lineGetMessage" will return LINEERR_OPERATIONFAILED
			//    if there is no message waiting in the queue
			if (lRet != LINEERR_OPERATIONFAILED)
			{
				g_pKato->Log(LOG_FAIL, TEXT("\"lineGetMessage\" failed: %s"),
				  FormatLineError(lRet, szBuff));
				++dwTotalErrors;
			}
			continue;
		}
		g_pKato->Log(LOG_COMMENT, TEXT("Message received: %s"),
		  FormatLineCallback(szBuff, LineMessage.hDevice, LineMessage.dwMessageID,
		  LineMessage.dwCallbackInstance, LineMessage.dwParam1, LineMessage.dwParam2,
		  LineMessage.dwParam3));

			// Sanity check: dwCallbackInstance should be index into "LineInfo"
		if (LineMessage.dwCallbackInstance >= nOpenLines)
		{
			g_pKato->Log(LOG_FAIL, TEXT("Callback ID %lu does not match an open line"),
			  LineMessage.dwCallbackInstance);
			if (LineMessage.dwMessageID == LINE_APPNEWCALL)
				DropCall((HCALL)LineMessage.dwParam2);
			dwTotalErrors++;
			continue;
		}

		i = LineMessage.dwCallbackInstance;
		switch (LineMessage.dwMessageID)
		{
			case LINE_APPNEWCALL:
				if (!bAcceptMoreCalls)
				{
					g_pKato->Log(LOG_COMMENT, TEXT("Quitting time - can't accept call"));
					DropCall((HCALL)LineMessage.dwParam2);
					break;
				}
				LineInfo[i].hCall = (HCALL)LineMessage.dwParam2;
				LineInfo[i].dwTime = dwCurrentTime;
				g_pKato->Log(LOG_COMMENT, TEXT("Call #%lu received at %lu - %lu active call(s)"),
				  ++dwCallsReceived, LineInfo[i].dwTime, ++nActiveCalls);
				break;

			case LINE_CALLSTATE:
				LineInfo[i].dwStatus = LineMessage.dwParam1;
				switch (LineMessage.dwParam1)
				{
					case LINECALLSTATE_OFFERING:
						g_pKato->Log(LOG_COMMENT, TEXT("Answering call:"));
						if ((lRet = lineAnswer(LineInfo[i].hCall, NULL, 0)) < 0)
						{
							g_pKato->Log(LOG_FAIL, TEXT("\"lineAnswer\" failed: %s"),
							  FormatLineError(lRet, szBuff));
							++dwTotalErrors;
						}
						else
							g_pKato->Log(LOG_COMMENT, TEXT("\"lineAnswer\" succeeded; request ID = %lu"),
							  lRet);
						break;

					case LINECALLSTATE_CONNECTED:
						g_pKato->Log(LOG_COMMENT, TEXT("Call connected - connection #%lu"),
						  ++dwCallsConnected);
						g_pKato->Log(LOG_COMMENT, TEXT("Connected after %lu milliseconds"),
						  dwCurrentTime - LineInfo[i].dwTime);
						break;

					case LINECALLSTATE_DISCONNECTED:
						g_pKato->Log(LOG_COMMENT, TEXT("Call disconnected on line %lu"),
						  LineInfo[i].dwDeviceID);

	// NT doesn't always send CALLSTATE_IDLE so we deallocate the call on DISCONNECTED
	// CE "lineDeallocateCall" fails unless we are in CALLSTATE_IDLE
#ifdef UNDER_CE
						break;

					case LINECALLSTATE_IDLE:
#endif
						g_pKato->Log(LOG_COMMENT, TEXT("Call on line %lu dropped - %lu active call(s)"),
						  LineInfo[i].dwDeviceID, --nActiveCalls);
						if (lRet = lineDeallocateCall(LineInfo[i].hCall))
						{
							g_pKato->Log(LOG_FAIL, TEXT("\"lineDeallocateCall\" failed: %s"),
							  FormatLineError(lRet, szBuff));
							++dwTotalErrors;
						}
						else
						{
							g_pKato->Log(LOG_DETAIL, TEXT("Call deallocated"));
							LineInfo[i].hCall = NULL;
						}
						break;
				}
				break;

			case LINE_REPLY:
				if (LineMessage.dwParam2)
				{
					g_pKato->Log(LOG_FAIL, TEXT("LINE_REPLY error: %s"),
					  FormatLineError(LineMessage.dwParam2, szBuff));
					++dwTotalErrors;
				}
				break;

		}	// end switch (LineMessage.dwMessageID)
	}	// end while ((bAcceptMoreCalls) ||(bCallsActive))

	for (i = 0; i < nOpenLines; ++i)
	{
		if (LineInfo[i].hLine)
		{
			if (lRet = lineClose(LineInfo[i].hLine))
			{
			    g_pKato->Log(LOG_FAIL, TEXT("\"lineClose\" failed for line %lu: %s"),
				  LineInfo[i].dwDeviceID, FormatLineError(lRet, szBuff));
				++dwTotalErrors;
			}
		    else
			{
				g_pKato->Log(LOG_COMMENT, TEXT("Line %lu successfully closed"),
				  LineInfo[i].dwDeviceID);
				LineInfo[i].hLine = NULL;
			}
		}
	}
	if (lRet = lineShutdown(hLineApp))
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineShutdown\" failed: %s"),
		  FormatLineError(lRet, szBuff));
		++dwTotalErrors;
	}
	else
		g_pKato->Log(LOG_DETAIL, TEXT("\"lineShutdown\" completed"));
}

long DropCall(HCALL hCall)
{
	long lRet;										 /* return value from function */
	TCHAR szBuff[64];								  /* buffer for error messages */

	g_pKato->Log(LOG_COMMENT, TEXT("Dropping call:"));
	if ((lRet = lineDrop(hCall, NULL, 0)) < 0)
	{
		g_pKato->Log(LOG_FAIL, TEXT("\"lineDrop\" failed: %s"),
		  FormatLineError(lRet, szBuff));
		++dwTotalErrors;
	}
	else
		g_pKato->Log(LOG_DETAIL, TEXT("\"lineDrop\" succeeded: request ID = %lu"), lRet);

	return lRet;
}
