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

BOOL g_fReturnPrevSP = TRUE;
BOOL g_fReturnProcHandle = TRUE;
BOOL g_fCanRelocateRwData = TRUE;
BOOL g_fARMVFP10Support = TRUE;
BOOL g_fDbgConnected;

/* Flag to alter the protocol to support the Concan processors */
BOOL g_fArmConcanSupport  = TRUE;

DBGKD_MANIPULATE_STATE ManipulateState;


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGEKD, KdpSendWaitContinue)
#pragma alloc_text(PAGEKD, KdpReadVirtualMemory)
#pragma alloc_text(PAGEKD, KdpWriteVirtualMemory)
#pragma alloc_text(PAGEKD, KdpGetContext)
#pragma alloc_text(PAGEKD, KdpSetContext)
#pragma alloc_text(PAGEKD, KdpWriteBreakpoint)
#pragma alloc_text(PAGEKD, KdpRestoreBreakpoint)
#pragma alloc_text(PAGEKD, KdpReportExceptionStateChange)
#pragma alloc_text(PAGEKD, KdpReportLoadSymbolsStateChange)
#pragma alloc_text(PAGEKD, KdpReadPhysicalMemory)
#pragma alloc_text(PAGEKD, KdpWritePhysicalMemory)
#pragma alloc_text(PAGEKD, KdpGetVersion)
#pragma alloc_text(PAGEKD, KdpWriteBreakPointEx)
#pragma alloc_text(PAGEKD, KdpRestoreBreakPointEx)
#endif // ALLOC_PRAGMA


typedef struct {
    DBGKD_WAIT_STATE_CHANGE StateChange;
    USHORT AdditionalDataLength;
    CHAR AdditionalData[80];
} STATE_PACKET;


KCONTINUE_STATUS
KdpSendWaitContinue (
    IN ULONG OutPacketType,
    IN PSTRING OutMessageHeader,
    IN PSTRING OutMessageData OPTIONAL,
    IN OUT CONTEXT * ContextRecord
    )
