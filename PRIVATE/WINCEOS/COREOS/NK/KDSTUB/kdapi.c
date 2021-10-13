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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
} g_dbgCmdPacket, g_dbgCmdPacket2;


#define g_dbgkdCmdPacket g_dbgCmdPacket.Kd
#define g_dbgOsAxsCmdPacket g_dbgCmdPacket.OsAxs
#define g_dbgOsAxsCmdPacket2 g_dbgCmdPacket2.OsAxs


DWORD g_dwOsAxsProtocolVersion = OSAXS_PROTOCOL_LATEST_VERSION_TSBC_WITH;


BOOL g_fResendNotifPacket = FALSE;
BOOL g_fValidOsAxsCmdPacket = FALSE;
STRING g_MessageHeader;


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

VOID 
KdpSendDbgMessage(
    IN CHAR*  pszMessage,
    IN DWORD  dwMessageSize
    )
{
    STRING MessageHeader;

    if (g_dbgkdCmdPacket.dwSubVersionId < 20)
    {
        DEBUGGERMSG(KDZONE_ALERT, (L"  KdpSendDbgMessage: Requires PB with protocol version 20 or larger\r\n"));
        return;
    }

    if (dwMessageSize >= KDP_MESSAGE_BUFFER_SIZE)
    {
        DEBUGGERMSG(KDZONE_ALERT, (L"  KdpSendDbgMessage: Message size (%u) larger than max allowed (%u)\r\n", dwMessageSize, KDP_MESSAGE_BUFFER_SIZE));
        return;
    }

    MessageHeader.Length = (USHORT)dwMessageSize;
    MessageHeader.Buffer = (PCHAR) pszMessage;

    KdpSendPacket (PACKET_TYPE_KD_DBG_MSG, GUID_KDDBGCLIENT_KDSTUB, &MessageHeader, NULL);
}


