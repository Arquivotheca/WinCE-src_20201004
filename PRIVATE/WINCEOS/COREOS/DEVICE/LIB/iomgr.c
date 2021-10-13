//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include "devloadp.h"

// More than this many distinct resources of a given type need to be represented by a
// sparse table; less than (or equal to) this many can be represented by a bit vector.
// This type of resource is typically used to represent IRQs -- a fixed table size of
// 64 should be sufficient since CE currently limits the number of SysIntrs.
#define TABLE_THRESHOLD 64

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
    ULONGLONG vb[1];                // bit vector for dwId's
    ULONGLONG vbs[1];               // bit vector for shareable resources
} DenseTable;

typedef struct {
    DWORD dwSize;
    union {
        SparseTable st;
        DenseTable  dt;
    };
} ResourceTable;

typedef struct ResourceDomain {
    DWORD dwResId;              // identifies the resource type
    DWORD dwOffset;             // minimum value for dwId
    CRITICAL_SECTION cs;
    ResourceTable table;
    struct ResourceDomain *pNext;
} ResourceDomain;


// -- forward declarations --
#ifdef DEBUG
void ResourceDump (ResourceDomain *);
void ResourceDumpAll (void);
#endif

// -- private data and text --

// A list of available resource domains from which resources can be allocated,
// and a way to protect it for thread-safety.
static ResourceDomain *g_DomainList;
static CRITICAL_SECTION g_csDomainList;

static ResourceDomain *FindDomain (DWORD dwResId)
{
    ResourceDomain *pdom;

    EnterCriticalSection(&g_csDomainList);
    pdom = g_DomainList;
    LeaveCriticalSection(&g_csDomainList);

    for (; pdom; pdom = pdom->pNext)
        if (pdom->dwResId == dwResId)
            break;

    return pdom;
}

// Computes a bitmask starting at bit `bit' and being `len' bits long.
static __inline ULONGLONG BITRANGE (int bit, int len)
{
    DEBUGCHK(bit >= 0);
    DEBUGCHK(bit < TABLE_THRESHOLD);
    DEBUGCHK(len > 0);
    DEBUGCHK((bit + len) < TABLE_THRESHOLD);

    return ((~0x0ui64 >> (64 - bit - len)) & (~0x0ui64 << bit));
}

static BOOL SetDenseResources (DenseTable *ptab, DWORD dwId, DWORD dwLen, BOOL fClaim)
{
    // This stuff assumes that the vb[] array has only 1 element!
    ULONGLONG resmask = BITRANGE(dwId, dwLen);

    // Shareable resources can be requested and released arbitrarily
    ULONGLONG shrmask = ptab->vbs[0];
    resmask &= ~shrmask;

    if ((ptab->vb[0] & resmask) != (fClaim ? resmask : 0))
        return FALSE;  // resource conflict

    ptab->vb[0] ^= resmask;

    return TRUE;
}

static BOOL SetDenseShared (DenseTable *ptab, DWORD dwId, DWORD dwLen, BOOL fClaim)
{
    // This stuff assumes that the vbs[] array has only 1 element!
    ULONGLONG resmask = BITRANGE(dwId, dwLen);

    // Cannot un/share an unavailable resource.
    if ((ptab->vb[0] & resmask) != resmask)
        return FALSE;

    if ((ptab->vbs[0] & resmask) != (fClaim ? resmask : 0))
        return FALSE;  // share conflict

    ptab->vbs[0] ^= resmask;

    return TRUE;
}

static BOOL SetSparseResources (SparseTable *ptab, DWORD dwId, DWORD dwLen, BOOL fClaim)
{
    SparseTableNode **ppnod, *pnod;
    
    for (pnod = *(ppnod = &ptab->pFirst); pnod != NULL; pnod = *(ppnod = &pnod->pNext))
        if (fClaim) {
            if (pnod->res.id <= dwId && pnod->res.id + pnod->res.len - 1 >= dwId)
                break;
        }
        else {
            if (pnod->res.id + pnod->res.len > dwId && pnod->res.id < dwId + dwLen)
                return FALSE; // requested range includes available resources! (serious caller error)
            if (pnod->res.id >= dwId + dwLen || pnod->res.id + pnod->res.len == dwId)
                break;
        }

    if (fClaim) {
        // make sure the resources are free so we can claim them
        if (pnod == NULL)
            return FALSE;  // dwId is unavailable
        if (dwId + dwLen  > pnod->res.id + pnod->res.len)
            return FALSE;  // some subset of the range is unavailable

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
                return FALSE;  // OOM!
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
                return FALSE;  // OOM!
            newnod->res.id = dwId;
            newnod->res.len = dwLen;
            newnod->pNext = pnod;
            *ppnod = newnod;
          }}
        else if (pnod->res.id + pnod->res.len == dwId) {
            // case 2 or 5
            SparseTableNode *pnxt = pnod->pNext;
            if (pnxt && pnxt->res.id < dwId + dwLen)
                return FALSE;  // cannot free resources already available!
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

    return TRUE;
}

