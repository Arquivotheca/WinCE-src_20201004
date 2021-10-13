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

    kdhandle.c

Abstract:

    This module contains code to implement handle queries and deletions.

--*/
// C4200: nonstandard extension used : zero-sized array in struct/union
#pragma warning(disable:4200)

// C4228: nonstandard extension used : qualifiers after comma in declarator list are ignored
#pragma warning(disable:4228)

#include <cmnintrin.h>
#include "kdp.h"

// We pull in these from filesys.h
typedef struct fsopenfile_t
{
    DWORD filepos;
    struct fsopenfile_t *next;
    WORD flags;
    HANDLE hFile;
    HANDLE hNotify;
} fsopenfile_t;

#ifdef _DEBUG
#define CODE_NEVER_REACHED()        ASSERT(0)
#else
#define CODE_NEVER_REACHED()        __assume(0)
#endif /* _DEBUG */

// Declare a KD_HANDLE_DESC that describes a field inside a structure
// id = one of the KD_HDATA_xxx IDs
// t = one of the KD_FIELD_xxx type IDs
// s = structure name
// p = field name in above structure
#define DESC_OFF(id,t,s,p)          {NULL, offsetof(s, p), sizeof(((s *) NULL)->p), id, t}

// Declare KD_HANDLE_DESC for data returned by a callback function
// id = one of the KD_HDATA_xxx IDs
// t = one of the KD_FIELD_xxx type IDs
// f = function to call to get the data
#define DESC_FUN(id,t,f)            {f, 0, 0, id, t}

// Declare KD_API_DESC
// p = KD_API_DESC array (KD_API_DESC[], not KD_API_DESC*)
// f = function to validate extra handle data for this particular API
#define APIDESC(p,f)                {p, lengthof(p), f}

// Declare an empty KD_API_DESC
// p = KD_API_DESC array (KD_API_DESC[], not KD_API_DESC*)
// f = function to validate extra handle data for this particular API
//
// Note that p and f are ignored; this is used as a placeholder. Once the API
// has a description, change APINULL -> APIDESC macro and you're good to go.
#define APINULL(p,f)                {NULL, 0, KdDontValidate}

typedef struct _KD_HANDLE_DESC
{
    // For extremely special fields that are annoying to encode. If this is
    // NULL, we copy a 4-byte variable at the specified offset.
    UINT32 (* pfnReadField)(PCVOID pvData);

    size_t nOffset;
    size_t nSize;
    UINT16 nFieldId;
    UINT16 nType;
} KD_HANDLE_DESC;

typedef KD_HANDLE_DESC *PKD_HANDLE_DESC;
typedef const KD_HANDLE_DESC *PCKD_HANDLE_DESC;

typedef struct
{
    PCKD_HANDLE_DESC pDesc;
    UINT cEntries;
    BOOL (* pfnValidateHandle)(HDATA *ph);
} KD_API_DESC;

typedef KD_API_DESC *PKD_API_DESC;
typedef const KD_API_DESC *PCKD_API_DESC;

// Function to extract position of least-significant one
static UINT _CountTrailingZeros(UINT32 nMask);

// Function to convert a KD_HANDLE_DESC into a DBGKD_HANDLE_FIELD_DATA
static void KdHandleGetField(PDBGKD_HANDLE_FIELD_DATA pField, PCKD_HANDLE_DESC pDesc, LPCVOID pvStruct);

// Some handle validation helper functions
static BOOL KdDontValidate(HDATA *ph);
static BOOL KdValidateThread(HDATA *ph);
static BOOL KdValidateProcess(HDATA *ph);

// Some handle data helper functions
static UINT32 KdReadGlobalRefCount(PCVOID pvData);
static UINT32 KdReadHandleType(PCVOID pvData);
static UINT32 KdReadHandleName(PCVOID pvData);
static UINT32 KdReadThreadPID(PCVOID pvData);
static UINT32 KdReadMutexOwner(PCVOID pvData);

