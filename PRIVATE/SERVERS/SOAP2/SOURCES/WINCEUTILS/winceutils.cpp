//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "WinCEUtils.h"
#include <stdlib.h>

const LPCTSTR k_szRegSoap = TEXT("Software\\Microsoft\\Soap");


class DummyMarshal : public IMarshal
{
    public:
        DummyMarshal(IUnknown *punkCtrl);
        ULONG STDMETHODCALLTYPE AddRef();
        ULONG STDMETHODCALLTYPE Release();
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);
        HRESULT STDMETHODCALLTYPE GetUnmarshalClass(REFIID riid,void * pv,DWORD dwDestContext,void * pvDestContext,DWORD mshlflags,CLSID * pCid ) {ASSERT(FALSE); return E_NOTIMPL;}
        HRESULT STDMETHODCALLTYPE GetMarshalSizeMax(REFIID riid,void *pv,DWORD dwDestContext,void * pvDestContext,DWORD mshlflags,ULONG * pSize ) {ASSERT(FALSE); return E_NOTIMPL;}
        HRESULT STDMETHODCALLTYPE MarshalInterface(IStream *pStm,REFIID riid,void *pv,DWORD dwDestContext,void *pvDestContext,DWORD mshlflags ) {ASSERT(FALSE); return E_NOTIMPL;} 
        HRESULT STDMETHODCALLTYPE UnmarshalInterface(IStream * pStm,REFIID riid,void ** ppv ) {ASSERT(FALSE); return E_NOTIMPL;}
        HRESULT STDMETHODCALLTYPE ReleaseMarshalData(IStream * pStm ) {ASSERT(FALSE); return E_NOTIMPL;}
        HRESULT STDMETHODCALLTYPE DisconnectObject(DWORD dwReserved ) {ASSERT(FALSE); return E_NOTIMPL;}
  
private:
        IUnknown *punkCtrl;    
        ULONG _refCount;
};


HRESULT (*g_pfn_CoCreateFreeThreadedMarshaler)(LPUNKNOWN punkOuter, LPUNKNOWN * ppunkMarshaler ) = NULL;
    
    
HRESULT CoCreateFreeThreadedMarshalerE(
LPUNKNOWN punkOuter,
LPUNKNOWN * ppunkMarshaler )
{    
    HRESULT hr = E_INVALIDARG;
    static BOOL fInited = FALSE;
    
    if(!fInited)
    {
        //NOTE:  we statically link against ole32.dll so its okay
        //  that we FreeLibrary and keep a pointer to the 
        //  CoCreateFreeThreadedMarshaler function
        HINSTANCE hInstOLE32DLL = LoadLibrary (L"ole32.dll");
        
        fInited = TRUE;
        g_pfn_CoCreateFreeThreadedMarshaler = NULL;
        
        //
        //  See if we can get the FTM from ole32.dll
        //
        if(NULL != hInstOLE32DLL)
        {
            g_pfn_CoCreateFreeThreadedMarshaler = (HRESULT (*)(LPUNKNOWN, LPUNKNOWN *))GetProcAddress (hInstOLE32DLL, L"CoCreateFreeThreadedMarshaler");  
            FreeLibrary(hInstOLE32DLL);
        }
    }
    
    //
    //  See if we can get the FTM from ole32.dll
    //
    if(NULL != g_pfn_CoCreateFreeThreadedMarshaler)
    {      
       return g_pfn_CoCreateFreeThreadedMarshaler(punkOuter, ppunkMarshaler);          
    }

    //
    //  If we *CANT* get it from ole32.dll, give them a dummy object
    //   
    *ppunkMarshaler = NULL;
    hr = E_OUTOFMEMORY;

    DummyMarshal *pDummy = new DummyMarshal(punkOuter);
  
    if(NULL != pDummy)
    {
        *ppunkMarshaler = pDummy;
        hr = S_OK;
    }


    return hr;
}

