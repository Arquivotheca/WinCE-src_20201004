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
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <tchar.h>
#include "reghlpr.h"


///////////////////////////////////////////////////////////////////////////////
//
//  Function        :   Hlp_GenStringData
//
//  Description    :   Generates string data. 
//
//  Params: 
//  dwFlags    PERF_FLAG_READABLE
//            PERF_FLAG_RANDOM (default)
//            PERF_FLAG_ALPHA
//            PERF_FLAG_ALPHA_NUM
//
//  cChars    is the count of characters that the buffer
//            can hold, including the NULL character.
//
///////////////////////////////////////////////////////////////////////////////
LPTSTR  Hlp_GenStringData(__out_ecount(cChars) LPTSTR pszString, DWORD cChars, DWORD dwFlags)
{
    UINT i=0;
    BYTE bData=0;
    BOOL fDone=FALSE;

    ASSERT(pszString);
    ASSERT(cChars);
    
    if (!cChars)
        return NULL;
    
    // Generate cChars-1 characters (leaving space for the terminating NULL)
    for (i=0; i<cChars-1; i++)
    {
        fDone = FALSE;
        while(!fDone)
        {
            bData=(BYTE)Random();
            if (bData<0) bData *= (BYTE)-1;
            bData = bData % 0xFF;  //  generate random chars between 0 and 255

            switch (dwFlags)
            {
                case PERF_FLAG_READABLE :
                    if ((bData >= 32) && (bData <= 126))
                        fDone=TRUE;
                break;
                case PERF_FLAG_ALPHA_NUM :
                    if ((bData >= '0') && (bData <= '9')
                        || (bData >= 'a') && (bData <= 'z')
                        || (bData >= 'A') && (bData <= 'Z'))
                        fDone=TRUE;
                break;
                case PERF_FLAG_ALPHA :
                    if ((bData >= 'a') && (bData <= 'z')
                        || (bData >= 'A') && (bData <= 'Z'))
                        fDone=TRUE;
                break;
                case PERF_FLAG_NUMERIC :
                    if ((bData >= '0') && (bData <= '9'))
                        fDone=TRUE;
                break;

                default : 
                    TRACE(_T("Should never reach here. Unknown Flags\n"));
                    ASSERT(FALSE);
                    GENERIC_FAIL(L_GenStringData);
            }
        }
        pszString[i] = (TCHAR) bData;
    }// for 

    // NULL terminate
    pszString[i] = _T('\0');

    return pszString;

ErrorReturn :
    return NULL;

}


