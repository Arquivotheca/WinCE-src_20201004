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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// -----------------------------------------------------------------------------
//
//      THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//      ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//      THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//      PARTICULAR PURPOSE.
//
// -----------------------------------------------------------------------------
#include "wavemain.h"
#include <svsutil.hxx>
#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_api.h>
#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("WaveDriver"), {
         TEXT("Test")           //  0
        ,TEXT("Params")         //  1
        ,TEXT("Verbose")        //  2
        ,TEXT("Interrupt")      //  3
        ,TEXT("WODM")           //  4
        ,TEXT("WIDM")           //  5
        ,TEXT("PDD")            //  6
        ,TEXT("MDD")            //  7
        ,TEXT("Regs")           //  8
        ,TEXT("Misc")           //  9
        ,TEXT("Init")           // 10
        ,TEXT("IOcontrol")      // 11
        ,TEXT("Alloc")          // 12
        ,TEXT("Function")       // 13
        ,TEXT("Warning")        // 14
        ,TEXT("Error")          // 15
    }
    ,
        (1 << 15)   // Errors
    |   (1 << 14)   // Warnings
};
#endif

CRITICAL_SECTION g_csWaveMainCS;

BOOL CALLBACK DllMain(HANDLE hDLL,
                      DWORD dwReason,
                      LPVOID lpvReserved)
{
    switch (dwReason) {
        case DLL_PROCESS_ATTACH :
            DEBUGREGISTER((HINSTANCE)hDLL);
            DisableThreadLibraryCalls((HMODULE) hDLL);
            InitializeCriticalSection(&g_csWaveMainCS);
            break;

        case DLL_PROCESS_DETACH :
            DeleteCriticalSection(&g_csWaveMainCS);
            break;

        case DLL_THREAD_DETACH :
            break;

        case DLL_THREAD_ATTACH :
            break;

        default :
            break;
    }
    return TRUE;
}


// -----------------------------------------------------------------------------
//
// @doc     WDEV_EXT
//
// @topic   WAV Device Interface | Implements the WAVEDEV.DLL device
//          interface. These functions are required for the device to
//          be loaded by DEVICE.EXE.
//
// @xref                          <nl>
//          <f WAV_Init>,         <nl>
//          <f WAV_Deinit>,       <nl>
//          <f WAV_Open>,         <nl>
//          <f WAV_Close>,        <nl>
//          <f WAV_IOControl>     <nl>
//
// -----------------------------------------------------------------------------
//
// @doc     WDEV_EXT
//
// @topic   Designing a Waveform Audio Driver |
//          A waveform audio driver is responsible for processing messages
//          from the Wave API Manager (WAVEAPI.DLL) to playback and record
//          waveform audio. Waveform audio drivers are implemented as
//          dynamic link libraries that are loaded by DEVICE.EXE The
//          default waveform audio driver is named WAVEDEV.DLL (see figure).
//          The messages passed to the audio driver are similar to those
//          passed to a user-mode Windows NT audio driver (such as mmdrv.dll).
//
//          <bmp blk1_bmp>
//
//          Like all device drivers loaded by DEVICE.EXE, the waveform
//          audio driver must export the standard device functions,
//          XXX_Init, XXX_Deinit, XXX_IoControl, etc (see
//          <t WAV Device Interface>). The Waveform Audio Drivers
//          have a device prefix of "WAV".
//
//          Driver loading and unloading is handled by DEVICE.EXE and
//          WAVEAPI.DLL. Calls are made to <f WAV_Init> and <f WAV_Deinit>.
//          When the driver is opened by WAVEAPI.DLL calls are made to
//          <f WAV_Open> and <f WAV_Close>.  All
//          other communication between WAVEAPI.DLL and WAVEDEV.DLL is
//          done by calls to <f WAV_IOControl>. The other WAV_xxx functions
//          are not used.
//
// @xref                                          <nl>
//          <t Designing a Waveform Audio PDD>    <nl>
//          <t WAV Device Interface>              <nl>
//          <t Wave Input Driver Messages>        <nl>
//          <t Wave Output Driver Messages>       <nl>
//
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//
//  @doc    WDEV_EXT
//
//  @func   PVOID | WAV_Init | Device initialization routine
//
//  @parm   DWORD | dwInfo | info passed to RegisterDevice
//
//  @rdesc  Returns a DWORD which will be passed to Open & Deinit or NULL if
//          unable to initialize the device.
//
// -----------------------------------------------------------------------------
extern "C" DWORD WAV_Init(DWORD Index)
{    
    return((DWORD)HardwareContext::CreateHWContext(Index));
}

