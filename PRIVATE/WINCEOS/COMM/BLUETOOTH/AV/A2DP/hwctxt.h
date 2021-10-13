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
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.


Module Name:    HWCTXT.H

Abstract:               Platform dependent code for the mixing audio driver.

-*/ 
#ifndef __HWCTXT_HPP_INCLUDED__
#define __HWCTXT_HPP_INCLUDED__


#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <mmddk.h>
#include <mmreg.h>
#include <svsutil.hxx>

#include "BthA2dp.h"



#include "wavemain.h"




typedef INT16 HWSAMPLE;
typedef HWSAMPLE *PHWSAMPLE;

// Set USE_MIX_SATURATE to 1 if you want the mixing code to guard against saturation
// This costs a couple of instructions in the inner loop
#define USE_MIX_SATURATE (1)

// The code will use the follwing values as saturation points
#define AUDIO_SAMPLE_MAX    (32767)
#define AUDIO_SAMPLE_MIN    (-32768)

#define DEFAULT_THREAD_PRIORITY 138

// DMA page size as max MTU for AVDTP
#define DEFAULT_NUMBER_OF_FRAMES 9
#define DEFAULT_PCM_FRAME_SIZE 512
#define AUDIO_DMA_PAGE_SIZE_DEFAULT     DEFAULT_PCM_FRAME_SIZE*DEFAULT_NUMBER_OF_FRAMES    //this is the max size
#define TIMEBETWEENINTERRUPTS (400)
#define TIMEBETWEENINTERRUPTSOUT (30)

// The following define the maximum attenuations for the SW volume controls in devctxt.cpp
// e.g. 100 => range is from 0dB to -100dB
#define STREAM_ATTEN_MAX        50
#define DEVICE_ATTEN_MAX        35
#define CLASS_ATTEN_MAX         35


class HardwareContext;
 
class OutputBuffer_t
{
public:
    OutputBuffer_t()
    {
        m_Data = NULL;
        m_ValidDataCount = 0;
        m_ReadingOrWriting = 0;
    }
    inline BOOL IsReadingOrWriting(){return m_ReadingOrWriting;}
    inline BOOL IsFull(DWORD AudioDmaPageSize)
        {
        ASSERT(AudioDmaPageSize <= AUDIO_DMA_PAGE_SIZE_DEFAULT);
        return AudioDmaPageSize == m_ValidDataCount;
        };
    inline BOOL IsEmpty(){return 0 == m_ValidDataCount;}
    inline void PadWithZeros(DWORD AudioDmaPageSize)
        {
        ASSERT(AudioDmaPageSize <= AUDIO_DMA_PAGE_SIZE_DEFAULT);
        memset(m_Data+m_ValidDataCount, 0, AudioDmaPageSize-m_ValidDataCount);
        };
    inline void Reading(){m_ReadingOrWriting=TRUE;}
    inline void Writing(){m_ReadingOrWriting=TRUE;}
    inline void DoneReading(){m_ReadingOrWriting=FALSE;}
    inline void DoneWriting(){m_ReadingOrWriting=FALSE;}
    
    PBYTE m_Data;
    DWORD m_ValidDataCount; //nunber of bytes on untransfered valid data
    BOOL  m_ReadingOrWriting; 
};

class HardwareContext
{
public:
    static const DWORD RAW_OUTPUT_BUFFER_COUNT = 8;
    enum HardwareContextStateEnum_e {
        HARDWARE_CONTEXT_NOT_INITIALIZED,
        HARDWARE_CONTEXT_INITIALIZED,
        HARDWARE_CONTEXT_OPENING, //trying to connect to a2dp
        HARDWARE_CONTEXT_OPENED, //OPENED, not writing data tp a2dp, ready to start writing
        HARDWARE_CONTEXT_STREAMING, //streaming data,
        HARDWARE_CONTEXT_PAUSED, //all streams are paused
    };
    
    static BOOL CreateHWContext(DWORD Index);

    HardwareContext();
    ~HardwareContext();
#ifdef DEBUG
    BOOL ThisThreadOwnsLock()
    {
        DWORD ThreadID = GetCurrentThreadId();
        if((HANDLE)ThreadID == m_Lock.OwnerThread)
            return TRUE;
        return FALSE;
        
    }
        
