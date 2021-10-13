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

#include <windows.h>
#include <resmgr.h>
#include "iormif.h"
#include "devzones.h"

#ifdef _PREFAST_
#ifdef DEBUGCHK
#undef DEBUGCHK
#endif // DEBUGCHK
#define DEBUGCHK(exp) PREFAST_ASSUME(exp)
#endif // _PREFAST_


// More than this many distinct resources of a given type need to be represented by a
// sparse table; less than (or equal to) this many can be represented by a bit vector.
// This type of resource is typically used to represent IRQs -- a fixed table size of
// 64 should be sufficient since CE currently limits the number of SysIntrs.
#define DEF_TABLE_THRESHOLD 64

typedef struct {
    DWORD id;
    DWORD len;
} ResourceRange;

typedef struct SparseTableNode {
    ResourceRange res;
    struct SparseTableNode *pNext;
} SparseTableNode;

typedef struct {
    SparseTableNode *pFirst;
} SparseTable;

typedef struct {
    DWORD dwResMax;             // max resource index
    PDWORD prvValid;            // even dense resource lists can have holes
    PDWORD prvShareable;        // indicates which resources are shareable
    PDWORD prvExclusive;        // indicates resources that are requested for exclusive access
    PUSHORT pUseCount;          // records the number of resource users
} DenseTable;

typedef struct {
    DWORD dwSize;
    union {
        SparseTable st;
        DenseTable  dt;
    } u;
} ResourceTable;

typedef struct ResourceDomain {
    DWORD dwResId;              // identifies the resource type
    DWORD dwOffset;             // minimum value for dwId
    ResourceTable table;
    struct ResourceDomain *pNext;
} ResourceDomain;


// -- forward declarations --
#ifdef DEBUG
void ResourceDump (ResourceDomain *);
void ResourceDumpAll (void);
#endif

// A list of available resource domains from which resources can be allocated,
// and a way to protect it for thread-safety.
static ResourceDomain *g_DomainList;
static CRITICAL_SECTION gcsIORM;
static DWORD gdwMaxDenseResources;

// This routine initializes an array of resource bitmasks.  The bitmask vector
// is an array of 32-bit DWORDs.  The max value is 0-relative and inclusive, so
// we add one to get the number of elements.
static void
RBMInit(DWORD dwResMax, __out_bcount((dwResMax+1)/8) const PDWORD pdwMask, BOOL fSet)
{
    DWORD dwSize;
    DEBUGCHK(pdwMask != NULL);

    // convert to byte count
    dwSize = (dwResMax + 1) / 8;

    // set or clear bits in the vector
    memset(pdwMask, fSet ? 0xFF : 0x00, dwSize);
}

// This routine tests whether a particular bit is set in a resource bitmask vector
// and returns TRUE if it is.
static BOOL
RBMIsSet(DWORD dwResMax, DWORD dwElement, __out_bcount((dwResMax+1)/8) DWORD const*const pdwMask)
{
    DWORD dwIndex;
    BOOL fIsSet;
    
    if (dwElement > dwResMax) {
        DEBUGCHK(dwElement <= dwResMax);
        return FALSE;
    }
    DEBUGCHK(pdwMask != NULL);

    // convert the element into a bit and an offset
    dwIndex = dwElement / 32;
    dwElement %= 32;

    // is the bit set in the mask?
    if((pdwMask[dwIndex] & (1 << dwElement)) != 0) {
        fIsSet = TRUE;
    } else {
        fIsSet = FALSE;
    }

    return fIsSet;
}

// this routine sets a bit in a resource bitmask vector
static void
RBMSet(DWORD dwResMax, DWORD dwElement, __out_bcount((dwResMax+1)/8) const PDWORD pdwMask)
{
    DWORD dwIndex;
    
    DEBUGCHK(dwElement <= dwResMax);
    DEBUGCHK(pdwMask != NULL);
    UNREFERENCED_PARAMETER(dwResMax);

    // convert the element into a bit and an offset
    dwIndex = dwElement / 32;
    dwElement %= 32;

    // is the bit set in the mask?
    pdwMask[dwIndex] |= (1 << dwElement);
}

// this routine clears a bit in a resource bitmask vector
static void
RBMClear(DWORD dwResMax, DWORD dwElement, __out_bcount((dwResMax+1)/8) const PDWORD pdwMask)
{
    DWORD dwIndex;
    
    if (dwElement > dwResMax) {
        DEBUGCHK(dwElement <= dwResMax);
        return;
    }
    DEBUGCHK(pdwMask != 0);

    // convert the element into a bit and an offset
    dwIndex = dwElement / 32;
    dwElement %= 32;

    // is the bit set in the mask?
    pdwMask[dwIndex] &= ~(1 << dwElement);
}

static ResourceDomain *FindDomain (DWORD dwResId)
{
    ResourceDomain *pdom;

    pdom = g_DomainList;

    for (; pdom; pdom = pdom->pNext)
        if (pdom->dwResId == dwResId)
            break;

    return pdom;
}

