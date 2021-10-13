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

#include "br_hlp.h"
#include <intsafe.h>
#include <SDCardDDK.h>
#include <sdcard.h>
#include <sd_tst_cmn.h>
#include <sddrv.h>
#include <SafeInt.hxx>
#include <sddetect.h>

SD_API_STATUS DetermineBlockSize(UINT /*indent*/, SD_DEVICE_HANDLE /*hDevice*/, PTEST_PARAMS /*pTestParams*/, PDWORD pBlockSize)
{/*
    SD_API_STATUS rStat = SD_API_STATUS_SUCCESS;
    PSD_PARSED_REGISTER_CSD     pCSD = NULL;
    PSD_HOST_BLOCK_CAPABILITY     pHBC = NULL;
    DWORD                         dwMax = 0;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("Det_BS: Allocating Memory necessary to struct sizes "));
    //3 First, allocate memory for data from the Host Controllers capabilities register an the card's CSD register
    pCSD = (PSD_PARSED_REGISTER_CSD) malloc(sizeof(SD_PARSED_REGISTER_CSD));
    if (!pCSD)
    {
        LogWarn(indent + 1,TEXT("Det_BS: Insufficient memory to get info cached by the bus driver from the CSD register. Test cannot proceed. Bailing..."));
        SetMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("Out of memory. Can not parse the CSD register to get relevant Block size info."));
        rStat = SD_API_STATUS_NO_MEMORY;
        goto DONE;
    }
    pHBC = (PSD_HOST_BLOCK_CAPABILITY) malloc(sizeof(SD_HOST_BLOCK_CAPABILITY));
    if (!pHBC)
    {
        LogWarn(indent + 1,TEXT("Det_BS: Insufficient memory to get info from the Host Controllers capabilities register. Test cannot proceed. Bailing..."));
        SetMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("Out of memory. Can not get relevant Block size info from the host controller."));
        rStat = SD_API_STATUS_NO_MEMORY;
        goto DONE;
    }
    //4 the Host Controllers pSlotOptionHandler function will expect predefined values. It will step down the Max block Sizes as appropriate
    pHBC->ReadBlocks = 0;
    pHBC->ReadBlockSize = 2048;
    pHBC->WriteBlocks = 0;
    pHBC->WriteBlockSize = 2048;

    //3 Second, get the CSD info from the card and the HC caps from the Capabilities register
    LogDebug(indent, SDIO_ZONE_TST, TEXT("Det_BS: Getting Maximum block Sizes from the Memory Card and the Host Controller..."));
    rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_REGISTER_CSD, pCSD, sizeof(SD_PARSED_REGISTER_CSD));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent + 1,TEXT("Det_BS: SDCardInfoQuery Request Failed. %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to get cached parsed CSD register info. SDCardInfoQuery Request Failed. %s (0x%x) returned.\nData on block sizes is stored in that register and is necessary, for the test to proceed."),TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_HOST_BLOCK_CAPABILITY, pHBC, sizeof(SD_HOST_BLOCK_CAPABILITY));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent + 1,TEXT("Det_BS: SDCardInfoQuery Request Failed. %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to get the HC's capabilities register info. SDCardInfoQuery Request Failed. %s (0x%x) returned.\n Data on block sizes is stored in that register and is necessary, for the test to proceed."),TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }

    //3 Third, settle on a value for the block length that is between the mins and maxes on both the card and the host controller, respecting the cars ability to handle partial block sizes
    LogDebug(indent, SDIO_ZONE_TST, TEXT("Det_BS: Calculating Block Size to use..."));
    if (!(pCSD->WriteBlockPartial))
    {
        LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("Det_BS: The only block size supported is an increment of 512 bytes. I will attempt to find one greater than 512 that both the card and the Host controller support."));
        //4 Block size must be a multiple of 512 on cards
        if ((pHBC->WriteBlockSize < 512) || (pCSD->MaxWriteBlockLength < 512) )
        {
            LogFail(indent + 2, TEXT("Det_BS: The SD specs require that at least block sizes of 512 bytes must be supported because of FAT requirements. The following Max Block Sizes were reported SDCard: %u; SDHC: %u "),pCSD->MaxWriteBlockLength, pHBC->WriteBlockSize);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("A device failed to support a block size necessary for fat, this violates the SD spec. The following Max Block Sizes were reported SDCard: %u; SDHC: %u "),pCSD->MaxWriteBlockLength, pHBC->WriteBlockSize);
            rStat = SD_API_STATUS_UNSUCCESSFUL;
            goto DONE;
        }
        else if (pCSD->MaxWriteBlockLength == 512)
        {
            LogWarn(indent + 2, TEXT("Det_BS: The maximum block length the card supports is 512. This means that it is the only block length supported by this system, since partial blocks are not allowed."));
            LogWarn(indent + 2, TEXT("Det_BS: The test will proceed, but this is not a very strenuous test of setting the block length"));
            *pBlockSize = 512;
        }
        else if (pHBC->WriteBlockSize == 512)
        {
            LogWarn(indent + 2, TEXT("Det_BS: The maximum block length the HC supports is 512. This means that it is likely the only block length supported by this system, since partial blocks are not allowed."));
            LogWarn(indent + 2, TEXT("Det_BS: The test will proceed, but this is not a very strenuous test of setting the block length"));
            *pBlockSize = 512;
//
        }
        else if (pHBC->WriteBlockSize < 1024)
        {
//
            LogWarn(indent + 2, TEXT("Det_BS: The HC does not support any increment of 512 bytes except 512 itself. So I am going to have to default to that value for a block size."));
            LogWarn(indent + 2, TEXT("Det_BS: The test will proceed, but this is not a very strenuous test of setting the block length"));
            *pBlockSize = 512;
        }
        else if ((pCSD->MaxWriteBlockLength) < 1024)
        {
            LogWarn(indent + 2,TEXT("Det_BS: The card does not support any increment of 512 bytes except 512 itself. So I am going to have to default to that value for a block size."));
            LogWarn(indent + 2, TEXT("Det_BS: The test will proceed, but this is not a very strenuous test of setting the block length"));
            *pBlockSize = 512;
        }
        else
        {
            LogDebug(indent + 2, SDIO_ZONE_TST, TEXT("Det_BS: Setting the block size to 1024 bytes. "));
            *pBlockSize = 1024;
        }

    }
    else
    {
        LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("Det_BS: The card you inserted supports block increments in sizes outside increments of 512 blocks. I will find one that both the card and the HC support."));
        dwMax = min(pCSD->MaxWriteBlockLength, pHBC->WriteBlockSize);
        if (dwMax < 512)
        {
            LogFail(indent + 2,TEXT("Det_BS: The test could still continue, but a requirement of SD is that 512 byte block transfers must be supported by all cards and Host controllers. Card Max Block Size = %u, Host Controller Max Block Size = %u, bailing..."),pCSD->MaxWriteBlockLength, pHBC->WriteBlockSize);
            SetMessageBufferAndResult( pTestParams, TPR_FAIL, TEXT("Even if the card supports block sizes < 512 blocks, it still must support block sizes of 512 bytes. The host controller must support block of that size as well. Card Max Block Size = %u, Host Controller Max Block Size = %u."),pCSD->MaxWriteBlockLength, pHBC->WriteBlockSize);
            rStat = SD_API_STATUS_UNSUCCESSFUL;
            goto DONE;
        }
        else
        {
            *pBlockSize = ((dwMax / 2) + 1);
        }
        if (*pBlockSize == 512) //4 I am not going to use the one block size required by FAT
        {
            *pBlockSize = *pBlockSize - 1;
        }

    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("Det_BS: Setting the block size to %u bytes. "), *pBlockSize);
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("Det_BS: Cleaning up..."));
    if (pCSD != NULL)
    {
        free(pCSD);
        pCSD = NULL;
    }
    if (pHBC != NULL)
    {
        free(pHBC);
        pHBC = NULL;
    }
    return rStat;*/
    *pBlockSize = 512;
    return SD_API_STATUS_SUCCESS;
}

