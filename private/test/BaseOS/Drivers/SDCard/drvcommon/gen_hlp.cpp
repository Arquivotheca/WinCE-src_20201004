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
// Copyright (c) Microsoft Corporation.  All rights reserved.//
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

#include "gen_hlp.h"
#include "sdtst.h"
#include "debug.h"
#include <strsafe.h>
#include <intsafe.h>

BOOL  AllocBuffer(UINT indent, UINT numUchars, SD_BUFF_TYPE bt, PUCHAR *pBUFF, DWORD dwSeed, LPCSTR txt)
{
    UINT uiNum = 0;
    UINT c = 0;
    BOOL bAllOK = FALSE;
    PUCHAR tmp;
    LPCSTR lpstr = NULL;
    LPCSTR alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    LogDebug(indent, SDIO_ZONE_TST, TEXT("AllocBuffer: Allocating buffer  and filling it..."));

    if (pBUFF == NULL)
    {
        LogFail(indent + 1, TEXT("AllocBuffer: Pointer to buffer passed in is NULL, bailing..."));
        goto DONE;
    }
    if (numUchars == 0)
    {
        LogFail(indent + 1, TEXT("AllocBuffer: Can't allocate Zero UCHAR buffer, bailing..."));
        goto DONE;
    }
    if (((txt == NULL) && (bt == BUFF_STRING)) || ((txt == NULL) && (bt == BUFF_ZSTRING)) )
    {
        bt = BUFF_ZERO;
        LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("AllocBuffer: Can't fill with empty string, so I will instead Zero the memory..."));
    }

    LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("AllocBuffer: Allocating buffer of %u uchars..."), numUchars);
    *pBUFF = (PUCHAR)VirtualAlloc(NULL, numUchars, MEM_COMMIT, PAGE_READWRITE);
    if (*pBUFF == NULL)
    {
        LogDebug(indent + 2, SDIO_ZONE_TST, TEXT("AllocBuffer: Failure allocating buffer due to probable lack of memory."));
    }
    else
    {
        switch(bt)
        {
            case BUFF_ZERO:
                ZeroMemory(*pBUFF, numUchars);
                break;
            case BUFF_HIGH:
                HighMemory(*pBUFF, numUchars);
                break;
            case BUFF_RANDOM:
                __fallthrough;
            case BUFF_RANDOM_NOSET:
                ZeroMemory(*pBUFF, numUchars);
                if (bt == BUFF_RANDOM)
                {
                    srand(dwSeed);
                }
                tmp = *pBUFF;

                if( ( c + 1 ) > 0 )
                {
                    for (c = 0; c < numUchars ; c++)
                    {
                        rand_s(&uiNum);
                        tmp[c] = (UCHAR)(uiNum & 0xff);
                    }
                }
                break;
            case BUFF_STRING:
                __fallthrough;
            case BUFF_ZSTRING:
                __fallthrough;
            case BUFF_ALPHANUMERIC:
                if (bt == BUFF_ALPHANUMERIC) lpstr = alphabet;
                else lpstr = txt;
                ZeroMemory(*pBUFF, numUchars);
                tmp = *pBUFF;
                StringCchCopyA((char *)(tmp), numUchars, "\0");
                while ((strlen((char*)(tmp)) + strlen(lpstr)) < (numUchars) )
                {
                    StringCchCatA((char *)tmp, numUchars, lpstr);
                }

                //Doing the safe subtraction for "c = numUchars - strlen((char *)(tmp));"
                if (UIntSub(numUchars, strlen((char *)(tmp)), &c) != S_OK)
                {
                    goto DONE;
                }

                if (c > 0)
                {
                        StringCchCatNA((char *)tmp, numUchars, lpstr, c - 1);
                }
                if (bt != BUFF_ZSTRING)
                {
                    tmp[(numUchars) - 1] = lpstr[c];
                }
                else
                {
                    tmp[(numUchars) - 1] = '\0';
                }
                break;

        }
        bAllOK = TRUE;
    }
DONE:
    return bAllOK;
}

void DeAllocBuffer(PUCHAR *pBUFF)
{
    if (*pBUFF)
    {
        VirtualFree(*pBUFF, 0, MEM_RELEASE);
        *pBUFF = NULL;
    }
}


