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

#ifdef KCOREDLL
#define WIN32_CALL(type, api, args) (*(type (*) args) g_ppfnWin32Calls[W32_ ## api])
#else
#define WIN32_CALL(type, api, args) IMPLICIT_DECL(type, SH_WIN32, W32_ ## api, args)
#endif

#include <windows.h>
#include <coredll.h>

#define ALIGNSIZE(x)            (((x) + 0xf) & ~0xf)


extern "C" BOOL xxx_ReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesRead);
extern "C" BOOL xxx_WriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress, LPVOID lpBuffer, DWORD nSize, LPDWORD lpNumberOfBytesWritten);
extern "C" HANDLE xxx_GetOwnerProcess(void);

//
// any buffer with size <= MARSHAL_CICO_SIZE are copied 
// in/out when passed into CeAllocAsynchronousBuffer 
// unless MARSHAL_FORCE_ALIAS flag is specified.
//
#define MARSHAL_CICO_SIZE       2*MAX_PATH

// Attach a header to heap allocations
typedef struct {
    HANDLE hSourceProcess;

#ifdef DEBUG
    // Extra information used to sanity-check on debug builds
    struct {
        PVOID  pOriginal;
        DWORD  cbOriginal;
        DWORD  ArgumentDescriptor;
        PVOID  pAliased;
        PVOID  pDuplicated;
    } debug;
#endif // DEBUG

} BufferHeader;
// Use ALIGNSIZE in order to guarantee the same allocation alignment that
// the heap code normally provides.
#define HEADER_SIZE     ALIGNSIZE(sizeof(BufferHeader))

#ifdef DEBUG

//
// array to hold all the LocalAlloc node addresses
// using fixed size arrays to avoid any contention
//
#define g_chMaxAllocList  1024
DWORD g_rgAllocList[g_chMaxAllocList];

//
// Add to the list of LocalAlloc nodes
//
BOOL AddToAllocList(LPCVOID pDuplicate)
{
    BufferHeader* pHeader = (BufferHeader*) ((DWORD)pDuplicate - HEADER_SIZE);
    for (int i = 0 ; i < g_chMaxAllocList; i++) {
        if (0 == InterlockedCompareExchangePointer((LONG*)&g_rgAllocList[i], (LONG)pHeader, 0))
            break;
    }
    return TRUE;
}

//
// Remove a node from the list of LocalAlloc nodes
//
BOOL RemoveFromAllocList(PVOID pDuplicate)
{
    BufferHeader* pHeader = (BufferHeader*) ((DWORD)pDuplicate - HEADER_SIZE);                
    for (int i = 0 ; i < g_chMaxAllocList; i++) {
        if (pHeader == InterlockedCompareExchangePointer((LONG*)&g_rgAllocList[i], 0, (LONG)pHeader))
            break;
    }
    return TRUE;
}

//
// validate a node in the list with the given parameters
//
BOOL ValidateNode(PVOID pDuplicate, PVOID pOriginal, DWORD cbSrc, DWORD ArgumentDescriptor)
{    
    BufferHeader* pHeader = (BufferHeader*) ((DWORD)pDuplicate - HEADER_SIZE);
    for (int i = 0 ; i < g_chMaxAllocList; i++) {
        if (g_rgAllocList[i] == (DWORD)pHeader) {
            BufferHeader* pNode = (BufferHeader*) g_rgAllocList[i];
            if ((pNode->debug.pOriginal != pOriginal)
                || (pNode->debug.ArgumentDescriptor != ArgumentDescriptor)
                || (pNode->debug.cbOriginal != cbSrc)) {
                //
                // This most likely means different arguments are passed to alloc and free/flush calls
                // Check the arguments to the calls to CeAllocAsynchronousBuffer/CeFreeAsynchronousBuffer
                //
                DEBUGCHK(0);
                break;
            }
        }
    }
    
    return TRUE;
}

//
// make a r/0 alias to a given buffer
//
BOOL AllocReadOnlyCopy(PVOID* ppDestMarshalled, DWORD cbSrc)
{
    BOOL fRet = FALSE;
    
    // in debug builds to catch if the server is writing to the shared heap address,
    // make a r/o virtual copy to the duplicated buffer and return that to the caller.
    BufferHeader* pHeader = (BufferHeader*) ((DWORD)*ppDestMarshalled - HEADER_SIZE);
    pHeader->debug.pAliased = VirtualAllocCopyEx(hActiveProc, hActiveProc, pHeader, cbSrc + HEADER_SIZE, PAGE_READONLY); 
    if (pHeader->debug.pAliased) {
        *ppDestMarshalled = (PVOID)((DWORD)pHeader->debug.pAliased + HEADER_SIZE);
        fRet = TRUE;
        // don't write to *ppDestMarshalled after this as it is marked as r/o
    }    

    return fRet;   
}

//
// remove the r/o alias on a given buffer
//
PVOID FreeReadOnlyCopy(PVOID pDestMarshalled)
{
    // pHeader is pointing to the start of the read only page
    // get the read-write buffer from this (pDuplicated)
    BufferHeader* pHeader = (BufferHeader*) ((DWORD)pDestMarshalled - HEADER_SIZE);
    PVOID pDuplicate = pHeader->debug.pDuplicated;
    DEBUGCHK(pDuplicate);   

    // pAliased can be NULL if this buffer was created with ForceDuplicate flag
    // in CeOpenCallerBuffer.
    PVOID pAliased = pHeader->debug.pAliased;
    
    // any updates to header should happen through the duplicated
    // pointer as aliased pointer is marked as read only!
    pHeader = (BufferHeader*) ((DWORD)pDuplicate - HEADER_SIZE);
    pHeader->debug.pAliased = NULL;    
    pHeader->debug.pDuplicated = NULL;

    // in debug builds we made an additional virtual copy to a r/o page
    // free it first before deleting the duplcated buffer.
    if (pAliased) {
        DEBUGCHK(VirtualFreeEx(hActiveProc, (LPVOID)((DWORD)pAliased & ~VM_BLOCK_OFST_MASK), 0, MEM_RELEASE, 0));
        // do not use pHeader after this
    }

    return pDuplicate;   
}

#endif // DEBUG

#ifdef KCOREDLL

