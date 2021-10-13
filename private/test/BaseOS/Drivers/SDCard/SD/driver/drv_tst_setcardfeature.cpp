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
#include <SDCardDDK.h>
#include <sdcard.h>
#include "scf_hlp.h"
#include "br_hlp.h"
#include <sddrv.h>

#define NUMBER_OF_CYCLES    20
//#define EXCEPTED_DELTA    100

DWORD findMinCRDivisor(PSD_CARD_INTERFACE_EX pciOrig, SD_DEVICE_HANDLE hDevice, SD_API_STATUS *prStat, PDWORD pdwCR)
{
    SD_API_STATUS rStat = 0;
    SD_CARD_INTERFACE_EX ci;
    int c;
    DWORD dwMinDiv = 10;    //  9 = min rate is not a max/ a power of 2 if this is the case it will not be an error if it is a non-standard host
                            // 10 = original rate not passed in, test error
                            // 11 = failure in initial SDSetCardFeature call, this will be a failure, but it should not bleed into the next test
                            // 12 = failure in second SDSetCardFeature call, can't return to original value, not only is this a failure, but it might effect later tests
                            // 13 = min rate = max rate. This may not be an error, if it is not a standard host, but the test must skip since multiple rates are not supported
    if (pciOrig != NULL)
    {
        ZeroMemory(&ci, sizeof(SD_CARD_INTERFACE_EX));
        ci.ClockRate = 1;
        ci.InterfaceModeEx = pciOrig->InterfaceModeEx;
        rStat = wrap_SDSetCardFeature(hDevice, SD_SET_CARD_INTERFACE_EX, &ci, sizeof(SD_CARD_INTERFACE_EX));
        if (SD_API_SUCCESS(rStat))
        {
            // Get the current running clock rate
            // note that when you set the card feature - you need to retrieve the value back
            // to see the lowest clock rate - it won't just set whatever you pass in.
            rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_CARD_INTERFACE_EX, &ci, sizeof(SD_CARD_INTERFACE_EX));

            if (SD_API_SUCCESS(rStat))
            {
                if (pdwCR) *pdwCR = ci.ClockRate;
                for (c = 8; (c >=0) && (ci.ClockRate > (pciOrig->ClockRate / ((DWORD)(pow((double) 2, (double) c))))); c--);

                if (ci.ClockRate == (pciOrig->ClockRate / ((DWORD)(pow((double) 2, (double) c)))))
                {
                    dwMinDiv = (DWORD)c;
                }
                else if (c == 0)
                {
                    dwMinDiv = 13;
                }
                else
                {
                    dwMinDiv = 9;
                }
                rStat = wrap_SDSetCardFeature(hDevice, SD_SET_CARD_INTERFACE_EX, pciOrig, sizeof(SD_CARD_INTERFACE_EX));
                if (!SD_API_SUCCESS(rStat))
                {
                    dwMinDiv = 12;
                }
            }
        }
        else
        {
            dwMinDiv = 11;
        }
    }
    if (prStat)
    {
        *prStat = rStat;
    }
    return dwMinDiv;
}

