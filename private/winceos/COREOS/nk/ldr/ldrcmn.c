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
#include <kernel.h>

#define KERNDLL     "kernel.dll"

ROMHDR *const volatile pTOC = (ROMHDR *)-1;     // Gets replaced by RomLoader with real address

// ROM Header extension.  The ROM loader (RomImage) will set the pExtensions field of the table
// of contents to point to this structure.  This structure contains the PID and a extra field to point
// to further extensions.
const volatile ROMPID RomExt = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    NULL
};

// Prevent the linker from removing RomExt even if it is completely unreferenced.
#if defined(_X86_) || defined(_SHX_)
#pragma comment(linker, "/INCLUDE:_RomExt")
#else
#pragma comment(linker, "/INCLUDE:RomExt")
#endif

//------------------------------------------------------------------------------
// initialize Kernel Globals from Copy section in ROM
// This is where the data sections become valid... can't read globals until after this
//------------------------------------------------------------------------------
void
KernelRelocate (
    ROMHDR *const pTOC
    )
{
    ULONG       loop;
    COPYentry   *cptr;

    // copy globals
    for (loop = 0; loop < pTOC->ulCopyEntries; loop++) {
        cptr = (COPYentry *)(pTOC->ulCopyOffset + loop*sizeof(COPYentry));
        if (cptr->ulCopyLen) {
            memcpy((LPVOID)cptr->ulDest,(LPVOID)cptr->ulSource,cptr->ulCopyLen);
        }
        if (cptr->ulCopyLen < cptr->ulDestLen) {
            memset((LPVOID)(cptr->ulDest+cptr->ulCopyLen),0,cptr->ulDestLen-cptr->ulCopyLen);
        }
    }

}

//--------------------------------------------------------------------
// FindKernelEntry - find the kernel entry point to kernel.dll
//----------------------------------------------------------------------
LPVOID
FindKernelEntry (
        ROMHDR *const pTOC
        )
{
    ULONG       loop;
    e32_rom     *e32rp = NULL;
    TOCentry    *tocptr;

    // find kernel.dll (case sensitive)
    tocptr = (TOCentry *)(pTOC+1);  // toc entries follow the header
    
    for (loop = 0; loop < pTOC->nummods; loop++) {

        if (_stricmp (tocptr->lpszFileName, KERNDLL) == 0) {
            e32rp = (e32_rom *)(tocptr->ulE32Offset);
            break;
        }
        tocptr++;
    }

    return (LPVOID) (e32rp? (e32rp->e32_vbase + e32rp->e32_entryrva) : 0);
}

