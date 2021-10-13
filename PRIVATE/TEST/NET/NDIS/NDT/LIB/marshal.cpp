//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDT_Request.h"
#include "ndterror.h"
#include "Marshal.h"

//------------------------------------------------------------------------------

typedef int NDIS_STATUS, *PNDIS_STATUS;
#define NDIS_STATUS_SUCCESS                  ((DWORD)0x00000000L)

//------------------------------------------------------------------------------

HRESULT MarshalItem(
   PVOID *ppvBuffer, DWORD *pcbBuffer, size_t dwSize, PVOID pv
)
{
   if (*pcbBuffer < dwSize) return NDT_STATUS_BUFFER_TOO_SHORT;
   memcpy(*ppvBuffer, pv, dwSize);
   *(PBYTE *)ppvBuffer += dwSize;
   *pcbBuffer -= dwSize;
   return S_OK;
}

//------------------------------------------------------------------------------

HRESULT UnmarshalItem(
   PVOID *ppvBuffer, DWORD *pcbBuffer, size_t dwSize, PVOID pv
)
{
   if (*pcbBuffer < dwSize) return NDT_STATUS_BUFFER_TOO_SHORT;
   memcpy(pv, *ppvBuffer, dwSize);
   *(PBYTE *)ppvBuffer += dwSize;
   *pcbBuffer -= dwSize;
   return S_OK;
}

//------------------------------------------------------------------------------
// Marshal NDIS_STRING - S

HRESULT MarshalString(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPWSTR sz
)
{
   HRESULT hr = S_OK;
   USHORT usSize = 0;

   if (sz != NULL) usSize = (lstrlen(sz) + 1) * sizeof(WCHAR);

   hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(USHORT), &usSize);
   if (FAILED(hr)) goto cleanUp;

   // If it is a zero string we are done
   if (usSize == 0) return S_OK;
   
   // String itself
   hr = MarshalItem(ppvBuffer, pcbBuffer, usSize, sz);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------
// Unmarshal NDIS_STRING - S

HRESULT UnmarshalString(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPWSTR *psz
)
{
   HRESULT hr = S_OK;
   USHORT usSize = 0;

   delete *psz;
   *psz = NULL;
   
   // Get size of string
   hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(USHORT), &usSize);
   if (FAILED(hr)) goto cleanUp;

   // If it is a zero string we are done
   if (usSize == 0) return S_OK;

   // Allocate memory from string
   *psz = new WCHAR[usSize];
   if (*psz == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   hr = UnmarshalItem(ppvBuffer, pcbBuffer, usSize, *psz);
   if (FAILED(hr)) goto cleanUp;
   
cleanUp:   
   return hr;
}

//------------------------------------------------------------------------------
// Marshal medium array - M

HRESULT MarshalMediumArray(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT uiSize, NDIS_MEDIUM aMedium[]
)
{
   HRESULT hr = S_OK;
   DWORD dwSize = 0;

   // If a pointer is NULL size is zero
   if (aMedium == NULL) uiSize = 0;
   
   // Get size of array
   hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), &uiSize);
   if (FAILED(hr)) goto cleanUp;

   // If size is zero we are done
   if (uiSize == 0) goto cleanUp;

   dwSize = uiSize * sizeof(NDIS_MEDIUM);
   hr = MarshalItem(ppvBuffer, pcbBuffer, dwSize, aMedium);
   if (FAILED(hr)) goto cleanUp;

cleanUp:      
   return hr;
}

//------------------------------------------------------------------------------
// Marshal medium array - M

HRESULT UnmarshalMediumArray(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT *puiSize, NDIS_MEDIUM *paMedium[]
)
{
   HRESULT hr = S_OK;
   DWORD dwSize = 0;

   // Free array to avoid possible memory leak
   delete *paMedium;
   *paMedium = NULL;

   // Get size of array
   hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), puiSize);
   if (FAILED(hr)) goto cleanUp;
   
   // If size is zero we are done
   if (*puiSize == 0) goto cleanUp;

   // Allocate array
   *paMedium = new NDIS_MEDIUM[*puiSize];
   if (*paMedium == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   // Get a medium array itself
   dwSize = *puiSize * sizeof(NDIS_MEDIUM);
   hr = UnmarshalItem(ppvBuffer, pcbBuffer, dwSize, *paMedium);
   if (FAILED(hr)) {
      delete *paMedium; 
      *paMedium = NULL;
      goto cleanUp;
   }

cleanUp:      
   return hr;
}