SD_API_STATUS WriteBlocks(UINT indent, ULONG numBlocks, ULONG blockSize, DWORD dwDataAddress, PUCHAR pBuff, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, BOOL bIssueCMD12)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    SD_API_STATUS                 rStat = 0;
    PSD_COMMAND_RESPONSE        pResp=NULL;
    DWORD                         dwFlag = 0;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("WriteBlocks: Writing Buffer to Card..."));
    AllocateResponseBuff(indent1, pTestParams, &pResp, FALSE);
     LogDebug(indent, SDIO_ZONE_TST, TEXT("WriteBlocks: Synchronous %s (%u) attempted..."), TranslateCommandCodes(SD_CMD_WRITE_MULTIPLE_BLOCK), SD_CMD_WRITE_MULTIPLE_BLOCK);

    DWORD dwArg = 0;

    // Data Address is in byte unites for SD memory, and in block unity for SDHC
    dwArg = (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType) == TRUE) ? dwDataAddress : dwDataAddress * blockSize;

    UCHAR dwCmd =  (numBlocks > 1)? SD_CMD_WRITE_MULTIPLE_BLOCK : SD_CMD_WRITE_BLOCK;

    if ((numBlocks > 1) && (bIssueCMD12))
        dwFlag = SD_AUTO_ISSUE_CMD12;

    rStat = wrap_SDSynchronousBusRequest(hDevice, dwCmd, dwArg, SD_WRITE, ResponseR1, pResp, numBlocks, blockSize, pBuff, dwFlag);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("WriteBlocks: Synchronous Bus Request Failed. %s (0x%x) returned"),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Synchronous bus call failed on Command Code %s, (%u). Error %s (0x%x) returned."),TranslateCommandCodes(SD_CMD_WRITE_MULTIPLE_BLOCK), SD_CMD_WRITE_MULTIPLE_BLOCK, TranslateErrorCodes(rStat), rStat);
        if (pResp)
        {
            LogFail(indent1,TEXT("WriteBlocks: Dumping status Info from the response..."));
            DumpStatusFromResponse(indent2, pResp);
        }

    }
    if (pResp)
    {
        free(pResp);
        pResp = NULL;
    }
    return rStat;

}

SD_API_STATUS ReadBlocks(UINT indent, ULONG numBlocks, ULONG blockSize, DWORD dwDataAddress, PUCHAR pBuff, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, BOOL bIssueCMD12)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    PSD_COMMAND_RESPONSE        pResp=NULL;
    SD_API_STATUS                 rStat = 0;
    DWORD                         dwFlag = 0;

    UCHAR dwCmd =  (numBlocks > 1)? SD_CMD_READ_MULTIPLE_BLOCK : SD_CMD_READ_SINGLE_BLOCK;

    // CMD12 Stop Transmission Command is for multiple block read/write only
    if ((numBlocks > 1) && bIssueCMD12)
         dwFlag = SD_AUTO_ISSUE_CMD12;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadBlocks: Reading Buffer from Card..."));

    AllocateResponseBuff(indent1, pTestParams, &pResp, FALSE);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("ReadBlocks: Synchronous %s (%u) attempted..."), TranslateCommandCodes(dwCmd), dwCmd);

    DWORD dwArg = 0;

    // Data Address is in byte unites for SD memory, and in block unity for SDHC
    dwArg = (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType) == TRUE) ? dwDataAddress : dwDataAddress * blockSize;

    rStat = wrap_SDSynchronousBusRequest(
                hDevice, dwCmd, dwArg, SD_READ, ResponseR1, pResp, numBlocks, blockSize, pBuff, dwFlag);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("ReadBlocks: Synchronous Bus Request Failed. %s (0x%x) returned"),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Synchronous bus call failed on Command Code %s, (%u). Error %s (0x%x) returned."),TranslateCommandCodes(dwCmd), dwCmd, TranslateErrorCodes(rStat), rStat);
        if (pResp)
        {
            LogFail(indent1,TEXT("ReadBlocks: Dumping status Info from the response..."));
            DumpStatusFromResponse(indent2, pResp);
        }

    }
    if (pResp)
    {
        free(pResp);
        pResp = NULL;
    }
    return rStat;

}

