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
/*---------------------------------------------------------------------------*\
 *  module: dstioctl.h
 *  purpose: header for IOCTLs between DST service and coredll
 *
\*---------------------------------------------------------------------------*/

#define DST_IOCTL_BASE  500
#define DST_METHOD(n)      (DST_IOCTL_BASE + n)
#define IOCTL_DSTSVC_NOTIFY_TIME_CHANGE CTL_CODE(FILE_DEVICE_SERVICE, DST_METHOD(0), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DSTSVC_NOTIFY_TZ_CHANGE   CTL_CODE(FILE_DEVICE_SERVICE, DST_METHOD(1), METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DSTSVC_TIME_CHANGE_INFORMATION {
    SYSTEMTIME stNew;       // SYSTEMTIME structure containing new time
    DWORD   dwType;         // Time Change Type: TM_LOCALTIME , TM_SYSTEMTIME
} DSTSVC_TIME_CHANGE_INFORMATION, *LPDSTSVC_TIME_CHANGE_INFORMATION;