// Constants for _CountTrailingZeros emulation
#if !_INTRINSIC_IS_SUPPORTED(_CountLeadingZeros)
static const UINT32 g_nCntLeadMagic32 = 0xF04653AE;
static const BYTE g_abCntLeadTbl32[32] =
{0,  31, 4,  5,  6,  10, 7,  15, 11, 20, 8,  18, 16, 25, 12, 27,
 21, 30, 3,  9,  14, 19, 17, 24, 26, 29, 2,  13, 23, 28, 1,  22};
#endif /* !_INTRINSIC_IS_SUPPORTED(_CountLeadingZeros) */

// Describe the HDATA structure.
static const KD_HANDLE_DESC g_pGenericDesc[] =
{DESC_OFF(KD_HDATA_HANDLE,         KD_FIELD_HANDLE,   HDATA,      hValue),
 DESC_OFF(KD_HDATA_AKY,            KD_FIELD_BITS,     HDATA,      lock),
 DESC_FUN(KD_HDATA_REFCNT,         KD_FIELD_UINT,     KdReadGlobalRefCount),
 DESC_FUN(KD_HDATA_TYPE,           KD_FIELD_WIDE_STR, KdReadHandleType),
 DESC_FUN(KD_HDATA_NAME,           KD_FIELD_WIDE_STR, KdReadHandleName)};

// These descriptors correspond to handle types (_HDATA.pci->type)
//static const KD_HANDLE_DESC g_pWin32Desc[] = {};

static const KD_HANDLE_DESC g_pThreadDesc[] =
{DESC_OFF(KD_HDATA_THREAD_SUSPEND, KD_FIELD_UINT,     THREAD,       bSuspendCnt),
 DESC_FUN(KD_HDATA_THREAD_PID,     KD_FIELD_UINT,     KdReadThreadPID),
 DESC_OFF(KD_HDATA_THREAD_BPRIO,   KD_FIELD_UINT,     THREAD,       bBPrio),
 DESC_OFF(KD_HDATA_THREAD_CPRIO,   KD_FIELD_UINT,     THREAD,       bCPrio),
 DESC_OFF(KD_HDATA_THREAD_KTIME,   KD_FIELD_UINT,     THREAD,       dwKernTime),
 DESC_OFF(KD_HDATA_THREAD_UTIME,   KD_FIELD_UINT,     THREAD,       dwUserTime)};

static const KD_HANDLE_DESC g_pProcessDesc[] =
{DESC_OFF(KD_HDATA_PROC_PID,       KD_FIELD_UINT,     PROCESS,      procnum),
 DESC_OFF(KD_HDATA_PROC_TRUST,     KD_FIELD_UINT,     PROCESS,      bTrustLevel),
 DESC_OFF(KD_HDATA_PROC_VMBASE,    KD_FIELD_PTR,      PROCESS,      dwVMBase),
 DESC_OFF(KD_HDATA_PROC_BASEPTR,   KD_FIELD_PTR,      PROCESS,      BasePtr),
 DESC_OFF(KD_HDATA_PROC_CMDLINE,   KD_FIELD_WIDE_STR, PROCESS,      pcmdline)};

static const KD_HANDLE_DESC g_pEventDesc[] =
{DESC_OFF(KD_HDATA_EVENT_STATE,    KD_FIELD_BOOL,     EVENT,        state),
 DESC_OFF(KD_HDATA_EVENT_RESET,    KD_FIELD_BOOL,     EVENT,        manualreset)};

static const KD_HANDLE_DESC g_pMutexDesc[] =
{DESC_OFF(KD_HDATA_MUTEX_LOCKCNT,  KD_FIELD_UINT,     MUTEX,        LockCount),
 DESC_FUN(KD_HDATA_MUTEX_OWNER,    KD_FIELD_HANDLE,   KdReadMutexOwner)};

//static const KD_HANDLE_DESC g_pAPISetDesc[] = {};

//static const KD_HANDLE_DESC g_pFileDesc[] = {};
//static const KD_HANDLE_DESC g_pFindDesc[] = {};
//static const KD_HANDLE_DESC g_pDBFileDesc[] = {};
//static const KD_HANDLE_DESC g_pDBFindDesc[] = {};
//static const KD_HANDLE_DESC g_pSocketDesc[] = {};
//static const KD_HANDLE_DESC g_pInterfaceDesc[] = {};

