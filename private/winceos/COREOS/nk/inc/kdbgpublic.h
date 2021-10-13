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


#pragma once

#ifndef INCLUDED_KDBGPUBLIC_H
#define INCLUDED_KDBGPUBLIC_H 1

#include "hdstub_common.h"

typedef BOOL (*KDBG_DLLINITFN)(struct DEBUGGER_DATA*, void*);

// public interface between kernel debugger components and the kernel

typedef struct KDBG_INIT
{
    DWORD sig0;
    DWORD sig1;
    DWORD sig2;

#include "kdbgimports.h"
#include "kdbgexports.h"
    
} KDBG_INIT;

enum
{
    KDBG_INIT_SIG0 = sizeof(KDBG_INIT),
    KDBG_INIT_SIG1 = offsetof(KDBG_INIT, pKData),
    KDBG_INIT_SIG2 = offsetof(KDBG_INIT, pfnKdSanitize)
};

#define VALIDATE_KDBG_INIT(pki)  (      \
    (pki)->sig0 == KDBG_INIT_SIG0 &&    \
    (pki)->sig1 == KDBG_INIT_SIG1 &&    \
    (pki)->sig2 == KDBG_INIT_SIG2       \
    )

#endif