static DWORD SetDenseResources (DenseTable *ptab, DWORD dwId, DWORD dwLen, BOOL fClaim, DWORD dwFlags)
{
    DWORD dwTrav, dwCount;
    
    DEBUGCHK(ptab);
    DEBUGCHK(dwId >= 0);
    DEBUGCHK(dwLen > 0);
    DEBUGCHK(dwId <= ptab->dwResMax);
    DEBUGCHK((dwId + (dwLen - 1)) <= ptab->dwResMax);
    DEBUGCHK(dwFlags == 0 || fClaim);

    if(fClaim) {
        BOOL fExclusive;

        // are we requesting exclusive access to a shared resource?
        if((dwFlags & RREXF_REQUEST_EXCLUSIVE) != 0) {
            fExclusive = TRUE;
        } else {
            fExclusive = FALSE;
        }
        
        // make sure that nobody has claimed any non-shareable resources
        for(dwTrav = dwId, dwCount = 0; dwCount < dwLen; dwTrav++, dwCount++) {
            if(! RBMIsSet(ptab->dwResMax, dwTrav, ptab->prvValid)) {
                // resource isn't valid and can't be allocated
                return ERROR_INVALID_PARAMETER;
            }
            
            if(ptab->pUseCount[dwTrav] != 0) {
                if(! RBMIsSet(ptab->dwResMax, dwTrav, ptab->prvShareable)
                || fExclusive
                || RBMIsSet(ptab->dwResMax, dwTrav, ptab->prvExclusive)) {
                    // the resource is in use but it's (a) not shareable, or (b) we
                    // want it exclusively, or (c) whoever has it wants it exclusively.
                    return ERROR_BUSY;
                }
            }
        }

        // now allocate the resource
        for(dwTrav = dwId, dwCount = 0; dwCount < dwLen; dwTrav++, dwCount++) {
            ptab->pUseCount[dwTrav]++;
            if(fExclusive) {
                DEBUGCHK(ptab->pUseCount[dwTrav] == 1);
                RBMSet(ptab->dwResMax, dwTrav, ptab->prvExclusive);
            }
        }
    } else {
        // validate the resource or make sure it has been allocated -- do this 
        // in two passes so we don't get partially released resources
        for(dwTrav = dwId, dwCount = 0; dwCount < dwLen; dwTrav++, dwCount++) {
            if(!RBMIsSet(ptab->dwResMax, dwTrav, ptab->prvValid)) {
                DEBUGCHK(ptab->pUseCount[dwTrav] == 0xAFAF);
            } else {
                if(ptab->pUseCount[dwTrav] == 0) {
                    return ERROR_INVALID_PARAMETER;
                }
            }
        }

        // validate the resource and/or free it by reducing its reference count
        for(dwTrav = dwId, dwCount = 0; dwCount < dwLen; dwTrav++, dwCount++) {
            if(!RBMIsSet(ptab->dwResMax, dwTrav, ptab->prvValid)) {
                DEBUGCHK(!RBMIsSet(ptab->dwResMax, dwTrav, ptab->prvExclusive));
                RBMSet(ptab->dwResMax, dwTrav, ptab->prvValid);
                ptab->pUseCount[dwTrav] = 0;
            } else {
                DEBUGCHK(ptab->pUseCount[dwTrav] != 0);
                if(RBMIsSet(ptab->dwResMax, dwTrav, ptab->prvExclusive)) {
                    DEBUGCHK(ptab->pUseCount[dwTrav] == 1);
                    RBMClear(ptab->dwResMax, dwTrav, ptab->prvExclusive);
                }
                ptab->pUseCount[dwTrav]--;
            }
        }
    }

    return ERROR_SUCCESS;
}

