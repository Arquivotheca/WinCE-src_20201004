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
/*
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
  
Module Name:    HWCTXT.CPP

Abstract:               Platform dependent code for the mixing audio driver.

Notes:                  The following file contains all the hardware specific code
for the mixing audio driver.  This code's primary responsibilities
are:

* Initialize audio hardware (including codec chip)
* Schedule DMA operations (move data from/to buffers)
* Handle audio interrupts

All other tasks (mixing, volume control, etc.) are handled by the "upper"
layers of this driver.
*/
#include "hwctxt.h"
#include "wavemain.h"


#include <ceddk.h>
#include <svsutil.hxx>
#include <bt_hcip.h>
#include <bt_debug.h>
#include <bt_tdbg.h>
#include <bt_api.h>
#include <bt_buffer.h>

#include <bt_ddi.h>
#include <windows.h>
#include "btha2dp.h"

HardwareContext *g_pHWContext           = NULL;
DWORD SAMPLE_RATE = SAMPLE_RATE_DEFAULT;
BYTE BIT_POOL = BIT_POOL_DEFAULT; 
UINT32 INVSAMPLERATE = ((UINT32)(((1i64<<32)/SAMPLE_RATE)+1));
DWORD SUSPEND_DELAY_MS = SUSPEND_DELAY_MS_DEFAULT;

DWORD 
HardwareContext::
GetAudioDmaPageSize() 
{ 
    //Lock that protects the data structures involving the output
    //buffers
    LockOutputBuffers();
    DWORD AudioDmaPageSize = m_AudioDmaPageSize;
    UnlockOutputBuffers();

    return AudioDmaPageSize;
}

void 
HardwareContext::
SetAudioDmaPageSize(DWORD NewAudioDmaPageSize) 
{
    //Lock that protects the data structures involving the output
    //buffers
    LockOutputBuffers();
    ASSERT(NewAudioDmaPageSize <= AUDIO_DMA_PAGE_SIZE_DEFAULT);
    m_AudioDmaPageSize = NewAudioDmaPageSize;
    UnlockOutputBuffers();
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       A2dpInit()

Description:    Initializes the A2dp variables and necessary state

Returns:        Boolean indicating status
-------------------------------------------------------------------*/
BOOL HardwareContext::A2dpInit()
{


    // 1 - Do the necessary first time only initialization
    int Res = a2dp_InitializeOnce();
    if (Res) {
        BSS_ERRMSG1("a2dp_InitializeOnce FAILED %d", Res);
        return FALSE;
    }
    //
    // create A2DP_t layer instance
    //
    Res = a2dp_CreateDriverInstance();
    if (Res) {
        BSS_ERRMSG1("a2dp_CreateDriverInstance FAILED %d", Res);
        return FALSE;
    }

    Res = a2dp_Bind(NULL, &g_pHWContext->m_A2dpInterface);
    if (Res) {
        BSS_ERRMSG1("a2dp_Bind FAILED %d", Res);
        return FALSE;
    }

    m_AudioDmaPageSize = AUDIO_DMA_PAGE_SIZE_DEFAULT;
    
    return TRUE;
}



BOOL HardwareContext::CreateHWContext(DWORD Index)
{
    if (g_pHWContext)
    {
        return TRUE;
    }

    g_pHWContext = new HardwareContext;
    if (!g_pHWContext)
    {
        return FALSE;
    }

    return g_pHWContext->Init(Index);
}

HardwareContext::HardwareContext()
: m_OutputDeviceContext()
{
    InitializeCriticalSection(&m_Lock);
    InitializeCriticalSection(&m_LockOutputBuffers);
    m_State = HARDWARE_CONTEXT_NOT_INITIALIZED;
}

HardwareContext::~HardwareContext()
{
    DeleteCriticalSection(&m_Lock);
    DeleteCriticalSection(&m_LockOutputBuffers);
}


BOOL HardwareContext::Init(DWORD Index)
{
    if (HasBeenInitialized())
    {
        return FALSE;
    }

    BOOL InitializedSuccessfully = FALSE;
    // Initialize the state/status variables
    m_DriverIndex       = Index;
    m_InPowerHandler    = FALSE;
#ifdef INPUT_ON

    m_InputDMARunning   = FALSE;
    m_InputDMABuffer    = 0;

#endif
    m_OutputDMARunning  = FALSE;
    m_NumForcedSpeaker  = 0;
    m_NextAudioBufferToTransfertoBT = 0;
    m_NextAudioBufferToWrite = 0;
    m_CloseA2dp= FALSE;
    m_AutoConnectEnabled = FALSE;
    m_AsyncOpenThreadHandle = NULL;
    //
    // Map the DMA buffers into driver's virtual address space
    if (!MapDMABuffers())
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("BTA2DP:HardwareContext::Init() - Failed to map DMA buffers.\r\n")));
        goto Exit;
    }

    // Configure the Codec 
    m_dwOutputGain = 0xFFFFFFFF;
    m_dwInputGain  = 0xFFFFFFFF;
    m_fInputMute  = FALSE;
    m_fOutputMute = FALSE;

    m_SuspendRequested = FALSE;

    //Initialize
        
    if (!InitInterruptThread())
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("BTA2DP:HardwareContext::Init() - Failed to initialize interrupt thread.\r\n")));
        goto Exit;
    }

    if (!A2dpInit())
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("BTA2DP:HardwareContext::Init() - Failed to initialize a2dp.\r\n")));
        goto Exit;
    }
        m_pTPSuspend= new SVSThreadPool(1);

    if (! m_pTPSuspend) {
        DEBUGMSG(ZONE_ERROR, (TEXT("BTA2DP:HardwareContext::Init() - Failed to initialize SVSThreadPool.\r\n")));    
        goto Exit;
    }
    m_SuspendCookie = NULL;

    InitializedSuccessfully = TRUE;
    Initialized();
    Exit:
    return InitializedSuccessfully;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       GetCurrentDeviceID

Description:    Gets the current Device Id of the driver

Params:         [out] UINT DeviceID

