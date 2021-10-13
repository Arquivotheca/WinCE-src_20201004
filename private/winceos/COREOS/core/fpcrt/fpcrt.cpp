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

#include <windows.h>
#include <corecrtstorage.h>
#include <cruntime.h>
#include <crttrans.h>


extern "C" {


unsigned int _vfpGetFPSID();
unsigned int _vfpGetFPSCR();
void _vfpSetFPSCR(unsigned int fpscr, unsigned int mask);


//------------------------------------------------------------------------------
// global flags indicating the presence of VFP support (read/only outside this file)
//------------------------------------------------------------------------------
BOOL g_haveVfpSupport = FALSE;
BOOL g_haveVfpHardware = FALSE;


//------------------------------------------------------------------------------
// Internal FPCRT control interface
//------------------------------------------------------------------------------

#define FPCRT_SET_VFP_SUPPORT       1
#define FPCRT_SET_VFP_HARDWARE      2
#define FPCRT_GET_VFP_SUPPORT       3
#define FPCRT_GET_VFP_HARDWARE      4
    
BOOL ControlFPCRT(DWORD dwCommand, void* ptr)
{
    switch(dwCommand)
    {
    case FPCRT_SET_VFP_SUPPORT:
        g_haveVfpSupport = *reinterpret_cast<BOOL*>(ptr);
        return TRUE;
    
    case FPCRT_SET_VFP_HARDWARE:
        g_haveVfpHardware = *reinterpret_cast<BOOL*>(ptr);
        return TRUE;
    
    case FPCRT_GET_VFP_SUPPORT:
        *reinterpret_cast<BOOL*>(ptr) = g_haveVfpSupport;
        return TRUE;
    
    case FPCRT_GET_VFP_HARDWARE:
        *reinterpret_cast<BOOL*>(ptr) = g_haveVfpHardware;
        return TRUE;
    }
    
    return FALSE;
}


//------------------------------------------------------------------------------
// Auto detection for VFP support (HW or emulation).
//------------------------------------------------------------------------------
static
void AutoDetectVfp()
{
    g_haveVfpSupport = IsProcessorFeaturePresent(PF_ARM_VFP_SUPPORT);
    g_haveVfpHardware = IsProcessorFeaturePresent(PF_ARM_VFP_HARDWARE);
}


//------------------------------------------------------------------------------
// FPCRT entry point
//------------------------------------------------------------------------------
BOOL WINAPI DllMain(HANDLE hdll, DWORD dwReason, void*)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls((HMODULE)hdll);
        __security_init_cookie();
        AutoDetectVfp();
        break;

    case DLL_PROCESS_DETACH:
        break;

    default:
        break;
    }
    
    return TRUE;
}


//------------------------------------------------------------------------------
// Floating point library state access
//------------------------------------------------------------------------------
DWORD* __crt_get_storage_fsr()
{
    return &GetCRTStorage()->fsr;
}


//------------------------------------------------------------------------------
// Kernel flags access
//------------------------------------------------------------------------------
DWORD* __crt_get_kernel_flags()
{
    return &UTlsPtr()[TLSSLOT_KERNEL];
}


//------------------------------------------------------------------------------
// abstract the access to the floating point control/status word (get)
//
// NOTE: if we have VFP support then we use the FPSCR bits
//------------------------------------------------------------------------------
unsigned int _get_fsr()
{
    // if we have VFP support then use FPSCR
    //
    return g_haveVfpSupport ?
        _fpscr2cw(_vfpGetFPSCR()) :
        *__crt_get_storage_fsr();
}


//------------------------------------------------------------------------------
// abstract the access to the floating point control/status word (set)
//
// NOTE: if we have VFP support then we sych the FPSCR bits too
//------------------------------------------------------------------------------
void _set_fsr(unsigned int fsr)
{
    // if we have VFP support then update FPSCR
    // 
    // NOTE: not every bit in FPSCR can be computed from the control word, preserve
    //    the extra FPSCR bits
    //
    if(g_haveVfpSupport)
    {
        _vfpSetFPSCR(_cw2fpscr(fsr), FPSCR_STATUS_MASK | FPSCR_CW_MASK | FPSCR_ROUND_MASK);
    }
    else
    {
        *__crt_get_storage_fsr() = fsr;
    }
}


//------------------------------------------------------------------------------
// References to RaiseException need to be redirected to COREDLL
//------------------------------------------------------------------------------
void __crtRaiseException(
    DWORD dwExceptionCode,
    DWORD dwExceptionFlags,
    DWORD nNumberOfArguments,
    CONST DWORD *lpArguments)
{
    RaiseException(dwExceptionCode, dwExceptionFlags, nNumberOfArguments, lpArguments);
}


} // extern "C"



