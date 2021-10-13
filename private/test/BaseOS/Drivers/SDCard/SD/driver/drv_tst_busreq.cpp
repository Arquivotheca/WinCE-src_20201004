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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
#include "br_hlp.h"
#include <intsafe.h>
#include <SDCardDDK.h>
#include <sdcard.h>
#include <sdcard.h>
#include <sddrv.h>
#include <sd_tst_cmn.h>
#include <safeInt.hxx>
#include <sddetect.h>
#define NEW_THREAD_PRIORITY 90
//#define SIZE

typedef struct
{
    PTEST_PARAMS                 pTestParams;
    PSD_COMMAND_RESPONSE       pResponse;
    UINT                         indent;
    SD_API_STATUS                      Status;
    UINT                        eventID;
}TEST_SYNC_INFO, *PTEST_SYNC_INFO;

typedef struct _BR_Params
{

    DWORD                         arg;
    SD_TRANSFER_CLASS             sdtc;
    SD_RESPONSE_TYPE             rt;
    PSD_COMMAND_RESPONSE         pResp;
    ULONG                         ulNBlks;
    ULONG                        ulBlkSize;
    PUCHAR                        pBuffer;
    DWORD                        dwFlags;
    //2 Synchronous Requests Only
    PSD_BUS_REQUEST            pReq;
} BR_Params, *PBR_Params;

void InitBRParams(PBR_Params pbrp)
{
    if (pbrp)
    {
        ZeroMemory(pbrp, sizeof(BR_Params));
        pbrp->arg = 0;
        pbrp->sdtc = SD_COMMAND;
        pbrp->rt = NoResponse;
        pbrp->pResp = NULL;
        pbrp->ulNBlks = 0;
        pbrp->ulBlkSize = 0;
        pbrp->pBuffer = NULL;
        pbrp->dwFlags = 0;
        pbrp->pReq = NULL;
    }
}

void GenericRequestCallback(SD_DEVICE_HANDLE /*hDevice*/, PSD_BUS_REQUEST pRequest, PVOID /*pContext*/, DWORD RequestParam)
{
    HANDLE hEvent = NULL;
    PTEST_SYNC_INFO pSync= NULL;
    UINT indent = 0;
    UINT indent1 = 1;
    UINT indent2 = 2;

    if (RequestParam != NULL)
    {
        pSync = (PTEST_SYNC_INFO)RequestParam;
        indent = pSync->indent;
        indent1 = indent + 1;
        PREFAST_ASSERT(indent1 > indent);
        indent2 = indent + 2;
        PREFAST_ASSERT(indent2 > indent);
        LogDebug(indent, SDIO_ZONE_TST, TEXT("GenericRequestCallback: Entering Callback..."));
        if (pSync->pResponse)
        {
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("GenericRequestCallback: Copying response data into structure so calling function can access it..."));

            pSync->Status = SdBusRequestResponse(pRequest, pSync->pResponse);
            if (!SD_API_SUCCESS(pSync->Status))
            {
                LogFail(indent2, TEXT("GenericRequestCallback: SdBusRequestResponse not succeed with error %d"), pSync->Status);
                pSync->Status = SD_API_STATUS_ACCESS_VIOLATION;
                if ((pSync->pTestParams) && (IsBufferEmpty(pSync->pTestParams)))
                {
                    SetMessageBufferAndResult(pSync->pTestParams, TPR_FAIL, TEXT("Return FAILED when trying to get the response from the bus request!!!"));
                }
            }
        }
    }
    else
    {
        LogDebug(indent, SDIO_ZONE_TST, TEXT("Entering Callback..."));
    }
    hEvent = OpenEvent (EVENT_ALL_ACCESS, FALSE, BR_EVENT_NAME);
    if (hEvent)
    {
        SetEvent(hEvent);
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("GenericRequestCallback:BusRequestEvent Found"));
    }
    else
    {
        LogFail(indent1, TEXT("GenericRequestCallback:BusRequestEvent Not Found!!!"));
        if ((pSync) && (pSync->pTestParams) && (IsBufferEmpty(pSync->pTestParams)))
        {
            SetMessageBufferAndResult(pSync->pTestParams, TPR_FAIL, TEXT("GenericRequestCallback:BusRequestEvent Not Found!!!"));
        }
        else if ((pSync) && (pSync->pTestParams) && (IsBufferEmpty(pSync->pTestParams) == FALSE))
        {
            AppendMessageBufferAndResult(pSync->pTestParams, TPR_FAIL, TEXT("GenericRequestCallback:BusRequestEvent Not Found!!!"));
        }
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GenericRequestCallback: Exiting Callback..."));

}

BOOL CMD7PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    BOOL                bInCorrectState = FALSE;
    DWORD                state;
    SD_CARD_RCA        RCA;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD7Pre: Entering Pre-CMD7 Bus Request Code..."));

    //2 Setting Card to Standby State
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD7Pre: Setting card to the STANDBY state..."));
    bInCorrectState = SendCardToStandbyState(indent2, hDevice, pTestParams, &rStat);
    if (!(bInCorrectState))
    {
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent1,TEXT("CMD7Pre: Function to set card to STANDBY state failed. Getting to that state is a requirement for this test. ErrorCode:%s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("CMD7Pre:  An error occurred while attempting to set the card to the STANDBY state. ErrorCode:%s (0x%x) returned.\nThe card needs to be in that state in order to get data from the CSD register."), TranslateErrorCodes(rStat), rStat);
            goto DONE;
        }
        else
        {
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD7Pre: Getting current state..."));
            state = GetCurrentState(indent2, hDevice, pTestParams);
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD7Pre :The card is in a state that I can not push it into the  STANDBY state. The card is in the %s (%u) state"), TranslateState(state), state);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("CMD7Pre: I am unable to transition from the cards current state to the STANDBY state.\nThe card needs to be in that state in order to be selected."));
            goto DONE;
        }
        goto DONE;
    }
    //2 Getting Relative address
    rStat = GetRelativeAddress(indent1, hDevice, pTestParams, &RCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD7Pre: Bailing..."));
        goto DONE;
    }
    //2 Setting params for Bus Request
    pbrp->arg = RCA <<16;
    pbrp->rt = ResponseR1b;
    //2 Allocating Response Buffer
    AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), FALSE);
    bContinue = TRUE;
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD7Pre: Exiting Pre-CMD7 Bus Request Code..."));
    return bContinue;
}

void CMD7PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    DWORD                state;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD7Post: Entering Post-CMD7 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD7Post: To verify if the card has been reselected I must verify that it is back in the TRANSFER state."));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD7Post: Getting current state..."));
    state = GetCurrentState(indent3, hDevice, pTestParams);
    if (state != SD_STATUS_CURRENT_STATE_TRAN)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD7Post: Current state = %s (%u) I expected it to be in the TRANSFER state if the request worked."), TranslateState(state), state);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("The card has not been correctly selected. It is in the %s (%u) state, not the TRANSFER state.."), TranslateState(state), state);
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD7Post: Exiting Post-CMD7 Bus Request Code..."));
}

BOOL CMD9PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp, SD_CARD_RCA *pRCA)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
//    SD_CARD_RCA        RCA;
    BOOL                bInCorrectState = FALSE;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD9Pre: Entering Pre-CMD9 Bus Request Code..."));
    //2 Getting Relative address
    rStat = GetRelativeAddress(indent1, hDevice, pTestParams, pRCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD9Pre: Bailing..."));
        goto DONE;
    }
    //2 Setting Card to Standby State
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD9Pre: Setting card to the STANDBY state..."));
    bInCorrectState = SendCardToStandbyState(indent2, hDevice, pTestParams, &rStat);
    if (!(bInCorrectState))
    {
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent3,TEXT("CMD9Pre: Function to set card to STANDBY state failed. Getting to that state is a requirement for this test. ErrorCode:%s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("CMD9Pre:  An error occurred while attempting to set the card to the STANDBY state. ErrorCode:%s (0x%x) returned.\nThe card needs to be in that state in order to get data from the CSD register."), TranslateErrorCodes(rStat), rStat);
            goto DONE;
        }
        else
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD9Pre: The card is in a state that I can not push it into the  STANDBY state. The test can not proceed."));
            SetMessageBufferAndResult(pTestParams, TPR_SKIP,TEXT("CMD9Pre: I am unable to transition from the card's current state to the STANDBY state.\nThe card needs to be in that state in order to get data from the CSD register. Skipping..."));
            goto DONE;
        }
        goto DONE;
    }
    //2 Setting Custom Parameters
    pbrp->arg = (*pRCA)<<16;
    pbrp->rt = ResponseR2;
    //2 Allocating Response Buffer
    if (!(AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), TRUE)))
    {
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The CSD  register data is gleaned from the response. "));
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD9Pre: Bailing..."));
        goto DONE;
    }
    bContinue = TRUE;
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD9Pre: Exiting Pre-CMD9 Bus Request Code..."));
    return bContinue;
}

void CMD9PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, SD_CARD_RCA const *pRCA, PSD_COMMAND_RESPONSE pResp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    SD_API_STATUS             rStat = 0;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD9Post: Entering Post-CMD9 Bus Request Code..."));
    //2 Set Card back to transfer state
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD9Post: Retuning card to the TRANSFER state..."));
    if (!(SendCardToTransferState(indent2, hDevice, *pRCA, pTestParams, &rStat)))
    {
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent2, TEXT("CMD9Post: An error occurred while trying to return the card to the TRANSFER state. %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("CMD9Post: An error occurred while trying to return the card to the TRANSFER state. %s (0x%x) returned.\nThis will likely lead to subsequent test failures."), TranslateErrorCodes(rStat), rStat);
        }
        else
        {
            LogFail(indent2, TEXT("CMD9Post: The card has wound up in a state that the test can not set back to the TRANSFER state."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("CMD9Post: The card has wound up in a state that the test can not set back to the TRANSFER state.\nThis will likely lead to subsequent test failures."));
        }
    }
    // comparison of the CSD data and data cached in bus register
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD9Post: To test if the CSD is correctly gotten I will compare the response from the bus request to the info cached in the SD bus driver..."));
    switch(bCompareResponseToCardInfo(indent2, hDevice, SD_INFO_REGISTER_CSD, pResp, &rStat))
    {
        case TRUE:
            //4 Every thing is ok
            break;
        case FALSE:
            //4 Hitting this option probably means the cached info is wrong
            LogFail(indent2, TEXT("CMD9Post: The CSD register returned in the Response to the bus request does not match the CSD register info cached by the bus driver."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure due to the CSD register gotten from the bus request not matching the CSD register cached in the bus. Perhaps the cached CSD register is not correctly being updated in the bus?."));
            break;
        case -1:
            //4 Hitting this option should not be possible.
            LogFail(indent2, TEXT("CMD9Post: The response returned from the bus request was NULL. Something fishy is happening here, because a null response should never have been passed to a bus request in this test."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Test code failure. Invalid parameter indicated."));
            break;
        case -2:
            //4 Low memory
            LogFail(indent2, TEXT("CMD9Post: Not enough memory to create a structure to contain the SD bus's cached info from the card's CSD register."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Not enough memory to verify the result of the bus request returned matches the cached CSD register.."));
            break;
        case -3:
            //4 Hitting this option should also not be possible
            LogFail(indent2, TEXT("CMD9Post: The function I used to make the comparison says it does not support comparisons of this ."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Test code failure. Invalid parameter indicated."));
            break;
        default:
            //4 The SDCardInfoQuery call failed
            LogFail(indent2, TEXT("CMD9Post: The comparison was unable to be made due to an API failure."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Can't get the CSD register info from the SD bus so I can not compare against the response from the bus request. SDCardInfoQuery %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);

    }    
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD9Post: Exiting Post-CMD9 Bus Request Code..."));
}

BOOL CMD10PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp, SD_CARD_RCA *pRCA)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    BOOL                bInCorrectState = FALSE;
//    SD_CARD_RCA        RCA;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD10Pre: Entering Pre-CMD10 Bus Request Code..."));
    //2 Getting Relative address
    rStat = GetRelativeAddress(indent1, hDevice, pTestParams, pRCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD10Pre: Bailing..."));
        goto DONE;
    }
    //2 Setting Card to Standby State
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD10Pre: Setting card to the STANDBY state..."));
    bInCorrectState = SendCardToStandbyState(indent2, hDevice, pTestParams, &rStat);
    if (!(bInCorrectState))
    {
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent1,TEXT("CMD10Pre: Function to set card to STANDBY state failed. Getting to that state is a requirement for this test. ErrorCode:%s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("CMD10Pre:  An error occurred while attempting to set the card to the STANDBY state. ErrorCode:%s (0x%x) returned.\nThe card needs to be in that state in order to get data from the CID register."), TranslateErrorCodes(rStat), rStat);
            goto DONE;
        }
        else
        {
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD10Pre: The card is in a state that I can not push it into the  STANDBY state. The test can not proceed."));
            SetMessageBufferAndResult(pTestParams, TPR_SKIP,TEXT("CMD10Pre: I am unable to transition from the card's current state to the STANDBY state.\nThe card needs to be in that state in order to get data from the CID register. Skipping..."));
            goto DONE;
        }
        goto DONE;
    }
    //2 Setting Custom Parameters
    pbrp->arg = (*pRCA) <<16;
    pbrp->rt = ResponseR2;
    //2 Allocating Response Buffer
    if (!(AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), TRUE)))
    {
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The CID register data is gleaned from the response."));
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD10Pre: Bailing..."));
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD10Pre: Exiting Pre-CMD10 Bus Request Code..."));
    bContinue = TRUE;
DONE:

    return bContinue;
}

void CMD10PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, SD_CARD_RCA const *pRCA, PSD_COMMAND_RESPONSE pResp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    SD_API_STATUS         rStat = 0;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD10Post: Entering Post-CMD10 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD10Post: To test if the CID info is correctly gotten I will compare the response from the bus request to the info cached in the SD bus driver..."));
    switch(bCompareResponseToCardInfo(indent2, hDevice, SD_INFO_REGISTER_CID, pResp, &rStat))
    {
        case TRUE:
            //4 Every thing is ok
            break;
        case FALSE:
            //4 Hitting this option probably means the cached info is wrong
            LogFail(indent2, TEXT("CMD10Post: The CID register returned in the Response to the bus request does not match the CID register info cached by the bus driver."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure due to the CID register gotten from the bus request not matching the CID register cached in the bus. As this register is not updatable it is a bit of a mystery as to how this can happen.\nPerhaps, the host architecture has changed since the test was written."));
            break;
        case -1:
            //4 Hitting this option should not be possible.
            LogFail(indent2, TEXT("CMD10Post: The response returned from the bus request was NULL. Something fishy is happening here, because a null response should never have been passed to a bus request in this test."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Test code failure. Invalid parameter indicated."));
            break;
        case -2:
            //4 Low memory
            LogFail(indent2, TEXT("CMD10Post: Not enough memory to create a structure to contain the SD bus's cached info from the card's CID register."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Not enough memory to verify the result of the bus request returned matches the cached CID register.."));
            break;
        case -3:
            //4 Hitting this option should also not be possible
            LogFail(indent2, TEXT("CMD10Post: The function I used to make the comparison says it does not support comparisons of this ."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Test code failure. Invalid parameter indicated."));
            break;
        default:
            //4 The SDCardInfoQuery call failed
            LogFail(indent2, TEXT("CMD10Post: The comparison was unable to be made due to an API failure."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Can't get the CID register info from the SD bus so I can not compare against the response from the bus request. SDCardInfoQuery %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);

    }
    //2 Set Card back to transfer state
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD10Post: Retuning card to the TRANSFER state..."));
    if (!(SendCardToTransferState(indent2, hDevice, *pRCA, pTestParams, &rStat)))
    {
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent2, TEXT("CMD10Post: An error occurred while trying to return the card to the TRANSFER state. %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("CMD10Post: An error occurred while trying to return the card to the TRANSFER state. %s (0x%x) returned.\nThis will likely lead to subsequent test failures."), TranslateErrorCodes(rStat), rStat);
        }
        else
        {
            LogFail(indent2, TEXT("CMD10Post: The card has wound up in a state that the test can not set back to the TRANSFER state."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("CMD10Post: The card has wound up in a state that the test can not set back to the TRANSFER state.\nThis will likely lead to subsequent test failures."));
        }
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD10Post: Exiting Post-CMD10 Bus Request Code..."));
}

BOOL CMD12PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    PUCHAR                pDataIn = NULL;
    PUCHAR                pDataOut = NULL;
    DWORD                dwStartAddressIndex = SDTST_REQ_RW_START_ADDR;
    DWORD                expState;
    DWORD                state;
    TCHAR                stateTxt[20];
    TCHAR                taskTxt[6];
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD12Pre: Entering Pre-CMD12 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD12Pre: Doing the bus requests necessary so that something is transmitted to the cad so I can stop it..."));
    //2 Allocate memory for response
    AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), FALSE);
    //2 Set block lengths for later calls
    rStat = SetBlockLength(indent2, hDevice, pTestParams, pbrp->pResp);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD12Pre: Bailing..."));
        goto DONE;
    }
    //2 Multi-Block Write or Read to memory to get to the right state
    if (pTestParams->TestFlags & SD_FLAG_WRITE)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD12Pre: Allocating memory for buffer to write from ..."));
        if (!(AllocBuffer(indent3, 3*SDTST_STANDARD_BLKSIZE, BUFF_RANDOM, &pDataIn, pTestParams->dwSeed) ) )
        {
            LogFail(indent4, TEXT("CMD12Pre: Insufficient memory to Create buffers to write to contain data to write to the card. "));
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to write random data into. \nThis was probably due to a lack of available memory"));
            goto DONE;
        }
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD12Pre: Writing Data to SD Memory Card..."));
        rStat = WriteBlocks(indent3, 3, SDTST_STANDARD_BLKSIZE, dwStartAddressIndex, pDataIn, hDevice, pTestParams, FALSE);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent4, TEXT("CMD12Pre: Bailing due to failure in writing buffer to card. Bailing..."));
            goto DONE;
        }
        expState = SD_STATUS_CURRENT_STATE_RCV;
        ZeroMemory(taskTxt, sizeof(TCHAR) * 6);
        StringCchCopy(taskTxt, _countof(taskTxt), TEXT("write"));
    }
    else
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD12Pre: Allocating memory for buffer to read to ..."));
        if (!(AllocBuffer(indent3, 3*SDTST_STANDARD_BLKSIZE, BUFF_ZERO, &pDataOut) ) )
        {
            LogFail(indent4, TEXT("CMD12Pre: Insufficient memory to Create buffers to write to contain data to read from the card. "));
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to read data into from card. \nThis was probably due to a lack of available memory"));
            goto DONE;
        }
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD12Pre: Reading Data from SD Memory Card..."));
        rStat = ReadBlocks(indent3, 3, SDTST_STANDARD_BLKSIZE, dwStartAddressIndex, pDataOut, hDevice, pTestParams, FALSE);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent4, TEXT("CMD12Pre: Bailing due to failure in reading buffer from card. Bailing..."));
            goto DONE;
        }
        expState = SD_STATUS_CURRENT_STATE_DATA;
        ZeroMemory(taskTxt, sizeof(TCHAR) * 6);
        StringCchCopy(taskTxt, _countof(taskTxt), TEXT("read"));
    }
    ZeroMemory(stateTxt, 20 * sizeof(TCHAR));
    StringCchCopy(stateTxt, _countof(stateTxt), TranslateState(expState));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD12Pre: Getting Card State. ..."));
    state = GetCurrentState(indent3, hDevice, pTestParams);
    if (state > SD_STATUS_CURRENT_STATE_DIS)
    {
        LogFail(indent4, TEXT("CMD12Pre: Card has gotten into a \"bad\" state."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Card has gone into I state that I can not interpret. The test must be in the %s state to continue."), stateTxt);
        goto DONE;
    }
    else if (state != expState)
    {
        LogFail(indent4, TEXT("CMD12Pre: After a multi-block %s Bus Request with no CMD12 I expect to be in a %s (%u) state, Instead I am in the %s (%u) State."), taskTxt, stateTxt, expState, TranslateState(state), state);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The card must be in the %s (%u) state  (after a multi-block %s) in order for the CMD12 to put it back into the TRANSFER state. Instead, it is in the %s (%u) State. "), stateTxt, expState, taskTxt, TranslateState(state), state);
        goto DONE;
    }
    else
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("CMD12Pre: As expected the card is in the %s state !!!"), TranslateState(state));
    }

    //2 Set Parameters for stop transmission operation
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD12Pre: Setting parameters necessary to stop transmission..."));
    pbrp->arg=0;
    pbrp->sdtc = SD_COMMAND;
    pbrp->rt = ResponseR1b;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD12Pre: Exiting Pre-CMD12 Bus Request Code..."));
    bContinue = TRUE;
DONE:

    return bContinue;
}

void CMD12PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    DWORD                state;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD12Post: Entering Post-CMD12 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD12Post: In order to verify that the CMD12 worked we need to get the current state of the card again..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT(" "));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD12Post: Getting Card State. ..."));
    state = GetCurrentState(indent3, hDevice, pTestParams);
    if (state > SD_STATUS_CURRENT_STATE_DIS)
    {
        LogFail(indent4, TEXT("CMD12Post: Card has gotten into a \"bad\" state."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Card has gone into I state that I can not interpret. The test must be in the TRANSFER state to succeed."));
    }
    else if (state != SD_STATUS_CURRENT_STATE_TRAN)
    {
        LogFail(indent4, TEXT("After a stop transmission bus request, I expect to be in the TRANSFER state. Instead I am in the %s (%u) State."), TranslateState(state), state);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The card must be back in  the TRANSFER state after a CMD12. Instead, it is in the %s (%u) State. "), TranslateState(state), state);
    }
    else
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("CMD12Post: As expected the card is in the TRANSFER state !!!"));
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD12Post: Exiting Post-CMD12 Bus Request Code..."));
}

BOOL CMD13PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp)
{
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    SD_CARD_RCA        RCA;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("(A)CMD13Pre: Entering Pre-CMD13 Bus Request Code..."));
    //2 Getting Relative address
    rStat = GetRelativeAddress(indent3, hDevice, pTestParams, &RCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("(A)CMD13Pre: Bailing..."));
        goto DONE;
    }
    //2 Allocating response buffer
    if (!(AllocateResponseBuff(indent3, pTestParams, &(pbrp->pResp), TRUE)))
    {
        if (!(pTestParams->TestFlags & SD_FLAG_ALT_CMD))
        {
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Status  register data is gleaned from the response."));
            LogDebug(indent4, SDIO_ZONE_TST, TEXT("(A)CMD13Pre: Bailing..."));
            goto DONE;
        }
    }
    if(pTestParams->TestFlags & SD_FLAG_ALT_CMD)
    {
        //2 Setting AppCmd
        rStat = AppCMD(indent3, hDevice, pTestParams, pbrp->pResp, RCA);
        if (!SD_API_SUCCESS(rStat))
        {
            LogDebug(indent4, SDIO_ZONE_TST, TEXT("(A)CMD13Pre: Bailing..."));
            goto DONE;
        }

        //2 Setting Parameters
        pbrp->rt = ResponseR1;
        pbrp->sdtc = SD_READ;
        pbrp->ulNBlks = 1;
        pbrp->ulBlkSize = 64;
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("(A)CMD13Pre: Allocating memory for buffer to read into..."), TranslateCommandCodes(pTestParams->BusRequestCode), pTestParams->BusRequestCode);
        if (!(AllocBuffer(indent3, 64, BUFF_ZERO, &(pbrp->pBuffer)) ) )
        {
                LogFail(indent4, TEXT("(A)CMD13Pre: Insufficient memory to Create buffers to contain the Alternate Status info from the card. "));
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to allocate buffer memory, and thus get alternate status data from card, due to probable lack of available memory."));
            goto DONE;
        }
    }
    else
    {
        //2 Setting Other Parameters
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("(A)CMD13Pre: Setting Other Parameters for bus request..."));
        pbrp->arg = RCA<<16;
        pbrp->rt = ResponseR1;
    }

    bContinue = TRUE;
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("(A)CMD13Pre: Exiting Pre-CMD13 Bus Request Code..."));
    return bContinue;
}

void CMD13PostReq(UINT indent, PTEST_PARAMS  pTestParams, PSD_COMMAND_RESPONSE pResp, UCHAR const *pBuffer)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("(A)CMD13Post: Entering Post-CMD13 Bus Request Code..."));
    if (pTestParams->TestFlags & SD_FLAG_ALT_CMD)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("(A)CMD13Post: Not much is done to verify if the Status Info is correctly returned. A check is made to determine if an acceptable bus width is reported."));
        LogDebug(indent1, SDIO_ZONE_TST, TEXT(" "));
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("(A)CMD13Post: Allocating memory for parsed alternate status info buffer..."));

        PSD_PARSED_REGISTER_ALT_STATUS pAltStat = NULL;
        pAltStat = (PSD_PARSED_REGISTER_ALT_STATUS) malloc(sizeof(SD_PARSED_REGISTER_ALT_STATUS));
        if (pAltStat)
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("(A)CMD13Post: Parsing Alternate status buffer..."));
            if (ParseAltStatus(pBuffer, pAltStat))
            {
                LogDebug(indent3, SDIO_ZONE_TST, TEXT("(A)CMD13Post: Examining reported bus width..."));
                if (!(pAltStat->ucBusWidth > 0))
                {
                    LogFail(indent4, TEXT("An invalid bus width is reported in the Alternate SD status info. Bus widths must be 1 or 4 bits."));
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to parse the buffer returned form the SD_Status bus request."));
                }
                else
                {
                    LogDebug(indent3, SDIO_ZONE_TST, TEXT("(A)CMD13Post: Acceptable bus width reported. Dumping full Status info..."));
                    DumpAltStatus(indent4, pAltStat);
                }
            }
            else
            {
                LogFail(indent4, TEXT("Unable to get info from the buffer returned by the SD_Status bus request."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to parse the buffer returned form the SD_Status bus request."));

            }
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("(A)CMD13Post: Freeing memory containing status info..."));
            free(pAltStat);
            pAltStat = NULL;
        }
        else
        {
            LogFail(indent3, TEXT("Unable to allocate memory for structure containing alternate status buffer info. This is probably due to a lack of memory."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Can not allocate memory for structure to contain parsed info from the alternate status buffer.\nThus, I can not evaluate if an acceptable bus with is reported.."));

        }
    }
    else
    {
        SD_CARD_STATUS cs;
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("(A)CMD13Post: Not much is done to verify if the Status Info is correctly returned. If a non null status is returned the test passes."));
        LogDebug(indent1, SDIO_ZONE_TST, TEXT(" "));
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("(A)CMD13Post: Getting Status from response..."));
        SDGetCardStatusFromResponse(pResp, &cs);
        if (cs)
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("(A)CMD13Post: Status is non-zero. Dumping status info..."));
            DumpStatus(indent4, cs);
        }
        else
        {
            LogFail(indent3, TEXT("Status returned from bus request is invalid (0)."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Bad status info returned by bus request."));
        }
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("(A)CMD13Post: Exiting Post-CMD13 Bus Request Code..."));
}

