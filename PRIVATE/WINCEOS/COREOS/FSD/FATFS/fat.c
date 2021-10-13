//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    fat.c

Abstract:

    This file contains routines for manipulating the FAT.

Revision History:

--*/

#include "fatfs.h"


/*  LockFAT - Wrapper around EnterCriticalSection for FAT streams
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *
 *  EXIT
 *      None.
 */

void LockFAT(PVOLUME pvol)
{
    EnterCriticalSection(&pvol->v_pstmFAT->s_cs);
}


/*  GetFAT - Wrapper around ReadStreamBuffer for FAT streams
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      dwOffset - offset within FAT stream to read
 *      ppvEntry - address of pointer to first byte of buffered data
 *      ppvEntryEnd - address of pointer to last byte of buffered data
 *
 *  EXIT
 *      ERROR_SUCCESS or other error code (same as ReadStreamBuffer)
 */

DWORD GetFAT(PVOLUME pvol, DWORD dwOffset, PVOID *ppvEntry, PVOID *ppvEntryEnd)
{
    DWORD dwError;
    PDSTREAM pstmFAT = pvol->v_pstmFAT;
    DWORD cbBuf = pvol->v_pdsk->d_diActive.di_bytes_per_sect;

    





    // However, releasing the FAT CS here isn't very feasible, because other
    // threads could arrive and change the FAT stream's current buffer info
    // without our knowledge.  BUT, we can get away with this for now, because
    // we know that the set of drivers that support demand-paging also acquire
    // their own critical sections, and by so doing, are obligated to avoid
    // making any kernel call that could block them.  So the fact that we still
    // hold some of our own critical sections does not make matters any worse. -JTP

    ASSERT(OWNCRITICALSECTION(&pvol->v_pstmFAT->s_cs));

    if (pstmFAT->s_flags & STF_BUFCURHELD) {

        DWORD off = dwOffset - pstmFAT->s_offbufCur;
        if (off < cbBuf) {
            *ppvEntry = pstmFAT->s_pbufCur->b_pdata + off;
            if (ppvEntryEnd)
                *ppvEntryEnd = pstmFAT->s_pbufCur->b_pdata + cbBuf;
            return ERROR_SUCCESS;
        }
    }

    dwError = ReadStreamBuffer(pstmFAT, dwOffset, 0, ppvEntry, ppvEntryEnd);

    if (dwError) {
        ASSERT(!(pstmFAT->s_flags & STF_BUFCURHELD));
    }
    else {
        // Use the FAT stream's s_offbufCur field to record the offset
        // that corresponds to the start of the FAT stream's current buffer.

        pstmFAT->s_offbufCur = dwOffset - ((PBYTE)*ppvEntry - pstmFAT->s_pbufCur->b_pdata);
    }
    return dwError;
}


/*  UnlockFAT - Wrapper around LeaveCriticalSection for FAT streams
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *
 *  EXIT
 *      None.  Nested calls to LockFAT/UnlockFAT are perfectly
 *      legitimate, because the critical section maintains a count;
 *      however, ReleaseStreamBuffer does not keep a count with respect
 *      to the buffer, so since the buffer could get reused now, we need
 *      to clear the current buffer pointer, so that we don't use it
 *      as a hint.
 */

void UnlockFAT(PVOLUME pvol)
{
    ASSERT(OWNCRITICALSECTION(&pvol->v_pstmFAT->s_cs));

    ReleaseStreamBuffer(pvol->v_pstmFAT, FALSE);
    LeaveCriticalSection(&pvol->v_pstmFAT->s_cs);
}


/*  Unpack12 - Unpack 12-bit FAT entries
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      clusIndex - cluster index to unpack (to obtain next cluster)
 *      pclusData - pointer to returned cluster (UNKNOWN_CLUSTER if error)
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, or an error code (eg, ERROR_INVALID_DATA)
 */

