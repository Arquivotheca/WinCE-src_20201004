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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*
 *              NK Kernel loader code
 *
 *
 * Module Name:
 *
 *              oemstub.c
 *
 * Abstract:
 *
 *              This file implements the OEM routines to be used in kernel, and KITLDLL
 *
 */

#include "kernel.h"

POEMGLOBAL g_pOemGlobal;

void OEMInitDebugSerial (void)
{
    g_pOemGlobal->pfnInitDebugSerial ();
}

void OEMInit (void)
{
    g_pOemGlobal->pfnInitPlatform ();
}

void OEMWriteDebugByte (BYTE ucChar)
{
    g_pOemGlobal->pfnWriteDebugByte (ucChar);
}

void OEMWriteDebugString (unsigned short * str)
{
    g_pOemGlobal->pfnWriteDebugString (str);
}

void OEMWriteDebugLED (WORD wIndex, DWORD dwPattern)
{
    g_pOemGlobal->pfnWriteDebugLED (wIndex, dwPattern);
}

int OEMReadDebugByte (void)
{
    return g_pOemGlobal->pfnReadDebugByte ();
}

void OEMCacheRangeFlush (LPVOID pAddr, DWORD dwLength, DWORD dwFlags)
{
    g_pOemGlobal->pfnCacheRangeFlush (pAddr, dwLength, dwFlags);
}

void OEMInitClock (void)
{
    g_pOemGlobal->pfnInitClock ();
}

BOOL OEMGetRealTime (LPSYSTEMTIME pst)
{
    return g_pOemGlobal->pfnGetRealTime (pst);
}

BOOL OEMSetRealTime (LPSYSTEMTIME pst)
{
    return g_pOemGlobal->pfnSetRealTime (pst);
}

BOOL OEMSetAlarmTime (LPSYSTEMTIME pst)
{
    return g_pOemGlobal->pfnSetAlarmTime (pst);
}

DWORD OEMGetTickCount (void)
{
    return g_pOemGlobal->pfnGetTickCount ();
}

void OEMIdle (DWORD dwIdleParam)
{
    g_pOemGlobal->pfnIdle (dwIdleParam);
}

void OEMPowerOff (void)
{
    g_pOemGlobal->pfnPowerOff ();
}

BOOL OEMGetExtensionDRAM (LPDWORD pMemoryStart, LPDWORD pMemoryLength)
{
    return g_pOemGlobal->pfnGetExtensionDRAM (pMemoryStart, pMemoryLength);
}

void OEMInterruptDisable (DWORD sysIntr)
{
    g_pOemGlobal->pfnInterruptDisable (sysIntr);
}

BOOL OEMInterruptEnable (DWORD sysIntr, LPVOID pvData, DWORD cbData)
{
    return g_pOemGlobal->pfnInterruptEnable (sysIntr, pvData, cbData);
}

void OEMInterruptDone (DWORD sysIntr)
{
    g_pOemGlobal->pfnInterruptDone (sysIntr);
}

void OEMInterruptMask (DWORD dwSysIntr, BOOL fDisable)
{
    g_pOemGlobal->pfnInterruptMask (dwSysIntr, fDisable);
}

DWORD OEMNotifyIntrOccurs (DWORD dwSysIntr)
{
    return g_pOemGlobal->pfnNotifyIntrOccurs (dwSysIntr);
}

BOOL OEMIoControl (DWORD code, VOID * pInBuffer, DWORD inSize, VOID * pOutBuffer, DWORD outSize, DWORD * pOutSize)
{
    BOOL fRet = g_pOemGlobal->pfnOEMIoctl (code, pInBuffer, inSize, pOutBuffer, outSize, pOutSize);
    DEBUGCHK(!fRet || (IOCTL_HAL_HALT != code));
    if (IOCTL_HAL_HALT == code) {
        RETAILMSG (1, (L"Halting system\r\n"));
        INTERRUPTS_OFF ();
        while (TRUE)
            ;
    }

    return fRet;    
}

void OEMProfileTimerDisable(void)
{
    if (g_pOemGlobal->pfnProfileTimerDisable) {
        g_pOemGlobal->pfnProfileTimerDisable();
    }
}

void OEMProfileTimerEnable(DWORD dwUSec)
{
    if (g_pOemGlobal->pfnProfileTimerEnable && g_pOemGlobal->pfnProfileTimerDisable) {
        g_pOemGlobal->pfnProfileTimerEnable(dwUSec);
    } else {
        RETAILMSG(1, (TEXT("!! Profiling is not implemented in the OAL!\r\n")));
    }
}

#if defined (x86)
//
// x86 specific functions
//
void OEMNMIHandler (void)
{
    g_pOemGlobal->pfnNMIHandler ();
}

#elif defined (ARM)
//
// ARM specific functions
//
DWORD OEMInterruptHandler (DWORD dwEPC)
{
    return g_pOemGlobal->pfnInterruptHandler (dwEPC);
}

void OEMInterruptHandlerFIQ (void)
{
    g_pOemGlobal->pfnFIQHandler ();
}

#elif defined (MIPS)

DWORD OEMGetLastTLBIdx (void)
{
    return g_pOemGlobal->dwOEMTLBLastIdx;
}

DWORD OEMGetArchFlagOverride (void)
{
    return g_pOemGlobal->dwArchFlagOverride;
}

#elif defined (SHx)
//
// Shx specific functions
//
DWORD OEMNMIHandler (void)
{
    return g_pOemGlobal->pfnNMIHandler ();
}

#else

#pragma error("No CPU Defined")

#endif


