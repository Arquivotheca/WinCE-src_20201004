//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * @(#)DataType.cxx 1.0 6/11/98
 * 
 */
 
#include <windows.h>
#include <ctype.h>

#include "assert.h"
#include "datatype.hxx"

#define TRY
#define CATCH	if(0)
#define ENDTRY
#define ERESULT_NOINFO E_FAIL

// SkipWhiteSpace
const WCHAR *
SkipWhiteSpace(const WCHAR * pS, int cch)
{
    while (cch-- && iswspace(*pS))
        pS++;

    return pS;
}


// ParseDecimal
static 
const WCHAR *
ParseDecimal(const WCHAR * pS, int cch, DWORD * pdw)
{
    WCHAR c;
    DWORD dw = 0;

    while (cch-- && iswdigit(c = *pS))
    {       
        dw = dw * 10 + (c - _T('0'));
        pS++;
        if (dw >= ((0xffffffff) / 10 - 9))    // Check for overflow
            break;
    }

    *pdw = dw;
    return pS;
}


// ParseBinHex
HRESULT
ParseBinHex( const WCHAR * pS, long lLen, 
            BYTE * abData, int * pcbSize, 
            const WCHAR ** ppwcNext)
{
    WCHAR wc;
    bool fLow = false;
    const WCHAR * pwc = pS;
    BYTE bByte;
    byte result;

    // fake a leading zero
    if ( lLen % 2 != 0)
    {
        bByte = 0;
        fLow = true;
    }

    *pcbSize = (lLen + 1) >> 1; // divide in 1/2 with round up

    // walk hex digits pairing them up and shoving the value of each pair into a byte
    while ( lLen-- > 0)
    {
        wc = *pwc++;
        if (wc >= L'a' && wc <= L'f')
        {
            result = 10 + (wc - L'a');
        }
        else if (wc >= L'A' && wc <= L'F')
        {
            result = 10 + (wc - L'A');
        }
        else if (wc >= L'0' && wc <= L'9')
        {
            result = wc - L'0';
        }
        else if (iswspace(wc))
        {
            continue; // skip whitespace
        }
        else
        {
            goto Error;
        }

        assert((0 <= result) && (result < 16));
        if ( fLow)
        {
            bByte += (BYTE)result;
            *abData++ = bByte;
            fLow = false;
        }
        else
        {
            // shift nibble into top half of byte
            bByte = (BYTE)(result << 4);
            fLow = true;
        }   
    }

//Cleanup:
    if (ppwcNext)
        *ppwcNext = pwc;
    return S_OK;

Error:
    return E_FAIL;
}



