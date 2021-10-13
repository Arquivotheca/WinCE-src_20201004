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

PVOID __inline GetRegistrationHead(void)
{
    __asm mov eax, dword ptr fs:[0];
}

#pragma intrinsic(_inp, _inpw, _inpd, _outp, _outpw, _outpd)
#endif

// stackwalk state globals
extern PCALLSTACK pStk;
extern PPROCESS   pLastProc;
extern PTHREAD    pWalkThread;

//void NKOtherPrintfA(char *szFormat, ...);


BOOL fThreadWalk;

DWORD ReloadAllSymbols(LPBYTE lpBuffer, BOOL fDoCopy);
#if defined(MIPS) || defined(x86) 
void CpuContextToContext(CONTEXT *pCtx, CPUCONTEXT *pCpuCtx);
#endif

VOID
KdpSetLoadState (
    IN PDBGKD_WAIT_STATE_CHANGE WaitStateChange,
    IN PCONTEXT ContextRecord
    )

/*++

Routine Description:

    Fill in the Wait_State_Change message record for the load symbol case.

Arguments:

    WaitStateChange - Supplies pointer to record to fill in

    ContextRecord - Supplies a pointer to a context record.

Return Value:

    None.

--*/

{
    //
    //  Copy context record
    //
    WaitStateChange->Context = *ContextRecord;
    return;
}

VOID
KdpSetStateChange (
    IN PDBGKD_WAIT_STATE_CHANGE WaitStateChange,
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextRecord,
    IN BOOLEAN SecondChance
    )

/*++

Routine Description:

    Fill in the Wait_State_Change message record.

Arguments:

    WaitStateChange - Supplies pointer to record to fill in

    ExceptionRecord - Supplies a pointer to an exception record.

    ContextRecord - Supplies a pointer to a context record.

    SecondChance - Supplies a boolean value that determines whether this is
        the first or second chance for the exception.

Return Value:

    None.

--*/

