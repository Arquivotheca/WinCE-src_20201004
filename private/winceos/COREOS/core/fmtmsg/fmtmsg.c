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
#include <memory.h>

extern HANDLE hInstCoreDll;

#define FMTMSG_MAX_RESERVE (64*1024)

typedef struct vmbuf {
        LPBYTE pBase;   // base pointer
        LPBYTE pCur;    // current pointer
        LPBYTE pHigh;   // one byte beyond last committed byte
        LPBYTE pMax;    // one byte beyond last reserved byte
} vmbuf;

BOOL AllocVMBuf(DWORD dwCommit, vmbuf *pvmbuf) {
        dwCommit = (dwCommit + UserKInfo[KINX_PAGESIZE] - 1) & ~(UserKInfo[KINX_PAGESIZE]-1);
        pvmbuf->pBase = VirtualAlloc(0,FMTMSG_MAX_RESERVE,MEM_RESERVE,PAGE_READWRITE);
        if (!(pvmbuf->pBase))
                return 0;
        if (!VirtualAlloc(pvmbuf->pBase,dwCommit,MEM_COMMIT,PAGE_READWRITE)) {
                VirtualFree(pvmbuf->pBase, 0, MEM_RELEASE);
                return 0;
        }
        pvmbuf->pCur = pvmbuf->pBase;
        pvmbuf->pHigh = pvmbuf->pBase + dwCommit;
        pvmbuf->pMax = pvmbuf->pBase + FMTMSG_MAX_RESERVE;
        return TRUE;
}

void FreeVMBuf(vmbuf *pvmbuf) {
        VirtualFree(pvmbuf->pBase,pvmbuf->pMax-pvmbuf->pBase,MEM_DECOMMIT);
        VirtualFree(pvmbuf->pBase,0,MEM_RELEASE);
}

BOOL vmmakespace(vmbuf *pvmbuf, DWORD len) {
        DWORD growth;
        if (pvmbuf->pCur + len <= pvmbuf->pHigh)
                return TRUE;
        if (pvmbuf->pCur + len <= pvmbuf->pMax) {
                growth = (pvmbuf->pCur + len - pvmbuf->pHigh + UserKInfo[KINX_PAGESIZE] - 1) & ~(UserKInfo[KINX_PAGESIZE] - 1);
                if (VirtualAlloc(pvmbuf->pHigh,growth,MEM_COMMIT,PAGE_READWRITE)) {
                        pvmbuf->pHigh += growth;
                        return TRUE;
                }
        }
        return FALSE;
}

BOOL vmbufcpy(vmbuf *pvmbuf, LPCWSTR pStr, DWORD len) {
        if (vmmakespace(pvmbuf,len)) {
                memcpy(pvmbuf->pCur,pStr,len);
                pvmbuf->pCur += len;
                return TRUE;
        }
        return FALSE;
}

int vmprintfHandler(DWORD ExceptionCode, PEXCEPTION_POINTERS ExceptionInfo, vmbuf *pBuffer) {
    const BYTE *BadAddress;
    if (ExceptionCode == STATUS_ACCESS_VIOLATION) {
        BadAddress = (LPBYTE)(ExceptionInfo->ExceptionRecord->ExceptionInformation[1]);
        if ((BadAddress >= pBuffer->pHigh && BadAddress < pBuffer->pMax) &&
                        (vmmakespace(pBuffer,BadAddress-pBuffer->pCur+1)))
                        return EXCEPTION_CONTINUE_EXECUTION;
        }
    return EXCEPTION_EXECUTE_HANDLER;
}

