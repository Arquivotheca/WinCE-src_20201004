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
#include "binfs.h"


#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

DWORD AdjustOffset(BinVolume *pVolume, DWORD dwRegion, DWORD dwOffset)
{
    return (dwOffset-pVolume->pChain[dwRegion].dwAddress)+pVolume->pChain[dwRegion].dwBinOffset;
}


void SafeCopyMemory(LPVOID pDest, LPVOID pSrc, DWORD dwSize)
{
    PREFAST_ASSUME(dwSize); // caller validated size
#pragma prefast(disable: 322, "Recover gracefully from hardware failure")   
    __try {
        memcpy( pDest, pSrc, dwSize);
    } __except( EXCEPTION_EXECUTE_HANDLER) {
    }
#pragma prefast(pop)    
}    

BOOL ReadAtOffset(BinVolume *pVolume,LPVOID buffer,DWORD dwBytesToRead, LPDWORD lpNumBytesRead, DWORD dwOffset)
{
    DWORD cbSector = pVolume->diskInfo.di_bytes_per_sect;
    BYTE *SectorBuffer = NULL;;
    DWORD dwError =ERROR_SUCCESS;
    BOOL fSuccess = FALSE;
    DWORD dwActualBytes = 0, dwSectorStart=0, dwSectorsToRead = 0;
    
    DEBUGMSG( ZONE_HELPER, (TEXT("BINFS:ReadAtOffset hDsk=%08X dwBytesToRead=%ld\n"), pVolume->hDsk, dwBytesToRead));

    SectorBuffer = new BYTE[cbSector];
    if (!SectorBuffer) {
        goto Fail;
    }

    dwActualBytes = min( dwBytesToRead, dwBytesToRead /*Put Actual size here later*/);
    dwBytesToRead = dwActualBytes;
    dwSectorStart = (dwOffset / cbSector);
    // BUGBUG: - Convert to multiple SG once we verify that NAND driver can take it.
    if (dwActualBytes) {
        DWORD dwBytesPartial = dwOffset % cbSector;
        if (dwBytesPartial) {
            dwBytesPartial = cbSector - dwBytesPartial;
            if (ERROR_SUCCESS != FSDMGR_ReadDisk(pVolume->hDsk, dwSectorStart, 1, SectorBuffer, cbSector)) goto Fail;
            if (dwActualBytes > dwBytesPartial) {
                SafeCopyMemory( buffer, SectorBuffer + (dwOffset % cbSector), dwBytesPartial);
                dwActualBytes -= dwBytesPartial;
            } else {    
                SafeCopyMemory( buffer, SectorBuffer + (dwOffset % cbSector), dwActualBytes);
                dwActualBytes = 0;
            }    
            DEBUGMSG( ZONE_IO, (TEXT("ReadAtOffset 1) Reading %ld bytes at sector %ld\r\n"), dwActualBytes, dwSectorStart));
            dwSectorStart++;
        }    
        if (dwActualBytes >= cbSector) {
            dwSectorsToRead = dwActualBytes / cbSector;
            dwActualBytes -= (dwSectorsToRead * cbSector);
            if (ERROR_SUCCESS != FSDMGR_ReadDisk(pVolume->hDsk, dwSectorStart, dwSectorsToRead, (LPBYTE)buffer+dwBytesPartial, dwSectorsToRead * cbSector)) goto Fail;
            DEBUGMSG( ZONE_IO, (TEXT("ReadAtOffset 2) Reading %ld Bytes at sector %ld, #of sectors = %ld\r\n"), dwSectorsToRead * cbSector, dwSectorStart, dwSectorsToRead));
            dwSectorStart += dwSectorsToRead;
        }
        DEBUGCHK( dwActualBytes < cbSector);
        if (dwActualBytes) {
            DEBUGMSG( ZONE_IO, (TEXT("ReadAtOffset 3) Reading %ld Bytes at sector %ld\r\n"), dwActualBytes, dwSectorStart));
            if (ERROR_SUCCESS != FSDMGR_ReadDisk(pVolume->hDsk, dwSectorStart, 1, SectorBuffer, cbSector)) goto Fail;
            SafeCopyMemory( (LPBYTE)buffer+(dwBytesToRead-dwActualBytes), SectorBuffer, dwActualBytes);
        }
    }
    dwOffset += dwBytesToRead;
    
    if (lpNumBytesRead)
        *lpNumBytesRead = dwBytesToRead;
    
    fSuccess = TRUE;        
    
Fail:    
    if (SectorBuffer) {
        delete[] SectorBuffer;
    }
    return fSuccess;
}

// the size of a compressed block plus padding for a header
#define LZHEADERMAX 64
BYTE pCompressedBits[COMP_BLOCK_SIZE + LZHEADERMAX];

inline DWORD Read3(BinVolume *pVolume, BinDirList *pDirectory, DWORD dwIndex) {
    BYTE pbBuf[3];
    DWORD dwVal;
    PBYTE pbVal = (PBYTE)&dwVal;
    ReadAtOffset(pVolume, pbBuf, 3, &dwVal, pDirectory->dwAddress+dwIndex);        
    dwVal = 0;
    *pbVal++ = pbBuf[0];
    *pbVal++ = pbBuf[1];
    *pbVal = pbBuf[2];
    return dwVal;
}

#define LZBLOCKBITS 12

// TODO: Optimize this !!!
// TODO: We should probably cache the block directory for the file or atleast the length etc (Read3(0))

