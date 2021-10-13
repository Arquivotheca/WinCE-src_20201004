//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "ShellProc.h"
#include "NdisDetectionTests.h"
#define MAX_MINIPORT_NAME 256

TEST_FUNCTION(TestIsEthernetMiniportPresent)
{
	TEST_ENTRY;
	HANDLE hNdisUio = NULL, hNdis = NULL;
	DWORD rc = TPR_FAIL;
	TCHAR multiSzBuffer[256];
	PTCHAR pszMiniport = NULL;
	size_t cchMiniport = 0;
	DWORD cbBuffer = sizeof(multiSzBuffer), cbBytesReturned;
	BOOL bSuccess = FALSE;
	NDISUIO_QUERY_OID queryOid;
	
	//Get Miniport Names
	hNdis = CreateFile(DD_NDIS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL, OPEN_ALWAYS, 0, NULL);

	if (hNdis == INVALID_HANDLE_VALUE)
	{
		CmnPrint(PT_FAIL, TEXT("Could not open NDIS device."));
		goto cleanUp;
	}

	bSuccess = DeviceIoControl(hNdis, IOCTL_NDIS_GET_ADAPTER_NAMES, NULL, 0, &multiSzBuffer[0], cbBuffer, &cbBuffer, NULL);
	if (bSuccess == FALSE)
	{
		CmnPrint(PT_FAIL, TEXT("Could not get adapter names."));
		goto cleanUp;
	}

	//Bind to NDISUIO
	hNdisUio = CreateFile(
				NDISUIO_DEVICE_NAME,							//	Object name.
				GENERIC_READ|GENERIC_WRITE,						//	Desired access.
				0x00,											//	Share Mode.
				NULL,											//	Security Attr
				OPEN_EXISTING,									//	Creation Disposition.
				FILE_ATTRIBUTE_NORMAL,	//	Flag and Attributes..
				NULL);

	if (hNdisUio == INVALID_HANDLE_VALUE)
	{
		CmnPrint(PT_FAIL, TEXT("Could not open NDISUIO device."));
		goto cleanUp;
	}


	//Query miniport for media type OID, see which one supports 8023
	//If at least one miniport reported 8023 support, return TRUE.

	queryOid.Oid = OID_GEN_MEDIA_IN_USE;
	queryOid.Data[0] = 0xff;
	queryOid.Data[1] = 0xff;
	queryOid.Data[2] = 0xff;
	queryOid.Data[3] = 0xff;

	for (pszMiniport = multiSzBuffer; *pszMiniport != '\0'; pszMiniport = pszMiniport + cchMiniport + 1)
	{
		if (FAILED(StringCchLength(pszMiniport, MAX_MINIPORT_NAME, &cchMiniport)) || (cchMiniport !=  (MAX_MINIPORT_NAME - 1)))
		{
			CmnPrint(PT_FAIL, TEXT("Could not find length of miniport %s"), pszMiniport);
			goto cleanUp;
		}

		queryOid.ptcDeviceName = pszMiniport;
		
		bSuccess = DeviceIoControl(hNdisUio, 
			IOCTL_NDISUIO_QUERY_OID_VALUE,
			(LPVOID) &queryOid,
			sizeof(NDISUIO_QUERY_OID),
		    (LPVOID) &queryOid,
			sizeof(NDISUIO_QUERY_OID),
			&cbBytesReturned,
			NULL);
		if (bSuccess == FALSE)
		{
			CmnPrint(PT_FAIL, TEXT("Could not query miniport %s."), pszMiniport);
			continue;
		}

		if (NdisMedium802_3 == *((PNDIS_MEDIUM) &(queryOid.Data)))
		{
			CmnPrint(PT_LOG, TEXT("Found 802.3 miniport %s."), pszMiniport);
			rc = TPR_PASS;
			break;
		}
		else
		{
			CmnPrint(PT_LOG, TEXT("Miniport %s reported medium type %d"), pszMiniport, *((PNDIS_PHYSICAL_MEDIUM) (queryOid.Data)));
		}
	}

cleanUp:
	if (rc == TPR_FAIL)
	{
		CmnPrint(PT_FAIL, TEXT("No 802.3 miniports found."));
	}
	if (hNdisUio)
		CloseHandle(hNdisUio);
	if (hNdis)
		CloseHandle(hNdis);	
	return rc;
}


