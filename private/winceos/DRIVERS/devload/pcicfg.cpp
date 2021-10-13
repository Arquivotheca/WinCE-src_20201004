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

// This module contains helper routines for reading PCI configuration information,
// including:
//	DDKReg_GetPciInfo
//

#include <windows.h>
#include <ceddk.h>
#include <ddkreg.h>
#include <pcibus.h>
#include "ddkreg_i.h"

//
// Populate the DDKPCIINFO structure with information from the registry.  Not all PCI bus ID
// fields may be included in the target device's PCI header.  The dwWhichIds member is a 
// bitmask indicating which ones are actually present.
//
// RETURN VALUE:
//	ERROR_SUCCESS means that the PDDKPCIINFO structure has been populated successfully.
//  ERROR_INVALID_PARAMETER may indicate a size problem with the PPDDKPCIINFO structure.
//	ERROR_INVALID_DATA may indicate that a registry value had an unexpected type or was
//		internally inconsistent.
//  Other return codes are defined in winerror.h.
//
DWORD WINAPI
DDKReg_GetPciInfo(HKEY hk, PDDKPCIINFO ppi)
{
	DWORD dwStatus, dwSize, dwType;
	int i;
	REGQUERYPARAMS rvRequired[] = {
		{ PCIBUS_DEVICENUMBER_VALNAME, PCIBUS_DEVICENUMBER_VALTYPE, &ppi->dwDeviceNumber, sizeof(ppi->dwDeviceNumber) },
		{ PCIBUS_FUNCTIONNUMBER_VALNAME, PCIBUS_FUNCTIONNUMBER_VALTYPE, &ppi->dwFunctionNumber, sizeof(ppi->dwFunctionNumber) },
		{ PCIBUS_INSTANCEINDEX_VALNAME, PCIBUS_INSTANCEINDEX_VALTYPE, &ppi->dwInstanceIndex, sizeof(ppi->dwInstanceIndex) },
	};
	REGQUERYPARAMS rvDeviceId[] = {
		{ PCIBUS_CLASS_VALNAME, PCIBUS_CLASS_VALTYPE, &ppi->idVals[PCIID_CLASS], sizeof(DWORD) },
		{ PCIBUS_SUBCLASS_VALNAME, PCIBUS_SUBCLASS_VALTYPE, &ppi->idVals[PCIID_SUBCLASS], sizeof(DWORD) },
		{ PCIBUS_PROGIF_VALNAME, PCIBUS_PROGIF_VALTYPE, &ppi->idVals[PCIID_PROGIF], sizeof(DWORD) },
		{ PCIBUS_VENDORID_VALNAME, PCIBUS_VENDORID_VALTYPE, &ppi->idVals[PCIID_VENDORID], sizeof(DWORD) },
		{ PCIBUS_DEVICEID_VALNAME, PCIBUS_DEVICEID_VALTYPE, &ppi->idVals[PCIID_DEVICEID], sizeof(DWORD) },
		{ PCIBUS_REVISIONID_VALNAME, PCIBUS_REVISIONID_VALTYPE, &ppi->idVals[PCIID_REVISIONID], sizeof(DWORD) },
		{ PCIBUS_SUBVENDORID_VALNAME, PCIBUS_SUBVENDORID_VALTYPE, &ppi->idVals[PCIID_SUBSYSTEMVENDORID], sizeof(DWORD) },
		{ PCIBUS_SUBSYSTEMID_VALNAME, PCIBUS_SUBSYSTEMID_VALTYPE, &ppi->idVals[PCIID_SUBSYSTEMID], sizeof(DWORD) },
	};

	// sanity check parameters
	if(hk == NULL || ppi == NULL || ppi->cbSize != sizeof(DDKPCIINFO)) {
		return ERROR_INVALID_PARAMETER;
	}

	// clear the structure
	dwSize = ppi->cbSize;
	memset(ppi, 0, dwSize);
	ppi->cbSize = dwSize;

	// read memory mapping and instance values
	for(i = 0; i < _countof(rvRequired); i++) {
		dwSize = rvRequired[i].dwValSize;
		dwStatus = RegQueryValueEx(hk, rvRequired[i].pszValName, NULL, &dwType, (LPBYTE) rvRequired[i].pvVal, &dwSize);
		if(dwStatus != ERROR_SUCCESS) {
			goto done;
		}
		if(dwType != rvRequired[i].dwValType) {
			dwStatus = ERROR_INVALID_DATA;
			goto done;
		}
	}

	// read device id values -- this table needs to have the same order as entries in idVals
	ppi->dwWhichIds = 0;
	for(i = 0; i < _countof(rvDeviceId); i++) {
		dwSize = rvDeviceId[i].dwValSize;
		dwStatus = RegQueryValueEx(hk, rvDeviceId[i].pszValName, NULL, &dwType, (LPBYTE) rvDeviceId[i].pvVal, &dwSize);
		if(dwStatus == ERROR_SUCCESS) {
			if(dwType != rvDeviceId[i].dwValType) {
				dwStatus = ERROR_INVALID_DATA;
				goto done;
			} else {
				ppi->dwWhichIds |= (1 << i);
			}
		}
	}

	// no errors during read
	dwStatus = ERROR_SUCCESS;

done:
	return dwStatus;
}