void SCF_ClockRate2_STDH(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_CARD_INTERFACE_EX pciInit, DWORD dwDiv)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    SD_CARD_INTERFACE_EX ciTest0;
    SD_CARD_INTERFACE_EX ciTest1;
    SD_CARD_INTERFACE_EX ciExpect;
    int k;
    DWORD dwStep = 0;
    BOOL bRet = TRUE;
    UINT diff = 50;
    SD_API_STATUS rStat;

    //2 Reset Clock Rate to Highest
    ZeroMemory(&ciTest0, sizeof(SD_CARD_INTERFACE_EX));
    ciTest0.ClockRate = pciInit->ClockRate;
    ciTest0.InterfaceModeEx = pciInit->InterfaceModeEx;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("SCF_ClockRate2_STDH: Presetting clock rate to fastest speed..."));
    rStat = wrap_SDSetCardFeature(hDevice, SD_SET_CARD_INTERFACE_EX, &ciTest0, sizeof(PSD_CARD_INTERFACE_EX));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1, TEXT("SCF_ClockRate2_STDH: Unable to return the clock rate to the fastest available speed %u Hz. SDSetCardFeature returned %s (0x%x) "), pciInit->ClockRate, TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to return the clock rate to the fastest available speed %u Hz. SDSetCardFeature returned %s (0x%x)"), pciInit->ClockRate, TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    if (ciTest0.ClockRate != pciInit->ClockRate)
    {
        LogFail(indent1, TEXT("SCF_ClockRate2_STDH: Unable to return the clock rate to the fastest available speed. For some reason instead of getting  %u Hz, %u Hz was returned"), pciInit->ClockRate, ciTest0.ClockRate);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to return the clock rate to the fastest available speed. For some reason instead of getting  %u Hz, %u Hz was returned"), pciInit->ClockRate, ciTest0.ClockRate);
        goto DONE;
    }
    for (k = (LONG)dwDiv; k>= 0; k--)
    {
        ZeroMemory(&ciTest0, sizeof(SD_CARD_INTERFACE_EX));
        ZeroMemory(&ciTest1, sizeof(SD_CARD_INTERFACE_EX));
        ZeroMemory(&ciExpect, sizeof(SD_CARD_INTERFACE_EX));
        dwStep = (DWORD)(pow((double) 2, (double) k));

        ciExpect.ClockRate = pciInit->ClockRate / dwStep;
        ciExpect.InterfaceModeEx = pciInit->InterfaceModeEx;

        ciTest0.ClockRate = ciExpect.ClockRate + diff;
        ciTest0.InterfaceModeEx = ciExpect.InterfaceModeEx;

        ciTest1.ClockRate = ciExpect.ClockRate - diff;
        ciTest1.InterfaceModeEx = ciExpect.InterfaceModeEx;

    //2 Above
        bRet = VerifySetCardFeatureCR(indent, pTestParams, hDevice, &ciTest0, &ciExpect, TEXT("%u Hz Above \"Highest value divided by 2^%d (%u Hz)\""), diff, k, ciExpect.ClockRate);
        if (!bRet)
        {
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate2_STDH: Bailing..."));
            goto DONE;
        }
    //2 Below
        ULONG tmp = ciExpect.ClockRate;
        if (k < (LONG)dwDiv)
        {
            ciExpect.ClockRate = ciExpect.ClockRate / 2;
        }
        bRet = VerifySetCardFeatureCR(indent, pTestParams, hDevice, &ciTest1, &ciExpect, TEXT("%u Hz Below \"Highest value divided by 2^%d (%u Hz)\""), diff, k, tmp);
        if (!bRet)
        {
            LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate2_STDH: Bailing..."));
            goto DONE;
        }
    }
DONE:
    rStat = 0;
}

void SCF_ClockRate(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_CARD_INTERFACE_EX pciInit)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    SD_CARD_INTERFACE_EX ciSlow;
    SD_CARD_INTERFACE_EX ciTest0;
    BOOL bRet = TRUE;
    PUCHAR puBuff = NULL;
    DWORD timesBRAtFull[NUMBER_OF_CYCLES];
    DWORD timesBRAtSlow[NUMBER_OF_CYCLES];
    DWORD dwTimeAvgAtFull;
    DWORD dwTimeAvgAtSlow;
    UINT numBlocks = 512;
    UINT blkSize = 512;
    UINT c;
    DWORD dwDiv;
    DWORD dwRet = 0;
    DWORD dwExpectedRate = 0;

    SD_API_STATUS rStat;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Setting SD_CARD_INTERFACE_EX Structures..."));
    ZeroMemory(&ciSlow, sizeof(SD_CARD_INTERFACE_EX));

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Determining Slowest Clock Rate..."));
    dwDiv = findMinCRDivisor(pciInit, hDevice, &rStat, &dwRet);
    if (dwDiv > 8)
    {
        switch (dwDiv)
        {
            case 9:
                ciSlow.ClockRate = dwRet;
                break;
            case 13:
                break;
            case 10:
            // Test Error, pcInit = NULL
                LogFail(indent2, TEXT("SCF_ClockRate: Probable Test Failure. pciInit = NULL..."),TranslateErrorCodes(rStat), rStat);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Probable test failure. The structure containing the initial (max) card interface is NULL.\nThis value was passed to the function determining the minimum clock rate. One can not return to the initial clock rate unless it is known."),TranslateErrorCodes(rStat), rStat);
                goto DONE;
            case 11:
            //Failure on first Set Card Feature
                LogFail(indent2, TEXT("SCF_ClockRate: SDSetCardFeature Failed on setting clock rate to 1, unable to determine minimum clock rate. Error = %s (0x%x). Bailing..."),TranslateErrorCodes(rStat), rStat);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSetCardFeature Failed on setting clock rate to 1, unable to determine minimum clock rate. Error = %s (0x%x)."),TranslateErrorCodes(rStat), rStat);
                goto DONE;
            case 12:
            //Failure on second Set Card Feature
                LogFail(indent2, TEXT("SCF_ClockRate: SDSetCardFeature Failed on returning clock rate to the initial rate. This may affect later tests. Error = %s (0x%x). Bailing..."),TranslateErrorCodes(rStat), rStat);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSetCardFeature Failed on returning clock rate to the initial rate. This may affect later tests. Error = %s (0x%x)."),TranslateErrorCodes(rStat), rStat);
                goto DONE;
        }
    }
    else
    {
        ciSlow.ClockRate = pciInit->ClockRate / (DWORD) pow((double) 2, (double) dwDiv);
    }

    ciSlow.InterfaceModeEx = pciInit->InterfaceModeEx;

