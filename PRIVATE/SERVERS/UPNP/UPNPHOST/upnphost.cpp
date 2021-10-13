//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


File Name:

    upnphost.cpp

Abstract:

This file implements the UPnP Registrar coclass



Created: Nov 2000

--*/

#include "upnphostp.h"

#include "auto_xxx.hxx"
// combook.cpp should be included in this file only
#include "combook.cpp"

#define ttidRegistrar 1

HINSTANCE g_hMod;	// Dll module handle

// map CLSID to GetClassObject/UpdateRegistry routine
BEGIN_COCLASS_TABLE(ClassTable)
    IMPLEMENTS_COCLASS(UPnPRegistrar)
END_COCLASS_TABLE()


// implement ModuleAddRef/ModuleRelease/ModuleIsStopping/ModuleIsIdle
IMPLEMENT_DLL_MODULE_ROUTINES()

// implement DllGetClassObject/DllCanUnloadNow/Dll[Un]RegisterServer
IMPLEMENT_DLL_ENTRY_POINTS(g_hMod, ClassTable, 0, FALSE)


#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("UPNPHOST"), {
    TEXT("Misc"), TEXT("Registrar"), TEXT("Control"), TEXT("Events"),
    TEXT(""),TEXT(""),TEXT(""),TEXT(""),
    TEXT(""),TEXT(""),TEXT(""),TEXT(""),
    TEXT(""),TEXT(""),TEXT("Trace"),TEXT("Error") },
    0x0000ffff          
};
#endif

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

		DisableThreadLibraryCalls((HINSTANCE)DllHandle);
		
		DEBUGREGISTER((HMODULE)DllHandle);
        g_hMod = (HINSTANCE)DllHandle; 

        break; 


    case DLL_PROCESS_DETACH:
        break;

    }
    return TRUE;
}

//+---------------------------------------------------------------------------
//
//  Function:   AddDevice
//
//  Purpose:    Create and publish the UPnP device tree.
//				Register for callbacks from the service layer.
//				The device has a locally unique name specified
//				by member bstrDeviceId
//
//  Arguments:
//      pszXMLDesc [in] XML device description template
//
//  Returns:
//		HRESULT
//

HRESULT
DeviceProxy::AddDevice(LPWSTR pszXMLDesc)
{
	UPNPDEVICEINFO devInfo;
	BOOL fRet;
	devInfo.cbStruct = sizeof(devInfo);
	devInfo.pszDeviceDescription = pszXMLDesc;
	devInfo.pszDeviceName = m_bstrDeviceId;
	// if the UDN is not NULL, it is assumed to be a UUID
	devInfo.pszUDN = m_bstrUDN;
	devInfo.cachecontrol = m_nLifeTime;
	// all callbacks get a pointer to the DeviceProxy instance
	// as the 2nd Parameter
	devInfo.pvUserDevContext = this;
	devInfo.pfCallback = DevCallback;

	fRet = UpnpAddDevice(&devInfo);
	if (fRet)
	{
		// send out the SSDP announcements
		fRet = UpnpPublishDevice(m_bstrDeviceId);
	}
	if (!fRet)
	{
		return HrFromLastWin32Error();
	}
	return S_OK;
}

//+---------------------------------------------------------------------------
//
//  Function:   RemoveDevice
//
//  Purpose:    Unpublish and unregister the device tree.
//
//  Arguments:
//
//  Returns:
//		HRESULT
//
HRESULT
DeviceProxy::RemoveDevice()
{
	BOOL fRet;
	// no need to explicitly call Unpublish
	fRet = UpnpRemoveDevice(m_bstrDeviceId);
	if (!fRet)
		return HrFromLastWin32Error();
	return S_OK;
}

//+---------------------------------------------------------------------------
//
//  Function:  DevCallback (static method) 
//
//  Purpose:    UPNPCALLBACK prototype that is invoked by the
//				device hosting layer when interesting events
//				occur. In particular, the callback is invoked when
//				control requests are received from remote control points.
//
//  Arguments:
//      callbackId [in] specifies the type of callback event
//		pvUserContext [in] DeviceProxy pointer
//		pvSvcParam [in] depends on callbackId
//
//  Returns:
//		TRUE if success; FALSE if something bad happens
//
DWORD DeviceProxy::DevCallback( 
    	UPNPCB_ID callbackId, 
    	PVOID pvUserContext,	// app context (from UPNPDEVICEINFO)
    	PVOID pvSvcParam)	// depends on CALLBACKID
{
	DeviceProxy *pDevProxy = (DeviceProxy *)pvUserContext;
	switch (callbackId)
	{
		case UPNPCB_INIT:
			return pDevProxy->InitCallback();
			break;
		case UPNPCB_SUBSCRIBING:
			return pDevProxy->SubscribeCallback((UPNPSUBSCRIPTION*)pvSvcParam);
			break;
		case UPNPCB_CONTROL:
			return pDevProxy->ControlCallback((UPNPSERVICECONTROL*)pvSvcParam);
			break;
		case UPNPCB_SHUTDOWN:
			return pDevProxy->ShutdownCallback();
			break;
	}
	return TRUE;
}

//+---------------------------------------------------------------------------
//
//  Method: FindDevProxyByName  
//
//  Purpose:    Locate the named device proxy in the registrar
//              list and return a pointer
//
//  Arguments:
//      pszDevId [in] local name for device tree
//
//  Returns:
//		NULL if the object is not found, else pointer to the DeviceProxy
//
DeviceProxy *
UPnPRegistrar::FindDevProxyByName(PCWSTR pszDevId)
{
	LIST_ENTRY *pLink;
	for (pLink = m_DevProxyList.Flink; pLink != &m_DevProxyList; pLink = pLink->Flink)
	{
		DeviceProxy *pDev;
		pDev = CONTAINING_RECORD(pLink, DeviceProxy, m_link);
		if (wcscmp(pszDevId, pDev->Name()) == 0)
			return pDev;
	}
	return NULL;
}

