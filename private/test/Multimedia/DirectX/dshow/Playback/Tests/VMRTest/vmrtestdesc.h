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
////////////////////////////////////////////////////////////////////////////////

#ifndef VIDEO_MIXING_RENDERER_TEST_DESC_H
#define VIDEO_MIXING_RENDERER_TEST_DESC_H

#include "BaseVRTestDesc.h"

const DWORD VMR_MAX_MONITORS = 15;
const DWORD VMR_TEST_DURATION = 10000; // 10 seconds

typedef enum {
    TESTSTATE_NULL,
    TESTSTATE_PRE_VMRCONFIG,        // before VMR is configured (before connection if mixing is involved)
    TESTSTATE_PRE_CONNECTION,       // before graph is connected
    TESTSTATE_PRE_ALLOCATION,       // before going to pause state, that is, before allocation takes place
    TESTSTATE_PAUSED,               // after allocation takes place, right before going to run state
    TESTSTATE_RUNNING,              // graph is running
    TESTSTATE_STOPPED               // graph is stopped after having run
} TestState;

class VMRTestDesc : public BaseVRTestDesc
{
public:
    // Constructor & Destructor
    VMRTestDesc();
    virtual ~VMRTestDesc();

public:
    STREAMINFO *GetStreamInfo( DWORD dwStreamID );
    DWORD GetNumberOfStreams() { return m_dwStreams; }

    // Interface to retrieve each of the media
    virtual PlaybackMedia* GetMedia(int i);
    BOOL    MixingMode() { return m_bMixerMode; }
    DWORD   GetRenderingMode() { return m_dwRenderingMode; }
    
    TestState   GetVMRTestState() { return m_State; }
    void        SetVMRTestState( TestState state ) { m_State = state; }

public:
    DWORD            m_dwRenderingMode;
    BOOL            m_bMixerMode;
    DWORD            m_dwStreams;
    VMRALPHABITMAP    m_AlphaBitmap;
    STREAMINFO    m_StreamInfo[VMR_MAX_INPUT_STREAMS];
    TCHAR                   szSrcImgPath[MAX_PATH];
    TCHAR                   szDestImgPath[MAX_PATH];
    VMR_TEST_MODE   m_vmrTestMode;
    DWORD   m_dwRenderingPrefsSurface;
    DWORD   m_dwPassRate;
    DWORD   m_dwTestSteps;
    BOOL    m_bCancelStep;
    BOOL    m_bRunFirst;
    BOOL    m_bPauseFirst;
    BOOL    m_bRunInMiddle;
    BOOL    m_bPauseInMiddle;
    long    m_lStepSize;
    long    m_lFirstStepSize;
    
private:
    TestState   m_State;
};

#ifndef SAFERELEASE
#define SAFERELEASE(p) if ((p) != NULL) { (p)->Release(); (p) = NULL; } else
#endif

BOOL
VerifyVideoPosition( IVMRWindowlessControl *pVMRWLControl, VMRTestDesc *pTestDesc );

BOOL
VerifyStreamParameters( IVMRMixerControl *pVMRMixerControl, VMRTestDesc *pTestDesc );

BOOL
SetStreamParameters( IVMRMixerControl *pVMRMixerControl, VMRTestDesc *pTestDesc );

#endif
