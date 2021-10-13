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
// Only use Bluetooth under CE
#ifdef UNDER_CE

#include <windows.h>
#include "bt_api.h"

HANDLE g_hDevice;


int GetBA (WCHAR **pp, BT_ADDR *pba) {
	while (**pp == ' ')
		++*pp;

	for (int i = 0 ; i < 4 ; ++i, ++*pp) {
		if (! iswxdigit (**pp))
			return FALSE;

		int c = **pp;
		if (c >= 'a')
			c = c - 'a' + 0xa;
		else if (c >= 'A')
			c = c - 'A' + 0xa;
		else c = c - '0';

		if ((c < 0) || (c > 16))
			return FALSE;

		*pba = *pba * 16 + c;
	}

	for (i = 0 ; i < 8 ; ++i, ++*pp) {
		if (! iswxdigit (**pp))
			return FALSE;

		int c = **pp;
		if (c >= 'a')
			c = c - 'a' + 0xa;
		else if (c >= 'A')
			c = c - 'A' + 0xa;
		else c = c - '0';

		if ((c < 0) || (c > 16))
			return FALSE;

		*pba = *pba * 16 + c;
	}

	if ((**pp != ' ') && (**pp != '\0'))
		return FALSE;

	return TRUE;
}

#define BPR		8

BOOL RegisterBTDevice(BOOL fMaster, WCHAR* wszBTAddr, int port, int channel)
{
	BOOL	fRetVal = TRUE;
	PORTEMUPortParams pp;

	// Check if we are already registered
	if(g_hDevice)
	{
		fRetVal = FALSE;
		goto exit;
	}
	
	memset (&pp, 0, sizeof(pp));

	if(fMaster == FALSE && GetBA(&wszBTAddr, &pp.device))
	{
		pp.channel = channel & 0xff;
		pp.uiportflags = RFCOMM_PORT_FLAGS_REMOTE_DCB;
	}
	else if(fMaster)
	{
		pp.flocal = TRUE;
		pp.channel = channel & 0xff;
	} 
	else
	{
		fRetVal = FALSE;
	}

	g_hDevice = RegisterDevice(L"COM", port, L"btd.dll", (DWORD)&pp);
	if(g_hDevice == NULL)
	{
		fRetVal = FALSE;
		goto exit;
	}

exit:	
	return fRetVal;
}

void UnregisterBTDevice()
{
	DeregisterDevice(g_hDevice);
	g_hDevice = NULL;
}

#endif
