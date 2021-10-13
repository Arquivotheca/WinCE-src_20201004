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
#include <tchar.h>
#include <sdcard.h>
#include <sdcardddk.h>
#include "sdtst.h"
#include <sd_tst_cmn.h>

LPTSTR lptStrUnknownCode= TEXT("Unknown Code");
LPTSTR lptStrUnknownStatusCode= TEXT("Unknown Status Code");
LPTSTR lptStrUnknownState= TEXT("Unknown State");
LPTSTR lptStrYes = TEXT("Yes");
LPTSTR lptStrNo = TEXT("No");
LPTSTR lptStrCustomTple = TEXT("CUSTOM_TUPLE");

// ------------------------------------------
// Command Code Translation
// ------------------------------------------
struct CMDCodeLookup
{
    UCHAR Code;
    LPTSTR str;
    LPTSTR appstr;
};

CMDCodeLookup cclTable[] =
{
    {SD_CMD_GO_IDLE_STATE, TEXT("SD_CMD_GO_IDLE_STATE"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_MMC_SEND_OPCOND, TEXT("SD_CMD_MMC_SEND_OPCOND"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_ALL_SEND_CID, TEXT("SD_CMD_ALL_SEND_CID"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_SEND_RELATIVE_ADDR, TEXT("SD_CMD_SEND_RELATIVE_ADDR or SD_CMD_MMC_SET_RCA "), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_SET_DSR, TEXT("SD_CMD_SET_DSR"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_IO_OP_COND, TEXT("SD_CMD_IO_OP_COND"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_SELECT_DESELECT_CARD, TEXT("SD_CMD_SELECT_DESELECT_CARD"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_SEND_CSD, TEXT("SD_CMD_SEND_CSD"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_SEND_CID, TEXT("SD_CMD_SEND_CID"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_STOP_TRANSMISSION, TEXT("SD_CMD_STOP_TRANSMISSION"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_SEND_STATUS, TEXT("SD_CMD_SEND_STATUS"), TEXT("SD_ACMD_SD_STATUS")},
    {SD_CMD_GO_INACTIVE_STATE, TEXT("SD_CMD_GO_INACTIVE_STATE"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_SET_BLOCKLEN, TEXT("SD_CMD_SET_BLOCKLEN"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_READ_SINGLE_BLOCK, TEXT("SD_CMD_READ_SINGLE_BLOCK"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_READ_MULTIPLE_BLOCK, TEXT("SD_CMD_READ_MULTIPLE_BLOCK"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_WRITE_BLOCK, TEXT("SD_CMD_WRITE_BLOCK"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_WRITE_MULTIPLE_BLOCK, TEXT("SD_CMD_WRITE_MULTIPLE_BLOCK"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_PROGRAM_CSD, TEXT("SD_CMD_PROGRAM_CSD"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_SET_WRITE_PROT, TEXT("SD_CMD_SET_WRITE_PROT"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_CLR_WRITE_PROT, TEXT("SD_CMD_CLR_WRITE_PROT"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_SEND_WRITE_PROT, TEXT("SD_CMD_SEND_WRITE_PROT"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_ERASE_WR_BLK_START, TEXT("SD_CMD_ERASE_WR_BLK_START"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_ERASE_WR_BLK_END, TEXT("SD_CMD_ERASE_WR_BLK_END"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_ERASE, TEXT("SD_CMD_ERASE"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_LOCK_UNLOCK, TEXT("SD_CMD_LOCK_UNLOCK"), TEXT("SD_ACMD_SET_CLR_CARD_DETECT")},
    {SD_CMD_IO_RW_DIRECT, TEXT("SD_CMD_IO_RW_DIRECT or SD_IO_RW_DIRECT"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_IO_RW_EXTENDED, TEXT("SD_CMD_IO_RW_EXTENDED or SD_IO_RW_EXTENDED"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_APP_CMD, TEXT("SD_CMD_APP_CMD"), TEXT("BAD_APP_CMD_CODE")},
    {SD_CMD_GEN_CMD, TEXT("SD_CMD_GEN_CMD"), TEXT("BAD_APP_CMD_CODE")},
    {SD_ACMD_SET_BUS_WIDTH, TEXT("BAD_NORM_CMD_CODE"), TEXT("SD_ACMD_SET_BUS_WIDTH")},
    {SD_ACMD_SEND_NUM_WR_BLOCKS, TEXT("BAD_NORM_CMD_CODE"), TEXT("SD_ACMD_SEND_NUM_WR_BLOCKS")},
    {SD_ACMD_SET_WR_BLOCK_ERASE_COUNT, TEXT("BAD_NORM_CMD_CODE"), TEXT("SD_ACMD_SET_WR_BLOCK_ERASE_COUNT")},
    {SD_ACMD_SD_SEND_OP_COND, TEXT("BAD_NORM_CMD_CODE"), TEXT("SD_ACMD_SD_SEND_OP_COND")},
    {SD_ACMD_SEND_SCR, TEXT("BAD_NORM_CMD_CODE"), TEXT("SD_ACMD_SEND_SCR")}
};

LPCTSTR TranslateCommandCodes(UCHAR code, BOOL bAppCMD)
{
    int len;
    len = sizeof(cclTable)/ sizeof (CMDCodeLookup);
    int c;
    for (c = 0; (c < len) && (code != cclTable[c].Code); c++);
    if (c == len)
    {
        return lptStrUnknownCode;
    }
    else if(bAppCMD)
    {
        return cclTable[c].appstr;
    }
    return cclTable[c].str;
}

// ------------------------------------------
// Status Translation
// ------------------------------------------
struct StatusLookup
{
    SD_API_STATUS status;
    LPTSTR str;
};

StatusLookup statlTable[] =
{
    {SD_API_STATUS_SUCCESS, TEXT("SD_API_STATUS_SUCCESS")},
    {SD_API_STATUS_PENDING, TEXT("SD_API_STATUS_PENDING")},
    {SD_API_STATUS_FAST_PATH_SUCCESS, TEXT("SD_API_STATUS_FAST_PATH_SUCCESS")},
    {SD_API_STATUS_FAST_PATH_OPT_SUCCESS, TEXT("SD_API_STATUS_FAST_PATH_OPT_SUCCESS")},
    {SD_API_STATUS_BUFFER_OVERFLOW, TEXT("SD_API_STATUS_BUFFER_OVERFLOW")},
    {SD_API_STATUS_DEVICE_BUSY, TEXT("SD_API_STATUS_DEVICE_BUSY")},
    {SD_API_STATUS_UNSUCCESSFUL, TEXT("SD_API_STATUS_UNSUCCESSFUL")},
    {SD_API_STATUS_NOT_IMPLEMENTED, TEXT("SD_API_STATUS_NOT_IMPLEMENTED")},
    {SD_API_STATUS_ACCESS_VIOLATION, TEXT("SD_API_STATUS_ACCESS_VIOLATION")},
    {SD_API_STATUS_INVALID_HANDLE, TEXT("SD_API_STATUS_INVALID_HANDLE")},
    {SD_API_STATUS_INVALID_PARAMETER, TEXT("SD_API_STATUS_INVALID_PARAMETER")},
    {SD_API_STATUS_NO_SUCH_DEVICE, TEXT("SD_API_STATUS_NO_SUCH_DEVICE")},
    {SD_API_STATUS_INVALID_DEVICE_REQUEST, TEXT("SD_API_STATUS_INVALID_DEVICE_REQUEST")},
    {SD_API_STATUS_NO_MEMORY, TEXT("SD_API_STATUS_NO_MEMORY")},
    {SD_API_STATUS_BUS_DRIVER_NOT_READY, TEXT("SD_API_STATUS_BUS_DRIVER_NOT_READY")},
    {SD_API_STATUS_DATA_ERROR, TEXT("SD_API_STATUS_DATA_ERROR")},
    {SD_API_STATUS_CRC_ERROR, TEXT("SD_API_STATUS_CRC_ERROR")},
    {SD_API_STATUS_INSUFFICIENT_RESOURCES, TEXT("SD_API_STATUS_INSUFFICIENT_RESOURCES")},
    {SD_API_STATUS_DEVICE_NOT_CONNECTED, TEXT("SD_API_STATUS_DEVICE_NOT_CONNECTED")},
    {SD_API_STATUS_DEVICE_REMOVED, TEXT("SD_API_STATUS_DEVICE_REMOVED")},
    {SD_API_STATUS_DEVICE_NOT_RESPONDING, TEXT("SD_API_STATUS_DEVICE_NOT_RESPONDING")},
    {SD_API_STATUS_CANCELED, TEXT("SD_API_STATUS_CANCELED")},
    {SD_API_STATUS_RESPONSE_TIMEOUT, TEXT("SD_API_STATUS_RESPONSE_TIMEOUT")},
    {SD_API_STATUS_DATA_TIMEOUT, TEXT("SD_API_STATUS_DATA_TIMEOUT")},
    {SD_API_STATUS_DEVICE_RESPONSE_ERROR, TEXT("SD_API_STATUS_DEVICE_RESPONSE_ERROR")},
    {SD_API_STATUS_DEVICE_UNSUPPORTED, TEXT("SD_API_STATUS_DEVICE_UNSUPPORTED")},
    {SD_API_STATUS_SHUT_DOWN, TEXT("SD_API_STATUS_SHUT_DOWN")},
    {SD_API_STATUS_INSUFFICIENT_HOST_POWER, TEXT("SD_API_STATUS_INSUFFICIENT_HOST_POWER")}
};

LPCTSTR TranslateErrorCodes(SD_API_STATUS stat)
{
    int len;
    len = sizeof(statlTable)/ sizeof (StatusLookup);
    int c;
    for (c = 0; (c < len) && (stat != statlTable[c].status); c++);
    if (c == len)
    {
        return lptStrUnknownStatusCode;
    }
    return statlTable[c].str;
}

// ------------------------------------------
// State Translation
// ------------------------------------------

struct StateLookup
{
    DWORD state;
    LPTSTR str;
};

StateLookup statelTable[] =
{
    {SD_STATUS_CURRENT_STATE_IDLE, TEXT("IDLE")},
    {SD_STATUS_CURRENT_STATE_READY, TEXT("READY")},
    {SD_STATUS_CURRENT_STATE_IDENT, TEXT("IDENTIFICATION")},
    {SD_STATUS_CURRENT_STATE_STDBY, TEXT("STANDBY")},
    {SD_STATUS_CURRENT_STATE_TRAN, TEXT("TRANSFER")},
    {SD_STATUS_CURRENT_STATE_DATA, TEXT("DATA")},
    {SD_STATUS_CURRENT_STATE_RCV, TEXT("RECEIVING")},
    {SD_STATUS_CURRENT_STATE_PRG, TEXT("PROGRAMMING")},
    {SD_STATUS_CURRENT_STATE_DIS, TEXT("DISCONNECTED")}
};

LPCTSTR TranslateState(DWORD dwState)
{
    int len;
    len = sizeof(statelTable)/ sizeof (StateLookup);
    int c;
    for (c = 0; (c < len) && (dwState != statelTable[c].state); c++);
    if (c == len)
    {
        return lptStrUnknownState;
    }
    return statelTable[c].str;
}

LPCTSTR TranslateInfoTypes(SD_INFO_TYPE it)
{
    static TCHAR buff[30];
    ZeroMemory(buff, (sizeof(TCHAR) * 30));

    switch(it)
    {
        case SD_INFO_REGISTER_OCR:
            wcscpy_s(buff, 30, TEXT("SD_INFO_REGISTER_OCR"));
            break;
        case SD_INFO_REGISTER_CID:
            wcscpy_s(buff, 30, TEXT("SD_INFO_REGISTER_CID"));
            break;
        case SD_INFO_REGISTER_CSD:
            wcscpy_s(buff, 30, TEXT("SD_INFO_REGISTER_CSD"));
            break;
        case SD_INFO_REGISTER_RCA:
            wcscpy_s(buff, 30, TEXT("SD_INFO_REGISTER_RCA"));
            break;
        case SD_INFO_REGISTER_IO_OCR:
            wcscpy_s(buff, 30, TEXT("SD_INFO_REGISTER_IO_OCR"));
            break;
        case SD_INFO_CARD_INTERFACE:
            wcscpy_s(buff, 30, TEXT("SD_INFO_CARD_INTERFACE"));
            break;
        case SD_INFO_CARD_STATUS:
            wcscpy_s(buff, 30, TEXT("SD_INFO_CARD_STATUS"));
            break;
        case SD_INFO_SDIO:
            wcscpy_s(buff, 30,TEXT("SD_INFO_SDIO"));
            break;
        case SD_INFO_HOST_IF_CAPABILITIES:
            wcscpy_s(buff, 30, TEXT("SD_INFO_HOST_IF_CAPABILITIES"));
            break;
        case SD_INFO_HOST_BLOCK_CAPABILITY:
            wcscpy_s(buff, 30, TEXT("SD_INFO_HOST_BLOCK_CAPABILITY"));
            break;                
        case SD_INFO_HIGH_CAPACITY_SUPPORT:
            wcscpy_s(buff, 30, TEXT("SD_INFO_HIGH_CAPACITY_SUPPORT"));
            break;
        case SD_INFO_CARD_INTERFACE_EX:
            wcscpy_s(buff, 30, TEXT("SD_INFO_CARD_INTERFACE_EX"));
            break;
        case SD_INFO_SWITCH_FUNCTION:
            wcscpy_s(buff, 30, TEXT("SD_INFO_SWITCH_FUNCTION"));
            break;
        default:
            wcscpy_s(buff, 30, TEXT("UNKNOWN"));
    }
    
    return buff;
}

SD_INTERFACE_MODE GetInterfaceMode(SD_INTERFACE_MODE_EX im)
{
    ASSERT ( im.bit.sd4Bit && im.bit.mmc8Bit);

    if (im.bit.sd4Bit)
       return SD_INTERFACE_SD_4BIT;
    
    if (im.bit.mmc8Bit)
       return SD_INTERFACE_MMC_8BIT;

    return SD_INTERFACE_SD_MMC_1BIT;
}

void SetInterfaceMode(SD_INTERFACE_MODE_EX& im /*set*/, SD_INTERFACE_MODE inMode)
{
    switch (inMode)
    { 
        case SD_INTERFACE_SD_MMC_1BIT:
            im.bit.sd4Bit = 0;
            im.bit.mmc8Bit = 0;
            break;
        case SD_INTERFACE_SD_4BIT:
            im.bit.sd4Bit = 1;
            im.bit.mmc8Bit = 0;
            break;
        case SD_INTERFACE_MMC_8BIT:
            im.bit.sd4Bit = 0;
            im.bit.mmc8Bit = 1;
            break;
        default:
            ASSERT(TRUE);
    }  
}

UINT GetInterfaceValue(SD_INTERFACE_MODE_EX im)
{
    UINT bw = 0;
    switch (GetInterfaceMode(im))
    { 
        case SD_INTERFACE_SD_MMC_1BIT:
            bw = 1;
            break;
        case SD_INTERFACE_SD_4BIT:
            bw = 4;
            break;
        case SD_INTERFACE_MMC_8BIT:
            bw = 8;
            break;
        default:
            ASSERT(TRUE);
    }
    return bw;
}

LPCTSTR TranslateInterfaceMode(SD_INTERFACE_MODE_EX im)
{
    static TCHAR buff[30];
    ZeroMemory(buff, (sizeof(TCHAR) * 30));
    switch(GetInterfaceMode(im))
    {
        case SD_INTERFACE_SD_MMC_1BIT:
            wcscpy_s(buff, 30,  TEXT("SD_INTERFACE_SD_MMC_1BIT"));
            break;
        case SD_INTERFACE_SD_4BIT:
            wcscpy_s(buff, 30,  TEXT("SD_INTERFACE_SD_4BIT"));
            break;
        case SD_INTERFACE_MMC_8BIT:
            wcscpy_s(buff, 30,  TEXT("SD_INTERFACE_MMC_8BIT"));
            break;
        default:
            wcscpy_s(buff, 30,  TEXT("UNKNOWN"));
    }
    return buff;
}


LPCTSTR TranslateTransferClass(SD_TRANSFER_CLASS tc)
{
    static TCHAR buff[11];
    ZeroMemory(buff, (sizeof(TCHAR) * 11));
    switch(tc)
    {
        case  SD_READ:
            wcscpy_s(buff, 11, TEXT("SD_READ"));
            break;
        case  SD_WRITE:
            wcscpy_s(buff, 11, TEXT("SD_WRITE"));
            break;
        case  SD_COMMAND:
            wcscpy_s(buff, 11, TEXT("SD_COMMAND"));
            break;
        default: wcscpy_s(buff, 11, TEXT("UNKNOWN"));

    }
    return buff;
}

LPCTSTR TranslateResponseType(SD_RESPONSE_TYPE rt)
{
    static TCHAR buff[12];
    ZeroMemory(buff, (sizeof(TCHAR) * 12));
    switch(rt)
    {
        case  NoResponse:
            wcscpy_s(buff, 12,  TEXT("NoResponse"));
            break;
        case  ResponseR1:
            wcscpy_s(buff, 12,  TEXT("ResponseR1"));
            break;
        case  ResponseR1b:
            wcscpy_s(buff, 12,  TEXT("ResponseR1b"));
            break;
        case  ResponseR2:
            wcscpy_s(buff, 12,  TEXT("ResponseR2"));
            break;
        case  ResponseR3:
            wcscpy_s(buff, 12,  TEXT("ResponseR3"));
            break;
        case  ResponseR4:
            wcscpy_s(buff, 12,  TEXT("ResponseR4"));
            break;
        case  ResponseR5:
            wcscpy_s(buff, 12,  TEXT("ResponseR5"));
            break;
        case  ResponseR6:
            wcscpy_s(buff, 12,  TEXT("ResponseR6"));
            break;
        case  ResponseR7:
            wcscpy_s(buff, 12,  TEXT("ResponseR7"));
            break;
        default: wcscpy_s(buff, 12,  TEXT("UNKNOWN"));
    }
    return buff;
}

LPCTSTR TranslateDeviceType(SDTST_DEVICE_TYPE devType)
{
    switch (devType) {
        case SDDEVICE_SD:
            return TEXT("SDDEVICE_SD");
        case SDDEVICE_SDHC:
            return TEXT("SDDEVICE_SDHC");
        case SDDEVICE_MMC:
            return TEXT("SDDEVICE_MMC");
        case SDDEVICE_MMCHC:
            return TEXT("SDDEVICE_MMCHC");
        case SDDEVICE_SDIO:
            return TEXT("SDDEVICE_SDIO");
        case SDDEVICE_UKN:
            return TEXT("SDDEVICE_UKN");
        }
    return TEXT("SDDEVICE_UKN");
}

LPCTSTR TranslateBusReqFlags(DWORD dwFlags)
{
    UINT tmpLen = 0;
    static TCHAR buff[50];
    ZeroMemory(buff, (sizeof(TCHAR) * 50));
    wcscpy_s(buff, 50,  TEXT(""));
    if (dwFlags & SD_AUTO_ISSUE_CMD12)
    {
        StringCchLength( buff, 50, &tmpLen );
        if (tmpLen > 0) wcscat_s(buff, 50, TEXT(" | SD_AUTO_ISSUE_CMD12"));
        else     wcscpy_s(buff, 50,  TEXT("SD_AUTO_ISSUE_CMD12"));
    }
    if (dwFlags & SD_SDIO_AUTO_IO_ABORT)
    {
        StringCchLength( buff, 50, &tmpLen );
        if (tmpLen> 0) wcscat_s(buff, 50, TEXT(" | SD_SDIO_AUTO_IO_ABORT"));
        else     wcscpy_s(buff, 50,  TEXT("SD_SDIO_AUTO_IO_ABORT"));
    }
    if (dwFlags == 0)
    {
        wcscpy_s(buff, 50,  TEXT("NO_FLAGS"));
    }
    return buff;
}

LPCTSTR TransState(SD_CARD_STATUS cardStatus)
{
    static TCHAR buff[20];
    ZeroMemory(buff, (sizeof(TCHAR) * 20));
    switch(SD_STATUS_CURRENT_STATE(cardStatus))
    {
        case SD_STATUS_CURRENT_STATE_IDLE:
            wcscpy_s(buff, 20,  TEXT("IDLE"));
            break;
        case SD_STATUS_CURRENT_STATE_READY:
            wcscpy_s(buff, 20,  TEXT("READY"));
            break;
        case SD_STATUS_CURRENT_STATE_IDENT:
            wcscpy_s(buff, 20,  TEXT("IDENTIFICATION"));
            break;
        case SD_STATUS_CURRENT_STATE_STDBY:
            wcscpy_s(buff, 20,  TEXT("STANDBY"));
            break;
        case SD_STATUS_CURRENT_STATE_TRAN:
            wcscpy_s(buff, 20,  TEXT("TRANSFER"));
            break;
        case SD_STATUS_CURRENT_STATE_DATA:
            wcscpy_s(buff, 20,  TEXT("SENDING DATA"));
            break;
        case SD_STATUS_CURRENT_STATE_RCV:
            wcscpy_s(buff, 20,  TEXT("RECEIVING DATA"));
            break;
        case SD_STATUS_CURRENT_STATE_PRG:
            wcscpy_s(buff, 20,  TEXT("PROGRAMING"));
            break;
        case SD_STATUS_CURRENT_STATE_DIS:
            wcscpy_s(buff, 20,  TEXT("DISCONECT"));
            break;
        default:
            wcscpy_s(buff, 20,  TEXT("BAD_STATE"));
    }
    return buff;
}

LPCTSTR YesNo(SD_CARD_STATUS input, DWORD setBit)
{
    if (input & setBit) return lptStrYes;
    else return lptStrNo;
}

LPCTSTR YesNo(SD_IOCARD_STATUS input, UCHAR setBit)
{
    if (input & setBit) return lptStrYes;
    else return lptStrNo;
}

LPCTSTR YesNo(BOOL b)
{
    if (b) return lptStrYes;
    else return lptStrNo;
}

LPCTSTR Supported(BOOL b)
{
    static TCHAR tcMsg[15];
    if (b) wcscpy_s(tcMsg, 15, TEXT("Support"));
    else wcscpy_s(tcMsg, 15, TEXT("Not Supported"));
    return tcMsg;
}

LPCTSTR Supported(UCHAR uc)
{
    static TCHAR tcMsg[15];
    if (uc != 0) wcscpy_s(tcMsg, 15, TEXT("Support"));
    else wcscpy_s(tcMsg, 15, TEXT("Not Supported"));
    return tcMsg;
}

LPCTSTR TransBOOLTF(BOOL b)
{
    static TCHAR tcMsg[6];
    ZeroMemory(tcMsg, 6*2);
    if (b) wcscpy_s(tcMsg, 6, TEXT("TRUE"));
    else wcscpy_s(tcMsg, 6, TEXT("FALSE"));
    return tcMsg;
}

LPCTSTR GetNumberSuffix(UINT i)
{
    static TCHAR suffix[3];
    UINT lastDigit, penultimateDigit;
    div_t div_result;

    div_result = div(((int)i) ,10);
    lastDigit = i - (((UINT)div_result.quot) * 10);
    div_result = div(((int)i) ,100);
    penultimateDigit = ((i - (((UINT)div_result.quot) * 100)) - lastDigit ) / 10;

    if (penultimateDigit != 1)
    {
        switch (lastDigit)
        {
            case 1:
                wcscpy_s(suffix, 3, TEXT("st"));
                break;
            case 2:
                wcscpy_s(suffix, 3, TEXT("nd"));
                break;
            case 3:
                wcscpy_s(suffix, 3, TEXT("rd"));
                break;
            default:
                wcscpy_s(suffix, 3, TEXT("th"));
        }
    }
    else
    {
        wcscpy_s(suffix, 3, TEXT("th"));
    }
    return suffix;
}

BOOL ParseAltStatus(const UCHAR *pBits, PSD_PARSED_REGISTER_ALT_STATUS pAltStat)
{
    BOOL bRet = FALSE;
    int c;
    UCHAR tmp;
    if (pAltStat)
    {
        ZeroMemory(pAltStat, sizeof (SD_PARSED_REGISTER_ALT_STATUS));
        if (pBits)
        {
            //4 Raw bits
            for (c = 0; c < 64; c++)
            {
                pAltStat->RawAltStatusRegister[c] = pBits[c];
            }

            //4 Bus Width
            tmp = pAltStat->RawAltStatusRegister[0] & 0xC0;
            if (tmp == 0x80) pAltStat->ucBusWidth = 4;
            else if (tmp == 0x40) pAltStat->ucBusWidth = 1;
            else pAltStat->ucBusWidth = (UCHAR)-1;

            //4 Secured
            if (pAltStat->RawAltStatusRegister[0] & 0x20) pAltStat->bSecured = TRUE;
            else pAltStat->bSecured = FALSE;

            //4 Card Type
            pAltStat->usCardType = ((pAltStat->RawAltStatusRegister[2]) << 8) + pAltStat->RawAltStatusRegister[3];

            //4 Protected Area Size in Blocks
            pAltStat->dwProtectedAreaSizeInBlocks = ((pAltStat->RawAltStatusRegister[4]) << 24) + ((pAltStat->RawAltStatusRegister[5]) << 16) + ((pAltStat->RawAltStatusRegister[6] )<< 8) + pAltStat->RawAltStatusRegister[7];

            bRet = TRUE;
        }
    }
    return bRet;
}

// ------------------------------------------
// Tuple Code Translation
// ------------------------------------------
struct TpleLookup
{
    DWORD dwTupleCode;
    LPTSTR str;
};

TpleLookup tpleTable[] =
{
    {SD_CISTPL_NULL, TEXT("SD_CISTPL_NULL")},
    {SD_CISTPL_CHECKSUM, TEXT("SD_CISTPL_CHECKSUM")},
    {SD_CISTPL_VERS_1, TEXT("SD_CISTPL_VERS_1")},
    {SD_CISTPL_MANFID, TEXT("SD_CISTPL_MANFID")},
    {SD_CISTPL_FUNCID, TEXT("SD_CISTPL_FUNCID")},
    {SD_CISTPL_FUNCE, TEXT("SD_CISTPL_FUNCE")},
    {SD_CISTPL_SDIO_STD, TEXT("SD_CISTPL_SDIO_STD")},
    {SD_CISTPL_SDIO_EXT, TEXT("SD_CISTPL_SDIO_EXT")},
    {SD_CISTPL_END, TEXT("SD_CISTPL_END")},
    {SD_TUPLE_LINK_END, TEXT("SD_TUPLE_LINK_END")}
};
LPCTSTR TranslateTupleCode(DWORD dwCode)
{
    int len;
    len = sizeof(tpleTable)/ sizeof (TpleLookup);
    int c;
    for (c = 0; (c < len) && (dwCode != tpleTable[c].dwTupleCode); c++);
    if (c == len)
    {
        return lptStrCustomTple;
    }
    return tpleTable[c].str;
}

LPCTSTR TranslateCardFeature(SD_SET_FEATURE_TYPE ft)
{
    static TCHAR buff[35];
    ZeroMemory(buff, 2*35);
    switch(ft)
    {
        case SD_IO_FUNCTION_ENABLE:
            wcscpy_s(buff, 35,  TEXT("SD_IO_FUNCTION_ENABLE"));
            break;
        case SD_IO_FUNCTION_DISABLE:
            wcscpy_s(buff, 35,  TEXT("SD_IO_FUNCTION_DISABLE"));
            break;
        case SD_IO_FUNCTION_SET_BLOCK_SIZE:
            wcscpy_s(buff, 35,  TEXT("SD_IO_FUNCTION_SET_BLOCK_SIZE"));
            break;
        case SD_SET_DATA_TRANSFER_CLOCKS:
            wcscpy_s(buff, 35,  TEXT("SD_SET_DATA_TRANSFER_CLOCKS"));
            break;
        case SD_SET_CLOCK_STATE_DURING_IDLE:
            wcscpy_s(buff, 35,  TEXT("SD_SET_CLOCK_STATE_DURING_IDLE"));
            break;
        case SD_IS_SOFT_BLOCK_AVAILABLE:
            wcscpy_s(buff, 35,  TEXT("SD_IS_SOFT_BLOCK_AVAILABLE"));
            break;
        case SD_SOFT_BLOCK_FORCE_UTILIZATION:
            wcscpy_s(buff, 35,  TEXT("SD_SOFT_BLOCK_FORCE_UTILIZATION"));
            break;
        case SD_SOFT_BLOCK_DEFAULT_UTILIZATON:
            wcscpy_s(buff, 35,  TEXT("SD_SOFT_BLOCK_DEFAULT_UTILIZATON"));
            break;
        case SD_CARD_SELECT_REQUEST:
            wcscpy_s(buff, 35,  TEXT("SD_CARD_SELECT_REQUEST"));
            break;
        case SD_CARD_DESELECT_REQUEST:
            wcscpy_s(buff, 35,  TEXT("SD_CARD_DESELECT_REQUEST"));
            break;
        case SD_CARD_FORCE_RESET:
            wcscpy_s(buff, 35,  TEXT("SD_CARD_FORCE_RESET"));
            break;            
        case SD_IS_FAST_PATH_AVAILABLE:
            wcscpy_s(buff, 35,    TEXT("SD_IS_FAST_PATH_AVAILABLE"));
            break;
        case SD_FAST_PATH_DISABLE:
            wcscpy_s(buff, 35,    TEXT("SD_FAST_PATH_DISABLE"));
            break;
        case SD_FAST_PATH_ENABLE:
            wcscpy_s(buff, 35,    TEXT("SD_FAST_PATH_ENABLE"));
            break;
        case SD_IO_FUNCTION_HIGH_POWER:
            wcscpy_s(buff, 35,    TEXT("SD_IO_FUNCTION_HIGH_POWER"));
            break;
        case SD_IO_FUNCTION_LOW_POWER:
            wcscpy_s(buff, 35,    TEXT("SD_IO_FUNCTION_LOW_POWER"));
            break;            
        case SD_INFO_POWER_CONTROL_STATE:
            wcscpy_s(buff, 35,    TEXT("SD_INFO_POWER_CONTROL_STATE"));
            break;
        case SD_SET_CARD_INTERFACE:
            wcscpy_s(buff, 35,  TEXT("SD_SET_CARD_INTERFACE"));
            break;
        case SD_SET_CARD_INTERFACE_EX:
            wcscpy_s(buff, 35,  TEXT("SD_SET_CARD_INTERFACE_EX"));
            break;
        case SD_SET_SWITCH_FUNCTION:
            wcscpy_s(buff, 35,  TEXT("SD_SET_SWITCH_FUNCTION"));
            break;
        case SD_DMA_ALLOC_PHYS_MEM:
            wcscpy_s(buff, 35,  TEXT("SD_DMA_ALLOC_PHYS_MEM"));
            break;
        case SD_DMA_FREE_PHYS_MEM:
            wcscpy_s(buff, 35,  TEXT("SD_DMA_FREE_PHYS_MEM"));
            break;
        default:
            wcscpy_s(buff, 35,  TEXT("UNKNOWN_CARD_FEATURE"));
    }
    return buff;
}

LPCTSTR TranslateIOTransferType(SD_IO_TRANSFER_TYPE tt)
{
    static TCHAR buff[15];
    ZeroMemory(buff, 2*15);
    switch(tt)
    {
        case SD_IO_READ:
            wcscpy_s(buff, 15,  TEXT("SD_IO_READ"));
            break;
        case SD_IO_WRITE:
            wcscpy_s(buff, 15,  TEXT("SD_IO_WRITE"));
            break;
        default:
            wcscpy_s(buff, 15,  TEXT("SD_IO_UKNWN"));
    }
    return buff;
}

