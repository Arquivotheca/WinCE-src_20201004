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


/*++


Module Name:

    dbg.c

Abstract:


Environment:

    WinCE

--*/

#include "kdp.h"
#include "kdfilter.h"

int g_cFrameInCallStack = 0;

//Extended help for the fex ar command
const char g_szFexARHelp[] =
"\nfex ar code/all procname/all level a/i\n"\
"code/all : specifies the exception code that this rule should be applied to,\n"\
"           you can use a particular hexadecimal value or the all keyword to refer to all possible values.\n"\
"procname/all : specifies the process name that this rule should be applied to,\n"\
"           you can use an app name myapp.exe or the all keyword to refer to all possible procs.\n"\
"level    : specifies the handling level of the exception that this rule should be applied to,\n"\
"           you can use 1 of handled, 4 for unhandled.\n"\
"allow/ignore : specifies the action to take when this rule is applied. Allow means break into the debugger;\n"\
"           ignore means dont break into the debugger\n";

// Hardware Address breakpoint help
const char g_szlHwAddrBpHelp[] =
"\nHardware Address Breakpoint:\n" \
"\thabp ?            - Request help on HW address breakpoint commands\n" \
"\thabp set addr [RWX [width]] - Add HW breakpoint at 'address' with access type\n" \
"\t                    RWX: optional combination of data read, write and execute - default (none) is data write only\n" \
"\t                    width: number of bytes watched at given address - default is 4\n" \
"\thabp clr id       - Remove breakpoint 'id'\n" \
"\thabp support      - provide number, type and width of HW address BP supported by platform.\n" \
"\thabp lst          - provide list of HW address breakpoints\n";

// Filter exceptions help
const char g_szFilterExceptionsHelp[] =
"\nFilter Exceptions (Target Side Command):\n" \
"\nFilter Exceptions:\n" \
"\tfex ?                                             - Request help on Filter Exceptions commands\n" \
"\tfex on/off                                        - Enable or disable Filter Exceptions functionality\n" \
"\tfex ar code/all procname/all level a/i           - Add an exception filter rule (allow/ignore)\n" \
"\tfex ar ?                                          - More help about the fex ar command\n" \
"\tfex dr ruleIndex                                  - Delete an exception filter rule\n"\
"\tfex cr [rule index] [field num] [new value]       - Chance a value of a field of the given rule\n";


static WCHAR rgchBuf[256];

static const CHAR lpszUnk[]="UNKNOWN";

KDFEX_RULE_INFO g_auxRuleInfo = {0};

