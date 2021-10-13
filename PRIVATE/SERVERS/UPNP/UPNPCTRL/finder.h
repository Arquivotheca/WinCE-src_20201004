//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _FINDER_H_
#define _FINDER_H_

#define DLLSVC 1
#include <combook.h>

#include "DispatchImpl.hxx"
#include "ConnectionPoint.h"
#include "list.hxx"

class FinderCallback : ConnectionPoint::ICallback
{
    ~FinderCallback()
    {
        stop_listening();
        
        if(bstrSearchTarget)
            SysFreeString(bstrSearchTarget);

        if(cb.pCallback)
	        cb.pCallback->Release();
    }

public:	
    FinderCallback()
        : dwSig(0),
          fDispatch(FALSE),
          bstrSearchTarget(NULL),
          dwFlags(0),
          hFind(NULL),
          pNext(NULL),
          dwCookie(0),
          lRefCount(1)
    {
        cb.pCallback = NULL;
        cb.pDispatch = NULL;
    }

    void Release()
    {
        if(InterlockedDecrement(&lRefCount) == 0)
            delete this;
    }

    void AddRef()
    {
        InterlockedIncrement(&lRefCount);
    }
    
    DWORD						dwSig;
	union Callback
	{
		IUPnPDeviceFinderCallback	*pCallback;
		IDispatch					*pDispatch;
	};

	Callback					cb;
	BOOL						fDispatch;

	BSTR						bstrSearchTarget;
	DWORD						dwFlags;
    HANDLE						hFind;
	FinderCallback	            *pNext;
	
    struct Device
    {
        Device(LPCSTR pszUSN, LPCSTR pszLocation, LPCSTR pszNls)
            : strUSN(pszUSN),
              strLocation(pszLocation),
              strNls(pszNls)
        {}

        ce::string strUSN;
        ce::string strLocation;
        ce::string strNls;
    };
    
    ce::list<Device>            listReported;

    void start_listening()
    {
        g_pConnectionPoint->advise(bstrSearchTarget, 0, this, &dwCookie);
    }

    void stop_listening()
    {
        g_pConnectionPoint->unadvise(dwCookie);
        dwCookie = 0;
    }

// ConnectionPoint::ICallback
private:	
	virtual void StateVariableChanged(LPCWSTR pwszName, LPCWSTR pwszValue);
	virtual void ServiceInstanceDied(LPCWSTR pszUSN);
    virtual void AliveNotification(LPCWSTR pszUSN, LPCWSTR pszLocation, LPCWSTR pszNls, DWORD dwLifeTime);

private:
    DWORD                   dwCookie;
    LONG                    lRefCount;
};


class CUPnPDeviceFinder : public ce::DispatchImpl<IUPnPDeviceFinder, &IID_IUPnPDeviceFinder>, 
						  public SVSSynch
{
public:
	CUPnPDeviceFinder()
		: _pfcbFirst(NULL)
	{}

	DEFAULT_CLASS_REGISTRY_TABLE(CUPnPDeviceFinder,
								 TEXT("{E2085F28-FEB7-404A-B8E7-E659BDEAAA02}"),	// CLSID
								 TEXT("UPnP DeviceFinder Class"),					// friendly name
								 TEXT("UPnP.UPnPDeviceFinder.1"),					// ProgID
								 TEXT("UPnP.UPnPDeviceFinder"),						// version independent ProgID
								 TEXT("Both"))										// threading model

// implement GetInterfaceTable
    BEGIN_INTERFACE_TABLE(CUPnPDeviceFinder)
        IMPLEMENTS_INTERFACE(IUPnPDeviceFinder)
        IMPLEMENTS_INTERFACE(IDispatch)
    END_INTERFACE_TABLE()

// implement QueryInterface/AddRef/Release
    IMPLEMENT_UNKNOWN(CUPnPDeviceFinder);

// implement static CreateInstance(IUnknown *, REFIID, void**)
    IMPLEMENT_CREATE_INSTANCE(CUPnPDeviceFinder);

// implement static GetClassObject(REFIID, void **) that uses a GenericClassFactory
    IMPLEMENT_GENERIC_CLASS_FACTORY(CUPnPDeviceFinder);

public:
// IUPnPDeviceFinder
    STDMETHOD(FindByType)(/* [in] */ BSTR bstrTypeURI,
                          /* [in] */ DWORD dwFlags,
                          /* [out, retval] */ IUPnPDevices ** pDevices);

    STDMETHOD(CreateAsyncFind)(/* [in] */ BSTR bstrTypeURI,
                               /* [in] */ DWORD dwFlags,
                               /* [in] */ IUnknown __RPC_FAR *punkDeviceFinderCallback,
                               /* [retval][out] */ LONG __RPC_FAR *plFindData);

    STDMETHOD(StartAsyncFind)(/* [in] */ LONG lFindData);

    STDMETHOD(CancelAsyncFind)(/* [in] */ LONG lFindData);

    STDMETHOD(FindByUDN)(/*[in]*/ BSTR bstrUDN,
                        /*[out, retval]*/ IUPnPDevice ** ppDevice);

protected:
	HRESULT Find(BSTR bstrType, IUPnPDevices **ppDevices, IUPnPDevice **pDevice);

private:
	FinderCallback *_pfcbFirst;
};

#endif
