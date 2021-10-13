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
#include "ciq_hlp.h"
#include <SDCardDDK.h>
#include <sdcard.h>
#include <sd_tst_cmn.h>
#include <sddrv.h>
#include <sddetect.h>

float DecipherMinCurrent(UCHAR val)
{
    float uf = 0;
    switch (val)
    {
        case 0:
            uf = 0.5;
            break;
        case 1:
            uf = 1;
            break;
        case 2:
            uf = 5;
            break;
        case 3:
            uf = 10;
            break;
        case 4:
            uf = 25;
            break;
        case 5:
            uf = 35;
            break;
        case 6:
            uf = 60;
            break;
        case 7:
            uf = 100;
    }
    return uf;
}

USHORT DecipherMaxCurrent(UCHAR val)
{
    USHORT us = 0;
    switch (val)
    {
        case 0:
            us = 1;
            break;
        case 1:
            us = 5;
            break;
        case 2:
            us = 10;
            break;
        case 3:
            us = 25;
            break;
        case 4:
            us = 35;
            break;
        case 5:
            us = 45;
            break;
        case 6:
            us = 80;
            break;
        case 7:
            us = 200;
    }
    return us;
}

LPTSTR DecipherFlagsB(FT_TYPE ft, BOOLEAN b)
{
    static TCHAR tc[40];
    ZeroMemory(tc, sizeof(TCHAR) *40);
    switch (ft)
    {
        case FT_COPY:
            if (b) StringCchCopy(tc, _countof(tc), TEXT("Copy"));
            else  StringCchCopy(tc, _countof(tc), TEXT("Original"));
            break;
        case FT_PERM_WP:
            if (b) StringCchCopy(tc, _countof(tc), TEXT("Permanently Write Protected"));
            else  StringCchCopy(tc, _countof(tc), TEXT("Not Permanently Write Protected"));
            break;
        case FT_TEMP_WP:
            if (b) StringCchCopy(tc, _countof(tc), TEXT("Temporarily Write Protected"));
            else  StringCchCopy(tc, _countof(tc), TEXT("Not Temporarily Write Protected"));
            break;

    }
    return tc;
}

LPTSTR DecipherFlagsU(FT_TYPE ft, UCHAR uc)
{
    static TCHAR tc[40];
    ZeroMemory(tc, sizeof(TCHAR) *40);
    switch (ft)
    {
        case FT_COPY:
            if ((uc &  0x40) != 0) StringCchCopy(tc, _countof(tc), TEXT("Copy"));
            else  StringCchCopy(tc, _countof(tc), TEXT("Original"));
            break;
        case FT_PERM_WP:
            if ((uc &  0x20) != 0) StringCchCopy(tc, _countof(tc), TEXT("Permanently Write Protected"));
            else StringCchCopy(tc, _countof(tc), TEXT("Not Permanently Write Protected"));
            break;
        case FT_TEMP_WP:
            if ((uc &  0x10) != 0) StringCchCopy(tc, _countof(tc), TEXT("Temporarily Write Protected"));
            else  StringCchCopy(tc, _countof(tc), TEXT("Not Temporarily Write Protected"));
            break;

    }
    return tc;
}

LPTSTR fsDecipherFS(SD_FS_TYPE fs)
{
    static TCHAR tc[40];
    ZeroMemory(tc, sizeof(TCHAR) *40);
    switch (fs)
    {
        case SD_FS_FAT_PARTITION_TABLE:
            StringCchCopy(tc, _countof(tc), TEXT("FAT with Partition Table"));
            break;
        case SD_FS_FAT_NO_PARTITION_TABLE:
            StringCchCopy(tc, _countof(tc), TEXT("FAT without Partition Table"));
            break;
        case SD_FS_UNIVERSAL:
            StringCchCopy(tc, _countof(tc), TEXT("Universal"));
            break;
        case SD_FS_OTHER:
            StringCchCopy(tc, _countof(tc), TEXT("Other"));

    }
    return tc;
}