BOOL CMD16PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD16Pre: Entering Pre-CMD16 Bus Request Code..."));
    //2 Precall
    LogDebug(indent , SDIO_ZONE_TST, TEXT("CMD16Pre: Determining block size..."));
    rStat = DetermineBlockSize(indent2, hDevice, pTestParams, &(pbrp->arg));
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD16Pre: Bailing..."));
        goto DONE;
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD16Pre: Setting rest of parameters..."));
    //2 Setting Custom Parameters
    pbrp->rt = ResponseR1;
    //2 Allocating Response Buffer
    AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), FALSE);
    bContinue = TRUE;
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD16Pre: Exiting Pre-CMD16 Bus Request Code..."));
    return bContinue;
}

void CMD16PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, DWORD arg)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    SD_API_STATUS        rStat = 0;
    PUCHAR               pDataIn = NULL;
    PUCHAR               pDataOut = NULL;
    UINT                 c;
    DWORD                dwStartAddressIndex = SDTST_REQ_RW_START_ADDR;
    UINT                 uiRand;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD16Post: Entering Post-CMD16 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD16Post: In order to verify that setting the buffer length actually worked, I will write from and read to the card using that buffer length."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT(" "));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD16Post: Allocating Memory for buffers..."));
    PREFAST_ASSERT(arg <= SDTST_STANDARD_BLKSIZE);
    if (!(AllocBuffer(indent2, 3*arg, BUFF_ZERO, &pDataIn) ) )
    {
        LogFail(indent3, TEXT("CMD16Post: Insufficient memory to Create buffers to write to contain data to write to the card. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to verify if setting block length worked because the system is out of memory."));
        goto DONE;
    }
    if (!(AllocBuffer(indent2, 3*arg, BUFF_ZERO, &pDataOut) ) )
    {
        LogFail(indent3, TEXT("CMD16Post: Insufficient memory to Create buffers to write to contain data to read from the  card. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to verify if setting block length worked because the system is out of memory."));
        goto DONE;
    }
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD16Post: Filling write buffer with random data..."));
    srand(pTestParams->dwSeed);
    for (c = 0; c < (3*arg); c++)
    {
        rand_s(&uiRand);
        pDataIn[c] = (UCHAR)uiRand;
    }
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD16Post: Writing Data..."));
    rStat = WriteBlocks(indent3, 3, arg, dwStartAddressIndex, pDataIn, hDevice, pTestParams);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent3, TEXT("CMD16Post: Bailing due to failure in writing buffer to card"));
        goto DONE;
    }
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD16Post: Reading Data..."));
    rStat = ReadBlocks(indent3, 3, arg, dwStartAddressIndex, pDataOut, hDevice, pTestParams);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent3, TEXT("CMD16Post: Bailing due to failure in reading buffer from card"));
        goto DONE;
    }
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD16Post: Compare the written and read blocks."));
    if (memcmp(pDataIn, pDataOut, 3*arg) != 0)
    {
        LogFail(indent3, TEXT("CMD16Post: The memory buffer read from the card does not match the memory buffer written to the card"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The memory buffer read from the card does not match the memory buffer written to the card"));
    }

DONE:
    DeAllocBuffer(&pDataOut);
    DeAllocBuffer(&pDataIn);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD16Post: Exiting Post-CMD16 Bus Request Code..."));
}

BOOL CMD17PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp, PUCHAR *ppBuff)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    PUCHAR                pDataIn = NULL;
    DWORD                dwStartAddressIndex = SDTST_REQ_RW_START_ADDR;

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD17Pre: Doing the bus requests necessary so that data is on the card that can be read..."));
    //2 Allocate memory for response, will be used on the next several requests
    AllocateResponseBuff(indent2, pTestParams, &(pbrp->pResp), FALSE);
    //2 Set block lengths for later calls
    rStat = SetBlockLength(indent2, hDevice, pTestParams, pbrp->pResp);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD17Pre: Bailing..."));
        goto DONE;
    }
    //2 Write to memory so we will have something to read to
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD17Pre: Allocating memory for buffer to write from ( I am going to write 3 blocks, although I will only read one)..."), TranslateCommandCodes(pTestParams->BusRequestCode), pTestParams->BusRequestCode);
    if (!(AllocBuffer(indent3, 3*SDTST_STANDARD_BLKSIZE, BUFF_RANDOM, &pDataIn, pTestParams->dwSeed) ) )
    {
        LogFail(indent4, TEXT("CMD17Pre: Insufficient memory to Create buffers to write to contain data to write to the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to write random data into. This buffer was to be used to fill SD memory card with data that would later be read.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    *ppBuff = pDataIn;
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD17Pre: Writing Data to SD Memory Card..."));
    rStat = WriteBlocks(indent2, 3, SDTST_STANDARD_BLKSIZE, dwStartAddressIndex, pDataIn, hDevice, pTestParams);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent3, TEXT("CMD17Pre: Bailing due to failure in writing buffer to card"));
        goto DONE;
    }

    //2 Set Parameters for read operation
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD17Pre: Setting parameters necessary to read the second block written..."));
    pbrp->ulNBlks = 1;
    pbrp->ulBlkSize = SDTST_STANDARD_BLKSIZE;
    if (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType))
        pbrp->arg= 1+ dwStartAddressIndex;
    else
        pbrp->arg= (1+ dwStartAddressIndex)*pbrp->ulBlkSize;

    pbrp->sdtc = SD_READ;
    pbrp->rt = ResponseR1;
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD17Pre: Allocating memory for buffer to read into..."), TranslateCommandCodes(pTestParams->BusRequestCode), pTestParams->BusRequestCode);
    if (!(AllocBuffer(indent3, pbrp->ulBlkSize, BUFF_ZERO, &(pbrp->pBuffer)) ) )
    {
            LogFail(indent4, TEXT("CMD17Pre: Insufficient memory to Create buffers to contain data to read from the  card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to allocate buffer memory, and thus read data from card, due to probable lack of available memory."));
        goto DONE;
    }

    bContinue = TRUE;
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD17Pre: Exiting Pre-CMD17 Bus Request Code..."));
    return bContinue;
}

void CMD17PostReq(UINT indent, PTEST_PARAMS  pTestParams, PBR_Params pbrp, UCHAR const *pDataIn)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD17Post: Entering Post-CMD17 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD17Post: Compare the second block written in with the one we just read out."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT(" "));
    if (memcmp(pDataIn + pbrp->ulBlkSize, pbrp->pBuffer, pbrp->ulBlkSize*pbrp->ulNBlks) != 0)
    {
        LogFail(indent2, TEXT("CMD17Post: The memory buffer read from the card does not match the second block written to the card"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The memory buffer read from the card does not match the memory buffer written to the card"));
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD17Post: Exiting Post-CMD17 Bus Request Code..."));
}

BOOL CMD18PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp, PUCHAR *ppBuff)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    PUCHAR                pDataIn = NULL;
    DWORD                dwStartAddressIndex = SDTST_REQ_RW_START_ADDR;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD18Pre: Entering Pre-CMD18 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD18Pre: Doing the bus requests necessary so that data is on the card that can be read..."));
    //2 Allocating Response Buffer
    AllocateResponseBuff(indent2, pTestParams, &(pbrp->pResp), FALSE);
    //2 Set block lengths for later calls
    rStat = SetBlockLength(indent2, hDevice, pTestParams, pbrp->pResp);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD18Pre: Bailing..."));
        goto DONE;
    }
    //2 Write to memory so we will have something to read to
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD18Pre: Allocating memory for buffer to write from ( I am going to write 5 blocks, although I will only read two)..."), TranslateCommandCodes(pTestParams->BusRequestCode), pTestParams->BusRequestCode);
    if (!(AllocBuffer(indent3, 5*SDTST_STANDARD_BLKSIZE, BUFF_RANDOM, &pDataIn, pTestParams->dwSeed) ) )
    {
        LogFail(indent4, TEXT("CMD18Pre: Insufficient memory to Create buffers to write to contain data to write to the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to write random data into. This buffer was to be used to fill SD memory card with data that would later be read.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    *ppBuff = pDataIn;
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD18Pre: Writing Data to SD Memory Card..."));
    rStat = WriteBlocks(indent2, 5, SDTST_STANDARD_BLKSIZE, dwStartAddressIndex, pDataIn, hDevice, pTestParams);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent3, TEXT("CMD18Pre: Bailing due to failure in writing buffer to card"));
        goto DONE;
    }
    //2 Set Parameters for read operation
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD18Pre: Setting parameters necessary to read the second block written..."));
    pbrp->ulNBlks = 2;
    pbrp->ulBlkSize = SDTST_STANDARD_BLKSIZE;

    if (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType))
        pbrp->arg= 1+ dwStartAddressIndex;
    else
        pbrp->arg= (1+ dwStartAddressIndex)*pbrp->ulBlkSize;

    pbrp->sdtc = SD_READ;
    pbrp->rt = ResponseR1;
    pbrp->dwFlags =SD_AUTO_ISSUE_CMD12;
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD18Pre: Allocating memory for buffer to read into..."), TranslateCommandCodes(pTestParams->BusRequestCode), pTestParams->BusRequestCode);
    if (!(AllocBuffer(indent3, pbrp->ulBlkSize * pbrp->ulNBlks, BUFF_ZERO, &(pbrp->pBuffer)) ) )
    {
            LogFail(indent4, TEXT("CMD18Pre: Insufficient memory to Create buffers to contain data to read from the  card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to allocate buffer memory, and thus read data from card, due to probable lack of available memory."));
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD18Pre: Exiting Pre-CMD18 Bus Request Code..."));
    bContinue = TRUE;
DONE:

    return bContinue;
}

void CMD18PostReq(UINT indent, PTEST_PARAMS  pTestParams, PBR_Params pbrp, UCHAR const *pDataIn)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD18Post: Entering Post-CMD18 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Compare the second and third block written in with the two blocks we just read out."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT(" "));
    if (memcmp((pDataIn + pbrp->ulBlkSize), pbrp->pBuffer, pbrp->ulBlkSize*pbrp->ulNBlks) != 0)
    {
        LogFail(indent2, TEXT("BSRT: The memory buffer read from the card does not match the second and third blocks written to the card"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The memory buffer read from the card does not match the memory buffer written to the card"));
        LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD18Post: Exiting Post-CMD18 Bus Request Code..."));
    }

}

BOOL CMD24n25PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp, PDWORD pdwEraseStartAddress, PDWORD pdwEraseEndAddress, PBOOL pbErasesZero)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD24&25Pre: Entering Pre-CMD24 and CMD25 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD24&25Pre: Doing the bus requests necessary so that data is on the card that can be written to..."));
    //2 Allocate memory for response, will be used on the next several requests
    AllocateResponseBuff(indent2, pTestParams, &(pbrp->pResp), FALSE);
    //2 Set block lengths for later calls
    rStat = SetBlockLength(indent2, hDevice, pTestParams, pbrp->pResp);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD24&25Pre: Bailing..."));
        goto DONE;
    }
    //2 Erasing memory so the region around where we are writing to is uniform
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD24&25Pre: Calling function that will erase a series of blocks on the SD Memory Card..."));
    
    *pdwEraseStartAddress = SDTST_REQ_RW_START_ADDR;
    *pdwEraseEndAddress = SDTST_REQ_RW_START_ADDR + 5;

    *pbErasesZero = -1;
    rStat = SDEraseMemoryBlocks(indent3, hDevice, pTestParams, *pdwEraseStartAddress, *pdwEraseEndAddress, pbErasesZero);
    if ((*pbErasesZero < FALSE) && SD_API_SUCCESS(rStat) )
    {
        LogFail(indent4,TEXT("CMD24&25Pre: A Critical memory allocation has failed due to a probable lack of resources"));
        goto DONE;
    }
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent4,TEXT("CMD24&25Pre: Unable to erase or rewrite blocks on SDMemory Card. %s (0x%x) returned"),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to erase or rewrite blocks on SDMemory Card. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    //2 Set Parameters for write operation
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD24&25Pre: Setting parameters necessary to write to the block at the second block (block # 1) on the card..."));
    if (pTestParams->BusRequestCode == SD_CMD_WRITE_MULTIPLE_BLOCK)
    {
        pbrp->ulNBlks = 3;
        pbrp->dwFlags =SD_AUTO_ISSUE_CMD12;
    }
    else
    {
        pbrp->ulNBlks = 1;
    }
    pbrp->ulBlkSize = SDTST_STANDARD_BLKSIZE;

    // This store the address when start sending CMD 24/25 - so this needs to be in Blocks for 2.0 and Bytes for 1.1
    if (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType))
        pbrp->arg = *pdwEraseStartAddress;
    else
        pbrp->arg = *pdwEraseStartAddress * pbrp->ulBlkSize;

    pbrp->sdtc = SD_WRITE;
    pbrp->rt = ResponseR1;
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD24&25Pre: Allocating memory for buffer to write from..."), TranslateCommandCodes(pTestParams->BusRequestCode), pTestParams->BusRequestCode);
    if (!(AllocBuffer(indent3, pbrp->ulBlkSize*pbrp->ulNBlks, BUFF_RANDOM, &(pbrp->pBuffer), pTestParams->dwSeed) ) )
    {
        LogFail(indent4, TEXT("CMD24&25Pre: Insufficient memory to Create buffer to contain data to write to the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to allocate buffer memory, and thus write data to the card, due to probable lack of available memory."));
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD24&25Pre: Exiting Pre-CMD24 and CMD25 Bus Request Code..."));
    bContinue = TRUE;
DONE:

    return bContinue;
}