{

    ULONG   Index;
    ULONG   Offset;
    PVOID   Lower;
    PVOID   Upper;
    KDP_BREAKPOINT_TYPE Instruction;
    ULONG Length;

    //
    //  Set up description of event, including exception record
    //

    WaitStateChange->NewState = DbgKdExceptionStateChange;
    WaitStateChange->dwCpuFamily = TARGET_CODE_CPU;
    WaitStateChange->NumberProcessors = (ULONG)1;
    WaitStateChange->Thread = (void *) (g_fForceReload ? 0 : 1); // hCurThread;
    g_fForceReload = FALSE;
    WaitStateChange->ProgramCounter = (PVOID)CONTEXT_TO_PROGRAM_COUNTER(ContextRecord);
    KdpQuickMoveMemory(
                (PCHAR)&WaitStateChange->u.Exception.ExceptionRecord,
                (PCHAR)ExceptionRecord,
                sizeof(EXCEPTION_RECORD)
                );
    WaitStateChange->u.Exception.FirstChance = !SecondChance;

    //
    //  Copy instruction stream immediately following location of event
    //

    WaitStateChange->ControlReport.InstructionCount =
        (WORD)KdpMoveMemory(
            &(WaitStateChange->ControlReport.InstructionStream[0]),
            WaitStateChange->ProgramCounter,
            DBGKD_MAXSTREAM
            );

#if defined(SH3)
    ContextRecord->DebugRegisters.BarA  = READ_REGISTER_ULONG(UBCBarA);
    ContextRecord->DebugRegisters.BasrA = READ_REGISTER_UCHAR(UBCBasrA);
    ContextRecord->DebugRegisters.BamrA = READ_REGISTER_UCHAR(UBCBamrA);
    ContextRecord->DebugRegisters.BbrA  = READ_REGISTER_USHORT(UBCBbrA); 
    ContextRecord->DebugRegisters.BarB  = READ_REGISTER_ULONG(UBCBarB);
    ContextRecord->DebugRegisters.BasrB = READ_REGISTER_UCHAR(UBCBasrB);
    ContextRecord->DebugRegisters.BamrB = READ_REGISTER_UCHAR(UBCBamrB);
    ContextRecord->DebugRegisters.BbrB  = READ_REGISTER_USHORT(UBCBbrB); 
    ContextRecord->DebugRegisters.BdrB  = READ_REGISTER_ULONG(UBCBdrB);
    ContextRecord->DebugRegisters.BdmrB = READ_REGISTER_ULONG(UBCBdmrB);
    ContextRecord->DebugRegisters.Brcr  = READ_REGISTER_USHORT(UBCBrcr);
    ContextRecord->DebugRegisters.Align = 0;
#elif defined(SH3e) || defined(SH4)
    WaitStateChange->DebugRegisters.BarA  = READ_REGISTER_ULONG(UBCBarA);
    WaitStateChange->DebugRegisters.BasrA = READ_REGISTER_UCHAR(UBCBasrA);
    WaitStateChange->DebugRegisters.BamrA = READ_REGISTER_UCHAR(UBCBamrA);
    WaitStateChange->DebugRegisters.BbrA  = READ_REGISTER_USHORT(UBCBbrA); 
    WaitStateChange->DebugRegisters.BarB  = READ_REGISTER_ULONG(UBCBarB);
    WaitStateChange->DebugRegisters.BasrB = READ_REGISTER_UCHAR(UBCBasrB);
    WaitStateChange->DebugRegisters.BamrB = READ_REGISTER_UCHAR(UBCBamrB);
    WaitStateChange->DebugRegisters.BbrB  = READ_REGISTER_USHORT(UBCBbrB); 
    WaitStateChange->DebugRegisters.BdrB  = READ_REGISTER_ULONG(UBCBdrB);
    WaitStateChange->DebugRegisters.BdmrB = READ_REGISTER_ULONG(UBCBdmrB);
    WaitStateChange->DebugRegisters.Brcr  = READ_REGISTER_USHORT(UBCBrcr);
    WaitStateChange->DebugRegisters.Align = 0;
#endif

#if defined(x86)
    if (*g_kdKernData.ppCurFPUOwner) 
    {
        KCall((LPVOID)FPUFlushContext,0,0,0);
        ContextRecord->FloatSave = *(PTH_TO_FLTSAVEAREAPTR(pCurThread));
    }
#endif

    //
    //  Copy context record immediately following instruction stream
    //
    WaitStateChange->Context = *ContextRecord;

#ifdef ARM
    // Not editting the _CONTEXT struction.  Here is the copy of additional register information
    if (g_fArmConcanSupport && DetectConcanCoprocessors ())
    {
        DEBUGGERMSG(KDZONE_CONCAN, (L"Adding on the concan registers\n\r"));;
        DEBUGGERMSG(KDZONE_CONCAN, (L"sizeof(CONTEXT) = %d\n\r", sizeof(CONTEXT)));
        DEBUGGERMSG(KDZONE_CONCAN, (L"sizeof(CONCAN_REGS) = %d\n\r", sizeof (CONCAN_REGS)));
        DEBUGGERMSG(KDZONE_CONCAN, (L"offsetof(DBGKD_WAIT_STATE_CHANGE, Context) = %d\n\r",
                offsetof(DBGKD_WAIT_STATE_CHANGE, Context)));
        DEBUGGERMSG(KDZONE_CONCAN, (L"offsetof(DBGKD_WAIT_STATE_CHANGE, ConcanRegs) = %d\n\r",
                offsetof(DBGKD_WAIT_STATE_CHANGE, ConcanRegs)));
        GetConcanRegisters (&WaitStateChange->ConcanRegs);
    }
    else
    {
        // Memfill the buffer to zero instead
        memset(&WaitStateChange->ConcanRegs, 0, sizeof(CONCAN_REGS));
    }
#endif

    //
    // Examine each entry in the table in turn
    //

    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index++) {
        Lower = WaitStateChange->ProgramCounter;
        Upper = (PVOID)((PUCHAR)WaitStateChange->ProgramCounter +
                       WaitStateChange->ControlReport.InstructionCount - 1);

        if ( (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE) &&
             ((KdpBreakpointTable[Index].Address >= Lower) &&
              (KdpBreakpointTable[Index].Address <= Upper))
           ) {

            //
            // Breakpoint is in use and falls in range, restore original
            // code in WaitStateChange InstructionStream 
            //

            if (Is16BitSupported) {
#if defined(MIPSII) || defined(THUMBSUPPORT)
                if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_16BIT) {
                    Instruction = KDP_BREAKPOINT_16BIT_VALUE;
                    Length = sizeof(KDP_BREAKPOINT_16BIT_TYPE);
                } else {
                    Instruction = KDP_BREAKPOINT_32BIT_VALUE;
                    Length = sizeof(KDP_BREAKPOINT_32BIT_TYPE);
                }
#endif
                if (KdpBreakpointTable[Index].Content != Instruction) {
                    Offset = (ULONG)KdpBreakpointTable[Index].Address -
                             (ULONG)Lower;
                    KdpMoveMemory(
                        &(WaitStateChange->ControlReport.InstructionStream[0])
                        + Offset,
                        (PCHAR)&KdpBreakpointTable[Index].Content,
                        Length
                    );
                }

            } else {

                if (KdpBreakpointTable[Index].Content != KDP_BREAKPOINT_VALUE) {
                    Offset = (ULONG)KdpBreakpointTable[Index].Address -
                             (ULONG)Lower;
                    KdpMoveMemory(
                        &(WaitStateChange->ControlReport.InstructionStream[0])
                        + Offset,
                        (PCHAR)&KdpBreakpointTable[Index].Content,
                        sizeof(KDP_BREAKPOINT_TYPE)
                        );
                }
            }

        }
    }
