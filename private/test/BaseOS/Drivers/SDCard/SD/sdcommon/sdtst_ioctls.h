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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
#ifndef SDTST_IOCLS_H
#define SDTST_IOCLS_H

#include <windows.h>
#include <sdcardddk.h>

//2 Ioctls

#define MAKE_IOCTL(c) (DWORD)CTL_CODE(0x8000, (c + 2048), METHOD_NEITHER, FILE_ANY_ACCESS)

#define IOCTL_SAMPLE_TEST1                      MAKE_IOCTL(0)
#define IOCTL_SAMPLE_TEST2                      MAKE_IOCTL(1)
#define IOCTL_SIMPLE_BUSREQ_TST             MAKE_IOCTL(2)
#define IOCTL_SIMPLE_INFO_QUERY_TST         MAKE_IOCTL(3)
#define IOCTL_CANCEL_BUSREQ_TST             MAKE_IOCTL(4)
#define IOCTL_RETRY_ACMD_BUSREQ_TST         MAKE_IOCTL(5)
#define IOCTL_INIT_TST                          MAKE_IOCTL(6)
#define IOCTL_GET_BIT_SLICE_TST             MAKE_IOCTL(7)
#define IOCTL_OUTPUT_BUFFER_TST             MAKE_IOCTL(8)
#define IOCTL_MEMLIST_TST                       MAKE_IOCTL(9)
#define IOCTL_SETCARDFEATURE_TST                MAKE_IOCTL(10)
#define IOCTL_RW_MISALIGN_OR_PARTIAL_TST        MAKE_IOCTL(11)
#define IOCTL_SD_CARD_REMOVAL       MAKE_IOCTL(12)
#define IOCTL_GET_HOST_INTERFACE        MAKE_IOCTL(13)
#define IOCTL_GET_CARD_INTERFACE        MAKE_IOCTL(14)

#define IS_TST_IOCTL(Ioctl) ((Ioctl >= IOCTL_SAMPLE_TEST1) && (Ioctl <= IOCTL_SD_CARD_REMOVAL))

#endif //SDTST_IOCLS_H