static LPVOID
MapAsyncPtr (
    LPVOID ptr,
    DWORD  cbSize,
    BOOL   fWrite
    )
{
    HANDLE hCallerProcess = (HANDLE) GetCallerVMProcessId ();
    return ((DWORD) ptr < VM_SHARED_HEAP_BASE)
            ? VirtualAllocCopyEx (hCallerProcess, hActiveProc, ptr, cbSize, fWrite? PAGE_READWRITE : PAGE_READONLY)
            : ptr;
}

static
BOOL
UnmapAsyncPtr (
    LPVOID  ptrOrig,
    LPVOID  ptrMapped,
    DWORD   cbSize
    )
{
    // VirtualFree matches desktop behavior now - MEM_RELASE doesn't require all pages 
    // to be committed or de-committed.
    return (ptrOrig != ptrMapped) ? VirtualFreeEx (hActiveProc, (LPBYTE) ((DWORD) ptrMapped & ~VM_BLOCK_OFST_MASK), 0, MEM_RELEASE, 0) : TRUE;
}

#endif // KCOREDLL

// Special copy-in functionality for a string whose length is unknown.
// Works on both ANSI and Unicode strings.
static _inline HRESULT
AllocDuplicateString(
    PVOID* ppDuplicate,         // Receives the new heap buffer
    PVOID  pOriginal,           // Local pointer or pointer inside the caller process
    BOOL   fUnicode             // TRUE=Unicode, FALSE=ANSI
    )
{
    BufferHeader* pHeader;
    HANDLE hCallerProcess = (HANDLE) GetCallerVMProcessId ();
    DWORD  cbToRead;
    DWORD  cbPrevRead;  // Bytes that have been read and are known to not contain a NULL

    // Reading a byte at a time from the other process' memory would be
    // extremely expensive.  Reading too large of a chunk can waste work and
    // cause unnecessary page faults inside the other process.  So do reads
    // up to a page in size, without crossing any page boundaries until
    // necessary.
    
    cbPrevRead = 0;
    cbToRead = VM_PAGE_SIZE - ((DWORD)pOriginal & (VM_PAGE_SIZE-1));
    while (IsValidUsrPtr(pOriginal, cbToRead, FALSE)) {
        // Allocate the buffer plus a header
        pHeader = (BufferHeader*) LocalAlloc(LMEM_FIXED, cbToRead + HEADER_SIZE);
        if (pHeader && ((DWORD)((DWORD)pHeader + HEADER_SIZE) == (DWORD)pOriginal)) {
            // try to re-allocate; otherwise marshaling helper functions
            // cannot distinguish between source and destination pointers
            // on calls to FreeDuplicateBuffer().
            BufferHeader* pHeader2 = (BufferHeader*) LocalAlloc(LMEM_FIXED, cbToRead + HEADER_SIZE);
            DEBUGCHK((DWORD)pOriginal != ((DWORD)pHeader2 + HEADER_SIZE));
            LocalFree(pHeader);
            pHeader = pHeader2;
        }

        if (pHeader) {
            // Put the allocation after the header to minimize corruption opportunity
            LPVOID pDuplicate = (LPBYTE)pHeader + HEADER_SIZE;

            // Assign the header information
            pHeader->hSourceProcess = hCallerProcess;

#ifdef DEBUG
            // Extra information used to sanity-check on debug builds
            pHeader->debug.pOriginal = pOriginal;
            pHeader->debug.cbOriginal = 0;
            pHeader->debug.ArgumentDescriptor = fUnicode ? ARG_I_WSTR : ARG_I_ASTR;
#endif // DEBUG

            // Perform copy-in operation up to the end of the page (ReadProcessMemory never 
            //          generate exception.
            BOOL CopyInSucceeded = xxx_ReadProcessMemory(pHeader->hSourceProcess,
                                                        pOriginal, pDuplicate,
                                                        cbToRead, NULL);

            if (!CopyInSucceeded) {
                VERIFY( !LocalFree(pHeader) );
                return E_ACCESSDENIED;
            }

            // Now we have to figure out if we've reached the end of the string
            // by detecting if there is a NULL in the buffer we just read.
            size_t cchMax;
            if (fUnicode) {
                // Only check the part that hasn't already been checked
                PCWSTR psz = (PCWSTR) ((LPBYTE) pDuplicate + cbPrevRead);
                cchMax = (cbToRead - cbPrevRead) / sizeof(WCHAR);
                while (cchMax && (*psz != L'\0')) {
                    psz++;
                    cchMax--;
                }
            } else {
                // Only check the part that hasn't already been checked
                PCSTR psz = (PCSTR) ((LPBYTE) pDuplicate + cbPrevRead);
                cchMax = (cbToRead - cbPrevRead) / sizeof(CHAR);
                while (cchMax && (*psz != '\0')) {
                    psz++;
                    cchMax--;
                }
            }
            if (cchMax) {
                // Hit a NULL -- we have successfully duplicated the whole string
                *ppDuplicate = pDuplicate;
                return S_OK;
            }

            // Haven't reached the end of the string.  We could do a LocalReAlloc
            // and hold onto the string we've copied so far, but it is approximately
            // the same amount of work to just re-read.  So, discard what we've
            // copied so far and try again with one additional page.
            VERIFY( !LocalFree(pHeader) );
            pHeader = NULL;
            
            cbPrevRead = cbToRead;
            cbToRead += VM_PAGE_SIZE;

        } else {
            return E_OUTOFMEMORY;
        }
    }

    return E_ACCESSDENIED;
}


