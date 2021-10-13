//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++

Module Name:

    kdapi.c

Abstract:

    Implementation of Kernel Debugger portable remote APIs.

--*/

#include "kdp.h"

DWORD NkCEProcessorType; // Temp change [GH]: was CEProcessorType

union
{
    DBGKD_COMMAND Kd;
    OSAXS_COMMAND OsAxs;
} g_dbgCmdPacket;

#define g_dbgkdCmdPacket g_dbgCmdPacket.Kd
#define g_dbgOsAxsCmdPacket g_dbgCmdPacket.OsAxs


DWORD g_dwOsAxsProtocolVersion = OSAXS_PROTOCOL_LATEST_VERSION;


BOOL KDIoControl (DWORD dwIoControlCode, LPVOID lpBuf, DWORD nBufSize)
{
    BOOL fRet = FALSE;
    if (g_kdKernData.pKDIoControl)
    {
#if defined(x86)
        fRet = KCall (g_kdKernData.pKDIoControl, dwIoControlCode, lpBuf, nBufSize);
        KDEnableInt (FALSE, NULL); // Re-Disable interupts (KCall restored them)
#else
        fRet = g_kdKernData.pKDIoControl (dwIoControlCode, lpBuf, nBufSize);
#endif
    }
    return fRet;
}


static void KdpCallOsAccess (OSAXS_COMMAND *, PSTRING);


void KdpResetBps (void)
{
    if (g_kdKernData.pKDIoControl)
    {
        KDIoControl (KD_IOCTL_INIT, NULL, 0);
    }
    KdpDeleteAllBreakpoints();
}


void
KdpSendKdApiCmdPacket (
    IN STRING *MessageHeader,
    IN OPTIONAL STRING *MessageData
    )
{
    KdpSendPacket
    (
        PACKET_TYPE_KD_CMD,
        GUID_KDDBGCLIENT_KDSTUB,
        MessageHeader,
        MessageData
    );
}


/*++

Routine Description:

    This function sends a packet, and then handles debugger commands until
    a "continue" (resume execution) command.
    BreakIns received while waiting will always cause a resend of the
    packet originally sent out.  While waiting, manipulate messages
    will be serviced.

    A resend always resends the original event sent to the debugger,
    not the last response to some debugger command.

Arguments:

    OutPacketType - Supplies the type of packet to send.

    OutMessageHeader - Supplies a pointer to a string descriptor that describes
        the message information.

    OutMessageData - Supplies a pointer to a string descriptor that describes
        the optional message data.

Return Value:

    A value of TRUE is returned if the continue message indicates
    success, Otherwise, a value of FALSE is returned.

--*/

