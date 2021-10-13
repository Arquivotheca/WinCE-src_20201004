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
#ifndef __TUXUSBCCID_H__
#define __TUXUSBCCID_H__

#define countof(a) (sizeof(a)/sizeof(*(a)))

// Test function prototypes (TestProc's)
TESTPROCAPI TestDispatch(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.

#define BASE 1000

const DWORD TEST_ID_CHECKCARDMONITOR          = BASE;      // 1000
const DWORD TEST_ID_CHECKREADER               = BASE + 1;  // 1001
const DWORD TEST_ID_SIMULATERESMNGR           = BASE + 2;  // 1002
const DWORD TEST_ID_SMARTCARDPROVIDERTEST     = BASE + 3;  // 1003
const DWORD TEST_ID_POWERMANAGEMENTTEST       = BASE + 4;  // 1004

const DWORD TEST_ID_INSERTREMOVEREADERTTEST   = 2*BASE;    // 2000
const DWORD TEST_ID_INTERRUPTREADERTTEST      = 2*BASE+1;  // 2001
const DWORD TEST_ID_REMOVEREADERWHENBLOCKED   = 2*BASE+2;  // 2002
const DWORD TEST_ID_INSERTREMOVEDURINGSUSPEND = 2*BASE+3;  // 2003

const DWORD TEST_ID_MULTIREADER_AUTOIFD       = 3*BASE;    // 3000


// Our function table that we pass to Tux
__declspec(selectany) FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    TEXT("IFDTest2"                                        ), 0, 0, 0,                                 NULL,
    TEXT(    "Part A - Check Card Monitor"                 ), 1, 0, TEST_ID_CHECKCARDMONITOR,          TestDispatch,
    TEXT(    "Part B - Check Reader"                       ), 1, 0, TEST_ID_CHECKREADER,               TestDispatch,
    TEXT(    "Part C - Simulate Resource Manager"          ), 1, 0, TEST_ID_SIMULATERESMNGR,           TestDispatch,
    TEXT(    "Part D - Smartcard Provider Test"            ), 1, 0, TEST_ID_SMARTCARDPROVIDERTEST,     TestDispatch,
    TEXT(    "Part E - Power Management Test"              ), 1, 0, TEST_ID_POWERMANAGEMENTTEST,       TestDispatch,

    TEXT("Insertion/Removal Tests"                         ), 0, 0, 0,                                 NULL,
    TEXT(    "Reader Insertion/Removal Test"               ), 1, 0, TEST_ID_INSERTREMOVEREADERTTEST,   TestDispatch,
    TEXT(    "Reader Surprise Removal Test"                ), 1, 0, TEST_ID_INTERRUPTREADERTTEST,      TestDispatch,
    TEXT(    "Reader Removal When Blocked Test"            ), 1, 0, TEST_ID_REMOVEREADERWHENBLOCKED,   TestDispatch,
    TEXT(    "Reader Insertion/Removal During Suspend Test"), 1, 0, TEST_ID_INSERTREMOVEDURINGSUSPEND, TestDispatch,

    TEXT("Multiple Reader Tests"                           ), 0, 0, 0,                                 NULL,
    TEXT(    "Multiple Reader AutoIFD"                     ), 1, 0, TEST_ID_MULTIREADER_AUTOIFD,       TestDispatch,

    NULL,                                                     0, 0, 0,                                 NULL  // END
};

#endif // __TUXUSBCCID__
