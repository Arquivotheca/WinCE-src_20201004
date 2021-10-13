//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include <winsock.h>
#include <oscfg.h>

#undef AF_IRDA
#include <af_irda.h>

#include "irsup.h"

//  ***********************************************************
//  *** IR Support Library
//  ***********************************************************

//
//	The af_irda.h header contains two nearly identical structures
//  for doing IAS_SETs and IAS_QUERYs.  They are different enough
//  that a single C function cannot print them, since even though
//  their fields have the same names they do not always have the
//  same data types (for reasons unknown).  These macros, however, can
//  be used to print them.
//

#define IAS_PRINT_CLASS(pSet) \
	OutStr(TEXT("%hs.%hs = "), &pSet->irdaClassName[0], &pSet->irdaAttribName[0]);

#define IAS_PRINT_VARIABLE(pSet) \
	switch(pSet->irdaAttribType) \
	{ \
	case IAS_ATTRIB_NO_ATTRIB: \
		OutStr(TEXT("[NONE]")); \
		break; \
	case IAS_ATTRIB_INT: \
		OutStr(TEXT("[INT] %d"), pSet->irdaAttribute.irdaAttribInt); \
		break; \
	case IAS_ATTRIB_OCTETSEQ: \
		OutStr(TEXT("[OCTETSEQ] length %d:"), pSet->irdaAttribute.irdaAttribOctetSeq.Len); \
		PrintHex(&pSet->irdaAttribute.irdaAttribOctetSeq.OctetSeq[0], pSet->irdaAttribute.irdaAttribOctetSeq.Len); \
		break; \
	case IAS_ATTRIB_STR: \
		OutStr(TEXT("[USRSTR] length %d:"), pSet->irdaAttribute.irdaAttribUsrStr.Len); \
		OutStr(TEXT("%.*hs"), pSet->irdaAttribute.irdaAttribUsrStr.Len, &pSet->irdaAttribute.irdaAttribUsrStr.UsrStr[0]); \
		break; \
	default: \
		OutStr(TEXT("???")); \
		break; \
	} \
	OutStr(TEXT("\n"));

static void
PrintHex(
	BYTE *pHex,
	int   length)
{
	while (length-- > 0)
		OutStr(TEXT("%02X "), *pHex++);
	OutStr(TEXT("\n"));
}

extern void
IRPrintIASSet(
	IAS_SET *pSet)
{
	IAS_PRINT_CLASS(pSet);
	IAS_PRINT_VARIABLE(pSet);
}

extern void
IRPrintIASQueryRequest(
	IAS_QUERY *pQuery)
{
	IRPrintDeviceID(&pQuery->irdaDeviceID[0]);
	IAS_PRINT_CLASS(pQuery);
}

extern void
IRPrintIASQueryResult(
	IAS_QUERY *pQuery)
{
	IRPrintIASQueryRequest(pQuery);
	IAS_PRINT_VARIABLE(pQuery);
}

extern void
IRPrintDeviceID(
	u_char	*pDeviceID)
{
	for (int i = 0; i < 4; i++)
		OutStr(TEXT(" %02X"), pDeviceID[i]);
}

extern void
IRPrintDeviceList(
	DEVICELIST	*pDevList)
{
	for (ULONG i = 0; i < pDevList->numDevice; i++)
	{
		OutStr(TEXT("Dev %d:"), i);
		IRPrintDeviceID(&pDevList->Device[i].irdaDeviceID[0]);
		OutStr(TEXT("\n"));
	}
}

// Get a list of all the visible IR devices

extern DEVICELIST *
IREnumDeviceList(
	int		maxDevices)
{
	DEVICELIST		*pDevList;
	int				devListLen;
	SOCKET          s;

	devListLen = sizeof(DEVICELIST) + sizeof(IRDA_DEVICE_INFO) * (maxDevices - 1);
	pDevList = (DEVICELIST *)malloc(devListLen);
	if (pDevList)
	{
		// Create a temporary socket to do the device enumeration
		s = socket(AF_IRDA, SOCK_STREAM, 0);
		if (s != INVALID_SOCKET)
		{
			pDevList->numDevice = 0;
			if (getsockopt(s,
					SOL_IRLMP, 
					IRLMP_ENUMDEVICES,
					(char *)pDevList, 
					&devListLen) != 0)
			{
				free(pDevList);
				pDevList = NULL;
				OutStr(TEXT("IRLMP_ENUMDEVICES got error %d\n"), WSAGetLastError());
			}
			closesocket(s);
		}
		else
		{
			free(pDevList);
			pDevList = NULL;
		}
	}
	else
	{
		OutStr(TEXT("Unable to allocate space for %d devices\n"), maxDevices);
	}
	return pDevList;
}

//
//	Find the deviceID of our peer IR device.
//  Because it takes a while for old deviceIDs to decay
//  and be thrown away by the IR protocol stack, we want
//  to use the most recent device ID if there are multiple
//  ones available.  We hope that the newest will be first
//  in the list (which begs the question of why we try to
//  get more than just one!)
//
//  Return 0 if successful, -1 otherwise.