// If any safe CRT functions throw an unrecoverable error while in the kd,
// we will try to take down the device.
void
__cdecl
__crt_unrecoverable_error(
    const wchar_t *pszExpression,
    const wchar_t *pszFunction,
    const wchar_t *pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
{
    DBGRETAILMSG(TRUE, (L"Kd: Hit __crt_unrecoverable_error.  Rebooting."));
    NKKernelLibIoControl((HANDLE)KMOD_OAL, IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);
    NKKernelLibIoControl((HANDLE)KMOD_OAL, IOCTL_HAL_HALT, NULL, 0, NULL, 0, NULL);
    for(;;) {} 
}

void WINAPIV NKDbgPrintfW(LPCWSTR lpszFmt, ...)
{
    va_list arglist;
    va_start(arglist, lpszFmt);
    NKvDbgPrintfW(lpszFmt, arglist);
    va_end(arglist);
}

LPCHAR GetWin32ExeName(PPROCESS pProc)
{
    if (pProc->lpszProcName) {
        kdbgWtoA(pProc->lpszProcName,g_auxRuleInfo.szProcessName);
        return g_auxRuleInfo.szProcessName;
    }
    return (LPCHAR)lpszUnk;
}

LPCHAR GetWin32DLLNameFromAddress(PPROCESS pProc, DWORD dwAddress, DWORD *pdwOffsetInModule)
{
    PDLIST ptrav;
    PMODULE pModule;
    PMODULE pMatchingModule;
    DWORD dwOffset;
    DWORD dwModuleNameLength;
    
    pMatchingModule = NULL;
    dwOffset = 0;
    
    if (pProc == g_pprcNK)
    {
       // kernel address, search backward
       for (ptrav = pProc->modList.pBack; ptrav != &pProc->modList; ptrav = ptrav->pBack) 
       {
           pModule = ((PMODULELIST) ptrav)->pMod;
           if ((int) ((PMODULE) ptrav)->BasePtr > 0) 
           {
               break;
           }
           dwOffset = (dwAddress - (DWORD)pModule->BasePtr);
           if (dwOffset < pModule->e32.e32_vsize)
           {
              pMatchingModule = pModule;
              break;
           }       
        }
    }
    else
    {
        for (ptrav = pProc->modList.pFwd; ptrav != &pProc->modList; ptrav = ptrav->pFwd)
        {
           pModule = ((PMODULELIST) ptrav)->pMod;
           
           if ((int) (pModule->BasePtr) < 0)
           {
               break;
           }
           dwOffset = (dwAddress - (DWORD)pModule->BasePtr);
           if (dwOffset < pModule->e32.e32_vsize)
           {
              pMatchingModule = pModule;
              break;
           }
        }
    }

    if (pMatchingModule != NULL && pMatchingModule->lpszModName != NULL)
    {
       dwModuleNameLength = wcsnlen(pMatchingModule->lpszModName, _countof(g_auxRuleInfo.szModuleName));
       if (dwModuleNameLength < _countof(g_auxRuleInfo.szModuleName))
       {
          kdbgWtoAmax(pMatchingModule->lpszModName,g_auxRuleInfo.szModuleName, _countof(g_auxRuleInfo.szModuleName));
          if (pdwOffsetInModule != NULL)
          {
              *pdwOffsetInModule = dwOffset;
          }
          return g_auxRuleInfo.szModuleName;
       }
       else
       {
          DEBUGGERMSG (KDZONE_ALERT, ((L"  KdFilter: [ERROR] The name of module %s len=%d does not fit into the predefined buffer size %d\r\n"), pMatchingModule->lpszModName, dwModuleNameLength, _countof(g_auxRuleInfo.szModuleName)));
          return (LPCHAR)lpszUnk;
       }

    }
    else
    {
       return (LPCHAR)lpszUnk;
    }
}

void kdbgWtoA (LPCWSTR pWstr, LPCHAR pAstr) 
{
    while ((*pAstr++ = (CHAR)*pWstr++) != '\0')
    {
        /* nothing */
    }
}


void kdbgWtoAmax (LPCWSTR pWstr, LPCHAR pAstr, int max)
{
    while ((max--) && (*pAstr++ = (CHAR)*pWstr++) != '\0')
    {
        /* nothing */
    }
}


DWORD kdbgwcslen(LPCWSTR pWstr)
{
    const wchar_t *eos = pWstr;
    while (*eos++);
    return ((size_t)(eos - pWstr - 1));
}


static BOOL sprintf_temp(
    PCHAR   pOutBuf,
    PUSHORT pOutBufIndex,
    PUSHORT pMaxBytes,
    LPCWSTR  lpszFmt,
    ...
    )
{
    WCHAR szTempString[256];
    WORD wLen;

    //
    // Write to a temporary string
    //
    NKwvsprintfW(szTempString, lpszFmt,
        (LPVOID) ( ((DWORD)&lpszFmt) + sizeof(lpszFmt) ),
        sizeof(szTempString) / sizeof(WCHAR));

    wLen = (WORD) kdbgwcslen(szTempString);

    if ((*pMaxBytes) < wLen) {
        // Not enough space
        return FALSE;
    }
    //
    // Convert to ASCII and write it to the output buffer
    //
    kdbgWtoA( szTempString, (CHAR*) &(pOutBuf[*pOutBufIndex]) );
    //
    // Don't include the NULL. Instead manually update index to include null at
    // the very end.
    //
    (*pOutBufIndex) += wLen;
    (*pMaxBytes) -= wLen;

    return TRUE;
}


// Like library function, returns number of chars copied or -1 on overflow

int snprintf(LPSTR pszBuf, UINT cBufLen, LPCWSTR pszFmt, ...)
{
    WCHAR szTempString[256];
    UINT cOutLen, cStrLen;

    // Do nothing if no buffer
    if (cBufLen == 0)
        return -1;

    NKwvsprintfW(szTempString, pszFmt, (va_list) ((&pszFmt) + 1), lengthof(szTempString));
    cStrLen = kdbgwcslen(szTempString);
    cOutLen = min(cBufLen - 1, cStrLen);
    szTempString[cOutLen] = L'\0';
    kdbgWtoA(szTempString, pszBuf);

    return (cBufLen > cStrLen ? cOutLen : -1);
}

typedef struct _MANAGED_BUFFER
{
  LPSTR   pBuffer;
  UINT   *puSize;
  UINT   *puOffset;
  
} MANAGED_BUFFER;

BOOL ManagedBufferAppend(MANAGED_BUFFER *pManBuffer, DWORD paddingFixedLen, LPCWSTR pszFmt, ...)
{
    BOOL fSuccess;
    WCHAR szTempString[256];
    UINT cOutLen, cStrLen, cBufLen;

    // Do nothing if no buffer
    if (*(pManBuffer->puSize) > 0 && *(pManBuffer->puSize) > *(pManBuffer->puOffset))
    {
       NKwvsprintfW(szTempString, pszFmt, (va_list) ((&pszFmt) + 1), lengthof(szTempString));
       cStrLen = kdbgwcslen(szTempString);
       cBufLen = *(pManBuffer->puSize) - *(pManBuffer->puOffset);

       if ((cStrLen < paddingFixedLen) && (paddingFixedLen < ((cBufLen-1))))
       {
          while (cStrLen < paddingFixedLen)
          {
             szTempString[cStrLen] = L' ';
             cStrLen++;
          }
       }
       
       cOutLen = min(cBufLen - 1, cStrLen);

       szTempString[cOutLen] = L'\0';
       kdbgWtoA(szTempString, &(pManBuffer->pBuffer[*(pManBuffer->puOffset)]));

       *(pManBuffer->puOffset) += cOutLen;
       fSuccess = (cBufLen > cStrLen);
    }
    else
    {
       fSuccess = FALSE;
    }

    return fSuccess;
}

unsigned long Kdstrtoul (LPCSTR lpStr, ULONG radix)
{
    unsigned long i = 0;
    char c;
    c = *lpStr++;

    // Skip whitespace
    while ((c==0x20) || (c>=0x09 && c<=0x0D))
    {
        c = *lpStr++;
    }

    // Auto detect radix if radix == 0
    if (!radix)
    {
        radix = (c != '0') ? 10 : ((*lpStr == 'x' || *lpStr == 'X')) ? 16 : 8;
    }

    // Skip Hex prefix if present
    if ((radix == 16) && (c == '0') && (*lpStr == 'x' || *lpStr == 'X'))
    {
        ++lpStr;
        c = *lpStr++;
    }

    if (radix < 2 || radix > 36) // sanity check
    {
        DEBUGGERMSG(KDZONE_DBG, (L"Invalid base (%u) passed to strtoul!\r\n", radix));
        i = ULONG_MAX;
    }
    else
    {
        unsigned long n;
        unsigned long oflow_mul = (unsigned long)(ULONG_MAX / (unsigned long) radix);
        unsigned long oflow_add = (unsigned long)(ULONG_MAX % (unsigned long) radix);

        // Convert string to number
        while((c >= '0' && c <= '9') || // numeric
              (c >= 'a' && c <= 'z') || // lower-case alpha
              (c >= 'A' && c <= 'Z'))   // upper-case alpha
        {
            if (c >= '0' && c <= '9')
                n = (c - '0');
            else if (c >= 'a' && c <= 'z')
                n = (10 + c - 'a');
            else // if (c >= 'A' && c <= 'Z')
                n = (10 + c - 'A');

            // Validate input
            if (n >= (unsigned long) radix)
            {
                i = ULONG_MAX;
                break;
            }

            // Check for overflow
            if (i > oflow_mul || (i == oflow_mul && n > oflow_add))
            {
                i = ULONG_MAX;
                break;
            }

            i = (i * radix) + n;
            c = *lpStr++;
        }
    }
    return i;
}


// We get isspace in headers but not imports, so we need to roll our own

int Kdp_isspace(char ch)
{
    return (ch == ' ' || (ch > 9 && ch < 13) ? 1 : 0);
}


/*++

Routine Name:

    ParseParamString

Routine Description:

    Part of the FlexPTI parser. This routine parses a parameter string into
    individual parameters.

Arguments:

    pszParamString  - [in/out] Pointer to parameter string

Return Value:

    Number of parameters parsed. On return, pszParamString will contain the
    address of the first parameter.

--*/

static UINT ParseParamString(LPSTR *ppszParamString)
{
    LPSTR pszCurPos = *ppszParamString;
    UINT nParams = 0;
    LPSTR pszCopyPos;

    // Skip leading whitespace
    while(Kdp_isspace(*pszCurPos))
        pszCurPos++;

    // Store start of parameter string
    *ppszParamString = pszCurPos;

    while(*pszCurPos != '\0')
    {
        pszCopyPos = pszCurPos;

        // Find first whitespace outside of quotes. As an added bonus, we copy
        // over quotes here so """f""o"o becomes foo.
        while ((pszCopyPos <= pszCurPos) &&
               (*pszCurPos != '\0') &&                 // not end of string and
               ((!Kdp_isspace(*pszCurPos)) ||         // haven't found a space or
                ((pszCurPos - pszCopyPos) % 2 != 0)))    // in the middle of quotes
        {
            // If we have a quote, this desyncs pszCopyPos and pszCurPos so we
            // start copying characters backward. The loop test prevents us
            // from exiting between quotes unless we hit the end of the string.
            if (*pszCurPos != '"')
            {
                *pszCopyPos = *pszCurPos;
                pszCopyPos++;
            }

            pszCurPos++;
        }

        // Null-terminate the end of the parameter
        while(pszCopyPos < pszCurPos)
           *pszCopyPos++ = '\0';

        // Skip trailing whitespace
        while(Kdp_isspace(*pszCurPos))
            *pszCurPos++ = '\0';

        nParams++;
    }

    return nParams;
}


/*++

Routine Name:

    GetNextParam

Routine Description:

    Part of the FlexPTI parser. This routine finds the offset of the next
    parameter in the parameter string. The parser splits the parameter string
    into individual parameters (like argc/argv). To avoid memory allocation, it
    leaves the parameters in-place and nulls out characters in the middle.

    It is imperative that the caller verify that there is another parameter in
    the string or this function may crash.

Arguments:

    pszCurParam     - [in]     Pointer to current parameter

Return Value:

    A pointer to the next parameter string is returned.

--*/

static LPCSTR GetNextParam(LPCSTR pszCurParam)
{
    // Find end of current string
    while(*pszCurParam != '\0')
        pszCurParam++;

    // Find start of next string
    while(*pszCurParam == '\0')
        pszCurParam++;

    return pszCurParam;
}


#define AppendImmByteToOutBuf_M(outbuf,immbyte,outbidx) {(outbuf) [(outbidx)++] = (immbyte);}
#define AppendObjToOutBuf_M(outbuf,obj,outbidx) {memcpy (&((outbuf) [(outbidx)]), &(obj), sizeof (obj)); (outbidx) += sizeof (obj);}
#define AppendStringZToOutBuf_M(outbuf,sz,outbidx) {memcpy (&((outbuf) [(outbidx)]), sz, (strlen (sz) + 1)); (outbidx) += (USHORT)((strlen (sz) + 1));}
#define AppendErrorToOutBuf_M(outbuf,err,outbidx,len) AppendErrorToOutBuf(outbuf, &outbidx, len, err)


void AppendErrorToOutBuf(LPSTR pszOutBuf, UINT *pnOutIndex, UINT nLen, NTSTATUS status)
{
    // Normalize buffer/length to skip block
    UINT nOutIndex = *pnOutIndex;
    pszOutBuf += nOutIndex;
    if ((nLen > nOutIndex) && (nLen > 0))
    {
        nLen -= nOutIndex;
        switch(status)
        {
        case STATUS_ACCESS_VIOLATION:
            strncpy_s(pszOutBuf, nLen, "ERROR: Access Violation", _TRUNCATE);
            pszOutBuf[nLen - 1] = '\0';
            break;
        case STATUS_DATATYPE_MISALIGNMENT:
            strncpy_s(pszOutBuf, nLen, "ERROR: Datatype Misalignment", _TRUNCATE);
            pszOutBuf[nLen - 1] = '\0';
            break;
        case STATUS_INVALID_PARAMETER:
            strncpy_s(pszOutBuf, nLen, "ERROR: Invalid Parameter", _TRUNCATE);
            pszOutBuf[nLen - 1] = '\0';
            break;
        case STATUS_BUFFER_TOO_SMALL:
            strncpy_s(pszOutBuf, nLen, "ERROR: Buffer too small", _TRUNCATE);
            pszOutBuf[nLen - 1] = '\0';
            break;
        case STATUS_INTERNAL_ERROR:
            strncpy_s(pszOutBuf, nLen, "ERROR: Internal error. Check COM1 debug output", _TRUNCATE);
            pszOutBuf[nLen - 1] = '\0';
            break;
        default:
            snprintf(pszOutBuf, nLen, L"ERROR: Unknown Error (%08lX)", status);
            break;
        }

        // Update buffer index
        *pnOutIndex += strlen(pszOutBuf);
    }
}


/*++

Routine Name:

    MarshalCriticalSectionInfo

Routine Description:

    Copy Critical Section Info in output buffer

Arguments:

    OutBuf          - Supplies and returns pointer to output buffer
    pOutBufIndex    - Supplies and returns pointer to output buffer index
    NbMaxOutByte    - Supplies maximum number of bytes that can be written in output buffer

Return Value:

    TRUE if succeed (size OK) otherwise FALSE.

--*/

BOOL
MarshalCriticalSectionInfo (
    IN OUT PCHAR OutBuf,
    IN OUT PUSHORT pOutBufIndex,
    IN USHORT nMaxBytes
    )
{
    PROCESS *pProc = g_pprcNK;
    THREAD  *pThread = 0;
    PPROXY   pProxy;

    sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("Critical Section Info (shows synchronization object status)\n"));

    do // for each Process
    {
        sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT(" pProcess : 0x%08X %s\n"), pProc, pProc ? pProc->lpszProcName : L"null");
        pThread = (THREAD *) (pProc->thrdList.pFwd); // Get process main thread
        while (pThread && (&(pProc->thrdList) != (DLIST *) pThread))
        {
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\tpThread 0x%08X\n"), pThread);
            
            pProxy = pThread->lpProxy;
            while (pProxy != NULL)
            {
                switch(pProxy->bType)
                {
                    case HT_CRITSEC :
                    {
                        PMUTEX pCrit = (PMUTEX) pProxy->pObject; 
                        sprintf_temp (OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\t\tpCritSect 0x%08X (pOwner 0x%X)\n"), pCrit, pCrit->pOwner->dwId);
                        break;
                    }
                    
                    case HT_EVENT :
                    {
                        PEVENT pEvent = (PEVENT) pProxy->pObject;
                        sprintf_temp (OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\t\tpEvent 0x%08X\n"), pEvent);
                        break;
                    }

                    default :
                    {
                        sprintf_temp (OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\t\tUnknown object type %d (0x%08X)\n"), pProxy->bType, pProxy->pObject);
                        break;
                    }
                }
                pProxy = pProxy->pThLinkNext;
            }
            pThread = (THREAD *) pThread->thLink.pFwd;
        }
        pProc = (PROCESS *) pProc->prclist.pFwd;
    } while (pProc != NULL && pProc != g_pprcNK);
    

    // Include the NULL
    (*pOutBufIndex)++;

    return TRUE;
} // End of MarshalCriticalSectionInfo


/*++

Routine Name:

    MarshalIOReadWrite

Routine Description:

    Copy Critical Section Info in output buffer

Arguments:

    OutBuf          - Supplies and returns pointer to output buffer
    pOutBufIndex    - Supplies and returns pointer to output buffer index
    nMaxBytes       - Supplies maximum number of bytes that can be written in output buffer
    fIORead         - TRUE indicate Read otherwise Write
    dwAddress       - Address to read /write
    dwSize          - Size to read /write
    dwData          - Data to write

Return Value:

    TRUE if succeed (size OK) otherwise FALSE.

--*/

BOOL
MarshalIOReadWrite (
    IN OUT PCHAR OutBuf,
    IN OUT PUSHORT pOutBufIndex,
    IN USHORT nMaxBytes,
    BOOL fIORead,
    DWORD dwAddress,
    DWORD dwSize,
    DWORD dwData
    )
{
    DBGKD_COMMAND dbgkdCmdPacket;
    STRING strIO;
    dbgkdCmdPacket.u.ReadWriteIo.qwTgtIoAddress = dwAddress;
    dbgkdCmdPacket.u.ReadWriteIo.dwDataSize = dwSize;
    dbgkdCmdPacket.u.ReadWriteIo.dwDataValue = dwData;
    strIO.Length = 0;
    strIO.Buffer = 0;
    strIO.MaximumLength = 0;

    if (fIORead)
    {
        sprintf_temp (OutBuf, pOutBufIndex, &nMaxBytes, TEXT("IO Read: Address=0x%08X, Size=%u  ==>  "),dwAddress,dwSize);
        KdpReadIoSpace (&dbgkdCmdPacket, &strIO, FALSE);
    }
    else
    {
        sprintf_temp (OutBuf, pOutBufIndex, &nMaxBytes, TEXT("IO Write: Address=0x%08X, Size=%u, Data=0x%08X  ==>  "),dwAddress,dwSize,dwData);
        KdpWriteIoSpace (&dbgkdCmdPacket, &strIO, FALSE);
    }

    switch (dbgkdCmdPacket.dwReturnStatus)
    {
        case STATUS_SUCCESS:
            if (fIORead)
            {
                sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("Success: Data=0x%08X\n"),dbgkdCmdPacket.u.ReadWriteIo.dwDataValue);
            }
            else
            {
                sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("Success\n"));
            }
            break;
        case STATUS_ACCESS_VIOLATION:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Access Violation\n"));
            break;
        case STATUS_DATATYPE_MISALIGNMENT:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Datatype Misalignment\n"));
            break;
        case STATUS_INVALID_PARAMETER:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Invalid Parameter\n"));
            break;
        default:
            sprintf_temp(OutBuf, pOutBufIndex, &nMaxBytes, TEXT("ERROR: Unknown Error\n"));
            break;
    }

    // Include the NULL
    (*pOutBufIndex)++;

    return TRUE;
} // End of MarshalCriticalSectionInfo