//  --> rather than pulling 'environment' variable from
//      environment, use the registry
DWORD GetEnvironmentVariableW(
  LPCTSTR lpName,  // environment variable name
  LPTSTR lpBuffer, // buffer for variable value
  DWORD nSize      // size of buffer
)
{
    HKEY hServiceKey;
    DWORD dwRes;
    if ((ERROR_SUCCESS == (dwRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        k_szRegSoap, 0, KEY_QUERY_VALUE, &hServiceKey)))) 
    {            
            DWORD dwType;
            DWORD dwSize = nSize;
            if((ERROR_SUCCESS == (dwRes = RegQueryValueEx(hServiceKey,
                lpName, NULL, &dwType, (LPBYTE)lpBuffer,
                &dwSize))))
            {
                if(dwType != REG_SZ)
                    return 0;
                return dwSize - 1; //chop the NULL (to conform to GetEnvirVar spec)                    
            }
            else if(dwRes == ERROR_MORE_DATA)
                return dwSize;
        RegCloseKey(hServiceKey);      
    }
    return 0;       
}


DWORD GetEnvironmentVariableA
(
    LPCSTR name,
    LPSTR value,
    DWORD size
)
{
    WCHAR wName[_MAX_PATH];
    
    if(_MAX_PATH <= size || -1 == mbstowcs(wName, name, size))
        return 0;


    //convert 'name' to a wide char
    WCHAR *pTempBuf = new WCHAR[size];
    if(!pTempBuf) 
        return 0;
    
    DWORD retSize =  GetEnvironmentVariableW(wName, pTempBuf, size);

    if(retSize == 0) 
    {
        delete [] pTempBuf;
        return 0;
    }

    if(-1 == wcstombs(value, pTempBuf, retSize+1))
    {
        delete [] pTempBuf;
        return 0;
    }
    
    delete [] pTempBuf;
    return retSize;
}



DWORD
WINAPI
My_GetModuleFileNameA(
    HMODULE hModule,
    LPSTR lpFilename,
    DWORD nSize
    )
{
    WCHAR *fName = new WCHAR[nSize + 1];

    if(!fName)
        return 0;

    DWORD dwRet = GetModuleFileName(hModule, fName, nSize);

    if(0 != dwRet)
    {
        size_t i;
        if(-1 == (i = wcstombs(lpFilename, fName, nSize)))
            dwRet = 0;
        else
            lpFilename[i] = NULL;        
    }

    delete [] fName;
    return dwRet;
}




HANDLE
WINAPI
My_CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
    )
{
    WCHAR buf[_MAX_PATH];
    mbstowcs(buf, lpFileName, _MAX_PATH);

    return CreateFile(
         buf,
         dwDesiredAccess,
         dwShareMode,
         lpSecurityAttributes,
         dwCreationDisposition,
         dwFlagsAndAttributes,
         hTemplateFile);
}






HMODULE
WINAPI
My_LoadLibraryExA(
     LPCSTR lpLibFileName,
     HANDLE hFile,
     DWORD dwFlags
    )
{
    if(dwFlags == LOAD_WITH_ALTERED_SEARCH_PATH)
        dwFlags = 0;

    WCHAR buf[_MAX_PATH];
    mbstowcs(buf, lpLibFileName, _MAX_PATH);

    return LoadLibraryEx(
      buf,
      hFile,
      dwFlags);    
}


LONG
My_RegCreateKeyExA (
     HKEY hKey,
     LPCSTR lpSubKey,
     DWORD Reserved,
     LPSTR lpClass,
     DWORD dwOptions,
     REGSAM samDesired,
     LPSECURITY_ATTRIBUTES lpSecurityAttributes,
     PHKEY phkResult,
     LPDWORD lpdwDisposition
    )
{
    WCHAR wcDest[_MAX_PATH];
    WCHAR *pwcDest = wcDest;
    if(lpSubKey)
    {
        if(-1 == mbstowcs(wcDest, lpSubKey, _MAX_PATH))
            return ERROR_BAD_ARGUMENTS;
    }
    else
        pwcDest = NULL;

    
    WCHAR wcDest2[_MAX_PATH];
    WCHAR *pwcDest2 = wcDest2;
    if(lpClass)
    {
       if(-1 == mbstowcs(wcDest2, lpClass, _MAX_PATH))
            return ERROR_BAD_ARGUMENTS;
    } 
    else
        pwcDest2 = NULL;       

    return RegCreateKeyEx (
          hKey,
          pwcDest,
          Reserved,
          pwcDest2,
          dwOptions,
          samDesired,
          lpSecurityAttributes,
          phkResult,
          lpdwDisposition);
}



