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


BOOL fThreadWalk;

DWORD ReloadAllSymbols(LPBYTE lpBuffer, BOOL fDoCopy);
static NTSTATUS GetModuleRefCount(HMODULE hDll, LPVOID pBuffer, USHORT *pnLength);


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
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    )

/*++

Routine Description:

    This function is called in response of a read control space command
    message.  Its function is to read implementation
    specific system data.

Arguments:

    pdbgkdCmdPacket - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

Return Value:

    None.

--*/

{

    DBGKD_READ_MEMORY *a = &pdbgkdCmdPacket->u.ReadMemory;
    STRING MessageHeader;
    ULONG NextOffset;
    DWORD dwReturnAddr;
    DWORD dwFramePtr = 0;
    DWORD dwStackPtr = 0;
    DWORD dwProcHandle = 0;


    MessageHeader.Length = sizeof(*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR)pdbgkdCmdPacket;

    pdbgkdCmdPacket->dwReturnStatus = STATUS_UNSUCCESSFUL; // By default (shorter and safer)

    //
    // qwTgtAddress field is being used as the ControlSpace request id
    //
    switch (a->qwTgtAddress)
    {

        case HANDLE_PROCESS_THREAD_INFO_REQ: // Replacing HANDLE_PROCESS_INFO_REQUEST

            GetProcessAndThreadInfo (AdditionalData);
            pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS; // the return status is in the message itself
            break;

        case HANDLE_GET_NEXT_OFFSET_REQUEST:

            if (AdditionalData->Length != sizeof(ULONG)) {
                AdditionalData->Length = 0;
                break; // Unsucessful
            }

            memcpy((PVOID)&NextOffset, (PVOID)AdditionalData->Buffer, sizeof(ULONG));

            if (!TranslateAddress(&NextOffset, g_pctxException)) {
                AdditionalData->Length = 0;
                break; // Unsucessful
            }

            memcpy((PVOID)AdditionalData->Buffer, (PVOID)&NextOffset, sizeof(ULONG));


            pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
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
                // Invalid argument
                DEBUGGERMSG (KDZONE_ALERT, (L"  ReadCtrl Bad AdditionalData Length (%i)\r\n", AdditionalData->Length));

                AdditionalData->Length = 0;
            }
            else
            {
                memcpy((VOID*)&dwReturnAddr, (PVOID)AdditionalData->Buffer, sizeof(DWORD));
                if (2*sizeof(DWORD) <= AdditionalData->Length)
                {
                    memcpy((PVOID)&dwFramePtr, (PVOID)(AdditionalData->Buffer + sizeof(DWORD)), sizeof(DWORD));

                    DEBUGGERMSG (KDZONE_STACKW, (L"  ReadCtrl dwFramePtr=%8.8lX\r\n", dwFramePtr));
                }
                else
                {
                    dwFramePtr = 0;
                }

                DEBUGGERMSG (KDZONE_STACKW, (L"  ReadCtrl Old RetAddr=%8.8lX\r\n", dwReturnAddr));

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

                    // Pass Previous SP
                    memcpy ((PVOID) (AdditionalData->Buffer + AdditionalData->Length), (PVOID)&dwStackPtr, sizeof(DWORD));
                    AdditionalData->Length += sizeof(DWORD);

                    memcpy ((PVOID) (AdditionalData->Buffer + AdditionalData->Length), (PVOID)&dwProcHandle, sizeof(DWORD));
                    AdditionalData->Length += sizeof(DWORD);

                    pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
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
                DEBUGGERMSG (KDZONE_ALERT, (L"  ReadCtrl pWalkThread is invalid (%8.8lX)\r\n", (DWORD)pWalkThread));

                AdditionalData->Length = 0;
            }
            else
            {
                fThreadWalk = TRUE;
                g_fTopFrame = TRUE;

                pStk = pWalkThread->pcstkTop;
                pLastProc = pWalkThread->pProc;

                DEBUGGERMSG(KDZONE_STACKW, (L"  ReadCtrl pWalkThread=%8.8lX pLastProc=%8.8lX pStk=%8.8lX\r\n", pWalkThread, pLastProc, pStk));

                //
                // The thread's context will be returned in AdditionalData->Buffer
                //

                AdditionalData->Length = sizeof(CONTEXT);
                if (pWalkThread == pCurThread)
                {
                    memcpy(AdditionalData->Buffer, g_pctxException, sizeof(CONTEXT));
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

                pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
            }

            DEBUGGERMSG(KDZONE_STACKW, (L"--ReadCtrl HANDLE_THREADSTACK_REQUEST\r\n"));

            break;

        case HANDLE_THREADSTACK_TERMINATE:

            //
            // Terminate a (thread-specific) stackwalk
            //

            DEBUGGERMSG(KDZONE_STACKW, (L"++ReadCtrl HANDLE_THREADSTACK_TERMINATE\r\n"));

            fThreadWalk = FALSE;

            pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;

            DEBUGGERMSG(KDZONE_STACKW, (L"--ReadCtrl HANDLE_THREADSTACK_TERMINATE\r\n"));

            break;

        case HANDLE_RELOAD_MODULES_REQUEST:
            AdditionalData->Length = (WORD)ReloadAllSymbols( AdditionalData->Buffer, FALSE);
            pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
            break;

        case HANDLE_RELOAD_MODULES_INFO:
            AdditionalData->Length = (WORD)ReloadAllSymbols( AdditionalData->Buffer, TRUE);
            pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
            break;

        case HANDLE_GETCURPROCTHREAD:
            memcpy( AdditionalData->Buffer, &pCurProc, sizeof(DWORD));
            memcpy( AdditionalData->Buffer+(sizeof(DWORD)), &pCurThread, sizeof(DWORD));
            memcpy( AdditionalData->Buffer+(sizeof(DWORD)*2), &hCurProc, sizeof(DWORD));
            memcpy( AdditionalData->Buffer+(sizeof(DWORD)*3), &hCurThread, sizeof(DWORD));
            memcpy( AdditionalData->Buffer+(sizeof(DWORD)*4), &(pCurThread->pOwnerProc), sizeof(DWORD));
            memcpy( AdditionalData->Buffer+(sizeof(DWORD)*5), &((PROCESS *)(pCurThread->pOwnerProc)->hProc), sizeof(DWORD));
            AdditionalData->Length = sizeof(DWORD)*6;
            pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
            break;

        case HANDLE_GET_EXCEPTION_REGISTRATION:
#if defined(x86)
            DEBUGGERMSG(KDZONE_CTRL, (L"++ReadCtrl HANDLE_GET_EXCEPTION_REGISTRATION\r\n"));
            // This is only valid for x86 platforms!!!  It is used
            // by the kernel debugger for unwinding through exception
            // handlers
            if (AdditionalData->Length == 2*sizeof(VOID*))
            {
                CALLSTACK* pStack;
                VOID* pRegistration;
                VOID* pvCallStack = AdditionalData->Buffer+sizeof(VOID*);

                memcpy(&pStack, pvCallStack, sizeof(VOID*));
                if (pStack)
                {
                    // we are at a PSL boundary and need to look up the next registration
                    // pointer -- don't trust the pointer we got from the host
                    pRegistration = (VOID*)pStack->extra;
                    pStack = pStack->pcstkNext;
                }
                else
                {
                    // request for the registration head pointer
                    pRegistration = GetRegistrationHead();
                    pStack = pCurThread->pcstkTop;
                }

                memcpy(AdditionalData->Buffer, &pRegistration, sizeof(VOID*));
                memcpy(pvCallStack, &pStack, sizeof(VOID*));
                DEBUGGERMSG(KDZONE_CTRL, (L"  ReadCtrl HANDLE_GET_EXCEPTION_REGISTRATION: Registration Ptr: %8.8lx pCallStack: %8.8lx\r\n", (DWORD)pRegistration, (DWORD)pStack));
                // AdditionalData->Length stays the same (2*sizeof(VOID*))
                pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;

            }
            else
            { // ERROR case!!!
                DEBUGGERMSG (KDZONE_ALERT, (L"  ReadCtrl HANDLE_GET_EXCEPTION_REGISTRATION: Invalid parameter.\r\n"));
            }
#else
            DEBUGGERMSG (KDZONE_ALERT, (L"  ReadCtrl HANDLE_GET_EXCEPTION_REGISTRATION: Registration Head request not supported on this platform.\r\n"));
            AdditionalData->Length = 0;
#endif
            DEBUGGERMSG (KDZONE_CTRL, (L"--ReadCtrl HANDLE_GET_EXCEPTION_REGISTRATION\r\n"));

            break;
        case HANDLE_MODULE_REFCOUNT_REQUEST:
            DEBUGGERMSG(KDZONE_CTRL, (L"++ReadCtrl HANDLE_MODULE_REFCOUNT_REQUEST\r\n"));

            /* Sanity check... */
            if (AdditionalData->Length != sizeof(HMODULE))
            {
                DEBUGGERMSG(KDZONE_CTRL, (L"--ReadCtrl HANDLE_MODULE_REFCOUNT_REQUEST\r\n"));
                pdbgkdCmdPacket->dwReturnStatus = STATUS_INVALID_PARAMETER;
                AdditionalData->Length = 0;
                break;
            }

            pdbgkdCmdPacket->dwReturnStatus = GetModuleRefCount(*((HMODULE *) AdditionalData->Buffer), AdditionalData->Buffer, &AdditionalData->Length);
            DEBUGGERMSG(KDZONE_CTRL, (L"--ReadCtrl HANDLE_MODULE_REFCOUNT_REQUEST\r\n"));
            break;
        case HANDLE_DESC_HANDLE_DATA:
            pdbgkdCmdPacket->dwReturnStatus = KdQueryHandleFields((PDBGKD_HANDLE_DESC_DATA) AdditionalData->Buffer, AdditionalData->Length);
            break;
        case HANDLE_GET_HANDLE_DATA:
            pdbgkdCmdPacket->dwReturnStatus = KdQueryHandleList((PDBGKD_HANDLE_GET_DATA) AdditionalData->Buffer, AdditionalData->Length);
            break;
        default:
            pdbgkdCmdPacket->dwReturnStatus = STATUS_NOT_IMPLEMENTED;
            AdditionalData->Length = 0;
            break; // Unsuccessful
    }

    if (AdditionalData->Length > a->dwTransferCount) {
        AdditionalData->Length = (USHORT)a->dwTransferCount;
    }

    a->dwActualBytesRead = AdditionalData->Length;

    KdpSendKdApiCmdPacket (&MessageHeader, AdditionalData);
}