BOOLEAN KdpSendNotifAndDoCmdLoop (
    IN PSTRING OutMessageHeader,
    IN PSTRING OutMessageData OPTIONAL
    )
{
    ULONG Length;
    STRING MessageData;
    STRING MessageHeader;
    USHORT ReturnCode;
    PUCHAR pucDummy;
    BOOLEAN fHandled = FALSE;
    BOOLEAN fExit = FALSE;

    if (!g_fKdbgRegistered)
    { // We never registered KDBG service
        // JIT Debugging
        EnableHDNotifs (TRUE);
        g_fKdbgRegistered = g_kdKernData.pKITLIoCtl(IOCTL_EDBG_REGISTER_DFLT_CLIENT, &pucDummy, KITL_SVC_KDBG, &pucDummy, 0, NULL);
    }

    if (!g_fKdbgRegistered) return FALSE;

    // Loop servicing command message until a continue message is received.
    
    MessageHeader.MaximumLength = sizeof (g_dbgCmdPacket);
    MessageHeader.Buffer = (PCHAR) &g_dbgCmdPacket;
    MessageData.MaximumLength = KDP_MESSAGE_BUFFER_SIZE;
    MessageData.Buffer = (PCHAR) g_abMessageBuffer;

ResendPacket:

    // Send event notification packet to debugger on host.  Come back
    // here any time we see a breakin sequence.
    KdpSendPacket (PACKET_TYPE_KD_NOTIF, GUID_KDDBGCLIENT_KDSTUB, OutMessageHeader, OutMessageData);

    while (!fExit)
    {
        GUID guidClient;

        // Make sure these two MaximumLength members never change while in break state
        KD_ASSERT (sizeof (g_dbgCmdPacket) == MessageHeader.MaximumLength);
        KD_ASSERT (KDP_MESSAGE_BUFFER_SIZE == MessageData.MaximumLength);

        ReturnCode = KdpReceiveCmdPacket(
                        &MessageHeader,
                        &MessageData,
                        &Length,
                        &guidClient
                        );
        if (ReturnCode == (USHORT) KDP_PACKET_RESEND)
        { // This means the host disconnected and reconnected
            DEBUGGERMSG(KDZONE_TRAP, (L"++KdpSendNotifAndDoCmdLoop: RESENDING NOTIFICATION\r\n"));
            KdpResetBps ();
            goto ResendPacket;
        }

        if (!memcmp (&guidClient, &GUID_KDDBGCLIENT_KDSTUB, sizeof (GUID)))
        {
            // Insert here flags of supported feature based on host version
            if (g_dbgkdCmdPacket.dwSubVersionId >= KDAPI_PROTOCOL_VERSION)
            {
                // Switch on the return message API number.
                DEBUGGERMSG(KDZONE_API, (L"Got Api %8.8lx\r\n", g_dbgkdCmdPacket.dwApiNumber));

                switch (g_dbgkdCmdPacket.dwApiNumber)
                {
                    case DbgKdReadVirtualMemoryApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdReadVirtualMemoryApi\r\n"));
                        KdpReadVirtualMemory (&g_dbgkdCmdPacket, &MessageData);
                        break;
                    case DbgKdWriteVirtualMemoryApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdWriteVirtualMemoryApi\r\n"));
                        KdpWriteVirtualMemory (&g_dbgkdCmdPacket, &MessageData);
                        break;
                    case DbgKdReadPhysicalMemoryApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdReadPhysicalMemoryApi\r\n"));
                        KdpReadPhysicalMemory (&g_dbgkdCmdPacket, &MessageData);
                        break;
                    case DbgKdWritePhysicalMemoryApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdWritePhysicalMemoryApi\r\n"));
                        KdpWritePhysicalMemory (&g_dbgkdCmdPacket, &MessageData);
                        break;
                    case DbgKdSetContextApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdSetContextApi\r\n"));
                        KdpSetContext(&g_dbgkdCmdPacket,&MessageData);
                        break;
                    case DbgKdReadControlSpaceApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdReadControlSpaceApi\r\n"));
                        KdpReadControlSpace (&g_dbgkdCmdPacket, &MessageData);
                        break;
                    case DbgKdWriteControlSpaceApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdWriteControlSpaceApi\r\n"));
                        KdpWriteControlSpace (&g_dbgkdCmdPacket, &MessageData);
                        break;
                    case DbgKdReadIoSpaceApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdReadIoSpaceApi\r\n"));
                        KdpReadIoSpace (&g_dbgkdCmdPacket, &MessageData, TRUE);
                        break;
                    case DbgKdWriteIoSpaceApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdWriteIoSpaceApi\r\n"));
                        KdpWriteIoSpace (&g_dbgkdCmdPacket, &MessageData, TRUE);
                        break;
                    case DbgKdContinueApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdContinueApi\r\n"));
                        DEBUGGERMSG(KDZONE_API, (L"Continuing target system %i\r\n", g_dbgkdCmdPacket.u.Continue.dwContinueStatus));
                        fHandled = (BOOLEAN) (g_dbgkdCmdPacket.u.Continue.dwContinueStatus == DBG_CONTINUE);
                        fExit = TRUE;
                        break;
                    case DbgKdTerminateApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdTerminateApi\r\n"));
                        KdpResetBps ();
                        if (g_fKdbgRegistered)
                        {
                            EnableHDNotifs (FALSE);
                            g_kdKernData.pKITLIoCtl (IOCTL_EDBG_DEREGISTER_CLIENT, NULL, KITL_SVC_KDBG, NULL, 0, NULL);
                            g_fKdbgRegistered = FALSE;
                        }
                        // Disconnect from hdstub
                        Hdstub.pfnUnregisterClient (&g_KdstubClient);
                        // Kernel specific clean ups...
                        KdCleanup();
                        g_fDbgConnected = FALSE; // tell KdpTrap to shutdown
                        fHandled = TRUE;
                        fExit = TRUE;
                        break;
                    case DbgKdManipulateBreakpoint:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdManipulateBreakpoint\r\n"));
                        KdpManipulateBreakPoint (&g_dbgkdCmdPacket, &MessageData);
                        break;
                    default: // Invalid message.
                        DEBUGGERMSG( KDZONE_ALERT, (L"  KdpSendNotifAndDoCmdLoop: Unknown API??\r\n"));
                        MessageData.Length = 0;
                        g_dbgkdCmdPacket.dwReturnStatus = STATUS_UNSUCCESSFUL;
                        KdpSendKdApiCmdPacket (&MessageHeader, &MessageData);
                        break;
                }
            }
            else
            {
                DEBUGGERMSG( KDZONE_ALERT, (L"  KdpSendNotifAndDoCmdLoop: protocol Version not supported\r\n"));
                MessageData.Length = 0;
                g_dbgkdCmdPacket.dwReturnStatus = STATUS_UNSUCCESSFUL;
                KdpSendKdApiCmdPacket (&MessageHeader, &MessageData);
            }
        }
        else if (!memcmp (&guidClient, &GUID_KDDBGCLIENT_OSAXS, sizeof (GUID)))
        {
            DEBUGGERMSG (KDZONE_API, (L"Calling OsAccess\r\n"));
            KdpCallOsAccess (&g_dbgOsAxsCmdPacket, &MessageData);
        }
    }
    return fHandled;
}

