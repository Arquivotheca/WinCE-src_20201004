//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <kernel.h>


#include "UsrExceptDmp.h"


#define THREADLIST_NESTED_IN_PROCLIST 1


#define MAX_CRASH_STACK_SIZE (20)
#define START_MEM_AROUND_PC (-64)
#define STOP_MEM_AROUND_PC (64)
#define START_MEM_AROUND_SP (-2048)
#define STOP_MEM_AROUND_SP (0)


typedef struct
{
    DWORD dwOsVer;
    DWORD dwOsBuild;
}
CRASH_SYS_INFO;

typedef struct
{
    CONTEXT ctx;
    DWORD dwCode;
    DWORD dwPID;
    DWORD dwTID;
    DWORD dwMID;
    WCHAR wszModName [MAX_NAME_SIZE];
    WCHAR wszProcName [MAX_NAME_SIZE];
    DWORD dwVMBase;
    DWORD adwStack [MAX_CRASH_STACK_SIZE];
    DWORD adwMemAroundPc [(STOP_MEM_AROUND_PC - START_MEM_AROUND_PC) / sizeof (DWORD)];
    DWORD adwMemAroundSp [(STOP_MEM_AROUND_SP - START_MEM_AROUND_SP) / sizeof (DWORD)];
}
CRASH_EXCEPT_INFO;

typedef struct
{
    WCHAR wszProcName [MAX_NAME_SIZE];
    DWORD dwVMBase;
    DWORD dwBasePtr;
    DWORD dwPID;
    DWORD dwNbThreads;
}
CRASH_PROC_INFO;

typedef struct
{
    DWORD dwTID;
    BYTE bPrio;
    BYTE bBasePrio;
    DWORD dwOwningProc;
    DWORD dwCurrentProc;
    DWORD dwPC;
    DWORD dwRA;
    WORD wInfo;
    DWORD dwAky;
}
CRASH_THREAD_INFO;

typedef struct
{
    BOOL fIsDll;
    DWORD dwMID;
    WCHAR wszModName [MAX_NAME_SIZE];
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
    DWORD dwFSize;
    DWORD dwModStartAddr;
    DWORD dwModStopAddr;
    DWORD dwModRelocStartAddr;
    DWORD dwModRelocStopAddr;
    DWORD dwModUsage;
}
CRASH_MODULE_INFO;

typedef struct
{
    CRASH_SYS_INFO sys;
    CRASH_EXCEPT_INFO excpt;
    CRASH_PROC_INFO procs [MAX_PROCESSES];
    CRASH_THREAD_INFO thds [MAX_CRASH_THREADS];
    CRASH_MODULE_INFO mods [MAX_CRASH_MODULES];
}
CRASH_DATA;


#ifdef x86
#define CONTEXT_TO_SP(Context) ((Context)->Esp)
#define CONTEXT_TO_RETURN_ADDRESS(Context) (0)
#endif

#ifdef MIPS
#define CONTEXT_TO_SP(Context) ((DWORD) (Context)->IntSp)
#define CONTEXT_TO_RETURN_ADDRESS(Context) ((DWORD) (Context)->IntRa)
#endif

#ifdef SHx
#define CONTEXT_TO_SP(Context) ((Context)->R15)
#define CONTEXT_TO_RETURN_ADDRESS(Context) (0)
#endif

#ifdef ARM
#define CONTEXT_TO_SP(Context) ((Context)->Sp)
#define CONTEXT_TO_RETURN_ADDRESS(Context) ((Context)->Lr)
#endif


inline void Ascii2UnicodeMax (WCHAR *wsz, const char *sz, int max)
{
    while ((max--) && (*wsz++ = (WCHAR) *sz++));
}


inline void Unicode2AsciiMax (char *sz, const WCHAR *wsz, int max)
{
    while ((max--) && (*sz++ = (char) *wsz++));
}


inline DWORD MinDW (DWORD A, DWORD B)
{
    return (A > B) ? B : A;
}

void GetSysInfo (CRASH_SYS_INFO *pcsys)
{
    PROCESS *pProcess = (PROCESS *) UserKInfo [KINX_PROCARRAY];
    pcsys->dwOsVer = pProcess [0].e32.e32_cevermajor * 100 + pProcess [0].e32.e32_ceverminor;
}


