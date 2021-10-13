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

