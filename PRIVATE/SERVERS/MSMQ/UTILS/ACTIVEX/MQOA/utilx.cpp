//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// utilx.Cpp
//=--------------------------------------------------------------------------=
//
// various routines et all that aren't in a file for a particular automation
// object, and don't need to be in the generic ole automation code.
//
//
#include <windows.h>

#if defined (winNT)
#include <assert.h>

#define ASSERT assert

#endif

#if defined (SDK_BUILD)
#if ! defined (ASSERT)
#define ASSERT(c)
#endif
#endif

#include "utilx.h"
#include "mq.h"
#include "limits.h"
#include "time.h"


//=--------------------------------------------------------------------------=
// HELPER: GetSafeArrayDataOfVariant
//=--------------------------------------------------------------------------=
// Gets safe array out of variant 
//
// Parameters:
//    pvarSrc   [in]    source variant containing array
//    ppbBuf    [out]   points to array data
//    pcbBuf    [out]   data size
//
// Output:
//
// Notes:
//
HRESULT GetSafeArrayDataOfVariant(
    VARIANT *pvarSrc,
    BYTE **ppbBuf,
    ULONG *pcbBuf)
{
    SAFEARRAY *psa = NULL;
    UINT nDim, i, cbElem, cbBuf;
    long lLBound, lUBound;
    VOID *pvData;
    HRESULT hresult = NOERROR;

    // UNDONE: for now only support arrays
    if (!V_ISARRAY(pvarSrc)) {
      return E_INVALIDARG;
    }
    *pcbBuf = cbBuf = 0;
    //
    // array: compute byte count
    //
    psa = V_ISBYREF(pvarSrc) ? 
            *pvarSrc->pparray : 
            pvarSrc->parray;
    if (psa) {
      nDim = SafeArrayGetDim(psa);
      cbElem = SafeArrayGetElemsize(psa);
      for (i = 1; i <= nDim; i++) {
        IfFailRet(SafeArrayGetLBound(psa, i, &lLBound));
        IfFailRet(SafeArrayGetUBound(psa, i, &lUBound));
        cbBuf += (lUBound - lLBound + 1) * cbElem;
      }
      IfFailRet(SafeArrayAccessData(psa, &pvData));
      *ppbBuf = (BYTE *)pvData;
    }
    *pcbBuf = cbBuf;
    if (psa) {
      SafeArrayUnaccessData(psa);
    }
    return hresult;
}


//=--------------------------------------------------------------------------=
// HELPER: GetSafeArrayOfVariant
//=--------------------------------------------------------------------------=
// Gets safe array out of variant and puts in user-supplied
//  byte buffer.
//
// Parameters:
//    pvarSrc   [in]    source variant containing array
//    prgbBuf   [out]   target buffer
//    pcbBuf    [out]   buffer size
//
// Output:
//
// Notes:
//
HRESULT GetSafeArrayOfVariant(
    VARIANT *pvarSrc,
    BYTE **prgbBuf,
    ULONG *pcbBuf)
{
    BYTE *pbBuf = NULL;
    ULONG cbBuf;
    HRESULT hresult = NOERROR;

    IfFailRet(GetSafeArrayDataOfVariant(
                pvarSrc,
                &pbBuf,
                &cbBuf));
    if (pbBuf) {
      //
      // delete current buffer
      //
      delete [] *prgbBuf;
      //
      // create new buffer and copy data
      //
      IfNullRet(*prgbBuf = new BYTE[cbBuf]);
      memcpy(*prgbBuf, pbBuf, cbBuf);
    }
    *pcbBuf = cbBuf;
    // fall through...

    return hresult;
}


