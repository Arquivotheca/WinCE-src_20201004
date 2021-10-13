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

//
// This module contains helper functions for determining device driver ISR configuration,
// including:
//	DDKReg_GetIsrInfo
//

#include <windows.h>
#include <nkintr.h>			// for SYSINTR_NOP
#include <devload.h>
#include <ceddk.h>
#include <ddkreg.h>
#include <pcibus.h>
#include "ddkreg_i.h"

//
// Populate a DDKISRINFO with information from the registry.  If an ISR DLL is specified,
// a handler entry point and an IRQ must also be specified.
//
//
// RETURN VALUE:
//	ERROR_SUCCESS means that the DDKISRINFO structure has been populated successfully.
//  ERROR_INVALID_PARAMETER may indicate a size problem with the PDDKISRINFO structure.
//	ERROR_INVALID_DATA may indicate that a registry value had an unexpected type, or that
//		some required fields were missing from the Registry.
//  Other return codes are defined in winerror.h.
//
// If no IRQ value is specified in the registry, dwIrq will be set to IRQ_UNSPECIFIED.
// If no SYSINTR value is specified in the registry, dwSysintr will be set to SYSINTR_NOP.
//
DWORD WINAPI
DDKReg_GetIsrInfo(HKEY hk, PDDKISRINFO pii)
{
	DWORD dwStatus, dwSize, dwType;
	int i;
	REGQUERYPARAMS rvInterrupt[] = {
		{ PCIBUS_IRQ_VALNAME, PCIBUS_IRQ_VALTYPE, &pii->dwIrq, sizeof(pii->dwIrq) },
		{ PCIBUS_SYSINTR_VALNAME, PCIBUS_SYSINTR_VALTYPE, &pii->dwSysintr, sizeof(pii->dwSysintr) },
		{ _T("IsrHandler"), REG_SZ, pii->szIsrHandler, sizeof(pii->szIsrHandler) },
		{ _T("IsrDll"), REG_SZ, pii->szIsrDll, sizeof(pii->szIsrDll) },
	};

	// sanity check parameters
	if(hk == NULL || pii == NULL || pii->cbSize != sizeof(DDKISRINFO)) {
		return ERROR_INVALID_PARAMETER;
	}

	// clear the structure
	dwSize = pii->cbSize;
	memset(pii, 0, dwSize);
	pii->cbSize = dwSize;

	// read interrupt configuration values
	pii->dwIrq = IRQ_UNSPECIFIED;
	pii->dwSysintr = SYSINTR_NOP;
	for(i = 0; i < _countof(rvInterrupt); i++) {
		dwSize = rvInterrupt[i].dwValSize;
		dwStatus = RegQueryValueEx(hk, rvInterrupt[i].pszValName, NULL, &dwType, (LPBYTE) rvInterrupt[i].pvVal, &dwSize);
		if(dwStatus == ERROR_SUCCESS && dwType != rvInterrupt[i].dwValType) {
			dwStatus = ERROR_INVALID_DATA;
			goto done;
		}
	}

	// no errors so far
	dwStatus = ERROR_SUCCESS;

	// sanity check return values
	if(pii->szIsrDll[0] != 0 || pii->szIsrHandler[0] != 0) {
		if(pii->szIsrDll[0] == 0 || pii->szIsrHandler[0] == 0 ) {
			dwStatus = ERROR_INVALID_DATA;
			goto done;
		}
	}

done:
	return dwStatus;
}
