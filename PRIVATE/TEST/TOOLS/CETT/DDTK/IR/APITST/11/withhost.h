//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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