static void KdpCallOsAccess (OSAXS_COMMAND *pOsAxsCmd, PSTRING AdditionalData)
{
    HRESULT hr;
    STRING ResponseHeader;

    DEBUGGERMSG (KDZONE_API, (L"++KdpCallOsAccess\r\n"));
    
    ResponseHeader.Length = sizeof (*pOsAxsCmd);
    ResponseHeader.Buffer = (BYTE *) pOsAxsCmd;
HandleRequest:
    if (pOsAxsCmd->dwVersion == g_dwOsAxsProtocolVersion)
    {
        switch (pOsAxsCmd->dwApi)
        {
            case OSAXS_API_GET_FLEXIPTMINFO:
                {
                    // Use tempory DWORD for IOCTL call, since AdditionalData->Length is a USHORT.
                    DWORD dwSize = AdditionalData->MaximumLength;

                    DEBUGGERMSG(KDZONE_API, (L"  KdpCallOsAccess -> GetFPTMI\r\n"));
                    // OsAccess requires an output buffer.  Need to report the
                    // size of the buffer to the API call so it won't overflow
                    hr = Hdstub.pfnCallClientIoctl (OSAXST0_NAME, OSAXST0_IOCTL_GET_FLEXIPTMINFO, (DWORD) &pOsAxsCmd->u.FlexiReq,
                            (DWORD) &dwSize, (DWORD) AdditionalData->Buffer, 0);

                    if (SUCCEEDED (hr))
                    {
                        AdditionalData->Length = (USHORT) dwSize;
                    }
                    else
                    {
                        DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdpCallOsAccess: Failed to get FlexiPTMI info from OsAxsT0, hr = 0x%08X\r\n"),hr));
                    }
                       
                }
                break;
            
            case OSAXS_API_GET_STRUCT:
                {
                    // Use tempory DWORD for IOCTL call, since AdditionalData->Length is a USHORT.
                    DWORD dwSize = AdditionalData->MaximumLength;
                    DEBUGGERMSG(KDZONE_API, (L"  KdpCallOsAccess -> GetStruct @0x%08x\r\n", pOsAxsCmd->u.Struct.StructAddr));

                    hr = Hdstub.pfnCallClientIoctl (OSAXST1_NAME, OSAXST1_IOCTL_GET_OSSTRUCT, (DWORD) &pOsAxsCmd->u.Struct,
                            (DWORD) AdditionalData->Buffer, (DWORD) &dwSize, 0);
                    if (SUCCEEDED (hr))
                    {
                        AdditionalData->Length = (USHORT) dwSize;
                    }
                    else
                    {
                        DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdpCallOsAccess: Failed to get OsStruct info from OsAxsT1, hr = 0x%08X\r\n"),hr));
                    }
                }
                break;
            case OSAXS_API_GET_THREADCTX:
                {
                    // Use tempory DWORD for IOCTL call, since AdditionalData->Length is a USHORT.
                    DWORD dwSize = AdditionalData->MaximumLength;

                    hr = Hdstub.pfnCallClientIoctl (OSAXST1_NAME, OSAXST1_IOCTL_GET_THREADCTX, (DWORD) pOsAxsCmd->u.Addr,
                            (DWORD) &AdditionalData->Buffer, (DWORD) &dwSize, 0);
                    if (SUCCEEDED (hr))
                    {
                        AdditionalData->Length = (USHORT) dwSize;
                    }
                    else
                    {
                        DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdpCallOsAccess: Failed to get ThreadCtx info from OsAxsT1, hr = 0x%08X\r\n"),hr));
                    }
                }
                break;
            case OSAXS_API_SET_THREADCTX:
                {
                    hr = Hdstub.pfnCallClientIoctl (OSAXST1_NAME, OSAXST1_IOCTL_SET_THREADCTX, (DWORD) pOsAxsCmd->u.Addr,
                            (DWORD) AdditionalData->Buffer, (DWORD)AdditionalData->Length, 0);
                    AdditionalData->Length = 0;
                    if (FAILED (hr))
                    {
                        DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdpCallOsAccess: Failed to set ThreadCtx info in OsAxsT1, hr = 0x%08X\r\n"),hr));
                    }
                }
                break;

            case OSAXS_API_GET_MOD_O32_LITE:
                {
                    DWORD dwSize = (DWORD) AdditionalData->MaximumLength;
                    hr = Hdstub.pfnCallClientIoctl (OSAXST1_NAME, OSAXST1_IOCTL_GET_MODULE_O32_DATA,
                            (DWORD) pOsAxsCmd->u.ModO32.in_hmod,
                            (DWORD) &pOsAxsCmd->u.ModO32.out_cO32Lite,
                            (DWORD) AdditionalData->Buffer,
                            (DWORD) &dwSize);
                    if (SUCCEEDED (hr))
                    {
                        AdditionalData->Length = (USHORT) dwSize;
                    }
                    else
                    {
                        DEBUGGERMSG (KDZONE_ALERT, (TEXT("  KdpCallOsAccess: Failed to call OsAxsT1\r\n")));
                    }
                    break;
                }
#if defined(x86)
            case OSAXS_API_GET_EXCEPTION_REGISTRATION:
                {
                    DEBUGGERMSG(KDZONE_API, (L"  KdpCallOsAccess:  OSAXST1_IOCTL_GET_EXCEPTION_REGISTRATION\r\n"));
                    hr = Hdstub.pfnCallClientIoctl (OSAXST1_NAME, OSAXST1_IOCTL_GET_EXCEPTION_REGISTRATION, (DWORD) &pOsAxsCmd->u.ExReg, 0, 0, 0);
                    AdditionalData->Length = 0;
                    break;
                }
#endif
            default:
                DEBUGGERMSG (KDZONE_API, (L"  KdpCallOsAccess: Invalid API Number %d\r\n", pOsAxsCmd->dwApi));
                hr = OSAXS_E_APINUMBER;
                break;
        }
    }
    else
    {
        DEBUGGERMSG(KDZONE_ALERT, (L"  KdpCallOsAccess: Protocol mismatch\r\n"));
        if (pOsAxsCmd->dwVersion < OSAXS_PROTOCOL_LATEST_VERSION)
        {
            g_dwOsAxsProtocolVersion = pOsAxsCmd->dwVersion;
            goto HandleRequest;
        }
        else
        {
            pOsAxsCmd->dwVersion = OSAXS_PROTOCOL_LATEST_VERSION;
            hr = OSAXS_E_PROTOCOLVERSION;
        }
    }

    /* Manufacture response */
    pOsAxsCmd->hr = hr;
    KdpSendPacket(PACKET_TYPE_KD_CMD, GUID_KDDBGCLIENT_OSAXS, &ResponseHeader, AdditionalData);

    DEBUGGERMSG (KDZONE_API, (L"--KdpCallOsAccess\r\n"));
}