// Validate the buffer size and type.  Get the size if it's a string and if the
// string is accessible.
static DWORD
GetBufferSize(
    PVOID  pSrcUnmarshalled,
    DWORD  cbSrc,
    DWORD  ArgumentDescriptor,
    BOOL   IsSourceLocal        // TRUE if the source can be accessed.
                                // FALSE if the source has not been marshalled.
    )
{
    switch (ArgumentDescriptor & ~MARSHAL_FORCE_ALIAS) {
    case ARG_I_PTR:
    case ARG_O_PTR:
    case ARG_IO_PTR:
        return cbSrc;

    case ARG_O_PDW:
    case ARG_IO_PDW:
        if (sizeof(DWORD) == cbSrc) {
            return cbSrc;
        } else {
            return 0;
        }
    
    case ARG_O_PI64:
    case ARG_IO_PI64:
        if (sizeof(LARGE_INTEGER) == cbSrc) {
            return cbSrc;
        } else {
            return 0;
        }

    case ARG_I_WSTR:
        if (cbSrc) {
            return cbSrc;
        } else if (IsSourceLocal 
                       && 
                       (
#ifdef KCOREDLL
                            // no need to check if the call is from one kernel mode server to other
                            (GetDirectCallerProcessId() == GetCurrentProcessId()) ||
#endif                                
                            // Validate 1 byte since the string cannot extend into kernel space after that
                            IsValidUsrPtr(pSrcUnmarshalled, sizeof(BYTE), FALSE))) {
            // The source is accessible, so get the string length directly
            size_t cchSrc = 0;
            __try {
                if (SUCCEEDED(StringCchLengthW((PCWSTR) pSrcUnmarshalled, STRSAFE_MAX_CCH, &cchSrc))) {
                    cchSrc ++; // Account for NULL termination.
                } else {
                    cchSrc = 0;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                cchSrc = 0;
            }
            return (cchSrc * sizeof(WCHAR));
        } else {
            // The string must be marshalled BEFORE its length can be known
            return 0;
        }
    
    case ARG_I_ASTR:
        if (cbSrc) {
            return cbSrc;
        } else if (IsSourceLocal
                       && 
                       (
#ifdef KCOREDLL
                            // no need to check if the call is from one kernel mode server to other
                            (GetDirectCallerProcessId() == GetCurrentProcessId()) ||
#endif                                
                            // Validate 1 byte since the string cannot extend into kernel space after that
                            IsValidUsrPtr(pSrcUnmarshalled, sizeof(BYTE), FALSE))) {
            // The source is accessible, so get the string length directly
            size_t cchSrc = 0;
            __try {
                if (SUCCEEDED(StringCchLengthA((PCSTR) pSrcUnmarshalled, STRSAFE_MAX_CCH, &cchSrc))) {
                    cchSrc ++; // Account for NULL termination.
                } else {
                    cchSrc = 0;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                cchSrc = 0;
            }
            return (cchSrc * sizeof(CHAR));
        } else {
            // The string must be marshalled BEFORE its length can be known
            return 0;
        }
    
    default:
        ERRORMSG(1, (TEXT("Attempted to marshal invalid buffer type %u!\r\n"),
                     ArgumentDescriptor));
        break;
    }
    
    return 0;
}


// Allocate a heap buffer and perform any required copy-in without throwing any
// exceptions.
static _inline HRESULT
AllocDuplicateBuffer(
    PVOID* ppDuplicate,         // Receives the new heap buffer
    PVOID  pOriginal,           // Local pointer or pointer inside the caller process
    DWORD  cbOriginal,
    DWORD  cbSrcOriginal,
    DWORD  ArgumentDescriptor,
    BOOL   IsOriginalLocal      // TRUE: pOriginal is local
                                // FALSE: pOriginal is from caller process
    )
{
    // Allocate the buffer plus the header
    if (ULONG_MAX - HEADER_SIZE < cbOriginal) {
        return E_INVALIDARG;
    }
    BufferHeader* pHeader = (BufferHeader*) LocalAlloc(LMEM_FIXED, cbOriginal + HEADER_SIZE);
    if (pHeader && ((DWORD)((DWORD)pHeader + HEADER_SIZE) == (DWORD)pOriginal)) {
        // try to re-allocate; otherwise marshaling helper functions
        // cannot distinguish between source and destination pointers
        // on calls to FreeDuplicateBuffer().        
        BufferHeader* pHeader2 = (BufferHeader*) LocalAlloc(LMEM_FIXED, cbOriginal + HEADER_SIZE);
        DEBUGCHK((DWORD)pOriginal != ((DWORD)pHeader2 + HEADER_SIZE));
        LocalFree(pHeader);
        pHeader = pHeader2;
    }
    
    if (pHeader) {
        // Put the allocation after the header to minimize corruption opportunity
        PVOID pDuplicate = (LPBYTE)pHeader + HEADER_SIZE;
        
        // Assign the header information
        pHeader->hSourceProcess = (HANDLE) GetCallerVMProcessId ();

#ifdef DEBUG
        // Extra information used to sanity-check on debug builds
        pHeader->debug.pOriginal = pOriginal;
        pHeader->debug.cbOriginal = cbSrcOriginal;
        pHeader->debug.ArgumentDescriptor = ArgumentDescriptor;
        pHeader->debug.pAliased = NULL;
        pHeader->debug.pDuplicated = pDuplicate;
#endif // DEBUG

        // Perform copy-in operation if required.
        // Copy-in even on OUT-only args since we shouldn't overwrite existing memory
        // if the server call returns without writing to the whole buffer.
        if (ArgumentDescriptor & (ARG_I_BIT | ARG_O_BIT)) {
            BOOL CopyInSucceeded = FALSE;

#ifdef KCOREDLL
    
            CopyInSucceeded = CeSafeCopyMemory(pDuplicate, pOriginal, cbOriginal);

#else  // KCOREDLL
    
            if (IsOriginalLocal) {
                // Local ptr
                CopyInSucceeded = CeSafeCopyMemory(pDuplicate, pOriginal, cbOriginal);
            } else {
                // Caller ptr (ReadProcessMemory never generates exception)
                CopyInSucceeded = xxx_ReadProcessMemory(pHeader->hSourceProcess,
                                                            pOriginal, pDuplicate,
                                                            cbOriginal, NULL);
            }

#endif // KCOREDLL
            
            if (!CopyInSucceeded) {
                VERIFY( !LocalFree(pHeader) );
                return E_ACCESSDENIED;
            }
        }

        // Successfully duplicated
        *ppDuplicate = pDuplicate;        
        return S_OK;
    }
    
    return E_OUTOFMEMORY;
}


// Perform any required copy-in or copy-out.
static _inline HRESULT
FlushDuplicateBuffer(
    PVOID  pDuplicate,          // The duplicate heap buffer
    PVOID  pOriginal,           // Local pointer or pointer inside the caller process
    DWORD  cbOriginal,
    DWORD  ArgumentDescriptor,
    BOOL   IsOriginalLocal      // TRUE: pOriginal is local
                                // FALSE: pOriginal is from caller process
    )
{
    HRESULT result = S_OK;
    BufferHeader* pHeader = (BufferHeader*) ((DWORD)pDuplicate - HEADER_SIZE);

    // Perform copy-out operation if required
    if (ArgumentDescriptor & ARG_O_BIT) {
        if (cbOriginal) {
            // Kernel-mode only uses WriteProcessMemory if the flush is asynchronous,
            // while user-mode always does unless it's a simple DuplicateBuffer
            if (
#ifdef KCOREDLL
                (((HANDLE) GetCallerVMProcessId () == pHeader->hSourceProcess) // Original caller's VM is currently active
                && ((DWORD)xxx_GetOwnerProcess() != GetCurrentProcessId()))    // AND the call is synchronous from a user thread
#else  // KCOREDLL
                (IsOriginalLocal)
#endif // KCOREDLL
                )
            {
                // Local ptr
                if (!CeSafeCopyMemory(pOriginal, pDuplicate, cbOriginal)) {
                    result = E_ACCESSDENIED;
                }
            } else {
                // Pointer in some other process or call came asynchronously from 
                // a kernel thread - (WriteProcessMemory never generate exception)
                if (!xxx_WriteProcessMemory(pHeader->hSourceProcess,
                                            pOriginal, (PVOID) pDuplicate,
                                            cbOriginal, NULL)) {
                    result = HRESULT_FROM_WIN32(GetLastError ());
                }
            }


        } else {
            // Size required for write-back
            result = E_INVALIDARG;
        }
        
        // Well-behaved code should never fail to copy-out
        DEBUGCHK((S_OK == result)
                 || (HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY) == result)
                 || (HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE) == result)); // target process could have exited
    }
    
    return result;
}


