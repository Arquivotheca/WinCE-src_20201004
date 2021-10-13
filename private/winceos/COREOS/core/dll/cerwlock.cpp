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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:
    cerwlock.cpp

Abstract:
    Shared/Exclusive lock APIs. This implementation supports:
        a) Recursion
            Thread holding the lock calling to acquire the lock again
        b) Automatic lock upgrade/downgrade
            Thread upgrading from shared to exclusive and downgrading from exclusive to shared
        c) Automatic lock ownership cleanup on thread exit.

--*/
 
#include <windows.h>
#include <cerwlock.h>

#define OwnCS(cs)  ((DWORD)(cs)->OwnerThread == GetCurrentThreadId())

//
// Owner entry structure used to record recursion count
// for a lock owned by a thread. This data is stored in 
// thread TLS slot.
//
typedef struct _OWNER_ENTRY 
{

    union {    

        //
        // count of active locks owned by current thread.
        // valid only for the header node.
        //
        ULONG hdr_ulActiveLocks;

        //
        // pointer to lock object; valid only for data nodes.
        //
        HRWLOCK data_hLock; 
    };
    
    union {

        //
        // count of total number of owner entries following the 
        // first one. valid only for the header node.
        //
        ULONG hdr_ulTotalLocks;

        //
        // recursion count of the current thread for the lock;
        // valid only data nodes.
        // HIWORD stores the exclusive recursion count
        // LOWORD stores the shared recursion count
        //
        ULONG data_ulRecursionCount;
    };
    
} OWNER_ENTRY, *POWNER_ENTRY;    

//
// Shared / Exclusive lock structure
//
typedef struct _CERWLOCK 
{
    
    //
    // Used to protect updates to ulGlobalCount and
    // state of hWaitObject.
    //    
    CRITICAL_SECTION csState;

    //
    // Used by threads to wait for the lock to be available
    //
    HANDLE hWaitObject;

    //
    // LOWORD: #of unique owners (max 64k threads)
    // HIWORD: current state of the lock. Can be one of
    // the following values:
    // - 0x0001xxxx (RWL_XBIT): Lock is currently held exclusive
    //    or a thread which has shared ownership is waiting tofor
    //    exclusive access to the lock.
    // - 0x0002xxxx (RWL_WBIT): at least one thread is waiting 
    //    for shared locking
    //
    //
    ULONG ulGlobalCount;

    //
    // handle to an event used by a thread trying to boost
    // a lock from shared to exclusive - auto reset event
    //
    HANDLE hWaitForBoost;
    
} CERWLOCK, *PCERWLOCK;

//
// different states of the lock (definition for HIWORD of ulGlobalCount field)
//    
#define RWL_XBIT 0x00010000
#define RWL_WBIT 0x00020000

#define RWL_MAX 0xFFFF // maximum #of threads which can hold the lock at any time

//
// TLS slot used by threads to store recursion counts
// for all the locks held by each thread.
//
// Per-thread recursion counts are stored as follows 
// 
// Header Node Followed by 0 or more pLock Node (s)
//
// Both header node and lock node are of type:
// OWNER_ENTRY
//
// Header Node has:
// -- #of lock nodes created for this thread
// -- Count of current active locks held by the thread
//
// Each of the lock node stores:
// -- handle to the lock object
// -- Current recursion count for the lock
//
DWORD g_dwLockSlot = TLS_OUT_OF_INDEXES;

//
// Minimum number of owner entries reserved in each thread TLS slot
// for locks -- this is the number by which thread lock table
// memory is increased. Initially we will have memory for supporting
// three simultaneous lock locks for each thread.
//
#define MIN_OWNER_ENTRIES 4

//
// Increase the recursion count value by shared or exclusive
// count increment. Shared recursion count is in the lower
// word of the recursion count field and Exclusive recursion
// count is in the upper word of the recursion count.
//
static
BOOL
UpdateEntry(
    __inout LPDWORD pdwCount,
    __in DWORD dwIncrement,
    __in DWORD dwMaxCount
    )
{
    DWORD dwCurCount = *pdwCount & dwMaxCount; // mask off the lo/hi word

    if ((dwCurCount + dwIncrement) < dwMaxCount) 
    {
        *pdwCount += dwIncrement;
        return TRUE;
    }

    ASSERT (0);
    return FALSE;
}