//+---------------------------------------------------------------------------
//
//  Function:   HrCreateDeviceIdentifier
//
//  Purpose:    Generate a Unique Device Identifier string.
//				uses SysAllocString for memory.
//				The identifier is a 64 bit mini-GUID.
//
//  Arguments:
//      pbstrDevId [out] BSTR UDN (without the uuid: prefix)
//
//  Returns:
//		HRESULT
//
HRESULT
UPnPRegistrar::HrCreateDeviceIdentifier(BSTR *pbstrDevId)
{
	WCHAR wsz[64];
	LONGLONG uuid64 = GenerateUUID64();
	*pbstrDevId = NULL;
	// we need a device Id that is unique and a valid filename 
	// so no illegal characters like ':'
	wsprintfW(wsz, L"%04x%04x-%04x-%04x-0000-000000000000", (WORD)uuid64, *((WORD*)&uuid64 + 1), *((WORD*)&uuid64 + 2), *((WORD*)&uuid64 + 3));
	
	if (*pbstrDevId = SysAllocString(wsz))
		return S_OK;
	else
		return E_OUTOFMEMORY;
}

//+---------------------------------------------------------------------------
//
//  Function:  IUPnPRegistrar::RegisterRunningDevice 
//
//  Purpose:    Publish an already instantiated UPnP device tree.
//				See public docs for more info
//
//  Arguments:
//      bstrXMLDesc [in] XML device description template
//		punkDeviceControl [in] IUPnPDeviceControl interface of the object
//		bstrInitString [in] optional string to be passed back to object on init
//		bstrResourcePath [in] NULL - not used for WinCE. The resources should
//						be under the \windows\upnp directory.
//		nLifeTime [in] lifetime in seconds. Maybe 0 for system default
//		pbstrDeviceIdentifier [out] id generated by UPnP service for use in
//					UnregisterDevice and ReregisterDevice
//  Returns:
//		HRESULT
//
STDMETHODIMP
UPnPRegistrar::RegisterRunningDevice(
        /*[in]*/ BSTR     bstrXMLDesc,
        /*[in]*/ IUnknown * punkDeviceControl,
        /*[in]*/ BSTR     bstrInitString,
        /*[in]*/ BSTR     bstrResourcePath,
        /*[in]*/ long     nLifeTime,
        /*[out, retval]*/ BSTR * pbstrDeviceIdentifier)
{
	HRESULT hr = S_OK;
	BSTR bstrDeviceId = NULL;
	
	*pbstrDeviceIdentifier = NULL;
	// construct pbstrDeviceIdentifier
	hr = HrCreateDeviceIdentifier( &bstrDeviceId);
	if (SUCCEEDED(hr))
	{
		// add device
		hr = ReregisterRunningDevice(
		    bstrDeviceId,
		    bstrXMLDesc,
		    punkDeviceControl,
		    bstrInitString,
		    bstrResourcePath,
		    nLifeTime
		    );
		if (SUCCEEDED(hr))
		{
			*pbstrDeviceIdentifier = bstrDeviceId;
		}
		else
	    {
		    SysFreeString(bstrDeviceId);
	    }
	}

	return hr;
}

static const int MaxInstances = 30;

//+---------------------------------------------------------------------------
//
//  Function:  IUPnPRegistrar::RegisterDevice 
//
//  Purpose:    Publish UPnP device tree with delayed instantiaton of the
//				device implementation
//				See public docs for more info
//
//  Arguments:
//      pszXMLDesc [in] XML device description template
//		bstrProgIDDeviceControlClass [in] CLSID of COM device implementation
//		bstrInitString [in] optional string to be passed back to object on init
//		bstrContainerId [in] NULL - not used on WinCE
//		bstrResourcePath [in] NULL - not used for WinCE. The resources should
//						be under the \windows\upnp directory.
//		nLifeTime [in] lifetime in seconds. Maybe 0 for system default
//		pbstrDeviceIdentifier [out] id generated by UPnP service for use in
//					UnregisterDevice and ReregisterDevice
//  Returns:
//		HRESULT
//
STDMETHODIMP UPnPRegistrar::RegisterDevice(
    /*[in]*/ BSTR     bstrXMLDesc,
    /*[in]*/ BSTR     bstrProgIDDeviceControlClass,
    /*[in]*/ BSTR     bstrInitString,
    /*[in]*/ BSTR     bstrContainerId,
    /*[in]*/ BSTR     bstrResourcePath,
    /*[in]*/ long     nLifeTime,
    /*[out, retval]*/ BSTR * pbstrDeviceIdentifier)
{
    HRESULT hr = E_FAIL;
	BSTR bstrDeviceId = NULL;
	
	*pbstrDeviceIdentifier = NULL;
	
    if (bstrXMLDesc == NULL
        || bstrProgIDDeviceControlClass == NULL
        || pbstrDeviceIdentifier == NULL)
    {
        return E_INVALIDARG;
    }
    
	// construct pbstrDeviceIdentifier
	hr = HrCreateDeviceIdentifier( &bstrDeviceId);
	if (SUCCEEDED(hr))
	{
	    hr = ReregisterDevice(
                bstrDeviceId,
                bstrXMLDesc,
                bstrProgIDDeviceControlClass,
                bstrInitString,
                bstrContainerId,
                bstrResourcePath,
                nLifeTime);
	    if (SUCCEEDED(hr))
	    {
	        *pbstrDeviceIdentifier = bstrDeviceId;
	    }
	    else
	        SysFreeString(bstrDeviceId);
	            
	}
    return hr;
}

