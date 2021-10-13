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
*   @module ppp.c | Point to Point Protocol (PPP)
*
*/


//  Include Files

#include "windows.h"
#include "cclib.h"
#include "types.h"
#include "netui.h"

#include "cxport.h"
#include "crypt.h"
#include "memory.h"

// VJ Compression Include Files

#include "ndis.h"
#include "tcpip.h"
#include "vjcomp.h"

//  PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "auth.h"
#include "lcp.h"
#include "ipcp.h"
#include "ncp.h"
#include "mac.h"
#include "raserror.h"

#include "ip_intf.h"


//  Externs


//  Defines

//  Globals


PPP_CONTEXT        *pppContextList;
CRITICAL_SECTION    v_ListCS;
HANDLE              v_hInstDLL;

//  Local Types

//  Local Functions

static  DWORD   WINAPI
ConnectionThread( LPVOID arg_p );

static  DWORD
pppConnectionDial( pppSession_t *s_p );

static  void
WaitForConnectionCompletion( pppSession_t *);

PPP_CONTEXT *
pppContextNew(void)
//
//	Create a new PPP context structure and add it to the global list.
{
	PPP_CONTEXT *pContext;

    DEBUGMSG( ZONE_PPP | ZONE_FUNCTION, (TEXT( "PPP: pppContextNew\n" )));

    // Allocate a new IP adapter context
    pContext = ( PPP_CONTEXT * )CTEAllocMem( sizeof( PPP_CONTEXT ) );
	if (pContext != NULL)
	{
		pContext->IPContext = NULL;
		pContext->IPV6Context = NULL;
		pContext->Session	= NULL;
		pContext->fOpen		= FALSE;
		pContext->ifinst	= INVALID_ENTITY_INSTANCE;

		EnterCriticalSection (&v_ListCS);

		// Add the new context to the list of PPP contexts

		pContext->Next = pppContextList;
		pppContextList = pContext;                  // Insert at the head.

		LeaveCriticalSection (&v_ListCS);
	}
    DEBUGMSG( ZONE_PPP | ZONE_FUNCTION, (TEXT( "PPP:-pppContextNew, result=%x\n" ), pContext));
	return pContext;
}

void
pppContextFree(
	IN  PPP_CONTEXT *pContext)
//
//	Free the memory allocated for a PPP_CONTEXT.
//
//  The context MUST NOT be in the pppContextList.
//
{
	CTEFreeMem(pContext);
}

BOOL
pppContextRemove(
	PPP_CONTEXT *pContext)
//
//	Remove a PPP context structure from the global pppContextList.
//
//  Return TRUE if it was found and removed, FALSE otherwise.
//
{
	BOOL        bFound = FALSE;
	PPP_CONTEXT **ppContext;

	EnterCriticalSection (&v_ListCS);

	// Find the context in the global list
	for (ppContext = &pppContextList; *ppContext; ppContext = &(*ppContext)->Next)
	{
		if (*ppContext == pContext)
		{
			// Found it, now remove it
			*ppContext = pContext->Next;
			DEBUGMSG (ZONE_PPP, (TEXT("PPP: Context: 0x%x removed..\r\n"), pContext));
			bFound = TRUE;
			break;
		}
	}

	LeaveCriticalSection (&v_ListCS);
	return bFound;
}

DWORD
ReadRegistryValues(
	IN	HKEY	 hKey,
	IN	TCHAR	*tszSubKeyName,	OPTIONAL
	...)
//
//	Read in a number of registry values.
//
//	For each value to be read in, the following parameters must be passed:
//
//		TCHAR  *tszValueName
//		DWORD	valueType
//		DWORD	dwFlags
//		void   *pValue
//		DWORD	cbValue		// size of space pointed to by pValue
//
//	The list is terminated with a NULL tszValueName
//
//	Returns the count of the number of values that were read in successfully.
//
{
	LONG	lResult;
	DWORD	dwNumRead = 0,
			dwRegistryType,
			cbRegistryValue,
			dwFlags,
			cbValue,
			*pcbValue,
			dwRequiredType;
	void	*pValue,
			**ppValue;
	va_list	argptr;
	PTCHAR	tszValueName;

	//
	//	If a subkeyname specified, open that key and it becomes the new hKey.
	//
	if (tszSubKeyName)
	{
		lResult = RegOpenKeyEx(hKey, tszSubKeyName, 0, KEY_READ, &hKey);
		if (lResult != ERROR_SUCCESS)
		{
			return dwNumRead;
		}
	}

	va_start(argptr, tszSubKeyName);
	while (TRUE)
	{
		tszValueName = va_arg(argptr, PTCHAR);
		if (tszValueName == NULL)
			break;

		dwRequiredType	= va_arg(argptr, DWORD);
		dwFlags			= va_arg(argptr, DWORD);
		ppValue			= NULL;
		pcbValue		= NULL;

		pcbValue = NULL;
		if (dwFlags & REG_ALLOC_MEMORY)
		{
			//
			//	We won't know the size of memory to allocate and read on the first
			//	try, so the first call to RegQueryValueEx will be just to find out
			//	the size.
			//
			pValue = NULL;
			cbValue = 0;
			ppValue			= va_arg(argptr, void **);
			pcbValue		= va_arg(argptr, PDWORD);
		}
		else
		{
			pValue			= va_arg(argptr, void *);

			if (dwRequiredType == REG_BINARY)
			{
				pcbValue = va_arg(argptr, PDWORD);
				cbValue = *pcbValue;
			}
			else
				cbValue			= va_arg(argptr, DWORD);
		}

		//
		//	2 passes used to read stuff from registry:
		//		1st pass: Find out type and size info
		//		2nd pass: Read it in if ok
		//	Problem: type and size could change between the 2 passess...
		//
		//	Find out the type and size of the registry value
		//
		lResult = RegQueryValueEx(hKey, tszValueName, NULL, &dwRegistryType, NULL, &cbRegistryValue);
		if (lResult == ERROR_SUCCESS || (lResult == ERROR_MORE_DATA))
		{
			if (dwRegistryType == dwRequiredType)
			{
				if (dwFlags & REG_ALLOC_MEMORY)
				{
					//
					//	Allocate the memory to hold the object now that we know the size
					//
					pValue = pppAllocateMemory(cbRegistryValue);
				}
				else
				{
					//
					// Make sure that the size of the registry object is not greater than
					// the space provided.
					//
					if (cbRegistryValue > cbValue)
					{
						pValue = NULL;
					}
				}

				if (pValue)
				{
					//
					//	Read in the value data now that we have the memory for it
					//
					lResult = RegQueryValueEx(hKey, tszValueName, NULL, NULL, pValue, &cbValue);
					if (lResult == ERROR_SUCCESS)
					{
						dwNumRead++;
						if (dwRequiredType == REG_BINARY)
							*pcbValue = cbValue;
					}
					else
					{
						//
						// Theoretically we should have an iteration, because the data size
						// could grow between the two calls to ReqQueryValueEx.  At this time
						// we take the simple approach.
						//

						if (dwFlags & REG_ALLOC_MEMORY)
						{
							pppFreeMemory(pValue, cbRegistryValue);
							pValue = NULL;
							cbValue = 0;
						}
					}
				}
			}
			else
			{
				// Wrong data type
			}
		}

		if (dwFlags & REG_ALLOC_MEMORY)
		{
			if (ppValue)
				*ppValue = pValue;
			if (pcbValue)
				*pcbValue = cbValue;
		}

	}
	va_end(argptr);

	if (tszSubKeyName)
		RegCloseKey(hKey);

	return dwNumRead;
}

DWORD
WriteRegistryValues(
	IN	HKEY	 hKey,
	IN	TCHAR	*tszSubKeyName,	OPTIONAL
	...)