// TODO: Relocate this to OSAXST0
BOOL KdpSendOsAxsResponse(
    IN STRING * pResponseHeader,
    IN OPTIONAL STRING * pAdditionalData
)
{
    GUID guidClient;
    ULONG Length;
    STRING MessageData;
    STRING MessageHeader;
    USHORT ReturnCode;
    BOOL fSuccess = FALSE;
    OSAXS_API_WATSON_DUMP_OTF *pOtfResponse = &(((OSAXS_COMMAND *) pResponseHeader->Buffer)->u.GetWatsonOTF);
    BOOL fProcessPacket = TRUE;

    KdpSendPacket (PACKET_TYPE_KD_CMD, GUID_KDDBGCLIENT_OSAXS, pResponseHeader, pAdditionalData);
    g_fValidOsAxsCmdPacket = FALSE;
    
    MessageHeader.MaximumLength = sizeof (g_dbgCmdPacket2);
    MessageHeader.Buffer = (PCHAR) &g_dbgCmdPacket2;
    MessageData.MaximumLength = KDP_MESSAGE_BUFFER_SIZE;
    MessageData.Buffer = (PCHAR) g_abMessageBuffer;

    ReturnCode = KdpReceiveCmdPacket(
                    &MessageHeader,
                    &MessageData,
                    &Length,
                    &guidClient
                    );
    if (ReturnCode == (USHORT) KDP_PACKET_RESEND)
    { // This means the host disconnected and reconnected
        DEBUGGERMSG(KDZONE_ALERT, (L"  KdpSendOsAxsResponse: RESENDING NOTIFICATION\r\n"));
        g_fResendNotifPacket = TRUE;
        goto Exit;
    }

    if (fProcessPacket && memcmp (&guidClient, &GUID_KDDBGCLIENT_OSAXS, sizeof (GUID)))
    {
        DEBUGGERMSG (KDZONE_ALERT, (L"  KdpSendOsAxsResponse: incorrect client type, not OsAxs client.\r\n"));
        fProcessPacket = FALSE;
    }

    if (fProcessPacket && g_dbgOsAxsCmdPacket2.dwVersion < OSAXS_PROTOCOL_LATEST_VERSION_TSBC_WITH)
    {
        DEBUGGERMSG (KDZONE_ALERT, (L"  KdpSendOsAxsResponse: Host is too old. Incompatible with protocol.\r\n"));
        fProcessPacket = FALSE;
    }

    if (fProcessPacket &&
        OSAXS_API_GET_WATSON_DUMP_FILE != g_dbgOsAxsCmdPacket2.dwApi &&
        OSAXS_API_GET_WATSON_DUMP_FILE_COMPAT != g_dbgOsAxsCmdPacket2.dwApi)
    {
        DEBUGGERMSG (KDZONE_ALERT, (L"  KdpSendOsAxsResponse: Unexpected API number, Expecting %d or %d, got %d\r\n",
                    OSAXS_API_GET_WATSON_DUMP_FILE,
                    OSAXS_API_GET_WATSON_DUMP_FILE_COMPAT,
                    g_dbgOsAxsCmdPacket2.dwApi));
        fProcessPacket = FALSE;
    }

    if (fProcessPacket && g_dbgOsAxsCmdPacket2.u.GetWatsonOTF.dwDumpType != 0)
    {
        if (g_dbgOsAxsCmdPacket2.u.GetWatsonOTF.dwDumpType != pOtfResponse->dwDumpType)
        {
            DEBUGGERMSG (KDZONE_ALERT, (L"  KdpSendOsAxsResponse: Wrong dump type.\r\n"));
            fProcessPacket = FALSE;
        }
        else if (g_dbgOsAxsCmdPacket2.u.GetWatsonOTF.dw64SegmentIndex != (pOtfResponse->dw64SegmentIndex + pOtfResponse->dwSegmentBufferSize))
        {
            DEBUGGERMSG (KDZONE_ALERT, (L"  KdpSendOsAxsResponse: Next segment index is wrong.\r\n"));
            fProcessPacket = FALSE;
        }
    }

    if (fProcessPacket)
    {        
        g_fValidOsAxsCmdPacket = TRUE; 
        if (g_dbgOsAxsCmdPacket2.u.GetWatsonOTF.dwDumpType == 0)
        { // Cancel - abort
            DEBUGGERMSG (KDZONE_ALERT, (L"  KdpSendOsAxsResponse: Cancel upload\r\n"));
        }
        else
        { // Success - continue
            DEBUGGERMSG (KDZONE_API, (L"  KdpSendOsAxsResponse: Send Next Response Block command\r\n"));
            fSuccess = TRUE;
        }
    }
    else
    { // Cancel - retry
        DEBUGGERMSG (KDZONE_ALERT, (L"  KdpSendOsAxsResponse: unexpected command while waiting for Next Response Block command\r\n"));
        DEBUGGERMSG (KDZONE_ALERT, (L"  KdpSendOsAxsResponse: OsAxsVersion=%i, OsAxsApi=%i, NewDumpType=%i, OldDumpType=%i, NewSegIdx=%08X, OldSegIdx=%08X\r\n", 
        g_dbgOsAxsCmdPacket2.dwVersion, g_dbgOsAxsCmdPacket2.dwApi, g_dbgOsAxsCmdPacket2.u.GetWatsonOTF.dwDumpType,
        pOtfResponse->dwDumpType, (DWORD) g_dbgOsAxsCmdPacket2.u.GetWatsonOTF.dw64SegmentIndex,
        (DWORD) (pOtfResponse->dw64SegmentIndex + pOtfResponse->dwSegmentBufferSize)));
        // Copy g_dbgCmdPacket2/MessageHeader to g_dbgCmdPacket/g_MessageHeader
        KD_ASSERT (g_MessageHeader.MaximumLength == MessageHeader.MaximumLength);
        g_MessageHeader.Length = MessageHeader.Length;
        memcpy (g_MessageHeader.Buffer, MessageHeader.Buffer, MessageHeader.Length);
    }

Exit:
    return fSuccess;
}