//
// Swap owner entry [SrcIndex] with owner entry [DstIndex]
// in the given owner table.
//
static
void
SwapEntry(
    POWNER_ENTRY pOwnerTable,
    ULONG ulSrcIndex,
    ULONG ulDstIndex
    )
{
#ifdef DEBUG
    ULONG ulEntryCount = pOwnerTable->hdr_ulTotalLocks; // 1-based count of TLS nodes    
    ASSERT ((ulSrcIndex) && (ulDstIndex) && (ulSrcIndex <= ulEntryCount) && (ulDstIndex <= ulEntryCount));
#endif

    if (ulSrcIndex != ulDstIndex) 
    {

        POWNER_ENTRY pSrcEntry = pOwnerTable + ulSrcIndex;
        POWNER_ENTRY pDstEntry = pOwnerTable + ulDstIndex;
        OWNER_ENTRY TempEntry = *pSrcEntry;
        *pSrcEntry = *pDstEntry;
        *pDstEntry = TempEntry;
    }
}

//
// Match lock entry with the given lock entry value.
// Returns the owner entry matched.
//
static
POWNER_ENTRY
FindEntry(
    HRWLOCK hLock
    )
{   
    POWNER_ENTRY pOwnerTable = (POWNER_ENTRY) TlsGetValue (g_dwLockSlot);

    if (pOwnerTable) 
    {
    
        //
        // first pOwnerEntry has the number of owner entries
        // following (in hdr_ulTotalLocks field).
        //
        
        ULONG EntryCount = 1;
        POWNER_ENTRY FirstEntry = pOwnerTable + 1;  
        POWNER_ENTRY CurrentEntry = FirstEntry;

        while (EntryCount++ <= pOwnerTable->hdr_ulTotalLocks) 
        {

            if (hLock == CurrentEntry->data_hLock) 
            {

                //
                // swap current node with the first node so that
                // subsequent finds for the same node will be
                // faster.
                //

                SwapEntry (pOwnerTable, 1, EntryCount-1);
                return FirstEntry;     
            }   
            
            CurrentEntry++;
        }
    }

    return NULL;
}

//
// Insert a new entry into the lock for the
// given thread.
//
static
POWNER_ENTRY
AddEntry(
    __in HRWLOCK hLock
    )
{
    POWNER_ENTRY pOwnerEntry = NULL;
    POWNER_ENTRY pOwnerTable = (POWNER_ENTRY) TlsGetValue (g_dwLockSlot);

    if (pOwnerTable) 
    {

        //
        // find a free entry in the thread lock list
        //
        
        pOwnerEntry = FindEntry (0);
    }

    if (!pOwnerEntry) 
    {
                
        //
        // either the owner table is not setup or there is no free
        // entry; create a new entry.
        //

        if (!pOwnerTable) 
        {

            //
            // first time this thread has ever requested a shared/exclusive lock.
            // create the owner table to hold MIN_OWNER_ENTRIES.
            //

            pOwnerTable = (POWNER_ENTRY) LocalAlloc (LPTR, sizeof(OWNER_ENTRY) * MIN_OWNER_ENTRIES);
            if (!pOwnerTable)
                return NULL;

            //
            // first entry stores the count of number of owner entries following
            //

            pOwnerTable->hdr_ulTotalLocks = MIN_OWNER_ENTRIES - 1;

        } 
        else 
        {

            //
            // the owner table in the thread is full; expand the owner table.
            //

            ULONG ulOldSize = pOwnerTable->hdr_ulTotalLocks; // current count of entries in the table
            ULONG ulNewSize = (ulOldSize + MIN_OWNER_ENTRIES + 1) * sizeof (OWNER_ENTRY);

            pOwnerTable = (POWNER_ENTRY) LocalReAlloc (pOwnerTable, ulNewSize, LMEM_ZEROINIT|LMEM_MOVEABLE);
            if (!pOwnerTable)
                return NULL;

            //
            // first entry stores the count of number of owner entries following
            //

            pOwnerTable->hdr_ulTotalLocks = ulOldSize + MIN_OWNER_ENTRIES;

            //
            // swap the first node and the new node at the end of the previous
            // limit so that we add the new entry at the beginning of the table.
            //
            
            SwapEntry (pOwnerTable, 1, ulOldSize + 1);
        }

        //
        // add the new owner table to the current thread local storage
        // slot for read-write locks
        //
        
        VERIFY (TlsSetValue (g_dwLockSlot, (LPVOID)pOwnerTable));

        //
        // free entry will always be the first node
        //
        
        pOwnerEntry = pOwnerTable + 1;        
    }

    //
    // set the owner entry default properties
    //
    
    ASSERT (pOwnerEntry && !pOwnerEntry->data_hLock);    
    pOwnerEntry->data_hLock= hLock;
    pOwnerEntry->data_ulRecursionCount= 0;

    return pOwnerEntry;
}