static const WCHAR c_szuuidprefix[] = L"uuid:";

//+---------------------------------------------------------------------------
//
//  Function:   
//
//  Purpose:    Create and publish the UPnP device tree.
//
//  Arguments:
//      pszXMLDesc [in] XML device description template
//
//  Returns:
//		HRESULT
//
STDMETHODIMP UPnPRegistrar::ReregisterRunningDevice(
    /*[in]*/ BSTR     bstrDeviceIdentifier,
    /*[in]*/ BSTR     bstrXMLDesc,
    /*[in]*/ IUnknown * punkDeviceControl,
    /*[in]*/ BSTR     bstrInitString,
    /*[in]*/ BSTR     bstrResourcePath,
    /*[in]*/ long     nLifeTime)
{
    HRESULT hr = S_OK;
	DeviceProxy *pDevProxy;
	WCHAR *pszUDN;

	pDevProxy = FindDevProxyByName(bstrDeviceIdentifier);
	if (pDevProxy)
	{
		return E_FAIL;	// TODO: proper error
	}
	// construct UDN as "uuid:<bstrDeviceIdentifier>"
	pszUDN = new WCHAR[wcslen(bstrDeviceIdentifier) + celems(c_szuuidprefix)];
	if (!pszUDN)
	{
	    return E_OUTOFMEMORY;
	}
	wcscpy(pszUDN, c_szuuidprefix);
	wcscat(pszUDN, bstrDeviceIdentifier);
	
    pDevProxy = new DeviceProxy(
    				bstrDeviceIdentifier,
    				bstrInitString,
    				bstrResourcePath,
    				bstrXMLDesc,
    				nLifeTime,
    				punkDeviceControl,
    				pszUDN
    				);
    if (pDevProxy)
    {
        hr = pDevProxy->AddDevice(bstrXMLDesc);
        if (SUCCEEDED(hr))
        {
        	pDevProxy->AddToList(&m_DevProxyList);
        }
        else
        	delete pDevProxy;
    }
    else
        hr = E_OUTOFMEMORY;

	delete[] pszUDN;
	
    return hr;
}

#define __OPT_PLAT_FLAG (defined (ARMV4T))
#define __OPT_VER_OFF
#define	__OPT_BUGNUMSTRING	"26316"
#include <optimizer.h>

//+---------------------------------------------------------------------------
//
//  Function:   
//
//  Purpose:    Create and publish the UPnP device tree.
//
//  Arguments:
//      pszXMLDesc [in] XML device description template
//
//  Returns:
//		HRESULT
//
STDMETHODIMP UPnPRegistrar::ReregisterDevice(
    /*[in]*/ BSTR     bstrDeviceIdentifier,
    /*[in]*/ BSTR     bstrXMLDesc,
    /*[in]*/ BSTR     bstrProgIDDeviceControlClass,
    /*[in]*/ BSTR     bstrInitString,
    /*[in]*/ BSTR     bstrContainerId,
    /*[in]*/ BSTR     bstrResourcePath,
    /*[in]*/ long     nLifeTime)
{
    HRESULT hr = E_FAIL;
    HANDLE hUPnPHostingEvent;
	
	
    TraceTag(ttidRegistrar, "UPnPRegistrar::ReregisterDevice");
    if ( bstrDeviceIdentifier == NULL 
        || bstrXMLDesc == NULL
        || bstrProgIDDeviceControlClass == NULL
        )
    {
        return E_INVALIDARG;
    }
    // make sure the hosting service is around
    hUPnPHostingEvent = CreateEvent(NULL,FALSE,FALSE,UPNPLOADEREVENTNAME);
    if (hUPnPHostingEvent == NULL)
        return E_FAIL;
    // the service is supposed to create the named event
    if (GetLastError() != ERROR_ALREADY_EXISTS)
    {
        CloseHandle(hUPnPHostingEvent);
        return HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_FOUND);
    }
    
	    
    RegEntry upnpDevicesKey(UPNPDEVICESKEY);
    if (upnpDevicesKey.Success())
    {
        RegEntry upnpDeviceKey(bstrDeviceIdentifier, upnpDevicesKey.GetKey());
        
        if (upnpDeviceKey.Success())
        {
            if ((bstrInitString && (hr = upnpDeviceKey.SetValue(L"InitString",bstrInitString)))
                ||(bstrProgIDDeviceControlClass && (hr = upnpDeviceKey.SetValue(L"ProgId",bstrProgIDDeviceControlClass)))
                || (bstrResourcePath && (hr = upnpDeviceKey.SetValue(L"ResourcePath",bstrResourcePath)))
                || (hr = upnpDeviceKey.SetValue(L"XMLDesc",bstrXMLDesc)) // is this too big for the registry?
                || (hr = upnpDeviceKey.SetValue(L"State", UPNPREG_STARTING))
				|| (hr = upnpDeviceKey.SetValue(L"LifeTime", nLifeTime))) 
            {
                hr = HRESULT_FROM_WIN32(hr);
                upnpDeviceKey.DeleteKey();
                RegDeleteKey(upnpDevicesKey.GetKey(),bstrDeviceIdentifier);
            }
        }
    } 
    if (SUCCEEDED(hr))
    {
        // signal the hosting process
        SetEvent(hUPnPHostingEvent);
    }
    CloseHandle(hUPnPHostingEvent);
    return hr;
}