// ParseISO8601
HRESULT
ParseISO8601(const WCHAR * pS, int cch, 
             DataType dt, DATE * pdate, 
             const WCHAR ** ppwcNext)
{
    HRESULT hr = S_OK;
    UDATE udate;
    const WCHAR * pN;
    FILETIME ftTime;
    __int64 i64Offset;
    int iSign = 0;
    DWORD dw;
    const WCHAR * pSStart = pS;

    // make sure the enums are ordered according to logic below
    assert(DT_DATE_ISO8601 < DT_DATETIME_ISO8601 &&
        DT_DATETIME_ISO8601 < DT_DATETIME_ISO8601TZ &&
        DT_DATETIME_ISO8601TZ < DT_TIME_ISO8601 &&
        DT_TIME_ISO8601 < DT_TIME_ISO8601TZ);

    memset(&udate, 0, sizeof(udate));
    udate.st.wMonth = 1;
    udate.st.wDay = 1;
    pS = SkipWhiteSpace(pS, cch);
    cch -= (int)(pS - pSStart);
    // parse date if allowed
    if (dt < DT_TIME_ISO8601)
    {
        pN = ParseDecimal(pS, cch, &dw);
        if (pN - pS != 4) // 4 digits
            goto Error;
        // HACK: VarDateFromUdate treats 2digit years specially, for y2k compliance
        if (dw < 100)
            goto Error;
        udate.st.wYear = (WORD) dw;
        cch -= 4;
        pS = pN;
        if (*pS == _T('-'))
        {
            pN = ParseDecimal(pS + 1, --cch, &dw);
            if (pN - pS != 3 || 0 == dw || dw > 12) // 2 digits + '-'
                goto Error;
            udate.st.wMonth = (WORD) dw;
            cch -= 2;
            pS = pN;
            if (*pS == _T('-'))
            {
                pN = ParseDecimal(pS + 1, --cch, &dw);
                if (pN - pS != 3 || 0 == dw || dw > 31) // 2 digits + '-'
                    goto Error;
                udate.st.wDay = (WORD) dw;
                cch -= 2;
                pS = pN;
            }
        }
        if (cch && dt >= DT_DATETIME_ISO8601)
        {
            // swallow T
            // Assume starts with T ?
            if (*pS != _T('T'))
                goto Error;
            pS++;
            cch--;
        }
    }
    else
    {
        udate.st.wYear = 1899;
        udate.st.wMonth = 12;
        udate.st.wDay = 30;
    }

    // parse time if allowed
    if (cch && dt >= DT_DATETIME_ISO8601)
    {
        pN = ParseDecimal(pS, cch, &dw);
        if (pN - pS != 2 || dw > 24) // 2 digits + 'T'
            goto Error;
        udate.st.wHour = (WORD) dw;
        cch -= 2;
        pS = pN;
        if (*pS == _T(':'))
        {
            pN = ParseDecimal(pS + 1, --cch, &dw);
            if (pN - pS != 3 || dw > 59) // 2 digits + ':'
                goto Error;
            udate.st.wMinute = (WORD) dw;
            cch -= 2;
            pS = pN;
            if (*pS == _T(':'))
            {
                pN = ParseDecimal(pS + 1, --cch, &dw);
                if (pN - pS != 3 || dw > 59) // 2 digits + ':'
                    goto Error;
                udate.st.wSecond = (WORD) dw;
                cch -= 2;
                pS = pN;
                if (*pS == _T('.'))
                {
                    pN = ParseDecimal(pS + 1, --cch, &dw);
                    int d = (int)(3 - (pN - (pS + 1)));
                    if (d > 2 || dw > 999999999) // at least 1 digit, not more than 9 digits
                        goto Error;
                    cch -= (int)(pN - pS - 1);
                    pS = pN;
                    while (d > 0)
                    {
                        dw *= 10;
                        d--;
                    }
                    while (d < 0)
                    {
                        dw /= 10;
                        d++;
                    }
                    udate.st.wMilliseconds = (WORD) dw;
                }
            }
        }

        // watch for 24:01... etc
        if (udate.st.wHour == 24 && (udate.st.wMinute > 0 || udate.st.wSecond > 0 || udate.st.wMilliseconds > 0))
            goto Error;

        // parse timezone if allowed
        if (cch && (dt == DT_DATETIME_ISO8601TZ || dt == DT_TIME_ISO8601TZ))
        {
            if (*pS == _T('+'))
                iSign = 1;
            else if (*pS == _T('-'))
                iSign = -1;
            else if (*pS == _T('Z'))
            {
                pS++;
                cch--;
            }
            if (iSign)
            {
                if (!SystemTimeToFileTime(&udate.st, &ftTime))
                    goto Error;
                pN = ParseDecimal(pS + 1, --cch, &dw);
                if (pN - pS != 3) // 2 digits + '+' or '-'
                    goto Error;
                i64Offset = (__int64)dw;
                cch -= 2;
                pS = pN;
                if (*pS != ':')
                    goto Error;
                pN = ParseDecimal(pS + 1, --cch, &dw);
                if (pN - pS != 3) // 2 digits + ':'
                    goto Error;
                cch -= 2;
                pS = pN;
                // convert to 100 nanoseconds
                i64Offset = 10000000 * 60 * ((__int64)dw + 60 * i64Offset);
                *(__int64 *)&ftTime = *(__int64 *)&ftTime + iSign * i64Offset;
                if (!FileTimeToSystemTime(&ftTime, &udate.st))
                    goto Error;
            }
        }
    }

    hr = VarDateFromUdate(&udate, 0, pdate);
    if (FAILED(hr))
        goto Cleanup;

Cleanup:
    if (ppwcNext)
        *ppwcNext = pS;

    return hr;

Error:
    hr = E_FAIL;
    goto Cleanup;
}


