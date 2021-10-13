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
#include "ciq_hlp.h"
#include "br_hlp.h"
#include <SDCardDDK.h>
#include <sdcard.h>
#include <sd_tst_cmn.h>
#include <sddrv.h>
#include <sddetect.h>

void SDInfoQueryTest(UINT indent, PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO pClientInfo)
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
    SD_CARD_STATUS                crdstat = 0;
    SD_API_STATUS                 rStat = 0;
    PSD_PARSED_REGISTER_CID    pCID = NULL;
    PSD_PARSED_REGISTER_CSD    pCSD = NULL;
    SD_CARD_RCA                RCA = 0;
    PSD_HOST_BLOCK_CAPABILITY    pHBC = NULL;
    PSD_CARD_INTERFACE_EX            pCI = NULL;
    PSD_CARD_INTERFACE_EX            pCIComp = NULL;
    PDWORD                        pOCR = NULL;
    ULONG                         structSize = 0;
    PVOID                        pCardInfo = NULL;
    TCHAR                        txt[20];
    BOOL                         bFail = FALSE;
    PUCHAR                        pCBUFF = NULL;
    UCHAR                         R1 = 0, R2 = 0, M = 0;
    USHORT                        Y = 0;
    PSD_COMMAND_RESPONSE        pResp=NULL;
    DOUBLE                         dTmp = 0.0;
    ULONG                      ulTmp = 0;
    ULONGLONG                  ullTmp = 0;
    float                            ufTmp = 0.0;
    USHORT                        usTmp = 0;
    LPCTSTR                        lptstrRaw = NULL;
    LPCTSTR                        lptstrParsed = NULL;
    BOOL                        bOKState = FALSE;
    BOOL                        bPreErr = FALSE;

    ZeroMemory(txt, sizeof(txt));
    LogDebug(indent, SDIO_ZONE_TST, TEXT("QIT: Entering Driver Test Function..."));

