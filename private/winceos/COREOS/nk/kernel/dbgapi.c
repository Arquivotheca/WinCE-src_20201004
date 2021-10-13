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
//    dbgapi.c - implementations of debug apis
//
#include <windows.h>
#include <kernel.h>
#include <msgqueue.h>

NAME *pDebugger;

// thread debug flags; set on debugger thread
#define TDBG_DISPATCHED     0x1
#define TDBG_KILLONEXIT     0x2

// process debug flags; used to mark sending 
// process create event
#define PDBG_READY          0x1

//------------------------------------------------------------------------------
// Lock the handle in the given process or kernel process handle table.
//------------------------------------------------------------------------------
static PHDATA LockLocalOrKernelHandle (HANDLE h, PPROCESS pprc)
{
    // handle has last two bits set whereas process/thread id has lsb not set.
    return LockHandleParam (h, ((DWORD)h & 0x1) ? pprc : g_pprcNK);
}

//------------------------------------------------------------------------------
// Initialize the debuggee thread in the given process
// --> Create a debug event handle within the debuggee thread (handle created in kernel
// handle table)
// Called from CreateThread() whenever a new thread is created.
//------------------------------------------------------------------------------
BOOL DbgrInitThread(PTHREAD pth)
{
    PPROCESS pprc = SwitchActiveProcess(g_pprcNK);
    if (!pth->hDbgEvent) {
        pth->hDbgEvent = NKCreateEvent(0, 0, 0, NULL);
    }
    SwitchActiveProcess(pprc);
    return NULL != pth->hDbgEvent;
}

//------------------------------------------------------------------------------
// Remove debug support from all threads within the debuggee process
// --> Set the debug event with the debuggee thread
// --> Close the debug event handle within the debuggee thread
//------------------------------------------------------------------------------
void DbgrDeInitAllDebuggeeThreads(PPROCESS pprc)
{
    PDLIST ptrav;
    PTHREAD pth;
    PHDATA phd;
    PEVENT lpe;
    HANDLE hdbg;

    // go through all debuggee threads and remove debug support
    LockLoader (pprc);        
    for (ptrav = pprc->thrdList.pFwd; ptrav != &pprc->thrdList; ptrav = ptrav->pFwd) {
        pth = (PTHREAD)ptrav;  
        hdbg = (HANDLE)InterlockedExchange((PLONG)&pth->hDbgEvent, 0);
        if (hdbg) {
            // unblock the debuggee thread
            phd = LockHandleData(hdbg, g_pprcNK);
            lpe = GetEventPtr(phd);
            if (lpe) {
                EVNTSetData(lpe, DBG_CONTINUE);
                EVNTModify(lpe, EVENT_SET);
            }
            UnlockHandleData(phd);
            HNDLCloseHandle(g_pprcNK, hdbg);
        }
    }    
    UnlockLoader (pprc);  
}

//------------------------------------------------------------------------------
// Remove debug support from a debuggee process; called whenever debuggee thread fails to
// either write to the message queue or whenever debugger thread detaches from the debuggee.
//------------------------------------------------------------------------------
BOOL DbgrDeInitDebuggeeProcess(PPROCESS pprc)
{
    HANDLE hMQDebuggeeWrite;
    
    // remove the pDbgrThrd as soon as possible
    pprc->pDbgrThrd = 0;

    // zero-out the write end of the message queue
    hMQDebuggeeWrite = (HANDLE)InterlockedExchange((PLONG)&pprc->hMQDebuggeeWrite, 0);
    
    // close the message queue handle
    HNDLCloseHandle(g_pprcNK, hMQDebuggeeWrite);

    // unblock all debuggee threads currently waiting for continue status from debugger
    DbgrDeInitAllDebuggeeThreads(pprc);
    
    // rest of the debuggee process fields
    pprc->bChainDebug = 0;
    
    return TRUE;
}

//------------------------------------------------------------------------------
// Initialize all debuggee threads in the given process
// --> Create a debug event handle within the debuggee thread (handle created in kernel
// handle table)
//------------------------------------------------------------------------------
BOOL DbgrInitAllDebuggeeThreads(PPROCESS pprc)
{
    PDLIST ptrav;
    PTHREAD pth;
    BOOL fRet = TRUE;

    // debug related handles are always created within the kernel handle table
    DEBUGCHK(pActvProc == g_pprcNK);

    // go through all threads in debuggee process and initialize the thread
    LockLoader (pprc);        
    for (ptrav = pprc->thrdList.pFwd; ptrav != &pprc->thrdList; ptrav = ptrav->pFwd) {
        pth = (PTHREAD)ptrav;            
        if (!pth->hDbgEvent && (NULL == (pth->hDbgEvent = NKCreateEvent(0, 0, 0, NULL)))) {
            fRet = FALSE;
            break;
        }
    }
    UnlockLoader (pprc);  

    return fRet;
}