static DWORD SetSparseResources (SparseTable *ptab, DWORD dwId, DWORD dwLen, BOOL fClaim, DWORD dwFlags)
{
    SparseTableNode **ppnod, *pnod;

    DEBUGCHK(dwFlags == 0 || fClaim);
    if(dwFlags != 0) {
        DEBUGMSG(ZONE_WARNING, (_T("SetSparseResources: flags value 0x%08x not supported\r\n"), dwFlags));
        return ERROR_NOT_SUPPORTED;
    }
    
    for (pnod = *(ppnod = &ptab->pFirst); pnod != NULL; pnod = *(ppnod = &pnod->pNext))
        if (fClaim) {
            if (pnod->res.id <= dwId && pnod->res.id + pnod->res.len - 1 >= dwId)
                break;
        }
        else {
            if (pnod->res.id + pnod->res.len > dwId && pnod->res.id < dwId + dwLen)
                return ERROR_INVALID_PARAMETER; // requested range includes available resources! (serious caller error)
            if (pnod->res.id >= dwId + dwLen || pnod->res.id + pnod->res.len == dwId)
                break;
        }

    if (fClaim) {
        // make sure the resources are free so we can claim them
        if (pnod == NULL)
            return ERROR_INVALID_PARAMETER;  // dwId is unavailable
        if (dwId + dwLen  > pnod->res.id + pnod->res.len)
            return ERROR_BUSY;  // some subset of the range is unavailable

        // OK, we can claim. Now adjust the list. There are several cases.
        ASSERT(pnod->res.len >= dwLen);
        if (pnod->res.len == dwLen) {
            // delete the entire current range
            ASSERT(pnod->res.id == dwId);
            *ppnod = pnod->pNext;
            LocalFree(pnod);
        }
        else if (pnod->res.id == dwId) {
            pnod->res.id += dwLen;
            pnod->res.len -= dwLen;
        }
        else if (pnod->res.id + pnod->res.len == dwId + dwLen) {
            pnod->res.len -= dwLen;
        }
        else {
            // we have to split the current resource range
            SparseTableNode *newnod = LocalAlloc(0, sizeof(*newnod));
            if (newnod == NULL)
                return ERROR_NOT_ENOUGH_MEMORY;  // OOM!
            newnod->res.id = dwId + dwLen;
            newnod->res.len = pnod->res.id + pnod->res.len - newnod->res.id;
            newnod->pNext = pnod->pNext;
            pnod->pNext = newnod;
            pnod->res.len -= dwLen + newnod->res.len;
        }
    }
    else {
        // make sure the resources are claimed so we can free them
        if (pnod == NULL) {
            // case 6 or 7 (same as case 1 or 3)
          mknew: {
            SparseTableNode *newnod = LocalAlloc(0, sizeof(*newnod));
            if (newnod == NULL)
                return ERROR_NOT_ENOUGH_MEMORY;  // OOM!
            newnod->res.id = dwId;
            newnod->res.len = dwLen;
            newnod->pNext = pnod;
            *ppnod = newnod;
          }}
        else if (pnod->res.id + pnod->res.len == dwId) {
            // case 2 or 5
            SparseTableNode *pnxt = pnod->pNext;
            if (pnxt && pnxt->res.id < dwId + dwLen)
                return ERROR_INVALID_PARAMETER;  // cannot free resources already available!
            pnod->res.len += dwLen;
            if (pnxt && pnxt->res.id == dwId + dwLen) {
                // case 5
                pnod->res.len += pnxt->res.len;
                pnod->pNext = pnxt->pNext;
                LocalFree(pnxt);
            }
        }
        else if (pnod->res.id == dwId + dwLen) {
            // case 4
            pnod->res.id -= dwLen;
            pnod->res.len += dwLen;
        }
        else {
            ASSERT(dwId + dwLen < pnod->res.id);
            // case 1 or 3
            goto mknew;  // see case 6 or 7
        }
    }

    return ERROR_SUCCESS;
}

BOOL IORM_ResourceMarkAsShareable(DWORD dwResId, DWORD dwId, DWORD dwLen, BOOL fShareable)
{
    ResourceDomain *pdom;
    DWORD size, min;
    DWORD dwTrav, dwCount;
    DWORD dwError = ERROR_INVALID_PARAMETER;
    BOOL fOk;

    DEBUGMSG(ZONE_RESMGR, (_T("IORM: marking 0x%x resources at 0x%08x as %s in list 0x%08x\r\n"), 
        dwLen, dwId, fShareable ? _T("shareable") : _T("unshareable"), dwResId));

    EnterCriticalSection(&gcsIORM);

    __try {
        if ((pdom = FindDomain(dwResId)) == NULL) {
            goto done;  // no such resource
        }
        
        min = pdom->dwOffset;
        if (dwId < min) {
            goto done;  // out of range
        }
        
        /// translate the resource to the domain's address space
        dwId -= min;
        
        size = pdom->table.dwSize;
        if (dwId >= size || dwLen > size - dwId) {
            goto done;  // out of range
        }
        
        if (size <= gdwMaxDenseResources) {
            DenseTable *ptab = &pdom->table.u.dt;
            
            // make sure that none of the resources are in use or invalid
            for(dwTrav = dwId, dwCount = 0; dwCount < dwLen; dwTrav++, dwCount++) {
                if(!RBMIsSet(ptab->dwResMax, dwTrav, ptab->prvValid)
                    || ptab->pUseCount[dwTrav] != 0) {
                    break;
                }
            }
            
            // did we get to the end of the list?
            if(dwCount != dwLen) {
                dwError = ERROR_INVALID_PARAMETER;
            } else {
                for(dwTrav = dwId, dwCount = 0; dwCount < dwLen; dwTrav++, dwCount++) {
                    if(fShareable) {
                        RBMSet(pdom->table.u.dt.dwResMax, dwTrav, pdom->table.u.dt.prvShareable);
                    } else {
                        RBMClear(pdom->table.u.dt.dwResMax, dwTrav, pdom->table.u.dt.prvShareable);
                    }
                }
                dwError = ERROR_SUCCESS;
            }
        } else {
            DEBUGMSG(ZONE_WARNING || ZONE_RESMGR,
                (_T("IORM_ResourceMarkAsShareable: shared space size of %u exceeds maximum of %u\r\n"),
                size, gdwMaxDenseResources));
            dwError = ERROR_NOT_SUPPORTED;
            goto done;  // cannot set shared sparse resources
        }
done:
        ;       // need this to make the compiler happy
    } 
    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_GEN_FAILURE;
        fOk = FALSE;
    }

    LeaveCriticalSection(&gcsIORM);
    
    if(dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
        fOk = FALSE;
    } else {
        fOk = TRUE;
    }

    DEBUGMSG(!fOk && (ZONE_WARNING || ZONE_RESMGR), 
        (_T("IORM: mark as %s returning %d, error code %d\r\n"), 
        fShareable ? _T("shareable") : _T("unshareable"), fOk, dwError));

    return fOk;
}