HRESULT
UnparseDecimal( ce::wstring * pSBuffer, WORD num, long digits)
{
    HRESULT hr = S_OK;
    unsigned short digit;
    unsigned short place = 1;
    // since num is WORD == 16bits, we are better off useing ushort-s above...
    if ( digits > 5)
    {
        hr = E_INVALIDARG;
        goto Cleanup;
    }
    // start with the most significant digit
    while( --digits) place *= 10;

	// for each digit, pull the digit out of num and store it in the string buffer
    while ( place > 0)
    {
        digit = num/place;
        if (digit > 9)
        {
            hr = E_INVALIDARG;
            goto Cleanup;
        }
        pSBuffer->append(_T('0') + digit);
        num -= digit*place;
        place /= 10;
    }
Cleanup:
    return hr;
}


HRESULT
UnparseISO8601( ce::wstring * pReturn, DataType dt, const DATE * pdate)
{
    HRESULT			hr = S_OK;
    UDATE			udate;
	ce::wstring*	pSBuffer;
    
    TRY
    {
        pReturn->reserve(25);
		pReturn->resize(0);
		pSBuffer = pReturn;

        memset(&udate, 0, sizeof(udate));
        hr = VarUdateFromDate(*pdate, 0, &udate);
        if (FAILED(hr))
            goto Cleanup;

        // parse date if allowed
        if (dt < DT_TIME_ISO8601)
        {
            if ((hr = UnparseDecimal(pSBuffer, udate.st.wYear, 4)) != S_OK)
                goto Cleanup;
            pSBuffer->append(_T('-'));

            if ((hr = UnparseDecimal(pSBuffer, udate.st.wMonth, 2)) != S_OK)
                goto Cleanup;
            pSBuffer->append(_T('-'));

            if ((hr = UnparseDecimal(pSBuffer, udate.st.wDay, 2)) != S_OK)
                goto Cleanup;

            if (dt >= DT_DATETIME_ISO8601)
            {
                // starts with T
                pSBuffer->append(_T('T'));
            }
        }

        // parse time if allowed
        if (dt >= DT_DATETIME_ISO8601)
        {
            if ((hr = UnparseDecimal(pSBuffer, udate.st.wHour, 2)) != S_OK)
                goto Cleanup;
            pSBuffer->append(_T(':'));

            if ((hr = UnparseDecimal(pSBuffer, udate.st.wMinute, 2)) != S_OK)
                goto Cleanup;
            pSBuffer->append(_T(':'));

            if ((hr = UnparseDecimal(pSBuffer, udate.st.wSecond, 2)) != S_OK)
                goto Cleanup;
            pSBuffer->append(_T('.'));

            if ((hr = UnparseDecimal(pSBuffer, udate.st.wMilliseconds, 3)) != S_OK)
                goto Cleanup;
        }

        //if (dt == DT_DATETIME_ISO8601TZ || dt == DT_TIME_ISO8601TZ)
        // no time zone...
    }
    CATCH
    {
        hr = ERESULT_NOINFO;
    }
    ENDTRY

Cleanup:
    return hr;

//Error:
    hr = E_FAIL;
    goto Cleanup;
}


