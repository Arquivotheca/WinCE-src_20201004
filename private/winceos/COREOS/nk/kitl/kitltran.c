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
#include <kitl.h>
#include <kitlprot.h>
#include "kernel.h"
#include "kitlp.h"

BOOL fKdRegistered;

//
// implement KITL transport related functions
//


static BOOL DoSendInSysCall (LPBYTE pbData, WORD wDataLen)
{
    BOOL fRet;
    AcquireKitlSpinLock ();
    fRet = Kitl.pfnSend (pbData, wDataLen);
    ReleaseKitlSpinLock ();
    return fRet;
}

static BOOL DoRecvInSysCall (LPBYTE pbData, LPWORD pwDataLen)
{
    BOOL fRet;
    AcquireKitlSpinLock ();
    fRet = Kitl.pfnRecv (pbData, pwDataLen);
    ReleaseKitlSpinLock ();
    return fRet;
}

static BOOL DoEnableIntInSyscall (BOOL fEnable)
{
    AcquireKitlSpinLock ();
    Kitl.pfnEnableInt (fEnable);
    ReleaseKitlSpinLock ();
    return TRUE;
}

static BOOL DoSend (LPBYTE pbData, WORD wDataLen)
{
    BOOL fRet;
    EnterCriticalSection (&KITLTransportCS);
    fRet = Kitl.pfnSend (pbData, wDataLen);
    LeaveCriticalSection (&KITLTransportCS);
    return fRet;
}

static BOOL DoRecv (LPBYTE pbData, LPWORD pwDataLen)
{
    BOOL fRet;
    EnterCriticalSection (&KITLTransportCS);
    fRet = Kitl.pfnRecv (pbData, pwDataLen);
    LeaveCriticalSection (&KITLTransportCS);
    return fRet;
}

static BOOL DoEnableInt (BOOL fEnable)
{
    EnterCriticalSection (&KITLTransportCS);
    Kitl.pfnEnableInt (fEnable);
    LeaveCriticalSection (&KITLTransportCS);
    return TRUE;
}

BOOL KITLKdRegister (void)
{
    if (!InSysCall ()) {
        EnterCriticalSection (&KITLTransportCS);
        fKdRegistered = TRUE;
        LeaveCriticalSection (&KITLTransportCS);
        
    } else {
        // KD tries to register in syscall. This could only happen when we're running passive KITL, and
        // hit a debugchk/break point in syscall. Not much we can do if KITL transport is in use.
        AcquireKitlSpinLock ();
        fKdRegistered = TRUE;
        ReleaseKitlSpinLock ();
    }

    return fKdRegistered;
}


BOOL KitlEnableInt (BOOL fEnable)
{
    if (InSysCall () || fKdRegistered) {
        DoEnableIntInSyscall (fEnable);
    } else {
        DoEnableInt (fEnable);
    }

    return TRUE;
}


//
// KitlSendRawData:
//      calling the transport Send Function, with the necessary protection
//
BOOL KitlSendRawData (LPBYTE pbData, WORD wDataLen)
{
    return (InSysCall () || fKdRegistered)
            ? DoSendInSysCall (pbData, wDataLen)
            : DoSend (pbData, wDataLen);
}

// NOTE: pFrame is pointing to the start of the frame, cbData is ONLY the size of the data
//       NOT the size of the frame
//          ----------------
//          | Frame Header |
//          |--------------|
//          |     Data     |        // of size cbData
//          |--------------|
//          | Frame Tailer |
//          |--------------|
BOOL KitlSendFrame (LPBYTE pbFrame, WORD cbData)
{
    return Kitl.pfnEncode (pbFrame, cbData)
        && KitlSendRawData (pbFrame, (USHORT) (cbData + Kitl.FrmHdrSize + Kitl.FrmTlrSize));
}

BOOL KitlRecvRawData (LPBYTE pRecvBuf, LPWORD pwBufLen)
{
    return (InSysCall () || fKdRegistered)
            ? DoRecvInSysCall (pRecvBuf, pwBufLen)
            : DoRecv (pRecvBuf, pwBufLen);
}

PKITL_HDR KitlRecvFrame (LPBYTE pRecvBuf, LPWORD pwBufLen)
{
    PKITL_HDR pHdr = NULL;
    WORD      wBufLen = *pwBufLen;
    while (KitlRecvRawData (pRecvBuf, pwBufLen)) {
        pHdr = (PKITL_HDR) Kitl.pfnDecode (pRecvBuf, pwBufLen);
        if (   pHdr
            && (KITL_ID == pHdr->Id)
            && (sizeof (KITL_HDR) <= *pwBufLen)) {

            if (KITLDebugZone & ZONE_FRAMEDUMP) {
                KITLFrameDump("<<KITLRecv", pHdr, *pwBufLen);
            }
            
            break;
        }
        KITL_DEBUGMSG (ZONE_RECV, ("KitlRecvFrame: Received Unhandled frame\n"));
        if (pHdr) {
            KITLFrameDump("<<Unknown", pHdr, *pwBufLen);
        }
        pHdr = NULL;
        *pwBufLen = wBufLen;
    }

    return pHdr;
}

BOOL IsDesktopDbgrExist (void)
{
    return fKdRegistered;
}


