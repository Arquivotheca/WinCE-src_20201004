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
#include <windows.h>
#include <sdcard.h>
#include <sdcardddk.h>
#include <sdmem.h>
#include "debug.h"
#include "sdtst.h"
#include "cmn_drv_trans.h"
#include "gen_hlp.h"

/************************************************************************************************/
/*                                                                                                */
/*   Note: The following are SDClient APIs. They are acessed via function pointers into the bus.                    */
/*                                                                                                */
/************************************************************************************************/
#define SD__IO_STATUS_CURRENT_STATE_MASK        0x30
#define SD_IO_STATUS_CURRENT_STATE_SHIFT       4

#define SD_IO_STATUS_CURRENT_STATE(sd_status) \
    (((sd_status)&SD__IO_STATUS_CURRENT_STATE_MASK)>>SD_IO_STATUS_CURRENT_STATE_SHIFT)

#define SD_IO_STATUS_CURRENT_STATE_DIS        0
#define SD_IO_STATUS_CURRENT_STATE_CMD       1
#define SD_IO_STATUS_CURRENT_STATE_TRAN       2
#define SD_IO_STATUS_CURRENT_STATE_RFU      3

#define IO_RW_EXTENDED_ARG_BLOCK_MODE(Arg)   (((Arg)>>27)&1)
#define IO_RW_EXTENDED_ARG_OPCODE(Arg)   (((Arg)>>26)&1)
#define IO_RW_EXTENDED_ARG_COUNT(Arg)  ((UCHAR)((Arg)&0x1FF))

//2SD & SDIO

SD_API_STATUS wrap_SDCardInfoQuery(SD_DEVICE_HANDLE hDevice, SD_INFO_TYPE InfoType, PVOID pCardInfo,  ULONG  StructureSize)
{
    SD_API_STATUS sdRet;
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: sdReturn = SDCardInfoQuery(0x%x,  %s, 0x%x, %u)"),hDevice, TranslateInfoTypes(InfoType), pCardInfo, StructureSize);
    sdRet = SDCardInfoQuery(hDevice, InfoType, pCardInfo, StructureSize);
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDCardInfoQuery returned: %s (0x%x)"),TranslateErrorCodes(sdRet), sdRet);
    return sdRet;

}

SD_API_STATUS wrap_SDSynchronousBusRequest(SD_DEVICE_HANDLE  hDevice, UCHAR Command, DWORD Argument, SD_TRANSFER_CLASS TransferClass,
                    SD_RESPONSE_TYPE ResponseType, PSD_COMMAND_RESPONSE pResponse, ULONG NumBlocks, ULONG  BlockSize, PUCHAR pBuffer, DWORD Flags,
                    BOOL bAppCmd, PSD_CARD_STATUS pCrdStat, BOOL bSkipState)
{
    BOOL bSDIO = FALSE;
    SD_API_STATUS sdRet;
    SDIO_CARD_INFO     sdioci;
    bSDIO = SD_API_SUCCESS(SDCardInfoQuery(hDevice, SD_INFO_SDIO, &sdioci, sizeof(SDIO_CARD_INFO)));

    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: sdReturn = SDSynchronousBusRequest(0x%x,  %s, %u, %s, %s, 0x%x, %u, %u, 0x%x, %s )"),hDevice, TranslateCommandCodes(Command, bAppCmd), Argument, TranslateTransferClass(TransferClass), TranslateResponseType(ResponseType), pResponse, NumBlocks, BlockSize, pBuffer, TranslateBusReqFlags(Flags));
    if (bSDIO)
    {
        DumpArgument(Argument, Command, TEXT("Wrap"));
    }
    sdRet = SDSynchronousBusRequest(hDevice, Command, Argument, TransferClass, ResponseType, pResponse, NumBlocks, BlockSize, pBuffer, Flags);
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDSynchronousBusRequest returned: %s (0x%x)"),TranslateErrorCodes(sdRet), sdRet);
    if ((!bSDIO) && (Command != SD_CMD_APP_CMD) && (bSkipState == FALSE))
    {
        DebugPrintState(0, SDCARD_ZONE_INFO, TEXT("Wrap: "), hDevice, pCrdStat);
    }
    return sdRet;
}

