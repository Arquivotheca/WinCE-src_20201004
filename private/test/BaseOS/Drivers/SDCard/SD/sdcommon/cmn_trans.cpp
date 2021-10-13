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
#include <windows.h>
#include "sdtst_ioctls.h"

LPTSTR lptstrUIOCTL = TEXT("Unknown IOCTL");
LPTSTR lptstrCRDErr = TEXT("Bad Card Type");

struct IOCTLLookup
{
    DWORD IOCTL;
    LPTSTR str;
};

IOCTLLookup ilTable[] =
{
    {IOCTL_SAMPLE_TEST1, TEXT("IOCTL_SAMPLE_TEST1")},
    {IOCTL_SAMPLE_TEST2, TEXT("IOCTL_SAMPLE_TEST2")},
    {IOCTL_SIMPLE_BUSREQ_TST, TEXT("IOCTL_SIMPLE_BUSREQ_TST")},
    {IOCTL_SIMPLE_INFO_QUERY_TST, TEXT("IOCTL_SIMPLE_INFO_QUERY_TST")},
    {IOCTL_CANCEL_BUSREQ_TST, TEXT("IOCTL_CANCEL_BUSREQ_TST")},
    {IOCTL_RETRY_ACMD_BUSREQ_TST, TEXT("IOCTL_RETRY_ACMD_BUSREQ_TST")},
    {IOCTL_INIT_TST, TEXT("IOCTL_INIT_TST")},
    {IOCTL_GET_BIT_SLICE_TST, TEXT("IOCTL_GET_BIT_SLICE_TST")},
    {IOCTL_OUTPUT_BUFFER_TST, TEXT("IOCTL_OUTPUT_BUFFER_TST")},
    {IOCTL_MEMLIST_TST, TEXT("IOCTL_MEMLIST_TST")},
    {IOCTL_SETCARDFEATURE_TST, TEXT("IOCTL_SETCARDFEATURE_TST")},
    {IOCTL_RW_MISALIGN_OR_PARTIAL_TST, TEXT("IOCTL_RW_MISALIGN_OR_PARTIAL_TST")},
    {IOCTL_SD_CARD_REMOVAL, TEXT("IOCTL_SD_CARD_REMOVAL")},
    {IOCTL_GET_HOST_INTERFACE, TEXT("IOCTL_GET_HOST_INTERFACE")},
    {IOCTL_GET_CARD_INTERFACE, TEXT("IOCTL_GET_CARD_INTERFACE")}
};

LPCTSTR TranslateIOCTLS(DWORD ioctl)
{
    int len;
    len = sizeof(ilTable)/ sizeof (IOCTLLookup);
    int c;
    for (c = 0; (c < len) && (ioctl != ilTable[c].IOCTL); c++);
    if (c == len)
    {
        return lptstrUIOCTL;
    }
    return ilTable[c].str;
}

struct CardTypeLookup
{
    SDCARD_DEVICE_TYPE dt;
    LPTSTR str;
};

CardTypeLookup ctTable[] =
{
    {Device_Unknown, TEXT("Device_Unknown")},
    {Device_MMC, TEXT("Device_MMC")},
    {Device_SD_Memory, TEXT("Device_SD_Memory")},
    {Device_SD_IO, TEXT("Device_SD_IO")},
    {Device_SD_Combo, TEXT("Device_SD_Combo")}
};

LPCTSTR TranslateCardType(SDCARD_DEVICE_TYPE sdcDevType)
{
    int len;
    len = sizeof(ctTable)/ sizeof (CardTypeLookup);
    int c;
    for (c = 0; (c < len) && (sdcDevType != ctTable[c].dt); c++);
    if (c == len)
    {
        return lptstrCRDErr;
    }
    return ctTable[c].str;
}