BOOL IsBlockEraseSupported(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, SD_API_STATUS *pRStat)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    BOOL bSupported = -1;
    BOOL bAlloc = FALSE;
    PSD_PARSED_REGISTER_CSD pCSD = NULL;
    if (pRStat == NULL)
    {
        bAlloc = TRUE;
        LogDebug(indent, SDIO_ZONE_TST, TEXT("IsBlkEraseSupp: Allocating memory for API status returns..."));
        pRStat = (SD_API_STATUS *) malloc(sizeof(SD_API_STATUS));
        if (pRStat == NULL)
        {
            LogFail(indent1,TEXT("IsBlkEraseSupp: Insufficient memory to store API status error codes."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Unable to allocate memory for API status error codes. There is probably insufficient memory. Bailing...."));
            goto DONE;
        }

    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsBlkEraseSupp: Allocating memory for CSD Register info..."));
    pCSD = (PSD_PARSED_REGISTER_CSD) malloc(sizeof(SD_PARSED_REGISTER_CSD));
    if (pCSD == NULL)
    {
        LogFail(indent1,TEXT("IsBlkEraseSupp: Insufficient memory to get the contents of the CSD Register."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Unable to get the parsed CSD register info. There is probably insufficient memory. Bailing...."));
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsBlkEraseSupp: Getting the card\'s CSD register..."));
    *pRStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_REGISTER_CSD, pCSD, sizeof(SD_PARSED_REGISTER_CSD));
    if (!SD_API_SUCCESS(*pRStat))
    {
        LogFail(indent1,TEXT("IsBlkEraseSupp: Unable to get the card's CSD register. Error Code: %s (0x%x) returned."),TranslateErrorCodes(*pRStat), *pRStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("SDCardInfoQuery failed so the contents of the CSD register could not be obtained. Error %s (0x%x) returned."), TranslateErrorCodes(*pRStat), *pRStat);
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsBlkEraseSupp: Groveling Write protection info..."));
    if (pCSD->EraseBlockEnable != 0)
    {
        bSupported = TRUE;
    }
    else
    {
        bSupported = FALSE;
    }
DONE:

    if ((bAlloc) && (pRStat))
    {
        free(pRStat);
        pRStat = NULL;
    }
    return bSupported;
}

SD_API_STATUS  FakeErase(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, ULONG addressFirstBlock, ULONG addressOfLastBlock, PSD_COMMAND_RESPONSE pResp, DWORD dwBlockLength, BOOL bLow)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    SD_API_STATUS rStat = SD_API_STATUS_SUCCESS;
    PUCHAR buffer = NULL;;
    ULONG ulAddr = 0;
    DWORD dwNumBlock = 0;

    if (dwBlockLength == 0 || dwBlockLength > 2048)
    {
        return SD_API_STATUS_INVALID_PARAMETER;
    }
    //2 Allocating memory for block to write to card
    LogDebug(indent, SDIO_ZONE_TST, TEXT("FakeErase: Allocating memory for buffer to write to card"));

    #pragma prefast(suppress: 419, "dwBlockLength is being checked above")
    buffer = (PUCHAR) malloc(sizeof(UCHAR) * dwBlockLength);
    if (buffer == NULL)
    {
        LogFail(indent1,TEXT("FakeErase: Insufficient memory to create necessary buffer."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Unable to create buffer. There is probably insufficient memory. Bailing...."));
        rStat = SD_API_STATUS_NO_MEMORY;
        goto DONE;
    }
    //2 Filling Buffer with bits high or low
    LogDebug(indent, SDIO_ZONE_TST, TEXT("FakeErase: Filling card with appropriate bit values"));
    if (bLow) ZeroMemory(buffer, dwBlockLength);
    else HighMemory(buffer, dwBlockLength);
    //2Faking the Erase
    ulAddr = addressFirstBlock;

    // Determine # of blocks
    dwNumBlock = (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType) == TRUE) ? 
                      addressOfLastBlock - addressFirstBlock : (addressOfLastBlock - addressFirstBlock) / dwBlockLength;

    DWORD dwArg = 0;

    while (dwNumBlock--)
    {
        BOOL bIsSD = FALSE;
        switch (pTestParams->sdDeviceType) {
            case SDDEVICE_SD:
            case SDDEVICE_SDHC:
                bIsSD = TRUE;
                break;
        }

        // Data Address is in byte unites for SD memory, and in block unity for SDHC
        dwArg = (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType) == TRUE) ? ulAddr : ulAddr * dwBlockLength;

        rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_WRITE_BLOCK, dwArg, SD_WRITE, ResponseR1, pResp, 1, dwBlockLength, buffer, 0 /* bIsSD ? 0 : SD_AUTO_ISSUE_CMD12*/);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent1,TEXT("FakeErase: Write block bus request failed. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("A bus request to write data to the card failed. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
            goto DONE;
        }
        ulAddr++;
    }
DONE:
    if (buffer)
    {
        free(buffer);
        buffer = NULL;
    }
    return rStat;
}

SD_API_STATUS SDPerformErase(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, ULONG addressFirstBlock, ULONG addressOfLastBlock, PSD_COMMAND_RESPONSE pResp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    SD_API_STATUS                 rStat = 0;

    //2 Performing Erase
    //3 Set First Block To Erase
    LogDebug(indent, SDIO_ZONE_TST, TEXT("PerfErase: Setting the first block to erase..."));
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_ERASE_WR_BLK_START, addressFirstBlock, SD_COMMAND, ResponseR1, pResp, 0, 0, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("PerfErase: Unable to set the location of the first block to erase. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to set the first block on the SD Card to erase. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        DumpStatusFromResponse(indent1, pResp);
        goto DONE;
    }
    //3 Set Last Block To Be Erased
    LogDebug(indent, SDIO_ZONE_TST, TEXT("PerfErase: Setting the last block to erase..."));
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_ERASE_WR_BLK_END, addressOfLastBlock, SD_COMMAND, ResponseR1, pResp, 0, 0, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("PerfErase: Unable to set the location of the last block to erase. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to set the last block on the SD Card to erase. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        DumpStatusFromResponse(indent1, pResp);
        goto DONE;
    }
    //3 Erase Blocks
    LogDebug(indent, SDIO_ZONE_TST, TEXT("PerfErase: Erasing..."));
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_ERASE, 0, SD_COMMAND, ResponseR1b, pResp, 0, 0, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("PerfErase: Unable to perform erase bus request. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to erase blocks within bounds. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        DumpStatusFromResponse(indent1, pResp);
    }
    // Add wait for the erase operation.
    Sleep(SDTST_REQ_TIMEOUT);

DONE:
    return rStat;

}

BOOL IsEraseLow(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, SD_API_STATUS *psdStat, PSD_COMMAND_RESPONSE pResp)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    SD_API_STATUS                 rStat = 0;
    SD_CARD_RCA                  RCA;
    PUCHAR                        pSCR_Bits = NULL;
    BOOL                         bSetLow = -1;

    if ((pTestParams->sdDeviceType == SDDEVICE_MMC) || (pTestParams->sdDeviceType == SDDEVICE_MMCHC))
    {
        return TRUE;
    }
    //2 First allocate memory for SCR bits

    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsLow: Allocating Memory for SCR Register bits..."));
    pSCR_Bits = (PUCHAR)malloc(sizeof(UCHAR)*SD_SCR_REGISTER_SIZE);
    if (!pSCR_Bits)
    {
        LogFail(indent1,TEXT("IsLow: Insufficient memory to get the contents of the SCR Register."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Unable to allocate memory for a buffer to contain the raw SCR bits. Cannot proceed."));
        goto DONE;
    }
    ZeroMemory(pSCR_Bits, sizeof(UCHAR)*SD_SCR_REGISTER_SIZE);

    //2 Next Determine if erase bits go high or low
    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsLow: Determining if bits go high or low on erase..."));
    //3 Get the RCA
    rStat = GetRelativeAddress(indent1, hDevice, pTestParams, &RCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("IsLow: Bailing..."));
        goto DONE;
    }

    //3 Send the a request to go to the application commands

    rStat = AppCMD(indent1, hDevice, pTestParams, pResp, RCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("IsLow: Bailing..."));
        goto DONE;
    }

    //3 Send a request to get the SCR register
    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsLow: Sending bus request to get SCR register..."));
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_ACMD_SEND_SCR, 0, SD_READ, ResponseR1, pResp, 1, SD_SCR_REGISTER_SIZE, pSCR_Bits, 0, TRUE);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("IsLow: Unable to get the card's SD Configuration Register. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to get the SD Card's SCR register. Without it, one cannot tell if erase bits are high or low. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        DumpStatusFromResponse(indent2, pResp);
        goto DONE;
    }

    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsLow: Reading bit that indicates if erased bits are high or low..."));
    //3 Determine if the bits go high or low on erase
    if (pSCR_Bits[1] & 1<<7)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("IsLow: Erase bits go high."));
        bSetLow = FALSE;
    }
    else
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("IsLow: Erase bits go low."));
        bSetLow= TRUE;
    }
