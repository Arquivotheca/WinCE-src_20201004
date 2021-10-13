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
//
//  TUXTEST TUX DLL
//
//  Module: ft.h
//          Declares the TUX function table and test function prototypes EXCEPT
//          when included by globals.cpp, in which case it DEFINES the function
//          table.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#if (!defined(__FT_H__) || defined(__GLOBALS_CPP__))
#ifndef __FT_H__
#define __FT_H__
#endif

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __DEFINE_FTE__
#undef BEGIN_FTE
#undef FTE
#undef FTH
#undef END_FTE
#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, c, d, e) { TEXT(b), a, d, c, e },
#define END_FTE { NULL, 0, 0, 0, NULL } };
#else // __DEFINE_FTE__
#ifdef __GLOBALS_CPP__
#define BEGIN_FTE
#else // __GLOBALS_CPP__
#define BEGIN_FTE extern FUNCTION_TABLE_ENTRY g_lpFTE[];
#endif // __GLOBALS_CPP__
#define FTH(a, b)
#define FTE(a, b, c, d, e) TESTPROCAPI e(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
#define END_FTE
#endif // __DEFINE_FTE__

////////////////////////////////////////////////////////////////////////////////
// TUX Function table
//  To create the function table and function prototypes, two macros are
//  available:
//
//      FTH(level, description)
//          (Function Table Header) Used for entries that don't have functions,
//          entered only as headers (or comments) into the function table.
//
//      FTE(level, description, code, param, function)
//          (Function Table Entry) Used for all functions. DON'T use this macro
//          if the "function" field is NULL. In that case, use the FTH macro.
//
//  You must not use the TEXT or _T macros here. This is done by the FTH and FTE
//  macros.
//
//  In addition, the table must be enclosed by the BEGIN_FTE and END_FTE macros.
#define PROPERTY_BASEID 1000
#define PIN(x)                     PROPERTY_BASEID + x
#define VIDEOPROCAMP(x)         PROPERTY_BASEID + x + 100
#define CAMERACONTROL(x)         PROPERTY_BASEID + x + 200
#define DROPPEDFRAMES(x)         PROPERTY_BASEID + x + 300
#define VPCONFIG(x)                PROPERTY_BASEID + x + 400
#define VIDEOCONTROL(x)            PROPERTY_BASEID + x + 500
#define VIDEOCOMPRESSION(x)    PROPERTY_BASEID + x + 600
#define METADATA(x)    PROPERTY_BASEID + x + 700

#define STREAM(x)                PROPERTY_BASEID + x + 9000