SD_API_STATUS wrap_SDBusRequest(SD_DEVICE_HANDLE  hDevice, UCHAR Command, DWORD Argument, SD_TRANSFER_CLASS TransferClass,
                    SD_RESPONSE_TYPE ResponseType, ULONG NumBlocks, ULONG BlockSize, PUCHAR pBuffer, PSD_BUS_REQUEST_CALLBACK pCallback,
                                  DWORD RequestParam, PSD_BUS_REQUEST *ppRequest, DWORD Flags, BOOL bAppCmd)
{
    BOOL bSDIO = FALSE;
    SD_API_STATUS sdRet;
    SDIO_CARD_INFO     sdioci;
    bSDIO = SD_API_SUCCESS(SDCardInfoQuery(hDevice, SD_INFO_SDIO, &sdioci, sizeof(SDIO_CARD_INFO)));

    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: sdReturn = SDBusRequest(0x%x,  %s, %u, %s, %s, %u, %u, 0x%x, 0x%x, 0x%x, 0x%x, %s )"),hDevice, TranslateCommandCodes(Command, bAppCmd), Argument, TranslateTransferClass(TransferClass), TranslateResponseType(ResponseType), NumBlocks, BlockSize, pBuffer, pCallback, RequestParam, ppRequest, TranslateBusReqFlags(Flags));
    if (bSDIO)
    {
        DumpArgument(Argument, Command, TEXT("Wrap"));
    }
    sdRet = SDBusRequest(hDevice, Command, Argument, TransferClass, ResponseType, NumBlocks, BlockSize, pBuffer, pCallback, RequestParam, ppRequest, Flags);
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDBusRequest returned: %s (0x%x)"),TranslateErrorCodes(sdRet), sdRet);
    return sdRet;
}

VOID wrap_SDFreeBusRequest(PSD_BUS_REQUEST  pRequest)
{
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDFreeBusRequest(0x%x)"), pRequest);
    SDFreeBusRequest(pRequest);
}

BOOLEAN wrap_SDCancelBusRequest(PSD_BUS_REQUEST  pRequest)
{
    BOOLEAN bRet;
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: bRet = SDCancelBusRequest(0x%x)"),pRequest);
    bRet = SDCancelBusRequest(pRequest);
    if (bRet) LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDCancelBusRequest returned: TRUE (%u)"), bRet);
    else LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDCancelBusRequest returned: FALSE (%u)"), bRet);
    return bRet;
}

//2 SDIO Only

SD_API_STATUS wrap_SDIOConnectInterrupt(SD_DEVICE_HANDLE hDevice, PSD_INTERRUPT_CALLBACK pIsrFunction)
{
    SD_API_STATUS sdRet;
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: sdReturn = SDIOConnectInterrupt(0x%x, 0x%x )"),hDevice, pIsrFunction);
    sdRet = SDIOConnectInterrupt(hDevice, pIsrFunction);
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDIOConnectInterrupt returned: %s (0x%x)"),TranslateErrorCodes(sdRet), sdRet);
    return sdRet;
}

void wrap_SDIODisconnectInterrupt(SD_DEVICE_HANDLE hDevice)
{
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDIODisconnectInterrupt(0x%x)"), hDevice);
    SDIODisconnectInterrupt(hDevice);
}

SD_API_STATUS wrap_SDGetTuple(SD_DEVICE_HANDLE hDevice, UCHAR TupleCode, PUCHAR pBuffer, PULONG pBufferSize, BOOL CommonCIS)
{
    SD_API_STATUS sdRet;
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: sdReturn = SDGetTuple(0x%x, %s, 0x%x, 0x%x, %s )"),hDevice, TranslateTupleCode(TupleCode), pBuffer, pBufferSize, TransBOOLTF(CommonCIS));
    sdRet = SDGetTuple(hDevice, TupleCode, pBuffer, pBufferSize, CommonCIS);
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDGetTuple returned: %s (0x%x)"),TranslateErrorCodes(sdRet), sdRet);
    return sdRet;
}

SD_API_STATUS wrap_SDSetCardFeature(SD_DEVICE_HANDLE hDevice, SD_SET_FEATURE_TYPE CardFeature, PVOID pCardInfo, ULONG StructureSize)
{
    SD_API_STATUS sdRet;
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: sdReturn = SDSetCardFeature(0x%x, %s, 0x%x, %u)"),hDevice, TranslateCardFeature(CardFeature), pCardInfo, StructureSize);
    sdRet = SDSetCardFeature(hDevice, CardFeature, pCardInfo, StructureSize);
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDSetCardFeature returned: %s (0x%x)"),TranslateErrorCodes(sdRet), sdRet);
    return sdRet;
}