extern int
IRFindNewestDevice(
	u_char *pDeviceID,
	int		maxDevices)
{
	DEVICELIST		*pDevList;
	int				result = -1;
	int				retries;

	for (retries = 0; retries < 5; retries++)
	{
		pDevList = IREnumDeviceList(maxDevices);
		if (pDevList != NULL && pDevList->numDevice > 0)
		{
			// Found 1 or more devices, return the newest
			memcpy(pDeviceID, &pDevList->Device[0].irdaDeviceID[0], sizeof(pDevList->Device[0].irdaDeviceID));
			free(pDevList);
			result = 0;
			break;
		}
		Sleep(2000);
	}
	return result;
}

//
//  Perform an IRLMP_IAS_QUERY operation on the specified socket with the
//  specified IAS_QUERY parameter.
//
extern void
IRIasQuery(
	SOCKET	   s,
	IAS_QUERY *pQuery,
	int		  *pResult,
	int		  *pWSAError)
{
	int length = sizeof(IAS_QUERY) + 1024;

	OutStr(TEXT("Query device"));
	IRPrintDeviceID(&pQuery->irdaDeviceID[0]);
	OutStr(TEXT(" for %hs.%hs\n"), &pQuery->irdaClassName[0], &pQuery->irdaAttribName[0]);

	*pResult = getsockopt(s, SOL_IRLMP, IRLMP_IAS_QUERY, (char *)pQuery, &length);
	*pWSAError = WSAGetLastError();

	OutStr(TEXT("QUERY result=%d WSAError=%d length=%d\n"), *pResult, *pWSAError, length);
	if (*pResult == 0)
		IRPrintIASQueryResult(pQuery);
}

//
//  Perform an IRLMP_IAS_SET operation on the specified socket with the
//  specified IAS_SET parameter.
//
extern void
IRIasSet(
	SOCKET	   s,
	IAS_SET   *pSet,
	int		  *pResult,
	int		  *pWSAError)
{
	IRPrintIASSet(pSet);

	*pResult = setsockopt(s, SOL_IRLMP, IRLMP_IAS_SET, (char *)pSet, sizeof(IAS_SET) + 1024);

	*pWSAError = WSAGetLastError();

	OutStr(TEXT("IRLMP_IAS_SET result %d WSAError %d\n"), *pResult, *pWSAError);
}

//
//  Bind the specified server name to the specified socket.
//
extern void
IRBind(
	SOCKET	   s,
	char	  *strServerName,
	int		  *pResult,
	int		  *pWSAError)
{
	SOCKADDR_IRDA *pAddr;

	ASSERT(strServerName != NULL);

	// Create a bed of 'a' chars for the SOCKADDR_IRDA structure
	// to ensure that characters beyond the end of it are not accessed.
	pAddr = (SOCKADDR_IRDA *)malloc(sizeof(SOCKADDR_IRDA) + 10);
	memset((char *)pAddr, 'a', sizeof(SOCKADDR_IRDA) + 10);

	pAddr->irdaAddressFamily = AF_IRDA;
	memset(&pAddr->irdaDeviceID[0], 0, sizeof(pAddr->irdaDeviceID));
	strncpy(&pAddr->irdaServiceName[0], strServerName, sizeof(pAddr->irdaServiceName));

	*pResult = bind(s, (struct sockaddr *)pAddr, sizeof(*pAddr));
	*pWSAError = WSAGetLastError();

	if (*pResult == SOCKET_ERROR)
	{
		OutStr(TEXT("SKIPPED: Bind failed for %.*hs, WSAError=%d\n"),
			sizeof(pAddr->irdaServiceName),
			&pAddr->irdaServiceName[0],
			*pWSAError);
	}

	if (*pResult == 0)
	{
		*pResult = listen(s, 1);
		*pWSAError = WSAGetLastError();
	}
}

//
//  Attempt to connect the specified socket to the specified server on the
//  newest IR device.
//
extern void
IRConnect(
	SOCKET	   s,
	char	  *strServerName,
	int		  *pResult,
	int		  *pWSAError)
{
	SOCKADDR_IRDA *pAddr;

	pAddr = (SOCKADDR_IRDA *)malloc(sizeof(*pAddr) + 10);
	memset((char *)pAddr, 'a', sizeof(*pAddr) + 10);

	ASSERT(strServerName != NULL);

	pAddr->irdaAddressFamily = AF_IRDA;
	if (IRFindNewestDevice(&pAddr->irdaDeviceID[0], 1) == 0)
	{
		strncpy(&pAddr->irdaServiceName[0], strServerName, sizeof(pAddr->irdaServiceName));

		*pResult = connect(s, (struct sockaddr *)pAddr, sizeof(*pAddr));
		*pWSAError = WSAGetLastError();
	}
	else
	{
		*pResult = -1;
		*pWSAError = WSAEHOSTUNREACH;
	}
	free((char *)pAddr);
}

// Initialize socket support

extern int
IRSockInit(void)
{
	WORD	wVersionRequested;
	WSADATA wsaData;
	int		err;

	wVersionRequested = MAKEWORD( 1, 1 ); 
	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 )
	{
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */    
		OutStr(TEXT("Unable to initialize winsock"));
		return -1;
	}
	return 0;
}