BOOL vmprintf(vmbuf *pDst, LPCWSTR pStr, LPCVOID p1, LPCVOID p2, LPCVOID p3)
{
    int len;
    BOOL fRet = FALSE;

    // expected exception. Don't break into debugger
    UTlsPtr()[TLSSLOT_KERNEL] |= TLSKERN_NOFAULT;
    try {
        // the use of swprintf here is safe and necessary, since this call is try/excepted and relies on faults
        // to commit more memory (see vmprintfHandler above).  Using a safe version of swprintf would break this.
#pragma warning(push)
#pragma warning(disable:4995) // C4995: Deprecated API warning
#pragma warning(disable:4996) // C4996: Unsafe API warning
        len = swprintf((LPWSTR)pDst->pCur,pStr,p1,p2,p3);
        /* swprintf return -1 indicate error case or WEOF 0xffff */
        if (len >= 0) {
            fRet = TRUE;
            pDst->pCur += len*sizeof(WCHAR);
        }
    } except(vmprintfHandler(GetExceptionCode(),GetExceptionInformation(),pDst)) {
    }
#pragma warning(pop)    
    UTlsPtr()[TLSSLOT_KERNEL] &= ~TLSKERN_NOFAULT;
    if (!fRet) {
        SetLastError(ERROR_OUTOFMEMORY);
    }
    return fRet;
}

#define MAX_INSERTS 200