void CMD24n25PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp, DWORD dwEraseStartAddress, DWORD dwEraseEndAddress, BOOL bErasesZero)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    SD_API_STATUS         rStat = 0;
    PUCHAR                 pDataCompare= NULL;
    PUCHAR                pDataOut = NULL;
    DWORD                 dwNumEraseBlocks = 0;
    DWORD                  dwStartAddress = 0;
    PUCHAR                 pDataStatOnErase = NULL;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD24&25Post: Entering Post-CMD24 and CMD25 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD24&25Post: Look at the first five blocks on the memory card and compare it against a simulation of what they should look like..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT(" "));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD24&25Post: Allocate memory for simulation buffer, and the buffer to be read from the card..."));
    
    dwStartAddress = SDTST_REQ_RW_START_ADDR;
    dwNumEraseBlocks = dwEraseEndAddress - dwEraseStartAddress;
    
    //3 Allocate memory buffers for read and comparison
    unsigned int uBlkSize = 0;
    safeIntUMul(dwNumEraseBlocks,pbrp->ulBlkSize,&uBlkSize);
    if (!(AllocBuffer(indent3, uBlkSize, BUFF_ZERO, &pDataOut) ) )
    {
        LogFail(indent4, TEXT("CMD24&25Post: Insufficient memory to Create buffers to read data from card. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to verify if write worked because there is not enough memory to create buffer to read from card."));
        goto DONE;
    }
    {
        SD_BUFF_TYPE bt = BUFF_HIGH;
        if (bErasesZero) bt  = BUFF_ZERO;
        safeIntUMul(dwNumEraseBlocks,pbrp->ulBlkSize,&uBlkSize);

        if (!(AllocBuffer(indent3, uBlkSize, bt, &pDataCompare) ) ) //Numblocks * blocksize * size of uchar
        {
        LogFail(indent4, TEXT("CMD24&25Post: Insufficient memory to create simulation buffer to compare against card contents. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to verify if write worked because there is not enough memory to create buffer compare against card contents"));
        goto DONE;
        }
    }
    if (pbrp->ulNBlks == 1)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD24&25Post: data from the one block of data from the write buffer is written to the %u%s block at address 0x%x..."),(pbrp->arg/pbrp->ulBlkSize)+ 1, GetNumberSuffix((pbrp->arg/pbrp->ulBlkSize)+ 1),  pbrp->arg);
    }
    else
    {
        TCHAR s0[3], s1[3];
        StringCchCopy(s0, _countof(s0), GetNumberSuffix((pbrp->arg/pbrp->ulBlkSize)+ 1));
        StringCchCopy(s1, _countof(s1), GetNumberSuffix((pbrp->arg/pbrp->ulBlkSize) + pbrp->ulNBlks));
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD24&25Post: data from the %u blocks of data from the write buffer is written to the %u%s - %u%s blocks from the block starting at the address of 0x%x through the block starting with the address of 0x%x..."), pbrp->ulNBlks, (pbrp->arg/pbrp->ulBlkSize)+ 1, s0, (pbrp->arg/pbrp->ulBlkSize) + pbrp->ulNBlks, s1, pbrp->arg, pbrp->arg + ((pbrp->ulNBlks -1) * pbrp->ulBlkSize) );
    }
    memcpy(pDataCompare, pbrp->pBuffer, pbrp->ulBlkSize * pbrp->ulNBlks);
    //3 Read
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD24&25Post: Reading %u blocks out of the SD card starting at address 0x%x..."),dwNumEraseBlocks, dwEraseStartAddress);
    rStat = ReadBlocks(indent1, dwNumEraseBlocks, pbrp->ulBlkSize, dwStartAddress, pDataOut, hDevice, pTestParams);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent3, TEXT("CMD24&25Post: Bailing due to failure in reading buffer from card"));
        goto DONE;
    }
    //3 Compare
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD24&25Post: Comparing buffer read out of memory and simulation buffer..."));
    if (memcmp(pDataOut, pDataCompare, dwNumEraseBlocks * pbrp->ulBlkSize) != 0)
    {
        // Check if the failure is due to incorrect reporting of DATA_STAT_AFTER_ERASE
        // This below code snippet creates a comparision buffer with complement of the reported DATA_STAT_AFTER_ERASE
        // and compares the Read blocks from the SD Card. If the comparision matches the test is failed with
        // additional info.
        SD_BUFF_TYPE bt = BUFF_ZERO;
        if (bErasesZero) bt  = BUFF_HIGH;
        safeIntUMul(dwNumEraseBlocks,pbrp->ulBlkSize,&uBlkSize);
        if (!(AllocBuffer(indent3, uBlkSize, bt, &pDataStatOnErase) ) ) //Numblocks * blocksize * size of uchar
        {
            LogFail(indent4, TEXT("CMD24&25Post: Insufficient memory to create simulation buffer to compare against card contents. "));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to verify if write worked because there is not enough memory to create buffer compare against card contents"));
            goto DONE;
        }

        memcpy(pDataStatOnErase, pbrp->pBuffer, pbrp->ulBlkSize * pbrp->ulNBlks);

        LogFail(indent3, TEXT("CMD24&25Post: The memory buffer read from the card does not match the simulation buffer"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The memory buffer read from the card does not match the simulation buffer"));

        if (memcmp(pDataOut, pDataStatOnErase, dwNumEraseBlocks * pbrp->ulBlkSize) == 0)
        {
            LogFail(indent3, TEXT("CMD24&25Pre: The memory buffer read from the card after erase does not match the simulation buffer created based on DATA_STAT_AFTER_ERASE"));
            LogFail(indent3, TEXT("CMD24&25Pre: Possible reason could be the card reports DATA_STAT_AFTER_ERASE to be '1' though the actual value is '0' or vice versa. "));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Possible reason could be the card reports DATA_STAT_AFTER_ERASE to be '1' though the actual value is '0' or vice versa. "));
        }
    }

DONE:
    //Free the temp buffer
    DeAllocBuffer(&pDataStatOnErase);
    DeAllocBuffer(&pDataOut);
    DeAllocBuffer(&pDataCompare);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD24&25Post: Exiting Post-CMD24 and CMD25 Bus Request Code..."));
}

BOOL CMD28PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp,  PBOOL pbErasesZero)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    BOOL                bWP = FALSE;
    BOOL                bSuccess = FALSE;
    UCHAR                ucSectorSize = 0;
    UCHAR                ucWPGSize = 0;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD28Pre: Entering Pre-CMD28 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD28Pre: Check if write protection is supported.."));
    bWP = IsWPSupported(indent2, hDevice, pTestParams, &bSuccess, &ucSectorSize, &ucWPGSize);
    if (bWP == FALSE)
    {
        if (bSuccess)
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD28Pre: Write Protection Groups are not supported on this card. The test can not continue."));
            AppendMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("Write protection groups are not supported by this card. This test will be skipped."));
        }
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD28Pre: Bailing..."));
        goto DONE;
    }
    AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), FALSE);
    //2 Check if pbErasesZero is not NULL, so it can be updated and returned
    if (pbErasesZero == NULL)
    {
        LogFail(indent2, TEXT("CMD28Pre: Test Error. pbErasesZero = NULL. This means the value of erased bits cannot be returned be passed to the post create code. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Test error. A necessary input parameter is NULL, so the rest of the test cannot keep track of the Erase bit values. The test cannot continue."));
        goto DONE;
    }
    //2 "Erase" Memory If block eraseing is supported, an erase call will be made, if it is not a write will be attempted first
    *pbErasesZero = -1;
    rStat = SDEraseMemoryBlocks(indent3, hDevice, pTestParams, 0, 0, pbErasesZero);
    if ((*pbErasesZero < FALSE) && SD_API_SUCCESS(rStat) )
    {
        LogFail(indent4,TEXT("CMD28Pre: A Critical memory allocation has failed due to a probable lack of resources"));
        goto DONE;
    }
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent4,TEXT("CMD28Pre: Unable to erase or rewrite blocks on SDMemory Card. %s (0x%x) returned"),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to erase or rewrite blocks on SDMemory Card. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    pbrp->arg = 0;
    pbrp->pReq = NULL;
    pbrp->rt =ResponseR1b;
    bContinue = TRUE;
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD28Pre: Exiting Pre-CMD28 Bus Request Code..."));
    return bContinue;
}

void CMD28PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_COMMAND_RESPONSE pResp, BOOL bErasesZero)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    UINT indent5 = indent + 5;
    PREFAST_ASSERT(indent5 > indent);
    SD_API_STATUS         rStat = 0;
    PUCHAR                 pBuff = NULL;
    PUCHAR                pBuff2 = NULL;
//    UCHAR                tmp[4];
    SD_CARD_STATUS        crdStat = 0;
    BOOL                 bErr = FALSE;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD28Post: Entering Post-CMD28 Bus Request Code..."));
    //2 Try writing block to address 0
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD28Post: Setting Block Length"));
    rStat = SetBlockLength(indent2, hDevice, pTestParams, pResp, SDTST_STANDARD_BLKSIZE);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD28Post: Bailing..."));
        goto DONE;
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD28Post: Allocating memory buffer to write..."));
    if (!(AllocBuffer(indent2, SDTST_STANDARD_BLKSIZE, BUFF_RANDOM, &pBuff, pTestParams->dwSeed)))
    {
        LogFail(indent3, TEXT("CMD28Post: Insufficient memory to create buffers to write to the card. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to write random data into. \nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD28Post: Attempting to write to a block in a protected sector..."));
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_WRITE_BLOCK, 0, SD_WRITE, ResponseR1, pResp, 1, SDTST_STANDARD_BLKSIZE, pBuff,0, FALSE, &crdStat);
//    rStat = WriteBlocks(indent2, 1, SDTST_STANDARD_BLKSIZE, 0, pBuff, hDevice, pTestParams);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD28Post: Even though WP is on at address 0, the Synchronous bus request should have succeeded. Bailing..."));
        goto DONE;
    }
    else
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD28Post: As expected the write suceeded."));
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD28Post: Checking Status bits returned from silent SDCardInfoQuery Call..."));
/*        rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_CARD_STATUS, &crdStat, sizeof(SD_CARD_STATUS));
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent3, TEXT("CMD28Post: Failure getting card Status. SDCardInfoQuery returned %s (0x%x). "), TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure getting card Status. SDCardInfoQuery returned %s (0x%x)."), TranslateErrorCodes(rStat), rStat);
            goto DONE;
        }
        DumpStatus(indent, crdStat, STAT_NORMAL);*/
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD28Post: Checking if WP_VIOLATION is set in the status bits."));
        if (crdStat & SD_STATUS_WP_VIOLATION)
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD28Post: As expected a Write Protection Violating was signaled in the Status Bits."));
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD28Post: verifying that write actually did not happen by reading card..."));
            if (AreBlocksCleared(indent4, &bErr, hDevice, pTestParams, 0, 0, bErasesZero, SDTST_STANDARD_BLKSIZE))
            {
                LogDebug(indent5, SDIO_ZONE_TST, TEXT("CMD28Post: The write protection group was sucessfully protected..."));
            }
            else
            {
                if (bErr)
                {
                    LogDebug(indent5, SDIO_ZONE_TST, TEXT("CMD28Post: Bailing..."));
                }
                else
                {
                    LogFail(indent5, TEXT("CMD28Post: The Cards WP does not seem to have been active, Block 0 has been rewritten."));
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Cards WP does not seem to have been active, Block 0 has been rewritten."));
                }
            }
        }
        else
        {
            LogFail(indent3, TEXT("CMD28Post: The Cards WP does not seem to have been active. Writing to block 0 failed to set the WP_VIOLATION flag in the Status register."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Cards WP does not seem to have been active. Writing to block 0 failed to set the WP_VIOLATION flag in the Status register."));
        }
    }
DONE:
    //2 CLR WP
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD28Post: Clearing write protection for address 0..."));
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_CLR_WRITE_PROT, 0, SD_COMMAND, ResponseR1b, pResp, 0, 0, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2, TEXT("CMD28Post: Error returned when attempting to turn off write protection bit. SDSynchronousBusRequest returned %u (0x%x)"), TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSynchronousBusRequest bus request failed when attempting to disable write protection for Address 0.\nThis failure may result in later tests failing. Error Returned = %s, (0x%x)"));
    }
    DeAllocBuffer(&pBuff);
    DeAllocBuffer(&pBuff2);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD28Post: Exiting Post-CMD28 Bus Request Code..."));
}

BOOL CMD29PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp, PDWORD pdwWPGSizeInBytes, PBOOL pbErasesZero)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    BOOL                bWP = FALSE;
    BOOL                bSuccess = FALSE;
    UCHAR                 ucSectorSize = 0;
    UCHAR                 ucWPGSize = 0;
    DWORD                dwCardSize;
    UINT                c;
    int                     k;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD29Pre: Entering Pre-CMD29 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD29Pre: Check if write protection is supported.."));
    //2 Is WP supported?
    bWP = IsWPSupported(indent2, hDevice, pTestParams, &bSuccess, &ucSectorSize, &ucWPGSize, &dwCardSize);
    if (bWP == FALSE)
    {
        if (bSuccess)
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD29Pre: Write Protection Groups are not supported on this card. The test can not continue."));
            AppendMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("Write protection groups are not supported by this card. This test will be skipped."));
        }
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD29Pre: Bailing..."));
        goto DONE;
    }
    //2 Allocate Response Buffer
    AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), FALSE);
    //2 Set Block Length to SDTST_STANDARD_BLKSIZE
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD29Pre: Setting Block Length"));
    rStat = SetBlockLength(indent2, hDevice, pTestParams, pbrp->pResp, SDTST_STANDARD_BLKSIZE);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD29Pre: Bailing..."));
        goto DONE;
    }

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD29Pre: Calculate if you can create at least 2 WP groups, and if so their size in bytes.."));
    //2 Calculate the number of WP Groups & absolute size of WP Group
    if (pdwWPGSizeInBytes == NULL)
    {
        LogFail(indent2, TEXT("CMD29Pre: Test Error. pdwWPGSizeInBytes = NULL. This means the size of WP groups cannot be returned be passed to the post create code. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Test error. A necessary input parameter is NULL, so the rest of the test cannot keep track of the Write Protection Group Size. The test cannot continue."));
        goto DONE;
    }
    *pdwWPGSizeInBytes = SDTST_STANDARD_BLKSIZE * (ucSectorSize + 1) * (ucWPGSize * 1);
    if ((dwCardSize / (*pdwWPGSizeInBytes)) <= 2)//(2 * (*pdwWPGSizeInBytes) < dwCardSize)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD29Pre: The card is too small to support more than 1 Write Protection group. Size of Card = %u bytes, Size of WP Group = %u bytes."), dwCardSize, *pdwWPGSizeInBytes);
        AppendMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("This test requires there to be at least two write protection groups to proceed. In reality there is only one."));
        goto DONE;
    }
    //2 Check if pbErasesZero is not NULL, so it can be updated and returned
    if (pbErasesZero == NULL)
    {
        LogFail(indent2, TEXT("CMD29Pre: Test Error. pbErasesZero = NULL. This means the value of erased bits cannot be returned be passed to the post create code. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Test error. A necessary input parameter is NULL, so the rest of the test cannot keep track of the Erase bit values. The test cannot continue."));
        goto DONE;
    }
    //2 "Erase" Memory If block eraseing is supported, an erase call will be made, if it is not a write will be attempted first
    *pbErasesZero = -1;
    rStat = SDEraseMemoryBlocks(indent3, hDevice, pTestParams, (*pdwWPGSizeInBytes) - SDTST_STANDARD_BLKSIZE, (*pdwWPGSizeInBytes), pbErasesZero);
    if ((*pbErasesZero < FALSE) && SD_API_SUCCESS(rStat) )
    {
        LogFail(indent4,TEXT("CMD29Pre: A Critical memory allocation has failed due to a probable lack of resources"));
        goto DONE;
    }
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent4,TEXT("CMD29Pre: Unable to erase or rewrite blocks on SDMemory Card. %s (0x%x) returned"),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to erase or rewrite blocks on SDMemory Card. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    //2Turn on WP for First 2 WP Groups
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD29Pre: Turn on WP in first two WP Groups"));
    for (c = 0; c < 2; c++)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD29Pre: Turn on WP in the %u%s group"), c+ 1, GetNumberSuffix(c + 1));
        rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_SET_WRITE_PROT, c * (*pdwWPGSizeInBytes), SD_COMMAND, ResponseR1b, pbrp->pResp, 0, 0, NULL, 0);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent3, TEXT("CMD29Pre: SDSynchronousBusRequest failed when attempting to enable write protection for the %u%s write protection group. %s (0x%x), returned. "), c+ 1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSynchronousBusRequest failed when attempting to enable write protection for the %u%s write protection group. %s (0x%x), returned. This test requires that the first 2 write protection groups have write protection enabled."), c+ 1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
            for (k = ((int)c) - 1;  k >= 0; k --)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("CMD29Pre: Turning off WP in the %d%s group"), k+ 1, GetNumberSuffix(k + 1));
                rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_CLR_WRITE_PROT, ((UINT)k) * (*pdwWPGSizeInBytes), SD_COMMAND, ResponseR1b, pbrp->pResp, 0, 0, NULL, 0);
                if (!SD_API_SUCCESS(rStat))
                {
                    LogFail(indent3, TEXT("CMD29Pre: SDSynchronousBusRequest failed when attempting to disable write protection for the %d%s write protection group. %s (0x%x), returned. "), k+ 1, GetNumberSuffix(k + 1), TranslateErrorCodes(rStat), rStat);
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSynchronousBusRequest failed when attempting to disable write protection for the %d%s write protection group. %s (0x%x), returned. A failure to do so will likely result in errors in later tests."), k+ 1, GetNumberSuffix(k + 1), TranslateErrorCodes(rStat), rStat);
                }

            }
            goto DONE;
        }

    }
//4 TEMP!!!!
    UCHAR buff[4];
    ZeroMemory(buff, 4);
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_SEND_WRITE_PROT, 0, SD_READ, ResponseR1, pbrp->pResp, 1, 4, buff, 0);

//4 TEMP!!!!
    //2 Set Params    for Bus Request
    pbrp->arg = *pdwWPGSizeInBytes; //turn off second WP Group
    pbrp->pReq = NULL;
    pbrp->rt =ResponseR1b;

    bContinue = TRUE;
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD29Pre: Exiting Pre-CMD29 Bus Request Code..."));
    return bContinue;
}

void CMD29PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_COMMAND_RESPONSE pResp, DWORD dwWPGSize, BOOL bErasesZero)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    SD_API_STATUS         rStat = 0;
    PUCHAR                 puBuffer = NULL;
    PUCHAR                 puRead = NULL;
    PUCHAR                 puCompare = NULL;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD29Post: Entering Post-CMD29 Bus Request Code..."));
//4 TEMP!!!!
    UCHAR buff[4];
    ZeroMemory(buff, 4);
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_SEND_WRITE_PROT, 0, SD_READ, ResponseR1, pResp, 1, 4, buff, 0);