VOID
KdpReadVirtualMemory(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    )

/*++

Routine Description:

    This function is called in response of a read virtual memory
    command message. Its function is to read virtual memory
    and return.

Arguments:

    pdbgkdCmdPacket - Supplies the command message.

    AdditionalData - Supplies any additional data for the message.

Return Value:

    None.

--*/

{
    DBGKD_READ_MEMORY *a = &pdbgkdCmdPacket->u.ReadMemory;
    ULONG ulLengthAtempted;
    STRING MessageHeader;

    MessageHeader.Length = sizeof (*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR) pdbgkdCmdPacket;

    // make sure that nothing but a read memory message was transmitted

    KD_ASSERT (AdditionalData->Length == 0);

    // Trim transfer count to fit in a single message
    if (a->dwTransferCount > KDP_MESSAGE_BUFFER_SIZE)
    {
        ulLengthAtempted = KDP_MESSAGE_BUFFER_SIZE;
    }
    else
    {
        ulLengthAtempted = a->dwTransferCount;
    }

    // Perform the actual memory read, if some locations are not readable, the read will be truncated
    AdditionalData->Length = (USHORT) KdpMoveMemory(
                                        AdditionalData->Buffer,
                                        (void *) a->qwTgtAddress,
                                        ulLengthAtempted
                                        );

    if (ulLengthAtempted == AdditionalData->Length)
    {
        pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
    }
    else
    {
        DEBUGGERMSG( KDZONE_API, (L"  KdpReadVirtualMemory: Only read %d of %d bytes starting at 0x%8.8x\r\n", AdditionalData->Length, ulLengthAtempted, a->qwTgtAddress));
        pdbgkdCmdPacket->dwReturnStatus = STATUS_UNSUCCESSFUL;
    }

    // Sanitize (remove SW BP artefacts) the memory block data before sending it back
    DEBUGGERMSG (KDZONE_VIRTMEM, (L"  KdpReadVirtualMemory: Sanitizing %d bytes starting at 0x%8.8x\r\n", AdditionalData->Length, a->qwTgtAddress));
    KdpSanitize (AdditionalData->Buffer, (void *) a->qwTgtAddress, AdditionalData->Length, TRUE);

    a->dwActualBytesRead = AdditionalData->Length;

    KdpSendKdApiCmdPacket (&MessageHeader, AdditionalData);
}


