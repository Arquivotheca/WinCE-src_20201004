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
//
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    apicall.h - internal kernel thread header
//

#ifndef _NK_APICALL_H_
#define _NK_APICALL_H_
/* Thread Call stack structure
 *
 *  This structure is used by the IPC mechanism to track
 * current process, access key, and return addresses for
 * IPC calls which are in progress. It is also used by
 * the exception handling code to hold critical thread
 * state while switching modes.
 */

#ifdef MIPSIV
#define REGSIZE     8
#else
#define REGSIZE     4
#endif

#define     MAX_NUM_PSL_ARG         14                              // max number of PSL arguments
#define     MAX_NUM_OUT_PTR_ARGS    6                               // max number of out pointer arguments
#define     MAX_SIZE_PSL_ARGS       (MAX_NUM_PSL_ARG * REG_SIZE)    // max space for PSL arguments

// bit fields in dwPrcInfo
#define     CST_MODE_FROM_USER      0x0001      // indicate if we were calling from KMode (0 == kMode, 1 == umode)
#define     CST_CALLBACK            0x0002      // indicate if this is a callback
#define     CST_UMODE_SERVER        0x0004      // indicate that we're in user-mode server
#define     CST_DIRECTCALL          0x0008      // indicate if we were in "direct call"
#define     CST_EXCEPTION           0x0010      // indicate if this callstack is a "fake" callstack constructed on exception

#if defined(_M_ARM)
#define     SIZE_CRTSTORAGE         68
#else
#define     SIZE_CRTSTORAGE         64
#endif

#ifndef ASM_ONLY
#include <corecrtstorage.h>
ERRFALSE (sizeof (crtGlob_t) <= SIZE_CRTSTORAGE);

#define CRTGlobFromTls(tlsPtr)      ((crtGlob_t *) ((DWORD) tlsPtr - SECURESTK_RESERVE + 4 * REGSIZE))
#endif

#if defined(x86)
// enough room to hold NK_PCR (can't use sizeof because it's used in assembly)
#define     SECURESTK_RESERVE       (SIZE_OF_FXSAVE_AREA + 4 * REGSIZE + SIZE_CRTSTORAGE + (PRETLS_RESERVED+3)*4)
#define     APIArgs(pcstk)          ((REGTYPE*) ((pcstk)->dwPrevSP + REGSIZE))

ERRFALSE (sizeof(NK_PCR) <= (TLS_MINIMUM_AVAILABLE * 4 + SECURESTK_RESERVE));

#else

#ifdef ASM_ONLY
// PRETLS_RESERVE is 32
#define     SECURESTK_RESERVE       (32 + SIZE_CRTSTORAGE + 4 * REGSIZE)              // +4 register save area for calling convention
#else
ERRFALSE(8 == PRETLS_RESERVED);
#define     SECURESTK_RESERVE       (SIZE_PRETLS + SIZE_CRTSTORAGE + 4 * REGSIZE)     // +4 register save area for calling convention
#endif

#endif

#ifndef ASM_ONLY

#ifdef MIPSIV
typedef ULONGLONG   REGTYPE;
#else
typedef ULONG       REGTYPE;
#endif

//
// Out pointer argument entry for user mode server - always copy-in/copy-out.
//
typedef struct {
    LPVOID ptr;             // ptr argument
    DWORD  cbSize;          // size of the ptr.
} ARGENTRY, *PARGENTRY;

typedef struct _ARGINFO {
    ARGENTRY    argent[MAX_NUM_OUT_PTR_ARGS];
    int         nArgs;
    WORD        wStkSizeInBlocks;
    WORD        wStkBaseInBlocks;
} ARGINFO, *PARGINFO;

typedef const ARGINFO *PCARGINFO;

struct _CALLSTACK {
    union {
        REGTYPE args[MAX_NUM_PSL_ARG];      // 0x0: arguments
        ARGINFO arginfo;                    // 0x0: argument information for user PSL servers
    };
    PCALLSTACK  pcstkNext;                  // 0x38: (MIPIIV: 0x70) link to next callstack 
    RETADDR     retAddr;                    // 0x3C: (MIPSIV: 0x74) return address 
    DWORD       dwPrevSP;                   // 0x40: (MIPSIV: 0x78) SP of caller 
    DWORD       dwPrcInfo;                  // 0x44: (MIPSIV: 0x7C) information about the caller (mode, callback?, etc) 
    PPROCESS    pprcLast;                   // 0x48: (MIPSIV: 0x80) previous process -- NULL if not an API call 
    PPROCESS    pprcVM;                     // 0x4C: (MIPSIV: 0x84) which process's VM is active 
    PHDATA      phd;                        // 0x50: (MIPSIV: 0x88) locked handle, need to unlock upon return
    LPDWORD     pOldTls;                    // 0x54: (MIPSIV: 0x8C) TLS PTR on API call 
    union {
        DWORD   iMethod;                    // 0x58: (MIPSIV: 0x90) which API to call (overloaded with user mode api set)
        PHDATA  phdApiSet;                    
    };
    DWORD       dwNewSP;                    // 0x5C: (MIPSIV: 0x94) (u-mode server) new SP when called into u-mode server
    REGTYPE     regs[CALLEE_SAVED_REGS];    // 0x60: (MIPSIV: 0x98) callee save registers
#ifdef ARM
    REGTYPE     _r0_r3[4];                  // ??: ARM specific - need to reserve 16 bytes more to avoid callstack overlap
                                            // NOTE: NEVER WRITE TO THIS AREA FOR IT CAN OVERLAP WITH OTHER CALLSTACK
#endif
};