//4 TEMP!!!!
    //2Allocate 2-block buffer to write
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD29Post: Allocating 1 block size buffer..."));
    if (! AllocBuffer(indent2, SDTST_STANDARD_BLKSIZE * 2, BUFF_ALPHANUMERIC, &puBuffer))//, pTestParams->dwSeed))
    {
        LogFail(indent2, TEXT("CMD29Post: Insufficient memory to Create buffer to write data to card with. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to allocate memory for a 1024 block buffer that was to be used to write to the card. This is probably due to a lack of memory."));
        goto DONE;
    }
    //2Allocate 2-block buffer to read
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD29Post: Allocating 1 block size buffer..."));
    if (! AllocBuffer(indent2, SDTST_STANDARD_BLKSIZE * 2, BUFF_ZERO, &puRead, pTestParams->dwSeed))
    {
        LogFail(indent2, TEXT("CMD29Post: Insufficient memory to Create buffer to read data from card with. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to allocate memory for a 1024 block buffer that was to be used to read from the card. This is probably due to a lack of memory."));
        goto DONE;
    }
    //2Allocate 2-block comparison buffer
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD29Post: Allocating 1 block size buffer..."));
    if (! AllocBuffer(indent2, SDTST_STANDARD_BLKSIZE * 2, BUFF_ZERO, &puCompare, pTestParams->dwSeed))
    {
        LogFail(indent2, TEXT("CMD29Post: Insufficient memory to Create buffer to simulate the card with. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to allocate memory for a 1024 block buffer that was to be used to simulate the card. This is probably due to a lack of memory."));
        goto DONE;
    }

    //2 Try writing across both WP groups
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD29Post: Writing to last block of WP Group 0 (this should fail)..."));
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_WRITE_MULTIPLE_BLOCK, dwWPGSize - SDTST_STANDARD_BLKSIZE, SD_WRITE, ResponseR1, pResp, 2, SDTST_STANDARD_BLKSIZE, puBuffer, SD_AUTO_ISSUE_CMD12);//, FALSE, &crdStat, TRUE);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2, TEXT("CMD29Post: Even though I am writing to a block with WP on, and one without, SDSyncronousBusRequest should have suceeded. It did not. Error = %s (0x%x). "), TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("First write (writting to WPGroup with WP On) failed. Even though I am writing to a block with WP on, and one without, SDSyncronousBusRequest should have suceeded. It did not. Error = %s (0x%x). "), TranslateErrorCodes(rStat), rStat);
    }
    else
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD29Post: As expected the CMD25 write suceeded."));
//        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD29Post: Issuing CMD12..."));
/*        rStat = IssueCMD12(hDevice, &crdStat);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent3, TEXT("CMD29Post: SDSyncronous buss request failed when issuing a CMD12. Error = %s (0x%x). "), TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSyncronous buss request failed when issuing a CMD12 afer writing to card. Error = %s (0x%x). "), TranslateErrorCodes(rStat), rStat);
        }*/

//        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD29Post: Checking Status bits returned from silent SDCardInfoQuery Call made after CMD25 write..."));
/*        rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_CARD_STATUS, &crdStat, sizeof(SD_CARD_STATUS));
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent3, TEXT("CMD29Post: Failure getting card Status. SDCardInfoQuery returned %s (0x%x). "), TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure getting card Status. SDCardInfoQuery returned %s (0x%x)."), TranslateErrorCodes(rStat), rStat);
            goto DONE;
        }*/
//        DumpStatus(indent, crdStat, STAT_NORMAL);
//        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD29Post: Checking if WP_VIOLATION is set in the status bits."));
//        if (crdStat & SD_STATUS_WP_VIOLATION)
//        {
//            LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD29Post: As expected a Write Protection Violating was signaled in the Status Bits."));
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD29Post: verifying that write actually did not happen to last block of first WP group, but happened to first block of the second WP Grooup by reading card..."));
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD29Post: Reading from card..."));
        rStat = ReadBlocks(indent4, 2, SDTST_STANDARD_BLKSIZE, (dwWPGSize / SDTST_STANDARD_BLKSIZE) - 1, puRead, hDevice, pTestParams);
        if (!SD_API_SUCCESS(rStat))
        {
            LogDebug(indent4, SDIO_ZONE_TST, TEXT("CMD29Post: Bailing..."));
            goto DONE;
        }
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD29Post: Creating Simulation.."));
        memcpy(puCompare, puBuffer, SDTST_STANDARD_BLKSIZE);
        if (bErasesZero == FALSE)
        {
            HighMemory(puCompare + SDTST_STANDARD_BLKSIZE, SDTST_STANDARD_BLKSIZE);
        }
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD29Post: Comparing Read buffer to Simulation.."));
        if (memcmp(puRead, puCompare, 2*SDTST_STANDARD_BLKSIZE) == 0)
        {
            LogDebug(indent4, SDIO_ZONE_TST, TEXT("CMD29Post: The write protection group was sucessfully protected, and the WP grop that had been turned off was sucessfully written to..."));
        }
        else
        {
            if (memcmp(puRead, puCompare, SDTST_STANDARD_BLKSIZE) != 0)
            {
                LogFail(indent4, TEXT("CMD29Post: The Cards WP does not seem to have been active, Block 0 has been rewritten."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Cards WP does not seem to have been active, Block 0 has been rewritten."));
            }
            if (memcmp(puRead+SDTST_STANDARD_BLKSIZE, puCompare+SDTST_STANDARD_BLKSIZE, SDTST_STANDARD_BLKSIZE) != 0)
            {
                LogFail(indent4, TEXT("CMD29Post: The CMD29 bus request to turn off WP on the Second WPGroup appears to have not goen into effect, as the first block in that group has not been written over."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The CMD29 bus request to turn off WP on the Second WPGroup appears to have not goen into effect, as the first block in that group has not been written over."));
            }
        }
/*        }
        else
        {
            LogFail(indent3, TEXT("CMD29Post: The Cards WP does not seem to have been active. Writing to block 0 failed to set the WP_VIOLATION flag in the Status register."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Cards WP does not seem to have been active. Writing to block 0 failed to set the WP_VIOLATION flag in the Status register."));
        }    */
    }
DONE:
    DeAllocBuffer(&puBuffer);
    DeAllocBuffer(&puRead);
    DeAllocBuffer(&puCompare);
    //2 Clean up WP
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_CLR_WRITE_PROT, 0, SD_COMMAND, ResponseR1b, pResp, 0, 0, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2, TEXT("CMD29Post: SDSynchronousBusRequest failed when attempting to turn of write protection in WP Group 0. Error = %s (0x%x). "), TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Attempting to Turn off write protection in Write Protection group 0 failed. SD Synchronous Bus request returned: %s (0x%x"), TranslateErrorCodes(rStat), rStat);
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD29Post: Exiting Post-CMD29 Bus Request Code..."));
}

BOOL CMD30PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp, PDWORD pdwWPGSizeInBytes, PDWORD pdwNumWPGroups)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    BOOL                 bWP = FALSE;
    BOOL                bSuccess = FALSE;
    UCHAR                 ucSectorSize = 0;
    UCHAR                 ucWPGSize = 0;
    DWORD                dwCardSize;
    DWORD                 dwTmp = 0;
    UINT c;
    int k;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD30Pre: Entering Pre-CMD30 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD30Pre: Check if write protection is supported..."));
    //2 Is WP supported?
    bWP = IsWPSupported(indent2, hDevice, pTestParams, &bSuccess, &ucSectorSize, &ucWPGSize, &dwCardSize);
    if (bWP == FALSE)
    {
        if (bSuccess)
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD30Pre: Write Protection Groups are not supported on this card. The test can not continue."));
            AppendMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("Write protection groups are not supported by this card. This test will be skipped."));
        }
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD30Pre: Bailing..."));
        goto DONE;
    }
    //2 Allocate Response Buffer
    AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), FALSE);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD30Pre: Verifying input parameters, and other basic requirements for the test..."));
    if (pdwWPGSizeInBytes == NULL)
    {
        LogFail(indent2, TEXT("CMD30Pre: Invalid Input parameter. pdwWPGSizeInBytes = NULL. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Test error!! Invalid Input parameter. pdwWPGSizeInBytes = NULL."));
        goto DONE;
    }
    *pdwWPGSizeInBytes = SDTST_STANDARD_BLKSIZE * (ucSectorSize + 1) * (ucWPGSize * 1);
    if (2 * (*pdwWPGSizeInBytes) < dwCardSize)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("There are less than 2 Write Protection Groups on this card, bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("There are less than 2 Write Protection Groups in the card being tested. This test requires multiple Write Protection Groups. "));
        goto DONE;
    }
    if (pdwNumWPGroups == NULL)
    {
        LogFail(indent2, TEXT("CMD30Pre: Invalid Input parameter. pdwNumWPGroups = NULL. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Test error!! Invalid Input parameter. pdwNumWPGroups = NULL."));
        goto DONE;
    }
    dwTmp = dwRndUp(dwCardSize, *pdwWPGSizeInBytes);
    *pdwNumWPGroups = min(dwTmp, 32);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD30Pre: Turning on Write Protection for Even Numbered Groups..."));
    for (c = 0; c < (*pdwNumWPGroups); c = c+2)
    {
        rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_SET_WRITE_PROT, c * (*pdwWPGSizeInBytes), SD_COMMAND, ResponseR1b, pbrp->pResp, 0, 0, NULL, 0);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent3, TEXT("CMD30Pre: SDSynchronousBusRequest failed when attempting to enable write protection for the %u%s write protection group. %s (0x%x), returned. "), c+ 1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSynchronousBusRequest failed when attempting to enable write protection for the %u%s write protection group. %s (0x%x), returned."), c+ 1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
            for (k = ((int)c) - 1;  k >= 0; k --)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("Turning off WP in the %d%s group"), k+ 1, GetNumberSuffix(k + 1));
                rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_CLR_WRITE_PROT, ((UINT)k) * (*pdwWPGSizeInBytes), SD_COMMAND, ResponseR1b, pbrp->pResp, 0, 0, NULL, 0);
                if (!SD_API_SUCCESS(rStat))
                {
                    LogFail(indent3, TEXT("CMD30Pre: SDSynchronousBusRequest failed when attempting to disable write protection for the %d%s write protection group. %s (0x%x), returned. "), k+ 1, GetNumberSuffix(k + 1), TranslateErrorCodes(rStat), rStat);
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSynchronousBusRequest failed when attempting to disable write protection for the %d%s write protection group. %s (0x%x), returned. A failure to do so will likely result in errors in later tests."), k+ 1, GetNumberSuffix(k + 1), TranslateErrorCodes(rStat), rStat);
                }

            }
            goto DONE;
        }
    }
    AllocBuffer(indent2, 4, BUFF_ZERO, &(pbrp->pBuffer));
    pbrp->arg = 0;
    pbrp->rt = ResponseR1;
    pbrp->sdtc = SD_READ;
    pbrp->ulNBlks = 1;
    pbrp->ulBlkSize = 4;
    bContinue = TRUE;
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD30Pre: Exiting Pre-CMD30 Bus Request Code..."));
    return bContinue;
}

void CMD30PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, DWORD dwNumWPGroups, DWORD dwWPGSizeInBytes, PSD_COMMAND_RESPONSE pResp, UCHAR const *pBuffer)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    SD_API_STATUS         rStat = 0;
    PUCHAR                 buff = NULL;
    UINT                 c;
    UINT                byteNum;
    UINT                bitNum;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD30Post: Entering Post-CMD30 Bus Request Code..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD30Post: Allocating buffer for comparison with Write Protection data..."));
    AllocBuffer(indent2, 4, BUFF_ZERO, &buff);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD30Post: Filling simulation buffer..."));
    for (c = 0; c < dwNumWPGroups; c = c + 2)
    {
        byteNum = c / 8;
        bitNum = c % 8;
        buff[byteNum] = buff[byteNum] | 1<<bitNum;
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD30Post: Comparing Simulation to output of CMD30 bus Request..."));
    if (memcmp(buff, pBuffer, 4) == 0)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD30Post: Returned buffer matches Simulation."));
    }
    else
    {
        LogFail(indent2, TEXT("CMD32Post: Buffer returned from CMD30 does not match simulation."));
        LogDebug(indent3, SDCARD_ZONE_ERROR, TEXT("CMD30 Buffer = %s."), GenerateBinString(pBuffer, 4));
        LogDebug(indent3, SDCARD_ZONE_ERROR, TEXT("Simulation Buffer = %s."), GenerateBinString(buff, 4));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Buffer returned from CMD30 does not match simulation."));
    }
//2 Cleanup
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD30Post: Clearing WP Groups..."));
    for (c = 0; c < dwNumWPGroups; c = c+2)
    {
        rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_CLR_WRITE_PROT, c * dwWPGSizeInBytes, SD_COMMAND, ResponseR1b, pResp, 0, 0, NULL, 0);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent2, TEXT("CMD32Post: SDSynchronousBusRequest failed when attempting to disable write protection for the %u%s write protection group. %s (0x%x), returned. "), c+ 1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSynchronousBusRequest failed when attempting to disable write protection for the %u%s write protection group. %s (0x%x), returned."), c+ 1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
        }
    }
    DeAllocBuffer(&buff);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD30Post: Exiting Post-CMD30 Bus Request Code..."));
}

BOOL CMD32PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    BOOL                bEraseSupp = FALSE;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD32Pre: Entering Pre-CMD32 Bus Request Code..."));
    //2 Check If Block Erasing is supported
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD32Pre: Checking if Block erasing is supported by the inserted SD card..."));
    bEraseSupp = IsBlockEraseSupported(indent2, hDevice, pTestParams, &rStat);
    if (bEraseSupp <FALSE)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD32Pre: Bailing..."));
        goto DONE;
    }
    else if (bEraseSupp == FALSE)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD32Pre: Block is not supported by the SD Memory Card. Bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_SKIP,TEXT("The Inserted SD Memory card does not support block erases\nThis test is only relevant to SD Memory cards that do."));
        goto DONE;
    }
    //2 Allocate memory for response, will be used on the next several requests
    AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), FALSE);
    //2 Set block lengths for later calls
    rStat = SetBlockLength(indent1, hDevice, pTestParams, pbrp->pResp);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD32Pre: Bailing..."));
        goto DONE;
    }
    //2 Setting Misc. commands
    pbrp->arg = 0;
    pbrp->rt = ResponseR1;

    bContinue = TRUE;
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD32Pre: Exiting Pre-CMD32 Bus Request Code..."));
    return bContinue;
}

void CMD32PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_COMMAND_RESPONSE pResp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    SD_API_STATUS         rStat = 0;
    SD_CARD_STATUS        crdstat;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD32Post: Entering Post-CMD32 Bus Request Code..."));
    //2 Set block lengths again to break the erase sequence
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD32Post: Setting Block length again to break erase sequence..."));
    rStat = SetBlockLength(indent2, hDevice, pTestParams, pResp);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD32Post: Getting Status from the response..."));
    if (pResp->ResponseType == ResponseR1)
    {
        SDGetCardStatusFromResponse(pResp, &crdstat);
    }
    else
    {
        LogFail(indent2, TEXT("CMD32Post: No response returned from attempt to set block length"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Bus request that was supposed to break the erase sequence failed to return a response."));
        goto DONE;
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD32Post: Checking if status has the ERASE_RESET bit set..."));
    if (!(SD_STATUS_ERASE_RESET & crdstat))
    {
        LogFail(indent2, TEXT("CMD32Post: Erase Sequence not reset"));
        DumpStatus(indent2, crdstat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Bus request that was supposed to break the erase sequence failed to do so."));
        goto DONE;
    }
    else
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD32Post: ERASE_RESET bit is set in status."));
        DumpStatus(indent1, crdstat);
    }

DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD32Post: Exiting Post-CMD32 Bus Request Code..."));
}

BOOL CMD33PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    BOOL                bEraseSupp = FALSE;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD33Pre: Entering Pre-CMD33 Bus Request Code..."));
    //2 Check If Block Erasing is supported
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD33Pre: Checking if Block erasing is supported by the inserted SD card..."));
    bEraseSupp = IsBlockEraseSupported(indent2, hDevice, pTestParams, &rStat);
    if (bEraseSupp <FALSE)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD33Pre: Bailing..."));
        goto DONE;
    }
    else if (bEraseSupp == FALSE)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD33Pre: Block is not supported by the SD Memory Card. Bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_SKIP,TEXT("The Inserted SD Memory card does not support block erases\nThis test is only relevant to SD Memory cards that do."));
        goto DONE;
    }
    //2 Allocate memory for response, will be used on the next several requests
    AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), FALSE);
    //2 Set block lengths for later calls
    rStat = SetBlockLength(indent1, hDevice, pTestParams, pbrp->pResp);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD33Pre: Bailing..."));
        goto DONE;
    }

    //2 Setting End Erase Block for the argument
    if (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType))
        pbrp->arg= SDTST_REQ_RW_START_ADDR + 3;
    else
        pbrp->arg= (SDTST_REQ_RW_START_ADDR + 3) * SDTST_STANDARD_BLKSIZE;

    pbrp->rt = ResponseR1;
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD33Pre: Setting the first block to erase..."));
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_ERASE_WR_BLK_START, SDTST_REQ_RW_START_ADDR, SD_COMMAND, ResponseR1, pbrp->pResp, 0, 0, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2,TEXT("CMD33Pre: Unable to set the location of the first block to erase. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to set the first block on the SD Card to erase. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        DumpStatusFromResponse(indent2, pbrp->pResp);
        goto DONE;
    }
    bContinue = TRUE;
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD33Pre: Exiting Pre-CMD33 Bus Request Code..."));
    return bContinue;
}

void CMD33PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_COMMAND_RESPONSE pResp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    SD_API_STATUS         rStat = 0;
    BOOL                bErasesZero = FALSE;
    PUCHAR                pDataOut = NULL;
    UINT                c;
    BOOL                bFail = FALSE;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD33Post: Entering Post-CMD33 Bus Request Code..."));
    //2 Erasing
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD33Post: Erasing..."));
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_ERASE, 0, SD_COMMAND, ResponseR1b, pResp, 0, 0, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2,TEXT("CMD33Post: Unable to perform erase bus request. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to erase blocks within bounds. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        DumpStatusFromResponse(indent2, pResp);
    }
    //2 Determining if bits go high or low        
    Sleep(SDTST_REQ_TIMEOUT);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD33Post: Determining if erased bits go high or low..."));
    bErasesZero = IsEraseLow(indent2, hDevice, pTestParams, &rStat, pResp);
    if (bErasesZero < FALSE)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD33Post: Bailing..."));
        goto DONE;
    }
    //2 Allocate memory for Read Buffer
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD33Post: Allocating memory for buffer to read from card..."));
    {
        SD_BUFF_TYPE bt = BUFF_ZERO;
        if (bErasesZero) bt  = BUFF_HIGH;
        if (!(AllocBuffer(indent2, 1024, bt, &pDataOut) ) ) //Numblocks * blocksize * size of uchar
        {
            LogFail(indent3, TEXT("CMD33Post: Insufficient memory to Create buffers to write to contain data to read from the  card. "));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to verify if setting block length worked because the system is out of memory."));
            goto DONE;
        }
    }
    //2 Reading first 2 blocks
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD33Post: Reading First 2 blocks from memory..."));
    rStat = ReadBlocks(indent2, 2, SDTST_STANDARD_BLKSIZE, SDTST_REQ_RW_START_ADDR, pDataOut, hDevice, pTestParams);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD33Post: Bailing due to failure in reading buffer from card"));
        goto DONE;
    }

    //2 Verifying that all 1024 bytes have bits that are all high or all low as appropriate
    if (bErasesZero)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD33Post: Verifying that all 1024 bytes go low..."));
    }
    else
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD33Post: Verifying that all 1024 bytes go high..."));
    }
    for (c = 0; c <1024; c++)
    {
        if ((bErasesZero) && (pDataOut[c] != 0))
        {
            LogFail(indent2, TEXT("The bits on the %u byte did not all go low"), c);
            bFail = TRUE;
        }
        else if ((bErasesZero == FALSE) && (pDataOut[c] != 0xFF))
        {
            LogFail(indent2, TEXT("The bits on the %u byte did not all go high"), c);
            bFail = TRUE;
        }
    }
    if (bFail)
    {
        if (bErasesZero) AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("One or more of the bits erased did not go low."));
        else AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("One or more of the bits erased did not go high."));
    }
    LogDebug((UINT)0, (BOOL)SDIO_ZONE_TST, TEXT("First 2 Blocks of memory from Card"));
    SDOutputBuffer(pDataOut, 1024);
