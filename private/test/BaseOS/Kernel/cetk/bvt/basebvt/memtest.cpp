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
#include <tchar.h>
#include "basebvt.h"
#include "memtest.h"

SYSTEM_INFO si = { 0 };
extern DWORD gdwVirtAllocSize;
extern DWORD gdwHeapSize;
extern DWORD gdwLocalAllocSize;

/*****************************************************************************
 *
 *    Description: 
 *
 *       Memory Test tests the allocation of memory using VirtualAlloc() and 
 *    LocalAlloc(). It allocates memory, perform read/write operations, 
 *    decommits the memory tries to write into it resulting in an exception,
 *    then frees the allocated memory.
 *
 *    To modify the test - 
 *    /v:num - num here will be multiplied with page size to allocate memory 
 *             using virtualAlloc().
 *    /l:num - num here will be multiplied with page size to allocate memory
 *             using LocalAlloc().
 *****************************************************************************/

TESTPROCAPI 
BaseMemoryTest(
               UINT uMsg, 
               TPPARAM /*tpParam*/, 
               LPFUNCTION_TABLE_ENTRY /*lpFTE*/
               ) 
{
    HANDLE hTest=NULL;
    LPBYTE lpMem=NULL;
    DWORD  dwSize=0;

    if (uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }
    GetSystemInfo( &si);

    
    hTest = StartTest( TEXT("Base Memory Test"));

    if (gdwVirtAllocSize<(1<<20) && si.dwPageSize<=(1<<12))
    {
        dwSize = gdwVirtAllocSize*si.dwPageSize;
    }
    else
    {
        LogDetail( TEXT("FAIL : Integer Overflow - try a smaller number of pages for Virtual Alloc"));
        FailTest(hTest);
        goto NXTST;
    }
    

    do {
        LogDetail(TEXT("Check if Virtual mem can be allocated"));
        CHECKTRUE( BaseVirtualAllocateMemory(dwSize, &lpMem));

        LogDetail(TEXT("Check if this allocated memory can be used for writing and reading"));
        CHECKTRUE( BaseVirtualUseMemory(dwSize, lpMem));

        LogDetail(TEXT("Decommit the buffer"));
        CHECKTRUE( YVirtualFree( lpMem, dwSize, MEM_DECOMMIT));

        LogDetail(TEXT("Now try writing to the buffer"));
        // Prevent exception from breaking into the debugger
        DISABLEFAULTS();
        __try 
        {
            memset( lpMem, 0, dwSize);
            LogDetail( TEXT("No Exception !!! - An Exception should have occured"));
            break;
        }
        __except( EXCEPTION_EXECUTE_HANDLER)
        {
            LogDetail( TEXT("Exception !!! - This is what we expected"));
        }
        ENABLEFAULTS();

        LogDetail( TEXT("Check if this memory can be freed"));
        CHECKTRUE( BaseVirtualFreeMemory( lpMem));
        goto NXTST;

    }while(FALSE);
    FailTest( hTest);
    BaseVirtualFreeMemory( lpMem);

NXTST:
    
    if (gdwLocalAllocSize < (1<<20) && si.dwPageSize <= (1<<12))
    {
       dwSize = gdwLocalAllocSize*si.dwPageSize;
    }
    else
    {
        LogDetail( TEXT("FAIL : Integer Overflow - Try Smaller number of Pages for Local Alloc"));
        FailTest(hTest);
        return TPR_FAIL;
    }
        
    do{
        LogDetail(TEXT("Check if local mem can be allocated"));
        CHECKTRUE( BaseLocalAllocateMemory(dwSize, &lpMem));

        LogDetail( TEXT("Check if local mem can be used for writing and reading"));
        CHECKTRUE( BaseLocalUseMemory(dwSize, lpMem));


        LogDetail(TEXT("Check if local mem  can be freed"));
        CHECKTRUE( BaseLocalFreeMemory( lpMem));
        goto DONE;

    }while(FALSE);

    FailTest( hTest);
    BaseLocalFreeMemory( lpMem);

DONE :
    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}




/*****************************************************************************
*
*   Name    : BaseVirtualAllocateMemory
*
*   Purpose : Attempt to allocate the mem using the Virtual alloc API
*
*   Entry   : Variation number,size of the mem to be allocated, pointer
*             to get a handle to the allocated mem
*
*   Exit    : none
*
*   Calls   : VirtualAlloc()
*
*
*****************************************************************************/