// Perform any required copy-out and then free a heap buffer.  Frees the buffer
// even if the copy-out fails.
static _inline HRESULT
FreeDuplicateBuffer(
    PVOID  pDuplicate,          // The heap buffer to be freed
    PVOID  pOriginal,           // Local pointer or pointer inside the caller process
    DWORD  cbOriginal,
    DWORD  ArgumentDescriptor,
    BOOL   IsOriginalLocal      // TRUE: pOriginal is local
                                // FALSE: pOriginal is from caller process
    )
{
    HRESULT result;
    BufferHeader* pHeader = (BufferHeader*) ((DWORD)pDuplicate - HEADER_SIZE);
    result = FlushDuplicateBuffer (pDuplicate, pOriginal, cbOriginal,
                                   ArgumentDescriptor, IsOriginalLocal);
    
    VERIFY( !LocalFree(pHeader) );
    
    return result;
}


//
// Access-checks and marshals a buffer pointer from the source process, so
// that it may be accessed by the current process.  Returns the marshalled
// pointer.  This function allocates resources which must be freed by a
// subsequent call to CeCloseCallerBuffer.
//
// Duplication prevents asynchronous modification of the buffer by the caller.
// If duplication is not required for security purposes, don't use it.  Then
// CeOpenCallerBuffer can select the most efficient marshalling method for
// best performance.
//
// If duplication is required, allocates a new heap buffer, copies data from the
// source buffer to the heap buffer [if necessary due to "in" descriptors
// ARG_I* or ARG_IO*], and returns the new heap buffer.  If duplication is not
// required, CeOpenCallerBuffer may still duplicate the buffer, or it may
// allocate virtual address space in the current process (VirtualAlloc) and
// point it at the caller process' memory (VirtualCopy) to create an alias to
// the same memory.  In all cases, any required write-back to the source buffer
// will be managed by CeCloseCallerBuffer [if necessary due to "out" descriptors
// ARG_IO* or ARG_O*].
//
// This call uses ReadProcessMemory and WriteProcessMemory to do its work.  If
// your code is running at a low enough privilege level that it does not have
// access to those APIs, this call will fail with E_ACCESSDENIED.
//
// Does not allocate any resources if the call fails, or if the source buffer
// was NULL.  If this call fails for any reason, the pointer returned in
// *ppDestMarshalled is NULL.
//
// This function opens the caller buffer for synchronous access during an API
// call.  You must call CeAllocAsynchronousBuffer in order to use the buffer
// returned by CeOpenCallerBuffer asynchronously.  Do not close the caller
// buffer until after you have called CeFreeAsynchronousBuffer.
//
// This function is protected by __try/__except so as not to throw an exception
// while accessing the input pointer pSrcUnmarshalled.
//
// Possible return values:
// E_INVALIDARG    pSrcUnmarshalled was NULL, the length was 0, or some other
//                 argument was invalid.
// E_ACCESSDENIED  The source buffer was an invalid address, or your code does
//                 not have sufficient privilege to access the memory.
// E_OUTOFMEMORY   The memory allocation failed.
// S_OK            The allocation (and duplication, if necessary) succeeded.
//
// It is strongly recommended to use the SUCCEEDED() / FAILED() macros to test
// the return value of this function.
//
extern "C"
HRESULT
xxx_CeOpenCallerBuffer(
    PVOID* ppDestMarshalled,        // Receives a pointer that the current
                                    // process can use to access the buffer
                                    // synchronously.
    PVOID  pSrcUnmarshalled,        // Pointer to the caller's data,
                                    // to be access checked, marshalled, and
                                    // possibly duplicated.
    DWORD  cbOriginal,              // Size of the caller's buffer, in bytes.
                                    // If the ArgumentDescriptor is a WSTR
                                    // or ASTR, then a size of 0 can be used.
                                    // If the size of a string is non-zero, then
                                    // it must include the terminating NULL.
    DWORD  ArgumentDescriptor,      // Descriptor explaining what kind of API
                                    // argument the buffer is, eg. ARG_I_WSTR,
                                    // ARG_O_PTR, etc. ARG_DW is NOT a valid
                                    // descriptor for marshalling!
    BOOL   ForceDuplicate           // Set to TRUE to require a temporary heap
                                    // buffer to be allocated in the current
                                    // process.  Set to FALSE to allow
                                    // CeOpenCallerBuffer to select the most
                                    // efficient marshalling method.
    )
{    
    HRESULT hr = E_INVALIDARG;
    
    // Indicates whether it's possible to find the length of a string without
    // first marshalling it.
#ifdef KCOREDLL
    BOOL IsSourceAccessible = TRUE;
#else  // KCOREDLL
    // User-mode can only access the source buffer directly if it's from the same process
    BOOL IsSourceAccessible = (GetCallerVMProcessId() == GetCurrentProcessId());
    
    // User-mode servers can't VirtualCopy, so there's no choice
    // but to duplicate the buffer.  Even during a server self-call, we duplicate
    // because CeCloseCallerBuffer assumes the buffer was duplicated.
    ForceDuplicate = TRUE;
#endif // KCOREDLL

    if (ppDestMarshalled && pSrcUnmarshalled) {
        DWORD cbSrc = GetBufferSize(pSrcUnmarshalled, cbOriginal, ArgumentDescriptor, IsSourceAccessible);
        if (cbSrc) {

            if (
#ifdef KCOREDLL
                // It's legal to pass a kernel address from one part of the kernel
                // to another.  This allows kernel APIs to re-marshal pointers
                // that have already been marshalled by other kernel APIs.
                (GetDirectCallerProcessId() == GetCurrentProcessId()) ||
#endif // KCOREDLL

                IsValidUsrPtr(pSrcUnmarshalled, cbSrc, (ArgumentDescriptor & ARG_O_BIT) ? TRUE : FALSE)) {

                if (ForceDuplicate) 
                {
                    // Need to use heap alloc / memcpy to duplicate the buffer
                    if (SUCCEEDED(hr = AllocDuplicateBuffer (ppDestMarshalled, pSrcUnmarshalled, cbSrc, cbOriginal, ArgumentDescriptor, IsSourceAccessible))) {
                        DEBUGCHK(AddToAllocList(*ppDestMarshalled));
                    }
                }
                
#ifdef KCOREDLL
                // Check if the source is in shared heap address and is an [IN] argument.
                // If it is we will always copy-in data to avoid overwrite of shared heap 
                // data by kernel mode components.
                else if (!(ArgumentDescriptor & ARG_O_BIT) && IsInSharedHeapRange(pSrcUnmarshalled, cbSrc)) {                    
                    if (SUCCEEDED(hr = AllocDuplicateBuffer (ppDestMarshalled, pSrcUnmarshalled, cbSrc, cbOriginal, ArgumentDescriptor, IsSourceAccessible))) {
                        DEBUGCHK(AllocReadOnlyCopy(ppDestMarshalled, cbSrc));
                        DEBUGCHK(AddToAllocList(*ppDestMarshalled));                        
                    }
                }
#endif    // KCOREDLL   

                else 
                {
                    // The kernel can access user buffers directly without using
                    // VirtualCopy to create an alias.  There is no more work
                    // to do when a duplicate is not required.
                    *ppDestMarshalled = pSrcUnmarshalled;
                    hr = S_OK;
                }

            } else {
                hr = E_ACCESSDENIED;
            }
        
        } else if (!IsSourceAccessible) {
            // This case only happens in user-mode servers, because the kernel can
            // always touch the user memory to determine the length of the string.
            if (ARG_I_WSTR == ArgumentDescriptor) {
                // Unicode string whose length is unknown.  Marshal it at the same
                // time its length is being determined.
                if (SUCCEEDED(hr = AllocDuplicateString(ppDestMarshalled, pSrcUnmarshalled, TRUE))) {
                    DEBUGCHK(AddToAllocList(*ppDestMarshalled));                    
                }
                
            } else if (ARG_I_ASTR == ArgumentDescriptor) {
                // ANSI string whose length is unknown.
                if (SUCCEEDED(hr = AllocDuplicateString(ppDestMarshalled, pSrcUnmarshalled, FALSE))) {
                    DEBUGCHK(AddToAllocList(*ppDestMarshalled));
                }
            }
        }
    }
    
    return hr;
}


