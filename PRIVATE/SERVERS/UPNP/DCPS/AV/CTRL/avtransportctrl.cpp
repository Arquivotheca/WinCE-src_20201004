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

using namespace av_upnp;
using namespace av_upnp::details;

/////////////////////////////////////////////////////////////////////////////
// AVTransportCtrl

//
// IAVTransport
//


//
// SetAVTransportURI
//
DWORD AVTransportCtrl::SetAVTransportURI(
	        /* [in] */ LPCWSTR pszCurrentURI,
	        /* [in] */ LPCWSTR pszCurrentURIMetaData)
{
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"SetAVTransportURI", m_InstanceID, pszCurrentURI, pszCurrentURIMetaData);
           
    return AVErrorFromUPnPError(hr);
}


//
// SetNextAVTransportURI
//
DWORD AVTransportCtrl::SetNextAVTransportURI(
	        /* [in] */ LPCWSTR pszNextURI,
	        /* [in] */ LPCWSTR pszNextURIMetaData)
{
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"SetNextAVTransportURI", 
                                 m_InstanceID,
                                 pszNextURI,
                                 pszNextURIMetaData);
           
    return AVErrorFromUPnPError(hr);
}


//
// GetMediaInfo
//
DWORD AVTransportCtrl::GetMediaInfo(
            /* [in, out] */ MediaInfo* pMediaInfo)
{
    if(!pMediaInfo)
        return ERROR_AV_POINTER;
        
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"GetMediaInfo", 
                                 m_InstanceID);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
                                 
    if(!m_proxyAVTransport.get_results(&pMediaInfo->nNrTracks,
                                       &pMediaInfo->strMediaDuration,
                                       &pMediaInfo->strCurrentURI,
                                       &pMediaInfo->strCurrentURIMetaData,
                                       &pMediaInfo->strNextURI,
                                       &pMediaInfo->strNextURIMetaData,
                                       &pMediaInfo->strPlayMedium,
                                       &pMediaInfo->strRecordMedium,
                                       &pMediaInfo->strWriteStatus))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
           
    return SUCCESS_AV;
}


//
// GetTransportInfo
//
DWORD AVTransportCtrl::GetTransportInfo(
	        /* [in, out] */ TransportInfo* pTransportInfo)
{
    if(!pTransportInfo)
        return ERROR_AV_POINTER;
        
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"GetTransportInfo", 
                                 m_InstanceID);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
                                 
    if(!m_proxyAVTransport.get_results(&pTransportInfo->strCurrentTransportState,
                                       &pTransportInfo->strCurrentTransportStatus,
                                       &pTransportInfo->strCurrentSpeed))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
           
    return SUCCESS_AV;
}


//
// GetPositionInfo
//
DWORD AVTransportCtrl::GetPositionInfo(
	        /* [in, out] */ PositionInfo* pPositionInfo)
{
    if(!pPositionInfo)
        return ERROR_AV_POINTER;
        
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"GetPositionInfo", 
                                 m_InstanceID);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
                                 
    if(!m_proxyAVTransport.get_results(&pPositionInfo->nTrack,
                                       &pPositionInfo->strTrackDuration,
                                       &pPositionInfo->strTrackMetaData,
                                       &pPositionInfo->strTrackURI,
                                       &pPositionInfo->strRelTime,
                                       &pPositionInfo->strAbsTime,
                                       &pPositionInfo->nRelCount,
                                       &pPositionInfo->nAbsCount))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
           
    return SUCCESS_AV;
}


//
// GetDeviceCapabilities
//
DWORD AVTransportCtrl::GetDeviceCapabilities(
	        /* [in, out] */ av_upnp::DeviceCapabilities* pDeviceCapabilities)
{
    if(!pDeviceCapabilities)
        return ERROR_AV_POINTER;
        
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"GetDeviceCapabilities", 
                                 m_InstanceID);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
                                 
    if(!m_proxyAVTransport.get_results(&pDeviceCapabilities->strPossiblePlayMedia,
                                       &pDeviceCapabilities->strPossibleRecMedia,
                                       &pDeviceCapabilities->strPossibleRecQualityModes))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
           
    return SUCCESS_AV;
}


//
// GetTransportSettings
//
DWORD AVTransportCtrl::GetTransportSettings(
	        /* [in, out] */ TransportSettings* pTransportSettings)
{
    if(!pTransportSettings)
        return ERROR_AV_POINTER;
        
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"GetTransportSettings", 
                                 m_InstanceID);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
                                 
    if(!m_proxyAVTransport.get_results(&pTransportSettings->strPlayMode,
                                       &pTransportSettings->strRecQualityMode))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
           
    return SUCCESS_AV;
}


//
// Stop
//
DWORD AVTransportCtrl::Stop()
{
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"Stop", m_InstanceID);
           
    return AVErrorFromUPnPError(hr);
}