//
//	Write out a number of registry values.
//
//	For each value to be written, the following parameters must be passed:
//
//		TCHAR  *tszValueName
//		DWORD	valueType
//		DWORD	dwFlags
//		void   *pValue
//		DWORD	cbValue		// size of space pointed to by pValue
//
//	The list is terminated with a NULL tszValueName
//
//	Returns the count of the number of values that were written out successfully.
//
{
	LONG	lResult;
	DWORD	dwNumWritten = 0,
			dwFlags,
			cbValue,
			dwValueType,
			dwDisposition;
	void	*pValue;
	va_list	argptr;
	PTCHAR	tszValueName;

	//
	//	If a subkeyname specified, create/open that key and it becomes the new hKey.
	//
	if (tszSubKeyName)
	{
		lResult = RegCreateKeyEx(hKey, tszSubKeyName, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
		if (lResult != ERROR_SUCCESS)
		{
			return dwNumWritten;
		}
	}

	va_start(argptr, tszSubKeyName);
	while (TRUE)
	{
		tszValueName = va_arg(argptr, PTCHAR);
		if (tszValueName == NULL)
			break;

		dwValueType		= va_arg(argptr, DWORD);
		dwFlags			= va_arg(argptr, DWORD);
		pValue			= va_arg(argptr, void *);
		cbValue			= va_arg(argptr, DWORD);

		lResult = RegSetValueEx(hKey, tszValueName, 0, dwValueType, pValue, cbValue);
		if (lResult == ERROR_SUCCESS)
		{
			dwNumWritten++;
		}
		else
		{
			DEBUGMSG(ZONE_ERROR, (TEXT("PPP: ERROR - Unable to write %s to registry\n"), tszValueName));
		}
	}
	va_end(argptr);

	if (tszSubKeyName)
		RegCloseKey(hKey);

	return dwNumWritten;
}

LONG
DeleteRegistryKeyAndContents(
	IN	HKEY	 hKey,
	IN	TCHAR	*tszSubKeyName)
{
	LONG	lResult;

#if 0
	HKEY		hSubKey;

	//
	//	If a subkeyname specified, create/open that key and it becomes the new hKey.
	//
	if (tszSubKeyName)
	{
		lResult = RegOpenKeyEx(hKey, tszSubKeyName, 0, KEY_READ, &hSubKey);
		if (lResult != ERROR_SUCCESS)
		{
			return lResult;
		}

		//
		//	Iterate through all the subkeys of the key and delete them.
		//	A key cannot be deleted until all its subkeys are gone
		//
		RegCloseKey(hSubKey);

	}

#endif

	//	Delete the key

	lResult = RegDeleteKey(hKey, tszSubKeyName);

	return lResult;
}

void
FreePppSession(
	pppSession_t    *s_p)
//
//	Free the resources of the specified session.
//
{
	if (s_p)
	{
		if (s_p->hOpenThread)
			CloseHandle(s_p->hOpenThread);

		//
		// Don't free device config info for a server session.
		//
		if (s_p->lpbDevConfig && !s_p->bIsServer)  LocalFree(s_p->lpbDevConfig);

        if( s_p->ncpCntxt     ) pppNcp_InstanceDelete ( s_p->ncpCntxt  );
        if( s_p->authCntxt    ) pppAuth_InstanceDelete( s_p->authCntxt );
        if( s_p->lcpCntxt     ) pppLcp_InstanceDelete ( s_p->lcpCntxt  );
        if( s_p->macCntxt     ) pppMac_InstanceDelete ( s_p->macCntxt  );
        if( s_p->StateEvent   ) CloseHandle( s_p->StateEvent );
        if( s_p->UserEvent    ) CloseHandle( s_p->UserEvent );

        DeleteCriticalSection(&s_p->SesCritSec);
        DeleteCriticalSection(&s_p->RasCritSec);

		pppFree(s_p, s_p->pRegisteredProtocolTable, L"PROTTAB");
		pppFree( s_p, s_p, TEXT( "PPP SESSION" ) );
	}
}

VOID
pppComputeAuthTypesAllowed(
	pppSession_t    *s_p)
//
//	Examine the RASENTRY.dwfOptions and compute the
//	bitmask of authentication types to allow on this
//	RAS connection.
//
{
	DWORD	dwfOptions,
			bmAuthTypesAllowed;

	// VPNs can be globally forced to use encryption

	if (s_p->dwForceVpnEncryption
	&&	(_tcscmp(s_p->rasEntry.szDeviceType, RASDT_Vpn) == 0))
	{
		DEBUGMSG( ZONE_PPP, ( TEXT( "PPP: LCP-Forcing VPN connection to use Data Encryption\n" )));
		s_p->rasEntry.dwfOptions|= RASEO_RequireMsEncryptedPw | RASEO_RequireDataEncryption;
	}

	//
	//	Compute the set of authentication types that are allowed.
	//
	dwfOptions = s_p->rasEntry.dwfOptions;

	bmAuthTypesAllowed = AUTH_MASK_PAP
					   | AUTH_MASK_CHAP_MD5
					   | AUTH_MASK_CHAP_MS
					   | AUTH_MASK_CHAP_MSV2
					   | AUTH_MASK_EAP;

	if (dwfOptions & RASEO_RequireEncryptedPw)
	{
		bmAuthTypesAllowed &= ~AUTH_MASK_PAP;
	}

	if ((dwfOptions & RASEO_RequireMsEncryptedPw)
	||  (dwfOptions & RASEO_RequireDataEncryption))
	{
		bmAuthTypesAllowed &= ~(AUTH_MASK_PAP | AUTH_MASK_CHAP_MD5);
	}
	if (dwfOptions & RASEO_ProhibitPAP)
	{
		bmAuthTypesAllowed &= ~AUTH_MASK_PAP;
	}
	if (dwfOptions & RASEO_ProhibitCHAP)
	{
		bmAuthTypesAllowed &= ~AUTH_MASK_CHAP_MD5;
	}
	if (dwfOptions & RASEO_ProhibitMsCHAP)
	{
		bmAuthTypesAllowed &= ~AUTH_MASK_CHAP_MS;
	}
	if (dwfOptions & RASEO_ProhibitMsCHAP2)
	{
		bmAuthTypesAllowed &= ~AUTH_MASK_CHAP_MSV2;
	}
	if ((dwfOptions & RASEO_ProhibitEAP)
	||  (s_p->rasEntry.dwCustomAuthKey == 0)
	||  (g_hEapDll == NULL))
	{
		bmAuthTypesAllowed &= ~AUTH_MASK_EAP;
	}

	s_p->bmAuthTypesAllowed = bmAuthTypesAllowed;
}


extern BOOL FixRasPassword(IN OUT LPRASDIALPARAMS lpRasDialParams);
/*****************************************************************************
*
*   @func   DWORD | pppSessionNew | Create a new PPP session
*
*   @rdesc  DWORD
*
*   @parm   pppStart_t | session_p | Start structure
*
*   @comm   This function allocates and initializes all internal data and
*           structures for a new PPP session.
*/

DWORD
pppSessionNew( pppStart_t *start_p )
{
	pppSession_t    *s_p = NULL;                                   // session pointer
	HANDLE          hHeap;
	DWORD           RetVal;
	PWCHAR          pwch;

    DEBUGMSG( ZONE_PPP | ZONE_FUNCTION, (TEXT( "PPP: +pppSessionNew\r\n" )));

    CTEInitialize();                                    // Ensure CTE is up

	//
	// Set the adapter name to the device name, replacing illegal registry key
	// name chars with '-'.
	//
	wcscpy(start_p->context->AdapterName, start_p->rasEntry->szDeviceName);
	for (pwch = start_p->context->AdapterName; *pwch != L'\0'; pwch++)
	{
		if (*pwch == L'\\')
		{
			*pwch = L'-';
		}
	}

    // Create a Heap for PPP

    hHeap = HeapCreate( 0, 100*1024, 200*1024 );        // 100K, max 200k

    if( NULL == hHeap )
    {
        DEBUGMSG( ZONE_ERROR | ZONE_PPP, ( TEXT( "PPP: ERROR: HeapCreate failed\r\n" )));
        RetVal = ERROR_NOT_ENOUGH_MEMORY;
    }
	else
	{
		// Allocate the session's context

		s_p = HeapAlloc( hHeap, 0, sizeof( pppSession_t ) );
		if( s_p == NULL )
		{
			DEBUGMSG( ZONE_PPP | ZONE_ERROR, (TEXT( "PPP: ERROR: HeapAlloc failed\r\n" )));
			RetVal = ERROR_NOT_ENOUGH_MEMORY;
		}
		else
		{
			DEBUGMSG( ZONE_PPP,
					  (TEXT( "HeapAlloc:Type: PPP CONTEXT  Length: %d\r\n" ),
					   sizeof( pppSession_t )));

			// At this point we have a valid heap and
			// memory allocated for this PPP session.

			memset( s_p, 0, sizeof( pppSession_t ) );       // init to zero
			s_p->hHeap		= hHeap;                        // save heap handle
			s_p->MemUsage	= sizeof( pppSession_t );
			s_p->RefCnt		= 1;							// add initial reference
#ifdef DEBUG
			s_p->TrackedRefCnt[REF_SESSIONNEW] = 1;
#endif
			s_p->bInitialRefCntDeleted			= FALSE;

			s_p->lpbDevConfig = start_p->lpbDevConfig;
			s_p->dwDevConfigSize = start_p->dwDevConfigSize;

			s_p->bIsServer = start_p->bIsServer;

			// Prevent the caller from freeing the lpbDevConfig, since
			// it is needed as long as the session is active.
			// The lpbDevConfig will be freed by FreePppSession when
			// pppDelRef reduces the refcount to 0.

			start_p->lpbDevConfig = NULL;

			// Initialize Session Elements

			InitializeCriticalSection( &s_p->SesCritSec );
			InitializeCriticalSection( &s_p->RasCritSec );

			CTEInitTimer(&s_p->dhcpTimer);
			s_p->dwMaxDhcpInformTries = 3;
			s_p->dwDhcpTimeoutMs = 250;

			// Set default configurable parameter values

			s_p->dwMaxCRRetries				= PPP_MAX_CONFIGURE;
			s_p->dwMaxTRRetries				= PPP_MAX_TERMINATE;
			s_p->dwReqTimeoutMs				= PPP_RESTART_TIMER;
			s_p->dwMaxCRFails				= PPP_MAX_FAILURE;
			s_p->dwAckDupCR					= TRUE;
			s_p->bmCryptTypes				= 0xFFFFFFFF;		// Support any CCP data encryption types by default
			s_p->dwForceVpnEncryption		= FALSE;			// Do not force  VPN connections to use data encryption by default
			s_p->dwForceVpnDefaultRoute		= TRUE;				// Force VPN connections to become the default route by default
			s_p->dwAlwaysAddSubnetRoute		= FALSE;			// Only add subnet route if not default route by default
			s_p->dwAlwaysAddDefaultRoute	= TRUE;			    // For BC with earlier versions of CE, since some apps expect a 0.0.0.0 route will get created
			s_p->bDisableMsChapLmResponse	= FALSE;			// Default to providing both Lm and Nt MSCHAP responses
			s_p->bDisableMsChapNtResponse	= FALSE;
			s_p->dwAllowAutoSuspendWhileSessionActive	= FALSE;
			s_p->dwAlwaysSuggestIpAddr		= FALSE;
			s_p->dwAlwaysRequestDNSandWINS	= FALSE;
			s_p->dwAuthMaxTries				= PPP_AUTH_DEFAULT_MAX_TRIES;
			s_p->dwAuthMaxFailures			= PPP_AUTH_MAX_RESPONSE_FAILURES;
			s_p->dwMinimumMRU			    = LCP_DEFAULT_MINIMUM_MRU;

			// Read configurable parameter override values from registry

			(void) ReadRegistryValues(HKEY_LOCAL_MACHINE, TEXT("Comm\\ppp\\Parms"),
										RAS_VALUENAME_MAXCONFIGURE,			REG_DWORD, 0, &s_p->dwMaxCRRetries,			    sizeof(s_p->dwMaxCRRetries),
										RAS_VALUENAME_MAXTERMINATE,			REG_DWORD, 0, &s_p->dwMaxTRRetries,			    sizeof(s_p->dwMaxTRRetries),
										RAS_VALUENAME_MAXFAILURE,			REG_DWORD, 0, &s_p->dwMaxCRFails,				sizeof(s_p->dwMaxCRFails),
										RAS_VALUENAME_RESTARTTIMER,			REG_DWORD, 0, &s_p->dwReqTimeoutMs,			    sizeof(s_p->dwReqTimeoutMs),
										RAS_VALUENAME_ACKDUPCR,				REG_DWORD, 0, &s_p->dwAckDupCR,				    sizeof(s_p->dwAckDupCR),
										RAS_VALUENAME_ALLOWSUSPEND,			REG_DWORD, 0, &s_p->dwAllowAutoSuspendWhileSessionActive, sizeof(s_p->dwAllowAutoSuspendWhileSessionActive),
										RAS_VALUENAME_ALWAYSSUGGESTIPADDR,  REG_DWORD, 0, &s_p->dwAlwaysSuggestIpAddr,      sizeof(s_p->dwAlwaysSuggestIpAddr),
										RAS_VALUENAME_ALWAYSREQUESTDNS,     REG_DWORD, 0, &s_p->dwAlwaysRequestDNSandWINS,  sizeof(s_p->dwAlwaysRequestDNSandWINS),
										RAS_VALUENAME_CRYPTTYPES,			REG_DWORD, 0, &s_p->bmCryptTypes,				sizeof(s_p->bmCryptTypes),
										RAS_VALUENAME_FORCEVPNCRYPT,		REG_DWORD, 0, &s_p->dwForceVpnEncryption,		sizeof(s_p->dwForceVpnEncryption),
										RAS_VALUENAME_FORCEVPNDEFAULTROUTE,	REG_DWORD, 0, &s_p->dwForceVpnDefaultRoute,		sizeof(s_p->dwForceVpnDefaultRoute),
										RAS_VALUENAME_ALWAYSADDSUBNETROUTE,	REG_DWORD, 0, &s_p->dwAlwaysAddSubnetRoute,		sizeof(s_p->dwAlwaysAddSubnetRoute),
										RAS_VALUENAME_ALWAYSADDDEFAULTROUTE,REG_DWORD, 0, &s_p->dwAlwaysAddDefaultRoute,	sizeof(s_p->dwAlwaysAddDefaultRoute),
									RAS_VALUENAME_DISABLEMSCHAPLMRESPONSE,	REG_DWORD, 0, &s_p->bDisableMsChapLmResponse,	sizeof(s_p->bDisableMsChapLmResponse),
									RAS_VALUENAME_DISABLEMSCHAPNTRESPONSE,  REG_DWORD, 0, &s_p->bDisableMsChapNtResponse,	sizeof(s_p->bDisableMsChapNtResponse),
										RAS_VALUENAME_AUTHMAXTRIES,			REG_DWORD, 0, &s_p->dwAuthMaxTries,				sizeof(s_p->dwAuthMaxTries),
										RAS_VALUENAME_AUTHMAXFAILURES,		REG_DWORD, 0, &s_p->dwAuthMaxFailures,			sizeof(s_p->dwAuthMaxFailures),
										RAS_VALUENAME_DHCPMAXTRIES,			REG_DWORD, 0, &s_p->dwMaxDhcpInformTries,		sizeof(s_p->dwMaxDhcpInformTries),
										RAS_VALUENAME_DHCPTIMEOUTMS,		REG_DWORD, 0, &s_p->dwDhcpTimeoutMs,			sizeof(s_p->dwDhcpTimeoutMs),
										RAS_VALUENAME_MINIMUM_MRU,		    REG_DWORD, 0, &s_p->dwMinimumMRU,			    sizeof(s_p->dwMinimumMRU),
										NULL);

			s_p->dwReqTimeoutMs *= 1000;

			//
			// It is not allowable to disable both Lm and Nt MSCHAP auth responses
			//
			if (s_p->bDisableMsChapLmResponse && s_p->bDisableMsChapNtResponse)
			{
				DEBUGMSG(ZONE_INIT || ZONE_ERROR, (TEXT("PPP: ERROR: Disabling both MSCHAP Lm and NT responses not allowed - reenabling\n")));
				s_p->bDisableMsChapLmResponse	= FALSE;
				s_p->bDisableMsChapNtResponse	= FALSE;
			}

			DEBUGMSG(ZONE_PPP | ZONE_INIT, (TEXT("PPP: MaxCRRetries = %u\n"), s_p->dwMaxCRRetries));
			DEBUGMSG(ZONE_PPP | ZONE_INIT, (TEXT("PPP: MaxTRRetries = %u\n"), s_p->dwMaxTRRetries));
			DEBUGMSG(ZONE_PPP | ZONE_INIT, (TEXT("PPP: MaxCRFails   = %u\n"), s_p->dwMaxCRFails));
			DEBUGMSG(ZONE_PPP | ZONE_INIT, (TEXT("PPP: ReqTimeoutMs = %u\n"), s_p->dwReqTimeoutMs));
			DEBUGMSG(ZONE_PPP | ZONE_INIT, (TEXT("PPP: AckDupCR     = %u\n"), s_p->dwAckDupCR));
			DEBUGMSG(ZONE_PPP | ZONE_INIT, (TEXT("PPP: CryptTypes   = %x\n"), s_p->bmCryptTypes));
			DEBUGMSG(ZONE_PPP | ZONE_INIT, (TEXT("PPP: DisableLmAuth= %u\n"), s_p->bDisableMsChapLmResponse));
			DEBUGMSG(ZONE_PPP | ZONE_INIT, (TEXT("PPP: DisableNtAuth= %u\n"), s_p->bDisableMsChapNtResponse));

			s_p->pPendingSessionCloseCompleteList = NULL;

			// Create Connection and User Prompt Events
			s_p->StateEvent = CreateEvent( 0, FALSE, FALSE, 0 );
			s_p->UserEvent = CreateEvent( 0, FALSE, FALSE, 0 );
			if (s_p->StateEvent == NULL || s_p->UserEvent == NULL)
			{
				DEBUGMSG( ZONE_ERROR | ZONE_PPP, (TEXT( "PPP: ERROR:CreateEvent failed %u\r\n" ), GetLastError()) );
				RetVal = ERROR_EVENT_INVALID;
			}
			else
			{
				// Convert from the public version to the smaller private version

				RaspConvPublicPriv( start_p->rasEntry, &s_p->rasEntry );

				pppComputeAuthTypesAllowed(s_p);

				// Force VPN to become default gateway as appropriate
				if (s_p->dwForceVpnDefaultRoute
				&&	(0 == _tcscmp(s_p->rasEntry.szDeviceType, RASDT_Vpn)))
				{
					s_p->rasEntry.dwfOptions |= RASEO_RemoteDefaultGateway;
				}

				// Set SLIP Mode if requested

				if (s_p->rasEntry.dwFramingProtocol == RASFP_Slip )
				{
					DEBUGMSG( ZONE_RAS, ( TEXT( "PPP: MODE=SLIP\n" )));

					s_p->Mode = PPPMODE_SLIP;
					s_p->RxPacketHandler = SLIPRxPacketHandler;
				}
				else
				{
					DEBUGMSG( ZONE_RAS, ( TEXT( "PPP: MODE=PPP\n" )));

					s_p->Mode = PPPMODE_PPP;
					s_p->RxPacketHandler = PPPRxPacketHandler;
				}
	
				// Save the PPP context and set our session context for caller.
				// IP will use the PPP context when calling into this session. We
				// will use this context to access this session via the pppContextList.
				// Save the RAS notifier interface.

				s_p->context      = start_p->context;           // IP's session context
				s_p->notifierType = start_p->notifierType;
				s_p->notifier     = (LPVOID (*)())start_p->notifier;
				s_p->RasConnState = RASCS_OpenPort;             // initial state:

				if (start_p->rasDialParams)
				{
					CTEMemCopy( &s_p->rasDialParams,                // save RAS dial parameters
								start_p->rasDialParams,
								sizeof( RASDIALPARAMS ) );

					if (!FixRasPassword(&s_p->rasDialParams))
					{
                		DEBUGMSG( ZONE_PPP,
						  (TEXT( "pppSessionNew: Could not get real RAS Password\r\n" )));

					}
				}
				//
				// Create Default Protocol Layer Instances for MAC, LCP, Authentication, and Network
				//
				do
				{
					RetVal = pppMac_InstanceCreate(s_p, &(s_p->macCntxt), s_p->rasEntry.szDeviceName, s_p->rasEntry.szDeviceType);
					if (SUCCESS != RetVal)
						break;

					RetVal = pppLcp_InstanceCreate(s_p, &(s_p->lcpCntxt));
					if (SUCCESS != RetVal)
						break;

					RetVal = pppAuth_InstanceCreate(s_p, &(s_p->authCntxt));
					if (SUCCESS != RetVal)
						break;

					RetVal = pppNcp_InstanceCreate( s_p, &(s_p->ncpCntxt));
					if (SUCCESS != RetVal)
						break;

				} while (FALSE);
			}
		}
	}

	if (RetVal == SUCCESS)
	{
		// Save this session's context in the PPP_CONTEXT record.
		// Note that this exposes it to apps via RasEnumConnections

		start_p->session = s_p;              // return session handle
		start_p->context->Session = s_p;
        start_p->context->fOpen = TRUE;

		// Turn off auto-suspend (if desired) while we have an active session

		if (!s_p->dwAllowAutoSuspendWhileSessionActive)
			CTEIOControl(CTE_IOCTL_SET_IDLE_TIMER_RESET, NULL, 0, NULL, 0, NULL);
	}
	else
	{
		// Remove this session and context from the pppContext list
		pppContextRemove(start_p->context);
		pppContextFree(start_p->context);
		start_p->context = NULL;

		FreePppSession(s_p);
		if (hHeap)
			HeapDestroy( hHeap );
	}

    DEBUGMSG( ZONE_PPP | ZONE_FUNCTION, (TEXT( "PPP: -pppSessionNew result=%d\n"), RetVal));
	return RetVal;
}

/*****************************************************************************
*
*   @func   DWORD | pppSessionRun | Run an active PPP session
*
*   @rdesc  DWORD
*
*   @parm   pppSession_t | s_p | Session pointer
*
*   @comm   This function starts an active PPP session up.
*/

DWORD
pppSessionRun( pppSession_t *s_p )
{
	DWORD	dwResult;

    DEBUGMSG( ZONE_PPP | ZONE_FUNCTION, (TEXT( "pppSessionRun\r\n" )));

    // Start a thread to handle the initial startup

    s_p->startHandle = CreateThread(NULL, 0, ConnectionThread, s_p, 0, NULL);
    if( s_p->startHandle == NULL )
    {
        DEBUGMSG( ZONE_ERROR | ZONE_PPP, (TEXT( "PPP: ERROR - CreateThread failed %u\r\n"), GetLastError() ) );
        return GetLastError();            // indicate failure
    }

    // Close the handle now as we do not need it

    CloseHandle( s_p->startHandle );

    // If a notifier function was provided then return and allow the
    // ConnectionThread thread to handle the remaining startup. If the notifier
    // is NULL we wait here for the connection's status and indicate this
    // result to the caller.

	dwResult = SUCCESS;

    if( s_p->notifier == NULL)
	{
		// wait for RASCS_Connected or RASCS_Disconnected state
        WaitForConnectionCompletion(s_p);

        switch( s_p->RasConnState )
		{
        case RASCS_Connected:
            break;

        case RASCS_Disconnected:
            dwResult = s_p->RasError;					// indicate failure
			if (dwResult == SUCCESS)
			{
				// This means that RasHangup was called, i.e. a user intiated disconnect.
				dwResult = ERROR_USER_DISCONNECTION;
			}
			break;

        default:
            ASSERT(FALSE);
			dwResult = ERROR_UNKNOWN;
            break;
        }
    }

	DEBUGMSG( (ZONE_ERROR && dwResult) || ZONE_FUNCTION, (TEXT( "PPP: -pppSessionRun result=%d\n"), dwResult));
    return dwResult;
}

/*****************************************************************************
*
*   @func   DWORD | ConnectionThread | Thread to initiate the PPP connection.
*
*   @parm   LPVOID * | arg_p | PPP session handle.
*
*   @comm   This function actually initiates the exchange of PPP packets
*           to establish the link.
*/

static  DWORD   WINAPI
ConnectionThread( LPVOID arg_p )
{
	pppSession_t    *s_p = (pppSession_t *)arg_p;
	DWORD       rc;

    DEBUGMSG( ZONE_PPP | ZONE_FUNCTION, (TEXT( "ppp:ConnectionThread\n" )));

	//
	// If the following ADDREF fails, then our creator, pppSessionRun, must
	// have exited and so we don't need to do anything.
	//
    if (PPPADDREF( s_p, REF_CONNECTIONTHREAD ))
	{
		// Attempt to connect. If the connection fails, update
		// the state while still holding the PPP lock. ie tie
		// the operation to a state transition.

		pppLock( s_p );

		rc = pppConnectionDial( s_p );

		if ( rc != SUCCESS )
		{
			// If the connection failed do not try to open the MAC
			pppChangeOfState( s_p, RASCS_Disconnected, s_p->HangUpReq ? 0 : rc );
		}
		else // ( rc == SUCCESS )
		{
			// Start PPP by opening the MAC and LCP layers if no "HangUpReq"
			// is pending.  If one is pending then unlock and exit.

			if( ! s_p->HangUpReq )      // or state must be DeviceConnected
			{
				PppHandleLineConnectedEvent(s_p);
			}
			// Save the start connect time.
			s_p->dwStartTickCount = GetTickCount();
		}
		pppUnLock( s_p );

		// Done with the session pointer

		PPPDELREF( s_p, REF_CONNECTIONTHREAD );   
	}

	DEBUGMSG( ZONE_PPP | ZONE_FUNCTION, (TEXT( "ppp:ConnectionThread EXITING\n" )));

    return 0;                                    // Thread exits here
}

/*****************************************************************************
*
*   @func   void | pppChangeOfState | Notify PPP Session of Connection Events
*
*   @parm   void * | session | PPP session handle.
*
*   @comm   This function handles state changes indicated by protocol layers
*           to the PPP Session Manager and ultimately to RAS.
*/

void
pppChangeOfState( pppSession_t *s_p, RASCONNSTATE State, DWORD Info )
{
	BOOL	bStateChanged = FALSE;

    DEBUGMSG( ZONE_PPP | ZONE_FUNCTION, (TEXT( "pppChangeOfState->: %d\r\n" ), State ));

    rasLock( s_p );

	if (s_p->RasConnState != State || State == RASCS_OpenPort)
	{
		// New state is different from current state.
		// RASCS_OpenPort is special since it is the initial state
		// and we want to indicate that first state change as well.
		// Update the session's RAS state and error

        s_p->RasConnState = State;              // set the RAS state
        s_p->RasError     = Info;               // set the error code
		bStateChanged     = TRUE;

#ifdef PPP_LOG_EVENTS
		s_p->eventLog[s_p->eventLogIndex].Event = State;
		s_p->eventLog[s_p->eventLogIndex].TickCount = GetTickCount();
		s_p->eventLogIndex = (s_p->eventLogIndex + 1) % PPP_EVENT_LOG_SIZE;
#endif
    }
    rasUnLock( s_p );

	if (bStateChanged)
	{
		// Indicate changed state only if a notifier function is present

		if(s_p->notifier)
		{
			// Asynchronous Path - indicate COS

			switch( s_p->notifierType )
			{
			case 0:                         // RASDIALFUNC
				// TBD - this call non-functional due to RASDIAL argument
				// TBD - list is setup for the window callback case.
				ASSERT(FALSE);
				//(s_p->notifier)( WM_RASDIALEVENT, State, Info );
				break;

			case 1:                         // RASDIALFUNC1
				//
				//	Internal PPP Server use only
				//
				(s_p->notifier)( s_p, WM_RASDIALEVENT, State, Info, 0 );
				break;

			case 0xffffffff:                // Window
				if (g_pfnPostMessageW)
				{
					DEBUGMSG(ZONE_PPP, (TEXT("PPP: PostMessage: HWND=%X\r\n"), s_p->notifier));
					g_pfnPostMessageW( (HWND )s_p->notifier, WM_RASDIALEVENT, State, Info );
				}
				break;

			default:
				ASSERT(FALSE);
				break;
			}
		}

		// If the state is RASCS_Connected or RASCS_Disconnected, then must
		// set the StateEvent so that a thread that called RasDial will be
		// awakened.

		SetEvent( s_p->StateEvent );
	}
}

/*****************************************************************************
*
*   @func   static void | WaitForConnectionCompletion | Wait for RASCS_Connected or RASCS_Disconnected state
*
*   @parm   pppSession_t * | session  | PPP session handle.
*
*   @comm
*/

static  void
WaitForConnectionCompletion(
	IN pppSession_t   *s_p)
{
	RASCONNSTATE State;

    DEBUGMSG(ZONE_PPP, (TEXT( "->WaitForConnectionCompletion\r\n")));
    
    while (TRUE)
    {
		State = s_p->RasConnState;
		if (RASCS_Connected    == State
		||  RASCS_Disconnected == State)
		{
			break;
		}

        // Wait on the state change event

		DEBUGMSG( ZONE_PPP, ( TEXT( "PPP: Wait for StateEvent\r\n" )) );

        WaitForSingleObject( s_p->StateEvent, INFINITE );

		DEBUGMSG( ZONE_PPP, ( TEXT( "PPP: Got StateEvent change\r\n" )) );
    }

    DEBUGMSG( ZONE_PPP, (TEXT( "<-WaitForConnectionCompletion State = %x\r\n"), State));
}

void
pppSessionLineCloseCompleteCallback(
	void *pData)
//
//	This function is called when the line
//	has been closed while we are
//	in the process of stopping a PPP session.
//
{
	pppSession_t *s_p = (pppSession_t *)pData;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: +pppSessionLineCloseCompleteCallback\n")));

	if (s_p->bInitialRefCntDeleted == FALSE)
	{
		s_p->bInitialRefCntDeleted = TRUE;

		//
		// Delete the initial reference taken in pppSessionNew
		// Note that the caller of pppSessionStop (AfdRasHangup) has taken a reference to the session
		// so the session will not be freed until until AfdRasHangup completes.
		//
		PPPDELREF( s_p, REF_SESSIONNEW );
	}

	pppExecuteCompleteCallbacks(&s_p->pPendingSessionCloseCompleteList);
	
    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: -pppSessionLineCloseCompleteCallback\n")));
}

void
pppSessionLcpCloseCompleteCallback(
	void *pData)
//
//	This function is called when the LCP layer has been brought
//	down as part of stopping a PPP session.
//
{
	pppSession_t *s_p = (pppSession_t *)pData;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: +pppSessionLcpCloseCompleteCallback\n")));

	pppMac_LineClose(s_p->macCntxt, pppSessionLineCloseCompleteCallback, s_p);

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "PPP: -pppSessionLcpCloseCompleteCallback\n")));
}