LONG
APIENTRY
My_RegDeleteKeyA (
    HKEY hKey,
    LPCSTR lpSubKey
    )
{
    WCHAR wcDest[_MAX_PATH];
    if(-1 == mbstowcs(wcDest, lpSubKey, _MAX_PATH))
        return ERROR_BAD_ARGUMENTS;

    return RegDeleteKey (
         hKey,
         wcDest);
}

LONG
My_RegEnumKeyExA (
     HKEY hKey,
     DWORD dwIndex,
     LPSTR lpName,
     LPDWORD lpcbName,
     LPDWORD lpReserved,
     LPSTR lpClass,
     LPDWORD lpcbClass,
     PFILETIME lpftLastWriteTime
    )
{
    //verify incoming params
    if(!lpName || !lpcbName)
    {
      ASSERT(FALSE);
      return ERROR_BAD_ARGUMENTS;
    }


    DWORD dwNameSize = *lpcbName;
    DWORD dwClassSize;
    WCHAR *wcName = new WCHAR[dwNameSize];
    WCHAR *wcClass = NULL;
    LONG ret;

    if(!wcName)
    {
        ASSERT(FALSE);
        ret =  ERROR_NOT_ENOUGH_MEMORY;
        goto Done;
    }

    if(lpcbClass)
    {
        dwClassSize = *lpcbClass;
        wcClass = new WCHAR[dwClassSize];
        if(!wcClass)
        {
            ret = ERROR_NOT_ENOUGH_MEMORY;
            goto Done;
        }
    }


    ret = RegEnumKeyEx (
             hKey,
             dwIndex,
             wcName,
             lpcbName,
             lpReserved,
             wcClass,
             lpcbClass,
             lpftLastWriteTime);

    if(ERROR_SUCCESS != ret)
        goto Done;

    if(-1 == wcstombs(lpName,wcName, dwNameSize))
    {
        ret = ERROR_BAD_ARGUMENTS;
        goto Done;
    }

    if(lpcbClass)
    {
        if(-1 == wcstombs(lpClass, wcClass, dwClassSize))
        {
             ret = ERROR_BAD_ARGUMENTS;
             goto Done;
        }
    }
    

Done:
    if(wcName)
        delete [] wcName;

    if(wcClass)
        delete [] wcClass;

    return ret;
}


LONG
My_RegSetValueExA (
    HKEY hKey,
    LPCSTR lpValueName,
    DWORD Reserved,
    DWORD dwType,
    CONST BYTE* lpData,
    DWORD cbData
    )
{
    WCHAR wcDest[_MAX_PATH];
    WCHAR *pwcDest = wcDest;

    if(lpValueName)
    {
        if(-1 == mbstowcs(wcDest, lpValueName, _MAX_PATH))
            return ERROR_BAD_ARGUMENTS;
    }
    else
        pwcDest = NULL;

    //if the data type is a string, convert it to unicode
    if(dwType == REG_SZ)
    {
         WCHAR dta[_MAX_PATH];
         size_t stSize;
         if(_MAX_PATH <= cbData || -1 == (stSize = mbstowcs(dta, (char *)lpData, cbData)))
            return ERROR_BAD_ARGUMENTS;

         dta[stSize] = NULL;

         return RegSetValueEx (
            hKey,
            pwcDest,
            Reserved,
            dwType,
            (UCHAR *)dta,
            (stSize+1) * 2);
    }
    else
    {
         return RegSetValueEx (
            hKey,
            pwcDest,
            Reserved,
            dwType,
            lpData,
            cbData);
    }
}




void * __cdecl bsearch (
        const void *key,
        const void *base,
        size_t num,
        size_t width,
        int (__cdecl *compare)(const void *, const void *)
        )
{
        char *lo = (char *)base;
        char *hi = (char *)base + (num - 1) * width;
        char *mid;
        unsigned int half;
        int result;

        while (lo <= hi)
                if (half = num / 2)
                {
                        mid = lo + (num & 1 ? half : (half - 1)) * width;
                        if (!(result = (*compare)(key,mid)))
                                return(mid);
                        else if (result < 0)
                        {
                                hi = mid - width;
                                num = num & 1 ? half : half-1;
                        }
                        else    {
                                lo = mid + width;
                                num = half;
                        }
                }
                else if (num)
                        return((*compare)(key,lo) ? NULL : lo);
                else
                        break;

        return(NULL);
}



