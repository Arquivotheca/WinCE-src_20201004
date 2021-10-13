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

using namespace av_upnp;
using namespace av_upnp::details;

/////////////////////////////////////////////////////////////////////////////
// RenderingControlCtrl

//
// IRenderingControl
//

//
// ListPresets
//
DWORD RenderingControlCtrl::ListPresets(
            /* [in, out] */ wstring* pstrPresetNameList)
{
    return GetXxx(L"ListPresets", pstrPresetNameList);
}


//
// SelectPreset
//
DWORD RenderingControlCtrl::SelectPreset(
            /* [in] */ LPCWSTR pszPresetName)
{
    return SetXxx(L"SelectPreset", pszPresetName);
}


//
// GetBrightness
//
DWORD RenderingControlCtrl::GetBrightness(
            /* [in, out] */ unsigned short* pBrightness)
{
    return GetXxx(L"GetBrightness", pBrightness);
}

//
// SetBrightness
//
DWORD RenderingControlCtrl::SetBrightness(
            /* [in] */ unsigned short Brightness)
{
    return SetXxx(L"SetBrightness", Brightness);
}


//
// GetContrast
//
DWORD RenderingControlCtrl::GetContrast(
            /* [in, out] */ unsigned short* pContrast)
{
    return GetXxx(L"GetContrast", pContrast);
}


//
// SetContrast
//
DWORD RenderingControlCtrl::SetContrast(
            /* [in] */ unsigned short Contrast)
{
    return SetXxx(L"SetContrast", Contrast);
}


//
// GetSharpness
//
DWORD RenderingControlCtrl::GetSharpness(
            /* [in, out] */ unsigned short* pSharpness)
{
    return GetXxx(L"GetSharpness", pSharpness);
}


//
// SetSharpness
//
DWORD RenderingControlCtrl::SetSharpness(
            /* [in] */ unsigned short Sharpness)
{
    return SetXxx(L"SetSharpness", Sharpness);
}


//
// GetRedVideoGain
//
DWORD RenderingControlCtrl::GetRedVideoGain(
            /* [in, out] */ unsigned short* pRedVideoGain)
{
    return GetXxx(L"GetRedVideoGain", pRedVideoGain);
}


//
// SetRedVideoGain
//
DWORD RenderingControlCtrl::SetRedVideoGain(
            /* [in] */ unsigned short RedVideoGain)
{
    return SetXxx(L"SetRedVideoGain", RedVideoGain);
}


//
// GetGreenVideoGain
//
DWORD RenderingControlCtrl::GetGreenVideoGain(
            /* [in, out] */ unsigned short* pGreenVideoGain)
{
    return GetXxx(L"GetGreenVideoGain", pGreenVideoGain);
}


//
// SetGreenVideoGain
//
DWORD RenderingControlCtrl::SetGreenVideoGain(
            /* [in] */ unsigned short GreenVideoGain)
{
    return SetXxx(L"SetGreenVideoGain", GreenVideoGain);
}


//
// GetBlueVideoGain
//
DWORD RenderingControlCtrl::GetBlueVideoGain(
            /* [in, out] */ unsigned short* pBlueVideoGain)
{
    return GetXxx(L"GetBlueVideoGain", pBlueVideoGain);
}


//
// SetBlueVideoGain
//
DWORD RenderingControlCtrl::SetBlueVideoGain(
            /* [in] */ unsigned short BlueVideoGain)
{
    return SetXxx(L"SetBlueVideoGain", BlueVideoGain);
}


//
// GetRedVideoBlackLevel
//
DWORD RenderingControlCtrl::GetRedVideoBlackLevel(
            /* [in, out] */ unsigned short* pRedVideoBlackLevel)
{
    return GetXxx(L"GetRedVideoBlackLevel", pRedVideoBlackLevel);
}


//
// SetRedVideoBlackLevel
//
DWORD RenderingControlCtrl::SetRedVideoBlackLevel(
            /* [in] */ unsigned short RedVideoBlackLevel)
{
    return SetXxx(L"SetRedVideoBlackLevel", RedVideoBlackLevel);
}


//
// GetGreenVideoBlackLevel
//
DWORD RenderingControlCtrl::GetGreenVideoBlackLevel(
            /* [in, out] */ unsigned short* pGreenVideoBlackLevel)
{
    return GetXxx(L"GetGreenVideoBlackLevel", pGreenVideoBlackLevel);
}


//
// SetGreenVideoBlackLevel
//
DWORD RenderingControlCtrl::SetGreenVideoBlackLevel(
            /* [in] */ unsigned short GreenVideoBlackLevel)
{
    return SetXxx(L"SetGreenVideoBlackLevel", GreenVideoBlackLevel);
}


//
// GetBlueVideoBlackLevel
//
DWORD RenderingControlCtrl::GetBlueVideoBlackLevel(
            /* [in, out] */ unsigned short* pBlueVideoBlackLevel)
{
    return GetXxx(L"GetBlueVideoBlackLevel", pBlueVideoBlackLevel);
}


//
// SetBlueVideoBlackLevel
//
DWORD RenderingControlCtrl::SetBlueVideoBlackLevel(
            /* [in] */ unsigned short BlueVideoBlackLevel)
{
    return SetXxx(L"SetBlueVideoBlackLevel", BlueVideoBlackLevel);
}


//
// GetColorTemperature
//
DWORD RenderingControlCtrl::GetColorTemperature(
            /* [in, out] */ unsigned short* pColorTemperature)
{
    return GetXxx(L"GetColorTemperature", pColorTemperature);
}


