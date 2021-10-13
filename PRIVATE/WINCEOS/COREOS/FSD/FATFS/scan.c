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

    scan.c

Abstract:

    This file contains routines for scanning volumes for inconsistencies.

Revision History:

--*/

#include "fatfs.h"


#ifndef USE_FATUTIL

#define LOSTCLUS_PARENT         0x10000000      // start (potentially) of some lost cluster chain
#define LOSTCLUS_CHILD          0x20000000      // referred to by another child or parent
#define LOSTCLUS_MODIFIED       0x80000000
#define LOSTCLUS_FLAGS          (LOSTCLUS_PARENT | LOSTCLUS_CHILD | LOSTCLUS_MODIFIED)

typedef struct _LOSTCLUS {
    DWORD   dwIndex;
    DWORD   dwData;
    struct _LOSTCLUS *plcNext;
} LOSTCLUS, *PLOSTCLUS;


int ScanLostClusters(PVOLUME pvol, PDWORD pdwClusArray, PLOSTCLUS plc, int cLostClusters)
{
    int i, j, k;
    DWORD dwClus;
    int cLostChains = 0;

    for (i=0, j=0, dwClus=DATA_CLUSTER; dwClus <= pvol->v_clusMax && j < cLostClusters; i++,dwClus++) {

        DWORD dwData;
        DWORD dwError = UNPACK(pvol, dwClus, &dwData);

        if (dwError) {
            DEBUGMSG(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXT("FATFS!ScanLostClusters: error reading cluster %d (%d)\n"), dwClus, dwError));
            break;
        }

        if (!TestBitArray(pdwClusArray, i)) {

            if (dwData != FREE_CLUSTER) {

                


                if (dwData == (pvol->v_clusEOF | 0xF))
                    dwData = UNKNOWN_CLUSTER;

                // Make sure the dwIndex fields never get these flag bits set "accidentally"
                ASSERT(!(dwClus & LOSTCLUS_FLAGS));

                plc[j].dwIndex = dwClus;
                plc[j].dwData = dwData;
                plc[j].plcNext = NULL;
                j++;
            }
        }
    }

    ASSERT(j == cLostClusters);

    // OK, now we're ready to identify all the chains.  For every lost cluster,
    // if it has a child, then that child better be lost too, or we must truncate
    // that cluster's chain immediately.  Normally, its child *will* also be lost;
    // we will keep marking all the lost clusters in that chain as LOSTCLUS_CHILD,
    // until we reach the end of the chain or we had to truncate it.

    for (i=0; i<cLostClusters; i++) {

        DWORD dwNext;

        if (plc[i].dwIndex & LOSTCLUS_CHILD)
            continue;   // already visited in the context of some other lost cluster's chain

        ASSERT(!(plc[i].dwIndex & LOSTCLUS_PARENT));
        plc[i].dwIndex |= LOSTCLUS_PARENT;

        k = i;
        dwNext = plc[i].dwData;

        while (dwNext) {

            j = 0;
            if (dwNext > pvol->v_clusMax) {
                // We are either at the natural end of a cluster chain,
                // or we've encountered a too-large cluster number;  if
                // the latter, then we need to properly truncate the chain.
                if (dwNext == UNKNOWN_CLUSTER)
                    break;      // the entry is OK as-is
                goto truncate;
            }

            for (; j<cLostClusters; j++) {

                DWORD dwCur = plc[j].dwIndex;
                if (dwNext == (dwCur & ~LOSTCLUS_FLAGS)) {

                    if (dwCur & LOSTCLUS_CHILD)
                        goto truncate;

                    plc[j].dwIndex |= LOSTCLUS_CHILD;
                    plc[k].plcNext = &plc[j];

                    if (dwCur & LOSTCLUS_PARENT) {
                        plc[j].dwIndex &= ~LOSTCLUS_PARENT;
                        dwNext = 0;
                        break;
                    }
                    dwNext = plc[j].dwData;
                    break;
                }
            }

            // If we couldn't find the next lost cluster, truncate the current
            // lost cluster chain

            if (j == cLostClusters) {
truncate:
                plc[k].dwIndex |= LOSTCLUS_MODIFIED;
                plc[k].dwData = UNKNOWN_CLUSTER;
                break;
            }
            k = j;
        }
    }

    // One more pass through the lost cluster array looking for LOSTCLUS_PARENT flags
    // will tell us how many total lost chains there are.

    for (i=0; i<cLostClusters; i++) {
        if (plc[i].dwIndex & LOSTCLUS_PARENT)
            cLostChains++;
    }

    return cLostChains;
}


DWORD FreeLostClusters(PVOLUME pvol, PLOSTCLUS plc, int cLostClusters)
{
    int i;
    DWORD dwError = ERROR_SUCCESS;

    // Simply zap all the clusters in the LOSTCLUS array...

    for (i=0; i<cLostClusters; i++) {
        dwError = PACK(pvol, plc[i].dwIndex & ~LOSTCLUS_FLAGS, FREE_CLUSTER, NULL);
        if (dwError)
            break;
    }
    return dwError;
}