//**********************************************************************
//
// VarDecCmp - Decimal Compare
//
//**********************************************************************
typedef union {
    DWORDLONG int64;  
    struct {         
#if HP_BIGENDIAN
      ULONG Hi;
      ULONG Lo;
#else             
      ULONG Lo;
      ULONG Hi;
#endif             
    };             
} SPLIT64;


#define COPYDEC(dest, src) {(dest).signscale = (src).signscale; (dest).Hi32 = (src).Hi32; (dest).Lo64 = (src).Lo64;}

#define DEC_SCALE_MAX    28
#define POWER10_MAX 9
ULONG rgulPower10[POWER10_MAX+1] = {1, 10, 100, 1000, 10000, 100000, 1000000,
                    10000000, 100000000, 1000000000};
static const ULONG ulTenToNine      = 1000000000;
#define Div64by32(num, den) ((ULONG)((DWORDLONG)(num) / (ULONG)(den)))
#define Mod64by32(num, den) ((ULONG)((DWORDLONG)(num) % (ULONG)(den)))

inline DWORDLONG DivMod64by32(DWORDLONG num, ULONG den)
{
    SPLIT64  sdl;

    sdl.Lo = Div64by32(num, den);
    sdl.Hi = Mod64by32(num, den);
    return sdl.int64;
}


/***
* ScaleResult
*
* Entry:
*   rgulRes - Array of ULONGs with value, least-significant first.
*   iHiRes  - Index of last non-zero value in rgulRes.
*   iScale  - Scale factor for this value, range 0 - 2 * DEC_SCALE_MAX
*
* Purpose:
*   See if we need to scale the result to fit it in 96 bits.
*   Perform needed scaling.  Adjust scale factor accordingly.
*
* Exit:
*   rgulRes updated in place, always 3 ULONGs.
*   New scale factor returned, -1 if overflow error.
*
***********************************************************************/

