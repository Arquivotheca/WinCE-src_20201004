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
/*++

Module Name:

    kdbgerrors.h

Abstract:

    This header contains all the error codes returned by the kernel debugger 
    components (hd.dll, kd.dll, osaxst0.dll, osaxst1.dll).
    This file is shared between the device and desktop.

Environment:

    CE Kernel

--*/

#pragma once

#ifdef UNDER_CE
#include <WinCeErrPriv.h>
#endif

/* Extra HRESULTS that may be returned by HDSTUB */
#define HDSTUB_E_NOCLIENT MAKE_HRESULT(1, FACILITY_WINDOWS_CE, (HDSTUB_ERR_BASE+0))  /* This is returned when hdstub is unable to find a specific client */

// KD -> K,D -> 11,4 (A=1,B=2,...Z=26)
#define FACILITY_KDBG   ((11 << 5) | 4)

// General failure in Debugger.
#define KDBG_E_FAIL MAKE_HRESULT(1, FACILITY_KDBG, 500)

// General failure in Breakpoint
#define KDBG_E_BPFAIL MAKE_HRESULT (1, FACILITY_KDBG, 510)

// No more resources for Breakpoints.
#define KDBG_E_BPRESOURCE MAKE_HRESULT(1, FACILITY_KDBG, 511)

// Unable to set breakpoint because it'll be intrusive
#define KDBG_E_BPINTRUSIVE MAKE_HRESULT(1, FACILITY_KDBG, 512)

// Failure to write Breakpoint.  (Read back showed no change)
#define KDBG_E_BPWRITE MAKE_HRESULT(1, FACILITY_KDBG, 513)

// Unable to read.
#define KDBG_E_BPREAD MAKE_HRESULT(1, FACILITY_KDBG, 514)

// General failure in ROM Breakpoint
#define KDBG_E_ROMBPFAIL MAKE_HRESULT(1, FACILITY_KDBG, 520)

// No more resources for ROM Breakpoints
#define KDBG_E_ROMBPRESOURCE MAKE_HRESULT(1, FACILITY_KDBG, 521)

// Failed to emulate copy on write for rom bp.
#define KDBG_E_ROMBPCOPY MAKE_HRESULT(1, FACILITY_KDBG, 522)

// Attempt to do ROM BP in kernel denied.
#define KDBG_E_ROMBPKERNEL MAKE_HRESULT(1, FACILITY_KDBG, 523)

// Attempt to do ROM BP in kernel denied.
#define KDBG_E_BPNOTFOUND MAKE_HRESULT(1, FACILITY_KDBG, 524)

// An invalid process was specificed
#define KDBG_E_DBP_INVALIDPROCESS MAKE_HRESULT(1, FACILITY_KDBG, 525)

// Address crossed a page boundary.
#define KDBG_E_DBP_PAGEBOUNDARY MAKE_HRESULT(1, FACILITY_KDBG, 526)

// Out of resource
#define KDBG_E_DBP_RESOURCES MAKE_HRESULT(1, FACILITY_KDBG, 527)

// Invalid Address (uncommited)
#define KDBG_E_DBP_INVALID_ADDRESS MAKE_HRESULT(1, FACILITY_KDBG, 528)

// Address isn't writable
#define KDBG_E_DBP_PAGE_NOT_WRITABLE MAKE_HRESULT(1, FACILITY_KDBG, 529)

// Address is section mapped
#define KDBG_E_DBP_SECTION_MAPPED MAKE_HRESULT(1, FACILITY_KDBG, 530)

// Page has been remapped
#define KDBG_E_DBP_PAGE_REMAPPED MAKE_HRESULT(1, FACILITY_KDBG, 531)

//Out of hardware dbp's
#define KDBP_E_DBP_OUT_OF_HWDBP MAKE_HRESULT(1, FACILITY_KDBG, 532)

//Out of software dbp's
#define KDBP_E_DBP_OUT_OF_SWDBP MAKE_HRESULT(1, FACILITY_KDBG, 533) 

//platform does not support hardware dbp's
#define KDBP_E_DBP_HWDBP_UNSUPPORTED MAKE_HRESULT(1, FACILITY_KDBG, 534) 

//RW Software Data breakpoint on same page as TLS
#define KDBP_E_DBP_TLS MAKE_HRESULT(1, FACILITY_KDBG, 535) 

//Software data brekapoint on secure stack
#define KDBP_E_DBP_SECURE_STACK MAKE_HRESULT(1, FACILITY_KDBG, 536) 

// OS Access error codes
#define OSAXS_E_PROTOCOLVERSION     MAKE_HRESULT(1, FACILITY_WINDOWS_CE, (OSAXS_ERR_BASE+0))   // Notify OsAxs Kdbg code that the version is incorrect.
#define OSAXS_E_APINUMBER           MAKE_HRESULT(1, FACILITY_WINDOWS_CE, (OSAXS_ERR_BASE+1))   // Bad api number was passed to OS Access
#define OSAXS_E_VANOTMAPPED         MAKE_HRESULT(1, FACILITY_WINDOWS_CE, (OSAXS_ERR_BASE+2))   // Unable to find the Virtual address in the address table
#define OSAXS_E_INVALIDTHREAD       MAKE_HRESULT(1, FACILITY_WINDOWS_CE, (OSAXS_ERR_BASE+3))   // Bad thread pointer.