DONE:
    DeAllocBuffer(&pDataOut);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD33Post: Exiting Post-CMD33 Bus Request Code..."));
}

BOOL CMD38PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp, PBOOL pbErasesZero, PUCHAR *ppDI)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    BOOL                bEraseSupp = FALSE;
    LPCSTR                lpcstr = NULL;
    PUCHAR                pDataIn = NULL;
    PUCHAR                pDataOut = NULL;
    UINT                 c;
    DWORD                dwArg = 0;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD38Pre: Entering Pre-CMD38 Bus Request Code..."));
    //2 Check If Block Erasing is supported
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD38Pre: Checking if Block erasing is supported by the inserted SD card..."));
    bEraseSupp = IsBlockEraseSupported(indent2, hDevice, pTestParams, &rStat);
    if (bEraseSupp <FALSE)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD38Pre: Bailing..."));
        goto DONE;
    }
    else if (bEraseSupp == FALSE)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD38Pre: Block is not supported by the SD Memory Card. Bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_SKIP,TEXT("The Inserted SD Memory card does not support block erases\nThis test is only relevant to SD Memory cards that do."));
        goto DONE;
    }
    //2 Allocating Response Buffer
    AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), FALSE);
    //2 Set block lengths for later calls
    rStat = SetBlockLength(indent1, hDevice, pTestParams, pbrp->pResp);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD38Pre: Bailing..."));
        goto DONE;
    }
    //2 Write to memory so we will have something to read
    LogDebug(indent1, TEXT("Creating Buffer to write to card..."));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD38Pre: Allocating memory for buffer..."), TranslateCommandCodes(pTestParams->BusRequestCode), pTestParams->BusRequestCode);
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD38Pre: Allocating memory for buffer to write from..."), TranslateCommandCodes(pTestParams->BusRequestCode), pTestParams->BusRequestCode);
    lpcstr =  "\xB0Secure Digital Memory Card\xB0";
    if (!(AllocBuffer(indent3, 20*SDTST_STANDARD_BLKSIZE, BUFF_STRING, &pDataIn, 0, lpcstr) ) )
    {
        LogFail(indent4, TEXT("CMD38Pre: Insufficient memory to Create buffers to write to contain data to write to the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to write data into. This buffer was to be used to fill SD memory card with data that would later be read.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    *ppDI = pDataIn;
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD38Pre: Writing Data to SD Memory Card..."));
    rStat = WriteBlocks(indent3, 20, SDTST_STANDARD_BLKSIZE, SDTST_REQ_RW_START_ADDR, pDataIn, hDevice, pTestParams);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent4,  SDIO_ZONE_TST, TEXT("CMD38Pre: Bailing due to failure in writing buffer to card..."));
        goto DONE;
    }
    //2 Allocate memory for Read Buffer
    *pbErasesZero = IsEraseLow(indent1, hDevice, pTestParams, &rStat, pbrp->pResp);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD38Pre: Allocating memory for buffer to read from card..."));
    {
        SD_BUFF_TYPE bt = BUFF_ZERO;
        if (*pbErasesZero) bt  = BUFF_HIGH;
        if (!(AllocBuffer(indent2, SDTST_STANDARD_BLKSIZE*20, bt, &pDataOut) ) )
        {
            LogFail(indent3, TEXT("CMD38Pre: Insufficient memory to Create buffers to write to contain data to read from the  card. "));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to verify if setting block length worked because the system is out of memory."));
            goto DONE;
        }
    }
    //2 Read Blocks
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD38Pre: Verification that Card was written to correctly..."));
    rStat = ReadBlocks(indent2, 20, SDTST_STANDARD_BLKSIZE, SDTST_REQ_RW_START_ADDR, pDataOut, hDevice, pTestParams);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD38Pre: Bailing due to failure in reading buffer from card"));
        goto DONE;
    }
    //2 Initial Comparison of blocks
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD38Pre: Comparing Data from card with source data..."));
    if (memcmp(pDataOut, pDataIn, 20 * SDTST_STANDARD_BLKSIZE) != 0)
    {
        LogFail(indent2, TEXT("CMD38Pre: The memory buffer read from the card does not match the source buffer"));
        for (c = 0; c < 20 * SDTST_STANDARD_BLKSIZE; c++)
        {
            if (pDataOut[c] != pDataIn[c]) LogDebug(indent3, SDIO_ZONE_TST, TEXT("Byte %u mismatch. Expected = 0x%x Observed = 0x%x"),c , pDataIn[c], pDataOut[c] );
        }
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The memory buffer read from the card does not match the source buffer"));
        goto DONE;
    }
    //2 Setting Erase Boundaries
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD38Pre: Setting Erase Boundaries..."));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD38Pre: Setting Erase Start Address..."));

    // Erase from block 14 (and 4 blocks)
    if (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType))
        dwArg = SDTST_REQ_RW_START_ADDR  + 14;
    else
        dwArg= (SDTST_REQ_RW_START_ADDR  + 14) * SDTST_STANDARD_BLKSIZE;

    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_ERASE_WR_BLK_START, dwArg, SD_COMMAND , ResponseR1, pbrp->pResp, 0, 0, NULL, 0, FALSE);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent3,TEXT("CMD38Pre: Unable to set the location of the first block to erase. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to set the first block on the SD Card to erase. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        DumpStatusFromResponse(indent2, pbrp->pResp);
        goto DONE;
    }
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD38Pre: Setting Erase End Address..."));

    // Erase from block 14 (and 4 blocks) - we deleted 4 blocks, this is the 
    // address for the last block to be deleted
    if (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType))
        dwArg = SDTST_REQ_RW_START_ADDR  + 14 + 3;
    else
        dwArg= (SDTST_REQ_RW_START_ADDR  + 14 + 3) * SDTST_STANDARD_BLKSIZE;

    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_ERASE_WR_BLK_END, dwArg, SD_COMMAND , ResponseR1, pbrp->pResp, 0, 0, NULL, 0, FALSE);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2,TEXT("CMD38Pre: Unable to set the location of the last block to erase. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to set the last block on the SD Card to erase. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        DumpStatusFromResponse(indent3, pbrp->pResp);
        goto DONE;
    }

    //2 Set Parameters for erase operation
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD38Pre: Setting parameters necessary to erase the 15th - 18th blocks..."));
    pbrp->sdtc = SD_COMMAND;
    pbrp->rt = ResponseR1b;

    bContinue = TRUE;
DONE:
    DeAllocBuffer(&pDataOut);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD38Pre: Exiting Pre-CMD38 Bus Request Code..."));
    return bContinue;
}

void CMD38PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_COMMAND_RESPONSE pResp, UCHAR const *pDataIn)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    SD_API_STATUS         rStat = 0;
    BOOL                bErasesZero = FALSE;
    PUCHAR                pDataOut = NULL;
    PUCHAR                pDataCompare = NULL;
    UINT                 c;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD38Post: Entering Post-CMD38 Bus Request Code..."));
    //2 Determine if Erase is high or low
    bErasesZero = IsEraseLow(indent1, hDevice, pTestParams, &rStat, pResp);
    if (bErasesZero < FALSE)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD38Post: Bailing..."));
        goto DONE;
    }
    //2 Allocate memory for Read Buffer
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD38Post: Allocating memory for buffer to read from card..."));
    {
        SD_BUFF_TYPE bt = BUFF_ZERO;
        if (bErasesZero) bt  = BUFF_HIGH;
        if (!(AllocBuffer(indent2, SDTST_STANDARD_BLKSIZE*20, bt, &pDataOut) ) ) //Numblocks * blocksize * size of uchar
        {
            LogFail(indent3, TEXT("CMD38Post: Insufficient memory to Create buffers to write to contain data to read from the  card. "));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to verify if setting block length worked because the system is out of memory."));
            goto DONE;
        }
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD38Post: Building Comparison buffer..."));
    //2 Allocate Memory for Comparison Buffer
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD38Post: Allocating memory for buffer to compare to card data read..."));
    if (!(AllocBuffer(indent3, SDTST_STANDARD_BLKSIZE*20, BUFF_ZERO, &pDataCompare) ) ) //Numblocks * blocksize * size of uchar
    {
        LogFail(indent4, TEXT("CMD38Post: Insufficient memory to create simulation buffer to compare against card contents. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to verify if write worked because there is not enough memory to create buffer compare against card contents"));
        goto DONE;
    }
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD38Post: Filling comparison buffer with data written to card..."));
    //2 Fill comparison buffer
    memcpy(pDataCompare, pDataIn, SDTST_STANDARD_BLKSIZE * 20);
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CMD38Post: Simulating erase in buffer..."));
    if (bErasesZero)
    {
        ZeroMemory(pDataCompare + 14*SDTST_STANDARD_BLKSIZE, 4*SDTST_STANDARD_BLKSIZE);
    }
    else
    {
        HighMemory(pDataCompare + 14*SDTST_STANDARD_BLKSIZE, 4*SDTST_STANDARD_BLKSIZE);
    }
    //2 Read data from memory
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD38Post: Reading Data from card..."));
    rStat = ReadBlocks(indent2, 20, SDTST_STANDARD_BLKSIZE, SDTST_REQ_RW_START_ADDR, pDataOut, hDevice, pTestParams);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("CMD38Post: Bailing due to failure in reading buffer from card"));
        goto DONE;
    }
    //2 Compare blocks
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CMD38Post: Comparing Data from card with simulation..."));
    if (memcmp(pDataOut, pDataCompare, 20 * SDTST_STANDARD_BLKSIZE) != 0)
    {
        LogFail(indent2, TEXT("CMD38Post: The memory buffer read from the card does not match the simulation buffer"));
        for (c = 0; c < 20 * SDTST_STANDARD_BLKSIZE; c++)
        {
            if (pDataOut[c] != pDataCompare[c]) LogDebug(indent3, SDIO_ZONE_TST, TEXT("Byte %u mismatch. Expected = 0x%x Observed = 0x%x"),c , pDataCompare[c], pDataOut[c] );
        }
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The memory buffer read from the card does not match the simulation buffer"));
    }
DONE:
    DeAllocBuffer(&pDataOut);
    DeAllocBuffer(&pDataCompare);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CMD38Post: Exiting Post-CMD38 Bus Request Code..."));
}

BOOL ACMD51PreReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PBR_Params pbrp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    BOOL                 bContinue = FALSE;
    SD_API_STATUS         rStat = 0;
    SD_CARD_RCA        RCA;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("ACMD51Pre: Entering Pre-ACMD51 Bus Request Code..."));
    //2 Getting Relative address
    rStat = GetRelativeAddress(indent1, hDevice, pTestParams, &RCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("ACMD51Pre: Bailing..."));
        goto DONE;
    }
    //2 Allocating response buffer
    AllocateResponseBuff(indent1, pTestParams, &(pbrp->pResp), FALSE);

    //2 Setting AppCmd
    rStat = AppCMD(indent1, hDevice, pTestParams, pbrp->pResp, RCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("ACMD51Pre: Bailing..."));
        goto DONE;
    }

    //2 Setting Parameters
    pbrp->rt = ResponseR1;
    pbrp->sdtc = SD_READ;
    pbrp->ulNBlks = 1;
    pbrp->ulBlkSize = 8;
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("ACMD51Pre: Allocating memory for buffer to read into..."), TranslateCommandCodes(pTestParams->BusRequestCode), pTestParams->BusRequestCode);
    if (!(AllocBuffer(indent2, 64, BUFF_ZERO, &(pbrp->pBuffer)) ) )
    {
        LogFail(indent3, TEXT("ACMD51Pre: Insufficient memory to Create buffers to contain the SCR Register info from the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to allocate buffer memory, and thus get SCR data from card, due to probable lack of available memory."));
        goto DONE;
    }

    bContinue = TRUE;
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("ACMD51Pre: Exiting Pre-ACMD51 Bus Request Code..."));
    return bContinue;
}

void CMD51PostReq(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, UCHAR const *pBuffer, PSD_COMMAND_RESPONSE pResp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    SD_API_STATUS         rStat = 0;
    BOOL                bErasesZero = FALSE;
    PUCHAR                pDataOut = NULL;

// erase one block of data
    ULONG ulStartEraseAddress = SDTST_REQ_RW_START_ADDR;
    ULONG ulEndEraseAddress = SDTST_REQ_RW_START_ADDR + 1;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("ACMD51Post: Entering Post-ACMD51 Bus Request Code..."));
    if (!(pBuffer[1] & 1<<7)) bErasesZero = TRUE;
    //2 Set block lengths for later calls
    rStat = SetBlockLength(indent1, hDevice, pTestParams, pResp);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
        goto DONE;
    }

    //2 Performing Erase
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Erasing one block of data from card..."));

    if (!IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType))
    {
        ulStartEraseAddress *= SDTST_STANDARD_BLKSIZE;
        ulEndEraseAddress *= SDTST_STANDARD_BLKSIZE;
    }

    rStat = SDPerformErase(indent2, hDevice, pTestParams, ulStartEraseAddress, ulEndEraseAddress, pResp);
    
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
        goto DONE;
    }

    //2 Allocate memory for Read Buffer
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Allocating memory for buffer to read from card..."));
    
    SD_BUFF_TYPE bt = BUFF_ZERO;
    if (bErasesZero) bt  = BUFF_HIGH;
    if (!(AllocBuffer(indent2, SDTST_STANDARD_BLKSIZE, bt, &pDataOut) ) ) //Numblocks * blocksize * size of uchar
    {
        LogFail(indent3, TEXT("BSRT: Insufficient memory to Create buffers to write to contain data to read from the  card. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to verify if setting block length worked because the system is out of memory."));
        goto DONE;
    }
    
    //2 ReadBlock
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Reading first block of memory from card..."));
    rStat= ReadBlocks(indent2, 1, SDTST_STANDARD_BLKSIZE, SDTST_REQ_RW_START_ADDR, pDataOut, hDevice, pTestParams);
    if (bErasesZero)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Checking if bits in first byte go low as expected ..."));
    }
    else
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Checking if bits in first byte go high as expected ..."));
    }
    if ((bErasesZero) && (pDataOut[0] == 0xff) )
    {
        LogFail(indent1, TEXT("BSRT: The bits on the first byte of memory from the card did not all go low as expected after an erase. first byte = 0x%x"), pDataOut[0]);
    }
    else if ((!(bErasesZero)) && (pDataOut[0] == 0) )
    {
        LogFail(indent1, TEXT("BSRT: The bits on the first byte of memory from the card did not all go high as expected after an erase. first byte = 0x%x"), pDataOut[0]);
    }
    else if ((pDataOut[0] != 0) && (pDataOut[0] != 0xff) )
    {
        LogFail(indent1, TEXT("BSRT: The bits on the first byte of memory from the card did not all go uniformly high or low as expected after an erase. first byte = 0x%x"), pDataOut[0]);
    }
DONE:
    DeAllocBuffer(&pDataOut);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("ACMD51Post: Exiting Post-ACMD51 Bus Request Code..."));
}

void SDBusSingleRequestTest(UINT indent, PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO pClientInfo)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    SD_CARD_RCA                RCA;
    HANDLE                         hEvnt = NULL;
    SD_API_STATUS                 rStat = 0;
    DWORD                         dwWait = 10000;
    DWORD                         dwWaitRes;
    PUCHAR                        pDataIn = NULL;
    DWORD                        dwEraseStartAddress = 0;
    DWORD                        dwEraseEndAddress = 0;
    BOOL                        bErasesZero = FALSE;
    BOOL                         bCancel = FALSE;
    BOOL                        bOKState = FALSE;
    BOOL                        bPreErr = FALSE;
    BR_Params                    brp;
    DWORD                         dwWPGrpSize = 0;
    DWORD                        dwNumWPGroups = 0;
//    UCHAR                        ucSectorSize;
//    UCHAR                        ucWPGSize;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("BSRT: Entering Driver Test Function..."));
    InitBRParams(&brp);
