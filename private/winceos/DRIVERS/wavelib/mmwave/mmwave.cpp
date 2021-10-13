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
#include <waveproxy.h>
#include <debug.h>
#include "DeviceId.h"

MMRESULT mapWaveGetDevCaps( LPWAVEOUTCAPS pwc, DWORD cbSize, BOOL bOutput);

// Maps a device ID.  Can optionally return the number of devices.
MMRESULT
mapGetPreferredId(
    BOOL fOutput,                   // Set to TRUE for wave output.
    __out DWORD *pdwPreferredId,    // The device ID to map
    __out_opt DWORD *pdwNumDevs,    // The number of devices.  Optional.
    DWORD dwStreamClassId = 0       // Stream class ID.  Optional.
    );

MMRESULT ProxyFromHandle(HWAVE hWave, BOOL bOutput, CWaveProxy **ppProxy)
{
    CWaveLib *pCWaveLib = GetWaveLib();
    if (!pCWaveLib)
    {
        return MMSYSERR_NOMEM;
    }

    CWaveProxy *pProxy=pCWaveLib->ProxyFromHandle(hWave, bOutput);
    if (!pProxy)
    {
        return MMSYSERR_INVALHANDLE;
    }

    *ppProxy = pProxy;
    return MMSYSERR_NOERROR;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutGetProperty(
                  UINT uDeviceId,
                  const GUID* pPropSetId,
                  ULONG ulPropId,
                  LPVOID pvPropParams,
                  ULONG cbPropParams,
                  LPVOID pvPropData,
                  ULONG cbPropData,
                  PULONG pcbReturn
                  )
{
    // The uDeviceId param can be a device ID or handle.
    if (IsDeviceId(uDeviceId))
    {
        // Extract the device ID only.
        uDeviceId = WAVE_GET_DEVICEID(uDeviceId);

        // If uDeviceId is WAVE_MAPPER, the call should affect the preferred device ID.
        if (uDeviceId==WAVE_MAPPER)
        {
            MMRESULT mmr = mapGetPreferredId(TRUE, (DWORD *)&uDeviceId, NULL);
            if (mmr != MMSYSERR_NOERROR)
            {
                return mmr;
            }
        }
    }

    const DWORD dwParams[] =  {
        (DWORD) uDeviceId,
        (DWORD) pPropSetId,
        (DWORD) ulPropId,
        (DWORD) pvPropParams,
        (DWORD) cbPropParams,
        (DWORD) pvPropData,
        (DWORD) cbPropData,
        (DWORD) pcbReturn
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_GET_PROPS, 8, dwParams));
}

MMRESULT
waveOutSetProperty(
                  UINT uDeviceId,
                  const GUID* pPropSetId,
                  ULONG ulPropId,
                  LPVOID pvPropParams,
                  ULONG cbPropParams,
                  LPVOID pvPropData,
                  ULONG cbPropData
                  )
{
    // The uDeviceId param can be a device ID or handle.
    if (IsDeviceId(uDeviceId))
    {
        // Extract the device ID only.
        uDeviceId = WAVE_GET_DEVICEID(uDeviceId);

        // If uDeviceId is WAVE_MAPPER, the call should affect the preferred device ID.
        if (uDeviceId==WAVE_MAPPER)
        {
            MMRESULT mmr = mapGetPreferredId(TRUE, (DWORD *)&uDeviceId, NULL);
            if (mmr != MMSYSERR_NOERROR)
            {
                return mmr;
            }
        }
    }

    const DWORD dwParams[] =  {
        (DWORD) uDeviceId,
        (DWORD) pPropSetId,
        (DWORD) ulPropId,
        (DWORD) pvPropParams,
        (DWORD) cbPropParams,
        (DWORD) pvPropData,
        (DWORD) cbPropData
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_SET_PROPS, 7, dwParams));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutBreakLoop(
                HWAVEOUT hWave
                )
{
    const DWORD dwParams[] =  {
        (DWORD) hWave
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_BREAK_LOOP, 1, dwParams));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT waveCommonGetDevCaps( UINT uDeviceId, LPWAVEOUTCAPS pwoc, UINT cbwoc, BOOL bOutput )
{
    // The uDeviceId param can be a device ID or handle.
    if (IsDeviceId(uDeviceId))
    {
        // Extract the device ID only.
        uDeviceId = WAVE_GET_DEVICEID(uDeviceId);

        if (uDeviceId==WAVE_MAPPER)
        {
            return mapWaveGetDevCaps(pwoc,cbwoc,bOutput);
        }
    }

    const DWORD dwParams[] =
    {
        (DWORD) uDeviceId,
        (DWORD) pwoc,
        (DWORD) cbwoc
    };

    DWORD dwIoCtl = (bOutput ? IOCTL_WAVE_OUT_GET_DEV_CAPS : IOCTL_WAVE_IN_GET_DEV_CAPS);
    return((MMRESULT) ___MmIoControl(dwIoCtl, 3, dwParams));
}