//
// This function checks the size of the OwnerTable and trims it in place.
//
static
void
TrimOwnerTable (
    __inout POWNER_ENTRY *ppOwnerTable
    )
{
    if((*ppOwnerTable)->hdr_ulTotalLocks / ((*ppOwnerTable)->hdr_ulActiveLocks + MIN_OWNER_ENTRIES + 1) < 2)
        return; // too few wasted entries to justify trimming

    POWNER_ENTRY pOwnerTable = *ppOwnerTable;
    ULONG ulLowIndex = 1;
    ULONG ulHighIndex = pOwnerTable->hdr_ulTotalLocks;
    ULONG ulNewSize = ulHighIndex / 2;

    // if they exist, swap non-NULL entries at high addresses with NULL entries at low addresses
    if(pOwnerTable->hdr_ulActiveLocks)
        for (;;)
        {
            while((pOwnerTable + ulLowIndex)->data_hLock) ulLowIndex++;
            while(!(pOwnerTable + ulHighIndex)->data_hLock) ulHighIndex--;
            if(ulLowIndex < ulHighIndex)
                SwapEntry(pOwnerTable, ulLowIndex, ulHighIndex);
            else
                break;
        }
    // trim owner table; ReAlloc failure just leaves everthing where it is
    pOwnerTable = (POWNER_ENTRY) LocalReAlloc(pOwnerTable, ulNewSize*sizeof(OWNER_ENTRY), LMEM_MOVEABLE);
    if(pOwnerTable)
    {
        pOwnerTable->hdr_ulTotalLocks = ulNewSize - 1;
        *ppOwnerTable = pOwnerTable;
    }
}

//
// lock state functions
//

__inline
LONG
GetCount(
    __in CONST PCERWLOCK pLock
    )
{
    return (0xFFFF & pLock->ulGlobalCount);
}

__inline
BOOL
IsHeldShared(
    __in CONST PCERWLOCK pLock
    )
{
    //
    // lock is considered held shared if
    // LOWORD in thread local ulRecursionCount is non-NULL
    
    POWNER_ENTRY pOwnerEntry = FindEntry ((HRWLOCK)pLock);
    
    if (pOwnerEntry) 
        return (LOWORD(pOwnerEntry->data_ulRecursionCount));
    
    return FALSE;
}

__inline
BOOL
IsWaitBitSet(
    __in CONST PCERWLOCK pLock
    )
{
    return (pLock->ulGlobalCount & RWL_WBIT);   
}

__inline
BOOL
IsExclusiveBitSet(
    __in CONST PCERWLOCK pLock
    )
{
    return (pLock->ulGlobalCount & RWL_XBIT);    
}

