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

//
// Declarations for internal implementation of the UPnP AV toolkit's control point functionality
//

#ifndef __AV_UPNP_CTRL_INTERNAL_H
#define __AV_UPNP_CTRL_INTERNAL_H

#include <hash_map.hxx>
#include <hash_set.hxx>
#include <sync.hxx>
#include <auto_xxx.hxx>

#ifndef UNDER_CE
#   include "desktop.h"
#endif

#include "sax.h"
#include "upnp_proxy.h"
#include "av_upnp.h"


namespace av_upnp
{
namespace details
{

extern DWORD AVErrorFromUPnPError(HRESULT hr);
extern bool GetService(IUPnPServices* pServices, LPCWSTR pszServiceID, LPCWSTR pszServiceType, IUPnPService** ppService);

// IEventSourceImpl
template<typename T>
class IEventSourceImpl : public T,
                         public IEventSink
{
public:
    virtual ~IEventSourceImpl()
    {
        // Every call to Advise must be matched with a call to Unadvise
        assert(m_setSink.empty());
    }

// av::IEventSource
public:
    // Advise
    virtual DWORD Advise(
            /*[in]*/ IEventSink *pSubscriber)
    {
        if(!pSubscriber)
        {
            return ERROR_AV_POINTER;
        }
        
        ce::gate<ce::critical_section_with_copy> gate(m_csSinkSet);
    
        if(m_setSink.end() != m_setSink.insert(pSubscriber))
        {
            return SUCCESS_AV;
        }
        else
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OOM when inserting subscriber")));
            return ERROR_AV_OOM;
        }
    }

    // Unadvise
    virtual DWORD Unadvise(
            /*[in]*/ IEventSink *pSubscriber)
    {
        ce::gate<ce::critical_section_with_copy> gate(m_csSinkSet);
    
        if(!m_setSink.erase(pSubscriber))
        {
            return ERROR_AV_POINTER;
        }
        else
        {
            return SUCCESS_AV;
        }
    }

// av::IEventSink
public:
    virtual DWORD OnStateChanged(
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszValue)
    {
        ce::gate<ce::critical_section_with_copy> gate(m_csSinkSet);
        
        for(EventSinkSet::iterator it = m_setSink.begin(), itEnd = m_setSink.end(); it != itEnd; ++it)
        {
            (*it)->OnStateChanged(pszStateVariableName, pszValue);
        }
        
        return SUCCESS_AV;
    }
        
    virtual DWORD OnStateChanged(
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  long nValue)
    {
        ce::gate<ce::critical_section_with_copy> gate(m_csSinkSet);
        
        for(EventSinkSet::iterator it = m_setSink.begin(), itEnd = m_setSink.end(); it != itEnd; ++it)
        {
            (*it)->OnStateChanged(pszStateVariableName, nValue);
        }
        
        return SUCCESS_AV;
    }
    
    virtual DWORD OnStateChanged(
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszChannel,
        /*[in]*/  long nValue)
    {
        ce::gate<ce::critical_section_with_copy> gate(m_csSinkSet);
        
        for(EventSinkSet::iterator it = m_setSink.begin(), itEnd = m_setSink.end(); it != itEnd; ++it)
        {
            (*it)->OnStateChanged(pszStateVariableName, pszChannel, nValue);
        }
            
        return SUCCESS_AV;
    }
        
protected:
    typedef ce::hash_set<IEventSink*> EventSinkSet;

    ce::critical_section_with_copy  m_csSinkSet;
    EventSinkSet                    m_setSink;
};


//
// IUPnPServiceCallbackImpl
//
class IUPnPServiceCallbackImpl : public IUPnPServiceCallback
{
// IUPnPServiceCallback
public:
    // ServiceInstanceDied
    STDMETHOD(ServiceInstanceDied)(
            /* [in] */ IUPnPService*)
    {
        return S_OK;
    }
    
// IUnknown
public:
    STDMETHOD(QueryInterface)(
            /* [in] */ REFIID riid,
            /* [in] */ LPVOID* ppv)
    {
        if(InlineIsEqualGUID(riid, IID_IUnknown) || 
           InlineIsEqualGUID(riid, IID_IUPnPServiceCallback))
        {
            *ppv = this;
            return S_OK;
        }
        else
        {
            return E_NOINTERFACE;
        }
    }