//------------------------------------------------------------------------------
// Marshal byte array - B

HRESULT MarshalByteArray(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT uiSize, BYTE aByte[]
)
{
   HRESULT hr = S_OK;
   DWORD dwSize = 0;
   
   // If a pointer is NULL size is zero
   if (aByte == NULL) uiSize = 0;

   // Get size of array
   hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), &uiSize);
   if (FAILED(hr)) goto cleanUp;

   // If size is zero we are done
   if (uiSize == 0) goto cleanUp;

   dwSize = uiSize * sizeof(BYTE);
   hr = MarshalItem(ppvBuffer, pcbBuffer, dwSize, aByte);
   if (FAILED(hr)) goto cleanUp;

cleanUp:      
   return hr;
}

//------------------------------------------------------------------------------
// Marshal byte array - B

NDIS_STATUS UnmarshalByteArray(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT *puiSize, BYTE *paByte[]
)
{
   HRESULT hr = S_OK;
   DWORD dwSize = 0;
   
   // Free array to avoid possible memory leak
   delete [] *paByte;
   *paByte = NULL;

   // Get size of array
   hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), puiSize);
   if (FAILED(hr)) goto cleanUp;
   
   // If size is zero we are done
   if (*puiSize == 0) goto cleanUp;

   // Allocate array
   *paByte = new BYTE[*puiSize];
   if (*paByte == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   // Get a medium array itself
   dwSize = *puiSize * sizeof(BYTE);
   hr = UnmarshalItem(ppvBuffer, pcbBuffer, dwSize, *paByte);
   if (FAILED(hr)) {
      delete *paByte; 
      *paByte = NULL;
      goto cleanUp;
   }

cleanUp:      
   return hr;
}

//------------------------------------------------------------------------------
// Marshal array of byte array - X

HRESULT MarshalByteArray2(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT uiSize, UINT uiItemSize, 
   BYTE* apByte[]
)
{
   HRESULT hr = S_OK;
   DWORD dwSize = 0;
   UINT ui = 0;
   
   // If a pointer is NULL size is zero
   if (apByte == NULL) uiSize = 0;

   // Get size of array
   hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), &uiSize);
   if (FAILED(hr)) goto cleanUp;

   // If size is zero we are done
   if (uiSize == 0) goto cleanUp;

   hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), &uiItemSize);
   if (FAILED(hr)) goto cleanUp;

   dwSize = uiItemSize * sizeof(BYTE);

   for (ui = 0; ui < uiSize; ui++) {
      hr = MarshalItem(ppvBuffer, pcbBuffer, dwSize, apByte[ui]);
      if (FAILED(hr)) goto cleanUp;
   }

cleanUp:      
   return hr;
}

//------------------------------------------------------------------------------
// Marshal byte array array - X

NDIS_STATUS UnmarshalByteArray2(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT *puiSize, UINT *puiItemSize, 
   BYTE **ppaByte[]
)
{
   HRESULT hr = S_OK;
   DWORD dwSize = 0;
   BYTE *pbItem = NULL;
   UINT ui = 0;
   
   // Free array to avoid possible memory leak (didn't work for all cases)
   delete *ppaByte;
   *ppaByte = NULL;

   // Get size of array
   hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), puiSize);
   if (FAILED(hr)) goto cleanUp;
   
   // If size is zero we are done
   if (*puiSize == 0) goto cleanUp;

   // Get size of item
   hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), puiItemSize);
   if (FAILED(hr)) goto cleanUp;

   *ppaByte = new PBYTE[*puiSize];
   if (*ppaByte == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }
   
   // Get item size
   dwSize = *puiItemSize * sizeof(BYTE);

   // Unmarshal items
   for (ui = 0; ui < *puiSize; ui++) {
      // Allocate array
      pbItem = new BYTE[dwSize];
      if (*pbItem == NULL) {
         delete *ppaByte; 
         *ppaByte = NULL;
         hr = E_OUTOFMEMORY;
         goto cleanUp;
      }
      // Save it to an array
      *ppaByte[ui] = pbItem;
      // Get a medium array itself
      hr = UnmarshalItem(ppvBuffer, pcbBuffer, dwSize, pbItem);
      if (FAILED(hr)) {
         while (ui > 0) delete *ppaByte[ui--];
         delete *ppaByte; 
         *ppaByte = NULL;
         goto cleanUp;
      }
   }