Returns:        Boolean indicating status
-------------------------------------------------------------------*/
BOOL HardwareContext::GetCurrentDeviceID(UINT &DeviceID)
{
    UINT TotalDevs = waveOutGetNumDevs();
    WAVEOUTCAPS wc;
    UINT Size = sizeof(wc);
    BOOL Ret = FALSE;
    for(UINT i =0; i<TotalDevs; i++)
        {
        MMRESULT Res = waveOutGetDevCaps(i, &wc, Size);
        if(MMSYSERR_NOERROR == Res && 
           (0 ==_tcsncmp(TEXT("Bluetooth Advanced Audio Output"),  wc.szPname, min(MAXPNAMELEN, sizeof(TEXT("Bluetooth Advanced Audio Output"))))))
            {
            DeviceID = i;
            Ret = TRUE;
            break;
            }
        }
    return Ret;
}

DWORD 
HardwareContext::
MakeNonPreferredDriver()
{
    UINT CurDeviceID;  
    DWORD Err = ERROR_INTERNAL_ERROR;
    BOOL ValidDeviceId = GetCurrentDeviceID(CurDeviceID);
    SVSUTIL_ASSERT(ValidDeviceId);
    if(CurDeviceID == 0)
        {
        Err = waveOutMessage((HWAVEOUT)WAVE_MAPPER,
               DRVM_MAPPER_PREFERRED_SET,
               CurDeviceID,
               (DWORD) -1
               ); 
        }
        
    return Err;
}

DWORD
HardwareContext::
MakePreferredDriver()
{
    UINT CurDeviceID;
    DWORD Err = ERROR_INTERNAL_ERROR;
    BOOL ValidDeviceId = g_pHWContext->GetCurrentDeviceID(CurDeviceID);
    SVSUTIL_ASSERT(ValidDeviceId);
    if(CurDeviceID != 0)
        {
        Err = waveOutMessage((HWAVEOUT)WAVE_MAPPER,
                        DRVM_MAPPER_PREFERRED_SET,
                        CurDeviceID,
                        0
                        ); 
        }
    return Err;
}

void
HardwareContext::
ResetDeviceContext()
{
    SVSUTIL_ASSERT(ThisThreadOwnsLock());
    m_OutputDeviceContext.ResetDeviceContext();
}

DWORD HardwareContext::Call(DWORD Type)
{
    SVSUTIL_ASSERT(ThisThreadOwnsLock());
    
    A2DP_USER_CALL * UserCall;
    int Err;
    UserCall = new A2DP_USER_CALL;
    if(UserCall)
        {
        memset(UserCall, 0, sizeof(A2DP_USER_CALL));
        UserCall->dwUserCallType = Type;
        
        Unlock();
        Err = m_A2dpInterface.Call(UserCall);
        Lock();
        
        delete UserCall;
        }
    else
        {
        Err = ERROR_OUTOFMEMORY;
        }
    return Err;
}

void HardwareContext::SetAsyncOpenThreadHandle(HANDLE Temp)
{
    SVSUTIL_ASSERT(ThisThreadOwnsLock());
    m_AsyncOpenThreadHandle = Temp;
}

HANDLE HardwareContext::GetAsyncOpenThreadHandle()
{
    SVSUTIL_ASSERT(ThisThreadOwnsLock());
    return m_AsyncOpenThreadHandle;
}

DWORD WINAPI HardwareContext::OpenA2dpConnectionAndMakePreffered(LPVOID pv)
{
    SVSUTIL_ASSERT(pv);
    DWORD Err;

    if(pv)
        {
        HardwareContext* pInst = (HardwareContext*)pv;
        pInst->Lock();
        pInst->OpenA2dpConnectionAndMakePreffered_Inst();
        CloseHandle(pInst->GetAsyncOpenThreadHandle());
        pInst->SetAsyncOpenThreadHandle(NULL);
        pInst->Unlock();

        Err = ERROR_SUCCESS;
        }
    else
        {
        Err = ERROR_INTERNAL_ERROR;
        }

    return Err;
    
}

DWORD HardwareContext::OpenA2dpConnectionAndMakePreffered_Inst()
{
    BSS_FUNCTION(TEXT("++HardwareContext::OpenA2dpConnectionAndMakePreffered_Inst()"));
    //SVSUTIL_ASSERT(!ThisThreadOwnsLockOutputBuffers());
    DWORD Err = OpenA2dpConnection();
    if(ERROR_SUCCESS ==  Err)
        {
        Opened();
        //switch in the driver
        MakePreferredDriver();

        }
    else
        {
        BSS_ERRMSG1("HardwareContext::OpenA2dpConnectionAndMakePreffered failed %d", Err);
        }
    return Err;
}
    
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       OpenA2dpConnection()

Description:    This function gets the a2dp connection from idle
                state to open state, negotiating the necessary 
                configurations        

Returns:        DWORD indicating success
-------------------------------------------------------------------*/


DWORD HardwareContext::OpenA2dpConnection()
{
    int Err = ERROR_SUCCESS;
    

    if(!HasA2dpBeenOpened())
    {
        Err = Call(UCT_DISCOVER);
        if (Err) {
            DEBUGMSG(ZONE_ERROR,(TEXT("Call UCT_DISCOVER failed %d"), Err));
            AbortA2dpConnection();
            return Err;
        }
        
        // 2 - Found an acceptable stream end point, 
        //     do a get capabilities to figure out what are the possible
        //     configurations

        Err = Call(UCT_GET_CAPABILITIES);
        if (Err) {
            DEBUGMSG(ZONE_ERROR,(TEXT("Call UCT_GET_CAPABILITIES failed %d"), Err));
            AbortA2dpConnection();
            return Err;
        }

        // 3 -  With the found capabilities
        //      call configure, which will take care of necessary
        //      negotiations
        Err = Call(UCT_CONFIGURE);
        if (Err) {
            DEBUGMSG(ZONE_ERROR,(TEXT("Call UCT_CONFIGURE failed %d"), Err));
            AbortA2dpConnection();
            return Err;
        }

        // 4 - Now that the connection settings are set
        //     Open the stream
        Err = Call(UCT_OPEN);
        if (Err) {
            DEBUGMSG(ZONE_ERROR,(TEXT("Call UCT_OPEN failed %d"), Err));
            AbortA2dpConnection();
            return Err;
        }


        //in case never got the close
        //headset turned off without alerting us
        
        
        StopRenderingThread();
        StopBTTransferThread();
        
        LockOutputBuffers();
        ClearRenderedBuffers();
        UnlockOutputBuffers();
        
        DEBUGMSG(ZONE_MISC,(TEXT("Succeeded opening bt stream")));


    }
    else
    {
        DEBUGMSG(ZONE_MISC,(TEXT("Bt stream already opened")));
    }

    return Err;

}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       CloseA2dpConnection()

