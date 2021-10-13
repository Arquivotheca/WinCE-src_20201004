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
#include "Marshal.h"

//------------------------------------------------------------------------------

NDIS_STATUS MarshalItem(
   PVOID *ppvBuffer, DWORD *pcbBuffer, size_t dwSize, PVOID pv
)
{
   if (*pcbBuffer < dwSize) return NDIS_STATUS_BUFFER_TOO_SHORT;
   NdisMoveMemory(*ppvBuffer, pv, dwSize);
   *(PBYTE *)ppvBuffer += dwSize;
   *pcbBuffer -= dwSize;
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

NDIS_STATUS UnmarshalItem(
   PVOID *ppvBuffer, DWORD *pcbBuffer, size_t dwSize, PVOID pv
)
{
   if (*pcbBuffer < dwSize) return NDIS_STATUS_BUFFER_TOO_SHORT;
   NdisMoveMemory(pv, *ppvBuffer, dwSize);
   *(PBYTE *)ppvBuffer += dwSize;
   *pcbBuffer -= dwSize;
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------
// Marshal NDIS_STRING - S

NDIS_STATUS MarshalString(
   PVOID *ppvBuffer, DWORD *pcbBuffer, NDIS_STRING ns
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   USHORT usSize = ns.Length + sizeof(WCHAR);
   WCHAR cNull = _T('\0');

   // Save length of string
   status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(USHORT), &usSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // String itself
   usSize -= sizeof(WCHAR);
   status = MarshalItem(ppvBuffer, pcbBuffer, usSize, ns.Buffer);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
   // Set L'\0' @ end          
   status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(WCHAR), &cNull);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:
   return status;
}

//------------------------------------------------------------------------------
// Unmarshal NDIS_STRING - S

NDIS_STATUS UnmarshalString(
   PVOID *ppvBuffer, DWORD *pcbBuffer, NDIS_STRING *pns
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   USHORT usSize = 0;
   
   // Free string to avoid possible memory leak
   if (pns->Buffer != NULL) NdisFreeString(*pns);

   // Get size of string
   status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(USHORT), &usSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
   // If it is a zero string we are done
   if (usSize == 0) goto cleanUp;

   // Is there string in buffer?
   if (*pcbBuffer < usSize) {
      status = NDIS_STATUS_BUFFER_TOO_SHORT;
      goto cleanUp;
   }
   
   // String should be zero ended
   if (*(PWCHAR)(*(PBYTE *)ppvBuffer + usSize - sizeof(WCHAR)) != L'\0') {
      status = NDIS_STATUS_BUFFER_TOO_SHORT;
      goto cleanUp;
   }

   // Initialize string
   NDT_NdisAllocateUnicodeString(pns, *(PWCHAR *)ppvBuffer);

   // Move pointer in buffer
   *(PBYTE *)ppvBuffer += usSize;
   *pcbBuffer -= usSize;

cleanUp:
   return status;
}

//------------------------------------------------------------------------------
// Marshal medium array - M

NDIS_STATUS MarshalMediumArray(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT uiSize, NDIS_MEDIUM aMedium[]
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   DWORD dwSize = 0;
   
   // Get size of array
   status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), &uiSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // If size is zero we are done
   if (uiSize == 0) goto cleanUp;

   dwSize = uiSize * sizeof(NDIS_MEDIUM);
   status = MarshalItem(ppvBuffer, pcbBuffer, dwSize, aMedium);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:      
   return status;
}

//------------------------------------------------------------------------------
// Marshal medium array - M

NDIS_STATUS UnmarshalMediumArray(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT *puiSize, NDIS_MEDIUM *paMedium[]
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   DWORD dwSize = 0;
   
   // Free array to avoid possible memory leak
   delete [] *paMedium;
   *paMedium = NULL;

   // Get size of array
   status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), puiSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
   // If size is zero we are done
   if (*puiSize == 0) goto cleanUp;

   // Allocate array
   *paMedium = new NDIS_MEDIUM[*puiSize];
   if (*paMedium == NULL) {
      status = NDIS_STATUS_RESOURCES;
      goto cleanUp;
   }

   // Get a medium array itself
   dwSize = *puiSize * sizeof(NDIS_MEDIUM);
   status = UnmarshalItem(ppvBuffer, pcbBuffer, dwSize, *paMedium);
   if (status != NDIS_STATUS_SUCCESS) {
      delete *paMedium; 
      *paMedium = NULL;
      goto cleanUp;
   }