//
// Frees any resources that were allocated by CeOpenCallerBuffer.
// Performs any required write-back to the caller buffer.  (Due to "out" 
// descriptors ARG_IO* or ARG_O*)
//
// This function is protected by __try/__except so as not to throw an exception
// while accessing the input pointer pSrcUnmarshalled.
//
// Possible return values:
// E_INVALIDARG    pSrcUnmarshalled was NULL, the length was 0, or some other
//                 argument was invalid.
// E_ACCESSDENIED  Required write-back could not be performed.  If this error
//                 occurs, resources are still released and the marshalled
//                 pointer is no longer accessible.
// S_OK            The free succeeded.
//
// It is strongly recommended to use the SUCCEEDED() / FAILED() macros to test
// the return value of this function.
//
extern "C"
HRESULT
xxx_CeCloseCallerBuffer(
    PVOID  pDestMarshalled,     // Pointer to the buffer that was allocated by
                                // CeOpenCallerBuffer.
    PVOID  pSrcUnmarshalled,    // The source pointer that was passed to
                                // CeOpenCallerBuffer.
    DWORD  cbSrc,               // The buffer size that was passed to
                                // CeOpenCallerBuffer.
    DWORD  ArgumentDescriptor   // The descriptor that was passed to
                                // CeOpenCallerBuffer.
    )
{
    HRESULT hr = E_INVALIDARG;
    DWORD dwOrigErr = GetLastError(); // for restoration before return

    // Making some assumptions here about arguments already being checked
    // by CeOpenCallerBuffer.  FreeDuplicateBuffer sanity-checks them on
    // debug builds.

    if (pDestMarshalled && pSrcUnmarshalled) {
        if (pDestMarshalled != pSrcUnmarshalled) {

            DEBUGCHK(ValidateNode(pDestMarshalled, pSrcUnmarshalled, cbSrc, ArgumentDescriptor));                
            DEBUGCHK(RemoveFromAllocList(pDestMarshalled));

#ifdef KCOREDLL
#ifdef DEBUG             
            if (!(ArgumentDescriptor & ARG_O_BIT) && IsInSharedHeapRange(pSrcUnmarshalled, cbSrc)) {
                pDestMarshalled = FreeReadOnlyCopy(pDestMarshalled);
                DEBUGCHK (pDestMarshalled);
            }
#endif // DEBUG
#endif // KCOREDLL

            // The buffer was duplicated
            hr = FreeDuplicateBuffer(pDestMarshalled, pSrcUnmarshalled, cbSrc, ArgumentDescriptor, FALSE);

        } else {

            // Only kernel mode can work without duplication
#ifdef KCOREDLL
            hr = S_OK;
#else  // KCOREDLL
            DEBUGCHK(0);
#endif // KCOREDLL

        }
    }

    SetLastError(dwOrigErr);
    return hr;
}