static BOOL AdjustShared (DWORD dwResId, DWORD dwId, DWORD dwLen, BOOL fClaim)
{
    ResourceDomain *pdom;
    DWORD size, min;
    
    if ((pdom = FindDomain(dwResId)) == NULL)
        return FALSE;  // no such resource

    min = pdom->dwOffset;
    if (dwId < min)
        return FALSE;  // out of range

    /// translate the resource to the domain's address space
    dwId -= min;

    size = pdom->table.dwSize;
    if (dwId >= size || dwId + dwLen > size)
        return FALSE;  // out of range
    
    if (size <= TABLE_THRESHOLD) {
        ASSERT(!fClaim);  // don't unshare resources
        return SetDenseShared(&pdom->table.dt, dwId, dwLen, fClaim);
    } else {
        DEBUGMSG(ZONE_WARNING || ZONE_RESMGR,
            (_T("AdjustShared: shared space size of %u exceeds maximum of %u\r\n"),
            size, TABLE_THRESHOLD));
        return FALSE;  // cannot set shared sparse resources
    }
}

// -- end of private --

// *** Must be first function called and should only be called once!
// Initializes global variables in this module.
void ResourceInitModule (void)
{
    DEBUGCHK(g_DomainList == NULL); // bss data must be initted to zero by the crt
    g_DomainList = NULL;
    InitializeCriticalSection(&g_csDomainList);
}

// Call this to tell the OS what resource types are available.
// Creates a resource domain with zero available resources.
// OEM can call ResourceRelease to make resources available.
BOOL FS_ResourceCreateList (DWORD dwResId, DWORD dwMinimum, DWORD dwCount)
{
    ResourceDomain *ndom;

    if (FindDomain(dwResId) != NULL)
        return FALSE;  // domain already exists

    if (dwMinimum + dwCount - 1 < dwMinimum)
        return FALSE;  // invalid parameters

    // create an appropriate domain
    ndom = LocalAlloc(0, sizeof(*ndom));
    if (ndom == NULL)
        return FALSE;  // OOM!

    ndom->dwResId = dwResId;
    ndom->table.dwSize = dwCount;
    ndom->dwOffset = dwMinimum;
    if (dwCount <= TABLE_THRESHOLD) {
        int i;
        for (i=0; i<sizeof(ndom->table.dt.vb)/sizeof(ndom->table.dt.vb[0]); ++i)
            ndom->table.dt.vb[i] = ndom->table.dt.vbs[i] = 0;
    } else {
        ndom->table.st.pFirst = NULL;
    }
    InitializeCriticalSection(&ndom->cs);

    // link it to the domain list
    EnterCriticalSection(&g_csDomainList);
    ndom->pNext = g_DomainList;
    g_DomainList = ndom;
    LeaveCriticalSection(&g_csDomainList);

    return TRUE;
}

// referenced by device.c
BOOL FS_ResourceAdjust (DWORD dwResId, DWORD dwId, DWORD dwLen, BOOL fClaim)
{
    ResourceDomain *pdom;
    DWORD size, min;
    BOOL r;
    
    if ((pdom = FindDomain(dwResId)) == NULL)
        return FALSE;  // no such resource

    min = pdom->dwOffset;
    if (dwId < min)
        return FALSE;  // out of range

    // translate the resource to the domain's address space
    dwId -= min;

    size = pdom->table.dwSize;
    if (dwId >= size || dwId + dwLen > size)
        return FALSE;  // out of range

    EnterCriticalSection(&pdom->cs);
    
    if (size <= TABLE_THRESHOLD) {
        r = SetDenseResources(&pdom->table.dt, dwId, dwLen, fClaim);
    } else {
        r = SetSparseResources(&pdom->table.st, dwId, dwLen, fClaim);
    }

#ifdef DEBUG
    if (ZONE_RESMGR)
        ResourceDump(pdom);
#endif

    LeaveCriticalSection(&pdom->cs);

    return r;
}