DWORD ReclaimLostClusters(PVOLUME pvol, PLOSTCLUS plc, int cLostClusters, int cLostChains)
{
    int i, j, k;
    PLOSTCLUS plcCur;
    DWORD dwError = ERROR_SUCCESS;

    // WARNING: If you change this template string, make sure that 
    // wsFileName is still large enough to hold the result from wsprintf! -JTP
    static CONST WCHAR awcTemplate[] = TEXTW("FILE%04d.CHK");


    // For each chain, we want to create a FILEnnnn.CHK file, and hook all the clusters
    // in that chain onto that file.

    for (i=1,k=1; i<=cLostChains && !dwError; i++) {

        PDSTREAM pstm;

        do {
            WCHAR wsFileName[OEMNAMESIZE+2];
            int flName = NAME_FILE | NAME_NEW | NAME_CREATE;

            // Each time we build a name, we also auto-increment the "nnnn" part

            wsprintf(wsFileName, awcTemplate, k++);
            pstm = OpenName((PDSTREAM)pvol, wsFileName, 0, &flName);

            





        } while (!pstm && GetLastError() == ERROR_FILE_EXISTS && k <= 999);

        if (pstm) {

            DWORD dwNewSize = 0;
            ASSERT(pstm->s_clusFirst == UNKNOWN_CLUSTER);

            // Find the beginning of some lost cluster chain, and start threading the
            // clusters on that chain onto this file.

            for (j=0; j<cLostClusters; j++) {
                if (plc[j].dwIndex & LOSTCLUS_PARENT)
                    break;
            }

            ASSERT(j<cLostClusters);    // we should always find a parent

            plcCur = &plc[j];           
                                        // we don't want to find this parent again
            plcCur->dwIndex &= ~LOSTCLUS_PARENT;

            while (plcCur) {

                DWORD dwClus = plcCur->dwIndex & ~LOSTCLUS_FLAGS;

                if (pstm->s_clusFirst == UNKNOWN_CLUSTER) {

                    









                    pstm->s_clusFirst = dwClus;
                    pstm->s_flags |= STF_DIRTY | STF_DIRTY_CLUS;
                }

                if (plcCur->dwIndex & LOSTCLUS_MODIFIED) {
                    dwError = PACK(pvol, dwClus, plcCur->dwData, NULL);
                    if (dwError)
                        break;          
                }

                dwNewSize += pvol->v_cbClus;

                plcCur = plcCur->plcNext;
                ASSERT(plcCur == NULL || plcCur->dwIndex & LOSTCLUS_CHILD);
            }

            // Commit and close the new entry

            if (dwNewSize > 0) {
                pstm->s_size = dwNewSize;
                pstm->s_flags |= STF_DIRTY;
            }

            CloseStream(pstm);
        }
        else
            dwError = GetLastError();
    }
    return dwError;
}


DWORD ModifyCluster(PDSTREAM pstmDir, PDIRENTRY pde, DWORD dwClus)
{
    DWORD dwError = ERROR_SUCCESS;

    if (dwClus < DATA_CLUSTER || dwClus == UNKNOWN_CLUSTER)
        dwClus = NO_CLUSTER;
    if (pde->de_clusFirst != LOWORD(dwClus))
        dwError = ModifyStreamBuffer(pstmDir, &pde->de_clusFirst, sizeof(pde->de_clusFirst));
    if (!dwError) {
        pde->de_clusFirst = LOWORD(dwClus);
        if (pde->de_clusFirstHigh != HIWORD(dwClus))
            dwError = ModifyStreamBuffer(pstmDir, &pde->de_clusFirstHigh, sizeof(pde->de_clusFirstHigh));
        if (!dwError)
            pde->de_clusFirstHigh = HIWORD(dwClus);
    }
    return dwError;
}


/*  ScanDirectory - Scan a directory (on a volume) for inconsistencies
 *
 *  ENTRY
 *      psd - pointer to SCANDATA
 *      pstmDir - pointer to DSTREAM
 *      pdiPrev - pointer to previous DIRINFO, if any
 *      pfdPrev - pointer to previous WIN32_FIND_DATAW, if any
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, error code if not
 */

DWORD ScanDirectory(PSCANDATA psd, PDSTREAM pstmDir, PDIRINFO pdiPrev, PWIN32_FIND_DATAW pfdPrev)
{
    int iFix;
    PWSTR pwsEnd;
    PSHANDLE psh;
    DWORD dwError;
    DIRINFO di;
    PVOLUME pvol = psd->sd_pvol;
    PWIN32_FIND_DATAW pfd = NULL;

    DEBUGMSGW(ZONE_LOGIO,(DBGTEXTW("FATFS!ScanDirectory(\"%s\")\n"), psd->sd_wsPath));

    pwsEnd = psd->sd_wsPath + wcslen(psd->sd_wsPath);

    if (!pstmDir) {

        if (pstmDir = OpenRoot(pvol)) {

#ifdef FAT32
            // Handle clusters in the root directory of a FAT32 volume

            DWORD dwClus = pvol->v_pstmRoot->s_clusFirst, dwData;

            while (dwClus >= DATA_CLUSTER && !ISEOF(pvol, dwClus)) {

                ASSERT(pvol->v_flags & VOLF_32BIT_FAT);
                ASSERT(!TestBitArray(psd->sd_pdwClusArray, dwClus-DATA_CLUSTER));

                dwError = UNPACK(pvol, dwClus, &dwData);
                if (dwError) {
                    DEBUGMSGW(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ScanDirectory: \"<ROOT>\" error reading cluster %d (%d)\n"), dwClus, dwError));
                    goto exit;
                }
                else if (dwData == FREE_CLUSTER) {
                    DEBUGMSGW(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ScanDirectory: \"<ROOT>\" cluster %d points to bogus cluster %d\n"), dwClus, dwData));
                    dwError = ERROR_INVALID_DATA;
                    goto exit;
                }
                SetBitArray(psd->sd_pdwClusArray, dwClus-DATA_CLUSTER);
                dwClus = dwData;
            }