// Referenced by device.c.  This routine is now only used for
// resource release.
BOOL WINAPI IORM_ResourceAdjust (DWORD dwResId, DWORD dwId, DWORD dwLen, BOOL fClaim)
{
    ResourceDomain *pdom = NULL;
    DWORD size, min;
    DWORD dwError = ERROR_SUCCESS;
    BOOL fOk;

    DEBUGCHK(!fClaim);

    DEBUGMSG(ZONE_RESMGR, (_T("IORM: releasing 0x%x resources at 0x%x from list 0x%08x\r\n"), 
        dwLen, dwId, dwResId));

    EnterCriticalSection(&gcsIORM);

    __try {
        if ((pdom = FindDomain(dwResId)) == NULL) {
            dwError = ERROR_FILE_NOT_FOUND;
            goto done;  // no such resource
        }
        
        min = pdom->dwOffset;
        if (dwId < min) {
            dwError = ERROR_INVALID_PARAMETER;
            goto done;  // out of range
        }
        
        // translate the resource to the domain's address space
        dwId -= min;
        
        size = pdom->table.dwSize;
        if (dwId >= size || dwLen > size - dwId) {
            dwError = ERROR_INVALID_PARAMETER;
            goto done;  // out of range
        }
        
        if (size <= gdwMaxDenseResources) {
            dwError = SetDenseResources(&pdom->table.u.dt, dwId, dwLen, fClaim, 0);
        } else {
            dwError = SetSparseResources(&pdom->table.u.st, dwId, dwLen, fClaim, 0);
        }
        
done:
        ;   // need this to make the compiler happy
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_GEN_FAILURE;
    }

    LeaveCriticalSection(&gcsIORM);

    if(dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
        fOk = FALSE;
    } else {
        fOk = TRUE;
    }

    DEBUGMSG(!fOk && (ZONE_WARNING || ZONE_RESMGR), 
        (_T("IORM: release returning %d, error code %d\r\n"), fOk, dwError));

#ifdef DEBUG
    if(fOk && ZONE_RESMGR)
        ResourceDump(pdom);
#endif

    return fOk;
}


BOOL 
IORM_ResourceRequestEx(DWORD dwResId, DWORD dwId, DWORD dwLen, DWORD dwFlags)
{
    ResourceDomain *pdom = NULL;
    DWORD size, min;
    BOOL fOk;
    DWORD dwError = ERROR_SUCCESS;

    DEBUGMSG(ZONE_RESMGR, (_T("IORM: requesting 0x%x resources at 0x%x from list 0x%08x, flags 0x%08x\r\n"), 
        dwLen, dwId, dwResId, dwFlags));

    // validate flags
    if((dwFlags & RREXF_REQUEST_EXCLUSIVE) != dwFlags) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    EnterCriticalSection(&gcsIORM);

    __try {
        if ((pdom = FindDomain(dwResId)) == NULL) {
            dwError = ERROR_FILE_NOT_FOUND;
            goto done;  // no such resource
        }
        
        min = pdom->dwOffset;
        if (dwId < min) {
            dwError = ERROR_INVALID_PARAMETER;
            goto done;  // out of range
        }
        
        // translate the resource to the domain's address space
        dwId -= min;
        
        size = pdom->table.dwSize;
        if (dwId >= size || dwLen > size - dwId) {
            dwError = ERROR_INVALID_PARAMETER;
            goto done;  // out of range
        }
        
        if (size <= gdwMaxDenseResources) {
            dwError = SetDenseResources(&pdom->table.u.dt, dwId, dwLen, TRUE, dwFlags);
        } else {
            dwError = SetSparseResources(&pdom->table.u.st, dwId, dwLen, TRUE, dwFlags);
        }
        
done:
        ;   // need this to make the compiler happy
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_GEN_FAILURE;
    }

    LeaveCriticalSection(&gcsIORM);

    if(dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
        fOk = FALSE;
    } else {
        fOk = TRUE;
    }

    DEBUGMSG(!fOk && ZONE_RESMGR, 
        (_T("IORM: request returning %d, error code %d\r\n"), fOk, dwError));

#ifdef DEBUG
    if (fOk && ZONE_RESMGR)
        ResourceDump(pdom);
#endif

    return fOk;
}