int ScaleResult(ULONG *rgulRes, int iHiRes, int iScale)
{
    int        iNewScale;
    int        iCur;
    ULONG   ulPwr;
    ULONG   ulTmp;
    ULONG   ulSticky;
    SPLIT64 sdlTmp;

    // See if we need to scale the result.  The combined scale must
    // be <= DEC_SCALE_MAX and the upper 96 bits must be zero.
    // 
    // Start by figuring a lower bound on the scaling needed to make
    // the upper 96 bits zero.    iHiRes is the index into rgulRes[]
    // of the highest non-zero ULONG.
    // 
    iNewScale =      iHiRes * 32 - 64 - 1;
    if (iNewScale > 0) {

      // Find the MSB.
      //
      ulTmp = rgulRes[iHiRes];
      if (!(ulTmp & 0xFFFF0000)) {
    iNewScale -= 16;
    ulTmp <<= 16;
      }
      if (!(ulTmp & 0xFF000000)) {
    iNewScale -= 8;
    ulTmp <<= 8;
      }
      if (!(ulTmp & 0xF0000000)) {
    iNewScale -= 4;
    ulTmp <<= 4;
      }
      if (!(ulTmp & 0xC0000000)) {
    iNewScale -= 2;
    ulTmp <<= 2;
      }
      if (!(ulTmp & 0x80000000)) {
    iNewScale--;
    ulTmp <<= 1;
      }
    
      // Multiply bit position by log10(2) to figure it's power of 10.
      // We scale the log by 256.  log(2) = .30103, * 256 = 77.     Doing this 
      // with a multiply saves a 96-byte lookup table.    The power returned
      // is <= the power of the number, so we must add one power of 10
      // to make it's integer part zero after dividing by 256.
      // 
      // Note: the result of this multiplication by an approximation of
      // log10(2) have been exhaustively checked to verify it gives the 
      // correct result.  (There were only 95 to check...)
      // 
      iNewScale = ((iNewScale * 77) >> 8) + 1;

      // iNewScale = min scale factor to make high 96 bits zero, 0 - 29.
      // This reduces the scale factor of the result.  If it exceeds the
      // current scale of the result, we'll overflow.
      // 
      if (iNewScale > iScale)
    return -1;
    }
    else
      iNewScale = 0;

    // Make sure we scale by enough to bring the current scale factor
    // into valid range.
    //
    if (iNewScale < iScale - DEC_SCALE_MAX)
      iNewScale = iScale - DEC_SCALE_MAX;

    if (iNewScale != 0) {
      // Scale by the power of 10 given by iNewScale.  Note that this is 
      // NOT guaranteed to bring the number within 96 bits -- it could 
      // be 1 power of 10 short.
      //
      iScale -= iNewScale;
      ulSticky = 0;
      sdlTmp.Hi = 0; // initialize remainder

      for (;;) {

    ulSticky |= sdlTmp.Hi; // record remainder as sticky bit

    if (iNewScale > POWER10_MAX)
      ulPwr = ulTenToNine;
    else
      ulPwr = rgulPower10[iNewScale];

    // Compute first quotient.
    // DivMod64by32 returns quotient in Lo, remainder in Hi.
    //
    sdlTmp.int64 = DivMod64by32(rgulRes[iHiRes], ulPwr);
    rgulRes[iHiRes] = sdlTmp.Lo;
    iCur = iHiRes - 1;

    if (iCur >= 0) {
      // If first quotient was 0, update iHiRes.
      //
      if (sdlTmp.Lo == 0)
        iHiRes--;

      // Compute subsequent quotients.
      //
      do {
        sdlTmp.Lo = rgulRes[iCur];
        sdlTmp.int64 = DivMod64by32(sdlTmp.int64, ulPwr);
        rgulRes[iCur] = sdlTmp.Lo;
        iCur--;
      } while (iCur >= 0);

    }

    iNewScale -= POWER10_MAX;
    if (iNewScale > 0)
      continue; // scale some more

    // If we scaled enough, iHiRes would be 2 or less.  If not,
    // divide by 10 more.
    //
    if (iHiRes > 2) {
      iNewScale = 1;
      iScale--;
      continue; // scale by 10
    }

    // Round final result.    See if remainder >= 1/2 of divisor.
    // If remainder == 1/2 divisor, round up if odd or sticky bit set.
    //
    ulPwr >>= 1;  // power of 10 always even
    if ( ulPwr <= sdlTmp.Hi && (ulPwr < sdlTmp.Hi ||
        ((rgulRes[0] & 1) | ulSticky)) ) {
      iCur = -1;
      while (++rgulRes[++iCur] == 0);

      if (iCur > 2) {
        // The rounding caused us to carry beyond 96 bits. 
        // Scale by 10 more.
        //
        iHiRes = iCur;
        ulSticky = 0;  // no sticky bit
        sdlTmp.Hi = 0; //    or remainder
        iNewScale = 1;
        iScale--;
        continue; // scale by 10
      }
    }

    // We may have scaled it more than we planned.    Make sure the scale 
    // factor hasn't gone negative, indicating overflow.
    // 
    if (iScale < 0)
      return -1;

    return iScale;
      } // for(;;)
    }
    return iScale;
}