//1 Good State Check
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent2, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent2, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state initially. Since it is not, it is likely that a previous test was unable to return the card to the Transfer State."), TRUE, TRUE);
    if (bOKState == FALSE)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
        goto DONE;
    }

    if (!(pTestParams->TestFlags & SD_FLAG_SYNCHRONOUS))
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Creating Event necessary for signaling end of an Asynchronous Bus Request..."));
        hEvnt = CreateEvent(NULL, FALSE,  FALSE, BR_EVENT_NAME);
        if (!(hEvnt))
        {
            LogFail(indent2,TEXT("BSRT: CreateEvent failed. an event is required if you are going to send a bus request asynchronously!!!"));
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Could not set event necessary for asynchronous bus requests, bailing..."));
            goto DONE;
        }
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Preparing to set bus request on command code: %s (%u)..."), TranslateCommandCodes(pTestParams->BusRequestCode, (BOOL)(pTestParams->TestFlags & SD_FLAG_ALT_CMD)), pTestParams->BusRequestCode);
//1 Pre-Bus Request

    switch(pTestParams->BusRequestCode)
    {
        case SD_CMD_GO_IDLE_STATE:
            __fallthrough;
        case SD_CMD_MMC_SEND_OPCOND:
            goto SKIP;
        case SD_CMD_ALL_SEND_CID:
            goto SKIP;
        case SD_CMD_SEND_RELATIVE_ADDR:
            __fallthrough;
        case SD_CMD_SET_DSR:
            __fallthrough;
        case SD_CMD_IO_OP_COND:
            goto SKIP;
        case SD_CMD_SELECT_DESELECT_CARD:
            if ( !(CMD7PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_SEND_CSD:
            if ( !(CMD9PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp, &RCA)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_SEND_CID:
            if ( !(CMD10PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp, &RCA)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_STOP_TRANSMISSION:
            if ( !(CMD12PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_SEND_STATUS:
            if ( !(CMD13PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_GO_INACTIVE_STATE:
            goto SKIP;
        case SD_CMD_SET_BLOCKLEN:
            if ( !(CMD16PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_READ_SINGLE_BLOCK:
            if ( !(CMD17PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp, &pDataIn)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_READ_MULTIPLE_BLOCK:
            if ( !(CMD18PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp, &pDataIn)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_WRITE_BLOCK:
            __fallthrough;
        case SD_CMD_WRITE_MULTIPLE_BLOCK:
            if ( !(CMD24n25PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp, &dwEraseStartAddress, &dwEraseEndAddress, &bErasesZero)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_PROGRAM_CSD:
            goto SKIP;
        case SD_CMD_SET_WRITE_PROT:
            if ( !(CMD28PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp, &bErasesZero)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_CLR_WRITE_PROT:
            if ( !(CMD29PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp, &dwWPGrpSize, &bErasesZero)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_SEND_WRITE_PROT:
            if ( !(CMD30PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp, &dwWPGrpSize, &dwNumWPGroups)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            goto SKIP;
        case SD_CMD_ERASE_WR_BLK_START:
            if ( !(CMD32PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_ERASE_WR_BLK_END:
            if ( !(CMD33PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_ERASE:
            if ( !(CMD38PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp, &bErasesZero, &pDataIn)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        case SD_CMD_LOCK_UNLOCK:
            if(pTestParams->TestFlags & SD_FLAG_ALT_CMD)
            {
            }
            else
            {
            }
            __fallthrough;
        case SD_CMD_IO_RW_DIRECT:
            __fallthrough;
        case SD_CMD_IO_RW_EXTENDED:
            __fallthrough;
        case SD_CMD_APP_CMD:
            __fallthrough;
        case SD_CMD_GEN_CMD:
            __fallthrough;
        case SD_ACMD_SET_BUS_WIDTH:
            __fallthrough;
        case SD_ACMD_SEND_NUM_WR_BLOCKS:
            __fallthrough;
        case SD_ACMD_SET_WR_BLOCK_ERASE_COUNT:
            __fallthrough;
        case SD_ACMD_SD_SEND_OP_COND:
            goto SKIP;
        case SD_ACMD_SEND_SCR:
            if ( !(ACMD51PreReq(indent2, pTestParams, pClientInfo->hDevice, &brp)))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Bailing..."));
                goto DONE;
            }
            break;
        default:
            goto SKIP;
    }
//1 Bus Request
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Performing bus request on command submitted as test parameter Command = %s (%u)"), TranslateCommandCodes(pTestParams->BusRequestCode, (BOOL)(pTestParams->TestFlags & SD_FLAG_ALT_CMD)), pTestParams->BusRequestCode);
    if (pTestParams->TestFlags & SD_FLAG_SYNCHRONOUS)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Performing Synchronous Bus Request..."));
        rStat = wrap_SDSynchronousBusRequest(pClientInfo->hDevice, pTestParams->BusRequestCode, brp.arg, brp.sdtc, brp.rt, brp.pResp, brp.ulNBlks, brp.ulBlkSize, brp.pBuffer, brp.dwFlags, (pTestParams->TestFlags & SD_FLAG_ALT_CMD));
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent3,TEXT("BSRT: Synchronous Bus Request Failed. %s (0x%x) returned"),TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Synchronous bus call failed on Command Code %s, (%u). Error %s (0x%x) returned."),TranslateCommandCodes(pTestParams->BusRequestCode, (BOOL)(pTestParams->TestFlags & SD_FLAG_ALT_CMD)), pTestParams->BusRequestCode, TranslateErrorCodes(rStat), rStat);
            if (brp.pResp)
            {
                LogFail(indent3,TEXT("BSRT: Dumping status Info from the response..."));
                DumpStatusFromResponse(indent3, brp.pResp);
            }
            goto DONE;
        }
    }
    else
    {
        TEST_SYNC_INFO tsi;
        tsi.pTestParams = pTestParams;
        tsi.pResponse = brp.pResp;
        tsi.indent = indent + 3;
        tsi.Status = SD_API_STATUS_SUCCESS;
        tsi.eventID=0;
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Performing Asynchronous Bus Request..."));
        rStat = wrap_SDBusRequest(pClientInfo->hDevice, pTestParams->BusRequestCode,  brp.arg, brp.sdtc, brp.rt, brp.ulNBlks, brp.ulBlkSize, brp.pBuffer, GenericRequestCallback, (DWORD) &tsi, &(brp.pReq), brp.dwFlags, (pTestParams->TestFlags & SD_FLAG_ALT_CMD));
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent3,TEXT("BSRT: Asynchronous Bus Request Failed. %s (0x%x) returned"),TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Asynchronous bus call failed on Command Code %s, (%u). Error %s (0x%x) returned."),TranslateCommandCodes(pTestParams->BusRequestCode, (BOOL)(pTestParams->TestFlags & SD_FLAG_ALT_CMD)), pTestParams->BusRequestCode, TranslateErrorCodes(rStat), rStat);
            if (brp.pResp)
            {
                LogFail(indent3,TEXT("BSRT: Dumping status Info from the response..."));
                DumpStatusFromResponse(indent3, brp.pResp);
            }
            if (pTestParams->BusRequestCode != SD_CMD_APP_CMD) DebugPrintState(3, SDCARD_ZONE_INFO, TEXT("BSRT: "), pClientInfo->hDevice);
            goto DONE;
        }
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Waiting for callback to signal completion of request..."));
        dwWaitRes = WaitForSingleObject(hEvnt, dwWait);
        if (dwWaitRes == WAIT_TIMEOUT)
        {
            bCancel= wrap_SDCancelBusRequest(brp.pReq);
            if (!bCancel)
            {
                LogFail(indent3,TEXT("BSRT: Asynchronous Bus Request Timed Out after %.1f seconds, and SDCancelBusRequest Failed. "), (double)(dwWait/1000));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The event set to signal the completion of the Asynchronous Bus Request Timed out, and the attempt to cancel the request failed. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);

            }
            else
            {
                LogFail(indent3,TEXT("BSRT: Asynchronous Bus Request Timed Out after %.1f seconds."),(double)(dwWait/1000));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The event set to signal the completion of the Asynchronous Bus Request Timed out"));
            }
            if (pTestParams->BusRequestCode != SD_CMD_APP_CMD) DebugPrintState(3, SDCARD_ZONE_INFO, TEXT("BSRT: "), pClientInfo->hDevice);
            goto DONE;
        }
        else if (dwWaitRes == WAIT_OBJECT_0)
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("BSRT: The callback set in the Asynchronous Bus Request signaled its completion successfully."));
            rStat = tsi.Status;
            brp.pResp = tsi.pResponse;
            if (!SD_API_SUCCESS(rStat))
            {    if (rStat == SD_API_STATUS_ACCESS_VIOLATION)
                {
                    LogFail(indent4,TEXT("BSRT: error copying response from within callback. Callback returns: %s (0x%x)" ),TranslateErrorCodes(rStat), rStat);
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Asynchronous bus callback failed in buffer copy during Command Code %s, (%u). Error %s (0x%x) returned."),TranslateCommandCodes(pTestParams->BusRequestCode, (BOOL)(pTestParams->TestFlags & SD_FLAG_ALT_CMD)), pTestParams->BusRequestCode, TranslateErrorCodes(rStat), rStat);
                }
                else
                {
                    LogFail(indent4,TEXT("BSRT: Bus Request returned after completion callback %s (0x%x)" ),TranslateErrorCodes(rStat), rStat);
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Asynchronous bus call failed after completion on Command Code %s, (%u). Error %s (0x%x) returned."),TranslateCommandCodes(pTestParams->BusRequestCode, (BOOL)(pTestParams->TestFlags & SD_FLAG_ALT_CMD)), pTestParams->BusRequestCode, TranslateErrorCodes(rStat), rStat);
                }
                goto DONE;
            }

            if (pTestParams->BusRequestCode != SD_CMD_APP_CMD) DebugPrintState(4, SDCARD_ZONE_INFO, TEXT("BSRT: "), pClientInfo->hDevice);
        }
        else
        {
            LogFail(indent3,TEXT("BSRT: Waiting for the callback to signal failed. WAIT_FAILED returned by WaitForSingleObject call"));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The call to WaitForSingleObject failed. this is most likely a test bug."));

            if (pTestParams->BusRequestCode != SD_CMD_APP_CMD) DebugPrintState(3, SDCARD_ZONE_INFO, TEXT("BSRT: "), pClientInfo->hDevice);

            goto DONE;
        }
    }

    // Add wait for general bus request
    Sleep(SDTST_REQ_TIMEOUT);


//1 VERIFY Request
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Verifying that the command %s (%u) worked..."),TranslateCommandCodes(pTestParams->BusRequestCode, (BOOL)(pTestParams->TestFlags & SD_FLAG_ALT_CMD)), pTestParams->BusRequestCode);
    switch(pTestParams->BusRequestCode)
    {
        case SD_CMD_GO_IDLE_STATE:
            __fallthrough;
        case SD_CMD_MMC_SEND_OPCOND:
            break;
        case SD_CMD_ALL_SEND_CID:
            break;
        case SD_CMD_SEND_RELATIVE_ADDR:
            __fallthrough;
        case SD_CMD_SET_DSR:
            __fallthrough;
        case SD_CMD_IO_OP_COND:
            __fallthrough;
        case SD_CMD_SELECT_DESELECT_CARD:
            CMD7PostReq(indent2, pTestParams, pClientInfo->hDevice);
            break;
        case SD_CMD_SEND_CSD:
            CMD9PostReq(indent2, pTestParams, pClientInfo->hDevice, &RCA, brp.pResp);
            break;
        case SD_CMD_SEND_CID:
            CMD10PostReq(indent2, pTestParams, pClientInfo->hDevice, &RCA, brp.pResp);
            break;
        case SD_CMD_STOP_TRANSMISSION:
            CMD12PostReq(indent2, pTestParams, pClientInfo->hDevice);
            break;
        case SD_CMD_SEND_STATUS:
            CMD13PostReq(indent2, pTestParams, brp.pResp, brp.pBuffer);
            break;
        case SD_CMD_GO_INACTIVE_STATE:
            break;
        case SD_CMD_SET_BLOCKLEN:
            CMD16PostReq(indent2, pTestParams, pClientInfo->hDevice, brp.arg);
            break;
        case SD_CMD_READ_SINGLE_BLOCK:
            CMD17PostReq(indent2, pTestParams, &brp, pDataIn);
            break;
        case SD_CMD_READ_MULTIPLE_BLOCK:
            CMD18PostReq(indent2, pTestParams, &brp, pDataIn);
            break;
        case SD_CMD_WRITE_BLOCK:
            __fallthrough;
        case SD_CMD_WRITE_MULTIPLE_BLOCK:
            CMD24n25PostReq(indent2, pTestParams, pClientInfo->hDevice, &brp, dwEraseStartAddress, dwEraseEndAddress, bErasesZero);
            break;
        case SD_CMD_PROGRAM_CSD:
            break;
        case SD_CMD_SET_WRITE_PROT:
            CMD28PostReq(indent2, pTestParams, pClientInfo->hDevice, brp.pResp, bErasesZero);
            break;
        case SD_CMD_CLR_WRITE_PROT:
            CMD29PostReq(indent2, pTestParams, pClientInfo->hDevice, brp.pResp, dwWPGrpSize, bErasesZero);
            break;
        case SD_CMD_SEND_WRITE_PROT:
            CMD30PostReq(indent2, pTestParams, pClientInfo->hDevice, dwNumWPGroups, dwWPGrpSize, brp.pResp, brp.pBuffer);
            break;
        case SD_CMD_ERASE_WR_BLK_START:
            CMD32PostReq(indent2, pTestParams, pClientInfo->hDevice, brp.pResp);
            break;
        case SD_CMD_ERASE_WR_BLK_END:
            CMD33PostReq(indent2, pTestParams, pClientInfo->hDevice, brp.pResp);
            break;
        case SD_CMD_ERASE:
            CMD38PostReq(indent2, pTestParams, pClientInfo->hDevice, brp.pResp, pDataIn);
            break;
        case SD_CMD_LOCK_UNLOCK:
            __fallthrough;
        case SD_CMD_IO_RW_DIRECT:
            __fallthrough;
        case SD_CMD_IO_RW_EXTENDED:
            __fallthrough;
        case SD_CMD_APP_CMD:
            __fallthrough;
        case SD_CMD_GEN_CMD:
            __fallthrough;
        case SD_ACMD_SET_BUS_WIDTH:
            __fallthrough;
        case SD_ACMD_SEND_NUM_WR_BLOCKS:
            __fallthrough;
        case SD_ACMD_SET_WR_BLOCK_ERASE_COUNT:
            __fallthrough;
        case SD_ACMD_SD_SEND_OP_COND:
            break;
        case SD_ACMD_SEND_SCR:
            CMD51PostReq(indent2, pTestParams, pClientInfo->hDevice, brp.pBuffer, brp.pResp);
    }
    goto DONE;
//1 Cleanup
SKIP:
    LogWarn(indent1,TEXT("BSRT: This test is not yet implemented!!"));
    SetMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("The command code %s (%u), is not yet supported by the test driver."), TranslateCommandCodes(pTestParams->BusRequestCode, (BOOL)(pTestParams->TestFlags & SD_FLAG_ALT_CMD)), pTestParams->BusRequestCode);
DONE:

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("BSRT: Cleaning up..."));
    //2Good State Check 2
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("BSRT: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent3, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent3, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state upon completion."), FALSE, TRUE);
    //2 Dealloc
    if (hEvnt)
    {
        CloseHandle(hEvnt);
    }
    if (brp.pReq)
    {
        wrap_SDFreeBusRequest(brp.pReq);
    }

    if (brp.pResp)
    {
        free(brp.pResp);
        brp.pResp = NULL;
    }
    DeAllocBuffer(&pDataIn);
    DeAllocBuffer(&(brp.pBuffer));

    if (IsBufferEmpty(pTestParams))
    {
        if ((pTestParams->BusRequestCode == SD_CMD_SET_BLOCKLEN) && (brp.arg == SDTST_STANDARD_BLKSIZE))
        {
            SetMessageBufferAndResult(pTestParams, TPR_PASS, TEXT("The test on Bus Request: %s, (%u) succeeded.\nHowever, the only working bus length was SDTST_STANDARD_BLKSIZE bytes."), TranslateCommandCodes(pTestParams->BusRequestCode, (BOOL)(pTestParams->TestFlags & SD_FLAG_ALT_CMD)), pTestParams->BusRequestCode);
        }
        else
        {
            SetMessageBufferAndResult(pTestParams, TPR_PASS, TEXT("The test on Bus Request: %s, (%u) succeeded."), TranslateCommandCodes(pTestParams->BusRequestCode, (BOOL)(pTestParams->TestFlags & SD_FLAG_ALT_CMD)), pTestParams->BusRequestCode);
        }
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("BSRT: Exiting Driver Test Function..."));
}

void CancelRequestCallback(SD_DEVICE_HANDLE /*hDevice*/, PSD_BUS_REQUEST pRequest, PVOID /*pContext*/, DWORD RequestParam)
{
    HANDLE hEvent = NULL;
    PTEST_SYNC_INFO pSync= NULL;
    UINT indent = 0;
    UINT indent1 = 1;
    UINT indent2 = 2;
    LPTSTR name = NULL;

    if (RequestParam != NULL)
    {
        pSync = (PTEST_SYNC_INFO)RequestParam;
        indent = pSync->indent;
        indent1 = indent + 1;
        PREFAST_ASSERT(indent1 > indent);
        indent2 = indent + 2;
        PREFAST_ASSERT(indent2 > indent);
        LogDebug(indent, SDIO_ZONE_TST, TEXT("CancelTstRequestCallback: Entering Callback..."));
        if (pSync->pResponse)
        {
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("CancelTstRequestCallback: Copying response data into structure so calling function can access it..."));
            pSync->Status = SdBusRequestResponse(pRequest, pSync->pResponse);
        }

        GenerateEventName(indent1, pSync->eventID, pSync->pTestParams, &name);
    }
    else
    {
        LogDebug(indent, SDIO_ZONE_TST, TEXT("CancelTstRequestCallback: Entering Callback..."));
    }

    if ((RequestParam != NULL) && (name) )
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CancelTstRequestCallback:Opening Event named: %s"),name);
        hEvent = OpenEvent (EVENT_ALL_ACCESS, FALSE, name);
        if (hEvent)
        {
            SetEvent(hEvent);
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("CancelTstRequestCallback: BusRequestEvent Found"));
        }
        else
        {
            LogFail(indent2, TEXT("CancelTstRequestCallback: BusRequestEvent Not Found!!!"));
            if ((pSync) && (pSync->pTestParams) && (IsBufferEmpty(pSync->pTestParams)))
            {
                AppendMessageBufferAndResult(pSync->pTestParams, TPR_FAIL, TEXT("GenericRequestCallback:BusRequestEvent Not Found!!!"));
            }
            else if ((pSync) && (pSync->pTestParams) && (IsBufferEmpty(pSync->pTestParams) == FALSE))
            {
                AppendMessageBufferAndResult(pSync->pTestParams, TPR_FAIL, TEXT("GenericRequestCallback:BusRequestEvent Not Found!!!"));
            }
        }
        free(name);
        name = NULL;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CancelTstRequestCallback: Exiting Callback..."));
}

void SDCancelBusRequestTest(UINT indent, PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO pClientInfo)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    PSD_BUS_REQUEST            pReq[NUM_QUEUED_EVNTS];
    HANDLE                         hEvnt[NUM_QUEUED_EVNTS];
    SD_API_STATUS                 rStat = 0;
    DWORD                         dwWait = 10000;
    DWORD                         dwWaitRes;
    int                             c;
    TEST_SYNC_INFO                tsi[NUM_QUEUED_EVNTS];
    BOOL                        bCanceled[NUM_QUEUED_EVNTS];
    LPTSTR                        evntName[NUM_QUEUED_EVNTS];
    BOOL                        bOKState = FALSE;
    BOOL                        bPreErr = FALSE;
    PUCHAR                        pBuffer = NULL;
    PSD_COMMAND_RESPONSE        pResp = NULL;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("CBR: Entering Driver Test Function..."));

    for (c = 0; c < NUM_QUEUED_EVNTS; c++)
    {
        pReq[c] = NULL;
        hEvnt[c] = NULL;
        tsi[c].eventID = 0;
        tsi[c].indent = 0;
        tsi[c].pResponse = NULL;
        tsi[c].pTestParams = NULL;
        tsi[c].Status = 0;
        bCanceled[c] = TRUE;
        evntName[c] = NULL;
    }

//1 Good State Check
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CBR: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent2, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent2, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state initially. Since it is not, it is likely that a previous test was unable to return the card to the Transfer State."), TRUE, TRUE);
    if (bOKState == FALSE)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CBR: Bailing..."));
        goto DONE;
    }

//1 Events
    for (c = 0; c < NUM_QUEUED_EVNTS; c++)
    {
        GenerateEventName(indent1, c, pTestParams, &(evntName[c]));
        if (evntName[c] == NULL)
        {
            LogFail(indent2,TEXT("CBR: Bailing..."));
        }
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("CBR: Creating %d%s Event necessary for signaling end of an Asynchronous Bus Request..."), c + 1,GetNumberSuffix(c + 1));
        hEvnt[c] = CreateEvent(NULL, FALSE,  FALSE, evntName[c]);
        if (!(hEvnt[c]))
        {
            LogFail(indent2,TEXT("CBR: %d%s CreateEvent failed. an event is required if you are going to send a bus request asynchronously!!!"), c + 1,GetNumberSuffix(c + 1));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Could not set event necessary for the %d%s asynchronous bus request, bailing..."), c + 1,GetNumberSuffix(c + 1));
            goto DONE;
        }
    }
//1 Allocating Response Buffer
    AllocateResponseBuff(indent1, pTestParams, &pResp, FALSE);
//1 Setting Block Length
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CBR: Setting the Card's Block Length for later..."));
    rStat = SetBlockLength(indent2, pClientInfo->hDevice, pTestParams, pResp, SDTST_STANDARD_BLKSIZE);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent, SDIO_ZONE_TST, TEXT("CBR: Bailing..."));
        goto DONE;
    }
//1 Allocating Buffer to Write
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CBR: Allocating 10 block buffer to write to card..."));
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE * 10, BUFF_RANDOM, &pBuffer, pTestParams->dwSeed))
    {
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure allocating memory for write buffer. This is probably due to a lack of memory."));
    }
//1Send Bus Requests
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CBR: Preparing and making bus requests..."));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("CBR: Setting request params for callbacks for all requests..."));
    for (c = 0; c < NUM_QUEUED_EVNTS; c++)
    {
        tsi[c].eventID= c;
        tsi[c].indent = 4;
        AllocateResponseBuff(indent3, pTestParams, &(tsi[c].pResponse), FALSE);
        tsi[c].pTestParams = pTestParams;
        tsi[c].Status = SD_API_STATUS_SUCCESS;
    }

    for (c = 0; c < NUM_QUEUED_EVNTS; c++)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CBR: Making %d%s asynchronous bus request..."),c+1, GetNumberSuffix(c + 1));
        rStat = wrap_SDBusRequest(pClientInfo->hDevice, SD_CMD_WRITE_MULTIPLE_BLOCK,  0, SD_WRITE, ResponseR1, 10, SDTST_STANDARD_BLKSIZE, pBuffer, CancelRequestCallback, (DWORD) &(tsi[c]), &(pReq[c]), SD_AUTO_ISSUE_CMD12);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent3, TEXT("CBR: The %d%s bus request failed. Error = %s (0x%x)"), c+1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
            LogDebug(indent, SDIO_ZONE_TST, TEXT("CBR: Bailing..."));
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The %d%s SDBusRequest call failed.\nError =%s (0x%x)."),c+1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
            goto WRAPUP;
        }
        else
        {
            bCanceled[c] = FALSE;
        }
    }