#if defined(x86)
    


    WaitStateChange->ControlReport.Dr6 = ContextRecord->Dr6;
    WaitStateChange->ControlReport.Dr7 = ContextRecord->Dr7;

    
    WaitStateChange->ControlReport.SegCs = (WORD)ContextRecord->SegCs;
    WaitStateChange->ControlReport.SegDs = (WORD)ContextRecord->SegDs;
    WaitStateChange->ControlReport.SegEs = (WORD)ContextRecord->SegEs;
    WaitStateChange->ControlReport.SegFs = (WORD)ContextRecord->SegFs;

    WaitStateChange->ControlReport.EFlags = ContextRecord->EFlags;

    WaitStateChange->ControlReport.ReportFlags = REPORT_INCLUDES_SEGS;
#endif
}

VOID
KdpGetStateChange (
    IN PDBGKD_MANIPULATE_STATE ManipulateState,
    IN PCONTEXT ContextRecord
    )

/*++

Routine Description:

    Extract continuation control data from Manipulate_State message

    N.B. This is a noop for MIPS.

Arguments:

    ManipulateState - supplies pointer to Manipulate_State packet

    ContextRecord - Supplies a pointer to a context record.

Return Value:

    None.

--*/

{
#if defined(x86)
    PDBGKD_CONTROL_SET pControlSet;

    pControlSet = &ManipulateState->u.Continue2.ControlSet;

    if (pControlSet->TraceFlag)
    {
        ContextRecord->EFlags |= 0x00000100;     // TF
    }
    else
    {
        ContextRecord->EFlags &= ~0x00000100;    // TF
    }

    


    ContextRecord->Dr6 = 0;

    ContextRecord->Dr7 = pControlSet->Dr7;
#endif
}


BOOL CheckIfPThreadExists (PTHREAD pThreadToValidate)

{
    DWORD dwProcessIdx;
    PTHREAD pThread;

    for (dwProcessIdx = 0; dwProcessIdx < MAX_PROCESSES; dwProcessIdx++)    
    { // For each process:
        if (kdProcArray [dwProcessIdx].dwVMBase)
        { // Only if VM Base Addr of process is valid (not null):
            for (pThread = kdProcArray [dwProcessIdx].pTh; // Get process main thread
                 pThread;
                 pThread = pThread->pNextInProc) // Next thread
            { // walk list of threads attached to process (until we reach the end of the list or we found the matching pThread)
                if (pThreadToValidate == pThread) return TRUE;
            }
        }
    }
    return FALSE;
}