#endif
        }
    }

    if (!pstmDir) {
        dwError = ERROR_ACCESS_DENIED;
        goto exit;
    }

    // The pattern we will search for in the current stream is always "*"

    ALLOCA(psh, sizeof(SHANDLE)-ARRAYSIZE(psh->sh_awcPattern)*sizeof(WCHAR)+2*sizeof(WCHAR));
    if (!psh) {
        psd->sd_flags |= SCANDATA_TOODEEP;
        dwError = ERROR_OUTOFMEMORY;
        goto exit;
    }

    memset(psh, 0, sizeof(SHANDLE)-ARRAYSIZE(psh->sh_awcPattern)*sizeof(WCHAR)+2*sizeof(WCHAR));
    psh->sh_pstm = pstmDir;
    psh->sh_flags = SHF_BYNAME | SHF_WILD;      
    psh->sh_cwPattern = 1;
    psh->sh_awcPattern[0] = '*';

    




    pfd = LocalAlloc(LPTR, sizeof(WIN32_FIND_DATAW));
    if (!pfd) {
        psd->sd_flags |= SCANDATA_TOODEEP;
        dwError = ERROR_OUTOFMEMORY;
        goto exit;
    }
    DEBUGALLOC(sizeof(WIN32_FIND_DATAW));

    do {
        int i;
        BOOL fBadDirent = FALSE;
        DWORD dwClus, dwData, cClus;

        iFix = FATUI_NO;
        dwError = FindNext(psh, &di, pfd);

        if (dwError) {

            if (dwError == ERROR_FILE_NOT_FOUND) {
                dwError = ERROR_SUCCESS;        // eventually running out of files is to be expected
                goto exit;
            }

            if (dwError == ERROR_BAD_UNIT || dwError == ERROR_GEN_FAILURE || dwError == ERROR_NOT_READY || dwError == ERROR_READ_FAULT) {
                dwError = SCAN_CANCELLED;       // abort scan if we get any "driver errors"
                goto exit;
            }

            if (dwError != ERROR_FILE_CORRUPT || pdiPrev == NULL || pfdPrev == NULL)
                goto exit;                      // unknown or uncorrectable error

            // When FindNext() returns ERROR_FILE_CORRUPT, it's telling us that
            // that one or both of the cluster numbers in the "dot" and "dotdot" entries
            // are inconsistent.  If the user elects to correct them, we'll try searching
            // the directory again.

            iFix = psd->sd_pfnScanFix(psd, pdiPrev, pfdPrev, SCANERR_DIR_BADCLUS|SCANERR_REPAIRABLE);
            if (iFix != FATUI_YES) {
                if (iFix == FATUI_CANCEL)
                    dwError = SCAN_CANCELLED;
                goto exit;
            }
            if (dwError = ReadStream(pstmDir, 0, &di.di_pde, NULL))
                goto exit;
            if (dwError = ModifyCluster(pstmDir, di.di_pde, pstmDir->s_clusFirst))
                goto exit;
            di.di_pde++;
            if (dwError = ModifyCluster(pstmDir, di.di_pde, pstmDir->s_sid.sid_clusDir))
                goto exit;
            if (dwError = CommitStreamBuffers(pstmDir))
                goto exit;

            psh->sh_pos = 0;
            continue;                           // try that FindNext() call again now
        }

        psd->sd_cFiles++;
        DEBUGMSG(ZONE_LOGIO,(DBGTEXT("FATFS!ScanDirectory: '%.11hs', clus=%d\n"), di.di_pde->de_name, di.di_clusEntry));

        // We call GETDIRENTRYCLUSTER again to catch the "NO_CLUSTER" case, because even though
        // FindNext() called it and saved the result in di_clusEntry, FindNext *also* maps
        // NO_CLUSTER to UNKNOWN_CLUSTER to reduce the number of different "invalid cluster" values
        // other callers must deal with.  Unfortunately, *we* need to deal with it.

        dwClus = di.di_clusEntry;
        if (GETDIRENTRYCLUSTER(pvol, di.di_pde) == NO_CLUSTER)
            dwClus = NO_CLUSTER;

        for (i=0; i<sizeof(di.di_pde->de_name); i++) {
            


            if (di.di_pde->de_name[i] < ' ' /* || di.di_pde->de_name[i] > 0x7F */ ) {
                DEBUGMSG(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXT("FATFS!ScanDirectory: '%.11hs' contains invalid char at %d: %02x\n"), di.di_pde->de_name, i, di.di_pde->de_name[i]));
                fBadDirent = TRUE;
            }
        }

        if (di.di_clusEntry == UNKNOWN_CLUSTER && (di.di_pde->de_attr & ATTR_DIRECTORY)) {
            DEBUGMSGW(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ScanDirectory: \"%s\" contains bogus directory cluster %d\n"), pfd->cFileName, dwClus));
            fBadDirent = TRUE;
        }

        if (dwClus > pvol->v_clusMax) {
            DEBUGMSGW(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ScanDirectory: \"%s\" contains invalid cluster %d\n"), pfd->cFileName, dwClus));
            fBadDirent = TRUE;
        }

        if (dwClus) {
            if (UNPACK(pvol, dwClus, &dwData) != ERROR_SUCCESS || dwData == FREE_CLUSTER) {
                DEBUGMSGW(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ScanDirectory: \"%s\" contains bogus cluster %d [0x%x]\n"), pfd->cFileName, dwClus, dwData));
                fBadDirent = TRUE;
            }
        }

        if (fBadDirent) {
            iFix = psd->sd_pfnScanFix(psd, &di, pfd, SCANERR_DIR_BADDATA|SCANERR_REPAIRABLE);
            if (iFix == FATUI_CANCEL) {
                dwError = SCAN_CANCELLED;
                goto exit;
            }
            if (iFix == FATUI_YES || iFix == FATUI_DELETE) {    // repair and delete are the same thing in this case...

                // We set di_clusEntry to UNKNOWN_CLUSTER to prevent DestroyName from
                // reclaiming any of the (potentially bogus) clusters referenced by the
                // afflicted DIRENTRY.

                di.di_clusEntry = UNKNOWN_CLUSTER;
                if (dwError = DestroyName(pstmDir, &di))
                    goto exit;
                goto next;      // nothing more to do with this (now deleted) DIRENTRY
            }

            // If we're still here, then the user simply told us not to modify this DIRENTRY.

            


        }

        // Walk all the clusters of the DIRENTRY we just found...

        cClus = 0;
        while (dwClus >= DATA_CLUSTER && !ISEOF(pvol, dwClus)) {

            if (TestBitArray(psd->sd_pdwClusArray, dwClus-DATA_CLUSTER)) {
                DEBUGMSGW(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ScanDirectory: \"%s\" cross-linked on cluster %d (after %d clusters)\n"), pfd->cFileName, dwClus, cClus));
                
                iFix = psd->sd_pfnScanFix(psd, &di, pfd, SCANERR_FAT_CROSSLINK);
                if (iFix == FATUI_CANCEL) {
                    dwError = SCAN_CANCELLED;
                    goto exit;
                }
                break;  // no point in examining the rest of the chain...
            }

            dwError = UNPACK(pvol, dwClus, &dwData);
            if (dwError) {
                DEBUGMSGW(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ScanDirectory: \"%s\" error reading cluster %d (%d)\n"), pfd->cFileName, dwClus, dwError));
            }
            else if (dwData == FREE_CLUSTER) {
                DEBUGMSGW(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ScanDirectory: \"%s\" cluster %d points to bogus cluster %d\n"), pfd->cFileName, dwClus, dwData));
                dwError = ERROR_INVALID_DATA;
            }

            if (dwError) {
                iFix = psd->sd_pfnScanFix(psd, &di, pfd, SCANERR_DIR_BADCLUS|SCANERR_REPAIRABLE);
                if (iFix == FATUI_CANCEL) {
                    dwError = SCAN_CANCELLED;
                    goto exit;
                }
                if (iFix == FATUI_YES) {
                    if (cClus == 0) {
                        // The cluster chain must be truncated inside the DIRENTRY
                        if (dwError = ModifyCluster(pstmDir, di.di_pde, NO_CLUSTER))
                            goto exit;
                        if (dwError = CommitStreamBuffers(pstmDir))
                            goto exit;
                        dwClus = NO_CLUSTER;
                        di.di_clusEntry = UNKNOWN_CLUSTER;
                    }
                    else {
                        // The cluster chain must be truncated outside the DIRENTRY
                        PACK(pvol, dwClus, UNKNOWN_CLUSTER, NULL);      
                    }
                }
                else if (iFix == FATUI_DELETE) {
                    if (dwError = DestroyName(pstmDir, &di))
                        goto exit;
                }
                break;  // in any case, no point in examining the rest of the chain...
            }

            SetBitArray(psd->sd_pdwClusArray, dwClus-DATA_CLUSTER);
            dwClus = dwData;
            cClus++;

            DEBUGMSG(ZONE_LOGIO && ZONE_CLUSTERS,(DBGTEXT("FATFS!ScanDirectory: '%.11hs', next=%d\n"), di.di_pde->de_name, dwClus));
        }

        if (di.di_pde->de_attr & ATTR_DIRECTORY) {

            PDSTREAM pstmSubDir;

            // Whenever we hit this limit, we set a flag that disables free cluster
            // checking (otherwise we would report a bunch of unvisited clusters as lost).

            if (psd->sd_cLevels == MAX_PATH/2) {
                DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!ScanDirectory: hit nesting limit!\n")));
                psd->sd_flags |= SCANDATA_TOODEEP;
                goto next;
            }

            // Since the DIRENTRY is a directory, traverse into it

            pstmSubDir = OpenStream(pvol,
                                    di.di_clusEntry,
                                    &di.di_sid,
                                    pstmDir, &di, OPENSTREAM_CREATE);
            if (pstmSubDir) {

                *pwsEnd = '\\';
                wcscpy(pwsEnd+1, pfd->cFileName);

                // Note that since FindNext() succeeded, it held onto a buffer for us;
                // we need to release that buffer now, otherwise the deepest tree structure
                // we can scan will be equal to the number of buffers in our pool.

                ReleaseStreamBuffer(pstmDir, FALSE);
                DEBUGONLY(di.di_pde = NULL);

                psd->sd_cLevels++;
                dwError = ScanDirectory(psd, pstmSubDir, &di, pfd);
                psd->sd_cLevels--;

                // Note that ScanDirectory always closes the pstm we pass to it, so
                // we do not need to explicitly call CloseStream() for pstmSubDir.
                // A corollary is that pstmSubDir is now invalid, so we will explicitly
                // invalidate it in DEBUG builds.

                DEBUGONLY(pstmSubDir = NULL);

                if (dwError == SCAN_CANCELLED)
                    goto exit;

                if (dwError) {
                    iFix = psd->sd_pfnScanFix(psd, &di, pfd, SCANERR_DIR_BADDATA|SCANERR_REPAIRABLE);
                    if (iFix == FATUI_CANCEL) {
                        dwError = SCAN_CANCELLED;
                        goto exit;
                    }
                    if (iFix == FATUI_YES || iFix == FATUI_DELETE) {        // repair and delete are the same thing in this case...

                        // Since we released buffer above, with the hope/expectation that
                        // we would no longer need it, we were proven wrong, so before we can
                        // call DestroyName, we must refetch a buffer containing the afflicted
                        // DIRENTRY.

                        if (dwError = ReadStream(pstmDir, di.di_pos, &di.di_pde, NULL))
                            goto exit;

                        // We could set di_clusEntry to UNKNOWN_CLUSTER to prevent DestroyName from
                        // reclaiming any of the (potentially bogus) clusters referenced by the
                        // afflicted DIRENTRY, but more often than not, they aren't really bogus. -JTP

                        // di.di_clusEntry = UNKNOWN_CLUSTER;

                        if (dwError = DestroyName(pstmDir, &di))
                            goto exit;

                        goto next;      // nothing more to do with this (now deleted) DIRENTRY
                    }
                }
            }
        }
        else {

            // Since the DIRENTRY is a file, make sure the size field in the DIRENTRY
            // is consistent with the number of clusters currently assigned to it;  this
            // isn't done for directory DIRENTRYs because FAT file systems do not normally
            // either update or rely on sizes, time-stamps, etc, for directories.

            DWORD cbMax = cClus * pvol->v_cbClus;
            DWORD cbMin = cbMax - (cbMax? (pvol->v_cbClus - 1) : 0);

            if (di.di_pde->de_size < cbMin || di.di_pde->de_size > cbMax) {

                DEBUGMSGW(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXTW("FATFS!ScanDirectory: \"%s\" size %d inconsistent with %d total clusters\n"), pfd->cFileName, di.di_pde->de_size, cClus));

                // If iFix is FATUI_YES, the user already said "yes" to repair an earlier error
                // in this file, so let's just go ahead and repair this error too...

                if (iFix != FATUI_YES) {
                    iFix = psd->sd_pfnScanFix(psd, &di, pfd, SCANERR_DIR_BADSIZE|SCANERR_REPAIRABLE);
                    if (iFix == FATUI_CANCEL) {
                        dwError = SCAN_CANCELLED;
                        goto exit;
                    }
                }
                if (iFix == FATUI_YES) {
                    if (dwError = ModifyStreamBuffer(pstmDir, &di.di_pde->de_size, sizeof(di.di_pde->de_size)))
                        goto exit;
                    di.di_pde->de_size = cbMax;
                    if (dwError = CommitStreamBuffers(pstmDir))
                        goto exit;
                }
                else if (iFix == FATUI_DELETE) {
                    if (dwError = DestroyName(pstmDir, &di))
                        goto exit;
                }
            }
        }

        // FindNext() does not advance the SHANDLE's search position automatically when
        // it is asked to fill in a DIRINFO, so we must do it ourselves.  Note that it must
        // be advanced relative to the entry we just found, NOT the last search position.