#define __OPT_PLAT_FLAG (defined (ARMV4T))
#define __OPT_VER_RESTORE
#define	__OPT_BUGNUMSTRING	"26316"
#include <optimizer.h>

//+---------------------------------------------------------------------------
//
//  Function:   
//
//  Purpose:    Create and publish the UPnP device tree.
//
//  Arguments:
//      pszXMLDesc [in] XML device description template
//
//  Returns:
//		HRESULT
//
STDMETHODIMP UPnPRegistrar::UnregisterDevice(
    /*[in]*/ BSTR     bstrDeviceIdentifier,
    /*[in]*/ BOOL     fPermanent)
{
    TraceTag(ttidRegistrar, "UPnPRegistrar::UnregisterDevice");
    HRESULT hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    HANDLE hUPnPHostingEvent = NULL;

    if (bstrDeviceIdentifier == NULL || *bstrDeviceIdentifier == 0)
    {
        return E_INVALIDARG;
    }
    RegEntry upnpDevicesKey(UPNPDEVICESKEY);
    if (upnpDevicesKey.Success())
    {
        if (fPermanent)
        {
            hr = RegDeleteKey(upnpDevicesKey.GetKey(),bstrDeviceIdentifier);
        }
        else
        {
            RegEntry upnpDeviceKey(bstrDeviceIdentifier, upnpDevicesKey.GetKey(), FALSE); // open existing device key
            hr = upnpDeviceKey.SetValue(UPNPSTATEVALUE,UPNPREG_DISABLED);
            hr = HRESULT_FROM_WIN32(hr);
        }
        if (SUCCEEDED(hr))
        {
            // this appears to be  registered device. Need to signal the hosting process if its running
            hUPnPHostingEvent = CreateEvent(NULL,FALSE,FALSE,UPNPLOADEREVENTNAME);
            if (hUPnPHostingEvent && GetLastError() != ERROR_ALREADY_EXISTS)
            {
                // the hosting process is not around
                CloseHandle(hUPnPHostingEvent);
                hUPnPHostingEvent = NULL;
            }
        }

    }
    if (!UpnpRemoveDevice(bstrDeviceIdentifier) && hr != 0)
    {
        // return failure if we were not able to remove the device and it was not in the registry
        hr = HRESULT_FROM_WIN32(GetLastError());
    }
    else
        hr = S_OK;

    if (hUPnPHostingEvent)
    {
        // signal the hosting process. Otherwise it may not get to know that the device has been unregistered
	    SetEvent(hUPnPHostingEvent);
	    CloseHandle(hUPnPHostingEvent);
    }
    
    return hr;
}

//+---------------------------------------------------------------------------
//
//  Function:   
//
//  Purpose:    Create and publish the UPnP device tree.
//
//  Arguments:
//      pszXMLDesc [in] XML device description template
//
//  Returns:
//		HRESULT
//
STDMETHODIMP UPnPRegistrar::GetUniqueDeviceName(
    /*[in]*/          BSTR   bstrDeviceIdentifier,
    /*[in]*/          BSTR   bstrTemplateUDN,
    /*[out, retval]*/ BSTR * pbstrUDN)
{
    HRESULT hr = S_OK;
    WCHAR szUDN[128];
    DWORD cchUDN = sizeof(szUDN)/sizeof(szUDN[0]);
    if (!UpnpGetUDN(bstrDeviceIdentifier, bstrTemplateUDN, szUDN, &cchUDN))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }
    else
    {
        *pbstrUDN = SysAllocString(szUDN);
        if (!*pbstrUDN)
            hr = E_OUTOFMEMORY;
    }
    
    return hr;
}

STDMETHODIMP UPnPRegistrar::RegisterDeviceProvider(
    /*[in]*/ BSTR     bstrProviderName,
    /*[in]*/ BSTR     bstrProgIDProviderClass,
    /*[in]*/ BSTR     bstrInitString,
    /*[in]*/ BSTR     bstrContainerId)
{
	return E_NOTIMPL;
}

STDMETHODIMP UPnPRegistrar::UnregisterDeviceProvider(
    /*[in]*/ BSTR     bstrProviderName)
{
	return E_NOTIMPL;
}

//
//  DeviceProxy implementation
//


//+---------------------------------------------------------------------------
//
//  Function:   FreeControlRequest
//
//  Purpose:    Frees resources used by the fields of a UPNP_CONTROL_REQUEST
//              structure
//
//  Arguments:
//      pucr [in] Address of the structure to cleanup
//
//  Returns:
//      (none)
//    This function just frees the resources used by the fields within
//    the passed in structure. It does not free the memory used by the
//    structure itself.
//
static VOID
FreeControlRequest(
    IN UPNP_CONTROL_REQUEST    * pucr)
{
    if (pucr->bstrActionName)
    {
        SysFreeString(pucr->bstrActionName);
        pucr->bstrActionName = NULL;
    }

    if (pucr->rgvarInputArgs)
    {
        for (DWORD i = 0; i < pucr->cInputArgs; i++)
        {
            VariantClear(&pucr->rgvarInputArgs[i]);
        }

        delete [] pucr->rgvarInputArgs;
        pucr->rgvarInputArgs = NULL;
        pucr->cInputArgs = 0;
    }
}