    STDMETHOD_(ULONG, AddRef)(void)
        {return 1; }

    STDMETHOD_(ULONG, Release)(void)
        {return 1; }    
};


//
// ServiceCtrl
// 
template<typename T>
class ServiceCtrl : public IEventSourceImpl<T>,
                    protected IUPnPServiceCallbackImpl
{
public:
    ~ServiceCtrl()
    {
        if(m_pService)
        {
            m_pService->RemoveCallback(m_dwCookie);
        }
    }
    
    bool Init(IUPnPService* pService, IUPnPDevice* pDevice)
    {
        assert(pService);
        assert(pDevice);
        
        HRESULT hr;
        
        m_pService = pService;
        
        if(FAILED(hr = m_pService->AddCallback(static_cast<IUPnPServiceCallbackImpl*>(this), &m_dwCookie)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("AddCallback failed with error 0x%08x"), hr));
            return false;
        }
        
        return true;
    }

// IUPnPServiceCallback
public:
    // StateVariableChanged
    STDMETHOD(StateVariableChanged)(
            /* [in] */ IUPnPService*,
            /* [in] */ LPCWSTR pcwszStateVarName,
            /* [in] */ VARIANT varValue)
    {
#ifdef DEBUG
        ce::variant varTemp;
        
        if(FAILED(VariantChangeType(&varTemp, &varValue, 0, VT_BSTR)))
        {
            varTemp.bstrVal = L"<unknown>";
        }

        DEBUGMSG(ZONE_AV_TRACE, (AV_TEXT("Event: %s=%s"), pcwszStateVarName, varTemp.bstrVal));
#endif        
        
        if(varValue.vt == VT_BSTR)
        {
            OnStateChanged(pcwszStateVarName, varValue.bstrVal);
        }
        else
        {
            ce::variant varDest;
            
            if(SUCCEEDED(VariantChangeType(&varDest, &varValue, 0, VT_I4)))
            {
                OnStateChanged(pcwszStateVarName, varDest.lVal);
            }
            else
            {
                DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("Received event of unsupported type %d for variable \"%s\""), varValue.vt, pcwszStateVarName));
            }
        }
        
        return S_OK;
    }
    
protected:
    CComPtr<IUPnPService> m_pService;
    DWORD                 m_dwCookie;    
};


//
// LastChangeParsing
//
class LastChangeParsing : protected ce::SAXContentHandler
{
public:
    LastChangeParsing(LPCWSTR pszNamespace);
    
// ISAXContentHandler
protected:
    virtual HRESULT STDMETHODCALLTYPE startDocument(void);
    
    virtual HRESULT STDMETHODCALLTYPE startElement(
        /* [in] */ const WCHAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const WCHAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const WCHAR *pwchQName,
        /* [in] */ int cchQName,
        /* [in] */ ISAXAttributes *pAttributes);
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const WCHAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const WCHAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const WCHAR *pwchQName,
        /* [in] */ int cchQName);

protected:
    virtual DWORD OnStateChanged(
        /*[in]*/  int     InstanceID,
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszValue) = 0;
        
    virtual DWORD OnStateChanged(
        /*[in]*/  int     InstanceID,
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  long    nValue) = 0;

    virtual DWORD OnStateChanged(
        /*[in]*/  int     InstanceID,
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszChannel,
        /*[in]*/  long    nValue) = 0;

protected:
    struct StateVar
    {
        StateVar(bool bNumeric)
            : bNumeric(bNumeric)
            {}
        
        bool bNumeric;
    };
    
    int                             m_InstanceID;
    int                             m_nLevelsUnderInstanceID;
    bool                            m_bUnderInstanceID;
    wstring                         m_strEventElement;
    wstring                         m_strInstanceElement;
    ce::hash_map<wstring, StateVar> m_mapStateVars;
};


