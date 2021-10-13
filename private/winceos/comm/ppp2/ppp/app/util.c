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
/*****************************************************************************
* 
*
*   @doc
*   @module util.c | Utility Functions
*
*/


//  Include Files

#include "windows.h"
#include "types.h"
#include "cclib.h"
#include <pkfuncs.h>

#include "cxport.h"
#include "crypt.h"
#include "memory.h"

//  PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "lcp.h"
#include "auth.h"
#include "ccp.h"
#include "mac.h"
#include "ras.h"

#include "pppserver.h"

#include "iphlpapi.h"
#include "dhcp.h"
#include <cenet.h>
#include <sha2.h>

//
// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//
NDIS_PHYSICAL_ADDRESS HighestAcceptableMax = NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);

void *
pppAllocateMemory(
	IN	UINT	nBytes)
//
//	Call NdisAllocateMemory to allocate a block of the requested size.
//
{
	void			*pMem;
	NDIS_STATUS		 Status;

    PREFAST_ASSERT(nBytes <= 1000000);

    Status = NdisAllocateMemory( (PVOID *)&pMem, nBytes, 0, HighestAcceptableMax);
    if (Status != NDIS_STATUS_SUCCESS)
	{
		DEBUGMSG (ZONE_ERROR, (TEXT("PPP: ERROR - AllocMem %u bytes failed\n"), nBytes));
		pMem = NULL;
	}
	else
	{
		DEBUGMSG(ZONE_ALLOC, (TEXT("PPP: ALLOC=%x size=%d\n"), pMem, nBytes));
        PREFAST_SUPPRESS(419, "pMem is guaranteed by NDIS to point to nBytes of memory");
		memset(pMem, 0, nBytes);
	}
	return pMem;
}

void
pppFreeMemory(
	IN	void	*pMem,
	IN	UINT	 nBytes)
{
	if (pMem != NULL)
	{
		DEBUGMSG(ZONE_ALLOC, (TEXT("PPP: FREE=%x size=%d\n"), pMem, nBytes));
		NdisFreeMemory(pMem, nBytes, 0);
	}
}

/*****************************************************************************
* 
*   @func   void * | pppAlloc | Allocate dynamic memory from ppp heap
*
*   @rdesc  returns void pointer
*   
*   @parm   pppSession_t * | s_p   | session handle
*   @parm   DWORD          | size  | size to allocate
*               
*   @comm 	This function allocates the requested data from the PPP heap,
*			and zero initializes it.
*/

void *
pppAlloc( pppSession_t *s_p, DWORD size)
{
	void	*pMem;

	ASSERT( size );

	pMem = HeapAlloc( s_p->hHeap, HEAP_ZERO_MEMORY, size );
	if(pMem )
	{
	    /* Keep track of memory usage */
	    s_p->MemUsage += size;
    }

    DEBUGMSG( ZONE_ERROR && (pMem == NULL), (TEXT( "PPP: HeapAlloc %u bytes failed\n" ), size));
    DEBUGMSG( ZONE_ALLOC, (TEXT( "PPP: HeapAlloc: Addr=%08x Length=%u\n" ), pMem, size ));
	return pMem;
}

/*****************************************************************************
* 
*   @func   void * | pppFree | Free dynamic memory from ppp heap
*
*   @rdesc  returns void pointer
*   
*   @parm   HANDLE * | hHeap    | heap handle
*   @parm   LPVOID   | address  | address to free
*               
*   @comm 
*           This function frees the supplied data back to the PPP heap.
*/

BOOL
pppFree(
	pppSession_t *s_p,
	LPVOID        address)
{
	BOOL	bOk = TRUE;

	if (address)
	{
		bOk = HeapFree( s_p->hHeap, 0, address );
	}

    DEBUGMSG(ZONE_ALLOC || (ZONE_ERROR && !bOk), (TEXT( "PPP: HeapFree: Addr=%08x %hs\n" ), address, bOk ? "Success" : "FAILED"));
	return bOk;
}


/*****************************************************************************
* 
*   @func   BYTE * | StrToUpper | Changes string to upper case
*
*   @rdesc  Returns a BYTE *
*   
*   @parm   BYTE *  | string     | String to convert
*               
*   @comm 
*           This function is used to convert a string to upper case.
*           Currently no unicode support.
*/

BYTE *
StrToUpper( BYTE *string )
{
    BYTE *rc = string;

    DEBUGMSG( ZONE_PPP, (TEXT( "string '%s'\r\n" ), string ) );

    while( *string )
    {
        if( (*string >= 'a') && (*string <= 'z') )
        {
            *string = *string - 0x20;
        }
        string++;
    }

    return( rc );
}