HRESULT
UnparseBinHex( ce::wstring * pReturn, BYTE * abData, long lLen)
{
    WCHAR * pText;
    WCHAR * pwc;
    BYTE * pb = abData;
    BYTE nibble;
    long lStrLen = lLen * 2;
    HRESULT hr = S_OK;

    pText = new WCHAR[lStrLen];
    if ( !pText)
    {
        return E_OUTOFMEMORY;
    }

    pwc = pText;
    while ( lLen--)
    {
        // high nibble
        nibble = ((0xf0) & *pb) >> 4;
        if ( nibble > 9)
            *pwc++ = L'a' + nibble - 10;
        else
            *pwc++ = L'0' + nibble;
        // low nibble
        nibble = (0x0f) & *pb;
        if ( nibble > 9)
            *pwc++ = L'a' + nibble - 10;
        else
            *pwc++ = L'0' + nibble;
        // next byte...
        pb++;
    }

    TRY
    {
        pReturn->assign(pText, lStrLen);
    }
    CATCH
    {
        hr = ERESULT_NOINFO;
    }
    ENDTRY


//Cleanup:
    delete [] pText;
    return hr;
}

//==============================================================================
// Base64 code from MTS code sample (SimpleLog)

// These characters are the legal digits, in order, that are 
// used in Base64 encoding 
// 
static 
const WCHAR rgwchBase64[] = 
    L"ABCDEFGHIJKLMNOPQ" 
    L"RSTUVWXYZabcdefgh" 
    L"ijklmnopqrstuvwxy" 
    L"z0123456789+/"; 

