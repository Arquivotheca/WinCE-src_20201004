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

#include <osaxs_p.h>

PPROCESS
OsAxsHandleToProcess(HANDLE h)
{
    DEBUGGERMSG(OXZONE_HANDLE, (L"++OSAXSHandleToProcess h = 0x%08x\r\n", h));
    PPROCESS pProc = 0;
    PHDATA phdata = h2pHDATA(h, g_pprcNK->phndtbl);
    DEBUGGERMSG(OXZONE_HANDLE, (L"  OSAXSHandleToProcess h = 0x%08x -> HDATA 0x%08x\r\n", h, phdata));
    if (phdata)
    {
        pProc = GetProcPtr(phdata);
    }
    DEBUGGERMSG(OXZONE_HANDLE, (L"--OSAXSHandleToProcess returning PROCESS 0x%08x\r\n", pProc));
    return pProc;
}


PTHREAD
OsAxsHandleToThread(HANDLE h)
{
    PTHREAD pthd = 0;
    PHDATA phdata = h2pHDATA(h, g_pprcNK->phndtbl);
    if (phdata)
    {
        pthd = GetThreadPtr(phdata);
    }
    return pthd;
}


PCCINFO
OsAxsHandleToPCInfo(HANDLE h, DWORD dwProcessHandle)
{
    DEBUGGERMSG(OXZONE_HANDLE, (L"++OSAXSHandleToPCInfo h = 0x%08x, p = 0x%8.8lx\r\n", h, dwProcessHandle));

    PCCINFO pcinfo = NULL;
    PHDATA phdata = OsAxsHandleToPHData(h, dwProcessHandle);
    if (phdata)
    {
        pcinfo = phdata->pci;
    }
    else
    {
        DEBUGGERMSG(OXZONE_HANDLE, (L"  OSAXSHandleToPCInfo h=%08x, p=%08x, failed to get PHDATA\r\n", h, dwProcessHandle));
    }
    DEBUGGERMSG(OXZONE_HANDLE, (L"--OSAXSHandleToPCInfo returning PCINFO 0x%08x\r\n", pcinfo));
    return pcinfo;
}


PHDATA
OsAxsHandleToPHData(HANDLE h, DWORD dwProcessHandle)
{
    DEBUGGERMSG(OXZONE_HANDLE, (L"++OSAXSHandleToPHData h = 0x%08x, p = 0x%8.8lx\r\n", h, dwProcessHandle));
    PPROCESS pProc = 0;
    PHNDLTABLE phdtbl = 0;
    PHDATA phdata = 0;
    if (0!=dwProcessHandle)
    {
        pProc = OsAxsHandleToProcess((HANDLE)dwProcessHandle);
        if (pProc)
            phdtbl = pProc->phndtbl;
        else
        {
            DEBUGGERMSG(OXZONE_HANDLE, (L"  OSAXSHandleToPHData: invalid process handle %08X\r\n", dwProcessHandle));
        }
        
    }
    else
    {
         phdtbl = pActvProc->phndtbl;
    }

    if (phdtbl)
    {
        phdata = h2pHDATA(h, phdtbl);
        if (!phdata)
        {
            DEBUGGERMSG(OXZONE_HANDLE, (L"  OSAXSHandleToPHData: no phdata %08X, %08X\r\n", h, dwProcessHandle));
        }
    }
    DEBUGGERMSG(OXZONE_HANDLE, (L"--OSAXSHandleToPHData returning PHDATA 0x%08x\r\n", phdata));
    return phdata;
}