DONE:
    if (pSCR_Bits)
    {
        free(pSCR_Bits);
        pSCR_Bits = NULL;
    }
    *psdStat = rStat;
    return bSetLow;
}

SD_API_STATUS SDEraseMemoryBlocks(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, ULONG addressFirstBlock, ULONG addressOfLastBlock, PBOOL pbSetLow, DWORD dwBlockLength)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    SD_API_STATUS                 rStat = 0;
    PSD_COMMAND_RESPONSE        pResp=NULL;
    BOOL                         bEraseSupp = FALSE;
    ULONG ulStartEraseAddress = addressFirstBlock;
    ULONG ulEndEraseAddress = addressOfLastBlock;

    *pbSetLow = -1;
    //2 First check if block erasing is supported

    bEraseSupp = IsBlockEraseSupported(indent, hDevice, pTestParams, &rStat);
    if (bEraseSupp < 0)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("EraseMemBlocks: Bailing..."));
        goto DONE;
    }

    //2 Next allocate memory for responses
    LogDebug(indent, SDIO_ZONE_TST, TEXT("EraseMemBlocks: Allocating Memory buffers..."));
    AllocateResponseBuff(indent1, pTestParams, &pResp, FALSE);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("EraseMemBlocks: Determining if bits go high or low on erase..."));

    //2 Determining if bits go Low or High
    *pbSetLow = IsEraseLow(indent1, hDevice, pTestParams, &rStat, pResp);

    if (!IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType))
    {
        ulStartEraseAddress *= dwBlockLength;
        ulEndEraseAddress *= dwBlockLength;
    }

    if (*pbSetLow >= FALSE)
    {
        if (bEraseSupp)
        {
            //2 Performing Erase
            LogDebug(indent, SDIO_ZONE_TST, TEXT("EraseMemBlocks: Performing erase operation..."));
            rStat = SDPerformErase(indent1, hDevice, pTestParams, ulStartEraseAddress, ulEndEraseAddress, pResp);
        }
        else
        {
            //2 Faking Erase
            if (*pbSetLow) LogDebug(indent, SDIO_ZONE_TST, TEXT("EraseMemBlocks: Zeroing memory on card between block addresses.."));
            else LogDebug(indent, SDIO_ZONE_TST, TEXT("EraseMemBlocks: Setting  memory on card high between block addresses.."));
            rStat = FakeErase(indent1, hDevice, pTestParams, ulStartEraseAddress, ulEndEraseAddress, pResp, dwBlockLength, *pbSetLow);
        }
    }
DONE:
    if (pResp)
    {
        free(pResp);
        pResp = NULL;
    }
    return rStat;
}

