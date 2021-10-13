//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

//  Globals


// Debug Globals

#ifdef DEBUG
TCHAR   *DbgStrLayCodes[] = 
{
    TEXT("NULL"),
    TEXT("OPEN"),
    TEXT("CLOSE"),
    TEXT("LOWER UP"),
    TEXT("LOWER DN"),
    TEXT("UP"),
    TEXT("DN"),
};
#endif


//  Externs

//  Defines

//  Local Functions

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

    Status = NdisAllocateMemory( (PVOID *)&pMem, nBytes, 0, HighestAcceptableMax);
    if (Status != NDIS_STATUS_SUCCESS)
	{
		DEBUGMSG (ZONE_ERROR, (TEXT("+PPP: AllocMem %d bytes failed\n")));
		pMem = NULL;
	}
	else
	{
		DEBUGMSG(ZONE_ALLOC, (TEXT("PPP: ALLOC=%x size=%d\n"), pMem, nBytes));
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
*   @parm   char *         | type  | comm
*               
*   @comm 	This function allocates the requested data from the PPP heap,
*			and zero initializes it.
*/

void *
pppAlloc( pppSession_t *s_p, DWORD size, TCHAR *type )
{
	void	*pMem;

    DEBUGMSG( ZONE_PPP | ZONE_FUNCTION | ZONE_ALLOC, (
	TEXT( "pppAlloc( %08X, %08X, %08X )\n" ), s_p, size, type ));
	ASSERT( size );
	ASSERT( type );


	pMem = HeapAlloc( s_p->hHeap, 0, size );

	if( NULL == pMem )
	{
    	DEBUGMSG( ZONE_ALLOC | ZONE_PPP | ZONE_ERROR, (
		TEXT( "HeapAlloc failed\n" )));
		return( NULL );
	}

	// Clear the allocated region
	memset(pMem, 0, size);

	/* Keep track of memory usage */

	s_p->MemUsage += size;

	// Memory successfully allocated

    DEBUGMSG( ZONE_PPP | ZONE_ALLOC, (
	TEXT( "HeapAlloc: Type: '%s' Addr:%08x Length: %d\n" ), type, pMem, size ));
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
*   @parm   char *   | type  	| type description
*               
*   @comm 
*           This function frees the supplied data back to the PPP heap.
*/

BOOL
pppFree(
	pppSession_t *s_p,
	LPVOID        address,
	TCHAR        *type )
{
	BOOL	bOk = TRUE;

    DEBUGMSG(ZONE_ALLOC, (TEXT( "pppFree( %08X, %08X, %s )\r\n" ), s_p, address, type ));
	ASSERT( type );

	if (address)
	{
		bOk = HeapFree( s_p->hHeap, 0, address );
	}

    DEBUGMSG(ZONE_ALLOC || (ZONE_ERROR && !bOk), (TEXT( "HeapFree:Type: '%s'  address:%08X %hs\n" ),
		type, address, bOk ? "Success" : "FAILED"));
	return bOk;
}

/*****************************************************************************
* 
*   @func   DWORD | PPPDecodeProtocol |   Decodes PPP packet protocol field
*
*   @rdesc  Returns a DWORD, the decoded protocol field (0 on failure)
*   
*   @parm   pppMsg_t   | pMsg          | Packet to decode
*               
*   @comm   This function is used to decode the packet's protocol type
*           for Protocol Management Layer routing.
*/

DWORD
PPPDecodeProtocol(
	IN  OUT pppMsg_t *msg_p)
{
	BYTE            *fr_p = msg_p->data;                // frame pntr
	DWORD			len = msg_p->len;
	USHORT			protocol = 0;							// protocol from frame header

	DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +PPPDecodeProtocol: pData=%x Len=%x\n"), fr_p, msg_p->len));

	do
	{
		//  Decode Address and Control Fields (FF 03), if present.
		//
		//  If address and control field compression was negotiated,
		//  then these fields may be absent.
		//
		//  Packet must be at least 2 bytes long to contain this field.

		if( len >= 2
		&&  fr_p[0] == PPP_ALL_STATIONS)
		{
			if (fr_p[1] == PPP_UI)
			{
				// Address and Control Bytes are
				// in the packet. Remove them.

				len   -= 2;       // adjust msg length
				fr_p  += 2;        // advance frame pntr
			}
			else
			{
				// First byte is ALL_STATIONS but second byte is not UI. 
				// This is a bad packet as no protocol in 8/16 bit mode 
				// is equal to ALL_STATIONS. Discard packet.

				DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - RX Packet Address=ALL_STATIONS(FF) but Control=%x (should be UI(03))\n"), fr_p[1]));
				break;
			}
		}

		// Save pointer to protocol field
		msg_p->pPPPFrame = fr_p;
		msg_p->cbPPPFrame = len;

		// Decode Protocol Field
		//
		// Protocol field is variable length. Least significant bit = 1 terminates the protocol field
		//

		while (TRUE)
		{
			if ((len == 0) || (protocol & 0xFF00))
			{
				// Length is insufficient to decode protocol, or protocol field too long
				DEBUGMSG(ZONE_WARN, (TEXT("PPP: WARNING - RX packet has missing/malformed protocol field\n")));
				protocol = 0;
				break;
			}
			protocol = (protocol << 8) | fr_p[0];
			fr_p += 1;
			len  -= 1;
			// byte with lsb set terminates protocol field
			if (protocol & 0x01)
			{
				// advance data pointer to byte that follows protocol type
				msg_p->data = fr_p;
				msg_p->len = len;
				break;
			}
		}

	} while (FALSE); // end do

	// If we were successful in decoding the header, then strip it from
	// the packet.


	msg_p->ProtocolType = protocol;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -PPPDecodeProtocol: protocol=%04X\n"), protocol));

    return protocol;
}

/*****************************************************************************
* 
*   @func   BOOL | IPConvertStringToAddress | Convert IP string to address
*
*   @rdesc  BOOL
*   
*   @parm   WCHAR   | AddressString  | address string format: X.X.X.X
*   @parm   DWORD * | AddressValue   | address string value
*               
*   @comm  This function converts the IP decimal dot notation to a value.
*/

BOOL

IPConvertStringToAddress( WCHAR *AddressString, DWORD *AddressValue )
{
	WCHAR   *pStr = AddressString;
	PUCHAR  AddressPtr = (PUCHAR)AddressValue;
	int     i;
	int     Value;

    // Parse the four pieces of the address.

    for( i=0; *pStr && (i < 4); i++ ) 
    {
        Value = 0;

        while (*pStr && TEXT('.') != *pStr) 
        {
            if ((*pStr < TEXT('0')) || (*pStr > TEXT('9'))) 
            {
                DEBUGMSG (ZONE_ERROR, (
                TEXT("Unable to convert %s to address\r\n"), AddressString));
                return( FALSE );
            }
            Value *= 10;
            Value += *pStr - TEXT('0');
            pStr++;
        }
        if( Value > 255 ) 
        {
            DEBUGMSG (ZONE_ERROR, (
            TEXT("Unable to convert %s to address\r\n"), AddressString));
            return( FALSE );
        }
        AddressPtr[i] = Value;

        if( TEXT('.') == *pStr ) 
        {
            pStr++;
        }
    }

    // Did we get all of the pieces?

    if (i != 4) 
    {
        DEBUGMSG (ZONE_ERROR, (
        TEXT( "ERR: Unable to convert %s to address\r\n"), AddressString ));
        return( FALSE );
    }

    // Fix the Address

    *AddressValue = net_long( *AddressValue );
    
    DEBUGMSG (ZONE_INIT, (
    TEXT(" Converted %s to address %X\r\n"), AddressString, *AddressValue ));

    return( TRUE );
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
//  Create a (hopefully unique) device ID from system DEVICEID info.
//
{
	DWORD      OemInfo[128]; // DEVICE_ID is DWORD-aligned
    DWORD      dwSize = sizeof(OemInfo);
    PDEVICE_ID pDeviceId = (PDEVICE_ID)&OemInfo[0];
	DWORD      bytesToCopy;
	BOOL       bGotDeviceID;
    MD5_CTX    md5ctx;

    memset(OemInfo, 0, sizeof(OemInfo));
	pDeviceId->dwSize = sizeof(OemInfo);
    if (KernelIoControl(IOCTL_HAL_GET_DEVICEID,NULL, 0, OemInfo, dwSize, &dwSize))
    {
		MD5Init(&md5ctx);
        MD5Update(&md5ctx, (PUCHAR)pDeviceId + pDeviceId->dwPresetIDOffset, pDeviceId->dwPresetIDBytes);
        MD5Update(&md5ctx, (PUCHAR)pDeviceId + pDeviceId->dwPlatformIDOffset, pDeviceId->dwPlatformIDBytes);
		MD5Final(&md5ctx);

		bytesToCopy = *pcbID;
		if (bytesToCopy > MD5DIGESTLEN)
			bytesToCopy = MD5DIGESTLEN;

		memcpy(pID, md5ctx.digest, bytesToCopy);

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