//
// Set/reset the wait event depending on whether the
// calling thread is releasing the lock or waiting for the
// lock.
//
static
void
UpdateLockState(
    __inout PCERWLOCK pLock,
    __in BOOL fWait
    )
{

    if (fWait) 
    {
        //
        // thread is waiting for the lock
        //
        
        if (!IsWaitBitSet (pLock))
        {
            pLock->ulGlobalCount |= RWL_WBIT;
            ResetEvent (pLock->hWaitObject);                    
        }
    }
    else
    {
        //
        // thread is releasing the lock
        //
        
        if (IsExclusiveBitSet(pLock)
            && (GetCount(pLock) == 1))
        {
            //
            // if a thread is releasing a lock and xbit is
            // set, then either:
            // - the thread owns the lock in both shared
            //   and exclusive mode and shared locks
            //   is being released OR
            // - a thread is blocked on this lock trying 
            //   to boost the lock from read to write
            //
            // we will signal the boost event in the latter
            // case as lock can now be granted as the
            // thread which is trying to boost is the only
            // one shared thread for the lock.
            //
            
            SetEvent (pLock->hWaitForBoost);
        } 

        if (IsWaitBitSet (pLock))
        {
            pLock->ulGlobalCount &= ~RWL_WBIT;    
            SetEvent (pLock->hWaitObject);
        }        
    }
}

//
// Release the lock and if necessary wake up any
// waiting threads for this lock.
//
static
BOOL
ReleaseLock(
    __in HRWLOCK  hLock,
    __in CERW_TYPE typeLock,
    __in BOOL ThreadExiting
    )
{   
    //
    // get the owner entry for the lock in the given thread
    //

    POWNER_ENTRY pOwnerTable = (POWNER_ENTRY) TlsGetValue (g_dwLockSlot);
    ASSERT (pOwnerTable);
    
    POWNER_ENTRY pOwnerEntry = FindEntry (hLock);
    ASSERT (pOwnerEntry);
    
    if (pOwnerEntry) 
    {

        LONG lAddend = 0;
        PCERWLOCK pLock = (PCERWLOCK) hLock;
        DWORD dwCurrentCount = pOwnerEntry->data_ulRecursionCount;
        
        //
        // clear single lock or all the locks for the current thread
        //

        if (ThreadExiting) 
        {
            //
            // release all lock count for this lock            
            //
            
            pOwnerEntry->data_ulRecursionCount = 0;

        }
        else if (typeLock == CERW_TYPE_SHARED)
        {
            //
            // release one shared count for the lock
            //
            
            if (!LOWORD (pOwnerEntry->data_ulRecursionCount)) 
            {
                ASSERT (0);
                return FALSE;
            }

            pOwnerEntry->data_ulRecursionCount -= 1;
        }
        else if (typeLock == CERW_TYPE_EXCLUSIVE)
        {
            //
            // release one exclusive count for the lock
            //
            
            if (!HIWORD (pOwnerEntry->data_ulRecursionCount))
            {
                ASSERT (0);
                return FALSE;
            }

            pOwnerEntry->data_ulRecursionCount -= MAKELONG (0, 0x1);
        }

        //
        // check if current thread holds this lock anymore
        //
        
        if (!pOwnerEntry->data_ulRecursionCount)
        {

            //
            // At this point we are relinquishing ownership of the lock 
            // for the current thread. Update global lock state and wake 
            // up any threads waiting for the lock.
            //

            ASSERT (pOwnerTable->hdr_ulActiveLocks > 0);
            pOwnerTable->hdr_ulActiveLocks -= 1; // reduce the active # of locks held by this thread
            pOwnerEntry->data_hLock = 0; // this thread no longer has access to this lock
            TrimOwnerTable(&pOwnerTable); // trim the table, if necessary
            
            if (HIWORD (dwCurrentCount))
            {

                //
                // current thread held this lock in exclusive mode.
                // clear the global exclusive bit.
                //

                lAddend += RWL_XBIT;
                ASSERT (pLock->ulGlobalCount & RWL_XBIT);
                if (GetCount(pLock) == 1 && !LOWORD(dwCurrentCount))   // special case handling:
                    lAddend += 0x1;         // ensure global count is decremented
                    
            }

            if (LOWORD (dwCurrentCount))
            {

                //
                // current thread held this lock in shared mode.
                // decrement the global count by 1.
                //
                
                lAddend += 0x1;
                ASSERT (GetCount(pLock) > 0);
            }

            EnterCriticalSection (&pLock->csState);

            //
            // update the global data
            //
            
            pLock->ulGlobalCount -= lAddend;    
            UpdateLockState (pLock, FALSE);            

            LeaveCriticalSection (&pLock->csState);         
            
        }
        else if ((typeLock == CERW_TYPE_EXCLUSIVE)
                    && !HIWORD (pOwnerEntry->data_ulRecursionCount))
        {
            //
            // at this point we are downgrading the lock from exlcusive to shared.
            // lock is downgraded from exclusive to shared if the following
            // conditions are true:
            // a) current thread is releasing exclusive access to the lock
            // b) and exclusive recursion count for the lock is 0
            //

            ASSERT (LOWORD (pOwnerEntry->data_ulRecursionCount)); // current thread still holds shared access to the lock

            EnterCriticalSection (&pLock->csState);
                        
            //
            // update the global data
            //

            ASSERT (pLock->ulGlobalCount & RWL_XBIT);            
            pLock->ulGlobalCount &= ~RWL_XBIT;
            UpdateLockState (pLock, FALSE);            

            LeaveCriticalSection (&pLock->csState);

        }

        return TRUE;
    }

    return FALSE;
}