///////////////////////////////////////////////////////////////////////////////
//
//  Function        :   Hlp_GenStringData
//
//  Description    :   Generate random registry data to fill in there. 
//
//  Params: 
//
//  pdwType    :    Specify the type of data you want generated. 
//                          If type is not specified, then a random type is selected. 
//  pbData    :    Buffer to contain the data. pbData cannot be NULL. 
//  cbData    :    Buffer size that controls the amount of data to generate. 
//
//  The function by default will resize the buffer if needed. 
//  If dwtype is not specified, then a randomly selected type will dictate
//  the size of the data generated. 
//
//  For example, if you pass in a 100 byte buffer and specify DWORD, then just 4 bytes
//  of random data will be generated. 
//  
///////////////////////////////////////////////////////////////////////////////
BOOL Hlp_GenRandomValData(DWORD *pdwType, __out_ecount(*pcbData) PBYTE pbData, DWORD *pcbData)
{
    BOOL fRetVal=FALSE;
    DWORD dwChars=0;
    DWORD i=0;
    size_t szlen = 0;
    ASSERT(pbData);
    ASSERT(*pcbData);
    
    if (!pbData)
    {
        TRACE(TEXT("ERROR : Null buffer specified to Hlp_GetValueData. %s %u \n"), _T(__FILE__), __LINE__);
        goto ErrorReturn;
    }

    // If no type is specified, then choose a random type
    if (0==*pdwType)
    {
        UINT uNumber = 0;
        rand_s(&uNumber);
        *pdwType = rg_RegTypes[uNumber % PERF_NUM_REG_TYPES]; //  set a random data type
    }

    UINT uNumber = 0;
    switch(*pdwType)
    {
        case REG_NONE :
        case REG_BINARY :
        case REG_LINK :
        case REG_RESOURCE_LIST :
        case REG_RESOURCE_REQUIREMENTS_LIST :
        case REG_FULL_RESOURCE_DESCRIPTOR :
            for (i=0; i<*pcbData; i++)
            {
                rand_s(&uNumber);
                pbData[i]=(BYTE)uNumber;
            }
        break;

        case REG_SZ : 
            dwChars = *pcbData/sizeof(TCHAR);
            ASSERT(dwChars);
            if(!Hlp_GenStringData((LPTSTR)pbData, dwChars, PERF_FLAG_ALPHA_NUM))
                goto ErrorReturn;
        break;

        case REG_EXPAND_SZ :
            StringCchLength(_T("%SystemRoot%"), STRSAFE_MAX_CCH, &szlen);
            ASSERT(*pcbData >=  (szlen+1) * sizeof(TCHAR) );
            StringCchCopy((LPTSTR)pbData, (*pcbData)-1, _T("%SystemRoot%"));
            
            *pcbData= (szlen+1)*sizeof(TCHAR);
            break;

        case REG_DWORD_BIG_ENDIAN :   
        case REG_DWORD :
            ASSERT(*pcbData >= sizeof(DWORD));
            if (*pcbData < sizeof(DWORD))
            {
                TRACE(TEXT("Insufficient mem passed to Hlp_GenValueData. Send %d bytes, required %d. %s %u\r\n"), 
                            *pcbData, sizeof(DWORD), _T(__FILE__), __LINE__);
                goto ErrorReturn;
            }

            rand_s(&uNumber);
            *(DWORD*)pbData = uNumber;
            *pcbData = sizeof(DWORD);
           
        break;

        // This generates 3 strings in 1.
        case REG_MULTI_SZ :
            dwChars = *pcbData/sizeof(TCHAR);
            memset(pbData, 33, *pcbData);

            ASSERT(dwChars > 6);

            // First Generate a string
            if(!Hlp_GenStringData((LPTSTR)pbData, dwChars, PERF_FLAG_ALPHA_NUM))
                goto ErrorReturn;

            // Now throw in some random terminating NULLs to make it a multi_sz
            for (i=dwChars/3; i<dwChars; i+=dwChars/3)
            {
                ASSERT(i<dwChars);
                *((LPTSTR)pbData+i) = _T('\0');
            }

            // and make sure the last 2 chars are also NULL terminators.
            *((LPTSTR)pbData+dwChars-1)= _T('\0');
            *((LPTSTR)pbData+dwChars-2)= _T('\0');
        break;

        default :
            TRACE(TEXT("TestError : Unknown reg type sent to Hlp_GenValueData : %d. %s %u\n"), *pdwType, _T(__FILE__), __LINE__);
            goto ErrorReturn;
    }
    
    fRetVal=TRUE;
    
ErrorReturn :
    return fRetVal;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Function: Hlp_HKeyToTLA
//
///////////////////////////////////////////////////////////////////////////////
TCHAR* Hlp_HKeyToTLA(HKEY hKey, __out_ecount(cBuffer) TCHAR *pszBuffer, DWORD cBuffer)
{
    if (NULL == pszBuffer)
        goto ErrorReturn;

    StringCchCopy(pszBuffer, cBuffer-1, _T(""));
        
    switch((DWORD)hKey)
    {
        case HKEY_LOCAL_MACHINE :
            StringCchCopy(pszBuffer, cBuffer-1, _T("HKLM"));
        break;
        case HKEY_USERS :
            StringCchCopy(pszBuffer, cBuffer-1, _T("HKU"));
        break;
        case HKEY_CURRENT_USER :
            StringCchCopy(pszBuffer, cBuffer-1, _T("HKCU"));            
        break;
        case HKEY_CLASSES_ROOT :
            StringCchCopy(pszBuffer, cBuffer-1, _T("HKCR"));            
        break;
        default : 
        {
            TRACE(_T("WARNING : WinCE does not support this HKey 0x%x\n"), hKey);
            ASSERT(0);
        }
    }

ErrorReturn :
    return pszBuffer;
}


///////////////////////////////////////////////////////////////////////////////
//
//  Function: Hlp_FillBuffer
//
//  Proposed Flags :
//      HLP_FILL_RANDOM
//      HLP_FILL_SEQUENTIAL
//      HLP_FILL_DONT_CARE  -   fastest
//  
///////////////////////////////////////////////////////////////////////////////
BOOL Hlp_FillBuffer(__out_ecount(cbBuffer) PBYTE pbBuffer, DWORD cbBuffer, DWORD dwFlags)
{
    DWORD   i=0;
    switch (dwFlags)
    {
        case HLP_FILL_RANDOM :
            for (i=0; i<cbBuffer; i++)
            {
                pbBuffer[i] = (BYTE)Random();
            }
            break;
        case HLP_FILL_SEQUENTIAL :
            for (i=0; i<cbBuffer; i++)
            {
                pbBuffer[i] = (BYTE)i;
            }
            break;
        case HLP_FILL_DONT_CARE :
            memset(pbBuffer, 33, cbBuffer);
        break;
    }
    
    return TRUE;
}