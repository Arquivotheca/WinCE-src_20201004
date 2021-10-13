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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
//
// Helper Library
//
//

#ifndef BR_HLP_H
#define BR_HLP_H

#include <windows.h>
#include <sdcommon.h>
#include <sdcard.h>
#include <sdcardddk.h>
#include <sd_tst_cmn.h>
#include <sdtst.h>

typedef enum RWType
{
    Read_Partial = 0,
    Write_Partial = 1,
    Read_Misalign = 2,
    Write_Misalign = 3
};

#define SDTST_REQ_TIMEOUT    500
#define SDTST_REQ_RW_START_ADDR 0x123
#define SDTST_STANDARD_BLKSIZE 512

SD_API_STATUS DetermineBlockSize(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams, PDWORD pBlockSize);

SD_API_STATUS WriteBlocks(UINT indent, ULONG numBlocks, ULONG blockSize, DWORD dwDataAddress, PUCHAR pBuff, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams, BOOL bIssueCMD12=TRUE);

SD_API_STATUS ReadBlocks(UINT indent, ULONG numBlocks, ULONG blockSize, DWORD dwDataAddress, PUCHAR pBuff, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams, BOOL bIssueCMD12=TRUE);

BOOL IsBlockEraseSupported(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, SD_API_STATUS *pRStat);

BOOL IsEraseLow(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, SD_API_STATUS *psdStat, PSD_COMMAND_RESPONSE pResp);

SD_API_STATUS SDPerformErase(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams, ULONG addressFirstBlock, ULONG addressOfLastBlock, PSD_COMMAND_RESPONSE pResp);

SD_API_STATUS SDEraseMemoryBlocks(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams, ULONG addressFirstBlock, ULONG addressOfLastBlock, PBOOL pbSetLow, DWORD dwBlockLength=512);

BOOL bCompareResponseToCardInfo(UINT indent, SD_DEVICE_HANDLE hDevice, SD_INFO_TYPE it, PSD_COMMAND_RESPONSE pResp, SD_API_STATUS *stat=NULL);

BOOL SendCardToStandbyState(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, SD_API_STATUS *rStat);

BOOL SendCardToTransferState(UINT indent, SD_DEVICE_HANDLE hDevice, SD_CARD_RCA RCA, PTEST_PARAMS pTestParams, SD_API_STATUS *rStat);

DWORD GetCurrentState(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pT);

DWORD GetCurrentStateNoFailOnError(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pT);

BOOL IsState(UINT indent, DWORD dwKnownState, DWORD dwUnknownState, PBOOL pbIsSkipped, PTEST_PARAMS pTestParams, LPCTSTR msg, BOOL bBadStateIsAbort, BOOL bSilentFail=FALSE);

SD_API_STATUS GetRelativeAddress(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams, SD_CARD_RCA *pRCA);

BOOL AllocateResponseBuff(UINT indent, PTEST_PARAMS  pTestParams, PSD_COMMAND_RESPONSE *ppResp, BOOL bIsFailure);

SD_API_STATUS AppCMD(UINT indent,  SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams, PSD_COMMAND_RESPONSE pResp, SD_CARD_RCA RCA);

SD_API_STATUS SetBlockLength(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, PSD_COMMAND_RESPONSE pResp, DWORD dwBlockLength = 512);

BOOL IsWPSupported(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pT, PBOOL pbSuccess, PUCHAR pucSectSize = NULL, PUCHAR pucWPGSize = NULL, PDWORD pdwCardSize = NULL);

void GenerateEventName(UINT indent, UINT num, PTEST_PARAMS pTestParams, LPTSTR *plptstr);

BOOL AreBlocksCleared(UINT indent, PBOOL pbErr, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams, ULONG addressFirstBlock, ULONG addressOfLastBlock, BOOL bSetLow, DWORD dwBlockLength=512);

SD_API_STATUS IssueCMD12(SD_DEVICE_HANDLE hDevice, PSD_CARD_STATUS pCrdStat = NULL);

BOOL IsRWTypeSupported(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams, RWType rwt, SD_API_STATUS* prStat);

#endif //BR_HLP_H