static const KD_HANDLE_DESC g_pSemaphoreDesc[] =
{DESC_OFF(KD_HDATA_SEM_COUNT,      KD_FIELD_SINT,     SEMAPHORE,    lCount),
 DESC_OFF(KD_HDATA_SEM_MAXCOUNT,   KD_FIELD_SINT,     SEMAPHORE,    lMaxCount)};

//static const KD_HANDLE_DESC g_pFSMapDesc[] = {};
//static const KD_HANDLE_DESC g_pWNetEnumDesc[] = {};

// NULL API set (no API-specific data). This is useful for simplifying the code
// later on.
const KD_API_DESC g_descNull = {NULL, 0, KdDontValidate};

static BOOL KdDontValidate(HDATA *ph);
static BOOL KdValidateThread(HDATA *ph);
static BOOL KdValidateProcess(HDATA *ph);

// APINULL macros correspond to unimplemented descriptors
KD_API_DESC g_pTypeMap[] =
{APINULL(g_pWin32Desc,     KdDontValidate),     // 0  = SH_WIN32
 APIDESC(g_pThreadDesc,    KdValidateThread),   // 1  = SH_CURTHREAD
 APIDESC(g_pProcessDesc,   KdValidateProcess),  // 2  = SH_CURPROC
 APINULL(g_pKWin32Desc,    KdDontValidate),     // 3  = SH_KWIN32
 APIDESC(g_pEventDesc,     KdDontValidate),     // 4  = HT_EVENT
 APIDESC(g_pMutexDesc,     KdDontValidate),     // 5  = HT_MUTEX
 APINULL(g_pAPISetDesc,    KdDontValidate),     // 6  = HT_APISET
 APINULL(g_pFileDesc,      KdDontValidate),     // 7  = HT_FILE
 APINULL(g_pFindDesc,      KdDontValidate),     // 8  = HT_FIND
 APINULL(g_pDBFileDesc,    KdDontValidate),     // 9  = HT_DBFILE
 APINULL(g_pDBFindDesc,    KdDontValidate),     // 10 = HT_DBFIND
 APINULL(g_pSocketDesc,    KdDontValidate),     // 11 = HT_SOCKET
 APINULL(g_pInterfaceDesc, KdDontValidate),     // 12 = HT_INTERFACE
 APIDESC(g_pSemaphoreDesc, KdDontValidate),     // 13 = HT_SEMAPHORE
 APINULL(g_pFSMapDesc,     KdDontValidate),     // 14 = HT_FSMAP
 APINULL(g_pWNetEnumDesc,  KdDontValidate),     // 15 = HT_WNETENUM
};

static LPCWSTR g_apszHandleTypeNames[] =
{L"Win32",
 L"Thread",
 L"Process",
 L"[Unused]",
 L"Event",
 L"Mutex",
 L"API set",
 L"File",
 L"Find",
 L"DB File",
 L"DB Find",
 L"Socket",
 L"Interface",
 L"Semaphore",
 L"FS Map",
 L"WNet Enum",
};

/*++

Routine Name:

    KdHandleToPtr

Routine Description:

    This routine converts a Windows CE HANDLE into the associated HDATA
    structure.

Arguments:

    hHandle         - [in]     Handle to convert

Return Value:

    If the handle is invalid, this routine returns NULL. Otherwise it returns
    a pointer to the HDATA structure for the handle.

--*/
HDATA *KdHandleToPtr(HANDLE hHandle)
{
    HDATA *phCurHandle;

    for(phCurHandle = (HDATA *) pHandleList->linkage.fwd;
        phCurHandle != pHandleList;
        phCurHandle = (HDATA *) phCurHandle->linkage.fwd)
    {
        if (phCurHandle->hValue == hHandle)
        {
            DEBUGGERMSG(KDZONE_HANDLEEX, (L"KdHandleToPtr: Handle %08p = HDATA@%08p\r\n", hHandle, phCurHandle));
            return phCurHandle;
        }
    }

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"KdHandleToPtr: Handle %08p not valid\r\n", hHandle));

    return NULL;
}

