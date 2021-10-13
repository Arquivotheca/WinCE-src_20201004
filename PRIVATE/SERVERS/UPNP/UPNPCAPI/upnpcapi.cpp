//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <ssdppch.h>
#include <upnpdevapi.h>
#include <ssdpapi.h>
#include <ssdpioctl.h>
#include <service.h>
#include "ssdpfuncc.h"
#include "string.hxx"

#define celems(_x)          (sizeof(_x) / sizeof(_x[0]))

BOOL InitCallbackTarget(PCWSTR pszName, PUPNPCALLBACK pfCallback, PVOID pvContext, DWORD *phCallback);
BOOL StopCallbackTarget(PCWSTR pszName);

ce::psl_proxy<>     proxy(L"UPP1:", UPNP_IOCTL_INVOKE, NULL);
CRITICAL_SECTION    g_csUPNPAPI;


BOOL WINAPI ClientStart()
{
    return TRUE;
}

void WINAPI ClientStop()
{
}


/*
    Device creation
 */
BOOL
WINAPI
UpnpAddDevice(IN UPNPDEVICEINFO *pDev)
{
    BOOL    fRet = FALSE;
	DWORD   dwCallbackHandle;
	
    fRet = InitCallbackTarget(pDev->pszDeviceName, pDev->pfCallback, pDev->pvUserDevContext, &dwCallbackHandle);
    
    if(fRet)
    {
        PVOID pvTemp = pDev->pvUserDevContext;
        pDev->pvUserDevContext = (PVOID)dwCallbackHandle;
        
        fRet = (ERROR_SUCCESS == proxy.call(UPNP_IOCTL_ADD_DEVICE, pDev));

    	// restore the changed parameters
    	pDev->pvUserDevContext = pvTemp;

    	if(!fRet)
    	    StopCallbackTarget(pDev->pszDeviceName);
    }

    return fRet;	    
}

BOOL
WINAPI
UpnpRemoveDevice(
    PCWSTR pszDeviceName)
{
	BOOL fRet;
	
    fRet = (ERROR_SUCCESS == proxy.call(UPNP_IOCTL_REMOVE_DEVICE, pszDeviceName));
    
    StopCallbackTarget(pszDeviceName); // this may fail if the target has already been removed
    
    return fRet;    
}


/*
  Publication control
*/
BOOL
WINAPI
UpnpPublishDevice(
    PCWSTR pszDeviceName)
{
	return ERROR_SUCCESS == proxy.call(UPNP_IOCTL_PUBLISH_DEVICE, pszDeviceName);
}

BOOL
WINAPI
UpnpUnpublishDevice(
    PCWSTR pszDeviceName)
{
    return ERROR_SUCCESS == proxy.call(UPNP_IOCTL_UNPUBLISH_DEVICE, pszDeviceName);	
}


/*
 Description info
 */

BOOL
WINAPI
UpnpGetUDN(
    IN PCWSTR pszDeviceName,    // [in] local device name
    IN PCWSTR pszTemplateUDN,   // [in, optional] the UDN element in the original device description
    OUT PWSTR pszUDNBuf,        // [in] buffer to hold the assigned UDN
    IN OUT PDWORD pchBuf)       // [in,out] size of buffer/ length filled (in WCHARs)
{
	return ERROR_SUCCESS == proxy.call(UPNP_IOCTL_GET_UDN, pszDeviceName, pszTemplateUDN, pszUDNBuf, pchBuf);
}


BOOL
WINAPI
UpnpGetSCPDPath(
    IN PCWSTR pszDeviceName,   // [in] local device name
    IN PCWSTR pszUDN,
    IN PCWSTR pszServiceId,    // [in] serviceId (from device description)
    OUT PWSTR  pszSCPDPath,    // [out] file path to SCPD
    IN DWORD  cchFilePath)     // [in] size of  pszSCPDFilePath in WCHARs
{
	return ERROR_SUCCESS == proxy.call(UPNP_IOCTL_GET_SCPD_PATH, pszDeviceName, pszUDN, pszServiceId, pszSCPDPath, cchFilePath);
}
/*
  Eventing

*/

