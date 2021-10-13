//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "av_upnp.h"
#include "av_upnp_ctrl_internal.h"

#include <auto_xxx.hxx>


DWORD av_upnp::details::AVErrorFromUPnPError(HRESULT hr)
{
    if(SUCCEEDED(hr))
        return SUCCESS_AV;
    
    if(hr >= UPNP_E_ACTION_SPECIFIC_BASE && hr <= UPNP_E_ACTION_SPECIFIC_MAX)
        return hr - UPNP_E_ACTION_SPECIFIC_BASE + FAULT_ACTION_SPECIFIC_BASE;
    else
    {
        SetLastError(hr);
        return ERROR_AV_UPNP_ERROR;
    }
}


//
// GetService
//
bool av_upnp::details::GetService(IUPnPServices* pServices, LPCWSTR pszServiceID, LPCWSTR pszServiceType, IUPnPService** ppService)
{
    HRESULT         hr;
    ce::auto_bstr   bstrServiceID;
    
    bstrServiceID = SysAllocString(pszServiceID);
    
    if(SUCCEEDED(hr = pServices->get_Item(bstrServiceID, ppService)))
        return true;
        
    DEBUGMSG(ZONE_AV_TRACE, (AV_TEXT("Couldn't find service with ID = \"%s\"; trying to find service by type"), pszServiceID));
    
    CComPtr<IEnumUnknown>   pEnumServices;
	CComPtr<IUnknown>       punkEnum;
    
    // get enumerator
	if(SUCCEEDED(hr = pServices->get__NewEnum(&punkEnum)))
	{
		if(SUCCEEDED(hr = punkEnum->QueryInterface(IID_IEnumUnknown, (void**)&pEnumServices)))
		{
			CComPtr<IUnknown>   punkService;
			ULONG               l;

			// enumerate all service
			while(S_OK == pEnumServices->Next(1, &punkService, &l))
			{
				CComPtr<IUPnPService> pService;

				if(SUCCEEDED(hr = punkService->QueryInterface(IID_IUPnPService, (void**)&pService)))
				{
                    ce::auto_bstr bstrType;
                    
                    pService->get_ServiceTypeIdentifier(&bstrType);
                    
                    if(0 == wcscmp(bstrType, pszServiceType))
                    {
                        if(SUCCEEDED(pService->QueryInterface(IID_IUPnPService, (void**)ppService)))
                            return true;
                    }
				}
				
				punkService.Release();
		    }
		}
	}
	
	DEBUGMSG(ZONE_AV_TRACE, (AV_TEXT("Couldn't find service with type = \"%s\""), pszServiceType));
		    
    return false;
}