//
// VirtualServiceCtrl
//
template<typename I, typename T>
class VirtualServiceCtrl : private IUPnPServiceCallbackImpl,
                           private LastChangeParsing
{
public:
    VirtualServiceCtrl()
        : LastChangeParsing(get_event_namespace<I>())
    {}
    
    ~VirtualServiceCtrl()
    {
        // Every virtual service instance must be released
        assert(m_vInstances.empty());
        
        if(m_pService)
        {
            m_pService->RemoveCallback(m_dwCookie);
        }
    }
    
    // Init
    bool Init(IUPnPService* pService)
    {
        assert(pService);
        
        // For now, hardcode which vars have numeric types in AVTransport and RenderingControl
        m_mapStateVars.insert(L"Brightness", true);
        m_mapStateVars.insert(L"Contrast", true);
        m_mapStateVars.insert(L"Sharpness", true);
        m_mapStateVars.insert(L"RedVideoGain", true);
        m_mapStateVars.insert(L"GreenVideoGain", true);
        m_mapStateVars.insert(L"BlueVideoGain", true);
        m_mapStateVars.insert(L"RedVideoBlackLevel", true);
        m_mapStateVars.insert(L"GreenVideoBlackLevel", true);
        m_mapStateVars.insert(L"BlueVideoBlackLevel", true);
        m_mapStateVars.insert(L"ColorTemperature", true);
        m_mapStateVars.insert(L"HorizontalKeystone", true);
        m_mapStateVars.insert(L"VerticalKeystone", true);
        m_mapStateVars.insert(L"Volume", true);
        m_mapStateVars.insert(L"VolumeDB", true);
        m_mapStateVars.insert(L"NumberOfTracks", true);
        m_mapStateVars.insert(L"CurrentTrack", true);
        
        m_pService = pService;
        
        HRESULT hr;
        
        if(FAILED(hr = pService->AddCallback(static_cast<IUPnPServiceCallbackImpl*>(this), &m_dwCookie)))
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("AddCallback failed with error 0x%08x"), hr));
            return false;
        }
        
        return true;
    }
    
    
    // GetInstance
    DWORD GetInstance(long InstanceID, I** ppInstance)
    {
        if(!ppInstance)
        {
            return ERROR_AV_POINTER;
        }
            
        *ppInstance = NULL;
            
        if(!m_pService)
        {
            return ERROR_AV_NONEXISTANT_SERVICE;
        }
        
        ce::gate<ce::critical_section> gate(m_csInstancesMap);

        InstanceIter it = FindInstance(InstanceID);
        if(m_vInstances.end() == it)
        {
            // not found, create a new one
            it = CreateServiceInstance(InstanceID);
            if (m_vInstances.end() == it)
            {
                DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("OOM creating service instance")));
                return ERROR_AV_OOM;
            }
        }
        
        *ppInstance = *it;
        unsigned long ul = (*ppInstance)->AddRef();
        
        return SUCCESS_AV;
    }

// IUPnPServiceCallback
public:
    // StateVariableChanged
    STDMETHOD(StateVariableChanged)(
            /* [in] */ IUPnPService *pus,
            /* [in] */ LPCWSTR pcwszStateVarName,
            /* [in] */ VARIANT varValue)
    {
        if(0 != wcscmp(pcwszStateVarName, L"LastChange"))
        {
            DEBUGMSG(ZONE_AV_WARN, (AV_TEXT("Ignoring event for state variable '%s'"), pcwszStateVarName));
            return S_OK;
        }
        
        if(varValue.vt != VT_BSTR)
        {
            DEBUGMSG(ZONE_AV_WARN, (AV_TEXT("Invalid type %d of LastChange state variable"), varValue.vt));
            return S_OK;
        }
        
        if(!varValue.bstrVal[0])
        {
            DEBUGMSG(ZONE_AV_WARN, (AV_TEXT("Received empty string for LastChange event")));
            return S_OK;
        }
        
        ce::SAXReader   Reader;
        HRESULT         hr;
        ISAXXMLReader *pXMLReader = NULL;
        
        hr = Reader.Init();
        
        if (SUCCEEDED(hr))
        {
            pXMLReader = Reader.GetXMLReader();
        }
        
        if(pXMLReader)
        {
            hr = pXMLReader->putContentHandler(static_cast<LastChangeParsing*>(this));
            
            assert(SUCCEEDED(hr));

            if(FAILED(hr = pXMLReader->parse(varValue)))
            {
                DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("Error 0x%08x when parsing LastChange event"), hr));
            }
            
            hr = pXMLReader->putContentHandler(NULL);
        }
        else
        {
            DEBUGMSG(ZONE_AV_ERROR, (AV_TEXT("Error 0x%08x instantiating SAXXMLReader"), hr));
        }
        
        return hr;
    }
    

