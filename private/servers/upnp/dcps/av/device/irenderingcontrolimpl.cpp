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

using namespace av_upnp;


/////////////////////////////////////////////////////////////////////////////
// IRenderingControlImpl

//
// IRenderingControl
//

DWORD IRenderingControlImpl::GetBrightness(
    /* [in, out] */ unsigned short* pBrightness)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetBrightness(
    /* [in] */ unsigned short Brightness)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetContrast(
    /* [in, out] */ unsigned short* pContrast)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetContrast(
    /* [in] */ unsigned short Contrast)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetSharpness(
    /* [in, out] */ unsigned short* pSharpness)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetSharpness(
    /* [in] */ unsigned short Sharpness)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetRedVideoGain(
    /* [in, out] */ unsigned short* pRedVideoGain)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetRedVideoGain(
    /* [in] */ unsigned short RedVideoGain)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetGreenVideoGain(
    /* [in, out] */ unsigned short* pGreenVideoGain)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetGreenVideoGain(
    /* [in] */ unsigned short GreenVideoGain)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetBlueVideoGain(
    /* [in, out] */ unsigned short* pBlueVideoGain)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetBlueVideoGain(
    /* [in] */ unsigned short BlueVideoGain)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetRedVideoBlackLevel(
    /* [in, out] */ unsigned short* pRedVideoBlackLevel)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetRedVideoBlackLevel(
    /* [in] */ unsigned short RedVideoBlackLevel)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetGreenVideoBlackLevel(
    /* [in, out] */ unsigned short* pGreenVideoBlackLevel)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetGreenVideoBlackLevel(
    /* [in] */ unsigned short GreenVideoBlackLevel)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetBlueVideoBlackLevel(
    /* [in, out] */ unsigned short* pBlueVideoBlackLevel)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetBlueVideoBlackLevel(
    /* [in] */ unsigned short BlueVideoBlackLevel)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetColorTemperature(
    /* [in, out] */ unsigned short* pColorTemperature)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetColorTemperature(
    /* [in] */ unsigned short ColorTemperature)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetHorizontalKeystone(
    /* [in, out] */ short* pHorizontalKeystone)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetHorizontalKeystone(
    /* [in] */ short HorizontalKeystone)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetVerticalKeystone(
    /* [in, out] */ short* pVerticalKeystone)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetVerticalKeystone(
    /* [in] */ short VerticalKeystone)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetMute(
    /* [in] */ LPCWSTR pszChannel,
    /* [in, out] */ bool* pMute)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetMute(
    /* [in] */ LPCWSTR pszChannel,
    /* [in] */ bool Mute)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetVolume(
    /* [in] */ LPCWSTR pszChannel,
    /* [in, out] */ unsigned short* pVolume)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetVolume(
    /* [in] */ LPCWSTR pszChannel,
    /* [in] */ unsigned short Volume)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetVolumeDB(
    /* [in] */ LPCWSTR pszChannel,
    /* [in, out] */ short* pVolumeDB)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetVolumeDB(
    /* [in] */ LPCWSTR pszChannel,
    /* [in] */ short VolumeDB)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetVolumeDBRange(
    /* [in] */ LPCWSTR pszChannel,
    /* [in, out] */ short* pMinValue,
    /* [in, out] */ short* pMaxValue)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::GetLoudness(
    /* [in] */ LPCWSTR pszChannel,
    /* [in, out] */ bool* pLoudness)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}


DWORD IRenderingControlImpl::SetLoudness(
    /* [in] */ LPCWSTR pszChannel,
    /* [in] */ bool Loudness)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}



DWORD IRenderingControlImpl::InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult)
{
    return ERROR_AV_UPNP_INVALID_ACTION;
}