cleanUp:      
   return hr;
}

//------------------------------------------------------------------------------
// Marshal multistring

HRESULT MarshalMultiString(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPWSTR sz
)
{
   HRESULT hr = S_OK;
   USHORT usSize = 0;
   LPWSTR szWork = sz;

   if (sz != NULL) {
      while (*szWork != _T('\0') && *(szWork + 1) != _T('\0')) szWork++;
      usSize = szWork - sz + 2;
   }
   
   hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(USHORT), &usSize);
   if (FAILED(hr)) goto cleanUp;

   // String itself
   hr = MarshalItem(ppvBuffer, pcbBuffer, usSize, sz);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------
// Unmarshal multistring

HRESULT UnmarshalMultiString(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPWSTR *psz
)
{
   HRESULT hr = S_OK;
   USHORT usSize = 0;

   delete *psz;
   *psz = NULL;
   
   // Get size of string
   hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(USHORT), &usSize);
   if (FAILED(hr)) goto cleanUp;

   // If it is a zero string we are done
   if (usSize == 0) return S_OK;

   // Allocate memory from string
   *psz = new WCHAR[usSize];
   if (*psz == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   hr = UnmarshalItem(ppvBuffer, pcbBuffer, usSize, *psz);
   if (FAILED(hr)) goto cleanUp;
   
cleanUp:   
   return hr;
}

//------------------------------------------------------------------------------
// Marshal byte buffer - Y

HRESULT MarshalByteBuffer(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT uiSize, BYTE aByte[]
)
{
   HRESULT hr = S_OK;
   DWORD dwSize = 0;
   
   // If a pointer is NULL size is zero
   if (aByte == NULL) uiSize = 0;

   // Get size of array
   hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), &uiSize);
   if (FAILED(hr)) goto cleanUp;

   // If size is zero we are done
   if (uiSize == 0) goto cleanUp;

   dwSize = uiSize * sizeof(BYTE);
   hr = MarshalItem(ppvBuffer, pcbBuffer, dwSize, aByte);
   if (FAILED(hr)) goto cleanUp;

cleanUp:      
   return hr;
}

//------------------------------------------------------------------------------
// Unmarshal byte buffer - Y

HRESULT UnmarshalByteBuffer(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT *puiSize, BYTE aByte[]
)
{
   HRESULT hr = S_OK;
   UINT uiSize = *puiSize;
   DWORD dwSize = 0;
   
   // Get size of array
   hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), puiSize);
   if (FAILED(hr)) goto cleanUp;
   
   // If size is zero we are done
   if (*puiSize == 0) goto cleanUp;

   if (*puiSize > uiSize) {
      hr = NDT_STATUS_BUFFER_TOO_SHORT;
      goto cleanUp;
   }
   
   // Get a medium array itself
   dwSize = *puiSize * sizeof(BYTE);
   hr = UnmarshalItem(ppvBuffer, pcbBuffer, dwSize, aByte);
   if (FAILED(hr)) goto cleanUp;

cleanUp:      
   return hr;
}

//------------------------------------------------------------------------------