//=--------------------------------------------------------------------------=
// HELPER: PutSafeArrayOfBuffer
//=--------------------------------------------------------------------------=
// Converts byte buffer into safe array.
//
// Parameters:
//    rgbBuf    [in]    byte buffer to convert
//    cbBuf     [in]    buffer size
//    pvarDest  [out]   destination variant to place safe array
//
// Output:
//
// Notes:
//
HRESULT PutSafeArrayOfBuffer(
    BYTE *rgbBuf,
    UINT cbBuf,
    VARIANT FAR* pvarDest)
{
    SAFEARRAY *psa;
    SAFEARRAYBOUND rgsabound[1];
    long rgIndices[1];
    UINT i;
    HRESULT hresult = NOERROR, hresult2 = NOERROR;

    ASSERT(pvarDest);
    VariantClear(pvarDest);

    // create a 1D byte array
    rgsabound[0].lLbound = 0;
    rgsabound[0].cElements = cbBuf;
    IfNullRet(psa = SafeArrayCreate(VT_UI1, 1, rgsabound));

    if (rgbBuf) {
      //
      // now copy array
      //
      for (i = 0; i < cbBuf; i++) {
        rgIndices[0] = i;
        IfFailGo(SafeArrayPutElement(psa, rgIndices, (VOID *)&rgbBuf[i]));
      }
    }

    // set variant to reference safearray of bytes
    V_VT(pvarDest) = VT_ARRAY | VT_UI1;
    pvarDest->parray = psa;
    return hresult;

Error:
    hresult2 = SafeArrayDestroy(psa);
    if (FAILED(hresult2)) {
      return hresult2;
    }
    return hresult;
}


////////////////////////////////////////////////////////////////////////////////////////////
// AllocateCauVector Allocates a body of a Falcon message, of type CAUI1.
// This allocation must be freed by DeleteCauVector.
//
UCHAR *AllocateCauVector(CAUB *pcaub)
{
    if (pcaub->pElems) {
      delete [] pcaub->pElems;
    }
    // Allocating buffer. 
    pcaub->pElems = new unsigned char[pcaub->cElems];
    return pcaub->pElems;
}

////////////////////////////////////////////////////////////////////////////////////////////
// DeleteCauVector free a body of a Falcon message, of type CAUI1, that was allocated by
// AllocateCauVector.
void DeleteCauVector(CAUB *pcaub)
{
    delete [] pcaub->pElems;
    pcaub->cElems = 0;
    pcaub->pElems = NULL;
}


//=--------------------------------------------------------------------------=
// HELPERS: GetFormatNameType, Is{Private, Public, Direct}OfFormatName
//=--------------------------------------------------------------------------=
// Determines kind of queue: direct, private, public
//
//
// Parameters:
//    bstrFormatName  [in]  names queue
//
// Output:
//    QUEUE_FORMAT_TYPE
//
// Notes:
//    Inspects format name string up to first "=" for
//     literal "DIRECT", "PRIVATE", "PUBLIC"
//
//
// Find out the type of a format name (private, public or direct).
//
QUEUE_FORMAT_TYPE
GetFormatNameType(BSTR bstrFormatName)
{
    LPWSTR lpwcsEqualSign;
    DWORD dwIDLen;

    while (*bstrFormatName != L'\0' && iswspace(*bstrFormatName)) {
      bstrFormatName++;
    }
    if (*bstrFormatName == L'\0') {
      return QUEUE_FORMAT_TYPE_UNKNOWN;
    }
    lpwcsEqualSign = wcschr(bstrFormatName, FORMAT_NAME_EQUAL_SIGN);
    if (!lpwcsEqualSign) {
      return QUEUE_FORMAT_TYPE_UNKNOWN;
    }
    while ((lpwcsEqualSign > bstrFormatName) && iswspace(*(--lpwcsEqualSign)));
    dwIDLen = (lpwcsEqualSign - bstrFormatName) + 1;
    if (dwIDLen == PRIVATE_QUEUE_INDICATOR_LENGTH) {
      if (_wcsnicmp(bstrFormatName, PRIVATE_QUEUE_INDICATOR, dwIDLen) == 0) {
        return QUEUE_FORMAT_TYPE_PRIVATE;
      }
    }

    if (dwIDLen == PUBLIC_QUEUE_INDICATOR_LENGTH) {
      if (_wcsnicmp(bstrFormatName, PUBLIC_QUEUE_INDICATOR, dwIDLen) == 0) {
        return QUEUE_FORMAT_TYPE_PUBLIC;
      }
    }

    if (dwIDLen == DIRECT_QUEUE_INDICATOR_LENGTH) {
      if (_wcsnicmp(bstrFormatName, DIRECT_QUEUE_INDICATOR, dwIDLen) == 0) {
        return QUEUE_FORMAT_TYPE_DIRECT;
      }
    }

    if (dwIDLen == MACHINE_QUEUE_INDICATOR_LENGTH) {
      if (_wcsnicmp(bstrFormatName, MACHINE_QUEUE_INDICATOR, dwIDLen) == 0) {
        return QUEUE_FORMAT_TYPE_MACHINE;
      }
    }

    if (dwIDLen == CONNECTOR_QUEUE_INDICATOR_LENGTH) {
      if (_wcsnicmp(bstrFormatName, CONNECTOR_QUEUE_INDICATOR, dwIDLen) == 0) {
        return QUEUE_FORMAT_TYPE_CONNECTOR;
      }
    }
    return QUEUE_FORMAT_TYPE_UNKNOWN;
}