BOOL ResourceShare (DWORD dwResId, DWORD dwId, DWORD dwLen)
{
    return AdjustShared(dwResId, dwId, dwLen, FALSE);
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

static void ResourceInitFromKey (HKEY hk)
{
    DWORD valsz, rsz;
    WCHAR *valbuf, *p;
    DWORD a[3];  // no Resource API takes more than 3 parms

    if (RegQueryInfoKey(hk, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &valsz, NULL, NULL) != ERROR_SUCCESS)
        return;
    if ((valbuf = alloca(valsz)) == NULL ||
        RegQueryValueEx(hk, L"Identifier", NULL, NULL, (PBYTE)&a[0], ((rsz = sizeof(DWORD)), &rsz)) != ERROR_SUCCESS ||
        RegQueryValueEx(hk, L"Minimum", NULL, NULL, (PBYTE)&a[1], ((rsz = sizeof(DWORD)), &rsz)) != ERROR_SUCCESS ||
        RegQueryValueEx(hk, L"Space", NULL, NULL, (PBYTE)&a[2], ((rsz = sizeof(DWORD)), &rsz)) != ERROR_SUCCESS ||
        FS_ResourceCreateList(a[0], a[1], a[2]) == FALSE) {
        return;
    }

    if (RegQueryValueEx(hk, L"Ranges", NULL, NULL, (PBYTE)valbuf, ((rsz = valsz), &rsz)) == ERROR_SUCCESS) {
        p = valbuf;
        while (ScanRange(&p, &a[1], &a[2]))
            FS_ResourceAdjust(a[0], a[1], a[2], FALSE);	// release resources
    }
    if (RegQueryValueEx(hk, L"Shared", NULL, NULL, (PBYTE)valbuf, ((rsz = valsz), &rsz)) == ERROR_SUCCESS) {
        p = valbuf;
        while (ScanRange(&p, &a[1], &a[2])) {
            BOOL fOk = ResourceShare(a[0], a[1], a[2]);
            DEBUGCHK(fOk);
        }
    }
}

void ResourceInitFromRegistry (LPCTSTR key)
{
    HKEY hk, hk2;
    WCHAR *sknm;
    DWORD i, sksz, rsz;

    DEBUGCHK( key != NULL );    // not a published API so no need for a retail runtime check
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, 0, &hk) != ERROR_SUCCESS ||
        RegQueryInfoKey(hk, NULL, NULL, NULL, NULL, &sksz, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
        return;
    sksz += 1; // account for termination
    sksz *= sizeof(*sknm); // convert to bytes
    if ((sknm = alloca(sksz)) == NULL)
        return;

    for (i=0; RegEnumKeyEx(hk, i, sknm, ((rsz = sksz), &rsz), NULL, NULL, NULL, NULL) == ERROR_SUCCESS; ++i) {
        if (RegOpenKeyEx(hk, sknm, 0, 0, &hk2) == ERROR_SUCCESS) {
            ResourceInitFromKey(hk2);
            RegCloseKey(hk2);
        }
    }
    
    RegCloseKey(hk);
}


#ifdef DEBUG

#ifndef UNIT_TEST
#define OUTPUT(a) DEBUGMSG(1, a)
#else
#define OUTPUT(a) wprintf a
#endif

void ResourceDump (ResourceDomain *pdom)
{
    if (pdom->table.dwSize <= TABLE_THRESHOLD) {
        OUTPUT((TEXT("Resource id %u has range %d - %d  (size %u: DENSE)\n"), pdom->dwResId, pdom->dwOffset, pdom->dwOffset + pdom->table.dwSize - 1, pdom->table.dwSize));
        OUTPUT((TEXT("\tExclusive resources: 0x%016I64x\n"), pdom->table.dt.vb[0]));
        OUTPUT((TEXT("\tShareable resources: 0x%016I64x\n"), pdom->table.dt.vbs[0]));
    } else {
        SparseTableNode *pnod;
        OUTPUT((TEXT("Resource id %u has range 0x%x - 0x%x  (size %u: SPARSE)\n"), pdom->dwResId, pdom->dwOffset, pdom->dwOffset + pdom->table.dwSize - 1, pdom->table.dwSize));
        OUTPUT((TEXT("\tAvailable resources:")));
        pnod = pdom->table.st.pFirst;
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

    EnterCriticalSection(&g_csDomainList);
    pdom = g_DomainList;
    LeaveCriticalSection(&g_csDomainList);

    if (pdom == NULL)
        OUTPUT((TEXT("* There are no resource domains defined. *\n")));

    while (pdom) {
        ResourceDump(pdom);
        if (pdom = pdom->pNext)
            OUTPUT((TEXT("\n")));
    }
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
            if (ResourceShare(a[0], a[1], a[2]))
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
