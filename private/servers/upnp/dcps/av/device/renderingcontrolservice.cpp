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

#include "av_dcp.h"
#include "av_upnp.h"
#include "av_upnp_device_internal.h"

using namespace av_upnp;


//
// Macros which handle the common GetFoo/SetFoo calls
// Must use as "[GET|SET]_CALL(fn_name, val_name);"
//

#define GET_CALL(FN_NAME, VAR_NAME)             \
    if(!VAR_NAME)                               \
    {                                           \
        return E_POINTER;                       \
    }                                           \
                                                \
    MAKE_INSTANCE_CALL_1(FN_NAME, VAR_NAME);    \
                                                \
    return S_OK


#define SET_CALL(FN_NAME, VAR_NAME)             \
    MAKE_INSTANCE_CALL_1(FN_NAME, VAR_NAME);    \
                                                \
    return S_OK



/////////////////////////////////////////////////////////////////////////////
// RenderingControlServiceImpl

RenderingControlServiceImpl::RenderingControlServiceImpl()
    : VirtualServiceSupport<IRenderingControl>(DISPID_LASTCHANGE_RC, 1)
        // Have the first assigned-by-VSS ID to be 1, leaving 0 for post-mix control by convention
        // (Reference: RenderingControl:1 Service Template Version 1.01, section 1.2)
{
    InitErrDescrips();
    InitToolkitErrs();
}


RenderingControlServiceImpl::~RenderingControlServiceImpl()
{
}



//
// IUPnPService_RenderingControl1
//


STDMETHODIMP RenderingControlServiceImpl::get_LastChange(BSTR* pLastChange)
{
    return VirtualServiceSupport<IRenderingControl>::GetLastChange(pLastChange);
}



// non-"get_LastChange()" get_foo() methods are implemented only to satisfy upnp's devicehost, they are not used

STDMETHODIMP RenderingControlServiceImpl::get_PresetNameList(BSTR* pPresetNameList)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_Brightness(unsigned short* pBrightness)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_Contrast(unsigned short* pContrast)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_Sharpness(unsigned short* pSharpness)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_RedVideoGain(unsigned short* pRedVideoGain)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_GreenVideoGain(unsigned short* pGreenVideoGain)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_BlueVideoGain(unsigned short* pBlueVideoGain)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_RedVideoBlackLevel(unsigned short* pRedVideoBlackLevel)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_GreenVideoBlackLevel(unsigned short* pGreenVideoBlackLevel)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_BlueVideoBlackLevel(unsigned short* pBlueVideoBlackLevel)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_ColorTemperature(unsigned short* pColorTemperature)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_HorizontalKeystone(short* pHorizontalKeystone)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_VerticalKeystone(short* pVerticalKeystone)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_Mute(VARIANT_BOOL* pMute)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_Volume(unsigned short* pVolume)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_VolumeDB(short* pVolumeDB)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_Loudness(VARIANT_BOOL* pLoudness)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_A_ARG_TYPE_Channel(BSTR* pA_ARG_TYPE_Channel)
{
    return S_OK;
}