protected:    
    virtual DWORD OnStateChanged(
        /*[in]*/  int     InstanceID,
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszValue)
    {
        ce::gate<ce::critical_section> gate(m_csInstancesMap);

        InstanceIter it = FindInstance(InstanceID);
        if(m_vInstances.end() != it)
        {
            return (*it)->OnStateChanged(pszStateVariableName,
                                                     pszValue);
        }
        
        return SUCCESS_AV;
    }
        
    virtual DWORD OnStateChanged(
        /*[in]*/  int     InstanceID,
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  long    nValue)
    {
        ce::gate<ce::critical_section> gate(m_csInstancesMap);

        InstanceIter it = FindInstance(InstanceID);
        if(m_vInstances.end() != it)
        {
            return (*it)->OnStateChanged(pszStateVariableName,
                                                     nValue);
        }
        
        return SUCCESS_AV;
    }

    virtual DWORD OnStateChanged(
        /*[in]*/  int     InstanceID,
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszChannel,
        /*[in]*/  long    nValue)
    {
        ce::gate<ce::critical_section> gate(m_csInstancesMap);

        InstanceIter it = FindInstance(InstanceID);
        if(m_vInstances.end() != it)
        {
            return (*it)->OnStateChanged(pszStateVariableName,
                                                     pszChannel,
                                                     nValue);
        }
        
        return SUCCESS_AV;
    }

protected:
    // private class manages destruction of service instances
    class InstanceCtrlImpl : public T
    {
    public:
        InstanceCtrlImpl(
            long InstanceID,
            IUPnPService* pService, 
            VirtualServiceCtrl& Ctrl) : T(InstanceID, pService), m_Callback(Ctrl)
        {}

        long InstanceID() { return m_InstanceID; }

    protected:
        // ServiceInstanceLifetime
        void OnFinalRelease() { m_Callback.RemoveInstance(m_InstanceID); }

    private:
        // disable default & copy ctor, assignment
        InstanceCtrlImpl();
        InstanceCtrlImpl(const InstanceCtrlImpl&);
        InstanceCtrlImpl& operator=(const InstanceCtrlImpl&);

        VirtualServiceCtrl& m_Callback;
    };

    typedef ce::vector<InstanceCtrlImpl*> InstanceVector;
    typedef typename InstanceVector::iterator InstanceIter;

    typename InstanceIter CreateServiceInstance(const long InstanceID)
    {
        ce::gate<ce::critical_section> gate(m_csInstancesMap);
        InstanceIter it = m_vInstances.end();

        ce::auto_ptr<InstanceCtrlImpl> pInstance =
            new InstanceCtrlImpl(InstanceID, m_pService, *this);
        if(pInstance)
        {
            if(m_vInstances.push_back(pInstance))
            {
                pInstance.release(); // don't delete instance in success case
                it = m_vInstances.end() - 1;
            }
        }

        return it;
    }

    // called only when the last point to the instance is released (the map
    // does not hold a refcount to the instance)
    void RemoveInstance(const long InstanceID)
    {
        ce::gate<ce::critical_section> gate(m_csInstancesMap);
        InstanceIter it = FindInstance(InstanceID);
        assert(m_vInstances.end() != it);
        if (m_vInstances.end() != it)
        {
            delete *it;
            m_vInstances.erase(it);
        }
    }

    InstanceIter FindInstance(const long InstanceID)
    {
        ce::gate<ce::critical_section> gate(m_csInstancesMap);
        InstanceIter it = m_vInstances.begin(), itEnd = m_vInstances.end();

        for(; it != itEnd; ++it)
        {
            if((*it)->InstanceID() == InstanceID)
            {
                break;
            }
        }

        return it;
    }

protected:
    ce::critical_section                    m_csInstancesMap;
    InstanceVector                          m_vInstances;
    CComPtr<IUPnPService>                   m_pService;
    DWORD                                   m_dwCookie;
};



//
// AVTransportCtrl
//
class AVTransportCtrl : public IEventSourceImpl<IAVTransport>
{
public:
    AVTransportCtrl(long InstanceID, IUPnPService* pService) :
        m_InstanceID(InstanceID), m_proxyAVTransport(pService)
    {}

// IAVTransport
public:
    virtual DWORD SetAVTransportURI(
            /* [in] */ LPCWSTR pszCurrentURI,
            /* [in] */ LPCWSTR pszCurrentURIMetaData);