VOID
KdpWriteVirtualMemory(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    )

/*++

Routine Description:

    This function is called in response of a write virtual memory
    command message. Its function is to write virtual memory
    and return.

Arguments:

    pdbgkdCmdPacket - Supplies the command message.

    AdditionalData - Supplies any additional data for the message.

Return Value:

    None.

--*/

{
    DBGKD_WRITE_MEMORY *a = &pdbgkdCmdPacket->u.WriteMemory;
    ULONG ulLengthWritten;
    STRING MessageHeader;

    MessageHeader.Length = sizeof (*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR) pdbgkdCmdPacket;

    ulLengthWritten = KdpMoveMemory (
                (void *) a->qwTgtAddress,
                AdditionalData->Buffer,
                AdditionalData->Length
                );

    if (ulLengthWritten == AdditionalData->Length)
    {
        pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
    }
    else
    {
        pdbgkdCmdPacket->dwReturnStatus = STATUS_UNSUCCESSFUL;
    }

    a->dwActualBytesWritten = ulLengthWritten;

    KdpSendKdApiCmdPacket (&MessageHeader, NULL);
}

VOID
KdpSetContext(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    )
/*++

Routine Description:

    This function is called in response of a set context state
    manipulation message.  Its function is set the current
    context.

Arguments:

    pdbgkdCmdPacket - Supplies the command message.

    AdditionalData - Supplies any additional data for the message.

Return Value:

    None.

--*/
{
    STRING MessageHeader;
    BOOL fOwner = FALSE;

#ifdef ARM
    CONCAN_REGS *pConcanRegs = (CONCAN_REGS *)(AdditionalData->Buffer + sizeof (CONTEXT));
#endif

    MessageHeader.Length = sizeof(*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR)pdbgkdCmdPacket;

#if defined (ARM)
    KD_ASSERT (AdditionalData->Length == (sizeof (CONTEXT) + sizeof (CONCAN_REGS)));
#else
    KD_ASSERT(AdditionalData->Length == sizeof(CONTEXT));
#endif

    pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
    memcpy (g_pctxException, AdditionalData->Buffer, sizeof (CONTEXT));

    // copy the DSP registers into the thread context
#if defined(SHx) && !defined(SH4) && !defined(SH3e)
    // copy over the DSP registers from the thread context
    fOwner = (pCurThread == g_CurDSPOwner);
    DSPFlushContext();
    // if DSPFlushContext updated pCurThread's PSR, keep exception context in sync
    if (fOwner) g_pctxException->Psr &= ~SR_DSP_ENABLED;
    memcpy (&(pCurThread->ctx.DSR), &(g_pctxException->DSR), sizeof (DWORD) * 13);
#endif

    // copy the floating point registers into the thread context
#if defined(SH4)
    fOwner = (pCurThread == g_CurFPUOwner);
    FPUFlushContext();
    // if FPUFlushContext updated pCurThread's PSR, keep exception context in sync
    if (fOwner) g_pctxException->Psr |= SR_FPU_DISABLED;
    memcpy (&(pCurThread->ctx.Fpscr), &(g_pctxException->Fpscr), sizeof (DWORD) * 34);
#elif defined(MIPS_HAS_FPU)
    FPUFlushContext();
    pCurThread->ctx.Fsr = g_pctxException->Fsr;
    memcpy (&(pCurThread->ctx.FltF0), &(g_pctxException->FltF0), sizeof (FREG_TYPE) * 32);
#elif defined(ARM)
    // ARM VFP10 Support
    // FPUFlushContext might modify FpExc, but apparently it can't be restored, so we shouldn't bother
    // trying update our context with it
    FPUFlushContext ();
    memcpy (&(pCurThread->ctx.Fpscr), &(g_pctxException->Fpscr), sizeof (DWORD) * 43);

    if (DetectConcanCoprocessors ())
    {
        SetConcanRegisters (pConcanRegs);
    }
#endif

    KdpSendKdApiCmdPacket (&MessageHeader, NULL);
}