//------------------------------------------------------------------------------
// Initialize debug support in the debuggee: create message queue handle within the debuggee 
// process; also initialize all threads in the debuggee process with a thread-specific event handle.
// All handles are always created in kernel handle table to prevent unauthorized access to the 
// debug related handles.
// pprc: debuggee process
// hMQRead: read-end of message queue opened in debugger thread
//------------------------------------------------------------------------------
BOOL DbgrInitDebuggeeProcess(PPROCESS pprc, HANDLE hMQRead)
{    
    HANDLE hMQWrite = NULL;
    MSGQUEUEOPTIONS msgopts = {0};

    PTHREAD pth = pCurThread;

    // message queue handles are always created within the kernel handle table
    DEBUGCHK(pActvProc == g_pprcNK);

    // create debuggee end of the message queue (write end)
    msgopts.dwSize = sizeof(MSGQUEUEOPTIONS);    
    msgopts.dwMaxMessages = 1;
    msgopts.cbMaxMessage = sizeof(DEBUG_EVENT);
    msgopts.bReadAccess = FALSE;

    // make sure current process is not being debugged already; if not, set the message queue
    hMQWrite = NKOpenMsgQueueEx(pActvProc, hMQRead, GetCurrentProcess(), &msgopts);
    if (!hMQWrite) {
        return FALSE;
    } else if (InterlockedCompareExchange((LPLONG)&pprc->hMQDebuggeeWrite, (LONG)hMQWrite, 0)) {
        // process is already being debugged; just close the message queue and return
        HNDLCloseHandle(g_pprcNK, hMQWrite);
        KSetLastError(pth, ERROR_ACCESS_DENIED);
        return FALSE;
    }

    // At this point the process is initialized with the debug support; go on to initialize all threads within the debuggee   
    if (DbgrInitAllDebuggeeThreads(pprc)) {
            return TRUE;
    } else {
        // cleanup
        DbgrDeInitDebuggeeProcess(pprc);            
    }

    return FALSE;
}

//------------------------------------------------------------------------------
// Terminate or detach all debuggee processes attached to the current thread.
//------------------------------------------------------------------------------
void DbgrDetachDebuggees(BOOL fTerminate)
{
    PDLIST ptrav;
    PHDATA phd;
    PPROCESS pprc = NULL;
    DWORD dwProcessId;

    do {
        dwProcessId = 0;

        // get a process which needs to be terminated; hold the modlist lock for the shortest time
        LockModuleList ();
        for (ptrav = g_pprcNK->prclist.pFwd; ptrav != &g_pprcNK->prclist; ptrav = ptrav->pFwd) {
            pprc = (PPROCESS)ptrav;
            if (pprc->pDbgrThrd == pCurThread) {
                dwProcessId = pprc->dwId;
                break;
            }            
        }
        UnlockModuleList ();

        // terminate the debuggee process        
        phd = LockHandleData((HANDLE)dwProcessId, g_pprcNK);
        pprc = GetProcPtr(phd);
        if (pprc) {
            // remove debuggee support from this process; otherwise when we terminate this process,
            // we will generate a process exit event which debugger cannot cope with as this was
            // called from debugger thread.
            DbgrDeInitDebuggeeProcess(pprc);

            // terminate the debuggee process
            if (fTerminate)
                PROCTerminate(pprc, 0);
        }
        UnlockHandleData(phd);

    }while(dwProcessId);    
}

//------------------------------------------------------------------------------
// Remove debug support from a debugger thread; this will unblock any debuggee threads
// waiting on response from debugger. Called from debugger exit thread function.
//------------------------------------------------------------------------------
BOOL DbgrDeInitDebuggerThread(PTHREAD pth)
{        
    HANDLE hMQDebuggerRead;

    // terminate or detach all debuggee processes
    DbgrDetachDebuggees(pth->dwDbgFlag & TDBG_KILLONEXIT);

    // get local copy
    hMQDebuggerRead = (HANDLE)InterlockedExchange((PLONG)&pth->hMQDebuggerRead, 0);
            
    // close the message queue handle within the debugger thread
    HNDLCloseHandle(g_pprcNK, hMQDebuggerRead);

    // cleanup rest of the debugger thread fields
    pth->pSavedCtx = 0;
    pth->dwDbgFlag = 0;

    return TRUE;
}

//------------------------------------------------------------------------------
// Create message queue handles within the debugger thread; handles are always created
// in kernel handle table to prevent unauthorized access to the message queues.
// pth: debugger thread
//------------------------------------------------------------------------------
BOOL DbgrInitDebuggerThread(void)
{
    PTHREAD pth = pCurThread;
    
    // message queue handles are always created within the kernel handle table
    DEBUGCHK(pActvProc == g_pprcNK);

    DEBUGMSG (ZONE_DEBUGGER, (L"DbgrInitDebuggerThread Initialiazing debugger thread support for thread 0x%8.8lx\r\n", pth->dwId));    

    if (!pth->hMQDebuggerRead) {
        // MQ init struct
        MSGQUEUEOPTIONS msgopts = {0};

        // create debugger end of the message queue (read end)
        msgopts.dwSize = sizeof(MSGQUEUEOPTIONS);    
        msgopts.dwMaxMessages = 1;
        msgopts.cbMaxMessage = sizeof(DEBUG_EVENT);
        msgopts.bReadAccess = TRUE;

        pth->hMQDebuggerRead = NKCreateMsgQueue(NULL, &msgopts);

        if (!pth->hMQDebuggerRead) {
            DbgrDeInitDebuggerThread(pth);        
        }
    }
    
    return NULL != pth->hMQDebuggerRead;
}

//------------------------------------------------------------------------------
// Initialize debuggee process and debugger thread.
//------------------------------------------------------------------------------
BOOL DbgrInitProcess(PPROCESS pprc, DWORD flags)
{           
    BOOL fRet = TRUE;
    PTHREAD pth = pCurThread;
    
    // switch to kernel so that all debug related handles are created within the kernel handle table
    PPROCESS pprcOld = SwitchActiveProcess(g_pprcNK);

    DEBUGMSG (ZONE_DEBUGGER, (L"DbgrInitProcess pprc = %8.8lx, flags = %8.8lx\r\n", pprc->dwId, flags));    

    if (flags & (DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS)) {

        fRet = DbgrInitDebuggerThread ()
            && DbgrInitDebuggeeProcess (pprc, pth->hMQDebuggerRead);
        
        if (fRet) {
            pprc->pDbgrThrd = pth;
            if (!(flags & DEBUG_ONLY_THIS_PROCESS))
                pprc->bChainDebug = 1;
        }        

    // if parent process has debugger attached and parent process has chain debug set            
    } else if (pprcOld->pDbgrThrd && pprcOld->bChainDebug) {

        // applies only to CreateProcess() calls --> process created as a chained debuggee
        // debugger attached to current process is the debugger for the given process      

        fRet = DbgrInitDebuggeeProcess (pprc, pprcOld->hMQDebuggeeWrite);

        if (fRet) {        
            pprc->pDbgrThrd   = pprcOld->pDbgrThrd;
            pprc->bChainDebug = 1;
        }
    }
    
    // switch back from kernel to current process
    SwitchActiveProcess(pprcOld);
    
    DEBUGMSG (ZONE_DEBUGGER, (L"DbgrInitProcess returns = %8.8lx\r\n", fRet));    
    
    return fRet;    
}

