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
#ifndef __PORTIO_H__
#define __PORTIO_H__

UCHAR READ_PORT_UCHAR ( DWORD Port )
{
#if x86
    _asm {
        mov     dx, word ptr Port
        in      al, dx
    }
#else
    return *(volatile PUCHAR const)Port; 
}

VOID WRITE_PORT_UCHAR ( DWORD Port, UCHAR Value )
{
#if x86
    _asm {
        mov     dx, word ptr Port
        mov     al, Value
        out     dx, al
    }
#else
    *(volatile PUCHAR)Port = Value;
}

#endif