STDAPI DecAddSub(LPDECIMAL pdecL, LPDECIMAL pdecR, LPDECIMAL pdecRes, char bSign)
{
    ULONG     rgulNum[6];
    ULONG     ulPwr;
    int          iScale;
    int          iHiProd;
    int          iCur;
    SPLIT64   sdlTmp;
    DECIMAL   decRes;
    DECIMAL   decTmp;
    LPDECIMAL pdecTmp;

    bSign ^= (pdecR->sign ^ pdecL->sign) & DECIMAL_NEG;

    if (pdecR->scale == pdecL->scale) {
      // Scale factors are equal, no alignment necessary.
      //
      decRes.signscale = pdecL->signscale;

AlignedAdd:
      if (bSign) {
    // Signs differ - subtract
    //
    decRes.Lo64 = pdecL->Lo64 - pdecR->Lo64;
    decRes.Hi32 = pdecL->Hi32 - pdecR->Hi32;

    // Propagate carry
    //
    if (decRes.Lo64 > pdecL->Lo64) {
      decRes.Hi32--;
      if (decRes.Hi32 >= pdecL->Hi32)
        goto SignFlip;
    }
    else if (decRes.Hi32 > pdecL->Hi32) {
      // Got negative result.  Flip its sign.
      // 
SignFlip:
      decRes.Lo64 = -(LONGLONG)decRes.Lo64;
      decRes.Hi32 = ~decRes.Hi32;
      if (decRes.Lo64 == 0)
        decRes.Hi32++;
      decRes.sign ^= DECIMAL_NEG;
    }

      }
      else {
    // Signs are the same - add
    //
    decRes.Lo64 = pdecL->Lo64 + pdecR->Lo64;
    decRes.Hi32 = pdecL->Hi32 + pdecR->Hi32;

    // Propagate carry
    //
    if (decRes.Lo64 < pdecL->Lo64) {
      decRes.Hi32++;
      if (decRes.Hi32 <= pdecL->Hi32)
        goto AlignedScale;
    }
    else if (decRes.Hi32 < pdecL->Hi32) {
AlignedScale:
      // The addition carried above 96 bits.  Divide the result by 10,
      // dropping the scale factor.
      // 
      if (decRes.scale == 0)
        return DISP_E_OVERFLOW;
      decRes.scale--;

      sdlTmp.Lo = decRes.Hi32;
      sdlTmp.Hi = 1;
      sdlTmp.int64 = DivMod64by32(sdlTmp.int64, 10);
      decRes.Hi32 = sdlTmp.Lo;

      sdlTmp.Lo = decRes.Mid32;
      sdlTmp.int64 = DivMod64by32(sdlTmp.int64, 10);
      decRes.Mid32 = sdlTmp.Lo;

      sdlTmp.Lo = decRes.Lo32;
      sdlTmp.int64 = DivMod64by32(sdlTmp.int64, 10);
      decRes.Lo32 = sdlTmp.Lo;

      // See if we need to round up.
      //
      if (sdlTmp.Hi >= 5 && (sdlTmp.Hi > 5 || (decRes.Lo32 & 1))) {
        if (++decRes.Lo64 == 0)
          decRes.Hi32++;
      }
    }
      }
    }
    else {
      // Scale factors are not equal.  Assume that a larger scale
      // factor (more decimal places) is likely to mean that number
      // is smaller.  Start by guessing that the right operand has
      // the larger scale factor.  The result will have the larger
      // scale factor.
      //
      decRes.scale = pdecR->scale;  // scale factor of "smaller"
      decRes.sign = pdecL->sign;    // but sign of "larger"
      iScale = decRes.scale - pdecL->scale;

      if (iScale < 0) {
    // Guessed scale factor wrong. Swap operands.
    //
    iScale = -iScale;
    decRes.scale = pdecL->scale;
    decRes.sign ^= bSign;
    pdecTmp = pdecR;
    pdecR = pdecL;
    pdecL = pdecTmp;
      }

      // *pdecL will need to be multiplied by 10^iScale so
      // it will have the same scale as *pdecR.     We could be
      // extending it to up to 192 bits of precision.
      //
      if (iScale <= POWER10_MAX) {
    // Scaling won't make it larger than 4 ULONGs
    //
    ulPwr = rgulPower10[iScale];
    decTmp.Lo64 = UInt32x32To64(pdecL->Lo32, ulPwr);
    sdlTmp.int64 = UInt32x32To64(pdecL->Mid32, ulPwr);
    sdlTmp.int64 += decTmp.Mid32;
    decTmp.Mid32 = sdlTmp.Lo;
    decTmp.Hi32 = sdlTmp.Hi;
    sdlTmp.int64 = UInt32x32To64(pdecL->Hi32, ulPwr);
    sdlTmp.int64 += decTmp.Hi32;
    if (sdlTmp.Hi == 0) {
      // Result fits in 96 bits.  Use standard aligned add.
      //
      decTmp.Hi32 = sdlTmp.Lo;
      pdecL = &decTmp;
      goto AlignedAdd;
    }
    rgulNum[0] = decTmp.Lo32;
    rgulNum[1] = decTmp.Mid32;
    rgulNum[2] = sdlTmp.Lo;
    rgulNum[3] = sdlTmp.Hi;
    iHiProd = 3;
      }
      else {
    // Have to scale by a bunch.  Move the number to a buffer
    // where it has room to grow as it's scaled.
    //
    rgulNum[0] = pdecL->Lo32;
    rgulNum[1] = pdecL->Mid32;
    rgulNum[2] = pdecL->Hi32;
    iHiProd = 2;

    // Scan for zeros in the upper words.
    //
    if (rgulNum[2] == 0) {
      iHiProd = 1;
      if (rgulNum[1] == 0) {
        iHiProd = 0;
        if (rgulNum[0] == 0) {
          // Left arg is zero, return right.
          //
          decRes.Lo64 = pdecR->Lo64;
          decRes.Hi32 = pdecR->Hi32;
          decRes.sign ^= bSign;
          goto RetDec;
        }
      }
    }

    // Scaling loop, up to 10^9 at a time.    iHiProd stays updated
    // with index of highest non-zero ULONG.
    //
    for (; iScale > 0; iScale -= POWER10_MAX) {
      if (iScale > POWER10_MAX)
        ulPwr = ulTenToNine;
      else
        ulPwr = rgulPower10[iScale];

      sdlTmp.Hi = 0;
      for (iCur = 0; iCur <= iHiProd; iCur++) {
        sdlTmp.int64 = UInt32x32To64(rgulNum[iCur], ulPwr) + sdlTmp.Hi;
        rgulNum[iCur] = sdlTmp.Lo;
      }

      if (sdlTmp.Hi != 0)
        // We're extending the result by another ULONG.
        rgulNum[++iHiProd] = sdlTmp.Hi;
    }
      }

      // Scaling complete, do the add.  Could be subtract if signs differ.
      //
      sdlTmp.Lo = rgulNum[0];
      sdlTmp.Hi = rgulNum[1];

      if (bSign) {
    // Signs differ, subtract.
    //
    decRes.Lo64 = sdlTmp.int64 - pdecR->Lo64;
    decRes.Hi32 = rgulNum[2] - pdecR->Hi32;

    // Propagate carry
    //
    if (decRes.Lo64 > sdlTmp.int64) {
      decRes.Hi32--;
      if (decRes.Hi32 >= rgulNum[2])
        goto LongSub;
    }
    else if (decRes.Hi32 > rgulNum[2]) {
LongSub:
      // If rgulNum has more than 96 bits of precision, then we need to 
      // carry the subtraction into the higher bits.  If it doesn't, 
      // then we subtracted in the wrong order and have to flip the 
      // sign of the result.
      // 
      if (iHiProd <= 2)
        goto SignFlip;

      iCur = 3;
      while(rgulNum[iCur++]-- == 0);
      if (rgulNum[iHiProd] == 0)
        iHiProd--;
    }
      }
      else {
    // Signs the same, add.
    //
    decRes.Lo64 = sdlTmp.int64 + pdecR->Lo64;
    decRes.Hi32 = rgulNum[2] + pdecR->Hi32;

    // Propagate carry
    //
    if (decRes.Lo64 < sdlTmp.int64) {
      decRes.Hi32++;
      if (decRes.Hi32 <= rgulNum[2])
        goto LongAdd;
    }
    else if (decRes.Hi32 < rgulNum[2]) {
LongAdd:
      // Had a carry above 96 bits.
      //
      iCur = 3;
      do {
        if (iHiProd < iCur) {
          rgulNum[iCur] = 1;
          iHiProd = iCur;
          break;
        }
      }while (++rgulNum[iCur++] == 0);
    }
      }

      if (iHiProd > 2) {
    rgulNum[0] = decRes.Lo32;
    rgulNum[1] = decRes.Mid32;
    rgulNum[2] = decRes.Hi32;
    decRes.scale = ScaleResult(rgulNum, iHiProd, decRes.scale);
    if (decRes.scale == -1)
      return DISP_E_OVERFLOW;

    decRes.Lo32 = rgulNum[0];
    decRes.Mid32 = rgulNum[1];
    decRes.Hi32 = rgulNum[2];
      }
    }

RetDec:
    COPYDEC(*pdecRes, decRes)
    return NOERROR;
}