//1 Part 1: Timing of Clock Rates
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Part 1: Verifying that ClockRate values are actually affecting the speed of bus requests"));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Note: due to the fact that timing functions are used in this section. wrapper functions are not used."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Allocating Memory to perform write to card."));
    if (!AllocBuffer(indent2, numBlocks * blkSize, BUFF_RANDOM, &puBuff, pTestParams->dwSeed))
    {
        LogFail(indent3, TEXT("SCF_ClockRate: Unable to allocate memory for a necessary buffer. Bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure allocating memory for a buffer that will be used to write to the card"));
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Writing to buffer to card %u times at full clock rate of %u Hz...."), NUMBER_OF_CYCLES, pciInit->ClockRate);
    bRet = TimeWrites(indent2, hDevice, timesBRAtFull, NUMBER_OF_CYCLES, puBuff, numBlocks * blkSize, pTestParams);
    if (bRet == FALSE)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Bailing..."));
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Setting Clock Rate to %u Hz...."), ciSlow.ClockRate);
    dwExpectedRate = ciSlow.ClockRate;
    rStat = wrap_SDSetCardFeature(hDevice, SD_SET_CARD_INTERFACE_EX, &ciSlow, sizeof(SD_CARD_INTERFACE_EX));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2, TEXT("SCF_ClockRate: SDSetCardFeature Failed. Error = %s (0x%x)..."), TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The attempt to set the clock rate to slowest possible speed any SD card can support (%u Hz) failed. SDSetCardFeature returned %s (0x%x)"), pciInit->ClockRate/ 256, TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Checking if clock rate returned equals slowest rate..."));
    if (ciSlow.ClockRate  != dwExpectedRate)
    {
        LogFail(indent2, TEXT("SCF_ClockRate: A Clock rate of  %u Hz was returned by SDSetCardFeature this is not equal to the expected value of %u (actual calculated slowest speed of this host), skipping to part 2..."), ciSlow.ClockRate, dwExpectedRate);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The attempt to set the clock rate to slowest possible speed any SD card can support (%u Hz) failed. SDSetCardFeature succeeded, but the clock rate returned was %u Hz"), dwExpectedRate, ciSlow.ClockRate, TranslateErrorCodes(rStat), rStat);
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Checking if clock rate listed in SDCardInfoQuery matches slowest rate as well..."));
    ZeroMemory(&ciTest0, sizeof(SD_CARD_INTERFACE_EX));
    rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_CARD_INTERFACE_EX, &ciTest0, sizeof(SD_CARD_INTERFACE_EX));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2, TEXT("SCF_ClockRate: SDCardInfoQuery Failed. Error = %s (0x%x)..."), TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The attempt to get the clock rate from the card with SDCardInfoQuery after setting the clock rate to %u Hz failed. SDCardInfoQuery returned %s (0x%x)"), dwExpectedRate, TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    if (ciTest0.ClockRate != dwExpectedRate)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_ClockRate: A Clock rate of  %u Hz was returned by SDCardInfoQuery this is not equal to expected value of %u (actual calculated slowest speed of this host), skipping to part 2..."), ciTest0.ClockRate, dwExpectedRate);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("After setting the card to the slowest possible clock rate (%u Hz), SDCardInfoQuery was used to retrieve the rate according to the card. SDSetCardInfoQuery succeeded, but the clock rate returned was %u Hz"), dwExpectedRate, ciTest0.ClockRate, TranslateErrorCodes(rStat), rStat);
    }

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Writing to buffer to card %u times at slow clock rate (%u Hz)...."), NUMBER_OF_CYCLES, ciSlow.ClockRate);
    bRet = TimeWrites(indent2, hDevice, timesBRAtSlow, NUMBER_OF_CYCLES, puBuff, numBlocks * blkSize, pTestParams);
    if (bRet == FALSE)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Bailing..."));
    }
    //2 Verify
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Averaging times (subtracting out lowest and highest values)...."), NUMBER_OF_CYCLES);
    dwTimeAvgAtFull = DWAvg(timesBRAtFull, NUMBER_OF_CYCLES);
    dwTimeAvgAtSlow= DWAvg(timesBRAtSlow, NUMBER_OF_CYCLES);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Dumping Timing...."));
    LogDebug(indent3, SDIO_ZONE_TST, TEXT(" "));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Times for writing %u blocks (at a clock Rate of %u Hz):"), numBlocks, pciInit->ClockRate);
    for (c = 0; c < NUMBER_OF_CYCLES; c++)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("Write %u:\t%u us"), c, timesBRAtFull[c]);
    }
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("Average:\t%u us"), dwTimeAvgAtFull);
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Times for writing %u blocks (at a clock Rate of %u Hz):"), numBlocks, ciSlow.ClockRate);
    for (c = 0; c < NUMBER_OF_CYCLES; c++)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("Write %u:\t%u us"), c, timesBRAtSlow[c]);
    }
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("Average:\t%u us"), dwTimeAvgAtSlow);
    LogDebug(indent3, SDIO_ZONE_TST, TEXT(" "));

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_ClockRate: Determining Success for Part 1 if the average times at full speed (%u Hz) is faster than the average time at slow speed (%u Hz)...."), pciInit->ClockRate, ciSlow.ClockRate);
    if (dwTimeAvgAtFull > dwTimeAvgAtSlow)
    {
        LogFail(indent2, TEXT("SCF_ClockRate: The average time for the full clock rate (%u Hz) was not less than the time in the slow clock rate (%u Hz)"), pciInit->ClockRate, ciSlow.ClockRate);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The average time for the full clock rate (%u Hz) was not less than the time in the slowest clock rate (%u Hz)\n Average Time at Full = %u us. Average Time at Slowest  Rate = %u us "), pciInit->ClockRate, ciSlow.ClockRate,  dwTimeAvgAtFull, dwTimeAvgAtSlow);
    }