BOOL BaseVirtualAllocateMemory( DWORD dwSize, LPBYTE *lpMem)
{
    *lpMem = (LPBYTE) YVirtualAlloc( NULL, dwSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    return ( *lpMem != NULL);
}



/*****************************************************************************
*
*   Name    : BaseVirtualUseMemory
*
*   Purpose : Attempt to write into and read from the allocated mem, to verify
*             that the allocated mem can be used.
*
*
*   Entry   : Variation number,size of the mem allocated, handle to the allocated
*             mem
*
*   Exit    : none
*
*   Calls   : VirtualLock() VirtualUnLock() DoWriteReadMem()
*
*
*****************************************************************************/


BOOL BaseVirtualUseMemory( DWORD dwSize, LPBYTE lpMem)
{
    BOOL bRc = FALSE;

    bRc = DoWriteReadMem(lpMem,dwSize,SIGNATURE_BYTE);

    return(bRc);
    
}


/*****************************************************************************
*
*   Name    : BaseVirtualFreeMemory
*
*   Purpose : Attempt to Free the allocated mem
*
*
*
*   Entry   : Variation number, handle to the allocated mem
*
*
*   Exit    : none
*
*   Calls   : VirtualFree()
*
*
*****************************************************************************/


BOOL BaseVirtualFreeMemory(LPBYTE lpMem)
{
    BOOL bRc = FALSE;

    bRc = YVirtualFree( lpMem, 0, MEM_RELEASE);

    return( bRc);
    
}


/*****************************************************************************
*
*   Name    : DoWriteReadMem
*
*   Purpose : Attempt to Write the signature byte into the block of allocated
*             mem and read back the contents to make sure that signature was
*             infact written. This is to verify the usability of the allocated
*             mem block.
*
*
*
*   Entry   : BaseAddress of mem block, size of mem block, signature byte
*
*
*   Exit    : TRUE if all is fine, FALSE otherwise
*
*   Calls   : none
*
*
*****************************************************************************/


BOOL DoWriteReadMem(LPBYTE lpMem,DWORD dwSize,BYTE sigByte)

{
    BOOL   bResult = FALSE;
    LPBYTE lpTmp = NULL;
    DWORD  dwTmp = 0;


// lets be optimistic..

    bResult = TRUE;

   // write signature byte
   dwTmp = dwSize;

   lpTmp = lpMem;


   __try  {
        while (dwTmp--)
         *lpTmp++ = sigByte;

        // verify signature bytes written correctly
        dwTmp = dwSize;
        lpTmp = lpMem;

        while (dwTmp--) {
           if (*lpTmp != sigByte)
            {
                LogDetail( TEXT("Signature byte incorrect at Offset=%lx\n"), dwSize - dwTmp);
                bResult = FALSE;
            }

           *lpTmp++ = ~sigByte;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER)  {
        LogDetail( TEXT("Exception occured accessing memory address %08X"), lpTmp);
        bResult = FALSE;
    }

    return bResult;
}


/*****************************************************************************
*
*   Name    : BaseLocalAllocateMemory
*
*   Purpose : Attempt to allocate the mem using the Local alloc API
*
*   Entry   : Variation number,size of the mem to be allocated, pointer
*             to get a handle to the allocated mem
*
*   Exit    : none
*
*   Calls   : LocalAlloc()
*
*
*****************************************************************************/




BOOL BaseLocalAllocateMemory(DWORD dwSize, LPBYTE *lpMem)
{

    DWORD dwFlags = 0;

    dwFlags = LMEM_ZEROINIT | LMEM_FIXED;

    *lpMem = (LPBYTE) YLocalAlloc( dwFlags, dwSize);

    return( *lpMem != NULL);
    
}

/*****************************************************************************
*
*   Name    : BaseLocalUseMemory
*
*   Purpose : Attempt to write into and read from the allocated mem, to verify
*             that the allocated mem can be used.
*
*
*   Entry   : Variation number,size of the mem allocated, handle to the allocated
*             mem
*
*   Exit    : none
*
*   Calls   : LocalLock() LocalUnLock() DoWriteReadMem()
*
*
*****************************************************************************/



BOOL BaseLocalUseMemory(DWORD dwSize, LPBYTE lpMem)
{
    BOOL bRc = 0;

    bRc = DoWriteReadMem( lpMem, dwSize,SIGNATURE_BYTE);

    return(bRc);
    

}


/*****************************************************************************
*
*   Name    : BaseLocalFreeMemory
*
*   Purpose : Attempt to Free the allocated mem
*
*
*
*   Entry   : Variation number, handle to the allocated mem
*
*
*   Exit    : none
*
*   Calls   : LocalFree()
*
*
*****************************************************************************/

BOOL BaseLocalFreeMemory( LPBYTE lpMem)
{

    HLOCAL hRc = NULL;

    hRc = YLocalFree( lpMem);

    return( hRc == NULL);
    
}

/*****************************************************************************
 *
 *    Description: 
 *
 *       A simple test to test the allocation of memory using HeapAlloc(). 
 *
 *    To modify the test -
 *    /p:numOfBytes - to be allocated using HeapAlloc()
 *
 *****************************************************************************/

TESTPROCAPI 
BaseHeapTest(
    UINT uMsg, 
    TPPARAM /*tpParam*/, 
    LPFUNCTION_TABLE_ENTRY /*lpFTE*/
    ) 
{
    HANDLE hTest;
    LPBYTE lpAddr=NULL;
    UINT j;

    if (uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }

    hTest = StartTest( TEXT("Allocating block with HEAP_ZERO_MEMORY"));

    do {
        lpAddr = (LPBYTE)YHeapAlloc( YGetProcessHeap(), HEAP_ZERO_MEMORY, gdwHeapSize);
        CHECKTRUE( lpAddr);
        for (j =0; j < gdwHeapSize; j++) {
            __try 
            {
                if (lpAddr[j]) {
                    LogDetail( TEXT("Failed in mem CHECKTRUE.... at address %08X"), lpAddr+j);
                    CHECKTRUE( YHeapFree( YGetProcessHeap(), 0, lpAddr));
                    break;
                }
            }
            __except( EXCEPTION_EXECUTE_HANDLER)
            {
                LogDetail( TEXT("Exception !!! - Should not have happend"));
                break;
            }
        }
        if (j!=gdwHeapSize)
        {
            LogDetail( TEXT("CHECKTRUE failed at line %ld\r\n"),__LINE__);
            break;
        }
        CHECKTRUE( YHeapFree( YGetProcessHeap(), 0, lpAddr));
        goto DONE;
    } while(FALSE);

    FailTest( hTest);
    HeapFree( YGetProcessHeap(), 0, lpAddr);

DONE:
    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}