DWORD Unpack12(PVOLUME pvol, DWORD clusIndex, PDWORD pclusData)
{
    DWORD dwError;
    DWORD dwOffset, clusData;
    PBYTE pEntry, pEntryEnd;

    ASSERT(OWNCRITICALSECTION(&pvol->v_pstmFAT->s_cs));

    DEBUGMSG(ZONE_FATIO,(DBGTEXT("FATFS!Unpack12: unpacking cluster %d\r\n"), clusIndex));

    *pclusData = UNKNOWN_CLUSTER;

    if (clusIndex > pvol->v_clusMax) {
        DEBUGMSG(ZONE_FATIO|ZONE_ERRORS,(DBGTEXT("FATFS!Unpack12: invalid cluster index: %d\r\n"), clusIndex));
        return ERROR_INVALID_DATA;
    }

    // The address of the first byte of a given cluster's data is the
    // cluster # times 1.5.

    dwOffset = clusIndex + (clusIndex>>1);
    dwError = GetFAT(pvol, dwOffset, &pEntry, &pEntryEnd);

    if (dwError)
        return dwError;

    if (pEntryEnd - pEntry > 1)
        clusData = *(UNALIGNED WORD *)pEntry;
    else {

        // We must dereference pEntry before calling GetFAT again,
        // because without an explicit hold on the current buffer, the
        // ReadStreamBuffer call that GetFAT makes could choose to reuse it.

        clusData = *pEntry;             // get low byte

        dwError = GetFAT(pvol, dwOffset+1, &pEntryEnd, NULL);
        if (dwError)
            return dwError;

        clusData |= *pEntryEnd << 8;    // get high byte
    }

    if (clusIndex & 1)
        clusData >>= 4;                 // adjust odd cluster value

    *pclusData = clusData & FAT12_EOF_MAX;
    return dwError;
}


/*  Pack12 - Pack 12-bit FAT entries
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      clusIndex - cluster index to pack
 *      clusData - cluster data to pack at clusIndex
 *      pclusOld - pointer to old cluster data at clusIndex, NULL if not needed
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, or an error code (eg, ERROR_INVALID_DATA)
 */

DWORD Pack12(PVOLUME pvol, DWORD clusIndex, DWORD clusData, PDWORD pclusOld)
{
    WORD wMask;
    DWORD dwError;
    DWORD clusOrig = clusData;
    DWORD dwOffset, clusCur, clusOld, clusNew;
    PBYTE pEntry, pEntryEnd;

    ASSERT(OWNCRITICALSECTION(&pvol->v_pstmFAT->s_cs));

    DEBUGMSG(ZONE_FATIO,(DBGTEXT("FATFS!Pack12: packing cluster %d\r\n"), clusIndex));

    if (clusIndex > pvol->v_clusMax) {
        DEBUGMSG(ZONE_FATIO|ZONE_ERRORS,(DBGTEXT("FATFS!Pack12: invalid cluster index: %d\r\n"), clusIndex));
        return ERROR_INVALID_DATA;
    }

    // The address of the first byte of a given cluster's data is the
    // cluster # times 1.5.

    dwOffset = clusIndex + (clusIndex>>1);

    dwError = GetFAT(pvol, dwOffset, &pEntry, &pEntryEnd);
    if (dwError)
        return dwError;

    if (pEntryEnd - pEntry > 1) {

        clusCur = clusOld = *(UNALIGNED WORD *)pEntry;
                                        // get combined bytes
        wMask = FAT12_EOF_MAX;
        if (clusIndex & 1) {
            clusData <<= 4;             // adjust odd cluster value
            wMask = FAT12_EOF_MAX << 4;
            clusOld >>= 4;              // fix the return value
        }

        clusNew = (clusCur & ~wMask) | (clusData & wMask);

        if (clusNew != clusCur) {       // if the cluster entry is changing...
            if (ModifyStreamBuffer(pvol->v_pstmFAT, pEntry, 2) == ERROR_SUCCESS) {
                *(UNALIGNED WORD *)pEntry = (WORD)clusNew;
            }
        }
    }
    else {
        PBUF pbuf = pvol->v_pstmFAT->s_pbufCur;

        // Because GetFAT does an implicit unhold on the current
        // FAT buffer, we need to explicitly hold the first buffer.

        HoldBuffer(pbuf);

        clusOld = *pEntry;              // get low byte

        dwError = GetFAT(pvol, dwOffset+1, &pEntryEnd, NULL);
        if (dwError) {
            UnholdBuffer(pbuf);
            return dwError;
        }

        clusOld |= *pEntryEnd << 8;     // get high byte
        clusCur = clusOld;              // get combined bytes

        wMask = FAT12_EOF_MAX;
        if (clusIndex & 1) {
            clusData <<= 4;             // adjust odd cluster value
            wMask = FAT12_EOF_MAX << 4;
            clusOld >>= 4;              // fix the return value
        }

        clusNew = (clusCur & ~wMask) | (clusData & wMask);

        if (clusNew != clusCur) {       // if the cluster entry is changing...
            if (ModifyBuffer(pbuf, pEntry, 1) == ERROR_SUCCESS) {
                *pEntry = (BYTE)clusNew;
                if (ModifyStreamBuffer(pvol->v_pstmFAT, pEntryEnd, 1) == ERROR_SUCCESS) {
                    *pEntryEnd = (BYTE)(clusNew >> 8);
                }
            }
        }
        UnholdBuffer(pbuf);
    }

    clusOld &= FAT12_EOF_MAX;

    if (clusNew != clusCur) {

        if (pvol->v_cclusFree != UNKNOWN_CLUSTER) {
            if (clusOrig == FREE_CLUSTER)
                pvol->v_cclusFree++;
            else if (clusOld == FREE_CLUSTER)
                pvol->v_cclusFree--;
        }
    }

    if (pclusOld)
        *pclusOld = clusOld;
    return dwError;
}