//------------------------------------------------------------------------------
// Send a debug info message from debuggee to debugger and get the response back
// Note: This is called in the context of the debugee thread - hence we cannot 
// update the last error of the calling thread. This function will restore the 
// last error for the calling thread at the function exit.
//------------------------------------------------------------------------------
BOOL SendAndReceiveDebugInfo(PPROCESS pprc, PTHREAD pth, LPCVOID lpBuffer, DWORD cbDataSize, LPDWORD pdwResponse)
{
    DWORD dwFlags = 0;
    BOOL fRet = FALSE;
    DWORD dwPrevErr = KGetLastError(pth);

    // get the MQ event (handles are always in kernel handle table)
    PHDATA phd = LockHandleData (pprc->hMQDebuggeeWrite, g_pprcNK);
    PEVENT lpe = GetEventPtr (phd);

    // send the debug event info and get the status back from debugger thread
    if (   lpe 
        && pth->hDbgEvent 
        && (phd->dwData & PDBG_READY) 
        && MSGQWrite (lpe, lpBuffer, cbDataSize, INFINITE, dwFlags)) {

        fRet = (WAIT_OBJECT_0 == NKWaitForKernelObject(pth->hDbgEvent, INFINITE));
        if (fRet && pdwResponse) {
            PHDATA phd2 = LockHandleData(pth->hDbgEvent, g_pprcNK);
            PEVENT lpe2 = GetEventPtr (phd2);
            *pdwResponse = lpe2? lpe2->dwData : DBG_CONTINUE;
            UnlockHandleData(phd2);
        }
    }
    
    // cleanup
    UnlockHandleData (phd);

    // restore last error
    SetLastError(dwPrevErr);
    
    return fRet;
}

//------------------------------------------------------------------------------
// notify process creation
//------------------------------------------------------------------------------
void DbgrNotifyProcCreate (PTHREAD pth, PPROCESS pprc)
{
    PHDATA phd = NULL;
    DEBUG_EVENT dbginfo = {0};
    LPCREATE_PROCESS_DEBUG_INFO pInfo = &dbginfo.u.CreateProcessInfo;

    DEBUGCHK (IsMainThread (pth));

    dbginfo.dwProcessId = pprc->dwId;
    dbginfo.dwThreadId = pth->dwId;
    dbginfo.dwDebugEventCode = CREATE_PROCESS_DEBUG_EVENT;
    pInfo->hFile = NULL;
    pInfo->hProcess = (HANDLE)pprc->dwId;
    pInfo->hThread = (HANDLE)pth->dwId;
    pInfo->lpStartAddress = (LPTHREAD_START_ROUTINE) pth->dwStartAddr;
    pInfo->lpBaseOfImage = pprc->BasePtr;
    pInfo->dwDebugInfoFileOffset = 0;
    pInfo->nDebugInfoSize = 0;
    pInfo->lpThreadLocalBase = pth->tlsNonSecure;
    pInfo->lpImageName = pprc->BasePtr; // ReadProcessMemory will return the name
    pInfo->fUnicode = 1;

    // mark the message queue as ready to send debug events
    // process create event should be the first debug event
    // (VS requirement)
    phd = LockHandleData (pprc->hMQDebuggeeWrite, g_pprcNK);
    if (phd) {
        phd->dwData |= PDBG_READY;
        UnlockHandleData(phd);
    }
    
    SendAndReceiveDebugInfo(pprc, pth, &dbginfo, sizeof(dbginfo), NULL);    
}

//------------------------------------------------------------------------------
// notify thread creation
//------------------------------------------------------------------------------
void DbgrNotifyThrdCreate (PTHREAD pth, PPROCESS pprc, LPTHREAD_START_ROUTINE lpStartAddress)
{
    DEBUG_EVENT dbginfo = {0};
    LPCREATE_THREAD_DEBUG_INFO pInfo = &dbginfo.u.CreateThread;
    
    dbginfo.dwProcessId = pprc->dwId;
    dbginfo.dwThreadId = pth->dwId;
    dbginfo.dwDebugEventCode = CREATE_THREAD_DEBUG_EVENT;

    pInfo->hThread = (HANDLE)pth->dwId;
    pInfo->lpStartAddress = lpStartAddress;
    pInfo->lpThreadLocalBase = pth->tlsNonSecure;

    SendAndReceiveDebugInfo(pprc, pth, &dbginfo, sizeof(dbginfo), NULL);    
}

//------------------------------------------------------------------------------
// notify process/thread exit
//------------------------------------------------------------------------------
void DbgrNotifyExit (PTHREAD pth, PPROCESS pprc, DWORD dwCode, DWORD dwData)
{   
    // construct the debug event
    DEBUG_EVENT dbginfo = {0};
    LPDWORD pdwCode = (EXIT_PROCESS_DEBUG_EVENT == dwCode)
        ? &dbginfo.u.ExitProcess.dwExitCode
        : &dbginfo.u.ExitThread.dwExitCode;
    
    dbginfo.dwProcessId = pprc->dwId;
    dbginfo.dwThreadId = pth->dwId;
    dbginfo.dwDebugEventCode = dwCode;
    *pdwCode = dwData;

    SendAndReceiveDebugInfo(pprc, pth, &dbginfo, sizeof(dbginfo), NULL);        
}

