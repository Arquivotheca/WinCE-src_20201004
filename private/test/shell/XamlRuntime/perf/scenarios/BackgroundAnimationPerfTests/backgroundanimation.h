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

#ifndef __EGGBEATERS_H__
#define __EGGBEATERS_H__

#include <XRCommon.h>
#include <xamlruntime.h>
#include <XRPtr.h>

#define CEPERF_ENABLE
#include <ceperf.h>
#include <perftestutils.h>
#undef TRACE //windowsqa.h defines TRACE

#define XAML_PATH_FORMAT        XRPERF_RESOURCE_DIR L"BackgroundAnimation\\%s"

// Tests cases
//
#define ID_TC_BUTTONS_OVER_ANIMATION          2
#define ID_TC_SLIDING_BUTTONS_OVER_ANIMATION  4
#define ID_TC_SLIDING_KBRD_OVER_ANIMATION     6

#endif // __IMAGEPERF_H__