LPTSTR GenerateHexString(UCHAR const *pInBuff, DWORD dwLengthInBuff)
{
    UINT c;
    TCHAR tcHexTmp[4];
    static TCHAR strHex[500 * 3] = {0};
    DWORD dwLength = 500;

    if (dwLengthInBuff == 0)
    {
        goto DONE;
    }
    if (pInBuff == NULL)
    {
        goto DONE;
    }

    if(dwLengthInBuff < 500)
    {
            dwLength = dwLengthInBuff;
    }
    ZeroMemory(strHex, dwLength* 3 * sizeof(TCHAR));

    for (c = 0; c < dwLength - 1; c++)
    {
        ZeroMemory(tcHexTmp, sizeof(tcHexTmp));
        StringCchPrintf(tcHexTmp, _countof(tcHexTmp), TEXT("%02x "),pInBuff[c]);
        StringCchCat(strHex, _countof(strHex), tcHexTmp);

    }
    ZeroMemory(tcHexTmp, sizeof(tcHexTmp));
    StringCchPrintf(tcHexTmp, _countof(tcHexTmp), TEXT("%02x "),pInBuff[c]);
    strHex[(dwLength * 3) - 3] = tcHexTmp[0];
    strHex[(dwLength * 3) - 2] = tcHexTmp[1];
    strHex[(dwLength * 3) - 1] = TCHAR('\0');

DONE:
    return strHex;
}

LPTSTR GenerateBin(const UCHAR inUC)
{
    static TCHAR strBin[10] = {0};
    UINT c;
    for (c = 0; c < 8; c++)
    {
        if (c <4)
        {
            if (inUC & (1<<(7-c)))
            {
                strBin[c] = TCHAR('1');
            }
            else
            {
                strBin[c] = TCHAR('0');
            }
        }
        if (c == 4)
        {
            strBin[c] = TCHAR(' ');
        }
        if (c >= 4)
        {
            if (inUC & (1<<(7-c)))
            {
                strBin[c + 1] = TCHAR('1');
            }
            else
            {
                strBin[c + 1] = TCHAR('0');
            }
        }
    }
    strBin[9] = TCHAR('\0');
    return strBin;
}

LPTSTR GenerateBinString(UCHAR const *pInBuff, DWORD dwLengthInBuff)
{
    UINT c;
    static TCHAR strBin[1500] = {0};
    TCHAR tmp[12];
    DWORD dwLength;

    dwLength = min((1500 / 11), dwLengthInBuff);
    if (dwLengthInBuff == 0)
    {
        goto DONE;
    }
    if (pInBuff == NULL)
    {
        goto DONE;
    }

    for (c = 0; c < dwLength; c++)
    {
        ZeroMemory(tmp, sizeof(tmp));
        _stprintf_s(tmp, _countof(tmp),  TEXT("%s  "), GenerateBin(pInBuff[c]));
        _tcscat_s(strBin, _countof(strBin),  tmp);
    }
DONE:
    return strBin;
}

BOOL AllocateTCString(UINT indent, SD_STRING_TYPE st, UCHAR const *pInBuff, DWORD dwLength, LPTSTR *ppOutBuff)
{
    UINT c;
    TCHAR tcHexTmp[4];
    BOOL bSuccess = FALSE;
    DWORD strlength = 0;
    LPTSTR tmp = NULL;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("AllocString: Note: This function does not assume a zero terminated string for pInBuff!"));
    LogDebug(indent, SDIO_ZONE_TST, TEXT("AllocString: Allocating LPTSTR and filling it..."));

    if (ppOutBuff == NULL)
    {
        LogFail(indent + 1, TEXT("AllocString: Pointer to String to be allocated is NULL, bailing..."));
        goto DONE;
    }
    if (dwLength == 0)
    {
        LogFail(indent + 1, TEXT("AllocString: Length passed in is zero, bailing..."));
        goto DONE;
    }
    if (((st == STRING_TEXT) || (st == STRING_HEX)) && (pInBuff == NULL) )
    {
        LogFail(indent + 1, TEXT("AllocString: If the SD_STRING_TYPE is STRING_TEXT or STRING_HEX, the input buffer cannot be NULL. This is not the case. Bailing..."));
        goto DONE;
    }
    switch(st)
    {
        case STRING_ZERO:
            __fallthrough;
        case STRING_HIGH:
            strlength = dwLength;
            break;
        case STRING_TEXT:
            strlength = dwLength + 1;
            break;
        case STRING_HEX:
            strlength = dwLength * 3;
    }
    *ppOutBuff = (LPTSTR)malloc(strlength * sizeof(TCHAR));
    if (*ppOutBuff == NULL)
    {
        LogFail(indent + 1, TEXT("AllocString: Failure allocating memory due to a probable lack of memory, bailing..."));
        goto DONE;
    }
    bSuccess = TRUE;
    tmp = *ppOutBuff; //dropping the cumbersom way of addressing the buffer
    if (st == STRING_HIGH)
    {
        HighMemory(tmp, strlength * sizeof(TCHAR));
        goto DONE;
    }
    ZeroMemory(tmp, strlength * sizeof(TCHAR));
    if (st == STRING_ZERO) goto DONE;

    for (c = 0; c < dwLength - 1; c++)
    {
        if (st  == STRING_HEX)
        {
            ZeroMemory(tcHexTmp, sizeof(tcHexTmp));
            _stprintf_s(tcHexTmp, _countof(tcHexTmp), TEXT("%02x "),pInBuff[c]);
            _tcscat_s(tmp, _countof(tcHexTmp), tcHexTmp);
        }
        else
        {
            if ((pInBuff[c] >= 0x20) && (pInBuff[c] <= 0x7E)) tmp[c] = TCHAR(pInBuff[c]);
            else tmp[c] = TCHAR('.');
        }

    }
    if (st  == STRING_HEX)
    {
        ZeroMemory(tcHexTmp, sizeof(tcHexTmp));
        _stprintf_s(tcHexTmp, _countof(tcHexTmp), TEXT("%02x "),pInBuff[c]);
        tmp[(dwLength * 3) - 3] = tcHexTmp[0];
        tmp[(dwLength * 3) - 2] = tcHexTmp[1];
        tmp[(dwLength * 3) - 1] = TCHAR('\0');
    }
    else
    {
        if ((pInBuff[dwLength - 1] >= 0x20) && (pInBuff[dwLength - 1] <= 0x7E)) tmp[dwLength - 1] = TCHAR(pInBuff[dwLength - 1]);
        else tmp[dwLength - 1] = TCHAR('.');
        tmp[dwLength] = TCHAR('\0');
    }