//
// Wait for the lock to be avalable
//
static
BOOL 
WaitForLock(
    __in HANDLE hWaitEvent,
    __inout PCERWLOCK pLock,
    __inout LPDWORD pdwTimeout
    )
{
    BOOL fRet = FALSE;
    DWORD dwStartTime = GetTickCount ();
    DWORD dwEndTime, dwElapsedTime;
    
    ASSERT (OwnCS (&pLock->csState));
    ASSERT (*pdwTimeout != 0);

    //
    // set the wait bit if necessary
    //
    
    UpdateLockState (pLock, TRUE);

    //
    // wait for the lock to be available
    //
    
    LeaveCriticalSection (&pLock->csState);

    fRet = (WAIT_OBJECT_0 == WaitForSingleObject (hWaitEvent, *pdwTimeout));

    EnterCriticalSection (&pLock->csState);

    //
    // compute the elapsed time and adjust the given time
    //

    if (INFINITE != *pdwTimeout)
    {
        dwEndTime = GetTickCount();
        if (dwEndTime < dwStartTime)
            dwElapsedTime = dwEndTime + 0xffffffff - dwStartTime;
        else
            dwElapsedTime = dwEndTime - dwStartTime;

        //
        // adjust the remaining time by the elapsed time
        //
        
         if (*pdwTimeout > dwElapsedTime)
             *pdwTimeout = (*pdwTimeout - dwElapsedTime);
         else
         {
             *pdwTimeout = 0;
             if (!fRet) // WaitObject not signalled and no time left to wait, so SetLastError()
             {
                 SetLastError(WAIT_TIMEOUT);
                 UpdateLockState (pLock, FALSE); // unsetting the wait bit should be OK...
             }
         }
    }
    
    return fRet;
}

//
// Helper function to acquire lock with wait
//
static
BOOL 
AcquireLockShared(
    __in HRWLOCK hLock,
    __in DWORD dwTimeout
    )
{
    BOOL fLockAcquired = FALSE;
    PCERWLOCK pLock = (PCERWLOCK) hLock;
    
    //
    // check if the current thread already has exclusive
    // or shared ownership of the lock.
    //
    
    POWNER_ENTRY pOwnerEntry = FindEntry (hLock);
    if (pOwnerEntry) 
    {

        //
        // current thread holds the lock in either shared or exclusive mode.
        // increase the recursion count and return.
        //        

        fLockAcquired = UpdateEntry (&pOwnerEntry->data_ulRecursionCount, 0x1, 0xFFFF);
        
    } 
    else 
    {
        
        //
        // current thread does not own this lock
        //
            
        EnterCriticalSection (&pLock->csState);

        do 
        {

            if (pLock->ulGlobalCount < RWL_MAX) {

                //
                // lock is not held by any thread in exclusive mode
                // AND no thread is waiting for the lock; grant lock
                // ownership.
                //
                pOwnerEntry = AddEntry (hLock);
                if (pOwnerEntry)
                {
                    POWNER_ENTRY pOwnerTable = (POWNER_ENTRY) TlsGetValue (g_dwLockSlot);
                    ASSERT (pOwnerTable);
                    
                    pLock->ulGlobalCount += 1; // global data
                    pOwnerEntry->data_ulRecursionCount = 1; // increment the recursion count for the lock in the current thread
                    pOwnerTable->hdr_ulActiveLocks += 1; // increment the active # of locks held by this thread
                    
                    fLockAcquired = TRUE;
                    break;
                }
                else
                {
                    SetLastError(ERROR_OUTOFMEMORY);
                    break;
                }
            }
                        
        } while (dwTimeout && WaitForLock(pLock->hWaitObject, pLock, &dwTimeout));

        LeaveCriticalSection (&pLock->csState);
    }

    return fLockAcquired;

}

