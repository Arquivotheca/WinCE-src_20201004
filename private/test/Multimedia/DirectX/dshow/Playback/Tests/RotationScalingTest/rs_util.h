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

#ifndef RS_UTIL_H
#define RS_UTIL_H

#include <atlbase.h>
#include "TestGraph.h"
#include "RotationScalingTestDesc.h"
#include "logging.h"
#include "VideoUtil.h"

typedef HRESULT (*InterfaceFunction)(IAMVideoTransform *pVideoTransform);


// Helpers
HRESULT VMRRotate(TestGraph* pTestGraph, AM_ROTATION_ANGLE rotateAngle, bool* pbRotateWindow, DWORD *pAngle);
HRESULT VMRScale(TestGraph* pTestGraph, IBaseFilter *pVMR, WndScale wndScale, bool bRotate, BOOL bMixingMode);

#endif // ROTSCALE_UTIL_H
