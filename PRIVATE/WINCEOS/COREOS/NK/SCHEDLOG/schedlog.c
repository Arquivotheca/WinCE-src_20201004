//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <windows.h>
#include <schedlog.h>
#include <kernel.h>
#include <profiler.h>

extern void CreateNewProc(ulong nextfunc, ulong param);
extern void (*TBFf)(LPVOID, ulong);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
SchedLogTranslate(
    DWORD dwThreadID,
    HANDLE* pThreadHandle,
    DWORD dwProcID,
    HANDLE* pProcessHandle
    )
{
    //
    // Translate the PTHREAD to a handle to the thread.
    //
    if (pThreadHandle != NULL) {
        if (dwThreadID) {
            *pThreadHandle = ((PTHREAD) dwThreadID)->hTh;
        } else {
            *pThreadHandle = NULL;
        }
    }
    //
    // Translate the PPROCESS to a handle to the process.
    //
    if (pProcessHandle != NULL) {
        if (dwProcID) {
            *pProcessHandle = ((PPROCESS) dwProcID)->hProc;
        } else {
            *pProcessHandle = NULL;
        }
    }
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPCSTR
SchedLogGetProcessName(
    DWORD dwProcID
    )
{
    PPROCESS pProc = (PPROCESS) dwProcID;

    if (!pProc) {
        return NULL;
    }

    //
    // Return a pointer to the process's name (from the table of contents)
    //
    if (pProc->oe.filetype == FT_ROMIMAGE) {
       TOCentry* tocptr = pProc->oe.tocptr;
       if (tocptr != NULL) {
           return tocptr->lpszFileName;
       }
    } 

    //
    // We have no direct pointer to the process name in the TOC.
    // So we can use the unicode name from the process struct to look up the
    // ASCII name in the TOC.
    //
    
    {
        LPWSTR lpszNameW = pProc->lpszProcName; // Unicode name
        CHAR lpszNameA[MAX_PATH];               // (local) ASCII copy of name
        WCHAR* pStart;
        TOCentry* pMod;
        DWORD i;
        
        if (!lpszNameW) {
            return NULL;
        }

        // Set up an ASCII name, stripped of path, to use for comparison
        pStart = lpszNameW;
        for (i = 0; lpszNameW[i]; i++) {
            if (lpszNameW[i] == TEXT('\\')) {
                pStart = lpszNameW + i + 1;
            }
        }
        KUnicodeToAscii(lpszNameA, pStart, MAX_PATH);


        // Scan the modules list looking for a module with the same name
        pMod = (TOCentry*) ((DWORD) pTOC + sizeof(ROMHDR));
        for (i = 0; i < pTOC->nummods; i++) {
            
            if (!strcmp(pMod[i].lpszFileName, lpszNameA)) {
                // Return pointer to ASCII string in TOC
                return pMod[i].lpszFileName;
            } 
        }
    }

    return NULL;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD SchedLogGetThreadProgramCounter(
    DWORD dwThreadID,
    LPCSTR* ppszThreadModuleName,
    LPCSTR* ppszThreadFunctionName
    )
{
    DWORD dwProgramCounter = 0;
    PTHREAD pth = (PTHREAD) dwThreadID;
    BOOL fNewProc = FALSE;

    //
    // Return a pointer to the thread's name if available.
    //
    if (dwThreadID != 0) {
        dwProgramCounter = (DWORD) GetThreadIP(pth);
        
        if (dwProgramCounter == (DWORD) TBFf) {
            //
            // ThreadBaseFunction. Get the underlying function.
            //
#ifdef x86            
            dwProgramCounter = *(DWORD*)(pth->ctx.TcxEsp+4);
#else
            dwProgramCounter = (DWORD) pth->ARG0;
#endif
        }

        if (dwProgramCounter == (DWORD) CreateNewProc) {
            //
            // Creating a new process, so thread and module name = process name
            //

            LPCSTR lpszProcName = SchedLogGetProcessName((DWORD) pth->pOwnerProc);

            if (ppszThreadModuleName != NULL) {
                *ppszThreadModuleName = lpszProcName;
            }
            if (ppszThreadFunctionName != NULL) {
                *ppszThreadFunctionName = lpszProcName;
            }
        
        } else {
            // Module name
            if (ppszThreadModuleName != NULL) {
               *ppszThreadModuleName = GetModName(pth, dwProgramCounter);
            }
            
            // Function name
            if (ppszThreadFunctionName != NULL) {
               *ppszThreadFunctionName = GetFunctionName(pth, dwProgramCounter);
            }
        }
        
    } else {
        if (ppszThreadModuleName != NULL) {
            *ppszThreadModuleName = NULL;
        }
        if (ppszThreadFunctionName != NULL) {
            *ppszThreadFunctionName = NULL;
        }
    }
    
    return dwProgramCounter;
}