/*  Unpack16 - Unpack 16-bit FAT entries
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      clusIndex - cluster index to unpack (to obtain next cluster)
 *      pclusData - pointer to returned cluster (UNKNOWN_CLUSTER if error)
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, or an error code (eg, ERROR_INVALID_DATA)
 */

DWORD Unpack16(PVOLUME pvol, DWORD clusIndex, PDWORD pclusData)
{
    PWORD pEntry;
    DWORD dwError;

    ASSERT(OWNCRITICALSECTION(&pvol->v_pstmFAT->s_cs));

    DEBUGMSG(ZONE_FATIO,(DBGTEXT("FATFS!Unpack16: unpacking cluster %d\r\n"), clusIndex));

    *pclusData = UNKNOWN_CLUSTER;

    if (clusIndex > pvol->v_clusMax) {
        DEBUGMSG(ZONE_FATIO|ZONE_ERRORS,(DBGTEXT("FATFS!Unpack16: invalid cluster index: %d\r\n"), clusIndex));
        return ERROR_INVALID_DATA;
    }

    dwError = GetFAT(pvol, clusIndex*2, &pEntry, NULL);
    if (!dwError)
        *pclusData = *pEntry & FAT16_EOF_MAX;

    return dwError;
}


/*  Pack16 - Pack 16-bit FAT entries
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      clusIndex - cluster index to pack
 *      clusData - cluster data to pack at clusIndex
 *      pclusOld - pointer to old cluster data at clusIndex, NULL if not needed
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, or an error code (eg, ERROR_INVALID_DATA)
 */

DWORD Pack16(PVOLUME pvol, DWORD clusIndex, DWORD clusData, PDWORD pclusOld)
{
    PWORD pEntry;
    DWORD clusOld, dwError;

    ASSERT(OWNCRITICALSECTION(&pvol->v_pstmFAT->s_cs));

    DEBUGMSG(ZONE_FATIO,(DBGTEXT("FATFS!Pack16: packing cluster %d\r\n"), clusIndex));

    if (clusIndex > pvol->v_clusMax) {
        DEBUGMSG(ZONE_FATIO|ZONE_ERRORS,(DBGTEXT("FATFS!Pack16: invalid cluster index: %d\r\n"), clusIndex));
        return ERROR_INVALID_DATA;
    }

    dwError = GetFAT(pvol, clusIndex*2, &pEntry, NULL);
    if (dwError)
        return dwError;

    clusData &= FAT16_EOF_MAX;
    clusOld = *pEntry & FAT16_EOF_MAX;

    if (clusOld != clusData) {

        if (ModifyStreamBuffer(pvol->v_pstmFAT, pEntry, 2) == ERROR_SUCCESS) {
            *pEntry = (WORD)clusData;
        }
        if (pvol->v_cclusFree != UNKNOWN_CLUSTER) {
            if (clusData == FREE_CLUSTER) {
                IncrementFreeClusterCount (pvol, clusIndex);
            }
            else if (clusOld == FREE_CLUSTER) {
                DecrementFreeClusterCount (pvol, clusIndex);
            }
        }
    }

    if (pclusOld)
        *pclusOld = clusOld;

    return dwError;
}


#ifdef FAT32

/*  Unpack32 - Unpack 32-bit FAT entries
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      clusIndex - cluster index to unpack (to obtain next cluster)
 *      pclusData - pointer to cluster data (UNKNOWN_CLUSTER if error)
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, or an error code (eg, ERROR_INVALID_DATA)
 */