SD_API_STATUS wrap_SDReadWriteRegistersDirect(SD_DEVICE_HANDLE hDevice, SD_IO_TRANSFER_TYPE ReadWrite, UCHAR Function, DWORD Address, BOOLEAN ReadAfterWrite, PUCHAR pBuffer, ULONG BufferLength)
{
    SD_API_STATUS sdRet;
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: sdReturn = SDReadWriteRegistersDirect(0x%x, %s, %u, %u, %s, 0x%x, %u)"),hDevice, TranslateIOTransferType(ReadWrite), Function, Address, TransBOOLTF(ReadAfterWrite), pBuffer, BufferLength);
    sdRet = SDReadWriteRegistersDirect(hDevice, ReadWrite, Function, Address, ReadAfterWrite, pBuffer, BufferLength);
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDReadWriteRegistersDirect returned: %s (0x%x)"),TranslateErrorCodes(sdRet), sdRet);
    return sdRet;
}

/************************************************************************************************/
/*                                                                                                */
/*       Note: The following APIs are not strictly speaking client APIs, but are in SDCardLib.lib                        */
/*                                                                                                */
/************************************************************************************************/
DWORD wrap_GetBitSlice(UCHAR const *pBuffer, ULONG BufferSize, DWORD BitOffset, UCHAR NumberOfBits)
{
    DWORD dwBitSlice = 0;
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: dwBitSlice = GetBitSlice(0x%x, %u, %u ,%u)"), pBuffer, BufferSize, BitOffset, NumberOfBits);
    dwBitSlice = GetBitSlice(pBuffer, BufferSize, BitOffset, NumberOfBits);
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: dwBitSlice=  (0x%x); from pBuffer (as hex string) = %s"), dwBitSlice, GenerateHexString(pBuffer, BufferSize));
    return dwBitSlice;
}

void wrap_SDOutputBuffer(PVOID pBuffer, ULONG BufferSize)
{
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDOutputBuffer(0x%x, %u)"), pBuffer, BufferSize);
    SDOutputBuffer(pBuffer, BufferSize);
}

SD_MEMORY_LIST_HANDLE wrap_SDCreateMemoryList(ULONG Tag, ULONG Depth, ULONG EntrySize)
{
    SD_MEMORY_LIST_HANDLE hMemList;
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: hMemList = SDCreateMemoryList(%u, %u, %u)"),Tag, Depth, EntrySize);
    hMemList = SDCreateMemoryList(Tag, Depth, EntrySize);
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDCreateMemoryList returned: 0x%x"), hMemList);
    return hMemList;
}

void wrap_SDDeleteMemList(SD_MEMORY_LIST_HANDLE hList)
{
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDDeleteMemList(0x%x)"), hList);
    SDDeleteMemList(hList);
}

void wrap_SDFreeToMemList(SD_MEMORY_LIST_HANDLE hList, PVOID pMemory)
{
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDFreeToMemList(0x%x, 0x%x)"), hList, pMemory);
    SDFreeToMemList(hList, pMemory);
}

PVOID wrap_SDAllocateFromMemList(SD_MEMORY_LIST_HANDLE hList)
{
    PVOID pVoid;
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: pVoid = SDAllocateFromMemList(0x%x)"), hList);
    pVoid = SDAllocateFromMemList(hList);
    LogDebug(SDIO_ZONE_WRAP, TEXT("Wrap: SDAllocateFromMemList returned: 0x%x"), pVoid);
    return pVoid;
}