//------------------------------------------------------------------------------
// notify DLL load
//------------------------------------------------------------------------------
void DbgrNotifyDllLoad (PTHREAD pth, PPROCESS pprc, PMODULE pMod)
{
    DEBUG_EVENT dbginfo = {0};
    LPLOAD_DLL_DEBUG_INFO pInfo = &dbginfo.u.LoadDll;
    
    dbginfo.dwProcessId = pprc->dwId;
    dbginfo.dwThreadId = pth->dwId;
    dbginfo.dwDebugEventCode = LOAD_DLL_DEBUG_EVENT;
    pInfo->hFile = NULL;
    pInfo->lpBaseOfDll = pMod->BasePtr;
    pInfo->dwDebugInfoFileOffset = 0;
    pInfo->nDebugInfoSize = 0;
    pInfo->lpImageName = pMod->BasePtr; // ReadProcessMemory will return the name
    pInfo->fUnicode = 1;

    SendAndReceiveDebugInfo(pprc, pth, &dbginfo, sizeof(dbginfo), NULL);    
}

//------------------------------------------------------------------------------
// notify DLL unload
//------------------------------------------------------------------------------
void DbgrNotifyDllUnload (PTHREAD pth, PPROCESS pprc, PMODULE pMod)
{
    DEBUG_EVENT dbginfo = {0};
    
    dbginfo.dwProcessId = pprc->dwId;
    dbginfo.dwThreadId = pth->dwId;
    dbginfo.dwDebugEventCode = UNLOAD_DLL_DEBUG_EVENT;
    dbginfo.u.UnloadDll.lpBaseOfDll = pMod->BasePtr;

    SendAndReceiveDebugInfo(pprc, pth, &dbginfo, sizeof(dbginfo), NULL);    
}


#define MAX_OBJ_LIST 64

//------------------------------------------------------------------------------
// Send DLL LOAD notifications for all modules loaded within a given process
// This is called in two scenarios:
// a) Debugger attaches to a process in which case all threads in the debuggee
//    are suspended
// b) Debugger creates a process in which case "pth" here is the main thread of
//    of debuggee
//
// pprc -- debuggee process
// pth -- main thread of the process
//------------------------------------------------------------------------------
BOOL SendAllDllLoadEvents(PPROCESS pprc, PTHREAD pth)
{
    DWORD dwModuleInfo[MAX_OBJ_LIST];
    PDWORD pdwModuleInfos = dwModuleInfo;
    int ModCnt = 0;
    int idx = 0;
    MODULE mod;
    PDLIST ptrav;        

    // find the number of modules
    for (ptrav = pprc->modList.pFwd; ptrav != &pprc->modList; ModCnt++, ptrav = ptrav->pFwd);

    // run-time allocate if static array doesn't fit
    if (ModCnt > MAX_OBJ_LIST) {
        // each module entry takes two dwords
        pdwModuleInfos = (LPDWORD)NKmalloc(sizeof(DWORD) * 2 * ModCnt);
        if (!pdwModuleInfos) {
            DEBUGCHK(0);
            return FALSE;
        }
    }

    // get the module info while holding process loader lock
    LockLoader(pprc);
    for (ptrav = pprc->modList.pFwd; (ModCnt > 0) && (ptrav != &pprc->modList); ModCnt--, ptrav = ptrav->pFwd) {
        PMODULELIST pModEntry = (PMODULELIST) ptrav;
        if (pModEntry) {
            PMODULE pMod = pModEntry->pMod;
            if (pMod && (pMod != (PMODULE)hCoreDll)) {
                // coredll notification is sent at the last
                pdwModuleInfos[idx++] = (DWORD)pMod->BasePtr;
            }
        }
    }
    UnlockLoader(pprc);

    // send all the module debug event info (not holding the loader lock)
    while (idx) {
        // only base ptr and image name fields are used
        mod.lpszModName = NULL;
        mod.BasePtr = (LPVOID)pdwModuleInfos[--idx];
        DbgrNotifyDllLoad (pth, pprc, &mod);
    }

    // notify coredll load notification; must be the last
    // module load notification (VS requirement)
    DbgrNotifyDllLoad (pth, pprc, (PMODULE)hCoreDll);

    // free the list
    if (pdwModuleInfos != dwModuleInfo) {
        NKfree(pdwModuleInfos);
    }
    
    return TRUE;
}

