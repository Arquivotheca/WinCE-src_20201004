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
#include "BthA2dpCodec.h"

#include <mmreg.h>
#include <msacm.h>
#include <msacmdrv.h>



#include <wavedbg.h>
#include "FunctionTrace.h"
#include <bt_debug.h>
#include <bt_tdbg.h>
#include <bt_buffer.h>

#include <windows.h>
#include <list.hxx>
#include <pkfuncs.h>
#include <svsutil.hxx>
#include <cxport.h>
#include <intsafe.h>

#include "BthA2dp.h"
#include "Btha2dp_debug.h"
#include "allocator.hxx"


const TCHAR* const BthA2dpCodec_t::DEFAULT_SUPPORTED_CODEC_DLL_NAMES[] = { TEXT("sbc.dll") };
const TCHAR * const BthA2dpCodec_t::CODEC_DLL_REGISTRY_KEY_NAME = TEXT("SOFTWARE\\Microsoft\\Bluetooth\\A2DP\\Settings\\CodecDllNames");




static const DWORD AVDTP_MTU = 650;


BthA2dpCodec_t::
BthA2dpCodec_t()
{
    m_pOutputWaveFormat = NULL;
    ResetInfo();
    ReadCodecDllNamesFromRegistry();
  
}

BthA2dpCodec_t
::~BthA2dpCodec_t()
{
    ResetInfo();
}


//protected functions

void
BthA2dpCodec_t::
ReadCodecDllNamesFromRegistry()
{

    HKEY hKey;

    //currently only SBC supported, need to 
    //change this code to support more than one 
    //codec

    //set default value
    StringCchCopy(m_pSupportedCodecs[SBC], 
                  MAX_CODEC_DLL_NAME_LENGTH,
                  DEFAULT_SUPPORTED_CODEC_DLL_NAMES[SBC]);

    //now try to read the registry
    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, CODEC_DLL_REGISTRY_KEY_NAME, 
                                       0, KEY_READ, &hKey)) 
        {
        if(GetRegSZValue (hKey, TEXT("sbc"), m_pSupportedCodecs[SBC], 
                           MAX_CODEC_DLL_NAME_LENGTH))
            {
            DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: ReadCodecDllNamesFromRegistry : SBC dll name in registry [%s]\r\n", m_pSupportedCodecs[SBC]));
            }
        RegCloseKey(hKey);
        }

    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: ReadCodecDllNamesFromRegistry : SBC dll name [%s]\r\n", m_pSupportedCodecs[SBC]));


}


//this function goes back to "just constructed state"
void
BthA2dpCodec_t::
ResetInfo()
{
    m_CurrentIndex = NO_CODEC;
    ClearStreamInfo();
    ClearDriverInfo();
    m_State = CODEC_STATE_IDLE;

}


void
BthA2dpCodec_t::
Reset()
{
    CloseStream();
    UnloadCurrent();
    m_CurrentIndex = NO_CODEC;
}



DWORD
BthA2dpCodec_t::
Load()
{
    FUNCTION_TRACE(BthA2dpCodec_t::Load);
    if(!IsCurrentIndexSupported())
        {
        m_CurrentIndex = DEFAULT_CODEC;
        }
    return(LoadCurrent());
}


//Load lib and open library
DWORD 
BthA2dpCodec_t::
LoadCurrent()
{
    FUNCTION_TRACE(BthA2dpCodec_t::LoadCurrent);
    if(IsCurrentLoaded())
        {
            return ERROR_ALREADY_EXISTS;
        }
    if(!IsCurrentIndexSupported())
        {
            return ERROR_BAD_CONFIGURATION;
        }
    DWORD Res = E_FAIL;

    m_Dll = LoadLibrary(m_pSupportedCodecs[m_CurrentIndex]);
    //If library loaded correctly, open the driver
    if(!m_Dll)
        {
        Res = GetLastError();
        }
    else
        {
        m_pDriverProc = (DRIVERPROC)GetProcAddress(m_Dll, L"DriverProc");
        if(m_pDriverProc)
            {
            ACMDRVOPENDESC   AcmOpenDesc;
            ZeroMemory(&AcmOpenDesc, sizeof(ACMDRVOPENDESC));
            AcmOpenDesc.cbStruct = sizeof(ACMDRVOPENDESC);
            AcmOpenDesc.fccType = ACMDRIVERDETAILS_FCCTYPE_AUDIOCODEC;
                
            m_DriverId = m_pDriverProc(0,(HDRVR)m_Dll,DRV_OPEN, 0, (LPARAM)&AcmOpenDesc);
            if(0 != m_DriverId)
                {
                Res = ERROR_SUCCESS;
                }
            }
        }
        
        

    if(ERROR_SUCCESS == Res)
        {
        Loaded();
        }
    else
        {
        UnloadCurrent();
        }

    return Res;
        
}


