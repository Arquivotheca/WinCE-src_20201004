//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