void GetExceptionInfo (const DEBUG_EVENT *pDebugEvent, CRASH_EXCEPT_INFO *pcex, const CRASH_MODULE_INFO *pcmi)
{
    pcex->dwCode = pDebugEvent->u.Exception.ExceptionRecord.ExceptionCode;
    pcex->dwPID = pDebugEvent->dwProcessId;
    pcex->dwTID = pDebugEvent->dwThreadId;

    PROCESS *pProcess = (PROCESS *) UserKInfo [KINX_PROCARRAY];
    DWORD dwProcIdx = 0;
    while ((dwProcIdx < MAX_PROCESSES) && !(pProcess->dwVMBase && ((DWORD) pProcess->hProc == pcex->dwPID)))
    {
        dwProcIdx++;
        pProcess++;
    }
    if (dwProcIdx < MAX_PROCESSES)
    {
        ::wcsncpy (pcex->wszProcName, pProcess->lpszProcName, MAX_NAME_SIZE);
        pcex->wszProcName [MAX_NAME_SIZE - 1] = 0;

        CONTEXT *pContext = pProcess->pTh->pThrdDbg->psavedctx;
        pcex->ctx = *pContext;
        
        DWORD dwPC = CONTEXT_TO_PROGRAM_COUNTER (pContext);
        dwPC = (DWORD) MapPtrProc (dwPC, pProcess);
        DWORD dwSP = CONTEXT_TO_SP (pContext);
        dwSP = (DWORD) MapPtrProc (dwSP, pProcess);
        DWORD dwExcAddr = (DWORD) pDebugEvent->u.Exception.ExceptionRecord.ExceptionAddress;
        dwExcAddr = (DWORD) MapPtrProc (dwExcAddr, pProcess);
        
        // Use already calculated Module info to find module name
        if (!pcmi->dwModStartAddr)
        {
            DEBUGMSG (1, (L"UsrExceptDmp: ERROR: Module list must be extracted before Exception info.\r\n"));
        }
        while (pcmi->dwModStartAddr)           
        {
            if ((pcmi->dwModStartAddr <= dwPC) && (pcmi->dwModStopAddr >= dwPC))
            { // Module Found
                ::wcsncpy (pcex->wszModName, pcmi->wszModName, MAX_NAME_SIZE);
                pcex->wszModName [MAX_NAME_SIZE - 1] = 0;
                pcex->dwMID = pcmi->dwMID;
                break;
            }
            pcmi++;
        }
        
        pcex->dwVMBase = pProcess->dwVMBase;
        
        int Sidx, Tidx;
        for (Sidx = START_MEM_AROUND_PC, Tidx = 0; Tidx < ((STOP_MEM_AROUND_PC - START_MEM_AROUND_PC) / sizeof (DWORD)); Sidx += 4, Tidx++)
        {
            pcex->adwMemAroundPc [Tidx] = * (DWORD *) (dwPC + Sidx);
        }
        for (Sidx = START_MEM_AROUND_SP, Tidx = 0; Tidx < ((STOP_MEM_AROUND_SP - START_MEM_AROUND_SP) / sizeof (DWORD)); Sidx += 4, Tidx++)
        {
            pcex->adwMemAroundSp [Tidx] = * (DWORD *) (dwSP + Sidx);
        }
        pcex->adwStack [0] = dwExcAddr;
#ifdef DEBUG
        if (dwExcAddr != dwPC)
        {
            DEBUGMSG (1, (L"UsrExceptDmp : WARNING: Exception frame PC (0x%08X) is different than Exception address (0x%08X).\r\n", dwPC, dwExcAddr));
        }
#endif // DEBUG
    }    
    ::KLibGetLastExcpCallStack (MAX_CRASH_STACK_SIZE - 1, (CallSnapshot *) &(pcex->adwStack [1]));
}

    
#define INFINITE_NB_THREADS (0xFFFFFFFF)


void GetProcessAndThreadInfo (CRASH_PROC_INFO *pcpi, CRASH_THREAD_INFO *pcti)
{
    PROCESS *pProcess = (PROCESS *) UserKInfo [KINX_PROCARRAY];
    DWORD dwProcIdx = 0;
    DWORD dwThrdIdx = 0;
    while (dwProcIdx < MAX_PROCESSES)
    {
        if (pProcess->dwVMBase)
        {
            ::wcsncpy (pcpi->wszProcName, pProcess->lpszProcName, MAX_NAME_SIZE);
            pcpi->wszProcName [MAX_NAME_SIZE - 1] = 0;
            pcpi->dwVMBase = pProcess->dwVMBase;
            pcpi->dwBasePtr = (DWORD) pProcess->BasePtr;
            pcpi->dwPID = (DWORD) pProcess->hProc;
            
            // Calculate number of threads
            // Check also that link list is terminated
            THREAD *pThdLead = pProcess->pTh;
            THREAD *pThdTrail = pProcess->pTh;
            DWORD dwNbThrds = 0;
            while (pThdLead)
            {
                dwNbThrds++;
                pThdLead = pThdLead->pNextInProc;
                if (!(dwNbThrds & 1)) pThdTrail = pThdTrail->pNextInProc;
                if (pThdLead == pThdTrail)
                {
                    dwNbThrds = INFINITE_NB_THREADS;
                    break;
                }
            }

            pcpi->dwNbThreads = dwNbThrds;

            if (dwNbThrds != INFINITE_NB_THREADS)
            {
                THREAD *pTh = pProcess->pTh;
                while (pTh && dwNbThrds-- && (dwThrdIdx++ < MAX_CRASH_THREADS))
                {
                    pcti->dwTID = (DWORD) pTh->hTh;
                    pcti->bPrio = pTh->bCPrio;
                    pcti->bBasePrio = pTh->bBPrio;
                    pcti->dwOwningProc = (pTh->pOwnerProc ? (DWORD) (pTh->pOwnerProc->hProc) : 0);
                    pcti->dwCurrentProc = (pTh->pProc ? (DWORD) (pTh->pProc->hProc) : 0);
                    pcti->dwPC = GetThreadIP (pTh);
                    pcti->dwRA = CONTEXT_TO_RETURN_ADDRESS (&(pTh->ctx));
                    pcti->wInfo = pTh->wInfo;
                    pcti->dwAky = pTh->aky;
                    pcti++;
                    pTh = pTh->pNextInProc;
                }
            }
            pcpi++;
        }
        dwProcIdx++;
        pProcess++;
    }
}