    virtual DWORD SetNextAVTransportURI(
            /* [in] */ LPCWSTR pszNextURI,
            /* [in] */ LPCWSTR pszNextURIMetaData);

    virtual DWORD GetMediaInfo(
            /* [in, out] */ MediaInfo* pMediaInfo);

    virtual DWORD GetTransportInfo(
            /* [in, out] */ TransportInfo* pTransportInfo);

    virtual DWORD GetPositionInfo(
            /* [in, out] */ PositionInfo* pPositionInfo);

    virtual DWORD GetDeviceCapabilities(
            /* [in, out] */ DeviceCapabilities* pDeviceCapabilities);

    virtual DWORD GetTransportSettings(
            /* [in, out] */ TransportSettings* pTransportSettings);

    virtual DWORD Stop();

    virtual DWORD Play(
            /* [in] */ LPCWSTR pszSpeed);

    virtual DWORD Pause();

    virtual DWORD Record();

    virtual DWORD Seek(
            /* [in] */ LPCWSTR pszUnit,
            /* [in] */ LPCWSTR pszTarget);

    virtual DWORD Next();

    virtual DWORD Previous();

    virtual DWORD SetPlayMode(
            /* [in] */ LPCWSTR pszNewPlayMode);

    virtual DWORD SetRecordQualityMode(
            /* [in] */ LPCWSTR pszNewRecordQualityMode);

    virtual DWORD GetCurrentTransportActions(
            /* [in, out] */ TransportActions* pActions);

    virtual DWORD InvokeVendorAction(
            /* [in] */ LPCWSTR pszActionName,
            /* [in] */ DISPPARAMS* pdispparams, 
            /* [in, out] */ VARIANT* pvarResult);

protected:
    const long m_InstanceID;

private:
    // disable copy ctor, assignment
    AVTransportCtrl(const AVTransportCtrl&);
    AVTransportCtrl& operator=(const AVTransportCtrl&);

    ce::upnp_proxy m_proxyAVTransport;
};


//
// RenderingControlCtrl
//
class RenderingControlCtrl : public IEventSourceImpl<IRenderingControl>
{
public:
    RenderingControlCtrl(long InstanceID, IUPnPService* pService) :
        m_InstanceID(InstanceID), m_proxyRenderingControl(pService)
    {}

// IRenderingControl
public:
    virtual DWORD ListPresets(
        /* [in, out] */ wstring* pstrPresetNameList);

    virtual DWORD SelectPreset(
        /* [in] */ LPCWSTR pszPresetName);

    virtual DWORD GetBrightness(
        /* [in, out] */ unsigned short* pBrightness);

    virtual DWORD SetBrightness(
        /* [in] */ unsigned short Brightness);

    virtual DWORD GetContrast(
        /* [in, out] */ unsigned short* pContrast);

    virtual DWORD SetContrast(
        /* [in] */ unsigned short Contrast);

    virtual DWORD GetSharpness(
        /* [in, out] */ unsigned short* pSharpness);

    virtual DWORD SetSharpness(
        /* [in] */ unsigned short Sharpness);

    virtual DWORD GetRedVideoGain(
        /* [in, out] */ unsigned short* pRedVideoGain);

    virtual DWORD SetRedVideoGain(
        /* [in] */ unsigned short RedVideoGain);

    virtual DWORD GetGreenVideoGain(
        /* [in, out] */ unsigned short* pGreenVideoGain);

    virtual DWORD SetGreenVideoGain(
        /* [in] */ unsigned short GreenVideoGain);

    virtual DWORD GetBlueVideoGain(
        /* [in, out] */ unsigned short* pBlueVideoGain);

    virtual DWORD SetBlueVideoGain(
        /* [in] */ unsigned short BlueVideoGain);

    virtual DWORD GetRedVideoBlackLevel(
        /* [in, out] */ unsigned short* pRedVideoBlackLevel);

    virtual DWORD SetRedVideoBlackLevel(
        /* [in] */ unsigned short RedVideoBlackLevel);

    virtual DWORD GetGreenVideoBlackLevel(
        /* [in, out] */ unsigned short* pGreenVideoBlackLevel);

    virtual DWORD SetGreenVideoBlackLevel(
        /* [in] */ unsigned short GreenVideoBlackLevel);

    virtual DWORD GetBlueVideoBlackLevel(
        /* [in, out] */ unsigned short* pBlueVideoBlackLevel);