cleanUp:      
   return status;
}

//------------------------------------------------------------------------------
// Marshal byte array - B

NDIS_STATUS MarshalByteArray(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT uiSize, BYTE aByte[]
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   DWORD dwSize = 0;

   // If a pointer is NULL size is zero
   if (aByte == NULL) uiSize = 0;

   // Get size of array
   status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), &uiSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // If size is zero we are done
   if (uiSize == 0) goto cleanUp;

   dwSize = uiSize * sizeof(BYTE);
   status = MarshalItem(ppvBuffer, pcbBuffer, dwSize, aByte);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:      
   return status;
}

//------------------------------------------------------------------------------
// Marshal byte array - B

NDIS_STATUS UnmarshalByteArray(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT *puiSize, BYTE *paByte[]
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   DWORD dwSize = 0;
   
   // Free array to avoid possible memory leak
   delete [] *paByte;
   *paByte = NULL;

   // Get size of array
   status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), puiSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
   // If size is zero we are done
   if (*puiSize == 0) goto cleanUp;

   // Allocate array
   *paByte = new BYTE[*puiSize];
   if (*paByte == NULL) {
      status = NDIS_STATUS_RESOURCES;
      goto cleanUp;
   }

   // Get a medium array itself
   dwSize = *puiSize * sizeof(BYTE);
   status = UnmarshalItem(ppvBuffer, pcbBuffer, dwSize, *paByte);
   if (status != NDIS_STATUS_SUCCESS) {
      delete *paByte; 
      *paByte = NULL;
      goto cleanUp;
   }

cleanUp:      
   return status;
}

//------------------------------------------------------------------------------
// Marshal array of byte array - X

NDIS_STATUS MarshalByteArray2(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT uiSize, UINT uiItemSize, 
   BYTE aByte[]
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   DWORD dwSize = 0;
   
   // If a pointer is NULL size is zero
   if (aByte == NULL) uiSize = 0;

   // Get size of array
   status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), &uiSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // If size is zero we are done
   if (uiSize == 0) goto cleanUp;

   status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), &uiItemSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   dwSize = uiSize * uiItemSize * sizeof(BYTE);
   status = MarshalItem(ppvBuffer, pcbBuffer, dwSize, aByte);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:      
   return status;
}

//------------------------------------------------------------------------------
// Marshal byte array array - X

NDIS_STATUS UnmarshalByteArray2(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT *puiSize, UINT *puiItemSize, 
   BYTE *paByte[]
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   DWORD dwSize = 0;
   
   // Free array to avoid possible memory leak
   delete [] *paByte;
   *paByte = NULL;

   // Get size of array
   status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), puiSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
   // If size is zero we are done
   if (*puiSize == 0) goto cleanUp;

   // Get size of item
   status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), puiItemSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Get total size
   dwSize = *puiSize * *puiItemSize * sizeof(BYTE);

   // Allocate array
   *paByte = new BYTE[dwSize];
   if (*paByte == NULL) {
      status = NDIS_STATUS_RESOURCES;
      goto cleanUp;
   }

   // Get a medium array itself
   status = UnmarshalItem(ppvBuffer, pcbBuffer, dwSize, *paByte);
   if (status != NDIS_STATUS_SUCCESS) {
      delete *paByte; 
      *paByte = NULL;
      goto cleanUp;
   }

cleanUp:      
   return status;
}

//------------------------------------------------------------------------------
// Marshal byte buffer - Y

NDIS_STATUS MarshalByteBuffer(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT uiSize, BYTE aByte[]
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   DWORD dwSize = 0;

   // If a pointer is NULL size is zero
   if (aByte == NULL) uiSize = 0;

   // Get size of array
   status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), &uiSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // If size is zero we are done
   if (uiSize == 0) goto cleanUp;

   dwSize = uiSize * sizeof(BYTE);
   status = MarshalItem(ppvBuffer, pcbBuffer, dwSize, aByte);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:      
   return status;
}