BOOL IsPrivateQueueOfFormatName(BSTR bstrFormatName)
{
    return GetFormatNameType(bstrFormatName) == QUEUE_FORMAT_TYPE_PRIVATE;
}

BOOL IsPublicQueueOfFormatName(BSTR bstrFormatName)
{
    return GetFormatNameType(bstrFormatName) == QUEUE_FORMAT_TYPE_PUBLIC;
}

BOOL IsDirectQueueOfFormatName(BSTR bstrFormatName)
{
    return GetFormatNameType(bstrFormatName) == QUEUE_FORMAT_TYPE_DIRECT;
}


//=--------------------------------------------------------------------------=
// SystemTimeOfTime
//=--------------------------------------------------------------------------=
// Converts time into systemtime
//
//
// Parameters:
//    iTime       [in] time
//
// Output:
//    [out] SYSTEMTIME
//
// Notes:
//    Various weird conversions: off-by-one months, 1900 blues.
//
BOOL SystemTimeOfTime(time_t iTime, SYSTEMTIME *psystime)
{
    tm *ptmTime = NULL; 
	
	/* TODO: Find an implementation of localtime() 
    ptmTime = localtime(&iTime);
	*/

    if (ptmTime == NULL) {
      // 
      // can't convert time
      //
      return FALSE;
    }
    psystime->wYear = ptmTime->tm_year + 1900;
    psystime->wMonth = ptmTime->tm_mon + 1;
    psystime->wDayOfWeek = ptmTime->tm_wday;
    psystime->wDay = ptmTime->tm_mday;
    psystime->wHour = ptmTime->tm_hour;
    psystime->wMinute = ptmTime->tm_min;
    psystime->wSecond = ptmTime->tm_sec;
    psystime->wMilliseconds = 0;
    return TRUE;
}


//=--------------------------------------------------------------------------=
// TimeOfSystemTime
//=--------------------------------------------------------------------------=
// Converts systemtime into time
//
//
// Parameters:
//    [in] SYSTEMTIME
//
// Output:
//    piTime       [out] time
//
// Notes:
//    Various weird conversions: off-by-one months, 1900 blues.
//
BOOL TimeOfSystemTime(SYSTEMTIME *psystime, time_t *piTime)
{
    tm tmTime; 

    tmTime.tm_year = psystime->wYear - 1900;
    tmTime.tm_mon = psystime->wMonth - 1;
    tmTime.tm_wday = psystime->wDayOfWeek;
    tmTime.tm_mday = psystime->wDay;
    tmTime.tm_hour = psystime->wHour; 
    tmTime.tm_min = psystime->wMinute;
    tmTime.tm_sec = psystime->wSecond; 

	/* TODO: Find an implementation of mktime
    *piTime = mktime(&tmTime);
	*/
	*piTime = (DWORD)-1; // mimic a failure from mktime
    return (*piTime == -1);
}


//=--------------------------------------------------------------------------=
// TimeToVariantTime(time_t iTime, pvtime)
//  Converts time_t to Variant time
//
// Parameters:
//    iTime       [in] time
//    pvtime      [out] 
//
// Output:
//    TRUE if successful else FALSE.
//
// Notes:
//
BOOL TimeToVariantTime(time_t iTime, double *pvtime)
{
    SYSTEMTIME systemtime;

    if (SystemTimeOfTime(iTime, &systemtime)) {
      return SystemTimeToVariantTime(&systemtime, pvtime);
    }
    return FALSE;
}