//1 Canceling Bus Requests
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CBR: Canceling bus requests..."));
    for (c = NUM_QUEUED_EVNTS - 1; c >= 0; c--)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CBR: Attempting to cancel %d%s request..."), c+1, GetNumberSuffix(c + 1));
        bCanceled[c] = wrap_SDCancelBusRequest(pReq[c]);
        if (bCanceled[c] == FALSE)
        {
            if (c == 0)
            {
                LogDebug(indent3, SDIO_ZONE_TST, TEXT("CBR: The %d%s bus request could not be canceled. This is allowed, but it must complete normally."), c+1, GetNumberSuffix(c + 1));
            }
            else
            {
                LogFail(indent3,TEXT("CBR: %d%s bus request could not be canceled. This is a failure. Only the first request in this test is allowed not to be cancelable. "), c+1, GetNumberSuffix(c + 1));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("CBR: The %d%s bus request could not be canceled.\n Only the first request in this test is allowed to not be cancelable."), c+1, GetNumberSuffix(c + 1));
            }
        }
    }

WRAPUP:
/*
//1 Reset Thread Priority
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CBR: Restoring original thread priority to %d..."),iOriginalPri);
    bPriSet = CeSetThreadPriority(me, iOriginalPri);
    if (bPriSet == FALSE)
    {
        LogFail(indent3,TEXT("CBR: Failure resetting thread priority to %d"), iOriginalPri);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SetThreadPriority Failure. Without resetting the thread priority, other tests are likely to fail as well, bailing..."));
        goto DONE;
    }
*/
//1 Waiting on all requests
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CBR: Waiting on all requests..."));
    for (c = NUM_QUEUED_EVNTS - 1; c >= 0; c--)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("CBR: Waiting for callback to signal completion of request %d%s..."), c+1, GetNumberSuffix(c + 1));
        dwWaitRes = WaitForSingleObject(hEvnt[c], dwWait);
        if (dwWaitRes == WAIT_TIMEOUT)
        {
            bCanceled[c] = wrap_SDCancelBusRequest(pReq[c]);
            if (!SD_API_SUCCESS(rStat))
            {
                LogFail(indent3,TEXT("CBR: Asynchronous Bus Request %d%s Timed Out after %.1f seconds, and 2nd attempt at SDCancelBusRequest Failed. %s (0x%x) returned"), c+1, GetNumberSuffix(c + 1), (double)(dwWait/1000), TranslateErrorCodes(rStat), rStat);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The event set to signal the completion of the Asynchronous Bus Request %d%s Timed out, and the attempt to cancel the request failed. Error %s (0x%x) returned."), c+1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);

            }
            else
            {
                LogFail(indent3,TEXT("CBR: Asynchronous Bus Request %d%s Timed Out after %.1f seconds."), c+1, GetNumberSuffix(c + 1) ,(double)(dwWait/1000));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The event set to signal the completion of the Asynchronous Bus Request %d%s Timed out"), c+1, GetNumberSuffix(c + 1));

            }

            DebugPrintState(3, SDCARD_ZONE_INFO, TEXT("CBR: "), pClientInfo->hDevice);

        }
        else if (dwWaitRes == WAIT_OBJECT_0)
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("CBR: The callback set in the Asynchronous Bus Request %d%s signaled its completion successfully."), c+1, GetNumberSuffix(c + 1));
            rStat = tsi[c].Status;
            if (!SD_API_SUCCESS(rStat))
            {
                if (rStat == SD_API_STATUS_CANCELED)
                {
                    if (bCanceled[c] == TRUE)
                    {
                        LogDebug(indent4, SDIO_ZONE_TST, TEXT("The %d%s bus request was canceled, and the callback successfully returned %s (0x%x)"), c+1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
                    }
                    else
                    {
                        LogFail(indent4,TEXT("CBR: Bus Request %d%s returned after completion callback %s (0x%x). But the attempt to cancel that request supposedly failed earlier." ), c+1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
                        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Asynchronous bus request %d%s call failed after completion on Command Code %u, (%d). Error %s (0x%x) returned. However, the SDCancelBusRequest call had supposedly failed earlier."), c+1, GetNumberSuffix(c + 1), TranslateCommandCodes(SD_CMD_SEND_STATUS, FALSE), pTestParams->BusRequestCode, TranslateErrorCodes(rStat), rStat);
                    }
                }
                else if (rStat == SD_API_STATUS_ACCESS_VIOLATION)
                {
                    LogFail(indent4,TEXT("CBR: error copying response from within callback. Callback returns: %s (0x%x)" ),TranslateErrorCodes(rStat), rStat);
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Asynchronous bus callback failed in buffer copy during Command Code %s, (%u). Error %s (0x%x) returned."),TranslateCommandCodes(SD_CMD_SEND_STATUS, FALSE), pTestParams->BusRequestCode, TranslateErrorCodes(rStat), rStat);
                }
                else
                {
                    LogFail(indent4,TEXT("CBR: Bus Request %d%s returned after completion callback %s (0x%x)" ), c+1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Asynchronous bus request %d%s call failed after completion on Command Code %s, (%u). Error %s (0x%x) returned."), c+1, GetNumberSuffix(c + 1), TranslateCommandCodes(SD_CMD_SEND_STATUS, FALSE), pTestParams->BusRequestCode, TranslateErrorCodes(rStat), rStat);
                }
                 // Added sleep time of 250 to ensure the card properly finish the transition
                 Sleep(250);
            }
            else
            {
                if (bCanceled[c] == TRUE)
                {
                    LogFail(indent4,TEXT("CBR: Bus Request %d%s returned after completion callback %s (0x%x). But the request was supposedly canceled earlier." ), c+1, GetNumberSuffix(c + 1), TranslateErrorCodes(rStat), rStat);
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Asynchronous bus request %d%s call failed after completion on Command Code %s, (%u). Error %s (0x%x) returned. \nHowever, SDCancelBusRequest supposedly succeeded. SD_API_STATUS_CANCELED (0xc0000013) should have been returned"), c+1, GetNumberSuffix(c + 1), TranslateCommandCodes(SD_CMD_SEND_STATUS, FALSE), pTestParams->BusRequestCode, TranslateErrorCodes(rStat), rStat);
                }
            }
            DebugPrintState(4, SDCARD_ZONE_INFO, TEXT("CBR: "), pClientInfo->hDevice);
        }
        else
        {
            LogFail(indent3,TEXT("CBR: Waiting for the callback to signal failed. WAIT_FAILED returned by WaitForSingleObject call"));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The call to WaitForSingleObject failed. this is most likely a test bug."));

            if (pTestParams->BusRequestCode != SD_CMD_APP_CMD) 
                DebugPrintState(3, SDCARD_ZONE_INFO, TEXT("CBR: "), pClientInfo->hDevice);
        }

    }
//1 Cleanup

DONE:
//1Good State Check 2
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CBR: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent2, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent2, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state upon completion."), FALSE, TRUE);

    for (c = 0; c < NUM_QUEUED_EVNTS; c++)
    {
        if (hEvnt[c])
        {
            CloseHandle(hEvnt[c]);
            hEvnt[c] = NULL;
        }
        if (pReq[c])
        {
            wrap_SDFreeBusRequest(pReq[c]);
            pReq[c] = NULL;
        }
        if (evntName[c])
        {
            free(evntName[c]);
            evntName[c] = NULL;
        }

        if (tsi[c].pResponse)
        {
            free(tsi[c].pResponse);
            tsi[c].pResponse = NULL;
        }
    }
    if (pResp)
    {
        free(pResp);
        pResp = NULL;
    }

    if (!pBuffer)
        DeAllocBuffer(&pBuffer);

    if (IsBufferEmpty(pTestParams))
    {
        SetMessageBufferAndResult(pTestParams, TPR_PASS, TEXT("The test on Canceling Bus Requests succeeded."));
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CBR: Exiting Driver Test Function..."));
}

void SDACMDRetryBusRequestTest(UINT indent, PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO pClientInfo)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    HANDLE                         hEvnt = NULL;
    SD_API_STATUS                 rStat = 0;
    SD_CARD_RCA                RCA;
    PSD_COMMAND_RESPONSE        pResp=NULL;
    PSD_BUS_REQUEST            pReq=NULL;
    UCHAR                        ucCode = 23;
    SD_TRANSFER_CLASS            sdtc=SD_COMMAND;
    SD_RESPONSE_TYPE            rt = NoResponse;
    ULONG                        ulNBlks=0;
    ULONG                        ulBlkSize=0;
    PUCHAR                        pBuffer=NULL;
    DWORD                        arg=0;
    DWORD                        dwFlags=0;
    DWORD                         dwWait = 10000;
    DWORD                         dwWaitRes;
    BOOL                         bCancel = FALSE;
//    SD_CARD_STATUS                crdstat;
    BOOL                        bOKState = FALSE;
    BOOL                        bPreErr = FALSE;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("ACMDRTT: Entering Driver Test Function..."));
//1 Good State Check
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("ACMDRTT: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent2, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent2, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state initially. Since it is not, it is likely that a previous test was unable to return the card to the Transfer State."), TRUE, TRUE);
    if (bOKState == FALSE)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("ACMDRTT: Bailing..."));
        goto DONE;
    }

//1 Create Event
    if (!(pTestParams->TestFlags & SD_FLAG_SYNCHRONOUS))
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("ACMDRTT: Creating Event necessary for signaling end of an Asynchronous Bus Request..."));
        hEvnt = CreateEvent(NULL, FALSE,  FALSE, BR_EVENT_NAME);
        if (!(hEvnt))
        {
            LogFail(indent2,TEXT("ACMDRTT: CreateEvent failed. an event is required if you are going to send a bus request asynchronously!!!"));
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Could not set event necessary for asynchronous bus requests, bailing..."));
            goto DONE;
        }
    }
//1 Checking if ACMD23 is supported
    if (ucCode == 23)
    {
        rt = ResponseR1;
        if (IsBlockEraseSupported(indent1, pClientInfo->hDevice, pTestParams, &rStat))
        {
            if (!SD_API_SUCCESS(rStat))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("ACMDRTT: Bailing..."));
            }
            else
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("ACMDRTT: Block erase is supported by this card, so ACMD%u is a legitimate command. Bailing..."),ucCode);
                AppendMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("ACMD%u is not an invalid command so retries will not always be triggered.\nPlease insert a card that does not support block erases, and try again."), ucCode);
            }
            goto DONE;
        }
    }

//1 Prepping for App Command
    //2 Getting Relative address
    rStat = GetRelativeAddress(indent1, pClientInfo->hDevice, pTestParams, &RCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("ACMDRTT: Bailing..."));
        goto DONE;
    }
    //2 Allocating response buffer
    AllocateResponseBuff(indent1, pTestParams, &pResp, FALSE);
    //2 Setting AppCmd
    rStat = AppCMD(indent1, pClientInfo->hDevice, pTestParams, pResp, RCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("ACMDRTT: Bailing..."));
        goto DONE;
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("ACMDRTT: Setting bus request to Non-existent ACMD%u..."),ucCode);
//1 Bus Request
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("ACMDRTT: Performing bus request on ACMD%u"),ucCode);
    if (pTestParams->TestFlags & SD_FLAG_SYNCHRONOUS)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("ACMDRTT: Performing Synchronous Bus Request on ACMD%u..."),ucCode);
        rStat = wrap_SDSynchronousBusRequest(pClientInfo->hDevice, ucCode, arg, sdtc, rt, pResp, ulNBlks, ulBlkSize, pBuffer, dwFlags, TRUE);
        if (!SD_API_SUCCESS(rStat))
        {
            if (rStat != SD_API_STATUS_RESPONSE_TIMEOUT)
            {
                LogFail(indent3,TEXT("ACMDRTT: Synchronous Bus Request Failed. %s (0x%x) returned"),TranslateErrorCodes(rStat), rStat);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Synchronous bus call failed on Command Code %s, (%u). Error %s (0x%x) returned."),TranslateCommandCodes(ucCode, TRUE), ucCode, TranslateErrorCodes(rStat), rStat);
            }
            else
            {
                LogDebug(indent3, SDIO_ZONE_TST, TEXT("ACMDRTT: Synchronous bus request correctly timed out."));
                AppendMessageBufferAndResult(pTestParams, TPR_PASS, TEXT("A bus request with the non-existent app command code ACMD%u errored correctly, Error ="), ucCode, TranslateErrorCodes(rStat), rStat);
            }
            goto DONE;
        }
        else
        {
            LogFail(indent4,TEXT("ACMDRTT: Bogus ACMD%u succeeded unexpectedly" ),ucCode);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("A bus request with the non-existent or non-supported app command code ACMD%u succeeded"),ucCode);
        }
    }
    else
    {
        TEST_SYNC_INFO tsi;
        tsi.pTestParams = pTestParams;
        tsi.pResponse = pResp;
        tsi.indent = indent + 3;
        tsi.Status = SD_API_STATUS_SUCCESS;
        tsi.eventID=0;
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("ACMDRTT: Performing Asynchronous Bus Request..."));
        rStat = wrap_SDBusRequest(pClientInfo->hDevice, ucCode,  arg, sdtc, rt, ulNBlks, ulBlkSize, pBuffer, GenericRequestCallback, (DWORD) &tsi, &pReq, dwFlags, TRUE);
        if (!SD_API_SUCCESS(rStat))
        {
            if (rStat != SD_API_STATUS_RESPONSE_TIMEOUT)
            {
                LogFail(indent3,TEXT("ACMDRTT: Asynchronous Bus Request Failed. %s (0x%x) returned"),TranslateErrorCodes(rStat), rStat);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Asynchronous bus call failed on Command Code %s, (%u). Error %s (0x%x) returned."),TranslateCommandCodes(ucCode, TRUE), ucCode, TranslateErrorCodes(rStat), rStat);

                DebugPrintState(3, SDCARD_ZONE_INFO, TEXT("ACMDRTT: "), pClientInfo->hDevice);

            }
            else
            {
                LogDebug(indent3, SDIO_ZONE_TST, TEXT("ACMDRTT: Synchronous bus request correctly timed out."));
                AppendMessageBufferAndResult(pTestParams, TPR_PASS, TEXT("A bus request with the non-existent app command code ACMD%u errored correctly, Error ="), ucCode, TranslateErrorCodes(rStat), rStat);
            }
            goto DONE;
        }
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("ACMDRTT: Waiting for callback to signal completion of request..."));
        dwWaitRes = WaitForSingleObject(hEvnt, dwWait);
        if (dwWaitRes == WAIT_TIMEOUT)
        {
            bCancel= wrap_SDCancelBusRequest(pReq);
            if (!bCancel)
            {
                LogFail(indent3,TEXT("ACMDRTT: Asynchronous Bus Request Timed Out after %.1f seconds, and SDCancelBusRequest Failed. "), (double)(dwWait/1000));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The event set to signal the completion of the Asynchronous Bus Request Timed out, and the attempt to cancel the request failed. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);

            }
            else
            {
                LogFail(indent3,TEXT("ACMDRTT: Asynchronous Bus Request Timed Out after %.1f seconds."),(double)(dwWait/1000));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The event set to signal the completion of the Asynchronous Bus Request Timed out"));

            }

            DebugPrintState(3, SDCARD_ZONE_INFO, TEXT("BSRT: "), pClientInfo->hDevice);

            goto DONE;
        }
        else if (dwWaitRes == WAIT_OBJECT_0)
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("ACMDRTT: The callback set in the Asynchronous Bus Request signaled its completion successfully."));
            rStat = tsi.Status;
            pResp = tsi.pResponse;
            if (SD_API_SUCCESS(rStat))
            {
                LogFail(indent4,TEXT("ACMDRTT: Bogus ACMD%u succeeded unexpectedly" ),ucCode);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("A bus request with the non-existent app command code ACMD%u succeeded"),ucCode);
            }
            else if (rStat != SD_API_STATUS_RESPONSE_TIMEOUT)
            {
                if (rStat == SD_API_STATUS_ACCESS_VIOLATION)
                {
                    LogFail(indent4,TEXT("ACMDRTT: error copying response from within callback. Callback returns: %s (0x%x)" ),TranslateErrorCodes(rStat), rStat);
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Asynchronous bus callback failed in buffer copy during Command Code %s (%u). Error %s (0x%x) returned."),TranslateCommandCodes(ucCode, TRUE), ucCode, TranslateErrorCodes(rStat), rStat);
                }
                else
                {
                    LogFail(indent4,TEXT("ACMDRTT: Bus Request returned after completion callback with odd error code. Error=%s (0x%x)" ),TranslateErrorCodes(rStat), rStat);
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Asynchronous bus call failed after completion on Command Code  %s (%u) with unexpected error value. Error %s (0x%x) returned."),TranslateCommandCodes(ucCode, TRUE), ucCode, TranslateErrorCodes(rStat), rStat);
                }
                goto DONE;
            }
            else
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("ACMDRTT: Bogus ACMD%u errored correctly. Error = %s (0x%x)" ),ucCode, TranslateErrorCodes(rStat), rStat);
                AppendMessageBufferAndResult(pTestParams, TPR_PASS, TEXT("A bus request with the non-existent app command code ACMD%u errored correctly, Error ="), ucCode, TranslateErrorCodes(rStat), rStat);
            }

            DebugPrintState(4, SDCARD_ZONE_INFO, TEXT("BSRT: "), pClientInfo->hDevice);

        }
        else
        {
            LogFail(indent3,TEXT("ACMDRTT: Waiting for the callback to signal failed. WAIT_FAILED returned by WaitForSingleObject call"));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The call to WaitForSingleObject failed. this is most likely a test bug."));

            if (pTestParams->BusRequestCode != SD_CMD_APP_CMD) DebugPrintState(3, SDCARD_ZONE_INFO, TEXT("ACMDRTT: "), pClientInfo->hDevice);

            goto DONE;
        }

    }
DONE:
//1 Good State Check 2
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("ACMDRTT: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent2, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent2, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state upon completion."), FALSE, TRUE);

    if (hEvnt)
    {
        CloseHandle(hEvnt);
    }
    if (pReq)
    {
        wrap_SDFreeBusRequest(pReq);
    }

    if (pResp)
    {
        free(pResp);
        pResp = NULL;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("ACMDRTT: Exiting Driver Test Function..."));
}

void ReadPartial(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_COMMAND_RESPONSE pResp, BOOL /*bLow*/)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    PUCHAR pFill = NULL;
    PUCHAR pRead = NULL;
    PUCHAR pCompare = NULL;
    SD_API_STATUS rStat;
    UINT c;
