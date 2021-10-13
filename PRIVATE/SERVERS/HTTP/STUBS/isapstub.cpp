//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: isapstub.cpp
Abstract: Common ISAPI Stub Functions
--*/

#include "httpd.h"

BOOL CHttpRequest::GetServerVariable(PSTR pszVar, PVOID pvOutBuf, PDWORD pdwOutSize, BOOL fFromFilter) {
	DEBUGCHK(FALSE);
	return FALSE;
}