next:
        psh->sh_pos = di.di_pos + sizeof(DIRENTRY);

    } while (TRUE);

exit:
    if (pfd) {
        DEBUGFREE(sizeof(WIN32_FIND_DATAW));
        VERIFYNULL(LocalFree(pfd));
    }

    if (pstmDir) {
        DWORD dw = CloseStream(pstmDir);
        if (!dwError)
            dwError = dw;
    }

    DEBUGONLY(*pwsEnd = '\0');
    DEBUGMSGW(ZONE_LOGIO,(DBGTEXTW("FATFS!ScanDirectory(\"%s\") returned %d\n"), psd->sd_wsPath, dwError));

    return dwError;
}


/*  ScanFixInteractive - Query user regarding scan error and ask to fix
 *
 *  ENTRY
 *      psd -> SCANDATA
 *      pdi -> DIRINFO
 *      pfd -> WIN32_FIND_DATAW
 *      dwScanErr == one or more SCANERR_* flags
 *
 *  EXIT
 *      FATUI_NONE, FATUI_YES, FATUI_NO, FATUI_CANCEL or FATUI_DELETE
 */

int ScanFixInteractive(PSCANDATA psd, PDIRINFO pdi, PWIN32_FIND_DATAW pfd, DWORD dwScanErr)
{
    CEOIDINFO oi;
    FATUIDATA fui;
    int ret;

    // If the caller to ScanVolume passed a pointer to a dwScanErr
    // variable, collect all the different error types that were encountered;
    // but don't include SCANERR_REPAIRABLE, because that bit is just a signal
    // to us as to whether ScanVolume can actually repair the given error.

    if (psd->sd_pRefData)
        *(PDWORD)psd->sd_pRefData |= (dwScanErr & ~SCANERR_REPAIRABLE);

    if (psd->sd_dwScanVol & SCANVOL_UNATTENDED)
        return (psd->sd_dwScanVol & SCANVOL_REPAIRALL)? FATUI_YES : FATUI_NO;

#if(IDS_FATUI_SCANERR_UNKNOWN   != IDS_FATUI_SCANERR_DIR_BADDATA - 1 || \
    SCANERR_DIR_BADCLUS   != (1 << (IDS_FATUI_SCANERR_DIR_BADCLUS   - IDS_FATUI_SCANERR_DIR_BADDATA)) || \
    SCANERR_DIR_BADSIZE   != (1 << (IDS_FATUI_SCANERR_DIR_BADSIZE   - IDS_FATUI_SCANERR_DIR_BADDATA)) || \
    SCANERR_FAT_BADINDEX  != (1 << (IDS_FATUI_SCANERR_FAT_BADINDEX  - IDS_FATUI_SCANERR_DIR_BADDATA)) || \
    SCANERR_FAT_CROSSLINK != (1 << (IDS_FATUI_SCANERR_FAT_CROSSLINK - IDS_FATUI_SCANERR_DIR_BADDATA)) || \
    SCANERR_FAT_WASTED    != (1 << (IDS_FATUI_SCANERR_FAT_WASTED    - IDS_FATUI_SCANERR_DIR_BADDATA)) )