HRESULT MarshalParametersV(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, va_list pArgs
)
{
   LPCTSTR szPos = szFormat;
   HRESULT hr = S_OK;
   PVOID pv = NULL;
   LPWSTR szValue = NULL;
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   UINT ui, ui2;

   while (*szPos != _T('\0')) {
      switch (*szPos) {
      case NDT_MARSHAL_VOID_REF:
         pv = &va_arg(pArgs, PVOID);
         hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(PVOID), pv);
         break;
      case NDT_MARSHAL_NDIS_STATUS:
         hr = va_arg(pArgs, HRESULT);
         status = (hr == S_OK) ? NDIS_FROM_HRESULT(hr) : NDIS_STATUS_SUCCESS;
         hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(NDIS_STATUS), &status);
         break;
      case NDT_MARSHAL_UCHAR:
         pv = &va_arg(pArgs, UCHAR);
         hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UCHAR), pv);
         break;
      case NDT_MARSHAL_USHORT:
         pv = &va_arg(pArgs, USHORT);
         hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(USHORT), pv);
         break;
      case NDT_MARSHAL_UINT:
         pv = &va_arg(pArgs, UINT);
         hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), pv);
         break;
      case NDT_MARSHAL_DWORD:
         pv = &va_arg(pArgs, DWORD);
         hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(DWORD), pv);
         break;
      case NDT_MARSHAL_ULONG:
         pv = &va_arg(pArgs, ULONG);
         hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(ULONG), pv);
         break;
      case NDT_MARSHAL_NDIS_OID:
         pv = &va_arg(pArgs, NDIS_OID);
         hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(NDIS_STATUS), pv);
         break;
      case NDT_MARSHAL_NDIS_REQUEST_TYPE:
         pv = &va_arg(pArgs, NDIS_REQUEST_TYPE);
         hr = MarshalItem(ppvBuffer, pcbBuffer, sizeof(NDIS_REQUEST_TYPE), pv);
         break;
      case NDT_MARSHAL_STRING:
         szValue = va_arg(pArgs, LPWSTR);
         hr = MarshalString(ppvBuffer, pcbBuffer, szValue);
         break;
      case NDT_MARSHAL_MEDIUM_ARRAY:
         ui = va_arg(pArgs, UINT);
         pv = va_arg(pArgs, NDIS_MEDIUM *);
         hr = MarshalMediumArray(ppvBuffer, pcbBuffer, ui, (NDIS_MEDIUM*)pv);
         break;
      case NDT_MARSHAL_BYTE_ARRAY:
         ui = va_arg(pArgs, UINT);
         pv = va_arg(pArgs, BYTE *);
         hr = MarshalByteArray(ppvBuffer, pcbBuffer, ui, (BYTE *)pv);
         break;
      case NDT_MARSHAL_BYTE_ARRAY2:
         ui = va_arg(pArgs, UINT);
         ui2 = va_arg(pArgs, UINT);
         pv = va_arg(pArgs, BYTE **);
         hr = MarshalByteArray2(ppvBuffer, pcbBuffer, ui, ui2, (BYTE **)pv);
         break;
      case NDT_MARSHAL_MULTISTRING:
         szValue = va_arg(pArgs, LPWSTR);
         hr = MarshalMultiString(ppvBuffer, pcbBuffer, szValue);
         break;
      case NDT_MARSHAL_BYTE_BUFFER:
         ui = va_arg(pArgs, UINT);
         pv = va_arg(pArgs, BYTE *);
         hr = MarshalByteBuffer(ppvBuffer, pcbBuffer, ui, (BYTE *)pv);
         break;
      default:
         hr = E_FAIL;
         break;
      }
      if (FAILED(hr)) break;
      szPos++;
   }
   
   return hr;
}

//------------------------------------------------------------------------------

