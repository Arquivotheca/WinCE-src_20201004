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

    cpuid.h

Abstract:

    Private include file for the Kernel Debugger subcomponent

Environment:

    WinCE


--*/


#pragma once


#define PROCESSOR_FAMILY_X86 (0)
#define PROCESSOR_FAMILY_SH3 (1)
#define PROCESSOR_FAMILY_SH4 (2)
#define PROCESSOR_FAMILY_MIPS (3)
#define PROCESSOR_FAMILY_ARM (4)
#define PROCESSOR_FAMILY_PPC (5)
#define PROCESSOR_FAMILY_SHX (6) // Not to be used by KdStub (reserved by DM)
#define PROCESSOR_FAMILY_MIPS4 (7) // For KdStub usage only - required for KdStubeXDI1
#define PROCESSOR_FAMILY_UNK (0xFFFFFFFF)


#if defined(SH3)
#define TARGET_CODE_CPU (PROCESSOR_FAMILY_SH3)
#elif defined(SH4)
#define TARGET_CODE_CPU (PROCESSOR_FAMILY_SH4)
#elif defined(x86)
#define TARGET_CODE_CPU (PROCESSOR_FAMILY_X86)
#elif defined(MIPS)
#if defined(MIPSIV)
#define TARGET_CODE_CPU (PROCESSOR_FAMILY_MIPS4)
#else
#define TARGET_CODE_CPU (PROCESSOR_FAMILY_MIPS)
#endif
#elif defined(ARM)
#define TARGET_CODE_CPU (PROCESSOR_FAMILY_ARM)
#else
#define TARGET_CODE_CPU (PROCESSOR_FAMILY_UNK)
#endif


