/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     slctport.cpp  

Abstract:
Functions:
Notes:
--*/
/*++
 
Copyright (c) 1996  Microsoft Corporation
 
Module Name:
 
	slctport.cpp
 
Abstract:
 
	This file contians the code need to select the comm port 
    to test.
    

 
 
Notes:
 
--*/
#define __THIS_FILE__   TEXT("SlctPort.cpp")
#include "pserial.h"
#include "resource.h"

#ifdef UNDER_NT
/*++
 
FillCommPortsList:
 
	This function checks the resource file to find all avalible comm ports
	in the system.
 
Arguments:
 
	hList   Handle to the list control to fill
 
Return Value:
 
	INT number of item in the list.
 

 
 
Notes:
 
--*/
INT FillCommPortsList( HWND hList )
{
    HKEY        hkSerialComm;
    LONG        lRtn;
    DWORD       dwIdx;
    DWORD       nItems = 0;
    TCHAR       szValueName[32];
    DWORD       dwNameLen;
    DWORD       dwValueType;
    BYTE        abValue[128];
    DWORD       dwValueLen;
    HANDLE      hCommPort;

    /* --------------------------------------------------------------------
    	Open the key
    -------------------------------------------------------------------- */
    lRtn = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                         TEXT("HARDWARE\\DEVICEMAP\\SERIALCOMM"), 0,
                         KEY_READ,
                         &hkSerialComm );
    if( ERROR_SUCCESS != lRtn )
    {
        g_pKato->Log( LOG_FAIL, 
                      TEXT("FAIL in %s @ line %d: First RegOpenKeyEx returned %l"), 
                      __THIS_FILE__, __LINE__, lRtn);
        return 0;

    } // end if( ERROR_SUCCESS != lRtn )

    /* --------------------------------------------------------------------
    	Enumerate the keys is this directory.
    -------------------------------------------------------------------- */
    dwIdx = 0;
    while( TRUE )
    {
        dwNameLen = 32;
        dwValueLen = 128;
        lRtn = RegEnumValue( hkSerialComm,
                             dwIdx,
                             szValueName,
                             &dwNameLen, 
                             NULL, 
                             &dwValueType,
                             abValue,
                             &dwValueLen );
        if( ERROR_NO_MORE_ITEMS == lRtn /*|| ERROR_SUCCESS != lRtn*/) break;

        g_pKato->Log( LOG_DETAIL, 
                      TEXT("In %s @ line %d: Found Key %s, Value %s"), 
                       __THIS_FILE__, __LINE__, szValueName, abValue );

        dwIdx++;

        // Verify we have a good comm port name.
        if( 0 == dwNameLen || REG_SZ != dwValueType ) continue;

        /* ----------------------------------------------------------------
        	Check and see if port is avalible.
        ---------------------------------------------------------------- */
        hCommPort = CreateFile( (TCHAR *)abValue,
                                GENERIC_READ | GENERIC_WRITE,
                                0, NULL, OPEN_EXISTING, 0, NULL );
        if( INVALID_HANDLE_VALUE == hCommPort ) continue;
        CloseHandle( hCommPort );

        /* ----------------------------------------------------------------
        	Time to add comm port to list.
        ---------------------------------------------------------------- */
        nItems = SendMessage( hList, CB_ADDSTRING, 0, (LPARAM)(LPTSTR)abValue );
        
    } // end while( TRUE ) 

    if( ERROR_NO_MORE_ITEMS != lRtn )
    {
        if( ERROR_SUCCESS != lRtn )
        {   
            g_pKato->Log( LOG_FAIL, 
                          TEXT("FAIL in %s @ line %d:  RegEnumKeyEx returned %l"), 
                          __THIS_FILE__, __LINE__, lRtn);
            RegCloseKey( hkSerialComm );
            return 0;

        } // end if( ERROR_SUCCESS != lRtn )

    } // if( ERROR_NO_MORE_ITEMS != lRtn )
    
    return (nItems+1);

} // end BOOL FillCommPortsList( HWND hList )

#else