HRESULT UnmarshalParametersV(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, va_list pArgs
)
{
   LPCTSTR szPos = szFormat;
   HRESULT hr = NDIS_STATUS_SUCCESS;
   PVOID pv = NULL;
   HRESULT *phr = NULL;
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   LPWSTR *pszValue = NULL;
   UINT *puiSize = NULL;
   UINT *puiItemSize = NULL;
   NDIS_MEDIUM **paMedium = NULL;
   BYTE*   pByte = NULL;
   BYTE**  paByte = NULL;
   BYTE*** ppaByte = NULL;

   while (*szPos != _T('\0')) {
      switch (*szPos) {
      case NDT_MARSHAL_VOID_REF:
         pv = va_arg(pArgs, PVOID *);
         hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(PVOID), pv);
         break;
      case NDT_MARSHAL_NDIS_STATUS:
         phr = va_arg(pArgs, HRESULT *);
         hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(NDIS_STATUS), &status);
         *phr = status ? HRESULT_FROM_NDIS(status) : S_OK;
         break;
      case NDT_MARSHAL_UCHAR:
         pv = va_arg(pArgs, UCHAR *);
         hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UCHAR), pv);
         break;
      case NDT_MARSHAL_USHORT:
         pv = va_arg(pArgs, USHORT *);
         hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(USHORT), pv);
         break;
      case NDT_MARSHAL_UINT:
         pv = va_arg(pArgs, UINT *);
         hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), pv);
         break;
      case NDT_MARSHAL_DWORD:
         pv = va_arg(pArgs, DWORD *);
         hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(DWORD), pv);
         break;
      case NDT_MARSHAL_ULONG:
         pv = va_arg(pArgs, ULONG *);
         hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(ULONG), pv);
         break;
      case NDT_MARSHAL_NDIS_OID:
         pv = va_arg(pArgs, NDIS_OID *);
         hr = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(NDIS_OID), pv);
         break;
      case NDT_MARSHAL_NDIS_REQUEST_TYPE:
         pv = va_arg(pArgs, NDIS_REQUEST_TYPE *);
         hr = UnmarshalItem(
            ppvBuffer, pcbBuffer, sizeof(NDIS_REQUEST_TYPE), pv
         );
         break;
      case NDT_MARSHAL_STRING:
         pszValue = va_arg(pArgs, LPWSTR *);
         hr = UnmarshalString(ppvBuffer, pcbBuffer, pszValue);
         break;
      case NDT_MARSHAL_MEDIUM_ARRAY:
         puiSize = va_arg(pArgs, UINT *);
         paMedium = va_arg(pArgs, NDIS_MEDIUM **);
         hr = UnmarshalMediumArray(ppvBuffer, pcbBuffer, puiSize, paMedium);
         break;
      case NDT_MARSHAL_BYTE_ARRAY:
         puiSize = va_arg(pArgs, UINT *);
         paByte = va_arg(pArgs, BYTE **);
         hr = UnmarshalByteArray(ppvBuffer, pcbBuffer, puiSize, paByte);
         break;         
      case NDT_MARSHAL_BYTE_ARRAY2:
         puiSize = va_arg(pArgs, UINT *);
         puiItemSize = va_arg(pArgs, UINT *);
         ppaByte = va_arg(pArgs, BYTE ***);
         hr = UnmarshalByteArray2(
            ppvBuffer, pcbBuffer, puiSize, puiItemSize, ppaByte
         );
         break;         
      case NDT_MARSHAL_MULTISTRING:
         pszValue = va_arg(pArgs, LPWSTR *);
         hr = UnmarshalString(ppvBuffer, pcbBuffer, pszValue);
         break;
      case NDT_MARSHAL_BYTE_BUFFER:
         puiSize = va_arg(pArgs, UINT *);
         pByte = va_arg(pArgs, BYTE *);
         hr = UnmarshalByteBuffer(ppvBuffer, pcbBuffer, puiSize, pByte);
         break;
      default:
         hr = E_FAIL;
         break;
      }
      if (FAILED(hr)) break;
      szPos++;
   }
   
   return hr;
}

//------------------------------------------------------------------------------

HRESULT MarshalParameters(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, ...
)
{
   HRESULT hr = S_OK;
   va_list pArgs;

   va_start(pArgs, szFormat);
   hr = MarshalParametersV(ppvBuffer, pcbBuffer, szFormat, pArgs);
   va_end(pArgs);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT UnmarshalParameters(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, ...
)
{
   HRESULT hr = S_OK;
   va_list pArgs;

   va_start(pArgs, szFormat);
   hr = UnmarshalParametersV(ppvBuffer, pcbBuffer, szFormat, pArgs);
   va_end(pArgs);
   return hr;
}

//------------------------------------------------------------------------------
