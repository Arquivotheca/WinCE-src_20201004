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
//    diagnose.cpp - DDx general diagnostic functions
//

#include "osaxs_p.h"
#include "DwCeDump.h"
#include "diagnose.h"


// Useful thing to have.
#define lengthof(x)                     (sizeof(x) / sizeof(*x))

static WCHAR* g_pszLogHead = NULL;
static WCHAR* g_pszLogTail = NULL;
static DWORD g_dwLogStart = NULL;

LPBYTE g_pDDxHeapBegin = NULL;
LPBYTE g_pDDxHeapFree  = NULL;

UINT   g_numProcs = 0;


CEDUMP_DIAGNOSIS_LIST       g_ceDmpDiagnosisList;

// Flat lists of fixed size (small lists, low freq. access)
CEDUMP_DIAGNOSIS_DESCRIPTOR g_ceDmpDiagnoses  [ DDX_MAX_DIAGNOSES ];
UINT                        g_nDiagnoses = 0;

DDX_DIAGNOSIS_ID            g_PersistentDiagnoses [ DDX_MAX_PERSISTENT_DIAGNOSES ];
int                         g_nPersistentDiagnoses = 0;

// Chained hash table (big-list, high freq. access)
int                         g_KernObjHashTable[DDX_MAX_KERNEL_OBJECTS];
KERNEL_OBJECT               g_KernelObjectList[DDX_MAX_KERNEL_OBJECTS];
int                         g_nKernelObjects = 0;

int g_HashSize = 0;

CEDUMP_BUCKET_PARAMETERS    g_ceDmpDDxBucketParameters;


/*++

Routine Description:


Arguments:


Return Value:

--*/
int 
ddxlog(
       LPCWSTR wszFmt, 
       ...
       )
{
    WCHAR wszBuf[256];
    UINT cStrLen = 0;

    va_list varargs;

    if (NKwvsprintfW != NULL)
    {
        va_start(varargs, wszFmt);
        NKwvsprintfW(wszBuf, wszFmt, varargs, sizeof(wszBuf)/sizeof(wszBuf[0]));
        va_end(varargs);

        wszBuf[255] = L'\0';

        cStrLen = kdbgwcslen(wszBuf);

        if ((((g_pszLogTail - g_pszLogHead) + cStrLen) * sizeof(WCHAR)) > LOG_SIZE)
        {
            return -1;
        }

        memcpy((void*)g_pszLogTail, wszBuf, (cStrLen + 1) * sizeof(WCHAR));

        g_pszLogTail += cStrLen;
    }

    return cStrLen;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void InitDDxLog(LPWSTR wszKernVA)
{
    g_pszLogHead = wszKernVA;
    g_pszLogTail = g_pszLogHead;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
DWORD BeginDDxLogging(void)
{
    g_dwLogStart = (DWORD) g_pszLogTail;
    return g_dwLogStart;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
DWORD EndDDxLogging(void)
{
    if (g_pszLogTail)
    {
        *g_pszLogTail = L'\0';
        g_pszLogTail++;
        return (DWORD) g_pszLogTail - g_dwLogStart;
    }
    else
    {
        return 0;
    }
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
LPWSTR GetDDxLogHead(void)
{
    return g_pszLogHead;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void DumpDDxLog(void)
{
    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"\r\n"));
    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDx!g_pszLogHead = 0x%08x\r\n", g_pszLogHead));
    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDx!g_pszLogTail = 0x%08x\r\n", g_pszLogTail));
    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"\r\n"));
    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDx!Logs\r\n"));
    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"%s\r\n", g_pszLogHead));
    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"\r\n"));
}