// TODO: Relocate this to OSAXST0
static void KdpCallOsAccess (OSAXS_COMMAND *pOsAxsCmd, PSTRING pAdditionalData) // TODO: this should be moved to OSAXS
{
    HRESULT hr;
    STRING ResponseHeader;

    DEBUGGERMSG (KDZONE_API, (L"++KdpCallOsAccess\r\n"));
    
    ResponseHeader.Length = sizeof (*pOsAxsCmd);
    ResponseHeader.Buffer = (BYTE *) pOsAxsCmd;
    
    if (pOsAxsCmd->dwVersion >= OSAXS_PROTOCOL_LATEST_VERSION_TSBC_WITH)
    {
        DBGRETAILMSG (KDZONE_API, (L"  KdpCallOsAccess: API = %d\r\n", pOsAxsCmd->dwApi));
        switch (pOsAxsCmd->dwApi)
        {
            case OSAXS_API_GET_FLEXIPTMINFO:
                {
                    OSAXSFN_GETFLEXIPTMINFO a = {0};
                    DEBUGGERMSG(KDZONE_API, (L"  KdpCallOsAccess -> GetFPTMI\r\n"));

                    a.pRequest       = &pOsAxsCmd->u.FlexiReq;
                    a.dwBufferSize   = pAdditionalData->MaximumLength;
                    a.pbBuffer       = pAdditionalData->Buffer;
                    
                    hr = Hdstub.pfnCallClientIoctl (OSAXST0_NAME, OSAXST0_IOCTL_GET_FLEXIPTMINFO, &a);

                    if (SUCCEEDED (hr))
                    {
                        pAdditionalData->Length = (USHORT) a.dwBufferSize;
                    }
                    else
                    {
                        DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdpCallOsAccess: Failed to get FlexiPTMI info from OsAxsT0, hr = 0x%08X\r\n"),hr));
                    }
                       
                }
                break;
            
            case OSAXS_API_GET_WATSON_DUMP_FILE:
            case OSAXS_API_GET_WATSON_DUMP_FILE_COMPAT:
                {
                    OSAXSFN_GETWATSONDUMP a = {0};
                    OSAXS_KDBG_RESPONSE_FUNC pKdpSendOsAxsResponse = &KdpSendOsAxsResponse;

                    DEBUGGERMSG(KDZONE_API, (L"  KdpCallOsAccess -> Generate Watson Dump on the fly\r\n"));

                    a.pWatsonOtfRequest = &pOsAxsCmd->u.GetWatsonOTF;
                    a.pbOutBuffer       = pAdditionalData->Buffer;
                    a.dwBufferSize      = pAdditionalData->MaximumLength;
                    a.pfnResponse       = &KdpSendOsAxsResponse;

                    hr = Hdstub.pfnCallClientIoctl (OSAXST0_NAME, OSAXST0_IOCTL_GET_WATSON_DUMP, &a);
                    if (SUCCEEDED (hr))
                    {
                        pAdditionalData->Length = (USHORT) a.dwBufferSize;
                        pOsAxsCmd->u.GetWatsonOTF.fDumpfileComplete = TRUE; // Force this since we exit the IOCTL
                    }
                    else
                    {
                        DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdpCallOsAccess: Failed to Generate Watson Dump on the fly in OsAxsT0, hr = 0x%08X\r\n"),hr));
                    }
                       
                }
                break;
            
#if defined(x86)
            case OSAXS_API_GET_EXCEPTION_REGISTRATION:
                {
                    DEBUGGERMSG(KDZONE_API, (L"  KdpCallOsAccess:  OSAXST1_IOCTL_GET_EXCEPTION_REGISTRATION\r\n"));
                    hr = Hdstub.pfnCallClientIoctl (OSAXST1_NAME, OSAXST1_IOCTL_GET_EXCEPTION_REGISTRATION, &pOsAxsCmd->u.ExReg);
                    if (SUCCEEDED (hr))
                    {
                        pAdditionalData->Length = 0;
                    }
                    else
                    {
                        DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdpCallOsAccess: Failed to get exception registration from OsAxsT1, hr = 0x%08X\r\n"),hr));
                    }
                    break;
                }
#endif
            case OSAXS_API_SWITCH_PROCESS_VIEW:
                {
                    DEBUGGERMSG(KDZONE_API, (L"  KdpCallOsAccess: SwitchProcessView to 0x%08x\r\n", pOsAxsCmd->u.Addr));
                    g_pFocusProcOverride = (PPROCESS) (DWORD) pOsAxsCmd->u.Addr;
                    hr = S_OK;
                    break;
                }

            case OSAXS_API_TRANSLATE_ADDRESS:
                {
                    OSAXSFN_TRANSLATEADDRESS a = {0};

                    a.hProcess   = (HANDLE) pOsAxsCmd->u.TranslateAddress.dwProcessHandle;
                    a.pvVirtual  = (void *) pOsAxsCmd->u.TranslateAddress.Address;
                    a.fReturnKVA = pOsAxsCmd->u.TranslateAddress.fReturnKVA;

                    DEBUGGERMSG(KDZONE_API, (L"  KdpCallOsAccess:  OSAXS_API_TRANSLATE_ADDRESS\r\n"));
                    hr = Hdstub.pfnCallClientIoctl(OSAXST1_NAME, OSAXST1_IOCTL_TRANSLATE_ADDRESS, &a);
                    if (SUCCEEDED(hr))
                    {
                        pOsAxsCmd->u.TranslateAddress.PhysicalAddrOrKVA = (DWORD)a.pvTranslated;
                    }
                    break;
                }

            case OSAXS_API_TRANSLATE_HPCI:
                {
                    OSAXSFN_TRANSLATEHPCI a = {0};
                    DEBUGGERMSG(KDZONE_API, (L"  KdpCallOsAccess:  OSAXS_API_TRANSLATE_HPCI\r\n"));

                    a.handle          = (HANDLE) pOsAxsCmd->u.Handle.dwHandle;
                    a.dwProcessHandle = pOsAxsCmd->u.Handle.dwProcessHandle;

                    hr = Hdstub.pfnCallClientIoctl(OSAXST1_NAME, OSAXST1_IOCTL_TRANSLATE_HPCI, &a);
                    if (SUCCEEDED(hr))
                    {
                        memcpy(pAdditionalData->Buffer, a.pcinfo, sizeof(CINFO));
                        pAdditionalData->Length = (USHORT) sizeof(CINFO);
                    }
                    break;
                }

            case OSAXS_API_GET_HDATA:
            {
                OSAXSFN_GETHDATA a = {0};

                DEBUGGERMSG(KDZONE_API, (L"  KdpCallOsAccess:  OSAXS_API_GET_HDATA\r\n"));
                a.handle = (HANDLE)pOsAxsCmd->u.Handle.dwHandle;
                a.dwProcessHandle = pOsAxsCmd->u.Handle.dwProcessHandle;

                hr = Hdstub.pfnCallClientIoctl(OSAXST1_NAME, OSAXST1_IOCTL_GET_HDATA, &a);
                if (SUCCEEDED(hr))
                {
                    memcpy(pAdditionalData->Buffer, a.phdata, sizeof(*a.phdata));
                    pAdditionalData->Length = (USHORT) sizeof(*a.phdata);
                }
                else
                {
                    DEBUGGERMSG(KDZONE_ALERT, (L"  KdpCallOsAccess: Failed in call to GET_HDATA, hr=%08X\r\n", hr));
                }
                break;
            }

            case OSAXS_API_CALLSTACK:
                {
                    OSAXSFN_CALLSTACK a = {0};
                    DEBUGGERMSG(KDZONE_API, (L"  KdpCallOsAccess:  OSAXS_API_CALLSTACK\r\n"));

                    a.hThread = (HANDLE)pOsAxsCmd->u.CallStack.hThread;
                    a.FrameStart = pOsAxsCmd->u.CallStack.FrameStart;
                    a.FramesToRead = pOsAxsCmd->u.CallStack.FramesToRead;
                    a.FrameBuffer = pAdditionalData->Buffer;
                    a.FrameBufferLength = pAdditionalData->MaximumLength;

                    hr = Hdstub.pfnCallClientIoctl(OSAXST1_NAME, OSAXST1_IOCTL_CALLSTACK, &a);
                    if (SUCCEEDED(hr))
                    {
                        pOsAxsCmd->u.CallStack.FramesReturned = a.FramesReturned;
                        pAdditionalData->Length = (USHORT) a.FrameBufferLength;
                    }
                    break;
                }
            
            default:
                DEBUGGERMSG (KDZONE_API, (L"  KdpCallOsAccess: Invalid API Number %d\r\n", pOsAxsCmd->dwApi));
                hr = OSAXS_E_APINUMBER;
                break;
        }
    }
    else
    {
        DEBUGGERMSG(KDZONE_ALERT, (L"  KdpCallOsAccess: Protocol mismatch\r\n"));
        hr = OSAXS_E_PROTOCOLVERSION;
    }

    // Manufacture response
    pOsAxsCmd->hr = hr;
    pOsAxsCmd->dwVersion = OSAXS_PROTOCOL_LATEST_VERSION;
    KdpSendPacket (PACKET_TYPE_KD_CMD, GUID_KDDBGCLIENT_OSAXS, &ResponseHeader, pAdditionalData);
    DBGRETAILMSG (KDZONE_API, (L"  KdpCallOsAccess: Responded\r\n"));

    DEBUGGERMSG (KDZONE_API, (L"--KdpCallOsAccess\r\n"));
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
    
    g_MessageHeader.MaximumLength = sizeof (g_dbgCmdPacket);
    g_MessageHeader.Buffer = (PCHAR) &g_dbgCmdPacket;
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
        KD_ASSERT (sizeof (g_dbgCmdPacket) == g_MessageHeader.MaximumLength);
        KD_ASSERT (KDP_MESSAGE_BUFFER_SIZE == MessageData.MaximumLength);

        DBGRETAILMSG (KDZONE_API, (L"  KdpSendNotifAndDoCmdLoop: Waiting for command\r\n"));

        ReturnCode = KdpReceiveCmdPacket(
                        &g_MessageHeader,
                        &MessageData,
                        &Length,
                        &guidClient
                        );
        if (ReturnCode == (USHORT) KDP_PACKET_RESEND)
        { // This means the host disconnected and reconnected
            DEBUGGERMSG(KDZONE_TRAP, (L"  KdpSendNotifAndDoCmdLoop: RESENDING NOTIFICATION\r\n"));
            KdpResetBps ();
            goto ResendPacket;
        }

    DecodeReceivedPacket:
        if (!memcmp (&guidClient, &GUID_KDDBGCLIENT_KDSTUB, sizeof (GUID)))
        {
            // Insert here flags of supported feature based on host version (KDAPI_PROTOCOL_VERSION)
            if (g_dbgkdCmdPacket.dwSubVersionId >= KDAPI_PROTOCOL_VERSION_TSBC_WITH)
            {
                // Switch on the return message API number.
                DBGRETAILMSG (KDZONE_API, (L" KdpSendNotifAndDoCmdLoop: Got Api %8.8lx\r\n", g_dbgkdCmdPacket.dwApiNumber));

                if(g_dbgkdCmdPacket.dwSubVersionId < KDAPI_PROTOCOL_VERSION_PROCTHDBP)
                {
                    DEBUGGERMSG(KDZONE_API, (L"Older PB, turning on Breakpoint Workaround.\r\n"));
                    g_fWorkaroundPBForVMBreakpoints = TRUE;
                }

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
                    case DbgKdReadIoSpaceApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdReadIoSpaceApi\r\n"));
                        KdpReadIoSpace (&g_dbgkdCmdPacket, &MessageData, TRUE);
                        break;
                    case DbgKdWriteIoSpaceApi:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdWriteIoSpaceApi\r\n"));
                        KdpWriteIoSpace (&g_dbgkdCmdPacket, &MessageData, TRUE);
                        break;
                    case DbgKdContinueApi:
                        DEBUGGERMSG(KDZONE_API, (L"DbgKdContinueApi\r\n"));
                        DBGRETAILMSG (KDZONE_API, (L"Continuing target system %i\r\n", g_dbgkdCmdPacket.u.Continue.dwContinueStatus));
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
                        DEBUGGERMSG (KDZONE_ALERT, (L"KD: Stopping kernel debugger software probe. Use loaddbg.exe to restart.\r\n"));
                        break;
                    case DbgKdManipulateBreakpoint:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdManipulateBreakpoint\r\n"));
                        KdpManipulateBreakPoint (&g_dbgkdCmdPacket, &MessageData);
                        break;
                    case DbgKdAdvancedCmd:
                        DEBUGGERMSG( KDZONE_API, (L"DbgKdAdvancedCmd\r\n"));
                        KdpAdvancedCmd (&g_dbgkdCmdPacket, &MessageData);
                        break;
                    default: // Invalid message.
                        DBGRETAILMSG ( KDZONE_ALERT, (L"  KdpSendNotifAndDoCmdLoop: Unknown API??\r\n"));
                        MessageData.Length = 0;
                        g_dbgkdCmdPacket.dwReturnStatus = STATUS_UNSUCCESSFUL;
                        KdpSendKdApiCmdPacket (&g_MessageHeader, &MessageData);
                        break;
                }
            }
            else
            {
                DBGRETAILMSG ( KDZONE_ALERT, (L"  KdpSendNotifAndDoCmdLoop: protocol Version not supported\r\n"));
                MessageData.Length = 0;
                g_dbgkdCmdPacket.dwReturnStatus = STATUS_UNSUCCESSFUL;
                KdpSendKdApiCmdPacket (&g_MessageHeader, &MessageData);
            }
        }
        else if (!memcmp (&guidClient, &GUID_KDDBGCLIENT_OSAXS, sizeof (GUID)))
        {
            DEBUGGERMSG (KDZONE_API, (L"Calling OsAccess\r\n"));
            g_fResendNotifPacket = FALSE;
            g_fValidOsAxsCmdPacket = TRUE; // Preset
            KdpCallOsAccess (&g_dbgOsAxsCmdPacket, &MessageData);
            if (g_fResendNotifPacket)
            {
                DEBUGGERMSG(KDZONE_TRAP, (L"  KdpSendNotifAndDoCmdLoop: RESENDING NOTIFICATION while in nested response\r\n"));
                KdpResetBps ();
                goto ResendPacket;
            }
            if (!g_fValidOsAxsCmdPacket)
            {
                DEBUGGERMSG( KDZONE_ALERT, (L"  KdpSendNotifAndDoCmdLoop: Invalid OsAxs Cmd Packet. Other command received.\r\n"));
                goto DecodeReceivedPacket;
                
            }
        }
    }
    return fHandled;
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
    CONTEXT * pctxException = g_exceptionInfo.ExceptionPointers.ContextRecord;

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
    memcpy (pctxException, AdditionalData->Buffer, sizeof (CONTEXT));

    // copy the DSP registers into the thread context