DWORD Unpack32(PVOLUME pvol, DWORD clusIndex, PDWORD pclusData)
{
    PDWORD pEntry;
    DWORD dwError;

    ASSERT(OWNCRITICALSECTION(&pvol->v_pstmFAT->s_cs));

    DEBUGMSG(ZONE_FATIO,(DBGTEXT("FATFS!Unpack32: unpacking cluster %d\r\n"), clusIndex));

    *pclusData = UNKNOWN_CLUSTER;

    if (clusIndex > pvol->v_clusMax) {
        DEBUGMSG(ZONE_FATIO|ZONE_ERRORS,(DBGTEXT("FATFS!Unpack32: invalid cluster index: %d\r\n"), clusIndex));
        return ERROR_INVALID_DATA;
    }

    dwError = GetFAT(pvol, clusIndex*4, &pEntry, NULL);
    if (!dwError)
        *pclusData = *pEntry & FAT32_EOF_MAX;

    return dwError;
}


/*  Pack32 - Pack 32-bit FAT entries
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      clusIndex - cluster index to pack
 *      clusData - cluster data to pack at clusIndex
 *      pclusOld - pointer to old cluster data at clusIndex, NULL if not needed
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, or an error code (eg, ERROR_INVALID_DATA)
 */

DWORD Pack32(PVOLUME pvol, DWORD clusIndex, DWORD clusData, PDWORD pclusOld)
{
    PDWORD pEntry;
    DWORD clusOld, dwError;

    ASSERT(OWNCRITICALSECTION(&pvol->v_pstmFAT->s_cs));

    DEBUGMSG(ZONE_FATIO,(DBGTEXT("FATFS!Pack32: packing cluster %d\r\n"), clusIndex));

    if (clusIndex > pvol->v_clusMax) {
        DEBUGMSG(ZONE_FATIO|ZONE_ERRORS,(DBGTEXT("FATFS!Pack32: invalid cluster index: %d\r\n"), clusIndex));
        return ERROR_INVALID_DATA;
    }

    dwError = GetFAT(pvol, clusIndex*4, &pEntry, NULL);
    if (dwError)
        return dwError;

    clusData &= FAT32_EOF_MAX;
    clusOld = *pEntry & FAT32_EOF_MAX;

    if (clusOld != clusData) {

        if (ModifyStreamBuffer(pvol->v_pstmFAT, pEntry, 4) == ERROR_SUCCESS) {
            *pEntry = clusData;
        }
        if (pvol->v_cclusFree != UNKNOWN_CLUSTER) {
            if (clusData == FREE_CLUSTER) {
                IncrementFreeClusterCount (pvol, clusIndex);
            }
            else if (clusOld == FREE_CLUSTER) {
                DecrementFreeClusterCount (pvol, clusIndex);
            }
        }
    }

    if (pclusOld)
        *pclusOld = clusOld;

    return dwError;
}

#endif  // FAT32





















DWORD UnpackRun(PDSTREAM pstm)
{
    DWORD Result = ERROR_SUCCESS;
    PVOLUME pvol = pstm->s_pvol;
    DWORD CurrentCluster = NO_CLUSTER;
    DWORD NextCluster = 0;
    
    LockFAT(pvol);

    NextCluster = GetNextClusterInRun (&pstm->s_runList);

    // If NextCluster is still set to UNKNOWN_CLUSTER, it means that
    // either the stream is not cluster-mapped (eg, FAT, root directory),
    // or no clusters have been allocated to it yet.

    if (NextCluster < DATA_CLUSTER || ISEOF(pvol, NextCluster))
    {
        Result = ERROR_HANDLE_EOF;
        goto exit;
    }

    // Start decoding the next run now.

    DEBUGMSG(ZONE_FATIO,(TEXT("FATFS!UnpackRun: unpacking run at cluster %d\r\n"), NextCluster));
    
    do 
    {
        CurrentCluster = NextCluster;

        Result = UNPACK(pvol, CurrentCluster, &NextCluster);
        if (Result != ERROR_SUCCESS)
        {
            goto exit;
        }

        Result = AppendRunList(&pstm->s_runList, INVALID_POS, NextCluster);
        if (Result != ERROR_SUCCESS)
        {
            goto exit;
        }
        
    } while (NextCluster == CurrentCluster + 1);

exit:
    UnlockFAT (pvol);
    return Result;
}