VOID
KdpWriteControlSpace (
    IN DBGKD_COMMAND *pdbgkdCmdPacket,
    IN PSTRING AdditionalData
    )

/*++

Routine Description:

    This function is called in response of a write control space command
    message.  Its function is to write implementation
    specific system data.

Arguments:

    pdbgkdCmdPacket - Supplies the state manipulation message.

    AdditionalData - Supplies any additional data for the message.

Return Value:

    None.

--*/

{
    DBGKD_WRITE_MEMORY *a = &pdbgkdCmdPacket->u.WriteMemory;
    STRING MessageHeader;
    LPSTR Params;
    HANDLE hProcessFocus;

    MessageHeader.Length = sizeof(*pdbgkdCmdPacket);
    MessageHeader.Buffer = (PCHAR)pdbgkdCmdPacket;

    pdbgkdCmdPacket->dwReturnStatus = STATUS_UNSUCCESSFUL; // By default (shorter and safer)

    //
    // None of these commands actually write anything directly to memory
    //

    a->dwActualBytesWritten = 0;

    DEBUGGERMSG(KDZONE_CTRL, (L"++KdpWriteControlSpace\r\n"));
    switch (a->qwTgtAddress)
    {
        case HANDLE_PROCESS_SWITCH_REQUEST:

            Params = AdditionalData->Buffer;
            DEBUGGERMSG(KDZONE_CTRL, (L"  KdpWriteControlSpace HANDLE_PROCESS_SWITCH_REQUEST %a\r\n", Params));
            hProcessFocus = (HANDLE) Kdstrtoul(Params, 16); // we are getting hProc as a param
            g_pFocusProcOverride = HandleToProc (hProcessFocus);
            if (g_pFocusProcOverride)
            {
                pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
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

            pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
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
                g_fTopFrame = TRUE;
            }
            else
            {
                DEBUGGERMSG(KDZONE_STACKW, (L"  WriteCtrl Thread stackwalk in progress - not updating state\r\n"));
            }

            DEBUGGERMSG(KDZONE_STACKW, (L"  WriteCtrl pStk=%8.8lx pLastProc=%8.8lx pWalkThread=%8.8lx\r\n", pStk, pLastProc, pCurThread));

            pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;

            DEBUGGERMSG(KDZONE_STACKW, (L"--WriteCtrl HANDLE_STACKWALK_REQUEST\r\n"));

            break;

        // TODO: Change this to manipulate data directly
        case HANDLE_DELETE_HANDLE:
            if (AdditionalData->Length < sizeof(HANDLE))
            {
                pdbgkdCmdPacket->dwReturnStatus = STATUS_BUFFER_TOO_SMALL;
            }
            else if (KdCloseHandle(*((HANDLE *) AdditionalData->Buffer)))
            {
                pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;
            }
            else
            {
                pdbgkdCmdPacket->dwReturnStatus = STATUS_UNSUCCESSFUL;
            }

            break;
        default:
            break; // Unsuccessful
    }

    KdpSendKdApiCmdPacket (&MessageHeader, AdditionalData);
    DEBUGGERMSG(KDZONE_CTRL, (L"--KdpWriteControlSpace\r\n"));
}


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

    pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;

    //
    // Check Size and Alignment
    //

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
                pdbgkdCmdPacket->dwReturnStatus = STATUS_ACCESS_VIOLATION;
            }
            break;
        case 2:
            if ((ULONG)a->qwTgtIoAddress & 1 ) {
                pdbgkdCmdPacket->dwReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                s = (PUSHORT) (a->qwTgtIoAddress);
                if ( s ) {
                    a->dwDataValue = (ULONG)*s;
                } else {
                    pdbgkdCmdPacket->dwReturnStatus = STATUS_ACCESS_VIOLATION;
                }
            }
            break;
        case 4:
            if ((ULONG)a->qwTgtIoAddress & 3 ) {
                pdbgkdCmdPacket->dwReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                l = (PULONG) (a->qwTgtIoAddress);
                if ( l ) {
                    a->dwDataValue = (ULONG)*l;
                } else {
                    pdbgkdCmdPacket->dwReturnStatus = STATUS_ACCESS_VIOLATION;
                }
            }
            break;
