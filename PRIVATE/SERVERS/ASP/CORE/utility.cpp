//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: utility.cpp
Abstract: utility fcns
--*/


// #include "script.h"
#include "aspmain.h"

//****************************************************************
//  ASP specific fcns / data
//****************************************************************


//****************************************************************
//  Generic String manipulation fcns
//****************************************************************
const char cszEmpty[] = "";


PSTR MySzDupA(PCSTR pszIn, int iLen) 
{ 
	if(!pszIn) return NULL;
	if(!iLen) iLen = strlen(pszIn);
	PSTR pszOut=MySzAllocA(iLen);
	if(pszOut) {
		memcpy(pszOut, pszIn, iLen); 
		pszOut[iLen] = 0;
	}
	return pszOut; 
}

PWSTR MySzDupW(PCWSTR wszIn, int iLen) 
{ 
	if(!wszIn) return NULL;
	if(!iLen) iLen = wcslen(wszIn);
	PWSTR wszOut=MySzAllocW(iLen);
	if(wszOut) {
		memcpy(wszOut, wszIn, sizeof(WCHAR)*iLen); 
		wszOut[iLen] = 0;
	}
	return wszOut; 
}


PSTR MySzDupWtoA(PCWSTR wszIn, int iInLen, LONG lCodePage)
{
	PSTR pszOut = 0;
	int   iOutLen = WideCharToMultiByte(lCodePage, 0, wszIn, iInLen, 0, 0, 0, 0);
	if(!iOutLen)
		goto error;
	pszOut = MySzAllocA(iOutLen);
	if(!pszOut)
		goto error;
	if(WideCharToMultiByte(lCodePage, 0, wszIn, iInLen, pszOut, iOutLen, 0, 0))
		return pszOut;

error:
	MyFree(pszOut);
	return FALSE;
}


PWSTR MySzDupAtoW(PCSTR pszIn, int iInLen)
{
	PWSTR pwszOut = 0;
	int   iOutLen = MultiByteToWideChar(CP_ACP, 0, pszIn, iInLen, 0, 0);
	if(!iOutLen)
		goto error;
	pwszOut = MySzAllocW(iOutLen);
	if(!pwszOut)
		goto error;
	if(MultiByteToWideChar(CP_ACP, 0, pszIn, iInLen, pwszOut, iOutLen))
		return pwszOut;

error:
	DEBUGMSG(ZONE_ERROR, (L"ASP: MySzDupAtoW(%a, %d) failed. pOut=%0x08x GLE=%d\r\n", pszIn, iInLen, pwszOut, GetLastError()));
	MyFree(pwszOut);
	return FALSE;
}




/*===================================================================
VariantResolveDispatch

    Convert an IDispatch VARIANT to a (non-Dispatch) VARIANT by
    invoking its default property until the object that remains
    is not an IDispatch.  If the original VARIANT is not an IDispatch
    then the behavior is identical to VariantCopyInd(), with the
    exception that arrays are copied.

Parameters:
    pVarOut      - if successful, the return value is placed here
    pVarIn       - the variant to copy
    GUID *iidObj - the calling interface (for error reporting)
    nObjID       - the Object's name from the resource file

    pVarOut need not be initialized.  Since pVarOut is a new
    variant, the caller must VariantClear this object.

Returns:
    The result of calling IDispatch::Invoke.  (either NOERROR or
    the error resulting from the call to Invoke)   may also return
    E_OUTOFMEMORY if an allocation fails

    This function always calls Exception() if an error occurs -
    this is because we need to call Exception() if an IDispatch
    method raises an exception.  Instead of having the client
    worry about whether we called Exception() on its behalf or
    not, we always raise the exception.
===================================================================*/

HRESULT VariantResolveDispatch(VARIANT *pVarOut, VARIANT *pVarIn)
    {
    VARIANT     varResolved;        // value of IDispatch::Invoke
    DISPPARAMS  dispParamsNoArgs = {NULL, NULL, 0, 0};
    EXCEPINFO   ExcepInfo;
    HRESULT     hrCopy;

    DEBUGCHK (pVarIn != NULL && pVarOut != NULL);

    VariantInit(pVarOut);
    if (V_VT(pVarIn) & VT_BYREF)
        hrCopy = VariantCopyInd(pVarOut, pVarIn);
    else
        hrCopy = VariantCopy(pVarOut, pVarIn);

    if (FAILED(hrCopy))
        {
    //    ExceptionId(iidObj, nObjID, (hrCopy == E_OUTOFMEMORY)? IDE_OOM : IDE_UNEXPECTED);
        return hrCopy;
        }

    // follow the IDispatch chain.
    //
    while (V_VT(pVarOut) == VT_DISPATCH)
        {
        HRESULT hrInvoke = S_OK;

        // If the variant is equal to Nothing, then it can be argued
        // with certainty that it does not have a default property!
        // hence we return DISP_E_MEMBERNOTFOUND for this case.
        //
        if (V_DISPATCH(pVarOut) == NULL)
            hrInvoke = DISP_E_MEMBERNOTFOUND;
        else
            {
            VariantInit(&varResolved);
            hrInvoke = V_DISPATCH(pVarOut)->Invoke(
                                                DISPID_VALUE,
                                                IID_NULL,
                                                LOCALE_SYSTEM_DEFAULT,
                                                DISPATCH_PROPERTYGET | DISPATCH_METHOD,
                                                &dispParamsNoArgs,
                                                &varResolved,
                                                &ExcepInfo,
                                                NULL);
            }

        if (FAILED(hrInvoke))
            {
            if (hrInvoke == DISP_E_EXCEPTION)
                {
                //
                // forward the ExcepInfo from Invoke to caller's ExcepInfo
                //
//                Exception(iidObj, ExcepInfo.bstrSource, ExcepInfo.bstrDescription);
                SysFreeString(ExcepInfo.bstrHelpFile);
                }

            else
            {
            }
            //    ExceptionId(iidObj, nObjID, IDE_UTIL_NO_VALUE);

            VariantClear(pVarOut);
            return hrInvoke;
            }

        // The correct code to restart the loop is:
        //
        //      VariantClear(pVar)
        //      VariantCopy(pVar, &varResolved);
        //      VariantClear(&varResolved);
        //
        // however, the same affect can be achieved by:
        //
        //      VariantClear(pVar)
        //      *pVar = varResolved;
        //      VariantInit(&varResolved)
        //
        // this avoids a copy.  The equivalence rests in the fact that
        // *pVar will contain the pointers of varResolved, after we
        // trash varResolved (WITHOUT releasing strings or dispatch
        // pointers), so the net ref count is unchanged. For strings,
        // there is still only one pointer to the string.
        //
        // NOTE: the next interation of the loop will do the VariantInit.
        //
        VariantClear(pVarOut);
        *pVarOut = varResolved;
        }

    return S_OK;
    }