VOID
KdpSetNotifPacket (
    IN DBGKD_NOTIF *pdbgNotifPacket,
    IN EXCEPTION_RECORD *pExceptionRecord,
    IN BOOLEAN SecondChance
    )

/*++

Routine Description:

    Fill in the Wait_State_Change message record.

Arguments:

    pdbgNotifPacket - Supplies pointer to record to fill in

    pExceptionRecord - Supplies a pointer to an exception record.

    SecondChance - Supplies a boolean value that determines whether this is
        the first or second chance for the exception.

Return Value:

    None.

--*/

{
    memset (pdbgNotifPacket, 0, sizeof (*pdbgNotifPacket)); // zero init

    //  Set up description of event, including exception record
    pdbgNotifPacket->dwNewState = DbgKdExceptionNotif;
    pdbgNotifPacket->NbBpAvail.dwNbHwCodeBpAvail = 0; // TODO: Get this from OAL
    pdbgNotifPacket->NbBpAvail.dwNbSwCodeBpAvail = BREAKPOINT_TABLE_SIZE - g_nTotalNumDistinctSwCodeBps;
    pdbgNotifPacket->NbBpAvail.dwNbHwDataBpAvail = 0; // TODO: Get this from OAL
    pdbgNotifPacket->NbBpAvail.dwNbSwDataBpAvail = 0;

    pdbgNotifPacket->TgtVerInfo.dwCpuFamily = TARGET_CODE_CPU;
    pdbgNotifPacket->TgtVerInfo.dwBuildNumber = VER_PRODUCTBUILD; // TODO: Get the real build
    pdbgNotifPacket->TgtVerInfo.wMajorOsVersion = 5;
    pdbgNotifPacket->TgtVerInfo.wMinorOsVersion = 0;
    pdbgNotifPacket->TgtVerInfo.dwNkCEProcessorType = NkCEProcessorType;
    pdbgNotifPacket->TgtVerInfo.dwCpuCapablilityFlags = 0;
    if (g_kdKernData.fDSPPresent)
    { // hardware DSP support used
        pdbgNotifPacket->TgtVerInfo.dwCpuCapablilityFlags |= DBGKD_VERS_FLAG_DSP;
    }
    if (g_kdKernData.fFPUPresent)
    { // hardware FPU support used
        pdbgNotifPacket->TgtVerInfo.dwCpuCapablilityFlags |= DBGKD_VERS_FLAG_FPU;
    }
#if defined (ARM)
    if (DetectConcanCoprocessors())
    { // Concan support
        pdbgNotifPacket->TgtVerInfo.dwCpuCapablilityFlags |= DBGKD_VERS_FLAG_MULTIMEDIA;
    }
#endif
    pdbgNotifPacket->dwSubVersionId = KDAPI_PROTOCOL_VERSION;
    pdbgNotifPacket->wNumberOfCpus = 1;
    if (g_fForceReload)
    {
        pdbgNotifPacket->dwKdpFlags |= DBGKD_STATE_DID_RESET;
        g_fForceReload = FALSE;
    }
    memcpy (&pdbgNotifPacket->u.Exception.ExceptionRecord,
            pExceptionRecord,
            sizeof (EXCEPTION_RECORD));
    pdbgNotifPacket->u.Exception.dwFirstChance = !SecondChance;

#if defined(SH3)
    g_pctxException->DebugRegisters.BarA  = READ_REGISTER_ULONG(UBCBarA);
    g_pctxException->DebugRegisters.BasrA = READ_REGISTER_UCHAR(UBCBasrA);
    g_pctxException->DebugRegisters.BamrA = READ_REGISTER_UCHAR(UBCBamrA);
    g_pctxException->DebugRegisters.BbrA  = READ_REGISTER_USHORT(UBCBbrA);
    g_pctxException->DebugRegisters.BarB  = READ_REGISTER_ULONG(UBCBarB);
    g_pctxException->DebugRegisters.BasrB = READ_REGISTER_UCHAR(UBCBasrB);
    g_pctxException->DebugRegisters.BamrB = READ_REGISTER_UCHAR(UBCBamrB);
    g_pctxException->DebugRegisters.BbrB  = READ_REGISTER_USHORT(UBCBbrB);
    g_pctxException->DebugRegisters.BdrB  = READ_REGISTER_ULONG(UBCBdrB);
    g_pctxException->DebugRegisters.BdmrB = READ_REGISTER_ULONG(UBCBdmrB);
    g_pctxException->DebugRegisters.Brcr  = READ_REGISTER_USHORT(UBCBrcr);
    g_pctxException->DebugRegisters.Align = 0;
#endif

    //  Copy context record immediately following instruction stream
    pdbgNotifPacket->u.Exception.Context = *g_pctxException;

#ifdef ARM
    // Not editting the _CONTEXT struction.  Here is the copy of additional register information
    if (DetectConcanCoprocessors ())
    {
        DEBUGGERMSG(KDZONE_CONCAN, (L"Adding on the concan registers\n\r"));;
        GetConcanRegisters (&pdbgNotifPacket->u.Exception.ConcanRegs);
    }
#endif

}


