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

/**************************************************************************


Module Name:

   doc.cpp 

Abstract:

   Gdi Tests: Printing API's

***************************************************************************/

#include "global.h"

void
passNull2Gdi(int testFunc)
{
    info(ECHO, TEXT("%s - passNull2Gdi"), funcName[testFunc]);

    DEVMODE dm;
    dm.dmSize = sizeof(DEVMODE);
#ifdef UNDER_CE
    dm.dmFields = DM_DISPLAYQUERYORIENTATION;
#endif

    switch(testFunc)
    {
        case EChangeDisplaySettingsEx:
            CheckForRESULT(ERROR_SUCCESS, ChangeDisplaySettingsEx(NULL, NULL, NULL, CDS_RESET, NULL));
            CheckForRESULT(DISP_CHANGE_BADFLAGS, ChangeDisplaySettingsEx(NULL, &dm, NULL, 0xBAD, NULL));
#ifdef UNDER_CE
            CheckForRESULT(DISP_CHANGE_BADPARAM, ChangeDisplaySettingsEx(NULL, &dm, NULL, CDS_VIDEOPARAMETERS, NULL));
#endif
            dm.dmSize = 0;
            CheckForRESULT(DISP_CHANGE_BADMODE, ChangeDisplaySettingsEx(NULL, &dm, NULL, CDS_TEST, NULL));
            break;
    }
}

/***********************************************************************************
***
***   Check Speed
***
************************************************************************************/

//***********************************************************************************
void
CheckSpeed(int testFunc)
{
#ifndef UNDER_CE
    info(ECHO, TEXT("%s - Stop Watch Check"), funcName[testFunc]);

    int     x;
    DWORD   start,
            end;
    float   totalTime,
            performNT,
            percent;

    // start stop watch
    start = GetTickCount();

    // look at speed of batch functions
    switch (testFunc)
    {
        case EGdiSetBatchLimit:
            for (x = 0; x < 1000000; x++)
                GdiSetBatchLimit(x);
            performNT = (float) 1450000;    // NT calls/sec
            break;
        case EGdiFlush:
            for (x = 0; x < 1000000; x++)
                GdiFlush();
            performNT = (float) 300000; // NT calls/sec
            break;
    }

    // stop stop watch
    end = GetTickCount();
    totalTime = (float) ((float) end - (float) start) / 1000;
    percent = (float) 100 *(((float) x / totalTime) / performNT);

    if (percent < (float) 50)
        info(FAIL, TEXT("%.2f percent of NT Performance: %d calls took secs:%.2f, average calls/sec:%.0f"), percent, x,
             totalTime, (float) x / totalTime);
    else
        info(ECHO, TEXT("%.2f percent NT Performance: %d calls took secs:%.2f, average calls/sec:%.0f"), percent, x, totalTime,
             (float) x / totalTime);
#endif // UNDER_CE
}

//***********************************************************************************
TESTPROCAPI GdiFlush_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

#ifdef UNDER_CE
    info(ECHO, TEXT("Currently not implented in Windows CE, placeholder for future test development."));
#else
    // breadth
    CheckSpeed(EGdiFlush);

    // depth
    // none
#endif

    return getCode();
}

//***********************************************************************************
TESTPROCAPI GdiSetBatchLimit_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

#ifdef UNDER_CE
    info(ECHO, TEXT("Currently not implented in Windows CE, placeholder for future test development."));
#else
    // breadth
    CheckSpeed(EGdiSetBatchLimit);

    // depth
    // none
#endif

    return getCode();
}

//**********************************************************************************
TESTPROCAPI ChangeDisplaySettingsEx_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Breadth
    passNull2Gdi(EChangeDisplaySettingsEx);

    // Depth
    // None

    return getCode();
}