#error SCANERR_* constants are out of sync with IDS_FATUI_* constants
#endif

    fui.dwSize = sizeof(fui);
    fui.dwFlags = (dwScanErr & SCANERR_REPAIRABLE)? (FATUI_YES | FATUI_NO | FATUI_CANCEL) : (FATUI_CANCEL);
    fui.idsEvent = IDS_FATUI_SCANERR_DIR_BADDATA + Log2(dwScanErr & ~SCANERR_REPAIRABLE);
    fui.idsEventCaption = IDS_FATUI_ERROR;

    fui.auiParams[0].dwType = UIPARAM_STRINGID;
    fui.auiParams[0].dwValue = (dwScanErr & SCANERR_REPAIRABLE)? IDS_FATUI_SCANERR_REPAIR : IDS_FATUI_SCANERR_ADVISE;

    // In the case of SCANERR_FAT_WASTED, pdi and pfd are not DIRINFO and WIN32_FIND_DATAW pointers;
    // pdi is used to pass the number of lost clusters, and pfd is the number of lost chains.

    if (fui.idsEvent == IDS_FATUI_SCANERR_FAT_WASTED) {
        fui.cuiParams = 4;
        fui.auiParams[1].dwType = UIPARAM_VALUE;
        fui.auiParams[1].dwValue = (DWORD)pdi;
        fui.auiParams[2].dwType = UIPARAM_VALUE;
        fui.auiParams[2].dwValue = (DWORD)pfd;
        fui.auiParams[3].dwType = UIPARAM_VALUE;
        fui.auiParams[3].dwValue = (DWORD)(psd->sd_pvol->v_pwsHostRoot+1);
    }
    else {
        fui.cuiParams = 2;
        fui.auiParams[1].dwType = UIPARAM_VALUE;
        fui.auiParams[1].dwValue = (DWORD)pfd->cFileName;
        if (pdi && GetSIDInfo(psd->sd_pvol, &pdi->di_sid, &oi) == ERROR_SUCCESS) {
            int i;
            fui.cuiParams = 3;
            fui.auiParams[2].dwType = UIPARAM_VALUE;
            fui.auiParams[2].dwValue = (DWORD)oi.infFile.szFileName;
            for (i=wcslen(oi.infFile.szFileName); --i > 0;) {
                if (oi.infFile.szFileName[i] == TEXTW('\\')) {
                    oi.infFile.szFileName[i] = 0;       // chop off the final component in the path
                    break;
                }
            }
        }
    }

