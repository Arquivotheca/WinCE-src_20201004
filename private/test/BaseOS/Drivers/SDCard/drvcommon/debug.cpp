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

#include <SDCardDDK.h>
#include "sdtst.h"
#include <tchar.h>
#include <stdio.h>
#include <sd_tst_cmn.h>
#include "cmn_drv_trans.h"

#include <stdlib.h>
#include <tchar.h>

#define MAX_NUMBER_OF_TABS  20
#define BUFF_SIZE               500
#define LARGER_BUFF_SIZE        BUFF_SIZE + 24
#define LARGEST_BUFF_SIZE       LARGER_BUFF_SIZE + MAX_NUMBER_OF_TABS

CRITICAL_SECTION cs;

#ifdef DEBUG 
    void dbgOutput(BOOL bZone, __inout_ecount(maxBufLength) LPTSTR str, UINT maxBufLength)
    {
        UINT len;
        len = _tcslen(str);
        if (len >= maxBufLength - 3)
        {
            str[maxBufLength - 3] = TCHAR('\r');
            str[maxBufLength - 2] = TCHAR('\n');
            str[maxBufLength - 1] = TCHAR('\0');
        }
        else
        {
            str[len] = TCHAR('\r');
            str[len+1] = TCHAR('\n');
            str[len + 2] = TCHAR('\0');
        }
        DbgPrintZo(bZone, (str));
    }

    void LogDebugV(BOOL bZone, LPCTSTR lpctstr, va_list pVAL)
    {
        TCHAR buff[BUFF_SIZE + 3];
        ZeroMemory(buff, sizeof(TCHAR)*BUFF_SIZE + 3);
        _vsnwprintf_s(buff, (BUFF_SIZE + 3), BUFF_SIZE, lpctstr,pVAL);
        buff[BUFF_SIZE] = TCHAR('\0');
        dbgOutput(bZone, buff, BUFF_SIZE + 3);
    }

    void LogDebugV(UINT uiIndent, BOOL bZone, LPCTSTR lpctstr, va_list pVAL)
    {
        TCHAR buff[BUFF_SIZE + 3];
        TCHAR buff2[BUFF_SIZE + MAX_NUMBER_OF_TABS + 3];
        TCHAR buff3[MAX_NUMBER_OF_TABS + 1];
        UINT c;

        ZeroMemory(buff, sizeof(TCHAR)*BUFF_SIZE + 3);
        ZeroMemory(buff2, sizeof(TCHAR)*BUFF_SIZE + MAX_NUMBER_OF_TABS + 3);
        ZeroMemory(buff3, sizeof(TCHAR)*MAX_NUMBER_OF_TABS + 1);

        for (c = 0; ((c <MAX_NUMBER_OF_TABS) && (c < uiIndent)); c++)
        {
            buff3[c] = TCHAR('\t');
        }
        buff3[c] = TCHAR('\0');
        _vsnwprintf_s(buff, (BUFF_SIZE + 3), BUFF_SIZE, lpctstr,pVAL);
        buff[BUFF_SIZE] = TCHAR('\0');
        _snwprintf_s(buff2,_countof(buff2), BUFF_SIZE + MAX_NUMBER_OF_TABS,TEXT("%s%s"),buff3,buff);
        buff2[BUFF_SIZE + MAX_NUMBER_OF_TABS] = TCHAR('\0');
        dbgOutput(bZone, buff2, BUFF_SIZE + MAX_NUMBER_OF_TABS + 3);
    }


    void LogDebugV(LPCTSTR lpctstr, va_list pVAL)
    {
        TCHAR buff[BUFF_SIZE + 3];
        ZeroMemory(buff, sizeof(TCHAR)*BUFF_SIZE + 3);
        _vsnwprintf_s(buff, (BUFF_SIZE + 3), BUFF_SIZE, lpctstr,pVAL);
        buff[BUFF_SIZE] = TCHAR('\0');
        dbgOutput(SDIO_ZONE_TST, buff, BUFF_SIZE + 3);
    }


    void LogDebug(BOOL bZone, LPCTSTR lpctstr, ...)
    {
        va_list pVal;
        va_start(pVal, lpctstr);

        LogDebugV(bZone, lpctstr, pVal);

    }

    void LogDebug(UINT uiIndent, BOOL bZone, LPCTSTR lpctstr, ...)
    {
        va_list pVal;
        va_start(pVal, lpctstr);
        LogDebugV(uiIndent, bZone, lpctstr, pVal);
    }

    void LogDebug(LPCTSTR lpctstr, ...)
    {
        va_list pVal;
        va_start(pVal, lpctstr);

        LogDebugV(lpctstr, pVal);

    }


    void LogFailV(LPCTSTR lpctstr, va_list pVAL)
    {
        TCHAR buff[BUFF_SIZE + 3];
        TCHAR buff2[LARGER_BUFF_SIZE + 3];
        ZeroMemory(buff, sizeof(TCHAR)*BUFF_SIZE + 3);
        ZeroMemory(buff2, sizeof(TCHAR)*LARGER_BUFF_SIZE + 3);

        _vsnwprintf_s(buff, (BUFF_SIZE + 3), BUFF_SIZE, lpctstr,pVAL);
        buff[BUFF_SIZE] = TCHAR('\0');
        _snwprintf_s(buff2,_countof(buff2),LARGER_BUFF_SIZE,TEXT("*** Driver Failure: %s ***"),buff);
        buff2[LARGER_BUFF_SIZE] = TCHAR('\0');
        dbgOutput(SDCARD_ZONE_ERROR, buff2, LARGER_BUFF_SIZE + 3);
    }

    void LogFailV(UINT uiIndent, LPCTSTR lpctstr, va_list pVAL)
    {
        TCHAR buff[BUFF_SIZE + 3];
        TCHAR buff2[LARGEST_BUFF_SIZE + 3];
        TCHAR buff3[MAX_NUMBER_OF_TABS + 1];
        UINT c;

        ZeroMemory(buff, sizeof(TCHAR)*BUFF_SIZE + 3);
        ZeroMemory(buff2, sizeof(TCHAR)*LARGEST_BUFF_SIZE + 3);
        ZeroMemory(buff3, sizeof(TCHAR)*MAX_NUMBER_OF_TABS + 1);

        for (c = 0; ((c <MAX_NUMBER_OF_TABS) && (c < uiIndent)); c++)
        {
            buff3[c] = TCHAR('\t');
        }
        buff3[c] = TCHAR('\0');

        _vsnwprintf_s(buff, (BUFF_SIZE + 3), BUFF_SIZE, lpctstr,pVAL);
        buff[BUFF_SIZE] = TCHAR('\0');
        _snwprintf_s(buff2,_countof(buff2),LARGEST_BUFF_SIZE,TEXT("%s*** Driver Failure: %s ***"),buff3,buff);
        buff2[LARGEST_BUFF_SIZE] = TCHAR('\0');
        dbgOutput(SDCARD_ZONE_ERROR, buff2, LARGEST_BUFF_SIZE + 3);
    }

    void LogFail(LPCTSTR lpctstr, ...)
    {
        va_list pVal;
        va_start(pVal, lpctstr);

        LogFailV(lpctstr, pVal);
    }

    void LogFail(UINT uiIndent, LPCTSTR lpctstr, ...)
    {
        va_list pVal;
        va_start(pVal, lpctstr);

        LogFailV(uiIndent, lpctstr, pVal);
    }

    void LogWarnV(LPCTSTR lpctstr, va_list pVAL)
    {
        TCHAR buff[BUFF_SIZE + 3];
        TCHAR buff2[LARGER_BUFF_SIZE + 3];
        ZeroMemory(buff, sizeof(TCHAR)*BUFF_SIZE + 3);
        ZeroMemory(buff2, sizeof(TCHAR)*LARGER_BUFF_SIZE + 3);
        _vsnwprintf_s(buff, (BUFF_SIZE + 3), BUFF_SIZE, lpctstr,pVAL);
        buff[BUFF_SIZE] = TCHAR('\0');
        _snwprintf_s(buff2,_countof(buff2),LARGER_BUFF_SIZE,TEXT("*** Driver Warning: %s ***"),buff);
        buff2[LARGER_BUFF_SIZE] = TCHAR('\0');
        dbgOutput(SDCARD_ZONE_WARN, buff2, LARGER_BUFF_SIZE + 3);
    }



    void LogWarnV(UINT uiIndent, LPCTSTR lpctstr, va_list pVAL)
    {
        TCHAR buff[BUFF_SIZE + 3];
        TCHAR buff2[LARGEST_BUFF_SIZE + 3];
        TCHAR buff3[MAX_NUMBER_OF_TABS + 1];
        UINT c;

        ZeroMemory(buff, sizeof(TCHAR)*BUFF_SIZE + 3);
        ZeroMemory(buff2, sizeof(TCHAR)*LARGEST_BUFF_SIZE + 3);
        ZeroMemory(buff3, sizeof(TCHAR)*MAX_NUMBER_OF_TABS + 1);

        for (c = 0; ((c <MAX_NUMBER_OF_TABS) && (c < uiIndent)); c++)
        {
            buff3[c] = TCHAR('\t');
        }
        buff3[c] = TCHAR('\0');
        _vsnwprintf_s(buff, (BUFF_SIZE + 3), BUFF_SIZE, lpctstr,pVAL);
        buff[BUFF_SIZE] = TCHAR('\0');
        _snwprintf_s(buff2,_countof(buff2),LARGEST_BUFF_SIZE,TEXT("%s*** Driver Warning: %s ***"), buff3, buff);
        buff2[LARGEST_BUFF_SIZE] = TCHAR('\0');
        dbgOutput(SDCARD_ZONE_WARN, buff2, LARGEST_BUFF_SIZE + 3);
    }


    void LogWarn(LPCTSTR lpctstr, ...)
    {
        va_list pVal;
        va_start(pVal, lpctstr);

        LogWarnV(lpctstr, pVal);

    }

    void LogWarn(UINT uiIndent, LPCTSTR lpctstr, ...)
    {
        va_list pVal;
        va_start(pVal, lpctstr);

        LogWarnV(uiIndent, lpctstr, pVal);

    }


    void LogAbortV(LPCTSTR lpctstr, va_list pVAL)
    {
        TCHAR buff[BUFF_SIZE + 3];
        TCHAR buff2[LARGER_BUFF_SIZE + 3];
        ZeroMemory(buff, sizeof(TCHAR)*BUFF_SIZE + 3);
        ZeroMemory(buff2, sizeof(TCHAR)*LARGER_BUFF_SIZE + 3);

        _vsnwprintf_s(buff, (BUFF_SIZE + 3), BUFF_SIZE, lpctstr,pVAL);
        buff[BUFF_SIZE] = TCHAR('\0');
        _snwprintf_s(buff2,_countof(buff2),LARGER_BUFF_SIZE,TEXT("*** Driver Abort: %s ***"),buff);
        buff2[LARGER_BUFF_SIZE] = TCHAR('\0');
        dbgOutput(SDIO_ZONE_ABORT, buff2, LARGER_BUFF_SIZE + 3);
    }

    void LogAbortV(UINT uiIndent, LPCTSTR lpctstr, va_list pVAL)
    {
        TCHAR buff[BUFF_SIZE + 3];
        TCHAR buff2[LARGEST_BUFF_SIZE + 3];
        TCHAR buff3[MAX_NUMBER_OF_TABS + 1];
        UINT c;

        ZeroMemory(buff, sizeof(TCHAR)*BUFF_SIZE + 3);
        ZeroMemory(buff2, sizeof(TCHAR)*LARGEST_BUFF_SIZE + 3);
        ZeroMemory(buff3, sizeof(TCHAR)*MAX_NUMBER_OF_TABS + 1);

        for (c = 0; ((c <MAX_NUMBER_OF_TABS) && (c < uiIndent)); c++)
        {
            buff3[c] = TCHAR('\t');
        }
        buff3[c] = TCHAR('\0');

        _vsnwprintf_s(buff, (BUFF_SIZE + 3), BUFF_SIZE, lpctstr,pVAL);
        buff[BUFF_SIZE] = TCHAR('\0');
        _snwprintf_s(buff2,_countof(buff2),LARGEST_BUFF_SIZE,TEXT("%s*** Driver Abort: %s ***"),buff3,buff);
        buff2[LARGEST_BUFF_SIZE] = TCHAR('\0');
        dbgOutput(SDIO_ZONE_ABORT, buff2, LARGEST_BUFF_SIZE + 3);
    }


    void LogAbort(LPCTSTR lpctstr, ...)
    {
        va_list pVal;
        va_start(pVal, lpctstr);

        LogFailV(lpctstr, pVal);
    }

    void LogAbort(UINT uiIndent, LPCTSTR lpctstr, ...)
    {
        va_list pVal;
        va_start(pVal, lpctstr);

        LogFailV(uiIndent, lpctstr, pVal);
    }