void DumpStatus(UINT indent, SD_CARD_STATUS cardStatus, DWORD st)
{
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT(" "));
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT("Status"));
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT("********************************************************************************************************"));

    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT(" "));
    if (st == STAT_NORMAL)
    {
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_OUT_OF_RANGE"),YesNo(cardStatus, SD_STATUS_OUT_OF_RANGE));
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_ADDRESS_ERROR"),YesNo(cardStatus, SD_STATUS_ADDRESS_ERROR));
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_BLOCK_LEN_ERROR"),YesNo(cardStatus, SD_STATUS_BLOCK_LEN_ERROR));
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_ERASE_SEQ_ERROR"),YesNo(cardStatus, SD_STATUS_ERASE_SEQ_ERROR));
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_ERASE_PARAM"),YesNo(cardStatus, SD_STATUS_ERASE_PARAM));
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_WP_VIOLATION"),YesNo(cardStatus, SD_STATUS_WP_VIOLATION));
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_CARD_IS_LOCKED"),YesNo(cardStatus, SD_STATUS_CARD_IS_LOCKED));
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_LOCK_UNLOCK_FAILED"),YesNo(cardStatus, SD_STATUS_LOCK_UNLOCK_FAILED));
    }
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_COM_CRC_ERROR"),YesNo(cardStatus, SD_STATUS_COM_CRC_ERROR));
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_ILLEGAL_COMMAND"),YesNo(cardStatus, SD_STATUS_ILLEGAL_COMMAND));
    if (st == STAT_NORMAL)
    {
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_CARD_ECC_FAILED"),YesNo(cardStatus, SD_STATUS_CARD_ECC_FAILED));
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_CC_ERROR"),YesNo(cardStatus, SD_STATUS_CC_ERROR));
    }
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_ERROR"),YesNo(cardStatus, SD_STATUS_ERROR));
    if (st == STAT_NORMAL)
    {
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_CID_CSD_OVERWRITE"),YesNo(cardStatus, SD_STATUS_CID_CSD_OVERWRITE));
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_WP_ERASE_SKIP"),YesNo(cardStatus, SD_STATUS_WP_ERASE_SKIP));
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_CARD_ECC_DISABLED"),YesNo(cardStatus, SD_STATUS_CARD_ECC_DISABLED));
        LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_ERASE_RESET"),YesNo(cardStatus, SD_STATUS_ERASE_RESET));
    }
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT(" "));
    switch (SD_STATUS_CURRENT_STATE(cardStatus))
    {
        case SD_STATUS_CURRENT_STATE_IDLE:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("Current State"), TEXT("IDLE"));
            break;
        case SD_STATUS_CURRENT_STATE_READY:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("Current State"), TEXT("READY"));
            break;
        case SD_STATUS_CURRENT_STATE_IDENT:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("Current State"), TEXT("IDENTIFICATION"));
            break;
        case SD_STATUS_CURRENT_STATE_STDBY:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("Current State"), TEXT("STANDBY"));
            break;
        case SD_STATUS_CURRENT_STATE_TRAN:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("Current State"), TEXT("TRANSFER"));
            break;
        case SD_STATUS_CURRENT_STATE_DATA:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("Current State"), TEXT("SENDING DATA"));
            break;
        case SD_STATUS_CURRENT_STATE_RCV:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("Current State"), TEXT("RECEIVING DATA"));
            break;
        case SD_STATUS_CURRENT_STATE_PRG:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("Current State"), TEXT("PROGRAMING"));
            break;
        case SD_STATUS_CURRENT_STATE_DIS:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("Current State"), TEXT("DISCONECT"));
            break;
        default:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("Current State"), TEXT("BAD_STATE"));
    }
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT(" "));
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_READY_FOR_DATA"),YesNo(cardStatus, SD_STATUS_READY_FOR_DATA));
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_APP_CMD"),YesNo(cardStatus, SD_STATUS_APP_CMD));
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-32s%s"), TEXT("SD_STATUS_AKE_SEQ_ERROR"),YesNo(cardStatus, SD_STATUS_AKE_SEQ_ERROR));
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT(" "));

    LogDebug(indent, SDCARD_ZONE_INFO, TEXT("********************************************************************************************************"));
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT(" "));
}

void DumpIOStatus(UINT indent, SD_IOCARD_STATUS ioCardStatus)
{
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT(" "));
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT("Status"));
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT("********************************************************************************************************"));

    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT(" "));

    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-25s%s"), TEXT("SD_IO_COM_CRC_ERROR"),YesNo(ioCardStatus, SD_IO_COM_CRC_ERROR));
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-25s%s"), TEXT("SD_IO_ILLEGAL_COMMAND"),YesNo(ioCardStatus, SD_IO_ILLEGAL_COMMAND));
    switch (SD_IO_STATUS_CURRENT_STATE(ioCardStatus))
    {
        case SD_IO_STATUS_CURRENT_STATE_DIS:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("Current State"), TEXT("DISABLED"));
            break;
        case SD_IO_STATUS_CURRENT_STATE_CMD:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("Current State"), TEXT("DAT LINES FREE"));
            break;
        case SD_IO_STATUS_CURRENT_STATE_TRAN:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("Current State"), TEXT("TRANSFER"));
            break;
        case SD_IO_STATUS_CURRENT_STATE_RFU:
            LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("Current State"), TEXT("RESERVED"));
            break;
    }
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-25s%s"), TEXT("SD_IO_GENERAL_ERROR"),YesNo(ioCardStatus, SD_IO_GENERAL_ERROR));
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-25s%s"), TEXT("SD_IO_INVALID_FUNCTION"),YesNo(ioCardStatus, SD_IO_INVALID_FUNCTION));
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-25s%s"), TEXT("SD_IO_ARG_OUT_OF_RANGE"),YesNo(ioCardStatus, SD_IO_ARG_OUT_OF_RANGE));

    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT(" "));

    LogDebug(indent, SDCARD_ZONE_INFO, TEXT("********************************************************************************************************"));
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT(" "));
}

