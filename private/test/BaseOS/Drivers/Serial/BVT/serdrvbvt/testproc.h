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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#include "TUXMAIN.H"

#pragma once

// test procedures
TESTPROCAPI Tst_WritePort               (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_SetCommEvents           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_CommBreak               (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_EventTxEmpty            (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_EscapeCommFunction      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_WaitCommEvent           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_WaitCommEventAndClose   (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_RecvTimeout             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_SetBadCommDCB           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_OpenCloseShare           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI Tst_PowerCaps			(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);