#else
    void dbgOutput(BOOL, LPTSTR, UINT){};

    void LogDebugV(BOOL, LPCTSTR, va_list){};
    void LogDebugV(UINT, BOOL, LPCTSTR, va_list ){};
    void LogDebugV(LPCTSTR, va_list){};

    void LogDebug(BOOL, LPCTSTR , ...){};
    void LogDebug(UINT, BOOL, LPCTSTR, ...){};
    void LogDebug(LPCTSTR, ...){};

    void LogFailV(LPCTSTR, va_list){};
    void LogFailV(UINT, LPCTSTR, va_list){};

    void LogFail(LPCTSTR, ...){};
    void LogFail(UINT, LPCTSTR, ...){};

    void LogWarnV(LPCTSTR, va_list){};
    void LogWarnV(UINT, LPCTSTR, va_list){};

    void LogWarn(LPCTSTR, ...){};
    void LogWarn(UINT, LPCTSTR, ...){};
    
    void LogAbortV(LPCTSTR, va_list){};
    void LogAbortV(UINT, LPCTSTR, va_list){};

    void LogAbort(LPCTSTR, ...){};
    void LogAbort(UINT, LPCTSTR, ...){};

#endif //if-DEBUG





void InitializeTestParamsBufferAndResult(PTEST_PARAMS pTP)
{
    InitializeCriticalSection(&cs);
    EnterCriticalSection(&cs);
    pTP->iResult = TPR_PASS;
    ZeroMemory(pTP->MessageBuffer,((MESSAGE_BUFFER_LENGTH + 1)*sizeof(TCHAR)));
    LeaveCriticalSection(&cs);
}

