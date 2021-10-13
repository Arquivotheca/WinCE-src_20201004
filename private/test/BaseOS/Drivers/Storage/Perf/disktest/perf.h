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

#include <PERFLOGGERAPI.H>
#include <Sdcardddk.h>
#include <ceddk.h>

#define DISK_SIZE_PERC 95 //percentage that a disk can be away from the true MB advertised
DBGPARAM   dpCurSettings;
// define performance markers
//
#define MARK_TEST           0

enum {
    PERF_WRITE = 0,
    PERF_READ,
    PERF_SG_READ_1,
    PERF_SG_READ_2,
    PERF_SG_READ_3,
    PERF_SG_READ_4,
    PERF_SG_READ_5,
    PERF_SG_READ_6,
    PERF_LINEAR_FILL_W,
    PERF_LINEAR_FILL_R,
    PERF_LINEAR_FREE_W,
    PERF_LINEAR_FREE_R,
    PERF_RANDOM_FILL_W,
    PERF_RANDOM_FILL_R,
    PERF_RANDOM_FREE_W,
    PERF_RANDOM_FREE_R,
    PERF_FRAGMENTED_FILL_W,
    PERF_FRAGMENTED_FILL_R,
    PERF_FRAGMENTED_FREE_W,
    PERF_FRAGMENTED_FREE_R,
};


struct CMFGLookup
{
    UCHAR MFGID;
    LPTSTR str;
};

CMFGLookup mfgTable[] =
{
    {0x1, TEXT("PQI")},
    {0x2, TEXT("Kingston")},
    {0x3, TEXT("Sandisk")},
    {0x12, TEXT("Infineon")},
    {0x25, TEXT("Kingmax")},
    {0x30, TEXT("ADATA")},
};
    
LPTSTR GetManufacturer(int code)
{
    int len;
    len = sizeof(mfgTable)/ sizeof (CMFGLookup);
    int c;
    for (c = 0; (c < len) && (code != mfgTable[c].MFGID); c++);
    if (c == len)
    {
        return lptStrUnknownManufacturer;
    }
    return mfgTable[c].str;
}

// Returns value in megabytes.
ULONGLONG GetDiskSize(ULONGLONG kiloBytes)
{
    ULONGLONG x = 1;
    ULONGLONG megaBytes = kiloBytes/1000;
    while(x < 1048576) //shouldn't approach a disk of this size
    {
        double diff = (double)x - ((double)x * DISK_SIZE_PERC / 100.0);
        if(megaBytes < x - diff || (megaBytes > x - diff && megaBytes < x + diff))
            return x;
        x = x << 1;
    }
    return 0;
}

#define CALIBRATION_COUNT 10
VOID
Calibrate(void)
  {
      for(int dwCount = 0; dwCount < CALIBRATION_COUNT; dwCount++)
    {
       Perf_MarkBegin(MARK_CALIBRATE);
       Perf_MarkEnd(MARK_CALIBRATE);
    }
  }