//+---------------------------------------------------------------------------
//
//  Function:   FormatControlRequest
//
//  Purpose:    Copy the action name, parameter names and values from the
//				UPNPSERVICECONTROL struct to the UPNP_CONTROL_REQUEST struct
//				
//              The existence of two different types is possibly avoidable
//				but one meshes with the COM world, and one does not.
//
//  Arguments:
//      pSvcCtl [in] pointer to the populated UPNPSERVICECONTROL struct
//		pucreq  [out] pointer to the empty UPNP_CONTROL_REQUEST struct
//
//  Returns:
//      TRUE if the fields were copied successully to pucreq
//
static
BOOL
FormatControlRequest(UPNPSERVICECONTROL *pSvcCtl, UPNP_CONTROL_REQUEST *pucreq)
{
	BOOL fRet = TRUE;
	UINT i;
	ZeroMemory(pucreq, sizeof(*pucreq));
	pucreq->bstrActionName = SysAllocString(pSvcCtl->pszAction);
	if (pSvcCtl->cInArgs)
	{
		pucreq->rgvarInputArgs = new VARIANT[pSvcCtl->cInArgs];
		if (pucreq->rgvarInputArgs)
		{
			pucreq->cInputArgs = pSvcCtl->cInArgs;
			for (i=0; i< pSvcCtl->cInArgs; i++)
			{
				V_BSTR(&pucreq->rgvarInputArgs[i]) = SysAllocString(pSvcCtl->pInArgs[i].pszValue);
				if (!V_BSTR(&pucreq->rgvarInputArgs[i]))
				{
					fRet = FALSE;	// is it ok to have a NULL value?
					break;
				}
				pucreq->rgvarInputArgs[i].vt = VT_BSTR;
			}
			
		}
		else
			fRet = FALSE;
	}
	if (!fRet)
	{
		// free any strings we allocated
		FreeControlRequest(pucreq);	
	}
	return fRet;
}

//+---------------------------------------------------------------------------
//
//  Function:   FreeControlResponse
//
//  Purpose:    Frees resources used by the fields of a UPNP_CONTROL_RESPONSE
//              structure
//
//  Arguments:
//      pucr [in] Address of the structure to cleanup
//
//  Returns:
//      (none)
//
//  Notes:
//    This function just frees the resources used by the fields within
//    the passed in structure. It does not free the memory used by the
//    structure itself.
//
static VOID
FreeControlResponse(
    IN UPNP_CONTROL_RESPONSE    * pucresp)
{
    if (pucresp->bstrActionName)
    {
        SysFreeString(pucresp->bstrActionName);
        pucresp->bstrActionName = NULL;
    }

    UPNP_CONTROL_RESPONSE_DATA * pucrd = &pucresp->ucrData;

    if (pucresp->fSucceeded)
    {
        if (pucrd->Success.rgvarOutputArgs)
        {
            for (DWORD i = 0; i < pucrd->Success.cOutputArgs; i++)
            {
                VariantClear(&pucrd->Success.rgvarOutputArgs[i]);
            }

            CoTaskMemFree(pucrd->Success.rgvarOutputArgs);
            pucrd->Success.rgvarOutputArgs = NULL;
            pucrd->Success.cOutputArgs = 0;
        }
    }
    else
    {
        if (pucrd->Fault.bstrFaultCode)
        {
            SysFreeString(pucrd->Fault.bstrFaultCode);
            pucrd->Fault.bstrFaultCode = NULL;
        }

        if (pucrd->Fault.bstrFaultString)
        {
            SysFreeString(pucrd->Fault.bstrFaultString);
            pucrd->Fault.bstrFaultString = NULL;
        }

        if (pucrd->Fault.bstrUPnPErrorCode)
        {
            SysFreeString(pucrd->Fault.bstrUPnPErrorCode);
            pucrd->Fault.bstrUPnPErrorCode = NULL;
        }

        if (pucrd->Fault.bstrUPnPErrorString)
        {
            SysFreeString(pucrd->Fault.bstrUPnPErrorString);
            pucrd->Fault.bstrUPnPErrorString = NULL;
        }
    }
}


//+---------------------------------------------------------------------------
//
//  Function:   GetServiceProxy
//
//  Purpose:    Locate the ServiceProxy object by serviceId. If it does not exist
//				attempt to create and initialize one and add it to the list.
//              
//
//  Arguments:
//      pszServiceId [in] serviceId string
//
//  Returns:
//      pointer to the ServiceProxy object or NULL if failure.
//		pointer is not AddRef'ed.
//
ServiceProxy *
DeviceProxy::GetServiceProxy(LPCWSTR pszUDN, LPCWSTR pszServiceId)
{
	ServiceProxy    *pSvc;
	HRESULT         hr;
	IDispatch       *pISO;
	
    for (pSvc=m_pServices;pSvc; pSvc = pSvc->m_pNext)
	{
		if (wcscmp(pszServiceId, pSvc->m_bstrSid) == 0 && wcscmp(pszUDN, pSvc->m_bstrUDN) == 0)
			break;
	}

	if (!pSvc)
	{
		// this may be the first time, so create one
		if (m_pDeviceControl)
		{
			ce::auto_bstr bstrUDN, bstrServiceId;

            bstrUDN = SysAllocString(pszUDN);
            bstrServiceId = SysAllocString(pszServiceId);

            hr = m_pDeviceControl->GetServiceObject(bstrUDN, bstrServiceId, &pISO);

			if (SUCCEEDED(hr))
			{
				WCHAR szSCPDPath[MAX_PATH];
				
				// get the SCPD
				UpnpGetSCPDPath(m_bstrDeviceId, pszUDN, pszServiceId, szSCPDPath, celems(szSCPDPath));

				pSvc = new ServiceProxy(this);
				
				if (pSvc)
				{
					pSvc->AddRef();		// set the ref count to 1
					
					hr = pSvc->Initialize(pszUDN, pszServiceId, szSCPDPath, pISO);
					if (SUCCEEDED(hr))
					{
						// add to service list
						pSvc->m_pNext = m_pServices;
						m_pServices = pSvc;
						// submit the initial state variables
						pSvc->OnStateChanged(0,NULL);	// 0,NULL => all variables
					}
					else
					{
						pSvc->Release();	// this should delete the object
						pSvc = NULL;
					}
				}
				
				pISO->Release();
			}
		}
	}
	return pSvc;
}

