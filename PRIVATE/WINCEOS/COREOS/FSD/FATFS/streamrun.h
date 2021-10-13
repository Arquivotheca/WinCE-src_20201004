//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#ifndef STREAMRUN_H
#define STREAMRUN_H

/*  Data run:
 *
 *  This structure is used to describe a contiguous run of data on the disk.
 *  Note that the start and end fields are stream-relative, whereas all the
 *  block and cluster fields are volume-relative.
 *
 *  NOTE that although r_blk and r_clusThis convey similar information, we
 *  can't eliminate either of them.  That's because the FAT (and possibly ROOT)
 *  streams are not cluster-mapped, so the only fields that get used in those
 *  cases are the first three (r_start, r_end and r_blk).
 */

#define NUM_STREAM_RUNS 10

typedef struct _ClusterRun {
    DWORD           StartCluster;    // first cluster of this run
    DWORD           NumClusters;     // number of clusters in this run
} ClusterRun, *PClusterRun;

typedef struct _RunInfo {
    DWORD           StartPosition;   // starting byte position of this run in stream  
    DWORD           EndPosition;     // ending byte (+1) position of this run in stream
    DWORD           StartCluster;    // first cluster of this run
    DWORD           NumClusters;     // number of clusters in this run
    DWORD           StartBlock;     // starting sector of the run
} RunInfo, *PRunInfo;

typedef struct _StreamRunList
{    
    PVOLUME         r_pvol;
    DWORD           r_StartPosition;   // starting byte position of run list
    DWORD           r_EndPosition;     // ending byte of run list + 1
    DWORD           r_StartBlock;     // used for non-cluster mapped streams
    DWORD           r_CurrentIndex;    // current index to use
    ClusterRun      r_RunList[NUM_STREAM_RUNS];    
} StreamRunList, *PStreamRunList;

VOID InitStreamRunList(PStreamRunList pRunList, PVOLUME pvol, DWORD FirstCluster);
VOID SetRunSize(PStreamRunList pRunList, DWORD Size);
VOID SetRunStartBlock(PStreamRunList pRunList, DWORD StartBlock);

DWORD ResetRunList (PStreamRunList pRunList, DWORD FirstCluster);
DWORD AppendRunList (PStreamRunList pRunList, DWORD Position, DWORD Cluster);
DWORD TruncateRunList(PStreamRunList pRunList, DWORD Position);

DWORD GetRunInfo (PStreamRunList pRunList, DWORD Position, PRunInfo pRunInfo);
DWORD GetClusterAtPosition (PStreamRunList pRunList, DWORD Position, PDWORD pCluster);
DWORD GetNextClusterInRun(PStreamRunList pRunList);

DWORD ShiftRunList(PStreamRunList pRunList);
DWORD GetRunSize (PStreamRunList pRunList, DWORD Index);
DWORD GetEndClusterInRun (PStreamRunList pRunList, DWORD Index);
BOOL  ValidateRunListIntegrity(PStreamRunList pRunList);

    
#endif // STREAMRUN_H