DONE:

    DeAllocBuffer(&puBuff);
}

void SCF_BusWidth(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_CARD_INTERFACE_EX pciInit)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    PSD_COMMAND_RESPONSE pResp = NULL;
    SD_API_STATUS rStat;
    UCHAR bw = 0;
    SD_CARD_INTERFACE_EX ci, ciGet;;
    DWORD timesBW1[NUMBER_OF_CYCLES];
    DWORD timesBW4[NUMBER_OF_CYCLES];
    LPDWORD lpdwTimes;
    DWORD dwTimeAvgAt4;
    DWORD dwTimeAvgAt1;
    BOOL bRet;
    PUCHAR puBuff = NULL;
    UINT numBlocks = 512;
    UINT blkSize = 512;
    UINT c;
    SD_INTERFACE_MODE_EX im;

    ZeroMemory(&ci,sizeof(SD_CARD_INTERFACE_EX));
    ZeroMemory(&ciGet,sizeof(SD_CARD_INTERFACE_EX));
  if ( (pTestParams->sdDeviceType == SDDEVICE_MMC) ||  (pTestParams->sdDeviceType == SDDEVICE_MMCHC)  )
  {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_BusWidth: An MMC card does not support multiple bus widths, The test can not proceed."));
        SetMessageBufferAndResult(pTestParams, TPR_SKIP,TEXT("An MMC card does not support multiple bus widths, The test can not proceed."));
        goto DONE;
  }
//1 Determine if Multiple Bus Widths Are Supported
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Determining if multiple bus widths are supported by Card..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Allocating the response buffer ..."));
    AllocateResponseBuff(indent1, pTestParams, &pResp, FALSE);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Determining from SCR Register what bus widths are supported ..."));
    rStat = GetBusWidthsSupported(indent1, hDevice, pTestParams, pResp, &bw);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2, TEXT("SCF_BusWidth: A critical error occurred when trying to determine what bus widths are supported, bailing..."));
        goto DONE;
    }
    if ((bw & 0x5) != 0x5)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_BusWidth: This SDMemory card does not support multiple bus widths, The test can not proceed."));
        SetMessageBufferAndResult(pTestParams, TPR_SKIP,TEXT("This SDMemory card only claims to support %s. This test will only be run if multiple bus widths are supported."), TranslateInterfaceMode(pciInit->InterfaceModeEx));
        goto DONE;
    }
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Multiple Bus Widths are supported by card."));
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Determining if multiple bus widths are supported by this Host..."));
    if (HighestBusWidthSupportAtHost(indent1, hDevice, pTestParams, &rStat) == SD_INTERFACE_SD_MMC_1BIT)
    {
        if (SD_API_SUCCESS(rStat))
        {
            LogFail(indent2, TEXT("SCF_BusWidth: This Host does not claim to support  multiple bus widths. However the user has indicated the host is of a type known to support 4 bit bus widths."));
            SetMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("This Host does not claim to support  multiple bus widths. However the user has indicated the host is of a type known to support 4 bit bus widths."));
        }
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Bailing..."));
        goto DONE;
    }

    if (GetInterfaceMode(pciInit->InterfaceModeEx) == SD_INTERFACE_SD_MMC_1BIT)
    {
        SetInterfaceMode(ci.InterfaceModeEx, SD_INTERFACE_SD_4BIT);
        SetInterfaceMode(im, SD_INTERFACE_SD_4BIT);
        lpdwTimes =  timesBW1;
    }
    else
    {
        SetInterfaceMode(ci.InterfaceModeEx, SD_INTERFACE_SD_MMC_1BIT);
        SetInterfaceMode(im, SD_INTERFACE_SD_MMC_1BIT);
        lpdwTimes =  timesBW4;
    }
    ci.ClockRate = pciInit->ClockRate;