/*++

Routine Name:

    KdValidateHandle

Routine Description:

    This routine verifies that a handle is valid.

Arguments:

    hHandle         - [in]     Handle to check

Return Value:

    TRUE    Handle is valid
    FALSE   Handle is invalid

--*/
BOOL KdValidateHandle(HANDLE hHandle)
{
    HDATA *phCurHandle;
    UINT nType;

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"KdValidateHandlePtr: Validating handle %08p\r\n", hHandle));

    phCurHandle = KdHandleToPtr(hHandle);
    if (phCurHandle == NULL)
        return FALSE;

    nType = phCurHandle->pci->type;
    if (nType < lengthof(g_pTypeMap) &&
        !g_pTypeMap[nType].pfnValidateHandle(phCurHandle))
        return FALSE;

    return TRUE;
}

/*++

Routine Name:

    KdValidateHandlePtr

Routine Description:

    This routine verifies that a pointer to a handle is valid.

Arguments:

    phHandle        - [in]     Handle to check

Return Value:

    TRUE    Handle is valid
    FALSE   Handle is invalid

--*/
BOOL KdValidateHandlePtr(HDATA *phHandle)
{
    HDATA *phCurHandle;
    UINT nType;

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"KdValidateHandlePtr: Validating handle pointer %08p\r\n", phHandle));

    for(phCurHandle = (HDATA *) pHandleList->linkage.fwd;
        phCurHandle != pHandleList;
        phCurHandle = (HDATA *) phCurHandle->linkage.fwd)
    {
        if (phCurHandle == phHandle)
            break;
    }

    if (phCurHandle == pHandleList)
        return FALSE;

    nType = phHandle->pci->type;
    if (nType < lengthof(g_pTypeMap) &&
        !g_pTypeMap[nType].pfnValidateHandle(phHandle))
        return FALSE;

    return TRUE;
}

/*++

Routine Name:

    KdGetProcHandleRef

Routine Description:

    This routine obtains the reference count for a particular process on an
    arbitrary handle.

Arguments:

    hHandle         - [in]     Handle to check
    nPID            - [in]     Process ID to obtain reference count for

Return Value:

    Number of references or -1 on invalid input

--*/
UINT KdGetProcHandleRef(HDATA *phHandle, UINT nPID)
{
    if (nPID >= MAX_PROCESSES)
        return -1;
    else if (phHandle->ref.count >= 0x10000)
        return phHandle->ref.pFr->usRefs[nPID];
    else if ((phHandle->lock & (1 << nPID)) != 0)
        return phHandle->ref.count;
    else
        return 0;
}