//-----------------------------------------------------------------------------
// Add Kernel Object
//-----------------------------------------------------------------------------
int
AddKernelObject (
                 DWORD type,
                 LPVOID pObj
                 )
{
    int listIndex = g_nKernelObjects;

    if ((listIndex >= 0) &&
        (listIndex < DDX_MAX_KERNEL_OBJECTS))
    {
        g_KernelObjectList[listIndex].type = type;
        g_KernelObjectList[listIndex].pObj = pObj;
        g_KernelObjectList[listIndex].next = 0;

        g_nKernelObjects++;

        return listIndex;
    }
    else
    {
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDx!AddKernelObject:  Max kernel objects exceeded!\r\n"));
    }

    return -1;
}


//-----------------------------------------------------------------------------
// Add Kernel Object
//
// WARNING: This will fail for MIPS and SH as they do not use the standard
// kernel heap mechanism.  This may not be a problem as these will not be full
// Windows Mobile devices and may not require the full KDMP optimizations.
//-----------------------------------------------------------------------------
int
CacheKernelObject (
                   DWORD type,
                   LPVOID pObj
                   )
{
    if (g_HashSize && pObj && g_pDebuggerData->ppKHeapBase != NULL)
    {
        int hashIndex = ((UINT) pObj - (UINT) g_pKHeapBase) / g_HashSize;

        if ((hashIndex >= 0) &&
            (hashIndex > DDX_MAX_KERNEL_OBJECTS))
        {
            DEBUGGERMSG(OXZONE_DIAGNOSE, ((L"DDx!CacheKernelObject:  Hash out off bounds.  index = %d, hash size = %d, base = 0x%08x\r\n"), 
                hashIndex, g_HashSize, g_pKHeapBase));
            return -1;
        }

        int listIndex = g_KernObjHashTable[hashIndex];

        if (listIndex >= 0)
        {
            // Slot is taken.
            // This is a chained has table, so we need to add 
            // to linked list of items at this slot.

            int i = listIndex;
            do 
            {
                // Same object
                if (g_KernelObjectList[listIndex].pObj == pObj)
                {
                    return i;
                }

                // End of linked list
                if (!g_KernelObjectList[listIndex].next)
                    break;

                i = g_KernelObjectList[listIndex].next;
            }
            while (listIndex && (listIndex < DDX_MAX_KERNEL_OBJECTS));


            // Add a new item to the
            int newIndex = AddKernelObject(type, pObj);

            if (newIndex >= 0)
            {
                g_KernelObjectList[listIndex].next = newIndex;
            }
        }
        else
        {
            // Open slot, add to kernel list and index through hash table

            listIndex = AddKernelObject(type, pObj);

            if (listIndex >= 0)
            {
                g_KernObjHashTable[hashIndex] = listIndex;
            }
        }
    }

    return -1;
}


//-----------------------------------------------------------------------------
// Find Kernel Object
//
// WARNING: This will fail for MIPS and SH as they do not use the standard
// kernel heap mechanism.  This may not be a problem as these will not be full
// Windows Mobile devices and may not require the full KDMP optimizations.
//-----------------------------------------------------------------------------
int
FindKernelObject (
                  DWORD type,
                  LPVOID pObj
                  )
{
    if (g_HashSize && pObj && g_pDebuggerData->ppKHeapBase != NULL)
    {
        int hashIndex = ((UINT) pObj - (UINT) g_pKHeapBase) / g_HashSize;

        if (hashIndex > DDX_MAX_KERNEL_OBJECTS)
        {
            DEBUGGERMSG(OXZONE_DIAGNOSE, ((L"DDx!FindKernelObject:  Hash out off bounds.  index = %d, hash size = %d, base = 0x%08x\r\n"), 
                hashIndex, g_HashSize, g_pKHeapBase));
            return -1;
        }

        int listIndex = g_KernObjHashTable[hashIndex];

        if (listIndex)
        {
            // Slot is taken.
            // This is a chained has table, so we need to add 
            // to linked list of items at this slot.

            int next = listIndex;
            do 
            {
                if (g_KernelObjectList[next].pObj == pObj)
                {
                    // Found object, return
                    return next;
                }

                next = g_KernelObjectList[next].next;
            }
            while (next);
        }
    }

    return -1;
}