#ifdef FATUI
    ret = FATUIEvent(hFATFS, psd->sd_pvol->v_pwsHostRoot+1, &fui);
#else
    ret = FATUI_YES;
#endif
    return ret;
}


BOOL ScanFixDefault(PSCANDATA psd, PDIRINFO pdi, PWIN32_FIND_DATAW pfd, DWORD dwScanErr)
{
    int i;
    PVOID p;

    if (p = psd->sd_pRefData)
        ((PSCANRESULTS)psd->sd_pRefData)->sr_dwScanErr |= (dwScanErr & ~SCANERR_REPAIRABLE);

    if (!(psd->sd_dwScanVol & SCANVOL_UNATTENDED)) {
        psd->sd_pRefData = NULL;
        i = ScanFixInteractive(psd, pdi, pfd, dwScanErr);
        psd->sd_pRefData = p;
        return i;
    }

    return (psd->sd_dwScanVol & SCANVOL_REPAIRALL)? FATUI_YES : FATUI_NO;
}


void ScanNotify(PVOLUME pvol, UINT idsMessage)
{
    FATUIDATA fui;

    fui.dwSize = sizeof(fui);
    fui.dwFlags = FATUI_NONE;
    fui.idsEvent = idsMessage;
    fui.idsEventCaption = IDS_FATUI_WARNING;
    fui.cuiParams = 0;
    // Since this is just a notification, we don't care what the FATUIEvent response was...
#ifdef FATUI
    FATUIEvent(hFATFS, pvol->v_pwsHostRoot+1, &fui);
#endif    
}