void GetModuleInfo (CRASH_MODULE_INFO *pcmi)
{
    DWORD dwModIdx = 0;
    
    // EXE MODULES

    PROCESS *pProcess = (PROCESS *) UserKInfo [KINX_PROCARRAY];
    DWORD dwProcIdx = 0;
    while ((dwProcIdx < MAX_PROCESSES) && (dwModIdx < MAX_CRASH_MODULES))
    {
        if (pProcess->dwVMBase)
        {
            pcmi->fIsDll = FALSE;
            ::wcsncpy (pcmi->wszModName, pProcess->lpszProcName, MAX_NAME_SIZE);
            pcmi->wszModName [MAX_NAME_SIZE - 1] = 0;
            pcmi->dwMID = (DWORD) pProcess;
            if (pProcess->oe.filetype == FT_ROMIMAGE)
            {
                pcmi->dwLowDateTime = pProcess->oe.tocptr->ftTime.dwLowDateTime;
                pcmi->dwHighDateTime = pProcess->oe.tocptr->ftTime.dwHighDateTime; 
                pcmi->dwFSize = pProcess->oe.tocptr->nFileSize;
            }
            else if (pProcess->oe.filetype == FT_OBJSTORE)
            {
                FILETIME ftCreate, ftAcc, ftMod;
                ::GetFileTime (pProcess->oe.hf, &ftCreate, &ftAcc, &ftMod);
                pcmi->dwLowDateTime = ftCreate.dwLowDateTime;
                pcmi->dwHighDateTime = ftCreate.dwHighDateTime;
                pcmi->dwFSize = ::GetFileSize (pProcess->oe.hf, NULL);
            }
            pcmi->dwModStartAddr = (DWORD) pProcess->BasePtr;
            if (pcmi->dwModStartAddr < 0x01FFFFFF)
            {
                pcmi->dwModStartAddr |= pProcess->dwVMBase;
            }
            pcmi->dwModStopAddr = pcmi->dwModStartAddr + (DWORD) pProcess->e32.e32_vsize;
            if (!dwProcIdx)
            { // NK.EXE specific
                ROMHDR *pTOC = (ROMHDR *) UserKInfo [KINX_PTOC];
                pcmi->dwModRelocStartAddr = ((COPYentry *)(pTOC->ulCopyOffset))->ulDest;
                pcmi->dwModRelocStopAddr = pcmi->dwModRelocStartAddr + ((COPYentry *)(pTOC->ulCopyOffset))->ulDestLen - 1;
            }
            else
            {
                pcmi->dwModRelocStartAddr = 0xFFFFFFFF;
                pcmi->dwModRelocStopAddr = 0;
            }
            pcmi->dwModUsage = 0;
            dwModIdx++;
            pcmi++;
        }
        dwProcIdx++;
        pProcess++;
    }

    // DLL MODULES

    MODULE *pMod = (MODULE *) UserKInfo [KINX_MODULES];

    MODULE *pModTrail = pMod;
    DWORD dwNbDlls = 0;
    while (pMod && (pMod->lpSelf == pMod) && (dwModIdx < MAX_CRASH_MODULES))
    {
        {
            pcmi->fIsDll = TRUE;
            ::wcsncpy (pcmi->wszModName, pMod->lpszModName, MAX_NAME_SIZE);
            pcmi->wszModName [MAX_NAME_SIZE - 1] = 0;
            pcmi->dwMID = (DWORD) pMod;
            if (pMod->oe.filetype == FT_ROMIMAGE)
            {
                pcmi->dwLowDateTime = pMod->oe.tocptr->ftTime.dwLowDateTime;
                pcmi->dwHighDateTime = pMod->oe.tocptr->ftTime.dwHighDateTime; 
                pcmi->dwFSize = pMod->oe.tocptr->nFileSize;
            }
            else if (pMod->oe.filetype == FT_OBJSTORE)
            {
                FILETIME ftCreate, ftAcc, ftMod;
                ::GetFileTime (pMod->oe.hf, &ftCreate, &ftAcc, &ftMod);
                pcmi->dwLowDateTime = ftCreate.dwLowDateTime;
                pcmi->dwHighDateTime = ftCreate.dwHighDateTime;
                pcmi->dwFSize = ::GetFileSize (pMod->oe.hf, NULL);
            }
            pcmi->dwModStartAddr = (DWORD) pMod->BasePtr;
            pcmi->dwModStopAddr = (DWORD) pMod->BasePtr + (DWORD) pMod->e32.e32_vsize;
            pcmi->dwModRelocStartAddr = pMod->rwLow;
            pcmi->dwModRelocStopAddr = pMod->rwHigh;
            pcmi->dwModUsage = pMod->inuse;
        }
        
        dwModIdx++;
        dwNbDlls++;
        pMod = pMod->pMod;
        if (!(dwNbDlls & 1)) pModTrail = pModTrail->pMod;
        // Check also that link list is terminated
        if (pMod == pModTrail)
        { // Infinite number of modules: stop now
            break;
        }
        pcmi++;
    }
}