/*++
 
FillCommPortsList:
 
	This fuction places avalible ports COM1: through COM4: in the port
	passed in port list.
 
Arguments:
 
    hList the handle to the list.	
 
Return Value:
 
	INT the number of items in the list
	

 

 
Notes:
 
--*/
BOOL FillCommPortsList( HWND hList )
{
    INT             iIdx;
    HANDLE          hCommPort;
    DWORD           nItems = 0;
    const   UINT    nPorts  = 4;
    const   TCHAR   *aszPorts[nPorts] = { TEXT("COM1:"), TEXT("COM2:"), 
                                          TEXT("COM3:"), TEXT("COM4:") };

    for( iIdx = 0; iIdx < nPorts; iIdx++ )
    {
       /* ----------------------------------------------------------------
        	Check and see if port is avalible.
        ---------------------------------------------------------------- */
        g_pKato->Log( LOG_DETAIL, 
                      TEXT("In %s % line %d: Attempt to open \"%s\""),
                      __THIS_FILE__, __LINE__, aszPorts[iIdx] );
                      
        hCommPort = CreateFile( aszPorts[iIdx],
                                GENERIC_READ | GENERIC_WRITE,
                                0, NULL, OPEN_EXISTING, 0, NULL );
        if( INVALID_HANDLE_VALUE == hCommPort ) continue;
        
        g_pKato->Log( LOG_DETAIL, 
                      TEXT("In %s % line %d: Opended \"%s\""),
                      __THIS_FILE__, __LINE__, aszPorts[iIdx] );
        CloseHandle( hCommPort );

        /* ----------------------------------------------------------------
        	Time to add comm port to list.
        ---------------------------------------------------------------- */
        nItems = SendMessage( hList, CB_ADDSTRING, 0, (LPARAM)aszPorts[iIdx] );

        g_pKato->Log( LOG_DETAIL, 
                      TEXT("In %s % line %d: Add \"%s\" to list returned %d"),
                      __THIS_FILE__, __LINE__, aszPorts[iIdx], nItems );
        
        DEFAULT_ERROR( CB_ERR == nItems, return 0 );

    } // end for( iIdx = 0; iIdx < nPorts; iIdx++ )

    return (nItems+1);

} // end BOOL FillCommPortsList( HWND hList )

#endif

/*++
 
GetCommPortDialogProc:
 
	This is the DialogProc for the Comm Port selection dialog.
 
Arguments:
 
	Win32 DialogProc arguments.
 
Return Value:
 
	Win32 DialogProc return values
 

 

 
Notes:
 
--*/
BOOL CALLBACK GetCommPortDialogProc( HWND hWndDlg, 
                                     UINT uMsg, 
                                     WPARAM wParam, 
                                     LPARAM lParam )

{

    HWND                        hItem;
    INT                         iRtn;
    static UINT                 uTimer = 0;
    static INT                  nTimer = 0;



    switch( uMsg )
    {
    case WM_INITDIALOG:

        hItem = GetDlgItem( hWndDlg, IDC_COMMPORTS );
        iRtn = FillCommPortsList( hItem );
        DEFAULT_ERROR(0 == iRtn, EndDialog( hWndDlg, 1 ) );
        
        SendMessage( hItem, CB_SETCURSEL, 0, 0 );

        MessageBeep( 0xFFFFFFFF);
        uTimer = SetTimer( hWndDlg, 1, 2000, NULL );
                     
        return TRUE;

    case WM_TIMER:
         
         if( 10 == nTimer )
         {
            lstrcpy( g_lpszCommPort, DEFCOMMPORT );
            EndDialog( hWndDlg, 1);
            return TRUE;

         }
         else if( 6 < nTimer )
         {
            MessageBeep( 0xFFFFFFFF);
         }
         
         nTimer++;

         return TRUE;

            
    case WM_COMMAND:

        switch( LOWORD(wParam) )
        {

        case IDC_OK:
            /* --------------------------------------------------------
                Get the dialog info.
            -------------------------------------------------------- */
            // Get Com Port
            hItem = GetDlgItem( hWndDlg, IDC_COMMPORTS );
            iRtn = SendMessage( hItem, CB_GETCURSEL, 0, 0 );
            SendMessage( hItem, CB_GETLBTEXT, iRtn, (LPARAM)g_lpszCommPort );

            EndDialog( hWndDlg, 0 );
            return TRUE;

        case IDC_CANCEL:
            g_lpszCommPort[0] = (TCHAR)'\0';
            EndDialog( hWndDlg, 1 );
            return TRUE;

        } // end switch( LOWORD(wParam) )

    } // end switch( uMsg )

    return FALSE;


} // end BOOL CALLBACK CreateServerDialogProc( HWND, UINT, WPARAM, LPARAM );


/*++
 
SetTestCommPort:
 
	This fuction determines the default COMM port.
 
Arguments:
 
	NONE
 
Return Value:
 
	TRUE for success
 

 
 
Notes:
 
--*/

BOOL SetTestCommPort()
{
    int iRtn;

    iRtn = DialogBox( g_spsShellInfo.hLib, MAKEINTRESOURCE(IDD_PORTSELECT), 
                      NULL, (DLGPROC)GetCommPortDialogProc );	
    if( iRtn )
    {
        FUNCTION_ERROR( -1 == iRtn, iRtn = 0 );
        g_pKato->Log( LOG_WARNING, 
                      TEXT("WARNING in %s @ line %d: COMM Port not slected using default"),
                      __THIS_FILE__, __LINE__ );
        lstrcpy( g_lpszCommPort, DEFCOMMPORT );
    }
    
    return TRUE;
    
} // end BOOL SetTestCommPort()