// -----------------------------------------------------------------------------
//
//  @doc    WDEV_EXT
//
//  @func   PVOID | WAV_Deinit | Device deinitialization routine
//
//  @parm   DWORD | dwData | value returned from WAV_Init call
//
//  @rdesc  Returns TRUE for success, FALSE for failure.
//
// -----------------------------------------------------------------------------
extern "C" BOOL WAV_Deinit(DWORD dwData)
{
    //return(g_pHWContext->Deinit());
    return FALSE;
}

// -----------------------------------------------------------------------------
//
//  @doc    WDEV_EXT
//
//  @func   PVOID | WAV_Open    | Device open routine
//
//  @parm   DWORD | dwData      | Value returned from WAV_Init call (ignored)
//
//  @parm   DWORD | dwAccess    | Requested access (combination of GENERIC_READ
//                                and GENERIC_WRITE) (ignored)
//
//  @parm   DWORD | dwShareMode | Requested share mode (combination of
//                                FILE_SHARE_READ and FILE_SHARE_WRITE) (ignored)
//
//  @rdesc  Returns a DWORD which will be passed to Read, Write, etc or NULL if
//          unable to open device.
//
// -----------------------------------------------------------------------------
extern "C" PDWORD WAV_Open( DWORD dwData,
              DWORD dwAccess,
              DWORD dwShareMode)
{
    // allocate and return handle context to efficiently verify caller trust level
    return new DWORD(NULL); // assume untrusted. Can't tell for sure until WAV_IoControl
}

// -----------------------------------------------------------------------------
//
//  @doc    WDEV_EXT
//
//  @func   BOOL | WAV_Close | Device close routine
//
//  @parm   DWORD | dwOpenData | Value returned from WAV_Open call
//
//  @rdesc  Returns TRUE for success, FALSE for failure
//
// -----------------------------------------------------------------------------
extern "C" BOOL WAV_Close(PDWORD pdwData)
{
    // we trust the device manager to give us a valid context to free.
    delete pdwData;
    return(TRUE);
}

