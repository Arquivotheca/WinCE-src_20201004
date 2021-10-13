//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "Log.h"

//------------------------------------------------------------------------------

#define NDT_SIGNATURE_MEMORY_BLOCK  0x4b5a6978
#define NDT_SIGNATURE_MEMORY_TAIL   0x0f1e2d3c

#define NDT_MEMORY_TAG              'aTDN'

//------------------------------------------------------------------------------

NDIS_SPIN_LOCK g_spinLockMemory;
void *g_pvMemoryListHead = NULL;

//------------------------------------------------------------------------------

struct NDT_PRIVATE_MEMORY
{
   DWORD  dwSignature;                 // NDT_SIGN_MEMORY_BLOCK
   size_t nSize;                       // Size of allocated memory block
   NDT_PRIVATE_MEMORY *pLastMemPtr;    // ptr to last allocated block
   NDT_PRIVATE_MEMORY *pNextMemPtr;    // ptr to next allocated block
};

void DumpMemory(PVOID pv, DWORD dwSize);

//------------------------------------------------------------------------------

void InitializeMemory()
{
   NdisAllocateSpinLock(&g_spinLockMemory);
}

//------------------------------------------------------------------------------

void DeinitializeMemory()
{
   NdisAcquireSpinLock(&g_spinLockMemory);

   // There should be no allocated block
   if (g_pvMemoryListHead != NULL) {

      NDT_PRIVATE_MEMORY *pPrivate;

      pPrivate = (NDT_PRIVATE_MEMORY *)g_pvMemoryListHead;
      Log(NDT_ERR_MEMORY_LEAKS);

      while (pPrivate != NULL) {

         // Is this a valid memory block?
         if (pPrivate->dwSignature != NDT_SIGNATURE_MEMORY_BLOCK) {
            Log(NDT_ERR_MEMORY_SIGNATURE, pPrivate);
            NDT_NdisBreakPoint();
         }

         Log(NDT_ERR_MEMORY_BLOCK, pPrivate, pPrivate->nSize);
         DumpMemory(pPrivate, pPrivate->nSize);
         
         // Check tail signature
         if (
            *((PDWORD)((PUCHAR)pPrivate + pPrivate->nSize) - 1) != 
            NDT_SIGNATURE_MEMORY_TAIL
         ) {
            Log(NDT_ERR_MEMORY_TAIL); 
            NDT_NdisBreakPoint();
         }

         pPrivate = pPrivate->pNextMemPtr;
      }
   }
   NdisReleaseSpinLock(&g_spinLockMemory);
   NdisFreeSpinLock(&g_spinLockMemory);
}

//------------------------------------------------------------------------------

void *operator new(size_t nSize)
{
   NDIS_STATUS status;
   void *pvAddress = NULL;
   void *pvBaseAddress = NULL;
   NDT_PRIVATE_MEMORY *pPrivate = NULL;

   // Allocate additional header and trailer
   nSize += sizeof(NDT_PRIVATE_MEMORY) + sizeof(ULONG);

   // Round to dword 
   nSize = (nSize + 3) & 0xFFFFFFFC;

   // Retry allocate memory even if failed to go around a fault injection
   for (INT count = 10; count > 0; count--) {
      // For fault injection       
      if (count < 5) NdisStallExecution(10);
            
      status = NdisAllocateMemoryWithTag(
         &pvBaseAddress, nSize, NDT_MEMORY_TAG
      );
      if (status == NDIS_STATUS_SUCCESS) break;
   }

   // Log an error when we was unsuccesfull
   if (pvBaseAddress == NULL) {
      goto cleanUp;
   }

   // This ptr we return to allocated memory
   pvAddress = (PUCHAR)pvBaseAddress + sizeof(NDT_PRIVATE_MEMORY);

   // Zero the memory
   NdisZeroMemory(pvBaseAddress, nSize);

   // Set up our header and trailer info
   pPrivate = (NDT_PRIVATE_MEMORY *)pvBaseAddress;

   // Fill the memory block header info
   pPrivate->dwSignature = NDT_SIGNATURE_MEMORY_BLOCK;
   pPrivate->nSize = nSize;

   // Set up the trailer information
   *((PDWORD)((PUCHAR)pvBaseAddress + nSize) - 1) = NDT_SIGNATURE_MEMORY_TAIL;

   // Insert at head of linked list..
   NdisAcquireSpinLock(&g_spinLockMemory);
   pPrivate->pNextMemPtr = (NDT_PRIVATE_MEMORY *)g_pvMemoryListHead;
   g_pvMemoryListHead = pPrivate;
   pPrivate = pPrivate->pNextMemPtr;
   if (pPrivate != NULL) {
      pPrivate->pLastMemPtr = (NDT_PRIVATE_MEMORY *)g_pvMemoryListHead;
   }
   NdisReleaseSpinLock(&g_spinLockMemory);

cleanUp:
   return pvAddress;
}

//------------------------------------------------------------------------------

void operator delete(void *p)
{
   // If pointer is null return (for C++ this is correct behavior)
   if (p == NULL) return;

   // Get base address and helper pointer to private structure
   PVOID pvBaseAddress = (PVOID)((PUCHAR)p - sizeof(NDT_PRIVATE_MEMORY));
   NDT_PRIVATE_MEMORY *pPrivate = (NDT_PRIVATE_MEMORY *)pvBaseAddress;
   size_t nSize = 0;

   // Check if we get memory allocated by newEx
   if (pPrivate->dwSignature != NDT_SIGNATURE_MEMORY_BLOCK) {
      NDT_NdisBreakPoint();
      return;
   }

   // We will need it later
   nSize = pPrivate->nSize;

   // Check tail signature
   if (
      *((PDWORD)((PUCHAR)pvBaseAddress + nSize) - 1) != 
      NDT_SIGNATURE_MEMORY_TAIL
   ) {
      NDT_NdisBreakPoint();
      return;
   }

   // Remove it from the linked list..
   NdisAcquireSpinLock(&g_spinLockMemory);

   // Is it first block in list?
   if (pPrivate->pLastMemPtr == NULL) {
      g_pvMemoryListHead = pPrivate->pNextMemPtr;
   } else {
      pPrivate->pLastMemPtr->pNextMemPtr = pPrivate->pNextMemPtr;
   }

   // Fix ptr of next memory block if necessary
   if (pPrivate->pNextMemPtr != NULL) {
      pPrivate->pNextMemPtr->pLastMemPtr = pPrivate->pLastMemPtr;
   }
   NdisReleaseSpinLock(&g_spinLockMemory);

   // Zero memory and free
   NdisZeroMemory(pvBaseAddress, nSize);
   NdisFreeMemory(pvBaseAddress, nSize, 0);
}

//------------------------------------------------------------------------------

void DumpMemory(PVOID pv, DWORD dwSize)
{
   TCHAR szDump[3 * 16 + 1] = _T("");
   LPTSTR szWork = szDump;
   PBYTE pb = (PBYTE)pv;

   for (DWORD i = 0; i < dwSize; i++) {
      wsprintf(szWork, _T(" %02x"), pb[i]);
      szWork += 3;
      if ((i % 16) == 15) {
         Log(NDT_ERR_MEMORY_DUMP, i/16, szDump);
         szWork = szDump;
         szDump[0] = _T('\0');
      }
   }
   if (szDump[0] != _T('\0')) Log(NDT_ERR_MEMORY_DUMP, (i - 1)/16, szDump);
}

//------------------------------------------------------------------------------