    virtual DWORD SetBlueVideoBlackLevel(
        /* [in] */ unsigned short BlueVideoBlackLevel);

    virtual DWORD GetColorTemperature(
        /* [in, out] */ unsigned short* pColorTemperature);

    virtual DWORD SetColorTemperature(
        /* [in] */ unsigned short ColorTemperature);

    virtual DWORD GetHorizontalKeystone(
        /* [in, out] */ short* pHorizontalKeystone);

    virtual DWORD SetHorizontalKeystone(
        /* [in] */ short HorizontalKeystone);

    virtual DWORD GetVerticalKeystone(
        /* [in, out] */ short* pVerticalKeystone);

    virtual DWORD SetVerticalKeystone(
        /* [in] */ short VerticalKeystone);

    virtual DWORD GetMute(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ bool* pMute);

    virtual DWORD SetMute(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ bool Mute);

    virtual DWORD GetVolume(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ unsigned short* pVolume);

    virtual DWORD SetVolume(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ unsigned short Volume);

    virtual DWORD GetVolumeDB(
            /* [in] */ LPCWSTR pszChannel,
            /* [in, out] */ short* pVolumeDB);

    virtual DWORD SetVolumeDB(
            /* [in] */ LPCWSTR pszChannel,
            /* [in] */ short VolumeDB);

    virtual DWORD GetVolumeDBRange(
            /* [in] */ LPCWSTR pszChannel,
            /* [in, out] */ short* pMinValue,
            /* [in, out] */ short* pMaxValue);

    virtual DWORD GetLoudness(
            /* [in] */ LPCWSTR pszChannel,
            /* [in, out] */ bool* pLoudness);

    virtual DWORD SetLoudness(
            /* [in] */ LPCWSTR pszChannel,
            /* [in] */ bool Loudness);

    virtual DWORD InvokeVendorAction(
            /* [in] */ LPCWSTR pszActionName,
            /* [in] */ DISPPARAMS* pdispparams, 
            /* [in, out] */ VARIANT* pvarResult);

protected:
    template <typename T>
    DWORD GetXxx(LPCWSTR pszName, T* pValue)
    {
        HRESULT hr;
        ce::upnp_proxy_context context;
    
        hr = m_proxyRenderingControl.call(context, pszName, m_InstanceID);
        
        if(FAILED(hr))
        {
            return AVErrorFromUPnPError(hr);
        }
                                     
        if(!m_proxyRenderingControl.get_results(context, pValue))
        {
            return ERROR_AV_INVALID_OUT_ARGUMENTS;
        }
               
        return SUCCESS_AV;
    }
    
    template <typename T>
    DWORD GetXxx(LPCWSTR pszName, LPCWSTR pszChannel, T* pValue)
    {
        HRESULT hr;
        ce::upnp_proxy_context context;
    
        hr = m_proxyRenderingControl.call(context, pszName, m_InstanceID, pszChannel);
        
        if(FAILED(hr))
        {
            return AVErrorFromUPnPError(hr);
        }
                                     
        if(!m_proxyRenderingControl.get_results(context, pValue))
        {
            return ERROR_AV_INVALID_OUT_ARGUMENTS;
        }
               
        return SUCCESS_AV;
    }
    
    template <typename T>
    DWORD SetXxx(LPCWSTR pszName, T Value)
    {
        HRESULT hr;
        ce::upnp_proxy_context context;
    
        hr = m_proxyRenderingControl.call(context, pszName, m_InstanceID, Value);
               
        return AVErrorFromUPnPError(hr);
    }
    
    template <typename T>
    DWORD SetXxx(LPCWSTR pszName, LPCWSTR pszChannel, T Value)
    {
        HRESULT hr;
        ce::upnp_proxy_context context;
    
        hr = m_proxyRenderingControl.call(context, pszName, m_InstanceID, pszChannel, Value);
               
        return AVErrorFromUPnPError(hr);
    }

protected:
    const long m_InstanceID;

private:
    // disable copy ctor, assignment
    RenderingControlCtrl(const RenderingControlCtrl&);
    RenderingControlCtrl& operator=(const RenderingControlCtrl&);

    ce::upnp_proxy m_proxyRenderingControl;
};