    BOOL ThisThreadOwnsLockOutputBuffers()
    {
        DWORD ThreadID = GetCurrentThreadId();
        if((HANDLE)ThreadID == m_LockOutputBuffers.OwnerThread)
            return TRUE;
        return FALSE;
        
    }
#endif
    void Lock()   {EnterCriticalSection(&m_Lock);}
    void Unlock() {LeaveCriticalSection(&m_Lock);}
    void LockOutputBuffers()   {EnterCriticalSection(&m_LockOutputBuffers);}
    void UnlockOutputBuffers() {LeaveCriticalSection(&m_LockOutputBuffers);}
    DWORD GetNumInputDevices()  {return 0;} //no input devices in this release
    DWORD GetNumOutputDevices() {return 1;}
    DWORD GetNumMixerDevices()  {return 1;}
    inline void Initialized(void)  { m_State = HARDWARE_CONTEXT_INITIALIZED; }
    inline void Opening(void)      { m_State = HARDWARE_CONTEXT_OPENING; }
    inline void Opened(void)       { m_State = HARDWARE_CONTEXT_OPENED; }
    inline void Streaming(void)    { m_State = HARDWARE_CONTEXT_STREAMING; }
    inline void Paused(void)       { m_State = HARDWARE_CONTEXT_PAUSED; }
    inline void Closed(void)       { m_State = HARDWARE_CONTEXT_INITIALIZED; }
    inline void Reset(void)        { m_State = HARDWARE_CONTEXT_NOT_INITIALIZED; }
    inline BOOL IsOpening(void) const 
    {  
        return (m_State == HARDWARE_CONTEXT_OPENING) ;
    }
    BOOL HasBeenInitialized(void) const 
    {  
        return (m_State != HARDWARE_CONTEXT_NOT_INITIALIZED) ;
    }
    BOOL HasBeenOpened(void) const 
    {  
        return (m_State == HARDWARE_CONTEXT_OPENED ||
                m_State == HARDWARE_CONTEXT_STREAMING ||
                m_State == HARDWARE_CONTEXT_PAUSED
                ) ;
    }
        
#ifdef INPUT_ON
    DeviceContext *GetInputDeviceContext(UINT DeviceId)
    {
        return &m_InputDeviceContext;
    }
#endif

    DeviceContext *GetOutputDeviceContext(UINT DeviceId)
    {
        return &m_OutputDeviceContext;
    }

    inline void EnableAutoConnect() { m_AutoConnectEnabled = TRUE; }
    inline void DisableAutoConnect(){ m_AutoConnectEnabled = FALSE; }

    inline BOOL IsAutoConnectEnabled() {return m_AutoConnectEnabled; }
    BOOL Init(DWORD Index);
    BOOL A2dpInit();

    BOOL Deinit();
    
    BOOL GetCurrentDeviceID(UINT &DeviceID);
    DWORD MakeNonPreferredDriver();
    DWORD MakePreferredDriver();

    DWORD OpenA2dpConnection();
    DWORD StartA2dpStream();
    DWORD SetSuspendRequest();
    DWORD UnsetSuspendRequest();

    static DWORD WINAPI DelayedSuspendA2dpStream(LPVOID pv);
    DWORD DelayedSuspendA2dpStream_Inst();    
    DWORD ReScheduleSuspendA2dpStream();
    DWORD UnScheduleSuspendA2dpStream();
    DWORD SuspendA2dpStreamImmediately();

    static DWORD WINAPI OpenA2dpConnectionAndMakePreffered(LPVOID pv);
    DWORD OpenA2dpConnectionAndMakePreffered_Inst();
    void SetAsyncOpenThreadHandle(HANDLE Temp);
    HANDLE GetAsyncOpenThreadHandle();

    BOOL IsReadyToWrite();
    BOOL HasA2dpBeenOpened();
    BOOL IsScoPresent(void);

    DWORD GetAudioDmaPageSize();
    void SetAudioDmaPageSize(DWORD NewAudioDmaPageSize);

    void PowerUp();
    void PowerDown();

#ifdef INPUT_ON
    BOOL StartInputDMA();
    void StopInputDMA();
    void InterruptThreadInput();

#endif
    BOOL StartRenderingThread();
    BOOL StartBTTransferThread();
    
    void StopRenderingThread();
    void StopBTTransferThread();
    void StopBTTransferThreadAndCheckForSuspend(BOOL Flush);

    void RenderingOutputThread();
    void BTOutputThread();
    BOOL ClearRenderedBuffers();

    void ResetDeviceContext();

    DWORD ForceSpeaker (BOOL bSpeaker);

    DWORD       GetOutputGain (void);
    MMRESULT    SetOutputGain (DWORD dwVolume);
    DWORD       GetInputGain (void);
    MMRESULT    SetInputGain (DWORD dwVolume);