//1 Time in current Width
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SCF_BusWidth:  Timing writes at a bus width of %u bits"), GetInterfaceValue(pciInit->InterfaceModeEx));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Note: due to the fact that timing functions are used in this section. wrapper functions are not used."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Allocating Memory to perform write to card..."));
    if (!AllocBuffer(indent2, numBlocks * blkSize, BUFF_RANDOM, &puBuff, pTestParams->dwSeed))
    {
        LogFail(indent3, TEXT("SCF_ClockRate: Unable to allocate memory for a necessary buffer. Bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure allocating memory for a buffer that will be used to write to the card"));
        goto DONE;
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Timing writes to card at a bus width of %u bits..."), GetInterfaceValue(pciInit->InterfaceModeEx));
    bRet = TimeWrites(indent2, hDevice, lpdwTimes, NUMBER_OF_CYCLES, puBuff, numBlocks * blkSize, pTestParams);
    if (bRet == FALSE)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Bailing..."));
        goto DONE;
    }
//1 Toggle Current Width
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SCF_BusWidth:  Toggling current bus width"));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Setting Bus Width to %u Bits...."), GetInterfaceValue(ci.InterfaceModeEx));
    rStat = wrap_SDSetCardFeature(hDevice, SD_SET_CARD_INTERFACE_EX, &ci, sizeof(SD_CARD_INTERFACE_EX));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2, TEXT("SCF_BusWidth: SDSetCardFeature Failed. Error = %s (0x%x)..."), TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The attempt to set the bus width to  %u bits failed. SDSetCardFeature returned %s (0x%x)"), GetInterfaceValue(ci.InterfaceModeEx), TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    //2 Verify with returned value with SDCardInfoQuery
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Getting new Card Interface...."), GetInterfaceValue(ci.InterfaceModeEx));
    rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_CARD_INTERFACE_EX, &ciGet, sizeof(SD_CARD_INTERFACE_EX));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1, TEXT("SCF_CI: Unable to get the new card interface. Error = %s (0x%x) "),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The new card interface data is necessary for the test to proceed. However, SDCardInfoQuery failed. Error =%s (0x%x)."),TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    if (ciGet.InterfaceModeEx.uInterfaceMode!= im.uInterfaceMode)
    {
        LogFail(indent1, TEXT("SCF_CI: The InterfaceModeEx returned by  SDCardInfoQuery does not match the InterfaceModeEx set in SDSetCardFeature. Interface Mode Returned = %s, InterfaceModeEx Set = %s "),TranslateInterfaceMode(ciGet.InterfaceModeEx), TranslateInterfaceMode(ci.InterfaceModeEx));
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SCF_CI: The InterfaceModeEx returned by  SDCardInfoQuery does not match the InterfaceMode set in SDSetCardFeature. Interface Mode Returned = %s, InterfaceMode Set = %s "),TranslateInterfaceMode(ciGet.InterfaceModeEx), TranslateInterfaceMode(ci.InterfaceModeEx));
        goto DONE;
    }
//1 Time in New width
    if (GetInterfaceMode(pciInit->InterfaceModeEx) == SD_INTERFACE_SD_MMC_1BIT) lpdwTimes = timesBW4;
    else lpdwTimes = timesBW1;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SCF_BusWidth:  Timing writes at a bus width of %u bits"), GetInterfaceValue(ci.InterfaceModeEx));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Note: due to the fact that timing functions are used in this section. wrapper functions are not used."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Timing writes to card at a bus width of %u bits..."), GetInterfaceValue(ci.InterfaceModeEx));
    bRet = TimeWrites(indent2, hDevice, lpdwTimes, NUMBER_OF_CYCLES, puBuff, numBlocks * blkSize, pTestParams);
    if (bRet == FALSE)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Bailing..."));
        goto DONE;
    }
