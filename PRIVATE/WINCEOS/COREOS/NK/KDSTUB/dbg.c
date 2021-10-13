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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++


Module Name:

    dbg.c

Abstract:


Environment:

    WinCE

--*/

#include "kdp.h"


int g_cFrameInCallStack = 0;

// Filter exceptions help
const char g_szFilterExceptionsHelp[] =
"\nFilter Exceptions:\n" \
"\tfex ?             - Request help on Filter Exceptions commands\n" \
"\tfex on/off        - Enable or disable Filter Exceptions functionality\n" \
"\tfex ap procname   - Add 'procname' to list of processes whos exceptions are allowed\n" \
"\tfex dp procname   - Delete 'procname' from list of processes\n" \
"\tfex ac code       - Add 'code' to list of exception codes allowed\n" \
"\tfex dc code       - Delete 'code' from list of exception codes\n";

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

static WCHAR rgchBuf[256];


static const CHAR lpszUnk[]="UNKNOWN";
CHAR lpszModuleName[MAX_PATH];

LPCHAR GetWin32ExeName(PPROCESS pProc)
{
    if (pProc->lpszProcName) {
        kdbgWtoA(pProc->lpszProcName,lpszModuleName);
        return lpszModuleName;
    }
    return (LPCHAR)lpszUnk;
}


void kdbgWtoA (LPCWSTR pWstr, LPCHAR pAstr) 
{
    while (*pAstr++ = (CHAR)*pWstr++);
}


