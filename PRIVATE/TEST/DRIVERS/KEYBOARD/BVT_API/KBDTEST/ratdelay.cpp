/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     ratdelay.cpp  

Abstract:

  Rate and Delay keyboard tests.


Notes:
	
  Adapted from the multitst suite "keys2.cpp"
--*/

#include "global.h"
#include "ratdelay.h"

static TCHAR  szThisFile[] = TEXT("ratdelay.cpp");

#ifdef UNDER_CE

/* ------------------------------------------------------------------------
	Borrowed from the HPC control panel source code.
------------------------------------------------------------------------ */
HRESULT GetKeyboardSettings(int *pkb_dly)
{
HKEY	hKey;
DWORD	dwSize, dwType;
long	hRes;

	*pkb_dly = 0;
	hRes = RegOpenKeyExW(HKEY_CURRENT_USER, szKBRegistryKey, 0, KEY_ALL_ACCESS, &hKey);
	if (hRes != ERROR_SUCCESS) {
		RETAILMSG(1, (L"Can't open '%s' registry key.\r\n",szKBRegistryKey));
		return hRes;
		}

	dwSize = sizeof(DWORD);
	hRes = RegQueryValueExW(hKey, szKBDispDelay, 0, &dwType, (LPBYTE)pkb_dly, &dwSize);

	if (hRes != ERROR_SUCCESS)
{
DPF("Error reading registry\r\n");
}

	RegCloseKey(hKey);
	DPF1("Get from Registry DispDelay=%d  \r\n",*pkb_dly);
	return hRes;
}

/* ------------------------------------------------------------------------
	Borrowed from control panel source code. 
	NOTE:  After calling this function call
	        NotifyWinUserSystem(NWUS_KEYBD_REPEAT_CHANGED);
------------------------------------------------------------------------ */
HRESULT SaveKeyboardSettings(int kb_dly, int rpt_rate, int kb_disp_dly)
{
HKEY	hKey;
DWORD	Disp;
HRESULT hRes;

    g_pKato->Log( LOG_DETAIL, TEXT("kb_dly      == %d"), kb_dly );
    g_pKato->Log( LOG_DETAIL, TEXT("rpt_rate    == %d"), rpt_rate );
    g_pKato->Log( LOG_DETAIL, TEXT("kb_disp_dly == %d"), kb_disp_dly );

	hRes = RegCreateKeyExW(HKEY_CURRENT_USER, szKBRegistryKey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, &Disp);
	if (hRes != ERROR_SUCCESS) {
		RETAILMSG(1, (L"Error creating %s registry key.\r\n", szKBRegistryKey));
		return hRes;
		}

	hRes = RegSetValueExW(hKey, szKBDelay,0,REG_DWORD, (LPBYTE)&kb_dly, sizeof(DWORD));
	if (hRes != ERROR_SUCCESS) 
		goto exit;

	hRes = RegSetValueExW(hKey, szKBDispDelay,0,REG_DWORD, (LPBYTE)&kb_disp_dly, sizeof(DWORD));
	if (hRes != ERROR_SUCCESS) 
		goto exit;

	hRes = RegSetValueExW(hKey, szRepeateRate,0,REG_DWORD, (LPBYTE)&rpt_rate, sizeof(DWORD));

exit:
	if (hRes != ERROR_SUCCESS)
	{
		DPF("Error saving to registry\r\n");
	}
	RegCloseKey(hKey);					   
	
	DPF3("Save to Registry '%s' Delay=%d Repeat=%d \r\n", szKBRegistryKey, kb_dly, rpt_rate);
	return hRes;
}


