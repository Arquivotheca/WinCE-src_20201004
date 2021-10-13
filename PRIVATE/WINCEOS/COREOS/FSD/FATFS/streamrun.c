//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//


#include "fatfs.h"

VOID InitStreamRunList(PStreamRunList pRunList, PVOLUME pvol, DWORD FirstCluster)
{
    pRunList->r_pvol = pvol;
    pRunList->r_StartPosition = 0;
    pRunList->r_EndPosition = 0;    
    pRunList->r_StartBlock = 0;
    pRunList->r_CurrentIndex = 0;
    
    memset (pRunList->r_RunList, 0, sizeof(pRunList->r_RunList));
    ResetRunList(pRunList, FirstCluster);
}

VOID SetRunSize(PStreamRunList pRunList, DWORD Size)
{
    // Only called by non-cluster mapped streams, to set up one fixed run
    //
    pRunList->r_EndPosition = Size;
}

VOID SetRunStartBlock(PStreamRunList pRunList, DWORD StartBlock)
{
    // Only called by non-cluster mapped streams, to set up one fixed run
    //
    pRunList->r_StartBlock = StartBlock;
}


DWORD  ResetRunList (PStreamRunList pRunList, DWORD FirstCluster)
{
    pRunList->r_StartPosition = 0;
    
    if ((FirstCluster < DATA_CLUSTER) || (FirstCluster == UNKNOWN_CLUSTER))
    {
        pRunList->r_RunList[0].StartCluster = FirstCluster;
        pRunList->r_RunList[0].NumClusters = 0;
        pRunList->r_EndPosition = 0;
    }
    else
    {
        pRunList->r_RunList[0].StartCluster = FirstCluster;
        pRunList->r_RunList[0].NumClusters = 1;
        pRunList->r_EndPosition = pRunList->r_pvol->v_cbClus;
    }
    
    pRunList->r_CurrentIndex = 0;
    
    return ERROR_SUCCESS;
}