//
// ConnectionManagerCtrl
//
class ConnectionManagerCtrl : public ServiceCtrl<IConnectionManager>
{
public:
    virtual ~ConnectionManagerCtrl()
    {}

    bool Init(/* [in] */ IUPnPDevice* pDevice);
    
    LPCWSTR ServiceID() const
    {
        assert(!m_strServiceID.empty());
        return m_strServiceID;
    }

// IConnectionManager
public:
    virtual DWORD GetProtocolInfo(
            /* [in, out] */ wstring* pstrSourceProtocolInfo,
            /* [in, out] */ wstring* pstrSinkProtocolInfo);

    virtual DWORD PrepareForConnection(
            /* [in] */ LPCWSTR pszRemoteProtocolInfo,
            /* [in] */ LPCWSTR pszPeerConnectionManager,
            /* [in] */ long PeerConnectionID,
            /* [in] */ DIRECTION Direction,
            /* [in, out] */ long* pConnectionID,
            /* [in, out] */ IAVTransport** ppAVTransport,
            /* [in, out] */ IRenderingControl** ppRenderingControl);

    virtual DWORD ConnectionComplete(
            /* [in] */ long ConnectionID);

    virtual DWORD GetFirstConnectionID(
            /* [in, out] */ long* pConnectionID);

    virtual DWORD GetNextConnectionID(
            /* [in, out] */ long* pConnectionID);

    virtual DWORD GetCurrentConnectionInfo(
            /* [in] */ long ConnectionID,
            /* [in, out] */ ConnectionInfo* pConnectionInfo);
            
    virtual DWORD InvokeVendorAction(
            /* [in] */ LPCWSTR pszActionName,
            /* [in] */ DISPPARAMS* pdispparams, 
            /* [in, out] */ VARIANT* pvarResult);

private:
    ce::upnp_proxy m_proxyConnectionManager;
    ce::wstring m_strConnectionIDs;
    ce::wstring m_strServiceID;

    VirtualServiceCtrl<IAVTransport, AVTransportCtrl> m_AVTransport;
    VirtualServiceCtrl<IRenderingControl, RenderingControlCtrl> m_RenderingControl;
};


//
// ContentDirectoryCtrl
// 
class ContentDirectoryCtrl : public ServiceCtrl<IContentDirectory>
{
public:
    virtual ~ContentDirectoryCtrl()
    {}
    
    bool Init(/* [in] */ IUPnPDevice* pDevice);

// IContentDirectory
public:
    virtual DWORD GetSearchCapabilities(
            /* [in, out] */ wstring* pstrSearchCaps);

    virtual DWORD GetSortCapabilities(
            /* [in, out] */ wstring* pstrSortCaps);

    virtual DWORD GetSystemUpdateID(
            /* [in, out] */ unsigned long* pId);

    virtual DWORD BrowseMetadata(
            /* [in] */ LPCWSTR pszObjectID,
            /* [in] */ LPCWSTR pszFilter,
            /* [in, out] */ wstring* pstrResult,
            /* [in, out] */ unsigned long* pUpdateID);

    virtual DWORD BrowseChildren(
            /* [in] */ LPCWSTR pszObjectID,
            /* [in] */ LPCWSTR pszFilter,
            /* [in] */ unsigned long StartingIndex,
            /* [in] */ unsigned long RequestedCount,
            /* [in] */ LPCWSTR pszSortCriteria,
            /* [in, out] */ wstring* pstrResult,
            /* [in, out] */ unsigned long* pNumberReturned,
            /* [in, out] */ unsigned long* pTotalMatches,
            /* [in, out] */ unsigned long* pUpdateID);
        
    virtual DWORD Search(
            /* [in] */ LPCWSTR pszContainerID,
            /* [in] */ LPCWSTR pszSearchCriteria,
            /* [in] */ LPCWSTR pszFilter,
            /* [in] */ unsigned long StartingIndex,
            /* [in] */ unsigned long RequestedCount,
            /* [in] */ LPCWSTR pszSortCriteria,
            /* [in, out] */ wstring* pstrResult,
            /* [in, out] */ unsigned long* pNumberReturned,
            /* [in, out] */ unsigned long* pTotalMatches,
            /* [in, out] */ unsigned long* pUpdateID);

    virtual DWORD CreateObject(
            /* [in] */ LPCWSTR pszContainerID,
            /* [in] */ LPCWSTR pszElements,
            /* [in, out] */ wstring* pstrObjectID,
            /* [in, out] */ wstring* pstrResult);