//1 Good State Check
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent2, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent2, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state initially. Since it is not, it is likely that a previous test was unable to return the card to the Transfer State."), TRUE, TRUE);
    if (bOKState == FALSE)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("QIT: Bailing..."));
        goto DONE;
    }

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Preparing to query card on info type: %s (%u)..."), TranslateInfoTypes(pTestParams->iT), pTestParams->iT);
//1 Pre-Bus Request
    switch(pTestParams->iT)
    {
        case SD_INFO_REGISTER_OCR:
/*            //2 Malloc Struct Memory
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Allocating memory for the OCR register info..."));
            pOCR = (PDWORD)malloc(sizeof(DWORD));
            if (pOCR == NULL)
            {
                LogFail(indent2, TEXT("QIT: Not enough memory to get cached OCR register info from bus. Bailing...."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to get the SD Card's OCR register.\n This is because the malloc to store the info failed, probably due to a lack of memory."));
                goto DONE;
            }
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Zeroing the OCR register info buffer..."));
            ZeroMemory(pOCR, SD_OCR_REGISTER_SIZE);
            //2 Set Params
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Setting params for the SDCardQueryInfo..."));
            structSize = SD_OCR_REGISTER_SIZE;
            pCardInfo = pOCR;
            break;*/
            goto SKIP;
        case SD_INFO_REGISTER_CID:
            //2 Malloc Struct Memory
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Allocating memory for the CID register info..."));
            pCID = (PSD_PARSED_REGISTER_CID)malloc(sizeof(SD_PARSED_REGISTER_CID));
            if (pCID == NULL)
            {
                LogFail(indent2, TEXT("QIT: Not enough memory to get cached CID register info from bus. Bailing...."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to get the SD Card's CID register.\n This is because the malloc to store the info failed, probably due to a lack of memory."));
                goto DONE;
            }
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Zeroing the CID register info buffer..."));
            ZeroMemory(pCID, sizeof(SD_PARSED_REGISTER_CID));
            //2 Set Params
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Setting params for the SDCardQueryInfo..."));
            structSize = sizeof(SD_PARSED_REGISTER_CID);
            pCardInfo = pCID;
            break;
        case SD_INFO_REGISTER_CSD:
            //2 Malloc Struct Memory
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Allocating memory for the CSD register info..."));
            pCSD = (PSD_PARSED_REGISTER_CSD)malloc(sizeof(SD_PARSED_REGISTER_CSD));
            if (pCSD == NULL)
            {
                LogFail(indent2, TEXT("QIT: Not enough memory to get cached CSD register info from bus. Bailing...."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to get the SD Card's CSD register.\n This is because the malloc to store the info failed, probably due to a lack of memory."));
                goto DONE;
            }
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Zeroing the CSD register info buffer..."));
            ZeroMemory(pCSD, sizeof(SD_PARSED_REGISTER_CSD));
            //2 Set Params
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Setting params for the SDCardQueryInfo..."));
            structSize = sizeof(SD_PARSED_REGISTER_CSD);
            pCardInfo = pCSD;
            break;
        case SD_INFO_REGISTER_RCA:
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Zeroing the RCA register info buffer..."));
            ZeroMemory(&RCA, sizeof(SD_CARD_RCA));
            //2 Set Params
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Setting params for the SDCardQueryInfo..."));
            structSize = sizeof(SD_CARD_RCA);
            pCardInfo = &RCA;
            break;
        case SD_INFO_CARD_INTERFACE_EX:
            StringCchCopy(txt, _countof(txt), TEXT("Card"));
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Allocating memory for the Card Interface info..."));
            __fallthrough;
        case SD_INFO_HOST_IF_CAPABILITIES:
            if (txt[0] == 0)
            {
                StringCchCopy(txt, _countof(txt), TEXT("Host"));
                LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Allocating memory for the Host Interface info..."));
            }
            //2 Malloc Struct Memory
            pCI = (PSD_CARD_INTERFACE_EX)malloc(sizeof(SD_CARD_INTERFACE_EX));
            if (pCI == NULL)
            {
                LogFail(indent2, TEXT("QIT: Not enough memory to get cached %s interface info from bus. Bailing...."), txt);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to get cached %s interface info from bus.\n This is because the malloc to store the info failed, probably due to a lack of memory."), txt);
                goto DONE;
            }
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Setting params for the SDCardQueryInfo..."));
            ZeroMemory(pCI, sizeof(SD_CARD_INTERFACE_EX));
            //2 Set Params
            structSize = sizeof(SD_CARD_INTERFACE_EX);
            pCardInfo = pCI;
            break;
        case SD_INFO_CARD_STATUS:
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Zeroing the Status info buffer..."));
            ZeroMemory(&crdstat, sizeof(SD_CARD_STATUS));
            //2 Set Params
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Setting params for the SDCardQueryInfo..."));
            crdstat = 0;
            structSize = sizeof(SD_CARD_STATUS);
            pCardInfo = &crdstat;
            break;
        case SD_INFO_HOST_BLOCK_CAPABILITY:
            //2 Malloc Struct Memory
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Allocating memory for the Host Block Capabilities info..."));
            pHBC = (PSD_HOST_BLOCK_CAPABILITY)malloc(sizeof(SD_HOST_BLOCK_CAPABILITY));
            if (pHBC == NULL)
            {
                LogFail(indent2, TEXT("QIT: Not enough memory to get the Host's Block Caps info from bus. Bailing...."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to get the Host's Block Caps info from bus.\n This is because the malloc to store the info failed, probably due to a lack of memory."));
                goto DONE;
            }
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Initializing the Host Block Capabilities info buffer..."));
            ZeroMemory(pHBC, sizeof(SD_HOST_BLOCK_CAPABILITY));
            pHBC->ReadBlocks = 0;
            pHBC->WriteBlocks = 0;
            pHBC->ReadBlockSize = 0;
            pHBC->WriteBlockSize = 0xFFFF;
            //2 Set Params
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Setting params for the SDCardQueryInfo..."));
            structSize = sizeof(SD_HOST_BLOCK_CAPABILITY);
            pCardInfo = pHBC;
            break;
        default:
            goto SKIP;
    }
//1 Perform SDCardInfoQuery
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Performing SDCardInfoQuery call with InfoType = %s (%u)"), TranslateInfoTypes(pTestParams->iT), pTestParams->iT);
    rStat = wrap_SDCardInfoQuery(pClientInfo->hDevice, pTestParams->iT, pCardInfo, structSize);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1, TEXT("QIT: The test failed during the call to SDCardInfoQuery. Error = %s (0x%x)"),TranslateErrorCodes(rStat), rStat);
        LogDebug(indent, SDIO_ZONE_TST, TEXT("QIT: Bailing..."));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The card failed in the call to SDCardInfoQuery with info Type %s (%u). Error =%s (0x%x)."), TranslateInfoTypes(pTestParams->iT), pTestParams->iT, TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
//1 Verify
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Verifying the SDCardInfoQuery call worked..."));
    switch(pTestParams->iT)
    {
        case SD_INFO_REGISTER_OCR:
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("QIT: Is the OCR data non-zero and has the power up succeeded?..."));
            if (pOCR != 0)
            {
                LogFail(indent1, TEXT("QIT: The OCR register according to SDCardInfoQuery has a value of  zero. "));
                LogDebug(indent, SDIO_ZONE_TST, TEXT("QIT: Bailing..."));
                SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The OCR register according to SDCardInfoQuery has a value of  zero."));
            }
            else if  (! (SD_CARD_POWER_UP_STATUS & *pOCR ))
            {
                LogFail(indent1, TEXT("QIT: The OCR register claims that power up has not completed, but as the test has run seems unlikely. "));
                LogDebug(indent, SDIO_ZONE_TST, TEXT("QIT: Bailing..."));
                SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The OCR register claims that power up has not completed, but as the test has run seems unlikely."));
            }
            break;
        case SD_INFO_REGISTER_CID:
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("QIT: The tests of CMD10 test if the cached CID register matches a newly groveled info, so I will instead verify internal consistency here..."));
            //2 Zero?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Is the returned raw value non-zero?..."));
            LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Allocating memory for comparison..."));
            pCBUFF= (PUCHAR)malloc(16*sizeof(UCHAR));
            if (pCBUFF == NULL)
            {
                LogFail(indent5, TEXT("QIT: Not enough memory to create a comparison buffer...."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to create a comparison buffer.\n This is because the malloc to store the info failed, probably due to a lack of memory."));
                goto DONE;
            }
            ZeroMemory(pCBUFF,16);
            if (memcmp(pCID->RawCIDRegister, pCBUFF,16) == 0)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("The raw CID is zero. This is an invalid return value."));
                bFail = TRUE;
                goto DONE;
            }
            else
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: No."));
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: RAW CID = %s."), GenerateBinString(pCID->RawCIDRegister, 16));
            }
            //2 Man ID ?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the Manufacturing ID match the raw?..."));
            if (pCID->ManufacturerID == pCID->RawCIDRegister[15])
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. ManID= 0x%x"), pCID->ManufacturerID);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = 0x%x, Parsed = 0x%x"), pCID->RawCIDRegister[15], pCID->ManufacturerID);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CID Manufacturing ID differs from the Raw.\nRaw = 0x%x, Parsed = 0x%x"), pCID->RawCIDRegister[0], pCID->ManufacturerID);
                bFail = TRUE;
            }
            //2 OEM ID ?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the OEM ID the raw?..."));
            if ((((pCID->RawCIDRegister[14])<< 8) + pCID->RawCIDRegister[13]) == (((pCID->OEMApplicationID[1])<<8) + pCID->OEMApplicationID[0]) )