//-----------------------------------------------------------------------------
// Init Kernel Object Hash
//-----------------------------------------------------------------------------
BOOL
InitKernelObjectHashTable (
                           UINT maxObjects
                           )
{
    // TODO: ?? Move lists to kernel allocated RAM, or leave as static lists?

    memset((void*)g_KernObjHashTable, -1, sizeof(int) * DDX_MAX_KERNEL_OBJECTS);
    memset((void*)g_KernelObjectList, 0x0, sizeof(KERNEL_OBJECT) * DDX_MAX_KERNEL_OBJECTS);

    g_HashSize       = 0;
    g_nKernelObjects = 0;
    UINT sizeKHeap   = 0;


    // Set up hash parameters

    if (g_pDebuggerData->ppKHeapBase != NULL &&
        g_pDebuggerData->ppCurKHeapFree != NULL &&
        (g_pCurKHeapFree > g_pKHeapBase) &&
        ((g_pCurKHeapFree - g_pKHeapBase) < MAX_KHEAP_SIZE))
    {
        sizeKHeap = g_pCurKHeapFree - g_pKHeapBase;
    }
    else
    {
        // WARNING: This will nearly degenerate the hash table into a simple linked list.
        sizeKHeap = MAX_KHEAP_SIZE;  
    }

    g_HashSize = (sizeKHeap + DDX_MAX_KERNEL_OBJECTS - 1) / DDX_MAX_KERNEL_OBJECTS;

    return TRUE;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL 
AddDiagnosis(
             CEDUMP_DIAGNOSIS_DESCRIPTOR* pDiagnosis
             )
{
    if (g_nDiagnoses >= DDX_MAX_DIAGNOSES)
    {
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"OsAxsT0: Diagnosis limit exceeded!\n"));
        return FALSE;
    }

    memcpy((void*)&g_ceDmpDiagnoses[g_nDiagnoses], pDiagnosis, sizeof(CEDUMP_DIAGNOSIS_DESCRIPTOR));
    g_nDiagnoses++;

    return TRUE;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int