/*============================================================================
SysAllocStringFromSz

Allocate a System BSTR and copy the given ANSI string into it.

Parameters:
    sz              - The string to copy (Note: this IS an "sz", we will stop at the first NULL)
    cch             - the number of ANSI characters in szT.  If 0, will calculate size.
    BSTR *pbstrRet  - the returned BSTR
    lCodePage       - the codepage for conversion

Returns:
    Allocated BSTR in return value
    NOERROR on success, E_OUTOFMEMORY on OOM

Side effects:
    Allocates memory.  Caller must deallocate
============================================================================*/
HRESULT SysAllocStringFromSz
(
CHAR *sz,
DWORD cch,
BSTR *pbstrRet,
UINT lCodePage
)
    {
    BSTR bstrRet;

    DEBUGCHK(pbstrRet != NULL);

    if (sz == NULL)
        {
        *pbstrRet = NULL;
        return(NOERROR);
        }

    // If they passed 0, then determine string length
    if (cch == 0)
        cch = strlen(sz);

    // Allocate a string of the desired length
    // SysAllocStringLen allocates enough room for unicode characters plus a null
    // Given a NULL string it will just allocate the space
    bstrRet = SysAllocStringLen(NULL, cch);
    if (bstrRet == NULL)
        return(E_OUTOFMEMORY);

    // If we were given "", we will have cch=0.  return the empty bstr
    // otherwise, really copy/convert the string
    // NOTE we pass -1 as 4th parameter of MulitByteToWideChar for DBCS support
    if (cch != 0)
        {
        UINT cchTemp = 0;
        if (MultiByteToWideChar(lCodePage, 0, sz, -1, bstrRet, cch+1) == 0)  
           {
               SysFreeString(bstrRet);
               return(HRESULT_FROM_WIN32(GetLastError()));
           }

        // If there are some DBCS characters in the sz(Input), then, the character count of BSTR(DWORD) is
        // already set to cch(strlen(sz)) in SysAllocStringLen(NULL, cch), we cannot change the count,
        // and later call of SysStringLen(bstr) always returns the number of characters specified in the
        // cch parameter at allocation time.  Bad, because one DBCS character(2 bytes) will convert
        // to one UNICODE character(2 bytes), not 2 UNICODE characters(4 bytes).
        // Example: For input sz contains only one DBCS character, we want to see SysStringLen(bstr)
        // = 1, not 2.
        bstrRet[cch] = 0;
        cchTemp = wcslen(bstrRet);
        if (cchTemp < cch)
            {
            BSTR bstrTemp = SysAllocString(bstrRet);
            SysFreeString(bstrRet);
            bstrRet = bstrTemp;
            cch = cchTemp;
            if (bstrTemp == NULL)
                return E_OUTOFMEMORY;
            }
        }
    bstrRet[cch] = 0;
    *pbstrRet = bstrRet;

    return(NOERROR);
    }

int URLEncodeLen(const char *szSrc)
{
    int cbURL = 1;      // add terminator now

    if (!szSrc)
        return 0;

    while (*szSrc)
        {
        if (*szSrc & 0x80)              // encode foreign characters
            cbURL += 3;

        else if (*szSrc == ' ')         // encoded space requires only one character
            ++cbURL;

        else if (!isalnum(*szSrc))  // encode non-alphabetic characters
            cbURL += 3;

        else
            ++cbURL;

        ++szSrc;
        }

    return cbURL;
}


/*===================================================================
URLEncode

URL Encode a string by changing space characters to '+' and escaping
non-alphanumeric characters in hex.

Parameters:
    szDest - Pointer to the buffer to store the URLEncoded string
    szSrc  - Pointer to the source buffer

Returns:
    A pointer to the NUL terminator is returned.
===================================================================*/

char *URLEncode(char *szDest, const char *szSrc)
{
    char hex[] = "0123456789ABCDEF";

    if (!szDest)
        return NULL;
    if (!szSrc)
        {
        *szDest = '\0';
        return szDest;
        }

    while (*szSrc)
        {
        if (*szSrc == ' ')
            {
            *szDest++ = '+';
            ++szSrc;
            }
        else if ( (*szSrc & 0x80) || !isalnum(*szSrc) )
            {
            *szDest++ = '%';
            *szDest++ = hex[BYTE(*szSrc) >> 4];
            *szDest++ = hex[*szSrc++ & 0x0F];
            }

        else
            *szDest++ = *szSrc++;
        }

    *szDest = '\0';
    return szDest;
}


