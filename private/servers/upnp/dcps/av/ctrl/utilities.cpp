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

#include "av_upnp.h"
#include "av_upnp_ctrl_internal.h"

#include <auto_xxx.hxx>


DWORD av_upnp::details::AVErrorFromUPnPError(HRESULT hr)
{
    if(SUCCEEDED(hr))
    {
        return SUCCESS_AV;
    }
    
    if(hr >= UPNP_E_ACTION_SPECIFIC_BASE && hr <= UPNP_E_ACTION_SPECIFIC_MAX)
    {
        return hr - UPNP_E_ACTION_SPECIFIC_BASE + FAULT_ACTION_SPECIFIC_BASE;
    }
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
    {
        return true;
    }
        
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
                        {
                            return true;
                        }
                    }
                }
                
                punkService.Release();
            }
        }
    }
    
    DEBUGMSG(ZONE_AV_TRACE, (AV_TEXT("Couldn't find service with type = \"%s\""), pszServiceType));
            
    return false;
}

