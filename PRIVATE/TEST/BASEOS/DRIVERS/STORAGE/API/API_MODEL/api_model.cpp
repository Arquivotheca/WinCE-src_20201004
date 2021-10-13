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
#include "main.h"
#include "utility.h"

TESTPROCAPI RunIOCTLCommon(PDRIVER_TEST pMyTest)
// --------------------------------------------------------------------
{
	if(!pMyTest->Init())
	{
		g_pKato->Log(LOG_DETAIL,_T("Failed to Init().\n"));
		return TPR_FAIL;
	}
	if(!pMyTest->LoadParameters())
	{
		g_pKato->Log(LOG_DETAIL,_T("Failed to load parameters().\n"));
		return TPR_FAIL;
	}
	if(!pMyTest->Start())
	{
		g_pKato->Log(LOG_DETAIL,_T("Failed to RunIOCTL().\n"));
		return TPR_FAIL;
	}

	return TPR_PASS;
}