AddAffectedProcess(
                   PPROCESS pProc
                   )
{
    return CacheKernelObject(DDX_AFFECTED_PROCESS, pProc);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int
AddAffectedThread(
                  PTHREAD pThread
                  )
{
    return CacheKernelObject(DDX_AFFECTED_THREAD, pThread);
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
IsKnownDiagnosis(
                 DDX_DIAGNOSIS_ID* pId
                 )
{
    if (pId)
    {

        /* DEBUGGERMSG(OXZONE_DIAGNOSE, (L"\r\n"));
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!IsKnownDiagnosis: Checking for ...\r\n"));
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!IsKnownDiagnosis: Type       = 0x%08x\r\n", pId->Type));
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!IsKnownDiagnosis: SubType    = 0x%08x\r\n", pId->SubType));
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!IsKnownDiagnosis: dwThreadId = 0x%08x\r\n", pId->dwThreadId));
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!IsKnownDiagnosis: dwData     = 0x%08x\r\n", pId->dwData));
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"\r\n"));*/

        for (int i = 0; i < g_nPersistentDiagnoses; i++)
        {

            /* DEBUGGERMSG(OXZONE_DIAGNOSE, (L"\r\n"));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!IsKnownDiagnosis: Comparing to (%i) ...\r\n", i));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!IsKnownDiagnosis:    Type       = 0x%08x\r\n", g_PersistentDiagnoses[i].Type));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!IsKnownDiagnosis:    SubType    = 0x%08x\r\n", g_PersistentDiagnoses[i].SubType));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!IsKnownDiagnosis:    dwThreadId = 0x%08x\r\n", g_PersistentDiagnoses[i].dwThread));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!IsKnownDiagnosis:    dwData     = 0x%08x\r\n", g_PersistentDiagnoses[i].dwData));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"\r\n"));*/

            if ((g_PersistentDiagnoses[i].Type       == pId->Type)       &&
                (g_PersistentDiagnoses[i].SubType    == pId->SubType)    &&
                (g_PersistentDiagnoses[i].dwThreadId == pId->dwThreadId) &&
                (g_PersistentDiagnoses[i].dwData     == pId->dwData))
            {
                DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!IsKnownDiagnosis: FOUND\r\n", i));
                return TRUE;
            }
        }
    }

    return FALSE;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL
AddPersistentDiagnosis(
                       DDX_DIAGNOSIS_ID* pId
                       )
{
    if (pId)
    {
        int index = g_nPersistentDiagnoses;

        if ((index >= 0) &&
            (index < DDX_MAX_PERSISTENT_DIAGNOSES))
        {
            g_PersistentDiagnoses[index].Type       = pId->Type;
            g_PersistentDiagnoses[index].SubType    = pId->SubType;
            g_PersistentDiagnoses[index].dwThreadId = pId->dwThreadId;
            g_PersistentDiagnoses[index].dwData     = pId->dwData;

            /*DEBUGGERMSG(OXZONE_DIAGNOSE, (L"\r\n"));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!AddPersistentDiagnosis: Adding diagnosis %i\r\n", g_nPersistentDiagnoses));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!AddPersistentDiagnosis: Type       = 0x%08x\r\n", pId->Type));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!AddPersistentDiagnosis: SubType    = 0x%08x\r\n", pId->SubType));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!AddPersistentDiagnosis: dwThreadId = 0x%08x\r\n", pId->dwThreadId));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  Diagnose!AddPersistentDiagnosis: dwData     = 0x%08x\r\n", pId->dwData));
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"\r\n"));*/

            g_nPersistentDiagnoses++;

            return TRUE;
        }
        else
        {
            // TODO: Should we wrap around??
        }
    }

    return FALSE;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
LPBYTE
DDxClearHeap(
             void
             )
{
    if (g_pDDxHeapBegin)
    {
        memset((void*) g_pDDxHeapBegin, 0x0, HEAP_SIZE);
        g_pDDxHeapFree = g_pDDxHeapBegin;
    }

    return g_pDDxHeapBegin;
}



//-----------------------------------------------------------------------------
//  Brain-dead heap
//
//  We allocate a chunk of kernel memory when the library is loaded (~1-2 pages).
//  We can use this block for dynamic allocations, since we cannot use system
//  heap code at break time.  However, "allocation" means nothing more than 
//  advancing the free address pointer.  
//
//  KEY POINTS:
//   +  There is no "free".  When we hit the end of the heap, we're done.  Plan allocs carefully!
//   +  Allocations are not persistent - the heap gets reset before every diagnosis.
//
//-----------------------------------------------------------------------------
LPBYTE
DDxAlloc(
         DWORD cbSize,
         DWORD dwFlags
         )
{
    LPBYTE p = NULL;

    if (!g_pDDxHeapFree)
        return NULL;

    if ((g_pDDxHeapFree + cbSize) < (g_pDDxHeapBegin + HEAP_SIZE))
    {
        // TODO: align
        p = g_pDDxHeapFree;
        g_pDDxHeapFree += cbSize;

        if (dwFlags & LMEM_ZEROINIT)
        {
            memset((void*)p, 0x0, cbSize);
        }
    }
    else
    {
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDx!DDxAlloc:  DDx internal heap full!\r\n"));
    }

    return p;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
UINT
GetNumProcesses (
                 void
                 )
{
    if (!g_numProcs)
    {
        PDLIST ptrav   = (PDLIST)g_pprcNK;

        while (ptrav)
        {
            g_numProcs++;
            ptrav = ptrav->pFwd; 

            if (ptrav == (PDLIST)g_pprcNK)
                break;
        }
    }

    return g_numProcs;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void
ResetDDx(
         void
         )
{
    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDx!ResettingDDx\r\n"));

    InitKernelObjectHashTable (0);

    memset((void*)&g_ceDmpDiagnosisList, 0x0, sizeof(CEDUMP_DIAGNOSIS_LIST));
    memset((void*)g_ceDmpDiagnoses, 0x0, sizeof(CEDUMP_DIAGNOSIS_DESCRIPTOR) * DDX_MAX_DIAGNOSES);
    memset((void*)&g_ceDmpDDxBucketParameters, 0x0, sizeof(CEDUMP_BUCKET_PARAMETERS));

    g_nDiagnoses = 0;
    g_numProcs   = 0;

    g_pszLogTail = g_pszLogHead;

    DDxClearHeap();
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL 
IsDDxEnabled(
             void
             ) 
{ 
    if(g_hProbeDll != NULL)
    {
        DWORD dwIsEnabled = FALSE;

        if (NKKernelLibIoControl(
            g_hProbeDll, 
            IOCTL_DDX_PROBE_IS_ENABLED, 
            NULL, 0, 
            (LPVOID) &dwIsEnabled, sizeof(DWORD),
            NULL))
        {
            return (dwIsEnabled != 0);
        } 
    }

    return FALSE;
}



/*++

Routine Description:


Arguments:


Return Value:

--*/
BOOL
Diagnose(
         DWORD ExceptionCode
         )
{
    DWORD dwDiagnosis = DDxResult_Inconclusive;
    static int n = 0;

    ResetDDx();

    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Diagnose!Diagnosing exception code = 0x%08x\r\n", ExceptionCode));

    switch (ExceptionCode)
    {
    case STATUS_NO_MEMORY:
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Diagnose!Diagnosing OOM ...\r\n"));
        dwDiagnosis = DiagnoseOOM(ExceptionCode);
        break;

    case STATUS_POSSIBLE_ABONDONED_SYNC:
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Diagnose!Diagnosing abandoned sync object ...\r\n"));
    case STATUS_POSSIBLE_DEADLOCK:
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Diagnose!Diagnosing deadlock ...\r\n"));
        dwDiagnosis = DiagnoseDeadlock();
        break;

    case STATUS_POSSIBLE_STARVATION:
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Diagnose!Diagnosing starvation ...\r\n"));
        dwDiagnosis = DiagnoseStarvation();
        break;

    case STATUS_HEAP_CORRUPTION:
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Diagnose!Diagnosing heap corruption ...\r\n"));
        dwDiagnosis = DiagnoseHeap(ExceptionCode);
        break;

    case STATUS_HANDLED_EX_HOLDING_SYNC_OBJECT:
        // TODO:
        break;

    case STATUS_STACK_OVERFLOW:
    case STATUS_STACK_OVERFLOW_WARNING:
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Diagnose!Diagnosing stack overflow ...\r\n"));
        dwDiagnosis = DiagnoseStackOverflow (ExceptionCode);
        break;

    case STATUS_ACCESS_VIOLATION:
    case STATUS_INTEGER_DIVIDE_BY_ZERO:

        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Diagnose!Diagnosing Exception = 0x%08x ...\r\n", ExceptionCode));
        dwDiagnosis = DiagnoseException(ExceptionCode);
        break;

    case STATUS_DATATYPE_MISALIGNMENT:
    case STATUS_ILLEGAL_INSTRUCTION:
    case STATUS_INTEGER_OVERFLOW:
    case STATUS_INSTRUCTION_MISALIGNMENT:
        // TODO: 
    default:
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Diagnose!Diagnosis: Unrecognized exception code = 0x%08x\r\n", ExceptionCode));
        dwDiagnosis = DDX_DIAGNOSIS_ERROR;
        break;
    }

    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"Diagnose!Diagnosis result = %d\r\n", dwDiagnosis));

    if (dwDiagnosis == DDxResult_Positive)
    {
        return TRUE;
    }

    // Clean up
    ResetDDx();

    return FALSE;
}