/*  ScanVolume - Scan a volume for inconsistencies
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      dwScanVol - one or more SCANVOL_* flags
 *      pfnScanFix - pointer to PFNSCANFIX function
 *      pRefData - pointer to caller-defined data;  if pfnScanFix
 *      is NULL, then the default handler (ScanFixDefault) expects
 *      pRefData to be either NULL or a pointer to a SCANRESULTS structure.
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, error code if not, or 0xFFFFFFFF
 *      if scan was interrupted/cancelled.
 *
 *  NOTES
 *      There are a very large number of problems that a FAT volume
 *      can suffer from, but many of them are minor (stuff like non-OEM
 *      characters in 8.3 names, dates in the future, paths that can't
 *      be reached by MSDOS-based applications, etc).
 *
 *      What I deem most important:  (1) that all files point to valid
 *      and unique sets of clusters, (2) that all file sizes agree with
 *      the number of clusters allocated to them, (3) that all other
 *      clusters are marked free, and (4) that all copies of the FAT are
 *      in agreement.
 *
 *      To verify (1) and (3), I allocate a bitarray representing all
 *      the clusters on the volume, and then start walking the directory
 *      structure, and every cluster chain in that structure, and flag
 *      every cluster in use.  If a cluster is already marked in use,
 *      then we have a cross-link.  Now a limitation of the bitmap is
 *      that I won't be able to (easily) identify the first file using
 *      the same clusters, but I can at least list the second file and offer
 *      to allocate it separate clusters with duplicate contents.
 */