// Call this to tell the OS what resource types are available.
// Creates a resource domain with zero available resources.
// OEM can call ResourceRelease to make resources available.
BOOL IORM_ResourceCreateList (DWORD dwResId, DWORD dwMinimum, DWORD dwCount)
{
    ResourceDomain *ndom;
    DWORD dwSize;
    BOOL fOk;
    DWORD dwError = ERROR_SUCCESS;

    DEBUGMSG(ZONE_RESMGR, (_T("IORM: creating list id 0x%08x - minimum is 0x%x, count is 0x%x\r\n"), 
        dwResId, dwMinimum, dwCount));

    EnterCriticalSection(&gcsIORM);

    __try {
        if (FindDomain(dwResId) != NULL) {
            dwError = ERROR_ALREADY_EXISTS;
            goto done;  // domain already exists
        }
        
        if (dwCount < 1) {
            dwError = ERROR_INVALID_PARAMETER;
            goto done;  // invalid parameters
        }
        
        // create an appropriate domain
        dwSize = sizeof(*ndom);
        if(dwCount <= gdwMaxDenseResources) {
            dwSize += dwCount * sizeof(USHORT);     // pUseCount array
            dwSize += 3 * sizeof(DWORD) * ((dwCount + 31) / 32);    // prvValid + prvShareable + prvExclusive
        }
        
        ndom = LocalAlloc(0, dwSize);
#ifdef DEBUG
        memset(ndom, 0xAF, dwSize);
#endif  // DEBUG
        if (ndom == NULL) {
            dwError = ERROR_NOT_ENOUGH_MEMORY;
            goto done;  // OOM!
        }
        
        ndom->dwResId = dwResId;
        ndom->table.dwSize = dwCount;
        ndom->dwOffset = dwMinimum;
        if (dwCount <= gdwMaxDenseResources) {
            DWORD dwVectorSize = (dwCount + 31) / 32;   // number of DWORDs in vectors
            PBYTE pbTmp = (PBYTE) ndom;             // pointer to our new structure
            
            // populate the domain structure's dense table member
            pbTmp += sizeof(*ndom);
            ndom->table.u.dt.prvValid = (PDWORD) pbTmp;
            pbTmp += dwVectorSize * sizeof(DWORD);
            ndom->table.u.dt.prvShareable = (PDWORD) pbTmp;
            pbTmp += dwVectorSize * sizeof(DWORD);
            ndom->table.u.dt.prvExclusive = (PDWORD) pbTmp;
            pbTmp += dwVectorSize * sizeof(DWORD);
            ndom->table.u.dt.pUseCount = (PUSHORT) pbTmp;
            
            // Fill in vector and array values.  Assume all resources
            // are invalid and unshareable.  We will initialize the use
            // count when the resource is validated.
            ndom->table.u.dt.dwResMax = dwCount - 1;
            RBMInit((dwVectorSize * 32) - 1, ndom->table.u.dt.prvValid, FALSE);
            RBMInit((dwVectorSize * 32) - 1, ndom->table.u.dt.prvShareable, FALSE);
            RBMInit((dwVectorSize * 32) - 1, ndom->table.u.dt.prvExclusive, FALSE);
        } else {
            ndom->table.u.st.pFirst = NULL;
        }
        
        // link it to the domain list
        ndom->pNext = g_DomainList;
        g_DomainList = ndom;
        
done:
        ;   // need this to make the compiler happy
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_GEN_FAILURE;
    }

    LeaveCriticalSection(&gcsIORM);

    if(dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
        fOk = FALSE;
    } else {
        fOk = TRUE;
    }

    DEBUGMSG(!fOk && (ZONE_WARNING || ZONE_RESMGR), (_T("IORM: creation of list id 0x%08x returned %d, error code %d\r\n"), 
        dwResId, fOk, dwError));

    return fOk;
}

BOOL 
IORM_ResourceDestroyList(DWORD dwResId)
{
    DWORD dwError = ERROR_SUCCESS;
    BOOL fOk;

    DEBUGMSG(ZONE_RESMGR, (_T("IORM: destroying list id 0x%08x\r\n"), dwResId));

    EnterCriticalSection(&gcsIORM);

    __try {
        ResourceDomain *pdom, *pdomPrevious;

        // look for the resource domain structure
        for (pdom = g_DomainList, pdomPrevious = NULL; pdom; pdomPrevious = pdom, pdom = pdom->pNext) {
            if (pdom->dwResId == dwResId) {
                break;      // found a match, exit the loop
            }
        }
        
        if(pdom == NULL) {
            dwError = ERROR_FILE_NOT_FOUND;
            goto done;  // no such resource
        }

        // handle deletion differently for dense and sparse tables
        if (pdom->table.dwSize <= gdwMaxDenseResources) {
            // make sure nobody is using the resources
            DWORD dwTrav = 0;
            DenseTable *ptab = &pdom->table.u.dt;
            for(dwTrav = 0; dwTrav < pdom->table.dwSize; dwTrav++) {
                if(ptab->pUseCount[dwTrav] != 0
                && RBMIsSet(ptab->dwResMax, dwTrav, ptab->prvValid)) {
                    dwError = ERROR_BUSY;
                    break;
                }
            }
        } else {
            // deallocate the list of free ranges -- we don't keep track
            // of which non-free resources have already been handed out.
            SparseTableNode *pnode = pdom->table.u.st.pFirst;
            while(pnode != NULL) {
                SparseTableNode *pNext = pnode->pNext;
                LocalFree(pnode);
                pnode = pNext;
            }
        }

        // can we delete the entry?
        if(dwError == ERROR_SUCCESS) {
            // yes, remove it from the list and free it
            if(pdomPrevious != NULL) {
                DEBUGCHK(pdomPrevious->pNext == pdom);
                pdomPrevious->pNext = pdom->pNext;
            } else {
                g_DomainList = pdom->pNext;
            }
            LocalFree(pdom);
        }

done:
        ;   // need this to keep the compiler happy
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        dwError = ERROR_GEN_FAILURE;
    }
    
    LeaveCriticalSection(&gcsIORM);

    if(dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
        fOk = FALSE;
    } else {
        fOk = TRUE;
    }

    DEBUGMSG(!fOk && (ZONE_WARNING || ZONE_RESMGR), 
        (_T("IORM: destruction of list id 0x%08x returned %d, error code %d\r\n"), 
        dwResId, fOk, dwError));

    return fOk;
}


