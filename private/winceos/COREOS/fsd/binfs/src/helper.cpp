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

#define INVALID_SECTOR      0xFFFFFFFF

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

DWORD AdjustOffset(const BinVolume *pVolume, DWORD dwRegion, DWORD dwOffset)
{
    return (dwOffset-pVolume->pChain[dwRegion].dwAddress)+pVolume->pChain[dwRegion].dwBinOffset;
}

FORCEINLINE LRESULT ReadDisk (const BinVolume *pVolume, DWORD dwStartSector, DWORD dwNumSectors, LPBYTE pBuf)
{
    DEBUGMSG (ZONE_IO, (TEXT("BINFS:ReadDisk - Reading %ld bytes at sector %ld\r\n"), SECTOR_TO_SIZE (pVolume, dwNumSectors), dwStartSector));
    
    return (INVALID_CACHE_ID == pVolume->dwCacheId)
         ? FSDMGR_ReadDisk (pVolume->hDsk, dwStartSector, dwNumSectors, pBuf, dwNumSectors * pVolume->diskInfo.di_bytes_per_sect)
         : FSDMGR_CachedRead (pVolume->dwCacheId, dwStartSector, dwNumSectors, pBuf, 0);
}

#define SECTOR_NUM_OF_SECTOR_BUFFER(pVolume, pSectorBuf)         (*(LPDWORD)((DWORD) (pSectorBuf) + (pVolume)->diskInfo.di_bytes_per_sect))

LPBYTE GetSectorBuffer (BinVolume *pVolume, DWORD dwSector)     
{
    LPBYTE pSectorBuf = (LPBYTE) InterlockedExchangePointer (&pVolume->pSectorBuf, NULL);

    if (!pSectorBuf) {
        pSectorBuf = new BYTE [pVolume->diskInfo.di_bytes_per_sect + sizeof (DWORD)];
        if (pSectorBuf) {
            SECTOR_NUM_OF_SECTOR_BUFFER (pVolume,pSectorBuf) = INVALID_SECTOR; 
        }
    }

    if (pSectorBuf) {
        if (dwSector != SECTOR_NUM_OF_SECTOR_BUFFER (pVolume, pSectorBuf)) {
            // was used to kept old sector data, read from disk
            if (ERROR_SUCCESS != ReadDisk (pVolume, dwSector, 1, pSectorBuf)) {
                delete [] pSectorBuf;
                pSectorBuf = NULL;
            } else {
                SECTOR_NUM_OF_SECTOR_BUFFER (pVolume,pSectorBuf) = dwSector;  // update sector number of the sector buffer
            }
        } else {
            DEBUGMSG (ZONE_HELPER, (TEXT("BINFS:GetSectorBuffer - reuse cached sector %ld\r\n"), dwSector));
        }
    }
    return pSectorBuf;
}

void ReleaseSectorBuffer (BinVolume *pVolume, LPBYTE pSectorBuf)
{
    LPVOID pOldSectorBuf = InterlockedExchangePointer (&pVolume->pSectorBuf, pSectorBuf);

    if (pOldSectorBuf) {
        delete [] pOldSectorBuf;
    }
}

BOOL ReadPartial (BinVolume *pVolume, LPBYTE pBuffer, DWORD dwSector, DWORD dwOfstSector, DWORD cbPartial)
{
    DWORD cbSector = pVolume->diskInfo.di_bytes_per_sect;

    DEBUGCHK (dwOfstSector + cbPartial <= cbSector);

    LPBYTE pSectorBuf = GetSectorBuffer (pVolume, dwSector);
    
    if (pSectorBuf) {

        VERIFY (CeSafeCopyMemory (pBuffer, pSectorBuf + dwOfstSector, cbPartial));

        ReleaseSectorBuffer (pVolume, pSectorBuf);
    }
    return NULL != pSectorBuf;
}

BOOL ReadAtOffset (BinVolume *pVolume, LPVOID buffer, DWORD dwBytesToRead, DWORD dwOffset)
{
    DEBUGMSG (ZONE_HELPER, (TEXT("BINFS:ReadAtOffset hDsk=%08X dwBytesToRead=%ld\n"), pVolume->hDsk, dwBytesToRead));

    if (dwBytesToRead) {
        LPBYTE pBuffer      = (LPBYTE) buffer;
        DWORD  cbSector     = pVolume->diskInfo.di_bytes_per_sect;
        DWORD  dwCurSector  = SIZE_TO_SECTOR (pVolume, dwOffset);
        DWORD  dwOfstSector = dwOffset & (cbSector - 1);
        DWORD  cbToRead;
        if (dwOfstSector) {
            cbToRead = cbSector - dwOfstSector;
            if (cbToRead > dwBytesToRead) {
                cbToRead = dwBytesToRead;
            }
            if (!ReadPartial (pVolume, pBuffer, dwCurSector, dwOfstSector, cbToRead)) {
                goto Exit;
            }
            pBuffer       += cbToRead;
            dwBytesToRead -= cbToRead;
            dwCurSector ++;
        }
        
        if (dwBytesToRead >= cbSector) {
            DWORD cSectors = SIZE_TO_SECTOR (pVolume, dwBytesToRead);
            cbToRead       = SECTOR_TO_SIZE (pVolume, cSectors);
            if (ERROR_SUCCESS != ReadDisk (pVolume, dwCurSector, cSectors, pBuffer)) {
                goto Exit;
            }
            pBuffer       += cbToRead;
            dwBytesToRead -= cbToRead;
            dwCurSector   += cSectors;
        }

        if (dwBytesToRead && ReadPartial (pVolume, pBuffer, dwCurSector, 0, dwBytesToRead)) {
            dwBytesToRead = 0;
        }
    }
Exit:
    return !dwBytesToRead;
}