//=--------------------------------------------------------------------------=
// VariantTimeToTime
//  Converts Variant time to time_t
//
// Parameters:
//    pvarTime   [in]  Variant datetime
//    piTime     [out] time_t
//
// Output:
//    TRUE if successful else FALSE.
//
// Notes:
//
BOOL VariantTimeToTime(VARIANT *pvarTime, time_t *piTime)
{
    // WORD wFatDate, wFatTime;
    SYSTEMTIME systemtime;
    double vtime;

    vtime = GetDateVal(pvarTime);
    if (vtime == 0) {
      return FALSE;
    }
    if (VariantTimeToSystemTime(vtime, &systemtime)) {
      return TimeOfSystemTime(&systemtime, piTime);
    }
    return FALSE;
}


//=--------------------------------------------------------------------------=
// GetVariantTimeOfTime
//=--------------------------------------------------------------------------=
// Converts time to variant time
//
// Parameters:
//    iTime      [in]  time to convert to variant
//    pvarTime - [out] variant time
//
// Output:
//
// Notes:
//
HRESULT GetVariantTimeOfTime(time_t iTime, VARIANT FAR* pvarTime)
{
    double vtime;
    VariantInit(pvarTime);
    if (TimeToVariantTime(iTime, &vtime)) {
      V_VT(pvarTime) = VT_DATE;
      V_DATE(pvarTime) = vtime;
    }
    else {
      V_VT(pvarTime) = VT_ERROR;
      V_ERROR(pvarTime) = 13; // UNDONE: VB type mismatch
    }
    return NOERROR;
}


//=--------------------------------------------------------------------------=
// BstrOfTime
//=--------------------------------------------------------------------------=
// Converts time into a displayable string in user's locale
//
//
// Parameters:
//    iTime       [in] time
//
// Output:
//    [out] string representation
//
// Notes:
//
BSTR BstrOfTime(time_t iTime)
{
    SYSTEMTIME sysTime;
    WCHAR bufDate[128], bufTime[128];
 
    UINT cbDate, cbTime;
    BSTR bstrDate = NULL;

    SystemTimeOfTime(iTime, &sysTime);    
    cbDate = GetDateFormat(
              LOCALE_USER_DEFAULT,
              DATE_SHORTDATE, // flags specifying function options
              &sysTime,       // date to be formatted
              0,              // date format string - zero means default for locale
              bufDate,        // buffer for storing formatted string
              sizeof(bufDate) // size of buffer
              );
    if (cbDate == 0) {

#if DEBUG
      DWORD dwErr;
      dwErr = GetLastError();
      ASSERT(dwErr == 0);
#endif // DEBUG
      IfNullGo(cbDate);
    }

    // add a space
    bufDate[cbDate - 1] = ' ';
    bufDate[cbDate] = 0;  // null terminate
    cbTime = GetTimeFormat(
              LOCALE_USER_DEFAULT,
              TIME_NOSECONDS, // flags specifying function options
              &sysTime,       // date to be formatted
              0,              // time format string - zero means default for locale
              bufTime,        // buffer for storing formatted string
              sizeof(bufTime)/sizeof(bufTime[0]) // size of buffer
        );
    if (cbTime == 0) {
#if DEBUG
      DWORD dwErr;
      dwErr = GetLastError();
      ASSERT(dwErr == 0);
#endif // DEBUG
      IfNullGo(cbTime);
    }
    //
    // concat
    //
    wcscat(bufDate, bufTime);
    //
    // convert to BSTR
    //

    IfNullGo(bstrDate = SysAllocString(bufDate));
   
    // fall through...

Error:
    return bstrDate;
}


