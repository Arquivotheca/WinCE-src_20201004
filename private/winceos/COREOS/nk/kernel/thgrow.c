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

#include <toolhelp.h>

#ifdef KERN_CORE
#define COMMITPAGE(p,size)  (VMAlloc (g_pprcNK, p, size, MEM_COMMIT, PAGE_READWRITE))
#else
#define COMMITPAGE(p,size)  (VirtualAlloc (p, size, MEM_COMMIT, PAGE_READWRITE))
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPBYTE
THGrow(
    THSNAP *pSnap,
    DWORD dwSize
    )
{
    LPBYTE lpRet = NULL;

    if (dwSize < pSnap->cbReserved) {
        
        DWORD cbNeeded = pSnap->cbInuse + dwSize - pSnap->cbCommit;

        lpRet = (LPBYTE)pSnap + pSnap->cbInuse;
        if ((int) cbNeeded > 0) {

            cbNeeded = PAGEALIGN_UP(cbNeeded);
            if (pSnap->cbCommit + cbNeeded > pSnap->cbReserved) {
                lpRet = NULL;
                pSnap->dwErr = ERROR_INSUFFICIENT_BUFFER;
            } else if (!COMMITPAGE ((LPBYTE)pSnap+pSnap->cbCommit, cbNeeded)) {
                // out of memory
                lpRet = NULL;
                pSnap->dwErr = ERROR_NOT_ENOUGH_MEMORY;
            } else {
                pSnap->cbCommit += cbNeeded;
            }
        }
        
        if (lpRet) {
            pSnap->cbInuse += dwSize;
        }
    } else {
        // indicate to caller that we need a larger reservation
        pSnap->dwErr = ERROR_INSUFFICIENT_BUFFER;
    }

    return lpRet;
}