BOOL bCompareResponseToCardInfo(UINT indent, SD_DEVICE_HANDLE hDevice, SD_INFO_TYPE it, PSD_COMMAND_RESPONSE pResp, SD_API_STATUS *stat)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    PSD_PARSED_REGISTER_CID     pParsedCID = NULL;
    PSD_PARSED_REGISTER_CSD     pParsedCSD = NULL;
    SD_API_STATUS                 rStat = 0;

    if (stat != NULL)
    {
        *stat = 0;
    }

    BOOL bRet = TRUE;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CompRespToCardInfo: Perform comparison of register data to Cached Card Info..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CompRespToCardInfo: Verifying the response pointer is not NULL..."));
    if (pResp == NULL)
    {
        LogFail(indent2, TEXT("CompRespToCardInfo: the pointer to the response is NULL. Bailing...."));
        bRet = -1;
        goto DONE;
    }

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CompRespToCardInfo: Allocating Memory for Card Info buffers..."));
    if (it == SD_INFO_REGISTER_CID)
    {
        pParsedCID = (PSD_PARSED_REGISTER_CID)malloc(sizeof(SD_PARSED_REGISTER_CID));
        if (pParsedCID == NULL)
        {
            LogFail(indent2, TEXT("CompRespToCardInfo: Not enough memory to get cached CID info from bus. Bailing...."));
            bRet = -2;
            goto DONE;
        }
        else
        {
            ZeroMemory(pParsedCID, sizeof(SD_PARSED_REGISTER_CID));
        }
    }
    else if (it == SD_INFO_REGISTER_CSD)
    {
        pParsedCSD = (PSD_PARSED_REGISTER_CSD)malloc(sizeof(SD_PARSED_REGISTER_CSD));
        if (pParsedCSD == NULL)
        {
            LogFail(indent2, TEXT("CompRespToCardInfo: Not enough memory to get cached CSD info from bus. Bailing...."));
            bRet = -2;
            goto DONE;
        }
    }
    else
    {
        LogFail(indent2, TEXT("CompRespToCardInfo: The Card info type you have passed in either is not groveled from a response, or that response is not supported in this function. Bailing...."));
        bRet = -3;
        goto DONE;
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CompRespToCardInfo: Getting Cached Card Data..."));
    if (it == SD_INFO_REGISTER_CID)
    {
        rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_REGISTER_CID, pParsedCID, sizeof(SD_PARSED_REGISTER_CID));
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent2,TEXT("CompRespToCardInfo: Unable to get the bus's cached CID register. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
            bRet = -4;
            if (stat) *stat = rStat;
            goto DONE;
        }
    }
    else
    {
        rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_REGISTER_CSD, pParsedCSD, sizeof(SD_PARSED_REGISTER_CSD));
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent2,TEXT("CompRespToCardInfo: Unable to get the bus's cached CSD register. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
            bRet = -4;
            if (stat) *stat = rStat;
            goto DONE;
        }
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CompRespToCardInfo: Performing Comparison..."));
    {
        if (it == SD_INFO_REGISTER_CID)
        {
            //first 8 bits are CRC, which isn't always the same.
            pResp->ResponseBuffer[0] = pParsedCID->RawCIDRegister[0];

            if (memcmp(pResp->ResponseBuffer, pParsedCID->RawCIDRegister, SD_CID_REGISTER_SIZE) == 0)
            {
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
        else
        {
            //first 8 bits are CRC, which isn't always the same.
            pResp->ResponseBuffer[0] = pParsedCSD->RawCSDRegister[0];
            if (memcmp(pResp->ResponseBuffer, pParsedCSD->RawCSDRegister, SD_CSD_REGISTER_SIZE) == 0)
            {
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
    }
DONE:
    if (pParsedCID)
    {
        free(pParsedCID);
        pParsedCID = NULL;
    }
    if (pParsedCSD)
    {
        free(pParsedCSD);
        pParsedCSD = NULL;
    }
    return bRet;
}

BOOL SendCardToStandbyState(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, SD_API_STATUS *pRStat)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL                         bInStandby = FALSE;
    PSD_COMMAND_RESPONSE        pResp=NULL;
    DWORD                         dwState;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("SndCrdToSB: Getting current State..."));
    dwState = GetCurrentState(indent1, hDevice, pTestParams);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SndCrdToSB: Determining if function can set card to STANDBY State..."));
    if (dwState == SD_STATUS_CURRENT_STATE_STDBY)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("SndCrdToSB: The card is already in the STANDBY state. "));
        bInStandby = TRUE;
    }
    else if (dwState == SD_STATUS_CURRENT_STATE_TRAN)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("SndCrdToSB: Attempting to set card to STANDBY State..."));
        AllocateResponseBuff(indent2, pTestParams, &pResp, FALSE);
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("SndCrdToSB: Deselecting card through a CMD7 bus request. Deselecting a card in the TRANSFER state sends it to the STANDBY state..."));
        *pRStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_SELECT_DESELECT_CARD, 0, SD_COMMAND, NoResponse, pResp, 0, 0, NULL, 0);
        if (!SD_API_SUCCESS(*pRStat))
        {
            LogFail(indent3,TEXT("SndCrdToSB: Unable to deselect card. Error Code: %s (0x%x) returned."),TranslateErrorCodes(*pRStat), *pRStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to deselect the card. Error Code: %s (0x%x) returned."), TranslateErrorCodes(*pRStat), *pRStat);
            DumpStatusFromResponse(indent3, pResp);
        }
        else
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("SndCrdToSB: Getting new State..."));
            dwState = GetCurrentState(indent4, hDevice, pTestParams);
            if (dwState == SD_STATUS_CURRENT_STATE_STDBY) bInStandby = TRUE;
            else
            {
                LogFail(indent4,TEXT("SndCrdToSB: Deselecting card succeeded, but the card is not in the STANDBY state. Card State = %s (%u)."),TranslateState(dwState), dwState);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Even though the card was successfully deselected, it was not put into the STANDBY state"), TranslateState(dwState), dwState);
            }
        }
    }
    else
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("SndCrdToSB: This function will only set a card to the STANDBY state if it is in the TRANSFER state. It is currently in the %s (%u) state. "), TranslateState(dwState), dwState);
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SndCrdToSB: Cleanup..."));
    if (pResp)
    {
        free(pResp);
        pResp = NULL;
    }
    return bInStandby;
}

BOOL SendCardToTransferState(UINT indent, SD_DEVICE_HANDLE hDevice, SD_CARD_RCA RCA, PTEST_PARAMS pTestParams, SD_API_STATUS *pRStat)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL                         bInTransfer = FALSE;
    PSD_COMMAND_RESPONSE        pResp=NULL;
//    SD_CARD_STATUS             *pCardStat = NULL;

    DWORD dwState;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("SndCrdToTr: Getting current State..."));
    dwState = GetCurrentState(indent1, hDevice, pTestParams);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SndCrdToTr: Determining if function can set card to STANDBY State..."));

    if (dwState == SD_STATUS_CURRENT_STATE_TRAN)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("SndCrdToTr: The card is already in the TRANSFER state. "));
        bInTransfer = TRUE;
    }
    else if (dwState == SD_STATUS_CURRENT_STATE_STDBY)
    {

        LogDebug(indent1, SDIO_ZONE_TST, TEXT("SndCrdToTr: Attempting to set card to TRANSFER State..."));
        AllocateResponseBuff(indent2, pTestParams, &pResp, FALSE);
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("SndCrdToTr: Selecting card through a CMD7 bus request. Selecting a card in the STANDBY state sends it to the TRANSFER state..."));
        *pRStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_SELECT_DESELECT_CARD, RCA<<16, SD_COMMAND, ResponseR1b, pResp, 0, 0, NULL, 0);
        if (!SD_API_SUCCESS(*pRStat))
        {
            LogFail(indent3,TEXT("SndCrdToTr: Unable to select card. Error Code: %s (0x%x) returned."),TranslateErrorCodes(*pRStat), *pRStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to select the card. Error Code: %s (0x%x) returned."), TranslateErrorCodes(*pRStat), *pRStat);
            DumpStatusFromResponse(indent3, pResp);
        }
        else
        {
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("SndCrdToTr: Getting new State..."));
            dwState = GetCurrentState(indent3, hDevice, pTestParams);
            if (dwState == SD_STATUS_CURRENT_STATE_TRAN) bInTransfer = TRUE;
            else
            {
                LogFail(indent4,TEXT("SndCrdToTr: Selecting card succeeded, but the card is not in the TRANSFER state. Card State = %s (%u)."),TranslateState(dwState), dwState);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Even though the card was successfully selected, it was not put into the TRANSFER state"), TranslateState(dwState), dwState);
            }
        }
    }
    else
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("SndCrdToTr: This function will only set a card to the TRANSFER state if it is in the STANDBY state. It is currently in the %s (%u) state. "), TranslateState(dwState), dwState);
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SndCrdToTr: Cleanup..."));
    if (pResp)
    {
        free(pResp);
        pResp = NULL;
    }
    return bInTransfer;
}

DWORD GetCurrentState(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pT)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    SD_API_STATUS         rStat;
    DWORD                 dwRet = 9;

    SD_CARD_STATUS     *pCardStat = NULL;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("GetCurrentState: Allocating memory for Status Register info..."));
    pCardStat = (SD_CARD_STATUS *)malloc(sizeof(SD_CARD_STATUS));
    if (pCardStat == NULL)
    {
        LogFail(indent1,TEXT("GetCurrentState: Insufficient memory to get the contents of the Status Register."));
        AppendMessageBufferAndResult(pT, TPR_FAIL,TEXT("Unable to get the parsed Status register info. There is probably insufficient memory. Bailing...."));
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GetCurrentState: Getting Status Register info..."));
    rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_CARD_STATUS, pCardStat, sizeof(SD_CARD_STATUS));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("GetCurrentState: Unable to get the card's status register information. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pT, TPR_FAIL,TEXT("Failure getting cards status register. This means I cannot get the card's current state, Bailing...."));
        goto DONE;
    }
    dwRet = SD_STATUS_CURRENT_STATE(*pCardStat);