// helper: gets default property of VARIANT
//  pvar        [in]
//  pvarDefault [out]
//
HRESULT GetDefaultPropertyOfVariant(
    VARIANT *pvar, 
    VARIANT *pvarDefault)
{
    IDispatch *pdisp;
    LCID lcid = LOCALE_USER_DEFAULT;
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0}; 
    HRESULT hresult;

    pdisp = GetPdisp(pvar);
    if (pdisp) {
      hresult = pdisp->Invoke(DISPID_VALUE,
                              IID_NULL,
			      lcid,
			      DISPATCH_PROPERTYGET,
			      &dispparamsNoArgs,
			      pvarDefault,
			      NULL, // pexcepinfo,
			      NULL  // puArgErr
                              );
      return hresult;
    }
    else {
      return E_INVALIDARG;
    }
}


// helper: gets newenum from object
//  pdisp        [in]
//  ppenum       [out]
//
HRESULT GetNewEnumOfObject(
    IDispatch *pdisp, 
    IEnumVARIANT **ppenum)
{
    LCID lcid = LOCALE_USER_DEFAULT;
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0}; 
    VARIANT varNewEnum;
    IDispatch *pdispNewEnum = NULL;
    HRESULT hresult = NOERROR;

    VariantInit(&varNewEnum);
    ASSERT(pdisp);
    IfFailRet(pdisp->Invoke(DISPID_NEWENUM,
                            IID_NULL,
			    lcid,
			    DISPATCH_PROPERTYGET,
			    &dispparamsNoArgs,
			    &varNewEnum,
			    NULL, // pexcepinfo,
			    NULL  // puArgErr
                            ));
    IfFailRet(VariantChangeType(
                &varNewEnum, 
                &varNewEnum, 
                0, 
                VT_DISPATCH));
    //
    // cast to IEnumVariant
    //
    pdispNewEnum = V_DISPATCH(&varNewEnum);
    IfFailRet(pdispNewEnum->QueryInterface(
                              IID_IEnumVARIANT, 
                              (LPVOID *)ppenum));
    return hresult;
}


// helper: gets a VARIANT VT_<number> or VT_<number> | VT_BYREF
// returns -1 if invalid
//
UINT GetNumber(VARIANT *pvar)
{
    VARIANT varDest;
    HRESULT hresult;

    // attempt to convert to an I4
    VariantInit(&varDest);
    hresult = VariantChangeType(&varDest, pvar, 0, VT_I4);
    return (UINT)(SUCCEEDED(hresult) ? varDest.lVal : UINT_MAX);
}


// helper: gets a VARIANT VT_BOOL or VT_BOOL | VT_BYREF
// returns FALSE if invalid
//
BOOL GetBool(VARIANT *pvar)
{
    switch (pvar->vt) {
    case (VT_BOOL | VT_BYREF):
      return (BOOL)*pvar->pboolVal;
    case VT_BOOL:
      return (BOOL)pvar->boolVal;
    default:
      return FALSE;
    }
}


// helper: gets a VARIANT VT_BSTR or VT_BSTR | VT_BYREF
// returns NULL if invalid
//
BSTR GetBstr(VARIANT *pvar)
{
    BSTR bstr;
    HRESULT hresult;

    hresult = GetTrueBstr(pvar, &bstr);
    return FAILED(hresult) ? NULL : bstr;
}


// helper: gets a VARIANT VT_BSTR or VT_BSTR | VT_BYREF
// returns error if invalid
//
HRESULT GetTrueBstr(VARIANT *pvar, BSTR *pbstr)
{
    VARIANT varDefault;
    HRESULT hresult = NOERROR;

    switch (pvar->vt) {
    case (VT_BSTR | VT_BYREF):
      *pbstr = *pvar->pbstrVal;
      break;
    case VT_BSTR:
      *pbstr = pvar->bstrVal;
      break;
    default:
      // see if it has a default string property
      VariantInit(&varDefault);
      hresult = GetDefaultPropertyOfVariant(pvar, &varDefault);
      if (SUCCEEDED(hresult)) {
        return GetTrueBstr(&varDefault, pbstr);
      }
      break;
    }
    return hresult;
}


// helper: gets a VARIANT VT_UNKNOWN or VT_UNKNOWN | VT_BYREF
// returns NULL if invalid
//
IUnknown *GetPunk(VARIANT *pvar)
{
    if (pvar) {
      if (pvar->vt == (VT_UNKNOWN | VT_BYREF)) {
        return *pvar->ppunkVal;
      }
      else if (pvar->vt == VT_UNKNOWN) {
        return pvar->punkVal;
      }
    }
    return NULL;
}