//
// Re-marshals a buffer that was already marshalled by CeOpenCallerBuffer, so
// that the server can use it asynchronously after the API call has returned.
// Call this function synchronously before your API call returns.  You can not
// call this function asynchronously.  This function allocates resources which
// must be freed by a subsequent call to CeFreeAsynchronousBuffer.
//
// API parameter access (KERNEL MODE ONLY):
//   You can use CeAllocAsynchronousBuffer to get asynchronous access to an API
//   parameter (which would have already been marshalled by the kernel).
//   However if there is any chance that your code will run in user mode, then
//   don't do this.  Instead follow the user mode instructions below.
// API parameter access (USER MODE):
//   To access an API parameter asynchronously, define the API function
//   signature so that the parameter is declared as an ARG_DW value, so that
//   the kernel does not automatically marshal the parameter for you.  Then call
//   CeOpenCallerBuffer to marshal the parameter.  The asynchronous buffer will
//   become inaccessible if you close the marshaled buffer by calling
//   CeCloseCallerBuffer, so you should call CeFreeAsynchronousBuffer before
//   calling CeCloseCallerBuffer.  In other words, do not call
//   CeCloseCallerBuffer until after you have called CeFreeAsynchronousBuffer.
//
// CeAllocAsynchronousBuffer is not required for buffers that have been
// duplicated by CeAllocDuplicateBuffer.  You do not need to do anything in
// order to use those buffers asynchronously.  Those buffers can be used until
// they are closed/freed.  But if you choose to call CeAllocAsynchronousBuffer
// on a duplicated buffer, it will work.  In that case you must not call
// CeFreeDuplicateBuffer until after you have called CeFreeAsynchronousBuffer.
//
// Does not allocate any resources if the call fails, or if the source buffer
// was NULL.  If duplication is required but no memory is allocated, the pointer
// returned by CeFreeAsynchronousBuffer is NULL.
//
// This function is protected by __try/__except so as not to throw an exception
// while accessing the input pointer pSrcSyncMarshalled.
//
// Possible return values:
// E_INVALIDARG    pSrcUnmarshalled was NULL, or the length was 0.
// E_ACCESSDENIED  The source buffer was an invalid address.
// E_OUTOFMEMORY   The memory allocation failed.
// S_OK            The allocation (and duplication, if necessary) succeeded.
//
// It is strongly recommended to use the SUCCEEDED() / FAILED() macros to test
// the return value of this function.
//
extern "C"
HRESULT
xxx_CeAllocAsynchronousBuffer(
    PVOID* ppDestAsyncMarshalled,   // Receives a pointer that the current
                                    // process can use to access the buffer
                                    // asynchronously.
    PVOID  pSrcSyncMarshalled,      // Pointer to the buffer that has already
                                    // been marshalled for synchronous access
                                    // by the current process.
    DWORD  cbOriginal,              // Size of the marshalled buffer, in bytes.
                                    // If the ArgumentDescriptor is a WSTR
                                    // or ASTR, then a size of 0 can be used.
                                    // If the size of a string is non-zero, then
                                    // it must include the terminating NULL.
    DWORD  ArgumentDescriptor       // Descriptor explaining what kind of API
                                    // argument the buffer is, eg. ARG_I_WSTR,
                                    // ARG_O_PTR, etc. ARG_DW is NOT a valid
                                    // descriptor for marshalling!
    )
{    
    if (ppDestAsyncMarshalled && pSrcSyncMarshalled) {
        DWORD cbSrc = GetBufferSize(pSrcSyncMarshalled, cbOriginal, ArgumentDescriptor, TRUE);
        if (cbSrc) {

#ifdef KCOREDLL
            BOOL NeedWriteAccess = (ArgumentDescriptor & ARG_O_BIT) ? TRUE : FALSE;
            if (IsValidUsrPtr(pSrcSyncMarshalled, cbSrc, NeedWriteAccess)) {
                // The buffer is in user memory -- it has not been duplicated.

                // Check if the source is in shared heap address and is an [IN] argument.
                // If it is we will always copy-in data to avoid overwrite of shared heap 
                // data by kernel mode components.
                if (!(ArgumentDescriptor & ARG_O_BIT) && IsInSharedHeapRange(pSrcSyncMarshalled, cbSrc)) {                    
                    if (SUCCEEDED(AllocDuplicateBuffer (ppDestAsyncMarshalled, pSrcSyncMarshalled, cbSrc, cbOriginal, ArgumentDescriptor, TRUE))) {
                        DEBUGCHK(AllocReadOnlyCopy(ppDestAsyncMarshalled, cbSrc));
                        DEBUGCHK(AddToAllocList(*ppDestAsyncMarshalled));                        
                    }

                } else if ((cbOriginal <= MARSHAL_CICO_SIZE) && !(ArgumentDescriptor & MARSHAL_FORCE_ALIAS)) {
                    // copy-in/copy-out
                    HRESULT hr = S_OK;
                    if (SUCCEEDED(hr = AllocDuplicateBuffer (ppDestAsyncMarshalled, pSrcSyncMarshalled, cbSrc, cbOriginal, ArgumentDescriptor, TRUE))) {
                        DEBUGCHK(AddToAllocList(*ppDestAsyncMarshalled));
                    }
                    return hr;

                } else {
                    // else virtual copy to kernel
                    PVOID pResult = MapAsyncPtr (pSrcSyncMarshalled, cbSrc, NeedWriteAccess);
                    if (pResult) {
                        *ppDestAsyncMarshalled = pResult;
                        return S_OK;

                    } else {
                       return HRESULT_FROM_WIN32(GetLastError());
                    }
                }

            } else {
                // The buffer is in kernel memory -- it has been duplicated.
                // Just use the duplicated pointer, which should remain valid
                // until it is closed with CeCloseCallerBuffer.
                *ppDestAsyncMarshalled = pSrcSyncMarshalled;
                return S_OK;
            }

#else  // KCOREDLL

            // The only way to use a buffer asynchronously in user mode is to
            // first call CeOpenCallerBuffer on it.  Even API parameters: the
            // user-mode server must receive the argument as an ARG_DW instead
            // of having the kernel do the marshalling, and then use
            // CeOpenCallerBuffer on it.  So CeAllocAsynchronousBuffer can just
            // use the already-duplicated buffer that was returned by
            // CeOpenCallerBuffer.  That buffer should remain valid until it
            // is closed with CeCloseCallerBuffer.
            *ppDestAsyncMarshalled = pSrcSyncMarshalled;
            return S_OK;

#endif // KCOREDLL

        }
    }
    
    return E_INVALIDARG;
}