    virtual DWORD DestroyObject(
            /* [in] */ LPCWSTR pszObjectID);

    virtual DWORD UpdateObject(
            /* [in] */ LPCWSTR pszObjectID,
            /* [in] */ LPCWSTR pszCurrentTagValue,
            /* [in] */ LPCWSTR pszNewTagValue);

    virtual DWORD ImportResource(
            /* [in] */ LPCWSTR pszSourceURI,
            /* [in] */ LPCWSTR pszDestinationURI,
            /* [in, out] */ unsigned long* pTransferID);

    virtual DWORD ExportResource(
            /* [in] */ LPCWSTR pszSourceURI,
            /* [in] */ LPCWSTR pszDestinationURI,
            /* [in, out] */ unsigned long* pTransferID);

    virtual DWORD StopTransferResource(
            /* [in] */ unsigned long TransferID);

    virtual DWORD GetTransferProgress(
            /* [in] */ unsigned long TransferID,
            /* [in, out] */ wstring* pstrTransferStatus,
            /* [in, out] */ wstring* pstrTransferLength,
            /* [in, out] */ wstring* pstrTransferTotal);

    virtual DWORD DeleteResource(
            /* [in] */ LPCWSTR pszResourceURI);

    virtual DWORD CreateReference(
            /* [in] */ LPCWSTR pszContainerID,
            /* [in] */ LPCWSTR pszObjectID,
            /* [in, out] */ wstring* pstrNewID);
            
    virtual DWORD InvokeVendorAction(
            /* [in] */ LPCWSTR pszActionName,
            /* [in] */ DISPPARAMS* pdispparams, 
            /* [in, out] */ VARIANT* pvarResult);

private:
    ce::upnp_proxy  m_proxyContentDirectory;
};


} // namespace av_upnp::details

namespace DIDL_Lite
{

namespace details
{

//
// PropertyInfo
//    

class PropertyInfo
{
public:
    PropertyInfo();
    ~PropertyInfo();
    
    HRESULT Init(LPCWSTR pszName, LPCWSTR pszValue, size_t cchValue);
    HRESULT SetValue(WCHAR *pszValue);
    
public:
    ce::auto_bstr m_bstrName;
    ce::auto_bstr m_bstrValue;
    PropertyInfo *m_pNext;
};

//
// properties
//    
class properties
{
public:
    properties();
    ~properties();
    
public:
    bool GetProperty(const WCHAR *pszName, WCHAR **ppszValue, unsigned long nIndex);
    bool AddProperty(const WCHAR *pszName, const WCHAR *pszValue, size_t cchValue);
    void Cleanup();

private:
    
    PropertyInfo *m_pPropertyInfo;
};


//
// objects
//
class objects : public ce::SAXContentHandler
{
public:
    objects();
    ~objects();
    
    void Cleanup();
    
    bool GetFirstObject(object* pObj);
    bool GetNextObject(object* pObj);
    
// ISAXContentHandler
public:
    virtual HRESULT STDMETHODCALLTYPE startDocument(void);
    
    virtual HRESULT STDMETHODCALLTYPE startElement(
        /* [in] */ const WCHAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const WCHAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const WCHAR *pwchQName,
        /* [in] */ int cchQName,
        /* [in] */ ISAXAttributes *pAttributes);
    
    virtual HRESULT STDMETHODCALLTYPE endElement( 
        /* [in] */ const WCHAR *pwchNamespaceUri,
        /* [in] */ int cchNamespaceUri,
        /* [in] */ const WCHAR *pwchLocalName,
        /* [in] */ int cchLocalName,
        /* [in] */ const WCHAR *pwchQName,
        /* [in] */ int cchQName);
        
    // characters
    virtual HRESULT STDMETHODCALLTYPE characters( 
        /* [in] */ const WCHAR *pwchChars,
        /* [in] */ int cchChars);
    
private:
    typedef ce::list<properties>    ObjectsList;
    
    ObjectsList                     m_listObjects;
    ObjectsList::iterator           m_itCurrent;

// parsing
private:
    wstring m_strElementQName;
    wstring m_strElementText;
};

};

}; // namespace DIDL_Lite

} // namespace av_upnp

#endif // __AV_UPNP_CTRL_INTERNAL_H