typedef struct _DHCALL_STRUCT {
    REGTYPE dwAPI;            // the index into function table
    REGTYPE dwErrRtn;         // return value in case of error 
    REGTYPE cArgs;            // # of arguments, excluding the handle    
    REGTYPE args[1];          // variable length, the argument list, including the handle
} DHCALL_STRUCT, *PDHCALL_STRUCT;


struct _CINFO {
    char            acName[4];       /* 00: object type ID string */
    uchar           disp;            /* 04: type of dispatch */
    uchar           type;            /* 05: api handle type */
    ushort          cMethods;        /* 06: # of methods in dispatch table */
    const PFNVOID   *ppfnExtMethods; /* 08: ptr to array of methods (for external interface) */
    const PFNVOID   *ppfnIntMethods; /* 0C: ptr to array of methods (for internal interface) */
    const ULONGLONG *pu64Sig;        /* 10: ptr to array of method signatures */
    DWORD           dwServerId;      /* 14: server process id */
    PHDATA          phdApiSet;       /* 18: HDATA of API set */
    PFNAPIERRHANDLER pfnErrorHandler; /* 1C: ptr to the API set error handler (could be NULL) */
    const DWORD     *lprgAccessMask; /* 20: ptr to the access mask associated with every API in this API set; valid only for handle based API sets */
};

struct _APISET {
    CINFO   cinfo;      /* description of the API set */
    int     iReg;       /* registered API set index (-1 if not registered) */
};

typedef struct _APICALLINFO{
    DWORD        dwTotalSize;
    DWORD        dwMethod;
    PCCINFO      pci;
    int          nArgs;
    ULONGLONG    u64Sig;
    REGTYPE      *pArgs;
    DWORD        ArgSize[MAX_NUM_PSL_ARG];
    PFNVOID      pfn;
}APICALLINFO, *PAPICALLINFO;

// list of all PSL servers
struct _PSLSERVER
{
    struct _PSLSERVER* pNext;
    DWORD ProcessId;
};


/** Macros for retrieving and walking the PSL client argument list  */
/*  (NB. Currently only used in ...nk\kernel\ObjDisp.c::ObjectCall) */
#if defined (MIPSIV)
  // *** NOTE: Processors for which "sizeof(int)" is smaller than the 
  // ***       argument size should be added to this list.
  #define INT_ARG_SIZE    sizeof(__int64)
#else
  #define INT_ARG_SIZE    sizeof(int)
#endif

#if defined (MIPSIV)
#define WriteArg(ap, type, value) (*(__int64 *)(ap)) = (__int64)(value)
#else
#define WriteArg(ap, type, value) (*(type *)(ap)) = (value)
#endif
#define roundedsizeof(t)    ((sizeof(t)+INT_ARG_SIZE-1) & ~(INT_ARG_SIZE-1))

#define Arg(ap, type)       (*(type *)(ap))
#define NextArg(ap, type)   ((ap) = (char *)(ap) + roundedsizeof(type))


#define DISPATCH_KERNEL     0   /* dispatch directly in kernel */
#define DISPATCH_I_KERNEL   1   /* dispatch implicit handle in kernel */
#define DISPATCH_KERNEL_PSL 2   /* dispatch as thread in kernel */
#define DISPATCH_I_KPSL     3   /* implicit handle as kernel thread */
#define DISPATCH_PSL        4   /* dispatch as user mode PSL */
#define DISPATCH_I_PSL      5   /* implicit handle as user mode PSL */


//
// the kernel APIsets
//
extern const CINFO cinfAPISet;
extern const CINFO cinfEvent;
extern const CINFO cinfMap;
extern const CINFO cinfMutex;
extern const CINFO cinfCRIT;
extern const CINFO cinfProc;
extern const CINFO cinfThread;
extern const CINFO cinfToken;
extern const CINFO cinfWin32;
extern const CINFO cinfSem;
extern const CINFO *SystemAPISets[NUM_API_SETS];