//
// Frees any resources that were allocated by CeAllocAsynchronousBuffer.
// Performs any required write-back to the source buffer.  (Due to "out" 
// descriptors ARG_IO* or ARG_O*)
//
// This function is protected by __try/__except so as not to throw an exception
// while accessing the input pointer pSrcSyncMarshalled.
//
// Possible return values:
// E_FAIL          Required write-back could not be performed.  If this error
//                 occurs, resources are still released and the marshalled
//                 pointer is no longer accessible.
// S_OK            The allocation (and duplication, if necessary) succeeded.
//
// It is strongly recommended to use the SUCCEEDED() / FAILED() macros to test
// the return value of this function.
//
extern "C"
HRESULT
xxx_CeFreeAsynchronousBuffer(
    PVOID  pDestAsyncMarshalled,// Pointer to the buffer that was allocated by
                                // CeAllocAsynchronousBuffer.
    PVOID  pSrcSyncMarshalled,  // The source pointer that was passed to
                                // CeAllocAsynchronousBuffer.
    DWORD  cbSrc,               // The buffer size that was passed to
                                // CeAllocAsynchronousBuffer.
    DWORD  ArgumentDescriptor   // The descriptor that was passed to
                                // CeAllocAsynchronousBuffer.
    )
{
    HRESULT hr = E_INVALIDARG;
    
    // Making some assumptions here about arguments already being checked
    // by CeAllocAsynchronousBuffer.  There is currently no sanity-check for
    // those assumptions.

    if (pDestAsyncMarshalled && pSrcSyncMarshalled) {

#ifdef KCOREDLL

        if (pDestAsyncMarshalled != pSrcSyncMarshalled) {

            if (!(ArgumentDescriptor & ARG_O_BIT) && IsInSharedHeapRange(pSrcSyncMarshalled, cbSrc)) {                    

#ifdef DEBUG
                DEBUGCHK(ValidateNode(pDestAsyncMarshalled, pSrcSyncMarshalled, cbSrc, ArgumentDescriptor));                
                DEBUGCHK(RemoveFromAllocList(pDestAsyncMarshalled));
                pDestAsyncMarshalled = FreeReadOnlyCopy(pDestAsyncMarshalled);
                DEBUGCHK(pDestAsyncMarshalled);
#endif // DEBUG                

                // [IN] arguments which are in shared heap range, we always duplicate.
                hr = FreeDuplicateBuffer (pDestAsyncMarshalled, pSrcSyncMarshalled, cbSrc, ArgumentDescriptor, TRUE);

            } else if ((cbSrc <= MARSHAL_CICO_SIZE) && !(ArgumentDescriptor & MARSHAL_FORCE_ALIAS)) {
                // copy-in/copy-out
                DEBUGCHK(ValidateNode(pDestAsyncMarshalled, pSrcSyncMarshalled, cbSrc, ArgumentDescriptor));                
                DEBUGCHK(RemoveFromAllocList(pDestAsyncMarshalled));
                hr = FreeDuplicateBuffer (pDestAsyncMarshalled, pSrcSyncMarshalled, cbSrc, ArgumentDescriptor, TRUE);

            } else {
                // The buffer was VirtualCopied
                if (UnmapAsyncPtr (pSrcSyncMarshalled, pDestAsyncMarshalled, cbSrc)) {
                    hr = S_OK;

                } else {
                    hr = HRESULT_FROM_WIN32(GetLastError());
                }
            }
        } 
        else

#endif  // KCOREDLL

        {
            // We were using the same duplicated buffer that was returned by
            // CeOpenCallerBuffer.  CeCloseCallerBuffer will free the memory.
            hr = S_OK;
        }
    }
    
    return hr;
}



//
// Flushes any changed data between source and destination buffer allocated by
// CeAllocAsynchronousBuffer.
// ARG_O_PTR:   Writes back data from asynchronous buffer into source buffer.
// ARG_IO_PTR:  Writes back data from asynchronous buffer into source buffer.
//              Does NOT read from source buffer.
// Others:      Fail with ERROR_NOT_SUPPORTED
//
// This function is protected by __try/__except so as not to throw an exception
// while accessing the input pointer pSrcSyncMarshalled.
//
// Possible return values:
// E_FAIL          Required read or write-back could not be performed.
// S_OK            The read or write succeeded.
//
// It is strongly recommended to use the SUCCEEDED() / FAILED() macros to test
// the return value of this function.
//
extern "C"
HRESULT
xxx_CeFlushAsynchronousBuffer(
    PVOID  pDestAsyncMarshalled,// Pointer to the buffer that was allocated by
                                // CeAllocAsynchronousBuffer.
    PVOID  pSrcSyncMarshalled,  // The source pointer that was passed to
                                // CeAllocAsynchronousBuffer.
    PVOID  pSrcUnmarshalled,    // The source pointer that was passed to
                                // CeOpenCallerBuffer, or NULL if the buffer
                                // was an API parameter than never came from
                                // CeOpenCallerBuffer (kernel mode only).
    DWORD  cbSrc,               // The buffer size that was passed to
                                // CeAllocAsynchronousBuffer.
    DWORD  ArgumentDescriptor   // The descriptor that was passed to
                                // CeAllocAsynchronousBuffer.
    )
{
    HRESULT hr = E_INVALIDARG;
    
    // Making some assumptions here about arguments already being checked
    // by CeAllocAsynchronousBuffer.  There is currently no sanity-check for
    // those assumptions.

    if (pDestAsyncMarshalled && pSrcSyncMarshalled) {

        if (!(ArgumentDescriptor & ARG_O_BIT)) {
            // Only ARG_O_PTR and ARG_IO_PTR are supported right now.
            hr = HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);

#ifdef KCOREDLL

        } else if (pDestAsyncMarshalled != pSrcSyncMarshalled) {
            if ((cbSrc <= MARSHAL_CICO_SIZE) && !(ArgumentDescriptor & MARSHAL_FORCE_ALIAS)) {
                // copy-in/copy-out
                DEBUGCHK(ValidateNode(pDestAsyncMarshalled, pSrcSyncMarshalled, cbSrc, ArgumentDescriptor));
                hr = FlushDuplicateBuffer (pDestAsyncMarshalled, pSrcSyncMarshalled, cbSrc, ArgumentDescriptor, TRUE);

            } else {
                // The buffer was VirtualCopied.  The changes are automatically 
                // in the right place.
                hr = S_OK;
            }

#endif // KCOREDLL

        } else if (pSrcUnmarshalled) {
            // We were using the same duplicated buffer that was returned by
            // CeOpenCallerBuffer.  The changes need to be written back to the
            // caller buffer.
            if (pSrcSyncMarshalled != pSrcUnmarshalled) {
                // The buffer was duplicated
                DEBUGCHK(ValidateNode(pSrcSyncMarshalled, pSrcUnmarshalled, cbSrc, ArgumentDescriptor));
                hr = FlushDuplicateBuffer(pSrcSyncMarshalled, pSrcUnmarshalled, cbSrc, ArgumentDescriptor, FALSE);

            } else {
                DEBUGCHK(0);
            }
        }
    }
    
    return hr;
}



