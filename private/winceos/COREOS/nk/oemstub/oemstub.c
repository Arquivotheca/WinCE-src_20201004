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

//------------------------------------------------------------------------------
// Placeholder for when debugger is not present
//------------------------------------------------------------------------------
void 
FakeKDReboot(
    BOOL fReboot
    ) 
{
    return;
}
VOID  (*KDReboot)(BOOL fReboot) = FakeKDReboot;

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
    BOOL fRet = FALSE;
    
    if (IOCTL_HAL_REBOOT == code) {
        KDReboot(TRUE);     // Inform kdstub we are about to reboot
    }
        
    fRet = g_pOemGlobal->pfnOEMIoctl (code, pInBuffer, inSize, pOutBuffer, outSize, pOutSize);

    if (IOCTL_HAL_REBOOT == code) {
        KDReboot(FALSE);     // Inform kdstub we failed to reboot
    }
    
    DEBUGCHK(!fRet || (IOCTL_HAL_HALT != code));
    if (IOCTL_HAL_HALT == code) {
        RETAILMSG (1, (L"Halting system\r\n"));
        INTERRUPTS_OFF ();
        while (TRUE)
            ;
    }

    return fRet;    
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

BOOL HaveVfpHardware()
{
    return (g_pOemGlobal->dwPlatformFlags & OAL_HAVE_VFP_HARDWARE) ? TRUE : FALSE;
}

BOOL HaveVfpSupport()
{
    return (g_pOemGlobal->dwPlatformFlags & OAL_HAVE_VFP_SUPPORT) ? TRUE : FALSE;
}

BOOL HaveExtendedVfp()
{
    return (g_pOemGlobal->dwPlatformFlags & OAL_EXTENDED_VFP_REGISTERS) ? TRUE : FALSE;
}

#elif defined (MIPS)
//
// MIPS specific functions
//
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