static BOOL MarshalKdFexChangeRule(long ruleIndex, long fieldNumber, LPCSTR pszNewFieldValue)
{
   BOOL bRet = FALSE;
   DWORD dwHandlingLevel = 0;
   KDFEX_RULE_INFO *pRuleInfo = GetFilterRulePtr((int)ruleIndex);

   if ((pRuleInfo != NULL) && (pRuleInfo->fgRelevanceFlags != KDFEX_RULE_RELEVANCE_NONE))
   {
      switch(fieldNumber)
      {
         case 1:
            //trying to change the exception code field.
            if (strcmp(pszNewFieldValue, KDFEX_ALL) != 0)
            {
              pRuleInfo->dwExceptionCode = (DWORD)Kdstrtoul(pszNewFieldValue, 0);
              pRuleInfo->fgRelevanceFlags |= KDFEX_RULE_RELEVANCE_CODE;
            }
            else
            {
              pRuleInfo->dwExceptionCode = 0;
              pRuleInfo->fgRelevanceFlags &= (~ (DWORD)KDFEX_RULE_RELEVANCE_CODE);
            }
            break;
            
         case 2:
            //trying to change the process name field.
            if (strcmp(pszNewFieldValue, KDFEX_ALL) != 0)
            {
              snprintf(pRuleInfo->szProcessName, sizeof(pRuleInfo->szProcessName), L"%S", pszNewFieldValue);
              pRuleInfo->fgRelevanceFlags |= KDFEX_RULE_RELEVANCE_PROCESS;
            }
            else
            {
              pRuleInfo->fgRelevanceFlags &= (~ (DWORD)KDFEX_RULE_RELEVANCE_PROCESS);
            }
            break;
            
         case 3:
            //trying to change the handling level field
            dwHandlingLevel = (DWORD)Kdstrtoul(pszNewFieldValue, 0); 
            if (dwHandlingLevel < (DWORD)KDFEX_HANDLING_LAST)
            {
               pRuleInfo->fgRelevanceFlags |= KDFEX_RULE_RELEVANCE_HANDLING;
               pRuleInfo->handlingLevel = (KDFEX_HANDLING)dwHandlingLevel;
            }
            break;
            
         case 4:
            //trying to change the rule action
            if ((pszNewFieldValue[0] == 'a') || (pszNewFieldValue[0] == 'b'))
            {
              pRuleInfo->decision = KDFEX_RULE_DECISION_BREAK;
            }
            else if (pszNewFieldValue[0] == 'i')
            {
              pRuleInfo->decision = KDFEX_RULE_DECISION_IGNORE;
            }
            else if (pszNewFieldValue[0] == 'h')
            {
              pRuleInfo->decision = KDFEX_RULE_DECISION_ABSORB;
            }
            else
            {
              pRuleInfo->decision = KDFEX_RULE_DECISION_BREAK;
            }
            break;
            
         default:
            //Unknown field... not supported in this version?
            break;
      }
   }
   return bRet;
}

