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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
TESTPROCAPI BasicRemoteConnectTest			     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RemoteConnectNameLengthTest		     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RemoteConnectNameValueTest		     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RemoteConnectSubAndSuperStringTest   (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RemoteConnectToUnboundNameTest       (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RemoteConnectAndCloseBeforeAcceptTest(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RemoteConnectMemoryLeakTest          (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RemoteConnectHardCodedLsapSelTest    (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BasicLocalConnectTest			     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI LocalConnectNameLengthTest		     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI LocalConnectNameValueTest		     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI LocalConnectSubAndSuperStringTest    (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI LocalConnectMemoryLeakTest           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI LocalConnectToUnknownDevice          (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI AfterIrSirCOMOpenFailedTest          (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI LocalConnectHardCodedLsapSelTest     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI CloseConnectInProgressTest           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ConnectAgainTest		             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