/*****************************************************************************
* 
*   @func   BYTE | HexCharValue | Returns the integer value of hex character 
*                                 'ch' or 0xFF is 'ch' is not a hexadecimal.
*
*   @parm   CHAR | ch   | character
*/

BYTE
HexCharValue( CHAR ch )
{
    if (ch >= '0' && ch <= '9')
    {
        return( (BYTE )(ch - '0') );
    }
    else if (ch >= 'A' && ch <= 'F')
    {
        return( (BYTE )(ch - 'A'+ 10) );
    }
    else if (ch >= 'a' && ch <= 'f')
    {
        return( (BYTE )(ch - 'a' + 10) );
    }
    else
    {
        return( 0xFF );
    }
}

DWORD
pppInsertCompleteCallbackRequest(
	IN	OUT	CompleteHanderInfo_t **ppCompletionList,
	IN		void				(*pCompleteCallback)(PVOID), OPTIONAL
	IN		PVOID				pCallbackData)				 OPTIONAL
{
	CompleteHanderInfo_t	*pPendingComplete;
	DWORD					 dwResult = ERROR_SUCCESS;

	if (pCompleteCallback)
	{
		pPendingComplete = pppAllocateMemory(sizeof(*pPendingComplete));

		if (pPendingComplete)
		{
			pPendingComplete->pNext = *ppCompletionList;
			pPendingComplete->pCompleteCallback = pCompleteCallback;
			pPendingComplete->pCallbackData = pCallbackData;
			*ppCompletionList = pPendingComplete;
		}
		else
		{
			dwResult = ERROR_OUTOFMEMORY;
		}
	}

	return dwResult;
}

void
pppExecuteCompleteCallbacks(
	IN	OUT	CompleteHanderInfo_t **ppCompletionList)
{
	CompleteHanderInfo_t	*pPendingComplete,
							*pNextPendingComplete;

	//
	//	Walk through the chain of completion requests
	//	and complete each one.
	//

	for (pPendingComplete = *ppCompletionList;
		 pPendingComplete != NULL;
		 pPendingComplete = pNextPendingComplete)
	{
		// Save the pointer to the next before we free the current

		pNextPendingComplete = pPendingComplete->pNext;

		// Call the completion callback function
		pPendingComplete->pCompleteCallback(pPendingComplete->pCallbackData);

		// Free the node
		pppFreeMemory(pPendingComplete, sizeof(*pPendingComplete));
	}

	*ppCompletionList = NULL;
}

extern HANDLE g_hIpHlpApiMod;
typedef DWORD (* PFN_GETADAPTERSINFO)(
    PIP_ADAPTER_INFO pAdapterInfo,
    PULONG pOutBufLen
    );

BOOL
IsValidMacAddress(
	IN	    	PBYTE	pData,
	IN			DWORD	cbData)
//
//	Function to verify that a MAC address conforms to the IEEE spec.
//
//	At this point, it just checks that the address is not all zero.
//	If it is all zero, it returns FALSE.
//
{
	BOOL bIsAllZero = TRUE;

	for ( ; cbData--; pData++)
	{
		if (*pData != 0)
		{
			bIsAllZero = FALSE;
			break;
		}
	}

	return !bIsAllZero;
}

BOOL
utilFindEUI(
	IN	DWORD	dwSkipCount,
	IN	BOOL	bNeedDhcpEnabled,
	OUT	PBYTE	pMacAddress,
	IN	DWORD	bmMacAddressSizesWanted,
	OUT	PDWORD	pcbMacAddress,
	OUT	PDWORD	pdwIfIndex OPTIONAL)