/*++

Routine Name:

    MarshalFilterExceptionsCmd

Routine Description:

    Processes a filter exceptions request. This is called from the 'fex' command in ProcessAdvancedCmd.
    Null-terminated text for the end user is stored in the buffer.

Arguments:

    pszBuffer       - [in]     Pointer to buffer
    cBufLen         - [in]     Length of buffer
    pszParams       - [in]     Pointer to first parameter
    cParams         - [in]     Number of parameters

Return Value:

    On success: TRUE
    On error:   FALSE, no additional information is available

--*/

static BOOL MarshalFilterExceptionsCmd(LPSTR pszBuffer, UINT cBufLen, LPCSTR pszParams, UINT cParams)
{
    UINT cOffset = 0;
    NTSTATUS status;
    UINT i, iField;
    BOOL fDisplayStatus = TRUE;

    DEBUGGERMSG(KDZONE_DBG, (L"++MarshalFilterExceptionsCmd\r\n"));

    if (cParams >= 1 && !strcmp(pszParams, "on"))
    {
        g_fFilterExceptions = TRUE;
    }
    else if (cParams >= 1 && !strcmp(pszParams, "off"))
    {
        g_fFilterExceptions = FALSE;
    }
    else if (cParams >= 1 && !strcmp(pszParams, "ar"))
    {
        cParams--;
        if (cParams >= 3)
        {
           DWORD dwFexChance = 0;
           
           memset(&g_auxRuleInfo, 0, sizeof(g_auxRuleInfo));
           g_auxRuleInfo.dwSize = sizeof(g_auxRuleInfo);
           g_auxRuleInfo.fgRelevanceFlags = (KDFEX_RULE_RELEVANCE_GENERAL);
           g_auxRuleInfo.handlingLevel= KDFEX_HANDLING_HANDLED;
           g_auxRuleInfo.decision = KDFEX_RULE_DECISION_BREAK;
           
           pszParams = GetNextParam(pszParams);
           cParams--;
           //First param refers to "Code"
           if (strcmp(pszParams, KDFEX_ALL) != 0)
           {
              g_auxRuleInfo.dwExceptionCode = (DWORD)Kdstrtoul(pszParams, 0);
              g_auxRuleInfo.fgRelevanceFlags |= (KDFEX_RULE_RELEVANCE_CODE);
           }
           //Next param refers to "procname"
           pszParams = GetNextParam(pszParams);
           cParams--;
           if (strcmp(pszParams, KDFEX_ALL) != 0)
           {
              if (snprintf(g_auxRuleInfo.szProcessName, sizeof(g_auxRuleInfo.szProcessName), L"%S", pszParams) < 0)
              {
                 status = STATUS_INVALID_PARAMETER;
                 goto Exit;
              }
              g_auxRuleInfo.fgRelevanceFlags |= (KDFEX_RULE_RELEVANCE_PROCESS);
           }
              
           //Next param refers to "chance"
           pszParams = GetNextParam(pszParams);
           cParams--;
           dwFexChance = (DWORD)Kdstrtoul(pszParams, 0);
           if (dwFexChance < KDFEX_HANDLING_LAST)
           {
              g_auxRuleInfo.handlingLevel = (KDFEX_HANDLING)dwFexChance;
              g_auxRuleInfo.fgRelevanceFlags |= (KDFEX_RULE_RELEVANCE_HANDLING);
           }
           
           if (cParams > 0)
           {
              pszParams = GetNextParam(pszParams);
              cParams--;
              if (pszParams[0] == 'i')
              {
                 g_auxRuleInfo.decision = KDFEX_RULE_DECISION_IGNORE;
              }
              else if (pszParams[0] == 'h')
              {
                 g_auxRuleInfo.decision = KDFEX_RULE_DECISION_ABSORB;
              }
           }
           
           if (AddFilterRule(&g_auxRuleInfo) < 0)
           {
              status = STATUS_INVALID_PARAMETER;
              goto Exit;
           }
        }
        else if (cParams == 1)
        {
           pszParams = GetNextParam(pszParams);
           cParams--;
           if (strcmp(pszParams, "?") == 0)
           {
              memcpy(&pszBuffer[cOffset], g_szFexARHelp, sizeof(g_szFexARHelp));
              cOffset += (lengthof(g_szFexARHelp) - 1);
              fDisplayStatus = FALSE;
           }
           else
           {
              status = STATUS_INVALID_PARAMETER;
              goto Exit;
           }
        }
        else
        {
           status = STATUS_INVALID_PARAMETER;
           goto Exit;
        }
    }
    else if (cParams >= 1 && !strcmp(pszParams, "dr"))
    {
        cParams--;
        if (cParams > 0)
        {
           pszParams = GetNextParam(pszParams);
           cParams--;
           i = Kdstrtoul(pszParams, 0);
           RemoveFilterRule(i);
        }
        else
        {
           status = STATUS_INVALID_PARAMETER;
           goto Exit;
        }
    }
    else if (cParams >= 1 && !strcmp(pszParams, "cr"))
    {
        cParams--;
        if (cParams >= 3)
        {
           pszParams = GetNextParam(pszParams);
           cParams--;
           i = Kdstrtoul(pszParams, 0);
           pszParams = GetNextParam(pszParams);
           cParams--;
           iField = Kdstrtoul(pszParams, 0);
           pszParams = GetNextParam(pszParams);
           cParams--;
           if (!MarshalKdFexChangeRule(i, iField, pszParams))
           {
              status = STATUS_INVALID_PARAMETER;
              goto Exit;
           }
        }
        else
        {
           status = STATUS_INVALID_PARAMETER;
           goto Exit;
        }
    }
    else if (cParams > 0)
    {
        fDisplayStatus = FALSE;
        // "fex ?" - Give information about fex command

        if (cParams >= 1 && strcmp(pszParams, "?") != 0)
        {
            // User entered an unknown command, so issue a warning
            cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Unknown command '%a'\n", pszParams);
        }

        if ((cBufLen <=  cOffset) ||
            ((cBufLen - cOffset) < lengthof(g_szFilterExceptionsHelp))) // Output buffer to small
        {
            status = STATUS_BUFFER_TOO_SMALL;
            goto Exit;
        }

        memcpy(&pszBuffer[cOffset], g_szFilterExceptionsHelp, sizeof(g_szFilterExceptionsHelp));
        cOffset += (lengthof(g_szFilterExceptionsHelp) - 1);
    }

    if (fDisplayStatus)
    {
        MANAGED_BUFFER managedBuffer = {0};
        
        if (cBufLen <=  cOffset)
        {
            status = STATUS_BUFFER_TOO_SMALL;
            goto Exit;
        }
   
        managedBuffer.pBuffer = pszBuffer;
        managedBuffer.puSize = &cBufLen;
        managedBuffer.puOffset = &cOffset;

        ManagedBufferAppend(&managedBuffer, 0, L"\nFilter Exceptions status: %s\n", g_fFilterExceptions ? L"ON" : L"OFF");
        
        if (g_fFilterExceptions)
        {
           int iter;
           KDFEX_RULE_INFO *pFexRule = NULL;
                 
           for (iter = 0; iter < KDFEX_MAXRULES; iter++)
           {
               if (!ManagedBufferAppend(&managedBuffer, 0, L" \n"))
               {
                  status = STATUS_BUFFER_TOO_SMALL;
                  goto Exit;
               }
               pFexRule = GetFilterRulePtr(iter);

               ManagedBufferAppend(&managedBuffer, ((iter < 10) ? 8 : 7), L"  Rule");
               ManagedBufferAppend(&managedBuffer, 0, L"%u: ", iter);

               if (pFexRule->fgRelevanceFlags != KDFEX_RULE_RELEVANCE_NONE)
               {
                  ManagedBufferAppend(&managedBuffer, 10, L"[%s] ", GetFilterRuleDecisionName(pFexRule->decision));
                  ManagedBufferAppend(&managedBuffer, 0, L"Exception Code: ");
                  if (IsRelevantField(pFexRule->fgRelevanceFlags, KDFEX_RULE_RELEVANCE_CODE))
                  {
                     ManagedBufferAppend(&managedBuffer, 11, L"0x%08X ", pFexRule->dwExceptionCode);
                  }
                  else
                  {
                     ManagedBufferAppend(&managedBuffer, 11, L"Any ");
                  }
                  ManagedBufferAppend(&managedBuffer, 0, L"  Handling: ");
                  if (IsRelevantField(pFexRule->fgRelevanceFlags, KDFEX_RULE_RELEVANCE_HANDLING))
                  {
                     ManagedBufferAppend(&managedBuffer, 13, L"%s ", GetHandlingLevelTypeName(pFexRule->handlingLevel));
                  }
                  else
                  {
                     ManagedBufferAppend(&managedBuffer, 13, L"Any ");
                  }
                  ManagedBufferAppend(&managedBuffer, 0, L"Process: ");
                  if (IsRelevantField(pFexRule->fgRelevanceFlags, KDFEX_RULE_RELEVANCE_PROCESS))
                  {        
                     ManagedBufferAppend(&managedBuffer, 18, L"%S ", pFexRule->szProcessName);
                  }
                  else
                  {
                     ManagedBufferAppend(&managedBuffer, 18, L"Any ");
                  }
                  ManagedBufferAppend(&managedBuffer, 0, L"Module: ");
                  if (IsRelevantField(pFexRule->fgRelevanceFlags, KDFEX_RULE_RELEVANCE_MODULE))
                  {
                     ManagedBufferAppend(&managedBuffer, 0, L"%S  ", pFexRule->szModuleName);
                  }
                  else
                  {
                     ManagedBufferAppend(&managedBuffer, 0, L"Any  ");
                  }
                  if (IsRelevantField(pFexRule->fgRelevanceFlags, KDFEX_RULE_RELEVANCE_ADDRESS))
                  {
                     ManagedBufferAppend(&managedBuffer, 0, L"[0x%08X - 0x%08X]", pFexRule->dwFirstAddress, pFexRule->dwLastAddress);
                  }               
               }
               else
               {
                  ManagedBufferAppend(&managedBuffer, 0, L"Empty", iter);
               }
           }
           ManagedBufferAppend(&managedBuffer, 0, L" \n");
        }
    }
    
    status = STATUS_SUCCESS;

Exit:

    if (status != STATUS_SUCCESS)
    {
        AppendErrorToOutBuf_M(pszBuffer, status, cOffset, cBufLen);
    }
    
    DEBUGGERMSG(KDZONE_DBG, (L"--MarshalFilterExceptionsCmd: st=0x%08X\r\n", status));

    return ((cBufLen > cOffset) && ((cBufLen - cOffset) > 1) ? TRUE : FALSE);
}

