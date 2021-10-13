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
#include "DeviceDescription.h"
#include "ssdpapi.h"

class FinderCallback : ConnectionPoint::ICallback
{
    ~FinderCallback()
    {
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
        hEventRelease = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
    
    BOOL Valid()
    {
        return hEventRelease.valid();
    }
    
    void Release()
    {
        ASSERT(lRefCount >= 1);
        
        if(InterlockedDecrement(&lRefCount) == 0)
            delete this;
        else
            SetEvent(hEventRelease);
    }
    
    void FinalRelease()
    {
        ASSERT(lRefCount >= 1);
        
	    stop_listening();
	    
        while(lRefCount > 1)
	        WaitForSingleObject(hEventRelease, INFINITE);
	    
        ASSERT(lRefCount == 1);
	    
	    Release();
    }

    void AddRef()
    {
        ASSERT(lRefCount >= 1);
        
        InterlockedIncrement(&lRefCount);
    }
    
    void Lock()
    {
        cs.lock();
    }
    
    void Unlock()
    {
        cs.unlock();
    }
    
    void RemoveReportedDevice(LPCSTR pszUSN)
    {
        Lock();
        
        for(ce::list<FinderCallback::Device>::iterator it = listReported.begin(), itEnd = listReported.end(); it != itEnd; ++it)
            if(it->strUSN == pszUSN)
            {
                listReported.erase(it);
                break;
            }
        
        Unlock();
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

protected:
    class AsyncDeviceLoader : public IUPnPDescriptionDocumentCallback
    {
    public:
        AsyncDeviceLoader(FinderCallback* pfcb, LPCSTR pszUSN, LPCSTR pszUrl, LPCSTR pszNls, UINT nLifeTime)
            : m_pfcb(pfcb),
              m_strUSN(pszUSN),
              m_strLocHeader(pszUrl),
              m_strNls(pszNls),
              m_lRefCount(0)
        {
            m_pfcb->AddRef();
            
            if(m_pDescription = new DeviceDescription(nLifeTime))
                m_pDescription->AddRef();
        }
        
        ~AsyncDeviceLoader()
        {
            if(m_pDescription)
                m_pDescription->Release();
            
            m_pfcb->Release();
        }
        
        HRESULT LoadAsync(LPCWSTR pszURL)
        {
            if(!m_pDescription)
                return E_OUTOFMEMORY;
                
            ce::auto_bstr bstrURL = SysAllocString(pszURL);
            
            return m_pDescription->LoadAsync(bstrURL, this);
        }
    
    public:
        // IUnknown
        ULONG STDMETHODCALLTYPE AddRef()
        {
            InterlockedIncrement(&m_lRefCount);
            
            return m_lRefCount;
        }
    
        ULONG STDMETHODCALLTYPE Release()
        {
            if(InterlockedDecrement(&m_lRefCount) == 0)
            {
                delete this;
                return 0;
            }
            else    
                return m_lRefCount;
        }
        
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject)
        {
            if(ce::InlineIsEqualGUID(iid, IID_IUnknown) || 
               ce::InlineIsEqualGUID(iid, IID_IUPnPDescriptionDocumentCallback))
            {
                *ppvObject = this;
                
                AddRef();

                return S_OK;
            }
            else
                return E_NOINTERFACE;
        }

        // IUPnPDescriptionDocumentCallback
        virtual HRESULT STDMETHODCALLTYPE LoadComplete(/* [in] */HRESULT hrLoadResult);
        
    private:
        DeviceDescription*  m_pDescription;
        FinderCallback*     m_pfcb;
        ce::string          m_strUSN, m_strLocHeader, m_strNls;
        LONG                m_lRefCount;
    };

public:
    void PerformCallback(SSDP_CALLBACK_TYPE cbType, BSTR bstrUDN, IUPnPDevice* pUPnPDevice);
    void GetUPnPDeviceAsync(LPCSTR pszUSN, LPCSTR pszUrl, LPCSTR pszNls, UINT nLifeTime);

// ConnectionPoint::ICallback
private:	
	virtual void StateVariableChanged(LPCWSTR pwszName, LPCWSTR pwszValue);
	virtual void ServiceInstanceDied(LPCWSTR pszUSN);
    virtual void AliveNotification(LPCWSTR pszUSN, LPCWSTR pszLocation, LPCWSTR pszNls, DWORD dwLifeTime);

private:
    DWORD                   dwCookie;
    LONG                    lRefCount;
    ce::critical_section    cs;
    ce::auto_handle         hEventRelease;
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