//1 Verify
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Averaging times (subtracting out lowest and highest values)...."), NUMBER_OF_CYCLES);
    dwTimeAvgAt4 = DWAvg(timesBW4, NUMBER_OF_CYCLES);
    dwTimeAvgAt1= DWAvg(timesBW1, NUMBER_OF_CYCLES);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Dumping Timing...."));
    LogDebug(indent3, SDIO_ZONE_TST, TEXT(" "));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Times for writing %u blocks (with a bus rate of 4 bits):"), numBlocks);
    for (c = 0; c < NUMBER_OF_CYCLES; c++)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("Write %u:\t%u us"), c, timesBW4[c]);
    }
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("Average:\t%u us"), dwTimeAvgAt4);
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Times for writing %u blocks (with a bus rate of 1 bits):"), numBlocks);
    for (c = 0; c < NUMBER_OF_CYCLES; c++)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("Write %u:\t%u us"), c, timesBW1[c]);
    }
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("Average:\t%u us"), dwTimeAvgAt1);
    LogDebug(indent3, SDIO_ZONE_TST, TEXT(" "));

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_BusWidth: Determining Success  if the average times at bus width of 4 bits is greater than average at 1 bit...."));
    if (dwTimeAvgAt4 > dwTimeAvgAt1)
    {
        LogFail(indent2, TEXT("SCF_BusWidth: The average time for bus widths of 4 bits is not greater than the average time for 1 bit bus widths. "));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" The average time for bus widths of 4 bits is not greater than the average time for 1 bit bus widths\n Average Time 4 bits = %u us. Average Time at 1 bit = %u us "),dwTimeAvgAt4, dwTimeAvgAt1);
    }
DONE:
    if (pResp)
    {
        free(pResp);
        pResp = NULL;
    }
    DeAllocBuffer(&puBuff);
}
void SCF_CI_Test(UINT indent, PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO pClientInfo)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    SD_API_STATUS               rStat = 0;
    SD_CARD_INTERFACE_EX           ci;
    SD_CARD_INTERFACE_EX           ci2;
    BOOL                        bOKState = FALSE;
    BOOL                        bPreErr = FALSE;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("SCF_CI: Entering Driver Test Function..."));
    ZeroMemory(&ci, sizeof(SD_CARD_INTERFACE_EX));
    ZeroMemory(&ci2, sizeof(SD_CARD_INTERFACE_EX));
//1 Good State Check
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_CI: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent2, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent2, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state initially. Since it is not, it is likely that a previous test was unable to return the card to the Transfer State."), TRUE, TRUE);
    if (bOKState == FALSE)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_CI: Bailing..."));
        goto DONE;
    }
//1 Presetting Block Size
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_CI: Presetting block Length to 512 bytes..."));
    rStat = SetBlockLength(indent2, pClientInfo->hDevice, pTestParams, NULL);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("SCF_CI: Bailing..."));
        goto DONE;
    }
//1 Get Initial CardInterface
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_CI: Getting Initial card Interface Data..."));
    rStat = wrap_SDCardInfoQuery(pClientInfo->hDevice, SD_INFO_CARD_INTERFACE_EX, &ci, sizeof(SD_CARD_INTERFACE_EX));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1, TEXT("SCF_CI: Unable to get the initial card interface values. Error = %s (0x%x) "),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The initial card interface data is necessary for the test to proceed. However, SDCardInfoQuery failed. Error =%s (0x%x)."),TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    ci2.ClockRate = ci.ClockRate;
    ci2.InterfaceModeEx = ci.InterfaceModeEx;

//1 Perform Test
    if (pTestParams->TestFlags & SD_FLAG_CI_CLOCK_RATE)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_CI: Testing The adjustment of the clock rate..."));
        SCF_ClockRate(indent2, pTestParams, pClientInfo->hDevice, &ci);
    }
    else
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_CI: Testing The adjustment of the bus width..."));
        SCF_BusWidth(indent2, pTestParams, pClientInfo->hDevice, &ci);
    }
DONE:
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SCF_CI: Cleaning up..."));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_CI: Returning to Original Clock Rate.."));
    if (ci2.ClockRate != 0)
    {
        rStat= wrap_SDSetCardFeature(pClientInfo->hDevice, SD_SET_CARD_INTERFACE_EX, &ci2, sizeof(SD_CARD_INTERFACE_EX));
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent3, TEXT("Verify_SCF: SDSetCardFeature Failed. Error = %s (0x%x)"),TranslateErrorCodes(rStat), rStat);
            SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSetCardFeature failed when trying to return the clock rate to %u Hz, and the Bus width to %u bits. Error = %s (0x%x)."), ci2.ClockRate, GetInterfaceValue(ci2.InterfaceModeEx), TranslateErrorCodes(rStat), rStat);
        }
    }
