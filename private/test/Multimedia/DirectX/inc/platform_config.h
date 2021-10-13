//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

// ========================================================================
//
// Compiler flags: 
// ————————————————————————————————————————————————————————————————————————
// __CFG_NAMESPACE_EXTERN_OBJ_DEF_BUG : indicates that the compiler has a
//     (rather common) bug when defining an object that was previously
//     declared as an extern member of a namespace.
// __CFG_COMPILER_BUG_1753 : indicates that the compiler has a rather nasty
//     bug when using namespace embedded derived classes.  The derived class
//     cannot see the base class' methods.
// __CFG_MEMBER_TEMPLATES : indicates that the compiler supports 
//     the use of templated members
// __CFG_LIMITED_DEFAULT_TEMPLATES : indicates that the compiler has limited 
//     ability to declare template parameters defaults, usually
//     when the default value uses a previous template parameter
// __CFG_TEMPLATE_SPECIALIZATION_SYNTAX : indicates that the compiler supports 
//     template specialization
// __CFG_PARTIAL_SPECIALIZATION_SYNTAX : indicates that the compiler supports
//     the standard C++ partial specialization syntax.
// __CFG_USE_EXCEPTIONS : indicates the compiler supports C++ exceptions
// __CFG_COMPILER_BUG_1565 : same as __CFG_COMPILER_BUG_1753
// EXPOSE_STL_WARNINGS : #define this macro to omit specific warning 
//     suppressions relevant to the ported STL
// __CFG_NO_IOSTREAMS : the current platform does not support iostreaming.
// ========================================================================
//
// Platform macros 
// ————————————————————————————————————————————————————————————————————————
// CFG_PLATFORM_NAME : Name of current platform
//
// CFG_PLATFORM_DESKTOP : Windows Desktop platform ver #
// CFG_PLATFORM_DXPACK : CE DirectX Pack platform ver #
//
// ========================================================================

#pragma once

#ifndef __PLATFORM_CONFIG_H__
#define __PLATFORM_CONFIG_H__

// =================================
// C o m p i l e r   S e t t i n g s
// =================================

// MIPS Compiler Settings
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#if defined(UNDER_CE) && defined(MIPS)
    // MIPS compiler
#   define __CFG_NAMESPACE_EXTERN_OBJ_DEF_BUG
#   ifndef EXPOSE_STL_WARNINGS
#       pragma warning (disable : 4804 4018 4786)
#   endif
#   define __CFG_TEMPLATE_SPECIALIZATION_SYNTAX
#   define __CFG_NAMESPACE_EXTERN_OBJ_DEF_BUG
#   define __CFG_MEMBER_TEMPLATES
#   define __CFG_NO_IOSTREAMS
#   define __CFG_COMPILER_BUG_1753
#pragma message("MIPS Compiler")
#endif

// ARM Compiler Settings
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#if defined(UNDER_CE) && defined(ARM)
    // ARM compiler
#   define __CFG_NAMESPACE_EXTERN_OBJ_DEF_BUG
#   ifndef EXPOSE_STL_WARNINGS
#       pragma warning (disable : 4804 4018 4786)
#   endif
#   define __CFG_TEMPLATE_SPECIALIZATION_SYNTAX
#   define __CFG_NAMESPACE_EXTERN_OBJ_DEF_BUG
#   define __CFG_MEMBER_TEMPLATES
#   define __CFG_NO_IOSTREAMS
#   define __CFG_COMPILER_BUG_1753
#pragma message("ARM Compiler")
#endif

// SH3 Compiler Settings
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#if defined(UNDER_CE) && defined(SH3)
    // SH3 compiler
#   define __CFG_NAMESPACE_EXTERN_OBJ_DEF_BUG
#   ifndef EXPOSE_STL_WARNINGS
#       pragma warning (disable : 4804 4018 4786)
#   endif
#   define __CFG_USE_EXCEPTIONS
#   define __CFG_TEMPLATE_SPECIALIZATION_SYNTAX
#   define __CFG_NAMESPACE_EXTERN_OBJ_DEF_BUG
#   define __CFG_MEMBER_TEMPLATES
#   define __CFG_NO_IOSTREAMS
#   define __CFG_COMPILER_BUG_1753
#pragma message("SH3 Compiler")
#endif

// SH4 Compiler Settings
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#if defined(UNDER_CE) && defined(SH4)
    // SH4 compiler
#   define __CFG_NAMESPACE_EXTERN_OBJ_DEF_BUG
#   ifndef EXPOSE_STL_WARNINGS
#       pragma warning (disable : 4804 4018 4786)
#   endif
#   define __CFG_USE_EXCEPTIONS
#   define __CFG_TEMPLATE_SPECIALIZATION_SYNTAX
#   define __CFG_NAMESPACE_EXTERN_OBJ_DEF_BUG
#   define __CFG_MEMBER_TEMPLATES
#   define __CFG_NO_IOSTREAMS
#   define __CFG_COMPILER_BUG_1753
#pragma message("SH4 Compiler")
#endif

// x86 CE Compiler Settings
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#if defined(UNDER_CE) && defined(x86)
    // CE x86 compiler
#   define __CFG_COMPILER_BUG_1753
#pragma message("x86 Compiler")
#endif

// x86 Desktop Compiler Settings
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#if !defined(UNDER_CE) && defined(x86)
    // Desktop x86 compiler
#   define __CFG_COMPILER_BUG_1753
#pragma message("x86 Compiler")
#endif

// __CFG_COMPILER_BUG_1565 is the same as __CFG_COMPILER_BUG_1753 (two different DB)
#ifdef __CFG_COMPILER_BUG_1753
#   define __CFG_COMPILER_BUG_1565
#endif

// =================================
// P l a t f o r m   S e t t i n g s 
// =================================

// DX For WindowsCE PC Settings
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#if !defined(CFG_PLATFORM_NAME) && (defined(UNDER_CE))
#   define CFG_PLATFORM_DX 100
#   define CFG_PLATFORM_DX_CEPC (defined(x86))
#   define CFG_PLATFORM_DX_ALTOONA (defined(MIPS))
#   define CFG_PLATFORM_NAME "DXPAK"
#else
#   define CFG_PLATFORM_DX 0
#   define CFG_PLATFORM_DX_CEPC 0
#   define CFG_PLATFORM_DX_ALTOONA 0
#   define CFG_PLATFORM_DX_DCT5000 0
#endif

#if !defined(CFG_PLATFORM_NAME) 
#   define CFG_PLATFORM_DESKTOP 500
#   define CFG_PLATFORM_NAME "MS Windows2000"
#else
#   define CFG_PLATFORM_DESKTOP 0
#endif

#if defined(x86)
#   define CFG_PROCESSOR_NAME "x86"
#elif defined(MIPS)
#   define CFG_PROCESSOR_NAME "MIPS"
#elif defined(SH3)
#   define CFG_PROCESSOR_NAME "SH3"
#elif defined(SH4)
#   define CFG_PROCESSOR_NAME "SH4"
#else
#   define CFG_PROCESSOR_NAME "Unknown"
#endif


#pragma message("Targeting " CFG_PLATFORM_NAME)

#endif // __PLATFORM_CONFIG_H__