/*++

Routine Name:

    KdQueryHandleFields

Routine Description:

    This routine traverses the kernel handle list. It returns in the buffer a
    list of handles matching specific APIs or owned by specific processes.

Arguments:

    pHandleFields   - [in/out] List of common fields for the handles
    cbBufLen        - [in]     Length of pHandleFields buffer in bytes

    The caller should fill out the "in" structure in pHandleFields according to
    its description. The "out" structure is used to return results. See kdp.h
    for more information about how this structure is used. If the length of the
    buffer indicates that pHandleFields.out.nFields is valid, it will always
    contain the number of fields on return. If the buffer is too small to hold
    all data available, STATUS_BUFFER_TOO_SMALL will be returned and the buffer
    will be filled with part of the data. For any other error, the contents of
    pHandleFields is undefined.

Return Value:

    STATUS_SUCCESS                  Success
    STATUS_INVALID_PARAMETER        Buffer lacks space for in/out headers
    STATUS_BUFFER_TOO_SMALL         Buffer is not large enough

--*/
NTSTATUS KdQueryHandleFields(PDBGKD_HANDLE_DESC_DATA pHandleFields,
                             UINT cbBufLen)
{
    const UINT32 nAPIFilter = pHandleFields->in.nAPIFilter;
    NTSTATUS status = STATUS_SUCCESS;
    PCKD_API_DESC pAPIDesc;
    UINT i, j, cFields, cBufFields;

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"++KdQueryHandleFields\r\n"));

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"  PID Mask: %08lX, API Mask: %08lX\r\n",
                pHandleFields->in.nPIDFilter, pHandleFields->in.nAPIFilter));

    if (cbBufLen < sizeof(*pHandleFields))
    {
        status = STATUS_INVALID_PARAMETER;
        goto CommonExit;
    }

    // Compute number of fields buffer has space for
    cBufFields = (cbBufLen - sizeof(pHandleFields->out)) / sizeof(DBGKD_HANDLE_FIELD_DESC);

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"  Buffer (addr=%08p, length=%u) has room for %u fields\r\n", pHandleFields, cbBufLen, cBufFields));

    // If we're only looking at a single API, add some extra fields
    if ((nAPIFilter & (nAPIFilter - 1)) == 0)
        pAPIDesc = &g_pTypeMap[_CountTrailingZeros(nAPIFilter)];
    else
        pAPIDesc = &g_descNull;

    cFields = lengthof(g_pGenericDesc) + pAPIDesc->cEntries;
    if (cBufFields > cFields)
        cBufFields = cFields;
    else if (cBufFields < cFields)
        status = STATUS_BUFFER_TOO_SMALL;

    pHandleFields->out.cFields = cBufFields;

    // Copy over fields
    j = 0;
    for(i = 0; i < lengthof(g_pGenericDesc) && cBufFields > 0; i++, j++, cBufFields--)
    {
        pHandleFields->out.pFieldDesc[j].nType = g_pGenericDesc[i].nType;
        pHandleFields->out.pFieldDesc[j].nFieldId = g_pGenericDesc[i].nFieldId;
    }

    for(i = 0; i < pAPIDesc->cEntries && cBufFields > 0; i++, j++, cBufFields--)
    {
        pHandleFields->out.pFieldDesc[j].nType = pAPIDesc->pDesc[i].nType;
        pHandleFields->out.pFieldDesc[j].nFieldId = pAPIDesc->pDesc[i].nFieldId;
    }

CommonExit:
    DEBUGGERMSG(KDZONE_HANDLEEX, (L"--KdQueryHandleFields\r\n"));

    return status;
}

/*++

Routine Name:

    KdQueryOneHandle

Routine Description:

    This routine fetches information for a specific handle. To determine which
    fields will be present in the data, use KdQueryHandleFields with pid=-1 and
    api=1 << handle->pci->type.

Arguments:

    hHandle         - [in]     Handle to query
    pHandleBuffer   - [out]    Pointer to results buffer
    nBufLen         - [in]     Length of pHandleBuffer buffer in bytes

    Unlike other routines, this one does not expect the caller to fill out the
    in structure in pHandleBuffer.

Return Value:

    STATUS_SUCCESS                  Success
    STATUS_INVALID_PARAMETER        hHandle is invalid
    STATUS_BUFFER_TOO_SMALL         Insufficient buffer space

--*/
NTSTATUS KdQueryOneHandle(HANDLE hHandle, PDBGKD_HANDLE_GET_DATA pHandleBuffer, UINT nBufLen)
{
    HDATA *phHandle = KdHandleToPtr(hHandle);
    PCKD_API_DESC pAPIDesc;
    UINT cFields, nMinLen;

    if (phHandle == NULL)
        return STATUS_INVALID_PARAMETER;

    pAPIDesc = &g_pTypeMap[phHandle->pci->type];
    cFields = lengthof(g_pGenericDesc) + pAPIDesc->cEntries;
    nMinLen = sizeof(pHandleBuffer->out) + (sizeof(DBGKD_HANDLE_FIELD_DATA) * cFields);
    if (nBufLen < nMinLen)
        return STATUS_BUFFER_TOO_SMALL;

    // This is a little tricky. The API filter causes handle-specific fields to
    // show up. We don't need a PID filter. Limiting the buffer size causes
    // only one handle to be returned. Setting the start point to the handle we
    // want information on causes it to get information for our specific
    // handle.
    pHandleBuffer->in.nPIDFilter = -1;
    pHandleBuffer->in.nAPIFilter = 1 << phHandle->pci->type;
    pHandleBuffer->in.hStart = (HANDLE) phHandle;
    return KdQueryHandleList(pHandleBuffer, nMinLen);
}