// Parse a registry key to set up initially available resources
// Looks like:
//      ResourceName\
//              "Identifier"=dword:1
//              "Minimum"=dword:1
//              "Space"=dword:16
//              "Ranges"="1-15"
//              "Shared"="5,7-9"

static BOOL ScanRange (WCHAR **ps, DWORD *a, DWORD *b)
{
    if (**ps == L'\0')
        return FALSE;  // end of string
    
    *a = wcstoul(*ps, ps, 0);
    if (**ps == L'-')
        *b = wcstoul(*ps+1, ps, 0);
    else
        *b = *a;

    if (*b < *a)
        return FALSE; // invalid range
    else
        *b -= *a - 1;  // turn it into a length

    // incr past the comma (if present) and make
    // sure the char after the comma is not \0.
    // it's ok for **ps to be \0 imm after a conversion.
    if (**ps == L'\0' || **ps == L',' && *++*ps != L'\0')
        return TRUE;
    
    return FALSE; // malformed range
}
static BOOL ResourceInitReserved(HKEY hk,DWORD dwResID)
{
#define MAX_KEYNAME 0x100

    DWORD dwRegIndex = 0 ;
    WCHAR  szName[MAX_KEYNAME];
    union { 
        WCHAR   szVal[MAX_KEYNAME];
        DWORD   dwVal;
    } RegValue;
    DWORD   dwType;
    DWORD   dwNameSize = _countof (szName);
    DWORD   dwValSize = sizeof(RegValue);
    LONG    lStatus;
        
    while ((lStatus=RegEnumValue(hk, dwRegIndex, szName, &dwNameSize, 0, &dwType,(LPBYTE)&RegValue, &dwValSize)) == ERROR_SUCCESS) {
        switch (dwType) {
          case REG_SZ:{
            DWORD a[2];  // no Resource API takes more than 3 parms
            WCHAR *pos = RegValue.szVal;
            DWORD dataLength ;
            if (dwValSize> sizeof(RegValue.szVal)- 2* sizeof(WCHAR)) {
                DEBUGMSG(ZONE_ERROR,(TEXT("ResourceInitReserved(dwResID=%d),: Not enough buffer. Registry value truncated"),dwResID));
                dataLength = sizeof(RegValue.szVal)- 2* sizeof(WCHAR);
            }
            else
                dataLength = dwValSize;
            dataLength /= sizeof(WCHAR);
            RegValue.szVal[dataLength] = RegValue.szVal[dataLength + 1]  = 0 ; // Double terminate for multi string.
            
            while (ScanRange(&pos, &a[0], &a[1])) {
                VERIFY(IORM_ResourceRequestEx(dwResID, a[0], a[1], RREXF_REQUEST_EXCLUSIVE));
            }
            
          }
            break;
          case REG_DWORD: {
            VERIFY(IORM_ResourceRequestEx(dwResID, RegValue.dwVal, 1 , RREXF_REQUEST_EXCLUSIVE));
          }
            break;
          default:
            DEBUGMSG(ZONE_ERROR,(TEXT("ResourceInitReserved(dwResID=%d): Unknown Registry type(%d)"),dwResID,dwType));
            break;
        }
        dwRegIndex++;
        dwNameSize = _countof (szName);
        dwValSize = sizeof(RegValue);
    }
    DEBUGMSG(ZONE_WARNING && lStatus!=ERROR_NO_MORE_ITEMS,(TEXT("ResourceInitReserved(dwResID=%d): RegEnumValue un-expected Error (%d)"),dwResID,lStatus));
    return TRUE;
}

