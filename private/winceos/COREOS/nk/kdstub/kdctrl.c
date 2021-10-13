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

    kdctrl.c

Abstract:

    This module implements CPU specific remote debug APIs.

Environment:

    WinCE

--*/

#include "kdp.h"

#if defined(x86)
int             __cdecl _inp(unsigned short);
int             __cdecl _outp(unsigned short, int);
unsigned short  __cdecl _inpw(unsigned short);
unsigned short  __cdecl _outpw(unsigned short, unsigned short);
unsigned long   __cdecl _inpd(unsigned short);
unsigned long   __cdecl _outpd(unsigned short, unsigned long);

#pragma intrinsic(_inp, _inpw, _inpd, _outp, _outpw, _outpd)
#endif


VOID
KdpReadIoSpace (
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData,
    IN BOOL fSendPacket
    )

/*++

Routine Description:

    This function is called in response of a read io space command
    message.  Its function is to read system io
    locations.

Arguments:

    pdbgkdCmdPacket - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    fSendPacket - TRUE send packet to Host, otherwise not

Return Value:

    None.

--*/

{
    DBGKD_READ_WRITE_IO *a = &pdbgkdCmdPacket->u.ReadWriteIo;
    STRING MessageHeader;
#if !defined(x86)
    PUCHAR b;
    PUSHORT s;
    PULONG l;
#endif

    MessageHeader.Length = sizeof(*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR)pdbgkdCmdPacket;

    KD_ASSERT(AdditionalData->Length == 0);

    pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_SUCCESS;

    //
    // Check Size and Alignment
    //

    __try {
        switch ( a->dwDataSize ) {
#if defined (x86) // x86 processor have a separate io mapping
            case 1:
                a->dwDataValue = _inp( (SHORT) a->qwTgtIoAddress);
                break;
            case 2:
                a->dwDataValue = _inpw ((SHORT) a->qwTgtIoAddress);
                break;
            case 4:
                a->dwDataValue = _inpd ((SHORT) a->qwTgtIoAddress);
                break;
#else // all processors other than x86 use the default memory mapped version
            case 1:
                b = (PUCHAR) (a->qwTgtIoAddress);
                if ( b ) {
                    a->dwDataValue = (ULONG)*b;
                } else {
                    pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_ACCESS_VIOLATION;
                }
                break;
            case 2:
                if ((ULONG)a->qwTgtIoAddress & 1 ) {
                    pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_DATATYPE_MISALIGNMENT;
                } else {
                    s = (PUSHORT) (a->qwTgtIoAddress);
                    if ( s ) {
                        a->dwDataValue = (ULONG)*s;
                    } else {
                        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_ACCESS_VIOLATION;
                    }
                }
                break;
            case 4:
                if ((ULONG)a->qwTgtIoAddress & 3 ) {
                    pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_DATATYPE_MISALIGNMENT;
                } else {
                    l = (PULONG) (a->qwTgtIoAddress);
                    if ( l ) {
                        a->dwDataValue = (ULONG)*l;
                    } else {
                        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_ACCESS_VIOLATION;
                    }
                }
                break;
#endif
            default:
                pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_INVALID_PARAMETER;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_ACCESS_VIOLATION;
    }

    if (fSendPacket)
    {
        KdpSendKdApiCmdPacket (&MessageHeader, NULL);
    }
}


VOID
KdpWriteIoSpace (
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData,
    IN BOOL fSendPacket
    )

/*++

Routine Description:

    This function is called in response of a write io space command
    message.  Its function is to write to system io
    locations.

Arguments:

    pdbgkdCmdPacket - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    fSendPacket - TRUE send packet to Host, otherwise not

Return Value:

    None.

--*/

{
    DBGKD_READ_WRITE_IO *a = &pdbgkdCmdPacket->u.ReadWriteIo;
    STRING MessageHeader;
#if !defined(x86)
    PUCHAR b;
    PUSHORT s;
    PULONG l;
#endif

    MessageHeader.Length = sizeof(*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR)pdbgkdCmdPacket;

    KD_ASSERT(AdditionalData->Length == 0);

    pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_SUCCESS;

    //
    // Check Size and Alignment
    //

    __try {
        switch ( a->dwDataSize ) {
#if defined(x86) // x86 processor have a separate io mapping
            case 1:
                _outp ((SHORT) a->qwTgtIoAddress, a->dwDataValue);
                break;
            case 2:
                _outpw ((SHORT) a->qwTgtIoAddress, (WORD) a->dwDataValue);
                break;
            case 4:
                _outpd ((SHORT) a->qwTgtIoAddress, (DWORD) a->dwDataValue);
                break;
#else // all processors other than x86 use the default memory mapped version
            case 1:
                b = (PUCHAR) (a->qwTgtIoAddress);
                if ( b ) {
                    WRITE_REGISTER_UCHAR(b,(UCHAR)a->dwDataValue);
                } else {
                    pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_ACCESS_VIOLATION;
                }
                break;
            case 2:
                if ((ULONG)a->qwTgtIoAddress & 1 ) {
                    pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_DATATYPE_MISALIGNMENT;
                } else {
                    s = (PUSHORT) (a->qwTgtIoAddress);
                    if ( s ) {
                        WRITE_REGISTER_USHORT(s,(USHORT)a->dwDataValue);
                    } else {
                        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_ACCESS_VIOLATION;
                    }
                }
                break;
            case 4:
                if ((ULONG)a->qwTgtIoAddress & 3 ) {
                    pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_DATATYPE_MISALIGNMENT;
                } else {
                    l = (PULONG) (a->qwTgtIoAddress);
                    if ( l ) {
                        WRITE_REGISTER_ULONG(l,a->dwDataValue);
                    } else {
                        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_ACCESS_VIOLATION;
                    }
                }
                break;
#endif
            default:
                pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_INVALID_PARAMETER;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_ACCESS_VIOLATION;
    }

    if (fSendPacket)
    {
        KdpSendKdApiCmdPacket (&MessageHeader, NULL);
    }
}

