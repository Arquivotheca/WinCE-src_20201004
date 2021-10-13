//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

/*
    Name: DefGestureAnimation.h
    Purpose: define functions exposed by the gesture physics engine. 
             The Physics Engine will provide a basic level of animation 
             in response to a flick gesture in one of the four primary 
             directions. This behaviour is available for controls that do 
             not handle the WM_GESTURE message in their WindowProc
             and have the appropriate scroll style set for the window,
             corresponding to the direction of the flick.
*/

#pragma once
#include <window.hpp>

/// <summary>
/// Provide default gesture handling.
/// </summary>
/// <param name="hWnd">
/// the target window handle.
/// </param>
/// <param name="uMsg">Message to be handled.</param>
/// <param name="wParam">WPARAM parameter associated with message </param>
/// <param name="lParam">LPARAM parameter associated with message </param>
/// <returns>
///   S_OK - the gesture was appropriate for the current window and 
///          has been successfully processed.
///   E_NOTIMPL - the gesture was not appropriate for the control or was not
///          supported, and the gesture has not been processed.
///   Any other E_xxx value - the gesture processing has failed and this
///   was the error value.
/// </returns>
/// <remarks>
///   <para> Process the gesture message, verify that it is appropriate for the control
///   and run the flick window processing against the passed windows handle </para>
/// </remarks>
HRESULT DefGestureProc_I( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/// <summary>
/// Determines whether a gesture animation is running for a given window
/// </summary>
/// <param name="hwnd">
/// The target window handle
/// </param>
/// <returns>
/// A non-zero value if there is an animation running for the specified window,
/// zero otherwise.
/// </returns>
BOOL IsGestureAnimationRunning(HWND hwnd);

enum StopGestureAnimationMode
{
    // Follow the normal stop logic - stop the animation synchronously if
    // possible, asynchronously otherwise. The GWE critical section may
    // be yielded
    SGAM_StopAnimationNormal        = 0x00,

    // Prevents the animation sending any messages to the window. This will
    // always complete synchronously. This should only be used by DestroyWindow
    // when the animation end messages are not important. The GWE critical
    // section will not be yielded.
    SGAM_StopAnimationNoSend        = 0x01,

    // Force the animation to be stopped asynchronously on its own thread.
    // The GWE critical section will not be yielded
    SGAM_StopAnimationAsync         = 0x02,

    // Animation will be stopped with another smooth animation instead of
    // snapping to the correct place
    SGAM_StopAnimationSmoothly      = 0x03
};

/// <summary>
/// Stops a gesture animation for the specified window if one is running.
/// </summary>
/// <param name="hwnd">
/// The target window handle
/// </param>
/// <param name="eMode">
/// A value from StopGestureAnimationMode indicating how the animation should
/// be stopped
/// </param>
void StopGestureAnimation(HWND hwnd, StopGestureAnimationMode eMode);

/// <summary>
/// Stops all gesture animations currently running
/// </summary>
/// <param name="eMode">
/// A value from StopGestureAnimationMode indicating how the animations should
/// be stopped
/// </param>
void StopAllGestureAnimations(StopGestureAnimationMode eMode);

enum TouchEventType
{
    TouchEvent_Down,

    TouchEvent_Up
};

/// <summary>
/// User input queue uses it to report touch events.
/// </summary>
/// <param name="hwndHit">Handle to a window directly under the cursor.</param>
/// <param name="eTouchEvent">Type of touch event.</param>
void ReportTouchEvent(HWND hwndHit, TouchEventType eTouchEvent);