//------------------------------------------------------------------------------
// Unmarshal byte buffer - Y

NDIS_STATUS UnmarshalByteBuffer(
   PVOID *ppvBuffer, DWORD *pcbBuffer, UINT *puiSize, BYTE aByte[]
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   UINT uiSize = *puiSize;
   DWORD dwSize = 0;
   
   // Get size of array
   status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), puiSize);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
   // If size is zero we are done
   if (*puiSize == 0) goto cleanUp;

   if (*puiSize > uiSize) {
      status = NDIS_STATUS_BUFFER_TOO_SHORT;
      goto cleanUp;
   }
   
   // Get a medium array itself
   dwSize = *puiSize * sizeof(BYTE);
   status = UnmarshalItem(ppvBuffer, pcbBuffer, dwSize, aByte);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:      
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS MarshalParameters(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, ...
)
{
   va_list pArgs;
   LPCTSTR szPos = szFormat;
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   PVOID pv = NULL;
   NDIS_STRING ns;
   UINT ui, ui2;

   va_start(pArgs, szFormat);

   while (*szPos != _T('\0')) {
      switch (*szPos) {
      case NDT_MARSHAL_VOID_REF:
         pv = &va_arg(pArgs, PVOID);
         status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(PVOID), pv);
         break;
      case NDT_MARSHAL_NDIS_STATUS:
         pv = &va_arg(pArgs, NDIS_STATUS);
         status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(NDIS_STATUS), pv);
         break;
      case NDT_MARSHAL_UCHAR:
         pv = &va_arg(pArgs, UCHAR);
         status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UCHAR), pv);
         break;
      case NDT_MARSHAL_USHORT:
         pv = &va_arg(pArgs, USHORT);
         status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(USHORT), pv);
         break;
      case NDT_MARSHAL_UINT:
         pv = &va_arg(pArgs, UINT);
         status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), pv);
         break;
      case NDT_MARSHAL_DWORD:
         pv = &va_arg(pArgs, DWORD);
         status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(DWORD), pv);
         break;
      case NDT_MARSHAL_ULONG:
         pv = &va_arg(pArgs, ULONG);
         status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(ULONG), pv);
         break;
      case NDT_MARSHAL_NDIS_OID:
         pv = &va_arg(pArgs, NDIS_OID);
         status = MarshalItem(ppvBuffer, pcbBuffer, sizeof(NDIS_OID), pv);
         break;
      case NDT_MARSHAL_NDIS_REQUEST_TYPE:
         pv = &va_arg(pArgs, NDIS_REQUEST_TYPE);
         status = MarshalItem(
            ppvBuffer, pcbBuffer, sizeof(NDIS_REQUEST_TYPE), pv
         );
         break;
      case NDT_MARSHAL_STRING:
         ns = va_arg(pArgs, NDIS_STRING);
         status = MarshalString(ppvBuffer, pcbBuffer, ns);
         break;
      case NDT_MARSHAL_MEDIUM_ARRAY:
         ui = va_arg(pArgs, UINT);
         pv = va_arg(pArgs, NDIS_MEDIUM *);
         status = MarshalMediumArray(
            ppvBuffer, pcbBuffer, ui, (NDIS_MEDIUM*)pv
         );
         break;
      case NDT_MARSHAL_BYTE_ARRAY:
         ui = va_arg(pArgs, UINT);
         pv = va_arg(pArgs, BYTE *);
         status = MarshalByteArray(ppvBuffer, pcbBuffer, ui, (BYTE *)pv);
         break;
      case NDT_MARSHAL_BYTE_ARRAY2:
         ui = va_arg(pArgs, UINT);
         ui2 = va_arg(pArgs, UINT);
         pv = va_arg(pArgs, BYTE *);
         status = MarshalByteArray2(ppvBuffer, pcbBuffer, ui, ui2, (BYTE *)pv);
         break;
      case NDT_MARSHAL_BYTE_BUFFER:
         ui = va_arg(pArgs, UINT);
         pv = va_arg(pArgs, BYTE *);
         status = MarshalByteBuffer(ppvBuffer, pcbBuffer, ui, (BYTE *)pv);
         break;
      default:
         status = NDIS_STATUS_FAILURE;
         break;
      }
      if (status != NDIS_STATUS_SUCCESS) break;
      szPos++;
   }
   
   va_end(pArgs);
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS UnmarshalParameters(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, ...
)
{
   va_list pArgs;
   LPCTSTR szPos = szFormat;
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   PVOID pv = NULL;
   NDIS_STRING *pns = NULL;
   UINT* puiSize = NULL;
   UINT* puiItemSize = NULL;
   NDIS_MEDIUM** paMedium = NULL;
   BYTE* pByte = NULL;
   BYTE** paByte = NULL;

   va_start(pArgs, szFormat);

   while (*szPos != _T('\0')) {
      switch (*szPos) {
      case NDT_MARSHAL_VOID_REF:
         pv = va_arg(pArgs, PVOID *);
         status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(PVOID), pv);
         break;
      case NDT_MARSHAL_NDIS_STATUS:
         pv = va_arg(pArgs, NDIS_STATUS *);
         status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(NDIS_STATUS), pv);
         break;
      case NDT_MARSHAL_UCHAR:
         pv = va_arg(pArgs, UCHAR *);
         status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UCHAR), pv);
         break;
      case NDT_MARSHAL_USHORT:
         pv = va_arg(pArgs, USHORT *);
         status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(USHORT), pv);
         break;
      case NDT_MARSHAL_UINT:
         pv = va_arg(pArgs, UINT *);
         status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(UINT), pv);
         break;
      case NDT_MARSHAL_DWORD:
         pv = va_arg(pArgs, DWORD *);
         status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(DWORD), pv);
         break;
      case NDT_MARSHAL_ULONG:
         pv = va_arg(pArgs, ULONG *);
         status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(ULONG), pv);
         break;
      case NDT_MARSHAL_NDIS_OID:
         pv = va_arg(pArgs, NDIS_OID *);
         status = UnmarshalItem(ppvBuffer, pcbBuffer, sizeof(NDIS_OID), pv);
         break;
      case NDT_MARSHAL_NDIS_REQUEST_TYPE:
         pv = va_arg(pArgs, NDIS_REQUEST_TYPE *);
         status = UnmarshalItem(
            ppvBuffer, pcbBuffer, sizeof(NDIS_REQUEST_TYPE), pv
         );
         break;
      case NDT_MARSHAL_STRING:
         pns = va_arg(pArgs, NDIS_STRING *);
         status = UnmarshalString(ppvBuffer, pcbBuffer, pns);
         break;
      case NDT_MARSHAL_MEDIUM_ARRAY:
         puiSize = va_arg(pArgs, UINT *);
         paMedium = va_arg(pArgs, NDIS_MEDIUM **);
         status = UnmarshalMediumArray(ppvBuffer, pcbBuffer, puiSize, paMedium);
         break;
      case NDT_MARSHAL_BYTE_ARRAY:
         puiSize = va_arg(pArgs, UINT *);
         paByte = va_arg(pArgs, BYTE **);
         status = UnmarshalByteArray(ppvBuffer, pcbBuffer, puiSize, paByte);
         break;         
      case NDT_MARSHAL_BYTE_ARRAY2:
         puiSize = va_arg(pArgs, UINT *);
         puiItemSize = va_arg(pArgs, UINT *);
         paByte = va_arg(pArgs, BYTE **);
         status = UnmarshalByteArray2(
            ppvBuffer, pcbBuffer, puiSize, puiItemSize, paByte
         );
         break;         
      case NDT_MARSHAL_BYTE_BUFFER:
         puiSize = va_arg(pArgs, UINT *);
         pByte = va_arg(pArgs, BYTE *);
         status = UnmarshalByteBuffer(ppvBuffer, pcbBuffer, puiSize, pByte);
         break;
      default:
         status = NDIS_STATUS_FAILURE;
         break;
      }
      if (status != NDIS_STATUS_SUCCESS) break;
      szPos++;
   }
   
   va_end(pArgs);
   return status;
}

//------------------------------------------------------------------------------