#if defined(SHx) && !defined(SH4) && !defined(SH3e)
    // copy over the DSP registers from the thread context
    fOwner = (pCurThread == g_CurDSPOwner);
    DSPFlushContext();
    // if DSPFlushContext updated pCurThread's PSR, keep exception context in sync
    if (fOwner) pctxException->Psr &= ~SR_DSP_ENABLED;
    memcpy (&(pCurThread->ctx.DSR), &(pctxException->DSR), sizeof (DWORD) * 13);
#endif

    // copy the floating point registers into the thread context
#if defined(SH4)
    fOwner = (pCurThread == g_CurFPUOwner);
    FPUFlushContext();
    // if FPUFlushContext updated pCurThread's PSR, keep exception context in sync
    if (fOwner) pctxException->Psr |= SR_FPU_DISABLED;
    memcpy (&(pCurThread->ctx.Fpscr), &(pctxException->Fpscr), sizeof (DWORD) * 34);
#elif defined(MIPS_HAS_FPU)
    FPUFlushContext();
    pCurThread->ctx.Fsr = pctxException->Fsr;
    memcpy (&(pCurThread->ctx.FltF0), &(pctxException->FltF0), sizeof (FREG_TYPE) * 32);
#elif defined(ARM)
    // ARM VFP10 Support
    // FPUFlushContext might modify FpExc, but apparently it can't be restored, so we shouldn't bother
    // trying update our context with it
    FPUFlushContext ();
    memcpy (&(pCurThread->ctx.Fpscr), &(pctxException->Fpscr), sizeof (DWORD) * 43);

    if(HasWMMX()) SetWMMXRegisters(pConcanRegs);
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
    CONTEXT * pctxException = g_exceptionInfo.ExceptionPointers.ContextRecord;

    memset (pdbgNotifPacket, 0, sizeof (*pdbgNotifPacket)); // zero init

    //  Set up description of event, including exception record
    pdbgNotifPacket->dwNewState = DbgKdExceptionNotif;
    pdbgNotifPacket->NbBpAvail.dwNbHwCodeBpAvail = 0; // TODO: Get this from OAL
    pdbgNotifPacket->NbBpAvail.dwNbSwCodeBpAvail = BREAKPOINT_TABLE_SIZE - g_nTotalNumDistinctSwCodeBps;
    pdbgNotifPacket->NbBpAvail.dwNbHwDataBpAvail = 0; // TODO: Get this from OAL
    pdbgNotifPacket->NbBpAvail.dwNbSwDataBpAvail = 0;

    pdbgNotifPacket->TgtVerInfo.dwCpuFamily = TARGET_CODE_CPU;
    pdbgNotifPacket->TgtVerInfo.dwBuildNumber = VER_PRODUCTBUILD; // TODO: Get the real build
    pdbgNotifPacket->TgtVerInfo.wMajorOsVersion = 6;
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

    // Find breakpoint handle, or return 0.
    pdbgNotifPacket->dwBreakpointHandle = KdpFindBreakpoint((void*)CONTEXT_TO_PROGRAM_COUNTER(pctxException));