BEGIN_FTE
    FTH(0, "CSPROPSETID_Pin")
    FTE(1,     "1: Test CSPROPERTY_PIN_CTYPES. Verify you can get the property", PIN(1) , 0, Test_PIN_CTYPE )
    FTE(1,     "2: Test CSPROPERTY_PIN_CTYPES. Verify that a client can not set the property", PIN(2) , 0, Test_PIN_CTYPE2 )
    FTE(1,     "3: Test CSPROPERTY_PIN_CTYPES. Verify that it handles invalid output buffers correctly. ", PIN(3) , 0, Test_PIN_CTYPE3 )
    FTE(1,     "4: Test CSPROPERTY_PIN_CINSTANCES. Verify there is one stream per pin and currentCount is 0 before stream creation ", PIN(4) , 0, Test_PIN_CINSTANCES )
    FTE(1,     "5: Test CSPROPERTY_PIN_CINSTANCES. Verify you can not set this property ", PIN(5) , 0, Test_PIN_CINSTANCES2 )
    FTE(1,     "6: Test CSPROPERTY_PIN_DATARANGES. Verify you can get this property", PIN(6) , 0, Test_PIN_DATARANGES )
    FTE(1,     "7: Test CSPROPERTY_PIN_DATARANGES. Verify you can not set this property ", PIN(7) , 0, Test_PIN_DATARANGES1 )
    FTE(1,     "8: Test CSPROPERTY_PIN_DEVICENAME. Verify that a GUID is retrieved for each Pin", PIN(8) , 0, Test_PIN_DEVICENAME )
    FTE(1,     "9: Test CSPROPERTY_PIN_DEVICENAME. Verify that CSPROPERTY_PIN_CATEGORY is a Read-only property", PIN(9) , 0, Test_PIN_DEVICENAME2 )
    FTE(1,     "10: Test CSPROPERTY_PIN_NAME. Verify that a Pin Name is retrieved for different Pins", PIN(10) , 0, Test_PIN_NAME )
    FTE(1,     "11: Test CSPROPERTY_PIN_NAME. Verify that PinName is a Read-only property", PIN(11) , 0, Test_PIN_NAME2 )
    FTE(1,     "12: Test CSPROPERTY_PIN_CATEGORY. Verify that a GUID is retrieved for each Pin", PIN(12) , 0, Test_PIN_CATEGORY )
    FTE(1,     "13: Test CSPROPERTY_PIN_CATEGORY. Verify that CSPROPERTY_PIN_CATEGORY is a Read-only property", PIN(13) , 0, Test_PIN_CATEGORY2 )
    FTH(0, "PROPSETID_VIDCAP_VIDEOPROCAMP")
    FTE(1,     "1: Test CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS. Try to get the current and default value", VIDEOPROCAMP(1) , 0, Test_CSProperty_VideoProcAmp_Brightness )
    FTE(1,     "2: Test CSPROPERTY_VIDEOPROCAMP_BRIGHTNESS. Try to get the range and set to various values", VIDEOPROCAMP(2) , 0, Test_CSProperty_VideoProcAmp_Brightness1 )
    FTE(1,     "3: Test CSPROPERTY_VIDEOPROCAMP_CONTRAST. Try to get the current and default value", VIDEOPROCAMP(3) , 0, Test_CSProperty_VideoProcAmp_Contrast )
    FTE(1,     "4: Test CSPROPERTY_VIDEOPROCAMP_CONTRAST. Try to get the range and set to various values", VIDEOPROCAMP(4) , 0, Test_CSProperty_VideoProcAmp_Contrast1 )
    FTE(1,     "5: Test CSPROPERTY_VIDEOPROCAMP_GAMMA. Try to get the current and default value", VIDEOPROCAMP(5) , 0, Test_CSProperty_VideoProcAmp_GAMMA )
    FTE(1,     "6: Test CSPROPERTY_VIDEOPROCAMP_GAMMA. Try to get the range and set to various values", VIDEOPROCAMP(6) , 0, Test_CSProperty_VideoProcAmp_GAMMA1 )
    FTE(1,     "7: Test CSPROPERTY_VIDEOPROCAMP_SATURATION. Try to get the current and default value", VIDEOPROCAMP(7) , 0, Test_CSProperty_VideoProcAmp_SATURATION )
    FTE(1,     "8: Test CSPROPERTY_VIDEOPROCAMP_SATURATION. Try to get the range and set to various values", VIDEOPROCAMP(8) , 0, Test_CSProperty_VideoProcAmp_SATURATION1 )
    FTE(1,     "9: Test CSPROPERTY_VIDEOPROCAMP_SHARPNESS. Try to get the current and default value", VIDEOPROCAMP(9) , 0, Test_CSProperty_VideoProcAmp_SHARPNESS )
    FTE(1,     "10: Test CSPROPERTY_VIDEOPROCAMP_SHARPNESS. Try to get the range and set to various values", VIDEOPROCAMP(10) , 0, Test_CSProperty_VideoProcAmp_SHARPNESS1 )
    FTE(1,     "11: Test CSPROPERTY_VIDEOPROCAMP_WHITEBALANCE. Try to get the current and default value", VIDEOPROCAMP(11) , 0, Test_CSProperty_VideoProcAmp_WHITEBALANCE )
    FTE(1,     "12: Test CSPROPERTY_VIDEOPROCAMP_WHITEBALANCE. Try to get the range and set to various values", VIDEOPROCAMP(12) , 0, Test_CSProperty_VideoProcAmp_WHITEBALANCE1 )
    FTE(1,     "13: Test CSPROPERTY_VIDEOPROCAMP_COLORENABLE. Try to get the current and default value", VIDEOPROCAMP(13) , 0, Test_CSProperty_VideoProcAmp_COLORENABLE )
    FTE(1,     "14: Test CSPROPERTY_VIDEOPROCAMP_COLORENABLE. Try to get the range and set to various values", VIDEOPROCAMP(14) , 0, Test_CSProperty_VideoProcAmp_COLORENABLE1 )
    FTE(1,     "15: Test CSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION. Try to get the current and default value", VIDEOPROCAMP(15) , 0, Test_CSProperty_VideoProcAmp_BACKLIGHT_COMPENSATION )
    FTE(1,     "16: Test CSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION. Try to get the range and set to various values", VIDEOPROCAMP(16) , 0, Test_CSProperty_VideoProcAmp_BACKLIGHT_COMPENSATION1 )
    FTE(1,     "17: Test CSPROPERTY_VIDEOPROCAMP_GAIN. Try to get the current and default value", VIDEOPROCAMP(17) , 0, Test_CSProperty_VideoProcAmp_GAIN )
    FTE(1,     "18: Test CSPROPERTY_VIDEOPROCAMP_GAIN. Try to get the range and set to various values", VIDEOPROCAMP(18) , 0, Test_CSProperty_VideoProcAmp_GAIN1 )
    FTE(1,     "19: Test CSPROPERTY_VIDEOPROCAMP_HUE. Try to get the current and default value", VIDEOPROCAMP(19) , 0, Test_CSProperty_VideoProcAmp_HUE )
    FTE(1,     "20: Test CSPROPERTY_VIDEOPROCAMP_HUE. Try to get the range and set to various values", VIDEOPROCAMP(20) , 0, Test_CSProperty_VideoProcAmp_HUE1 )
    FTH(0,     "PROPSETID_VIDCAP_CAMERACONTROL")
    FTE(1,     "1: Test CSPROPERTY_CAMERACONTROL_EXPOSURE. Try to get the current and default value", CAMERACONTROL(1) , 0, Test_VidCap_CameraControl_EXPOSURE )
    FTE(1,     "2: Test CSPROPERTY_CAMERACONTROL_EXPOSURE. Try to get the range and set to various values", CAMERACONTROL(2) , 0, Test_VidCap_CameraControl_EXPOSURE1 )
    FTE(1,     "3: Test CSPROPERTY_CAMERACONTROL_FOCUS. Try to get the current and default value", CAMERACONTROL(3) , 0, Test_VidCap_CameraControl_FOCUS )
    FTE(1,     "4: Test CSPROPERTY_CAMERACONTROL_FOCUS. Try to get the range and set to various values", CAMERACONTROL(4) , 0, Test_VidCap_CameraControl_FOCUS1 )
    FTE(1,     "5: Test CSPROPERTY_CAMERACONTROL_IRIS. Try to get the current and default value", CAMERACONTROL(5) , 0, Test_VidCap_CameraControl_IRIS )
    FTE(1,     "6: Test CSPROPERTY_CAMERACONTROL_IRIS. Try to get the range and set to various values", CAMERACONTROL(6) , 0, Test_VidCap_CameraControl_IRIS1 )
    FTE(1,     "7: Test CSPROPERTY_CAMERACONTROL_ZOOM. Try to get the current and default value", CAMERACONTROL(7) , 0, Test_VidCap_CameraControl_ZOOM )
    FTE(1,     "8: Test CSPROPERTY_CAMERACONTROL_ZOOM. Try to get the range and set to various values", CAMERACONTROL(8) , 0, Test_VidCap_CameraControl_ZOOM1 )
    FTE(1,     "9: Test CSPROPERTY_CAMERACONTROL_PAN. Try to get the current and default value", CAMERACONTROL(9) , 0, Test_VidCap_CameraControl_PAN )
    FTE(1,     "10: Test CSPROPERTY_CAMERACONTROL_PAN. Try to get the range and set to various values", CAMERACONTROL(10) , 0, Test_VidCap_CameraControl_PAN1 )
    FTE(1,     "11: Test CSPROPERTY_CAMERACONTROL_ROLL. Try to get the current and default value", CAMERACONTROL(11) , 0, Test_VidCap_CameraControl_ROLL )
    FTE(1,     "12: Test CSPROPERTY_CAMERACONTROL_ROLL. Try to get the range and set to various values", CAMERACONTROL(12) , 0, Test_VidCap_CameraControl_ROLL1 )
    FTE(1,     "13: Test CSPROPERTY_CAMERACONTROL_TILT. Try to get the current and default value", CAMERACONTROL(13) , 0, Test_VidCap_CameraControl_TILT )
    FTE(1,     "14: Test CSPROPERTY_CAMERACONTROL_TILT. Try to get the range and set to various values", CAMERACONTROL(14) , 0, Test_VidCap_CameraControl_TILT1 )
    FTE(1,     "15: Test CSPROPERTY_CAMERACONTROL_FLASH. Try to get the current and default value", CAMERACONTROL(15) , 0, Test_VidCap_CameraControl_FLASH )
    FTE(1,     "16: Test CSPROPERTY_CAMERACONTROL_FLASH. Try to get the range and set to various values", CAMERACONTROL(16) , 0, Test_VidCap_CameraControl_FLASH1 )
    FTE(1,     "17: Test CSPROPERTY_CAMERACONTROL_FOCUS. Verify Asynchronous AutoFocus support is correct", CAMERACONTROL(17) , 0, Test_VidCap_CameraControl_FOCUS_ASYNC )
    FTH(0,    "PROPSETID_VIDCAP_DROPPEDFRAMES")
    FTE(1,     "1: Test CSPROPERTY_DROPPEDFRAMES_CURRENT. Verify you can get the property", DROPPEDFRAMES(1) , 0, Test_DROPPEDFRAMES_CURRENT )
    FTE(1,     "2: Test CSPROPERTY_DROPPEDFRAMES_CURRENT. Verify that a client can not set the property", DROPPEDFRAMES(2) , 0, Test_DROPPEDFRAMES_CURRENT2 )
    FTE(1,     "3: Test CSPROPERTY_DROPPEDFRAMES_CURRENT. Verify that it handles invalid output buffers correctly. ", DROPPEDFRAMES(3) , 0, Test_DROPPEDFRAMES_CURRENT3 )
    FTE(1,     "4: Test CSPROPERTY_DROPPEDFRAMES_CURRENT. Verify you can get the property correctly when the state change. ", DROPPEDFRAMES(4) , 0, Test_DROPPEDFRAMES_CURRENT4 )

    FTH(0,    "PROPSETID_VICAP_VIDEOCONTROL")
    FTE(1,     "1: Test CSPROPERTY_VIDEOCONTROL_CAPS.", VIDEOCONTROL(1) , 0, Test_VIDEOCONTROL_CAPS )
    FTE(1,     "2: Test CSPROPERTY_VIDEOCONTROL_CAPS2.", VIDEOCONTROL(2) , 0, Test_VIDEOCONTROL_CAPS2 )
    FTE(1,     "3: Test CSPROPERTY_VIDEOCONTROL_CAPS3. ", VIDEOCONTROL(3) , 0, Test_VIDEOCONTROL_CAPS3 )
    FTE(1,     "4: Test CSPROPERTY_VIDEOCONTROL_ACTUAL_FRAME_RATE. ", VIDEOCONTROL(4) , 0, Test_VIDEOCONTROL_ACTUAL_FRAME_RATE )
    FTE(1,     "5: Test CSPROPERTY_VIDEOCONTROL_FRAME_RATES. ", VIDEOCONTROL(5) , 0, Test_VIDEOCONTROL_FRAME_RATES )
    FTH(0,    "PROPSETID_VICAP_VIDEOCOMPRESSION")
    FTE(1,     "1: Test CSPROPERTY_VIDEOCOMPRESSION_GETINFO.", VIDEOCOMPRESSION(1) , 0, Test_CSProperty_VIDEOCOMPRESSION_GETINFO )
    FTE(1,     "2: Test CSPROPERTY_VIDEOCOMPRESSION_KEYFRAME_RATE.", VIDEOCOMPRESSION(2) , 0, Test_CSProperty_VIDEOCOMPRESSION_KEYFRAME_RATE )
    FTE(1,     "3: Test CSPROPERTY_VIDEOCOMPRESSION_PFRAMES_PER_KEYFRAME. ", VIDEOCOMPRESSION(3) , 0, Test_CSProperty_VIDEOCOMPRESSION_PFRAMES_PER_KEYFRAME )
    FTE(1,     "4: Test CSPROPERTY_VIDEOCOMPRESSION_VIDEOCOMPRESSION_QUALITY. ", VIDEOCOMPRESSION(4) , 0, Test_CSProperty_VIDEOCOMPRESSION_QUALITY )
    FTE(1,     "5: Test CSPROPERTY_VIDEOCOMPRESSION_VIDEOCOMPRESSION_WINDOWSIZE. ", VIDEOCOMPRESSION(5) , 0, Test_CSProperty_VIDEOCOMPRESSION_WINDOWSIZE )

    FTH(0,    "PROPSETID_Metadata")
    FTE(1,     "1: Test CSPROPERTY_METADATA_ALL.", METADATA(1) , 0, Test_CSProperty_MetadataAll )

END_FTE

////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__