//
// Direct Call related macros
//
#define IsInDirectCall()            (pCurThread->tlsSecure[TLSSLOT_KERNEL] & TLSKERN_DIRECTCALL)
#define SetDirectCall()             (pCurThread->tlsSecure[TLSSLOT_KERNEL] |= TLSKERN_DIRECTCALL)
#define ClearDirectCall()           (pCurThread->tlsSecure[TLSSLOT_KERNEL] &= ~TLSKERN_DIRECTCALL)

//
// API registration event
//
extern PHDATA g_phdAPIRegEvt;

//
// trap address to get to NKExitThread
//
#define TrapAddrExitThread          PRIV_IMPLICIT_CALL(SH_WIN32, W32_NKExitThread)

//------------------------------------------------------------------------------
//
// function prototypes
//

// initialization function
void APICallInit (void);

// NKCreateAPISet - create an API set (internal interface)
HANDLE NKCreateAPISet (
    char acName[4],                 /* 'class name' for debugging & QueryAPISetID() */
    USHORT cFunctions,              /* # of functions in set */
    const PFNVOID *ppfnMethods,     /* array of API functions */
    const ULONGLONG *pu64Sig        /* array of signatures for API functions */
    );

// NKCreateAPISet - create an API set (external interface)
HANDLE EXTCreateAPISet (
    char acName[4],                 /* 'class name' for debugging & QueryAPISetID() */
    USHORT cFunctions,              /* # of functions in set */
    const PFNVOID *ppfnMethods,     /* array of API functions */
    const ULONGLONG *pu64Sig        /* array of signatures for API functions */
    );

// APISClose - delete an API Set
void APISClose (PAPISET pas);

// APISPreClose - handle server closed the handle to the API set
void APISPreClose (PAPISET pas);

// APISRegister - register an API set
BOOL APISRegister (PAPISET pas, DWORD dwSetID);

// APISCreateAPIHandle - create a handle of a particular API set
HANDLE APISCreateAPIHandle (PAPISET pas, LPVOID pvData);

// APISCreateAPIHandleWithAccess - create a handle of a particular API set with a given access mask in the target process
HANDLE APISCreateAPIHandleWithAccess (PAPISET pas, LPVOID pvData, DWORD dwAccessMask, HANDLE hTargetProcess);

// APISLockAPIHandle - lock an handle of a particular API set
LPVOID APISVerify (PAPISET pas, HANDLE h);

// APISQueryID - given a name, query the API set ID
BOOL APISQueryID (const char *pName, int* lpRetVal);

// APISSetErrorHandler - register a PSL error handler
BOOL APISSetErrorHandler (PAPISET pas, PFNAPIERRHANDLER pfnHandler);

// CeRegisterAccessMask - register access mask for handle based APIs
BOOL APISRegisterAccessMask (PAPISET pas, const DWORD *lprgAccessMask, DWORD cAccessMask);
BOOL EXTAPISRegisterAccessMask (PAPISET pas, const DWORD *lprgAccessMask, DWORD cAccessMask);

// register a direct call function table 
BOOL APISRegisterDirectMethods (PAPISET pas, const PFNVOID *ppfnDirectMethods);
BOOL EXTAPISRegisterDirectMethods (PAPISET pas, const PFNVOID *ppfnDirectMethods);

// NKPerformCallBack - direct call to perform callback to user/kernel code
DWORD NKPerformCallBack (PCALLBACKINFO pcbi, ...);

// API server waiting for caller leaving the process
void WaitForCallerLeave (PPROCESS pprc);

//
// NKWaitForAPIReady - waiting for an API set to be ready
//
DWORD NKWaitForAPIReady (DWORD dwAPISet, DWORD dwTimeout);

// trapped API interface
void ServerCallReturn (void);
RETADDR ObjectCall (PCALLSTACK pcstk);
RETADDR NKPrepareCallback (PCALLSTACK pcstk);
void UnwindOneCstk (PCALLSTACK pcstk, BOOL fCleanOnly);
void CleanupAllCallstacks (PTHREAD pth);

// direct call to handle-based API
DWORD NKHandleCall (
        DWORD dwHtype,          // the expected handle type
        PDHCALL_STRUCT phc      // information of the api to call
        );

// Mark whether a direct-call is being made, used for non handle-based APIs
BOOL NKSetDirectCall (
    BOOL fNewValue
    );

RETADDR SetupCallToUserServer (PHDATA phd, PDHCALL_STRUCT phc, PCALLSTACK pcstk);
DWORD MDCallKernelHAPI (FARPROC pfnAPI, DWORD cArgs, LPVOID phObj, REGTYPE *pArgs);
DWORD MDCallUserHAPI (PHDATA phd, PDHCALL_STRUCT phc);

BOOL IsSystemAPISetReady(DWORD dwAPISet);

#endif  // ASM_ONLY

#endif  // _NK_APICALL_H_
