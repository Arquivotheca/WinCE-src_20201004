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

#ifndef ROTATIONSCALING_TEST_DESC_H
#define ROTATIONSCALING_TEST_DESC_H

#include "TestDesc.h"
#include "BaseVRTestDesc.h"

typedef enum {
    STATE_CREATED = 0x1,
    STATE_PAUSED  = 0x2,
    STATE_RUNNING = 0x4,
    STATE_ALL     = STATE_CREATED | STATE_PAUSED | STATE_RUNNING
} GraphState;

typedef std::vector<AM_ROTATION_ANGLE> RotateConfigList;

class RotationScalingTestDesc : public BaseVRTestDesc
{
public:
    DWORD dwTimeOut;
    // The list of strech/shrink/full screen/min/max to iterate the window through
    WndScaleList multiScaleList;
    WndScaleList dynamicScaleList;
    WndScaleList pausedScaleList;

    // Source rectangle
    bool bSrcRect;
    RECT srcRect;
    WndRectList m_lstSrcRects;

    // Destination rectangle
    bool bDestRect;
    RECT destRect;
    WndRectList m_lstDestRects;

    //Rotate
    RotateConfigList multiRotateConfigList;
    RotateConfigList dynamicRotateConfigList;
    RotateConfigList pausedRotateConfigList;

    //Set log and using VR flag
    bool bSetLog;
    bool bPrefUpstreamRotate;
    bool bPrefUpstreamScale;
    bool bCheckUsingOverlay;
    
    //VMR specific vars
    DWORD                 m_dwRenderingMode;
    DWORD                 m_dwPassRate;
    DWORD                 m_dwWindowSizeChange;
    BOOL                    m_bUpdateWindow;
    BOOL                    m_bMixerMode;
    DWORD                 m_dwStreams;
    VMRALPHABITMAP  m_AlphaBitmap;
    STREAMINFO          m_StreamInfo[VMR_MAX_INPUT_STREAMS];
    TCHAR                   szSrcImgPath[MAX_PATH];
    TCHAR                   szDestImgPath[MAX_PATH];
    VMR_TEST_MODE   m_vmrTestMode;
    DWORD                 m_dwRenderingPrefsSurface;
    DWORD               m_dwTestSteps;
    long                    m_lStepSize;
    
public:
    // Constructor & Destructor
    RotationScalingTestDesc();
    ~RotationScalingTestDesc();

    STREAMINFO *GetStreamInfo( DWORD dwStreamID );
    DWORD GetNumberOfStreams() { return m_dwStreams; }

    // Interface to retrieve each of the media
    virtual PlaybackMedia* GetMedia(int i);
    BOOL    MixingMode() { return m_bMixerMode; }
    DWORD   GetRenderingMode() { return m_dwRenderingMode; }

};

#ifndef SAFERELEASE
#define SAFERELEASE(p) if ((p) != NULL) { (p)->Release(); (p) = NULL; } else
#endif

BOOL
SetStreamParameters( IVMRMixerControl *pVMRMixerControl, RotationScalingTestDesc *pTestDesc );

#endif