/*++

Routine Name:

    KdQueryHandleList

Routine Description:

    This routine traverses the kernel handle list. It returns in the buffer a
    list of handles matching specific APIs or owned by specific processes.

Arguments:

    pHandleBuffer   - [in/out] Pointer to results buffer
    nBufLen         - [in]     Length of pHandleBuffer buffer in bytes

    The caller should fill out the "in" structure in pHandleBuffer according to
    its description. The "out" structure is used to return results. See kdp.h
    for more information about how this structure is used. If an error is
    returned, the contents of pHandleBuffer are undefined.

    The actual buffer size is allowed to vary, so use nBufLen to indicate the
    size of pHandleBuffer (including headers).

Return Value:

    STATUS_SUCCESS                  Success
    STATUS_INVALID_PARAMETER        pHandleBuffer.in.hStart is invalid

--*/
NTSTATUS KdQueryHandleList(PDBGKD_HANDLE_GET_DATA pHandleBuffer, UINT nBufLen)
{
    HDATA *pCurHandle;
    PDBGKD_HANDLE_FIELD_DATA pCurField;
    PCKD_API_DESC pAPIDesc;
    NTSTATUS status;
    HDATA hTemp;
    UINT32 nPIDFilter, nAPIFilter;
    UINT i, j, cBufFields;

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"++KdQueryHandleList\r\n"));

    // Not having buffer space for the headers is an error. We must have enough
    // space for both input and output data!
    if (nBufLen < sizeof(*pHandleBuffer))
    {
        status = STATUS_INVALID_PARAMETER;
        goto CommonExit;
    }

    // Save some input parameters and zero the buffer
    pCurHandle = (HDATA *) pHandleBuffer->in.hStart;
    nPIDFilter = pHandleBuffer->in.nPIDFilter;
    nAPIFilter = pHandleBuffer->in.nAPIFilter;
    memset(pHandleBuffer, 0, nBufLen);

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"  PID Mask: %08lX, API Mask: %08lX, Start=%08p\r\n",
                nPIDFilter, nAPIFilter, pCurHandle));

    // Compute number of DBGKD_HANDLE_FIELD_DATA entries in the buffer
    cBufFields = (nBufLen - sizeof(pHandleBuffer->out)) / sizeof(DBGKD_HANDLE_FIELD_DATA);

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"  Buffer (addr=%08p, length=%u) has room for %u fields\r\n", pHandleBuffer, nBufLen, cBufFields));

    // NULL handle means start at the beginning
    if (pCurHandle == NULL)
        pCurHandle = (HDATA *) pHandleList->linkage.fwd;

    // Test the waters...
    if (KdpMoveMemory(&hTemp, pCurHandle, sizeof(HDATA)) < sizeof(HDATA))
    {
        status = STATUS_INVALID_PARAMETER;
        goto CommonExit;
    }

    pHandleBuffer->out.cFields = 0;

    for(i = 0; i < cBufFields; i++)
        pHandleBuffer->out.pFields[i].fValid = 0;

    // Seems like a valid pointer, so lets start blasting data.
    pCurField = pHandleBuffer->out.pFields;
    for(; pCurHandle != pHandleList; pCurHandle = (HDATA *) pCurHandle->linkage.fwd)
    {
        // Verify that handle matches both filters
        if ((nPIDFilter & pCurHandle->lock) == 0 ||
            (nAPIFilter & (1 << pCurHandle->pci->type)) == 0)
            continue;

        // Cache description of handle-specific data. If we're not doing
        // handle-specific data, cache the NULL descriptor to simplify the
        // processing code.
        if ((nAPIFilter & (nAPIFilter - 1)) == 0 &&
            pCurHandle->pci->type < lengthof(g_pTypeMap))
            pAPIDesc = &g_pTypeMap[pCurHandle->pci->type];
        else
            pAPIDesc = &g_descNull;

        // If we don't have enough space to process this handle, we'll come
        // back to it.
        if (cBufFields < (lengthof(g_pGenericDesc) + pAPIDesc->cEntries))
            break;

        for(i = 0, j = 0; i < lengthof(g_pGenericDesc); i++, j++)
            KdHandleGetField(&pCurField[j], &g_pGenericDesc[i], pCurHandle);

        // Skip app-specific data. This happens for instance on threads
        // that die. The pointer is no longer valid, but the handle doesn't
        // disappear while apps have a reference to it.
        if (pAPIDesc->pfnValidateHandle(pCurHandle))
        {
            for(i = 0; i < pAPIDesc->cEntries; i++, j++)
                KdHandleGetField(&pCurField[j], &pAPIDesc->pDesc[i], pCurHandle->pvObj);
        }

        // Now update buffer & length
        i = (lengthof(g_pGenericDesc) + pAPIDesc->cEntries);
        cBufFields -= i;
        pCurField += i;
        pHandleBuffer->out.cFields += i;
    }

    // We don't want to traverse the list all over again!
    if (pCurHandle == pHandleList)
        pCurHandle = NULL;

    // We broke out on the current handle, so we should resume
    pHandleBuffer->out.hContinue = (HANDLE) pCurHandle;

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"  Next Handle: %08p\r\n", pCurHandle));

    status = STATUS_SUCCESS;