DWORD 
BthA2dpCodec_t::
CalculateBitRate(
    void
    )
{
    DWORD FrameSize = ((SBCWAVEFORMAT*)m_pOutputWaveFormat)->blockAlignment();
    m_CachedBitRate = ((8*FrameSize*((WAVEFORMATEX*)m_pOutputWaveFormat)->nSamplesPerSec))/
              ((SBCWAVEFORMAT*)m_pOutputWaveFormat)->nSubbands/((SBCWAVEFORMAT*)m_pOutputWaveFormat)->nBlocks;
    
    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: GetBitRate : Calculated [%d]\r\n", m_CachedBitRate));
   
    return ERROR_SUCCESS;
}   

DWORD 
BthA2dpCodec_t::
OpenStream(
    const WAVEFORMATEX * const pWfxInput
    )
{
    FUNCTION_TRACE(BthA2dpCodec_t::OpenStream);
    WAVEFORMATEX const * pWfxInputUsed;
    if(pWfxInput == NULL)
        {
        //use default WAVEFORMATEX
        WAVEFORMATEX wfxDefaultInput;
        wfxDefaultInput.cbSize = 0;
        wfxDefaultInput.nChannels = OUT_CHANNELS;
        wfxDefaultInput.nSamplesPerSec = SAMPLE_RATE;
        wfxDefaultInput.wBitsPerSample = BITS_PER_SAMPLE;
        wfxDefaultInput.wFormatTag = WAVE_FORMAT_PCM;
        wfxDefaultInput.nBlockAlign = (UINT)((wfxDefaultInput.wBitsPerSample >> 3) << (wfxDefaultInput.nChannels >> 1));
        wfxDefaultInput.nAvgBytesPerSec = wfxDefaultInput.nSamplesPerSec * wfxDefaultInput.nBlockAlign;
        pWfxInputUsed = &wfxDefaultInput;
        }
    else
        {
        pWfxInputUsed = pWfxInput;
        }
    if(!IsCurrentLoaded())
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }
    if(IsOpened())
        {
        DEBUGMSG(ZONE_A2DP_CODEC_TRACE, ( (TEXT("A2DP: BthA2dpCodec_t :: OpenStream : Already Open, doing nothing"))));
        return ERROR_ALREADY_EXISTS;
        }


    DWORD Res = ERROR_SUCCESS;
    switch(m_CurrentIndex)
        {
        case SBC:
            {
            m_pOutputWaveFormat = new SBCWAVEFORMAT;
            if(!m_pOutputWaveFormat)
                {
                Res = ERROR_OUTOFMEMORY;
                m_StreamInstance.pwfxDst = NULL;
                break;
                }
            SBCWAVEFORMAT * pOutputWaveFormatTemp =  (SBCWAVEFORMAT *) m_pOutputWaveFormat;
            pOutputWaveFormatTemp->wfx.wFormatTag = WAVE_FORMAT_SBC;
            pOutputWaveFormatTemp->wfx.wBitsPerSample = SBC_BITS_PER_SAMPLE;
            //any other bits per sample other than 16 are unaceptable
            SVSUTIL_ASSERT(pWfxInputUsed->wBitsPerSample == SBC_BITS_PER_SAMPLE);
            pOutputWaveFormatTemp->wfx.nSamplesPerSec = pWfxInputUsed->nSamplesPerSec;
            if(1 == pWfxInputUsed->nChannels)
                {
                pOutputWaveFormatTemp->setChannelMode(SBC_CHANNEL_MODE_MONO);
                // don't expect mono
                SVSUTIL_ASSERT(0);
                }
            else 
                {
                pOutputWaveFormatTemp->setChannelMode(CHANNEL_MODE);

                // expect either 1 or 2 channels
                SVSUTIL_ASSERT(2 == pWfxInputUsed->nChannels);
                }
            pOutputWaveFormatTemp->allocation_method = ALLOCATION_METHOD;
            pOutputWaveFormatTemp->nSubbands = SUBBANDS;
            pOutputWaveFormatTemp->nBlocks = BLOCK_SIZE;
            pOutputWaveFormatTemp->bitpool = BIT_POOL;
            pOutputWaveFormatTemp->evalNonstandard();
            m_HeaderByteCount = 1;

            m_StreamInstance.pwfxDst = (PWAVEFORMATEX)m_pOutputWaveFormat;  
            CalculateBitRate();
            break;
            }
            
        default:
            m_StreamInstance.pwfxDst = NULL;
            Res = ERROR_NOT_SUPPORTED;
            break;
        }
    

    if(ERROR_SUCCESS == Res)
        {       
        m_StreamInstance.cbStruct = sizeof(ACMDRVSTREAMINSTANCE);
        memcpy(&m_InputWaveFormat, pWfxInputUsed, sizeof(m_InputWaveFormat));
        m_StreamInstance.pwfxSrc = (LPWAVEFORMATEX)&m_InputWaveFormat;
        m_StreamInstance.fdwOpen = ACM_STREAMOPENF_NONREALTIME;

        // Open the stream
        DWORD Err = m_pDriverProc(m_DriverId,(HDRVR)m_Dll, ACMDM_STREAM_OPEN, (LPARAM)&m_StreamInstance, (LPARAM)0);
        if(MMSYSERR_NOERROR != Err)
            {
            SVSUTIL_ASSERT(0);
            Res = E_FAIL;
            }    
        }
        
    if(ERROR_SUCCESS == Res)
        {      
        Opened();
        }
    else
        {
        CloseStream();
        }

    return Res;
}