DONE:
    return bSuccess;
}

void DeAllocTCString(LPTSTR *ppStr)
{
    if (*ppStr)
    {
        free(*ppStr);
        *ppStr = NULL;
    }
}

BOOL ReportPassFail(BOOL bCondition, UINT indent, LPCTSTR leadTxt, LPCTSTR failureMsg, BOOL bTrueIsPass)
{
    BOOL bVal = TRUE;
    TCHAR buff[50] = {0};
    
    if (bTrueIsPass) bVal = bCondition;
    else bVal = !bCondition;
    if (bCondition) _tcscpy_s(buff, _countof(buff), TEXT("Yes"));
    else _tcscpy_s(buff, _countof(buff), TEXT("No"));
    if (bVal)
    {
        LogDebug(indent, SDIO_ZONE_TST, TEXT("%s: %s, Pass"), leadTxt, buff);
    }
    else
    {
        LogFail(indent, TEXT("%s: %s. %s"), leadTxt, buff, failureMsg);
    }

    return bVal;
}

void LShiftBuffer(UCHAR const *inBuffer, __out_ecount(dwBuffSize) PUCHAR outBUFF, IN DWORD dwBuffSize, IN DWORD dwBitsShifted)
{
    DWORD bitShift = 0;
    DWORD byteShift = 0;
    UCHAR ucMask = 0;
    DWORD c;

    byteShift = dwBitsShifted / 8;
    bitShift = dwBitsShifted % 8;

    for (c = 0; c < bitShift; c++)
    {
        ucMask = ucMask |( 1<<(7-c));
    }

    PREFAST_ASSERT(dwBuffSize >= (byteShift + 1));
    for (c = 0; c < (dwBuffSize - (byteShift + 1)); c++)
    {
        outBUFF[c] = (inBuffer[byteShift + c] << bitShift) | ((inBuffer[byteShift + c + 1] & ucMask) >> (8-bitShift));
    }
    outBUFF[c] = inBuffer[byteShift + c] << bitShift;
}

void RShiftBuffer(UCHAR const *inBuffer, __out_ecount(dwBuffSize) PUCHAR outBUFF, IN DWORD dwBuffSize, IN DWORD dwBitsShifted)
{
    DWORD bitShift = 0;
    DWORD byteShift = 0;
    UCHAR ucMask = 0;
    UINT c;

    PREFAST_ASSERT(dwBitsShifted < (sizeof(UCHAR) * dwBuffSize - 1));

    byteShift = dwBitsShifted / 8;
    bitShift = dwBitsShifted % 8;

    for (c = 0; c < bitShift; c++)
    {
        ucMask = ucMask |( 1<<c);
    }

    outBUFF[byteShift] = inBuffer[0] >>bitShift;

    PREFAST_ASSERT(dwBuffSize >= byteShift);
    for (c = 1; c < (dwBuffSize - byteShift); c++)
    {
        outBUFF[byteShift + c] =( (inBuffer[c - 1]  & ucMask)  << (8-bitShift)) | (inBuffer[c] >> bitShift);
    }
}

