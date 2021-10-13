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
#include <bincomp.h>

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

#define STRSIZEMASK 0x3fff
#define STRPART1RAW 0x8000
#define STRPART2RAW 0x4000

extern CRITICAL_SECTION CompCS;


PVOID AllocBCBuffer (DWORD cbSize)
{
    return VMAlloc (g_pprcNK, 0, cbSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD 
DecompressROM(
    LPBYTE BufIn,
    DWORD  InSize,
    LPBYTE BufOut,
    DWORD  OutSize,
    DWORD  skip
    ) 
{
    DWORD result;
    
    DEBUGCHK(!(skip & (VM_PAGE_SIZE-1)));

    // Must not get page faults while holding CompCS
    VERIFY(VMLockPages (pVMProc, BufIn,InSize,0,LOCKFLAG_READ));
    if (BufOut) {
        VERIFY(VMLockPages (pVMProc, BufOut,OutSize,0,LOCKFLAG_WRITE));
    }
    
    EnterCriticalSection(&CompCS);
    
    result = CEDecompressROM(BufIn, InSize, BufOut, OutSize, skip, 1, VM_PAGE_SIZE);
    
    LeaveCriticalSection(&CompCS);
    
    VMUnlockPages (pVMProc, BufIn,InSize);
    if (BufOut)
        VMUnlockPages (pVMProc, BufOut,OutSize);
    
    return result;
}


//------------------------------------------------------------------------------
// Compressors - bufin and bufout must be WORD aligned, lenout must be at least as
// large as lenin, CECOMPRESS_FAILED is returned if it doesn't fit, else compressed length
// CECOMPRESS_ALL_ZEROS returned if buffer entirely zero.  If bufout is NULL, the output
// length is computed, but the results are not stored.  Lenin must be a multiple of 2.
//------------------------------------------------------------------------------
DWORD 
NKStringCompress(
    LPBYTE bufin,
    DWORD  lenin,
    LPBYTE bufout,
    DWORD  lenout
    ) 
{
    DWORD result, result2, newsize;
    LPBYTE ptr1;
    const BYTE *ptr2, *ptrend;

    TRUSTED_API (L"NKStringCompress", CECOMPRESS_FAILED);
    DEBUGMSG(ZONE_ENTRY, (L"NKStringCompress entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n", bufin, lenin, bufout, lenout));
    
    result = CECompress(bufin, (WORD)((lenin+1)/2), (bufout ? bufout+2 : 0), lenout - 2, 2, 1024);
    if (result == CECOMPRESS_ALLZEROS) {
        newsize = 0;
        if (bufout)
            *(LPWORD)bufout = 0;
    } else if ((result != CECOMPRESS_FAILED) && (result < (lenin+1)/2)) {
        newsize = result;
        if (bufout) {
            DEBUGCHK(newsize <= STRSIZEMASK);
            *(LPWORD)bufout = (WORD)newsize;
        }
    } else {
        newsize = (lenin+1)/2;
        if (newsize + 2 > lenout) {
            DEBUGMSG(ZONE_ENTRY,(L"NKStringCompress exit 1: %8.8lx\r\n",CECOMPRESS_FAILED));
            return CECOMPRESS_FAILED;
        }
        if (bufout) {
            DEBUGCHK(newsize <= STRSIZEMASK);
            *(LPWORD)bufout = STRPART1RAW | (WORD)newsize;
            for (ptr1 = &bufout[2], ptr2 = bufin, ptrend = ptr1+newsize; ptr1 < ptrend; ptr2+=2)
                *ptr1++ = *ptr2;
        }
    }
    
    result2 = CECompress(bufin+1, (WORD)(lenin/2), (bufout ? bufout+2+newsize : 0), lenout - 2 - newsize, 2, 1024);
    
    if (result2 == CECOMPRESS_ALLZEROS) {
        if (result == CECOMPRESS_ALLZEROS) {
            DEBUGMSG(ZONE_ENTRY, (L"NKStringCompress exit: %8.8lx\r\n", CECOMPRESS_ALLZEROS));
            return CECOMPRESS_ALLZEROS;
        } else {
            DEBUGMSG(ZONE_ENTRY, (L"NKStringCompress exit 2: %8.8lx\r\n", 2+newsize >= lenin ? CECOMPRESS_FAILED : 2+newsize));
            return (2+newsize >= lenin ? CECOMPRESS_FAILED : 2+newsize);
        }
    } else if ((result2 == CECOMPRESS_FAILED) || (result2 >= (lenin/2))) {
        result2 = lenin/2;
        if (2 + newsize + result2 > lenout) {
            DEBUGMSG(ZONE_ENTRY, (L"NKStringCompress exit 3: %8.8lx\r\n", CECOMPRESS_FAILED));
            return CECOMPRESS_FAILED;
        }
        if (bufout) {
            *(LPWORD)bufout |= STRPART2RAW;
            for (ptr1 = &bufout[2+newsize], ptr2 = &bufin[1], ptrend = ptr1+(lenin/2); ptr1 < ptrend; ptr2+=2)
                *ptr1++ = *ptr2;
        }
    }
    
    DEBUGMSG(ZONE_ENTRY, (L"NKStringCompress exit 4: %8.8lx\r\n", 2+newsize+result2 >= lenin ? CECOMPRESS_FAILED : 2+newsize+result2));
    
    return (2+newsize+result2 >= lenin ? CECOMPRESS_FAILED : 2+newsize+result2);
}



//------------------------------------------------------------------------------
// Decompressors - bufin and bufout must be WORD aligned, lenout *must* be large
// enough to handle the entire decompressed contents.
//------------------------------------------------------------------------------
#pragma prefast(disable: 11, "bufout will not be NULL if LockPage succeeds")
DWORD 
NKStringDecompress(
    LPBYTE bufin,
    DWORD  lenin,
    LPBYTE bufout,
    DWORD  lenout
    ) 
{
    DWORD newsize1, newsize2, insize1, insize2, loop;

    TRUSTED_API (L"NKStringDecompress", CEDECOMPRESS_FAILED);
    
    DEBUGMSG(ZONE_ENTRY, (L"NKStringDecompress entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n", bufin, lenin, bufout, lenout));
    
    insize1 = *(LPWORD)bufin;
    insize2 = lenin - (insize1 & STRSIZEMASK) - 2;
    if (!(insize1 & STRSIZEMASK)) {
        for (loop = 0; loop < lenout; loop+=2)
            bufout[loop] = 0;
        newsize1 = 0;
    } else if (insize1 & STRPART1RAW) {
        newsize1 = (insize1 & STRSIZEMASK);
        if ((newsize1*2 > lenout) || (newsize1+2 > lenin)) {
            DEBUGCHK(0);
            return CEDECOMPRESS_FAILED;
        }
        for (loop = 0; loop < newsize1; loop++)
            bufout[loop*2] = bufin[2+loop];
    } else {
        newsize1 = CEDecompress(bufin+2, insize1&STRSIZEMASK, bufout, (lenout+1)/2, 0, 2, 1024);
        if (newsize1 == CEDECOMPRESS_FAILED) {
            DEBUGMSG(ZONE_ENTRY, (L"NKStringDecompress exit 1: %8.8lx\r\n", CEDECOMPRESS_FAILED));
            return CEDECOMPRESS_FAILED;
        }
    }
    if (!insize2) {
        for (loop = 1; loop < lenout; loop+=2)
            bufout[loop] = 0;
        newsize2 = 0;
    } else if (insize1 & STRPART2RAW) {
        newsize2 = insize2;
        for (loop = 0; loop < newsize2; loop++)
            bufout[loop*2+1] = bufin[2+(insize1&STRSIZEMASK)+loop];
    } else {
        newsize2 = CEDecompress(bufin+2+(insize1&STRSIZEMASK), insize2, bufout+1, lenout/2, 0, 2, 1024);
        if (newsize2 == CEDECOMPRESS_FAILED) {
            DEBUGMSG(ZONE_ENTRY, (L"NKStringDecompress exit 2: %8.8lx\r\n", CEDECOMPRESS_FAILED));
            return CEDECOMPRESS_FAILED;
        }
    }
    
    if (newsize1 > newsize2) {
        DEBUGMSG(ZONE_ENTRY, (L"NKStringDecompress exit 3: %8.8lx\r\n", newsize1*2));
        return (newsize1*2);
    }
    
    DEBUGMSG(ZONE_ENTRY, (L"NKStringDecompress exit: %8.8lx\r\n", newsize2*2));
    
    return (newsize2*2);
}
#pragma prefast(pop)



//------------------------------------------------------------------------------
// Compressors - bufin and bufout must be WORD aligned, lenout must be at least as
// large as lenin, CECOMPRESS_FAILED is returned if it doesn't fit, else compressed length
// CECOMPRESS_ALL_ZEROS returned if buffer entirely zero.  If bufout is NULL, the output
// length is computed, but the results are not stored.  Lenin must be a multiple of 2.
//------------------------------------------------------------------------------
DWORD 
NKBinaryCompress(
    LPBYTE bufin,
    DWORD  lenin,
    LPBYTE bufout,
    DWORD  lenout
    ) 
{
    DWORD retval;

    TRUSTED_API (L"NKBinaryCompress", CECOMPRESS_FAILED);

    DEBUGMSG(ZONE_ENTRY, (L"NKBinaryCompress entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n", bufin, lenin, bufout, lenout));
    
    retval = CECompress(bufin, lenin, bufout, lenout, 1, 1024);
    
    DEBUGMSG(ZONE_ENTRY, (L"NKBinaryCompress exit: %8.8lx\r\n", retval));
    
    return retval;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#pragma prefast(disable: 262, "use 4K decompress buffer")
static DWORD 
BufferedDecompress(
    LPBYTE bufin,
    DWORD  lenin,
    LPBYTE bufout,
    DWORD  lenout,
    DWORD  skip
    ) 
{
    BYTE buf[4096]; // file system blocking factor
    DWORD retval;
    DWORD cbDec = lenout + (skip & 1023);  // Number of bytes actually decompressed
    
    retval = CEDecompress(bufin, lenin, buf, cbDec, skip & ~1023, 1, 1024);
    
    PREFAST_ASSERT((skip & 1023) < 1024);  // NOTENOTE prefast gets confused about bitwise ops
    if (retval != CEDECOMPRESS_FAILED)
        memcpy(bufout, buf+(skip & 1023), lenout);
    
    return retval;      
}
#pragma prefast(pop)



//------------------------------------------------------------------------------
// Decompressors - bufin and bufout must be WORD aligned, lenout *must* be large
// enough to handle the entire decompressed contents.
//------------------------------------------------------------------------------
DWORD 
NKBinaryDecompress(
    LPBYTE bufin,
    DWORD  lenin,
    LPBYTE bufout,
    DWORD  lenout,
    DWORD  skip
    ) 
{
    DWORD retval;

    DEBUGMSG(ZONE_ENTRY, (L"NKBinaryDecompress entry: %8.8lx %8.8lx %8.8lx %8.8lx\r\n", bufin, lenin, bufout, lenout));
    
    // Currently nothing is calling this function with a skip value anything other than 0.
    // For the BINFS to be able to use this function currectly it needs a VM_PAGE_SIZE passed to it rather than 1024
    // The 1024 is left for filesys objectstore compatibility
    if (skip & 1023) {
        retval = CEDecompressROM(bufin, lenin, bufout, lenout, skip & ~1023, 1, VM_PAGE_SIZE);
    } else {
        retval = CEDecompress(bufin, lenin, bufout, lenout, skip & ~1023, 1, 1024);
    }
    
    DEBUGMSG(ZONE_ENTRY, (L"NKBinaryDecompress exit: %8.8lx\r\n", retval));
    
    return retval;
}

#define ALIGN_TO_DWORD(x) (((x) + 0x3) & ~0x3)
#define ALIGN_UP_DWORD(x) ((x) + 0x3)

//------------------------------------------------------------------------------
// Decompressors - bufin and bufout must be WORD aligned, lenout *must* be large
// enough to handle the entire decompressed contents.
//------------------------------------------------------------------------------
DWORD 
ExtBinaryDecompress(
    const BYTE *bufin,
    DWORD  lenin,
    LPBYTE bufout,
    DWORD  lenout,
    DWORD  skip
    ) 
{
    LPBYTE BufInCopy = NULL;
    LPBYTE BufOutCopy = NULL;
    DWORD retval = CEDECOMPRESS_FAILED;
    DWORD totallen = ALIGN_UP_DWORD(lenin + lenout);

    // check for overflow
    if (totallen < lenin || totallen < lenout) {
        return retval;
    }

    // create temp buffer to hold in/out parameters
    BufInCopy = NKmalloc (totallen);    
    if (BufInCopy) {

        // get the pointer to the out buffer (DWORD aligned)
        BufOutCopy = (LPBYTE) ALIGN_TO_DWORD((DWORD)BufInCopy + lenin);

        // copy in from user buffers    
        if (CeSafeCopyMemory (BufInCopy, bufin, lenin)
            && CeSafeCopyMemory (BufOutCopy, bufout, lenout)) {

            // call the internal api
            retval = NKBinaryDecompress (BufInCopy, lenin, BufOutCopy, lenout, skip);

            // copy out to user buffer
            if ((retval != CEDECOMPRESS_FAILED)
                && !CeSafeCopyMemory (bufout, BufOutCopy, lenout)) {
                retval = CEDECOMPRESS_FAILED;
            }
        }
        NKfree (BufInCopy);
    }
    
    return retval;
}


DWORD 
NKDecompressBinaryBlock(
    LPBYTE  lpbSrc,             // Input buffer
    DWORD   cbSrc,              // Input length
    LPBYTE  lpbDest,            // Output buffer
    DWORD   cbDest              // Maximium output size (after applying steprate)
    )
{
    DWORD retval = 0;
    
    // Note caller must guarantee that we do not get page faults while holding CompCS

    EnterCriticalSection(&CompCS);
    __try {
        retval = BinDecompressROM(lpbSrc, cbSrc, lpbDest, &cbDest);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ;
    }
    LeaveCriticalSection(&CompCS);
    
    return retval;
}
