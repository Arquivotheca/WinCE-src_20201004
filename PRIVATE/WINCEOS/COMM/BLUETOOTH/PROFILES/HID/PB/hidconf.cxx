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
#include <windows.h>

#include <svsutil.hxx>

#include <bt_debug.h>
#include <bt_os.h>
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_api.h>

#include "../base/hidconf.h"

int GetDeviceConfig (BD_ADDR *pba, DWORD *pdwFlags, unsigned char **ppsdp, unsigned int *pcsdp) {
	*pdwFlags = 0;
	*ppsdp = NULL;
	*pcsdp = 0;

	WCHAR szKeyName[MAX_PATH];
	StringCchPrintf (STRING_AND_COUNTOF(szKeyName), L"software\\microsoft\\bluetooth\\device\\hid\\%04x%08x", pba->NAP, pba->SAP);

	HKEY hk;
	if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, szKeyName, 0, KEY_READ, &hk) != ERROR_SUCCESS)
		return FALSE;

	DWORD dwType = 0;
	DWORD dw = 0;
	DWORD cb = sizeof(dw);

	DWORD dwRes = ERROR_SUCCESS;
	
	dwRes = RegQueryValueEx (hk, L"active", NULL, &dwType, (LPBYTE)&dw, &cb);
	if ((dwRes == ERROR_SUCCESS) && (dwType == REG_DWORD) && (cb == sizeof(dw)) && dw)
		*pdwFlags |= HIDCONF_FLAGS_ACTIVE;

	dwType = 0;
	dw = 0;
	cb = sizeof(dw);

	dwRes = RegQueryValueEx (hk, L"auth", NULL, &dwType, (LPBYTE)&dw, &cb);
	if ((dwRes == ERROR_SUCCESS) && (dwType == REG_DWORD) && (cb == sizeof(dw)) && dw)
		*pdwFlags |= HIDCONF_FLAGS_AUTH;

	dwType = 0;
	dw = 0;
	cb = sizeof(dw);

	dwRes = RegQueryValueEx (hk, L"encrypt", NULL, &dwType, (LPBYTE)&dw, &cb);
	if ((dwRes == ERROR_SUCCESS) && (dwType == REG_DWORD) && (cb == sizeof(dw)) && dw)
		*pdwFlags |= HIDCONF_FLAGS_ENCRYPT;

	dwType = 0;
	cb = 0;

	dwRes = RegQueryValueEx (hk, L"sdp_record", NULL, &dwType, NULL, &cb);

	if (dwRes == ERROR_SUCCESS) {
		if (*ppsdp = (unsigned char *)g_funcAlloc (cb, g_pvAllocData))
			dwRes = RegQueryValueEx (hk, L"sdp_record", NULL, &dwType, *ppsdp, &cb);
	}

	RegCloseKey (hk);

	if ((dwRes != ERROR_SUCCESS) || (! *ppsdp)) {
		*pdwFlags = 0;

		if (*ppsdp) {
			g_funcFree (*ppsdp, g_pvFreeData);
			*ppsdp = NULL;
		}

		return FALSE;
	}

	*pcsdp = cb;

	return TRUE;
}
