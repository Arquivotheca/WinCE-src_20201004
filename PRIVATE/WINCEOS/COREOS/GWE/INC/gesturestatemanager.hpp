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
#pragma once
#include <window.hpp>
#include <auto_xxx.hxx>

class AutoGestureInfo;
class AnimationState;

/// <summary>
/// Class holds settings of automatic gestures and managing gesture animations.
/// </summary>
class GestureStateManager
{
public:
    explicit GestureStateManager();
    ~GestureStateManager();

    HRESULT GetWAGInfo(__out LPWAGINFO pWAGI) const;
    HRESULT SetWAGInfo(__in LPCWAGINFO pWAGI);

    HRESULT GetWAGInfo(__out AutoGestureInfo* pAutoGestureInfo) const;
    HRESULT SetWAGInfo(__in const AutoGestureInfo* pAutoGestureInfo);

    inline AnimationState* GetAnimationState() const;
    inline void SetAnimationState(AnimationState* pAnimationState);

    static BOOL GetWindowAutoGesture_I(
        __in HWND hWnd,
        __out_bcount(cbInfo) LPWAGINFO pWAGI,
        __in UINT cbInfo
        );

    static BOOL SetWindowAutoGesture_I(
        __in HWND,
        __in_bcount(cbInfo) LPCWAGINFO pWAGI,
        __in UINT cbInfo
        );

private:
    ce::auto_ptr<AutoGestureInfo> m_pAgInfo;
    AnimationState* m_pAnimationState;
};

/// <summary>
/// Return current animation state.
/// </summary>
inline AnimationState* GestureStateManager::GetAnimationState() const
{
    return m_pAnimationState;
}

/// <summary>
/// Set current animation state.
/// </summary>
inline void GestureStateManager::SetAnimationState(AnimationState* pAnimationState)
{
    m_pAnimationState = pAnimationState;
}
