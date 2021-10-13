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
#ifndef __IRIASTST_H__
#define __IRIASTST_H__

// Test function prototypes (TestProc's)
TESTPROCAPI SocketCreateTest			(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SocketCreateInvalidTest     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SocketCloseTest				(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SocketMemoryLeakTest        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindTest					(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindInvalidFamilyTest       (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindTwiceTest				(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindCloseBindTest			(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindSameNameTo2SocketsTest	(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindNullSocketTest          (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindNullAddrTest			(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindHugeAddrLengthTest      (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindVariousAddrTest         (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI GetBoundSockNameTest        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindMemoryLeakTest          (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetIASTest					(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetIASInvalidTest			(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetIASVariousIntAttribTest	(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetIASVariousOctetSeqAttribTest(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetIASVariousUsrStrAttribTest(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetIASClassNameLengthTest   (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetIASAttribNameLengthTest  (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetIASMaxAttributeCountTest (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI SetIASMemoryLeakTest        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindHardCodedLsapSelTest    (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindRandomLsapSelTest       (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI BindSameHardCodedLsapSelTest(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI COMPortUsageTest            (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI IrSIROpenFailTest           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

void	GenerateName( char*, int );

extern HANDLE	COMPortOpen( int, int* );
extern int		GetSIRCOMPort();

#endif // __IRIASTST_H__