BOOL
WINAPI
UpnpSubmitPropertyEvent(
    PCWSTR pszDeviceName,
    PCWSTR pszUDN,
    PCWSTR pszServiceName,
    DWORD nArgs,
    UPNPPARAM *rgArgs)
{
	return ERROR_SUCCESS == proxy.call(UPNP_IOCTL_SUBMIT_PROPERTY_EVENT, pszDeviceName, pszUDN, pszServiceName, UPNPPARAMS(nArgs, rgArgs));
}


BOOL
WINAPI
UpnpUpdateEventedVariables(
    PCWSTR pszDeviceName,
    PCWSTR pszUDN,
    PCWSTR pszServiceName,
    DWORD nArgs,
    UPNPPARAM *rgArgs)
{
	return ERROR_SUCCESS == proxy.call(UPNP_IOCTL_UPDATE_EVENTED_VARIABLES, pszDeviceName, pszUDN, pszServiceName, UPNPPARAMS(nArgs, rgArgs));
}


bool EncodeForXML(LPCWSTR pwsz, ce::wstring* pstr)
{
	wchar_t		aCharacters[] = {L'<', L'>', L'\'', L'"', L'&'};
	wchar_t*	aEntityReferences[] = {L"&lt;", L"&gt;", L"&apos;", L"&quot;", L"&amp;"};
	bool		bReplaced;
	
	pstr->reserve(static_cast<ce::wstring::size_type>(1.1 * wcslen(pwsz)));
	pstr->resize(0);
	
	for(const wchar_t* pwch = pwsz; *pwch; ++pwch)
	{
		bReplaced = false;

		for(int i = 0; i < sizeof(aCharacters)/sizeof(*aCharacters); ++i)
			if(*pwch == aCharacters[i])
			{
				pstr->append(aEntityReferences[i]);
				bReplaced = true;
				break;
			}

		if(!bReplaced)
			pstr->append(pwch, 1);
	}

	return true;
}


/*
<s:Envelope
    xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"
    s:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/">
  <s:Body>
    <u:actionNameResponse xmlns:u="urn:schemas-upnp-org:service:serviceType:v">
      <argumentName>out arg value</argumentName>
      other out args and their values go here, if any
    </u:actionNameResponse>
  </s:Body>
</s:Envelope>
*/

const WCHAR c_szSOAPResp1[] = L"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" \r\n"
    						L"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
    						L" <s:Body>\r\n"
    						L"   <u:%sResponse xmlns:u=\"%s\">\r\n";
const WCHAR c_szSOAPResp2[] = L" </u:%sResponse>\r\n"
  							L" </s:Body>\r\n"
							L"</s:Envelope>\r\n";
   						
const WCHAR c_szSOAPArg[] = L"<%s>%s</%s>\r\n";


static bool _UpnpSetControlResponse(
    UPNPSERVICECONTROL *pUPNPAction,
    DWORD cOutArgs,
    UPNPPARAM *aOutArg,
	PWSTR* ppszRawResp)
{
	int			cch = celems(c_szSOAPResp1) + celems(c_szSOAPResp2), cch2;
	ce::wstring str;
	DWORD		i;

	// force out argument for QueryStateVariable to be "return"
	if (wcscmp(pUPNPAction->pszAction, L"QueryStateVariable") == 0 && cOutArgs == 1)
		aOutArg[0].pszName = L"return";

	cch += 2*wcslen(pUPNPAction->pszAction) + wcslen(pUPNPAction->pszServiceType);
	
	for (i=0; i < cOutArgs; i++)
	{
		EncodeForXML(aOutArg[i].pszValue, &str);

		cch += wcslen(c_szSOAPArg) + 2*wcslen(aOutArg[i].pszName) + str.length();
	}

	*ppszRawResp = new WCHAR [cch];

	if (!*ppszRawResp)
		return false;

	cch2 = wsprintfW(*ppszRawResp,c_szSOAPResp1,pUPNPAction->pszAction, pUPNPAction->pszServiceType);

	for (i=0; i < cOutArgs; i++)
	{
		EncodeForXML(aOutArg[i].pszValue, &str);
		
		cch2+=wsprintfW(*ppszRawResp + cch2, c_szSOAPArg, aOutArg[i].pszName, static_cast<LPCWSTR>(str), aOutArg[i].pszName);
	}

	cch2 += wsprintfW(*ppszRawResp + cch2, c_szSOAPResp2, pUPNPAction->pszAction);
	
	DBGCHK(L"UPNPAPI", cch2 < cch);

	return true;
}


