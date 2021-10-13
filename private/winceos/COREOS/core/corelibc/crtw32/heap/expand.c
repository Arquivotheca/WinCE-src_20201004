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
/***
*expand.c - Win32 expand heap routine
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*Revision History:
*       01-15-92  JCR   Module created.
*       02-04-92  GJF   Replaced windows.h with oscalls.h.
*       05-06-92  DJM   ifndef out of POSIX build.
*       09-23-92  SRW   Change winheap code to call NT directly always
*       10-15-92  SKS   Removed the ill-named HEAP_GROWTH_ALLOWED flag
*                       which was causing a bug: _expand was behaving like
*                       realloc(), by moving the block when it could not be
*                       grown in place.  _expand() must NEVER move the block.
*                       Also added a safety check to work around a bug in
*                       HeapReAlloc, where it returns success even
*                       when it fails to grow the block in place.
*       10-28-92  SRW   Change winheap code to call Heap????Ex calls
*       11-05-92  SKS   Change name of variable "CrtHeap" to "_crtheap"
*       11-07-92  SRW   _NTIDW340 replaced by linkopts\betacmp.c
*       11-16-92  SRW   Heap???Ex functions renamed to Heap???
*       10-21-93  GJF   Replace _CALLTYPE1 with _cdecl. Cleaned up format.
*       04-06-95  GJF   Added support for debug heap.
*       04-29-95  GJF   Copied over from winheap and made conditional on
*                       WINHEAP.
*       05-22-95  GJF   Test against _HEAP_MAXREQ before calling API. Also,
*                       removed workaround for long-ago NT problem.
*       05-24-95  CFW   Official ANSI C++ new handler added.
*       05-23-95  GJF   Really removed workaround this time...
*       03-04-96  GJF   Added support for small-block heap. Moved heaphook
*                       invocation to the very start of the function.
*       04-10-96  GJF   Return type of __sbh_find_block and __sbh_resize_block
*                       changed to __map_t *.
*       05-30-96  GJF   Minor changes for latest version of small-block heap.
*       05-22-97  RDK   New small-block heap scheme implemented.
*       09-26-97  BWT   Fix POSIX
*       11-05-97  GJF   Small POSIX fix from Roger Lanser.
*       12-17-97  GJF   Exception-safe locking.
*       07-28-98  JWM   RTC update.
*       09-29-98  GJF   Bypass all small-block heap code when __sbh_initialized
*                       is 0.
*       11-16-98  GJF   Merged in VC++ 5.0 version of small-block heap.
*       12-02-98  GJF   One too many munlocks!
*       12-18-98  GJF   Changes for 64-bit size_t.
*       05-01-99  PML   Disable small-block heap for Win64.
*       05-26-99  KBF   Updated RTC hook func params
*       06-21-99  GJF   Removed old small-block heap from static libs.
*       08-04-00  PML   Don't round allocation sizes when using system
*                       heap (VS7#131005).
*       02-20-02  BWT   prefast fixes - don't return from try block
*       10-10-03  AC    Added validation.
*       10-04-04  AC    Fixed _expand problem related to LFH.
*                       VSW#164763
*       10-05-04  ESC   Set errno and return null when newsize>_HEAP_MAX,
*                       instead of _VALIDATE_RETURN (VSW#298543)
*       03-30-05  PAL   Disable LFH workaround for Win32.
*       04-06-07  WL    Enable LFH workaround for all platforms. DDB#87911
*
*******************************************************************************/

#include <cruntime.h>
#include <malloc.h>
#include <winheap.h>
#include <windows.h>
#include <mtdll.h>
#include <dbgint.h>
#include <rtcsup.h>
#include <internal.h>

// Check if the low fragmentation heap is enabled
static BOOL _is_LFH_enabled ()
{
    LONG heaptype = -1;

#ifdef _WIN32_WCE
    // CE doesn't support HeapQueryInformation, so simply assume it doesn't enable 
    return FALSE;
#else
    return ((HeapQueryInformation(_crtheap, HeapCompatibilityInformation, &heaptype, sizeof(heaptype), NULL)) && (heaptype == 2));
#endif // _WIN32_WCE
}

/***
*void *_expand(void *pblck, size_t newsize) - expand/contract a block of memory
*       in the heap
*
*Purpose:
*       Resizes a block in the heap to newsize bytes. newsize may be either
*       greater (expansion) or less (contraction) than the original size of
*       the block. The block is NOT moved.
*
*       NOTES:
*
*       (1) In this implementation, if the block cannot be grown to the
*       desired size, the resulting block will NOT be grown to the max
*       possible size.  (That is, either it works or it doesn't.)
*
*       (2) Unlike other implementations, you can NOT pass a previously
*       freed block to this routine and expect it to work.
*
*Entry:
*       void *pblck - pointer to block in the heap previously allocated
*                 by a call to malloc(), realloc() or _expand().
*
*       size_t newsize  - requested size for the resized block
*
*Exit:
*       Success:  Pointer to the resized memory block (i.e., pblck)
*       Failure:  NULL, errno is set
*
*Uses:
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function. 
*
*       If pblck does not point to a valid allocation block in the heap,
*       _expand() will behave unpredictably and probably corrupt the heap.
*
*******************************************************************************/

void * __cdecl _expand_base (void * pBlock, size_t newsize)
{
#ifdef _POSIX_
        return (HeapReAlloc( _crtheap,
                             HEAP_REALLOC_IN_PLACE_ONLY,
                             pBlock,
                             (DWORD)newsize ));
#else
        void *      pvReturn;

#ifdef HEAPHOOK
        /* call heap hook if installed */
        if (_heaphook)
        {
            void *  pvReturn;
            if ((*_heaphook)(_HEAP_EXPAND, newsize, pBlock, (void *)&pvReturn))
                return pvReturn;
        }
#endif /* HEAPHOOK */
        size_t oldsize;

        /* validation section */
        _VALIDATE_RETURN(pBlock != NULL, EINVAL, NULL);
        if (newsize > _HEAP_MAXREQ) {
            errno = ENOMEM;
            return NULL;
        }

        if (newsize == 0)
        {
            newsize = 1;
        }

        oldsize = (size_t)HeapSize(_crtheap, 0, pBlock);

        pvReturn = HeapReAlloc(_crtheap, HEAP_REALLOC_IN_PLACE_ONLY, pBlock, newsize);

        if (pvReturn == NULL)
        {
            /* If the failure is caused by the use of the LFH, just return the original block. */
            if (oldsize <= 0x4000 /* LFH can only allocate blocks up to 16 KB. */
                    && newsize <= oldsize && _is_LFH_enabled())
                pvReturn = pBlock;
            else
                errno = _get_errno_from_oserr(GetLastError());
        }

        if (pvReturn)
        {
            RTCCALLBACK(_RTC_Free_hook, (pBlock, 0));
            RTCCALLBACK(_RTC_Allocate_hook, (pvReturn, newsize, 0));
        }

        return pvReturn;
#endif  /* _POSIX_ */
}