//------------------------------------------------------------------------------
// Send the current state of the debuggee process to debugger; send notifications 
// for process creation, all the module loads within the process, and all the 
// thread creation within the process.
// Note that the debuggee process is suspended for the duration of this call.
// Hence we don't hold loader lock when traversing the process module list
//------------------------------------------------------------------------------
BOOL DbgrSendCurrentDebuggeeState(PPROCESS pprc) 
{
    int iNumThreads;
    PDLIST ptrav;
    PTHREAD pth;
    PTHREAD pthMain;
    PHDATA phd;
    DWORD dwThreadIds[MAX_OBJ_LIST];
    PDWORD pdwThreadIds = dwThreadIds;
        
    pthMain = MainThreadOfProcess(pprc);
    DEBUGCHK(pthMain);
    
    // send process creation event
    DbgrNotifyProcCreate (pthMain, pprc);

    if (pprc->wThrdCnt > MAX_OBJ_LIST) {
        pdwThreadIds = (LPDWORD)NKmalloc(sizeof(DWORD) * pprc->wThrdCnt);
        if (!pdwThreadIds) {
            DEBUGCHK(0);
            return FALSE;
        }
    }
    
    // get the list of all threads in the process
    LockLoader (pprc);
    for (ptrav = pprc->thrdList.pFwd, iNumThreads = 0; (ptrav != &pprc->thrdList) && (iNumThreads < pprc->wThrdCnt); ptrav = ptrav->pFwd, iNumThreads++) {
        pdwThreadIds[iNumThreads] = ((PTHREAD)ptrav)->dwId;
    }
    UnlockLoader(pprc);

    // now send the thread create notifications for all threads
    while(--iNumThreads >= 0) {
        phd = LockHandleData((HANDLE)pdwThreadIds[iNumThreads], g_pprcNK);
        pth = GetThreadPtr(phd);
        if (pth)
            DbgrNotifyThrdCreate(pth, pprc, (LPTHREAD_START_ROUTINE) (pth->dwStartAddr));
        UnlockHandleData(phd);
    }
    
    // free the list
    if (pdwThreadIds != dwThreadIds) {
        NKfree(pdwThreadIds);
    }
    
    // send all the dll load events
    SendAllDllLoadEvents(pprc, pthMain);

    return TRUE;
}

//------------------------------------------------------------------------------
// Resume all the suspended threads within the process.
//------------------------------------------------------------------------------
void DbgrResumeAllThreadsInProcess(PPROCESS pprc) 
{    
    PDLIST ptrav;
    PTHREAD pth;

    // get a thread which needs to be resumed; hold the process loader lock for the shortest time
    LockLoader (pprc);        
    for (ptrav = pprc->thrdList.pFwd; ptrav != &pprc->thrdList; ptrav = ptrav->pFwd) {
        pth = (PTHREAD)ptrav;
        if (GET_DEBUGBLK(pth)) {
            CLEAR_DEBUGBLK(pth);        
            SCHL_ThreadResume (pth);
        }            
    }
    UnlockLoader (pprc);  
}

//------------------------------------------------------------------------------
// Sent all the debug events from debuggee process and then resume the process 
// execution. This could be called after failed to add debug support to a process; in which
// case threads in debuggee process are resumed but we don't send the debug events.
//------------------------------------------------------------------------------
void DbgrResumeProcess(PPROCESS pprc) 
{
    // if we added debug support to the process, send all debug events
    if (pprc->pDbgrThrd) {
        if (!DbgrSendCurrentDebuggeeState(pprc)) {
            // failed to send debug events from debuggee; remove debugger from debuggee
            DbgrDeInitDebuggeeProcess(pprc);
        }
    }
    
    // resume all threads within the debuggee process
    DbgrResumeAllThreadsInProcess(pprc);
    
    NKExitThread(0);
}

//------------------------------------------------------------------------------
// Suspend all the threads within the process.
//------------------------------------------------------------------------------
BOOL DbgrSuspendAllThreadsInProcess(PPROCESS pprc) 
{
    BOOL fRet = TRUE;
    
    PDLIST ptrav;
    PHDATA phd;
    PTHREAD pth;

    DWORD dwRes;
    DWORD dwThreadId;

    do {
        dwThreadId = 0;

        // get a thread which needs to be suspended; hold the process loader lock for the shortest time
        LockLoader (pprc);        
        for (ptrav = pprc->thrdList.pFwd; ptrav != &pprc->thrdList; ptrav = ptrav->pFwd) {
            pth = (PTHREAD)ptrav;
            if (!GET_DEBUGBLK(pth)) {
                dwThreadId = pth->dwId;
                break;
            }            
        }
        UnlockLoader (pprc);  

        // suspend the thread        
        phd = LockHandleData((HANDLE)dwThreadId, g_pprcNK);
        pth = GetThreadPtr(phd);
        if (pth) {
            dwRes = SCHL_ThreadSuspend (pth);
            if (dwRes == 0xfffffffe) {
                Sleep(10); // try again
            } else if (dwRes == 0xffffffff) {
                fRet = FALSE;
            } else {
                SET_DEBUGBLK(pth);
            }
        }
        UnlockHandleData(phd);

    }while(dwThreadId && fRet);    

    return fRet;
}

//------------------------------------------------------------------------------
// DbgrSetDispatchFlag - checks if given thread is being debugged by the curren thread and if so,
// sets the debug dispatch bit; used to validate the debugger response to unblock this thread.
//------------------------------------------------------------------------------
BOOL DbgrSetDispatchFlag (DWORD dwThreadId)
{
    BOOL fRet = FALSE;
    PHDATA phd = LockHandleData ((HANDLE)dwThreadId, g_pprcNK);
    PTHREAD pth = GetThreadPtr(phd);

    if (pth && pth->pprcOwner && (pth->pprcOwner->pDbgrThrd == pCurThread)) { 
        // current thread debugging the given thread
        pth->dwDbgFlag |= TDBG_DISPATCHED;
        fRet = TRUE;
    }

    // cleanup
    UnlockHandleData(phd);            
    return fRet;
}

//------------------------------------------------------------------------------
// NKDebugSetProcessKillOnExit -DebugSetProcessKillOnExit WIN32 API call 
//------------------------------------------------------------------------------
BOOL NKDebugSetProcessKillOnExit(BOOL fKillOnExit)
{
    BOOL fRet = TRUE;

    DEBUGMSG (ZONE_DEBUGGER, (L"DebugSetProcessKillOnExit: fKillOnExit = %8.8lx\r\n", fKillOnExit));

    if (!pCurThread->hMQDebuggerRead) {
        fRet = FALSE;
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);        
    } else if (fKillOnExit) {
        // kill on exit
        pCurThread->dwDbgFlag |= TDBG_KILLONEXIT;
    } else {
        // detach on exit
        pCurThread->dwDbgFlag &= ~TDBG_KILLONEXIT;
    }

    DEBUGMSG (ZONE_DEBUGGER, (L"DebugSetProcessKillOnExit: returns = %8.8lx\r\n", fRet));

    return fRet;
}