STDAPI VarDecCmp(LPDECIMAL pdecL, LPDECIMAL pdecR)
{
    ULONG   ulSgnL;
    ULONG   ulSgnR;
    DECIMAL decRes;
  
    // First check signs and whether either are zero.  If both are
    // non-zero and of the same sign, just use subtraction to compare.
    // 
    ulSgnL = pdecL->Lo32 | pdecL->Mid32 | pdecL->Hi32;
    ulSgnR = pdecR->Lo32 | pdecR->Mid32 | pdecR->Hi32;
    if (ulSgnL != 0)
      ulSgnL = (pdecL->sign & DECIMAL_NEG) | 1;

    if (ulSgnR != 0)
      ulSgnR = (pdecR->sign & DECIMAL_NEG) | 1;

    // ulSgnL & ulSgnR have values 1, 0, or 0x81 depending on if the left/right
    // operand is +, 0, or -.
    //
    if (ulSgnL == ulSgnR) {
      if (ulSgnL == 0)      // both are zero
        return VARCMP_EQ; // return equal

      DecAddSub(pdecL, pdecR, &decRes, DECIMAL_NEG);
      if (decRes.Lo64 == 0 && decRes.Hi32 == 0)
        return VARCMP_EQ;
      if (decRes.sign & DECIMAL_NEG)
        return VARCMP_LT;

      return VARCMP_GT;
    }

    // Signs are different.  Used signed byte compares
    //
    if ((char)ulSgnL > (char)ulSgnR)
      return VARCMP_GT;
    return VARCMP_LT;
}