void DumpStatusRegister(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTP)
{
    SD_CARD_STATUS      cardStatus;
    SD_API_STATUS       status;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("DmpStatFromReg: Dumping the Status Register..."));

    status = wrap_SDCardInfoQuery(hDevice, SD_INFO_CARD_STATUS, &cardStatus, sizeof(SD_CARD_STATUS));
    if (!SD_API_SUCCESS(status))
    {
        LogFail(indent +1,TEXT("DmpStatFromReg: Unable to get status register from card. ErrorCode:%s (0x%x) returned."),TranslateErrorCodes(status), status);
        if (IsBufferEmpty(pTP))
        {
            SetMessageBufferAndResult(pTP, TPR_FAIL,TEXT("SDCardInfoQuery failed so the contents of the status register could not be obtained. Error %s (0x%x) returned."), TranslateErrorCodes(status), status);
        }
    }
    else
    {
        DumpStatus(indent + 1, cardStatus, STAT_NORMAL);
    }
}

void DumpStatusFromResponse(UINT indent, PSD_COMMAND_RESPONSE psdResp)
{
    SD_CARD_STATUS         cardStatus = 0;
    SD_IOCARD_STATUS        ioCardStatus = 0;
    DWORD                     statType = STAT_NORMAL;
    BYTE                    bTemp = 0;
    BYTE                    bTemp1 = 0;

    ZeroMemory(&cardStatus, sizeof(cardStatus));

    if (psdResp == NULL)
    {
        LogDebug(indent, SDIO_ZONE_TST, TEXT("DmpStatFromResp: No response passed in. Bailing..."));
    }
    else
    {
        LogDebug(indent, SDIO_ZONE_TST, TEXT("DmpStatFromResp: Dumping Status info from the response..."));
        switch (psdResp->ResponseType)
        {
            case NoResponse:
                LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("DmpStatFromResp: \"Response: NoResponse\" %u has no status info"), psdResp->ResponseType);
                break;
            case ResponseR1:
                __fallthrough;
            case ResponseR1b:

//                cardStatus = (psdResp->ResponseBuffer[1]  | (psdResp->ResponseBuffer[2] << 8)  |  (psdResp->ResponseBuffer[3] << 16)  |  (psdResp->ResponseBuffer[4]) <<24);
                SDGetCardStatusFromResponse(psdResp, &cardStatus);
                break;
            case ResponseR2:
                LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("DmpStatFromResp: \"Response: NoResponse\" %u has no status info"), psdResp->ResponseType);
                break;
            case ResponseR3:
                LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("DmpStatFromResp: \"Response: NoResponse\" %u has no status info"), psdResp->ResponseType);
                break;
            case ResponseR4:
                LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("DmpStatFromResp: \"Response: NoResponse\" %u has no status info"), psdResp->ResponseType);
                break;
            case ResponseR5:
                ioCardStatus = psdResp->ResponseBuffer[2];
                break;
            case ResponseR6:
                if (psdResp->ResponseBuffer[2] & 1<<7)
                {
                    bTemp1 |= 1<<7;
                }
                if (psdResp->ResponseBuffer[2] & 1<<6)
                {
                    bTemp1 |= 1<<6;
                }
                if (psdResp->ResponseBuffer[2] & 1<<5)
                {
                    bTemp1 |= 1<<3;
                }
                bTemp = psdResp->ResponseBuffer[2] & 0x1F;

                cardStatus = psdResp->ResponseBuffer[1]  | bTemp <<8 | bTemp1 <<16;
                statType = STAT_ABREV;
                break;
            default:
                LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("DmpStatFromResp: Undefined Response %u"), psdResp->ResponseType);
        }
        if (!cardStatus)
        {
            if (ioCardStatus)
            {
                DumpIOStatus(indent + 1, ioCardStatus);
            }
        }
        else
        {
            DumpStatus(indent + 1, cardStatus, statType);
        }
    }

}