// Tags:
typedef enum _PROC_THREAD_INFO_TAGS
{
    PtitagsStartProcessInfoDescFieldV1,
    PtitagsStartThreadInfoDescFieldV1,
    PtitagsStartProcessInfoDataV1,
    PtitagsStartThreadInfoDataV1,
    PtitagsExtraRawString
} PROC_THREAD_INFO_TAGS; // Must have 256 values max (stored on a byte)

// Sub-services masks:
#define PtissmskProcessInfoDescriptor       (0x00000001uL)
#define PtissmskThreadInfoDescriptor        (0x00000002uL)
#define PtissmskProcessInfoData             (0x00000004uL)
#define PtissmskThreadInfoData              (0x00000008uL)
#define PtissmskAllExtraRawStrings          (0x00000010uL)

// Range of DWORD
typedef struct _DW_RANGE
{
    DWORD   dwFirst;
    DWORD   dwLast;
} DW_RANGE;

// Return status:
typedef enum _PROC_THREAD_INFO_RETURN_STATUS
{
    PtirstOK,
    PtirstResponseBufferTooShort,
    PtirstInvalidInputParamValues,
    PtirstInvalidInputParamCount
} PROC_THREAD_INFO_RETURN_STATUS; // Must have 256 values max (stored on a byte)