BOOL IsBufferEmpty(PTEST_PARAMS pTP)
{
    EnterCriticalSection(&cs);
    if (_tcscmp(pTP->MessageBuffer,TEXT(""))==0)
    {
        LeaveCriticalSection(&cs);
        return TRUE;
    }
    else
    {
        LeaveCriticalSection(&cs);
        return FALSE;
    }
}

void SetMessageBufferAndResultV(PTEST_PARAMS pTP, int iRes, LPCTSTR lpctstr, va_list pVal)
{
    EnterCriticalSection(&cs);
    int i;
    ZeroMemory(pTP->MessageBuffer,((MESSAGE_BUFFER_LENGTH + 1)*sizeof(TCHAR)));
    i = _vsntprintf_s(pTP->MessageBuffer,(MESSAGE_BUFFER_LENGTH + 1), MESSAGE_BUFFER_LENGTH,lpctstr,pVal);
    if (i < 0) pTP->bMessageOverflow = TRUE;
    pTP->MessageBuffer[MESSAGE_BUFFER_LENGTH] = TCHAR('\0');
    pTP->iResult = iRes;
    LeaveCriticalSection(&cs);
}

void SetMessageBufferAndResult(PTEST_PARAMS pTP, int iRes, LPCTSTR lpctstr, ...)
{
    va_list pVal;
    va_start(pVal, lpctstr);
    SetMessageBufferAndResultV(pTP,iRes,lpctstr,pVal);

}

