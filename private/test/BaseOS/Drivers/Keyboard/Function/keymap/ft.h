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
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998  Microsoft Corporation.  All Rights Reserved.

Module Name:

ft.h

Abstract:

This is file contains the test cases


--*/

//********************************************************************************************************

extern TESTPROCAPI testUSKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
extern TESTPROCAPI testJapKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE ); //a-rajtha
extern TESTPROCAPI test_AllKeyboardsinImage( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

extern TESTPROCAPI testARAKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
extern TESTPROCAPI testARA102KeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
extern TESTPROCAPI testHEBKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
extern TESTPROCAPI testHINKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
extern TESTPROCAPI testGUJKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
extern TESTPROCAPI testTAMKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
extern TESTPROCAPI testKANKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
extern TESTPROCAPI testMARKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
extern TESTPROCAPI testPUNKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
extern TESTPROCAPI testTELKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );
extern TESTPROCAPI testTHAKeyMapping( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE );

extern TESTPROCAPI Instructions_T( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE ); //a-rajtha


#define BVT_BASE 10

FUNCTION_TABLE_ENTRY g_lpFTE[] = 
{
    { TEXT("Keyboard Mapping Test"          ), 0,   0,  0,              NULL },
    { TEXT("English Keymap"                 ), 1,   0,  BVT_BASE+40,    testUSKeyMapping },
    { TEXT("Japanese Keymap"                ), 1,   0,  BVT_BASE+50,    testJapKeyMapping},
    { TEXT("All Keyboards in OS image"      ), 1,   0,  BVT_BASE+60,    test_AllKeyboardsinImage},
    { TEXT("Arabic (101) Keymap"            ), 1,   0,  BVT_BASE+70,    testARAKeyMapping},
    { TEXT("Arabic (102) Keymap"            ), 1,   0,  BVT_BASE+71,    testARA102KeyMapping},
    { TEXT("Hebrew Keymap"                  ), 1,   0,  BVT_BASE+80,    testHEBKeyMapping},
    { TEXT("Hindi Keymap"                   ), 1,   0,  BVT_BASE+90,    testHINKeyMapping},
    { TEXT("Thai Keymap"                    ), 1,   0,  BVT_BASE+100,   testTHAKeyMapping},
    { TEXT("Gujarati Keymap"                ), 1,   0,  BVT_BASE+110,   testGUJKeyMapping},
    { TEXT("Tamil Keymap"                   ), 1,   0,  BVT_BASE+120,   testTAMKeyMapping},
    { TEXT("Kannada Keymap"                 ), 1,   0,  BVT_BASE+130,   testKANKeyMapping},
    { TEXT("Marathi Keymap"                 ), 1,   0,  BVT_BASE+140,   testMARKeyMapping},
    { TEXT("Punjabi Keymap"                 ), 1,   0,  BVT_BASE+150,   testPUNKeyMapping},
    { TEXT("Telugu Keymap"                  ), 1,   0,  BVT_BASE+160,   testTELKeyMapping},
    { NULL,                                    0,   0,  0,              NULL }  // marks end of list
};