/*****************************************************************************
*
*   @func   void | pppSessionStop | Stop a PPP session.
*
*   @parm   void * | session | PPP session handle.
*
*   @comm   This function terminates a PPP session.
*/

DWORD
pppSessionStop(
	pppSession_t *s_p,
	void		(*pSessionCloseCompleteCallback)(PVOID),
	PVOID		pCallbackData)
{
	BOOL    bCloseInProgress;
	DWORD   dwResult;

    DEBUGMSG(ZONE_FUNCTION, (TEXT( "->pppSessionStop\r\n" )) );

	pppLock( s_p );

	bCloseInProgress = s_p->pPendingSessionCloseCompleteList != NULL;
	dwResult = pppInsertCompleteCallbackRequest(&s_p->pPendingSessionCloseCompleteList, pSessionCloseCompleteCallback, pCallbackData);
	
	if (ERROR_SUCCESS == dwResult)
	{
		if (!bCloseInProgress)
		{
			// Start the session close operation

			if (0 == wcsncmp(L"L2TP", s_p->rasEntry.szDeviceName, 4)
			&&  RASCS_OpenPort == s_p->RasConnState)
			{
				// Ugly kludge to force L2TP to abort IKE negotiations,
				// since we cannot get L2TP an OID_TAPI_CLOSE request until
				// the OID_TAPI_MAKE_CALL request completes.
				pppMac_Reset(s_p->macCntxt);
			}


			//
			// Stop a connection attempt if one is in progress.
			//
			s_p->HangUpReq = TRUE;
			pppMac_AbortDial(s_p->macCntxt);

			//
			// Begin shutting down the layers.  As the layers are shut down,
			// the appropriate callback will be invoked that will proceed to
			// close the next lower layer.
			//
			if (s_p->Mode == PPPMODE_PPP)
			{
				pppLcp_Close(s_p->lcpCntxt, pppSessionLcpCloseCompleteCallback, s_p);
			}
			else // SLIP
			{
				// No LCP layer to close for SLIP
				pppSessionLcpCloseCompleteCallback(s_p);
			}
		}
	}

	pppUnLock( s_p );

    DEBUGMSG( ZONE_PPP, (TEXT( "<-pppSessionStop\r\n" )) );
    return dwResult;
}