#endif
        default:
            pdbgkdCmdPacket->dwReturnStatus = STATUS_INVALID_PARAMETER;
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

    pdbgkdCmdPacket->dwReturnStatus = STATUS_SUCCESS;

    //
    // Check Size and Alignment
    //

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
                pdbgkdCmdPacket->dwReturnStatus = STATUS_ACCESS_VIOLATION;
            }
            break;
        case 2:
            if ((ULONG)a->qwTgtIoAddress & 1 ) {
                pdbgkdCmdPacket->dwReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                s = (PUSHORT) (a->qwTgtIoAddress);
                if ( s ) {
                    WRITE_REGISTER_USHORT(s,(USHORT)a->dwDataValue);
                } else {
                    pdbgkdCmdPacket->dwReturnStatus = STATUS_ACCESS_VIOLATION;
                }
            }
            break;
        case 4:
            if ((ULONG)a->qwTgtIoAddress & 3 ) {
                pdbgkdCmdPacket->dwReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
            } else {
                l = (PULONG) (a->qwTgtIoAddress);
                if ( l ) {
                    WRITE_REGISTER_ULONG(l,a->dwDataValue);
                } else {
                    pdbgkdCmdPacket->dwReturnStatus = STATUS_ACCESS_VIOLATION;
                }
            }
            break;