CommonExit:
    DEBUGGERMSG(KDZONE_HANDLEEX, (L"--KdQueryHandleList\r\n"));

    return status;
}

/*++

Routine Name:

    _CountTrailingZeros

Routine Description:

    This is a clever function to extract the zero-based index of the
    least-significant one bit in nMask. If we ever get an intrinsic for this,
    this function should go away promptly.

Arguments:

    nMask           - [in]     Bitmask to extract least-significant one from

Return Value:

    Zero-based index of the least-significant one

--*/
#pragma warning(push)
#pragma warning(disable:4146)
static UINT _CountTrailingZeros(UINT32 nMask)
{
#if !_INTRINSIC_IS_SUPPORTED(_CountLeadingZeros)
    nMask = (nMask & -nMask) - 1;
    return (UINT) g_abCntLeadTbl32[((g_nCntLeadMagic32 * nMask) >> 27) & 0x1F];
#else
    return ((sizeof(UINT32) * CHAR_BIT) - (_CountLeadingZeros(nMask & -nMask) + 1));
#endif /* !_INTRINSIC_IS_SUPPORTED(_CountLeadingZeros) */
}
#pragma warning(pop)

/*++

Routine Name:

    KdHandleGetField

Routine Description:

    This function interprets a KD_HANDLE_DESC and extracts the datum into a
    DBGKD_HANDLE_FIELD_DATA structure.

Arguments:

    pField          - [out]    Buffer that receives the datum
    pDesc           - [in]     Description of datum to read
    pvStruct        - [in]     Pointer to structure to read datum from

Return Value:

    Zero-based index of the least-significant one

--*/
static void KdHandleGetField(PDBGKD_HANDLE_FIELD_DATA pField, PCKD_HANDLE_DESC pDesc, LPCVOID pvStruct)
{
    UINT32 nData;
    BOOL fSigned;

    pField->nFieldId = pDesc->nFieldId;

    if (pDesc->pfnReadField != NULL)
    {
        pField->nData = pDesc->pfnReadField(pvStruct);
    }
    else
    {
        // move pointer to field
        pvStruct = (LPCVOID)((DWORD) pvStruct + pDesc->nOffset);

        fSigned = (pDesc->nType == KD_FIELD_SINT ? TRUE : FALSE);
        switch(pDesc->nSize)
        {
        case 1:
            if (fSigned)
                nData = (UINT32) *((signed __int8 *) pvStruct);
            else
                nData = (UINT32) *((unsigned __int8 *) pvStruct);

            break;
        case 2:
            if (fSigned)
                nData = (UINT32) *((signed __int16 *) pvStruct);
            else
                nData = (UINT32) *((unsigned __int16 *) pvStruct);

            break;
        case 4:
            if (fSigned)
                nData = (UINT32) *((signed __int32 *) pvStruct);
            else
                nData = (UINT32) *((unsigned __int32 *) pvStruct);

            break;
        default:
            CODE_NEVER_REACHED();
        }

        pField->nData = nData;
    }

    pField->fValid = 1;
}

