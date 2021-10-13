//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
// debug.h: defines for debugging

#pragma once

// Debug zone bits
#define BIT_AVTRANSPORT 12
#define BIT_TRACE       13
#define BIT_WARN        14
#define BIT_ERROR       15

// Debug zones
#undef ZONE_WARN
#undef ZONE_ERROR

#define ZONE_AVTRANSPORT DEBUGZONE(BIT_AVTRANSPORT)
#define ZONE_TRACE       DEBUGZONE(BIT_TRACE)
#define ZONE_WARN        DEBUGZONE(BIT_WARN)
#define ZONE_ERROR       DEBUGZONE(BIT_ERROR)

// Default zones
#define ZONE_MASK(x)    (1 << (x))
#define DEFAULT_ZONES   ZONE_MASK(BIT_ERROR) | ZONE_MASK(BIT_WARN)


