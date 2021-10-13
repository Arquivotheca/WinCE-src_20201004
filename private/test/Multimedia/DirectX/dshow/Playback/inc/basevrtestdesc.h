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

#ifndef _BASE_VIDEO_RENDERER_TEST_DESC_H
#define _BASE_VIDEO_RENDERER_TEST_DESC_H

#include "TestDesc.h"

const DWORD VMR_MAX_INPUT_STREAMS = 4;

enum WhichVideoWindowTest
{
    AllTest,
    VideoMediaTest,
    VisibleTest,
    LeftAndTopTest,
    WidthAndHeightTest,
    WindowOwnerTest,
    WindowPosTest,
	WindowStyleTest,
	WindowScreenModeTest
};

typedef enum {
    CROP_NONE,
    CROP_TOPLEFT,
    CROP_TOPRIGHT,
    CROP_BOTTOMLEFT,
    CROP_BOTTOMRIGHT,
    CROP_CENTER
} CropMode;

typedef enum {
    ROTATION_ANGLE_NONE,    // don't rotate (that is, don't send any rotation command)
    ROTATION_ANGLE_0,       // rotate by 0 degrees
    ROTATION_ANGLE_90,      // rotate by 90 degrees
    ROTATION_ANGLE_180,     // rotate by 180 degrees
    ROTATION_ANGLE_270,     // rotate by 270 degrees
    ROTATION_ANGLE_DYNAMIC, // rotate randomly
    ROTATION_ANGLE_SIZEOF
} RotationAngle;

typedef enum {
    NORMAL_MODE = 0x1,
    IMG_SAVE_MODE = 0x2,
    IMG_VERIFICATION_MODE = 0x4,
    IMG_STM_VERIFICATION_MODE = 0x8,
    IMG_CAPTURE_MODE = 0x10,
} VMR_TEST_MODE;

struct STREAMINFO
{
    DWORD   dwStreamID;
    TCHAR   szMedia[MAX_PATH];
    TCHAR   szDownloadLocation[MAX_PATH];
    BOOL    m_bDRM;
    int     iZOrder;
    float   fAlpha;
    NORMALIZEDRECT  nrOutput;
    COLORREF    clrBkg;
    DWORD   dwMixerPrefs;
    STREAMINFO()
    {
        iZOrder = -1;
        fAlpha = -1.0f;
        m_bDRM = FALSE;
        nrOutput.left = -1.0f;
        nrOutput.top = -1.0f;
        nrOutput.bottom = -1.0f;
        nrOutput.right = -1.0f;
    }
};


class BaseVRTestDesc : public TestDesc
{
public:
	// Constructor & Destructor
	BaseVRTestDesc();
	virtual ~BaseVRTestDesc();

	bool IsUsingVR() { return m_bUseVR; }
	void SetUseVR( bool data ) { m_bUseVR = data; }

	WndPosList *GetVWPositions() { return &m_lstWndPos; }
	WndScaleList *GetVWScaleLists() { return &m_lstWndScaleLists; }
	WndRectList *GetTestWinPositions() { return &m_lstTestWinPos; }

	FILTER_STATE GetTestState() { return m_State; }
	void SetTestState( FILTER_STATE state ) { m_State = state; }
	StateList *GetTestStateList() { return &m_lstStates; }
	bool IsTestStatePresent() { return m_bStatePresent; }
	void SetStatePresent( bool data ) { m_bStatePresent = data; }

	RECT *GetSrcDestRect( bool bSrcRect = true );
	WndRectList *GetSrcDestRectList( bool bSrcRect = true );
	bool IsSrcRectPresent() { return m_bSrcRect; }
	bool IsDestRectPresent() { return m_bDestRect; }
	void SetSrcDestRectFlag( bool data, bool bSrcRect = true );

	WhichVideoWindowTest GetVWTests() { return m_eTest; }
	void SetVWTests( WhichVideoWindowTest data ) { m_eTest = data; }
	
protected:
	// The list of video window positions to test with
	WndPosList m_lstWndPos;

	// The list of strech/shrink/full screen/min/max to iterate the window through
	WndScaleList m_lstWndScaleLists;

	// The list of client areas for the occluding window
	WndRectList m_lstTestWinPos;

	// The state in which to run the test
	FILTER_STATE m_State;
	StateList    m_lstStates;
	bool m_bStatePresent;

	// Source rectangle
	bool m_bSrcRect;
	RECT m_rectSrc;
	WndRectList m_lstSrcRects;

	// Destination rectangle
	bool m_bDestRect;
	RECT m_rectDest;
	WndRectList m_lstDestRects;

	// which video windwo tests
	WhichVideoWindowTest	m_eTest;

	bool	m_bUseVR;
};

#endif