//+---------------------------------------------------------------------------
//
//  Method:   DeviceProxy::SubscribeCallback
//
//  Purpose:    Invoked when a remote subscription request is received.
//
//  Arguments:
//      UPNPSUBSCRIPTION *pUPnPSubscription
//
//  Returns:
//		TRUE if the subscription is accepted
//
BOOL
DeviceProxy::SubscribeCallback(UPNPSUBSCRIPTION *pUPnPSubscription)
{
	ServiceProxy *pServiceProxy;
    // We need to have a pointer to the implementation object by now
    if (!m_pDeviceControl)
    {
    	return FALSE;
    }

	// Find the service proxy by serviceId 
	// create if necessary
	pServiceProxy = GetServiceProxy(pUPnPSubscription->pszUDN, pUPnPSubscription->pszSID);
	
    if (!pServiceProxy)
	{
		TraceTag(ttidError, "SubscribeCallback: No service proxy for Service ID %S\n", pUPnPSubscription->pszSID);
		return FALSE;	
	}
	return TRUE;
}

//+---------------------------------------------------------------------------
//
//  Function:   ControlCallback
//
//  Purpose:    Invoked when a remote control request is received
//
//  Arguments:
//      pszXMLDesc [in] XML device description template
//
//  Returns:
//		HRESULT
//
BOOL
DeviceProxy::ControlCallback(UPNPSERVICECONTROL *pSvcCtl)
{
	HRESULT hr;
	BOOL fRet = FALSE;
	UPNP_CONTROL_REQUEST ucreq;
	UPNP_CONTROL_RESPONSE ucresp;
	ServiceProxy *pServiceProxy;
    TraceTag(ttidRegistrar, "DeviceProxy::ControlRequest\n");
    Assert(pSvcCtl->pszSID);
    Assert(m_pDeviceControl);
    // We need to have a pointer to the implementation object by now
    if (!m_pDeviceControl)
    {
    	return FALSE;
    }

	// Find the service proxy by serviceId 
	// create if necessary
	pServiceProxy = GetServiceProxy(pSvcCtl->pszUDN, pSvcCtl->pszSID);
	if (!pServiceProxy)
	{
		TraceTag(ttidError, "ControlCallback: No service proxy for Service ID %S\n", pSvcCtl->pszSID);
		return FALSE;	
	}

	// Format the parameters in the way the AutomationProxy wants them
	if (!FormatControlRequest(pSvcCtl, &ucreq))
		return FALSE;
	memset(&ucresp,0, sizeof(ucresp));
	
	hr = pServiceProxy->m_pap->ExecuteRequest(&ucreq, &ucresp);

	if (SUCCEEDED(hr))
	{
		hr = pServiceProxy->SetControlResponse(pSvcCtl, &ucresp);
		if (SUCCEEDED(hr))
		{
			fRet = TRUE;
		}
	}

	// cleanup
	::FreeControlRequest(&ucreq);
	::FreeControlResponse(&ucresp);
	
	// if we return false the UPNP service will return a generic fault
	// which is fine.
    return fRet;
}

//+---------------------------------------------------------------------------
//
//  Function:  InitCallback 
//
//  Purpose:    Invoked before any other device callbacks,
//				when the first subsribe or control request is received.
//
//  Arguments:
//
//  Returns:
//		TRUE if ok, FALSE otherwise
//
BOOL
DeviceProxy::InitCallback()
{
    TraceTag(ttidRegistrar, "DeviceProxy::InitRequest\n");
    
    // We need to have a pointer to the implementation object by now
    if (!m_pDeviceControl)
    {
    	return FALSE;
    }
    
    m_pDeviceControl->Initialize(m_bstrXMLDesc, m_bstrDeviceId, m_bstrInitString);
    
    return TRUE;		
}

//+---------------------------------------------------------------------------
//
//  Function: ShutdownCallback  
//
//  Purpose: The UPnP service wants to shutdown the device. This
//			 is the last callback to be received, so this is where we free
//			 the DeviceProxy object
//
//  Arguments:
//
//  Returns:
//		returns TRUE
//
BOOL
DeviceProxy::ShutdownCallback()
{
    TraceTag(ttidRegistrar, "DeviceProxy::ShutdowndRequest\n");
	RemoveFromList();
	delete this;
    return TRUE;
}

//+---------------------------------------------------------------------------
//
//  Function:   
//
//  Purpose:    Create and publish the UPnP device tree.
//
//  Arguments:
//      pszXMLDesc [in] XML device description template
//
//  Returns:
//		HRESULT
//
DeviceProxy::~DeviceProxy()
{
    ServiceProxy *pSvc = m_pServices;

    while (m_pServices)
    {
        LONG ref;
        pSvc = m_pServices;
        m_pServices = pSvc->m_pNext;
        pSvc->Shutdown();     // unadvise the event source
        ref = pSvc->Release(); // this should delete the service
        Assert(ref == 0);
    }
    SysFreeString(m_bstrDeviceId);
    SysFreeString(m_bstrInitString);
    SysFreeString(m_bstrResourcePath);
    SysFreeString(m_bstrUDN);
    SysFreeString(m_bstrXMLDesc);
    if (m_pDeviceControl)
    {
        m_pDeviceControl->Release();
    }
}

//
// ServiceProxy implementation
//