#if defined (ARM)
    if(HasWMMX()) pdbgNotifPacket->TgtVerInfo.dwCpuCapablilityFlags |= DBGKD_VERS_FLAG_MULTIMEDIA;
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
    pctxException->DebugRegisters.BarA  = READ_REGISTER_ULONG(UBCBarA);
    pctxException->DebugRegisters.BasrA = READ_REGISTER_UCHAR(UBCBasrA);
    pctxException->DebugRegisters.BamrA = READ_REGISTER_UCHAR(UBCBamrA);
    pctxException->DebugRegisters.BbrA  = READ_REGISTER_USHORT(UBCBbrA);
    pctxException->DebugRegisters.BarB  = READ_REGISTER_ULONG(UBCBarB);
    pctxException->DebugRegisters.BasrB = READ_REGISTER_UCHAR(UBCBasrB);
    pctxException->DebugRegisters.BamrB = READ_REGISTER_UCHAR(UBCBamrB);
    pctxException->DebugRegisters.BbrB  = READ_REGISTER_USHORT(UBCBbrB);
    pctxException->DebugRegisters.BdrB  = READ_REGISTER_ULONG(UBCBdrB);
    pctxException->DebugRegisters.BdmrB = READ_REGISTER_ULONG(UBCBdmrB);
    pctxException->DebugRegisters.Brcr  = READ_REGISTER_USHORT(UBCBrcr);
    pctxException->DebugRegisters.Align = 0;