//------------------------------------------------------------------------------
// NKDebugNotify - DebugNotify API call
//------------------------------------------------------------------------------
void NKDebugNotify(
    DWORD dwFlags,
    DWORD data
    ) 
{
    PPROCESS pprc = pActvProc;
    PTHREAD pth = pCurThread;
    
    if (pprc->pDbgrThrd) {
        switch (dwFlags) {
            case DLL_PROCESS_ATTACH:
                DbgrNotifyProcCreate (pth, pprc);
                DbgrNotifyThrdCreate (pth, pprc, (LPTHREAD_START_ROUTINE) data); // main thread create event                
                SendAllDllLoadEvents(pprc, pth);
                break;
            case DLL_PROCESS_DETACH:
                DbgrNotifyExit (pth, pprc, EXIT_PROCESS_DEBUG_EVENT, data);
                break;
            case DLL_THREAD_ATTACH:
                DbgrNotifyThrdCreate (pth, pprc, (LPTHREAD_START_ROUTINE) data);
                break;
            case DLL_THREAD_DETACH:
                DbgrNotifyExit (pth, pprc, EXIT_THREAD_DEBUG_EVENT, data);
                break;
        }
    }
}

//------------------------------------------------------------------------------
// NKDebugActiveProcessStop - DebugActiveProcessStop WIN32 API call
// dwProcessId could be a process Id or a handle returned from OpenProcess call
//------------------------------------------------------------------------------
BOOL NKDebugActiveProcessStop (DWORD dwProcessId)
{
    PTHREAD pth = pCurThread;
    DWORD dwError = ERROR_INVALID_PARAMETER;
    PHDATA phd = LockLocalOrKernelHandle ((HANDLE)dwProcessId, pActvProc);  // debuggee process hdata
    PPROCESS pDebuggee = GetProcPtr(phd); // debuggee process object
    PHDATA phdQ = LockHandleData (pth->hMQDebuggerRead, g_pprcNK); // debugger thread message queue
    PEVENT pEvent = GetEventPtr(phdQ); // debugger thread message queue event
    
    DEBUGMSG (ZONE_DEBUGGER, (L"DebugActiveProcessStop: dwProcessId = %8.8lx\r\n", dwProcessId));

    // check if valid debuggee and current thread is debugger for the debuggee process        
    if (pDebuggee 
        && (pDebuggee->pDbgrThrd == pth)) { 
    
        // remove debug support from the process
        DbgrDeInitDebuggeeProcess(pDebuggee);

        // if this is the last debugger attached to current thread, remove debugger support
        if (pEvent && pEvent->pMsgQ && !(pEvent->pMsgQ->phdWriter)) {
            DbgrDeInitDebuggerThread(pth);        
        }

        dwError = 0;
    }
    
    UnlockHandleData (phd);
    UnlockHandleData (phdQ);

    DEBUGMSG (ZONE_DEBUGGER, (L"DebugActiveProcessStop: GLE = 0x%8.8lx\r\n", dwError));

    if (dwError) {
        KSetLastError(pth,dwError);
    }
    
    return !dwError;
}

//------------------------------------------------------------------------------
// NKDebugActiveProcess - DebugActiveProcess WIN32 API call
// dwProcessId could be a process Id or a handle returned from OpenProcess call
//------------------------------------------------------------------------------
BOOL NKDebugActiveProcess (DWORD dwProcessId)
{
    DWORD dwError = 0;
    HANDLE hth = NULL;
    
    PPROCESS pprc = pActvProc;
    PTHREAD pth = pCurThread;
    PHDATA phd = LockLocalOrKernelHandle ((HANDLE)dwProcessId, pprc);
    PPROCESS pDebuggee = GetProcPtr(phd);
   
    DEBUGMSG (ZONE_DEBUGGER, (L"DebugActiveProcess: dwProcessId = %8.8lx\r\n", dwProcessId));

    // Is this a valid process or trying to debug self
    if (!pDebuggee || (pDebuggee->dwId == pprc->dwId)) {
        dwError = ERROR_INVALID_PARAMETER;
    } else if (ProcessNotDebuggable (pDebuggee) || pDebuggee->pDbgrThrd) {
        dwError = ERROR_ACCESS_DENIED;
    } else {    
        DWORD dwPrio;
        THRDGetPrio256 (pth, &dwPrio);

        // scale down the priority of the current thread (debugger)
        THRDSetPrio256 (pth, pDebuggee->bPrio? pDebuggee->bPrio - 1 : 0);

        // Add debug support to the process (debuggee)
        if (!DbgrSuspendAllThreadsInProcess(pDebuggee)) { // suspend all the thread in the process
            dwError = WAIT_TIMEOUT;          
        } else {
            // add debug support to the debuggee and possibly debugger thread
            if (!DbgrInitProcess(pDebuggee, DEBUG_ONLY_THIS_PROCESS)) {
                dwError = (!GetLastError()) ? ERROR_INVALID_PARAMETER : GetLastError();
            }
            
            // resume all the threads irrespective of debug support is added to the process or not
            hth = CreateKernelThread((LPTHREAD_START_ROUTINE)DbgrResumeProcess,pDebuggee,(WORD) (pDebuggee->bPrio? pDebuggee->bPrio-1 : 0), 0x40000000);
            if (!hth) {
                dwError = (!GetLastError()) ? ERROR_OUTOFMEMORY : GetLastError();
            }
        }

        // restore the priority of the current thread (debugger)
        THRDSetPrio256 (pth, dwPrio);
    }
    
    HNDLCloseHandle(g_pprcNK, hth);
    UnlockHandleData (phd);

    DEBUGMSG (ZONE_DEBUGGER, (L"DebugActiveProcess GLE = 0x%8.8lx\r\n", dwError));    

    if (dwError) {
        KSetLastError(pth, dwError);
    }
    
    return !dwError;
}