BOOL
WINAPI
UpnpSetControlResponse(
    UPNPSERVICECONTROL *pUPNPAction,
    DWORD cOutArgs,
    UPNPPARAM *aOutArg)
{
	BOOL fRet = FALSE;
	PWSTR pszRawResp = NULL;
	
	__try 
	{
		fRet = _UpnpSetControlResponse(pUPNPAction, cOutArgs, aOutArg, &pszRawResp);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		fRet = FALSE;
	}
	
	if (fRet)
		fRet = UpnpSetRawControlResponse(pUPNPAction,HTTP_STATUS_OK, pszRawResp);

	if (pszRawResp)
		delete[] pszRawResp;

	return fRet;
}


const WCHAR c_szSOAPFault[]= L"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"\r\n"
   							L" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
							L"  <s:Body>\r\n"
							L"    <s:Fault>\r\n"
							L"      <faultcode>s:Client</faultcode>\r\n"
							L"	      <faultstring>UPnPError</faultstring>\r\n"
							L"	      <detail>\r\n"
							L"	        <UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">\r\n"
							L"	          <errorCode>%d</errorCode>\r\n"
							L"	          <errorDescription>%s</errorDescription>\r\n"
							L"	        </UPnPError>\r\n"
							L"	      </detail>\r\n"
							L"	    </s:Fault>\r\n"
							L"	  </s:Body>\r\n"
							L"	</s:Envelope>\r\n";

static bool _UpnpSetErrorResponse(
	UPNPSERVICECONTROL *pUPNPAction,
	DWORD dwErrorCode,
	PCWSTR pszErrorDescription,	// error code and description
	PWSTR* ppszRawResp)
{
	int			cch;
	ce::wstring strErrorDescription;

	EncodeForXML(pszErrorDescription, &strErrorDescription);

	cch = strErrorDescription.length() + 10 + celems(c_szSOAPFault);
	
	*ppszRawResp = new WCHAR [cch];
	
	if (!*ppszRawResp)
		return false;

	cch -= wsprintfW(*ppszRawResp, c_szSOAPFault, dwErrorCode, static_cast<LPCWSTR>(strErrorDescription));
	
	DBGCHK(L"UPNPAPI",cch > 0);

	return true;
}

BOOL
WINAPI
UpnpSetErrorResponse(
	UPNPSERVICECONTROL *pUPNPAction,
	DWORD dwErrorCode,
	PCWSTR pszErrorDescription	// error code and description
	)
{
	BOOL fRet = FALSE;
	PWSTR pszRawResp = NULL;

	__try 
	{
		fRet = _UpnpSetErrorResponse(pUPNPAction, dwErrorCode, pszErrorDescription, &pszRawResp);	
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		fRet = FALSE;
	}
	
	if (fRet)
		fRet = UpnpSetRawControlResponse(pUPNPAction, HTTP_STATUS_SERVER_ERROR, pszRawResp);

	if (pszRawResp)
		delete[] pszRawResp;

	return fRet;
}


#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("UPNPCAPI"), {
    TEXT("Misc"), TEXT("Init"), TEXT("Announce"), TEXT("Events"),
    TEXT("Socket"),TEXT("Search"), TEXT("Parser"),TEXT("Timer"),
    TEXT("Cache"),TEXT("Control"),TEXT("Cache"),TEXT(""),
    TEXT(""),TEXT(""),TEXT("Trace"),TEXT("Error") },
    0x00008000          
};
#endif

//extern CRITICAL_SECTION g_csSSDPAPI;

extern "C"
BOOL
WINAPI
DllMain(IN PVOID DllHandle,
        IN ULONG Reason,
        IN PVOID Context OPTIONAL)
{
    switch (Reason)
    {
        case DLL_PROCESS_ATTACH:
    		
		    DEBUGREGISTER((HMODULE)DllHandle);

            InitializeCriticalSection(&g_csUPNPAPI); 
            SocketInit();

            // We don't need to receive thread attach and detach
            // notifications, so disable them to help application
            // performance.
            DisableThreadLibraryCalls((HMODULE)DllHandle);
            svsutil_Initialize();
            break; 

        case DLL_PROCESS_DETACH:
    		
            SocketFinish();
            DeleteCriticalSection(&g_csUPNPAPI); 
            break;
    }

    return TRUE;
}

