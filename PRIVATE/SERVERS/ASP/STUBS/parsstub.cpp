//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: parsstub.cpp
Abstract: Extended parsing functions that can be compomentalized away
  		  Stubs for use if extended parsing options aren't use.
--*/

#include "aspmain.h"


const BOOL c_fUseExtendedParse = FALSE;


BOOL CASPState::ParseIncludes()
{
	DEBUGCHK(FALSE);
	return FALSE;
}

BOOL CASPState::ParseProcDirectives()
{
	DEBUGCHK(FALSE);
	return FALSE;
}

void CASPState::GetIncErrorInfo(int iScriptErrLine, 
								 int *piFileErrLine, PSTR *ppszFileName)

{
	DEBUGCHK(FALSE);
}

