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

/*
    sqmutil.h - kernel sqm marker support

    This file contains the kernel's sqm marker support.
*/

#ifndef __SQMUTIL_H__
#define __SQMUTIL_H__

// SQM support
typedef enum _enSQMMarker{
    SQM_MARKER_PHYSOOMCOUNT = 0,
    SQM_MARKER_VMOOMCOUNT,
    SQM_MARKER_PHYSLOWBYTES,
    SQM_MARKER_RAMBASELINEBYTES,
    SQM_MARKER_LAST_ITEM // always has to be last in this enum
}enSQMMarker;

void InitSQMMarkers ();
BOOL ResetSQMMarkers ();      
BOOL GetSQMMarkers (LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
BOOL UpdateSQMMarkers (enSQMMarker marker, DWORD dwAddendOrValue);

#endif