//
// Current thread holds the lock in shared mode and
// is now trying to acquire exclusive access to the lock.
//
static
BOOL 
BoostLockToExclusive(
    __in HRWLOCK hLock,
    __inout POWNER_ENTRY pOwnerEntry,
    __in DWORD dwTimeout
    )
{
    BOOL fLockAcquired = FALSE;
    PCERWLOCK pLock = (PCERWLOCK)hLock;

    ASSERT (LOWORD (pOwnerEntry->data_ulRecursionCount)); // current thread already has shared access to this lock
          
    EnterCriticalSection (&pLock->csState);

    if (IsExclusiveBitSet (pLock))
    {
        //
        // some other thread has come in and boosted the
        // lock to exclusive; fail this call! otherwise there
        // will be a deadlock since both threads will not
        // release their shared access to the lock and the
        // exclusive access will never be granted.
        //

        LeaveCriticalSection (&pLock->csState);
        SetLastError(ERROR_LOCK_VIOLATION);
        return fLockAcquired;            
    }

    //
    // set the exclusive bit to prevent any other threads
    // from acquiring shared access to this lock from now
    // on until the lock is released by the current thread.
    //

    pLock->ulGlobalCount |= RWL_XBIT; // update global data

    do 
    {

        if (0x1 == GetCount(pLock)) {

            //
            // current thread is the only owner of this lock.
            // update the per-thread recursion count to
            // include the exclusive recursion count. note
            // that thread already owns the lock in shared
            // mode and hence we don't need to update
            // the global data.
            //
            
            pOwnerEntry->data_ulRecursionCount += MAKELONG (0, 0x1);
            fLockAcquired = TRUE;
            break;
        }
                    
    } while (dwTimeout && WaitForLock(pLock->hWaitForBoost, pLock, &dwTimeout));

    if (!fLockAcquired) 
    {
        //
        // failed due to timeout
        //
        pLock->ulGlobalCount &= ~RWL_XBIT; // undo the state changes since we did not get the lock
    }

    LeaveCriticalSection (&pLock->csState);

    return fLockAcquired;

}