/*****************************************************************************
*
*   @func   void | pppConnectionDial | Establish Connection
*
*   @parm   void * | session | PPP session handle.
*
*   @comm   This function establishes a the MAC connection
*/

static  DWORD
pppConnectionDial( pppSession_t *s_p )
{
	DWORD		 dwResult;
    DWORD        IPAddr;

    DEBUGMSG( ZONE_PPP | ZONE_FUNCTION, (TEXT( "pppConnectionDial\r\n" )));

    pppChangeOfState( s_p, RASCS_OpenPort, 0 );

	// Tell the Miniport to dial...
	dwResult = pppMac_Dial (s_p->macCntxt, &(s_p->rasEntry), &(s_p->rasDialParams));
	if (dwResult != 0)
		return dwResult;
	
    // Prompt user for IP Address if SLIP or CSLIP Mode

    switch( s_p->Mode ) {
    case PPPMODE_PPP:

        // NCP IPCP takes care of this in PPP Mode

        break;

    case PPPMODE_SLIP:
    case PPPMODE_CSLIP:

        // Prompt for IP Address if 0.  If not the address
        // specified in the wizard is used.
        memcpy( &IPAddr, &s_p->rasEntry.ipaddr, sizeof( RASIPADDR ) );

        if( 0 == IPAddr) {
            // Prompt for the IP: it is assigned by the server and
            // probably changes for each connection.

            DEBUGMSG(ZONE_PPP, (TEXT("About to call GetIPAddress\r\n")));

            if( IPAddr = CallGetIPAddress(NULL) ) {
                DEBUGMSG (1, (TEXT("IP Address is 0x%X\r\n"), IPAddr));
                // Save the IP Address away.
                memcpy( &s_p->rasEntry.ipaddr, &IPAddr, sizeof( RASIPADDR ) );
            } else {
                DEBUGMSG (1, (TEXT("Error getting IP Address\r\n")));

                return( ERROR_SLIP_REQUIRES_IP );
            }
        }
        
        break;

    default:
		ASSERT(FALSE);
		break;
    }

    // Attach MAC layer to the connected device

    pppChangeOfState( s_p, RASCS_DeviceConnected, 0 );

    return( SUCCESS );
}