//------------------------------------------------------------------------------
// NKWaitForDebugEvent - WaitForDebugEvent WIN32 API call
//------------------------------------------------------------------------------
BOOL 
NKWaitForDebugEvent (
    LPDEBUG_EVENT lpDebugEvent,
    DWORD dwMilliseconds
    ) 
{
    BOOL    fRet   = FALSE;
    PTHREAD pCurTh = pCurThread;
    PHDATA  phdQ   = LockHandleData (pCurTh->hMQDebuggerRead, g_pprcNK);
    PEVENT  pMsgQ  = GetEventPtr(phdQ);
    
    DEBUGMSG (ZONE_DEBUGGER, (L"WaitForDebugEvent timeout: 0x%8.8lx\r\n", dwMilliseconds));    
    
    if (!lpDebugEvent || !pMsgQ)
    {
        KSetLastError(pCurTh, ERROR_INVALID_PARAMETER);

    } else {
    
        DWORD dwPrevErr = GetLastError();
        DWORD ccbSizeRead;
        DWORD dwFlags;

        // loop until we get a valid debug notification or until we fail
        do {
            //
            // DO NOT SET USERBLOCK HERE, OR THE THREAD CAN BE TERMINTAED in the middle of processing.
            //
        
            KSetLastError(pCurTh, 0);

            // assume failure
            ccbSizeRead = 0;
            lpDebugEvent->dwDebugEventCode = (DWORD) -1;
            
            // get the debug event from debuggee
            if (MSGQRead(pMsgQ, lpDebugEvent, sizeof(DEBUG_EVENT), &ccbSizeRead, dwMilliseconds, &dwFlags, NULL)) // read failed?
            { 
                DEBUGCHK(ccbSizeRead == sizeof(DEBUG_EVENT));
                if (!DbgrSetDispatchFlag(lpDebugEvent->dwThreadId)) { // debuggee thread invalid?
                    lpDebugEvent->dwDebugEventCode = 0; // drop the message and continue the wait
                } else {              
                    fRet = TRUE;
                }                

            } else if (ERROR_PIPE_NOT_CONNECTED == GetLastError()) {
                // all attached debuggees have exited; remove debugger support from current thread
                DbgrDeInitDebuggerThread(pCurTh);

            } else if (ERROR_TIMEOUT == GetLastError()) {
                // VS expects the last error to be WAIT_TIMEOUT
                KSetLastError(pCurTh, WAIT_TIMEOUT);
            }

            // cleanup
            
        }while(!lpDebugEvent->dwDebugEventCode);
        
        DEBUGMSG (ZONE_DEBUGGER, (L"WaitForDebugEvent returns = %8.8lx, event code = %8.8lx\r\n", fRet, lpDebugEvent->dwDebugEventCode));    

        // restore last error
        if (0 == KGetLastError(pCurTh)) {
            KSetLastError(pCurTh, dwPrevErr);
        }
    }

    UnlockHandleData (phdQ);            
    return fRet;
}