LONG
My_RegOpenKeyExA (
    HKEY hKey,
    LPCSTR lpSubKey,
    DWORD ulOptions,
    REGSAM samDesired,
    PHKEY phkResult
    )
{
    WCHAR wcDest[_MAX_PATH];
    WCHAR *pwcDest = wcDest;

    if(lpSubKey)
    {
    if(-1 == mbstowcs(wcDest, lpSubKey, _MAX_PATH))
        return ERROR_BAD_ARGUMENTS;
    }
    else
        pwcDest = NULL;

    return RegOpenKeyEx (
            hKey,
            pwcDest,
            ulOptions,
            samDesired,
            phkResult);
}



void  _ui64toa (     
        unsigned __int64 val,
        char *buf,
        unsigned int radix)
{

        char *p;                /* pointer to traverse string */
        char *firstdig;         /* pointer to first digit */
        char temp;              /* temp char */
        unsigned digval;        /* value of digit */
 
        p = buf; 
       
        firstdig = p;           /* save pointer to first digit */
        do {
            digval = (unsigned) (val % radix);
            val /= radix;       /* get next digit */

            /* convert to ascii and store */
            if (digval > 9)
                *p++ = (char) (digval - 10 + 'a');  /* a letter */
            else
                *p++ = (char) (digval + '0');       /* a digit */
        } while (val > 0);
 

        /* We now have the digit of the number in the buffer, but in reverse
           order.  Thus we reverse them now. */
        *p-- = '\0';            /* terminate string; p points to last digit */

        do {
            temp = *p;
            *p = *firstdig;
            *firstdig = temp;   /* swap *p and *firstdig */
            --p;
            ++firstdig;         /* advance to next two digits */
        } while (firstdig < p); /* repeat until halfway */
}




DummyMarshal::DummyMarshal(IUnknown *_punkCtrl) : punkCtrl(_punkCtrl), _refCount(1) 
{
}

HRESULT STDMETHODCALLTYPE 
DummyMarshal::QueryInterface(REFIID riid, void** ppv) 
{
   HRESULT hr = E_NOINTERFACE;
   ASSERT(FALSE);
   return hr;
}


ULONG STDMETHODCALLTYPE 
DummyMarshal::AddRef() 
{
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE 
DummyMarshal::Release() 
{   
    ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount); 
    
    if(!ret) 
        delete this; 
    return ret;
}