BOOLEAN
KdpReportExceptionNotif (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN BOOLEAN SecondChance
    )
/*++

Routine Description:

    This routine sends an exception state change packet to the kernel
    debugger and waits for a manipulate state message.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

    SecondChance - Supplies a boolean value that determines whether this is
        the first or second chance for the exception.

Return Value:

    A value of TRUE is returned if the exception is handled. Otherwise, a
    value of FALSE is returned.

--*/
{
    STRING MessageData;
    STRING MessageHeader;
    DBGKD_NOTIF dbgNotifPacket = {0};
    BOOLEAN fHandled;

    KdpSetNotifPacket (&dbgNotifPacket,
                        ExceptionRecord,
                        SecondChance
                        );

    MessageHeader.Length = sizeof (DBGKD_NOTIF);
    MessageHeader.Buffer = (PCHAR) &dbgNotifPacket;

    MessageData.Length = 0;

    // Send packet to the kernel debugger on the host machine, wait for answer.
    fHandled = KdpSendNotifAndDoCmdLoop (&MessageHeader, &MessageData);
    
    return fHandled;
}


#define MmDbgTranslatePhysicalAddress(Address) (Address)


VOID
KdpReadPhysicalMemory(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    )
/*++

Routine Description:

    This function is called in response to a read physical memory
    command message. Its function is to read physical memory
    and return.

Arguments:

    pdbgkdCmdPacket - Supplies the command message.

    AdditionalData - Supplies any additional data for the message.

Return Value:

    None.

--*/
{
    DBGKD_READ_MEMORY *a = &pdbgkdCmdPacket->u.ReadMemory;
    ULONG Length;
    STRING MessageHeader;
    PVOID VirtualAddress;
    PHYSICAL_ADDRESS Source;
    PUCHAR Destination;
    USHORT NumberBytes;
    USHORT BytesLeft;

    MessageHeader.Length = sizeof(*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR)pdbgkdCmdPacket;

    //
    // make sure that nothing but a read memory message was transmitted
    //

    KD_ASSERT(AdditionalData->Length == 0);

    //
    // Trim transfer count to fit in a single message
    //

    if (a->dwTransferCount > KDP_MESSAGE_BUFFER_SIZE)
    {
        Length = KDP_MESSAGE_BUFFER_SIZE;
    }
    else
    {
        Length = a->dwTransferCount;
    }

    //
    // Since the MmDbgTranslatePhysicalAddress only maps in one physical
    // page at a time, we need to break the memory move up into smaller
    // moves which don't cross page boundaries.  There are two cases we
    // need to deal with.  The area to be moved may start and end on the
    // same page, or it may start and end on different pages (with an
    // arbitrary number of pages in between)
    //

    Source = (PHYSICAL_ADDRESS) a->qwTgtAddress;

    Destination = AdditionalData->Buffer;
    BytesLeft = (USHORT)Length;
    if (PAGE_ALIGN((PUCHAR) a->qwTgtAddress) ==
        PAGE_ALIGN((PUCHAR)(a->qwTgtAddress)+Length))
    {
        //
        // Memory move starts and ends on the same page.
        //

        VirtualAddress = (PVOID)MmDbgTranslatePhysicalAddress(Source);
        AdditionalData->Length = (USHORT)KdpMoveMemory(
                                            Destination,
                                            VirtualAddress,
                                            BytesLeft
                                            );
        BytesLeft -= AdditionalData->Length;
    }
    else
    {

        //
        // Memory move spans page boundaries
        //

        VirtualAddress = (PVOID)MmDbgTranslatePhysicalAddress(Source);
        NumberBytes = (USHORT)(PAGE_SIZE - BYTE_OFFSET(VirtualAddress));
        AdditionalData->Length = (USHORT)KdpMoveMemory(
                                        Destination,
                                        VirtualAddress,
                                        NumberBytes
                                        );
        Source += NumberBytes;
        Destination += NumberBytes;
        BytesLeft -= NumberBytes;
        while(BytesLeft > 0) {

            //
            // Transfer a full page or the last bit,
            // whichever is smaller.
            //

            VirtualAddress = (PVOID) MmDbgTranslatePhysicalAddress (Source);
            NumberBytes = (USHORT) ((PAGE_SIZE < BytesLeft) ? PAGE_SIZE : BytesLeft);

            AdditionalData->Length += (USHORT)KdpMoveMemory(
                                            Destination,
                                            VirtualAddress,
                                            NumberBytes
                                            );
            Source += NumberBytes;
            Destination += NumberBytes;
            BytesLeft -= NumberBytes;
        }
    }

    if (Length == AdditionalData->Length)
    {
        pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
    }
    else
    {
        pdbgkdCmdPacket->dwReturnStatus = STATUS_UNSUCCESSFUL;
    }
    a->dwActualBytesRead = AdditionalData->Length;

    KdpSendKdApiCmdPacket (&MessageHeader, AdditionalData);
}