Description:    Closes an a2dp connection
                 

Returns:        DWORD indicating success
-------------------------------------------------------------------*/


DWORD HardwareContext::CloseA2dpConnection()
{

    StopBTTransferThread();
    StopRenderingThread();
    LockOutputBuffers();
    ClearRenderedBuffers();
    UnlockOutputBuffers();
    int Err = Call(UCT_CLOSE);

    //if (Err)
        //{
        //Close always fails because of abortedevent so do not write error output message
        //DEBUGMSG(ZONE_ERROR,(TEXT("Call UCT_Close failed %d"), Err));
        //}
    Closed();
    //Close always fails because of abortedevent so do not return error
    return ERROR_SUCCESS;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               AbortA2dpConnection()

Description:            Function resets a2dp state. Should be called 
                        when internal failure happens
Notes:                  

Returns:               DWORD indicating success
-------------------------------------------------------------------*/


DWORD HardwareContext::AbortA2dpConnection()
{
    int Err = Call(UCT_ABORT);
    if (Err) 
        {
        DEBUGMSG(ZONE_ERROR,(TEXT("Call UCT_ABORT failed [0x%8x]"), Err));
        }

    //make sure the threads are stoped within the locks
    //also, to avoid any extra writes that might restart them 
    //before a2dp is set to closed value
    StopRenderingThread();
    StopBTTransferThread();
    
    LockOutputBuffers();
    ClearRenderedBuffers();
    UnlockOutputBuffers();
    
    //Go back to initialized only state
    Closed();


    //switch out the driver
    //TODO: add reg setting here?
    g_pHWContext->MakeNonPreferredDriver();
    return Err;
}

DWORD HardwareContext::ForceSpeaker (BOOL bSpeaker)
{
    return 0;
}

BOOL HardwareContext::IsScoPresent(void)
{   
    BSS_FUNCTION(TEXT("++HardwareContext::IsScoPresent()"));
    
    BOOL fScoPresent = FALSE;

    BASEBAND_CONNECTION connections[5];
    int cConnectionsIn = 5;
    int cConnectionsOut = 0;

    // Get a list of BT baseband connections

    BASEBAND_CONNECTION* pConnections = connections;
    int iErr;
    do 
    {
        if (cConnectionsOut > cConnectionsIn) 
        {
            if (pConnections != connections)
                delete[] pConnections;
            pConnections = new BASEBAND_CONNECTION[cConnectionsOut];
            if (! pConnections)             
                break;
            cConnectionsIn = cConnectionsOut;
        }
        iErr = BthGetBasebandConnections(cConnectionsIn, pConnections, &cConnectionsOut);
    } while (ERROR_INSUFFICIENT_BUFFER == iErr);

    // Check if any SCO connections are present in the list

    if (ERROR_SUCCESS == iErr) 
    {   
        for (int i = 0; i < cConnectionsOut; i++) 
        {
            if (0 == pConnections[i].fLinkType) 
            {
                fScoPresent = TRUE;
                break;
            }
        }
    }

    if (pConnections != connections)
        delete[] pConnections;

    BSS_FUNCTION1(TEXT("++HardwareContext::IsScoPresent() result=%d"), fScoPresent);
    
    return fScoPresent;    
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               StartA2dpStream()

Description:    
Notes:                  

Returns:               DWORD indicating success
-------------------------------------------------------------------*/


DWORD HardwareContext::StartA2dpStream()
{
    SVSUTIL_ASSERT(ThisThreadOwnsLock());
    int Err = MMSYSERR_NOERROR;

    if (! IsScoPresent())
        {
        UnsetSuspendRequest(); 

        Err = Call(UCT_START);
        if (Err)
            {
            RETAILMSG(1, (TEXT("++HardwareContext::StartA2dpStream() failed\r\n")));

            BSS_ERRMSG1("Call UCT_START failed %d", Err);
            AbortA2dpConnection();
            }
        }
    
    return Err;
}

DWORD HardwareContext::SuspendA2dpStreamImmediately(void)
{
    SVSUTIL_ASSERT(ThisThreadOwnsLock());
    DWORD Err = Call(UCT_SUSPEND);
    if (Err)
        {
        RETAILMSG(1, (TEXT("++HardwareContext::SuspendA2dpStreamImmediately() failed\r\n")));
        BSS_ERRMSG1("Call UCT_SUSPEND failed %d", Err);
        }
    UnsetSuspendRequest();
    return Err;
}

DWORD HardwareContext::ReScheduleSuspendA2dpStream()
{
    SVSUTIL_ASSERT(ThisThreadOwnsLock());

    if(m_SuspendCookie)
        {
        int Ret = m_pTPSuspend->UnScheduleEvent(m_SuspendCookie);
        m_SuspendCookie = NULL;
        }
        
    m_SuspendCookie = m_pTPSuspend->ScheduleEvent(DelayedSuspendA2dpStream, (LPVOID)this, SUSPEND_DELAY_MS);
    if(m_SuspendCookie == 0) 
        BSS_ERRMSG(TEXT("HardwareContext::ScheduleSuspendA2dpStream() schedule failed  \r\n"));
        
    return ERROR_SUCCESS;
}
DWORD HardwareContext::UnScheduleSuspendA2dpStream()
{
    SVSUTIL_ASSERT(ThisThreadOwnsLock());

    if(m_SuspendCookie)
        {
        int Ret = m_pTPSuspend->UnScheduleEvent(m_SuspendCookie);
        if(Ret == 0) //FALSE
            {
            BSS_ERRMSG(TEXT("HardwareContext::UnScheduleSuspendA2dpStream() unschedule failed  \r\n"));
            }
        m_SuspendCookie = NULL;
        }
    return ERROR_SUCCESS;

}


    
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       SetSuspendRequest()

Description:    This sets m_SuspendRequested to TRUE syncronously so
                when the suspend thread eventually fires know should
                suspend (received a close or pause)
                and will go ahead with the AVDTP suspend.
Notes:                  

Returns:               DWORD indicating success
-------------------------------------------------------------------*/
DWORD HardwareContext::SetSuspendRequest()
{    
    SVSUTIL_ASSERT(ThisThreadOwnsLock());
    m_SuspendRequested = TRUE;
    return ERROR_SUCCESS;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       UnSuspendA2dpStream()

Description:    This sets m_SuspendRequested to FALSE syncronously 
                so if the suspend thread eventually fires 
                know should not suspend (received an open, restart
                or the bt stream was restarted)
Notes:                  
-------------------------------------------------------------------*/
DWORD HardwareContext::UnsetSuspendRequest()
{    
    SVSUTIL_ASSERT(ThisThreadOwnsLock());
    m_SuspendRequested = FALSE;
    return ERROR_SUCCESS;
}
  
    
    
DWORD WINAPI HardwareContext::DelayedSuspendA2dpStream(LPVOID pv)
{
    SVSUTIL_ASSERT(pv);
    HardwareContext* pInst = (HardwareContext*)pv;
    pInst->DelayedSuspendA2dpStream_Inst();
    return ERROR_SUCCESS;
}

DWORD HardwareContext::DelayedSuspendA2dpStream_Inst()
{
    Lock();
    m_SuspendCookie = NULL;
    int Err = ERROR_SUCCESS;
    if(m_SuspendRequested)
        {
        Err = Call(UCT_SUSPEND);
        if (Err) 
            {
            BSS_ERRMSG1("Call UCT_SUSPEND failed %d", Err);
            }
        }
    UnsetSuspendRequest();
    Unlock();
    return Err;
}

BOOL HardwareContext::IsReadyToWrite()
{
    return a2dp_IsReadyToWrite();
}

BOOL HardwareContext::HasA2dpBeenOpened()
{
    return a2dp_HasBeenOpened();
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               Deinit()

Description:    Deinitializest the hardware: disables DMA channel(s),
                clears any pending interrupts, powers down the audio
                codec chip, etc.

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
BOOL HardwareContext::Deinit()
{
    //----- 3. Turn the audio hardware off -----

    //----- 4. Unmap the control registers and DMA buffers -----
    UnmapRegisters();
    UnmapDMABuffers();
    Reset();

    delete m_pTPSuspend;
    m_pTPSuspend = NULL;


    return TRUE;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               UnmapRegisters()

Description:    Unmaps the config registers used by both the SPI and
                                I2S controllers.

Notes:                  The SPI and I2S controllers both use the GPIO config
                                registers, so these MUST be deinitialized LAST.

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
BOOL HardwareContext::UnmapRegisters()
{
    return TRUE;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               MapDMABuffers()

Description:    Maps the DMA buffers used for audio input/output
                                on the I2S bus.

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
BOOL HardwareContext::MapDMABuffers()
{
    PBYTE pVirtDMABufferAddr = (PBYTE) LocalAlloc(LMEM_FIXED, AUDIO_DMA_PAGE_SIZE_DEFAULT*RAW_OUTPUT_BUFFER_COUNT);
    if(pVirtDMABufferAddr)
    {
        // Setup the DMA page pointers.
        for(uint i=0; i<RAW_OUTPUT_BUFFER_COUNT; i++)
            {
            m_OutputBuffer[i].m_Data= pVirtDMABufferAddr + AUDIO_DMA_PAGE_SIZE_DEFAULT*i;
            }
        return(TRUE);
     }
     return (FALSE);

}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       ClearRenderedBuffers()

Description:    Clears the flat buffers that get tranferred to 
                BT and sets the free buffers event

Note:           assumes holds LockOutputBuffers() with only one
                refcount, otherwise might never be able to get
                a buffer out of writing stage, since the render
                thread needs the lock to do that. The unlocked
                sleep should allow that to happen
                
Returns:        void
-------------------------------------------------------------------*/
BOOL HardwareContext::ClearRenderedBuffers()
{
    BOOL CanClear = TRUE;
    BOOL bLoop = TRUE;
    DWORD TotalTime = 0;
    const DWORD TIMEOUT = 10000;//10sec
    DEBUGMSG(1, (TEXT("++HardwareContext::ClearRenderedBuffers()\r\n")));

    while(bLoop)
    {
        DEBUGMSG(1, (TEXT("++HardwareContext::ClearRenderedBuffers() in bLoop\r\n")));

        CanClear = TRUE;
        for(uint i=0; i<RAW_OUTPUT_BUFFER_COUNT; i++)
        {
            if(m_OutputBuffer[i].IsReadingOrWriting())
            {
                CanClear = FALSE;
                DEBUGMSG(1, (TEXT("++HardwareContext::ClearRenderedBuffers() buffer %x in use\r\n"),i));

                //StopBTTransferThread();
                //StopRenderingThread();
            }
        }
        if(CanClear)
        {
            DEBUGMSG(1, (TEXT("++HardwareContext::ClearRenderedBuffers() clearing\r\n")));
            bLoop = FALSE;
            for(uint i=0; i<RAW_OUTPUT_BUFFER_COUNT; i++)
            {
                m_OutputBuffer[i].m_ValidDataCount= 0;

            }
            m_NextAudioBufferToTransfertoBT = 0;
            m_NextAudioBufferToWrite = 0;
            SetEvent(m_FreeBufferAvailable); //freed up a buffer
        }
        else
        {
            UnlockOutputBuffers();
            Sleep(100);
            TotalTime += 100;
            LockOutputBuffers();
            if(TotalTime > TIMEOUT)
            {
                ASSERT(0);
                DEBUGMSG(1, (TEXT("--HardwareContext::ClearRenderedBuffers() returning FALSE\r\n")));

                return FALSE;
            }
        }
    }
    DEBUGMSG(1, (TEXT("--HardwareContext::ClearRenderedBuffers()\r\n")));

    return TRUE;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               UnmapDMABuffers()

Description:    Unmaps the DMA buffers used for audio input/output
                                on the I2S bus.

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
BOOL HardwareContext::UnmapDMABuffers()
{
    if(m_OutputBuffer[0].m_Data)
    {
        LocalFree(m_OutputBuffer[0].m_Data);
    }
    return TRUE;
}


MMRESULT HardwareContext::SetOutputGain (DWORD dwGain)
{
    m_dwOutputGain = dwGain; // save off so we can return this from GetGain

    return MMSYSERR_NOERROR;
}

MMRESULT HardwareContext::SetOutputMute (BOOL fMute)
{
    m_fOutputMute = fMute;

    return MMSYSERR_NOERROR;
}

BOOL HardwareContext::GetOutputMute (void)
{
    return m_fOutputMute;
}

DWORD HardwareContext::GetOutputGain (void)
{
    return m_dwOutputGain;
}
#ifdef INPUT_ON

BOOL HardwareContext::GetInputMute (void)
{
    return m_fInputMute;
}

MMRESULT HardwareContext::SetInputMute (BOOL fMute)
{
    m_fInputMute = fMute;
    return MMSYSERR_NOERROR;
}

DWORD HardwareContext::GetInputGain (void)
{
    return m_dwInputGain;
}

MMRESULT HardwareContext::SetInputGain (DWORD dwGain)
{
    m_dwInputGain = dwGain;
    if (! m_fInputMute)
    {
        m_InputDeviceContext.SetGain(dwGain);
    }
    return MMSYSERR_NOERROR;
}
#endif

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               InitOutputDMA()

Description:    Initializes the DMA channel for output.

Notes:                  DMA Channel 2 is used for transmitting output sound
                                data from system memory to the I2S controller.

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
BOOL HardwareContext::InitOutputDMA()
{
    return TRUE;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       StartRenderingThread()

Description:    Stops the thread that transfers and renders data 
                from the upper layer buffers to our flat buffers

Returns:        Boolean indicating success
-------------------------------------------------------------------*/
BOOL HardwareContext::StartRenderingThread()
{
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE, (TEXT("+++StartRenderingThread\n")));

    if ( !m_OutputDMARunning )
        {
        m_OutputDMARunning = TRUE;
        SetEvent(m_StreamDataAvailable);
    
        }
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE, (TEXT("---StartRenderingThread\n")));
    
    return TRUE;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       StopRenderingThread()

Description:    Stops the thread that transfers and renders data 
                from the upper layer buffers to our flat buffers

Returns:        void
-------------------------------------------------------------------*/
void HardwareContext::StopRenderingThread()
{
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE, (TEXT("+++StopRenderingThread\n")));
    if ( m_OutputDMARunning ) 
        {
        ResetEvent(m_StreamDataAvailable);
        m_OutputDMARunning = FALSE;
        }
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE, (TEXT("---StopRenderingThread\n")));

}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       StartBTTransferThread()

Description:    Starts the thread that writes to the bluetooth
                stack

Returns:        Boolean indicating success
-------------------------------------------------------------------*/
BOOL HardwareContext::StartBTTransferThread()
{
    SVSUTIL_ASSERT(ThisThreadOwnsLock());
    SVSUTIL_ASSERT(!ThisThreadOwnsLockOutputBuffers());
    
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE, (TEXT("+++StartBTTransferThread\n")));
    UnScheduleSuspendA2dpStream();
    SetEvent(m_BTOutputThreadEvent);
    DEBUGMSG(ZONE_FUNCTION && ZONE_VERBOSE, (TEXT("---StartBTTransferThread\n")));
    return TRUE;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       StopBTTransferThread()

Description:    Stops the thread that writes to the bluetooth
                stack

Note:           assumes holds LockOutputBuffers() with only one
                refcount
                
Returns:        void
-------------------------------------------------------------------*/
void HardwareContext::StopBTTransferThread()
{
    ResetEvent(m_BTOutputThreadEvent);
}

//must always be called from within BT output thread
void HardwareContext::StopBTTransferThreadAndCheckForSuspend(BOOL Flush)
{
    SVSUTIL_ASSERT(!ThisThreadOwnsLock());
    SVSUTIL_ASSERT(!ThisThreadOwnsLockOutputBuffers());
    ResetEvent(m_BTOutputThreadEvent);
    Lock();
    if(m_SuspendRequested && Flush)
        {
        Unlock();
        OutputFlatBuffersToBT(FALSE);
        Lock();
        }
    ReScheduleSuspendA2dpStream();
        
    Unlock();
    
}
#ifdef INPUT_ON

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               InitInputDMA()

Description:    Initializes the DMA channel for input.

Notes:

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
BOOL HardwareContext::InitInputDMA()
{
    DEBUGMSG(ZONE_FUNCTION,(TEXT("+++InitInputDMA\n")));

    return(TRUE);
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               StartInputDMA()

Description:    Starts inputting the recorded sound data from the
                                audio codec chip via DMA.

Notes:

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
BOOL HardwareContext::StartInputDMA()
{
    BOOL bRet=FALSE;

    DEBUGMSG(ZONE_FUNCTION,(TEXT("+++StartInputDMA\n")));

    if (!m_InputDMARunning)
    {
        //----- 1. Initialize our buffer counters -----
        m_InputDMARunning=TRUE;

        //----- 2. Prime the output buffer with sound data -----
        m_InputDMABuffer = 0;

        //----- 3. Configure the DMA channel for record -----
        if (!InitInputDMA())
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("HardwareContext::StartInputDMA() - Unable to initialize input DMA channel!\r\n")));
            goto Exit;
        }

        //----- 5. Start the input DMA -----
        SetEvent(m_hInputRunning);
    }

    DEBUGMSG(ZONE_FUNCTION,(TEXT("---StartInputDMA\n")));
    bRet=TRUE;

Exit:
    return bRet;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               StopInputDMA()

Description:    Stops any DMA activity on the input channel.

Notes:

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
void HardwareContext::StopInputDMA()
{

    if (m_InputDMARunning)
    {
        ResetEvent(m_hInputRunning);
        m_InputDMARunning = FALSE;
    }
}
#endif

DWORD HardwareContext::GetInterruptThreadPriority()
{
    HKEY hDevKey;
    DWORD dwValType;
    DWORD dwValLen;
    DWORD dwPrio = DEFAULT_THREAD_PRIORITY;

    hDevKey = OpenDeviceKey((LPWSTR)m_DriverIndex);
    if (hDevKey)
    {
        dwValLen = sizeof(DWORD);
        RegQueryValueEx(
                       hDevKey,
                       TEXT("PriorityRenderOutput256"),
                       NULL,
                       &dwValType,
                       (PUCHAR)&dwPrio,
                       &dwValLen);
        RegCloseKey(hDevKey);
    }

    return dwPrio;
}



/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               InitInterruptThread()

Description:    Initializes the IST for handling DMA interrupts.

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
BOOL HardwareContext::InitInterruptThread()
{
    m_StreamDataAvailable = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!m_StreamDataAvailable)
    {
        ERRMSG("Unable to create interrupt event");
        return(FALSE);
    }
    m_FreeBufferAvailable = CreateEvent(NULL, TRUE, TRUE, NULL); //start with free buffers
    if (!m_FreeBufferAvailable)
    {
        ERRMSG("Unable to create interrupt event");
        return(FALSE);
    }

    m_BTOutputThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!m_BTOutputThreadEvent)
    {
        ERRMSG("Unable to create interrupt event");
        return(FALSE);
    }

#ifdef INPUT_ON

    m_hInputRunning = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!m_hInputRunning)
    {
        ERRMSG("Unable to create interrupt event");
        DeinitInterruptThread();
        return(FALSE);
    }
#endif
    m_AudioRenderingThreadOutput  = CreateThread((LPSECURITY_ATTRIBUTES)NULL,
                                            0,
                                            (LPTHREAD_START_ROUTINE)CallRenderingThreadOutput,
                                            this,
                                            0,
                                            NULL);
    if (!m_AudioRenderingThreadOutput)
    {
        ERRMSG("Unable to create interrupt thread");
        DeinitInterruptThread();
        return FALSE;
    }
    m_AudioBTWritingThreadOutput  = CreateThread((LPSECURITY_ATTRIBUTES)NULL,
                                            0,
                                            (LPTHREAD_START_ROUTINE)CallBTWritingThreadOutput,
                                            this,
                                            0,
                                            NULL);
    if (!m_AudioBTWritingThreadOutput)
    {
        ERRMSG("Unable to create interrupt thread");
        DeinitInterruptThread();
        return FALSE;
    }
#ifdef INPUT_ON

    m_hAudioInterruptThreadInput  = CreateThread((LPSECURITY_ATTRIBUTES)NULL,
                                        0,
                                        (LPTHREAD_START_ROUTINE)CallInterruptThreadInput,
                                        this,
                                        0,
                                        NULL);
    if (!m_hAudioInterruptThreadInput)
    {
        ERRMSG("Unable to create interrupt thread");
        DeinitInterruptThread();
        return FALSE;
    }
    CeSetThreadPriority(m_hAudioInterruptThreadInput, GetInterruptThreadPriority());    

#endif
    // Bump up the priority since the interrupts must be serviced immediately.
    CeSetThreadPriority(m_AudioRenderingThreadOutput, GetInterruptThreadPriority());
    CeSetThreadPriority(m_AudioBTWritingThreadOutput, GetInterruptThreadPriority());

    return(TRUE);
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               DeinitInterruptThread()

Description:    Initializes the IST for handling DMA interrupts.

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
void HardwareContext::DeinitInterruptThread()
{
    if(m_StreamDataAvailable)
        {
            CloseHandle(m_StreamDataAvailable);
        }
    if(m_FreeBufferAvailable)
        {
            CloseHandle(m_FreeBufferAvailable);
        }
       
    if( m_BTOutputThreadEvent)
        {
            CloseHandle( m_BTOutputThreadEvent);
        }
#ifdef INPUT_ON

    if(m_hInputRunning)
        {
            CloseHandle(m_hInputRunning);
        }
    if(m_hAudioInterruptThreadInput)
        {
            CloseHandle(m_hAudioInterruptThreadInput);
        }
#endif
    if(m_AudioRenderingThreadOutput)
        {
            CloseHandle(m_AudioRenderingThreadOutput);
        }


    return;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               PowerUp()

Description:            Powers up the audio codec chip.

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
void HardwareContext::PowerUp()
{
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               PowerDown()

Description:    Powers down the audio codec chip.

Notes:                  Even if the input/output channels are muted, this
                                function powers down the audio codec chip in order
                                to conserve battery power.

Returns:                Boolean indicating success
-------------------------------------------------------------------*/
void HardwareContext::PowerDown()
{
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       ConvertStreamsToFlatBuffer()

Description:    Copies and mixes app data buffers into internal 
                a2dp buffers

Returns:        Number of valid data bytes the last written to
                buffer now holds

Note:           Fill at most one internal buffer at a time,
                so don't stay locked for very long
                Which also allows better behavior towards
                upper apps that need the lock to access anything in 
                the driver
-------------------------------------------------------------------*/


 ULONG HardwareContext::ConvertStreamsToFlatBuffer()
{
    SVSUTIL_ASSERT(ThisThreadOwnsLock());
    SVSUTIL_ASSERT(!ThisThreadOwnsLockOutputBuffers());
    
    ULONG BytesTransferred = 0;
    ULONG BytesInWrittenBuffer = 0;
    //Lock shared variables between the two output threads
    LockOutputBuffers();
    const DWORD AudioDmaPageSize = GetAudioDmaPageSize();
    if((m_OutputBuffer[m_NextAudioBufferToWrite].IsReadingOrWriting()) ||
        (m_OutputBuffer[m_NextAudioBufferToWrite].IsFull(AudioDmaPageSize)))
    {
        //all buffers are filled, need to wait for them to free up
        ResetEvent(m_FreeBufferAvailable);
        UnlockOutputBuffers();
        return 0;
    }

    //set to writing so other threads will not touch this buffer
    m_OutputBuffer[m_NextAudioBufferToWrite].Writing();
    UnlockOutputBuffers();
    
    PBYTE pBufferStart = m_OutputBuffer[m_NextAudioBufferToWrite].m_Data + 
                         m_OutputBuffer[m_NextAudioBufferToWrite].m_ValidDataCount;
                         
    PBYTE pBufferEnd = m_OutputBuffer[m_NextAudioBufferToWrite].m_Data + AudioDmaPageSize;
    PBYTE pBufferLast = m_OutputDeviceContext.TransferBuffer(pBufferStart, pBufferEnd,NULL);
    
    BytesTransferred = pBufferLast-pBufferStart;

    //Done rendering, now lock again to update
    //shared variables states
    LockOutputBuffers();
    
    m_OutputBuffer[m_NextAudioBufferToWrite].m_ValidDataCount+= BytesTransferred;
    BytesInWrittenBuffer = m_OutputBuffer[m_NextAudioBufferToWrite].m_ValidDataCount;
    m_OutputBuffer[m_NextAudioBufferToWrite].DoneWriting();

    //next time run this function, write to this 
    //buffer or the next one in the ring?
    if(m_OutputBuffer[m_NextAudioBufferToWrite].IsFull(AudioDmaPageSize))
    {
        m_NextAudioBufferToWrite = (m_NextAudioBufferToWrite+1)% RAW_OUTPUT_BUFFER_COUNT;
    
        if((m_OutputBuffer[m_NextAudioBufferToWrite].IsReadingOrWriting()) ||
           (!m_OutputBuffer[m_NextAudioBufferToWrite].IsEmpty()))
        {
            //if next buffer is already filled with data
            //or BT not done transfering it yet, 
            //reset free buffer event
            ResetEvent(m_FreeBufferAvailable);
        }
    }

    //Try stopping the rendering thread as soon as possible
    //This will fail in the case where the last byte of audio data matches perfectly
    //the end of the buffer
    //This function might run again if that is the case
    //but that should have no negative side effects other than efficiency 
    if(BytesInWrittenBuffer < AudioDmaPageSize) 
    {
        StopRenderingThread();
    }
    
    UnlockOutputBuffers();
    
    return BytesInWrittenBuffer;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       OutputFlatBuffersToBT()

Description:    Transfers internal buffers to BT using a2dp layer

Returns:        Number of bytes that were transfered

Note: This function is usually only called within the BT tranfer
      thread - except for when flushing data from a2dp when an 
      app executes a close. 
      It only acquires the LockOutputBuffers lock under normal
      conditions. In case of a write failure, the Lock() lock
      is acquired, which could cause a deadlock if a close
      was requested by the user (and thus holds Lock()) and is 
      waiting for the BT thread to stop before finally closing
      and releasing Lock(). So *always* unset the BT thread
      event before acquiring Lock(). Also, unset from reading
      state so ClearOutputBuffers in flushA2dpData can return 
      successfully.
-------------------------------------------------------------------*/
ULONG HardwareContext::OutputFlatBuffersToBT(BOOL Flush)
{
    SVSUTIL_ASSERT(!ThisThreadOwnsLock());
    SVSUTIL_ASSERT(!ThisThreadOwnsLockOutputBuffers());
    ULONG BytesTransferred = 0;
    
    //Lock shared variables between the two output threads
    LockOutputBuffers();
    const DWORD AudioDmaPageSize = GetAudioDmaPageSize();

    if((m_OutputBuffer[m_NextAudioBufferToTransfertoBT].IsEmpty()) ||
       (m_OutputBuffer[m_NextAudioBufferToTransfertoBT].IsReadingOrWriting()))
    {
        //no data to transfer yet
        UnlockOutputBuffers();
        StopBTTransferThreadAndCheckForSuspend(Flush);
        return 0;
    }
    
    if(!m_OutputBuffer[m_NextAudioBufferToTransfertoBT].IsFull(AudioDmaPageSize))
    {
        // The buffer is not full.  This means the stream is closed and we
        // are flushing the remaining data from the buffer.  We need to pad
        // the buffer and send the full size since some headphones don't
        // like the partial buffer.
        m_OutputBuffer[m_NextAudioBufferToTransfertoBT].PadWithZeros(AudioDmaPageSize);
        m_OutputBuffer[m_NextAudioBufferToTransfertoBT].m_ValidDataCount = AudioDmaPageSize;
        
        ASSERT(m_NextAudioBufferToTransfertoBT == m_NextAudioBufferToWrite);
        m_NextAudioBufferToWrite = (m_NextAudioBufferToWrite+1)% RAW_OUTPUT_BUFFER_COUNT;
    }
        
    m_OutputBuffer[m_NextAudioBufferToTransfertoBT].Reading();
    UnlockOutputBuffers();
    
    PBYTE pBufferStart = m_OutputBuffer[m_NextAudioBufferToTransfertoBT].m_Data;
    BytesTransferred = m_OutputBuffer[m_NextAudioBufferToTransfertoBT].m_ValidDataCount;

    BSS_VERBOSE1(TEXT("[A2DP] BTOutputThread - Write buffer of size %d to AVDTP\n"), BytesTransferred);
    
    //call into a2dp to write
    A2DP_USER_CALL  UserCallWrite;
    memset(&UserCallWrite, 0, sizeof(UserCallWrite));
    UserCallWrite.dwUserCallType = UCT_WRITE;
    UserCallWrite.uParameters.Write.pDataBuffer = pBufferStart;
    UserCallWrite.uParameters.Write.DataBufferByteCount = BytesTransferred;
    int Err = m_A2dpInterface.Call(&UserCallWrite);
    if (Err != ERROR_SUCCESS) 
        {
        BytesTransferred = 0;
        }

    LockOutputBuffers();
    m_OutputBuffer[m_NextAudioBufferToTransfertoBT].m_ValidDataCount = 0;
    m_OutputBuffer[m_NextAudioBufferToTransfertoBT].DoneReading();
    m_NextAudioBufferToTransfertoBT = (m_NextAudioBufferToTransfertoBT+1)% RAW_OUTPUT_BUFFER_COUNT;
    SetEvent(m_FreeBufferAvailable); //freed up a buffer

    if (Err && (Err != ERROR_NOT_READY)) 
        {
        BSS_ERRMSG1("Call UCT_WRITE failed %d", Err);

        ASSERT(BytesTransferred == 0);

        UnlockOutputBuffers();

        Lock();
        m_OutputDeviceContext.ResetDeviceContext();
        AbortA2dpConnection();
        Unlock();

        //stop both threads
        StopRenderingThread();
        StopBTTransferThread();        
        }
    else if (Err) 
        {
        ASSERT(Err == ERROR_NOT_READY);

        BSS_ERRMSG1("Call UCT_WRITE failed with ERROR_NOT_READY", Err);    
        UnlockOutputBuffers();        
        }
    else if((m_OutputBuffer[m_NextAudioBufferToTransfertoBT].m_ValidDataCount < AudioDmaPageSize) ||
       (m_OutputBuffer[m_NextAudioBufferToTransfertoBT].IsReadingOrWriting()) ||
       (0 == BytesTransferred)
       )
        {
        //next buffer is empty
        UnlockOutputBuffers();
        StopBTTransferThreadAndCheckForSuspend(Flush);        
        }
    else 
        {
        UnlockOutputBuffers();    
        }

    return BytesTransferred;
}

#ifdef INPUT_ON
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:               TransferInputBuffer()

Description:    Retrieves the chunk of recorded sound data and inputs
                                it into an audio buffer for potential "mixing".

Returns:                Number of bytes needing to be transferred.
-------------------------------------------------------------------*/
ULONG HardwareContext::TransferInputBuffer(ULONG NumBuf)
{
    ULONG BytesTransferred = 0;

    PBYTE pBufferStart = m_DMAInPageVirtAddr[NumBuf];
    PBYTE pBufferEnd = pBufferStart + GetAudioDmaPageSize();
    PBYTE pBufferLast;

    pBufferLast = m_InputDeviceContext.TransferBuffer(pBufferStart, pBufferEnd,NULL);
    BytesTransferred = pBufferLast-pBufferStart;

    return BytesTransferred;
}


void HardwareContext::InterruptThreadInput()
{
    // Need to be able to access everyone else's address space during interrupt handler.
    SetProcPermissions((DWORD)-1);

    while (TRUE)
    {
        Sleep(TIMEBETWEENINTERRUPTS);
        WaitForSingleObject(m_hInputRunning, INFINITE);

        Lock();
        ULONG InputTransferred;

        InputTransferred = TransferInputBuffer(m_InputDMABuffer);

        // For input DMA, we can stop the DMA as soon as we have no data to transfer.
        if (InputTransferred==0)
        {
            StopInputDMA();
        }
        else    // setup for the next DMA operation
        {
            m_InputDMABuffer^=1;
        }
        Unlock();
    }
}
void CallInterruptThreadInput(HardwareContext *pHWContext)
{
    pHWContext->InterruptThreadInput();
}
#endif
void HardwareContext::RenderingOutputThread()
{
    // Fast way to access embedded pointers in wave headers in other processes.
    SetProcPermissions((DWORD)-1);
    while (TRUE)
    {
        //wait to have data to write and free buffers to write it to
        WaitForSingleObject(m_FreeBufferAvailable, INFINITE);
        WaitForSingleObject(m_StreamDataAvailable, INFINITE);

        // If we queued up a buffer but are not in a state to write
        // we should stop the rendering thread mark all the buffers
        // as done and go back to waiting.
        BOOL fReady = IsReadyToWrite();
        
        Lock();
        ULONG OutputTransferred;

        if (! fReady)
        {
            // Since there is no flow control in this scenario it will cause 
            // the audio app to send data way to fast.  If anything we want to
            // slow down audio more than usual in this "suspended" scenario; so 
            // let's do just that and sleep for a bit.
            Unlock();
            Sleep(100);
            Lock();
            
            StopRenderingThread();
            m_OutputDeviceContext.ResetDeviceContext();
        }
    
        //need to check this again in case I was waiting on the lock
        //that was closing
        DWORD Res = WaitForSingleObject(m_StreamDataAvailable, 0);
        if(Res != WAIT_TIMEOUT)
        {
            OutputTransferred = ConvertStreamsToFlatBuffer();
            if(OutputTransferred >= GetAudioDmaPageSize())
            {
                StartBTTransferThread();
            }
        }
        Unlock();

    }
}


void HardwareContext::BTOutputThread()
{
    while (TRUE)
    {
        WaitForSingleObject(m_BTOutputThreadEvent, INFINITE);
        BSS_VERBOSE(TEXT("[A2DP] BTOutputThread - woke up\n"));
        ULONG OutputTransferred = OutputFlatBuffersToBT(TRUE);
        BSS_VERBOSE1(TEXT("[A2DP] BTOutputThread - transfered %d bytes\n"), OutputTransferred);
    }
}


void CallRenderingThreadOutput(HardwareContext *pHWContext)
{
    pHWContext->RenderingOutputThread();
}
void CallBTWritingThreadOutput(HardwareContext *pHWContext)
{
    pHWContext->BTOutputThread();
}