VOID
KdpReadControlSpace (
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a read control space state
    manipulation message.  Its function is to read implementation
    specific system data.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{

    PDBGKD_READ_MEMORY a = &m->u.ReadMemory;
    STRING MessageHeader;
    ULONG NextOffset;
    DWORD dwReturnAddr;
    DWORD dwFramePtr = 0;
    DWORD dwStackPtr = 0;
    DWORD dwProcHandle = 0;


    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    m->ReturnStatus = STATUS_UNSUCCESSFUL; // By default (shorter and safer)

    //
    // TargetBaseAddress field is being used as the ControlSpace request id 
    //
    switch((ULONG)a->TargetBaseAddress) {

        case HANDLE_PROCESS_THREAD_INFO_REQ: // Replacing HANDLE_PROCESS_INFO_REQUEST

            GetProcessAndThreadInfo (AdditionalData);
            m->ReturnStatus = STATUS_SUCCESS; // the return status is in the message itself
            break;

        case HANDLE_GET_NEXT_OFFSET_REQUEST:

            if (AdditionalData->Length != sizeof(ULONG)) {
                AdditionalData->Length = 0;
                break; // Unsucessful
            }

            memcpy((PVOID)&NextOffset, (PVOID)AdditionalData->Buffer, sizeof(ULONG));

            if (!TranslateAddress(&NextOffset, Context)) {
                AdditionalData->Length = 0;
                break; // Unsucessful
            }

            memcpy((PVOID)AdditionalData->Buffer, (PVOID)&NextOffset, sizeof(ULONG));


            m->ReturnStatus = STATUS_SUCCESS;
            AdditionalData->Length = sizeof(ULONG);

            break;

        case HANDLE_STACKWALK_REQUEST:
            
            //
            // Translate a stack frame address
            //
            
            DEBUGGERMSG(KDZONE_STACKW, (L"++ReadCtrl HANDLE_STACKWALK_REQUEST\r\n"));

            if ((sizeof(DWORD) != AdditionalData->Length) && 
                (2*sizeof(DWORD) != AdditionalData->Length) &&
                (3*sizeof(DWORD) != AdditionalData->Length))
            {
                //
                // Invalid argument
                //
                DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl Bad AdditionalData Length (%i)\r\n", AdditionalData->Length));

                AdditionalData->Length = 0;
            }
            else
            {
                memcpy((VOID*)&dwReturnAddr, (PVOID)AdditionalData->Buffer, sizeof(DWORD));
                if (2*sizeof(DWORD) <= AdditionalData->Length)
                {
                    memcpy((PVOID)&dwFramePtr, (PVOID)(AdditionalData->Buffer + sizeof(DWORD)), sizeof(DWORD));

                    DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl dwFramePtr=%8.8lX\r\n", dwFramePtr));
                }
                else
                {
                    dwFramePtr = 0;
                }

                DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl Old RetAddr=%8.8lX\r\n", dwReturnAddr));

                if (!TranslateRA (&dwReturnAddr, &dwStackPtr, dwFramePtr, &dwProcHandle))
                {
                    DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl TranslateRA FAILED\r\n"));

                    AdditionalData->Length = 0;
                }
                else
                {
                    DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl: New RetAddr=%8.8lX\r\n", dwReturnAddr));
                    if (dwStackPtr)
                    {
                        DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl: New dwStackPtr=%8.8lX\r\n", dwStackPtr));
                    }
                    DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl: New dwProcHandle=%8.8lX\r\n", dwProcHandle));

                    memcpy((PVOID)AdditionalData->Buffer, (PVOID)&dwReturnAddr, sizeof(DWORD));
                    AdditionalData->Length = sizeof(DWORD);

                    if (g_fReturnPrevSP)
                    {
                        memcpy ((PVOID) (AdditionalData->Buffer + AdditionalData->Length), (PVOID)&dwStackPtr, sizeof(DWORD));
                        AdditionalData->Length += sizeof(DWORD);
                        if (g_fReturnProcHandle)
                        {
                            memcpy ((PVOID) (AdditionalData->Buffer + AdditionalData->Length), (PVOID)&dwProcHandle, sizeof(DWORD));
                            AdditionalData->Length += sizeof(DWORD);
                        }
                    }

                    m->ReturnStatus = STATUS_SUCCESS;
                }
            }

            DEBUGGERMSG(KDZONE_STACKW, (L"--ReadCtrl HANDLE_STACKWALK_REQUEST\r\n"));
            break;
              
        case HANDLE_THREADSTACK_REQUEST:

            //
            // Set up a (thread-specific) stackwalk
            //

            DEBUGGERMSG(KDZONE_STACKW, (L"++ReadCtrl HANDLE_THREADSTACK_REQUEST\r\n")); 

            pWalkThread = (PTHREAD)Kdstrtoul(AdditionalData->Buffer, 0x10);

            // Ensure pWalkThread is valid 
            if (!CheckIfPThreadExists(pWalkThread))
            {
                DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl pWalkThread is invalid (%8.8lX)\r\n", (DWORD)pWalkThread)); 
                
                AdditionalData->Length = 0;
            }
            else
            {
                fThreadWalk = TRUE;

                pStk = pWalkThread->pcstkTop;
                pLastProc = pWalkThread->pProc;
                
                DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl pWalkThread=%8.8lX pLastProc=%8.8lX pStk=%8.8lX\r\n", pWalkThread, pLastProc, pStk));

                //
                // The thread's context will be returned in AdditionalData->Buffer
                //

                AdditionalData->Length = sizeof(CONTEXT);
                if (pWalkThread == pCurThread)
                {
                    memcpy(AdditionalData->Buffer, Context, sizeof(CONTEXT));
                }
                else
                {
                    CpuContextToContext((CONTEXT*)AdditionalData->Buffer, &pWalkThread->ctx);
                }

                DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl Context copied\r\n"));

                //
                // Slotize the PC if not in the kernel or a DLL
                //

                if (pLastProc)
                {
                    if ((ZeroPtr(CONTEXT_TO_PROGRAM_COUNTER((PCONTEXT)AdditionalData->Buffer)) > (1 << VA_SECTION)) || 
                        (ZeroPtr(CONTEXT_TO_PROGRAM_COUNTER((PCONTEXT)AdditionalData->Buffer)) < (DWORD)DllLoadBase))
                         CONTEXT_TO_PROGRAM_COUNTER((PCONTEXT)AdditionalData->Buffer) =
                           (UINT)MapPtrProc(CONTEXT_TO_PROGRAM_COUNTER((PCONTEXT)AdditionalData->Buffer), pLastProc);
                }
                else
                {
                    DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl *** ERROR pLastProc is NULL\r\n"));
                }

                m->ReturnStatus = STATUS_SUCCESS;
            }

            DEBUGGERMSG(KDZONE_STACKW, (L"--ReadCtrl HANDLE_THREADSTACK_REQUEST\r\n")); 

            break;

        case HANDLE_THREADSTACK_TERMINATE:

            //
            // Terminate a (thread-specific) stackwalk
            //

            DEBUGGERMSG(KDZONE_STACKW, (L"++ReadCtrl HANDLE_THREADSTACK_TERMINATE\r\n")); 

            fThreadWalk = FALSE;

            m->ReturnStatus = STATUS_SUCCESS;

            DEBUGGERMSG(KDZONE_STACKW, (L"--ReadCtrl HANDLE_THREADSTACK_TERMINATE\r\n")); 

            break;

        case HANDLE_RELOAD_MODULES_REQUEST:
            AdditionalData->Length = (WORD)ReloadAllSymbols( AdditionalData->Buffer, FALSE);
            m->ReturnStatus = STATUS_SUCCESS;           
            break;

        case HANDLE_RELOAD_MODULES_INFO:
            AdditionalData->Length = (WORD)ReloadAllSymbols( AdditionalData->Buffer, TRUE);
            m->ReturnStatus = STATUS_SUCCESS;           
            break;

        case HANDLE_GETCURPROCTHREAD:
            memcpy( AdditionalData->Buffer, &pCurProc, sizeof(DWORD));
            memcpy( AdditionalData->Buffer+(sizeof(DWORD)), &pCurThread, sizeof(DWORD));
            memcpy( AdditionalData->Buffer+(sizeof(DWORD)*2), &hCurProc, sizeof(DWORD));
            memcpy( AdditionalData->Buffer+(sizeof(DWORD)*3), &hCurThread, sizeof(DWORD));
            memcpy( AdditionalData->Buffer+(sizeof(DWORD)*4), &(pCurThread->pOwnerProc), sizeof(DWORD));
            memcpy( AdditionalData->Buffer+(sizeof(DWORD)*5), &((PROCESS *)(pCurThread->pOwnerProc)->hProc), sizeof(DWORD));
            AdditionalData->Length = sizeof(DWORD)*6;
            m->ReturnStatus = STATUS_SUCCESS;
            break;

        case HANDLE_GET_EXCEPTION_REGISTRATION:
#if defined(x86)
            DEBUGGERMSG(KDZONE_CTRL, (L"++ReadCtrl HANDLE_GET_EXCEPTION_REGISTRATION\r\n"));
            // This is only valid for x86 platforms!!!  It is used
            // by the kernel debugger for unwinding through exception
            // handlers
            if (AdditionalData->Length == 2*sizeof(VOID*))
            {
                CALLSTACK* pStk;
                VOID* pRegistration;
                VOID* pvCallStack = AdditionalData->Buffer+sizeof(VOID*);
                
                memcpy(&pStk, pvCallStack, sizeof(VOID*));
                if (pStk)
                {
                    // we are at a PSL boundary and need to look up the next registration
                    // pointer -- don't trust the pointer we got from the host
                    pRegistration = (VOID*)pStk->extra;
                    pStk = pStk->pcstkNext;
                }
                else
                {
                    // request for the registration head pointer
                    pRegistration = GetRegistrationHead();
                    pStk = pCurThread->pcstkTop;
                }
                
                memcpy(AdditionalData->Buffer, &pRegistration, sizeof(VOID*));
                memcpy(pvCallStack, &pStk, sizeof(VOID*));
                DEBUGGERMSG(KDZONE_CTRL, (L"  Registration Ptr: %8.8lx pCallStack: %8.8lx\r\n", (DWORD)pRegistration, (DWORD)pStk));
                // AdditionalData->Length stays the same (2*sizeof(VOID*))
                m->ReturnStatus = STATUS_SUCCESS;
                
            } 
            else
            {
                // TODO: REMOVE -- OBSOLETE
                PVOID pRH = GetRegistrationHead();
                DEBUGGERMSG(KDZONE_CTRL, (L"Registration Head = %08X.\r\n", pRH) ); 
                memcpy( AdditionalData->Buffer, &pRH, sizeof(PVOID) );
                AdditionalData->Length = sizeof(PVOID);
                m->ReturnStatus = STATUS_SUCCESS;
            }
#else
            DEBUGGERMSG(KDZONE_CTRL, (L"Registration Head request not supported on this platform.\r\n")); 
            AdditionalData->Length = 0;
#endif
            DEBUGGERMSG(KDZONE_CTRL, (L"--ReadCtrl HANDLE_GET_EXCEPTION_REGISTRATION\r\n"));

            break;

        default:
            AdditionalData->Length = 0;
            break; // Unsuccessful
    }

    if (AdditionalData->Length > a->TransferCount) {
        AdditionalData->Length = (USHORT)a->TransferCount;
    }

    a->ActualBytesRead = AdditionalData->Length;

    KdpSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData
        );
}