//
// This function abstracts the work required to make secure-copies of API
// arguments.  Don't use it for buffers other than API arguments.  Don't 
// expect the duplicated buffer to be accessible after the API call returns.
//
// Allocates a new heap buffer, copies data from the source buffer to the heap
// buffer [if necessary due to "in" descriptors ARG_I* or ARG_IO*], and returns
// the new heap buffer.  This function allocates resources which must be freed
// by a subsequent call to CeFreeDuplicateBuffer.  Any required write-back to
// the source buffer will be managed by CeFreeDuplicateBuffer [if necessary due
// to "out" descriptors ARG_IO* or ARG_O*].
//
// Duplication prevents asynchronous modification of the buffer by the caller.
// If duplication is not required for security purposes, don't use it.  Just
// access the caller's buffer as passed to your API.
//
// Does not allocate any memory if the call fails, or if the source buffer was
// NULL.  If no memory is allocated, the pointer returned in *ppDestDuplicate
// is NULL.
//
// Do not use CeAllocDuplicateBuffer with a buffer marshalled by 
// CeOpenCallerBuffer.  Instead have CeOpenCallerBuffer do the duplication.
//
// You do not need to call CeAllocAsynchronousBuffer in order to use the buffer
// returned by CeAllocDuplicateBuffer asynchronously.  The duplicate buffer can
// be used until it is closed by CeCloseCallerBuffer.  CeAllocAsynchronousBuffer
// will not work on buffers that have been duplicated by CeAllocDuplicateBuffer.
//
// This function is protected by __try/__except so as not to throw an exception
// while accessing the input pointer pSrcMarshalled.
//
// Possible return values:
// E_INVALIDARG    pSrcMarshalled was NULL, or the length was 0.
// E_ACCESSDENIED  The source buffer was an invalid address, possibly a pointer
//                 that has not been marshalled.
// E_OUTOFMEMORY   The memory allocation failed.
// S_OK            The allocation and copy succeeded.
//
// It is strongly recommended to use the SUCCEEDED() / FAILED() macros to test
// the return value of this function.
//
extern "C"
HRESULT
xxx_CeAllocDuplicateBuffer(
    PVOID* ppDestDuplicate,     // Receives a pointer to a newly-allocated heap
                                // buffer.
    PVOID  pSrcMarshalled,      // Pointer to the caller's data, that has 
                                // already been marshalled.
    DWORD  cbOriginal,          // Size of the caller's buffer, in bytes.
                                // If the ArgumentDescriptor is a WSTR
                                // or ASTR, then a size of 0 can be used.
                                // If the size of a string is non-zero, then
                                // it must include the terminating NULL.
    DWORD  ArgumentDescriptor   // Descriptor explaining what kind of API
                                // argument the buffer is, eg. ARG_I_WSTR,
                                // ARG_O_PTR, etc. ARG_DW is NOT a valid
                                // descriptor for duplicating!
    )
{
    HRESULT hr = E_INVALIDARG;

    if (ppDestDuplicate && pSrcMarshalled) {
        DWORD cbSrc = GetBufferSize(pSrcMarshalled, cbOriginal, ArgumentDescriptor, TRUE);
        if (cbSrc) {
            // Kernel mode: server parameters are never duplicated by the
            //     kernel during the API call, so we must always duplicate.
            // User mode: All calls to user mode servers are copy-in/copy-out,
            //     but the reflector sometimes VirtualCopies arguments to
            //     user-mode drivers, so we must always duplicate (though
            //     sometimes it's duplicating for a second time).
            if (SUCCEEDED(hr = AllocDuplicateBuffer (ppDestDuplicate, pSrcMarshalled, cbSrc, cbOriginal, ArgumentDescriptor, TRUE))) {
                DEBUGCHK(AddToAllocList(*ppDestDuplicate));
            }
        }
    }
    
    return hr;
}



//
// Frees a duplicate buffer that was allocated by CeAllocDuplicateBuffer.
// Performs any required write-back to the source buffer.  (Due to "out" 
// descriptors ARG_IO* or ARG_O*)
//
// This function is protected by __try/__except so as not to throw an exception
// while accessing the input pointer pSrcMarshalled.
//
// Possible return values:
// E_FAIL          Required write-back could not be performed.  If this error
//                 occurs, resources are still released and the duplicated
//                 pointer is no longer accessible.
// S_OK            The free (and write-back, if necessary) succeeded.
//
// It is strongly recommended to use the SUCCEEDED() / FAILED() macros to test
// the return value of this function.
//
extern "C"
HRESULT
xxx_CeFreeDuplicateBuffer(
    PVOID  pDestDuplicate,      // Pointer to the buffer that was allocated by
                                // CeAllocDuplicateBuffer.
    PVOID  pSrcMarshalled,      // The source pointer that was passed to
                                // CeAllocDuplicateBuffer.
    DWORD  cbSrc,               // The buffer size that was passed to
                                // CeAllocDuplicateBuffer.
    DWORD  ArgumentDescriptor   // The descriptor that was passed to
                                // CeAllocDuplicateBuffer.
    )
{
    HRESULT hr = E_INVALIDARG;
    
    // Making some assumptions here about arguments already being checked
    // by CeAllocDuplicateBuffer.  FreeDuplicateBuffer sanity-checks them on
    // debug builds.

    if (pDestDuplicate && pSrcMarshalled) {
        DEBUGCHK (pDestDuplicate != pSrcMarshalled);
        DEBUGCHK(ValidateNode(pDestDuplicate, pSrcMarshalled, cbSrc, ArgumentDescriptor));                
        DEBUGCHK(RemoveFromAllocList(pDestDuplicate));        
        
        hr = FreeDuplicateBuffer(pDestDuplicate, pSrcMarshalled, cbSrc, ArgumentDescriptor, TRUE);
    }
    
    return hr;
}