LPTSTR ucDecipherFS(UCHAR uc)
{
    static TCHAR tc[40];
    ZeroMemory(tc, sizeof(TCHAR) *40);
    UCHAR grp, ff;
    grp = uc & 0x80;
    ff = (uc & 0xC) >>2;
    if (grp != 0)
    {
        StringCchCopy(tc, _countof(tc), TEXT("Bad FS"));
    }
    else
    {
        switch (ff)
        {
            case 0:
                StringCchCopy(tc, _countof(tc), TEXT("FAT with Partition Table"));
                break;
            case 1:
                StringCchCopy(tc, _countof(tc), TEXT("FAT without Partition Table"));
                break;
            case 2:
                StringCchCopy(tc, _countof(tc), TEXT("Universal"));
                break;
            case 3:
                StringCchCopy(tc, _countof(tc), TEXT("Other"));
        }
    }
    return tc;
}

DOUBLE GenerateVal(UCHAR uc)
{
    DOUBLE dVal = 0.0;
    switch (uc)
    {
        case 0x0:
            dVal = 0.0; //Bad.
            break;
        case 0x1:
            dVal = 1;
            break;
        case 0x2:
            dVal = 1.2;
            break;
        case 0x3:
            dVal = 1.3;
            break;
        case 0x4:
            dVal = 1.5;
            break;
        case 0x5:
            dVal = 2.0;
            break;
        case 0x6:
            dVal = 2.5;
            break;
        case 0x7:
            dVal = 3.0;
            break;
        case 0x8:
            dVal = 3.5;
            break;
        case 0x9:
            dVal = 4.0;
            break;
        case 0xA:
            dVal = 4.5;
            break;
        case 0xB:
            dVal = 5.0;
            break;
        case 0xC:
            dVal = 5.5;
            break;
        case 0xD:
            dVal = 6.0;
            break;
        case 0xE:
            dVal = 7.0;
            break;
        case 0xF:
            dVal = 8.0;
    }
    return dVal;
}

DOUBLE DecipherTAAC(UCHAR uc)
{
    DOUBLE dVal = 0, dUnit = 0;
    UCHAR ucKey = 0;

    ucKey = (uc & 0x78) >> 3;

    dUnit =  pow(10.0, ((DOUBLE)(uc & 0x7)));
    dVal = GenerateVal (ucKey);

    return dVal * dUnit;
}

ULONG DecipherTRAN_SPEED(UCHAR uc)
{
    DOUBLE dVal = 0;
    ULONG ulRet = 0;
    DOUBLE dUnit = 0;
    UCHAR ucKey = 0;

    ucKey = (uc & 0x78) >> 3;

    dUnit =  pow(10.0, ((DOUBLE)(uc & 0x7))) * 100.0;
    if (dUnit > 100000) dUnit = 0.0;
    dVal = GenerateVal (ucKey);
    ulRet = (ULONG)(dVal * dUnit);
    return ulRet;
}

ULONGLONG CalculateCapacity(UCHAR const *buff, PTEST_PARAMS pTestParams)
{
    ULONG ulBlockLen = 0, ulC_SIZE = 0, ulC_SIZE_MULT = 0, ulMULT = 0, ulBLOCKNR = 0;
    ulBlockLen = (ULONG)pow (2,(buff[10] & 0xf));

    if (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType)) {
    // for 2.0 spec, device size in [69:48]
    // buff[8]- bit 64-71
    // buff[7]- bit 56-63
    // buff[6]- bit 48-55
        ulC_SIZE = (buff[8]<<16) + (buff[7]<<8) + (buff[6]);
        ulC_SIZE = ulC_SIZE & 0x3FFFFF;
        ulBLOCKNR = (ulC_SIZE + 1) * 0x400;
    }
    else {
    // for 1.1 spec, device size in [73:62]
    // buff[9]- bit 72-80
    // buff[8]- bit 64-71
    // buff[7]- bit 56-63
        ulC_SIZE = (buff[9]<<16) + (buff[8]<<8) + buff[7];
        ulC_SIZE = ulC_SIZE & 0x3ffc0;  // mask 111111111111000000
        ulC_SIZE = ulC_SIZE >> 6;

    // for 1.1 spec, device size multiplier [49:47]
        ulC_SIZE_MULT = (buff[6] <<8) + buff[5];
        ulC_SIZE_MULT = ulC_SIZE_MULT & 0x380;
        ulC_SIZE_MULT = ulC_SIZE_MULT >>7;
        ulMULT = (ULONG)pow (2,(ulC_SIZE_MULT + 2));
        ulBLOCKNR = (ulC_SIZE + 1) * ulMULT;
    }
    return (ULONGLONG) ulBLOCKNR * ulBlockLen;
}