static PPROTOCOL_CONTEXT
pppSessionFindProtocolContextForPacket(
	IN pppSession_t *pSession,
	IN pppMsg_t     *pMsg)
//
//  Given a PPP packet, decode the protocol field and look for a
//  registered context of that type in the session.
//
{
	PPROTOCOL_CONTEXT pContext = NULL,
		              pEndContext;
	DWORD             ProtocolType;

    ProtocolType = PPPDecodeProtocol( pMsg );
	if (ProtocolType != 0)
	{
		pEndContext = &pSession->pRegisteredProtocolTable[pSession->numRegisteredProtocols];
		for (pContext = &pSession->pRegisteredProtocolTable[0]; TRUE; pContext++)
		{
			if (pContext >= pEndContext)
			{
				pContext = NULL;
				break;
			}
			if (pContext->Type == ProtocolType)
			{
				// Matched, return pContext.
				break;
			}
		}
	}

	return pContext;
}

void
PPPRxProtocolReject(
	IN pppSession_t *pSession,
	IN pppMsg_t     *pMsg)
//
//  Called when the peer has rejected a packet we sent because it does
//  not support that protocol.
//
{
	PPROTOCOL_CONTEXT pContext;

	//
	// Lookup the protocol context for the protocol type and call
	// its protocol reject handler.
	//
	pContext = pppSessionFindProtocolContextForPacket(pSession, pMsg);
	if (pContext == NULL)
	{
		DEBUGMSG( ZONE_PPP | ZONE_WARN, (L"PPP: Peer Rejected Unsupported Protocol: %04X\n", pMsg->ProtocolType));
	}
	else
	{
		if (pContext->pDescriptor->ProtReject)
		{
			pContext->pDescriptor->ProtReject(pContext->Context);
		}
	}
}