// the size of a compressed block plus padding for a header
#define LZHEADERMAX 64
BYTE pCompressedBits[COMP_BLOCK_SIZE + LZHEADERMAX];

inline DWORD Read3(BinVolume *pVolume, BinDirList *pDirectory, DWORD dwIndex) {
    DWORD dwVal = 0;
    ReadAtOffset(pVolume, (LPBYTE) &dwVal, 3, pDirectory->dwAddress+dwIndex);        
    return dwVal;
}

#define LZBLOCKBITS             12

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
    int iNumBlocks,iBlockIdx,iCurPos;

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
            break;
        ReadAtOffset(pVolume, pCompressedBits, iNxtPos-iCurPos, pDirectory->dwAddress+iCurPos);        
        if (DecompressBinaryBlock(pCompressedBits, iNxtPos-iCurPos, bufout, iOutLen) != 0)
            break;
        if (iOutLen < 0)
            break;
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
    DWORD dwBytesRead = 0;
    
    if (dwPosition < pDirectory->dwRealFileSize) {
        if (dwPosition + dwBytesToRead > pDirectory->dwRealFileSize) {
            dwBytesToRead = pDirectory->dwRealFileSize - dwPosition;
        }
        if (dwBytesToRead) {
            DWORD  dwFullSize = dwBytesToRead;
            LPVOID buffercopy = pBuffer;
            
            LockPages(buffercopy, dwFullSize, 0, LOCKFLAG_WRITE);
            if (pDirectory->dwRealFileSize == pDirectory->dwCompFileSize) {
                ReadAtOffset(pVolume, pBuffer, dwBytesToRead, pDirectory->dwAddress+dwPosition);
                dwBytesRead = dwBytesToRead;
            } else if (!pDirectory->dwCompFileSize) {
                memset (pBuffer, 0, dwBytesToRead);
            } else {
                DWORD dwBlock,bRead,csize;
                DWORD dwResult;
                
                EnterCriticalSection(&pVolume->csComp);
                while (dwBytesToRead) {
                    dwBlock = dwPosition / COMP_BLOCK_SIZE;
                    bRead = dwBytesToRead;
                    if ((dwPosition + bRead-1) / COMP_BLOCK_SIZE != dwBlock) {
                        bRead = COMP_BLOCK_SIZE - (dwPosition & (COMP_BLOCK_SIZE-1));
                    }
                    if ((pVolume->dwCurFileAddress != pDirectory->dwAddress) || (dwBlock != pVolume->dwCurCompBlock)) {
                        if (dwBlock == pDirectory->dwRealFileSize / COMP_BLOCK_SIZE) {
                            csize = pDirectory->dwRealFileSize - (COMP_BLOCK_SIZE*dwBlock);
                        } else {
                            csize = COMP_BLOCK_SIZE;
                        }
                        if (bRead == csize) {
                            dwResult = GetCompressedBytes(pVolume, pDirectory, pBuffer, csize, dwBlock*COMP_BLOCK_SIZE);
                        } else {
                            dwResult = GetCompressedBytes(pVolume, pDirectory, pVolume->pDecBuf, csize, dwBlock*COMP_BLOCK_SIZE);
                            pVolume->dwCurFileAddress = pDirectory->dwAddress;
                            pVolume->dwCurCompBlock = dwBlock;
                            CeSafeCopyMemory (pBuffer, pVolume->pDecBuf + (dwPosition & (COMP_BLOCK_SIZE-1)), bRead);
                        }
                        if (dwResult != csize) {
                            dwBytesToRead = 0;
                            dwBytesRead = dwFullSize;
                            continue;
                        }
                    } else {
                        CeSafeCopyMemory (pBuffer, pVolume->pDecBuf + (dwPosition & (COMP_BLOCK_SIZE-1)), bRead);
                    }
                    pBuffer = pBuffer+bRead;
                    dwBytesToRead -= bRead;
                    dwPosition += bRead;
                }
                dwBytesRead = dwFullSize;
                LeaveCriticalSection(&pVolume->csComp);
            }
            UnlockPages(buffercopy, dwFullSize);
        }
    }
    return dwBytesRead;
}