//+---------------------------------------------------------------------------
//
//  Function:   
//
//  Purpose:    Create and publish the UPnP device tree.
//
//  Arguments:
//      pszXMLDesc [in] XML device description template
//
//  Returns:
//		HRESULT
//
HRESULT
ServiceProxy::Initialize(PCWSTR pszUDN, PCWSTR pszSid, PCWSTR pszSCPD, IUnknown *pUPnPServiceObj)
{
	HRESULT hr;
	
    m_bstrSid = SysAllocString(pszSid);
	if (!m_bstrSid)
		return E_OUTOFMEMORY;

    m_bstrUDN = SysAllocString(pszUDN);
	if (!m_bstrUDN)
		return E_OUTOFMEMORY;

	hr = CUPnPAutomationProxy::CreateInstance(NULL, IID_IUPnPAutomationProxy, (void **)&m_pap);

	if (SUCCEEDED(hr))
	{
		hr = m_pap->Initialize(pUPnPServiceObj, (PWSTR)pszSCPD);
		m_pap->QueryInterface(IID_IUPnPServiceDescriptionInfo, (void **)&m_psdi);
		if (SUCCEEDED(hr))
		{
			// initialize eventing
			hr = pUPnPServiceObj->QueryInterface(IID_IUPnPEventSource,(void **)&m_pes);
			if (SUCCEEDED(hr))
			{
				hr = m_pes->Advise((IUPnPEventSink *)this);
				if (FAILED(hr))
				{
					m_pes->Release();
					m_pes = NULL;
				}
			}
			
		}
	}
	return hr;
}

//+---------------------------------------------------------------------------
//
//  Function:   
//
//  Purpose:    release the automation proxy and event source objects.
//             Can't put this code in the destructor, because this is where the refcount gets
//             decremented; the destructor gets called only when the refcount goes down to zero
//
//  Arguments:
//
//  Returns:
//		nothing
//
void
ServiceProxy::Shutdown()
{
	LONG cRef;
	if (m_pap)
	{
		if (m_psdi)
		{
			cRef = m_psdi->Release();
			m_psdi = NULL;
		}
		cRef = m_pap->Release();
		Assert(cRef == 0);
		m_pap = NULL;
	}
	if (m_pes)
	{
		m_pes->Unadvise((IUPnPEventSink *)this);
		m_pes->Release();
		m_pes = NULL;
	}
}

ServiceProxy::~ServiceProxy()
{
	Shutdown();     // won't do anything except perhaps in init error cases
	SysFreeString(m_bstrSid);
    SysFreeString(m_bstrUDN);
}

//+---------------------------------------------------------------------------
//
//  Member:     ServiceProxy::OnStateChanged
//
//  Purpose:    Notifies the eventing manager that the state of a service on
//              a hosted device has changed
//
//  Arguments:
//      cChanges        [in]    Number of state variables that have changed
//      rgdispidChanges [in]    Array of DISPIDs for those state variables
//
//  Returns:    S_OK if success, E_OUTOFMEMORY, or any other OLE interface
//              error code otherwise.
//  Notes:	
//
STDMETHODIMP ServiceProxy::OnStateChanged(DWORD cChanges,
                                                  DISPID rgdispidChanges[])
{                                                  
    HRESULT     hr = S_OK;
    LPWSTR      szBody = NULL;
    DWORD       cVars, ivar;
    LPWSTR *    rgszNames;
    LPWSTR *    rgszTypes;
    VARIANT *   rgvarValues;

    AssertSz(m_bstrSid, "What? Did we not get initialized or something?");
    AssertSz(m_pap, "Automation proxy not initialized?");

    if (!m_pes)
    {
        hr = E_UNEXPECTED;
    }
    /*
    else if (!cChanges && !rgdispidChanges)
    {
        // ISSUE-2000/09/21-danielwe: Correct error code?
        hr = S_OK;
    }
    */
    else if (cChanges && !rgdispidChanges)
    {
        hr = E_INVALIDARG;
    }
    else
    {
    	if (!cChanges && !rgdispidChanges)
    	{
    		TraceTag(ttidEvents, "Sending all state variables for %S\n", m_bstrSid);
    	}
        hr = m_pap->QueryStateVariablesByDispIds(cChanges, rgdispidChanges,
                                                  &cVars, &rgszNames,
                                                  &rgvarValues, &rgszTypes);
        if (SUCCEEDED(hr))
        {
        	UPNPPARAM *rgParams = new UPNPPARAM[cVars];
        	if (rgParams)
        	{
        		for (ivar = 0; ivar < cVars; ivar++)
        		{
            		rgParams[ivar].pszName = rgszNames[ivar];
            		
            		hr = VariantChangeType(rgvarValues+ivar, rgvarValues+ivar, 0, VT_BSTR);
            		if (FAILED(hr))
            		{
            			TraceError("OnStateChanged:Can't change arg type to BSTR\n", hr);
            			break;
            		}
            		rgParams[ivar].pszValue = V_BSTR(rgvarValues+ivar);
        		}
        		if (SUCCEEDED(hr) && !UpnpSubmitPropertyEvent(m_pDevProxy->Name(), m_bstrUDN, m_bstrSid, cVars, rgParams))
        		{
        			TraceTag(ttidError,"OnStateChanged: UpnpSubmitPropertyEvent failed error=%d\n", GetLastError());
        			hr = HrFromLastWin32Error();
        		}
        		delete [] rgParams;
        	}
        	else
        		hr = E_OUTOFMEMORY;
            


            for (ivar = 0; ivar < cVars; ivar++)
            {
                CoTaskMemFree(rgszTypes[ivar]);
                CoTaskMemFree(rgszNames[ivar]);
                VariantClear(&rgvarValues[ivar]);
            }

            CoTaskMemFree(rgvarValues);
            CoTaskMemFree(rgszTypes);
            CoTaskMemFree(rgszNames);
        }
    }

    TraceError("ServiceProxy::OnStateChanged", hr);
    return hr;
}