VOID
KdpWriteControlSpace (
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context
    )

/*++

Routine Description:

    This function is called in response of a write control space state
    manipulation message.  Its function is to write implementation
    specific system data.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

Return Value:

    None.

--*/

{
    PDBGKD_WRITE_MEMORY a = &m->u.WriteMemory;
    STRING MessageHeader;
    LPSTR Params;
    HANDLE hProcessFocus;

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    m->ReturnStatus = STATUS_UNSUCCESSFUL; // By default (shorter and safer)

    //
    // None of these commands actually write anything directly to memory
    //

    a->ActualBytesWritten = 0;

    DEBUGGERMSG(KDZONE_CTRL, (L"++KdpWriteControlSpace\r\n"));
    switch((ULONG)a->TargetBaseAddress)
    {
        case HANDLE_PROCESS_SWITCH_REQUEST:

            Params = AdditionalData->Buffer;
		    DEBUGGERMSG(KDZONE_CTRL, (L"  KdpWriteControlSpace HANDLE_PROCESS_SWITCH_REQUEST %a\r\n", Params));
            hProcessFocus = (HANDLE) Kdstrtoul(Params, 16); // we are getting hProc as a param
            g_pFocusProcOverride = HandleToProc (hProcessFocus);
			if (g_pFocusProcOverride)
			{
				m->ReturnStatus = STATUS_SUCCESS;
			}
		    DEBUGGERMSG(KDZONE_CTRL, (L"  KdpWriteControlSpace g_pFocusProcOverride=0x%08X\r\n", g_pFocusProcOverride));
		    break;

        case HANDLE_THREAD_SWITCH_REQUEST:

			// Unsupported in CE
			/*
            Params = AdditionalData->Buffer;
            Thread = Kdstrtoul(Params, 16);

            if (!SwitchToThread((PTHREAD)Thread)) {
                break; // Unsuccessful
            }

            m->ReturnStatus = STATUS_SUCCESS;
			*/
            break;

        case HANDLE_STACKWALK_REQUEST:

            //
            // Set up for a stackwalk of the current thread
            //
            
            DEBUGGERMSG(KDZONE_STACKW, (L"++WriteCtrl HANDLE_STACKWALK_REQUEST\r\n"));
            
            if (!fThreadWalk)
            {
                pStk        = pCurThread->pcstkTop;
                pLastProc   = pCurProc;
                pWalkThread = pCurThread;
            }
            else
            {
                DEBUGGERMSG(KDZONE_STACKW, (L"  WriteCtrl Thread stackwalk in progress - not updating state\r\n"));
            }

            DEBUGGERMSG(KDZONE_STACKW, (L"  WriteCtrl pStk=%8.8lx pLastProc=%8.8lx pWalkThread=%8.8lx\r\n", pStk, pLastProc, pCurThread)); 

            m->ReturnStatus = STATUS_SUCCESS;

            DEBUGGERMSG(KDZONE_STACKW, (L"--WriteCtrl HANDLE_STACKWALK_REQUEST\r\n"));
            
            break;

       default:
            break; // Unsuccessful
    }

    KdpSendPacket(
        PACKET_TYPE_KD_STATE_MANIPULATE,
        &MessageHeader,
        AdditionalData
        );
    DEBUGGERMSG(KDZONE_CTRL, (L"--KdpWriteControlSpace\r\n"));
}