//
// SetColorTemperature
//
DWORD RenderingControlCtrl::SetColorTemperature(
            /* [in] */ unsigned short ColorTemperature)
{
    return SetXxx(L"SetColorTemperature", ColorTemperature);
}


//
// GetHorizontalKeystone
//
DWORD RenderingControlCtrl::GetHorizontalKeystone(
            /* [in, out] */ short* pHorizontalKeystone)
{
    return GetXxx(L"GetHorizontalKeystone", pHorizontalKeystone);
}


//
// SetHorizontalKeystone
//
DWORD RenderingControlCtrl::SetHorizontalKeystone(
            /* [in] */ short HorizontalKeystone)
{
    return SetXxx(L"SetHorizontalKeystone", HorizontalKeystone);
}


//
// GetVerticalKeystone
//
DWORD RenderingControlCtrl::GetVerticalKeystone(
            /* [in, out] */ short* pVerticalKeystone)
{
    return GetXxx(L"GetVerticalKeystone", pVerticalKeystone);
}


//
// SetVerticalKeystone
//
DWORD RenderingControlCtrl::SetVerticalKeystone(
            /* [in] */ short VerticalKeystone)
{
    return SetXxx(L"SetVerticalKeystone", VerticalKeystone);
}


//
// GetMute
//
DWORD RenderingControlCtrl::GetMute(
            /* [in] */ LPCWSTR pszChannel,
            /* [in, out] */ bool* pMute)
{
    return GetXxx(L"GetMute", pszChannel, pMute);
}


//
// SetMute
//
DWORD RenderingControlCtrl::SetMute(
            /* [in] */ LPCWSTR pszChannel,
            /* [in] */ bool Mute)
{
    return SetXxx(L"SetMute", pszChannel, Mute);
}


//
// GetVolume
//
DWORD RenderingControlCtrl::GetVolume(
            /* [in] */ LPCWSTR pszChannel,
            /* [in, out] */ unsigned short* pVolume)
{
    return GetXxx(L"GetVolume", pszChannel, pVolume);
}


//
// SetVolume
//
DWORD RenderingControlCtrl::SetVolume(
            /* [in] */ LPCWSTR pszChannel,
            /* [in] */ unsigned short Volume)
{
    return SetXxx(L"SetVolume", pszChannel, Volume);
}


//
// GetVolumeDB
//
DWORD RenderingControlCtrl::GetVolumeDB(
            /* [in] */ LPCWSTR pszChannel,
            /* [in, out] */ short* pVolumeDB)
{
    return GetXxx(L"GetVolumeDB", pszChannel, pVolumeDB);
}


//
// SetVolumeDB
//
DWORD RenderingControlCtrl::SetVolumeDB(
            /* [in] */ LPCWSTR pszChannel,
            /* [in] */ short VolumeDB)
{
    return SetXxx(L"SetVolumeDB", pszChannel, VolumeDB);
}


//
// GetVolumeDBRange
//
DWORD RenderingControlCtrl::GetVolumeDBRange(
            /* [in] */ LPCWSTR pszChannel,
            /* [in, out] */ short* pMinValue,
            /* [in, out] */ short* pMaxValue)
{
    HRESULT hr;
    ce::upnp_proxy_context context;
    
    hr = m_proxyRenderingControl.call(context, L"GetVolumeDBRange", m_InstanceID, pszChannel);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
                                    
    if(!m_proxyRenderingControl.get_results(context, pMinValue, pMaxValue))
    {
        return ERROR_AV_INVALID_OUT_ARGUMENTS;
    }
            
    return SUCCESS_AV;
}


//
// GetLoudness
//
DWORD RenderingControlCtrl::GetLoudness(
            /* [in] */ LPCWSTR pszChannel,
            /* [in, out] */ bool* pLoudness)
{
    return GetXxx(L"GetLoudness", pszChannel, pLoudness);
}


//
// SetLoudness
//
DWORD RenderingControlCtrl::SetLoudness(
            /* [in] */ LPCWSTR pszChannel,
            /* [in] */ bool Loudness)
{
    return SetXxx(L"SetLoudness", pszChannel, Loudness);
}


//
// InvokeVendorAction
//
DWORD RenderingControlCtrl::InvokeVendorAction(
            /* [in] */ LPCWSTR pszActionName,
            /* [in] */ DISPPARAMS* pdispparams, 
            /* [in, out] */ VARIANT* pvarResult)
{
    if(!pdispparams)
    {
        return ERROR_AV_POINTER;
    }
        
    if(!pdispparams->rgvarg)
    {
        return ERROR_AV_POINTER;
    }
    
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
    {
        return AVErrorFromUPnPError(E_OUTOFMEMORY);
    }
    
    DispParams.rgvarg = pArgs;
    DispParams.cArgs = pdispparams->cArgs + 1;
    
    // copy existing arguments
    memcpy(pArgs, pdispparams->rgvarg, pdispparams->cArgs * sizeof(VARIANTARG));
    
    // add InstanceID as the first argument (last element in the array)
    DispParams.rgvarg[DispParams.cArgs - 1] = varInstanceID;
    
    // invoke the vendor action
    hr = m_proxyRenderingControl.invoke(pszActionName, &DispParams, pvarResult);
    
    if(FAILED(hr))
    {
        return AVErrorFromUPnPError(hr);
    }
    
    return SUCCESS_AV;
}