int  GetRateDelayMinMax(int *piRateMin, int *piRateMax, int *piRateOrg,
                        int *piDelayMin, int *piDelayMax, int *piDelayOrg)
{
    struct KBDI_AUTOREPEAT_INFO Info;
    int                         iDelaySettings;
    int                         iRepeatSettings;
    INT32                       *pInitialDelaySettings;
    INT32                       *pRepeatRateSettings;
    BOOL                        bRtn;

    *piDelayMin = *piDelayMax = *piDelayOrg = 0;
    *piRateMin = *piRateMax = *piRateOrg = 0;
    
    // Set Rate and Delay to default
    bRtn = KeybdGetDeviceInfo(KBDI_AUTOREPEAT_INFO_ID, &Info);
    if( FALSE == bRtn  )
    {
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL in %s @ line %d: KeybdGetDeviceInfo(KBDI_AUTOREPEAT_INFO_ID, &Info)"),
                      szThisFile, __LINE__ );
        return 0;
        
    } // end if( FALSE == bRtn  )
        
    g_pKato->Log( LOG_DETAIL, TEXT("Info.CurrentInitialDelay	    = %d"),
                        Info.CurrentInitialDelay );
    g_pKato->Log( LOG_DETAIL, TEXT("Info.CurrentRepeatRate		    = %d"),
                        Info.CurrentRepeatRate );
    g_pKato->Log( LOG_DETAIL, TEXT("Info.cInitialDelaysSelectable  = %d"),
                        Info.cInitialDelaysSelectable );
    g_pKato->Log( LOG_DETAIL, TEXT("Info.cRepeatRatesSelectable	= %d"),
                        Info.cRepeatRatesSelectable );
    
    *piDelayOrg = Info.CurrentInitialDelay;
	*piRateOrg = Info.CurrentRepeatRate;

	// Allocate buffer for autorepeat settings
	if ( Info.cInitialDelaysSelectable == -1 )
		iDelaySettings = 2;
	else
		iDelaySettings = Info.cInitialDelaysSelectable;

	if ( Info.cRepeatRatesSelectable == -1 )
		iRepeatSettings = 2;
	else
		iRepeatSettings = Info.cRepeatRatesSelectable;

	pInitialDelaySettings = new INT32[(iDelaySettings + iRepeatSettings)];
	if( NULL == pInitialDelaySettings )
	    return 0;
	pRepeatRateSettings = pInitialDelaySettings + iDelaySettings;

	// Get the allowable settings.
	bRtn = KeybdGetDeviceInfo(KBDI_AUTOREPEAT_SELECTIONS_INFO_ID, 
	                          pInitialDelaySettings);
    if( FALSE == bRtn  )
    {
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL in %s @ line %d: KeybdGetDeviceInfo(KBDI_AUTOREPEAT_SELECTIONS_INFO_ID, &Info)"),
                      szThisFile, __LINE__ );
        return 0;
        
    } // end if( FALSE == bRtn  )
	                   

    if( Info.cInitialDelaysSelectable )
    {
        *piDelayMin = *pInitialDelaySettings;
        *piDelayMax = *(pInitialDelaySettings + iDelaySettings - 1);
        
    } // end if( Info.cInitialDelaysSelectable )

    if( Info.cRepeatRatesSelectable )
    {
	    *piRateMax = *pRepeatRateSettings;
	    *piRateMin = *(pRepeatRateSettings + iRepeatSettings - 1);
	    
	} // end if( Info.cRepeatrateSelectable )

	delete pInitialDelaySettings;

	return Info.cRepeatRatesSelectable;
	
} // int GetRateDelayMinMax(int *, int *, int *, int *)