VOID
KdpReadIoSpace (
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context,
    IN BOOL fSendPacket
    )

/*++

Routine Description:

    This function is called in response of a read io space state
    manipulation message.  Its function is to read system io
    locations.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

    fSendPacket - TRUE send packet to Host, otherwise not

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_IO a = &m->u.ReadWriteIo;
    STRING MessageHeader;
#if !defined(x86)
    PUCHAR b;
    PUSHORT s;
    PULONG l;
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    KD_ASSERT(AdditionalData->Length == 0);

    m->ReturnStatus = STATUS_SUCCESS;

    //
    // Check Size and Alignment
    //

    switch ( a->DataSize ) {
#if defined (x86) // x86 processor have a separate io mapping
        case 1:
            a->DataValue = _inp( (SHORT) a->IoAddress);
            break;
        case 2:
            a->DataValue = _inpw ((SHORT) a->IoAddress);
            break;
        case 4:
            a->DataValue = _inpd ((SHORT) a->IoAddress);
            break;
#else // all processors other than x86 use the default memory mapped version
        case 1:
            b = (PUCHAR)MmDbgReadCheck(a->IoAddress);
            if ( b ) {
                a->DataValue = (ULONG)*b;
            } else {
                m->ReturnStatus = STATUS_ACCESS_VIOLATION;
            }
            break;
        case 2:
            if ((ULONG)a->IoAddress & 1 ) {
                m->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                s = (PUSHORT)MmDbgReadCheck(a->IoAddress);
                if ( s ) {
                    a->DataValue = (ULONG)*s;
                } else {
                    m->ReturnStatus = STATUS_ACCESS_VIOLATION;
                }
            }
            break;
        case 4:
            if ((ULONG)a->IoAddress & 3 ) {
                m->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                l = (PULONG)MmDbgReadCheck(a->IoAddress);
                if ( l ) {
                    a->DataValue = (ULONG)*l;
                } else {
                    m->ReturnStatus = STATUS_ACCESS_VIOLATION;
                }
            }
            break;
#endif
        default:
            m->ReturnStatus = STATUS_INVALID_PARAMETER;
    }

    if (fSendPacket)
    {
        KdpSendPacket(
            PACKET_TYPE_KD_STATE_MANIPULATE,
            &MessageHeader,
            NULL
            );
    }
}

VOID
KdpWriteIoSpace (
    IN PDBGKD_MANIPULATE_STATE m,
    IN PSTRING AdditionalData,
    IN PCONTEXT Context,
    IN BOOL fSendPacket
    )

/*++

Routine Description:

    This function is called in response of a write io space state
    manipulation message.  Its function is to write to system io
    locations.

Arguments:

    m - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

    Context - Supplies the current context.

    fSendPacket - TRUE send packet to Host, otherwise not

Return Value:

    None.

--*/