DONE:
    if (pCardStat)
    {
        free(pCardStat);
        pCardStat = NULL;
    }

    return dwRet;
}

DWORD GetCurrentStateNoFailOnError(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS /*pT*/)
{
    SD_API_STATUS         rStat;
    DWORD                 dwRet = 9;

    SD_CARD_STATUS     *pCardStat = NULL;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("GetCurrentState: Allocating memory for Status Register info..."));
    pCardStat = (SD_CARD_STATUS *)malloc(sizeof(SD_CARD_STATUS));
    if (pCardStat == NULL)
    {
        //AppendMessageBufferAndResult(pT, TPR_FAIL,TEXT("Unable to get the parsed Status register info. There is probably insufficient memory. Bailing...."));
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GetCurrentState: Getting Status Register info..."));
    rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_CARD_STATUS, pCardStat, sizeof(SD_CARD_STATUS));
    if (!SD_API_SUCCESS(rStat))
    {
    //    AppendMessageBufferAndResult(pT, TPR_FAIL,TEXT("Failure getting cards status register. This means I cannot get the card's current state, Bailing...."));
        goto DONE;
    }
    dwRet = SD_STATUS_CURRENT_STATE(*pCardStat);
DONE:
    if (pCardStat)
    {
        free(pCardStat);
        pCardStat = NULL;
    }

    return dwRet;
}

BOOL IsState(UINT indent, DWORD dwKnownState, DWORD dwUnknownState, PBOOL pbIsSkipped, PTEST_PARAMS pTestParams, LPCTSTR msg, BOOL bBadStateIsAbort, BOOL bSilentFail)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    BOOL bRet = FALSE;
    TCHAR buff[18];
    //Note: bSilentFail should only be TRUE if you call GetCurrentState within the function call of IsState (i.e. dwUnknownState is the result)
    //        dwUnknownState will exceed known good states. A failure will already be logged in the test params. And there is no need to make
    //        it look like there is some sort of test error (a new failure) to have a bad state in that case.

    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsState: Checking For Valid Params..."));
    if (pbIsSkipped == NULL)
    {
        LogFail(indent1,TEXT("IsState: Invalid Params. pbIsSkipped = NULL."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Test Error!! Invalid Params in IsState function. pbIsSkipped = NULL."));
        goto DONE;
    }
    *pbIsSkipped = FALSE;
    if (dwKnownState > 8)
    {
        LogFail(indent1,TEXT("IsState: Invalid Params. dwKnownState exceeds last good state."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Test Error!! Invalid Params in IsState function. dwKnownState exceeds last good state."));
        *pbIsSkipped = TRUE;
        goto DONE;
    }
    if (!bSilentFail)
    {
        LogDebug(indent, SDIO_ZONE_TST, TEXT("IsState: Checking if unknown state is good..."));
    }
    if (dwUnknownState > 8)
    {
        if (!bSilentFail)
        {
            LogFail(indent1,TEXT("IsState: The Card has gone into a state that I can not interpret. The test should have been in the %s state."), TranslateState(dwKnownState));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("The Card has gone into I state that I can not interpret. The test should have been in the %s state."), TranslateState(dwKnownState));
        }
        *pbIsSkipped = TRUE;
        goto DONE;
    }

    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsState: Checking if Current Card state is %s (%u)..."), TranslateState(dwKnownState), dwKnownState);
    if (dwKnownState == dwUnknownState)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("IsState: Current Card State is %s as expected."), TranslateState(dwKnownState));
        bRet = TRUE;
    }
    else
    {
        ZeroMemory(buff, 18 *sizeof(TCHAR));
        StringCchCopy(buff, _countof(buff), TranslateState(dwKnownState));
        if (msg != NULL)
        {
            if (bBadStateIsAbort)
            {
                LogAbort(indent1,TEXT("IsState: Current card state is %s (%u) not %s (%u). %s"), TranslateState(dwUnknownState), dwUnknownState, buff, dwKnownState, msg);
                AppendMessageBufferAndResult(pTestParams, TPR_ABORT,TEXT("Current card state is %s (%u) not %s (%u). %s"), TranslateState(dwUnknownState), dwUnknownState, buff, dwKnownState, msg);
            }
            else
            {
                LogFail(indent1,TEXT("IsState: Current card state is %s (%u) not %s (%u). %s"), TranslateState(dwUnknownState), dwUnknownState, buff, dwKnownState, msg);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Current card state is %s (%u) not %s (%u). %s"), TranslateState(dwUnknownState), dwUnknownState, buff, dwKnownState, msg);
            }
        }
        else
        {
            if (bBadStateIsAbort)
            {
                LogAbort(indent1,TEXT("IsState: Current card state is %s (%u) not %s (%u)."), TranslateState(dwUnknownState), dwUnknownState, buff, dwKnownState);
                AppendMessageBufferAndResult(pTestParams, TPR_ABORT, TEXT("Current card state is %s (%u) not %s (%u)."), TranslateState(dwUnknownState), dwUnknownState, buff, dwKnownState);
            }
            else
            {
                LogFail(indent1,TEXT("IsState: Current card state is %s (%u) not %s (%u)."), TranslateState(dwUnknownState), dwUnknownState, buff, dwKnownState);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Current card state is %s (%u) not %s (%u)."), TranslateState(dwUnknownState), dwUnknownState, buff, dwKnownState);
            }
        }
    }
DONE:
    return bRet;
}

SD_API_STATUS GetRelativeAddress(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, SD_CARD_RCA *pRCA)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    SD_API_STATUS                 rStat = 0;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GetRelAddr: Getting the cards relative address..."));
    rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_REGISTER_RCA, pRCA, sizeof(SD_CARD_RCA));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("GetRelAddr: Unable to get the card's relative address. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("SDCardInfoQuery failed so the contents of the RCA register could not be obtained. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
    }
    return rStat;
}