DWORD  AppendRunList (PStreamRunList pRunList, DWORD Position, DWORD Cluster)
{
    DWORD Result = ERROR_SUCCESS;
    BOOL IsEofCluster = ISEOF (pRunList->r_pvol, Cluster);

    if (Cluster < DATA_CLUSTER)
    {
        DEBUGMSGBREAK (1, (TEXT("FATFS!AppendRunList: Invalid cluster (%d)"), Cluster));
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    // The position is not required, and is only used for verification purposes
    // in a stream extend case
    //
    if ((Position != INVALID_POS) && (Position != pRunList->r_EndPosition))
    {
        DEBUGMSGBREAK (1, (TEXT("FATFS!AppendRunList: Invalid Position (%d)"), Position));
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    // If the current run is pointed to an empty run, then move
    // the index back one place, if possible
    //
    if ((pRunList->r_RunList[pRunList->r_CurrentIndex].NumClusters == 0) && (pRunList->r_CurrentIndex != 0))
    {
        ASSERT (Position != INVALID_POS);
        pRunList->r_CurrentIndex--;
    }
    
    if (!IsEofCluster && (GetEndClusterInRun(pRunList, pRunList->r_CurrentIndex) + 1) == Cluster)
    {
        // Add the new cluster to the current run
        //        
        pRunList->r_RunList[pRunList->r_CurrentIndex].NumClusters++;
        pRunList->r_EndPosition += pRunList->r_pvol->v_cbClus;
    }
    else
    {
        // Shift the runs down one slot if there is no more room
        // left for the new run.  Else, add new run in next slot.
        //
        if (pRunList->r_CurrentIndex == (NUM_STREAM_RUNS - 1))
        {
            ShiftRunList(pRunList);
        }
        else if (pRunList->r_RunList[pRunList->r_CurrentIndex].NumClusters != 0)
        {
            pRunList->r_CurrentIndex++;
        }
        
        pRunList->r_RunList[pRunList->r_CurrentIndex].StartCluster = Cluster;

        if (IsEofCluster)
        {
            pRunList->r_RunList[pRunList->r_CurrentIndex].NumClusters = 0;
        }
        else
        {
            pRunList->r_RunList[pRunList->r_CurrentIndex].NumClusters = 1;
            pRunList->r_EndPosition += pRunList->r_pvol->v_cbClus;
        }
    }

exit:       
    DEBUGMSG (ZONE_READVERIFY, (TEXT("FATFS!AppendRunList: Position = %d, Cluster = %d.\r\n"), Position, Cluster));
    ASSERT (ValidateRunListIntegrity(pRunList));
    return Result;
}

DWORD  TruncateRunList(PStreamRunList pRunList, DWORD Position)
{
    DWORD Result = ERROR_SUCCESS;
    DWORD CurrentIndex = 0;
    DWORD CurrentStartPosition = pRunList->r_StartPosition;
    DWORD CurrentEndPosition = CurrentStartPosition + GetRunSize(pRunList, CurrentIndex);
    
    if (Position < pRunList->r_StartPosition || Position > pRunList->r_EndPosition)
    {
        DEBUGMSGBREAK (1, (TEXT("FATFS!TruncateRun: Invalid Position (%d)"), Position));
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }
    
    while (Position > CurrentEndPosition)
    {
        CurrentIndex++;
        if (CurrentIndex > pRunList->r_CurrentIndex)
        {
            DEBUGMSGBREAK (1, (TEXT("FATFS!TruncateRun: Invalid run index (%d)."), CurrentIndex));
            Result = ERROR_INVALID_PARAMETER;
            goto exit;
        }
        
        CurrentStartPosition = CurrentEndPosition;
        CurrentEndPosition = CurrentStartPosition + GetRunSize(pRunList, CurrentIndex);
    }

    // Number of clusters is Position - CurrentStartPosition in terms of clusters, rounded up
    //
    pRunList->r_RunList[CurrentIndex].NumClusters = (Position - CurrentStartPosition + pRunList->r_pvol->v_cbClus - 1) >> 
        pRunList->r_pvol->v_log2cbBlk >> pRunList->r_pvol->v_log2cblkClus;
    
    pRunList->r_EndPosition = CurrentStartPosition + GetRunSize(pRunList, CurrentIndex);

    // Remove the run from the list if it is zero length
    //
    if (pRunList->r_RunList[CurrentIndex].NumClusters == 0)
    {
        if (CurrentIndex)
        {
            CurrentIndex--;
        }
        else
        {
            pRunList->r_RunList[CurrentIndex].StartCluster = 0;
        }
    }

    pRunList->r_CurrentIndex = CurrentIndex;

exit:
    DEBUGMSG (ZONE_READVERIFY, (TEXT("FATFS!TruncateRun: Position = %d.\r\n"), Position));
    ASSERT (ValidateRunListIntegrity(pRunList));
    return Result;
}

DWORD  GetRunInfo (PStreamRunList pRunList, DWORD Position, PRunInfo pRunInfo)
{
    DWORD Result = ERROR_SUCCESS;
    DWORD CurrentIndex = 0;
    DWORD CurrentStartPosition = 0;
    DWORD CurrentEndPosition = 0;
    
    if (Position < pRunList->r_StartPosition || Position > pRunList->r_EndPosition)
    {
        DEBUGMSGBREAK (1, (TEXT("FATFS!GetRunInfo: Invalid Position (%d)"), Position));
        Result = ERROR_INVALID_PARAMETER;
        goto exit;
    }

    if (pRunList->r_RunList[0].StartCluster < DATA_CLUSTER)
    {
        // This is not cluster mapped, so there is only one run
        //
        pRunInfo->StartPosition = pRunList->r_StartPosition;
        pRunInfo->EndPosition = pRunList->r_EndPosition;
        pRunInfo->StartCluster = 0;
        pRunInfo->NumClusters = 0;
        pRunInfo->StartBlock = pRunList->r_StartBlock;
        goto exit;
    }
    
    CurrentStartPosition = pRunList->r_StartPosition;
    CurrentEndPosition = CurrentStartPosition + GetRunSize(pRunList, CurrentIndex);
    
    while (Position >= CurrentEndPosition)
    {
        CurrentIndex++;
        if (CurrentIndex > pRunList->r_CurrentIndex)
        {
            DEBUGMSGBREAK (1, (TEXT("FATFS!GetRunInfo: Invalid run index (%d)."), CurrentIndex));
            Result = ERROR_INVALID_PARAMETER;
            goto exit;
        }
        
        CurrentStartPosition = CurrentEndPosition;
        CurrentEndPosition = CurrentStartPosition + GetRunSize(pRunList, CurrentIndex);
    }

    // Current run found
    //
    pRunInfo->StartPosition = CurrentStartPosition;
    pRunInfo->EndPosition = CurrentEndPosition;
    pRunInfo->StartCluster = pRunList->r_RunList[CurrentIndex].StartCluster;
    pRunInfo->NumClusters = pRunList->r_RunList[CurrentIndex].NumClusters;
    pRunInfo->StartBlock = CLUSTERTOBLOCK(pRunList->r_pvol, pRunInfo->StartCluster);
    
exit:
    return Result;
}


DWORD  GetClusterAtPosition (PStreamRunList pRunList, DWORD Position, PDWORD pCluster)
{
    DWORD Result = ERROR_SUCCESS;
    RunInfo RunInfo;
    DWORD NumClusters = 0;

    *pCluster = UNKNOWN_CLUSTER;
    
    Result = GetRunInfo (pRunList, Position, &RunInfo);
    if (Result != ERROR_SUCCESS)
    {
        goto exit;
    }

    NumClusters = (Position - RunInfo.StartPosition) >> pRunList->r_pvol->v_log2cbBlk >> pRunList->r_pvol->v_log2cblkClus;
    *pCluster = RunInfo.StartCluster + NumClusters;

exit:
    return Result;
}

DWORD  ShiftRunList(PStreamRunList pRunList)
{
    DWORD i = 0;
    
    // The first run will be removed from the list.  Slide the window
    // of the start position for currently cached run list to
    // account for this
    //
    pRunList->r_StartPosition += GetRunSize(pRunList, 0);
    
    for (i = 0; i < NUM_STREAM_RUNS - 1; i++)
    {
        pRunList->r_RunList[i] = pRunList->r_RunList[i+1];    
    }
    
    return ERROR_SUCCESS;
}

DWORD GetNextClusterInRun(PStreamRunList pRunList)
{
    DWORD NextCluster = pRunList->r_RunList[pRunList->r_CurrentIndex].StartCluster;

    if (pRunList->r_RunList[pRunList->r_CurrentIndex].NumClusters > 1)
    {
        NextCluster += (pRunList->r_RunList[pRunList->r_CurrentIndex].NumClusters - 1);
    }
    
    return NextCluster; 
}


DWORD GetRunSize (PStreamRunList pRunList, DWORD Index)
{
    return pRunList->r_RunList[Index].NumClusters << pRunList->r_pvol->v_log2cbBlk << pRunList->r_pvol->v_log2cblkClus;
}

DWORD GetEndClusterInRun (PStreamRunList pRunList, DWORD Index)
{
    return pRunList->r_RunList[Index].StartCluster + pRunList->r_RunList[Index].NumClusters - 1;
}

BOOL  ValidateRunListIntegrity(PStreamRunList pRunList)
{
    DWORD RunListSize = 0;
    DWORD i = 0;
    
#ifdef DEBUG    
    if (pRunList->r_CurrentIndex >= NUM_STREAM_RUNS)
    {
        return FALSE;
    }

    if (pRunList->r_EndPosition < pRunList->r_StartPosition)
    {
        return FALSE;
    }
    
    DEBUGMSG (ZONE_READVERIFY, (TEXT("FATFS!ValidateListIntegrity: Start Position = %d, End Position = %d.\r\n"), pRunList->r_StartPosition, pRunList->r_EndPosition));

    
    for (i = 0; i <= pRunList->r_CurrentIndex; i++)
    {
        DEBUGMSG (ZONE_READVERIFY, (TEXT("FATFS!ValidateListIntegrity: %d.  StartCluster = %d, Num clusters = %d.\r\n"), i, pRunList->r_RunList[i].StartCluster, pRunList->r_RunList[i].NumClusters));
        RunListSize += GetRunSize(pRunList, i);
    }

    if (RunListSize != (pRunList->r_EndPosition - pRunList->r_StartPosition))
    {
        return FALSE;
    }
#endif

    return TRUE;
}
