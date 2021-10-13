//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
TESTPROCAPI ConfirmSetIASValidInt			(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ConfirmSetIASValidOctetSeq		(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ConfirmSetIASValidUsrStr		(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ConfirmQueryIASValidInt			(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ConfirmQueryIASValidOctetSeq	(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ConfirmQueryIASValidUsrStr		(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ConfirmQueryClassNameLen		(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ConfirmQueryAttribNameLen		(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI ConfirmDeleteIASAttribute		(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

void	GenerateName( char*, int );