{
    PDBGKD_READ_WRITE_IO a = &m->u.ReadWriteIo;
    STRING MessageHeader;
#if !defined(x86)
    PUCHAR b;
    PUSHORT s;
    PULONG l;
#endif

    MessageHeader.Length = sizeof(*m);
    MessageHeader.Buffer = (PCHAR)m;

    KD_ASSERT(AdditionalData->Length == 0);

    m->ReturnStatus = STATUS_SUCCESS;

    //
    // Check Size and Alignment
    //

    switch ( a->DataSize ) {
#if defined(x86) // x86 processor have a separate io mapping
        case 1:
            _outp ((SHORT) a->IoAddress, a->DataValue);
            break;
        case 2:
            _outpw ((SHORT) a->IoAddress, (WORD) a->DataValue);
            break;
        case 4:
            _outpd ((SHORT) a->IoAddress, (DWORD) a->DataValue);
            break;
#else // all processors other than x86 use the default memory mapped version
        case 1:
            b = (PUCHAR)MmDbgWriteCheck(a->IoAddress);
            if ( b ) {
                WRITE_REGISTER_UCHAR(b,(UCHAR)a->DataValue);
            } else {
                m->ReturnStatus = STATUS_ACCESS_VIOLATION;
            }
            break;
        case 2:
            if ((ULONG)a->IoAddress & 1 ) {
                m->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                s = (PUSHORT)MmDbgWriteCheck(a->IoAddress);
                if ( s ) {
                    WRITE_REGISTER_USHORT(s,(USHORT)a->DataValue);
                } else {
                    m->ReturnStatus = STATUS_ACCESS_VIOLATION;
                }
            }
            break;
        case 4:
            if ((ULONG)a->IoAddress & 3 ) {
                m->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                l = (PULONG)MmDbgWriteCheck(a->IoAddress);
                if ( l ) {
                    WRITE_REGISTER_ULONG(l,a->DataValue);
                } else {
                    m->ReturnStatus = STATUS_ACCESS_VIOLATION;
                }
            }
            break;
#endif
        default:
            m->ReturnStatus = STATUS_INVALID_PARAMETER;
    }

    if (fSendPacket)
    {
        KdpSendPacket(
            PACKET_TYPE_KD_STATE_MANIPULATE,
            &MessageHeader,
            NULL
            );
    }
}