static void ResourceInitFromKey (HKEY hk)
{
    HKEY hReservedKey ;
    DWORD valsz, rsz;
    WCHAR *valbuf, *p;
    DWORD a[3];  // no Resource API takes more than 3 parms

    if (RegQueryInfoKey(hk, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &valsz, NULL, NULL) != ERROR_SUCCESS)
        return;
    valsz = ((valsz + 1 )/sizeof(WCHAR) + 2) * sizeof(WCHAR);
    __try {
        valbuf = malloc(valsz);
    } 
    __except(EXCEPTION_EXECUTE_HANDLER) {
        valbuf = NULL;
    }

    // create a default 
    if (valbuf == NULL ||
        RegQueryValueEx(hk, L"Identifier", NULL, NULL, (PBYTE)&a[0], ((rsz = sizeof(DWORD)), &rsz)) != ERROR_SUCCESS ||
        RegQueryValueEx(hk, L"Minimum", NULL, NULL, (PBYTE)&a[1], ((rsz = sizeof(DWORD)), &rsz)) != ERROR_SUCCESS ||
        RegQueryValueEx(hk, L"Space", NULL, NULL, (PBYTE)&a[2], ((rsz = sizeof(DWORD)), &rsz)) != ERROR_SUCCESS ||
        IORM_ResourceCreateList(a[0], a[1], a[2]) == FALSE) {
        return;
    }

    // initialize resources based on the registry

    if (RegQueryValueEx(hk, L"Ranges", NULL, NULL, (PBYTE)valbuf, ((rsz = valsz), &rsz)) == ERROR_SUCCESS) {
        p = valbuf;
        // Force Terminate if it is not.
        p[valsz/sizeof(WCHAR)-1] = p[valsz/sizeof(WCHAR)-2] = 0;
        while (ScanRange(&p, &a[1], &a[2]))
            IORM_ResourceAdjust(a[0], a[1], a[2], FALSE);   // mark ranges as valid
    }
    if (RegQueryValueEx(hk, L"Shared", NULL, NULL, (PBYTE)valbuf, ((rsz = valsz), &rsz)) == ERROR_SUCCESS) {
        p = valbuf;
        // Force Terminate if it is not.
        p[valsz/sizeof(WCHAR)-1] = p[valsz/sizeof(WCHAR)-2] = 0;
        while (ScanRange(&p, &a[1], &a[2])) {
            VERIFY(IORM_ResourceMarkAsShareable(a[0], a[1], a[2], TRUE));
        }
    }
    if (RegOpenKeyEx( hk, L"Reserved", 0, 0, &hReservedKey) == ERROR_SUCCESS) {
        ResourceInitReserved(hReservedKey,a[0]);
        RegCloseKey(hReservedKey);
    }

    if (valbuf)
        free(valbuf);
}

void ResourceInitFromRegistry (LPCTSTR key)
{
    HKEY hk, hk2;
    WCHAR *sknm = NULL;
    DWORD i, sksz, rsz;

    DEBUGCHK( key != NULL );    // not a published API so no need for a retail runtime check
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, 0, &hk) != ERROR_SUCCESS ||
        RegQueryInfoKey(hk, NULL, NULL, NULL, NULL, &sksz, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
        return;

    // get the max table size
    rsz = sizeof(i);
    if(RegQueryValueEx(hk, _T("MaxDenseResources"), NULL, NULL, (LPBYTE) &i, &rsz) == ERROR_SUCCESS) {
        DEBUGMSG(ZONE_INIT, (_T("ResourceInitFromRegistry: max resources set to %d\r\n"),
            gdwMaxDenseResources));
        gdwMaxDenseResources = i;
    }      
    
    if (sksz+1 > sksz) {
        sksz += 1; // account for termination
        if (sksz * sizeof(*sknm) > sksz) {
            sksz *= sizeof(*sknm); // convert to bytes
            __try {
                sknm = malloc(sksz);
            } 
            __except(EXCEPTION_EXECUTE_HANDLER) {
                sknm = NULL;
            }
        } else {
            ASSERT(FALSE);
        }
    } else {
        ASSERT(FALSE);
    }
    
    if (sknm != NULL) {
        sksz /= sizeof(*sknm); // back to unit.
        for (i=0; RegEnumKeyEx(hk, i, sknm, ((rsz = sksz), &rsz), NULL, NULL, NULL, NULL) == ERROR_SUCCESS; ++i) {
            if (RegOpenKeyEx(hk, sknm, 0, 0, &hk2) == ERROR_SUCCESS) {
                ResourceInitFromKey(hk2);
                RegCloseKey(hk2);
            }
        }

        free(sknm);
    }
    
    RegCloseKey(hk);
}

// *** Must be first function called and should only be called once!
// Initializes global variables in this module.
void ResourceInitModule (void)
{
    DEBUGCHK(g_DomainList == NULL); // bss data must be initted to zero by the crt
    g_DomainList = NULL;
    InitializeCriticalSection(&gcsIORM);
    gdwMaxDenseResources = DEF_TABLE_THRESHOLD;
}


#ifdef DEBUG

#ifndef UNIT_TEST
#define OUTPUT(a) DEBUGMSG(1, a)
#else
#define OUTPUT(a) wprintf a
#endif

