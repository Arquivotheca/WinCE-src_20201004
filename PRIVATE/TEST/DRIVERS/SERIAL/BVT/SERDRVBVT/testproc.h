// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------
#include "TUXMAIN.H"

#ifndef __TESTPROC_H__

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

#endif // __TESTPROC_H__