VOID
KdpWritePhysicalMemory(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    )
/*++

Routine Description:

    This function is called in response to a write physical memory
    command message. Its function is to write physical memory
    and return.

Arguments:

    pdbgkdCmdPacket - Supplies the command message.

    AdditionalData - Supplies any additional data for the message.

Return Value:

    None.

--*/
{
    DBGKD_WRITE_MEMORY *a = &pdbgkdCmdPacket->u.WriteMemory;
    ULONG Length;
    STRING MessageHeader;
    PVOID VirtualAddress;
    PHYSICAL_ADDRESS Destination;
    PUCHAR Source;
    USHORT NumberBytes;
    USHORT BytesLeft;

    MessageHeader.Length = sizeof(*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR)pdbgkdCmdPacket;

    //
    // Since the MmDbgTranslatePhysicalAddress only maps in one physical
    // page at a time, we need to break the memory move up into smaller
    // moves which don't cross page boundaries.  There are two cases we
    // need to deal with.  The area to be moved may start and end on the
    // same page, or it may start and end on different pages (with an
    // arbitrary number of pages in between)
    //

    Destination = (PHYSICAL_ADDRESS) a->qwTgtAddress;

    Source = AdditionalData->Buffer;
    BytesLeft = (USHORT) a->dwTransferCount;

    // TODO: Call kdpIsROM before writting

    if(PAGE_ALIGN((PUCHAR)Destination) ==
       PAGE_ALIGN((PUCHAR)(Destination)+BytesLeft)) {
        //
        // Memory move starts and ends on the same page.
        //
        VirtualAddress = (PVOID)MmDbgTranslatePhysicalAddress(Destination);
        Length = (USHORT)KdpMoveMemory(
                                       VirtualAddress,
                                       Source,
                                       BytesLeft
                                      );
        BytesLeft -= (USHORT) Length;
    } else {
        //
        // Memory move spans page boundaries
        //
        VirtualAddress = (PVOID)MmDbgTranslatePhysicalAddress(Destination);
        NumberBytes = (USHORT) (PAGE_SIZE - BYTE_OFFSET(VirtualAddress));
        Length = (USHORT)KdpMoveMemory(
                                       VirtualAddress,
                                       Source,
                                       NumberBytes
                                      );
        Source += NumberBytes;
        Destination += NumberBytes;

        BytesLeft -= NumberBytes;
        while(BytesLeft > 0) {

            //
            // Transfer a full page or the last bit,
            // whichever is smaller.
            //

            VirtualAddress = (PVOID)MmDbgTranslatePhysicalAddress(Destination);
            NumberBytes = (USHORT) ((PAGE_SIZE < BytesLeft) ? PAGE_SIZE : BytesLeft);

            Length += (USHORT)KdpMoveMemory(
                                            VirtualAddress,
                                            Source,
                                            NumberBytes
                                           );
            Source += NumberBytes;
            Destination += NumberBytes;
            BytesLeft -= NumberBytes;
        }
    }


    if (Length == AdditionalData->Length) {
        pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
    } else {
        pdbgkdCmdPacket->dwReturnStatus = STATUS_UNSUCCESSFUL;
    }

    a->dwActualBytesWritten = Length;

    KdpSendKdApiCmdPacket (&MessageHeader, NULL);
}