DWORD GetCompressedBytes(
    BinVolume *pVolume, 
    BinDirList *pDirectory,
    LPBYTE bufout,
    DWORD  lenout,
    DWORD dwSkip
    )
{
    DWORD dwOutLen,dwOffset;
    DWORD dwBytesRead;
    int iNumBlocks,iBlockIdx,iCurPos;
    // Make sure this is a legal skip
//    if (dwSkip & (LZHASHENTRIES-1))
//        return CEDECOMPRESS_FAILED;
    // Have to AT LEAST have room for the header
    dwOutLen = Read3(pVolume, pDirectory, 0);                                
    if(dwOutLen == 0) {
        iNumBlocks = 2;
    } else {
        iNumBlocks = ((dwOutLen -1) >> LZBLOCKBITS) + 2;
    }
    // Room for the rest of the table?
    // Skip to the closest block
    iBlockIdx = dwSkip >> LZBLOCKBITS;
    if (iBlockIdx >= iNumBlocks)
        return 0;
    iCurPos = (iBlockIdx ? Read3(pVolume, pDirectory, iBlockIdx*3) : 3 * iNumBlocks);
    dwOffset = 0;
    while (++iBlockIdx < iNumBlocks && lenout > 0) {
        //        int iOutLen,iNxtPos;
        DWORD iOutLen,iNxtPos;
        iNxtPos = Read3(pVolume, pDirectory, iBlockIdx*3);
        iOutLen = lenout;
        DEBUGCHK(iNxtPos-iCurPos <= sizeof(pCompressedBits));
        if (iNxtPos-iCurPos > sizeof(pCompressedBits)) 
            return dwOffset;
        ReadAtOffset(pVolume, pCompressedBits, iNxtPos-iCurPos, &dwBytesRead, pDirectory->dwAddress+iCurPos);        
        if (DecompressBinaryBlock(pCompressedBits, iNxtPos-iCurPos, bufout, iOutLen) != 0)
            return dwOffset;
        if (iOutLen < 0)
            return dwOffset;
        // Update pointers
        bufout += iOutLen;
        dwOffset += iOutLen;
        lenout -= iOutLen;
        iCurPos = iNxtPos;
    }
    return dwOffset;
}



DWORD GetFileData(BinVolume *pVolume, BinDirList *pDirectory, DWORD dwPosition, BYTE *pBuffer, DWORD dwBytesToRead)
{
    DWORD dwBlock,bRead,csize, dwFullSize = 0, dwBytesRead = 0;
    LPVOID buffercopy;
    DWORD  dwResult;

    
    if (dwPosition > pDirectory->dwRealFileSize) {
        return 0;
    }
    if (dwPosition + dwBytesToRead > pDirectory->dwRealFileSize) {
        dwBytesToRead = pDirectory->dwRealFileSize - dwPosition;
    }
    if (!dwBytesToRead) {
        dwFullSize = 0;
    } else {
        dwFullSize = dwBytesToRead;
        buffercopy = pBuffer;
        LockPages(buffercopy, dwFullSize, 0, LOCKFLAG_WRITE);
        EnterCriticalSection(&pVolume->csComp);
        if (pDirectory->dwRealFileSize == pDirectory->dwCompFileSize) {
            ReadAtOffset(pVolume, pBuffer, dwBytesToRead, &dwBytesRead, pDirectory->dwAddress+dwPosition);
        } else if (!pDirectory->dwCompFileSize) {
            SafeCopyMemory(pBuffer, 0, dwBytesToRead);
        } else {
            while (dwBytesToRead) {
                dwBlock = dwPosition / COMP_BLOCK_SIZE;
                bRead = dwBytesToRead;
                if ((dwPosition + bRead-1) / COMP_BLOCK_SIZE != dwBlock) {
                    bRead = COMP_BLOCK_SIZE - (dwPosition & (COMP_BLOCK_SIZE-1));
                }
                if ((pVolume->pCurCompDir != pDirectory) || (dwBlock != pVolume->dwCurCompBlock)) {
                    if (dwBlock == pDirectory->dwRealFileSize / COMP_BLOCK_SIZE) {
                        csize = pDirectory->dwRealFileSize - (COMP_BLOCK_SIZE*dwBlock);
                    } else {
                        csize = COMP_BLOCK_SIZE;
                    }
                    if (bRead == csize) {
                        dwResult = GetCompressedBytes(pVolume, pDirectory, pBuffer, csize, dwBlock*COMP_BLOCK_SIZE);
                        pVolume->pCurCompDir = pDirectory;
                        pVolume->dwCurCompBlock = -1;
                    } else {
                        dwResult = GetCompressedBytes(pVolume, pDirectory, pVolume->pDecBuf, csize, dwBlock*COMP_BLOCK_SIZE);
                        pVolume->pCurCompDir = pDirectory;
                        pVolume->dwCurCompBlock = dwBlock;
                        SafeCopyMemory(pBuffer, pVolume->pDecBuf + (dwPosition & (COMP_BLOCK_SIZE-1)), bRead);
                    }
                    if (dwResult != csize) {
                        dwBytesToRead = 0;
                        dwBytesRead = dwFullSize;
                        continue;
                    }
                } else {
                    SafeCopyMemory(pBuffer, pVolume->pDecBuf + (dwPosition & (COMP_BLOCK_SIZE-1)), bRead);
                }
                pBuffer = pBuffer+bRead;
                dwBytesToRead -= bRead;
                dwPosition += bRead;
            }
            dwBytesRead = dwFullSize;
        }
        LeaveCriticalSection(&pVolume->csComp);
        UnlockPages(buffercopy, dwFullSize);
    }
    return dwBytesRead;
}