// helper: gets a VARIANT VT_DISPATCH or VT_DISPATCH | VT_BYREF
// returns NULL if invalid
//
IDispatch *GetPdisp(VARIANT *pvar)
{
    if (pvar) {
      if (pvar->vt == (VT_DISPATCH | VT_BYREF)) {
        return *pvar->ppdispVal;
      }
      else if (pvar->vt == VT_DISPATCH) {
        return pvar->pdispVal;
      }
    }
    return NULL;
}


// helper: gets a VARIANT VT_DATE or VT_DATE | VT_BYREF
// returns 0 if invalid
//
double GetDateVal(VARIANT *pvar)
{
    if (pvar) {
      if (pvar->vt == (VT_DATE | VT_BYREF)) {
        return *V_DATEREF(pvar);
      }
      else if (pvar->vt == VT_DATE) {
        return V_DATE(pvar);
      }
    }
    return 0;
}


//=--------------------------------------------------------------------------=
// StrOfGuidW
//=--------------------------------------------------------------------------=
// returns a wide string from a CLSID or GUID
//
// Parameters:
//    REFIID               - [in]  clsid to make string out of.
//    LPTSTR                - [in]  buffer in which to place resultant GUID.
//
// Output:
//    int                  - number of chars written out.
//
// Notes:
//
int StrOfGuidW
(
    REFIID   riid,
    LPTSTR    pwszBuf
)
{
    return wsprintf(pwszBuf, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", riid.Data1, 
            riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], 
            riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);

}

#define MAXMSGCHARLEN 1024
/*=======================================================
GetMessageOfId
  dwMsgId         [in]
  szDllFile       [in]
  fUseDefaultLcid [in]
  pbstrMessage    [out]

  Returns callee-allocated buffer containing message.
  Caller must release.

 ========================================================*/
BOOL GetMessageOfId(
    DWORD dwMsgId, 
    LPTSTR szDllFile, 
    BOOL fUseDefaultLcid,
    BSTR *pbstrMessage)
{
    DWORD cbMsg;
    HINSTANCE hInst;
    WCHAR wszTmp[MAXMSGCHARLEN];   // unicode chars
    

    *pbstrMessage = NULL;
    hInst = LoadLibrary(szDllFile);
    if (hInst == 0) {
      return FALSE;
    }
    if (fUseDefaultLcid) {
      cbMsg = FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | 
                              FORMAT_MESSAGE_MAX_WIDTH_MASK,
                             hInst,
                             dwMsgId,
                             0,
                             wszTmp,
                             MAXMSGCHARLEN,
                             NULL);
    }
    else {
      cbMsg = LoadString(hInst, dwMsgId, wszTmp, MAXMSGCHARLEN); 
    }
    
    FreeLibrary(hInst);
	
    return (cbMsg != 0);
}


/*=======================================================
GetMessageOfError

  Translate an MQError to a string
  hardwired to loaded error messages from mqutil.dll
  Returns callee-allocated buffer containing message.
  Caller must release.

 ========================================================*/
BOOL GetMessageOfError(DWORD dwMsgId, BSTR *pbstrMessage)
{
    return GetMessageOfId(dwMsgId, 
                          L"MQUTIL.DLL", 
                          TRUE, /* fUseDefaultLcid */
                          pbstrMessage);
}


