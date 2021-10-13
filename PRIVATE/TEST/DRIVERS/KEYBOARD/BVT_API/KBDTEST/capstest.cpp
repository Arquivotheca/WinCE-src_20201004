//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     capstest.cpp  

Abstract:

  The Caps Lock Test.

--*/

#include "global.h"


//Entry point for Caps Lock test
TESTPROCAPI CapsLock_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE){
   NO_MESSAGES;
   
    int     iRetVal = 0;
	
    /*
    *   Make sure CAPSLOCK light is turned on
    */
    MessageBox(NULL,
        TEXT("Please turn CapsLock keyboard light on, then press OK."),
        TEXT("Caps Lock testing"),
        MB_OK
        );

    
    /*
    *   Turn it off programatically
    */

    keybd_event( VK_CAPITAL, 0, 0, 0 );
	keybd_event( VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0 );


    /*
    *   Ask if this was so
    */
    iRetVal = MessageBox(NULL,
        TEXT("Is CapsLock keyboard light off now?"),
        TEXT("Caps Lock testing"),
        MB_YESNO
        );

    if ( iRetVal != IDYES )
        g_pKato->Log(LOG_FAIL, 
			TEXT("Incorrect Caps Lock State.  It should be off"));

    /*
    *   Turn it on again
    */
    keybd_event( VK_CAPITAL, 0, 0, 0 );
    keybd_event( VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0 );

    /*
    *   Ask if this was so
    */
    iRetVal = MessageBox(NULL,
        TEXT("Is CapsLock keyboard light on now?"),
        TEXT("Caps Lock testing"),
        MB_YESNO
        );

    if (iRetVal != IDYES )
        g_pKato->Log(LOG_FAIL, 
			TEXT("Incorrect Caps Lock State.  It should be on"));

   return getCode();
}