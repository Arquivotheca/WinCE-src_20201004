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
//
// Define the compiler feature absolute symbol items
//  Depends upon winnt.h being loaded for symbol attributes
//
#pragma once

enum {
    cfSafeExceptions = 0x00000001,
    cfPureMSIL       = 0x00000002,
    cfSafeMSIL       = 0x00000004,
    cfWinRTMeta      = 0x00000008,
    // Build metadata reporting:
    //    3-bit field to reflect level of reporting: Legacy/Unknown (0), Dev11 or above (1), Future use (2-7)
    //    1 bit for C/C++ module (versus bit not set for MASM/CVTRES)
    //    1 bit for  /GS enabled/disabled
    //    1 bit for /sdl enabled/disabled
    cfReportingLevel_Dev11 = 0x00000010, // eg Level 1 == Dev11 compiler supports /GS and /sdl reporting 
    cfReportingLevelMask   = 0x00000070, 
    cfModuleIsC_CPP  = 0x00000080, 
    cfGSEnabled      = 0x00000100, // /GS specified  (=> linker will generate image debug directory to reflect whether all security-aware modules opt in to default /GS security features)
    cfSdlEnabled     = 0x00000200, // /sdl specified (=> linker will generate image debug directory to reflect whether all security-aware modules opt in to default /sdl security features)
    cfKernelMode     = 0x40000000,
    cfKernelAware    = 0x80000000,
};

#define symCompFeatIdentName        "@feat.00"
#define symCompFeatIdentShortName   '@','f','e','a','t','.','0','0'
#define symCompFeatIdentClass       IMAGE_SYM_CLASS_STATIC
#define symCompFeatIdentSection     IMAGE_SYM_ABSOLUTE

// 
// The last argument in IObjFileForFile2() from interface IObjHandler5
// is a pointer to DWORD, by setting which c2 can pass back attributes
// of a module.  The following enumeration defines what info is carried
// by which bit.
//

enum {
    MOD_ATTRIBUTE_KERNEL = 0x00000001,
    MOD_ATTRIBUTE_GS     = 0x00000002,
    MOD_ATTRIBUTE_SDL    = 0x00000004,
};