/*++

Routine Description:

    This function sends a packet, and then waits for a continue message.
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

    ContextRecord - Exception context

Return Value:

    A value of TRUE is returned if the continue message indicates
    success, Otherwise, a value of FALSE is returned.

--*/
{
    ULONG Length;
    STRING MessageData;
    STRING MessageHeader;
    USHORT ReturnCode;
    NTSTATUS Status;
    PUCHAR pucDummy;

    if (!g_fKdbgRegistered)
    { // We never registered KDBG service
        // JIT Debugging - Make sure no other threads run while we wait for host to connect
        DWORD cSavePrio = GET_CPRIO (pCurThread);
        // Set the quantum to zero to prevent any other real-time thread from running
        DWORD dwSaveQuantum = pCurThread->dwQuantum;
        pCurThread->dwQuantum = 0;
        SET_CPRIO (pCurThread, 0);
        g_fKdbgRegistered = g_kdKernData.pKITLIoCtl(IOCTL_EDBG_REGISTER_DFLT_CLIENT, &pucDummy, KITL_SVC_KDBG, &pucDummy, 0, NULL);
        pCurThread->dwQuantum = dwSaveQuantum;
        SET_CPRIO (pCurThread, cSavePrio);
    }

	if (!g_fKdbgRegistered) return ContinueError;
	
    //
    // Loop servicing state manipulation message until a continue message
    // is received.
    //

    MessageHeader.MaximumLength = sizeof(DBGKD_MANIPULATE_STATE);
    MessageHeader.Buffer = (PCHAR)&ManipulateState;
    MessageData.MaximumLength = KDP_MESSAGE_BUFFER_SIZE;
    MessageData.Buffer = (PCHAR)KdpMessageBuffer;

    KdEnableInts (FALSE);

ResendPacket:

    //
    // Send event notification packet to debugger on host.  Come back
    // here any time we see a breakin sequence.
    //
    //DEBUGGERMSG(KDZONE_API, (L"SendPacket\n"));

    KdpSendPacket(OutPacketType, OutMessageHeader, OutMessageData);
    
    while (TRUE)
    {
        // Wait for State Manipulate Packet without timeout.
        do {
            ReturnCode = KdpReceivePacket(
                            PACKET_TYPE_KD_STATE_MANIPULATE,
                            &MessageHeader,
                            &MessageData,
                            &Length
                            );
            if (ReturnCode == (USHORT)KDP_PACKET_RESEND) {
                goto ResendPacket;
            }
        } while (ReturnCode == KDP_PACKET_TIMEOUT);

        // Set CPU Family
        if (TARGET_CODE_CPU != ManipulateState.dwCpuFamily)
        {
            DEBUGGERMSG (KDZONE_API, (L"CPU is not what was expected (exp=%i,actual=%i)\r\n", ManipulateState.dwCpuFamily, TARGET_CODE_CPU));
            ManipulateState.dwCpuFamily = TARGET_CODE_CPU;
        }

        // Switch on the return message API number.
        DEBUGGERMSG(KDZONE_API, (L"Got Api %8.8lx\r\n", ManipulateState.ApiNumber));

        switch (ManipulateState.ApiNumber) {

        case DbgKdReadVirtualMemoryApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdReadVirtualMemoryApi\r\n"));
            KdpReadVirtualMemory(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteVirtualMemoryApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdWriteVirtualMemoryApi\r\n"));
            KdpWriteVirtualMemory(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdReadPhysicalMemoryApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdReadPhysicalMemoryApi\r\n"));
            KdpReadPhysicalMemory(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWritePhysicalMemoryApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdWritePhysicalMemoryApi\r\n"));
            KdpWritePhysicalMemory(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdGetContextApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdGetContextApi\r\n"));
            KdpGetContext(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdSetContextApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdSetContextApi\r\n"));
            KdpSetContext(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdReadControlSpaceApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdReadControlSpaceApi\r\n"));
            KdpReadControlSpace(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdWriteControlSpaceApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdWriteControlSpaceApi\r\n"));
            KdpWriteControlSpace(&ManipulateState,&MessageData,ContextRecord);
            break;

        case DbgKdReadIoSpaceApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdReadIoSpaceApi\r\n"));
            KdpReadIoSpace(&ManipulateState,&MessageData,ContextRecord,TRUE);
            break;

        case DbgKdWriteIoSpaceApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdWriteIoSpaceApi\r\n"));
            KdpWriteIoSpace(&ManipulateState,&MessageData,ContextRecord,TRUE);
            break;

        case DbgKdContinueApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdContinueApi\r\n"));
            DEBUGGERMSG(KDZONE_API, (L"Continuing target system %8.8lx\r\n", ManipulateState.u.Continue.ContinueStatus));
            if (NT_SUCCESS(ManipulateState.u.Continue.ContinueStatus) != FALSE) {
                KdEnableInts (TRUE);
                return ContinueSuccess;
            } else {
                KdEnableInts (TRUE);
                return ContinueError;
            }
            break;

        case DbgKdTerminateApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdTerminateApi\r\n"));
            KdpDeleteAllBreakpoints();
            if (g_fKdbgRegistered)
            {
                g_kdKernData.pKITLIoCtl (IOCTL_EDBG_DEREGISTER_CLIENT, NULL, KITL_SVC_KDBG, NULL, 0, NULL);
                g_fKdbgRegistered = FALSE;
            }
            KdCleanup();
            g_fDbgConnected = FALSE; // tell KdpTrap to shutdown
            KdEnableInts (TRUE);
            return ContinueSuccess;

        case DbgKdContinueApi2:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdContinueApi2\r\n"));
            if (NT_SUCCESS(ManipulateState.u.Continue2.ContinueStatus) != FALSE) {
                KdpGetStateChange(&ManipulateState,ContextRecord);
                KdEnableInts (TRUE);
                return ContinueSuccess;
            } else {
                KdEnableInts(TRUE);
                return ContinueError;
            }
            break;

        case DbgKdRebootApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdRebootApi\r\n"));
            if (g_kdKernData.pKDIoControl) {
                g_kdKernData.pKDIoControl (KD_IOCTL_RESET, NULL, 0);
            }
            break;

        case DbgKdGetVersionApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdGetVersionApi\r\n"));
            KdpGetVersion(&ManipulateState);
            break;

        case DbgKdCauseBugCheckApi:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdCauseBugCheckApi\r\n"));
            MessageData.Length = 0;
            ManipulateState.ReturnStatus = STATUS_UNSUCCESSFUL;
            KdpSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &MessageHeader, &MessageData);
            break;

        case DbgKdManipulateBreakpoint:
            DEBUGGERMSG( KDZONE_API, (L"DbgKdManipulateBreakpoint\r\n"));
            Status = KdpManipulateBreakPoint(&ManipulateState,
                                                  &MessageData,
                                                  ContextRecord);
            if (Status) {
                ManipulateState.ApiNumber = DbgKdContinueApi;
                ManipulateState.u.Continue.ContinueStatus = Status;
                KdEnableInts (TRUE);
                return ContinueError;
            }
            break;

            // Invalid message.
        default:
            DEBUGGERMSG( KDZONE_API, (L"Unknown API ??\r\n"));
            MessageData.Length = 0;
            ManipulateState.ReturnStatus = STATUS_UNSUCCESSFUL;
            KdpSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &MessageHeader, &MessageData);
            break;
        }

    }
    KdEnableInts (TRUE);
}


VOID
KdpReadVirtualMemory(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN CONTEXT * Context
    )

/*++

Routine Description:

    This function is called in response of a read virtual memory
    state manipulation message. Its function is to read virtual memory
    and return.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_READ_MEMORY a = &m->u.ReadMemory;
    ULONG Length;
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    //
    // make sure that nothing but a read memory message was transmitted
    //

    KD_ASSERT(AdditionalData->Length == 0);

    // Trim transfer count to fit in a single message
    if (a->TransferCount > KDP_MESSAGE_BUFFER_SIZE)
    {
        Length = KDP_MESSAGE_BUFFER_SIZE;
    }
    else
    {
        Length = a->TransferCount;
    }

    AdditionalData->Length = (USHORT)KdpMoveMemory(
                                        AdditionalData->Buffer,
                                        a->TargetBaseAddress,
                                        Length
                                        );

    if (Length == AdditionalData->Length) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    a->ActualBytesRead = AdditionalData->Length;

    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  AdditionalData
                  );
    UNREFERENCED_PARAMETER(Context);
}


VOID
KdpWriteVirtualMemory(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN CONTEXT * Context
    )

/*++

Routine Description:

    This function is called in response of a write virtual memory
    state manipulation message. Its function is to write virtual memory
    and return.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_WRITE_MEMORY a = &m->u.WriteMemory;
    ULONG Length;
    STRING MessageHeader;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;


    Length = KdpMoveMemory(
                a->TargetBaseAddress,
                AdditionalData->Buffer,
                AdditionalData->Length
                );

    if (Length == AdditionalData->Length) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }

    a->ActualBytesWritten = Length;

    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  NULL
                  );
    UNREFERENCED_PARAMETER(Context);
}


BOOL
KdpFlushExtendedContext(
    IN CONTEXT* pCtx
    )
/*++

Routine Description:

    For CPUs that support it, this function retrieves the extended portion of the context
    that is only backed up when used by a thread.

Arguments:

    pCtx - Supplies the exception context to be updated.

Return Value:

    TRUE if the current thread was the owner of the extended context

--*/
{
    BOOL fOwner = FALSE;
        
    // FPU/DSPFlushContext code.  Calling these functions alters the state of the g_CurFPUOwner
    // thread in that it disables the FPU/DSP (ExceptionDispatch will reenable in on the next
    // access).  For SHx, the disable alters the context, so we update the exception context
    // to reflect the change.  See also: KdpTrap, KdpSetContext.
    //
    // TODO: the OS should give kdstub a context with the proper FPU/DSP registers filled in
    //
#if defined(SHx) && !defined(SH4) && !defined(SH3e)
    // copy over the DSP registers from the thread context 
    fOwner = (pCurThread == g_CurDSPOwner);
    DSPFlushContext();
    // if DSPFlushContext updated pCurThread's PSR, keep exception context in sync
    if (fOwner) pCtx->Psr &= ~SR_DSP_ENABLED;
    KdpQuickMoveMemory((PCHAR)&(pCtx->DSR), (PCHAR)&(pCurThread->ctx.DSR),sizeof(DWORD)*13);
#endif

    // Get the floating point registers from the thread context
#if defined(SH4)
    fOwner = (pCurThread == g_CurFPUOwner);
    FPUFlushContext();
    // if FPUFlushContext updated pCurThread's PSR, keep exception context in sync
    if (fOwner) pCtx->Psr |= SR_FPU_DISABLED;
    KdpQuickMoveMemory((PCHAR)&(pCtx->Fpscr), (PCHAR)&(pCurThread->ctx.Fpscr),sizeof(DWORD)*34);
#elif defined(MIPS_HAS_FPU)
    fOwner = (pCurThread == g_CurFPUOwner);
    FPUFlushContext();
    // if FPUFlushContext updated pCurThread's FSR, keep exception context in sync
    if (fOwner) pCtx->Fsr = pCurThread->ctx.Fsr;
    KdpQuickMoveMemory((PCHAR)&(pCtx->FltF0), (PCHAR)&(pCurThread->ctx.FltF0),sizeof(FREG_TYPE)*32);
#elif defined(ARM)
    // FPUFlushContext might modify FpExc, but apparently it can't be restored, so we shouldn't bother
    // trying update our context with it
    FPUFlushContext();
    KdpQuickMoveMemory((PCHAR)&(pCtx->Fpscr), (PCHAR)&(pCurThread->ctx.Fpscr),sizeof(DWORD)*43);
#elif defined(x86)
    if (*g_kdKernData.ppCurFPUOwner) 
    {
        KCall ((LPVOID) FPUFlushContext, 0, 0, 0);
        pCtx->FloatSave = *(PTH_TO_FLTSAVEAREAPTR(pCurThread));
    }
#endif

    return fOwner;
}


VOID
KdpGetContext(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN CONTEXT * Context
    )
/*++

Routine Description:

    This function is called in response of a get context state
    manipulation message.  Its function is to return the current
    context.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/
{
    STRING MessageHeader;
#if defined(SH3e) || defined(SH4)
    DEBUG_REGISTERS DebugRegisters;
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    KD_ASSERT(AdditionalData->Length == 0);

    m->ReturnStatus = STATUS_SUCCESS;
    AdditionalData->Length = sizeof(CONTEXT);


#if defined(SH3)
    Context->DebugRegisters.BarA  = READ_REGISTER_ULONG(UBCBarA);
    Context->DebugRegisters.BasrA = READ_REGISTER_UCHAR(UBCBasrA);
    Context->DebugRegisters.BamrA = READ_REGISTER_UCHAR(UBCBamrA);
    Context->DebugRegisters.BbrA  = READ_REGISTER_USHORT(UBCBbrA);
    Context->DebugRegisters.BarB  = READ_REGISTER_ULONG(UBCBarB);
    Context->DebugRegisters.BasrB = READ_REGISTER_UCHAR(UBCBasrB);
    Context->DebugRegisters.BamrB = READ_REGISTER_UCHAR(UBCBamrB);
    Context->DebugRegisters.BbrB  = READ_REGISTER_USHORT(UBCBbrB);
    Context->DebugRegisters.BdrB  = READ_REGISTER_ULONG(UBCBdrB);
    Context->DebugRegisters.BdmrB = READ_REGISTER_ULONG(UBCBdmrB);
    Context->DebugRegisters.Brcr  = READ_REGISTER_USHORT(UBCBrcr);
    Context->DebugRegisters.Align = 0;
#elif defined(SH3e) || defined(SH4)
    DebugRegisters.BarA  = READ_REGISTER_ULONG(UBCBarA);
    DebugRegisters.BasrA = READ_REGISTER_UCHAR(UBCBasrA);
    DebugRegisters.BamrA = READ_REGISTER_UCHAR(UBCBamrA);
    DebugRegisters.BbrA  = READ_REGISTER_USHORT(UBCBbrA);
    DebugRegisters.BarB  = READ_REGISTER_ULONG(UBCBarB);
    DebugRegisters.BasrB = READ_REGISTER_UCHAR(UBCBasrB);
    DebugRegisters.BamrB = READ_REGISTER_UCHAR(UBCBamrB);
    DebugRegisters.BbrB  = READ_REGISTER_USHORT(UBCBbrB);
    DebugRegisters.BdrB  = READ_REGISTER_ULONG(UBCBdrB);
    DebugRegisters.BdmrB = READ_REGISTER_ULONG(UBCBdmrB);
    DebugRegisters.Brcr  = READ_REGISTER_USHORT(UBCBrcr);
    DebugRegisters.Align = 0;

    // Follow the context in the buffer with the debug register values.
    AdditionalData->Length += sizeof(DEBUG_REGISTERS);
#endif

    KdpFlushExtendedContext(Context);

    KdpQuickMoveMemory(AdditionalData->Buffer, (PCHAR)Context, sizeof(CONTEXT));
#if defined(SH3e) || defined(SH4)
    KdpQuickMoveMemory(AdditionalData->Buffer + sizeof(CONTEXT),
                       (PCHAR)&DebugRegisters, sizeof(DEBUG_REGISTERS));
#endif

#ifdef ARM
    if (g_fArmConcanSupport)
    {
        CONCAN_REGS ConcanRegs = {0};

        if (DetectConcanCoprocessors ()) GetConcanRegisters (&ConcanRegs);
        AdditionalData->Length += sizeof (CONCAN_REGS);
        KdpQuickMoveMemory(AdditionalData->Buffer + sizeof(CONTEXT), (PCHAR)&ConcanRegs,
                sizeof(CONCAN_REGS));
    }
#endif

    KdpSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &MessageHeader, AdditionalData);
}


VOID
KdpSetContext(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN CONTEXT * Context
    )
/*++

Routine Description:

    This function is called in response of a set context state
    manipulation message.  Its function is set the current
    context.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/
{
    STRING MessageHeader;
    BOOL fOwner = FALSE;
#if defined(SH3e) || defined(SH4)
    PDEBUG_REGISTERS DebugRegisters = (PDEBUG_REGISTERS)(AdditionalData->Buffer + sizeof(CONTEXT));
#endif

#ifdef ARM
    PCONCAN_REGS pConcanRegs = (PCONCAN_REGS)(AdditionalData->Buffer + sizeof (CONTEXT));
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

#if defined(SH3e) || defined(SH4)
    //
    // Debug register values passed following the context
    //
    KD_ASSERT(AdditionalData->Length == sizeof(CONTEXT) + sizeof(DEBUG_REGISTERS));
#elif defined (ARM)
    /* Need to consider Concan registers for ARM */
    if (g_fArmConcanSupport)
    {
        KD_ASSERT (AdditionalData->Length == (sizeof (CONTEXT) + sizeof (CONCAN_REGS)));
    }
    else
    {
        // Backwards compatibility required
        KD_ASSERT (AdditionalData->Length == sizeof (CONTEXT));
    }
#else
    KD_ASSERT(AdditionalData->Length == sizeof(CONTEXT));
#endif

    m->ReturnStatus = STATUS_SUCCESS;
    KdpQuickMoveMemory((PCHAR)Context, AdditionalData->Buffer, sizeof(CONTEXT));

    // copy the DSP registers into the thread context
#if defined(SHx) && !defined(SH4) && !defined(SH3e)
    // copy over the DSP registers from the thread context 
    fOwner = (pCurThread == g_CurDSPOwner);
    DSPFlushContext();
    // if DSPFlushContext updated pCurThread's PSR, keep exception context in sync
    if (fOwner) Context->Psr &= ~SR_DSP_ENABLED;
    KdpQuickMoveMemory((PCHAR)&(pCurThread->ctx.DSR), (PCHAR)&(Context->DSR), sizeof(DWORD)*13);
#endif

    // copy the floating point registers into the thread context
#if defined(SH4)
    fOwner = (pCurThread == g_CurFPUOwner);
    FPUFlushContext();
    // if FPUFlushContext updated pCurThread's PSR, keep exception context in sync
    if (fOwner) Context->Psr |= SR_FPU_DISABLED;
    KdpQuickMoveMemory((PCHAR)&(pCurThread->ctx.Fpscr),(PCHAR)&(Context->Fpscr), sizeof(DWORD)*34);
#elif defined(MIPS_HAS_FPU)
    fOwner = (pCurThread == g_CurFPUOwner);
    FPUFlushContext();
    // if FPUFlushContext updated pCurThread's FSR, keep exception context in sync
    if (fOwner) Context->Fsr = pCurThread->ctx.Fsr;
    KdpQuickMoveMemory((PCHAR)&(pCurThread->ctx.FltF0),(PCHAR)&(Context->FltF0), sizeof(FREG_TYPE)*32);
#elif defined(ARM)
    if (g_fARMVFP10Support)
    {
        // FPUFlushContext might modify FpExc, but apparently it can't be restored, so we shouldn't bother
        // trying update our context with it
        FPUFlushContext();
        KdpQuickMoveMemory((PCHAR)&(pCurThread->ctx.Fpscr),(PCHAR)&(Context->Fpscr), sizeof(DWORD)*43);
    }
    if (g_fArmConcanSupport && DetectConcanCoprocessors())
    {
        SetConcanRegisters (pConcanRegs);
    }
#endif



/*
#if defined(SH3)
    WRITE_REGISTER_ULONG(UBCBarA,  Context->DebugRegisters.BarA);
    WRITE_REGISTER_UCHAR(UBCBasrA, Context->DebugRegisters.BasrA);
    WRITE_REGISTER_UCHAR(UBCBamrA, Context->DebugRegisters.BamrA);
    WRITE_REGISTER_USHORT(UBCBbrA, Context->DebugRegisters.BbrA);
    WRITE_REGISTER_ULONG(UBCBarB,  Context->DebugRegisters.BarB);
    WRITE_REGISTER_UCHAR(UBCBasrB, Context->DebugRegisters.BasrB);
    WRITE_REGISTER_UCHAR(UBCBamrB, Context->DebugRegisters.BamrB);
    WRITE_REGISTER_USHORT(UBCBbrB, Context->DebugRegisters.BbrB);
    WRITE_REGISTER_ULONG(UBCBdrB,  Context->DebugRegisters.BdrB);
    WRITE_REGISTER_ULONG(UBCBdmrB, Context->DebugRegisters.BdmrB);
    WRITE_REGISTER_USHORT(UBCBrcr, Context->DebugRegisters.Brcr);
#elif defined(SH3e) || defined(SH4)
    WRITE_REGISTER_ULONG(UBCBarA,  DebugRegisters->BarA);
    WRITE_REGISTER_UCHAR(UBCBasrA, DebugRegisters->BasrA);
    WRITE_REGISTER_UCHAR(UBCBamrA, DebugRegisters->BamrA);
    WRITE_REGISTER_USHORT(UBCBbrA, DebugRegisters->BbrA);
    WRITE_REGISTER_ULONG(UBCBarB,  DebugRegisters->BarB);
    WRITE_REGISTER_UCHAR(UBCBasrB, DebugRegisters->BasrB);
    WRITE_REGISTER_UCHAR(UBCBamrB, DebugRegisters->BamrB);
    WRITE_REGISTER_USHORT(UBCBbrB, DebugRegisters->BbrB);
    WRITE_REGISTER_ULONG(UBCBdrB,  DebugRegisters->BdrB);
    WRITE_REGISTER_ULONG(UBCBdmrB, DebugRegisters->BdmrB);
    WRITE_REGISTER_USHORT(UBCBrcr, DebugRegisters->Brcr);
#endif
*/

    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  NULL
                  );
}


BOOLEAN
KdpReportExceptionStateChange (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN OUT CONTEXT * ContextRecord,
    IN BOOLEAN SecondChance
    )
/*++

Routine Description:

    This routine sends an exception state change packet to the kernel
    debugger and waits for a manipulate state message.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

    ContextRecord - Supplies a pointer to a context record.

    SecondChance - Supplies a boolean value that determines whether this is
        the first or second chance for the exception.

Return Value:

    A value of TRUE is returned if the exception is handled. Otherwise, a
    value of FALSE is returned.

--*/
{
    STRING MessageData;
    STRING MessageHeader;
    DBGKD_WAIT_STATE_CHANGE WaitStateChange;
    KCONTINUE_STATUS Status;


    do {

        //
        // Construct the wait state change message and message descriptor.
        //

        KdpSetStateChange(&WaitStateChange,
                            ExceptionRecord,
                            ContextRecord,
                            SecondChance
                            );

        MessageHeader.Length = sizeof(DBGKD_WAIT_STATE_CHANGE);
        MessageHeader.Buffer = (PCHAR)&WaitStateChange;

        MessageData.Length = 0;

        //
        // Send packet to the kernel debugger on the host machine,
        // wait for answer.
        //
        Status = KdpSendWaitContinue(
                    PACKET_TYPE_KD_STATE_CHANGE,
                    &MessageHeader,
                    &MessageData,
                    ContextRecord
                    );

    } while (Status == ContinueProcessorReselected) ;

    return (BOOLEAN) Status;
}


BOOLEAN
KdpReportLoadSymbolsStateChange (
    IN PSTRING PathName,
    IN PKD_SYMBOLS_INFO SymbolInfo,
    IN BOOLEAN UnloadSymbols,
    IN OUT CONTEXT * ContextRecord
    )
/*++

Routine Description:

    This routine sends a load symbols state change packet to the kernel
    debugger and waits for a manipulate state message.

Arguments:

    PathName - Supplies a pointer to the pathname of the image whose
        symbols are to be loaded.

    BaseOfDll - Supplies the base address where the image was loaded.

    ProcessId - Unique 32-bit identifier for process that is using
        the symbols.  -1 for system process.

    CheckSum - Unique 32-bit identifier from image header.

    UnloadSymbol - TRUE if the symbols that were previously loaded for
        the named image are to be unloaded from the debugger.

Return Value:

    A value of TRUE is returned if the exception is handled. Otherwise, a
    value of FALSE is returned.

--*/
{

    PSTRING AdditionalData;
    STRING MessageData;
    STRING MessageHeader;
    DBGKD_WAIT_STATE_CHANGE WaitStateChange;
    KCONTINUE_STATUS Status;

    DEBUGGERMSG(KDZONE_API, (L"++KdpReportLoadSymbolsStateChange\r\n"));

    //
    // Construct the wait state change message and message descriptor.
    //

    WaitStateChange.NewState = DbgKdLoadSymbolsStateChange;
    WaitStateChange.dwCpuFamily = TARGET_CODE_CPU;
    WaitStateChange.NumberProcessors = (ULONG)1;
    WaitStateChange.Thread = (void *) (g_fForceReload ? 0 : 1); // pCurThread;

    WaitStateChange.ProgramCounter = (PVOID)CONTEXT_TO_PROGRAM_COUNTER(ContextRecord);
    KdpSetLoadState(&WaitStateChange, ContextRecord);
    WaitStateChange.u.LoadSymbols.UnloadSymbols = UnloadSymbols;
    WaitStateChange.u.LoadSymbols.BaseOfDll = SymbolInfo->BaseOfDll;
    
    WaitStateChange.u.LoadSymbols.dwDllRwStart = SymbolInfo->dwDllRwStart;
    WaitStateChange.u.LoadSymbols.dwDllRwEnd = SymbolInfo->dwDllRwEnd;
    
    WaitStateChange.u.LoadSymbols.ProcessId = (g_fForceReload ? 0 : SymbolInfo->ProcessId); // This is for Client version < 4
    g_fForceReload = FALSE;
    WaitStateChange.u.LoadSymbols.CheckSum = SymbolInfo->CheckSum;
    WaitStateChange.u.LoadSymbols.SizeOfImage = SymbolInfo->SizeOfImage;
    if (PathName)
    {
        WaitStateChange.u.LoadSymbols.PathNameLength = PathName->Length + 1;
        KdpQuickMoveMemory ((PCHAR)KdpMessageBuffer, (PCHAR)PathName->Buffer, PathName->Length);
        MessageData.Buffer = KdpMessageBuffer;
        MessageData.Length = (USHORT) WaitStateChange.u.LoadSymbols.PathNameLength;
        MessageData.Buffer[MessageData.Length-1] = '\0';
        AdditionalData = &MessageData;
    }
    else
    {
        WaitStateChange.u.LoadSymbols.PathNameLength = 0;
        AdditionalData = NULL;
    }

    MessageHeader.Length = sizeof(DBGKD_WAIT_STATE_CHANGE);
    MessageHeader.Buffer = (PCHAR)&WaitStateChange;
    //
    // Send packet to the kernel debugger on the host machine, wait
    // for the reply.
    //
    DEBUGGERMSG(KDZONE_API, (L"Updating symbols for %a %8.8lx %8.8lx\r\n",
        PathName ? KdpMessageBuffer : "unknown",
        WaitStateChange.u.LoadSymbols.BaseOfDll,
        WaitStateChange.u.LoadSymbols.SizeOfImage));

        Status = KdpSendWaitContinue(
            PACKET_TYPE_KD_STATE_CHANGE,
            &MessageHeader,
            AdditionalData,
            ContextRecord
            );

    DEBUGGERMSG(KDZONE_API, (L"--KdpReportLoadSymbolsStateChange\r\n"));

    return (BOOLEAN) Status;
}


VOID
KdpReadPhysicalMemory(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN CONTEXT * Context
    )
/*++

Routine Description:

    This function is called in response to a read physical memory
    state manipulation message. Its function is to read physical memory
    and return.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/
{
    PDBGKD_READ_MEMORY a = &m->u.ReadMemory;
    ULONG Length;
    STRING MessageHeader;
    PVOID VirtualAddress;
    PHYSICAL_ADDRESS Source;
    PUCHAR Destination;
    USHORT NumberBytes;
    USHORT BytesLeft;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    //
    // make sure that nothing but a read memory message was transmitted
    //

    KD_ASSERT(AdditionalData->Length == 0);

    //
    // Trim transfer count to fit in a single message
    //

    if (a->TransferCount > KDP_MESSAGE_BUFFER_SIZE)
    {
        Length = KDP_MESSAGE_BUFFER_SIZE;
    }
    else
    {
        Length = a->TransferCount;
    }

    //
    // Since the MmDbgTranslatePhysicalAddress only maps in one physical
    // page at a time, we need to break the memory move up into smaller
    // moves which don't cross page boundaries.  There are two cases we
    // need to deal with.  The area to be moved may start and end on the
    // same page, or it may start and end on different pages (with an
    // arbitrary number of pages in between)
    //

    Source = (ULONG)a->TargetBaseAddress;

    Destination = AdditionalData->Buffer;
    BytesLeft = (USHORT)Length;
    if(PAGE_ALIGN((PUCHAR)a->TargetBaseAddress) ==
       PAGE_ALIGN((PUCHAR)(a->TargetBaseAddress)+Length)) {
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
    } else {

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

            VirtualAddress = (PVOID)MmDbgTranslatePhysicalAddress(Source);
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

    if (Length == AdditionalData->Length) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    a->ActualBytesRead = AdditionalData->Length;

    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  AdditionalData
                  );
    UNREFERENCED_PARAMETER(Context);
}


VOID
KdpWritePhysicalMemory(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN CONTEXT * Context
    )
/*++

Routine Description:

    This function is called in response to a write physical memory
    state manipulation message. Its function is to write physical memory
    and return.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/
{
    PDBGKD_WRITE_MEMORY a = &m->u.WriteMemory;
    ULONG Length;
    STRING MessageHeader;
    PVOID VirtualAddress;
    PHYSICAL_ADDRESS Destination;
    PUCHAR Source;
    USHORT NumberBytes;
    USHORT BytesLeft;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    //
    // Since the MmDbgTranslatePhysicalAddress only maps in one physical
    // page at a time, we need to break the memory move up into smaller
    // moves which don't cross page boundaries.  There are two cases we
    // need to deal with.  The area to be moved may start and end on the
    // same page, or it may start and end on different pages (with an
    // arbitrary number of pages in between)
    //

    Destination = (ULONG)a->TargetBaseAddress;

    Source = AdditionalData->Buffer;
    BytesLeft = (USHORT) a->TransferCount;

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
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }

    a->ActualBytesWritten = Length;

    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  NULL
                  );
    UNREFERENCED_PARAMETER(Context);
}


const ULONG NtBuildNumber = VER_PRODUCTBUILD | 0xC0000000;


VOID
KdpGetVersion(
    IN PDBGKD_MANIPULATE_STATE m
    )
/*++

Routine Description:

    This function returns to the caller a general information packet
    that contains useful information to a debugger.  This packet is also
    used for a debugger to determine if the writebreakpointex and
    readbreakpointex apis are available.

Arguments:

    m - Supplies the state manipulation message.

Return Value:

    None.

--*/
{
    STRING                  messageHeader;

    messageHeader.Length = sizeof(*m);
    messageHeader.Buffer = (PCHAR)m;

    if (m->u.GetVersion.ProtocolVersion < 2)
    {
        g_fReturnPrevSP = FALSE;
    }

    if (m->u.GetVersion.ProtocolVersion < 4)
    {
        g_fCanRelocateRwData = FALSE;
    }

    if (m->u.GetVersion.ProtocolVersion < 5)
    {
        g_fARMVFP10Support = FALSE;
    }

    if (m->u.GetVersion.ProtocolVersion < 6)
    {
        /* If the protocol can't handle Concan, then don't even bother. */
        g_fArmConcanSupport = FALSE;
    }

    if (m->u.GetVersion.ProtocolVersion < 7)
    {
        // If the protocol can't handle Process handle for thread frame then don't send
        g_fReturnProcHandle = FALSE;
    }

    m->u.GetVersion.dwProcessorName = NkCEProcessorType;
    m->u.GetVersion.MachineType = (USHORT) TARGET_CODE_CPU;

    //
    // the current build number
    //
    m->u.GetVersion.MinorVersion = (short) NtBuildNumber;
    m->u.GetVersion.MajorVersion = (short)((NtBuildNumber >> 28) & 0xFFFFFFF);

    //
    // kd protocol version number.  this should be incremented if the
    // protocol changes.
    //
    m->u.GetVersion.ProtocolVersion = 11;
    














    m->u.GetVersion.Flags = 0;

    // Determine if hardware DSP support used
    if (g_kdKernData.fDSPPresent)
    {
        m->u.GetVersion.Flags |= DBGKD_VERS_FLAG_DSP;
    }

    // Determine if hardware FPU support used
    if (g_kdKernData.fFPUPresent)
    {
        m->u.GetVersion.Flags |= DBGKD_VERS_FLAG_FPU;
    }

    /* Determine the status of Concan support */
#if defined (ARM)
    if (DetectConcanCoprocessors())
    {
        m->u.GetVersion.Flags |= DBGKD_VERS_FLAG_MULTIMEDIA;
    }
#endif

    //
    // address of the loader table
    //

    m->u.GetVersion.PsLoadedModuleList  = (ULONG)NULL;

    //
    // This is where the firmware loads the target program
    //

    m->u.GetVersion.KernBase = ((TOCentry *)((LPBYTE)pTOC+sizeof(ROMHDR)))->ulLoadOffset;

    //
    // This is the relocated kernel data section offset.
    //

    m->u.GetVersion.KernDataSectionOffset = ((COPYentry *)(pTOC->ulCopyOffset))->ulDest;

    //
    // the usual stuff
    //
    m->ReturnStatus = STATUS_SUCCESS;
    m->ApiNumber = DbgKdGetVersionApi;


    if (g_kdKernData.pKDIoControl) {
        KCall (g_kdKernData.pKDIoControl, KD_IOCTL_INIT, NULL, 0);
    }

    KdpDeleteAllBreakpoints();

    KdpSendPacket(PACKET_TYPE_KD_STATE_MANIPULATE, &messageHeader, NULL);

    return;
} // End of KdpGetVersion


NTSTATUS
KdpManipulateBreakPoint(
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN CONTEXT * Context
    )
/*++

Routine Description:

    This function is called in response of a manipulate breakpoint 
    message.  Its function is to query/write/set a breakpoint
    and return a handle to the breakpoint.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/
{
    PDBGKD_MANIPULATE_BREAKPOINT a = &m->u.ManipulateBreakPoint;
    PDBGKD_MANIPULATE_BREAKPOINT_DATA b = (PDBGKD_MANIPULATE_BREAKPOINT_DATA) AdditionalData->Buffer;
    STRING MessageHeader;
    BOOL fSuccess = FALSE;
    DWORD i;
    BOOL fHALBP=FALSE;
    KD_BPINFO bpInfo;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    DEBUGGERMSG(KDZONE_HAL, (L"KDManipulateBreakPoint\r\n"));
    // The request has the first breakpoint..if more than one than AdditionalData has the others
    if (AdditionalData->Length != (a->Count) *sizeof(DBGKD_MANIPULATE_BREAKPOINT_DATA)) {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
        KdpSendPacket(
                      PACKET_TYPE_KD_STATE_MANIPULATE,
                      &MessageHeader,
                      AdditionalData
                      );

        DEBUGGERMSG(KDZONE_HAL, (L"KdpManipulateBreakPoint: Length mismatch\n"));
    }
        
    DEBUGGERMSG(KDZONE_HAL, (L"Count = %ld\n", a->Count));
    for (i=0; i < a->Count; i++) {
        DEBUGGERMSG(KDZONE_HAL, (L"Address = %08X Flags=%08X Handle=%08X\n", b[i].Address, b[i].Flags, b[i].Handle));
        if ((b[i].Flags & DBGKD_MBP_FLAG_DP) && g_kdKernData.pKDIoControl) {
            if (b[i].Flags & DBGKD_MBP_FLAG_SET) {  
                bpInfo.nVersion = 1;
                bpInfo.ulAddress = ZeroPtr(b[i].Address);
                bpInfo.ulFlags = 0;
                bpInfo.ulHandle = 0;
                if (KCall (g_kdKernData.pKDIoControl, KD_IOCTL_SET_DBP, &bpInfo, sizeof(KD_BPINFO))) {
                    fSuccess = TRUE;
                    b[i].Handle = bpInfo.ulHandle | 0x80000000;
                    DEBUGGERMSG(KDZONE_HAL, (L"Set Hard DBP Address = %08X Flags=%08X Handle=%08X\n", b[i].Address, b[i].Flags, b[i].Handle));
                }
            } else if (b[i].Handle & 0x80000000) {
                bpInfo.nVersion = 1;
                bpInfo.ulHandle = b[i].Handle & 0x7FFFFFFF;
                bpInfo.ulFlags = 0;
                bpInfo.ulAddress = 0;
                if (KCall (g_kdKernData.pKDIoControl, KD_IOCTL_CLEAR_DBP, &bpInfo, sizeof(KD_BPINFO))) {
                    DEBUGGERMSG(KDZONE_HAL, (L"Clear Hard DBP Address = %08X Flags=%08X Handle=%08X\n", b[i].Address, b[i].Flags, b[i].Handle));
                    fSuccess = TRUE;
                }
            }
        } else {
            if (g_kdKernData.pKDIoControl) {
                if (b[i].Flags & DBGKD_MBP_FLAG_SET) {
                    bpInfo.nVersion = 1;
                    bpInfo.ulAddress = ZeroPtr(b[i].Address);
                    bpInfo.ulFlags = 0;
                    if (KCall (g_kdKernData.pKDIoControl, KD_IOCTL_SET_CBP, &bpInfo, sizeof(KD_BPINFO))) {
                        fSuccess = TRUE;
                        fHALBP = TRUE;
                        b[i].Handle = bpInfo.ulHandle | 0x80000000;
                            DEBUGGERMSG(KDZONE_HAL, (L"Set  Hard CBP Address = %08X Flags=%08X Handle=%08X\n", b[i].Address, b[i].Flags, b[i].Handle));
                    }
                } else 
                if (b[i].Handle & 0x80000000) {
                    fHALBP = TRUE;
                    bpInfo.nVersion = 1;
                    bpInfo.ulHandle = b[i].Handle & 0x7FFFFFFF;
                    bpInfo.ulFlags = 0;
                    if (KCall (g_kdKernData.pKDIoControl, KD_IOCTL_CLEAR_CBP, &bpInfo, sizeof(KD_BPINFO))) {
                        fSuccess = TRUE;
                        DEBUGGERMSG(KDZONE_HAL, (L"Clear  Hard CBP Address = %08X Flags=%08X Handle=%08X\n", b[i].Address, b[i].Flags, b[i].Handle));
                    }
                }
            }   
            if (!fHALBP && (b[i].Flags & DBGKD_MBP_FLAG_CP)) {
                if (b[i].Flags & DBGKD_MBP_FLAG_SET) {
                    b[i].Flags |= DBGKD_MBP_SOFTWARE;

                    if (Is16BitSupported && (b[i].Flags & DBGKD_MBP_16BIT)) b[i].Address |= 0x1;

                    if (b[i].Handle = KdpAddBreakpoint((PVOID)b[i].Address)) {
                        DEBUGGERMSG(KDZONE_HAL, (L"Set Soft CBP Address = %08X Flags=%08X Handle=%08X\n", b[i].Address, b[i].Flags, b[i].Handle));
                        fSuccess = TRUE;
                    }
                } else {
                    if (KdpDeleteBreakpoint(b[i].Handle)) {
                        DEBUGGERMSG(KDZONE_HAL, (L"Clear Soft CBP Address = %08X Flags=%08X Handle=%08X\n", b[i].Address, b[i].Flags, b[i].Handle));
                        fSuccess = TRUE;
                    }
                }
            }   
        }
    }

    if (fSuccess) {
        m->ReturnStatus = STATUS_SUCCESS;
    } else {
        m->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    DEBUGGERMSG(KDZONE_HAL, (L"Status = %ld\n", m->ReturnStatus));
    KdpSendPacket(
                  PACKET_TYPE_KD_STATE_MANIPULATE,
                  &MessageHeader,
                  AdditionalData
                  );
    UNREFERENCED_PARAMETER(Context);
    return a->ContinueStatus;
}