#endif

    //  Copy context record immediately following instruction stream
    pdbgNotifPacket->u.Exception.Context = *pctxException;

#ifdef ARM
    if(HasWMMX()) GetWMMXRegisters(&pdbgNotifPacket->u.Exception.ConcanRegs);
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


// TODO: implement this if PB need read or write to phys mem directly
// TODO: Reserve a vm page for KdStub to point to working phys page and copy page per page
#define MmDbgTranslatePhysicalAddress(Address) (Address)


typedef DWORD PHYSICAL_ADDRESS;


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
        NumberBytes = (USHORT)(VM_PAGE_SIZE - BYTE_OFFSET(VirtualAddress));
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
            NumberBytes = (USHORT) ((VM_PAGE_SIZE < BytesLeft) ? VM_PAGE_SIZE : BytesLeft);

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

    // Note: This is not used by PB at this time - implement MmDbgTranslatePhysicalAddress() if this becomes the case

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
        NumberBytes = (USHORT) (VM_PAGE_SIZE - BYTE_OFFSET(VirtualAddress));
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
            NumberBytes = (USHORT) ((VM_PAGE_SIZE < BytesLeft) ? VM_PAGE_SIZE : BytesLeft);

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

VOID
KdpAdvancedCmd(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    )

/*++

Routine Description:

    This function is called in response of a advanced debugger
    command message. Its function is to perform the command and return.

Arguments:

    pdbgkdCmdPacket - Supplies the command message.

    AdditionalData - Supplies any additional data for the message.

Return Value:

    None.

--*/

{
    DBGKD_ADVANCED_CMD *a = &pdbgkdCmdPacket->u.AdvancedCmd;
    STRING MessageHeader;

    DEBUGGERMSG(KDZONE_API, (L"++KdpAdvancedCmd: (Len=%i,MaxLen=%i)\r\n", AdditionalData->Length, AdditionalData->MaximumLength));

    MessageHeader.Length = sizeof (*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR) pdbgkdCmdPacket;

    if (ProcessAdvancedCmd(AdditionalData) && (AdditionalData->Length < KDP_MESSAGE_BUFFER_SIZE))
    {
        a->dwResponseSize = AdditionalData->Length;
        pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
    }
    else
    {
        pdbgkdCmdPacket->dwReturnStatus = STATUS_UNSUCCESSFUL;
    }

    KdpSendKdApiCmdPacket (&MessageHeader, (pdbgkdCmdPacket->dwReturnStatus == STATUS_SUCCESS) ? AdditionalData : NULL);

    DEBUGGERMSG(KDZONE_API, (L"--KdpAdvancedCmd: status=%u, Length=%u\r\n", pdbgkdCmdPacket->dwReturnStatus, AdditionalData->Length));
}