static BOOL KdDontValidate(HDATA *ph)
{
    return TRUE;
}

static BOOL KdValidateThread(HDATA *ph)
{
    PROCESS *pProc;
    THREAD *pThread;
    UINT32 nRefKey;

    for(nRefKey = ph->lock; nRefKey != 0; nRefKey &= (nRefKey - 1))
    {
        pProc = &kdProcArray[_CountTrailingZeros(nRefKey)];

        for(pThread = pProc->pTh; pThread != NULL; pThread = pThread->pNextInProc)
        {
            if (pThread->hTh == ph->hValue)
                return TRUE;
        }
    }

    return FALSE;
}

static BOOL KdValidateProcess(HDATA *ph)
{
    UINT i;

    for(i = 0; i < MAX_PROCESSES; i++)
    {
        if (kdProcArray[i].hProc == ph->hValue)
            return TRUE;
    }

    return FALSE;
}

static UINT32 KdReadGlobalRefCount(HANDLE hHandle)
{
    const HDATA *pHandle = (const HDATA *) hHandle;
    UINT i, c;

    // typedef union REFINFO
    // {
    //     ulong count;
    //     FULLREF *pFr;
    // } REFINFO;
    //
    // When count < 0x10000, the handle is owned by 1 process and the count
    // corresponds to the reference count in that process. When
    // count >= 0x10000, the pFr field is used for reference counting.
    if (pHandle->ref.count < 0x10000)
        return pHandle->ref.count;

    // Sum up the number of references in each process
    c = 0;
    for(i = 0; i < MAX_PROCESSES; i++)
        c += pHandle->ref.pFr->usRefs[i];

    return c;
}

static UINT32 KdReadHandleType(PCVOID pvData)
{
    UINT nType = ((const HDATA *) pvData)->pci->type;

    if (nType == HT_CRITSEC)
        return (UINT32) L"Crit Sec";
    else if (nType == HT_MANUALEVENT)
        return (UINT32) L"Manual Event";
    else if (nType < lengthof(g_apszHandleTypeNames))
        return (UINT32) g_apszHandleTypeNames[nType];

    DEBUGGERMSG(KDZONE_HANDLEEX, (L"  KdReadHandleType: hHandle=%08lX, unknown type=%u\r\n", pvData, nType));

    return (UINT32) L"?";
}

static UINT32 KdReadHandleName(PCVOID pvData)
{
    const HDATA *phHandle = (const HDATA *) pvData;

    switch(phHandle->pci->type)
    {
    case HT_EVENT:
    {
        const EVENT *pEvent = (const EVENT *) phHandle->pvObj;
        return (UINT32)(pEvent->name == NULL ? NULL : pEvent->name->name);
    }
    case HT_MUTEX:
    {
        const MUTEX *pMutex = (const MUTEX *) phHandle->pvObj;
        return (UINT32)(pMutex->name == NULL ? NULL : pMutex->name->name);
    }
    case HT_SEMAPHORE:
    {
        const SEMAPHORE *pSem = (const SEMAPHORE *) phHandle->pvObj;
        return (UINT32)(pSem->name == NULL ? NULL : pSem->name->name);
    }
    case SH_CURPROC:
        return (UINT32)((PROCESS *) phHandle->pvObj)->lpszProcName;
    default:
        return (UINT32) L"";
    }
}

static UINT32 KdReadThreadPID(PCVOID pvData)
{
    return (UINT32)(((const THREAD *) pvData)->pOwnerProc->procnum);
}

static UINT32 KdReadMutexOwner(PCVOID pvData)
{
    return (UINT32)(((const MUTEX *) pvData)->pOwner->hTh);
}
