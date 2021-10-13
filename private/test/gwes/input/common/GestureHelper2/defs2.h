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

#ifndef __DEFS2_H__
#define __DEFS2_H__

#ifndef ARRAYSIZE
#define     ARRAYSIZE(x)    (sizeof(x)/sizeof(x[0]))
#endif  ARRAYSIZE

#ifndef MAXUINT
#define     MAXUINT         ((UINT)~((UINT)0))
#endif // MAXUINT

#define CHKGSE(x, msg)          if(FAILED((x))) { info(FAIL, msg); goto Error; }
#define CHKGESTURE(x, msg)      if(WAIT_OBJECT_0 != WaitForSingleObject(x, CGestureWnd::EVENT_DEFAULTTIMEOUT)) { info(FAIL, msg); goto Error; }
#define CHKNGESTURE(x, msg)     if(WAIT_TIMEOUT != WaitForSingleObject(x, CGestureWnd::EVENT_DEFAULTTIMEOUT)) { info(FAIL, msg); goto Error; }

#define CHKGSEQ(x, msg)         if(FALSE == x) { info(FAIL, msg); goto Error; }

// Need to define this to prevent having to depend on tchddi.h from the OS subtree
#ifndef TOUCH_SCALING_FACTOR
            // The scaling factor for converting between screen and touch coordinates
#define     TOUCH_SCALING_FACTOR    4
#endif // TOUCH_SCALING_FACTOR

#define GID_CUSTOM          GID_LAST+1

#define GET_X_LPARAM(dw)    ((int)(short)LOWORD((dw)))
#define GET_Y_LPARAM(dw)    ((int)(short)HIWORD((dw)))

#define FLICK_RIGHT_ANGLE   0
#define FLICK_UP_ANGLE      90
#define FLICK_LEFT_ANGLE    180
#define FLICK_DOWN_ANGLE    270

#define PI                  3.14159265358979323846

#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))

#define DEG_TO_RAD(_arg_)   ((double)(_arg_) * (PI / 180.0))
#define RAD_TO_DEG(_arg_)   (int)(round((_arg_) * (180.0 / PI)))


__declspec(selectany) int g_CoreGestures[] = {
    GID_BEGIN,
    GID_END,
    GID_PAN,
    //GID_ROTATE, Blocked by 256355
    GID_SCROLL,
    GID_HOLD,
    GID_SELECT,
    GID_DOUBLESELECT
};

__declspec(selectany) int g_CanBeDisabledCoreGestures[] = {
    GID_PAN,
    //GID_ROTATE, Blocked by 256355
    GID_SCROLL,
    GID_HOLD,
    GID_SELECT,
    GID_DOUBLESELECT
};

__declspec(selectany) ULONGLONG g_TGFCombo[] = {
        GID_TO_TGF(GID_PAN)     | GID_TO_TGF(GID_SELECT),
        GID_TO_TGF(GID_PAN)     | GID_TO_TGF(GID_SCROLL),
        GID_TO_TGF(GID_HOLD)    | GID_TO_TGF(GID_SCROLL),
        GID_TO_TGF(GID_SELECT)  | GID_TO_TGF(GID_DOUBLESELECT),
        GID_TO_TGF(GID_DOUBLESELECT) | GID_TO_TGF(GID_SELECT),
        GID_TO_TGF(GID_SELECT) | GID_TO_TGF(GID_DOUBLESELECT) | 
            GID_TO_TGF(GID_HOLD) | GID_TO_TGF(GID_PAN) | 
            GID_TO_TGF(GID_SCROLL) /* | GID_TO_TGF(GID_ROTATE) Blocked by 256355 */
};

__declspec(selectany) DWORD g_GestureScopes[] = {
    TGF_SCOPE_WINDOW, 
    TGF_SCOPE_PROCESS
};

__declspec(selectany) ULONGLONG g_AllEnabledGesturesTGF = 0xFFF;

#endif // __DEFS_H__