#define SetReturnStatus_M(retstat) pParamBuf->Buffer [0] = (BYTE) (retstat);

CHAR szUnknownCommandError [] =
"Unknown target side command.\n"\
"\n";

CHAR szTargetSideParmHelp [] =
"Target Side advanced debugger commands:\n"
".?                     -> this help\n"
".cs                    -> show Critical Section Info (and any synchronization object status)\n"
".ior address,size      -> read IO from 'address' for 'size' bytes (size = 1,2, or 4)\n"
".iow address,size,data -> write IO 'data' to 'address' for 'size' bytes (size = 1,2, or 4)\n"
".fvmp <state>          -> get or set Forced VM Paging state. Specify 'state' to set (state = ON, OFF)\n"
".fex                   -> Filter exceptions. Use \".fex ?\" for more information\n"
".nml <state>           -> get or set Notify Module (un)Load state. Specify 'state' to set (state = ON, OFF)\n"
#if !defined(SHIP_BUILD)
".cbpd <state>          -> turn ON/OFF breakpoint sanitization optimization"                              
#endif
"\n";

CHAR szTargetSideParmHelpOnPrimaryCpu[] =
".fvmp <state>          -> get or set Forced VM Paging state. Specify 'state' to set (state = ON, OFF)\n";

CHAR szFVMP_ON [] =
"Forced VM Paging is ON.\n"\
"\n";

CHAR szFVMP_OFF [] =
"Forced VM Paging is OFF.\n"\
"\n";

CHAR szNML_ON [] =
"Notify Module (un)Load is ON.\n"\
"\n";

CHAR szNML_OFF [] =
"Notify Module (un)Load is OFF.\n"\
   "\n";

const char szBPOpt_on[] = "BP optimizations are on (all BPs disabled on halt)\n\n";
const char szBPOpt_off[] = "BP optimizations are off (all Reads force BP list scan)\n\n";


int kdbgcmpAandW(LPSTR pAstr, LPWSTR pWstr)
{
    while (*pAstr && (*pAstr == (BYTE)*pWstr)) {
        pAstr++;
        pWstr++;
    }
    return (!*pWstr ? 0 : 1);
}

static char *TrimLeft(char *pch)
{
    while ((*pch == ' ') || ((*pch >= 0x9) && (*pch <= 0xD)))
    {
        ++pch;
    }
    return pch;
}

/*++

Routine Name:

    ProcessAdvancedCmd

Routine Description:

    Handle advanced debugger commands
    
Arguments:

    pParamBuf - Supplies and returns pointer to STRING (size + maxsize + raw buffer)

Return Value:

    TRUE if succeed (size OK) otherwise FALSE.

--*/