/*  NewCluster - Find available cluster
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      clusPrev - last allocated cluster
 *      pclusNew - pointer to DWORD to receive new cluster #
 *      (set to UNKNOWN_CLUSTER if no more clusters are available)
 *
 *      If the caller is trying to extend a stream, then he should pass
 *      clusPrev equal to the last cluster in the stream, in the hope of
 *      extending the file contiguously.  Callers that don't care or don't
 *      know can simply pass UNKNOWN_CLUSTER, and we'll start searching
 *      from the last allocated cluster.
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, or an error code (eg, ERROR_INVALID_DATA)
 *
 *  NOTES
 *      We index into the FAT at clusPrev+1, and if the entry is zero
 *      (FREE_CLUSTER), then we return that entry.  Otherwise, we scan
 *      forward, wrapping around to the beginning of the FAT, until we
 *      we reach clusPrev again.
 *
 *      If clusPrev is UNKNOWN_CLUSTER, then we first set clusPrev
 *      to pvol->v_clusAlloc, and if pvol->v_clusAlloc is UNKNOWN_CLUSTER
 *      as well, then we set clusPrev to DATA_CLUSTER-1 (ie, the cluster
 *      immediately preceding the beginning of the data area, for lack of a
 *      better starting point).
 *
 *      At the end of every successful allocation, we set v_clusAlloc
 *      to the cluster just allocated.
 *
 *      To prevent a free cluster from being used before we have a chance
 *      to link it into a cluster chain, the caller must own v_pstmFAT's
 *      critical section.
 *
 *      Last but not least, don't forget that you're holding onto the last
 *      FAT buffer examined, assuming we successfully located a free cluster.
 */

DWORD NewCluster(PVOLUME pvol, DWORD clusPrev, PDWORD pclusNew)
{
    DWORD dwError;
    DWORD clus;
    DWORD oldClusPrev = clusPrev;

    ASSERT(OWNCRITICALSECTION(&pvol->v_pstmFAT->s_cs));

    // As long as cclusFree is non-zero (which includes UNKNOWN_CLUSTER, which means
    // we have no idea yet how many free clusters there are, if any), then we have to
    // search the FAT.

    dwError = ERROR_SUCCESS;
    *pclusNew = UNKNOWN_CLUSTER;

    if (pvol->v_cclusFree == 0) {
        goto exit;
    }

    if (clusPrev == UNKNOWN_CLUSTER) {
        clusPrev = pvol->v_clusAlloc;
        if (clusPrev == UNKNOWN_CLUSTER) {
            clusPrev = DATA_CLUSTER-1;
        }
    }

    if (pvol->v_pFreeClusterList) {

        // Case where free cluster list is cached
        LPDWORD pdwFreeClusterList = NULL; 
        DWORD indexCount = 0;
        if (++clusPrev > pvol->v_clusMax) {
            clusPrev = DATA_CLUSTER;
        }

        pdwFreeClusterList = (LPDWORD)pvol->v_pFreeClusterList + (clusPrev / CLUSTERS_PER_LIST_DWORD_ENTRY);

        // The loop will potentially visit every free cluster list entry,
        // and intentially may visit the first entry twice, since the starting
        // cluster (clusPrev) may be in the middle of the entry
        while (indexCount <= (pvol->v_clusterListSize / sizeof(DWORD))) {
           
            if (*pdwFreeClusterList != 0) {

                // Determine the byte offset within the current DWORD where the cluster
                // entry falls
                DWORD byteOffset = (clusPrev % CLUSTERS_PER_LIST_DWORD_ENTRY) / CLUSTERS_PER_LIST_ENTRY;
                LPBYTE pbFreeClusterList = (LPBYTE)pdwFreeClusterList + byteOffset;
                
                // Calculate the ending cluster (one past) of the current byte
                DWORD clusEnd = (clusPrev + CLUSTERS_PER_LIST_ENTRY) & ~(CLUSTERS_PER_LIST_ENTRY - 1);

                // Examine each of the bytes within the DWORD to find one with free clusters
                while (byteOffset < sizeof(DWORD)) {
                    
                    if (*pbFreeClusterList != 0) {
                        
                        // Don't read past the max cluster
                        clusEnd = min (clusEnd, pvol->v_clusMax + 1);

                        while (clusPrev < clusEnd) {
                            
                            dwError = UNPACK(pvol, clusPrev, &clus);
                            if (dwError) {
                                goto exit;
                            }

                            ASSERT(clus != UNKNOWN_CLUSTER);

                            if (clus == FREE_CLUSTER) {
                                pvol->v_clusAlloc = clusPrev;
                                *pclusNew = clusPrev;
                                goto exit;
                            }      
                            clusPrev++;
                        }
                    }
                    
                    pbFreeClusterList++;        
                    byteOffset++;
                    clusPrev = clusEnd;
                    clusEnd += CLUSTERS_PER_LIST_ENTRY;
                    
                }
            }
            else {
                // Advance clusPrev to the next dword
                clusPrev = (clusPrev + CLUSTERS_PER_LIST_DWORD_ENTRY) & ~(CLUSTERS_PER_LIST_DWORD_ENTRY - 1);
            }            
            pdwFreeClusterList++;
            indexCount++;
            if (clusPrev > pvol->v_clusMax) {
                clusPrev = DATA_CLUSTER;
                pdwFreeClusterList = (LPDWORD)pvol->v_pFreeClusterList;
            }
        }
    }
    else {

        // Case where free cluster list is not cached
        DWORD cclusTotal;
        
        for (cclusTotal = DATA_CLUSTER; cclusTotal < pvol->v_clusMax;  cclusTotal++) {

            if (++clusPrev > pvol->v_clusMax)
                clusPrev = DATA_CLUSTER;

            dwError = UNPACK(pvol, clusPrev, &clus);
            if (dwError) {
                goto exit;
            }
            ASSERT(clus != UNKNOWN_CLUSTER);

            if (clus == FREE_CLUSTER) {
                pvol->v_clusAlloc = clusPrev;
                *pclusNew = clusPrev;
                goto exit;
            }
        }
    }
    
    // If we didn't break out of the loop prematurely, there are no more clusters
    pvol->v_cclusFree = 0;

exit:
    // If a clusPrev was provided, this mean that the file is being extended
    // from clusPrev.  However, clusPrev may not have been marked as used yet,
    // so add check to ensure the same cluster isn't being returned back.
    if (oldClusPrev == *pclusNew) {
        *pclusNew = UNKNOWN_CLUSTER;
    }
        
    
    DEBUGMSG(ZONE_FATIO,(DBGTEXT("FATFS!NewCluster: returned cluster 0x%08x (%d)\r\n"), *pclusNew, dwError));
    return dwError;
}