MMRESULT waveOutGetDevCaps( UINT uDeviceId, LPWAVEOUTCAPS pwoc, UINT cbwoc )
{
    return waveCommonGetDevCaps(uDeviceId, pwoc, cbwoc, TRUE);
}

MMRESULT waveInGetDevCaps( UINT uDeviceId, LPWAVEINCAPS pwoc, UINT cbwoc )
{
    return waveCommonGetDevCaps(uDeviceId, (LPWAVEOUTCAPS)pwoc, cbwoc, FALSE);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutGetErrorText(
                   MMRESULT mmrError,
                   LPTSTR pszText,
                   UINT cchText
                   )
{
    const DWORD dwParams[] =  {
        (DWORD) mmrError,
        (DWORD) pszText,
        (DWORD) cchText
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_GET_ERROR_TEXT, 3, dwParams));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutGetID(
            HWAVEOUT hWave,
            LPUINT   puDeviceID
            )
{
    const DWORD dwParams[] =  {
        (DWORD) hWave,
        (DWORD) puDeviceID
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_GET_ID, 2, dwParams));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
UINT
waveOutGetNumDevs(
                 void
                 )
{
    DWORD dwRet = 0;
    DWORD dwTmp;

    CWaveLib *pCWaveLib = GetWaveLib();
    if (pCWaveLib)
    {
        pCWaveLib->MmDeviceIoControl (IOCTL_WAVE_OUT_GET_NUM_DEVS, NULL, 0, &dwRet, sizeof(DWORD), &dwTmp);
    }
    else
    {
        SetLastError(ERROR_NOT_READY);
    }

    return((UINT) dwRet);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutGetPitch(
               HWAVEOUT hWave,
               LPDWORD pdwPitch
               )
{
    const DWORD dwParams[] =  {
        (DWORD) hWave,
        (DWORD) pdwPitch
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_GET_PITCH, 2, dwParams));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutGetPlaybackRate(
                      HWAVEOUT hWave,
                      LPDWORD pdwRate
                      )
{
    const DWORD dwParams[] =  {
        (DWORD) hWave,
        (DWORD) pdwRate
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_GET_PLAYBACK_RATE, 2, dwParams));
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutGetVolume(
                HWAVEOUT hWave,
                LPDWORD pdwVolume
                )
{
    // The hWave param can be a device ID or handle.
    if (IsDeviceId((UINT) hWave))
    {
        // Extract the device ID only.
        hWave = (HWAVEOUT) WAVE_GET_DEVICEID((UINT) hWave);

        // If hWave is WAVE_MAPPER, the call should affect the preferred device ID.
        if ((DWORD)hWave==WAVE_MAPPER)
        {
            MMRESULT mmr = mapGetPreferredId(TRUE, (DWORD *)&hWave, NULL);
            if (mmr != MMSYSERR_NOERROR)
            {
                return mmr;
            }
        }
    }

    const DWORD dwParams[] =  {
        (DWORD) hWave,
        (DWORD) pdwVolume
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_GET_VOLUME, 2, dwParams));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutMessage(
              HWAVEOUT hWave,
              UINT uMsg,
              DWORD dwParam1,
              DWORD dwParam2
              )
{
    // The hWave param can be a device ID or handle.
    if (IsDeviceId((UINT) hWave))
    {
        // Extract the device ID only.
        hWave = (HWAVEOUT) WAVE_GET_DEVICEID((UINT) hWave);

        // If hWave is WAVE_MAPPER, the call should affect the preferred device ID.
        if ((DWORD)hWave==WAVE_MAPPER)
        {
            MMRESULT mmr = mapGetPreferredId(TRUE, (DWORD *)&hWave, NULL);
            if (mmr != MMSYSERR_NOERROR)
            {
                return mmr;
            }
        }
    }

    if (uMsg==DRVM_MAPPER_PREFERRED_GET) {
        return mapGetPreferredId(TRUE, (LPDWORD)dwParam1, NULL);
    }

    if (uMsg==DRVM_MAPPER_PREFERRED_SET) {
        // preferred-set is done inside kernel
        // this is an OUT message, so do not set the high bit on dwParam1
    }

    const DWORD dwParams[] =  {
        (DWORD) hWave,
        (DWORD) uMsg,
        (DWORD) dwParam1,
        (DWORD) dwParam2
    };

    return((DWORD) ___MmIoControl(IOCTL_WAVE_OUT_MESSAGE, 4, dwParams));
}

MMRESULT
waveInMessage(
             HWAVEIN hWave,
             UINT uMsg,
             DWORD dwParam1,
             DWORD dwParam2
             )
{
    // The hWave param can be a device ID or handle.
    if (IsDeviceId((UINT) hWave))
    {
        // Extract the device ID only.
        hWave = (HWAVEIN) WAVE_GET_DEVICEID((UINT) hWave);

        // If hWave is WAVE_MAPPER, the call should affect the preferred device ID.
        if ((DWORD)hWave==WAVE_MAPPER)
        {
            MMRESULT mmr = mapGetPreferredId(FALSE, (DWORD *)&hWave, NULL);
            if (mmr != MMSYSERR_NOERROR)
            {
                return mmr;
            }
        }
    }

    if (uMsg==DRVM_MAPPER_PREFERRED_GET) {
        return mapGetPreferredId(FALSE, (LPDWORD)dwParam1, NULL);
    }

    if (uMsg==DRVM_MAPPER_PREFERRED_SET) {
        // preferred-set is done inside kernel
        // this is an IN message, so we set the high bit on dwParam1
        dwParam1 |= 0x80000000;
        // the stuff in the kernel looks for this to see if it is an in-set or an out-set
    }

    const DWORD dwParams[] =  {
        (DWORD) hWave,
        (DWORD) uMsg,
        (DWORD) dwParam1,
        (DWORD) dwParam2
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_IN_MESSAGE, 4, dwParams));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT waveCommonClose( HWAVE hWave, BOOL bOutput)
{
    CWaveProxy *pProxy;
    MMRESULT mmRet = ProxyFromHandle(hWave, bOutput, &pProxy);
    if (mmRet != MMSYSERR_NOERROR)
    {
        return mmRet;
    }

    mmRet = pProxy->waveClose();

    pProxy->Release();

    return mmRet;
}

MMRESULT waveOutClose( HWAVEOUT hWave )
{
    return waveCommonClose((HWAVE)hWave, TRUE);
}

MMRESULT waveInClose( HWAVEIN hWave )
{
    return waveCommonClose((HWAVE)hWave, FALSE);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT waveOutPause( HWAVEOUT hWave )
{
    const DWORD dwParams[] =
    {
        (DWORD) hWave
    };

    CWaveProxy *pProxy;
    MMRESULT mmRet = ProxyFromHandle((HWAVE) hWave, TRUE, &pProxy);
    if (mmRet != MMSYSERR_NOERROR)
    {
        return mmRet;
    }

    mmRet = (MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_PAUSE, 1, dwParams);

    pProxy->Release();

    return mmRet;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT waveCommonPrepareHeader( HWAVE hWave, LPWAVEHDR pwh, UINT cbwh, BOOL bOutput)
{
    CWaveProxy *pProxy;
    MMRESULT mmRet = ProxyFromHandle(hWave, bOutput, &pProxy);
    if (mmRet != MMSYSERR_NOERROR)
    {
        return mmRet;
    }

    mmRet = pProxy->wavePrepareHeader(pwh,cbwh);

    pProxy->Release();

    return mmRet;
}

MMRESULT waveOutPrepareHeader( HWAVEOUT hWave, LPWAVEHDR pwh, UINT cbwh )
{
    return waveCommonPrepareHeader((HWAVE)hWave, pwh, cbwh, TRUE);
}

MMRESULT waveInPrepareHeader( HWAVEIN hWave, LPWAVEHDR pwh, UINT cbwh )
{
    return waveCommonPrepareHeader((HWAVE)hWave, pwh, cbwh, FALSE);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT waveCommonUnprepareHeader( HWAVE hWave, LPWAVEHDR pwh, UINT cbwh, BOOL bOutput )
{
    CWaveProxy *pProxy;
    MMRESULT mmRet = ProxyFromHandle(hWave, bOutput, &pProxy);
    if (mmRet != MMSYSERR_NOERROR)
    {
        return mmRet;
    }

    mmRet = pProxy->waveUnprepareHeader(pwh,cbwh);

    pProxy->Release();

    return mmRet;
}

MMRESULT waveOutUnprepareHeader( HWAVEOUT hWave, LPWAVEHDR pwh, UINT cbwh )
{
    return waveCommonUnprepareHeader((HWAVE)hWave, pwh, cbwh, TRUE);
}

MMRESULT waveInUnprepareHeader( HWAVEIN hWave, LPWAVEHDR pwh, UINT cbwh )
{
    return waveCommonUnprepareHeader((HWAVE)hWave, pwh, cbwh, FALSE);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveCommonReset ( HWAVE hWave, BOOL bOutput )
{
    CWaveProxy *pProxy;
    MMRESULT mmRet = ProxyFromHandle(hWave, bOutput, &pProxy);
    if (mmRet != MMSYSERR_NOERROR)
    {
        return mmRet;
    }

    mmRet = pProxy->waveReset();

    pProxy->Release();

    return mmRet;
}

MMRESULT
waveOutReset( HWAVEOUT hWave )
{
    return waveCommonReset((HWAVE)hWave, TRUE);
}

MMRESULT
waveInReset( HWAVEIN hWave )
{
    return waveCommonReset((HWAVE)hWave, FALSE);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutSetPitch(
               HWAVEOUT hWave,
               DWORD dwPitch
               )
{
    const DWORD dwParams[] =  {
        (DWORD) hWave,
        (DWORD) dwPitch
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_SET_PITCH, 2, dwParams));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutSetPlaybackRate(
                      HWAVEOUT hWave,
                      DWORD dwRate
                      )
{
    const DWORD dwParams[] =  {
        (DWORD) hWave,
        (DWORD) dwRate
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_SET_PLAYBACK_RATE, 2, dwParams));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutSetVolume(
                HWAVEOUT hWave,
                DWORD dwVolume
                )
{
    // The hWave param can be a device ID or handle.
    if (IsDeviceId((UINT) hWave))
    {
        // Extract the device ID only.
        hWave = (HWAVEOUT) WAVE_GET_DEVICEID((UINT) hWave);

        // If hWave is WAVE_MAPPER, the call should affect the preferred device ID.
        if ((DWORD)hWave==WAVE_MAPPER)
        {
            MMRESULT mmr = mapGetPreferredId(TRUE, (DWORD *)&hWave, NULL);
            if (mmr != MMSYSERR_NOERROR)
            {
                return mmr;
            }
        }
    }

    const DWORD dwParams[] =  {
        (DWORD) hWave,
        (DWORD) dwVolume
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_OUT_SET_VOLUME, 2, dwParams));
}






//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT waveCommonQueueBuffer( HWAVE hWave, LPWAVEHDR pwh, UINT cbwh, BOOL bOutput )
{
    CWaveProxy *pProxy;
    MMRESULT mmRet = ProxyFromHandle(hWave, bOutput, &pProxy);
    if (mmRet != MMSYSERR_NOERROR)
    {
        return mmRet;
    }

    mmRet = pProxy->waveQueueBuffer(pwh, cbwh);

    pProxy->Release();

    return mmRet;
}

MMRESULT waveOutWrite( HWAVEOUT hWave, LPWAVEHDR pwh, UINT cbwh )
{
    return waveCommonQueueBuffer((HWAVE)hWave, pwh, cbwh, TRUE);
}

MMRESULT waveInAddBuffer( HWAVEIN hWave, LPWAVEHDR pwh, UINT cbwh )
{
    return waveCommonQueueBuffer((HWAVE)hWave, pwh, cbwh, FALSE);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveInGetProperty(
                 UINT uDeviceId,
                 const GUID* pPropSetId,
                 ULONG ulPropId,
                 LPVOID pvPropParams,
                 ULONG cbPropParams,
                 LPVOID pvPropData,
                 ULONG cbPropData,
                 PULONG pcbReturn
                 )
{
    // The uDeviceId param can be a device ID or handle.
    if (IsDeviceId(uDeviceId))
    {
        // Extract the device ID only.
        uDeviceId = WAVE_GET_DEVICEID(uDeviceId);

        // If hWave is WAVE_MAPPER, the call should affect the preferred device ID.
        if (uDeviceId==WAVE_MAPPER)
        {
            MMRESULT mmr = mapGetPreferredId(FALSE, (DWORD *)&uDeviceId, NULL);
            if (mmr != MMSYSERR_NOERROR)
            {
                return mmr;
            }
        }
    }

    const DWORD dwParams[] =  {
        (DWORD) uDeviceId,
        (DWORD) pPropSetId,
        (DWORD) ulPropId,
        (DWORD) pvPropParams,
        (DWORD) cbPropParams,
        (DWORD) pvPropData,
        (DWORD) cbPropData,
        (DWORD) pcbReturn
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_IN_GET_PROPS, 8, dwParams));
}

MMRESULT
waveInSetProperty(
                 UINT uDeviceId,
                 const GUID* pPropSetId,
                 ULONG ulPropId,
                 LPVOID pvPropParams,
                 ULONG cbPropParams,
                 LPVOID pvPropData,
                 ULONG cbPropData
                 )
{
    // The uDeviceId param can be a device ID or handle.
    if (IsDeviceId(uDeviceId))
    {
        // Extract the device ID incase WAVE_MAKE_DEVICEID is used.
        uDeviceId = WAVE_GET_DEVICEID(uDeviceId);

        // If hWave is WAVE_MAPPER, the call should affect the preferred device ID.
        if (uDeviceId==WAVE_MAPPER)
        {
            MMRESULT mmr = mapGetPreferredId(FALSE, (DWORD *)&uDeviceId, NULL);
            if (mmr != MMSYSERR_NOERROR)
            {
                return mmr;
            }
        }
    }

    const DWORD dwParams[] =  {
        (DWORD) uDeviceId,
        (DWORD) pPropSetId,
        (DWORD) ulPropId,
        (DWORD) pvPropParams,
        (DWORD) cbPropParams,
        (DWORD) pvPropData,
        (DWORD) cbPropData
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_IN_SET_PROPS, 7, dwParams));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveInGetErrorText(
                  MMRESULT mmrError,
                  LPTSTR pszText,
                  UINT cchText
                  )
{
    const DWORD dwParams[] =  {
        (DWORD) mmrError,
        (DWORD) pszText,
        (DWORD) cchText
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_IN_GET_ERROR_TEXT, 3, dwParams));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveInGetID(
           HWAVEIN hWave,
           LPUINT   puDeviceID
           )
{
    const DWORD dwParams[] =  {
        (DWORD) hWave,
        (DWORD) puDeviceID
    };

    return((MMRESULT) ___MmIoControl(IOCTL_WAVE_IN_GET_ID, 2, dwParams));
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
UINT
waveInGetNumDevs(
                void
                )
{
    DWORD dwRet = 0;
    DWORD dwTmp;

    CWaveLib *pCWaveLib = GetWaveLib();
    if (pCWaveLib)
    {
        pCWaveLib->MmDeviceIoControl(IOCTL_WAVE_IN_GET_NUM_DEVS, NULL, 0, &dwRet, sizeof(DWORD), &dwTmp);
    }
    else
    {
        SetLastError(ERROR_NOT_READY);
    }

    return((UINT) dwRet);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT waveCommonGetPosition( HWAVE hWave, LPMMTIME pmmt, UINT cbmmt, BOOL bOutput)
{
    CWaveProxy *pProxy;
    MMRESULT mmRet = ProxyFromHandle(hWave, bOutput, &pProxy);
    if (mmRet != MMSYSERR_NOERROR)
    {
        return mmRet;
    }

    mmRet = pProxy->waveGetPosition(pmmt, cbmmt);

    pProxy->Release();

    return mmRet;
}

MMRESULT waveOutGetPosition( HWAVEOUT hWave, LPMMTIME pmmt, UINT cbmmt)
{
    return waveCommonGetPosition((HWAVE)hWave, pmmt, cbmmt, TRUE);
}

MMRESULT waveInGetPosition( HWAVEIN hWave, LPMMTIME pmmt, UINT cbmmt)
{
    return waveCommonGetPosition((HWAVE)hWave, pmmt, cbmmt, FALSE);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveInStop(
          HWAVEIN hWave
          )
{
    const DWORD dwParams[] =  {
        (DWORD) hWave
    };

    CWaveProxy *pProxy;
    MMRESULT mmRet = ProxyFromHandle((HWAVE) hWave, FALSE, &pProxy);
    if (mmRet != MMSYSERR_NOERROR)
    {
        return mmRet;
    }

    mmRet = (MMRESULT) ___MmIoControl(IOCTL_WAVE_IN_STOP, 1, dwParams);

    pProxy->Release();

    return mmRet;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveCommonStart(
    HWAVE hWave,
    BOOL bOutput
    )
{
    const DWORD dwParams[] =  {
        (DWORD) hWave
    };

    CWaveProxy *pProxy;
    MMRESULT mmRet = ProxyFromHandle(hWave, bOutput, &pProxy);
    if (mmRet != MMSYSERR_NOERROR)
    {
        return mmRet;
    }

    mmRet = ((MMRESULT) ___MmIoControl(bOutput ? IOCTL_WAVE_OUT_RESTART : IOCTL_WAVE_IN_START, 1, dwParams));

    pProxy->Release();

    return mmRet;
}

MMRESULT
waveOutRestart(
    HWAVEOUT hWave
    )
{
    return waveCommonStart((HWAVE) hWave, TRUE);
}

MMRESULT
waveInStart(
    HWAVEIN hWave
    )
{
    return waveCommonStart((HWAVE) hWave, FALSE);
}

// waveCommonOpen is the top-level common entry point for opening a wave stream
MMRESULT
waveCommonOpen(
              HWAVE          *phw,
              UINT            uDeviceID,
              LPCWAVEFORMATEX pwfx,
              DWORD           dwCallback,
              DWORD           dwInstance,
              DWORD           fdwOpen,
              BOOL            bOutput
              )
{
    MMRESULT mmRet;
    BOOL bQuery = (0 != (WAVE_FORMAT_QUERY & fdwOpen));
    DWORD dwStreamClassId;

    // Extract the device ID and stream class ID.
    dwStreamClassId = WAVE_GET_STREAMCLASSID(uDeviceID);
    uDeviceID = WAVE_GET_DEVICEID(uDeviceID);

    BOOL bMapDeviceId = (uDeviceID==WAVE_MAPPER);
    BOOL bUseACM = ((uDeviceID==WAVE_MAPPER) && !(fdwOpen & WAVE_FORMAT_DIRECT)) || (fdwOpen & WAVE_MAPPED);

    // Get ptr to wavelib object
    CWaveLib *pCWaveLib = GetWaveLib();
    if (!pCWaveLib)
    {
        return MMSYSERR_NOMEM;
    }

    // Open wave streams may use the callback interface, so make sure it's installed
    if (!pCWaveLib->InitCallbackInterface())
    {
        return MMSYSERR_NOMEM;
    }

    // There are two proxys- a standard one and an ACM one (for when we do format conversions using ACM).
    // We don't know which one we'll need, so we might have to allocate both before we figure it out.

    CWaveProxy *pProxy = NULL;
    CWaveProxy *pProxyACM = NULL;

    // Whichever proxy turns out to be the right one, we'll remember it here.
    CWaveProxy *pProxyToUse = NULL;

    // We need to allocate a standard proxy first
    pProxy = CreateWaveProxy(dwCallback,dwInstance,fdwOpen,bOutput,pCWaveLib);
    if (!pProxy)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("FilterWaveOpen: unable to create proxy instance\r\n")));
        mmRet = MMSYSERR_NOMEM;
        goto Exit;
    }

    // Set stream class ID.
    pProxy->SetStreamClassId(dwStreamClassId);

    // To keep a common code path, we use the same loop in both the case where
    // the caller specified a specific device ID and where they asked us to choose
    // a device ID.
    DWORD uNumDevs, uFirst, uLast;
    if (bMapDeviceId)
    {
        // Mapped device ID. We need to search each available ID starting at the preferred device
        // We can get both the preferred device and total # of devices with one call
        mapGetPreferredId(bOutput, &uFirst, &uNumDevs, dwStreamClassId);
        if (uNumDevs==0)
        {
            mmRet = MMSYSERR_NODRIVER;
            goto Exit;
        }

        uLast = (uFirst==0) ? uNumDevs-1 : uFirst-1;
    }
    else
    {
        // User-specified device ID. We will only try this one ID.
        uNumDevs = uDeviceID; // Doesn't really matter for this case
        uFirst = uDeviceID;
        uLast = uDeviceID;
    }

    // Loop to search for a compatible device
    DWORD uDevId = uFirst;
    while (1)
    {
        //  Try to open this wave device with this format without using ACM
        pProxy->SetDeviceId(uDevId);
        mmRet = pProxy->waveOpen(pwfx);

        // If we succeeded, we're done!
        if (MMSYSERR_NOERROR == mmRet)
        {
            pProxyToUse = pProxy;
            break;  //  Acm doesn't need to get involved
        }

        // Try to use ACM to open the device if the format is not supported
        if (((WAVERR_BADFORMAT == mmRet) || (MMSYSERR_NOTSUPPORTED == mmRet)) && bUseACM)
        {
            // Lazy-allocate an ACM proxy
            if (!pProxyACM)
            {
                pProxyACM = CreateWaveProxyACM(dwCallback,dwInstance,fdwOpen,bOutput,pCWaveLib);
                if (pProxyACM)
                {
                    pProxyACM->SetStreamClassId(dwStreamClassId);
                }
            }

            if (pProxyACM)
            {
                pProxyACM->SetDeviceId(uDevId);
                MMRESULT mmRetACM = pProxyACM->waveOpen(pwfx);

                // If we succeeded, we're done!
                if (mmRetACM == MMSYSERR_NOERROR)
                {
                    // Use the ACM proxy
                    pProxyToUse = pProxyACM;
                    mmRet = mmRetACM;
                    break;
                }
            }
        }

        // Are we done with the loop yet?
        if (uDevId==uLast)
        {
            break;
        }

        // Wrap around if we're about to fall off the end
        uDevId++;
        if (uDevId==uNumDevs)
        {
            uDevId=0;
        }
    }

Exit:
    // if we succeeded & this isn't a query
    if ((mmRet == MMSYSERR_NOERROR) && (!bQuery))
    {
        // This will take a ref on whichever proxy we used
        *phw = pCWaveLib->AddProxy(pProxyToUse);

        // Do the client WxM_OPEN callback on the client's thread before returning
        DWORD uMsg = (bOutput ? WOM_OPEN : WIM_OPEN);
        pProxyToUse->DoClientCallback(uMsg,0,0);
    }

    // Release proxies; AddProxy will have taken a ref on whichever one was successful
    if (pProxy)
    {
        pProxy->Release();
    }

    if (pProxyACM)
    {
        pProxyACM->Release();
    }

    return mmRet;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
waveOutOpen(
           LPHWAVEOUT      phwo,
           UINT            uDeviceId,
           LPCWAVEFORMATEX pwfx,
           DWORD           dwCallback,
           DWORD           dwInstance,
           DWORD           fdwOpen
           )
{
    return waveCommonOpen((HWAVE *)phwo,
                          uDeviceId,
                          pwfx,
                          dwCallback,
                          dwInstance,
                          fdwOpen,
                          TRUE
                         );
}

MMRESULT
waveInOpen(
          LPHWAVEIN        phwi,
          UINT             uDeviceId,
          LPCWAVEFORMATEX  pwfx,
          DWORD            dwCallback,
          DWORD            dwInstance,
          DWORD            fdwOpen
          )
{
    return waveCommonOpen((HWAVE *)phwi,
                          uDeviceId,
                          pwfx,
                          dwCallback,
                          dwInstance,
                          fdwOpen,
                          FALSE);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
mapGetPreferredId(
    BOOL fOutput,
    DWORD *pdwPreferredId,
    DWORD *pdwNumDevs,
    DWORD dwStreamClassId
    )
{
    if (NULL == pdwPreferredId)
    {
        return MMSYSERR_INVALPARAM;
    }

    // Default to device zero.
    DWORD dwPreferredId = 0;

    // Get number of devices.
    DWORD dwNumDevs = fOutput ? waveOutGetNumDevs() : waveInGetNumDevs();

    *pdwPreferredId = dwPreferredId;

    if (pdwNumDevs != NULL)
    {
        *pdwNumDevs = dwNumDevs;
    }

    return MMSYSERR_NOERROR;
}

MMRESULT mapWaveGetDevCaps( LPWAVEOUTCAPS pwc, DWORD cbSize, BOOL bOutput)
{
    MMRESULT    mmRet;
    WAVEOUTCAPS woc;
    DWORD       cWaveDevs;

    if (bOutput)
    {
        cbSize = min(sizeof(WAVEOUTCAPS), cbSize);
        cWaveDevs = waveOutGetNumDevs();
    }
    else
    {
        cbSize = min(sizeof(WAVEINCAPS), cbSize);
        cWaveDevs = waveInGetNumDevs();
    }

    //
    //  If there is only one mappable device ID, then get the caps from it to
    //  fill in the dwSupport fields.  Otherwise, let's hardcode the dwSupport
    //  field.
    //
    if (cWaveDevs==1)
    {
        if (bOutput)
        {
            mmRet = waveOutGetDevCaps(0, &woc, cbSize);
        }
        else
        {
            mmRet = waveInGetDevCaps(0, (LPWAVEINCAPS)&woc, cbSize);
        }
    }
    else
    {
        woc.dwSupport = WAVECAPS_VOLUME | WAVECAPS_LRVOLUME;
        mmRet         = MMSYSERR_NOERROR;
    }

    //
    //  Bail on error
    //
    if (MMSYSERR_NOERROR != mmRet)
    {
        goto Exit;
    }

    //
    //  Fill in the rest of the fields.
    //
    woc.wMid        = MM_MICROSOFT;
    woc.wPid        = MM_WAVE_MAPPER;

#define VERSION_MSACMMAP_MAJOR  3
#define VERSION_MSACMMAP_MINOR  50
#define VERSION_MSACMMAP        ((VERSION_MSACMMAP_MAJOR << 8) | VERSION_MSACMMAP_MINOR)

#define IDS_ACM_CAPS_TAG                    400
#define IDS_ACM_CAPS_DESCRIPTION            (IDS_ACM_CAPS_TAG+0)

    woc.vDriverVersion  = VERSION_MSACMMAP;
    woc.wChannels   = 2;

    LoadString(g_hInstance, IDS_ACM_CAPS_DESCRIPTION, woc.szPname, _countof(woc.szPname));
    //wcscpy(woc.szPname, TEXT("Microsoft Sound Mapper"));

    //
    //
    //
    woc.dwFormats      = WAVE_FORMAT_1M08 |
                         WAVE_FORMAT_1S08 |
                         WAVE_FORMAT_1M16 |
                         WAVE_FORMAT_1S16 |
                         WAVE_FORMAT_2M08 |
                         WAVE_FORMAT_2S08 |
                         WAVE_FORMAT_2M16 |
                         WAVE_FORMAT_2S16 |
                         WAVE_FORMAT_4M08 |
                         WAVE_FORMAT_4S08 |
                         WAVE_FORMAT_4M16 |
                         WAVE_FORMAT_4S16;

    memcpy(pwc, &woc, cbSize);

    mmRet = MMSYSERR_NOERROR;
    Exit:

    return(mmRet);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID
AudioUpdateFromRegistry(VOID)
{
    const DWORD dwParams[] =  {
        (DWORD) -2L,                // This hWaveOut has special meaning.
        (DWORD) 1,
        (DWORD) 0,
        (DWORD) 0
    };

    ___MmIoControl(IOCTL_WAVE_OUT_MESSAGE, 4, dwParams);
}