void
PPPRxPacketHandler(
	IN pppSession_t *pSession,
	IN pppMsg_t     *pMsg)
//
//  This function is called to process a received packet.
//  It decodes the PPP protocol field then passes the packet
//  to the handler for that protocol, or sends an LCP reject
//  if there is no installed handler for that protocol type.
//
{
	PPROTOCOL_CONTEXT pContext;

	ASSERT(PPPMODE_PPP == pSession->Mode);

	// Set flag to indicate we have received data, used
	// by LCP for idle link detection
	pSession->bRxData = TRUE;

	// 
    // Get the protocol type of the packet and route it
	// to the appropriate protocol receive packet handler.
	//
    // After decoding, the msg_p->data and msg_p->len
    // point to the message's information field.

	pContext = pppSessionFindProtocolContextForPacket(pSession, pMsg);
	if (pContext == NULL)
	{
		// Route unsupported network packets to pppLcpSendProtocolReject.
		// While LCP is in OPENED state any protocol packet which is
		// unsupported MUST be returned in a Protocol-Reject packet.
		// Only protocols which are supported are silently discarded.

		DEBUGMSG( ZONE_PPP | ZONE_WARN, (L"PPP: Unsupported Protocol: %04X\n", pMsg->ProtocolType));
		pppLcpSendProtocolReject(pSession->lcpCntxt, pMsg);
	}
	else
	{
		if (0 == pMsg->cbMACPacket)
		{
			// Preserve the size of the information field indicated from MAC
			pMsg->cbMACPacket = pMsg->len;
		}

		// Route the packet to this protocol since the type matches
		pContext->pDescriptor->RcvData(pContext->Context, pMsg);
	}
}

void
SLIPRxPacketHandler(
	IN pppSession_t *pSession,
	IN pppMsg_t     *pMsg)
{
	ASSERT(pSession->Mode == PPPMODE_CSLIP || pSession->Mode == PPPMODE_SLIP);

	// Preserve the size of the information field indicated from MAC
	pMsg->cbMACPacket = pMsg->len;

	if (pSession->Mode == PPPMODE_CSLIP)
	{
        // TBD - VJ Header Decompression
	}

	// Pass the packet straight to IP

    Receive(pSession->context, pMsg);
}

void
PPPSessionUpdateRxPacketStats(
	IN pppSession_t *pSession,
	IN pppMsg_t     *pMsg)
//
//  This function is called when PPP receives a packet that is
//  being indicated up to TCP/IP. It updates necessary statistics
//  information.
//
{
	ncpCntxt_t      *pNcpContext  = (ncpCntxt_t *)pSession->ncpCntxt;

	pSession->Stats.BytesReceivedCompressed += pMsg->cbMACPacket;
	pSession->Stats.BytesReceivedUncompressed += pMsg->len;

	pSession->Stats.FramesRcvd++;
	pSession->Stats.BytesRcvd += pMsg->len;

	DEBUGMSG(ZONE_STATS, (L"PPP: RX Stats: %u Frames, Bytes %u/%u (ratio=%u%%)\n",
		pSession->Stats.FramesRcvd,
		pSession->Stats.BytesReceivedCompressed,
		pSession->Stats.BytesReceivedUncompressed,
		(pSession->Stats.BytesReceivedCompressed * 100) / (pSession->Stats.BytesReceivedUncompressed ? pSession->Stats.BytesReceivedUncompressed : 1)));

	pNcpContext->dwLastTxRxIpDataTickCount = GetTickCount();
}