DWORD 
BthA2dpCodec_t::
GetOutputBufferSize(
    DWORD InputByteCount, 
    DWORD & OutputByteCount
    )
{
    FUNCTION_TRACE(BthA2dpCodec_t::GetOutputBufferSize);

    //SVSUTIL_ASSERT(IsOpened());
    if(!IsOpened())
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }
    
        
    ACMDRVSTREAMSIZE StreamSize = {0};
    StreamSize.cbStruct = sizeof(ACMDRVSTREAMSIZE);
    StreamSize.fdwSize = ACM_STREAMSIZEF_SOURCE;
    StreamSize.cbSrcLength = InputByteCount;
    StreamSize.cbDstLength = 0;

    // Figure out how bit a destination buffer I need
    DWORD Err = m_pDriverProc(m_DriverId,(HDRVR)m_Dll, ACMDM_STREAM_SIZE, (LPARAM)&m_StreamInstance, (LPARAM)&StreamSize);
    DWORD Res = ERROR_SUCCESS; 
    if(MMSYSERR_NOERROR == Err)
        {
        OutputByteCount = StreamSize.cbDstLength;
        DEBUGMSG(ZONE_A2DP_CODEC_TRACE, ( L"A2DP: BthA2dpCodec_t :: GetOutputBufferSize :: Input [0x%08x], Output [0x%08x]\n", InputByteCount,OutputByteCount));
        }
    else
        {
        Res = E_FAIL;
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP: BthA2dpCodec_t :: GetOutputBufferSize :: Failed Err:[0x%08x]\n", Err));
        }
        
    return Res;
}

