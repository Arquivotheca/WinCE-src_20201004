//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#include <windows.h>
#include <upnpdevapi.h>
#include <ncdefine.h>
#include <ncbase.h>
#include <ncdebug.h>
#include <linklist.h>
#include "trace.h"
#include <guidgen.h>
#include <regentry.h>
#include <upnphostkeys.h>

#define DLLSVC	1 	//include inproc DLL defns from combook.h
#include "combook.h"
#include "upnphost.h"
#include "automationproxy.h"

class ServiceProxy : public IUPnPEventSink
{
    friend class DeviceProxy;
    private:
    class ServiceProxy *m_pNext;
    BSTR m_bstrUDN;
    BSTR m_bstrSid;
    class DeviceProxy *m_pDevProxy;
    IUPnPAutomationProxy *m_pap;
    IUPnPServiceDescriptionInfo *m_psdi;
    IUPnPEventSource *m_pes;

    
// implement GetInterfaceTable
    BEGIN_INTERFACE_TABLE(ServiceProxy)
        IMPLEMENTS_INTERFACE(IUPnPEventSink)
    END_INTERFACE_TABLE()

// implement QueryInterface/AddRef/Release
    IMPLEMENT_UNKNOWN(ServiceProxy)

    public:
    ServiceProxy(class DeviceProxy *pOwner) :
        m_pNext(NULL),
        m_bstrSid(NULL),
        m_pap(NULL),
        m_psdi(NULL),
        m_pes(NULL)
    {
        m_pDevProxy = pOwner;
    };
    ~ServiceProxy();

    HRESULT Initialize(PCWSTR pszUDN, PCWSTR pszSid, PCWSTR pszSCPD, IUnknown *pUPnPServiceObj );
    void Shutdown();
    HRESULT SetControlResponse(UPNPSERVICECONTROL *pSvcCtl, UPNP_CONTROL_RESPONSE *pucresp); 
//  IUPnPEventSink methods
    
    STDMETHOD (OnStateChanged) ( 
            /* [in] */ DWORD cChanges,
            /* [size_is][in] */ DISPID rgdispidChanges[  ]);
        
    STDMETHOD(OnStateChangedSafe)( 
            /* [in] */ VARIANT sadispidChanges);

};           

class DeviceProxy 
{
    private:
    BSTR m_bstrDeviceId;
    BSTR m_bstrInitString;
    BSTR m_bstrUDN;
    BSTR m_bstrResourcePath;
    BSTR m_bstrXMLDesc;
    LONG m_nLifeTime;
    IUPnPDeviceControl *m_pDeviceControl;
    ServiceProxy *m_pServices;  // list of services

    // private methods
    
    ServiceProxy *GetServiceProxy(LPCWSTR pszUDN, LPCWSTR pszServiceId);
    
    static DWORD DevCallback( 
        UPNPCB_ID callbackId, 
        PVOID pvUserContext,    // app context (from UPNPDEVICEINFO)
        PVOID pvSvcParam);    // depends on CALLBACKID
    BOOL ControlCallback(UPNPSERVICECONTROL *pSvcCtl);
    BOOL InitCallback();
    BOOL SubscribeCallback(UPNPSUBSCRIPTION *pUPnPSubscription);
    BOOL ShutdownCallback();
        
    
    public:
    LIST_ENTRY m_link;
    
    DeviceProxy(PCWSTR pszDeviceId,
                PCWSTR pszInitString,
                PCWSTR pszResourcePath,
                PCWSTR pszXMLDesc,
                LONG nLifetime,
                IUnknown *punkDevControl,
                PCWSTR pszUDN = NULL) :
                    m_pServices(NULL)
    {
        m_bstrDeviceId = SysAllocString(pszDeviceId);
        m_bstrInitString = SysAllocString(pszInitString);
        m_bstrResourcePath = SysAllocString(pszResourcePath);
        m_bstrUDN = SysAllocString(pszUDN);	// may be NULL
        m_bstrXMLDesc = SysAllocString(pszXMLDesc);
        m_nLifeTime = nLifetime;
        m_pDeviceControl = NULL;
        if (punkDevControl)
        {
            punkDevControl->QueryInterface(IID_IUPnPDeviceControl, (void **)&m_pDeviceControl);
        }
        InitializeListHead(&m_link);
    }
    ~DeviceProxy();
    
    void AddToList(LIST_ENTRY *pList)
    {
        InsertHeadList(pList, &m_link);
    }
    void RemoveFromList()
    {
    	if (m_link.Flink == NULL ||  m_link.Blink == NULL)
    		return;	// not on the list
        RemoveEntryList(&m_link);
        m_link.Flink = m_link.Blink = NULL;
    }
    BSTR Name() { return m_bstrDeviceId; }
    HRESULT AddDevice(LPWSTR pszXMLDesc);
    HRESULT RemoveDevice();
    