void kdbgWtoAmax (LPCWSTR pWstr, LPCHAR pAstr, int max)
{
    while ((max--) && (*pAstr++ = (CHAR)*pWstr++));
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
    LPWSTR  lpszFmt,
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


unsigned long Kdstrtoul (const BYTE *lpStr, ULONG radix)
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
#define AppendStringZToOutBuf_M(outbuf,sz,outbidx) {memcpy (&((outbuf) [(outbidx)]), sz, (strlen (sz) + 1)); (outbidx) += (strlen (sz) + 1);}
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
            strncpy(pszOutBuf, "ERROR: Access Violation", nLen);
            pszOutBuf[nLen - 1] = '\0';
            break;
        case STATUS_DATATYPE_MISALIGNMENT:
            strncpy(pszOutBuf, "ERROR: Datatype Misalignment", nLen);
            pszOutBuf[nLen - 1] = '\0';
            break;
        case STATUS_INVALID_PARAMETER:
            strncpy(pszOutBuf, "ERROR: Invalid Parameter", nLen);
            pszOutBuf[nLen - 1] = '\0';
            break;
        case STATUS_BUFFER_TOO_SMALL:
            strncpy(pszOutBuf, "ERROR: Buffer too small", nLen);
            pszOutBuf[nLen - 1] = '\0';
            break;
        case STATUS_INTERNAL_ERROR:
            strncpy(pszOutBuf, "ERROR: Internal error. Check COM1 debug output", nLen);
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

            if (pProxy = pThread->lpProxy)
            {
                while (pProxy)
                {
                    switch(pProxy->bType)
                    {
                        case HT_CRITSEC :
                        {
                            PCRIT pCrit = (PCRIT) pProxy->pObject; 
                            PTHREAD pOwnerThread = GetThreadPtr (kdpHandleToHDATA ((HANDLE) ((DWORD) pCrit->lpcs->OwnerThread & ~1), NULL));
                            sprintf_temp (OutBuf, pOutBufIndex, &nMaxBytes, TEXT("\t\tpCritSect 0x%08X (pOwner 0x%X)\n"), pCrit->lpcs, pOwnerThread);
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
            }
            else
            {
                // Not blocked
            }

            pThread = (THREAD *) pThread->thLink.pFwd;
        }
    } while ((pProc = (PROCESS *) pProc->prclist.pFwd) && (pProc != g_pprcNK));
    

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
    UINT i;
    DWORD dwCodeNo, dwProcNo;
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
    else if (cParams >= 1 && !strcmp(pszParams, "ap"))
    {
        cParams--;
        if (cParams > 0)
        {
            DWORD dwProcFree = -1;
            
            pszParams = GetNextParam(pszParams);
            cParams--;

            for (dwProcNo = 0; (dwProcNo < MAX_FILTER_EXCEPTION_PROC); ++ dwProcNo)
            {
                if (strcmp(pszParams, g_szFilterExceptionProc[dwProcNo]) == 0)
                {
                    // Process already in list, return error
                    status = STATUS_INVALID_PARAMETER;
                    goto Exit;
                }

                if (!g_szFilterExceptionProc[dwProcNo][0] && (dwProcFree == -1))
                {
                    // Found free slot
                    dwProcFree = dwProcNo;
                }
            }

            if (dwProcFree != -1)
            {
                snprintf(g_szFilterExceptionProc[dwProcFree], sizeof(g_szFilterExceptionProc[dwProcFree]), L"%S", pszParams);
            }
            else
            {
                // No more space in list
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
    else if (cParams >= 1 && !strcmp(pszParams, "dp"))
    {
        cParams--;
        if (cParams > 0)
        {
            BOOL fFoundProc = FALSE;
            
            pszParams = GetNextParam(pszParams);
            cParams--;

            for (dwProcNo = 0; (dwProcNo < MAX_FILTER_EXCEPTION_PROC); ++ dwProcNo)
            {
                if (strcmp(pszParams, g_szFilterExceptionProc[dwProcNo]) == 0)
                {
                    g_szFilterExceptionProc[dwProcNo][0] = 0;
                    fFoundProc = TRUE;
                }
            }

            if (!fFoundProc)
            {
                // Process not found in list
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
    else if (cParams >= 1 && !strcmp(pszParams, "ac"))
    {
        cParams--;
        if (cParams > 0)
        {
            DWORD dwCodeFree = -1;
            
            pszParams = GetNextParam(pszParams);
            cParams--;

            i = Kdstrtoul(pszParams, 0);

            for (dwCodeNo = 0; (dwCodeNo < MAX_FILTER_EXCEPTION_CODE); ++ dwCodeNo)
            {
                if (g_dwFilterExceptionCode[dwCodeNo] == i)
                {
                    // Code already in list, return error
                    status = STATUS_INVALID_PARAMETER;
                    goto Exit;
                }

                if (!g_dwFilterExceptionCode[dwCodeNo] && (dwCodeFree == -1))
                {
                    // Found free slot
                    dwCodeFree = dwCodeNo;
                }
            }

            if (dwCodeFree != -1)
            {
                g_dwFilterExceptionCode[dwCodeFree] = i;
            }
            else
            {
                // No more space in list
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
    else if (cParams >= 1 && !strcmp(pszParams, "dc"))
    {
        cParams--;
        if (cParams > 0)
        {
            BOOL fFoundCode = FALSE;
            pszParams = GetNextParam(pszParams);
            cParams--;

            i = Kdstrtoul(pszParams, 0);

            for (dwCodeNo = 0; (dwCodeNo < MAX_FILTER_EXCEPTION_CODE); ++ dwCodeNo)
            {
                if (g_dwFilterExceptionCode[dwCodeNo] == i)
                {
                    g_dwFilterExceptionCode[dwCodeNo] = 0;
                    fFoundCode = TRUE;
                }
            }

            if (!fFoundCode)
            {
                // Code not found in list
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
        if (cBufLen <=  cOffset)
        {
            status = STATUS_BUFFER_TOO_SMALL;
            goto Exit;
        }

        cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"\nFilter Exceptions status: %s\n", g_fFilterExceptions ? L"ON" : L"OFF");
        
        if (g_fFilterExceptions)
        {
            for (dwCodeNo = 0; (dwCodeNo < MAX_FILTER_EXCEPTION_CODE); ++ dwCodeNo)
            {
                if (cBufLen <=  cOffset)
                {
                    status = STATUS_BUFFER_TOO_SMALL;
                    goto Exit;
                }

                cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"  Allow code [%u]: 0x%08X\n", dwCodeNo, g_dwFilterExceptionCode[dwCodeNo]);
            }
            
            for (dwProcNo = 0; (dwProcNo < MAX_FILTER_EXCEPTION_PROC); ++ dwProcNo)
            {
                if (cBufLen <=  cOffset)
                {
                    status = STATUS_BUFFER_TOO_SMALL;
                    goto Exit;
                }

                cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"  Allow process [%u]: %S\n", dwProcNo, 
                                                                            g_szFilterExceptionProc[dwProcNo][0] ? g_szFilterExceptionProc[dwProcNo] : "<NULL>");
            }
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


/*++

Routine Name:

    MarshallHwAddrBpCmd

Routine Description:

    Processes an HW address breakpoints request. This is called from the 'habp' command in ProcessAdvancedCmd.
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

#define KD_HBP_CLIENT_VERSION         1

static BOOL MarshallHwAddrBpCmd(LPSTR pszBuffer, UINT cBufLen, LPCSTR pszParams, UINT cParams)
{
    UINT cOffset = 0;
    NTSTATUS status;
    KD_BPINFO bpInfo;
    int iNbDataReadBp  = 0;
    int iNbDataWriteBp = 0;
    int iNbInstExecBp  = 0;
    int KdHbpVersion = 1;

    DEBUGGERMSG(KDZONE_DBG, (L"++MarshallHwAddrBpCmd\r\n"));

    bpInfo.nVersion = KD_HBP_CLIENT_VERSION; // initialize to 1 by default in case it is not changed
    bpInfo.ulFlags = KD_HBP_FLAG_EXEC;
    if (KDIoControl (KD_IOCTL_QUERY_BP, &bpInfo, sizeof (KD_BPINFO))) 
    { // success
        iNbInstExecBp = bpInfo.ulCount;
    }
    else
    {
        bpInfo.nVersion = KD_HBP_CLIENT_VERSION;
        if (KDIoControl (KD_IOCTL_QUERY_CBP, &bpInfo, sizeof (KD_BPINFO)))
        { // success
            iNbInstExecBp = bpInfo.ulCount;
        }
    }
    
    bpInfo.nVersion = KD_HBP_CLIENT_VERSION; // initialize to 1 by default in case it is not changed
    bpInfo.ulFlags = KD_HBP_FLAG_WRITE;
    if (KDIoControl (KD_IOCTL_QUERY_BP, &bpInfo, sizeof (KD_BPINFO))) 
    { // success
        iNbDataWriteBp = bpInfo.ulCount;
    }
    else
    {
        bpInfo.nVersion = KD_HBP_CLIENT_VERSION;
        if (KDIoControl (KD_IOCTL_QUERY_DBP, &bpInfo, sizeof (KD_BPINFO)))
        { // success
            iNbDataWriteBp = bpInfo.ulCount;
        }
    }

    bpInfo.nVersion = 1; // initialize to 1 by default in case it is not changed
    bpInfo.ulFlags = KD_HBP_FLAG_READ;
    if (KDIoControl (KD_IOCTL_QUERY_BP, &bpInfo, sizeof (KD_BPINFO))) 
    { // success
        iNbDataReadBp = bpInfo.ulCount;
        KdHbpVersion = bpInfo.nVersion;
    }

    if (cParams >= 1 && !strcmp(pszParams, "lst"))
    {   
        BOOL fSuccess = TRUE;
        BOOL fAnyBp = FALSE;
        bpInfo.ulHandle = 0; // Start enum
            
        while (fSuccess)
        {
            bpInfo.nVersion = KD_HBP_CLIENT_VERSION;
            bpInfo.ulFlags = (KD_HBP_FLAG_READ + KD_HBP_FLAG_WRITE + KD_HBP_FLAG_EXEC);
            bpInfo.ulCount = 0;
            if (fSuccess = KDIoControl (KD_IOCTL_ENUM_BP, &bpInfo, sizeof (KD_BPINFO)))
            { // success
                if (bpInfo.ulHandle)
                {
                    fAnyBp = TRUE;
                    cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"BP ID=%i Addr=0x%08X Acc=%s%s%s Width=%i\n", bpInfo.ulHandle, bpInfo.ulAddress, 
                        (bpInfo.ulFlags & KD_HBP_FLAG_READ) ? L"R" : L"", 
                        (bpInfo.ulFlags & KD_HBP_FLAG_WRITE) ? L"W" : L"", 
                        (bpInfo.ulFlags & KD_HBP_FLAG_EXEC) ? L"X" : L"",
                        (KdHbpVersion > 1) && (bpInfo.ulCount) ? bpInfo.ulCount : 4);
                }
                else if (!fAnyBp)
                { // Empty
                    cOffset += snprintf (&pszBuffer[cOffset], cBufLen - cOffset, L"Empty - no BP set\n");
                }
            }
            else 
            { // failure
                if (!fAnyBp) // and no BP
                    cOffset += snprintf (&pszBuffer[cOffset], cBufLen - cOffset, L"Failed to enumerate HW address BP, operation may not be supported by platform\n");
            }
        }
    }
    else if (cParams >= 1 && !strcmp(pszParams, "support"))
    {
        cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Number of HW address BP (R-W-X): %i-%i-%i\n", iNbDataReadBp, iNbDataWriteBp, iNbInstExecBp);
        cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Version=%i\n", KdHbpVersion);
        if (KdHbpVersion <= 1)
        {
            cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Fixed Width of 4 bytes - No Data Read access\n");
        }
        else
        {
            cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Variable Width and Data Read access supported\n");
        }
    }
    else if (cParams >= 1 && !strcmp(pszParams, "set"))
    {
        cParams--;
        if (cParams > 0)
        {
            DWORD dwAddr;
            DWORD dwFlags = KD_HBP_FLAG_WRITE; // default is data write only
            DWORD dwWidth = 4;
            
            pszParams = GetNextParam(pszParams);
            cParams--;

            dwAddr = Kdstrtoul (pszParams, 0);

            if (cParams > 0)
            {
                int iRWXSzLen;
                dwFlags = 0;
                pszParams = GetNextParam(pszParams);
                cParams--;
                iRWXSzLen = strlen (pszParams);

                while (iRWXSzLen--)
                {
                    switch (pszParams [iRWXSzLen])
                    {
                        case 'R': 
                        case 'r': 
                            dwFlags |= KD_HBP_FLAG_READ;
                            break;
                        case 'W': 
                        case 'w': 
                            dwFlags |= KD_HBP_FLAG_WRITE;
                            break;
                        case 'X': 
                        case 'x': 
                            dwFlags |= KD_HBP_FLAG_EXEC;
                            break;
                    }
                }

                if (!dwFlags)
                { // Can't have a flags
                    cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Invalid parameter - use Data write by default\n");
                    dwFlags = KD_HBP_FLAG_WRITE;
                }
                else
                {
                    if (cParams > 0)
                    { // Extract width
                        pszParams = GetNextParam(pszParams);
                        cParams--;
                        dwWidth = Kdstrtoul (pszParams, 0);
                        if ((KdHbpVersion < 2) && (dwWidth != 4))
                        {
                            cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"This platform may force width to 4 bytes\n");
                        }
                    }
                    else
                    {
                        cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Use 4 byte width by default\n");
                    }
                }

            }
            else
            {
                cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Use 4 byte width Data Write BP by default\n");
            }

            bpInfo.ulCount = dwWidth;
            bpInfo.ulAddress = dwAddr;
            bpInfo.ulFlags = dwFlags;
            bpInfo.ulHandle = 0;
            bpInfo.nVersion = KD_HBP_CLIENT_VERSION;
            if (KDIoControl (KD_IOCTL_SET_BP, &bpInfo, sizeof (KD_BPINFO))) // New style (flag based)
            { // success
                cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"HW BP set at address 0x%08X, Id is %i\n", dwAddr, bpInfo.ulHandle);
            }
            else
            { // failure
                bpInfo.nVersion = KD_HBP_CLIENT_VERSION;
                if ((dwFlags == KD_HBP_FLAG_WRITE) && KDIoControl (KD_IOCTL_SET_DBP, &bpInfo, sizeof (KD_BPINFO)))
                { // success
                    cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"HW BP set at address 0x%08X, Id is %i\n", dwAddr, bpInfo.ulHandle);
                }
                else
                {
                    bpInfo.nVersion = KD_HBP_CLIENT_VERSION;
                    if ((dwFlags == KD_HBP_FLAG_EXEC) && KDIoControl (KD_IOCTL_SET_CBP, &bpInfo, sizeof (KD_BPINFO)))
                    { // success
                        cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"HW BP set at address 0x%08X, Id is %i\n", dwAddr, bpInfo.ulHandle);
                    }
                    else
                    { // failure
                        cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Failed to set HW BP set at address 0x%08X. Check if access type is supported by target\n", dwAddr);
                    }
                }
            }
        }
        else
        {
            cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Invalid parameter\n");
        }
    }
    else if (cParams >= 1 && !strcmp(pszParams, "clr"))
    {
        cParams--;
        if (cParams > 0)
        {
            DWORD dwBpHandle;

            pszParams = GetNextParam(pszParams);
            cParams--;

            dwBpHandle = Kdstrtoul (pszParams, 0);

            bpInfo.nVersion = KD_HBP_CLIENT_VERSION;
            bpInfo.ulAddress = 0;
            bpInfo.ulFlags = 0; // Don't care - handle is the only thing that counts
            bpInfo.ulHandle = dwBpHandle;
            if (KDIoControl (KD_IOCTL_CLEAR_BP, &bpInfo, sizeof (KD_BPINFO))) // New style (flag based)
            { // success
                cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"HW BP of Id %i has been deleted\n", dwBpHandle);
            }
            else
            { // failure
                bpInfo.nVersion = KD_HBP_CLIENT_VERSION;
                if (KDIoControl (KD_IOCTL_CLEAR_DBP, &bpInfo, sizeof (KD_BPINFO))) // Try Data BP 1st
                { // success
                    cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"HW BP of Id %i has been deleted\n", dwBpHandle);
                }
                else
                {
                    bpInfo.nVersion = KD_HBP_CLIENT_VERSION;
                    if (KDIoControl (KD_IOCTL_CLEAR_CBP, &bpInfo, sizeof (KD_BPINFO))) // then try Code BP if nothing else works
                    { // success
                        cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"HW BP of Id %i has been deleted\n", dwBpHandle);
                    }
                    else
                    { // failure
                        cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Failed to clear HW BP of Id %i\n", dwBpHandle);
                    }
                }
            }



        }
        else
        {
            cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Invalid parameter\n");
        }
    }
    else if (cParams > 0)
    {
        // "adp ?" - Give information about fex command

        if (cParams >= 1 && strcmp(pszParams, "?") != 0)
        {
            // User entered an unknown command, so issue a warning
            cOffset += snprintf(&pszBuffer[cOffset], cBufLen - cOffset, L"Unknown command '%a'\n", pszParams);
        }

        if ((cBufLen <=  cOffset) ||
            ((cBufLen - cOffset) < lengthof(g_szlHwAddrBpHelp))) // Output buffer to small
        {
            status = STATUS_BUFFER_TOO_SMALL;
            goto Exit;
        }

        memcpy(&pszBuffer[cOffset], g_szlHwAddrBpHelp, sizeof(g_szlHwAddrBpHelp));
        cOffset += (lengthof(g_szlHwAddrBpHelp) - 1);
    }
    
    status = STATUS_SUCCESS;

Exit:

    if (status != STATUS_SUCCESS)
    {
        AppendErrorToOutBuf_M(pszBuffer, status, cOffset, cBufLen);
    }
    
    DEBUGGERMSG(KDZONE_DBG, (L"--MarshallHwAddrBpCmd: st=0x%08X\r\n", status));

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

// Parameter structure
typedef struct _PROC_THREAD_INFO_PARAM
{
    DWORD       dwSubServiceMask; // 0..15
    DW_RANGE    dwrProcessOrdNum; // Ordinal number (not id) range (optional)
    DW_RANGE    dwrThreadOrdNum; // Ordinal number (not id) range (optional)
    CHAR        szExtraParamInfo []; // optional parameter string (open for future extensions)
} PROC_THREAD_INFO_PARAM, *PPROC_THREAD_INFO_PARAM;

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
"Target Side advanced debugger commands:\n"\
".?                     -> this help\n"\
".cs                    -> show Critical Section Info (and any synchronization object status)\n"\
".ior address,size      -> read IO from 'address' for 'size' bytes (size = 1,2, or 4)\n"\
".iow address,size,data -> write IO 'data' to 'address' for 'size' bytes (size = 1,2, or 4)\n"\
".fvmp <state>          -> get or set Forced VM Paging state. Specify 'state' to set (state = ON, OFF)\n"\
".fex                   -> Filter exceptions. Use \"fex ?\" for more information\n"\
".habp                  -> hardware address breakpoint. Use \"habp ?\" for more information\n"\
".nml <state>           -> get or set Notify Module (un)Load state. Specify 'state' to set (state = ON, OFF)\n"\
"\n";

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


kdbgcmpAandW(LPSTR pAstr, LPWSTR pWstr) {
    while (*pAstr && (*pAstr == (BYTE)*pWstr)) {
        pAstr++;
        pWstr++;
    }
    return (!*pWstr ? 0 : 1);
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
    BOOL                    flHwAddrBpCmd = FALSE;
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
    else if (strncmp (szExtraParamInfo, "fvmp", 4) == 0)
    { // Forced VM Paging
        fDispForceVmPaging = TRUE;
    }
    else if (strncmp (szExtraParamInfo, "fex", 3) == 0)
    { // Filter Exceptions command
        fFilterExceptionsCmd = TRUE;
    }
    else if (strncmp (szExtraParamInfo, "habp", 4) == 0)
    { // Address BP command
        flHwAddrBpCmd = TRUE;
    }
    else if (strncmp (szExtraParamInfo, "nml", 3) == 0)
    { // Notify Module Load
        fDispNotifyModuleLoad = TRUE;
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

        // Skip whitespace
        while (((*szFVMPParam) == ' ') || (((*szFVMPParam) >= 0x9) && ((*szFVMPParam) <= 0xD))) szFVMPParam++;

        if (*szFVMPParam)
        { // parameter present -> set the state
            BOOL fPrevFpState = *(g_kdKernData.pfForcedPaging);
            if (strncmp (szFVMPParam, "on", 2) == 0)
            {
                *(g_kdKernData.pfForcedPaging) = TRUE;
            }
            else if (strncmp (szFVMPParam, "off", 3) == 0)
            {
                *(g_kdKernData.pfForcedPaging) = FALSE;
            }
            g_fDbgKdStateMemoryChanged = (*(g_kdKernData.pfForcedPaging) != fPrevFpState);
        }
    }

    if (fDispNotifyModuleLoad)
    { // Parse parameters
        CHAR * szNMLParam = szExtraParamInfo + 4;

        // Skip whitespace
        while (((*szNMLParam) == ' ') || (((*szNMLParam) >= 0x9) && ((*szNMLParam) <= 0xD))) szNMLParam++;

        if (*szNMLParam)
        { // parameter present -> set the state
            DWORD dwPrevNmlState = *g_pdwModLoadNotifDbg;
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
    }

    if (fDisplayCriticalSections)
    {
        if (!MarshalCriticalSectionInfo (   pParamBuf->Buffer,
                                            &OutBufIndex,
                                            (USHORT)(pParamBuf->MaximumLength - OutBufIndex)))
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
        if (*(g_kdKernData.pfForcedPaging))
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

        OutBufIndex += strlen(pParamBuf->Buffer + OutBufIndex);
    }

    if (flHwAddrBpCmd)
    {
        // Skip the "habp" parameter
        pszParams = GetNextParam(pszParams);
        cParams--;

        if (!MarshallHwAddrBpCmd(pParamBuf->Buffer + OutBufIndex, pParamBuf->MaximumLength - OutBufIndex, pszParams, cParams))
        {
            SetReturnStatus_M(PtirstResponseBufferTooShort);
            return TRUE; // Return TRUE so that error gets propagated back to PB DM code
        }

        OutBufIndex += strlen(pParamBuf->Buffer + OutBufIndex);
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

    // Reannotate end position of block just after block tag
    memcpy (&(pParamBuf->Buffer [CurrentBlockEndReannotateIdx]), &OutBufIndex, sizeof (OutBufIndex));

    // Refresh return buffer size 
    pParamBuf->Length = OutBufIndex;

    SetReturnStatus_M (PtirstOK);
    DEBUGGERMSG(KDZONE_API, (L"--ProcessAdvancedCmd: (OK - OutBufIdx=%i)\r\n", OutBufIndex));
    return TRUE;
} // End of ProcessAdvancedCmd


PROCESS *g_pFocusProcOverride = NULL;

void * MapToDebuggeeCtxKernEquivIfAcc (PPROCESS pVM, BYTE * pbAddr, BOOL fProbeOnly)
{
    void *pvRet;

    DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"++MapToDebuggeeCtxKernEquivIfAcc(pVM=%08X, Addr=%08X, Probe = %d)\r\n", pVM, pbAddr, fProbeOnly));
    if (pVM == NULL && g_pFocusProcOverride != NULL)
    {
        DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"  MapToDebuggeeCtxKernEquivIfAcc(%08X) Using FocusProcessOverride %08X\r\n", pbAddr,
            g_pFocusProcOverride));
        pVM = g_pFocusProcOverride;
    }

    KD_ASSERT(Hdstub.pfnCallClientIoctl);
    {
        OSAXSFN_MAPTOKVA a = {0};
        HRESULT hrOsAxsT0;

        a.pVM        = pVM;
        a.pvAddr     = pbAddr;
        a.fProbeOnly = fProbeOnly;
        
        hrOsAxsT0 = Hdstub.pfnCallClientIoctl(OSAXST0_NAME, OSAXST0_IOCTL_MAP_TO_KVA_IF_ACC, &a);
        if (SUCCEEDED(hrOsAxsT0))
        {
            pvRet = a.pvKVA;
        }
        else
        {
            DEBUGGERMSG(KDZONE_ALERT, (L"  MapToDebuggeeCtxKernEquivIfAcc: Failed to call to OsAxsT0\r\n"));
            pvRet = NULL;
        }
    }

    DEBUGGERMSG (KDZONE_KERNCTXADDR, (L"--MapToDebuggeeCtxKernEquivIfAcc (0x%08x) -- returning 0x%08x\r\n", pbAddr, pvRet));
    return pvRet;
}


/*++

Routine Name:

    KdpHandlePageIn

Routine Description:

    This function is called by the OS upon a successful page-in.
    The function pointer pfnNKLogPageIn is set to this function.
    Other functions in KD that need to perform work when a page
    in occurs are called within this function.

Arguments:

    ulAddr - the starting address of the page just paged in
    bWrite - TRUE if page is write-accessible, else FALSE

Return Value:

    None

--*/

BOOL
KdpHandlePageIn(
    IN ULONG ulAddress,
    IN ULONG ulNumPages,
    IN BOOL bWrite
    )
{
    DEBUGGERMSG (KDZONE_TRAP && KDZONE_VIRTMEM, (L"++KdpHandlePageIn(addr=0x%08x, pages=%d, writeable=%d)\r\n", ulAddress, ulNumPages, bWrite));
    // Call any KD functions that are interested in page-in's
    KdpHandlePageInBreakpoints (ulAddress, ulNumPages);
    DEBUGGERMSG (KDZONE_TRAP && KDZONE_VIRTMEM, (L"--KdpHandlePageIn\r\n"));
    return FALSE;
}


#if defined(x86)

EXCEPTION_DISPOSITION __cdecl
_except_handler3
(
    PEXCEPTION_RECORD XRecord,
    void *Registration,
    PCONTEXT Context,
    PDISPATCHER_CONTEXT Dispatcher
)
{
    return g_kdKernData.p_except_handler3(XRecord, Registration, Context, Dispatcher);
}

BOOL __abnormal_termination(void)
{
	return g_kdKernData.p__abnormal_termination();
}



#else

EXCEPTION_DISPOSITION __C_specific_handler
(
    PEXCEPTION_RECORD ExceptionRecord,
    PVOID EstablisherFrame,
    PCONTEXT ContextRecord,
    PDISPATCHER_CONTEXT DispatcherContext
)
{
    return g_kdKernData.p__C_specific_handler(ExceptionRecord, EstablisherFrame, ContextRecord, DispatcherContext);
}

#endif

void FlushCache(void) {
    g_kdKernData.FlushCacheRange (0, 0, CACHE_SYNC_INSTRUCTIONS | CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
}

void FlushICache(void) {
    g_kdKernData.FlushCacheRange (0, 0, CACHE_SYNC_INSTRUCTIONS);
}

void FlushDCache(void) {
    g_kdKernData.FlushCacheRange (0, 0, CACHE_SYNC_WRITEBACK | CACHE_SYNC_DISCARD);
}

// Flushing caches and contexts
#if defined(SHx)

#if !defined(SH3e) && !defined(SH4)
void DSPFlushContext(void) {
    g_kdKernData.DSPFlushContext();
}
#endif
#endif


#if defined(MIPS_HAS_FPU) || defined(SH4) || defined(x86) || defined(ARM)
void FPUFlushContext(void) {
    g_kdKernData.FPUFlushContext();
}
#endif

#undef LocalFree
// dummy function so the CRT links.  Should never be called
HLOCAL WINAPI
LocalFree(HLOCAL hMem)
{
    KD_ASSERT(FALSE);
    return hMem;
}