void DumpAltStatus(UINT indent, PSD_PARSED_REGISTER_ALT_STATUS pAltStat)
{
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT(" "));
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT("Alternate Status"));
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT("********************************************************************************************************"));

    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT(" "));

    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-30s%u"),TEXT("Bus Width"), pAltStat->ucBusWidth);
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-30s%s"),TEXT("Secured Mode"),YesNo(pAltStat->bSecured));
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-30s%u"),TEXT("Card Type"),pAltStat->usCardType);
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT("%-30s%u"),TEXT("Protected Area Size (blocks)"),pAltStat->usCardType);
    LogDebug(indent + 1, SDCARD_ZONE_INFO, TEXT(" "));
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT("********************************************************************************************************"));
    LogDebug(indent, SDCARD_ZONE_INFO, TEXT(" "));
}

void DumpArgument(DWORD dwArg, UCHAR ucCmdCode, LPCTSTR txt)
{
    ArgType Arg = {0};
    if ((ucCmdCode == SD_CMD_IO_RW_DIRECT ) || (ucCmdCode == SD_CMD_IO_RW_EXTENDED) )
    {
        Arg.RWFLag = (UCHAR) IO_RW_DIRECT_ARG_RW(dwArg);
        Arg.FunctionNumber = (UCHAR) IO_RW_DIRECT_ARG_FUNC(dwArg);
        Arg.RegisterAddress = IO_RW_DIRECT_ARG_ADDR(dwArg);
        if (ucCmdCode == SD_CMD_IO_RW_DIRECT)
        {
            Arg.Mode = (UCHAR) IO_RW_DIRECT_ARG_RAW(dwArg);
            Arg.WriteData = (UCHAR) IO_RW_DIRECT_ARG_DATA(dwArg);
        }
        else
        {
            Arg.Mode = (UCHAR) IO_RW_EXTENDED_ARG_BLOCK_MODE(dwArg);
            Arg.OPCode = (UCHAR) IO_RW_EXTENDED_ARG_OPCODE(dwArg);
            Arg.Count = (USHORT) IO_RW_EXTENDED_ARG_COUNT(dwArg);
        }
        //2 Whole value
        LogDebug((UINT)1, SDCARD_ZONE_INFO, TEXT("%s: Argument = %u"), txt, dwArg);
        //2RWFlag (same for both)
        if (Arg.RWFLag) LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: RWFlag = SD_IO_OP_WRITE"), txt);
        else LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: RWFlag = SD_IO_OP_READ"), txt);
        //2 FunctionNumber (same for both)
        LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: FunctionNumber = %u"), txt, Arg.FunctionNumber);
        //2 RAW (CMD52)/BlockMode (CMD53)
        if (ucCmdCode == SD_CMD_IO_RW_DIRECT)
        {
            if (Arg.Mode) LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: RAW = SD_IO_RW_RAW"), txt);
            else LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: RAW = SD_IO_RW_NORMAL"), txt);
        }
        else
        {
            if (Arg.Mode) LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: BlockMode = SD_IO_BLOCK_MODE"), txt);
            else LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: BlockMode = SD_IO_BYTE_MODE"), txt);
        }
        //2 OPCode(CMD53) [Stuff bit (CMD52)]
        if (ucCmdCode == SD_CMD_IO_RW_EXTENDED)
        {
            if (Arg.OPCode) LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: OPCode = SD_IO_INCREMENT_ADDRESS"), txt);
            else LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: OPCode = SD_IO_FIXED_ADDRESS"), txt);
        }
        //2RegisterAddress (same for both)
        LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: RegisterAddress = 0x%05x"), txt, Arg.RegisterAddress);
        //2Data[or Stuff bits](CMD52)/Count(CMD53)
        if (ucCmdCode == SD_CMD_IO_RW_DIRECT)
        {
            if (Arg.RWFLag == SD_IO_OP_WRITE)
            {
                LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: WriteData = 0x%02x"), txt, Arg.WriteData);
            }
        }
        else
        {
            if (Arg.Mode == SD_IO_BLOCK_MODE) LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: Count = %u blocks"), txt, Arg.Count);
            else LogDebug((UINT)2, SDCARD_ZONE_INFO, TEXT("%s: Count = %u bytes"), txt, Arg.Count);
        }
    }
}