void AppendMessageBufferAndResult(PTEST_PARAMS pTP, int iRes, LPCTSTR lpctstr, ...)
{

    LPTSTR pBuff = NULL;
    LPTSTR pBuff2 = NULL;
    pBuff = (LPTSTR) malloc(sizeof(TCHAR) * (MESSAGE_BUFFER_LENGTH + 1) );
    if (pBuff == NULL)
    {
        LogWarn(0, TEXT("Not enough free memory on the heap to create a success message."));
        goto DONE;
    }
    pBuff2 = (LPTSTR) malloc(sizeof(TCHAR) * (MESSAGE_BUFFER_LENGTH + 1) );
    if (pBuff2 == NULL)
    {
        LogWarn(0, TEXT("Not enough free memory on the heap to create a success message."));
        goto DONE;
    }
    int i;
    va_list pVal;
    va_start(pVal, lpctstr);
    if (_tcscmp(pTP->MessageBuffer,TEXT(""))==0)
    {
        SetMessageBufferAndResultV(pTP, iRes, lpctstr, pVal);
    }
    else
    {
        EnterCriticalSection(&cs);
        ZeroMemory(pBuff,((MESSAGE_BUFFER_LENGTH + 1)*sizeof(TCHAR)));
        ZeroMemory(pBuff2,((MESSAGE_BUFFER_LENGTH + 1)*sizeof(TCHAR)));
        wcscpy_s(pBuff,(MESSAGE_BUFFER_LENGTH + 1),pTP->MessageBuffer);
        pBuff[MESSAGE_BUFFER_LENGTH] = TCHAR('\0');
        ZeroMemory(pTP->MessageBuffer,((MESSAGE_BUFFER_LENGTH + 1)*sizeof(TCHAR)));
        i = _vsntprintf_s(pBuff2,(MESSAGE_BUFFER_LENGTH + 1),MESSAGE_BUFFER_LENGTH,lpctstr,pVal);
        if (i < 0) pTP->bMessageOverflow = TRUE;
        pBuff2[MESSAGE_BUFFER_LENGTH] = TCHAR('\0');
        i = _snwprintf_s(pTP->MessageBuffer, (MESSAGE_BUFFER_LENGTH + 1),MESSAGE_BUFFER_LENGTH, TEXT("%s\n%s"), pBuff, pBuff2);
        if (i < 0) pTP->bMessageOverflow = TRUE;
        switch (iRes)
        {
            case TPR_SKIP:
                if (pTP->iResult == TPR_PASS) pTP->iResult = iRes;
                break;
            case TPR_PASS:
                break;
            case TPR_FAIL:
                pTP->iResult = iRes;
                break;
            case TPR_ABORT:
                if ((pTP->iResult == TPR_SKIP) ||(pTP->iResult == TPR_PASS)) pTP->iResult = iRes;
                break;
            default:
                LogWarn(0, TEXT("AppendMessageBufferAndResult: attempt to set return value to unknown result. No result changes made."));
        }
        LeaveCriticalSection(&cs);
    }
DONE:
    if (pBuff)
    {
        free(pBuff);
        pBuff = NULL;
    }
    if (pBuff2)
    {
        free(pBuff2);
        pBuff2 = NULL;
    }
}