BOOL AllocateResponseBuff(UINT indent, PTEST_PARAMS pTestParams, PSD_COMMAND_RESPONSE *ppResp, BOOL bIsFailure)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    BOOL     bSucc = FALSE;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("AllocRespBuff: Allocating memory for bus response..."), TranslateCommandCodes(pTestParams->BusRequestCode), pTestParams->BusRequestCode);
    *ppResp = (PSD_COMMAND_RESPONSE)malloc(sizeof(SD_COMMAND_RESPONSE));
    if (!(*ppResp))
    {
        if (bIsFailure)
        {
            LogFail(indent1,TEXT("AllocRespBuff: Insufficient memory to study response. The only way to get the data out of this register is by studying the response. Thus lack of memory is a failure condition."));
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure allocating memory for the bus request response.\nParsing the response buffer is vital for this test."));
        }
        else
        {
            LogWarn(indent1,TEXT("AllocRespBuff: Insufficient memory to study response This is not necessary for this test."));
            AppendMessageBufferAndResult(pTestParams, TPR_PASS, TEXT("Insufficient memory to study response. This is not necessary for this test."));
        }
    }
    else
    {
        ZeroMemory(*ppResp, sizeof(SD_COMMAND_RESPONSE));
        bSucc = TRUE;
    }
    return bSucc;
}

SD_API_STATUS AppCMD(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, PSD_COMMAND_RESPONSE pResp, SD_CARD_RCA RCA)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    SD_API_STATUS     rStat = 0;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("AppCmd: Sending bus request to switch to application command mode..."));
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_APP_CMD, RCA<<16, SD_COMMAND, ResponseR1, pResp, 0, 0, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("AppCmd: Unable to get the card's relative address. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure in the bus request to set the command type to an Application Command. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        DumpStatusFromResponse(indent3, pResp);
    }
    return rStat;
}

SD_API_STATUS SetBlockLength(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, PSD_COMMAND_RESPONSE pResp, DWORD dwBlockLength)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    SD_API_STATUS     rStat = 0;
    if (dwBlockLength == 512) LogDebug(indent, SDIO_ZONE_TST, TEXT("SetBL: Setting Block length for later call to 512 bytes (All cards must support this block length)..."));
    else LogDebug(indent, SDIO_ZONE_TST, TEXT("SetBL: Attempting to set  Block length for later call to %u bytes ..."), dwBlockLength);
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_SET_BLOCKLEN, dwBlockLength, SD_COMMAND, ResponseR1, pResp, 0, 0, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("SetBL: Unable to set block length. %s (0x%x) returned"),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Attempt to Set block length through bus request failed. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        if (pResp)
        {
            LogFail(indent1,TEXT("SetBL: Dumping status Info from the response..."));
            DumpStatusFromResponse(indent2, pResp);
        }
    }
    return rStat;
}

BOOL IsWPSupported(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pT, PBOOL pbSuccess, PUCHAR pSectSize, PUCHAR pWPGSize, PDWORD pdwCardSize)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    PSD_PARSED_REGISTER_CSD pCSD = NULL;
    BOOL bSupported = FALSE;
    BOOL bSuccess = FALSE;
    SD_API_STATUS rStat;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsWPSupp: Allocating memory for CSD Register info..."));
    pCSD = (PSD_PARSED_REGISTER_CSD) malloc(sizeof(SD_PARSED_REGISTER_CSD));
    if (pCSD == NULL)
    {
        LogFail(indent1,TEXT("IsWPSupp: Insufficient memory to get the contents of the CSD Register."));
        AppendMessageBufferAndResult(pT, TPR_FAIL,TEXT("Unable to get the parsed CSD register info. There is probably insufficient memory. Bailing...."));
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsWPSupp: Getting the card\'s CSD register..."));
    rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_REGISTER_CSD, pCSD, sizeof(SD_PARSED_REGISTER_CSD));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("IsWPSupp: Unable to get the card's CSD register. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pT, TPR_FAIL,TEXT("SDCardInfoQuery failed so the contents of the CSD register could not be obtained. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    bSuccess = TRUE;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("IsWPSupp: Groveling Write protection info..."));
    if (pCSD->WPGroupEnable != 0)
    {
        bSupported = TRUE;
        if (pSectSize != NULL)
        {
            *pSectSize= pCSD->EraseSectorSize;
        }
        if (pWPGSize != NULL)
        {
            *pWPGSize= pCSD->WPGroupSize;
        }
    }
    if (pdwCardSize != NULL)
    {
        *pdwCardSize = pCSD->DeviceSize;
    }
DONE:
    if (pbSuccess) *pbSuccess = bSuccess;
    return bSupported;
}

void GenerateEventName(UINT indent, UINT num, PTEST_PARAMS pTestParams, LPTSTR *plptstr)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    TCHAR tmp[4];

    ZeroMemory(tmp, sizeof(TCHAR) *4);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GTName: Allocating memory for name of %u%s event and filling it..."),num + 1,GetNumberSuffix(num + 1));

    *plptstr = (LPTSTR)malloc(50);//sizeof(TCHAR) * 25);
    if (*plptstr == NULL)
    {
        LogFail(indent1,TEXT("GTName: Insufficient memory to create necessary buffer to hold the %u%s event name"), num + 1,GetNumberSuffix(num + 1));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Could not create a buffer to contain the %u%s event name.\nThus, the corresponding bus request can not be made. This is due to a probable lack of memory.\nBailing..."),num + 1,GetNumberSuffix(num + 1));
    }
    else
    {
        ZeroMemory(*plptstr, sizeof(TCHAR) * 25);
        StringCchPrintf(*plptstr, 25, TEXT("%s%u"), BR_EVENT_NAME, num);
    }

}