//
// Helper function to acquire lock with wait
//
static
BOOL 
AcquireLockExclusive(
    __in HRWLOCK hLock,
    __in DWORD dwTimeout
    )
{
    BOOL fLockAcquired = FALSE;
    PCERWLOCK pLock = (PCERWLOCK) hLock;    
    
    //
    // check if the current thread already has exclusive
    // ownership of the lock.
    //
    
    POWNER_ENTRY pOwnerEntry = FindEntry (hLock);
    if (pOwnerEntry) 
    {

        if (HIWORD(pOwnerEntry->data_ulRecursionCount)) 
        {

            //
            // current thread holds the lock in exclusive mode.
            // increase the recursion count and return.
            //        

            fLockAcquired = UpdateEntry (&pOwnerEntry->data_ulRecursionCount, (DWORD)MAKELONG (0, 1), (DWORD)MAKELONG (0, 0xFFFF));

        } else {

            //
            // current thread holds the lock in shared mode.
            // try to boost the lock to exclusive mode. this call 
            // could fail if there is already a thread in the middle 
            // of boost from shared to exclusive mode.
            //

            fLockAcquired = BoostLockToExclusive (hLock, pOwnerEntry, dwTimeout);
        }
        
    } 
    else
    {

        //
        // current thread does not hold this lock.
        // create a new owner entry for the current thread.
        //
                    
        EnterCriticalSection (&pLock->csState);

        do 
        {

            if (!pLock->ulGlobalCount) {

                //
                // lock is not held by any thread in shared or exclusive 
                // mode AND no thread is waiting for the lock; grant lock
                // ownership.
                //
                pOwnerEntry = AddEntry (hLock);
                if (pOwnerEntry)
                {
                    POWNER_ENTRY pOwnerTable = (POWNER_ENTRY) TlsGetValue (g_dwLockSlot);
                    ASSERT (pOwnerTable);
                    
                    pLock->ulGlobalCount = RWL_XBIT; // global data
                    pOwnerEntry->data_ulRecursionCount = MAKELONG (0, 0x1); // increment the recusrion count for this lock in the current thread
                    pOwnerTable->hdr_ulActiveLocks += 1; // increment the active # of locks held by this thread
                    
                    fLockAcquired = TRUE;
                    break;
                }
                else
                {
                    SetLastError(ERROR_OUTOFMEMORY);
                    break;
                }
            }
                        
        } while (dwTimeout && WaitForLock(pLock->hWaitObject, pLock, &dwTimeout));

        LeaveCriticalSection (&pLock->csState);
    }

    return fLockAcquired;
}

//
// Free the tls memory associated with shared/exclusive locks.
//
void
CeFreeRwTls(
    DWORD dwThreadId,
    LPVOID lpvSlotValue
    )
{
    POWNER_ENTRY OwnerTable = (POWNER_ENTRY) lpvSlotValue;
    if (OwnerTable) {

        //
        // if the following assert fires, then this thread which is exiting
        // has outstanding owned r/w locks.
        //
        
        ASSERT (!OwnerTable->hdr_ulActiveLocks);
        
        //
        // free the memory associated with the lock list and clear
        // the associated tls slot.
        //
        
        LocalFree (OwnerTable);
    }
}

//
// Checks if the calling thread is holding the lock in exclusive mode.
//
extern "C"
BOOL
CeIsRwLockAcquired(
    __in HRWLOCK     hLock,
    __in CERW_TYPE typeLock
    )
{
    BOOL fRet = FALSE;
    
     if (!hLock 
        || ((typeLock != CERW_TYPE_SHARED) 
            && (typeLock != CERW_TYPE_EXCLUSIVE)))
    {
        ASSERT (0);
        SetLastError (ERROR_INVALID_PARAMETER);

    } else {

        CONST POWNER_ENTRY pOwnerEntry = FindEntry (hLock);
        if (pOwnerEntry) 
        {
            //
            // LOWORD of the data_ulRecursionCount: shared recursion count
            // HIWORD of the data_ulRecursionCount: exclusive recursion count
            //

            if (typeLock == CERW_TYPE_SHARED)
                fRet = LOWORD (pOwnerEntry->data_ulRecursionCount) ? TRUE : FALSE;
            else
                fRet = HIWORD (pOwnerEntry->data_ulRecursionCount) ? TRUE : FALSE;
        }
        if(!fRet)
            SetLastError(ERROR_SUCCESS);
    }
    
    return fRet;
}