DWORD FormatMessage2(LPCWSTR MessageFormat, ULONG MaximumWidth, BOOL IgnoreInserts, BOOL ArgumentsAreAnArray, va_list *Arguments,
        vmbuf *pBuffer) {
        LPCWSTR lpDstBeg, s, s1;
    LPWSTR lpDstLastSpace;
        DWORD CurInsert, PrintParameterCount, MaxInsert, Column, cSpaces, PrintParameter1, PrintParameter2;
        DWORD rgInserts[MAX_INSERTS];
        WCHAR c, PrintFormatString[32];
        lpDstLastSpace = 0;
        MaxInsert = 0;
        Column = 0;
        s = MessageFormat;
        while (*s) {
                if (*s == L'%') {
                        s++;
                        lpDstBeg = (LPWSTR)pBuffer->pCur;
                        if (*s >= L'1' && *s <= L'9') {
                                CurInsert = *s++ - L'0';
                                if (*s >= L'0' && *s <= L'9') {
                                        CurInsert = (CurInsert * 10) + (*s++ - L'0');
                                        if (*s >= L'0' && *s <= L'9') {
                                                CurInsert = (CurInsert * 10) + (*s++ - L'0');
                                                if (*s >= L'0' && *s <= L'9') {
                                                        SetLastError(ERROR_INVALID_PARAMETER);
                                                        return 0;
                                                }
                                        }
                                }
                                CurInsert--;
                                PrintParameterCount = 0;
                                if (*s == L'!') {
                                        LPWSTR pFmt;
                                        PrintFormatString[0] = L'%';

                                        s++;
                                        for (pFmt = &PrintFormatString[1]; *s && (L'!' != *s) && (pFmt < &PrintFormatString[31]); pFmt ++, s ++) {
                                            if ((L'*' == *s) && (PrintParameterCount++ > 1)) {
                                                // more than 1 '*', error
                                                break;
                                            }
                                            *pFmt = *s;
                                        }
                                        // the only valid case is when L'!' == *s
                                        if (L'!' != *s ++) {
                                                SetLastError(ERROR_INVALID_PARAMETER);
                                                return 0;
                                        }

                                        *pFmt = 0;
                                } else {
                                        memcpy(PrintFormatString, L"%s", sizeof(L"%s"));
                                }
                                if (IgnoreInserts) {
                                        if (!wcscmp(PrintFormatString, L"%s")) {
                                                if (!vmprintf(pBuffer, L"%%%u", (LPVOID)(CurInsert+1),0,0)) {
                                                        SetLastError(ERROR_BUFFER_OVERFLOW);
                                                        return 0;
                                                }
                                        } else {
                                                if (!vmprintf(pBuffer, L"%%%u!%s!",(LPVOID)(CurInsert+1),&PrintFormatString[1],0)) {
                                                        SetLastError(ERROR_BUFFER_OVERFLOW);
                                                        return 0;
                                                }
                                        }
                                } else {
                                        if (Arguments) {
                                                if ((CurInsert+PrintParameterCount) >= MAX_INSERTS) {
                                                        SetLastError(ERROR_INVALID_PARAMETER);
                                                        return 0;
                                                }
                                                while (CurInsert >= MaxInsert) {
                                                        if (ArgumentsAreAnArray)
                                                                rgInserts[MaxInsert++] = *((PULONG)Arguments)++;
                                                        else
                                                                rgInserts[MaxInsert++] = va_arg(*Arguments, ULONG);
                                                }
                                                s1 = (PWSTR)rgInserts[CurInsert];
                                                PrintParameter1 = 0;
                                                PrintParameter2 = 0;
                                                if (PrintParameterCount > 0) {
                                                        if (ArgumentsAreAnArray)
                                                                PrintParameter1 = rgInserts[MaxInsert++] = *((PULONG)Arguments)++;
                                                        else
                                                                PrintParameter1 = rgInserts[MaxInsert++] = va_arg( *Arguments, ULONG );
                                                        if (PrintParameterCount > 1) {
                                                                if (ArgumentsAreAnArray)
                                                                        PrintParameter2 = rgInserts[MaxInsert++] = *((PULONG)Arguments)++;
                                                                else
                                                                        PrintParameter2 = rgInserts[MaxInsert++] = va_arg( *Arguments, ULONG );
                                                        }
                                                }
                                                if (!vmprintf(pBuffer, PrintFormatString, s1, (LPVOID)PrintParameter1, (LPVOID)PrintParameter2)) {
                                                        SetLastError(ERROR_BUFFER_OVERFLOW);
                                                        return 0;
                                                }
                                        } else {
                                                SetLastError(ERROR_INVALID_PARAMETER);
                                                return 0;
                                        }
                                }
                        } else {
                                if (*s == L'0')
                                        break;
                                if (!*s) {
                                        SetLastError(ERROR_INVALID_PARAMETER);
                                        return 0;
                                }
                                if (*s == L'r') {
                                        if (!vmbufcpy(pBuffer,L"\r",2)) {
                                                SetLastError(ERROR_BUFFER_OVERFLOW);
                                                return 0;
                                        }
                                        s++;
                                        lpDstBeg = NULL;
                                } else if (*s == L'n') {
                                        if (!vmbufcpy(pBuffer,L"\r\n",4)) {
                                                SetLastError(ERROR_BUFFER_OVERFLOW);
                                                return 0;
                                        }
                                        s++;
                                        lpDstBeg = NULL;
                                } else if (*s == L't') {
                                        lpDstLastSpace = (LPWSTR)pBuffer->pCur;
                                        if (!vmbufcpy(pBuffer,L"\t",2)) {
                                                SetLastError(ERROR_BUFFER_OVERFLOW);
                                                return 0;
                                        }
                                        if (Column % 8)
                                                Column = (Column + 7) & ~7;
                                        else
                                                Column += 8;
                                        s++;
                                } else if (*s == L'b') {
                                        lpDstLastSpace = (LPWSTR)pBuffer->pCur;
                                        if (!vmbufcpy(pBuffer,L" ",2)) {
                                                SetLastError(ERROR_BUFFER_OVERFLOW);
                                                return 0;
                                        }
                                        s++;
                                } else if (IgnoreInserts) {
                                        if (!vmbufcpy(pBuffer,L"%",2)) {
                                                SetLastError(ERROR_BUFFER_OVERFLOW);
                                                return 0;
                                        }
                                        if (!vmbufcpy(pBuffer,s,2)) {
                                                SetLastError(ERROR_BUFFER_OVERFLOW);
                                                return 0;
                                        }
                                        s++;
                                } else {
                                        if (!vmbufcpy(pBuffer,s,2)) {
                                                SetLastError(ERROR_BUFFER_OVERFLOW);
                                                return 0;
                                        }
                                        s++;
                                }
                        }
                        if (lpDstBeg)
                                Column += (LPWSTR)pBuffer->pCur - lpDstBeg;
                        else {
                                lpDstLastSpace = NULL;
                                Column = 0;
                        }
                } else {
                        c = *s++;
                        if (c == L'\r' || c == L'\n') {
                                if ((c == L'\n' && *s == L'\r') || (c == L'\r' && *s == L'\n'))
                                        s++;
                                if (MaximumWidth) {
                                        lpDstLastSpace = (LPWSTR)pBuffer->pCur;
                                        c = L' ';
                                } else
                                        c = L'\n';
                        }
                        if (c == L'\n') {
                                if (!vmbufcpy(pBuffer,L"\r\n",4)) {
                                        SetLastError(ERROR_BUFFER_OVERFLOW);
                                        return 0;
                                }
                                lpDstLastSpace = NULL;
                                Column = 0;
                        } else {
                                if (c == L' ')
                                        lpDstLastSpace = (LPWSTR)pBuffer->pCur;
                                if (!vmbufcpy(pBuffer,&c,2)) {
                                        SetLastError(ERROR_BUFFER_OVERFLOW);
                                        return 0;
                                }
                                Column += 1;
                        }
                }
                if (MaximumWidth && (MaximumWidth != 0xFFFFFFFF) && (Column >= MaximumWidth)) {
                        if (lpDstLastSpace) {
                                lpDstBeg = lpDstLastSpace;
                                while (*lpDstBeg == L' ' || *lpDstBeg == L'\t') {
                                        lpDstBeg += 1;
                                        if (lpDstBeg == (LPWSTR)pBuffer->pCur)
                                                break;
                                }
                                while (lpDstLastSpace > (LPWSTR)pBuffer->pBase) {
                                        if (lpDstLastSpace[-1] == L' ' || lpDstLastSpace[-1] == L'\t')
                                                lpDstLastSpace--;
                                        else
                                                break;
                                }
                                cSpaces = lpDstBeg - lpDstLastSpace;
                                if (cSpaces == 1) {
                                        if (!vmmakespace(pBuffer,1)) {
                                                SetLastError(ERROR_BUFFER_OVERFLOW);
                                                return 0;
                                        }
                                }
                                memmove(lpDstLastSpace + 2, lpDstBeg, ((LPWSTR)pBuffer->pCur - lpDstBeg)*sizeof(WCHAR));
                                *lpDstLastSpace++ = L'\r';
                                *lpDstLastSpace++ = L'\n';
                                Column = (LPWSTR)pBuffer->pCur - lpDstBeg;
                                pBuffer->pCur = (LPBYTE)(lpDstLastSpace + Column);
                                lpDstLastSpace = NULL;
                        } else {
                                if (!vmbufcpy(pBuffer,L"\r\n",4)) {
                                        SetLastError(ERROR_BUFFER_OVERFLOW);
                                        return 0;
                                }
                                lpDstLastSpace = NULL;
                                Column = 0;
                        }
                }
        }
        if (!vmbufcpy(pBuffer,L"",2)) {
                SetLastError(ERROR_BUFFER_OVERFLOW);
                return 0;
        }
        return pBuffer->pCur - pBuffer->pBase;
}