//=--------------------------------------------------------------------------=
// CreateError
//=--------------------------------------------------------------------------=
// fills in the rich error info object so that both our vtable bound interfaces
// and calls through ITypeInfo::Invoke get the right error informaiton.
//
// Parameters:
//    hrExcep          - [in] the SCODE that should be associated with this err
//    pguid            - [in] the interface id of the offending object:
//                              can be NULL.
//    szName           - [in] the name of the offending object:
//                              can be NULL.
//
// Output:
//    HRESULT          - the HRESULT that was passed in.
//
// Notes:
//
HRESULT CreateError(
    HRESULT hrExcep,
    GUID *pguid,
    LPTSTR wszName)
{
    ICreateErrorInfo *pCreateErrorInfo;
    IErrorInfo *pErrorInfo;
    BSTR bstrMessage = NULL;
    HRESULT hresult = NOERROR;
    
    // first get the createerrorinfo object.
    //
    hresult = CreateErrorInfo(&pCreateErrorInfo);
    if (FAILED(hresult)) return hrExcep;

    // set up some default information on it.
    //
    if (pguid) {
      pCreateErrorInfo->SetGUID(*pguid);
    }
    // pCreateErrorInfo->SetHelpFile(wszHelpFile);
    // pCreateErrorInfo->SetHelpContext(dwHelpContextID);

    // load in the actual error string value.  max of 256.
    if (!GetMessageOfError(hrExcep, &bstrMessage)) {
      return hrExcep;
    }
    pCreateErrorInfo->SetDescription(bstrMessage);

    if (wszName) {
      // load in the source
      pCreateErrorInfo->SetSource(wszName);
    }

    // now set the Error info up with the system
    //
    IfFailGo(pCreateErrorInfo->QueryInterface(IID_IErrorInfo, (void **)&pErrorInfo));
    SetErrorInfo(0, pErrorInfo);
    pErrorInfo->Release();

Error:
    pCreateErrorInfo->Release();
    SysFreeString(bstrMessage);
    return hrExcep;
}


#define SAFE_GUID_KEY L"{7DD95801-9882-11CF-9FA9-00AA006C42C4}"

//=--------------------------------------------------------------------------=
// RegisterAutomationObjectAsSafe
//=--------------------------------------------------------------------------=
// Declares that a given OA obj is safe by adding a bit of stuff:
//  really just a new sub-key.
//
// we add the following information in addition to that set up in
// RegisterAutomationObject:
//
// HKEY_CLASSES_ROOT\
//    CLSID\
//      <CLSID>\
//        Implemented categories\
//          {7DD95801-9882-11CF-9FA9-00AA006C42C4}
//
// Parameters:
//    REFCLSID     - [in] CLSID of the object
//
// Output:
//    BOOL         - FALSE means not all of it was registered
//
// Notes:
//
BOOL RegisterAutomationObjectAsSafe
(
    REFCLSID riidObject
)
{
    HKEY  hk = NULL, hkSub = NULL, hkSafe = NULL;
	WCHAR  wszScratch[MAX_PATH];
    WCHAR  wszGuidStr[LENSTRCLSID]; 
    DWORD dwDummy;
    HRESULT hresult;

    // HKEY_CLASSES_ROOT\CLSID\<CLSID>\
    //        Implemented categories\
    //          {7DD95801-9882-11CF-9FA9-00AA006C42C4}
    //
    if (!StrOfGuidW(riidObject, wszGuidStr)) goto Error;
    wsprintf(wszScratch, L"CLSID\\%s", wszGuidStr);

    IfFailGo(RegCreateKeyEx(
              HKEY_CLASSES_ROOT, 
              wszScratch, 
              0, 
              L"", 
              REG_OPTION_NON_VOLATILE,
              KEY_READ | KEY_WRITE, 
              NULL, 
              &hk, 
              &dwDummy));
    IfFailGo(RegCreateKeyEx(
              hk, 
              L"Implemented categories", 
              0, 
              L"", 
              REG_OPTION_NON_VOLATILE,
              KEY_READ | KEY_WRITE, 
              NULL, 
              &hkSub, 
              &dwDummy));
    IfFailGo(RegCreateKeyEx(
              hkSub, 
              SAFE_GUID_KEY, 
              0, 
              L"", 
              REG_OPTION_NON_VOLATILE,
              KEY_READ | KEY_WRITE, 
              NULL, 
              &hkSafe, 
              &dwDummy));
    RegCloseKey(hkSafe);
    RegCloseKey(hkSub);
    RegCloseKey(hk);
    return TRUE;

Error:
    if (hkSafe) RegCloseKey(hkSafe);
    if (hkSub) RegCloseKey(hkSub);
    if (hk) RegCloseKey(hk);
    return FALSE;
}