void ResourceDump (ResourceDomain *pdom)
{
    if (pdom->table.dwSize <= gdwMaxDenseResources) {
        DWORD dwVectorEntries = (pdom->table.dwSize + 31) / 32;
        DWORD dwTrav;
        OUTPUT((TEXT("Resource id 0x%08x has range 0x%x - 0x%x (size 0x%x (%u): DENSE)\n"), 
            pdom->dwResId, pdom->dwOffset, pdom->dwOffset + pdom->table.dwSize - 1, 
            pdom->table.dwSize, pdom->table.dwSize));
        for(dwTrav = 0; dwTrav < dwVectorEntries; dwTrav++) {
            DWORD dwInUse = 0;
            DWORD dwBaseId = dwTrav * 32;
            DWORD dwMaxId = dwBaseId + 32;
            DWORD dwId;

            // figure out which drivers are in use
            if(dwMaxId > pdom->table.dwSize) {
                dwMaxId = pdom->table.dwSize;
            }
            dwMaxId -= dwBaseId;
            for(dwId = 0; dwId < dwMaxId; dwId++) {
                if(RBMIsSet(pdom->table.dwSize, dwId + dwBaseId, pdom->table.u.dt.prvValid)
                && pdom->table.u.dt.pUseCount[dwId + dwBaseId] != 0) {
                    dwInUse |= (1 << dwId);
                }
            }

            // display the status
            OUTPUT((TEXT("\tValid/Allocated/Shareable/exclusive %d: 0x%08x 0x%08x 0x%08x\n"), 
                dwTrav, pdom->table.u.dt.prvValid[dwTrav], dwInUse, pdom->table.u.dt.prvShareable[dwTrav],
                pdom->table.u.dt.prvExclusive[dwTrav]));
        }
    } else {
        SparseTableNode *pnod;
        OUTPUT((TEXT("Resource id 0x%08x has range 0x%08x - 0x%08x  (size 0x%08x (%u): SPARSE)\n"), 
            pdom->dwResId, pdom->dwOffset, pdom->dwOffset + pdom->table.dwSize - 1, 
            pdom->table.dwSize, pdom->table.dwSize));
        OUTPUT((TEXT("\tAvailable resources:")));
        pnod = pdom->table.u.st.pFirst;
        if (pnod == NULL) OUTPUT((TEXT(" *none*")));
        while (pnod) {
            if (pnod->res.len == 0) OUTPUT((TEXT(" 0x%x/*ZEROLENGTH*"), pnod->res.id));
            else if (pnod->res.len == 1) OUTPUT((TEXT(" 0x%x"), pnod->res.id));
            else OUTPUT((TEXT(" 0x%x-0x%x"), pnod->res.id,  pnod->res.id + pnod->res.len - 1));
            pnod = pnod->pNext;
        }
    }
    OUTPUT((TEXT("\n")));
}

void ResourceDumpAll (void)
{
    ResourceDomain *pdom;

    EnterCriticalSection(&gcsIORM);

    pdom = g_DomainList;

    if (pdom == NULL)
        OUTPUT((TEXT("* There are no resource domains defined. *\n")));

    while (pdom) {
        ResourceDump(pdom);
        pdom = pdom->pNext;
        if (pdom)
            OUTPUT((TEXT("\n")));
    }

    LeaveCriticalSection(&gcsIORM);
}

#endif

#ifdef UNIT_TEST
main() {
    char buf[50], c;
    DWORD r, a[3];
    while (1) {
        printf("RES> "); fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin) == NULL) exit(0);
        r = sscanf(buf, "%c %d %d %d", &c, &a[0], &a[1], &a[2]);
        if (r < 1) continue;
        switch (tolower(c)) {
          case 'd':
            if (r == 1)
                ResourceDumpAll();
            else {
                ResourceDomain *d = FindDomain(a[0]);
                if (d == NULL) OUTPUT((TEXT("* No resource domain with id %u. *\n"), a[0]));
                else ResourceDump(d);
            }
            break;
          case 'c':
            if (r < 4) {
                OUTPUT((TEXT("* Create resource domain: c resid min cnt *\n")));
                continue;
            }
            if (ResourceCreateList(a[0], a[1], a[2]))
                OUTPUT((TEXT("Created.\n")));
            else
                OUTPUT((TEXT("FAILED.\n")));
            break;
          case 'g':
            if (r < 3) {
                OUTPUT((TEXT("* Request resources: g resid id [len] *\n")));
                continue;
            }
            if (r < 4)
                a[2] = 1;
            if (ResourceRequest(a[0], a[1], a[2]))
                OUTPUT((TEXT("Granted.\n")));
            else
                OUTPUT((TEXT("DENIED.\n")));
            break;
          case 'f':
            if (r < 3) {
                OUTPUT((TEXT("* Release resources: f resid id [len] *\n")));
                continue;
            }
            if (r < 4)
                a[2] = 1;
            if (ResourceRelease(a[0], a[1], a[2]))
                OUTPUT((TEXT("Released.\n")));
            else
                OUTPUT((TEXT("DENIED.\n")));
            break;
          case 's':
            if (r < 3) {
                OUTPUT((TEXT("* Make resources shareable: s resid id [len] *\n")));
                continue;
            }
            if (r < 4)
                a[2] = 1;
            if (IORM_ResourceMarkAsShareable(a[0], a[1], a[2], TRUE))
                OUTPUT((TEXT("Done.\n")));
            else
                OUTPUT((TEXT("FAILED.\n")));
            break;
          case '$':
            exit(0);
          case '?': case 'h':
            OUTPUT((TEXT("c=create g=request f=release s=share d=dump $=exit\n")));
            break;
        }
    }
}
#endif /* UNIT_TEST */