STDMETHODIMP RenderingControlServiceImpl::get_A_ARG_TYPE_InstanceID(unsigned long* pA_ARG_TYPE_InstanceID)
{
    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::get_A_ARG_TYPE_PresetName(BSTR* pA_ARG_TYPE_PresetName)
{
    return S_OK;
}



STDMETHODIMP RenderingControlServiceImpl::ListPresets(unsigned long InstanceID,
                                                      BSTR* pCurrentPresetNameList)
{
    if(!pCurrentPresetNameList)
    {
        return E_POINTER;
    }


    wstring strCurrentPresetNameList;

    MAKE_INSTANCE_CALL_1(ListPresets, &strCurrentPresetNameList);


    // Set out arg
    if(!(*pCurrentPresetNameList = SysAllocString(strCurrentPresetNameList)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }

    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::SelectPreset(unsigned long InstanceID,
                                                       BSTR PresetName)
{
    // Convert argument for call

    wstring strPresetName;

    if(!strPresetName.assign(PresetName, SysStringLen(PresetName)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    MAKE_INSTANCE_CALL_1(SelectPreset, strPresetName);


    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::GetBrightness(unsigned long InstanceID,
                                                        unsigned short* pCurrentBrightness)
{
    GET_CALL(GetBrightness, pCurrentBrightness);
}


STDMETHODIMP RenderingControlServiceImpl::SetBrightness(unsigned long InstanceID,
                                                        unsigned short DesiredBrightness)
{
    SET_CALL(SetBrightness, DesiredBrightness);
}


STDMETHODIMP RenderingControlServiceImpl::GetContrast(unsigned long InstanceID,
                                                      unsigned short* pCurrentContrast)
{
    GET_CALL(GetContrast, pCurrentContrast);
}


STDMETHODIMP RenderingControlServiceImpl::SetContrast(unsigned long InstanceID,
                                                      unsigned short DesiredContrast)
{
    SET_CALL(SetContrast, DesiredContrast);
}


STDMETHODIMP RenderingControlServiceImpl::GetSharpness(unsigned long InstanceID,
                                                       unsigned short* pCurrentSharpness)
{
    GET_CALL(GetSharpness, pCurrentSharpness);
}


STDMETHODIMP RenderingControlServiceImpl::SetSharpness(unsigned long InstanceID,
                                                       unsigned short DesiredSharpness)
{
    SET_CALL(SetSharpness, DesiredSharpness);
}


STDMETHODIMP RenderingControlServiceImpl::GetRedVideoGain(unsigned long InstanceID,
                                                          unsigned short* pCurrentRedVideoGain)
{
    GET_CALL(GetRedVideoGain, pCurrentRedVideoGain);
}


STDMETHODIMP RenderingControlServiceImpl::SetRedVideoGain(unsigned long InstanceID,
                                                          unsigned short DesiredRedVideoGain)
{
    SET_CALL(SetRedVideoGain, DesiredRedVideoGain);
}


STDMETHODIMP RenderingControlServiceImpl::GetGreenVideoGain(unsigned long InstanceID,
                                                            unsigned short* pCurrentGreenVideoGain)
{
    GET_CALL(GetGreenVideoGain, pCurrentGreenVideoGain);
}


STDMETHODIMP RenderingControlServiceImpl::SetGreenVideoGain(unsigned long InstanceID,
                                                            unsigned short DesiredGreenVideoGain)
{
    SET_CALL(SetGreenVideoGain, DesiredGreenVideoGain);
}


STDMETHODIMP RenderingControlServiceImpl::GetBlueVideoGain(unsigned long InstanceID,
                                                           unsigned short* pCurrentBlueVideoGain)
{
    GET_CALL(GetBlueVideoGain, pCurrentBlueVideoGain);
}


STDMETHODIMP RenderingControlServiceImpl::SetBlueVideoGain(unsigned long InstanceID,
                                                           unsigned short DesiredBlueVideoGain)
{
    SET_CALL(SetBlueVideoGain, DesiredBlueVideoGain);
}


STDMETHODIMP RenderingControlServiceImpl::GetRedVideoBlackLevel(unsigned long InstanceID,
                                                                unsigned short* pCurrentRedVideoBlackLevel)
{
    GET_CALL(GetRedVideoBlackLevel, pCurrentRedVideoBlackLevel);
}


STDMETHODIMP RenderingControlServiceImpl::SetRedVideoBlackLevel(unsigned long InstanceID,
                                                                unsigned short DesiredRedVideoBlackLevel)
{
    SET_CALL(SetRedVideoBlackLevel, DesiredRedVideoBlackLevel);
}


STDMETHODIMP RenderingControlServiceImpl::GetGreenVideoBlackLevel(unsigned long InstanceID,
                                                                  unsigned short* pCurrentGreenVideoBlackLevel)
{
    GET_CALL(GetGreenVideoBlackLevel, pCurrentGreenVideoBlackLevel);
}


STDMETHODIMP RenderingControlServiceImpl::SetGreenVideoBlackLevel(unsigned long InstanceID,
                                                                  unsigned short DesiredGreenVideoBlackLevel)
{
    SET_CALL(SetGreenVideoBlackLevel, DesiredGreenVideoBlackLevel);
}


STDMETHODIMP RenderingControlServiceImpl::GetBlueVideoBlackLevel(unsigned long InstanceID,
                                                                 unsigned short* pCurrentBlueVideoBlackLevel)
{
    GET_CALL(GetBlueVideoBlackLevel, pCurrentBlueVideoBlackLevel);

}


STDMETHODIMP RenderingControlServiceImpl::SetBlueVideoBlackLevel(unsigned long InstanceID,
                                                                 unsigned short DesiredBlueVideoBlackLevel)
{
    SET_CALL(SetBlueVideoBlackLevel, DesiredBlueVideoBlackLevel);
}


STDMETHODIMP RenderingControlServiceImpl::GetColorTemperature(unsigned long InstanceID,
                                                              unsigned short* pCurrentColorTemperature)
{
    GET_CALL(GetColorTemperature, pCurrentColorTemperature);

}


STDMETHODIMP RenderingControlServiceImpl::SetColorTemperature(unsigned long InstanceID,
                                                              unsigned short DesiredColorTemperature)
{
    SET_CALL(SetColorTemperature, DesiredColorTemperature);
}


STDMETHODIMP RenderingControlServiceImpl::GetHorizontalKeystone(unsigned long InstanceID,
                                                                short* pCurrentHorizontalKeystone)
{
    GET_CALL(GetHorizontalKeystone, pCurrentHorizontalKeystone);

}


STDMETHODIMP RenderingControlServiceImpl::SetHorizontalKeystone(unsigned long InstanceID,
                                                                short DesiredHorizontalKeystone)
{
    SET_CALL(SetHorizontalKeystone, DesiredHorizontalKeystone);
}


STDMETHODIMP RenderingControlServiceImpl::GetVerticalKeystone(unsigned long InstanceID,
                                                              short* pCurrentVerticalKeystone)
{
    GET_CALL(GetVerticalKeystone, pCurrentVerticalKeystone);

}


STDMETHODIMP RenderingControlServiceImpl::SetVerticalKeystone(unsigned long InstanceID,
                                                              short DesiredVerticalKeystone)
{
    SET_CALL(SetVerticalKeystone, DesiredVerticalKeystone);
}


STDMETHODIMP RenderingControlServiceImpl::GetMute(unsigned long InstanceID,
                                                  BSTR Channel,
                                                  VARIANT_BOOL* pCurrentMute)
{
    // Convert argument

    wstring strChannel;

    if(!strChannel.assign(Channel, SysStringLen(Channel)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    bool fCurrentMute;

    MAKE_INSTANCE_CALL_2(GetMute, strChannel, &fCurrentMute);


    // Set out arg
    if(fCurrentMute)
    {
        *pCurrentMute = VARIANT_TRUE;
    }
    else
    {
        *pCurrentMute = VARIANT_FALSE;
    }

    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::SetMute(unsigned long InstanceID,
                                                  BSTR Channel,
                                                  VARIANT_BOOL DesiredMute)
{
    // Convert arguments

    wstring strChannel;

    if(!strChannel.assign(Channel, SysStringLen(Channel)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }

    bool fDesiredMute;
    
    if(VARIANT_TRUE == DesiredMute)
    {
        fDesiredMute = true;
    }
    else if(VARIANT_FALSE == DesiredMute)
    {
        fDesiredMute = false;
    }
    else // illegal VARIANT_BOOL value
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    MAKE_INSTANCE_CALL_2(SetMute, strChannel, fDesiredMute);


    return S_OK;

}


STDMETHODIMP RenderingControlServiceImpl::GetVolume(unsigned long InstanceID,
                                                    BSTR Channel,
                                                    unsigned short* pCurrentVolume)
{
    // Convert argument

    wstring strChannel;

    if(!strChannel.assign(Channel, SysStringLen(Channel)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    MAKE_INSTANCE_CALL_2(GetVolume, strChannel, pCurrentVolume);

    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::SetVolume(unsigned long InstanceID,
                                                    BSTR Channel,
                                                    unsigned short DesiredVolume)
{
    // Convert arguments

    wstring strChannel;

    if(!strChannel.assign(Channel, SysStringLen(Channel)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    MAKE_INSTANCE_CALL_2(SetVolume, strChannel, DesiredVolume);


    return S_OK;

}


STDMETHODIMP RenderingControlServiceImpl::GetVolumeDB(unsigned long InstanceID,
                                                      BSTR Channel,
                                                      short* pCurrentVolume)
{
    // Convert argument

    wstring strChannel;

    if(!strChannel.assign(Channel, SysStringLen(Channel)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    MAKE_INSTANCE_CALL_2(GetVolumeDB, strChannel, pCurrentVolume);


    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::SetVolumeDB(unsigned long InstanceID,
                                                      BSTR Channel,
                                                      short DesiredVolume)
{
    // Convert arguments

    wstring strChannel;

    if(!strChannel.assign(Channel, SysStringLen(Channel)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    MAKE_INSTANCE_CALL_2(SetVolumeDB, strChannel, DesiredVolume);


    return S_OK;

}


STDMETHODIMP RenderingControlServiceImpl::GetVolumeDBRange(unsigned long InstanceID,
                                                           BSTR Channel,
                                                           short* pMinValue,
                                                           short* pMaxValue)
{
    // Convert argument

    wstring strChannel;

    if(!strChannel.assign(Channel, SysStringLen(Channel)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    MAKE_INSTANCE_CALL_3(GetVolumeDBRange, strChannel, pMinValue, pMaxValue);


    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::GetLoudness(unsigned long InstanceID,
                                                      BSTR Channel,
                                                      VARIANT_BOOL* pCurrentLoudness)
{
    // Convert argument

    wstring strChannel;

    if(!strChannel.assign(Channel, SysStringLen(Channel)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    bool fCurrentMute;

    MAKE_INSTANCE_CALL_2(GetLoudness, strChannel, &fCurrentMute);


    // Set out arg
    if(fCurrentMute)
    {
        *pCurrentLoudness = VARIANT_TRUE;
    }
    else
    {
        *pCurrentLoudness = VARIANT_FALSE;
    }

    return S_OK;
}


STDMETHODIMP RenderingControlServiceImpl::SetLoudness(unsigned long InstanceID,
                                                      BSTR Channel,
                                                      VARIANT_BOOL DesiredLoudness)
{
    // Convert arguments

    wstring strChannel;

    if(!strChannel.assign(Channel, SysStringLen(Channel)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }

    bool fDesiredLoudness;

    if(VARIANT_TRUE == DesiredLoudness)
    {
        fDesiredLoudness = true;
    }
    else if(VARIANT_FALSE == DesiredLoudness)
    {
        fDesiredLoudness = false;
    }
    else // illegal VARIANT_BOOL value
    {
        return m_ErrReport.ReportError(ERROR_AV_UPNP_ACTION_FAILED);
    }


    MAKE_INSTANCE_CALL_2(SetLoudness, strChannel, fDesiredLoudness);


    return S_OK;

}



//
// Private
//


HRESULT RenderingControlServiceImpl::InitToolkitErrs()
{
    const int vErrMap[][2] =
    {
        {ERROR_AV_POINTER,          ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_OOM,              ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_INVALID_INSTANCE, ERROR_AV_UPNP_RC_INVALID_INSTANCE_ID},
        {ERROR_AV_INVALID_STATEVAR, ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_ALREADY_INITED,   ERROR_AV_UPNP_ACTION_FAILED},
        {0,                         0} // 0 is used to denote the end of this array
    };


    for(unsigned int i=0; 0 != vErrMap[i][0]; ++i)
    {
        if(ERROR_AV_OOM == m_ErrReport.AddToolkitError(vErrMap[i][0], vErrMap[i][1]))
        {
            return E_OUTOFMEMORY;
        }
    }

    return S_OK;
}


HRESULT RenderingControlServiceImpl::InitErrDescrips()
{
    const int vErrNums[] =
    {
        // UPnP
        ERROR_AV_UPNP_INVALID_ACTION,
        ERROR_AV_UPNP_ACTION_FAILED,
        // RenderingControl
        ERROR_AV_UPNP_RC_INVALID_PRESET_NAME,
        ERROR_AV_UPNP_RC_INVALID_INSTANCE_ID,
        0 // 0 is used to denote the end of this array
    };
    
    const wstring vErrDescrips[] =
    {
        // UPnP
        L"Invalid Action",
        L"Action Failed",
        // ConnectionManager
        L"The specified name is not a valid preset name",
        L"The specified InstanceID is invalid",
    };


    for(unsigned int i=0; 0 != vErrNums[i]; ++i)
    {
        if(ERROR_AV_OOM == m_ErrReport.AddErrorDescription(vErrNums[i], vErrDescrips[i]))
        {
            return E_OUTOFMEMORY;
        }
    }

    return S_OK;
}