/*++
 
VerifyRateDelayTest:
 
	This is the testing state machine for the rate delay tests
 
Arguments:
 
    	
 
Return Value:
 
	
 

 
	Unknown (unknown)
 
Notes:
 
--*/
#define MAX_RATEDELAYTEST 7
int VerifyRateDelayTest( int iTest )
{
    static int  iDelayMin, iDelayMax, iDelayOrg;
    static int  iRateMin, iRateMax, iRateOrg;
    int  iRtn;
    
    switch( iTest )
    {
    case 0:
        iRtn = GetRateDelayMinMax(&iRateMin, &iRateMax, &iRateOrg,
                           &iDelayMin, &iDelayMax, &iDelayOrg);
        if( 0 == iRateMin && 0 == iRateMax )
            return 4;

        g_pKato->Log( LOG_DETAIL, TEXT("iDelayMin == %d"), iDelayMin );
        g_pKato->Log( LOG_DETAIL, TEXT("iDelayMax == %d"), iDelayMax );
        g_pKato->Log( LOG_DETAIL, TEXT("iDelayOrg == %d"), iDelayOrg );
        g_pKato->Log( LOG_DETAIL, TEXT("iRateMin  == %d"), iRateMin );
        g_pKato->Log( LOG_DETAIL, TEXT("iRateMax  == %d"), iRateMax );
        g_pKato->Log( LOG_DETAIL, TEXT("iRateOrg  == %d"), iRateOrg );

        SaveKeyboardSettings( iDelayOrg, iRateMin, iDelayOrg );
        NotifyWinUserSystem( NWUS_KEYBD_REPEAT_CHANGED );
        
        return 1;

    case 1:

        if( askMessage( TEXT("Did you take notice of the repeat rate?"), 1) )
        {
            g_pKato->Log( LOG_DETAIL, TEXT("Noted average repeate rate.") );
            SaveKeyboardSettings( iDelayOrg, iRateMax, iDelayOrg );
            NotifyWinUserSystem( NWUS_KEYBD_REPEAT_CHANGED );
            return 2;
        }

        return 1;

    case 2:
        
        if( askMessage( TEXT("Was the repeat rate slower?"), 1) )
            g_pKato->Log( LOG_DETAIL, TEXT("Passed slow repeate rate") );
        else if( askMessage(TEXT("Do you want to try again?"),1) )
            return 2; 
		else  // failure
			g_pKato->Log( LOG_FAIL, TEXT("Failure: slow repeate rate") );

        SaveKeyboardSettings( iDelayOrg, iRateMin, iDelayOrg );
        NotifyWinUserSystem(NWUS_KEYBD_REPEAT_CHANGED);
            
        return 3;
        
    case 3:

        if( askMessage( TEXT("Was the repeat rate faster?"), 1 ) )
            g_pKato->Log( LOG_DETAIL, TEXT("Passed fast repeate rate") );
        else if( askMessage(TEXT("Do you want to try again?"),1) )
            return 3;
		else  // failure
			g_pKato->Log( LOG_FAIL, TEXT("Failure: faster repeate rate") );

        SaveKeyboardSettings( iDelayMin, iRateOrg, iDelayMin );
        NotifyWinUserSystem(NWUS_KEYBD_REPEAT_CHANGED);

        return 4;

    case 4:
    
        if( askMessage( TEXT("Did you take notice of the delay before repeat?"), 1 ) )
        {
            g_pKato->Log( LOG_DETAIL, TEXT("Noted average delay rate.") );
            SaveKeyboardSettings( iDelayMax, iRateOrg, iDelayMax );
            NotifyWinUserSystem(NWUS_KEYBD_REPEAT_CHANGED);
            return 5;
        }

        return 4;

    case 5:

        if( askMessage( TEXT("Was the delay longer?"), 1) )
            g_pKato->Log( LOG_DETAIL, TEXT("Passed long delay") );
        else if( askMessage(TEXT("Do you want to try again?"),1) )
            return 5;
		else  // failure
			g_pKato->Log( LOG_FAIL, TEXT("Failure: long delay") );

        SaveKeyboardSettings( iDelayMin, iRateOrg, iDelayMin );
        NotifyWinUserSystem(NWUS_KEYBD_REPEAT_CHANGED);
        return 6;

    case 6:

        if( askMessage( TEXT("Was the delay shorter?"), 1) )
            g_pKato->Log( LOG_DETAIL, TEXT("Passed short delay") );
        else if( askMessage(TEXT("Do you want to try again?"),1) )
            return 6;
		else  // failure
			g_pKato->Log( LOG_FAIL, TEXT("Failure: short delay") );

        SaveKeyboardSettings( iDelayOrg, iRateOrg, iDelayOrg );
        NotifyWinUserSystem(NWUS_KEYBD_REPEAT_CHANGED);
        
        return 7;
    
    } // end switch;
    
    return MAX_RATEDELAYTEST;
    
} // end int VerifyRateDelayTest( int iTest )