    BOOL InitOK() { return (m_pDeviceControl && m_bstrDeviceId); }
    
    
};


class UPnPRegistrar : 
	public IUPnPRegistrar,
    public IUPnPReregistrar
{
public:    
	DEFAULT_CLASS_REGISTRY_TABLE(UPnPRegistrar,
								 TEXT("{204810b9-73b2-11d4-bf42-00b0d0118b56}"),// CLSID
								 TEXT("UPnP Registrar Class"),					// friendly name
								 TEXT("UPnP.UPnPRegistrar.1"),					// ProgID
								 TEXT("UPnP.UPnPRegistrar"),					// version independent ProgID
								 TEXT("free"))									// threading model

// implement GetInterfaceTable
    BEGIN_INTERFACE_TABLE(UPnPRegistrar)
        IMPLEMENTS_INTERFACE(IUPnPRegistrar)
        IMPLEMENTS_INTERFACE(IUPnPReregistrar)
    END_INTERFACE_TABLE()

// implement QueryInterface/AddRef/Release
    IMPLEMENT_UNKNOWN(UPnPRegistrar)

// implement static CreateInstance(IUnknown *, REFIID, void**)
    IMPLEMENT_CREATE_INSTANCE(UPnPRegistrar)

// implement static GetClassObject(REFIID, void **) that uses a GenericClassFactory
    IMPLEMENT_GENERIC_CLASS_FACTORY(UPnPRegistrar);

    // IUPnPRegistrar methods
    STDMETHOD(RegisterDevice)(
        /*[in]*/ BSTR     bstrXMLDesc,
        /*[in]*/ BSTR     bstrProgIDDeviceControlClass,
        /*[in]*/ BSTR     bstrInitString,
        /*[in]*/ BSTR     bstrContainerId,
        /*[in]*/ BSTR     bstrResourcePath,
        /*[in]*/ long     nLifeTime,
        /*[out, retval]*/ BSTR * pbstrDeviceIdentifier);
    STDMETHOD(RegisterRunningDevice)(
        /*[in]*/ BSTR     bstrXMLDesc,
        /*[in]*/ IUnknown * punkDeviceControl,
        /*[in]*/ BSTR     bstrInitString,
        /*[in]*/ BSTR     bstrResourcePath,
        /*[in]*/ long     nLifeTime,
        /*[out, retval]*/ BSTR * pbstrDeviceIdentifier);
    STDMETHOD(RegisterDeviceProvider)(
        /*[in]*/ BSTR     bstrProviderName,
        /*[in]*/ BSTR     bstrProgIDProviderClass,
        /*[in]*/ BSTR     bstrInitString,
        /*[in]*/ BSTR     bstrContainerId);
    STDMETHOD(GetUniqueDeviceName)(
        /*[in]*/          BSTR   bstrDeviceIdentifier,
        /*[in]*/          BSTR   bstrTemplateUDN,
        /*[out, retval]*/ BSTR * pbstrUDN);
    STDMETHOD(UnregisterDevice)(
        /*[in]*/ BSTR     bstrDeviceIdentifier,
        /*[in]*/ BOOL     fPermanent);
    STDMETHOD(UnregisterDeviceProvider)(
        /*[in]*/ BSTR     bstrProviderName);

    // IUPnPReregistrar methods
    STDMETHOD(ReregisterDevice)(
        /*[in]*/ BSTR     bstrDeviceIdentifier,
        /*[in]*/ BSTR     bstrXMLDesc,
        /*[in]*/ BSTR     bstrProgIDDeviceControlClass,
        /*[in]*/ BSTR     bstrInitString,
        /*[in]*/ BSTR     bstrContainerId,
        /*[in]*/ BSTR     bstrResourcePath,
        /*[in]*/ long     nLifeTime);
    STDMETHOD(ReregisterRunningDevice)(
        /*[in]*/ BSTR     bstrDeviceIdentifier,
        /*[in]*/ BSTR     bstrXMLDesc,
        /*[in]*/ IUnknown * punkDeviceControl,
        /*[in]*/ BSTR     bstrInitString,
        /*[in]*/ BSTR     bstrResourcePath,
        /*[in]*/ long     nLifeTime);
        

	UPnPRegistrar()
	{
		InitializeListHead(&m_DevProxyList);
	}
	private:
	static HRESULT HrCreateDeviceIdentifier(BSTR *pbstrDevId);
	DeviceProxy *FindDevProxyByName(PCWSTR pszDevId);

	LIST_ENTRY m_DevProxyList;
	

};