DWORD 
BthA2dpCodec_t::
GetInputBufferSize(
    DWORD & InputByteCount, 
    DWORD OutputByteCount
    )
{
    FUNCTION_TRACE(BthA2dpCodec_t::GetOutputBufferSize);

    //SVSUTIL_ASSERT(IsOpened());
    if(!IsOpened())
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }
    
        
    ACMDRVSTREAMSIZE StreamSize = {0};
    StreamSize.cbStruct = sizeof(ACMDRVSTREAMSIZE);
    StreamSize.fdwSize = ACM_STREAMSIZEF_DESTINATION;
    StreamSize.cbSrcLength = 0;
    StreamSize.cbDstLength = OutputByteCount;

    // Figure out how bit a destination buffer I need
    DWORD Err = m_pDriverProc(m_DriverId,(HDRVR)m_Dll, ACMDM_STREAM_SIZE, (LPARAM)&m_StreamInstance, (LPARAM)&StreamSize);
    DWORD Res = ERROR_SUCCESS; 
    if(MMSYSERR_NOERROR == Err)
        {
        InputByteCount = StreamSize.cbSrcLength;
        DEBUGMSG(ZONE_A2DP_CODEC_TRACE, ( L"A2DP: BthA2dpCodec_t :: GetOutputBufferSize :: Input [0x%08x], Output [0x%08x]\n", InputByteCount,OutputByteCount));
        }
    else
        {
        Res = E_FAIL;
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP: BthA2dpCodec_t :: GetOutputBufferSize :: Failed Err:[0x%08x]\n", Err));
        }
        
    return Res;
}

#define MAX_ALLOCATION_SIZE     678 // use this since allocater needs constant size
#define A2DP_FREE_LIST_SIZE     30  // Free list size
static ce::free_list<A2DP_FREE_LIST_SIZE> g_fl;

void BufferFree (BD_BUFFER *pBuf) {
    if (! pBuf->fMustCopy)
        g_fl.deallocate(pBuf);
}