void GatherAllCrashData (const DEBUG_EVENT *pDebugEvent, CRASH_DATA *pcd)
{
    __try
    {
        GetSysInfo (&(pcd->sys));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGMSG (1, (L"UsrExceptDmp: Unexpected exception in GetSysInfo.\r\n"));
    }
    __try
    {
        GetModuleInfo (pcd->mods); // Module info must be generated before Exception info
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGMSG (1, (L"UsrExceptDmp: Unexpected exception in GetModuleInfo.\r\n"));
    }
    __try
    {
        GetExceptionInfo (pDebugEvent, &(pcd->excpt), pcd->mods);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGMSG (1, (L"UsrExceptDmp: Unexpected exception in GetExceptionInfo.\r\n"));
    }
    __try
    {
        GetProcessAndThreadInfo (pcd->procs, pcd->thds);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DEBUGMSG (1, (L"UsrExceptDmp: Unexpected exception in GetProcessAndThreadInfo.\r\n"));
    }
}


const char szErrorStringToLarge [] = "\r\nWARNING: String too large to be processed!!!!!\r\n";


BOOL WriteStringToDumpFile (HANDLE hDumpFile, const WCHAR *wsz)
{
    BOOL fRet = TRUE;
    DWORD dwBytesWritten;
    char sz [MAX_STRING_SIZE];
    if (::wcslen (wsz) > MAX_STRING_SIZE)
    {
        DEBUGMSG (1, (L"UsrExceptDmp : WARNING: String too large to be processed.\r\n"));
        ::WriteFile (hDumpFile, szErrorStringToLarge, sizeof (szErrorStringToLarge), &dwBytesWritten, NULL);
    }
    else
    {
        Unicode2AsciiMax (sz, wsz, MAX_STRING_SIZE);
        if (!::WriteFile (hDumpFile, sz, ::strlen (sz), &dwBytesWritten, NULL)) // Note: we don't write the termination zero byte
        {
            fRet = FALSE;
        }
    }
#if 0
    DEBUGMSG (1, (wsz));
#endif // 0
    return fRet;
}