//            if ((pCID->OEMApplicationID[0] == pCID->RawCIDRegister[13]) && (pCID->OEMApplicationID[1] == pCID->RawCIDRegister[14]))
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. OEMID= 0x%x"), ((pCID->OEMApplicationID[0])<<8) + pCID->OEMApplicationID[1]);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = 0x%x, Parsed = 0x%x"), ((pCID->RawCIDRegister[14])<<8) + pCID->RawCIDRegister[13], ((pCID->OEMApplicationID[0])<<8) + pCID->OEMApplicationID[1]);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CID OEM ID differs from the Raw.\nRaw = 0x%x, Parsed = 0x%x"), ((pCID->RawCIDRegister[14])<<8) + pCID->RawCIDRegister[13], ((pCID->OEMApplicationID[0])<<8) + pCID->OEMApplicationID[1]);
                bFail = TRUE;
            }
            //2 Product Name
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the Product Name match the raw?..."));
            ZeroMemory(pCBUFF,16);
            pCBUFF[0] = pCID->RawCIDRegister[12];
            pCBUFF[1] = pCID->RawCIDRegister[11];
            pCBUFF[2] = pCID->RawCIDRegister[10];
            pCBUFF[3] = pCID->RawCIDRegister[9];
            pCBUFF[4] = pCID->RawCIDRegister[8];
            pCBUFF[5] = TCHAR('\0');
            if (strcmp((char *)pCBUFF, (char *)(pCID->ProductName)) == 0 || strcmp(_strrev((char *)pCBUFF), (char *)(pCID->ProductName)) == 0)
            {
                TCHAR tmp[6];
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, pCID->ProductName, -1, tmp, 6);
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. Product Name= %s"), tmp);
            }
            else
            {
                TCHAR tmp[6];
                TCHAR tmp1[6];
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, pCID->ProductName, -1, tmp, 6);
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (char *)pCBUFF, -1, tmp1, 6);
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), tmp1, tmp);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CID Product Name differs from the Raw.\nRaw = %s, Parsed = %s"), tmp1, tmp);
                bFail = TRUE;
            }
            //2 Revision
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the Revision match the raw?..."));
            R1 = (pCID->RawCIDRegister[7] & 0xf0) >>4;
            R2 = pCID->RawCIDRegister[7] & 0x0f;
            if ((R1 == pCID->MajorProductRevision) && (R2 == pCID->MinorProductRevision) )
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. Revision= %u.%u"), R1, R2);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u.%u, Parsed = %u.%u"), R1,R2, pCID->MajorProductRevision, pCID->MinorProductRevision);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CID Revision differs from the Raw.\nRaw = %u.%u, Parsed = %u.%u"), R1,R2, pCID->MajorProductRevision, pCID->MinorProductRevision);
                bFail = TRUE;
            }
            //2 Serial Number
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the Serial Number match the raw?..."));
            if (pCID->ProductSerialNumber == (((DWORD)pCID->RawCIDRegister[6])<<24) + (((DWORD)pCID->RawCIDRegister[5])<<16) + (((DWORD)pCID->RawCIDRegister[4])<<8) + (DWORD)pCID->RawCIDRegister[3])
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. Serial Number= %u"), pCID->ProductSerialNumber);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u, Parsed = %u"), (pCID->RawCIDRegister[6]<<24) + ((pCID->RawCIDRegister[5])<<16) + ((pCID->RawCIDRegister[4])<<8) + pCID->RawCIDRegister[3], pCID->ProductSerialNumber);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CID Manufacturing ID differs from the Raw.\nRaw = %u, Parsed = %u"), ((pCID->RawCIDRegister[6])<<24) + ((pCID->RawCIDRegister[5])<<16) + ((pCID->RawCIDRegister[4])<<8) + pCID->RawCIDRegister[3], pCID->ProductSerialNumber);
                bFail = TRUE;
            }
            //2 Manufacturing Date
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the Manufacturing Date match the raw?..."));
            M = pCID->RawCIDRegister[1] & 0x0f;
            Y = ((pCID->RawCIDRegister[2] & 0x0f) <<4) + ((pCID->RawCIDRegister[1] & 0xf0) >>4) + 2000;
            if ((M == pCID->ManufacturingMonth) && (Y == pCID->ManufacturingYear) )
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. Manufacturing Date = %u/%u"), pCID->ManufacturingMonth, pCID->ManufacturingYear);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u/%u, Parsed = %u/%u"), M, Y, pCID->ManufacturingMonth, pCID->ManufacturingYear);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CID Manufacturing ID differs from the Raw.\nRaw = %u/%u, Parsed = %u/%u"), M, Y, pCID->ManufacturingMonth, pCID->ManufacturingYear);
                bFail = TRUE;
            }
            break;
        case SD_INFO_REGISTER_CSD: {
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("QIT: The tests of CMD9 test if the cached CSD register matches a newly groveled info, so I will instead verify internal consistency here..."));
            //2 Zero?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Is the returned raw value non-zero?..."));
            LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Allocating memory for comparison..."));
            pCBUFF= (PUCHAR)malloc(16*sizeof(UCHAR));
            if (pCBUFF == NULL)
            {
                LogFail(indent5, TEXT("QIT: Not enough memory to create a comparison buffer...."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to create a comparison buffer.\n This is because the malloc to store the info failed, probably due to a lack of memory."));
                goto DONE;
            }
            ZeroMemory(pCBUFF,16);
            if (memcmp(pCSD->RawCSDRegister, pCBUFF,16) == 0)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("The raw CSD is zero. This is an invalid return value."));
                bFail = TRUE;
                goto DONE;
            }
            else
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: No."));
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: RAW CSD = %s."), GenerateBinString(pCSD->RawCSDRegister, 16));
            }
            //2 CSD Structure ?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD structure match the raw?..."));
            if (pCSD->CSDVersion == ((pCSD->RawCSDRegister[15] & 0xC0) >> 6))
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. Version = 0x%x"), pCSD->CSDVersion);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = 0x%x, Parsed = 0x%x"),  ((pCSD->RawCSDRegister[15] & 0xC0) >> 6), pCSD->CSDVersion);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Version differs from the Raw.\nRaw = 0x%x, Parsed = 0x%x"), (pCSD->RawCSDRegister[15] & 0xC0), pCSD->CSDVersion);
                bFail = TRUE;
            }
            //2 Data Read Access Time-1?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD Data Read Access Time-1 match the raw?..."));
            dTmp = DecipherTAAC(pCSD->RawCSDRegister[14]);
            if (pCSD->DataAccessTime.TAAC == dTmp)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. TAAC = %.1f ns"), pCSD->DataAccessTime.TAAC);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %.1f ns, Parsed = %.1f ns"), dTmp, pCSD->DataAccessTime.TAAC);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Data Read Access Time-1 differs from the Raw.\nRaw = %.1f ns, Parsed = %.1f ns"), dTmp, pCSD->DataAccessTime.TAAC);
                bFail = TRUE;
            }
            //2 Data Read Access Time-2?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD Data Read Access Time-2 match the raw?..."));
            if (pCSD->DataAccessTime.NSAC == pCSD->RawCSDRegister[13])
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. NSAC = %u clock cycles"), pCSD->DataAccessTime.NSAC);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u clock cycles, Parsed = %u clock cycles"), pCSD->RawCSDRegister[13], pCSD->DataAccessTime.NSAC);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Data Read Access Time-2 differs from the Raw.\nRaw = %u clock cycles, Parsed = %u clock cycles"), pCSD->RawCSDRegister[13], pCSD->DataAccessTime.NSAC);
                bFail = TRUE;
            }
            //2 Transfer Rate?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD Transfer rate match the raw?..."));
            ulTmp = DecipherTRAN_SPEED(pCSD->RawCSDRegister[12]);
            if (pCSD->MaxDataTransferRate == ulTmp)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. MaxDataTransferRate = %u kbits/sec"), pCSD->MaxDataTransferRate);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u kbits/sec, Parsed = %u kbits/sec"), ulTmp, pCSD->MaxDataTransferRate);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Max Data Transfer Rate differs from the Raw.\nRaw = %u kbits/sec ns, Parsed = %u kbits/sec"), ulTmp, pCSD->MaxDataTransferRate);
                bFail = TRUE;
            }
            //2 Command Classes Supported?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD Card Command class supported flags match the raw?..."));
            if (pCSD->CardCommandClasses == ((pCSD->RawCSDRegister[11])<< 4) + ((pCSD->RawCSDRegister[10])>> 4))
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. CCC = 0x%x"), pCSD->CardCommandClasses);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = 0x%x, Parsed = 0x%x"), ((pCSD->RawCSDRegister[11])<< 4) + ((pCSD->RawCSDRegister[10])>> 4), pCSD->CardCommandClasses);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Card Command Classes Supported flags differs from the Raw.\nRaw = 0x%x, Parsed = 0x%x"), ((pCSD->RawCSDRegister[11])<< 4) + ((pCSD->RawCSDRegister[10])>> 4), pCSD->CardCommandClasses);
                bFail = TRUE;
            }
            //2 Read Block Length?
            usTmp = (USHORT)pow (2,(pCSD->RawCSDRegister[10] & 0xf));
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD maximum read block length match the raw?..."));
            if (pCSD->MaxReadBlockLength == usTmp )
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. MaxReadBlockLength = %u bytes"), pCSD->MaxReadBlockLength);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u bytes, Parsed = %u bytes"), usTmp, pCSD->MaxReadBlockLength);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Max Read Block Length differs from the Raw.\nRaw = %u bytes, Parsed = %u bytes"), usTmp, pCSD->MaxReadBlockLength);
                bFail = TRUE;
            }

            //2 Partial Read Block Support?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD boolean indicating partial read block support match the raw?..."));
            if (((pCSD->ReadBlockPartial == TRUE) && (pCSD->RawCSDRegister[9] & 0x80))  || ((pCSD->ReadBlockPartial == FALSE) && ((pCSD->RawCSDRegister[9] & 0x80) == 0)) )
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. ReadBlockPartial = %s"), Supported((BOOL)pCSD->ReadBlockPartial));
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[9] & 0x80), Supported((BOOL)pCSD->ReadBlockPartial));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Read Block Partial Support boolean differs from the Raw.\nRaw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[9] & 0x80), Supported(pCSD->ReadBlockPartial));
                bFail = TRUE;
            }
            //2 Write Block Misalignment Support?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD boolean indicating write block misalignment support match the raw?..."));
            if (((pCSD->WriteBlockMisalign== TRUE) && (pCSD->RawCSDRegister[9] & 0x40))  || ((pCSD->WriteBlockMisalign == FALSE) && ((pCSD->RawCSDRegister[9] & 0x40) == 0)) )
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. WriteBlockMisalign = %s"), Supported((BOOL)pCSD->WriteBlockMisalign));
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[9] & 0x40), Supported((BOOL)pCSD->WriteBlockMisalign));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Write Block Misalignment Support boolean differs from the Raw.\nRaw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[9] & 0x40), Supported(pCSD->WriteBlockMisalign));
                bFail = TRUE;
            }
            //2 Read Block Misalignment Support?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD boolean indicating read block misalignment support match the raw?..."));
            if (((pCSD->ReadBlockMisalign== TRUE) && (pCSD->RawCSDRegister[9] & 0x20))  || ((pCSD->ReadBlockMisalign== FALSE) && ((pCSD->RawCSDRegister[9] & 0x20) == 0)) )
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. ReadBlockMisalign= %s"), Supported((BOOL)pCSD->ReadBlockMisalign));
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[9] & 0x20), Supported((BOOL)pCSD->ReadBlockMisalign));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Read Block Misalignment Support boolean differs from the Raw.\nRaw = %s, Parsed = %s"), Supported((BOOL)pCSD->RawCSDRegister[9] & 0x20), Supported(pCSD->ReadBlockMisalign));
                bFail = TRUE;
            }
            //2 DSR?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD boolean indicating the existence of the DSR register match the raw?..."));
            if (((pCSD->DSRImplemented== TRUE) && (pCSD->RawCSDRegister[9] & 0x10))  || ((pCSD->DSRImplemented== FALSE) && ((pCSD->RawCSDRegister[9] & 0x10) == 0)) )
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. DSR = %s"), Supported((BOOL)pCSD->DSRImplemented));
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[9] & 0x10), Supported((BOOL)pCSD->DSRImplemented));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD DSR Implementation boolean differs from the Raw.\nRaw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[9] & 0x10), Supported((BOOL)pCSD->DSRImplemented));
                bFail = TRUE;
            }
            //2 Device Size?
            ullTmp = CalculateCapacity(pCSD->RawCSDRegister, pTestParams);
            ULONGLONG ulCSDDeviceSize = (ULONGLONG)pCSD->DeviceSize;

            // device size shown as BLOCK size for SDHC/MMCHC card
            if (IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType)){
                ulCSDDeviceSize *= 0x200;
            }

            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD device size match the raw?..."));

            if (ullTmp == ulCSDDeviceSize)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. DeviceSize = %u bytes"), ulCSDDeviceSize);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u bytes, Parsed = %u bytes"), ulTmp, ulCSDDeviceSize);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Device Size differs from the Raw.\nRaw = %u bytes, Parsed = %u bytes"), ulTmp, ulCSDDeviceSize);
                bFail = TRUE;
            }
            //2 Min Read Curr?
            if (!IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType) ) {
                // in 1.1 spec VDD_R_CURR_MIN [61:59]
                ufTmp =     DecipherMinCurrent((pCSD->RawCSDRegister[7] & 0x38) >>3);
                LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD minimum read current match the raw?..."));
                if (ufTmp == pCSD->VDDReadCurrentMin)
                {
                    LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. VDDReadCurrentMin = %.1f mA"), ufTmp);
                }
                else
                {
                    LogFail(indent4, TEXT("QIT: Mismatch. Raw = %.1f mA, Parsed = %.1f mA"), ufTmp, (float)(pCSD->VDDReadCurrentMin));
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Minimum Read Current differs from the Raw.\nRaw = %.1f mA, Parsed = %.1f mA"), ufTmp, (float)(pCSD->VDDReadCurrentMin));
                    bFail = TRUE;
                }
            }
            //2 Max Read Curr?
            if (!IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType) ) {
                usTmp=     DecipherMaxCurrent(pCSD->RawCSDRegister[7] & 0x7);
                LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD maximum read current match the raw?..."));
                if (usTmp == pCSD->VDDReadCurrentMax)
                {
                    LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. VDDReadCurrentMax = %u mA"), usTmp);
                }
                else
                {
                    LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u mA, Parsed = %u mA"), usTmp, (float)(pCSD->VDDReadCurrentMax));
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Maximum Read Current differs from the Raw.\nRaw = %u mA, Parsed = %u mA"), usTmp, (float)(pCSD->VDDReadCurrentMax));
                    bFail = TRUE;
                }
            }
            //2 Min Write Curr?
            if (!IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType) ) {
                ufTmp =     DecipherMinCurrent((pCSD->RawCSDRegister[6] & 0xe0) >>5);
                LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD minimum write current match the raw?..."));
                if (ufTmp == pCSD->VDDWriteCurrentMin)
                {
                    LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. VDDWriteCurrentMin = %.1f mA"), ufTmp);
                }
                else
                {
                    LogFail(indent4, TEXT("QIT: Mismatch. Raw = %.1f mA, Parsed = %.1f mA"), ufTmp, (float)(pCSD->VDDWriteCurrentMin));
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Minimum Write Current differs from the Raw.\nRaw = %.1f mA, Parsed = %.1f mA"), ufTmp, (float)(pCSD->VDDWriteCurrentMin));
                    bFail = TRUE;
                }
            }
            //2 Max Write Curr?
            if (!IS_HIGHCAPACITY_MEDIA(pTestParams->sdDeviceType) ) {
                usTmp =     DecipherMaxCurrent((pCSD->RawCSDRegister[6] & 0x1c) >>2);
                LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD maximum write current match the raw?..."));
                if (usTmp == pCSD->VDDWriteCurrentMax)
                {
                    LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. VDDWriteCurrentMax = %u mA"), usTmp);
                }
                else
                {
                    LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u mA, Parsed = %u mA"), usTmp, (float)(pCSD->VDDWriteCurrentMax));
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Maximum Write Current differs from the Raw.\nRaw = %u mA, Parsed = %u mA"), usTmp, (float)(pCSD->VDDWriteCurrentMax));
                    bFail = TRUE;
                }
            }
            //2 Block Erase Support?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD boolean indicating erase block support match the raw?..."));
            if (((pCSD->EraseBlockEnable== TRUE) && (pCSD->RawCSDRegister[5] & 0x40))  || ((pCSD->EraseBlockEnable== FALSE) && ((pCSD->RawCSDRegister[5] & 0x40) == 0)) )
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. EraseBlockEnable = %s"), Supported((BOOL)pCSD->WriteBlockMisalign));
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[5] & 0x40), Supported((BOOL)pCSD->EraseBlockEnable));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Erase Block Support boolean differs from the Raw.\nRaw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[5] & 0x40), Supported((BOOL)pCSD->EraseBlockEnable));
                bFail = TRUE;
            }
            //2 Sector Size?
            R1 = ((pCSD->RawCSDRegister[5] & 0x3f) <<1) + ((pCSD->RawCSDRegister[4] & 0x80) >> 7) + 1;
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD maximum write current match the raw?..."));
            if (R1 == pCSD->EraseSectorSize)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. EraseSectorSize = %u blocks"), pCSD->EraseSectorSize);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u blocks, Parsed = %u blocks"), R1, pCSD->EraseSectorSize);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Sector Size differs from the Raw.\nRaw = %u blocks, Parsed = %u blocks"), R1, pCSD->EraseSectorSize);
                bFail = TRUE;
            }
            //2 WP Group Size?
            R1 = ((pCSD->RawCSDRegister[4] & 0x7f)) + 1;
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD write protection group size match the raw?..."));
            if (R1 == pCSD->WPGroupSize)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. WPGroupSize = %u sectors"), pCSD->WPGroupSize);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u sectors, Parsed = %u sectors"), R1, pCSD->WPGroupSize);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Write Protection Group Size differs from the Raw.\nRaw = %u sectors, Parsed = %u sectors"), R1, pCSD->WPGroupSize);
                bFail = TRUE;
            }
            //2 Group Write protection Support?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD boolean indicating write protection group support match the raw?..."));
            if (((pCSD->WPGroupEnable== TRUE) && (pCSD->RawCSDRegister[3] & 0x80))  || ((pCSD->WPGroupEnable== FALSE) && ((pCSD->RawCSDRegister[3] & 0x80) == 0)) )
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. WPGroupEnable = %s"), Supported((BOOL)pCSD->WPGroupEnable));
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[3] & 0x80), Supported((BOOL)pCSD->WPGroupEnable));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Write Protection Group Support boolean differs from the Raw.\nRaw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[3] & 0x80), Supported((BOOL)pCSD->WPGroupEnable));
                bFail = TRUE;
            }
            //2 Write Speed Factor?
            R1 = ((pCSD->RawCSDRegister[3] & 0x1c)) >> 2;
            R1 = (UCHAR)pow(2,R1);
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD write speed factor match the raw?..."));
            if (R1 == pCSD->WriteSpeedFactor)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. WriteSpeedFactor = %u"), pCSD->WriteSpeedFactor);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u, Parsed = %u"), R1, pCSD->WriteSpeedFactor);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Write Speed Factor differs from the Raw.\nRaw = %u, Parsed = %u"), R1, pCSD->WriteSpeedFactor);
                bFail = TRUE;
            }
            //2 Write Block Length?
            usTmp = (((pCSD->RawCSDRegister[3]) & 0x3)<<2) + (((pCSD->RawCSDRegister[2]) & 0xc0)>>6);
            usTmp = (USHORT)pow(2,usTmp);
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD write block length match the raw?..."));
            if (usTmp == pCSD->MaxWriteBlockLength)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. MaxWriteBlockLength = %u bytes "), pCSD->MaxWriteBlockLength);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %u bytes , Parsed = %u bytes "), usTmp, pCSD->MaxWriteBlockLength);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD Maximum Write Block Length differs from the Raw.\nRaw = %u bytes, Parsed = %u bytes"), usTmp, pCSD->MaxWriteBlockLength);
                bFail = TRUE;
            }
            //2 Partial Write Block Support?
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD boolean indicating write partial block support match the raw?..."));
            if (((pCSD->WriteBlockPartial== TRUE) && (pCSD->RawCSDRegister[2] & 0x20))  || ((pCSD->WriteBlockPartial== FALSE) && ((pCSD->RawCSDRegister[2] & 0x20) == 0)) )
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. WriteBlockPartial = %s"), Supported((BOOL)pCSD->WriteBlockPartial));
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[2] & 0x20), Supported((BOOL)pCSD->WriteBlockPartial));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD  Write Partial Block Support boolean differs from the Raw.\nRaw = %s, Parsed = %s"), Supported(pCSD->RawCSDRegister[2] & 0x20), Supported((BOOL)pCSD->WriteBlockPartial));
                bFail = TRUE;
            }
            //2 Are the Contents Copied?
            lptstrRaw = DecipherFlagsU(FT_COPY, pCSD->RawCSDRegister[1]);
            lptstrParsed= DecipherFlagsB(FT_COPY, pCSD->CopyFlag);
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD boolean indicating copied status match the raw?..."));
            if (_tcscmp(lptstrRaw, lptstrParsed) == 0)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. CopyFlag = %s"), lptstrParsed);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), lptstrRaw, lptstrParsed);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD  Copy Flag boolean differs from the Raw.\nRaw = %s, Parsed = %s"), lptstrRaw, lptstrParsed);
                bFail = TRUE;
            }
            //2 Are the contents Permanently Write Protected?
            lptstrRaw = DecipherFlagsU(FT_PERM_WP, pCSD->RawCSDRegister[1]);
            lptstrParsed= DecipherFlagsB(FT_PERM_WP, pCSD->PermanentWP);
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD boolean indicating permanent write protection status match the raw?..."));
            if (_tcscmp(lptstrRaw, lptstrParsed) == 0)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. PermanentWP = %s"), lptstrParsed);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), lptstrRaw, lptstrParsed);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD  Permanent Write Protection boolean differs from the Raw.\nRaw = %s, Parsed = %s"), lptstrRaw, lptstrParsed);
                bFail = TRUE;
            }
            //2 Are the contents Temporarily  Write Protected?
            lptstrRaw = DecipherFlagsU(FT_TEMP_WP, pCSD->RawCSDRegister[1]);
            lptstrParsed= DecipherFlagsB(FT_TEMP_WP, pCSD->TemporaryWP);
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD boolean indicating temporary write protection status match the raw?..."));
            if (_tcscmp(lptstrRaw, lptstrParsed) == 0)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. TemporaryWP = %s"), lptstrParsed);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), lptstrRaw, lptstrParsed);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Parsed CSD  Temporary Write Protection boolean differs from the Raw.\nRaw = %s, Parsed = %s"), lptstrRaw, lptstrParsed);
                bFail = TRUE;
            }
            //2 File Format?
            lptstrRaw = ucDecipherFS(pCSD->RawCSDRegister[1]);
            lptstrParsed= fsDecipherFS(pCSD->FileSystem);
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Does the CSD file system match the raw?..."));
            if (_tcscmp(lptstrRaw, lptstrParsed) == 0)
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. FileSystem = %s"), lptstrParsed);
            }
            else
            {
                LogFail(indent4, TEXT("QIT: Mismatch. Raw = %s, Parsed = %s"), lptstrRaw, lptstrParsed);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The File System differs from the Raw.\nRaw = %s, Parsed = %s"), lptstrRaw, lptstrParsed);
                bFail = TRUE;
            }    }
            break;
        case SD_INFO_REGISTER_RCA:
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("QIT: Is the RCA returned the card's true relative address (Let's use it)..."));
            //2 Allocate for Response
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("QIT: Allocating memory for response..."));
            AllocateResponseBuff(indent3, pTestParams, &pResp, TRUE);
            //2 Get Status
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("QIT: Using RCA to get status register through bus request..."));
            rStat = wrap_SDSynchronousBusRequest(pClientInfo->hDevice, SD_CMD_SEND_STATUS, RCA<<16, SD_COMMAND, ResponseR1, pResp, 0, 0, NULL, 0);
            if (!SD_API_SUCCESS(rStat))
            {
                LogFail(indent3,TEXT("QIT: Failure in bus request to get status. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure in synchronous bus request using RCA value retrieved. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
                DumpStatusFromResponse(indent3, pResp);
                goto DONE;
            }
            break;
        case SD_INFO_CARD_INTERFACE_EX:
            __fallthrough;
        case SD_INFO_HOST_IF_CAPABILITIES:
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("QIT: Verify that a non-zero value is returned..."));
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Allocate memory, and zero it..."));
            pCIComp= (PSD_CARD_INTERFACE_EX)malloc(sizeof(SD_CARD_INTERFACE_EX));
            if (pCIComp == NULL)
            {
                LogFail(indent4, TEXT("QIT: Not enough memory to create comparison structure. Bailing...."));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to allocate memory that can be compared to an empty SD_CARD_INTERFACE_EX struct.\n This is because the malloc to store the info failed, probably due to a lack of memory."));
                goto DONE;
            }
            ZeroMemory(pCIComp, sizeof(SD_CARD_INTERFACE_EX));
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Compare Data..."));
            if (memcmp(pCI, pCIComp, sizeof(SD_CARD_INTERFACE_EX)) == 0)
            {
                LogFail(indent4, TEXT("QIT: No valid %s interface data returned by SDCardInfoQuery"), txt);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Zeroed Interface struct returned by SDCardInfoQuery call.\nIf nothing else a non-zero clock rate should have been returned."));
            }
            if (pTestParams->iT == SD_INFO_HOST_IF_CAPABILITIES)
            {
                SD_CARD_INTERFACE_EX ci;
                ZeroMemory(&ci, sizeof(SD_CARD_INTERFACE_EX));
                LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Getting Card Interface to verify that Caps now indicate real maximum value..."));
                rStat = wrap_SDCardInfoQuery(pClientInfo->hDevice, SD_INFO_CARD_INTERFACE_EX, &ci, sizeof(SD_CARD_INTERFACE_EX));
                if (!SD_API_SUCCESS(rStat))
                {
                    LogFail(indent4,TEXT("QIT: SDCardInfoQuery Failed. Unable to get Card Interface. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
                    AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Unable to get Card Interface to compare against caps. SDCardInfoQuery failed. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
                }
                else
                {
                    LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Because the Clock Speed defaults the maximum rate, the Caps should return the same rate as the current card interface. Comparing..."));

                    // Check if Host and card interface clock rate are less than or equal to the standard clock rates.
                    // According to the SD spec, the operating clock rate frequency can be less than the specified max clock rates.
                    if ((ci.ClockRate > 0 && (ci.ClockRate <= SD_HIGH_SPEED_RATE || ci.ClockRate <= MMC_HIGH_SPEED_RATE || ci.ClockRate <= SD_FULL_SPEED_RATE || ci.ClockRate <= MMC_FULL_SPEED_RATE)) &&
                        (pCI->ClockRate > 0 && (pCI->ClockRate <= SD_HIGH_SPEED_RATE || pCI->ClockRate <= MMC_HIGH_SPEED_RATE || pCI->ClockRate <= SD_FULL_SPEED_RATE || pCI->ClockRate <= MMC_FULL_SPEED_RATE)))
                    {
                        // Check if clock rate from the two card interface calls match.  They should since the clock rate should not fluctuate.
                        if (ci.ClockRate == pCI->ClockRate)
                        {
                            AppendMessageBufferAndResult(pTestParams, TPR_PASS,TEXT("The maximum clock rate listed in the Caps equals the actual clock rate in the Card interface.  Clock Rate = %u Hz"), ci.ClockRate);
                        }
                        // Valid condition. Interface set to Full Speed even when though Host supports High Speed
                        else if ((pCI->ClockRate <= SD_HIGH_SPEED_RATE && ci.ClockRate <= SD_FULL_SPEED_RATE) ||
                                 (pCI->ClockRate <= MMC_HIGH_SPEED_RATE && ci.ClockRate <= MMC_FULL_SPEED_RATE))
                        {
                            AppendMessageBufferAndResult(pTestParams, TPR_PASS,TEXT("Though the maximum clock rate listed in the Caps does not equal the actual clock rate in the Card interface - \nTest Passes as Host supports High speed but the card does not - which is a valid situation. \nActual Clock Rate = %u Hz, Maximum Clock rate (caps) = %u Hz"), ci.ClockRate, pCI->ClockRate);
                        }
                        // Invalid condition
                        else
                        {
                            AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("The maximum clock rate listed in the Caps does not equal the actual clock rate in the Card interface. \nBecause the default value should be the maximum real value supported by the card this is a failure. \nActual Clock Rate = %u Hz, Maximum Clock rate (caps) = %u Hz"), ci.ClockRate, pCI->ClockRate);
                        }
                    }
                    // Host and/or Card interface not supporting standard High/Full speed clock rate.
                    else
                    {
                        LogFail(indent4,TEXT("QIT: The Card interface containing the current state of the card and/or the clock rate of Host Interface Caps does not match expected standard full/high speed clock rate. Current Clock Rate = %u, Maximum Clock Rate (caps) = %u"), ci.ClockRate, pCI->ClockRate);
                        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("The maximum clock rate listed in the Caps and/or the actual clock rate in the Card interface does not match expected standard full/high speed clock rate. \nActual Clock Rate = %u Hz, Maximum Clock rate (caps) = %u Hz"), ci.ClockRate, pCI->ClockRate);
                    }
                }
            }
            break;
        case SD_INFO_CARD_STATUS:
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("QIT: Verify that a non-zero value is returned..."));
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Verify status returned is non-zero..."));
            if (crdstat == 0)
            {
                LogFail(indent4, TEXT("QIT: No valid status was returned."), txt);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Card Status returned equals zero.\nIf nothing else a non-zero current should have been returned, as the card is not in idle mode."));
            }
            break;
        case SD_INFO_HOST_BLOCK_CAPABILITY:
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("QIT: Verify that host block sizes have been changed from what was originally set..."));
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Has Read Block size been adjusted up from an unreasonably small value?"));
            if (pHBC->ReadBlockSize == 0)
            {
                LogFail(indent4, TEXT("QIT: No. ReadBlockSize was left at 0"));
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Read Block size was left at unreasonably small value by host driver."));
            }
            else
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. Was =  0 Now = %u"),pHBC->ReadBlockSize);

            }
            LogDebug(indent3, SDIO_ZONE_TST, TEXT("QIT: Has Write Block size been adjusted down from an unreasonably large value?"));
            if  (pHBC->WriteBlockSize == 0xffff)
            {
                LogFail(indent4, TEXT("QIT: No. WriteBlockSize was left at %u"),pHBC->WriteBlockSize);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Write Block size was left at an unreasonably large value by host driver."));
            }
            else
            {
                LogDebug(indent4, SDIO_ZONE_TST, TEXT("QIT: Yes. Was =  %u Now = %u"),0xffff, pHBC->WriteBlockSize);
            }
            break;
    }
    goto DONE;