BOOL
IsValidSession(pppSession_t *s_p)
//
//	Determine whether a ppp session structure is valid.
//	This function must be called with v_ListCS held.
//
{
	BOOL		bValid = FALSE;
	PPP_CONTEXT *pContext;

	for (pContext = pppContextList; pContext; pContext = pContext->Next)
	{
		if (pContext->Session == s_p)
		{
			bValid = TRUE;
			break;
		}
	}

	return bValid;
}

pppSession_t *
IsValidMacContext(void *macCntxt)
//
//	Determine whether a MAC context structure is valid.
//	This function must be called with v_ListCS held.
//	Returns the session pointer that contains the mac context.
//
{
	pppSession_t *s_p = NULL;
	PPP_CONTEXT  *pContext;

	for (pContext = pppContextList; pContext; pContext = pContext->Next)
	{
		if (pContext->Session && pContext->Session->macCntxt == macCntxt)
		{
			s_p = pContext->Session;
			break;
		}
	}

	return s_p;
}

/*****************************************************************************
*
*   @func   void | pppAddRef | Increment the reference count for this session.
*
*   @parm   pppSession_t * | c_p | PPP session context
*
*   @rdesc  Returns TRUE if the session is validated and AddRef'ed successfully.
*			FALSE otherwise.

*   @comm   This function is used to maintain a ppp context reference counter
*           to prevent the context from being deleted when there are out-
*           standing references to it - primarily from expired timers
*           threads waiting to access the ppp context.
*/

BOOL
pppAddRef(
	IN  OUT pppSession_t *s_p)
{
	BOOL	bSuccess = FALSE;

	DEBUGMSG (ZONE_REFCNT, (TEXT("PPP: +pppAddRef: %x\n"), s_p));

	EnterCriticalSection (&v_ListCS);
	if (IsValidSession(s_p))
	{
		bSuccess = TRUE;
        s_p->RefCnt++;

		DEBUGMSG(ZONE_REFCNT,(TEXT("PPP: -pppAddRef: RefCnt now %d\n"), s_p->RefCnt));
    }
	LeaveCriticalSection (&v_ListCS);

	DEBUGMSG (ZONE_REFCNT, (TEXT("PPP: -pppAddRef: %x -- success=%d\n"), s_p, bSuccess));

	return bSuccess;
}

/*****************************************************************************
*
*   @func   void | pppDelRef | Decrement the referenc count for this session
*                              and delete the context when zero.
*   @rdesc  Returns TRUE if the RefCnt becomes 0 and the session is deleted,
*			FALSE otherwise.
*
*   @parm   pppSession_t * | c_p | PPP session context
*
*   @comm   This function is used to maintain a ppp context reference counter
*           to prevent the context from being deleted when there are out-
*           standing references to it - primarily from expired timers
*           threads waiting to access the ppp context.
*/

BOOL
pppDelRef(
	IN OUT pppSession_t *s_p )
{
	PPP_CONTEXT *pIPContext;
    HANDLE       hHeap;
	int          refcnt;

	DEBUGMSG (ZONE_REFCNT, (TEXT("PPP: +pppDelRef: %x\n"), s_p));

	//
	// The following actions must be done atomically:
	//   1. Decrementing the session RefCnt
	//   2. Checking to see if the RefCnt is 0
	//   3. If the RefCnt is 0, preventing any other thread
	//      from referencing the session.
	// The v_ListCS is used to enforce this atomicity.
	//
	EnterCriticalSection (&v_ListCS);

	// Since there must have been a prior call to pppAddRef, the session must
	// be valid.  That is, an AddRef'ed session must remain valid at least
	// until the corresponding pppDelRef.
	ASSERT(IsValidSession(s_p));
	ASSERT(s_p->RefCnt > 0);

    // Decrement the reference count
    refcnt = --s_p->RefCnt;
    
	DEBUGMSG(ZONE_REFCNT, (TEXT("PPP: pppDelRef: RefCnt %d\r\n"), refcnt));

    // If the reference count is zero then delete the context.

    if (refcnt == 0)
	{
		// Save the IPcontext and heap pointers for the session
		pIPContext = s_p->context;
        hHeap = s_p->hHeap;

		DEBUGMSG( ZONE_PPP, (TEXT("PPP: RefCnt 0 - DELETING CONTEXT\r\n")));

		// Remove this session and context from the pppContext list, this
		// will prevent any other thread from referencing the context.
        pIPContext->Session = 0;
		pppContextRemove(pIPContext);
		s_p->context = NULL;
	}

	//
	// We are done with the pppContextList, release it prior to completing
	// the session deletion if the refcnt is 0, because the subsequent
	// deletion operations may attempt to acquire other locks.
	//
	LeaveCriticalSection (&v_ListCS);

	if (refcnt == 0)
	{
		// Delete the IP interface (if not already deleted by IPCP terminate)
		PPPDeleteInterface(pIPContext, s_p);
		pppContextFree(pIPContext);

		// Reenable auto-suspend if disabled at session create

		if (!s_p->dwAllowAutoSuspendWhileSessionActive)
			CTEIOControl(CTE_IOCTL_CLEAR_IDLE_TIMER_RESET, NULL, 0, NULL, 0, NULL);

		FreePppSession(s_p);

        // Destroy the PPP heap
        HeapDestroy( hHeap );
    }

	DEBUGMSG (ZONE_REFCNT, (TEXT("PPP: -pppDelRef: %x -- deleted=%d\n"), s_p, refcnt == 0));

	// Return TRUE if the session was deleted because its refcnt became 0
    return refcnt == 0;  
}

/*****************************************************************************
*
*   @func   void | pppAddRefForMacContext | Increment the reference count
*			to the session that owns a Mac Context.
*
*   @parm   void * | pMac | PPP MAC context
*
*   @rdesc  Returns a pointer to the session containing the MAC context if
*			the context is validated and AddRef'ed successfully,
*			NULL otherwise.  If non-NULL, then the returned session pointer should
*			be passed into a subsequent call to pppDelRef.
*
*   @comm   This function is used to maintain a ppp context reference counter
*           to prevent the context from being deleted when there are out-
*           standing references to it.
*/

pppSession_t *
pppAddRefForMacContext(
	void *pMac )
{
	pppSession_t *s_p;

	DEBUGMSG (ZONE_REFCNT, (TEXT("PPP: +pppAddRefForMacContext: %x\n"), pMac));

	EnterCriticalSection (&v_ListCS);
	if (s_p = IsValidMacContext(pMac))
	{
        s_p->RefCnt++;
		DEBUGMSG( ZONE_REFCNT,(TEXT("PPP: pppAddRefForMacContext: RefCnt now %d\n"), s_p->RefCnt));
    }
	LeaveCriticalSection (&v_ListCS);

	return s_p;
}

#ifdef DEBUG
void
CheckTrackedRefSum(
	IN    pppSession_t *s_p)
{
	int i;
	LONG sum = 0;

	for (i = 0; i < REF_MAX_ID; i++)
		sum += s_p->TrackedRefCnt[i];

	ASSERT(sum == s_p->RefCnt);
}

BOOL
pppAddRefTracked(
	IN OUT pppSession_t *s_p,
	IN     DWORD        idRef)
{
	BOOL bResult;

	ASSERT(idRef < REF_MAX_ID);

	EnterCriticalSection (&v_ListCS);
	bResult = pppAddRef(s_p);
	if (bResult)
	{
		ASSERT(s_p->TrackedRefCnt[idRef] >= 0);
		s_p->TrackedRefCnt[idRef]++;
 		CheckTrackedRefSum(s_p);
	}
	LeaveCriticalSection (&v_ListCS);

	return bResult;
}

BOOL
pppDelRefTracked(
	IN OUT pppSession_t *s_p,
	IN     DWORD        idRef)
{
	BOOL bSessionDeleted;

	ASSERT(idRef < REF_MAX_ID);

	EnterCriticalSection (&v_ListCS);
	ASSERT(IsValidSession(s_p));
	ASSERT(s_p->TrackedRefCnt[idRef] > 0);
 	CheckTrackedRefSum(s_p);
	s_p->TrackedRefCnt[idRef]--;
	bSessionDeleted = pppDelRef(s_p);
	LeaveCriticalSection (&v_ListCS);
	return bSessionDeleted;
}