//------------------------------------------------------------------------------
// NKContinueDebugEvent - ContinueDebugEvent WIN32 API call
//------------------------------------------------------------------------------
BOOL 
NKContinueDebugEvent(
    DWORD dwProcessId, // unused
    DWORD dwThreadId,
    DWORD dwContinueStatus
    ) 
{
    PEVENT lpe = NULL;
    PHDATA phd = NULL;
    DWORD dwError = 0;
    PTHREAD pth = pCurThread;
    PHDATA phd_dbg = LockLocalOrKernelHandle ((HANDLE)dwThreadId, pActvProc);
    PTHREAD pth_dbg = GetThreadPtr(phd_dbg);
    
    DEBUGMSG (ZONE_DEBUGGER, (L"ContinueDebugEvent PID: 0x%8.8lx, TID: 0x%8.8lx, Status: 0x%8.8lx\r\n", dwProcessId, dwThreadId, dwContinueStatus));    
    
    if (!pth_dbg                        // valid debuggee thread?
        || !pth_dbg->pprcOwner          // valid process object?
        || !pth_dbg->hDbgEvent          // valid thread debug event?
        || (pth_dbg->pprcOwner->pDbgrThrd != pth)                   // valid debugger?
        || !(pth_dbg->dwDbgFlag & TDBG_DISPATCHED)                  // did this thread generate a debug event? 
        || (NULL == (phd = LockHandleData(pth_dbg->hDbgEvent, g_pprcNK)))    // valid debug event?
        || (NULL == (lpe = GetEventPtr(phd))))                      // valid event?
    {
        dwError = ERROR_INVALID_PARAMETER;
    } else {
        EVNTSetData(lpe, dwContinueStatus);
        EVNTModify(lpe, EVENT_SET);
        pth_dbg->dwDbgFlag &= ~TDBG_DISPATCHED;
    }

    // cleanup
    UnlockHandleData (phd_dbg);
    UnlockHandleData (phd);
    
    DEBUGMSG (ZONE_DEBUGGER, (L"ContinueDebugEvent GLE = %8.8lx\r\n", dwError));    

    if (dwError) {
        KSetLastError(pth, dwError);
    }
        
    return !dwError;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
NKSetJITDebuggerPath (
    LPCWSTR pszDbgrPath
    )
{
    // allocate memory to store the name
    PNAME pDbgr = DupStrToPNAME (pszDbgrPath);

    if (pszDbgrPath && !pDbgr) {
        KSetLastError (pCurThread, ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    DEBUGMSG (ZONE_DEBUGGER, (L"SetJITDebuggerPath name = %s\r\n", pDbgr->name));    

    // use InterlockedExchangePointer to update the 'global' debugger name pointer
    pDbgr = InterlockedExchangePointer (&pDebugger, pDbgr);

    // free the old name if exist
    if (pDbgr) {
        FreeName (pDbgr);
    }

    return TRUE;
}

const char Hexmap[17] = "0123456789abcdef";

CallSnapshot cstks[MAX_CALLSTACK];
ULONG        nCstks;

DWORD JITGetCallStack (HANDLE hThrd, ULONG dwMaxFrames, CallSnapshot lpFrames[])
{
    // hThrd is ignored for current implementation. We'll make use of it when
    // we have the implementation for getting call stack for arbitrary threads.

    if (dwMaxFrames > nCstks) {
        dwMaxFrames = nCstks;
    }

    memcpy (lpFrames, cstks, dwMaxFrames * sizeof (CallSnapshot));
    return dwMaxFrames;        
}

static void DbgGetExcpStack (PCONTEXT pctx)
{
    // we need use a local copy of the context to pass to NKGetCallStack since it'll overwrite the context while walking the stack.
    CONTEXT ctx = *pctx;
    nCstks = NKGetThreadCallStack (pCurThread, MAX_CALLSTACK, cstks, 0, 0, &ctx);
}

//------------------------------------------------------------------------------
// Called by exception dispatch routine to send an exception to attached debugger for the current
// thread.
//------------------------------------------------------------------------------
BOOL 
UserDbgTrap(
    EXCEPTION_RECORD *er,
    PCONTEXT pctx,
    BOOL bSecond
    ) 
{

    PTHREAD pth = pCurThread;
    PPROCESS pprc = pActvProc;
    DWORD dwActive = dwActvProcId;
    
    // get callstack of current running thread if second chance. Don't do it if it's stack overflow or we might fault recursively
    if (bSecond 
        && (STATUS_BREAKPOINT != er->ExceptionCode)
        && (STATUS_STACK_OVERFLOW != er->ExceptionCode)) {
        DbgGetExcpStack (pctx);
    }

    // only launch JIT debugger on second chance exception
    if (bSecond && (er->ExceptionCode != STATUS_BREAKPOINT) && pDebugger && !pprc->pDbgrThrd) {
        CE_PROCESS_INFORMATION CeProcInfo = {sizeof(CE_PROCESS_INFORMATION), 0, NULL, 0};
        DWORD ExitCode;
        WCHAR cmdline[11];
        cmdline[0] = '0';
        cmdline[1] = 'x';
        cmdline[2] = (WCHAR)Hexmap[(((dwActive)>>28)&0xf)];
        cmdline[3] = (WCHAR)Hexmap[(((dwActive)>>24)&0xf)];
        cmdline[4] = (WCHAR)Hexmap[(((dwActive)>>20)&0xf)];
        cmdline[5] = (WCHAR)Hexmap[(((dwActive)>>16)&0xf)];
        cmdline[6] = (WCHAR)Hexmap[(((dwActive)>>12)&0xf)];
        cmdline[7] = (WCHAR)Hexmap[(((dwActive)>> 8)&0xf)];
        cmdline[8] = (WCHAR)Hexmap[(((dwActive)>> 4)&0xf)];
        cmdline[9] = (WCHAR)Hexmap[(((dwActive)>> 0)&0xf)];
        cmdline[10] = 0;
        if (NKCreateProcess(pDebugger->name,cmdline,&CeProcInfo)) {
            HNDLCloseHandle(pprc, CeProcInfo.ProcInfo.hThread);
            while (PROCGetCode(CeProcInfo.ProcInfo.hProcess,&ExitCode) && (ExitCode == STILL_ACTIVE) && !pprc->pDbgrThrd)
                Sleep(250);
            HNDLCloseHandle(pprc, CeProcInfo.ProcInfo.hProcess);
            while (pth->bPendSusp)
                Sleep (250);
        }
    }

    // send the exception event to debugger for curren thread
    if (pprc->pDbgrThrd) {
        DWORD dwDebugEventCode = 0;
        DWORD dwPrio = 0;
        DEBUG_EVENT dbginfo = {0};
        
        THRDGetPrio256 (pprc->pDbgrThrd, &dwPrio);
        pth->pSavedCtx = pctx;
        dbginfo.dwDebugEventCode = EXCEPTION_DEBUG_EVENT;
        dbginfo.dwProcessId = dwActive;
        dbginfo.dwThreadId = dwCurThId;
        dbginfo.u.Exception.dwFirstChance = !bSecond;
        dbginfo.u.Exception.ExceptionRecord = *er;

        // boost the priority of the debugger thread
        if (dwPrio >= pprc->bPrio) {
            // can't call SC_CeSetThreadPriority since the debugee might not be trusted
            SCHL_SetThreadBasePrio (pprc->pDbgrThrd, pprc->bPrio? pprc->bPrio-1 : 0);
        }

        // send the debug info to the debugger and get the response back
        SendAndReceiveDebugInfo(pprc, pth, &dbginfo, sizeof(dbginfo), &dwDebugEventCode);    
        
        // restore the priority of the debugger thread
        //  -- can't call SC_CeSetThreadPriority since the debugee might not be trusted
        SCHL_SetThreadBasePrio (pprc->pDbgrThrd, dwPrio);
        
        pth->pSavedCtx = 0;
        if (dwDebugEventCode == DBG_CONTINUE)
            return TRUE;
    }
    
    return FALSE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
HANDLE 
WakeIfDebugWait(
    HANDLE hThrd
    ) 
{
    return NULL;
}