void DeleteTestBufferCriticalSection()
{
    DeleteCriticalSection(&cs);
}

void DebugPrintState(UINT uiIndent, BOOL /*bZone*/, LPCTSTR lpctstrPre, SD_DEVICE_HANDLE hDevice, PSD_CARD_STATUS pCrdStat)
{
    SD_CARD_STATUS crdStatus = 0xffffffff;
    SD_API_STATUS rStat;
    rStat = SDCardInfoQuery(hDevice, SD_INFO_CARD_STATUS, &crdStatus, sizeof(SD_CARD_STATUS));

    #ifdef DEBUG 
        TCHAR buff[MAX_NUMBER_OF_TABS + 1];
        UINT c;

        ZeroMemory(buff, sizeof(TCHAR)*MAX_NUMBER_OF_TABS + 1);

        for (c = 0; ((c <MAX_NUMBER_OF_TABS) && (c < uiIndent)); c++)
        {
            buff[c] = TCHAR('\t');
        }
        buff[c] = TCHAR('\0');
        
        if (!SD_API_SUCCESS(rStat))
        {
            DbgPrintZo(SDCARD_ZONE_ERROR, (TEXT("%s%s**************State = ERROR**************\r\n"), buff, lpctstrPre));
        }
        else
        {
            DbgPrintZo(SDCARD_ZONE_ERROR, (TEXT("%s%s**************State = %s**************\r\n"), buff, lpctstrPre, TransState(crdStatus)));
        }
    #else
        UNREFERENCED_PARAMETER(uiIndent);
        UNREFERENCED_PARAMETER(lpctstrPre);
    #endif
  
    if (pCrdStat) *pCrdStat = crdStatus;
}