//1Good State Check
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("SCF_CI: Checking if the SD card is in the %s (%u) state..."), TranslateState(SD_STATUS_CURRENT_STATE_TRAN), SD_STATUS_CURRENT_STATE_TRAN);
    bOKState = IsState(indent3, SD_STATUS_CURRENT_STATE_TRAN, GetCurrentState(indent2, pClientInfo->hDevice, pTestParams), &bPreErr, pTestParams, TEXT("This test is required to be in the TRANSFER state upon completion."), FALSE, TRUE);

    GenerateSucessMessage(pTestParams);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SCF_CI: Exiting Driver Test Function..."));
}

SD_API_STATUS CardSelectTest(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams,PSDCLNT_CARD_INFO pCardInfo)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    SD_API_STATUS                   rStat = SD_API_STATUS_SUCCESS;
    PSD_IO_FUNCTION_ENABLE_INFO     pfe = NULL;

    rStat = wrap_SDSetCardFeature(hDevice, SD_CARD_DESELECT_REQUEST, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("CardSelectTest: Failure while deselecting slot Error = %s (0x%x)"),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure while deselecting slot. Error %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    Sleep(5000);
    //disable the slot
    rStat = wrap_SDSetCardFeature(hDevice, SD_CARD_SELECT_REQUEST, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("CardSelectTest: SDSetCardFeature failed on attempt to wrap_SDSetCardFeature(hDevice, SD_CARD_FORCE_RESET Error = %s (0x%x)"),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure while Forcing Reset. Error %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    //wait for card to be selected again in reset thread
    Sleep(5000);

    //1 make sure the card is selected
    if(!pCardInfo->CardSelected)
    {
        LogFail(indent1,TEXT("CardSelectTest: Card is not selected after SD_CARD_SELECT_REQUEST.  Client callback was not successful\n"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("CardSelectTest: Card is not selected after SD_CARD_SELECT_REQUEST.  Client callback was not successful\n"));
        goto DONE;
    }

    DWORD state = GetCurrentStateNoFailOnError(indent4,hDevice,pTestParams);

    if(state > SD_STATUS_CURRENT_STATE_DIS)
    {
        LogFail(indent1,TEXT("CardSelectTest: Card is not in a valid state after select\n"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("CardSelectTest:Card is not in a valid state after select\n"));
        goto DONE;
    }

DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SetCrdFeat: Cleaning up..."));
    if(pfe)
    {
        free(pfe);
        pfe = NULL;
    }
    return rStat;
}

SD_API_STATUS CardDeselectTest(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams,PSDCLNT_CARD_INFO pCardInfo)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    SD_API_STATUS                   rStat = SD_API_STATUS_SUCCESS;
    PSD_IO_FUNCTION_ENABLE_INFO     pfe = NULL;

    //disable the slot
    rStat = wrap_SDSetCardFeature(hDevice, SD_CARD_DESELECT_REQUEST, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("CardDeselectTest: SDSetCardFeature failed on attempt to wrap_SDSetCardFeature(hDevice, SD_CARD_FORCE_RESET Error = %s (0x%x)"),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure while Forcing Reset. Error %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }

    Sleep(2000);

    //1 make sure the card is deselected
    if(pCardInfo->CardSelected)
    {
        LogFail(indent1,TEXT("CardDeselectTest: Card is selected after SD_CARD_DESELECT_REQUEST.  Client callback was not successful\n"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("CardDeselectTest: Card is selected after SD_CARD_DESELECT_REQUEST.  Client callback was not successful\n"));
        goto DONE;
    }

    DWORD state = GetCurrentStateNoFailOnError(indent4,hDevice,pTestParams);
    if(state <= SD_STATUS_CURRENT_STATE_DIS)
    {
        LogFail(indent1,TEXT("CardDeselectTest: Card returned a valid state, even though the card was deselected\n"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("CardDeselectTest: Card returned a valid state, even though the card was deselected\n"));
        goto DONE;
    }
DONE:
    rStat = wrap_SDSetCardFeature(hDevice, SD_CARD_SELECT_REQUEST, NULL, 0);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("CardDeselectTest: SDSetCardFeature failed on attempt to wrap_SDSetCardFeature(hDevice, SD_CARD_FORCE_RESET Error = %s (0x%x)"),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure while selecting card. Error %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    //wait for card to be selected again in reset thread
    Sleep(2000);

    LogDebug(indent, SDIO_ZONE_TST, TEXT("SetCrdFeat: Cleaning up..."));
    if(pfe)
    {
        free(pfe);
        pfe = NULL;
    }
    return rStat;
}

SD_API_STATUS CardForceResetTest(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS  pTestParams,PSDCLNT_CARD_INFO pCardInfo)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    SD_API_STATUS                   rStat = SD_API_STATUS_SUCCESS;
    PSD_IO_FUNCTION_ENABLE_INFO     pfe = NULL;

    DWORD dwSlotNum = 0;
    HANDLE hEvnt = NULL;
//  hEvnt = CreateEvent(NULL, FALSE,  FALSE, TEXT("CardSelectedEvent"));

    //force reset
    rStat = wrap_SDSetCardFeature(hDevice, SD_CARD_FORCE_RESET, &dwSlotNum, sizeof(DWORD));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("CardForceResetTest: SDSetCardFeature failed on attempt to wrap_SDSetCardFeature(hDevice, SD_CARD_FORCE_RESET Error = %s (0x%x)"),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure while Forcing Reset. Error %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        goto DONE;
    }
    Sleep(2000);
    //wait for card to be selected again in reset thread
//  DWORD dwWait = WaitForSingleObject(hEvnt, 10000);
//  if (dwWait == WAIT_TIMEOUT)
//  {
//      LogFail(indent1,TEXT("CardForceResetTest: SDSetCardFeature failed on attempt to wrap_SDSetCardFeature(hDevice, SD_CARD_FORCE_RESET Error = %s (0x%x)"),TranslateErrorCodes(rStat), rStat);
//      AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure while Forcing Reset. Error %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
//      goto DONE;

//  }

    //1 make sure the card is selected
    if(!pCardInfo->CardSelected)
    {
        LogFail(indent1,TEXT("CardForceResetTest: Card is not selected after forced reset.  Client callback was not successful\n"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("CardForceResetTest: Card is not selected after forced reset.  Client callback was not successful\n"));
        goto DONE;
    }

    DWORD state = GetCurrentStateNoFailOnError(indent4,hDevice,pTestParams);

    if(state > SD_STATUS_CURRENT_STATE_DIS)
    {
        LogFail(indent1,TEXT("CardForceResetTest: Card is not in a valid state after forced reset\n"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("CardForceResetTest:Card is not in a valid state after forced reset\n"));
        goto DONE;
    }

    LogDebug(indent, SDIO_ZONE_TST,TEXT("Card state is %s\n"),TranslateState(state));
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SetCrdFeat: Cleaning up..."));
    if(pfe)
    {
        free(pfe);
        pfe = NULL;
    }
    if (hEvnt)
    {
        CloseHandle(hEvnt);
    }
    return rStat;
}

void SetCardFeatureTest(UINT indent, PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO pClientInfo)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    SD_API_STATUS                   rStat = SD_API_STATUS_SUCCESS;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SetCrdFeat: Entering Driver Test Function..."));

    LogDebug(indent1,SDIO_ZONE_TST, TEXT("SetCrdFeat: Beginning test on %s (%d) feature. "), TranslateCardFeature(pTestParams->ft), pTestParams->ft);
    switch(pTestParams->ft)
    {
        case SD_SET_CARD_INTERFACE_EX:
            SCF_CI_Test(indent2, pTestParams,pClientInfo);
            break;
        case SD_CARD_FORCE_RESET:
            rStat = CardForceResetTest(indent2, pClientInfo->hDevice, pTestParams,pClientInfo);
            break;
        case SD_CARD_SELECT_REQUEST:
            rStat = CardSelectTest(indent2, pClientInfo->hDevice, pTestParams,pClientInfo);
            break;
        case SD_CARD_DESELECT_REQUEST:
            rStat = CardDeselectTest(indent2, pClientInfo->hDevice, pTestParams,pClientInfo);
            break;
        default:
            goto SKIP;
//Note: the other two feature types are available in SDMemory as well and will be tested there.
    }
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2,TEXT("SetCrdFeat: Bailing..."));
    }
    goto DONE;

SKIP:
    LogWarn(indent1,TEXT("SetCrdFeat: This test is not yet implemented!!"));
    SetMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("The feature type %s (%d), is not yet supported by the test driver."), TranslateCardFeature(pTestParams->ft), pTestParams->ft);

DONE:
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("SetCrdFeat: Cleaning up..."));

    GenerateSucessMessage(pTestParams);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("SetCrdFeat: Exiting Driver Test Function..."));
}