void GetExceptionDescription (WCHAR wsc [MAX_STRING_SIZE], DWORD dwCode)
{
    switch (dwCode)
    {
        case EXCEPTION_ACCESS_VIOLATION:
            ::wcsncpy (wsc, L"Access Violation: The thread tried to read from or write to a virtual address for which it does not have the appropriate access.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_DATATYPE_MISALIGNMENT :
            ::wcsncpy (wsc, L"Datatype misalignment: The thread tried to read or write data that is misaligned on hardware that does not provide alignment. For example, 16-bit values must be aligned on 2-byte boundaries; 32-bit values on 4-byte boundaries, and so on.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_BREAKPOINT :
            ::wcsncpy (wsc, L"Breakpoint: A breakpoint was encountered.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_SINGLE_STEP :
            ::wcsncpy (wsc, L"Single Step: A trace trap or other single-instruction mechanism signaled that one instruction has been executed.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED :
            ::wcsncpy (wsc, L"Array Bounds Exceeded: The thread tried to access an array element that is out of bounds and the underlying hardware supports bounds checking.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_FLT_DENORMAL_OPERAND :
            ::wcsncpy (wsc, L"Float Denormal Operand: One of the operands in a floating-point operation is denormal. A denormal value is one that is too small to represent as a standard floating-point value.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO :
            ::wcsncpy (wsc, L"Float Divide By Zero: The thread tried to divide a floating-point value by a floating-point divisor of zero.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_FLT_INEXACT_RESULT :
            ::wcsncpy (wsc, L"Float Inexact Result: The result of a floating-point operation cannot be represented exactly as a decimal fraction.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_FLT_INVALID_OPERATION :
            ::wcsncpy (wsc, L"Float Invalid Operation: This exception represents any floating-point exception not included in this list.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_FLT_OVERFLOW :
            ::wcsncpy (wsc, L"Float Overflow: The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_FLT_STACK_CHECK :
            ::wcsncpy (wsc, L"Float Stack Check: The stack overflowed or underflowed as the result of a floating-point operation.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_FLT_UNDERFLOW :
            ::wcsncpy (wsc, L"Float Overflow: The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_INT_DIVIDE_BY_ZERO :
            ::wcsncpy (wsc, L"Integer Divide By Zero: The thread tried to divide an integer value by an integer divisor of zero.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_INT_OVERFLOW :
            ::wcsncpy (wsc, L"Integer Overflow: The result of an integer operation caused a carry out of the most significant bit of the result.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_PRIV_INSTRUCTION :
            ::wcsncpy (wsc, L"Private Instruction: The thread tried to execute an instruction whose operation is not allowed in the current machine mode.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_IN_PAGE_ERROR :
            ::wcsncpy (wsc, L"In Page Error: The thread tried to access a page that was not present, and the system was unable to load the page. For example, this exception might occur if a network connection is lost while running a program over the network.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_ILLEGAL_INSTRUCTION :
            ::wcsncpy (wsc, L"Illegal Instruction: The thread tried to execute an invalid instruction.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_NONCONTINUABLE_EXCEPTION :
            ::wcsncpy (wsc, L"Noncontinuable Exception: The thread tried to continue execution after a noncontinuable exception occurred.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_STACK_OVERFLOW :
            ::wcsncpy (wsc, L"Stack Overflow: The thread used up its stack.", MAX_STRING_SIZE);
            break ;
        case EXCEPTION_INVALID_DISPOSITION :
            ::wcsncpy (wsc, L"Invalid Disposition: An exception handler returned an invalid disposition to the exception dispatcher. Programmers using a high-level language such as C should never encounter this exception.", MAX_STRING_SIZE);
            break ;
        default : 
            ::wcsncpy (wsc, L"Unknow Exception Code.", MAX_STRING_SIZE);
    }
    wsc [MAX_STRING_SIZE - 1] = 0;
}


#define TAG_CE_MICRODUMP L"CE_MICRODUMPv1"
#define TAG_SYSINFO L"SYSINFO"
#define TAG_CPU L"CPU"
#define TAG_ARCH L"ARCH"
#define TAG_TYPE L"TYPE"
#define TAG_LEVEL L"LEVEL"
#define TAG_REV L"REV"
#define TAG_OSVER L"OSVER"
#define TAG_OSBUILD L"OSBUILD"
#define TAG_DEVID L"DEVID"
#define TAG_EXCEPT L"EXCEPT"
#define TAG_CONTEXT L"CONTEXT"
#define TAG_EXCODE L"EXCODE"
#define TAG_EXDESC L"EXDESC"
#define TAG_PID L"PID"
#define TAG_TID L"TID"
#define TAG_MID L"MID"
#define TAG_MODNAME L"MODNAME"
#define TAG_PROCNAME L"PROCNAME"
#define TAG_VMBASE L"VMBASE"
#define TAG_STACK L"STACK"
#define TAG_MEMDUMP L"MEMDUMP"
#define TAG_PC_P64M64 L"PC_P64M64"
#define TAG_SP_P0M2K L"SP_P0M2K"
#define TAG_PROCLIST L"PROCLIST"
#define TAG_PROC L"PROC"
#define COMMENT_PROCLIST L"!-- NAME - VMBASE - PID - NBTHREADS --"
#define TAG_THRDLIST L"THRDLIST"
#define COMMENT_THRDLIST L"!-- TID - PRIO - BASEPRIO - OWNPROC - CURPROC - PC - RA - STATEINFO - AKY --"
#define TAG_MODLIST L"MODLIST"
#define COMMENT_MODLIST L"!-- NAME - MID - TIME - FILESIZE - FIXED ADDR RG - RELOC DATA ADDR RG - PROCUSE --"
#define TAG_CECONFIG L"CECONFIG"

#define DWORD_DISP_GROUP_SIZE (4)

#define WSDF(x) {if (!WriteStringToDumpFile (hDumpFile, (x))) goto ErrExit;}
#define WSDFREG(x) {WSDF (L"\t\t<" L#x L">"); ::swprintf (wsTemp, L"%08X", pcd->excpt.ctx.x); WSDF (wsTemp); WSDF (L"</" L#x L">\r\n");}

#define COPY_BUFF_SIZE (1024)
BYTE g_abBuf [COPY_BUFF_SIZE];
const WCHAR szCeConfigHFName [] = L"\\windows\\ceconfig.h";


BOOL WriteCrashDataToDumpFile (HANDLE hDumpFile, const CRASH_DATA *pcd)
{
    BOOL fRet = FALSE;
    WCHAR wsTemp [MAX_STRING_SIZE];
    int idx = 0;
    WSDF (L"<" TAG_CE_MICRODUMP L">\r\n\r\n");
    {
        WSDF (L"<" TAG_SYSINFO L">\r\n");
        {
            WSDF (L"\t<" TAG_CPU L">\r\n");
            {
                SYSTEM_INFO sysi;
                ::GetSystemInfo (&sysi);
                    
                WSDF (L"\t\t<" TAG_ARCH L">");
                {
                    switch (sysi.wProcessorArchitecture)
                    {
                        case PROCESSOR_ARCHITECTURE_ARM:
                            WSDF (L"ARM");
                            break;
                        case PROCESSOR_ARCHITECTURE_SHX:
                            WSDF (L"SuperH");
                            break;
                        case PROCESSOR_ARCHITECTURE_MIPS:
                            WSDF (L"MIPS");
                            break;
                        case PROCESSOR_ARCHITECTURE_INTEL:
                            WSDF (L"IA32");
                            break;
                        default:
                            WSDF (L"???");
                            break;
                    }
                }
                WSDF (L"</" TAG_ARCH L">\r\n");
                WSDF (L"\t\t<" TAG_TYPE L">");
                {
                    ::swprintf (wsTemp, L"%i", sysi.dwProcessorType);
                    WSDF (wsTemp);
                }
                WSDF (L"</" TAG_TYPE L">\r\n");
                WSDF (L"\t\t<" TAG_LEVEL L">");
                {
                    ::swprintf (wsTemp, L"%i", sysi.wProcessorLevel);
                    WSDF (wsTemp);
                }
                WSDF (L"</" TAG_LEVEL L">\r\n");
                WSDF (L"\t\t<" TAG_REV L">");
                {
                    ::swprintf (wsTemp, L"%i", sysi.wProcessorRevision);
                    WSDF (wsTemp);
                }
                WSDF (L"</" TAG_REV L">\r\n");
            }
            WSDF (L"\t</" TAG_CPU L">\r\n");
            WSDF (L"\t<" TAG_OSVER L">");
            {
                ::swprintf (wsTemp, L"%i", pcd->sys.dwOsVer);
                WSDF (wsTemp);
            }
            WSDF (L"</" TAG_OSVER L">\r\n");
            WSDF (L"\t<" TAG_OSBUILD L">");
            {
                OSVERSIONINFO osvi;
                ::GetVersionEx (&osvi);
                ::swprintf (wsTemp, L"%i", osvi.dwBuildNumber);
                WSDF (wsTemp);
            }
            WSDF (L"</" TAG_OSBUILD L">\r\n");
            WSDF (L"\t<" TAG_DEVID L">");
            {
                DWORD cbRead =  MAX_STRING_SIZE;
                KernelIoControl (IOCTL_HAL_GET_DEVICEID, NULL, 0, wsTemp,  MAX_STRING_SIZE, &cbRead);
                WSDF (wsTemp);
            }
            WSDF (L"</" TAG_DEVID L">\r\n");
        }
        WSDF (L"</" TAG_SYSINFO L">\r\n\r\n");
        WSDF (L"<" TAG_EXCEPT L">\r\n");
        {
            WSDF (L"\t<" TAG_CONTEXT L">\r\n");
            {
#ifdef ARM
                WSDFREG (Pc);
                WSDFREG (Sp);
                WSDFREG (Psr);
                WSDFREG (Lr);
                WSDFREG (R0);
                WSDFREG (R1);
                WSDFREG (R2);
                WSDFREG (R3);
                WSDFREG (R4);
                WSDFREG (R5);
                WSDFREG (R6);
                WSDFREG (R7);
                WSDFREG (R8);
                WSDFREG (R9);
                WSDFREG (R10);
                WSDFREG (R11);
                WSDFREG (R12);
#endif  // ARM
#ifdef x86
                WSDFREG (Eip);
                WSDFREG (Esp);
                WSDFREG (EFlags);
                WSDFREG (Ebp);
                WSDFREG (Eax);
                WSDFREG (Ebx);
                WSDFREG (Ecx);
                WSDFREG (Edx);
                WSDFREG (Esi);
                WSDFREG (Edi);
                WSDFREG (SegCs);
                WSDFREG (SegSs);
                WSDFREG (SegDs);
                WSDFREG (SegEs);
                WSDFREG (SegFs);
                WSDFREG (SegGs);
#endif  // x86
#ifdef SHx
                WSDFREG (Fir);
                WSDFREG (Psr);
                WSDFREG (PR);
                WSDFREG (MACH);
                WSDFREG (MACL);
                WSDFREG (GBR);
                WSDFREG (R0);
                WSDFREG (R1);
                WSDFREG (R2);
                WSDFREG (R3);
                WSDFREG (R4);
                WSDFREG (R5);
                WSDFREG (R6);
                WSDFREG (R7);
                WSDFREG (R8);
                WSDFREG (R9);
                WSDFREG (R10);
                WSDFREG (R11);
                WSDFREG (R12);
                WSDFREG (R13);
                WSDFREG (R14);
                WSDFREG (R15);
#endif  // SHx
#ifdef MIPS
                WSDFREG (Fir);
                WSDFREG (IntSp);
                WSDFREG (IntRa);
                WSDFREG (Psr);
                WSDFREG (Fsr);
                WSDFREG (IntLo);
                WSDFREG (IntHi);
                WSDFREG (IntAt);
                WSDFREG (IntV0);
                WSDFREG (IntV1);
                WSDFREG (IntA0);
                WSDFREG (IntA1);
                WSDFREG (IntA2);
                WSDFREG (IntA3);
                WSDFREG (IntT0);
                WSDFREG (IntT1);
                WSDFREG (IntT2);
                WSDFREG (IntT3);
                WSDFREG (IntT4);
                WSDFREG (IntT5);
                WSDFREG (IntT6);
                WSDFREG (IntT7);
                WSDFREG (IntS0);
                WSDFREG (IntS1);
                WSDFREG (IntS2);
                WSDFREG (IntS3);
                WSDFREG (IntS4);
                WSDFREG (IntS5);
                WSDFREG (IntS6);
                WSDFREG (IntS7);
                WSDFREG (IntT8);
                WSDFREG (IntT9);
                WSDFREG (IntK0);
                WSDFREG (IntK1);
                WSDFREG (IntGp);
                WSDFREG (IntS8);
#endif  // MIPS
            }
            WSDF (L"\t</" TAG_CONTEXT L">\r\n");
            WSDF (L"\t<" TAG_EXCODE L">");
            {
                ::swprintf (wsTemp, L"%08X", pcd->excpt.dwCode);
                WSDF (wsTemp);
            }
            WSDF (L"</" TAG_EXCODE L">\r\n");
            WSDF (L"\t<" TAG_EXDESC L">");
            {
                GetExceptionDescription (wsTemp, pcd->excpt.dwCode);
                WSDF (wsTemp);
            }
            WSDF (L"</" TAG_EXDESC L">\r\n");
            WSDF (L"\t<" TAG_PID L">");
            {
                ::swprintf (wsTemp, L"%08X", pcd->excpt.dwPID);
                WSDF (wsTemp);
            }
            WSDF (L"</" TAG_PID L">\r\n");
            WSDF (L"\t<" TAG_TID L">");
            {
                ::swprintf (wsTemp, L"%08X", pcd->excpt.dwTID);
                WSDF (wsTemp);
            }
            WSDF (L"</" TAG_TID L">\r\n");
            WSDF (L"\t<" TAG_MID L">");
            {
                ::swprintf (wsTemp, L"%08X", pcd->excpt.dwMID);
                WSDF (wsTemp);
            }
            WSDF (L"</" TAG_MID L">\r\n");
            WSDF (L"\t<" TAG_MODNAME L">");
            {
                WSDF (pcd->excpt.wszModName);
            }
            WSDF (L"</" TAG_MODNAME L">\r\n");
            WSDF (L"\t<" TAG_PROCNAME L">");
            {
                WSDF (pcd->excpt.wszProcName);
            }
            WSDF (L"</" TAG_PROCNAME L">\r\n");
            WSDF (L"\t<" TAG_VMBASE L">");
            {
                ::swprintf (wsTemp, L"%08X", pcd->excpt.dwVMBase);
                WSDF (wsTemp);
            }
            WSDF (L"</" TAG_VMBASE L">\r\n");
            WSDF (L"\t<" TAG_STACK L">\r\n");
            {
                for (idx = 0; (idx < MAX_CRASH_STACK_SIZE) && (pcd->excpt.adwStack [idx]); idx++)
                {
                    ::swprintf (wsTemp, L"\t\t%08X,\r\n", pcd->excpt.adwStack [idx]);
                    WSDF (wsTemp);
                }
            }
            WSDF (L"\t</" TAG_STACK L">\r\n");
            WSDF (L"\t<" TAG_MEMDUMP L">\r\n");
            {
                WSDF (L"\t\t<" TAG_PC_P64M64 L">\r\n");
                {
                    for (idx = 0; idx < ((STOP_MEM_AROUND_PC - START_MEM_AROUND_PC) / sizeof (DWORD)); idx++)
                    {
                        if (!(idx % DWORD_DISP_GROUP_SIZE))
                        {
                            WSDF (L"\t\t\t");
                        }
                        ::swprintf (wsTemp, L"%08X,", pcd->excpt.adwMemAroundPc [idx]);
                        WSDF (wsTemp);
                        if ((idx % DWORD_DISP_GROUP_SIZE) == (DWORD_DISP_GROUP_SIZE - 1))
                        {
                            WSDF (L"\r\n");
                        }
                        else
                        {
                            WSDF (L" ");
                        }
                    }
                }
                WSDF (L"\t\t</" TAG_PC_P64M64 L">\r\n");
                WSDF (L"\t\t<" TAG_SP_P0M2K L">\r\n");
                {
                    for (idx = 0; idx < ((STOP_MEM_AROUND_SP - START_MEM_AROUND_SP) / sizeof (DWORD)); idx++)
                    {
                        if (!(idx % DWORD_DISP_GROUP_SIZE))
                        {
                            WSDF (L"\t\t\t");
                        }
                        ::swprintf (wsTemp, L"%08X,", pcd->excpt.adwMemAroundSp [idx]);
                        WSDF (wsTemp);
                        if ((idx % DWORD_DISP_GROUP_SIZE) == (DWORD_DISP_GROUP_SIZE - 1))
                        {
                            WSDF (L"\r\n");
                        }
                        else
                        {
                            WSDF (L" ");
                        }
                    }
                }
                WSDF (L"\t\t</" TAG_SP_P0M2K L">\r\n");
            }
            WSDF (L"\t</" TAG_MEMDUMP L">\r\n");
        }
        WSDF (L"</" TAG_EXCEPT L">\r\n\r\n");
        WSDF (L"<" TAG_PROCLIST L"> <" COMMENT_PROCLIST L">\r\n");
        {
            const CRASH_PROC_INFO *pcproc = pcd->procs;
#ifdef THREADLIST_NESTED_IN_PROCLIST
            const CRASH_THREAD_INFO *pcthd = pcd->thds;
            int ThdIdx = 0;
#endif // THREADLIST_NESTED_IN_PROCLIST
            for (idx = 0; idx < MAX_PROCESSES; idx++, pcproc++)
            {
                if (pcproc->dwVMBase)
                {
                    WSDF (L"\t<" TAG_PROC L">\r\n");
                    {
                        ::swprintf (wsTemp, L"\t%s,\t%08X,\t%08X,\t%i,\r\n", pcproc->wszProcName, pcproc->dwVMBase, /*pcproc->dwBasePtr,*/ pcproc->dwPID, pcproc->dwNbThreads);
                        WSDF (wsTemp);
#ifdef THREADLIST_NESTED_IN_PROCLIST
                        WSDF (L"\t\t<" TAG_THRDLIST L">");
                        if (!ThdIdx)
                        { // display comments only once
                            WSDF (L" <" COMMENT_THRDLIST L">\r\n");
                        }
                        else
                        { // display comments only once
                            WSDF (L"\r\n");
                        }
                        {
                            DWORD dwNbThdInProc = pcproc->dwNbThreads;
                            for (; dwNbThdInProc && (dwNbThdInProc < INFINITE_NB_THREADS) && (ThdIdx < MAX_CRASH_THREADS) && (pcthd->dwTID); ThdIdx++, dwNbThdInProc--, pcthd++)
                            {
                                ::swprintf (wsTemp, L"\t\t\t%08X,\t%i,\t%i,\t%08X,\t%08X,\t%08X,\t%08X,\t%04X,\t%08X,\r\n", pcthd->dwTID, pcthd->bPrio, pcthd->bBasePrio, pcthd->dwOwningProc, pcthd->dwCurrentProc, pcthd->dwPC, pcthd->dwRA, pcthd->wInfo, pcthd->dwAky);
                                WSDF (wsTemp);
                            }
                        }
                        WSDF (L"\t\t</" TAG_THRDLIST L">\r\n\r\n");
#endif // THREADLIST_NESTED_IN_PROCLIST
                    }
                    WSDF (L"\t</" TAG_PROC L">\r\n");
                }
            }
        }
        WSDF (L"</" TAG_PROCLIST L">\r\n\r\n");
        
#ifndef THREADLIST_NESTED_IN_PROCLIST
        WSDF (L"<" TAG_THRDLIST L"> <" COMMENT_THRDLIST L">\r\n");
        {
            const CRASH_THREAD_INFO *pcthd = pcd->thds;
            for (idx = 0; idx < MAX_CRASH_THREADS; idx++, pcthd++)
            {
                if (pcthd->dwTID)
                {
                    ::swprintf (wsTemp, L"\t%08X,\t%i,\t%i,\t%08X,\t%08X,\t%08X,\t%08X,\t%04X,\t%08X,\r\n", pcthd->dwTID, pcthd->bPrio, pcthd->bBasePrio, pcthd->dwOwningProc, pcthd->dwCurrentProc, pcthd->dwPC, pcthd->dwRA, pcthd->wInfo, pcthd->dwAky);
                    WSDF (wsTemp);
                }
            }
        }
        WSDF (L"</" TAG_THRDLIST L">\r\n\r\n");
#endif // !THREADLIST_NESTED_IN_PROCLIST

        WSDF (L"<" TAG_MODLIST L"> <" COMMENT_MODLIST L">\r\n");
        {
            const CRASH_MODULE_INFO *pcmod = pcd->mods;
            for (idx = 0; idx < MAX_CRASH_MODULES; idx++, pcmod++)
            {
                if (pcmod->dwModStartAddr)
                {
                    ::swprintf (wsTemp, L"\t%s,\t%08X,\t%08X%08X,\t%08X,\t%08X,\t%08X,\t", pcmod->wszModName, pcmod->dwMID, pcmod->dwHighDateTime, pcmod->dwLowDateTime, pcmod->dwFSize, pcmod->dwModStartAddr, pcmod->dwModStopAddr);
                    WSDF (wsTemp);
                    if (pcmod->dwModRelocStartAddr < pcmod->dwModRelocStopAddr)
                    {
                        ::swprintf (wsTemp, L"%08X,\t%08X,\t", pcmod->dwModRelocStartAddr, pcmod->dwModRelocStopAddr);
                        WSDF (wsTemp);
                    }
                    else
                    {
                        WSDF (L"n/a\tn/a\t");
                    }
                    if (pcmod->fIsDll)
                    {
                        ::swprintf (wsTemp, L"%08X,\r\n", pcmod->dwModUsage);
                        WSDF (wsTemp);
                    }
                    else
                    {
                        WSDF (L"n/a,\r\n");
                    }
                }
            }
        }
        WSDF (L"</" TAG_MODLIST L">\r\n\r\n");
        WSDF (L"<" TAG_CECONFIG L">\r\n");
        {
            HANDLE hFile = ::CreateFile (szCeConfigHFName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (INVALID_HANDLE_VALUE != hFile)
            {
                DWORD dwCeConfSize = ::GetFileSize (hFile, NULL);
                DWORD dwBytesRead;
                DWORD dwBytesWritten;
                while (::ReadFile (hFile, g_abBuf, MinDW (dwCeConfSize, COPY_BUFF_SIZE), &dwBytesRead, NULL) && dwBytesRead)
                {
                    ::WriteFile (hDumpFile, g_abBuf, dwBytesRead, &dwBytesWritten, NULL);
                    dwCeConfSize -= dwBytesRead;                
                }
            }
        }
        WSDF (L"</" TAG_CECONFIG L">\r\n\r\n");        
    }   
    WSDF (L"</" TAG_CE_MICRODUMP L">\r\n");
    
    fRet = TRUE;
ErrExit:
    return fRet;
}


CRASH_DATA g_cd = {0};

BOOL GenerateDumpFileContent (HANDLE hDumpFile, const DEBUG_EVENT *pDebugEvent)
{
    BOOL fRet = FALSE;
    
    // 1st gather all data atomically
    HANDLE hCurThd = ::GetCurrentThread (); 
    DWORD dwOldQuantum = ::CeGetThreadQuantum (hCurThd);
    int iOldPrio = ::CeGetThreadPriority (hCurThd);
    ::CeSetThreadQuantum (hCurThd, 0);
    ::CeSetThreadPriority (hCurThd, 0); // Now real-time
    __try
    {
        GatherAllCrashData (pDebugEvent, &g_cd);
    }    
    __finally
    {
        ::CeSetThreadPriority (hCurThd, iOldPrio);
        ::CeSetThreadQuantum (hCurThd, dwOldQuantum); // back to normal scheduling      
    }
    fRet = WriteCrashDataToDumpFile (hDumpFile, &g_cd);
    
    return fRet ;
}

 