//
//	Attempt to find a IEEE 48 or 64 bit MAC address from an adapter
//  in the system.
//
{
    PIP_ADAPTER_INFO	AdapterInfo;
    PIP_ADAPTER_INFO	AdaptersInfo;
	PFN_GETADAPTERSINFO pGetAdaptersInfo;
    ULONG				Length;
	BOOL				bSuccess = FALSE;
	DWORD				Error;

	//
	//	If the IP helper dll is not currently loaded, try to load it
	//
	if (g_hIpHlpApiMod == NULL)
		g_hIpHlpApiMod = LoadLibrary(TEXT("iphlpapi.dll"));

    if (g_hIpHlpApiMod == NULL)
	{
		DEBUGMSG(ZONE_ERROR, (TEXT("PPP: Unable to LoadLibrary iphlpapi.dll\n")));
	}
	else
	{
        pGetAdaptersInfo = (PFN_GETADAPTERSINFO)GetProcAddress(g_hIpHlpApiMod, L"GetAdaptersInfo");

		if (pGetAdaptersInfo == NULL)
		{
			DEBUGMSG(ZONE_ERROR, (TEXT("PPP: Unable to GetProcAddress GetAdaptersInfo\n")));
		}
		else
		{
			Length = 0;
			Error = pGetAdaptersInfo(NULL, &Length);
			if ((Error == ERROR_NO_DATA)
			||  (Error == SUCCESS && Length == 0))
			{
				DEBUGMSG(ZONE_PPP, (TEXT("PPP: utilFindEUI- No network adapters\n")));
			}
			else if (Error && Error != ERROR_BUFFER_OVERFLOW)
			{
				DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR - GetAdaptersInfo failed Error=%d\n"), Error));
			}
			else
			{
				AdaptersInfo = (PIP_ADAPTER_INFO)pppAllocateMemory(Length);

				if (AdaptersInfo == NULL)
				{
					DEBUGMSG(ZONE_ERROR, (TEXT("PPP: Allocate AdaptersInfo %d failed\n"), Length));
				}
				else
				{
					Error = pGetAdaptersInfo(AdaptersInfo, &Length);
					if (Error)
					{
						DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR - GetAdaptersInfo failed %d\n"), Error));
					}
					else if (Length == 0)
					{
						DEBUGMSG(ZONE_WARN, (TEXT("PPP: No network adapters present in system\n")));
					}
					else
					{
						for (AdapterInfo = AdaptersInfo;
							 AdapterInfo;
							 AdapterInfo = AdapterInfo->Next)
						{
							//
							//	Look for an adapter with the appropriate length MAC address
							//
							ASSERT(AdapterInfo->AddressLength < 32);

							if (((1 << AdapterInfo->AddressLength) & bmMacAddressSizesWanted)
							&&  (!bNeedDhcpEnabled || (AdapterInfo->DhcpEnabled && 
							                           AdapterInfo->DhcpServer.IpAddress.String[0] &&
													   memcmp(&AdapterInfo->DhcpServer.IpAddress.String[0], "255.255.255.255", 15) != 0))
							&&	 IsValidMacAddress(AdapterInfo->Address, AdapterInfo->AddressLength))
							{
								if (dwSkipCount > 0)
								{
									dwSkipCount--;
								}
								else
								{
									*pcbMacAddress = AdapterInfo->AddressLength;
									memcpy(pMacAddress, AdapterInfo->Address, AdapterInfo->AddressLength);
									if (pdwIfIndex)
										*pdwIfIndex = AdapterInfo->Index;

									bSuccess = TRUE;
									break;
								}
							}
						}
					}
					pppFreeMemory(AdaptersInfo, Length);
				}
			}
		}
	}

	return bSuccess;
}

BOOL
utilGetEUI(
	IN	DWORD	dwSkipCount,
	OUT	PBYTE	pMacAddress,
	OUT	PDWORD	pcbMacAddress)
//
//	Attempt to find a IEEE 48 or 64 bit MAC address from an adapter
//  in the system.
//
{
	BOOL bSuccess;

	bSuccess = utilFindEUI(dwSkipCount, FALSE, pMacAddress, (1<<6) | (1<<8), pcbMacAddress, NULL);

	return bSuccess;
}

BOOL
utilGetDeviceIDHash(
	OUT	    PBYTE	pID,
	IN  OUT	PDWORD	pcbID)
//
//  Create a (hopefully unique) device ID from system unique identifier (UUID).
//
{
    DWORD      dwSize;
	DWORD      bytesToCopy;
	BOOL       bGotDeviceID;
    SHA256_CTX Context;
	DWORD      dwSPI = SPI_GETUUID;
	GUID       guid;

	dwSize = sizeof(guid);
    if (KernelIoControl(IOCTL_HAL_GET_DEVICE_INFO, (PVOID)&dwSPI, sizeof(dwSPI), (PVOID)&guid, dwSize, &dwSize))
    {    
        uchar UuidTemp[SHA256_DIGEST_LEN];
        //we have modified this function to use SHA2 instead of MD5 hashing.This was done as a part of Crypto BADAPI removal.
        SHA256Init(&Context);
        SHA256Update(&Context, (uchar *)&guid, sizeof(guid));
        SHA256Final(&Context,UuidTemp);

		bytesToCopy = *pcbID;
		if (bytesToCopy > SHA256_DIGEST_LEN)
			bytesToCopy = SHA256_DIGEST_LEN;

		memcpy(pID, UuidTemp, bytesToCopy);

		*pcbID = bytesToCopy;
		bGotDeviceID = TRUE;
    }
	else
	{
		*pcbID = 0;
		bGotDeviceID = FALSE;
	}
	return bGotDeviceID;
}