DWORD ReloadAllSymbols(LPBYTE lpBuffer, BOOL fDoCopy)
{
    int i;
    DWORD dwSize;
    PMODULE pMod;
    WCHAR *szModName;
    DWORD dwBasePtr, dwModuleSize, dwRwDataStart, dwRwDataEnd;

    dwSize = 0;
    for (i=0; i < MAX_PROCESSES; i++)
    {
        if (kdProcArray[i].dwVMBase)
        {
            szModName = kdProcArray[i].lpszProcName;
            dwBasePtr = (DWORD)kdProcArray[i].BasePtr;
            if (0 == i)
            { // kernel
                dwRwDataStart = ((COPYentry *)(pTOC->ulCopyOffset))->ulDest;
                dwRwDataEnd   = dwRwDataStart + ((COPYentry *)(pTOC->ulCopyOffset))->ulDestLen - 1;
            }
            else
            {
                dwRwDataStart = 0xFFFFFFFF;
                dwRwDataEnd = 0x00000000;
            }

            if (dwBasePtr == 0x10000)
            {
                dwBasePtr |= kdProcArray[i].dwVMBase;
            }
            dwModuleSize = kdProcArray[i].e32.e32_vsize;
            if (fDoCopy) kdbgWtoA(szModName,lpBuffer+dwSize);
            dwSize = dwSize+kdbgwcslen(szModName)+1;
            if (fDoCopy) memcpy( lpBuffer+dwSize,  &dwBasePtr, sizeof(DWORD));
            dwSize += sizeof(DWORD);
            if (fDoCopy) memcpy( lpBuffer+dwSize,  &dwModuleSize, sizeof(DWORD));
            dwSize += sizeof(DWORD);
            if (g_fCanRelocateRwData)
            {
	            if (fDoCopy) memcpy( lpBuffer+dwSize,  &dwRwDataStart, sizeof(DWORD));
	            dwSize += sizeof(DWORD);
	            if (fDoCopy) memcpy( lpBuffer+dwSize,  &dwRwDataEnd, sizeof(DWORD));
	            dwSize += sizeof(DWORD);
            }

        }
    }
    pMod = pModList;
    while(pMod)
    {
        szModName = pMod->lpszModName;
        dwBasePtr = ZeroPtr(pMod->BasePtr);
        dwModuleSize = pMod->e32.e32_vsize;
        dwRwDataStart = pMod->rwLow;
        dwRwDataEnd = pMod->rwHigh;
        if (fDoCopy) kdbgWtoA(szModName, lpBuffer+dwSize);
        dwSize = dwSize+kdbgwcslen(szModName)+1;
        if (fDoCopy) memcpy( lpBuffer+dwSize,  &dwBasePtr, sizeof(DWORD));
        dwSize += sizeof(DWORD);
        if (fDoCopy) memcpy( lpBuffer+dwSize,  &dwModuleSize, sizeof(DWORD));
        dwSize += sizeof(DWORD);
        if (g_fCanRelocateRwData)
        {
            if (fDoCopy) memcpy( lpBuffer+dwSize,  &dwRwDataStart, sizeof(DWORD));
            dwSize += sizeof(DWORD);
            if (fDoCopy) memcpy( lpBuffer+dwSize,  &dwRwDataEnd, sizeof(DWORD));
            dwSize += sizeof(DWORD);
        }
        pMod=pMod->pMod;
    }
    if (!fDoCopy) 
    {
        memcpy( lpBuffer, &dwSize, sizeof(DWORD));
    }
    return dwSize;
}   