void ShiftBuffer(IN UINT indent, UCHAR const *inBuffer, OUT PUCHAR *poutBUFF, IN DWORD dwBuffSize, IN DWORD dwBitsShifted, IN BOOL bRight)
{
    PUCHAR buff = NULL;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("ShiftBuff: Verifying parameters..."));
    if (inBuffer == NULL && indent + 1 > indent)
    {
        LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("ShiftBuff: inBuffer = NULL. Cannot create a new buffer based on a NULL buffer."));
        goto DONE;
    }
    if (poutBUFF == NULL)
    {
        LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("ShiftBuff: poutBUFF = NULL. Cannot return new buffer when passed in pointer to it is NULL."));
        goto DONE;
    }
    if (dwBuffSize == 0)
    {
        LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("ShiftBuff: inBuffer = NULL. Cannot create a new buffer of size 0."));
        goto DONE;
    }

    LogDebug(indent, SDIO_ZONE_TST, TEXT("ShiftBuff: Allocating Buffer..."));
    if (AllocBuffer(indent + 1, dwBuffSize, BUFF_ZERO, &buff))
    {
        LogDebug(indent, SDIO_ZONE_TST, TEXT("ShiftBuff: Checking Shift Range..."));
        if (dwBitsShifted >= (8 * dwBuffSize -1))
        {
            LogDebug(indent + 1, SDIO_ZONE_TST, TEXT("ShiftBuff: No need to perform shift because dwBitsShifted equals or exceeds buffer size.\nBits Shifted = %u bits Buffer Size = %u bits"), dwBitsShifted, dwBuffSize * 8);
            goto DONE;
        }
        if (bRight)
        {
            LogDebug(indent, SDIO_ZONE_TST, TEXT("ShiftBuff: Shifting Buffer Right %u bits..."), dwBitsShifted);
            RShiftBuffer(inBuffer, buff, dwBuffSize, dwBitsShifted);
        }
        else
        {
            LogDebug(indent, SDIO_ZONE_TST, TEXT("ShiftBuff: Shifting Buffer Right %u bits..."), dwBitsShifted);
            LShiftBuffer(inBuffer, buff, dwBuffSize, dwBitsShifted);
        }
    }
DONE:
    if (poutBUFF != NULL)
    {
        *poutBUFF = buff;
    }
}

UCHAR ReverseUCHAR (IN UCHAR uc)
{
    UCHAR tmp = 0;
    UCHAR ucRet = 0;
    UCHAR ucMask;
    UINT c;
    for (c = 0; c < 8; c++)
    {
        ucMask = 1 << c;
        tmp  = ucMask & uc;
        if (tmp)
        {
            ucRet = ucRet | (1<<(7-c));
        }
    }
    return ucRet;
}

DWORD dwRndUp(DWORD dwDividend,  DWORD dwDivisor)
{
    DWORD dwQuotient = 0;
    if (dwDivisor == 0) return 0;
    dwQuotient = dwDividend / dwDivisor;
    if (dwDividend % dwDivisor) dwQuotient++;
    return dwQuotient;
}

BOOL SameVal(UINT first, ...)
{
    BOOL bYes = TRUE;
    UINT ui;
    if (first == STOP_SERIES) return bYes;
    va_list nums;
    va_start( nums, first );
     ui = va_arg( nums, UINT);
    while((ui != STOP_SERIES) && (bYes == TRUE) )
    {
        if (ui != first) bYes = FALSE;
        ui = va_arg( nums, UINT);
    }
    va_end( nums );
    return bYes;
}

BOOL InOrderVal(BOOL bAssend, UINT first, ...)
{
    BOOL bYes = TRUE;
    UINT ui;
    UINT prev;
    if (first == STOP_SERIES) return bYes;
    va_list nums;
    va_start( nums, first );
    prev = first;
     ui = va_arg( nums, UINT);
    while((ui != STOP_SERIES) && (bYes == TRUE) )
    {
        if (bAssend)
        {
            if (ui != prev + 1) bYes = FALSE;
        }
        else
        {
            if (prev != ui + 1) bYes = FALSE;
        }
        prev = ui;
        ui = va_arg( nums, UINT);
    }
    va_end( nums );
    return bYes;
}