DWORD ScanVolume(PVOLUME pvol, DWORD dwScanVol, PFNSCANFIX pfnScanFix, PVOID pRefData)
{
    BOOL iFix;
    SCANDATA sd;
    DWORD i, dwClus;
    DWORD cClusters;
    DWORD dwError = ERROR_SUCCESS;

    // An invalid volume is generally an unformatted volume;  in any case, if the
    // volume is known to be invalid, there's no point in scanning it.  Similarly, if
    // we're already scanning it, no need to do so again.

    if (pvol->v_flags & (VOLF_INVALID | VOLF_SCANNING))
        goto abort;             

    DEBUGMSGW(ZONE_LOGIO,(DBGTEXTW("FATFS!ScanVolume(%s) starting...\n"), pvol->v_pwsHostRoot));

    dwError = LockVolume(pvol, VOLF_READLOCKED);
    if (dwError) {
        DEBUGMSG(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXT("FATFS!ScanVolume: unable to lock volume (%d)\n"), dwError));
        goto exit;
    }

    // Set the SCANNING flag to prevent re-entrancy, and clear the MODIFIED flag
    // so that if no modifications are made during the scan, UnlockVolume can avoid
    // an expensive unmount/remount of the volume.

    pvol->v_flags |= VOLF_SCANNING;
    pvol->v_flags &= ~VOLF_MODIFIED;

    ScanNotify(pvol, IDS_FATUI_SCANSTART_WARNING);

    // Initialize the SCANDATA structure and allocate a bitarray for all the clusters on the volume

    sd.sd_pvol = pvol;
    sd.sd_dwScanVol = dwScanVol;
    sd.sd_pfnScanFix = (pfnScanFix? pfnScanFix : ScanFixDefault);
    sd.sd_pRefData = pRefData;

    ASSERT(pvol->v_clusMax > DATA_CLUSTER);
    cClusters = pvol->v_clusMax - DATA_CLUSTER + 1;

    CreateBitArray(sd.sd_pdwClusArray, cClusters);
    if (!sd.sd_pdwClusArray) {
        dwError = ERROR_NOT_ENOUGH_MEMORY;
        goto unlockvol;
    }

    LockFAT(pvol);

    sd.sd_cFiles = 0;
    sd.sd_cLevels = 0;
    sd.sd_flags = SCANDATA_NONE;
    sd.sd_wsPath[0] = '\0';
    dwError = ScanDirectory(&sd, NULL, NULL, NULL);

    DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!ScanVolume scanned %d files (%d)\n"), sd.sd_cFiles, dwError));

    if (sd.sd_flags & SCANDATA_TOODEEP) {

        // This should simply generate the IDS_SCANERR_UNKNOWN message.  We
        // don't care what the response is, because it's a generic, unrepairable error.

        sd.sd_pfnScanFix(&sd, NULL, NULL, 0);
    }
    else if (dwError != SCAN_CANCELLED) {
        
        // Now make sure that all the clusters in the FAT for which the
        // corresponding bit in the bitarray is clear are actually "free",
        // and since we're verifying the set of free clusters, we might as well
        // take this opportunity to update the cached free cluster count in the
        // VOLUME structure too...

        




        


        int cLostClusters = 0;

        dwError = ERROR_SUCCESS;
        pvol->v_cclusFree = 0;

        for (i=0, dwClus=DATA_CLUSTER; dwClus <= pvol->v_clusMax; i++,dwClus++) {
            DWORD dwData;
            dwError = UNPACK(pvol, dwClus, &dwData);
            if (dwError) {
                DEBUGMSG(ZONE_LOGIO || ZONE_ERRORS,(DBGTEXT("FATFS!ScanVolume: error reading cluster %d (%d)\n"), dwClus, dwError));
                pvol->v_cclusFree = UNKNOWN_CLUSTER;    // if we bail, zap cached free cluster count too
                break;
            }
            if (!TestBitArray(sd.sd_pdwClusArray, i)) {
                if (dwData == FREE_CLUSTER)
                    pvol->v_cclusFree++;                // update cached free cluster count
                else if (dwData == (pvol->v_clusEOF|0x8)-1) {
                    SetBitArray(sd.sd_pdwClusArray, i);
                    DEBUGMSG(ZONE_LOGIO,(DBGTEXT("FATFS!ScanVolume: cluster %d marked bad (0x%x)\n"), dwClus, dwData));
                }
                else {
                    cLostClusters++;
                    // Used to be ZONE_LOGIO || ZONE_ERRORS, but too noisy when card has lots of lost clusters...
                    DEBUGMSG(ZONE_LOGIO,(DBGTEXT("FATFS!ScanVolume: cluster %d should be free but instead contains %d (0x%x)\n"), dwClus, dwData, dwData));
                }
            }
            else {
                // dwData == FREE_CLUSTER suggests a bit-array inconsistency, hence the break -JTP
                DEBUGMSGBREAK(ZONE_LOGIO && dwData == FREE_CLUSTER,(DBGTEXT("FATFS!ScanVolume: cluster %d should be used but instead is free\n"), dwClus));
            }
        }

        


        if (cLostClusters*sizeof(LOSTCLUS) > 32*1024) {

            // This should simply generate the IDS_SCANERR_UNKNOWN message.  We
            // don't care what the response is, because it's a generic, unrepairable error.

            sd.sd_pfnScanFix(&sd, NULL, NULL, 0);
            dwError = ERROR_OUTOFMEMORY;
            cLostClusters = 0;
        }

        if (cLostClusters) {

            PLOSTCLUS plc;

            ALLOCA(plc, cLostClusters*sizeof(LOSTCLUS));
            if (!plc) {
                DEBUGMSG(ZONE_ERRORS,(DBGTEXT("FATFS!ScanVolume: unable to allocate %d bytes to recover lost clusters\n"), cLostClusters*sizeof(LOSTCLUS)));
                dwError = ERROR_OUTOFMEMORY;
            }
            else {
                // Identify all the chains now

                int cLostChains;
                cLostChains = ScanLostClusters(pvol, sd.sd_pdwClusArray, plc, cLostClusters);

                // Offer to repair (ie, recover) all the chains

                iFix = sd.sd_pfnScanFix(&sd, (PVOID)cLostClusters, (PVOID)cLostChains, SCANERR_FAT_WASTED|SCANERR_REPAIRABLE);
                if (iFix == FATUI_CANCEL) {
                    dwError = SCAN_CANCELLED;
                }
                else if (iFix == FATUI_YES) {
                    dwError = ReclaimLostClusters(pvol, plc, cLostClusters, cLostChains);
                }
                else if (iFix == FATUI_DELETE) {
                    dwError = FreeLostClusters(pvol, plc, cLostClusters);
                }
            }
        }

        


        
    }

    // Commit the FAT now, just in case we dirtied any of its buffers...

    CommitAndReleaseStreamBuffers(pvol->v_pstmFAT);

#ifdef TFAT
    //  sync the FAT1 back to FAT0  
    if (pvol->v_fTfat)
        CommitTransactions (pvol);
#endif
    
    UnlockFAT(pvol);

    DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!ScanVolume scanned %d clusters (%d free)\n"), i, pvol->v_cclusFree));

unlockvol:
    ScanNotify(pvol, IDS_FATUI_SCANDONE_WARNING);

    UnlockVolume(pvol);

exit:
    pvol->v_flags &= ~VOLF_SCANNING;

    DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!ScanVolume(%s) complete, returned %d\n"), pvol->v_pwsHostRoot, dwError));

abort:
    return dwError;
}

#endif