//
// Release ownership for the lock in the current thread.
//
extern "C"
BOOL
CeReleaseRwLock(
    __in HRWLOCK hLock,
    __in CERW_TYPE typeLock
    )
{    
     if (!hLock 
        || ((typeLock != CERW_TYPE_SHARED) 
            && (typeLock != CERW_TYPE_EXCLUSIVE))
        || ((CERW_TYPE_SHARED == typeLock)
            && !IsHeldShared ((PCERWLOCK)hLock))
        || ((CERW_TYPE_EXCLUSIVE == typeLock)
            && !IsExclusiveBitSet ((PCERWLOCK)hLock)))
    {

        //
        // failure cases:
        // a) lock is not valid
        // b) passed in lock type is invalid
        // c) lock is not held in the lock type passed in
        //
        
        ASSERT (0);
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    //
    // Lock is held in the correct mode; go ahead and
    // release the lock for the current thread. Lock
    // will not be completely released for the current
    // thread if the thread recursion count for this lock 
    // is > 0 after the release call below.
    //
    
    return ReleaseLock (hLock, typeLock, FALSE);
}

//
// Acquire the lock in specified mode.
//
extern "C"
BOOL
CeAcquireRwLock(
    __in HRWLOCK hLock,
    __in CERW_TYPE typeLock,
    __in DWORD dwTimeout
    )
{
    BOOL fRet = FALSE;
    
     if (!hLock 
        || ((typeLock != CERW_TYPE_SHARED) 
            && (typeLock != CERW_TYPE_EXCLUSIVE)))
    {
        
        ASSERT (0);
        SetLastError (ERROR_INVALID_PARAMETER);

    } 
    else if (CERW_TYPE_SHARED == typeLock)
    {
        fRet = AcquireLockShared (hLock, dwTimeout);
    }
    else
    {
        fRet = AcquireLockExclusive (hLock, dwTimeout);
    }

    return fRet;
}
    
//
// Delete the memory associated with the lock
//
extern "C"
BOOL
CeDeleteRwLock(
    __in HRWLOCK hLock
    )
{
    if (!hLock) 
    {
        ASSERT (0);
        SetLastError (ERROR_INVALID_PARAMETER);
        return FALSE;

    } 
    else 
    {

        PCERWLOCK pLock = (PCERWLOCK) hLock;
             
        //
        // if the following asserts fire, then there are outstanding
        // locks to this lock while the lock is being deleted
        // - this should not happen!!
        //
        
        ASSERT (pLock->ulGlobalCount == 0);

        CloseHandle (pLock->hWaitObject); // intentionally not signaling the event
        CloseHandle (pLock->hWaitForBoost);
        DeleteCriticalSection (&pLock->csState);
        LocalFree (pLock);
        
        return TRUE;
    }
}

//
// Initialize the lock
//
extern "C"
HRWLOCK
CeCreateRwLock (
    DWORD dwFlags // unused
    )
{
    //
    // Allocate a new lock structure
    //
    
    PCERWLOCK pLock = (PCERWLOCK) LocalAlloc (LPTR, sizeof(CERWLOCK));
    if (pLock) 
    {

        //
        // set the lock structure members    
        //
        
        pLock->hWaitObject = CreateEvent (
                                                    NULL,   // security attributes
                                                    TRUE,   // manual reset
                                                    FALSE,  // initial state: non-signaled
                                                    NULL    // unnamed
                                                    );
        pLock->hWaitForBoost = CreateEvent (
                                                    NULL,   // security attributes
                                                    FALSE,  // auto reset
                                                    FALSE,  // initial state: non-signaled
                                                    NULL    // unnamed
                                                    );

        InitializeCriticalSection (&pLock->csState);

        // free up memory if we failed part way through init
        if (!pLock->hWaitObject || !pLock->hWaitForBoost || !pLock->csState.hCrit) 
        {            
            if (pLock->hWaitObject) 
            {
                CloseHandle (pLock->hWaitObject);
                pLock->hWaitObject = NULL;
            }

            if (pLock->hWaitForBoost) 
            {
                CloseHandle (pLock->hWaitForBoost);
                pLock->hWaitForBoost = NULL;
            }
            
            LocalFree (pLock);
            pLock = NULL;            
        }                
    }
    
    return (HRWLOCK)pLock;
}

//
// Initialize the support for r/w locks in the calling process.
// Called by process load on startup.
//
extern "C"
BOOL
CeInitializeRwLocks(
    void
    )
{
    //
    // Create the TLS slot which is used to store recursion count for all
    // r/w locks held by a thread.
    //
    
    ASSERT (g_dwLockSlot == TLS_OUT_OF_INDEXES);
    g_dwLockSlot = CeTlsAlloc (CeFreeRwTls);
    return (g_dwLockSlot != TLS_OUT_OF_INDEXES);
}