#endif
        default:
            pdbgkdCmdPacket->dwReturnStatus = STATUS_INVALID_PARAMETER;
    }

    if (fSendPacket)
    {
        KdpSendKdApiCmdPacket (&MessageHeader, NULL);
    }
}

DWORD ReloadAllSymbols(LPBYTE lpBuffer, BOOL fDoCopy)
{
    int i;
    DWORD dwSize, dwTimeStamp;
    PMODULE pMod;
    WCHAR *szModName;
    DWORD dwBasePtr, dwModuleSize, dwRwDataStart, dwRwDataEnd;
    DBGKD_RELOAD_MOD_INFO_BASE rmib;
    DBGKD_RELOAD_MOD_INFO_V8 rmi8;
    DBGKD_RELOAD_MOD_INFO_V14 rmi14;
    DBGKD_RELOAD_MOD_INFO_V15 rmi15;

    dwSize = 0;
    for (i=0; i < MAX_PROCESSES; i++)
    {
        if (kdProcArray[i].dwVMBase)
        {
            szModName = kdProcArray[i].lpszProcName;
            dwBasePtr = (DWORD) kdProcArray[i].BasePtr;
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

            dwTimeStamp = kdProcArray[i].e32.e32_timestamp;

            if (dwBasePtr == 0x10000)
            {
                dwBasePtr |= kdProcArray[i].dwVMBase;
            }
            dwModuleSize = kdProcArray[i].e32.e32_vsize;
            if (fDoCopy) kdbgWtoA(szModName,lpBuffer+dwSize);
            dwSize = dwSize+kdbgwcslen(szModName)+1;

            if (fDoCopy)
            {
                rmib.dwBasePtr = dwBasePtr;
                rmib.dwModuleSize = dwModuleSize;
                memcpy(lpBuffer + dwSize, &rmib, sizeof(rmib));
            }
            dwSize += sizeof(rmib);

            // Pass Relocated RW Data
            if (fDoCopy)
            {
                rmi8.dwRwDataStart = dwRwDataStart;
                rmi8.dwRwDataEnd = dwRwDataEnd;
                memcpy(lpBuffer + dwSize, &rmi8, sizeof(rmi8));
            }
            dwSize += sizeof(rmi8);

            // Pass file time stamp data (Used for PDB matching)
            if (fDoCopy)
            {
                rmi14.dwTimeStamp = dwTimeStamp;
                memcpy(lpBuffer + dwSize, &rmi14, sizeof(rmi14));
            }

            dwSize += sizeof(rmi14);

            if (fDoCopy)
            {
                rmi15.hDll = NULL; // not valid for processes
                rmi15.dwInUse = 1 << kdProcArray[i].procnum;
                rmi15.wFlags = 0;
                rmi15.bTrustLevel = kdProcArray[i].bTrustLevel;
                memcpy(lpBuffer + dwSize, &rmi15, sizeof(rmi15));
            }
            dwSize += sizeof(rmi15);
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
        dwTimeStamp = pMod->e32.e32_timestamp;
        if (fDoCopy) kdbgWtoA(szModName, lpBuffer+dwSize);
        dwSize = dwSize+kdbgwcslen(szModName)+1;
        if (fDoCopy)
        {
            rmib.dwBasePtr = dwBasePtr;
            rmib.dwModuleSize = dwModuleSize;
            memcpy(lpBuffer + dwSize, &rmib, sizeof(rmib));
        }
        dwSize += sizeof(rmib);

        // Pass Relocated RW Data
        if (fDoCopy)
        {
            rmi8.dwRwDataStart = dwRwDataStart;
            rmi8.dwRwDataEnd = dwRwDataEnd;
            memcpy(lpBuffer + dwSize, &rmi8, sizeof(rmi8));
        }
        dwSize += sizeof(rmi8);

        // Pass file time stamp data (Used for PDB matching)
        if (fDoCopy)
        {
            rmi14.dwTimeStamp = dwTimeStamp;
            memcpy(lpBuffer + dwSize, &rmi14, sizeof(rmi14));
        }

        dwSize += sizeof(rmi14);

        if (fDoCopy)
        {
            rmi15.hDll = (HMODULE) pMod;
            rmi15.dwInUse = pMod->inuse;
            rmi15.wFlags = pMod->wFlags;
            rmi15.bTrustLevel = pMod->bTrustLevel;
            memcpy(lpBuffer + dwSize, &rmi15, sizeof(rmi15));
        }

        dwSize += sizeof(rmi15);

        pMod=pMod->pMod;
    }
    if (!fDoCopy)
    {
        memcpy( lpBuffer, &dwSize, sizeof(DWORD));
    }
    return dwSize;
}

/*++

Routine Name:

    GetModuleRefCount

Routine Description:

    This routine is used to process the HANDLE_MODULE_REFCOUNT_REQUEST
    command. It returns the reference count for the DLL requested as well
    as process handle information. The latter is required to determine
    which reference count entries are valid. (If the process entry is not
    in use, it will be NULL.)

Arguments:

    hDll            - [in]  Handle to requested DLL (ptr to Module structure)
    pBuffer         - [out] Pointer to packet buffer
    pnLength        - [out] Pointer to length of packet data returned

Return Value:

    Error status:
        STATUS_SUCCESS                  Success
        STATUS_INVALID_PARAMETER        hDll is not a valid handle
--*/
static NTSTATUS GetModuleRefCount(HMODULE hDll, LPVOID pBuffer, USHORT *pnLength)
{
    DBGKD_GET_MODULE_REFCNT *pGMRC = (DBGKD_GET_MODULE_REFCNT *) pBuffer;
    DBGKD_GET_MODULE_REFCNT_PROC *pGMRP;
    PMODULE pMod;
    NTSTATUS status;
    UINT i, j, nProcs;

    *pnLength = 0;

    /* Verify valid module */
    pMod = pModList;
    while(pMod != (PMODULE) hDll && pMod != NULL)
        pMod = pMod->pMod;

    if (pMod == NULL)
    {
        status = STATUS_INVALID_PARAMETER;
        goto Error;
    }

    nProcs = 0;
    for(i = 0; i < MAX_PROCESSES; i++)
    {
        /* If process slot is valid... */
        if (kdProcArray[i].dwVMBase != 0)
        {
            pGMRP = &pGMRC->pGMRCP[nProcs++];
            pGMRP->wRefCount = pMod->refcnt[i];

            /* We don't have strncpy in KdStub. */
            j = 0;
            do
            {
                pGMRP->szProcName[j] = kdProcArray[i].lpszProcName[j];
            }
            while(j < lengthof(pGMRP->szProcName) &&
                  kdProcArray[i].lpszProcName[j++] != L'\0');
        }
    }

    pGMRC->nProcs = nProcs;
    *pnLength = (USHORT) offsetof(DBGKD_GET_MODULE_REFCNT, pGMRCP[nProcs]);

    return STATUS_SUCCESS;

Error:
    return status;
}
