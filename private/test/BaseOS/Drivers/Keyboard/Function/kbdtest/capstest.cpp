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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
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