BOOL HandleWaveMessage(PMMDRV_MESSAGE_PARAMS pParams, DWORD *pdwResult)
{

    UINT Msg = pParams->uMsg;
    UINT DeviceId = pParams->uDeviceId;
    DWORD Param1 = pParams->dwParam1;
    DWORD Param2 = pParams->dwParam2;
    DWORD User   = pParams->dwUser;
    StreamContext *pStreamContext = (StreamContext *)User;

    DWORD dwRet;
    BOOL LockingMsg = FALSE;

//messages that don't need locking
    switch (Msg)
    {
        case WODM_GETDEVCAPS:
        {
            DeviceContext* pDeviceContext = g_pHWContext->GetOutputDeviceContext(DeviceId);
            dwRet = pDeviceContext->GetDevCaps((PVOID)Param1,Param2);
            break;
        }
        default:
            LockingMsg = TRUE;
    }

    if(LockingMsg)
    {
        // We need a critical section that protects against multiple outstanding calls into
        // the driver.  The HWContext lock only guards data in the HWContext class.  We need
        // this additional CS since that lock does not always stay locked for the life of a
        // particular HandleWaveMessage call.
        EnterCriticalSection(&g_csWaveMainCS);
        
        g_pHWContext->Lock();
        switch (Msg)
        {
#ifdef INPUT_ON

        case WIDM_GETDEVCAPS:
            {
                DeviceContext *pDeviceContext;

                if (pStreamContext)
                {
                    pDeviceContext=pStreamContext->GetDeviceContext();
                }
                else
                {
                    pDeviceContext = g_pHWContext->GetInputDeviceContext(DeviceId);
                }

                dwRet = pDeviceContext->GetDevCaps((PVOID)Param1,Param2);
                break;
            }
        case WIDM_OPEN:
            {
                DEBUGMSG(ZONE_WIDM, (TEXT("WIDM_OPEN\r\n")));                
                //RETAILMSG(1, (TEXT("WIDM_OPEN\r\n")));
                DeviceContext *pDeviceContext = g_pHWContext->GetInputDeviceContext(DeviceId);
                dwRet = pDeviceContext->OpenStream((LPWAVEOPENDESC)Param1, Param2, (StreamContext **)User);
                break;
            }
#endif
        case WODM_GETNUMDEVS:
            {
                dwRet = g_pHWContext->GetNumOutputDevices();
                break;
            }

        case WODM_GETEXTDEVCAPS:
            {
                DeviceContext *pDeviceContext;
                UINT NumDevs = g_pHWContext->GetNumOutputDevices();

                if (pStreamContext)
                {
                    pDeviceContext=pStreamContext->GetDeviceContext();
                }
                else
                {
                    pDeviceContext = g_pHWContext->GetOutputDeviceContext(DeviceId);
                }

                dwRet = pDeviceContext->GetExtDevCaps((PVOID)Param1,Param2);
                break;
            }

        case WODM_OPEN:
            {
                DEBUGMSG(ZONE_WODM, (TEXT("WODM_OPEN\r\n")));
                //RETAILMSG(1, (TEXT("WODM_OPEN\r\n")));

                DeviceContext *pDeviceContext = g_pHWContext->GetOutputDeviceContext(DeviceId);
                dwRet = pDeviceContext->OpenStream((LPWAVEOPENDESC)Param1, Param2, (StreamContext **)User);
                break;
            }
        case WIDM_GETNUMDEVS:
            {
                dwRet = g_pHWContext->GetNumInputDevices();
                break;
            }


        case WODM_CLOSE:
        case WIDM_CLOSE:
            {
                DEBUGMSG(ZONE_WODM || ZONE_WIDM, (TEXT("WIDM_CLOSE/WODM_CLOSE\r\n")));
                //RETAILMSG(1, (TEXT("WIDM_CLOSE/WODM_CLOSE\r\n")));

                dwRet = pStreamContext->Close();

                // Release stream context here, rather than inside StreamContext::Close, so that if someone
                // (like CMidiStream) has subclassed Close there's no chance that the object will get released
                // out from under them.
                if (dwRet==MMSYSERR_NOERROR)
                {
                    pStreamContext->Release();
                }
                break;
            }
            
        case WODM_RESTART:
        case WIDM_START:
            {
                //always try to start stream in case I was in suspended state for whatever reason
                DEBUGMSG(ZONE_WODM || ZONE_WIDM, (TEXT("WIDM_START/WODM_RESTART\r\n")));
                //RETAILMSG(1, (TEXT("WIDM_START/WODM_RESTART\r\n")));

                if(g_pHWContext->HasA2dpBeenOpened())
                    {
                    if(!g_pHWContext->IsReadyToWrite())
                        {
                        dwRet = g_pHWContext->StartA2dpStream();
                        }
                    else
                        {
                        dwRet = ERROR_SUCCESS;
                        }
                    
                    if (dwRet != ERROR_SUCCESS)
                        {
                        g_pHWContext->AbortA2dpConnection();
                        dwRet = MMSYSERR_ERROR;
                        }
                    else
                        {
                        g_pHWContext->UnsetSuspendRequest();
                        dwRet = pStreamContext->Run();
                        }
                    }
                else
                    {
                    dwRet = MMSYSERR_ERROR;    
                    }
                
                break;
            }

        case WODM_PAUSE:
        case WIDM_STOP:
            {
                DEBUGMSG(ZONE_WODM || ZONE_WIDM, (TEXT("WIDM_STOP/WODM_PAUSE\r\n")));
                //RETAILMSG(1, (TEXT("WIDM_STOP/WODM_PAUSE\r\n")));
                dwRet = pStreamContext->Stop();
                DeviceContext *pDeviceContext;
                pDeviceContext = g_pHWContext->GetOutputDeviceContext(DeviceId);
                pDeviceContext->FlushOutputAndSetSuspend();
                break;
            }

        case WODM_GETPOS:
        case WIDM_GETPOS:
            {
                dwRet = pStreamContext->GetPos((PMMTIME)Param1);
                break;
            }

        case WODM_RESET:
        case WIDM_RESET:
            {
                dwRet = pStreamContext->Reset();
                break;
            }
        case WODM_WRITE:
        case WIDM_ADDBUFFER:
            {                
                DEBUGMSG((ZONE_WODM || ZONE_WIDM)&&ZONE_VERBOSE, (TEXT("WODM_WRITE/WIDM_ADDBUFFER, queue buffer=0x%08x size=%d\r\n"),Param1,((LPWAVEHDR)Param1)->dwBufferLength));
                dwRet = pStreamContext->QueueBuffer((LPWAVEHDR)Param1);
                break;
            }

        case WODM_GETVOLUME:
            {
                PDWORD pdwGain = (PDWORD)Param1;

                if (pStreamContext)
                {
                    *pdwGain = pStreamContext->GetGain();
                }
                else
                {
#ifdef USE_HW_GAIN_WODM_SETGETVOLUME
                    // Handle device gain in hardware
                    *pdwGain = g_pHWContext->GetOutputGain();
#else
                    // Handle device gain in software
                    DeviceContext *pDeviceContext = g_pHWContext->GetOutputDeviceContext(DeviceId);
                    *pdwGain = pDeviceContext->GetGain();
#endif
                }
                dwRet = MMSYSERR_NOERROR;
                break;
            }

        case WODM_SETVOLUME:
            {
                DWORD dwGain = Param1;
                if (pStreamContext)
                {
                    dwRet = pStreamContext->SetGain(dwGain);
                }
                else
                {
#ifdef USE_HW_GAIN_WODM_SETGETVOLUME
                    // Handle device gain in hardware
                    dwRet = g_pHWContext->SetOutputGain(dwGain);
#else
                    // Handle device gain in software
                    DeviceContext *pDeviceContext = g_pHWContext->GetOutputDeviceContext(DeviceId);
                    dwRet = pDeviceContext->SetGain(dwGain);
#endif
                }
                break;
            }

        case WODM_BREAKLOOP:
            {
                dwRet = pStreamContext->BreakLoop();
                break;
            }

        case WODM_SETPLAYBACKRATE:
            {
                WaveStreamContext *pWaveStream = (WaveStreamContext *)User;
                dwRet = pWaveStream->SetRate(Param1);
                break;
            }

        case WODM_GETPLAYBACKRATE:
            {
                WaveStreamContext *pWaveStream = (WaveStreamContext *)User;
                dwRet = pWaveStream->GetRate((DWORD *)Param1);
                break;
            }

        case MM_WOM_SETSECONDARYGAINCLASS:
            {
                dwRet = pStreamContext->SetSecondaryGainClass(Param1);
                break;
            }

        case MM_WOM_SETSECONDARYGAINLIMIT:
            {
                DeviceContext *pDeviceContext;
                if (pStreamContext)
                {
                    pDeviceContext = pStreamContext->GetDeviceContext();
                }
                else
                {
                    pDeviceContext = g_pHWContext->GetOutputDeviceContext(DeviceId);
                }
                dwRet = pDeviceContext->SetSecondaryGainLimit(Param1,Param2);
                break;
            }

        case MM_WOM_FORCESPEAKER:
            {
                if (pStreamContext)
                {
                    dwRet = pStreamContext->ForceSpeaker((BOOL)Param1);
                }
                else
                {
                    dwRet = g_pHWContext->ForceSpeaker((BOOL)Param1);
                }
                break;
            }

        case MM_MOM_MIDIMESSAGE:
            {
                CMidiStream *pMidiStream = (CMidiStream *)User;
                dwRet = pMidiStream->MidiMessage(Param1);
                break;
            }
        case WODM_OPEN_CLOSE_A2DP:
            {
            DEBUGMSG(ZONE_WODM, (TEXT("WODM_OPEN_CLOSE_A2DP\r\n")));

            dwRet = MMSYSERR_NOERROR;
            g_pHWContext->UnsetSuspendRequest();
            if(Param1 == WODM_PARAM_OPENASYNC_A2DP)
                {
                DEBUGMSG(ZONE_WODM, (TEXT("WODM_OPEN_CLOSE_A2DP: open async\r\n")));
                if(!g_pHWContext->GetAsyncOpenThreadHandle())
                    {
                    HANDLE Thread = CreateThread((LPSECURITY_ATTRIBUTES)NULL,
                                                    0,
                                                    (LPTHREAD_START_ROUTINE)HardwareContext::OpenA2dpConnectionAndMakePreffered,
                                                    g_pHWContext,
                                                    0,
                                                    NULL);
                    if(NULL != Thread)
                        {
                        g_pHWContext->SetAsyncOpenThreadHandle(Thread);
                        }
                    else
                        {
                        dwRet  = MMSYSERR_ERROR;
                        }
                    }
                else
                    {
                    dwRet = MMSYSERR_NOERROR;
                    }
                }
            else if(Param1 == WODM_PARAM_OPEN_A2DP)
                {
                DEBUGMSG(ZONE_WODM, (TEXT("WODM_OPEN_CLOSE_A2DP: open\r\n")));
                DWORD Err = g_pHWContext->OpenA2dpConnectionAndMakePreffered_Inst();
                if(ERROR_SUCCESS !=  Err)
                    {
                    dwRet = MMSYSERR_ERROR;
                    }
                }
            else if(Param1 == WODM_PARAM_CLOSE_A2DP)
                {
                DEBUGMSG(ZONE_WODM, (TEXT("WODM_OPEN_CLOSE_A2DP: close\r\n")));

                DeviceContext *pDeviceContext;
                pDeviceContext = g_pHWContext->GetOutputDeviceContext(DeviceId);
                pDeviceContext->ResetDeviceContext();
                g_pHWContext->CloseA2dpConnection();
                g_pHWContext->MakeNonPreferredDriver();
                    
                }
            else
                {
                dwRet  = MMSYSERR_NOTSUPPORTED;
                }
            break;
            }

        case WODM_BT_A2DP_SUSPEND:
            {
            DEBUGMSG(ZONE_WODM, (TEXT("WODM_BT_A2DP_SUSPEND\r\n")));

            dwRet = MMSYSERR_ERROR;

            if(g_pHWContext->HasA2dpBeenOpened() &&
                g_pHWContext->IsReadyToWrite())
                {
                dwRet = g_pHWContext->SuspendA2dpStreamImmediately();
                if (MMSYSERR_NOERROR == dwRet)
                    {
                        g_pHWContext->Opened();
                    }                
                }
                
            break;
            }

        case WODM_BT_A2DP_START:
            {
            DEBUGMSG(ZONE_WODM, (TEXT("WODM_BT_A2DP_START\r\n")));

            dwRet = MMSYSERR_ERROR;

            if(g_pHWContext->HasA2dpBeenOpened() &&
                !g_pHWContext->IsReadyToWrite())
                {
                dwRet = g_pHWContext->StartA2dpStream();
                if (MMSYSERR_NOERROR == dwRet)
                    {
                        g_pHWContext->Streaming();
                    }
                }
            
            break;
            }

        case WODM_GETPROP:
            {
            DEBUGMSG(ZONE_WODM, (TEXT("WODM_GETPROP\r\n")));

            dwRet = MMSYSERR_ERROR;
            PWAVEPROPINFO PropInfo = (PWAVEPROPINFO) Param1;
            if(NULL != PropInfo && IsEqualGUID(*PropInfo->pPropSetId, GUID_STATE))
                {
                DWORD State = 0;

                
                if(g_pHWContext->HasA2dpBeenOpened())
                    {
                    if(g_pHWContext->IsReadyToWrite())
                        {
                        State = BT_AVDTP_STATE_STREAMING;
                        }
                    else
                        {
                        State = BT_AVDTP_STATE_SUSPENDED;
                        }
                    }
                else
                    {
                    State = BT_AVDTP_STATE_DISCONNECTED;
                    }
                
                if(NULL != PropInfo->pvPropData && PropInfo->cbPropData >= sizeof(State))
                    {
                    ((DWORD*)PropInfo->pvPropData)[0] = State;
                    dwRet = MMSYSERR_NOERROR;
                    if(NULL != PropInfo->pcbReturn)
                        {
                        *(PropInfo->pcbReturn) = sizeof(State);
                        }
                
                    }
                }
            break;
            }

    // unsupported messages
        case WODM_GETPITCH:
        case WODM_SETPITCH:
        case WODM_PREPARE:
        case WODM_UNPREPARE:
        case WIDM_PREPARE:
        case WIDM_UNPREPARE:
            default:
            dwRet  = MMSYSERR_NOTSUPPORTED;
        }
        g_pHWContext->Unlock();

        LeaveCriticalSection(&g_csWaveMainCS);
    }
    // Pass the return code back via pBufOut
    if (pdwResult)
    {
        *pdwResult = dwRet;
    }

    return(TRUE);
}