    BOOL        GetOutputMute (void);
    MMRESULT    SetOutputMute (BOOL fMute);
    BOOL        GetInputMute (void);
    MMRESULT    SetInputMute (BOOL fMute);

    BOOL PmControlMessage (
                          DWORD  dwCode,
                          PBYTE  pBufIn,
                          DWORD  dwLenIn,
                          PBYTE  pBufOut,
                          DWORD  dwLenOut,
                          PDWORD pdwActualOut
                          );

    

    BOOL     InitA2DPEvents();
    void     DeinitA2DPEvents();
    DWORD    CloseA2dpConnection();
    DWORD    AbortA2dpConnection();


protected:
    static const DWORD AUDIO_EVENT_EXIT_THREAD         = 0;
    static const DWORD AUDIO_EVENT_RX_DATA             = 1;
    static const DWORD AUDIO_EVENT_TX_DATA             = 2;
    static const DWORD AUDIO_EVENT_STACK_DOWN          = 3;
    static const DWORD AUDIO_EVENT_STACK_UP            = 4;
    static const DWORD AUDIO_EVENT_SCO_LINK_DISCONNECT = 5;
    static const DWORD NUM_AUDIO_EVENTS                = 6;


    
    void Zero();
    DWORD m_dwOutputGain;
    DWORD m_dwInputGain;
    BOOL  m_fInputMute;
    BOOL  m_fOutputMute;


    BOOL InitInterruptThread();
    void DeinitInterruptThread();

    BOOL InitInputDMA();
    BOOL InitOutputDMA();

    BOOL MapRegisters();
    BOOL UnmapRegisters();
    BOOL MapDMABuffers();
    BOOL UnmapDMABuffers();

    ULONG TransferInputBuffer(ULONG NumBuf);
    ULONG OutputFlatBuffersToBT(BOOL Flush);
    ULONG ConvertStreamsToFlatBuffer();

    DWORD GetInterruptThreadPriority();
    DWORD Call(DWORD Type);

    DWORD m_DriverIndex;
    CRITICAL_SECTION m_Lock;
    CRITICAL_SECTION m_LockOutputBuffers;

    HardwareContextStateEnum_e m_State;
    //HardwareContextState_t m_InputState;

    USHORT m_A2dpConnHandle;
    
    A2DP_INTERFACE m_A2dpInterface;
    BOOL  m_AutoConnectEnabled;


    
    BOOL m_InPowerHandler;
    DWORD m_dwSysintrOutput;
    DWORD m_dwSysintrInput;

#ifdef INPUT_ON
    InputDeviceContext m_InputDeviceContext;
    PBYTE m_InputDMABuffer;
    BOOL m_InputDMARunning;
    HANDLE m_hInputRunning;  
    HANDLE m_hAudioInterruptThreadInput;                     // Handle to thread which waits on an audio interrupt event.

#endif
    OutputDeviceContext m_OutputDeviceContext;

    PBYTE               m_DMAInPageVirtAddr[2];
    OutputBuffer_t      m_OutputBuffer[RAW_OUTPUT_BUFFER_COUNT]; 
    DWORD               m_NextAudioBufferToWrite;
    DWORD               m_NextAudioBufferToTransfertoBT;


    BOOL m_OutputDMARunning;

    LONG m_NumForcedSpeaker;
    void SetSpeakerEnable(BOOL bEnable);
    void RecalcSpeakerEnable();


    HANDLE m_StreamDataAvailable;
    HANDLE m_FreeBufferAvailable;                       // data for rendering available
    HANDLE m_BTOutputThreadEvent;                       //data for BT transfer available
    
    HANDLE m_AudioRenderingThreadOutput;                    
    HANDLE m_AudioBTWritingThreadOutput;                    
    

    BOOL m_ConnectOut;
    BOOL m_CloseA2dp;

    SVSThreadPool* m_pTPSuspend;
    SVSCookie m_SuspendCookie;
    BOOL m_SuspendRequested;

    
    HANDLE m_AsyncOpenThreadHandle;

    DWORD m_AudioDmaPageSize; 


    //------------------------------------------------------------------------------------

    CEDEVICE_POWER_STATE m_DxState;
};
#ifdef INPUT_ON

void CallInterruptThreadInput(HardwareContext *pHWContext);
#endif
void CallRenderingThreadOutput(HardwareContext *pHWContext);
void CallBTWritingThreadOutput(HardwareContext *pHWContext);

extern HardwareContext *g_pHWContext;
#endif