//
// Play
//
DWORD AVTransportCtrl::Play(
            /* [in] */ LPCWSTR pszSpeed)
{
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"Play", m_InstanceID, pszSpeed);
           
    return AVErrorFromUPnPError(hr);
}


//
// Pause
//
DWORD AVTransportCtrl::Pause()
{
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"Pause", m_InstanceID);
           
    return AVErrorFromUPnPError(hr);
}


//
// Record
//
DWORD AVTransportCtrl::Record()
{
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"Record", m_InstanceID);
           
    return AVErrorFromUPnPError(hr);
}


//
// Seek
//
DWORD AVTransportCtrl::Seek(
	        /* [in] */ LPCWSTR pszUnit,
	        /* [in] */ LPCWSTR pszTarget)
{
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"Seek", m_InstanceID, pszUnit, pszTarget);
           
    return AVErrorFromUPnPError(hr);
}


//
// Next
//
DWORD AVTransportCtrl::Next()
{
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"Next", m_InstanceID);
           
    return AVErrorFromUPnPError(hr);
}


//
// Previous
//
DWORD AVTransportCtrl::Previous()
{
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"Previous", m_InstanceID);
           
    return AVErrorFromUPnPError(hr);
}


//
// SetPlayMode
//
DWORD AVTransportCtrl::SetPlayMode(
            /* [in] */ LPCWSTR pszNewPlayMode)
{
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"SetPlayMode", m_InstanceID, pszNewPlayMode);
           
    return AVErrorFromUPnPError(hr);
}


//
// SetRecordQualityMode
//
DWORD AVTransportCtrl::SetRecordQualityMode(
	        /* [in] */ LPCWSTR pszNewRecordQualityMode)
{
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"SetRecordQualityMode", m_InstanceID, pszNewRecordQualityMode);
           
    return AVErrorFromUPnPError(hr);
}


//
// GetCurrentTransportActions
//
DWORD AVTransportCtrl::GetCurrentTransportActions(
	        /* [in, out] */ TransportActions* pActions)
{
    if(!pActions)
        return ERROR_AV_POINTER;
        
    HRESULT hr;
    
    hr = m_proxyAVTransport.call(L"GetCurrentTransportActions", 
                                 m_InstanceID);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    wistring strActions;
                                 
    if(!m_proxyAVTransport.get_results(&strActions))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
    
    pActions->bPlay     = (wstring::npos != strActions.find(TransportAction::Play));
    pActions->bStop     = (wstring::npos != strActions.find(TransportAction::Stop));
    pActions->bPause    = (wstring::npos != strActions.find(TransportAction::Pause));
    pActions->bSeek     = (wstring::npos != strActions.find(TransportAction::Seek));
    pActions->bNext     = (wstring::npos != strActions.find(TransportAction::Next));
    pActions->bPrevious = (wstring::npos != strActions.find(TransportAction::Previous));
    pActions->bRecord   = (wstring::npos != strActions.find(TransportAction::Record)); 
          
    return SUCCESS_AV;
}

//
// InvokeVendorAction
//
DWORD AVTransportCtrl::InvokeVendorAction(
            /* [in] */ LPCWSTR pszActionName,
            /* [in] */ DISPPARAMS* pdispparams, 
            /* [in, out] */ VARIANT* pvarResult)
{
    if(!pdispparams)
        return ERROR_AV_POINTER;
        
    if(!pdispparams->rgvarg)
        return ERROR_AV_POINTER;
        
    HRESULT                         hr;
    DISPPARAMS                      DispParams = {0};
    ce::auto_array_ptr<VARIANTARG>  pArgs;
    ce::variant                     varInstanceID(m_InstanceID);
    
    // we don't support named arguments
    assert(pdispparams->cNamedArgs == 0);
    assert(pdispparams->rgdispidNamedArgs == NULL);
    
    // allocate arguments array so that we can add InstanceID as the first argument
    pArgs = new VARIANTARG[pdispparams->cArgs + 1];
    
    if(!pArgs)
        return AVErrorFromUPnPError(E_OUTOFMEMORY);
    
    DispParams.rgvarg = pArgs;
    DispParams.cArgs = pdispparams->cArgs + 1;
    
    // copy existing arguments
    memcpy(pArgs, pdispparams->rgvarg, pdispparams->cArgs * sizeof(VARIANTARG));
    
    // add InstanceID as the first argument (last element in the array)
    DispParams.rgvarg[DispParams.cArgs - 1] = varInstanceID;
    
    // invoke the vendor action
    hr = m_proxyAVTransport.invoke(pszActionName, &DispParams, pvarResult);
    
    if(FAILED(hr))
        return AVErrorFromUPnPError(hr);
    
    return SUCCESS_AV;
}
