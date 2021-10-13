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

    kdapi.c

Abstract:

    Implementation of Kernel Debugger portable remote APIs.

--*/

#include "kdp.h"


DWORD NkCEProcessorType; // Temp change [GH]: was CEProcessorType

DWORD g_HostKdstubProtocolVersion = KDAPI_PROTOCOL_VERSION_TSBC_WITH;

BOOL g_fResendNotifPacket = FALSE;
BOOL g_fValidOsAxsCmdPacket = FALSE;

void KdpResetBps (void)
{
    if (OEMKDIoControl)
    {
        DD_IoControl(KD_IOCTL_INIT, NULL, 0);
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

    if (dwMessageSize >= KDP_MESSAGE_BUFFER_SIZE)
    {
        DEBUGGERMSG(KDZONE_ALERT, (L"  KdpSendDbgMessage: Message size (%u) larger than max allowed (%u)\r\n", dwMessageSize, KDP_MESSAGE_BUFFER_SIZE));
        return;
    }

    MessageHeader.Length = (USHORT)dwMessageSize;
    MessageHeader.Buffer = (PCHAR) pszMessage;

    KdpSendPacket (PACKET_TYPE_KD_DBG_MSG, GUID_KDDBGCLIENT_KDSTUB, &MessageHeader, NULL);
}


BOOL KdpHandleKdApi(STRING *MessageData, BOOL *ignoreException)
{
    STRING ResponseHeader;
    DBGKD_COMMAND dbgkdCmdPacket;
    BOOL fExit;

    ResponseHeader.MaximumLength = ResponseHeader.Length = sizeof(dbgkdCmdPacket);
    ResponseHeader.Buffer = (char *)&dbgkdCmdPacket;

    fExit = FALSE;
    KDASSERT(MessageData->Length >= sizeof(dbgkdCmdPacket));
    if (MessageData->Length >= sizeof(dbgkdCmdPacket))
    {
        memcpy(&dbgkdCmdPacket, MessageData->Buffer, sizeof(dbgkdCmdPacket));
        MessageData->Length -= sizeof(dbgkdCmdPacket);
        memmove(MessageData->Buffer, MessageData->Buffer + sizeof(dbgkdCmdPacket),
                MessageData->Length);
    
        // Insert here flags of supported feature based on host version (KDAPI_PROTOCOL_VERSION)
        if (dbgkdCmdPacket.dwSubVersionId >= KDAPI_PROTOCOL_VERSION_TSBC_WITH)
        {
            g_HostKdstubProtocolVersion = dbgkdCmdPacket.dwSubVersionId;
            
            // Switch on the return message API number.
            DBGRETAILMSG (KDZONE_API, (L" KdpSendNotifAndDoCmdLoop: Got Api %8.8lx\r\n", dbgkdCmdPacket.dwApiNumber));
            switch (dbgkdCmdPacket.dwApiNumber)
            {
                case DbgKdReadVirtualMemoryApi:
                    KdpReadVirtualMemory (&dbgkdCmdPacket, MessageData);
                    break;
                case DbgKdWriteVirtualMemoryApi:
                    DEBUGGERMSG( KDZONE_API, (L"DbgKdWriteVirtualMemoryApi\r\n"));
                    KdpWriteVirtualMemory (&dbgkdCmdPacket, MessageData);
                    break;
                case DbgKdReadPhysicalMemoryApi:
                    DEBUGGERMSG( KDZONE_API, (L"DbgKdReadPhysicalMemoryApi\r\n"));
                    KdpReadPhysicalMemory (&dbgkdCmdPacket, MessageData);
                    break;
                case DbgKdWritePhysicalMemoryApi:
                    DEBUGGERMSG( KDZONE_API, (L"DbgKdWritePhysicalMemoryApi\r\n"));
                    KdpWritePhysicalMemory (&dbgkdCmdPacket, MessageData);
                    break;
                case DbgKdSetContextApi:
                    DEBUGGERMSG( KDZONE_API, (L"DbgKdSetContextApi\r\n"));
                    KdpSetContext(&dbgkdCmdPacket,MessageData);
                    break;
                case DbgKdReadIoSpaceApi:
                    DEBUGGERMSG( KDZONE_API, (L"DbgKdReadIoSpaceApi\r\n"));
                    KdpReadIoSpace (&dbgkdCmdPacket, MessageData, TRUE);
                    break;
                case DbgKdWriteIoSpaceApi:
                    DEBUGGERMSG( KDZONE_API, (L"DbgKdWriteIoSpaceApi\r\n"));
                    KdpWriteIoSpace (&dbgkdCmdPacket, MessageData, TRUE);
                    break;
                case DbgKdContinueApi:
                    DEBUGGERMSG(KDZONE_API, (L"DbgKdContinueApi\r\n"));
                    *ignoreException = KdpContinue(&dbgkdCmdPacket, MessageData);
                    fExit = TRUE;
                    break;
                case DbgKdTerminateApi:
                    DEBUGGERMSG( KDZONE_API, (L"DbgKdTerminateApi\r\n"));
                    KdpResetBps ();
                    // Kernel specific clean ups...
                    KDCleanup();
                    // tell KdpTrap to shutdown
                    KdpEnable(FALSE);
                    *ignoreException = TRUE;
                    fExit = TRUE;
                    DEBUGGERMSG (KDZONE_ALERT, (L"KD: Stopping kernel debugger software probe. Use loaddbg.exe to restart.\r\n"));
                    break;
                case DbgKdManipulateBreakpoint:
                    DEBUGGERMSG( KDZONE_API, (L"DbgKdManipulateBreakpoint\r\n"));
                    KdpManipulateBreakPoint (&dbgkdCmdPacket, MessageData);
                    break;
                case DbgKdAdvancedCmd:
                    DEBUGGERMSG( KDZONE_API, (L"DbgKdAdvancedCmd\r\n"));
                    KdpAdvancedCmd (&dbgkdCmdPacket, MessageData);
                    break;
                default: // Invalid message.
                    DBGRETAILMSG ( KDZONE_ALERT, (L"  KdpSendNotifAndDoCmdLoop: Unknown API??\r\n"));
                    MessageData->Length = 0;
                    dbgkdCmdPacket.dwReturnStatus = (DWORD) STATUS_UNSUCCESSFUL;
                    KdpSendKdApiCmdPacket (&ResponseHeader, MessageData);
                    break;
            }
        }
        else
        {
            DBGRETAILMSG ( KDZONE_ALERT, (L"  KdpSendNotifAndDoCmdLoop: protocol Version not supported\r\n"));
            MessageData->Length = 0;
            dbgkdCmdPacket.dwReturnStatus = (DWORD) STATUS_UNSUCCESSFUL;
            KdpSendKdApiCmdPacket (&ResponseHeader, MessageData);
        }
    }

    return fExit;
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

BOOL KdpSendNotifAndDoCmdLoop (
    IN PSTRING OutMessageHeader,
    IN PSTRING OutMessageData OPTIONAL
    )
{
    ULONG Length;
    STRING MessageData;
    USHORT ReturnCode;
    BOOL fHandled = FALSE;
    BOOL fExit = FALSE;
    BOOL fWaitForCommand;

    KDASSERT(g_fKdbgRegistered);

    // Loop servicing command message until a continue message is received.
    
    MessageData.MaximumLength = KDP_MESSAGE_BUFFER_SIZE;
    MessageData.Buffer = (PCHAR) g_abMessageBuffer;

    // Send event notification packet to debugger on host.  Come back
    // here any time we see a breakin sequence.
    KdpSendPacket (PACKET_TYPE_KD_NOTIF, GUID_KDDBGCLIENT_KDSTUB, OutMessageHeader, OutMessageData);

    while (!fExit)
    {
        GUID guidClient;

        // Make sure these two MaximumLength members never change while in break state
        KD_ASSERT (KDP_MESSAGE_BUFFER_SIZE == MessageData.MaximumLength);

        do
        {
            DBGRETAILMSG(KDZONE_API, (L"  KdpSendNotifAndDoCmdLoop: Waiting for command\r\n"));
            fWaitForCommand = FALSE;
            ReturnCode = KdpReceivePacket(
                            &MessageData,
                            &Length,
                            &guidClient
                            );
            if (ReturnCode == (USHORT) KDP_PACKET_RESEND)
            { // This means the host disconnected and reconnected
                DEBUGGERMSG(KDZONE_TRAP, (L"  KdpSendNotifAndDoCmdLoop: RESENDING NOTIFICATION\r\n"));
                KdpResetBps ();
                KdpSendPacket(PACKET_TYPE_KD_NOTIF, GUID_KDDBGCLIENT_KDSTUB, OutMessageHeader, OutMessageData);
                fWaitForCommand = TRUE;
            }
        } while (fWaitForCommand);

        if (!memcmp(&guidClient, &GUID_KDDBGCLIENT_KDSTUB, sizeof (GUID)))
        {
            fExit = DD_HandleKdApi(&MessageData, &fHandled);
        }
        else if (!memcmp(&guidClient, &GUID_KDDBGCLIENT_OSAXS, sizeof(GUID)))
        {
            DD_HandleOsAxsApi(&MessageData);
        }
        else
        {
            DEBUGGERMSG(KDZONE_ALERT,(TEXT("KDAPI:Unsupported client type\r\n")));
        }
    }

    DBGRETAILMSG(KDZONE_API, (L"--KdpSendNotifAndDoCmdLoop: %d\r\n", fHandled));
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
    MEMADDR dest, src;

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

    DEBUGGERMSG( KDZONE_API, (L"%08X:%08X:DbgKdReadVirtualMemoryApi : %08X, %d bytes\r\n",
                              PcbGetActvProcId(),
                              PcbGetCurThreadId(),
                              (DWORD) a->qwTgtAddress, ulLengthAtempted));

    // Perform the actual memory read, if some locations are not readable, the read will be truncated
    SetDebugAddr(&dest, AdditionalData->Buffer);
    SetVirtAddr(&src, DD_GetProperVM(NULL, (void*)(DWORD)a->qwTgtAddress), (void*)(DWORD)a->qwTgtAddress);
    AdditionalData->Length = (USHORT)DD_MoveMemory0(&dest, &src, ulLengthAtempted);
    if (ulLengthAtempted == AdditionalData->Length)
    {
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_SUCCESS;
    }
    else
    {
        DEBUGGERMSG( KDZONE_API, (L"  KdpReadVirtualMemory: Only read %d of %d bytes starting at 0x%8.8x\r\n", AdditionalData->Length, ulLengthAtempted, a->qwTgtAddress));
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_UNSUCCESSFUL;
    }

    // Sanitize (remove SW BP artefacts) the memory block data before sending it back
    DEBUGGERMSG (KDZONE_VIRTMEM, (L"  KdpReadVirtualMemory: Sanitizing %d bytes starting at 0x%8.8x\r\n", AdditionalData->Length, a->qwTgtAddress));
    DD_Sanitize (AdditionalData->Buffer, &src, AdditionalData->Length);

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
    MEMADDR dest, src;

    MessageHeader.Length = sizeof (*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR) pdbgkdCmdPacket;

    SetVirtAddr(&dest, DD_GetProperVM(NULL, (void*)(DWORD)a->qwTgtAddress), (void*)(DWORD)a->qwTgtAddress);
    SetDebugAddr(&src, AdditionalData->Buffer);
    ulLengthWritten = DD_MoveMemory0(&dest, &src, AdditionalData->Length);
    if (ulLengthWritten == AdditionalData->Length)
    {
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_SUCCESS;
    }
    else
    {
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_UNSUCCESSFUL;
    }

    a->dwActualBytesWritten = ulLengthWritten;

    KdpSendKdApiCmdPacket (&MessageHeader, NULL);
}


VOID
KdpGetContext(
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    )
{
    STRING MessageHeader;
    HRESULT hr;

    MessageHeader.Length = sizeof(*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR)pdbgkdCmdPacket;

#if defined (ARM)
    AdditionalData->Length = sizeof(CONTEXT) + sizeof(CONCAN_REGS);
#else
    AdditionalData->Length = sizeof(CONTEXT);
#endif

    hr = DD_GetCpuContext(pdbgkdCmdPacket->u.Context.dwCpuNumber,
                          (PCONTEXT)AdditionalData->Buffer,
                          AdditionalData->Length);
    if (SUCCEEDED(hr))
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_SUCCESS;
    else
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_INVALID_PARAMETER;
    
    KdpSendKdApiCmdPacket(&MessageHeader, AdditionalData);
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
    HRESULT hr;
    
    MessageHeader.Length = sizeof(*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR)pdbgkdCmdPacket;

    hr = DD_SetCpuContext(pdbgkdCmdPacket->u.Context.dwCpuNumber,
                          (PCONTEXT)AdditionalData->Buffer,
                          AdditionalData->Length);
    if (SUCCEEDED(hr))
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_SUCCESS;
    else
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_INVALID_PARAMETER;
    
    KdpSendKdApiCmdPacket (&MessageHeader, NULL);
}


VOID
KdpSetNotifPacket (
    IN DBGKD_NOTIF *pdbgNotifPacket,
    IN EXCEPTION_RECORD *pExceptionRecord,
    IN BOOL SecondChance
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
    CONTEXT * pctxException = DD_ExceptionState.context;
    // DWORD i;
    
    DEBUGGERMSG(KDZONE_API,(L"++KdpSetNotifPacket\r\n"));

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
    if (IsFPUPresent())
    { // hardware FPU support used
        pdbgNotifPacket->TgtVerInfo.dwCpuCapablilityFlags |= DBGKD_VERS_FLAG_FPU;
    }
#pragma warning(suppress:4127)
    if (IsFPUPresentEx())
    { // Extended FPU (ex. VFPv3 with extended register set)
        pdbgNotifPacket->TgtVerInfo.dwCpuCapablilityFlags |= DBGKD_VERS_FLAG_FPU_EX;
    }
    

    // Find breakpoint handle, or return 0.
    pdbgNotifPacket->dwBreakpointHandle = KdpFindBreakpoint((void*)CONTEXT_TO_PROGRAM_COUNTER(pctxException));

    if(0 == pdbgNotifPacket->dwBreakpointHandle)
    {
        if((STATUS_ACCESS_VIOLATION == DD_ExceptionState.exception->ExceptionCode) && 
            (DD_ExceptionState.exception->NumberParameters >= AV_EXCEPTION_PARAMETERS))
        {
            BOOL fWrite = DD_ExceptionState.exception->ExceptionInformation[AV_EXCEPTION_ACCESS];
            DWORD dwAddress = DD_ExceptionState.exception->ExceptionInformation[AV_EXCEPTION_ADDRESS];
            PPROCESS pVM = (dwAddress > VM_SHARED_HEAP_BASE ? g_pprcNK : GetPCB()->pVMPrc);                

            pdbgNotifPacket->dwBreakpointHandle = DD_FindDataBreakpoint(pVM, dwAddress, fWrite);
        }
    }
    DD_DisableAllDataBreakpoints();

#if defined (ARM)
    if(HasWMMX()) pdbgNotifPacket->TgtVerInfo.dwCpuCapablilityFlags |= DBGKD_VERS_FLAG_MULTIMEDIA;
#endif
    pdbgNotifPacket->dwSubVersionId = KDAPI_PROTOCOL_VERSION;
    pdbgNotifPacket->wNumberOfCpus = (WORD) (g_pKData->nCpus & 0xFFFF);
    pdbgNotifPacket->wCurrentCpuId = (WORD) (PcbGetCurCpu () & 0xFFFF);
    pdbgNotifPacket->dwOsAxsDataBlockOffset = (DWORD) g_pKData->pOsAxsDataBlock;

    if (g_fForceReload)
    {
        pdbgNotifPacket->dwKdpFlags |= DBGKD_STATE_DID_RESET;
    }
    else if (DD_fIgnoredModuleNotifications)
    {
        if (g_HostKdstubProtocolVersion >= KDAPI_PROTOCOL_VERSION_MODULELIST_REFRESH)
        {
            // Host supports the module list refresh flag. Just refresh the module
            // list.
            pdbgNotifPacket->dwKdpFlags |= DBGKD_STATE_REFRESH_MODULE_LIST;
        }
        else
        {
            // Host does not, send a reset notification to force the entire debugger
            // state to refresh.
            pdbgNotifPacket->dwKdpFlags |= DBGKD_STATE_DID_RESET;
        }
    }

    g_fForceReload = FALSE;
    DD_fIgnoredModuleNotifications = FALSE;
    
    memcpy (&pdbgNotifPacket->u.Exception.ExceptionRecord,
            pExceptionRecord,
            sizeof (EXCEPTION_RECORD));
    pdbgNotifPacket->u.Exception.dwFirstChance = !SecondChance;

    DD_GetCpuContext(0,
                     (PCONTEXT)&pdbgNotifPacket->u.Exception.Context,
#ifdef ARM
                     sizeof(CONTEXT) + sizeof(CONCAN_REGS)
#else
                     sizeof(CONTEXT)
#endif
                     );
    
    DEBUGGERMSG(KDZONE_API,(L"--KdpSetNotifPacket\r\n"));
}


BOOL
KdpReportExceptionNotif (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN BOOL SecondChance
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
    BOOL fHandled;

    DEBUGGERMSG(KDZONE_API,(L"++KdpReportExceptionNotif\r\n"));

    KdpSetNotifPacket (&dbgNotifPacket,
                        ExceptionRecord,
                        SecondChance
                        );

    MessageHeader.Length = sizeof (DBGKD_NOTIF);
    MessageHeader.Buffer = (PCHAR) &dbgNotifPacket;

    MessageData.Length = 0;

    // Send packet to the kernel debugger on the host machine, wait for answer.
    fHandled = KdpSendNotifAndDoCmdLoop (&MessageHeader, &MessageData);
    
    DEBUGGERMSG(KDZONE_API,(L"--KdpReportExceptionNotif\r\n"));
    return fHandled;
}


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
    ULONG ulLengthAtempted;
    STRING MessageHeader;
    MEMADDR dest, src;

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

    SetDebugAddr(&dest, AdditionalData->Buffer);
    SetPhysAddr(&src, (void*)(DWORD)a->qwTgtAddress);

    // Perform the actual memory read, if some locations are not readable, the read will be truncated
    AdditionalData->Length = (USHORT)DD_MoveMemory0(&dest, &src, ulLengthAtempted);
    if (ulLengthAtempted == AdditionalData->Length)
    {
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_SUCCESS;
    }
    else
    {
        DEBUGGERMSG( KDZONE_API, (L"  KdpReadPhysicalMemory: Only read %d of %d bytes starting at 0x%8.8x\r\n", AdditionalData->Length, ulLengthAtempted, a->qwTgtAddress));
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_UNSUCCESSFUL;
    }

#if 0
    // Sanitization only works on a page by page basis with the debugger turning the physical address back to KVA to test.
    // This code path should only be used if the PA can not be translated into a KVA.

    // Sanitize (remove SW BP artefacts) the memory block data before sending it back
    DEBUGGERMSG (KDZONE_VIRTMEM, (L"  KdpReadPhysicalMemory: Sanitizing %d bytes starting at 0x%8.8x\r\n", AdditionalData->Length, a->qwTgtAddress));
    KdpSanitize (AdditionalData->Buffer, (void *) a->qwTgtAddress, AdditionalData->Length, TRUE);
#endif

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
    ULONG ulLengthWritten;
    STRING MessageHeader;
    MEMADDR dest, src;

    MessageHeader.Length = sizeof (*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR) pdbgkdCmdPacket;

    SetPhysAddr(&dest, (void*)(DWORD)a->qwTgtAddress);
    SetDebugAddr(&src, AdditionalData->Buffer);
    ulLengthWritten = DD_MoveMemory0(&dest, &src, AdditionalData->Length);
    if (ulLengthWritten == AdditionalData->Length)
    {
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_SUCCESS;
    }
    else
    {
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_UNSUCCESSFUL;
    }

    a->dwActualBytesWritten = ulLengthWritten;

    KdpSendKdApiCmdPacket (&MessageHeader, NULL);
}

BOOL KdpContinue(DBGKD_COMMAND *pdbgkdCmdPacket, PSTRING AdditionalData)
{
    MEMADDR Address;
    Breakpoint *Bp;
    BOOL IgnoreException;
    ULONG NextPC;
    HRESULT hr;
    PPROCESS pprcVM;
    ULONG pc;
    BOOL fRun = TRUE;
    const ULONG ContinueStatus = pdbgkdCmdPacket->u.Continue.dwContinueStatus;
    const BOOL fSupportSingleStep = (pdbgkdCmdPacket->dwSubVersionId >= KDAPI_PROTOCOL_VERSION_SINGLESTEP);

    DEBUGGERMSG(KDZONE_API, (L"++KdpContinue, %08X\r\n", ContinueStatus));

    DD_EnableAllDataBreakpoints();  //re-enable all data breakpoints.

    if (fSupportSingleStep && 
        (ContinueStatus == DBGKD_SINGLE_STEP_CONTINUE ||
         ContinueStatus == DBGKD_SINGLE_STEP_NOT_HANDLED))
    {
        DEBUGGERMSG(KDZONE_API, (L"  KdpContinue, Single Step + Halt\r\n"));
        IgnoreException = ContinueStatus == DBGKD_SINGLE_STEP_CONTINUE;
        DD_SingleStepThread();
        fRun = FALSE;
    }
    else
    {
        DEBUGGERMSG(KDZONE_API, (L"  KdpContinue, Single Step + Run\r\n"));
        IgnoreException = ContinueStatus == DBG_CONTINUE;
        fRun = TRUE;
    }

    if (fSupportSingleStep)
    {
        // Fix up the current VMProc so that g_pprcNK will be used for
        // All high addresses and DLL code.
        pc = CONTEXT_TO_PROGRAM_COUNTER(DD_ExceptionState.context);
        pprcVM = PcbGetVMProc();
        pprcVM = DD_GetProperVM(pprcVM, (void *) pc);
        SetVirtAddr(&Address, pprcVM, (void*) pc);
        if (SUCCEEDED(DD_FindVMBreakpoint(&Address, NULL, &Bp)) && Bp != NULL)
        {
            DD_SingleStepCBP(fRun);
        }


        if((STATUS_ACCESS_VIOLATION == DD_ExceptionState.exception->ExceptionCode) && 
            (DD_ExceptionState.exception->NumberParameters >= AV_EXCEPTION_PARAMETERS))
        {   
            BOOL fWrite = DD_ExceptionState.exception->ExceptionInformation[AV_EXCEPTION_ACCESS];
            DWORD dwAddress = DD_ExceptionState.exception->ExceptionInformation[AV_EXCEPTION_ADDRESS];
            PPROCESS pVM = (dwAddress > VM_SHARED_HEAP_BASE ? g_pprcNK : GetPCB()->pVMPrc);                

            DWORD dwDBPHandle = DD_FindDataBreakpoint(pVM, dwAddress, fWrite);
            if(dwDBPHandle != INVALID_DBP_HANDLE)
            {
                DEBUGGERMSG(KDZONE_API, (L"  KdpContinue from DataBP\r\n"));
                DD_SingleStepDBP(fRun);
            }
        }

        if (DD_OnEmbeddedBreakpoint())
        {
            DEBUGGERMSG(KDZONE_API, (L"  KdpContinue, On Embedded Breakpoint\r\n"));

            // Since a debugbreak is an invalid instruction exception,
            // we never actually execute it.  Since we don't execute the
            // instruction, we may miss out on certain updates to the processor
            // state that go along with instruction executions.
            //
            // Here we perform any architecture-specific updates needed
            // to simulate having executed the debug break instruction.
            DD_SimulateDebugBreakExecution();

            hr = DD_GetNextPC(&NextPC);
            if (SUCCEEDED(hr))
            {
                DEBUGGERMSG(KDZONE_API, (L"  KdpContinue, PC=%08X\r\n", 
                        CONTEXT_TO_PROGRAM_COUNTER(DD_ExceptionState.context)));
                DEBUGGERMSG(KDZONE_API, (L"  KdpContinue, PC:=%08X\r\n", NextPC));
                CONTEXT_TO_PROGRAM_COUNTER(DD_ExceptionState.context) = NextPC;
            }
        }
    }

    DEBUGGERMSG(KDZONE_API, (L"--KdpContinue, %d\r\n", IgnoreException));
    return IgnoreException;
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
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_SUCCESS;
    }
    else
    {
        pdbgkdCmdPacket->dwReturnStatus = (DWORD) STATUS_UNSUCCESSFUL;
    }

    KdpSendKdApiCmdPacket (&MessageHeader, (pdbgkdCmdPacket->dwReturnStatus == (DWORD) STATUS_SUCCESS) ? AdditionalData : NULL);

    DEBUGGERMSG(KDZONE_API, (L"--KdpAdvancedCmd: status=%u, Length=%u\r\n", pdbgkdCmdPacket->dwReturnStatus, AdditionalData->Length));
}