BOOL ProcessAdvancedCmd (IN PSTRING pParamBuf)
{
    USHORT                  InpParamLength;
    USHORT                  OutBufIndex = 1; // Skip return status (1st byte)
    USHORT                  CurrentBlockEndReannotateIdx;
    BOOL                    fRequestHelpOnTargetSideParam = FALSE;
    BOOL                    fDisplayCriticalSections = FALSE;
    BOOL                    fIOR = FALSE;
    BOOL                    fIOW = FALSE;
    BOOL                    fDispForceVmPaging = FALSE;
    BOOL                    fDispNotifyModuleLoad = FALSE;
    BOOL                    fUnknownCommand = FALSE;
    BOOL                    fFilterExceptionsCmd = FALSE;
    BOOL                    fBreakpointOptCmd = FALSE;
    DWORD                   dwIOAddress = 0;
    DWORD                   dwIOSize = 0;
    DWORD                   dwIOData = 0;
    LPCSTR                  pszParams = NULL;
    UINT                    cParams = 0;
    CHAR                    * szExtraParamInfo;

    DEBUGGERMSG(KDZONE_API, (L"++ProcessAdvancedCmd: (Len=%i,MaxLen=%i)\r\n", pParamBuf->Length, pParamBuf->MaximumLength));

    InpParamLength = pParamBuf->Length;

    pParamBuf->Length = 1; // Default returned length is one for status byte
    if (InpParamLength < 2) // at least 1 parameter ?
    { // size not appropriate: exit with error
        SetReturnStatus_M (PtirstInvalidInputParamCount);
        return TRUE; // Return TRUE so that error gets propagated back to PB DM code
    }

    //
    // Parse parameter string
    //

    szExtraParamInfo = (CHAR *) pParamBuf->Buffer;

    if (strncmp (szExtraParamInfo, "?", 1) == 0)
    { // Return help string on Target side parameters
        fRequestHelpOnTargetSideParam = TRUE;
    }
    else if (strncmp (szExtraParamInfo, "cs", 2) == 0) // Added : JOHNELD 08/29/99
    { // Display critical section information
        fDisplayCriticalSections = TRUE;
    }
    else if (strncmp (szExtraParamInfo, "ior", 3) == 0)
    { // Read I/O
        fIOR = TRUE;
    }
    else if (strncmp (szExtraParamInfo, "iow", 3) == 0)
    { // Write I/O
        fIOW = TRUE;
    }
    else if (strncmp (szExtraParamInfo, "fvmp", 4) == 0 && PcbGetCurCpu() == 1)
    {
        // Forced VM Paging.  Also ensure that this can only happen when running on the primary CPU.
        fDispForceVmPaging = TRUE;
    }
    else if (strncmp (szExtraParamInfo, "fex", 3) == 0)
    { // Filter Exceptions command
        fFilterExceptionsCmd = TRUE;
    }
    else if (strncmp (szExtraParamInfo, "nml", 3) == 0)
    { // Notify Module Load
        fDispNotifyModuleLoad = TRUE;
    }
    else if (strncmp(szExtraParamInfo, "cbpd", 4) == 0)
    {
        fBreakpointOptCmd = TRUE;
    }

    else
    { // Return error and help string on Target side parameters
        fUnknownCommand = TRUE;
    }

    if (fIOR || fIOW)
    { // Parse parameters
        CHAR * szIOParam = szExtraParamInfo + 3;

        dwIOAddress = (DWORD) Kdstrtoul (szIOParam, 0);
        while ((*szIOParam) && (*szIOParam++ != ',')) ;
        if (*szIOParam)
        {
            dwIOSize = (DWORD) Kdstrtoul (szIOParam, 0);
            if (fIOW)
            {
                while ((*szIOParam) && (*szIOParam++ != ',')) ;
                if (*szIOParam)
                {
                    dwIOData = (DWORD) Kdstrtoul (szIOParam, 0);
                }
            }
        }
    }

    if (fDispForceVmPaging)
    { // Parse parameters
        CHAR * szFVMPParam = szExtraParamInfo + 4;
        szFVMPParam = TrimLeft(szFVMPParam);
        if (*szFVMPParam)
        { // parameter present -> set the state
            BOOL fPrevFpState = fIntrusivePaging;
            if (strncmp (szFVMPParam, "on", 2) == 0)
            {
                fIntrusivePaging = TRUE;
            }
            else if (strncmp (szFVMPParam, "off", 3) == 0)
            {
                fIntrusivePaging = FALSE;
            }
            g_fDbgKdStateMemoryChanged = (fIntrusivePaging != fPrevFpState);
        }
    }

    if (fDispNotifyModuleLoad)
    { // Parse parameters
        CHAR * szNMLParam = szExtraParamInfo + 4;
        szNMLParam = TrimLeft(szNMLParam);
        if (*szNMLParam)
        { // parameter present -> set the state
            if (strncmp (szNMLParam, "on", 2) == 0)
            {
                *g_pdwModLoadNotifDbg = MLNO_ALWAYS_ON;
            }
            else if (strncmp (szNMLParam, "off", 3) == 0)
            {
                *g_pdwModLoadNotifDbg = MLNO_ALWAYS_OFF;
            }
        }
    }

    if (fBreakpointOptCmd)
    {
        char *szCBPDParam = szExtraParamInfo + 4;
        szCBPDParam = TrimLeft(szCBPDParam);
        if (*szCBPDParam != '\0')
        {
            if (strncmp(szCBPDParam, "on", 2) == 0)
            {
                DD_SetDisableAllBPOnHalt(TRUE);
            }
            else if (strncmp(szCBPDParam, "off", 3) == 0)
            {
                DD_SetDisableAllBPOnHalt(FALSE);
            }
        }
    }

    // At this point, older code has its parameters. New code should use
    // this common parser. It is a little awkward to avoid memory
    // allocation. I write nulls over the buffer to separate params. To
    // traverse, use the GetNextParam() function.
    //
    // By the way, if you use this parser, examine your parameters before
    // you start on output. Output occupies the same buffer.
    //
    DEBUGGERMSG(KDZONE_API, (L"ProcessAdvancedCmd: Parsing '%a'\r\n", szExtraParamInfo));
    cParams = ParseParamString(&szExtraParamInfo);
    pszParams = szExtraParamInfo;
    DEBUGGERMSG(KDZONE_API, (L"ProcessAdvancedCmd: Have %d params\r\n", cParams));

    // Warning: before this point do not write to pParamBuf->Buffer (input params not saved)

    SetReturnStatus_M (PtirstResponseBufferTooShort); // Default return status is buffer too short (common error)

    //
    // Handle advanced commands
    //

    DEBUGGERMSG(KDZONE_API, (L"ProcessAdvancedCmd: Provide Raw String (OutBufIdx=%i)\r\n", OutBufIndex));

    if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + sizeof (OutBufIndex)))
    { // At least 3 more bytes left in buffer (1 for tag + 2 for block end position)
        AppendImmByteToOutBuf_M (   pParamBuf->Buffer,
                                    PtitagsExtraRawString,
                                    OutBufIndex);
        CurrentBlockEndReannotateIdx = OutBufIndex; // save block end pos annotation index
        OutBufIndex += 2; // skip size (will be written at the end of the block)
    }
    else
    { // Buffer not large enough: exit with error
        return TRUE; // Return TRUE so that error gets propagated back to PB DM code
    }

    if (fUnknownCommand)
    { // Unknown command, display error message:
        if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen (szUnknownCommandError)))
        {
            AppendStringZToOutBuf_M (   pParamBuf->Buffer,
                                        szUnknownCommandError,
                                        OutBufIndex);
            fRequestHelpOnTargetSideParam = TRUE;
            OutBufIndex--; // remove null termination to append help
        }
        else
        { // Buffer not large enough: exit with error
            return TRUE; // Return TRUE so that error gets propagated back to PB DM code
        }
    }

    if (fRequestHelpOnTargetSideParam)
    { // Help on Target side param requested:
        if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen (szTargetSideParmHelp)))
        {
            AppendStringZToOutBuf_M (   pParamBuf->Buffer,
                                        szTargetSideParmHelp,
                                        OutBufIndex);
        }
        else
        { // Buffer not large enough: exit with error
            return TRUE; // Return TRUE so that error gets propagated back to PB DM code
        }

        if(PcbGetCurCpu() == 1)
        {
            if(pParamBuf->MaximumLength >= OutBufIndex + 1 + strlen(szTargetSideParmHelpOnPrimaryCpu))
            {
                AppendStringZToOutBuf_M(pParamBuf->Buffer, szTargetSideParmHelpOnPrimaryCpu, OutBufIndex);
            }
            else
            {
                return TRUE;
            }
        }

        if(pParamBuf->MaximumLength >= OutBufIndex + 1 + 1)
        {
            AppendStringZToOutBuf_M(pParamBuf->Buffer, "\n", OutBufIndex);
        }
        else
        {
            return TRUE;
        }
    }

    if (fDisplayCriticalSections)
    {
        if (!MarshalCriticalSectionInfo (   pParamBuf->Buffer,
                                            &OutBufIndex,
                                            pParamBuf->MaximumLength - OutBufIndex))
        { // Buffer not large enough: exit with error
            return TRUE; // Return TRUE so that error gets propagated back to PB DM code
        }
    }

    if (fIOR || fIOW)
    {
        if (!MarshalIOReadWrite (   pParamBuf->Buffer,
                                    &OutBufIndex,
                                    (USHORT)(pParamBuf->MaximumLength - OutBufIndex),
                                    fIOR,
                                    dwIOAddress,
                                    dwIOSize,
                                    dwIOData))
        { // Buffer not large enough: exit with error
            return TRUE; // Return TRUE so that error gets propagated back to PB DM code
        }
    }

    if (fDispForceVmPaging)
    {
        if (fIntrusivePaging)
        {
            if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen (szFVMP_ON)))
            {
                AppendStringZToOutBuf_M (   pParamBuf->Buffer,
                                            szFVMP_ON,
                                            OutBufIndex);
            }
            else
            { // Buffer not large enough: exit with error
                return TRUE; // Return TRUE so that error gets propagated back to PB DM code
            }
        }
        else
        {
            if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen (szFVMP_OFF)))
            {
                AppendStringZToOutBuf_M (   pParamBuf->Buffer,
                                            szFVMP_OFF,
                                            OutBufIndex);
            }
            else
            { // Buffer not large enough: exit with error
                return TRUE; // Return TRUE so that error gets propagated back to PB DM code
            }
        }
    }

    if (fFilterExceptionsCmd)
    {
        // Skip the "fex" parameter
        pszParams = GetNextParam(pszParams);
        cParams--;

        if (!MarshalFilterExceptionsCmd(pParamBuf->Buffer + OutBufIndex, pParamBuf->MaximumLength - OutBufIndex, pszParams, cParams))
        {
            SetReturnStatus_M(PtirstResponseBufferTooShort);
            return TRUE; // Return TRUE so that error gets propagated back to PB DM code
        }

        OutBufIndex += (USHORT)strlen(pParamBuf->Buffer + OutBufIndex);
    }

    if (fDispNotifyModuleLoad)
    {
        if (*g_pdwModLoadNotifDbg)
        {
            if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen (szNML_ON)))
            {
                AppendStringZToOutBuf_M (   pParamBuf->Buffer,
                                            szNML_ON,
                                            OutBufIndex);
            }
            else
            { // Buffer not large enough: exit with error
                return TRUE; // Return TRUE so that error gets propagated back to PB DM code
            }
        }
        else
        {
            if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen (szNML_OFF)))
            {
                AppendStringZToOutBuf_M (   pParamBuf->Buffer,
                                            szNML_OFF,
                                            OutBufIndex);
            }
            else
            { // Buffer not large enough: exit with error
                return TRUE; // Return TRUE so that error gets propagated back to PB DM code
            }
        }
    }

    if (fBreakpointOptCmd)
    {
        const char *sz = DD_DisableAllBPOnHalt()? szBPOpt_on : szBPOpt_off;
        if (pParamBuf->MaximumLength >= (OutBufIndex + 1 + strlen(sz)))
        {
            AppendStringZToOutBuf_M(pParamBuf->Buffer, sz, OutBufIndex);
        }
        else
        {
            return TRUE;
        }
    }

    // Reannotate end position of block just after block tag
    memcpy (&(pParamBuf->Buffer [CurrentBlockEndReannotateIdx]), &OutBufIndex, sizeof (OutBufIndex));

    // Refresh return buffer size 
    pParamBuf->Length = (USHORT) OutBufIndex;

    SetReturnStatus_M (PtirstOK);
    DEBUGGERMSG(KDZONE_API, (L"--ProcessAdvancedCmd: (OK - OutBufIdx=%i)\r\n", OutBufIndex));
    return TRUE;
} // End of ProcessAdvancedCmd



IMPL_EXCEPTION_HANDLER()

void FlushCache(void) {
    NKCacheRangeFlush(0, 0, CACHE_SYNC_INSTRUCTIONS | CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
}

void FlushICache(void) {
    NKCacheRangeFlush(0, 0, CACHE_SYNC_INSTRUCTIONS);
}

void FlushDCache(void) {
    NKCacheRangeFlush(0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
}

#undef LocalFree
// dummy function so the CRT links.  Should never be called
HLOCAL WINAPI
LocalFree(HLOCAL hMem)
{
    KD_ASSERT(FALSE);
    return hMem;
}