//1 Allocate buffers
    LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadPartial: Allocating Buffers..."));
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE, BUFF_ALPHANUMERIC, &pFill))
    {
        LogFail(indent2, TEXT("ReadPartial: Insufficient memory to Create buffer to write to the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to write data into. This buffer was to be used to fill SD memory card with data that would later be read.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE, BUFF_ZERO, &pRead))
    {
        LogFail(indent2, TEXT("ReadPartial: Insufficient memory to Create buffer to read from the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to read data into. This buffer was to be used to read the  SD memory card.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE, BUFF_ZERO, &pCompare))
    {
        LogFail(indent2, TEXT("ReadPartial: Insufficient memory to Create buffer to simulate a read from the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to simulate a read. This buffer was to be used to simulate a read from  the SD memory card.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
//1 Write To card
    LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadPartial: Writing to block 0..."));
    rStat = WriteBlocks(indent1, 1, SDTST_STANDARD_BLKSIZE, 0, pFill, hDevice, pTestParams, TRUE);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("ReadPartial: Bailing..."));
        goto DONE;
    }
//1 Read From Card, Simulate and Compare
    for (c = 1; c <= SDTST_STANDARD_BLKSIZE; c++)
    {
        //2 Set Block Length and Read
        LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadPartial: Setting Block length to %u bytes..."), c);
        rStat = SetBlockLength(indent1, hDevice, pTestParams, pResp, c);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent1, TEXT("ReadPartial: SDSyncronousBusRequest failed. Error = %s (0x%x) . "), TranslateErrorCodes(rStat), rStat);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSyncronousBusRequest failed to set bus width to %u bytes. Error = %s (0x%x) ."), c, TranslateErrorCodes(rStat), rStat);
            goto CLEAR;
        }
        LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadPartial: Reading %u bytes at address 0..."), c);
        rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_READ_SINGLE_BLOCK, 0, SD_READ, ResponseR1, pResp, 1, c, pRead, 0);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent1, TEXT("ReadPartial: SDSyncronousBusRequest failed. Error = %s (0x%x) . "), TranslateErrorCodes(rStat), rStat);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSyncronousBusRequest failed with partial CMD17 read (%u bytes)  at address 0. Error = %s (0x%x) ."), c, TranslateErrorCodes(rStat), rStat);
            goto CLEAR;
        }
        //2 Simulate
        LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadPartial: Simulating Data Read..."));
        memcpy(pCompare, pFill, c);
        //2 Compare
        LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadPartial: Comparing Data Read to Simulation..."));
        if (memcmp(pRead, pCompare, SDTST_STANDARD_BLKSIZE) == 0)
        {
            LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadPartial: Data Read when reading %u bytes at address 0 matches Simulation.") ,c);
        }
        else
        {
            LogFail(indent1, TEXT("ReadPartial: The Data Read when reading %u bytes at address 0 does not match the Simulation."), c);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Data Read when reading %u bytes at address 0 does not match the Simulation."), c);
        }
CLEAR:
        //2 Clear
        LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadPartial: Zeroing Buffers..."));
        ZeroMemory(pRead, SDTST_STANDARD_BLKSIZE);
        ZeroMemory(pCompare,SDTST_STANDARD_BLKSIZE);
    }
DONE:
    DeAllocBuffer(&pRead);
    DeAllocBuffer(&pFill);
    DeAllocBuffer(&pCompare);
}

void WritePartial(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_COMMAND_RESPONSE pResp, BOOL bLow)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    PUCHAR pWrite = NULL;
    PUCHAR pRead = NULL;
    PUCHAR pCompare = NULL;
    SD_API_STATUS rStat;
    BOOL bE;
    UINT c;
//1 Allocate buffers
    LogDebug(indent, SDIO_ZONE_TST, TEXT("WritePartial: Allocating Buffers..."));
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE, BUFF_ALPHANUMERIC, &pWrite))
    {
        LogFail(indent2, TEXT("WritePartial: Insufficient memory to Create buffer to write to the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to write data into. This buffer was to be used to fill SD memory card with data that would later be read.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE, BUFF_ZERO, &pRead))
    {
        LogFail(indent2, TEXT("WritePartial: Insufficient memory to Create buffer to read from the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to read data into. This buffer was to be used to read the  SD memory card.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE, BUFF_ZERO, &pCompare))
    {
        LogFail(indent2, TEXT("WritePartial: Insufficient memory to Create buffer to simulate a read from the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to simulate a read. This buffer was to be used to simulate a read from  the SD memory card.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    for (c = 1; c <=SDTST_STANDARD_BLKSIZE; c++)
    {
        //2 Write bytes
        LogDebug(indent, SDIO_ZONE_TST, TEXT("WritePartial: Setting Block length to %u bytes..."), c);
        rStat = SetBlockLength(indent1, hDevice, pTestParams, pResp, c);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent1, TEXT("WritePartial: SDSyncronousBusRequest failed. Error = %s (0x%x) . "), TranslateErrorCodes(rStat), rStat);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSyncronousBusRequest failed to set bus width to %u bytes. Error = %s (0x%x) ."), c, TranslateErrorCodes(rStat), rStat);
            goto CLEAR;
        }
        LogDebug(indent, SDIO_ZONE_TST, TEXT("WritePartial: Writing %u bytes at address 0..."), c);
        rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_WRITE_BLOCK, 0, SD_WRITE, ResponseR1, pResp, 1, c, pWrite, 0);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent1, TEXT("WritePartial: SDSyncronousBusRequest failed. Error = %s (0x%x) . "), TranslateErrorCodes(rStat), rStat);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSyncronousBusRequest failed with partial CMD24 write (%u bytes)  at address 0. Error = %s (0x%x) ."), c, TranslateErrorCodes(rStat), rStat);
            goto CLEAR;
        }
        //2 Read Block
        if (c < SDTST_STANDARD_BLKSIZE)
        {
            LogDebug(indent, SDIO_ZONE_TST, TEXT("WritePartial: Returning Block length to SDTST_STANDARD_BLKSIZE bytes..."));
            rStat = SetBlockLength(indent1, hDevice, pTestParams, pResp);
            if (!SD_API_SUCCESS(rStat))
            {
                LogFail(indent1, TEXT("WritePartial: SDSyncronousBusRequest failed. Error = %s (0x%x) . "), TranslateErrorCodes(rStat), rStat);
                SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSyncronousBusRequest failed to set bus width to SDTST_STANDARD_BLKSIZE bytes. Error = %s (0x%x) ."), TranslateErrorCodes(rStat), rStat);
                goto CLEAR;
            }
        }
        LogDebug(indent, SDIO_ZONE_TST, TEXT("WritePartial: Reading 1 block at address 0..."));
        rStat = ReadBlocks(indent1, 1, SDTST_STANDARD_BLKSIZE, SDTST_REQ_RW_START_ADDR, pRead, hDevice, pTestParams);
        if (!SD_API_SUCCESS(rStat))
        {
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("WritePartial: Skipping to next Write..."));
            goto CLEAR;
        }
        //2 Simulate Block
        LogDebug(indent, SDIO_ZONE_TST, TEXT("WritePartial: Simulating Data Read..."));
        if (!bLow)
        {
            HighMemory(pCompare, SDTST_STANDARD_BLKSIZE);
        }
        memcpy(pCompare, pWrite, c);
        //2 Compare
        LogDebug(indent, SDIO_ZONE_TST, TEXT("WritePartial: Comparing Data Read to Simulation..."));
        if (memcmp(pRead, pCompare, SDTST_STANDARD_BLKSIZE) == 0)
        {
            LogDebug(indent, SDIO_ZONE_TST, TEXT("WritePartial: Data Read when reading 1 block at address 0 after a %u byte Write matches Simulation.") ,c);
        }
        else
        {
            LogFail(indent1, TEXT("WritePartial: The Data Read when reading 1 block bytes at address 0 after a %u byte write does not match the Simulation."), c);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Data Read when reading 1 block at address 0 after a %u byte write does not match the Simulation."), c);
        }
CLEAR:
        //2"Erase"
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("WritePartial: \"Erasing\" the first block of memory..."));
        rStat = SDEraseMemoryBlocks(indent1, hDevice, pTestParams, 0, 0, &bE);
        if (!SD_API_SUCCESS(rStat))
        {
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("WritePartial: Bailing..."));
            goto DONE;
        }
        //2 Zero Buffers
        LogDebug(indent, SDIO_ZONE_TST, TEXT("WritePartial: Zeroing Buffers..."));
        ZeroMemory(pRead, SDTST_STANDARD_BLKSIZE);
        ZeroMemory(pCompare, SDTST_STANDARD_BLKSIZE);
    }
DONE:
    DeAllocBuffer(&pRead);
    DeAllocBuffer(&pWrite);
    DeAllocBuffer(&pCompare);
}

void ReadMisalign(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_COMMAND_RESPONSE pResp, BOOL /*bLow*/)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    PUCHAR pFill = NULL;
    PUCHAR pRead = NULL;
    PUCHAR pCompare = NULL;
    SD_API_STATUS rStat;
    UINT c;
//1 Allocate buffers
    LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadMisalign: Allocating Buffers..."));
    if (!AllocBuffer(indent1, 2*SDTST_STANDARD_BLKSIZE, BUFF_ALPHANUMERIC, &pFill))
    {
        LogFail(indent2, TEXT("ReadMisalign: Insufficient memory to Create buffer to write to the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to write data into. This buffer was to be used to fill SD memory card with data that would later be read.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE, BUFF_ZERO, &pRead))
    {
        LogFail(indent2, TEXT("ReadMisalign: Insufficient memory to Create buffer to read from the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to read data into. This buffer was to be used to read the  SD memory card.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE, BUFF_ZERO, &pCompare))
    {
        LogFail(indent2, TEXT("ReadMisalign: Insufficient memory to Create buffer to simulate a read from the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to simulate a read. This buffer was to be used to simulate a read from  the SD memory card.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
//1 Write To card
    LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadMisalign: Writing to block 0..."));
    rStat = WriteBlocks(indent1, 2, 4, SDTST_REQ_RW_START_ADDR, pFill, hDevice, pTestParams, TRUE);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("ReadPartial: Bailing..."));
        goto DONE;
    }
//1 Read From Card, Simulate and Compare
    for (c = 0; c <= SDTST_STANDARD_BLKSIZE; c++)
    {
        //2 Read
        LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadMisalign: Reading 1 block from address 0x%x..."), c);
        rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_READ_SINGLE_BLOCK, c, SD_READ, ResponseR1, pResp, 1, SDTST_STANDARD_BLKSIZE, pRead, 0);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent1, TEXT("ReadMisalign: SDSyncronousBusRequest failed. Error = %s (0x%x) . "), TranslateErrorCodes(rStat), rStat);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSyncronousBusRequest failed with partial CMD17 read (%u bytes)  at address 0. Error = %s (0x%x) ."), c, TranslateErrorCodes(rStat), rStat);
            goto CLEAR;
        }
        //2 Simulate
        LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadMisalign: Simulating Data Read..."));
        memcpy(pCompare, pFill+c, SDTST_STANDARD_BLKSIZE);
        //2 Compare
        LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadMisalign: Comparing Data Read to Simulation..."));
        if (memcmp(pRead, pCompare, SDTST_STANDARD_BLKSIZE) == 0)
        {
            LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadMisalign: Data Read when reading 1 block at address 0x%x matches Simulation.") , c);
        }
        else
        {
            LogFail(indent1, TEXT("ReadMisalign: The Data Read when reading 1 block at address 0x%x does not match the Simulation."), c);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Data Read when reading 1 block at address 0x%x does not match the Simulation."), c);
        }
CLEAR:
        //2 Clear
        LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadMisalign: Zeroing Buffers..."));
        ZeroMemory(pRead, SDTST_STANDARD_BLKSIZE);
        ZeroMemory(pCompare,SDTST_STANDARD_BLKSIZE);
    }
DONE:
    DeAllocBuffer(&pRead);
    DeAllocBuffer(&pFill);
    DeAllocBuffer(&pCompare);
}

void WriteMisalign(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_COMMAND_RESPONSE pResp, BOOL bLow)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    PUCHAR pWrite = NULL;
    PUCHAR pRead = NULL;
    PUCHAR pCompare = NULL;
    SD_API_STATUS rStat;
    BOOL bE;
    UINT c;
//1 Allocate buffers
    LogDebug(indent, SDIO_ZONE_TST, TEXT("WritePartial: Allocating Buffers..."));
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE, BUFF_ALPHANUMERIC, &pWrite))
    {
        LogFail(indent2, TEXT("WriteMisalign: Insufficient memory to Create buffer to write to the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to write data into. This buffer was to be used to fill SD memory card with data that would later be read.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE*2, BUFF_ZERO, &pRead))
    {
        LogFail(indent2, TEXT("WriteMisalign: Insufficient memory to Create buffer to read from the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to read data into. This buffer was to be used to read the  SD memory card.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    if (!AllocBuffer(indent1, SDTST_STANDARD_BLKSIZE*2, BUFF_ZERO, &pCompare))
    {
        LogFail(indent2, TEXT("WriteMisalign: Insufficient memory to Create buffer to simulate a read from the card. "));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to create buffer to simulate a read. This buffer was to be used to simulate a read from  the SD memory card.\nThis was probably due to a lack of available memory"));
        goto DONE;
    }
    for (c = 0; c <=SDTST_STANDARD_BLKSIZE; c++)
    {
        //2 Write Block
        LogDebug(indent, SDIO_ZONE_TST, TEXT("WriteMisalign: Writing 1 block at address 0x%x..."), c);
        rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_WRITE_BLOCK, SDTST_REQ_RW_START_ADDR, SD_WRITE, ResponseR1, pResp, 1, SDTST_STANDARD_BLKSIZE, pWrite, 0);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent1, TEXT("WriteMisalign: SDSyncronousBusRequest failed. Error = %s (0x%x) . "), TranslateErrorCodes(rStat), rStat);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSyncronousBusRequest failed with partial CMD24 write 1 block at address 0x%x. Error = %s (0x%x) ."), c, TranslateErrorCodes(rStat), rStat);
            goto CLEAR;
        }
        //2 Read Block
        LogDebug(indent, SDIO_ZONE_TST, TEXT("WriteMisalign: Reading 2 blocks at address 0..."));
        rStat = ReadBlocks(indent1, 2, 4, SDTST_REQ_RW_START_ADDR, pRead, hDevice, pTestParams);
        if (!SD_API_SUCCESS(rStat))
        {
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("WriteMisalign: Skipping to next Write..."));
            goto CLEAR;
        }
        //2 Simulate Block
        LogDebug(indent, SDIO_ZONE_TST, TEXT("WriteMisalign: Simulating Data Read..."));
        if (!bLow)
        {
            HighMemory(pCompare, SDTST_STANDARD_BLKSIZE*2);
        }
        memcpy(pCompare+c, pWrite, SDTST_STANDARD_BLKSIZE);
        //2 Compare
        LogDebug(indent, SDIO_ZONE_TST, TEXT("WriteMisalign: Comparing Data Read to Simulation..."));
        if (memcmp(pRead, pCompare, SDTST_STANDARD_BLKSIZE*2) == 0)
        {
            LogDebug(indent, SDIO_ZONE_TST, TEXT("WriteMisalign: Data Read when reading 2 blocks at address 0 after a 1 block  Write at address 0x%x matches Simulation.") ,c);
        }
        else
        {
            LogFail(indent1, TEXT("WriteMisalign: The Data Read when reading 2 blocks at address 0 after a 1 block  Write at address 0x%x does not match the Simulation."), c);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Data Read when reading 2 blocks at address 0 after a 1 block  Write at address 0x%x does not match the Simulation."), c);
        }
CLEAR:
        //2"Erase"
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("WriteMisalign: \"Erasing\" the first two blocks of memory..."));
        rStat = SDEraseMemoryBlocks(indent1, hDevice, pTestParams, SDTST_REQ_RW_START_ADDR, 2, &bE);
        if (!SD_API_SUCCESS(rStat))
        {
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("WriteMisalign: Bailing..."));
            goto DONE;
        }
        //2 Zero Buffers
        LogDebug(indent, SDIO_ZONE_TST, TEXT("WriteMisalign: Zeroing Buffers..."));
        ZeroMemory(pRead, SDTST_STANDARD_BLKSIZE*2);
        ZeroMemory(pCompare, SDTST_STANDARD_BLKSIZE*2);
    }
DONE:
    DeAllocBuffer(&pRead);
    DeAllocBuffer(&pWrite);
    DeAllocBuffer(&pCompare);
}

void SDReadWritePartialMisalignTest(UINT indent, PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO pClientInfo)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    SD_API_STATUS                 rStat = 0;
    BOOL                        bOKState = FALSE;
    BOOL                        bPreErr = FALSE;
    PSD_COMMAND_RESPONSE        pResp = NULL;
    BOOL                        bLow = FALSE;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Entering Driver Test Function..."));
//1 Good State Check
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent2, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent2, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state initially. Since it is not, it is likely that a previous test was unable to return the card to the Transfer State."), TRUE, TRUE);
    if (bOKState == FALSE)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Bailing..."));
        goto DONE;
    }
//1 Allocating Response Buffer
    AllocateResponseBuff(indent1, pTestParams, &pResp, FALSE);
//1 Set block lengths for later calls
    rStat = SetBlockLength(indent1, pClientInfo->hDevice, pTestParams, pResp);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Bailing..."));
        goto DONE;
    }
//1 "Erase" First two blocks
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("RWPartialMisalign: \"Erasing\" first two blocks of memory..."));
    rStat = SDEraseMemoryBlocks(indent1, pClientInfo->hDevice, pTestParams, 0, SDTST_STANDARD_BLKSIZE, &bLow);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Bailing..."));
        goto DONE;
    }
//1 If statement to call main test code
    if (pTestParams->TestFlags & SD_FLAG_MISALIGN)
    {
        if (pTestParams->TestFlags & SD_FLAG_WRITE)
        {
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Checking if Misaligned Writes are supported..."));
            if (IsRWTypeSupported(indent2, pClientInfo->hDevice, pTestParams, Write_Misalign, &rStat))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Misaligned Writes are supported on this card."));
                WriteMisalign(indent3, pTestParams, pClientInfo->hDevice, pResp, bLow);
            }
            else
            {
                if (!SD_API_SUCCESS(rStat))
                {
                    LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Bailing..."));
                }
                else
                {
                    LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Misaligned Writes are not supported on this card."));
                    AppendMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("Writing Misaligned Blocks is not supported by this memory card."));
                }
            }
        }
        else
        {
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Checking if Misaligned Reads are supported..."));
            if (IsRWTypeSupported(indent2, pClientInfo->hDevice, pTestParams, Read_Misalign, &rStat))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Misaligned Reads are supported on this card."));
                ReadMisalign(indent3, pTestParams, pClientInfo->hDevice, pResp, bLow);
            }
            else
            {
                if (!SD_API_SUCCESS(rStat))
                {
                    LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Bailing..."));
                }
                else
                {
                    LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Misaligned Reads are not supported on this card."));
                    AppendMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("Reading Misaligned Blocks is not supported by this memory card."));
                }
            }
        }
    }
    else
    {
        if (pTestParams->TestFlags & SD_FLAG_WRITE)
        {
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Checking if Partial Block Writes are supported..."));
            if (IsRWTypeSupported(indent2, pClientInfo->hDevice, pTestParams, Write_Partial, &rStat))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Partial Writes are supported on this card."));
                WritePartial(indent3, pTestParams, pClientInfo->hDevice, pResp, bLow);
            }
            else
            {
                if (!SD_API_SUCCESS(rStat))
                {
                    LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Bailing..."));
                }
                else
                {
                    LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Partial Writes are not supported on this card."));
                    AppendMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("Writing Partial Blocks is not supported by this memory card."));
                }
            }
        }
        else
        {
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Checking if Partial Block Reads are supported..."));
            if (IsRWTypeSupported(indent2, pClientInfo->hDevice, pTestParams, Read_Partial, &rStat))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Partial Reads are supported on this card."));
                ReadPartial(indent3, pTestParams, pClientInfo->hDevice, pResp, bLow);
            }
            else
            {
                if (!SD_API_SUCCESS(rStat))
                {
                    LogDebug(indent2, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Bailing..."));
                }
                else
                {
                    LogWarn(indent2, TEXT("RWPartialMisalign: Partial Reads are not supported on this card, however the SDMemory spec says they must be."));
                    AppendMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("Reading Partial Blocks is not supported by this memory card, even though this is a required feature."));
                }
            }
        }
    }
DONE:
//1 Good State Check 2
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent2, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent2, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state upon completion."), FALSE, TRUE);
    if (pResp)
    {
        free(pResp);
        pResp = NULL;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("RWPartialMisalign: Exiting Driver Test Function..."));
}