//1Cleanup
SKIP:
    LogWarn(indent1,TEXT("QIT: This test is not yet implemented!!"));
    SetMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("The InfoType %s (%u), is not yet supported by the test driver."), TranslateInfoTypes(pTestParams->iT), pTestParams->iT);
DONE:

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("QIT: Cleaning up..."));
//1Good State Check 2
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("QIT: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent3, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent2, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state upon completion."), FALSE, TRUE);

    if (IsBufferEmpty(pTestParams))
    {
        SetMessageBufferAndResult(pTestParams, TPR_PASS, TEXT("The test on SDCardQueryInfo with Infotype: %s, (%u) succeeded."), TranslateInfoTypes(pTestParams->iT), pTestParams->iT);
    }

    if (pCID)
    {
        free(pCID);
        pCID = NULL;
    }
    if (pCSD)
    {
        free(pCSD);
        pCSD = NULL;
    }
    if (pHBC)
    {
        free(pHBC);
        pHBC = NULL;
    }
    if (pCI)
    {
        free(pCI);
        pCI = NULL;
    }
    if (pOCR)
    {
        free(pOCR);
        pOCR = NULL;
    }
    if (pResp)
    {
        free(pResp);
        pResp = NULL;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("QIT: Exiting Driver Test Function..."));
}