void GenerateSucessMessage(PTEST_PARAMS pTP, LPCTSTR lpctstr, ...)
{
    EnterCriticalSection(&cs);
    if (pTP->iResult == TPR_PASS)
    {
        va_list pVal;
        va_start(pVal, lpctstr);
        if (_tcscmp(pTP->MessageBuffer,TEXT(""))==0)
        {
            ZeroMemory(pTP->MessageBuffer,((MESSAGE_BUFFER_LENGTH + 1)*sizeof(TCHAR)));
            _vsntprintf_s(pTP->MessageBuffer,(MESSAGE_BUFFER_LENGTH + 1), MESSAGE_BUFFER_LENGTH,lpctstr,pVal);
            pTP->MessageBuffer[MESSAGE_BUFFER_LENGTH] = TCHAR('\0');
        }
        else
        {
            LPTSTR pBuff = NULL;
            LPTSTR pBuff2 = NULL;
            pBuff = (LPTSTR) malloc(sizeof(TCHAR) * (MESSAGE_BUFFER_LENGTH + 1) );
            if (pBuff == NULL)
            {
                LogWarn(0, TEXT("Not enough free memory on the heap to create a success message."));
                goto DONE;
            }
            pBuff2 = (LPTSTR) malloc(sizeof(TCHAR) * (MESSAGE_BUFFER_LENGTH + 1) );
            if (pBuff2 == NULL)
            {
                LogWarn(0, TEXT("Not enough free memory on the heap to create a success message."));
                goto DONE;
            }
            ZeroMemory(pBuff,((MESSAGE_BUFFER_LENGTH + 1)*sizeof(TCHAR)));
            ZeroMemory(pBuff2,((MESSAGE_BUFFER_LENGTH + 1)*sizeof(TCHAR)));
            wcscpy_s(pBuff,(MESSAGE_BUFFER_LENGTH + 1),pTP->MessageBuffer);
            ZeroMemory(pTP->MessageBuffer,((MESSAGE_BUFFER_LENGTH + 1)*sizeof(TCHAR)));
            _vsntprintf_s(pBuff2,(MESSAGE_BUFFER_LENGTH + 1),MESSAGE_BUFFER_LENGTH,lpctstr,pVal);

            pBuff2[MESSAGE_BUFFER_LENGTH] = TCHAR('\0');
            _snwprintf_s(pTP->MessageBuffer, (MESSAGE_BUFFER_LENGTH + 1),MESSAGE_BUFFER_LENGTH, TEXT("%s\n%s"), pBuff, pBuff2);
DONE:
            if (pBuff)
            {
                free(pBuff);
                pBuff = NULL;
            }
            if (pBuff2)
            {
                free(pBuff2);
                pBuff2 = NULL;
            }
        }
    }
    else if (_tcscmp(pTP->MessageBuffer,TEXT("")) == 0)
    {
        _snwprintf_s(pTP->MessageBuffer, (MESSAGE_BUFFER_LENGTH + 1), MESSAGE_BUFFER_LENGTH, TEXT("Test Error. Somewhere in the test the return value was set to something other than TPR_PASS, but the return message was left blank!!!"));
    }
    LeaveCriticalSection(&cs);
}

void GenerateSucessMessage(PTEST_PARAMS pTP)
{
    static LPCTSTR msg = TEXT("Test succeeded.");
    GenerateSucessMessage(pTP, msg);
}


