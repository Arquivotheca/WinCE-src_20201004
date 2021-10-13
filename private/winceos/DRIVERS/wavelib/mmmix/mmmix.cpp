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
#include <wavelib.h>

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
mixerGetControlDetails (
                       HMIXEROBJ hmxobj,
                       LPMIXERCONTROLDETAILS pmxcd,
                       DWORD fdwDetails
                       )
{
    const DWORD dwParams[] =
    {
        (DWORD) hmxobj,
        (DWORD) pmxcd,
        (DWORD) fdwDetails
    };

    return (MMRESULT) ___MmIoControl(IOCTL_MIXER_GETCONTROLDETAILS, 3, dwParams);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
mixerGetDevCaps (UINT uMxId, LPMIXERCAPS pmxcaps, UINT cbmxcaps)

{
    const DWORD dwParams[] =
    {
        (DWORD) uMxId,
        (DWORD) pmxcaps,
        (DWORD) cbmxcaps
    };
    return (MMRESULT) ___MmIoControl(IOCTL_MIXER_GETDEVCAPS, 3, dwParams);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
mixerGetID (HMIXEROBJ hmxobj, PUINT puMxId, DWORD fdwId)

{
    const DWORD dwParams[] =
    {
        (DWORD) hmxobj,
        (DWORD) puMxId,
        (DWORD) fdwId
    };
    return (MMRESULT) ___MmIoControl(IOCTL_MIXER_GETID, 3, dwParams);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
mixerGetLineControls(
                    HMIXEROBJ hmxobj,
                    LPMIXERLINECONTROLS pmxlc,
                    DWORD fdwControls
                    )
{
    const DWORD dwParams[] =
    {
        (DWORD) hmxobj,
        (DWORD) pmxlc,
        (DWORD) fdwControls
    };
    return (MMRESULT) ___MmIoControl(IOCTL_MIXER_GETLINECONTROLS, 3, dwParams);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
mixerGetLineInfo(
                HMIXEROBJ hmxobj,
                LPMIXERLINE pmxl,
                DWORD fdwInfo
                )
{
    const DWORD dwParams[] =
    {
        (DWORD) hmxobj,
        (DWORD) pmxl,
        (DWORD) fdwInfo
    };
    return (MMRESULT) ___MmIoControl(IOCTL_MIXER_GETLINEINFO, 3, dwParams);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
UINT
mixerGetNumDevs (VOID)
{
    DWORD dwRet = 0;
    DWORD dwTmp;

    CWaveLib *pCWaveLib = GetWaveLib();
    if (pCWaveLib)
        {
        pCWaveLib->MmDeviceIoControl (IOCTL_MIXER_GET_NUM_DEVS, NULL, 0, &dwRet, sizeof(DWORD), &dwTmp);
        }
    else
        {
        SetLastError(ERROR_NOT_READY);
        }

    return (UINT) dwRet;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD
mixerMessage (
             HMIXER hmx,
             UINT uMsg,
             DWORD dwParam1,
             DWORD dwParam2
             )
{
    const DWORD dwParams[] =
    {
        (DWORD) hmx,
        (DWORD) uMsg,
        (DWORD) dwParam1,
        (DWORD) dwParam2
    };
    return (MMRESULT) ___MmIoControl(IOCTL_MIXER_MESSAGE, 4, dwParams);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
mixerOpen (
          LPHMIXER phmx,
          UINT uMxId,
          DWORD dwCallback,
          DWORD dwInstance,
          DWORD fdwOpen
          )
{
    // Get ptr to wavelib object
    CWaveLib *pCWaveLib = GetWaveLib();
    if (!pCWaveLib)
        {
        return MMSYSERR_NOMEM;
        }

    if (!pCWaveLib->InitCallbackInterface())
        {
        return MMSYSERR_NOMEM;
        }

    const DWORD dwParams[] =
    {
        (DWORD) phmx,
        (DWORD) uMxId,
        (DWORD) dwCallback,
        (DWORD) dwInstance,
        (DWORD) fdwOpen
    };
    return (MMRESULT) ___MmIoControl(IOCTL_MIXER_OPEN, 5, dwParams);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
mixerSetControlDetails (
                       HMIXEROBJ hmxobj,
                       LPMIXERCONTROLDETAILS pmxcd,
                       DWORD fdwDetails
                       )
{
    const DWORD dwParams[] =
    {
        (DWORD) hmxobj,
        (DWORD) pmxcd,
        (DWORD) fdwDetails
    };
    return (MMRESULT) ___MmIoControl(IOCTL_MIXER_SETCONTROLDETAILS, 3, dwParams);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
mixerClose(
          HMIXER hmx
          )
{
    const DWORD dwParams[] =  {
        (DWORD) hmx
    };

    return  (MMRESULT) ___MmIoControl(IOCTL_MIXER_CLOSE, 1, dwParams);
}
