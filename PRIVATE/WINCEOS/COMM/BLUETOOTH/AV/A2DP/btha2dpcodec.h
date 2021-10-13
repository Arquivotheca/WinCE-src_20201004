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
#ifndef __BTHA2DPCODEC_T_HXX_INCLUDED__
#define __BTHA2DPCODEC_T_HXX_INCLUDED__

#include <windows.h>

#include "sbc.hxx"
#include <mmreg.h>
#include <msacm.h>
#include <msacmdrv.h>
#include <bt_buffer.h>

//this is a helper class for loading and negotiating all the codecs used 
//for A2DP_t

// 



class BthA2dpCodec_t
{
public:
    BthA2dpCodec_t();
    ~BthA2dpCodec_t();
    
    DWORD
    Load();
    
    DWORD 
    LoadCurrent();

    DWORD 
    OpenStream(
        const WAVEFORMATEX * const pwfxInput
        );
    
    BD_BUFFER*
    AllocateOutputBDBuffer(
        DWORD InputDataByteCount, 
        DWORD MetaDataByteCount,
        DWORD& OutputDataByteCount,
        DWORD & CodecMetaDataByteCount
        );

    void
    FreeBDBuffer(
        BD_BUFFER * pBuf
        );


    DWORD 
    GetInputBufferSize(
        DWORD & InputByteCount, 
        DWORD OutputByteCount
        );


    DWORD 
    GetOutputBufferSize(
        DWORD InputByteCount,
        DWORD & OutputByteCount
        );

    DWORD 
    GetBitRate(
        DWORD &BitRate
        );

    DWORD 
    GetFrameSize(
            DWORD &FrameSize
            );

    DWORD 
    GetPCMBufferSize(
        DWORD &NewPCMBufferSize,
        DWORD MaxPCMBufferSize
        );
        
    DWORD 
    GetHeaderSize(
            DWORD &FrameSize
            );
    DWORD
    WriteHeader(
        PBYTE pBuffer,
        DWORD OutputByteCount
        );
    

    DWORD 
    EncodeBuffer(
        const PBYTE pInputBuffer, 
        DWORD InputByteCount, 
        PBYTE pOutputBuffer, 
        DWORD OutputByteCount
        );

    BOOL 
    IsCurrentLoaded();

    BOOL 
    IsOpened();   

    BOOL 
    IsCurrentIndexSupported();

    DWORD 
    CloseStream();

    DWORD 
    UnloadCurrent();

    void
    Reset();

    

private:
    void 
    Idle();

    void     
    Loaded();
    
    void 
    Opened();

    void
    Closed();




    

private:
    enum BthA2dpCodec_e {
        CODEC_STATE_IDLE,
        CODEC_STATE_LOADED,
        CODEC_STATE_OPENED, 
    } m_State;
    
    static const DWORD MAX_CODEC_DLL_NAME_LENGTH = 256;
    
    enum eSupportedCodecs { SBC =0};

    static const DWORD DEFAULT_CODEC = SBC;
    static const DWORD MAX_INDEX_SUPPORTED = 0;
    static const DWORD NO_CODEC = MAX_INDEX_SUPPORTED+1;
    static const TCHAR * const DEFAULT_SUPPORTED_CODEC_DLL_NAMES[];
    static const TCHAR * const CODEC_DLL_REGISTRY_KEY_NAME; 

    TCHAR m_pSupportedCodecs[MAX_INDEX_SUPPORTED+1][MAX_CODEC_DLL_NAME_LENGTH];
    HMODULE m_Dll;    
    DWORD m_CurrentIndex;
    DRIVERPROC m_pDriverProc;
    DWORD m_DriverId;


    ACMDRVSTREAMINSTANCE m_StreamInstance;


    void * m_pOutputWaveFormat;
    WAVEFORMATEX m_InputWaveFormat;
    int m_CachedBitRate;
    DWORD m_HeaderByteCount;

    void
    ResetInfo();

    void
    ReadCodecDllNamesFromRegistry();

    void
    ClearStreamInfo();

    void
    ClearDriverInfo();
    
    void 
    SetNameDefault();
    
    DWORD 
    CalculateBitRate();

};

#endif