//+---------------------------------------------------------------------------
//
//  Member:     CUPnPEventingManager::OnStateChangedSafe
//
//  Purpose:    Same as OnStateChanged, except this is for VB users that need
//              to pass the array of DISPIDs in a SafeArray.
//
//  Arguments:
//      psa     [in]    SafeArray of DISPIDs that have changed
//
//  Returns:    Same as OnStateChanged
//
//  /09/21
//
//  Notes:	From Whistler
//
STDMETHODIMP ServiceProxy::OnStateChangedSafe(VARIANT varsadispidChanges)
{
    HRESULT         hr = S_OK;
    SAFEARRAY *psa = V_ARRAY(&varsadispidChanges);
    DISPID HUGEP *  rgdispids;

    // Get a pointer to the elements of the array.
    hr = SafeArrayAccessData(psa, (void HUGEP**)&rgdispids);
    if (SUCCEEDED(hr))
    {
        hr = OnStateChanged(psa->rgsabound[0].cElements, rgdispids);
        SafeArrayUnaccessData(psa);
    }

    TraceError("CUPnPEventingManager::OnStateChangedSafe", hr);
    return hr;
}

//+---------------------------------------------------------------------------
//
//  Function:   
//
//  Purpose:    Create and publish the UPnP device tree.
//
//  Arguments:
//      pszXMLDesc [in] XML device description template
//
//  Returns:
//		HRESULT
//
HRESULT
ServiceProxy::SetControlResponse(UPNPSERVICECONTROL *pSvcCtl, UPNP_CONTROL_RESPONSE *pucresp)
{
	HRESULT hr = S_OK;
	
	if (pucresp->fSucceeded)
	{
		UPNPPARAM *pParams;
        DWORD i, cOutputArgs = 0;
		VARIANT *pvar;
        BSTR  * rgbstrNames = NULL;
        BSTR  * rgbstrTypes = NULL;
        
		// success response
		if (!pucresp->ucrData.Success.cOutputArgs)
			return S_OK;	// the UPNP service will return a default success response

		pParams = new UPNPPARAM [pucresp->ucrData.Success.cOutputArgs];
		if (!pParams)
			return E_OUTOFMEMORY;

		Assert(m_psdi);
		if (wcscmp(pSvcCtl->pszAction, L"QueryStateVariable") == 0)
		{
			pParams[0].pszName = L"return";
			pvar = &pucresp->ucrData.Success.rgvarOutputArgs[0];
			hr = VariantChangeType(pvar,pvar,0,VT_BSTR);
			if (SUCCEEDED(hr))
			{
				pParams[0].pszValue = V_BSTR(pvar);
				cOutputArgs = 1;
			}
		}
		else
		{
			// one or more output vars need to be specified
			// unfortunately we don't have the var names in pucresp (that would be too easy)
			// so they need to be determined
            hr = m_psdi->GetOutputArgumentNamesAndTypes(
                	pucresp->bstrActionName,
                	&cOutputArgs,
                	&rgbstrNames,
                	&rgbstrTypes);

            if (SUCCEEDED(hr))
            {
            	Assert(cOutputArgs == pucresp->ucrData.Success.cOutputArgs);
            	
            	for (i=0; i < cOutputArgs; i++)
            	{
            		pParams[i].pszName = rgbstrNames[i];
            		pvar = &pucresp->ucrData.Success.rgvarOutputArgs[i];
            		hr = VariantChangeType(pvar, pvar, 0, VT_BSTR);
            		if (FAILED(hr))
            		{
            			TraceError("SetControlResponse:Can't change arg type to BSTR\n", hr);
            			break;
            		}
            		pParams[i].pszValue = V_BSTR(pvar);
            	}

            }
		}
    	if (SUCCEEDED(hr))
    	{
    		if (!UpnpSetControlResponse(pSvcCtl, cOutputArgs, pParams))
    		{
    			TraceTag(ttidError, "UpnpSetControlResponse returned error %d for %S\n",
    				GetLastError(), pSvcCtl->pszAction);

    			hr = HrFromLastWin32Error();
    		}
    	}
        // Clean up.
        if (rgbstrNames)
        {
	        for (i = 0; i < cOutputArgs; i++)
	        {
	            SysFreeString(rgbstrNames[i]);
	            rgbstrNames[i] = NULL;
	            SysFreeString(rgbstrTypes[i]);
	            rgbstrTypes[i] = NULL;
	        }
	        CoTaskMemFree(rgbstrNames);
	        rgbstrNames = NULL;
	        CoTaskMemFree(rgbstrTypes);
	        rgbstrTypes = NULL;
        }
		delete [] pParams;
		
	}
	else
	{
		int errCode = 501;	// Action failed
		Assert(pucresp->ucrData.Fault.bstrUPnPErrorCode);
		if (pucresp->ucrData.Fault.bstrUPnPErrorCode)
		{
			errCode = _wtol(pucresp->ucrData.Fault.bstrUPnPErrorCode);
		}
		// error response
		if (!UpnpSetErrorResponse(pSvcCtl, errCode, pucresp->ucrData.Fault.bstrUPnPErrorString))
		{
			TraceTag(ttidError, "UpnpSetControlResponse returned error %d for %S\n",
				GetLastError(), pSvcCtl->pszAction);

			hr = HrFromLastWin32Error();
		}
	}
	return hr;
}