// -----------------------------------------------------------------------------
//
//  @doc    WDEV_EXT
//
//  @func   BOOL | WAV_IOControl | Device IO control routine
//
//  @parm   DWORD | dwOpenData | Value returned from WAV_Open call
//
//  @parm   DWORD | dwCode |
//          IO control code for the function to be performed. WAV_IOControl only
//          supports one IOCTL value (IOCTL_WAV_MESSAGE)
//
//  @parm   PBYTE | pBufIn |
//          Pointer to the input parameter structure (<t MMDRV_MESSAGE_PARAMS>).
//
//  @parm   DWORD | dwLenIn |
//          Size in bytes of input parameter structure (sizeof(<t MMDRV_MESSAGE_PARAMS>)).
//
//  @parm   PBYTE | pBufOut | Pointer to the return value (DWORD).
//
//  @parm   DWORD | dwLenOut | Size of the return value variable (sizeof(DWORD)).
//
//  @parm   PDWORD | pdwActualOut | Unused
//
//  @rdesc  Returns TRUE for success, FALSE for failure
//
//  @xref   <t Wave Input Driver Messages> (WIDM_XXX) <nl>
//          <t Wave Output Driver Messages> (WODM_XXX)
//
// -----------------------------------------------------------------------------
extern "C" BOOL WAV_IOControl(PDWORD  pdwOpenData,
                   DWORD  dwCode,
                   PBYTE  pBufIn,
                   DWORD  dwLenIn,
                   PBYTE  pBufOut,
                   DWORD  dwLenOut,
                   PDWORD pdwActualOut)
{

    //  set the error code to be no error first
    SetLastError(MMSYSERR_NOERROR);
    
    _try
    {
        switch (dwCode)
        {
        //case IOCTL_MIX_MESSAGE:
           // return HandleMixerMessage((PMMDRV_MESSAGE_PARAMS)pBufIn, (DWORD *)pBufOut);

        case IOCTL_WAV_MESSAGE:
            return HandleWaveMessage((PMMDRV_MESSAGE_PARAMS)pBufIn, (DWORD *)pBufOut);

        default:
            BSS_ERRMSG(TEXT("Unsupported dwCode in WAV_IOControl()"));
            return(FALSE);
        }

    }
    _except (EXCEPTION_EXECUTE_HANDLER)
    {
        //RETAILMSG(1, (TEXT("EXCEPTION IN WAV_IOControl!!!!\r\n")));
        SetLastError(E_FAIL);
    }

    return(FALSE);
}