pppSession_t *
pppAddRefForMacContextTracked(
	IN     void *pMac,
	IN     DWORD idRef)
{
	pppSession_t *s_p;

	DEBUGMSG (ZONE_REFCNT, (TEXT("PPP: +pppAddRefForMacContext: %x\n"), pMac));

	ASSERT(idRef < REF_MAX_ID);

	EnterCriticalSection (&v_ListCS);
	if (s_p = pppAddRefForMacContext(pMac))
	{
		ASSERT(s_p->TrackedRefCnt[idRef] >= 0);
		s_p->TrackedRefCnt[idRef]++;
 		CheckTrackedRefSum(s_p);
    }
	LeaveCriticalSection (&v_ListCS);

	return s_p;
}

#endif
/*****************************************************************************
*
*   @func   void | pppStartTimer | start timer with reference count processing
*
*   @parm   pppSession_t * | c_p | PPP session context
*   @parm   CTETimer *  | tmr_p   | Pointer to timer
*
*   @comm   This function is used to maintain a ppp context reference counter
*           to prevent the context from being deleted when there are out-
*           standing references to it - primarily from expired timers
*           threads waiting to access the ppp context.
*/

void
pppStartTimer( pppSession_t  *s_p,
               CTETimer      *tmr_p,
               unsigned long  delay,
               CTEEventRtn    func,
               void          *arg )
{
	DEBUGMSG (ZONE_TIMING, (TEXT("PPP: pppStartTimer: tmr_p=%p delay=%d func=%x\n"), tmr_p, delay, func));

	// Add a reference to the session to keep it around until the timer
	// expires.  When the timer goes off, 'func' will delete this reference.

	if (PPPADDREF(s_p, REF_TIMER))
	{
		//
		//	Sometimes a timer fires just as a response is received.
		//	This can result in the response being processed and
		//	the timer restarted prior to processing the timeout,
		//	which also starts the timer.
		//
		//	CTE doesn't like it when a running timer is started
		//	again, so make sure the timer is stopped, and any
		//  reference for it to the session is deleted.
		//
		pppStopTimer(s_p, tmr_p);

		if( !CTEStartTimer( tmr_p, delay, func, arg ) )
		{
			// Timer failed to start - delete reference

			PPPDELREF(s_p, REF_TIMER);
		}
	}
}

/*****************************************************************************
*
*   @func   void | pppStopTimer | stop a timer with reference count processing
*
*   @parm   pppSession_t * | c_p | PPP session context
*   @parm   CTETimer *  | tmr_p   | Pointer to timer
*
*   @comm   This function is used to maintain a ppp context reference counter
*           to prevent the context from being deleted when there are out-
*           standing references to it - primarily from expired timers
*           threads waiting to access the ppp context.
*/

DWORD
pppStopTimer( pppSession_t *s_p, CTETimer *tmr_p )
{
	BOOL	bTimerStoppedSuccessfully;

	DEBUGMSG (ZONE_TIMING, (TEXT("PPP: pppStopTimer: tmr_p=%p\n"), tmr_p));

    bTimerStoppedSuccessfully = CTEStopTimer( tmr_p );
	if (bTimerStoppedSuccessfully)
    {
        // Timer stopped - decrement context ref counter
		PPPDELREF(s_p, REF_TIMER);
    }

    return bTimerStoppedSuccessfully;
}

/*****************************************************************************
*
*   @func   void | pppLock/UnLock | Lock/UnLock the PPP session
*
*   @parm   pppSession_t | s_p | Session pointer
*
*   @comm   This function enters/leaves PPP's critical section.
*/

void
pppLock( pppSession_t *s_p )
{
	DEBUGMSG( ZONE_LOCK, (TEXT( "PPP: +pppLock: 0x%x\n" ), &s_p->SesCritSec) );

	// Any lock/unlock of the session MUST be done with the session referenced!
	ASSERT(s_p->RefCnt > 0);

	ASSERT(IsValidSession(s_p));

    s_p->LockWaitCnt++;

#ifdef DEBUG
    DEBUGMSG (ZONE_WARN &
              (s_p->SesCritSec.OwnerThread == (HANDLE)GetCurrentThreadId()),
              (TEXT( "ppplocking Already own lock (0x%X)!!!!\r\n"),
               GetCurrentThreadId()));
#endif

    EnterCriticalSection( &s_p->SesCritSec );

	DEBUGMSG( ZONE_LOCK, (TEXT( "PPP: pppLock - threadId 0x%x\r\n" ), s_p->SesCritSec.OwnerThread ));

    s_p->LockWaitCnt--;
    s_p->Locked = TRUE;

	DEBUGMSG( ZONE_LOCK, (TEXT( "PPP: -pppLock\n" )) );
}

void
pppUnLock( pppSession_t *s_p )
{
	DEBUGMSG( ZONE_LOCK, (TEXT( "PPP: +pppUnLock\n" )) );

	// Any lock/unlock of the session MUST be done with the session referenced!
	ASSERT(IsValidSession(s_p));
	ASSERT(s_p->RefCnt > 0);

    s_p->Locked = FALSE;
	DEBUGMSG( ZONE_LOCK, (TEXT( "PPP: pppUnLock - threadId 0x%x\r\n" ), s_p->SesCritSec.OwnerThread ));

    LeaveCriticalSection( &s_p->SesCritSec );

	// Any lock/unlock of the session MUST be done with the session referenced!
	ASSERT(IsValidSession(s_p));
	ASSERT(s_p->RefCnt > 0);

	DEBUGMSG( ZONE_LOCK, (TEXT( "PPP: -pppUnLock\n" )) );
}

DWORD
PPPSessionRegisterProtocol(
    IN OUT pppSession_t    *pSession,
	IN USHORT               ProtocolType,
	IN PPROTOCOL_DESCRIPTOR pDescriptor,
	IN PVOID                Context)
{
	DWORD              Result = SUCCESS;
	PPROTOCOL_CONTEXT  pNewTable;
	DWORD              cNewTable;
	DWORD              i,
		               BytesToMove;

	if (pSession->numRegisteredProtocols >= pSession->cRegisteredProtocolTable)
	{
		// Try to grow the table
		cNewTable = pSession->cRegisteredProtocolTable + 15;
		pNewTable = (PPROTOCOL_CONTEXT)pppAlloc(pSession, sizeof(PROTOCOL_CONTEXT) * cNewTable, L"PROTTAB");
		if (pNewTable)
		{
			memset(pNewTable, 0, sizeof(PROTOCOL_CONTEXT) * cNewTable);
			memcpy(pNewTable, pSession->pRegisteredProtocolTable, sizeof(PROTOCOL_CONTEXT) * pSession->cRegisteredProtocolTable);
			pppFree(pSession, pSession->pRegisteredProtocolTable, L"PROTTAB");
			pSession->pRegisteredProtocolTable = pNewTable;
			pSession->cRegisteredProtocolTable = cNewTable;
		}
		else
		{
			Result = ERROR_OUTOFMEMORY;
		}
	}
	
	if (SUCCESS == Result)
	{
		//
		// Insert the new entry into the table such that the table remains
		// sorted in ascending order by ProtocolType.
		//
		// Note that the more performance critical protocols, e.g. IP and CCP,
		// have protocol types 0x00xx, so they will appear earlier in the array
		// than the less time critical protocols that have a non-zero high byte
		// (e.g. LCP is 0xC021). Thus a linear search of the array for is acceptable
		// since the most frequently used types will be found within the first few
		// entries.
		//
		for (i = 0; i < pSession->numRegisteredProtocols; i++)
		{
			if (ProtocolType <= pSession->pRegisteredProtocolTable[i].Type)
			{
				// Insert new entry before 'i'
				break;
			}
		}

		BytesToMove = (pSession->numRegisteredProtocols - i) * sizeof(PROTOCOL_CONTEXT);
		memmove(&pSession->pRegisteredProtocolTable[i + 1], &pSession->pRegisteredProtocolTable[i], BytesToMove);
		pSession->pRegisteredProtocolTable[i].Type        = ProtocolType;
		pSession->pRegisteredProtocolTable[i].pDescriptor = pDescriptor;
		pSession->pRegisteredProtocolTable[i].Context     = Context;
		pSession->numRegisteredProtocols += 1;
	}

	return Result;
}
