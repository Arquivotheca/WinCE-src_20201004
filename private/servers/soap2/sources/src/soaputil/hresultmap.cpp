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
//+----------------------------------------------------------------------------
//
//
// File:
//      HResultMap.cpp
//
// Contents:
//
//      Implementation of error mapping mechanism
//
//-----------------------------------------------------------------------------

#include "Headers.h"

#ifdef _DEBUG
//NOTE: this is only in debug code
static void VerifySortedMap(HResultMap *pMap)
{
    if (pMap == 0 || pMap->elc == 0 || pMap->elv == 0)
        return;

    HRESULT hr = pMap->elv[0].hrFrom;

    for (DWORD i = 1; i < pMap->elc; i ++)
    {
        HResultMapEntry *pEntry = pMap->elv + i;

        if (pEntry->hrFrom <= hr)
        {
#ifndef UNDER_CE
            CHAR buffer[1024] = { 0 };
            sprintf(buffer, "Unsorted error mapping table in file \"%s\", line \"%i\"", pMap->file, pEntry->line);

            int mb = ::MessageBoxA(0, buffer, "Assertion failed", MB_SERVICE_NOTIFICATION | MB_ICONERROR | MB_ABORTRETRYIGNORE);
         
            if (mb == IDABORT)
                ::ExitProcess(-1);
            else if (mb == IDRETRY)
                _DebugBreak();
#else
        ASSERT(FALSE);
#endif
           
        }

        hr  = pEntry->hrFrom;
    }
}

#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT HResultToHResult(HResultMap *pMap, HRESULT hrFrom)
//
//  parameters:
//
//  description:
//        Maps HRESULT into another HRESULT based on passed-in HResultMap
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT HResultToHResult(HResultMap *pMap, HRESULT hrFrom)
{
    ASSERT(pMap != 0);

#ifdef _DEBUG
    VerifySortedMap(pMap);
#endif

    HRESULT hrTo = pMap->dflt;

    if (pMap->elv != 0)
    {
        int i = 0;
        int j = pMap->elc - 1;

        while (i <= j)
        {
            int k = (i + j) >> 1;
            HResultMapEntry *pEntry = pMap->elv + k;

            if (pEntry->hrFrom == hrFrom)
            {
                hrTo = pEntry->hrTo;
                break;
            }
            else if (pEntry->hrFrom < hrFrom)
            {
                i = k + 1;
            }
            else
            {
                j = k - 1;
            }
        }
    }

    return hrTo;
};