/*++
 
RepeatRateKeyDelayWndProc:
 
	This is the windows procedure for the Repeat Rate Key Delay test.
 
Arguments:
 
    Standard windows procedure arguments.	
 
Return Value:
 
	Standard windows procedure returns.
 

 
	Unknown (unknown)
 
Notes:
 
--*/
LRESULT CALLBACK RepeatRateKeyDelayWndProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam) 
{
    static int  goal = 0;
    static HWND hEdit = NULL;
    static int  iTest = 0;
    PAINTSTRUCT ps;
    HDC         hdc;
    TEXTMETRIC  tm;
    INT         cyChar;
    INT         cxChar;
    RECT        rectWorking;
 
    TCHAR       *aszInstruction[2] = 
                { TEXT("Press and hold a single key, take note of the repeat rate."),
                  TEXT("Press and hold a single key, take note of the delay before repeat."), };

    switch(message) 
    {
    case WM_PAINT:
        BeginPaint(hwnd, &ps);
        
        initWindow(ps.hdc,
                   TEXT("Driver Tests: Repeat Rate and Key Delay"),
                   aszInstruction[iTest / 4 ]);

        EndPaint(hwnd, &ps);
        return 0;

    case WM_INIT:
        goal     = (int)wParam;

        hdc = GetDC( globalHwnd );
        GetTextMetrics( hdc, &tm );
        cxChar = tm.tmAveCharWidth;
        cyChar = tm.tmHeight + tm.tmExternalLeading;
        ReleaseDC( globalHwnd, hdc );
        SystemParametersInfo( SPI_GETWORKAREA, 0, &rectWorking, FALSE );
        hEdit = CreateWindow(TEXT("edit"),
                             NULL,
                             WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
                             25, 97, rectWorking.right - 50, 5 * cyChar / 4,
                             globalHwnd, (HMENU)1,
                             globalInst, NULL );
        if( NULL == hEdit )
        {
            g_pKato->Log( LOG_FAIL, 
                          TEXT("FAIL in %s @ line %d: Couldn't create Edit Control: %d"), 
                          szThisFile, __LINE__, GetLastError() );
            return 1;                                    
        }

        if ( MAX_RATEDELAYTEST == (iTest = VerifyRateDelayTest( 0 )) )
            goal = 1;

        SendMessage( hEdit, EM_SETLIMITTEXT, 12, 0 );
        SetFocus( hEdit );
        
        return 0;

    case WM_COMMAND:
        // Test is over.
        if( 1 == LOWORD(wParam)  && EN_MAXTEXT == HIWORD(wParam) )
        {
            if ( MAX_RATEDELAYTEST == (iTest = VerifyRateDelayTest( iTest )) )
                goal = 1;

            doUpdate(NULL);
            SendMessage( hEdit, WM_SETTEXT, 0, (LPARAM)(LPSTR)"");
            SetFocus( hEdit );
            
        } // end if( 1 == LOWORD(wParam)  && EN_MAXTEXT == HIWORD(wParam) )
        
        return 0;

    case WM_GOAL:
        if( goal )
        {
            if( NULL != hEdit )
            {
                DestroyWindow( hEdit );
                hEdit = NULL;

            } // end if( NULL != hEdit )

            return 1;
        }

        return 0;
        
   } // end switch(message)
   
   return (DefWindowProc(hwnd, message, wParam, lParam));
   
} // end LRESULT CALLBACK EditTextWndProc(HWND, UINT,WPARAM, LPARAM)



/*++
 
RepeatRateKeyDelay_T:
 
	This test test the Repeat Rate and Key Delay of the
	keyboard.
 
Arguments:
 
	TUX standard arguments.
 
Return Value:
 
	TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
	TPR_EXECUTE: for TPM_EXECUTE
	TPR_NOT_HANDLED: for all other messages.
 

 
	Unknown (unknown)
 
Notes:
 
	
 
--*/
TESTPROCAPI RepeatRateKeyDelay_T( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	
	if( uMsg == TPM_QUERY_THREAD_COUNT )
	{
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
		return SPR_HANDLED;
	}
	else if (uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	} // end if else if

    /* -------------------------------------------------------------------
     	The real work.
    ------------------------------------------------------------------- */
    BOOL bDone = FALSE;

    g_pKato->Log(LOG_DETAIL, TEXT("In RepeatRateKeyDelay_T") );
    
    SendMessage( globalHwnd, WM_SWITCHPROC, KEYRATEDELAY, 0);

    SendMessage( globalHwnd, WM_INIT, 0, 0);
    doUpdate(NULL);

  	g_pKato->Log(LOG_DETAIL, TEXT("Pumping RepeatRateKeyDelay_T") );

    pump();
    
    g_pKato->Log(LOG_DETAIL, TEXT("Done With RepeatRateKeyDelay_T") );

    return getCode();

} // end RepeatRateKeyDelay_T( ... ) 

#else	//UNDER_CE

LRESULT CALLBACK RepeatRateKeyDelayWndProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam) 
{
	return (DefWindowProc(hwnd, message, wParam, lParam));
}


TESTPROCAPI RepeatRateKeyDelay_T( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	
	if( uMsg == TPM_QUERY_THREAD_COUNT )
	{
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
		return SPR_HANDLED;
	}
	else if (uMsg != TPM_EXECUTE)
	{
		return TPR_NOT_HANDLED;
	} // end if else if

	return TPR_SKIP;
}
#endif //UNDER_CE