HRESULT
UnparseBase64(void * pvData, int cbData, ce::wstring * pReturn) 
{ 
    int cb = cbData; 
    HRESULT hr = S_OK; 
    int cchPerLine = 72;                        // conservative, must be mult of 4 for us 
    int cbPerLine  = cchPerLine / 4 * 3; 
    int cbSafe     = cb + 3;                    // allow for padding 
    int cLine      = cbSafe / cbPerLine + 2;    // conservative 
    int cchNeeded  = cLine * (cchPerLine + 2 /*CRLF*/) + 1 /*NULL*/; 
    int cbNeeded   = cchNeeded * sizeof(WCHAR); 
    WCHAR * wsz = new WCHAR[cbNeeded];
    BYTE*  pb   = (BYTE*)pvData; 
    WCHAR* pch  = wsz; 
    int cchLine = 0; 

    if (!wsz)
    {
        hr = E_OUTOFMEMORY;
    }
    else
    {
        // 
        // Main encoding loop 
        // 
        while (cb >= 3) 
        { 
            BYTE b0 =                     ((pb[0]>>2) & 0x3F); 
            BYTE b1 = ((pb[0]&0x03)<<4) | ((pb[1]>>4) & 0x0F); 
            BYTE b2 = ((pb[1]&0x0F)<<2) | ((pb[2]>>6) & 0x03); 
            BYTE b3 = ((pb[2]&0x3F)); 

            *pch++ = rgwchBase64[b0]; 
            *pch++ = rgwchBase64[b1]; 
            *pch++ = rgwchBase64[b2]; 
            *pch++ = rgwchBase64[b3]; 

            pb += 3; 
            cb -= 3; 
         
            // put in line breaks 
            cchLine += 4; 
            if (cchLine >= cchPerLine) 
            { 
                *pch++ = L'\r'; 
                *pch++ = L'\n'; 
                cchLine = 0; 
            } 
        } 
        // 
        // Account for gunk at the end 
        // 
        if ((cchLine+4) >= cchPerLine)
        {
            *pch++ = L'\r';     // easier than keeping track
            *pch++ = L'\n';
        }

        if (cb==0) 
        { 
            // nothing to do 
        } 
        else if (cb==1) 
        { 
            BYTE b0 =                     ((pb[0]>>2) & 0x3F); 
            BYTE b1 = ((pb[0]&0x03)<<4) | 0; 
            *pch++ = rgwchBase64[b0]; 
            *pch++ = rgwchBase64[b1]; 
            *pch++ = L'='; 
            *pch++ = L'='; 
        } 
        else if (cb==2) 
        { 
            BYTE b0 =                     ((pb[0]>>2) & 0x3F); 
            BYTE b1 = ((pb[0]&0x03)<<4) | ((pb[1]>>4) & 0x0F); 
            BYTE b2 = ((pb[1]&0x0F)<<2) | 0; 
            *pch++ = rgwchBase64[b0]; 
            *pch++ = rgwchBase64[b1]; 
            *pch++ = rgwchBase64[b2]; 
            *pch++ = L'='; 
        } 
     
        // 
        // NULL terminate the string 
        // 
        *pch = NULL; 

        // 
        // Allocate our final output 
        // 
        *pReturn = (wsz);

        #ifdef _DEBUG 
        if (hr==S_OK) 
        { 
            int cb; void * pv = new BYTE[cbData+1];
            assert(S_OK == ParseBase64(wsz, (LONG)(pch - wsz), pv, &cb, null));
            assert(cb == cbData); 
            assert(memcmp(pv, pvData, cbData) == 0); 
            delete [] (BYTE *)pv; 
        } 
        #endif

        delete [] wsz;
    }

//Cleanup:
    return hr;
} 
 
 
HRESULT
ParseBase64(const WCHAR * pwc, int  cch, 
            void * pvData, int * pcbData, 
            const WCHAR ** ppwcNext)
{ 
    HRESULT hr = S_OK;
    BYTE* rgb = (BYTE *)pvData;

    BYTE  mpwchb[256]; 
    BYTE  bBad = (BYTE)-1; 
    BYTE  i; 

    if ( !rgb)
    {
        hr = E_OUTOFMEMORY;
    }
    else
    {
        // 
        // Initialize our decoding array 
        // 
        memset(&mpwchb[0], bBad, 256); 
        for (i = 0; i < 64; i++) 
        { 
            WCHAR wch = rgwchBase64[i]; 
            mpwchb[wch] = i; 
        } 

        // 
        // Loop over the entire input buffer 
        // 
        ULONG bCurrent = 0;         // what we're in the process of filling up 
        int  cbitFilled = 0;        // how many bits in it we've filled 
        BYTE* pb = rgb;             // current destination (not filled)
        const WCHAR* pwch;
        WCHAR wch;
        // 
        for (pwch=pwc; (wch = *pwch) && cch--; pwch++) 
        { 
            // 
            // Ignore white space 
            // 
            if (wch==0x0A || wch==0x0D || wch==0x20 || wch==0x09) 
                continue; 
            // 
            // Have we reached the end? 
            // 
            if (wch==L'=') 
                break; 
            // 
            // How much is this character worth? 
            //
            BYTE bDigit;
            if (wch > 127 || (bDigit = mpwchb[wch]) == bBad)
            {
                hr = E_INVALIDARG;
                goto Cleanup;
            } 
            // 
            // Add in its contribution 
            // 
            bCurrent <<= 6; 
            bCurrent |= bDigit; 
            cbitFilled += 6; 
            // 
            // If we've got enough, output a byte 
            // 
            if (cbitFilled >= 8) 
            {
                ULONG b = (bCurrent >> (cbitFilled-8));     // get's top eight valid bits 
                *pb++ = (BYTE)(b&0xFF);                     // store the byte away 
                cbitFilled -= 8; 
            } 
        } 

        *pcbData = (ULONG)(pb-rgb); // length

        while (wch==L'=')
        {
            cbitFilled = 0;
            pwch++;
            wch = *pwch;
        }

        if (cbitFilled)
        {
            ULONG b = (bCurrent >> (cbitFilled-8));     // get's top eight valid bits 
            if (b)
            {
                hr = E_INVALIDARG;
                goto Cleanup;
            }
        }

        // characters used
        if (ppwcNext)
            *ppwcNext = pwch;
    }
    
Cleanup:
    return hr;
}