BOOL AreBlocksCleared(UINT indent, PBOOL pbErr, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams, ULONG addressFirstBlock, ULONG addressOfLastBlock, BOOL bSetLow, DWORD dwBlockLength)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    SD_API_STATUS rStat = SD_API_STATUS_SUCCESS;
    BOOL bErased = FALSE;
    BOOL bTmp = TRUE;
    UINT uiBuffLengthInBlocks = 0;
    UINT uiTotalLengthInBlocks = 0;
    UINT cBytesInBuffer = 0;
    ULONG numReads = 0;
    ULONG blocksRemainder = 0;
    PUCHAR puReadBuff = NULL, puCompareBuff = NULL;
    ULONG c;
    BOOL bErr = FALSE;

    if (addressFirstBlock > addressOfLastBlock)
    {
        LogFail(indent1,TEXT("AreBlksCleared: Address of first block is greater than Address of second block."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Parameter error. The Address of the first block should be les than or ewual to the address of the last block. It is not. Bailing...."));
        bErr= TRUE;
        goto DONE;
    }
    if (((addressOfLastBlock - addressFirstBlock) % dwBlockLength) != 0)
    {
        LogFail(indent1,TEXT("AreBlksCleared: The difference between the first and last block adresses  is not a multiple of block size."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Parameter error. The Address of the last block - the Adress of the First block should be a multiple of block size or 0. Bailing...."));
        bErr = TRUE;
        goto DONE;
    }

    uiTotalLengthInBlocks = ((addressOfLastBlock - addressFirstBlock) / dwBlockLength) + 1;
    uiBuffLengthInBlocks = min(uiTotalLengthInBlocks, 512);
    numReads = dwRndUp(uiTotalLengthInBlocks, uiBuffLengthInBlocks);

    if(UIntMult(uiBuffLengthInBlocks, dwBlockLength, &cBytesInBuffer) != S_OK)
    {
    
        cBytesInBuffer = 0;
        rStat = 1;
        LogFail(indent1,TEXT("AreBlksCleared: Invalid size specified for buffer creation."));
        bErr = TRUE;
        goto DONE;
    }

    if(numReads > 1)
    {
        blocksRemainder = uiTotalLengthInBlocks - ((numReads - 1) * 512);
    }
    //2 Allocating memory for block to read from card
    LogDebug(indent, SDIO_ZONE_TST, TEXT("AreBlksCleared: Allocating memory for buffer to read from  card"));
    AllocBuffer(indent1, cBytesInBuffer, BUFF_ZERO, &puReadBuff);
    if (puReadBuff == NULL)
    {
        LogFail(indent1,TEXT("AreBlksCleared: Insufficient memory to create necessary buffer."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Unable to create buffer. There is probably insufficient memory. Bailing...."));
        bErr = TRUE;
        goto DONE;
    }
    //2 Allocating comparison buffer
    LogDebug(indent, SDIO_ZONE_TST, TEXT("AreBlksCleared: Allocating memory for comparison buffer."));
    if (bSetLow)
    {
        AllocBuffer(indent1, cBytesInBuffer, BUFF_ZERO, &puCompareBuff);
    }
    else
    {
        AllocBuffer(indent1, cBytesInBuffer, BUFF_HIGH, &puCompareBuff);
    }
    if (puCompareBuff == NULL)
    {
        LogFail(indent1,TEXT("AreBlksCleared: Insufficient memory to create necessary buffer."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Unable to create buffer. There is probably insufficient memory. Bailing...."));
        bErr = FALSE;
        goto DONE;
    }
    //2 Read and Compare
    for (c = 0; c < numReads; c++)
    {
        if ((c == numReads - 1) && (blocksRemainder != 0))
        {
            LogDebug(indent, SDIO_ZONE_TST, TEXT("AreBlksCleared: Reading %u Blocks at adress 0x%x."), blocksRemainder, addressFirstBlock + (c*cBytesInBuffer));

            rStat = ReadBlocks(indent1, blocksRemainder, dwBlockLength, addressFirstBlock + (c*cBytesInBuffer), puReadBuff, hDevice, pTestParams);
            if (!SD_API_SUCCESS(rStat))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("AreBlksCleared: Bailing..."));
                bErr = FALSE;
                goto DONE;
            }
            if (memcmp(puReadBuff, puCompareBuff, (dwBlockLength *blocksRemainder)) != 0)
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("AreBlksCleared: Mismatch detected in the %u%s %u block region read. (Actually only %u blocs was read, but all the previous reads were %u blocks)"), c+1, GetNumberSuffix(c + 1), uiBuffLengthInBlocks, blocksRemainder, uiBuffLengthInBlocks);
                bTmp = FALSE;
            }
        }
        else
        {
            LogDebug(indent, SDIO_ZONE_TST, TEXT("AreBlksCleared: Reading %u Blocks at adress 0x%x."), uiBuffLengthInBlocks, addressFirstBlock + (c*cBytesInBuffer));
            rStat = ReadBlocks(indent1, uiBuffLengthInBlocks, dwBlockLength, addressFirstBlock + (c*cBytesInBuffer), puReadBuff, hDevice, pTestParams);
            if (!SD_API_SUCCESS(rStat))
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("AreBlksCleared: Bailing..."));
                bErr = FALSE;
                goto DONE;
            }
            if (memcmp(puReadBuff, puCompareBuff, cBytesInBuffer) != 0)
            {
                LogDebug(indent2, SDIO_ZONE_TST, TEXT("AreBlksCleared: Mismatch detected in the %u%s %u block region read."), c+1, GetNumberSuffix(c + 1), uiBuffLengthInBlocks);
                bTmp = FALSE;
            }
        }
    }

DONE:
    if ((bErr == FALSE) && (bTmp != FALSE)) bErased = TRUE;
    if (pbErr) *pbErr = bErr;
    DeAllocBuffer(&puReadBuff);
    DeAllocBuffer(&puCompareBuff);
    return bErased;
}

SD_API_STATUS IssueCMD12(SD_DEVICE_HANDLE hDevice, PSD_CARD_STATUS pCrdStat)
{
    SD_API_STATUS rStat;
    SD_COMMAND_RESPONSE resp;
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_STOP_TRANSMISSION, 0, SD_COMMAND, ResponseR1b, &resp, 0, 0, NULL, 0);//, FALSE, pCrdStat);
    if (pCrdStat)
    {
        SDGetCardStatusFromResponse(&resp, pCrdStat);
    }
    return rStat;
}

BOOL IsRWTypeSupported(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams, RWType rwt, SD_API_STATUS* prStat)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    BOOL bSupport = FALSE;
    SD_API_STATUS rStat = 0;
    PSD_PARSED_REGISTER_CSD pCSD;
    pCSD = (PSD_PARSED_REGISTER_CSD)malloc(sizeof(SD_PARSED_REGISTER_CSD));
    if (pCSD == NULL)
    {
        LogFail(indent1,TEXT("IsRWTypeSupported: Insufficient memory to create SD_PARSED_REGISTER_CSD structure."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Unable to create SD_PARSED_REGISTER_CSD structure. There is probably insufficient memory. Bailing...."));
        if (prStat) *prStat = SD_API_STATUS_NO_MEMORY;
        return FALSE;
    }
    ZeroMemory(pCSD, sizeof(SD_PARSED_REGISTER_CSD));
    rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_REGISTER_CSD, pCSD, sizeof(SD_PARSED_REGISTER_CSD));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("IsRWTypeSupported: Unable to get parsed CSD register. SDCardInfoQuery failed %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT(" Unable to get parsed CSD register, and thus determine what type of reads and writes are supported. SDCardInfoQuery failed %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
    }
    else
    {
        switch(rwt)
        {
            case Read_Partial:
                if (pCSD->ReadBlockPartial) bSupport = TRUE;
                break;
            case Write_Partial:
                if (pCSD->WriteBlockPartial) bSupport = TRUE;
                break;
            case Read_Misalign:
                if (pCSD->ReadBlockMisalign) bSupport = TRUE;
                break;
            case Write_Misalign:
                if (pCSD->WriteBlockMisalign) bSupport = TRUE;
        }
    }
    if (prStat) *prStat = rStat;
    return bSupport;
}