DWORD FormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId, LPWSTR lpBuffer, DWORD nSize, va_list *Arguments) {
        HMODULE DllHandle;
        HRSRC hRsrc;
        vmbuf Buffer;
        DWORD NumBlocks, count;
        LPBYTE s;
        MESSAGE_RESOURCE_DATA *pmrd = NULL;
        const MESSAGE_RESOURCE_BLOCK *pmrb;
        ULONG MaximumWidth;
        ULONG LengthNeeded;
        LPCWSTR MessageFormat = NULL;
        LPWSTR lpDst, UnicodeString = 0;

        UNREFERENCED_PARAMETER (dwLanguageId);

        /* If this is a Win32 error wrapped as an OLE HRESULT then unwrap it */
        if (((dwMessageId & 0xffff0000) == 0x80070000) && (dwFlags & FORMAT_MESSAGE_FROM_SYSTEM) &&
                !(dwFlags & FORMAT_MESSAGE_FROM_HMODULE) && !(dwFlags & FORMAT_MESSAGE_FROM_STRING))
                dwMessageId &= 0x0000ffff;
        if (!lpBuffer) {
                SetLastError(ERROR_INVALID_PARAMETER);
                return 0;
        }
        if (dwFlags & FORMAT_MESSAGE_FROM_STRING)
                MessageFormat = (LPWSTR)lpSource;
        else {
retrySystem:
                if (dwFlags & FORMAT_MESSAGE_FROM_HMODULE)
                        DllHandle = (HMODULE)lpSource;
                else if (dwFlags & FORMAT_MESSAGE_FROM_SYSTEM)
                        DllHandle = (HMODULE)hInstCoreDll;
                else {
                        SetLastError(ERROR_INVALID_PARAMETER);
                        return 0;
                }

                hRsrc = FindResource(DllHandle,0,RT_MESSAGETABLE);
                if (hRsrc)
                    pmrd = LoadResource(DllHandle,hRsrc);
                if (hRsrc && pmrd) {
                        for (NumBlocks = pmrd->NumberOfBlocks, pmrb = &pmrd->Blocks[0]; NumBlocks; NumBlocks--, pmrb++) {
                                if (dwMessageId >= pmrb->LowId && dwMessageId <= pmrb->HighId) {
                                        s = (LPBYTE)pmrd + pmrb->OffsetToEntries;
                                        count = dwMessageId - pmrb->LowId;
                                        while (count--)
                                                s += ((PMESSAGE_RESOURCE_ENTRY)s)->Length;
                                        if (((PMESSAGE_RESOURCE_ENTRY)s)->Flags & MESSAGE_RESOURCE_UNICODE)
                                                MessageFormat = (LPWSTR)((PMESSAGE_RESOURCE_ENTRY)s)->Text;
                                        else {
                                                UnicodeString = (LPWSTR)LocalAlloc(LMEM_FIXED,((PMESSAGE_RESOURCE_ENTRY)s)->Length*2);
                                                if (!UnicodeString) {
                                                        SetLastError(ERROR_OUTOFMEMORY);
                                                        return 0;
                                                }
                                                if (!MultiByteToWideChar(CP_ACP,0,(LPCSTR)((PMESSAGE_RESOURCE_ENTRY)s)->Text,((PMESSAGE_RESOURCE_ENTRY)s)->Length,
                                                        UnicodeString,((PMESSAGE_RESOURCE_ENTRY)s)->Length*2)) {
                                                        LocalFree(UnicodeString);
                                                        return 0;
                                                }
                                                MessageFormat = UnicodeString;
                                        }
                                        break;
                                }
                        }
                        if (!NumBlocks)
                                goto notfound;
                } else {
notfound:
                        if ((dwFlags & (FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_FROM_SYSTEM)) == (FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_FROM_SYSTEM)) {
                                dwFlags &= ~FORMAT_MESSAGE_FROM_HMODULE;
                                goto retrySystem;
                        }
                        SetLastError(ERROR_MR_MID_NOT_FOUND);
                        return 0;
                }
        }
        MaximumWidth = (dwFlags & FORMAT_MESSAGE_MAX_WIDTH_MASK);
        if (MaximumWidth == FORMAT_MESSAGE_MAX_WIDTH_MASK)
                MaximumWidth = 0xFFFFFFFF;
        if (!AllocVMBuf((nSize+1)*sizeof(WCHAR),&Buffer)) {
                if (UnicodeString)
                        LocalFree(UnicodeString);
                SetLastError(ERROR_OUTOFMEMORY);
                return 0;
        }
        LengthNeeded = FormatMessage2(MessageFormat, MaximumWidth,
                (dwFlags & FORMAT_MESSAGE_IGNORE_INSERTS) ? TRUE : FALSE,
                (dwFlags & FORMAT_MESSAGE_ARGUMENT_ARRAY) ? TRUE : FALSE,
                Arguments, &Buffer);
        if (UnicodeString)
                LocalFree(UnicodeString);
        if (!LengthNeeded)
                goto done;
        if (dwFlags & FORMAT_MESSAGE_ALLOCATE_BUFFER) {
                lpDst = (LPWSTR)LocalAlloc(LMEM_FIXED, LengthNeeded);
                if (!lpDst) {
                        LengthNeeded = 0;
                        SetLastError(ERROR_OUTOFMEMORY);
                        goto done;
                }
                *(LPVOID *)lpBuffer = lpDst;
        } else {
                if ((LengthNeeded / sizeof( WCHAR )) > nSize) {
                        LengthNeeded = 0;
                        SetLastError(ERROR_INSUFFICIENT_BUFFER);
                        goto done;
                }
                lpDst = lpBuffer;
        }
        memcpy(lpDst,Buffer.pBase,LengthNeeded);
done:
        FreeVMBuf(&Buffer);
        if (!LengthNeeded)
                return 0;
        return (LengthNeeded - sizeof(WCHAR)) / sizeof(WCHAR);
}

