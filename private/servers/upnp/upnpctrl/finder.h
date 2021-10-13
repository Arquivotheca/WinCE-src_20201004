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
#ifndef _FINDER_H_
#define _FINDER_H_

#define DLLSVC 1
#include <combook.h>

#include "DispatchImpl.hxx"
#include "ConnectionPoint.h"
#include "list.hxx"
#include "DeviceDescription.h"
#include "ssdpapi.h"
#include "upnpmem.h"

class FinderCallback : ConnectionPoint::ICallback
{
    ~FinderCallback()
    {
        if(bstrSearchTarget)
        {
            SysFreeString(bstrSearchTarget);
        }

        if(cb.pCallback)
        {
            cb.pCallback->Release();
        }
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
          lRefCount(1),
          fReleasing(FALSE)
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

        long refCnt = InterlockedDecrement(&lRefCount);
        if(refCnt == 0)
        {
            delete this;
        }
        else
        {
            SetEvent(hEventRelease);
        }

        ASSERT(lRefCount >= 1 || fReleasing);
    }

    void FinalRelease()
    {
        ASSERT(lRefCount >= 1);
        fReleasing = TRUE;

        stop_listening();

        // poll every 30s to make sure that the lRefCount hasn't decreased without a signal
        // this will resolve hang issues if there is a problem with AddRef/Release code
        while(lRefCount > 1)
        {
            WaitForSingleObject(hEventRelease, 30000);
        }

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
        {
            if(it->strUSN == pszUSN)
            {
                listReported.erase(it);
                break;
            }
        }

        Unlock();
    }

    void RemoveReportedDevice(LPCSTR pszUSN, LPCSTR pszURL)
    {
        Lock();
        int i;
        bool bFound = false;
        ce::list<FinderCallback::Device>::iterator it, itEnd;

        // Find the entry
        for(it = listReported.begin(), itEnd = listReported.end(); it != itEnd; ++it)
        {
            if(it->strUSN == pszUSN)
            {
                for (i = 0; i < _countof(it->rgstrLocations); i++)
                {
                    if (it->rgstrLocations[i] == pszURL)
                    {
                        bFound = true;
                        break;
                    }
                }
                if (bFound)
                {
                    break;
                }
            }
        }

        // Delete the URL entry, if found
        if (bFound)
        {
            do
            {
                // Erase this entry
                it->rgstrLocations[i] = "";
                it->rgstrNls[i] = "";

                // Copy down the next
                if (i + 1 < _countof(it->rgstrLocations))
                {
                    it->rgstrLocations[i] = it->rgstrLocations[i+1];
                    it->rgstrNls[i] = it->rgstrNls[i+1];
                }
            } while (++i < _countof(it->rgstrLocations));
        }


        // Delete the USN entry, if now empty
        if (bFound && it->rgstrLocations[0] == "")
        {
            listReported.erase(it);
        }

        Unlock();
    }

    DWORD                       dwSig;
    union Callback
    {
        IUPnPDeviceFinderCallback   *pCallback;
        IDispatch                   *pDispatch;
    };

    Callback                    cb;
    BOOL                        fDispatch;

    BSTR                        bstrSearchTarget;
    DWORD                       dwFlags;
    HANDLE                      hFind;
    FinderCallback              *pNext;

    struct Device
    {
        Device(LPCSTR pszUSN, LPCSTR pszLocation, LPCSTR pszNls)
            : strUSN(pszUSN)
        {
              rgstrLocations[0] = pszLocation;
              rgstrNls[0] = pszNls;
        }

        ce::string strUSN;
        ce::string rgstrLocations[NUM_OF_URLS];
        ce::string rgstrNls[NUM_OF_URLS];
    };

    ce::list<Device>            listReported;

    void start_listening()
    {
        if(g_pConnectionPoint)
        {
            g_pConnectionPoint->advise(bstrSearchTarget, 0, this, &dwCookie);
        }
    }

    void stop_listening()
    {
        if(g_pConnectionPoint)
        {
            g_pConnectionPoint->unadvise(dwCookie);
        }
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
            {
                m_pDescription->AddRef();
            }
        }

        ~AsyncDeviceLoader()
        {
            if(m_pDescription)
            {
                m_pDescription->Release();
            }

            m_pfcb->Release();
        }

        HRESULT LoadAsync(LPCWSTR pszURL)
        {
            if(!m_pDescription)
            {
                return E_OUTOFMEMORY;
            }

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
            long refCnt = InterlockedDecrement(&m_lRefCount);
            if(refCnt == 0)
            {
                delete this;
                return 0;
            }
            else
            {
                return m_lRefCount;
            }
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
            {
                return E_NOINTERFACE;
            }
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
    BOOL                    fReleasing;
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
                                 TEXT("{E2085F28-FEB7-404A-B8E7-E659BDEAAA02}"),    // CLSID
                                 TEXT("UPnP DeviceFinder Class"),                   // friendly name
                                 TEXT("UPnP.UPnPDeviceFinder.1"),                   // ProgID
                                 TEXT("UPnP.UPnPDeviceFinder"),                     // version independent ProgID
                                 TEXT("Both"))                                      // threading model

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