VOID IncrementFreeClusterCount (PVOLUME pvol, DWORD clusIndex)
{
    if (pvol->v_cclusFree >= (DATA_CLUSTER + pvol->v_clusMax - 1)) {
        DEBUGMSGBREAK (ZONE_ERRORS, (TEXT("FATFS!IncrementFreeClusterCount - Overall free cluster count is already at max")));
    } else {
        pvol->v_cclusFree++;
    }

    if (pvol->v_pFreeClusterList) { 
        if (pvol->v_pFreeClusterList[clusIndex / CLUSTERS_PER_LIST_ENTRY] >= CLUSTERS_PER_LIST_ENTRY) {
            DEBUGMSGBREAK (ZONE_ERRORS, (TEXT("FATFS!IncrementFreeClusterCount - Free cluster count at entry for cluster %d is already at max"), clusIndex));
        } else {
            pvol->v_pFreeClusterList[clusIndex / CLUSTERS_PER_LIST_ENTRY]++;
        }    
    }
}

VOID DecrementFreeClusterCount (PVOLUME pvol, DWORD clusIndex)
{
    if (pvol->v_cclusFree == 0) {
        DEBUGMSGBREAK (ZONE_ERRORS, (TEXT("FATFS!DecrementFreeClusterCount - Overall free cluster count is already at 0")));
    } else {
        pvol->v_cclusFree--;
    }
    
    if (pvol->v_pFreeClusterList) { 
        if (pvol->v_pFreeClusterList[clusIndex / CLUSTERS_PER_LIST_ENTRY] == 0) {
            DEBUGMSGBREAK (ZONE_ERRORS, (TEXT("FATFS!DecrementFreeClusterCount - Free cluster count at entry for cluster %d is already at 0"), clusIndex));
        } else {
            pvol->v_pFreeClusterList[clusIndex / CLUSTERS_PER_LIST_ENTRY]--;
        }    
    }   
}