BD_BUFFER *BufferAlloc (int cSize) {
    SVSUTIL_ASSERT (cSize > 0);
    SVSUTIL_ASSERT (cSize <= MAX_ALLOCATION_SIZE);
    
    BD_BUFFER *pRes = (BD_BUFFER *)g_fl.allocate(MAX_ALLOCATION_SIZE + sizeof(BD_BUFFER));
    if (pRes) {
        pRes->cSize = cSize;

        pRes->cEnd = pRes->cSize;
        pRes->cStart = 0;

        pRes->fMustCopy = FALSE;
        pRes->pFree = BufferFree;
        pRes->pBuffer = (unsigned char *)(pRes + 1);
    }
    
    return pRes;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       AllocateOutputBDBuffer()

Description:    This function alocates a BD buffer that has enough 
                space for:
                    the converted input data (PCM to codec in use) 
                    metadata (that is not converted by the codec) 
                    a2dp specific header for codec
    
Returns:        pointer to buffer, null indicates failure
-------------------------------------------------------------------*/
BD_BUFFER*
BthA2dpCodec_t::
AllocateOutputBDBuffer(
    DWORD InputDataByteCount, 
    DWORD MetaDataByteCount,
    DWORD & OutputDataByteCount,
    DWORD & CodecMetaDataByteCount
    )
{
    FUNCTION_TRACE(BthA2dpCodec_t::AllocateOutputBDBuffer);
    BD_BUFFER* OutputBuffer = NULL;
    DWORD Err =  GetOutputBufferSize(InputDataByteCount, OutputDataByteCount);
    if(ERROR_SUCCESS != Err)
        {
        return NULL;
        }
    CodecMetaDataByteCount  = m_HeaderByteCount;
    UINT TotalOutputBufferSize = 0;
    
    HRESULT Result = CeUIntAdd3(OutputDataByteCount, 
                                  MetaDataByteCount, 
                                  m_HeaderByteCount,
                                  &TotalOutputBufferSize);

    if(S_OK == Result)
        {
        DEBUGMSG(ZONE_A2DP_CODEC_TRACE, 
                 ( L"A2DP: BthA2dpCodec_t :: AllocateOutputBDBuffer : Allocating [0x%x] sized buffer\r\n", 
                 TotalOutputBufferSize));

        OutputBuffer = BufferAlloc(TotalOutputBufferSize); 
            
         }
    else
        {
        DEBUGMSG(ZONE_ERROR, 
                ( L"A2DP: BthA2dpCodec_t :: AllocateOutputBDBuffer : arithmetic overflow, CeUIntAdd3 returned = [%d], not allocating buffer \r\n",
                Result));
        }
    
    return OutputBuffer;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       FreeBDBuffer()

Description:    Frees DD buffer  

Returns:        void 
-------------------------------------------------------------------*/ 
void
BthA2dpCodec_t::
FreeBDBuffer(BD_BUFFER * pBuf)
{
    BufferFree(pBuf);
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       GetBitRate()

Description:    sets BitRate to the average bitrate of the codec 

Returns:        DWORD with status 
-------------------------------------------------------------------*/ 

DWORD 
BthA2dpCodec_t::
GetBitRate(
    DWORD &BitRate
    )
{
    if(!IsOpened())
        {
        BitRate= 0;
        return ERROR_SERVICE_NOT_ACTIVE;
        }
    ASSERT(m_CachedBitRate != -1);
    BitRate = m_CachedBitRate;
    
    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: GetBitRate : Calculated [%d]\r\n", BitRate));
   
    return ERROR_SUCCESS;
}   

DWORD 
BthA2dpCodec_t::
GetPCMBufferSize(
    DWORD &NewPCMBufferSize,
    DWORD MaxPCMBufferSize
    )
{
    if(!IsOpened())
        {
        MaxPCMBufferSize = 0;
        return ERROR_SERVICE_NOT_ACTIVE;
        }
    DWORD FrameSize = ((SBCWAVEFORMAT*)m_pOutputWaveFormat)->blockAlignment();
    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: GetPCMBufferSize : Calculated frame size[%d] \r\n", FrameSize));

    DWORD PCMFrameSize = 0;
    GetInputBufferSize(PCMFrameSize, FrameSize);

    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: GetPCMBufferSize : Calculated PCMFrameSize[%d] \r\n", PCMFrameSize));
    DWORD MaxNuberFrames = AVDTP_MTU/FrameSize;
    NewPCMBufferSize = MaxNuberFrames*PCMFrameSize;
    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: GetPCMBufferSize : Calculated NewPCMBufferSize[%d] \r\n", NewPCMBufferSize));

    while(NewPCMBufferSize > MaxPCMBufferSize)
        {
        NewPCMBufferSize -= PCMFrameSize;
        }

    return ERROR_SUCCESS;
}    


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       GetFrameSize()

Description:    sets FrameSize the size of the output frames  

Returns:        DWORD with status 
-------------------------------------------------------------------*/ 


DWORD 
BthA2dpCodec_t::
GetFrameSize(
    DWORD &FrameSize
    )
{
    if(!IsOpened())
        {
        FrameSize = 0;
        return ERROR_SERVICE_NOT_ACTIVE;
        }
    FrameSize = ((SBCWAVEFORMAT*)m_pOutputWaveFormat)->blockAlignment();
    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: GetFrameSize : Calculated [%d] \r\n", FrameSize));
    return ERROR_SUCCESS;
}    

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       GetHeaderSize()

Description:    sets HeaderSize the size of the output frames  

Returns:        DWORD with status 
-------------------------------------------------------------------*/ 


DWORD 
BthA2dpCodec_t::
GetHeaderSize(
    DWORD &HeaderSize
    )
{
    if(!IsOpened())
        {
        HeaderSize = 0;
        return ERROR_SERVICE_NOT_ACTIVE;
        }
    HeaderSize = m_HeaderByteCount;
    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: GetHeaderSize : [%d] \r\n", HeaderSize));
    return ERROR_SUCCESS;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Function:       WriteHeader()

Description:    calculates writes the a2dp specific header to 
                pBuffer  

Returns:        DWORD with status 
-------------------------------------------------------------------*/
DWORD
BthA2dpCodec_t::
WriteHeader(
    PBYTE pBuffer,
    DWORD OutputByteCount
    )
{
    SVSUTIL_ASSERT(pBuffer);
    DWORD FrameSize;
    DWORD Res = GetFrameSize(FrameSize);
    if(ERROR_SUCCESS == Res)
        {
        BYTE Frames = OutputByteCount/FrameSize;
        if(OutputByteCount % FrameSize != 0)
            {
            Frames++;
            }
        pBuffer[0] = Frames;
        }
    return Res;
}

DWORD
BthA2dpCodec_t::
EncodeBuffer(
    const PBYTE pInputBuffer, 
    DWORD InputByteCount, 
    PBYTE pOutputBuffer, 
    DWORD OutputByteCount
    )
{

    FUNCTION_TRACE(BthA2dpCodec_t::ConvertBuffer);
    //SVSUTIL_ASSERT(IsOpened());
    if(!IsOpened())
        {
        return ERROR_SERVICE_NOT_ACTIVE;
        }
        
    ACMSTREAMHEADER StreamHeader;
    memset(&StreamHeader, 0, sizeof(StreamHeader));
    StreamHeader.cbStruct = sizeof(StreamHeader);

    StreamHeader.pbSrc = pInputBuffer; // the source data to convert
    StreamHeader.cbSrcLength = InputByteCount;

    StreamHeader.pbDst = pOutputBuffer + m_HeaderByteCount;
    StreamHeader.cbDstLength = OutputByteCount;
    

#ifdef DEBUG
    DWORD TimeStart, TimeEnd, TimeElapsed, TimeAudio;
    TimeStart = GetTickCount();
#endif
    DWORD Err = m_pDriverProc(m_DriverId,(HDRVR)m_Dll, ACMDM_STREAM_CONVERT, (LPARAM)&m_StreamInstance, (LPARAM)&StreamHeader);
#ifdef DEBUG
    TimeEnd = GetTickCount();
    TimeAudio = (InputByteCount*1000)/m_StreamInstance.pwfxSrc->nAvgBytesPerSec;
    TimeElapsed = TimeEnd - TimeStart;

#endif
    if(Err == MMSYSERR_NOERROR)
        {
        DEBUGMSG(ZONE_A2DP_CODEC_TRACE, ( L"A2DP: BthA2dpCodec_t :: EncodeBuffer : Succeded Converting [%d] msec of PCM audio in [%d] msec\r\n", TimeAudio, TimeElapsed));
        WriteHeader(pOutputBuffer, OutputByteCount);
        //pOutputBuffer[0] = 0x0e;
        return ERROR_SUCCESS;
        }
    else
        {
        DEBUGMSG(DEBUG_ERROR, ( L"A2DP: BthA2dpCodec_t :: EncodeBuffer : Failed Converting [%d] msec of PCM audiowith Err [0x%x]\r\n", TimeAudio, Err));
        return E_FAIL;
        }
}
    
        
inline BOOL
BthA2dpCodec_t::
IsCurrentLoaded()
{
    FUNCTION_TRACE(BthA2dpCodec_t::IsCurrentLoaded);
    return (m_State != CODEC_STATE_IDLE);
}

//currently just assume SBC
//BOOL BthA2dpCodec_t::NegotiateCodecType()
//{
//    StringCchCopy(m_Name, sizeof(m_Name), m_pSupportedCodecs[SBC]);
//}
    
BOOL 
BthA2dpCodec_t::
IsCurrentIndexSupported()
{
    FUNCTION_TRACE(BthA2dpCodec_t::IsCurrentIndexSupported);
    if(m_CurrentIndex > MAX_INDEX_SUPPORTED)
    {
        return FALSE;
    }
    return TRUE;
}



DWORD
BthA2dpCodec_t::
CloseStream()
{
    FUNCTION_TRACE(BthA2dpCodec_t::CloseStream);
   // SVSUTIL_ASSERT(IsOpened());
    DWORD Res = ERROR_SUCCESS;
    /*if(!IsOpened())
        {
        Res = ERROR_SERVICE_NOT_ACTIVE;
        }
    else
        {*/
        DWORD Err = m_pDriverProc(m_DriverId,(HDRVR)m_Dll, ACMDM_STREAM_CLOSE, (LPARAM)&m_StreamInstance, 0);
        Closed();
        if(Err != MMSYSERR_NOERROR)
            {
            Res = E_FAIL;
            }
       // }
        
    ClearStreamInfo();
    return Res;

}

DWORD BthA2dpCodec_t::UnloadCurrent()
{

    FUNCTION_TRACE(BthA2dpCodec_t::UnloadCurrent);
    SVSUTIL_ASSERT(CODEC_STATE_LOADED == m_State);
    DWORD Res = E_FAIL;
    if(IsOpened())
        {
        DEBUGMSG(ZONE_A2DP_CODEC_TRACE, ( L"A2DP: BthA2dpCodec_t :: UnloadCurrent : stream is open, can't unload, m_Dll [0x%x]", m_Dll));
        return Res;
        }
    /*if(CODEC_STATE_LOADED != m_State)
        {
        Res = ERROR_SERVICE_NOT_ACTIVE;
        }*/
    if(m_Dll)
        {
        if(m_pDriverProc)
            {
            m_pDriverProc(m_DriverId,(HDRVR)m_Dll, DRV_CLOSE, 0, 0);
            Res = ERROR_SUCCESS;
            }
        FreeLibrary(m_Dll);
        }

    DEBUGMSG(ZONE_A2DP_CODEC_TRACE, ( L"A2DP: BthA2dpCodec_t :: UnloadCurrent : Res: [0x%x], m_Dll [0x%x]", Res, m_Dll));
    ClearDriverInfo();
    Idle();
    return Res;        
}
    


inline void 
BthA2dpCodec_t::
Idle(
    void
    )        
{
    FUNCTION_TRACE(BthA2dpCodec_t::Idle);

    m_State = CODEC_STATE_IDLE; 
    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: Idle :: BthA2dpCodec_t::: [0x%08x] state CODEC_STATE_IDLE\n", this));

    
}

inline void 
BthA2dpCodec_t::
Loaded(
    void
    )        
{ 
    FUNCTION_TRACE(BthA2dpCodec_t::Loaded);

    SVSUTIL_ASSERT(CODEC_STATE_IDLE == m_State);
    m_State = CODEC_STATE_LOADED; 
    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: Loaded :: BthA2dpCodec_t::: [0x%08x] state CODEC_STATE_LOADED\n", this));

}

inline void 
BthA2dpCodec_t::
Opened(
    void
    )        
{ 
    FUNCTION_TRACE(BthA2dpCodec_t::Opened);

    SVSUTIL_ASSERT(CODEC_STATE_LOADED == m_State);
    m_State = CODEC_STATE_OPENED; 
    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: Opened :: BthA2dpCodec_t::: [0x%08x] state CODEC_STATE_OPENED\n", this));

}


inline void 
BthA2dpCodec_t::
Closed(
    void
    )        
{ 
    FUNCTION_TRACE(BthA2dpCodec_t::Closed);

    //SVSUTIL_ASSERT(CODEC_STATE_OPENED == m_State);
    m_State = CODEC_STATE_LOADED; 
    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: Closed :: BthA2dpCodec_t::: [0x%08x] state CODEC_STATE_LOADED\n", this));

}



inline BOOL 
BthA2dpCodec_t::
IsOpened(
    void
    )        
{ 
    FUNCTION_TRACE(BthA2dpCodec_t::IsOpened);

    DEBUGMSG(ZONE_A2DP_CODEC_VERBOSE, ( L"A2DP: BthA2dpCodec_t :: IsOpened :: BthA2dpCodec_t::: [0x%08x] state [0x%08x]\n", this,m_State));
    return(CODEC_STATE_OPENED == m_State); 

}


void
BthA2dpCodec_t::
ClearStreamInfo()
{
    delete m_pOutputWaveFormat;
    m_pOutputWaveFormat = NULL;
    memset(&m_InputWaveFormat, 0, sizeof(m_InputWaveFormat));
    memset(&m_StreamInstance, 0, sizeof(m_StreamInstance));
    m_CachedBitRate = -1;
}

void
BthA2dpCodec_t::
ClearDriverInfo()
{
    m_Dll = NULL;    
    memset(&m_pDriverProc, 0, sizeof(m_pDriverProc));
    m_DriverId = 0;
}    











