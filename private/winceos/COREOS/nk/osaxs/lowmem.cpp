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
//
//    lowmem.cpp - DDx low memory diagnosis
//


#include "osaxs_p.h"
#include "DwCeDump.h"
#include "diagnose.h"
#include "celog.h"


LPCWSTR c_wszPageType[] =
{ 
    L"PAGETYPE_FREE",
    L"PAGETYPE_RESERVED", 
    L"PAGETYPE_STACK", 
    L"PAGETYPE_UNKNOWN", 
    L"PAGETYPE_CODE_ROM", 
    L"PAGETYPE_CODE_RAM",  
    L"PAGETYPE_CODE_DLL_ROM", 
    L"PAGETYPE_CODE_DLL_RAM", 
    L"PAGETYPE_CODE_EXE_ROM", 
    L"PAGETYPE_CODE_EXE_RAM",
    L"PAGETYPE_WRITABLE", 
    L"PAGETYPE_RW_DLL",
    L"PAGETYPE_RW_EXE",
    L"PAGETYPE_PERIPHERAL",
    L"PAGETYPE_RO_ROM",
    L"PAGETYPE_RO_RAM",
    L"PAGETYPE_HEAP", 
    L"PAGETYPE_OBJSTORE", 
    L"PAGETYPE_PAGEPOOL", 
    L"PAGETYPE_DUPLICATE"
};


DWORD g_dwPfnRAMStart = 0;
DWORD g_dwPfnRAMEnd   = 0;



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static DWORD _GetPFN (PPROCESS pprc, LPCVOID pAddr)
{
    DWORD    dwPFN   = INVALID_PHYSICAL_ADDRESS;
    DWORD    dwEntry = GetPDEntry (pprc->ppdir, pAddr);
    PPAGETABLE pptbl;

    DEBUGCHK (!((DWORD) pAddr & VM_PAGE_OFST_MASK));

    if (IsSectionMapped (dwEntry)) 
    {
        dwPFN = PFNfromSectionEntry (dwEntry) + SectionOfst (pAddr);
        
    } 
    else 
    {
        pptbl = GetPageTable(pprc->ppdir, VA2PDIDX (pAddr));

        if (pptbl) 
        {
            dwEntry = pptbl->pte[VA2PT2ND(pAddr)];

            if (IsPageCommitted (dwEntry)) 
            {
                dwPFN = PFNfromEntry (dwEntry);
            }
        }
    }

    return dwPFN;
}



//------------------------------------------------------------------------------
//
// get physical page number of a virtual address
//
//------------------------------------------------------------------------------
DWORD ddx_GetPFN (LPCVOID pAddr)
{
    return _GetPFN (((DWORD) pAddr >= VM_SHARED_HEAP_BASE)? g_pprcNK : pVMProc, pAddr);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
IsPfnInRAM (
            DWORD dwPfn
            )
{
    return (dwPfn >= g_dwPfnRAMStart) && (dwPfn < g_dwPfnRAMEnd);
}



//------------------------------------------------------------------------------
// Function Name: IsAddrInProcExecutable
// 
// Purpose: 
//      Determine if virtual address within a process' executable space
//
//------------------------------------------------------------------------------
o32_lite * 
IsAddrInProcExecutable(
                       PPROCESS pProc, 
                       DWORD dwAddr
                       )
{
    o32_lite * o32 = NULL;

    if ((!pProc) || (!pProc->o32_ptr)) 
        return NULL;


    for (int i = 0; i < pProc->e32.e32_objcnt; i++)
    {
        o32 = &pProc->o32_ptr[i];

        if ((dwAddr >= o32->o32_realaddr)
            && (dwAddr < o32->o32_realaddr + o32->o32_vsize))
        {
            return o32;
        }
    }

    return NULL;
}



//------------------------------------------------------------------------------
// Function Name: IsAddrInModule
// 
// Purpose: 
//      Determine if virtual address within a module's space
//
//------------------------------------------------------------------------------
PMODULE 
IsAddrInModule(
               PPROCESS pProcess, 
               DWORD dwAddr
               )
{
    if (!pProcess)
        return NULL;

    PMODULELIST pmodlist = (PMODULELIST) pProcess->modList.pFwd;
    PMODULE pMod = NULL;
    
    while (pmodlist && pmodlist != (PMODULELIST) &pProcess->modList)
    {
        pMod = pmodlist->pMod;

        o32_lite * o32 = NULL;

        if ((!pMod) || (!pMod->o32_ptr)) 
            return NULL;

        for (int i = 0; i < pMod->e32.e32_objcnt; i++)
        {
            o32 = &pMod->o32_ptr[i];

            if ((dwAddr >= o32->o32_realaddr)
                && (dwAddr < o32->o32_realaddr + o32->o32_vsize))
            {
                return pMod;
            }
        }
    
        pmodlist = (PMODULELIST) pmodlist->link.pFwd;   
    }

    return NULL;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD  
GetPageData(
            DWORD dwEntry,
            DWORD dwStartAddr,
            DWORD dwPager,
            PPROCESS pProc
            )
{
    DWORD pfn      = 0;
    DWORD type     = PAGETYPE_UNKNOWN;
    DWORD dwExeEnd = pProc ? (DWORD) pProc->BasePtr + pProc->e32.e32_vsize : 0;

    if (    (dwStartAddr && (dwStartAddr  <  dwExeEnd))                          // EXE
        || ((dwStartAddr >= VM_KDLL_BASE) && (dwStartAddr < VM_OBJSTORE_BASE))   // k-mode DLL
        || ((dwStartAddr >= VM_DLL_BASE)  && (dwStartAddr < VM_RAM_MAP_BASE)))   // u-mode DLL
    {    
       dwPager = VM_PAGER_LOADER;
    }

    if (!dwEntry) 
    {
        type = PAGETYPE_FREE;
    } 
    else if (IsPageCommitted (dwEntry)) 
    {
        pfn = PFNfromEntry (dwEntry);

        if (IsPageWritable(dwEntry)) 
        {
            if (VM_PAGER_AUTO == dwPager) 
            {
                type = PAGETYPE_STACK;
            } 
            else if ((dwStartAddr >= VM_OBJSTORE_BASE) &&
                     (dwStartAddr <  (VM_NKVM_BASE - VM_BLOCK_SIZE))) 
            {
                type = PAGETYPE_OBJSTORE;
            } 
            else if (IsPfnInRAM (pfn)) 
            {
                type = PAGETYPE_WRITABLE;

                if (IsAddrInProcExecutable(pProc, dwStartAddr))
                {
                    type = PAGETYPE_RW_EXE;
                }
                else 
                {
                    PMODULE pMod = IsAddrInModule(pProc, dwStartAddr);

                    if (pMod)
                    {
                        type = PAGETYPE_RW_DLL;
                    }
                    // TODO: 
                    /*else
                    {
                        PRHEAP pHeap = IsAddrInHeap(dwStartAddr, pProc->dwId);

                        if (pHeap)
                        {
                            type = PAGETYPE_HEAP;
                        }
                    }*/
                }
            } 
            else 
            {
                type = PAGETYPE_PERIPHERAL;
            }
        } 
        else if (VM_PAGER_LOADER == dwPager)
        {
            BOOL fInRom = kdpIsROM(pProc, (LPVOID)dwStartAddr, 1);

            if (IsAddrInProcExecutable(pProc, dwStartAddr))
            {
                if (fInRom)
                {
                    type = PAGETYPE_CODE_EXE_ROM;
                }
                else
                {
                    type = PAGETYPE_CODE_EXE_RAM;
                }
            }
            else
            {
                PMODULE pMod = IsAddrInModule(pProc, dwStartAddr);

                if (pMod)
                {
                    if (fInRom)
                    {
                        type = PAGETYPE_CODE_DLL_ROM;
                    }
                    else
                    {
                        type = PAGETYPE_CODE_DLL_RAM;
                    }
                }
                else
                {
                    if (fInRom)
                    {
                        type = PAGETYPE_CODE_ROM;
                    }
                    else
                    {
                        type = PAGETYPE_CODE_RAM;
                    }
                }
            }
        } 
        else 
        {
            if (kdpIsROM(pProc, (LPVOID)dwStartAddr, 1)) 
            {
                 type = PAGETYPE_RO_ROM;
            } 
            else 
            {
                 type = PAGETYPE_RO_RAM;
            }
        }  
    } 
    else 
    {
        type = PAGETYPE_RESERVED;
    }

    return type;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
HRESULT
ReadPageTables (
                PPROCESS pProcess,
                DWORD idxStart,
                DWORD idxEnd,
                ULONG32 pageCount[]
                )
{
    HRESULT hRes = E_FAIL;
    UINT nBlocks  = VM_NUM_PT_ENTRIES / VM_PAGES_PER_BLOCK;
    UINT idx      = 0;
    DWORD dwPager = 0;
    DWORD dwEntry = 0;
    DWORD dwAddr  = 0;
    DWORD type    = PAGETYPE_UNKNOWN;

    DEBUGGERMSG(OXZONE_DIAGNOSE,(L"--lowmem!ReadPageTables: Enter\r\n"));

    KD_ASSERT(NULL != pProcess);
    if (NULL == pProcess)
    {
        hRes = E_INVALIDARG;
        goto Exit;
    }

    if (!OSAXSIsProcessValid(pProcess))
    {
        DEBUGGERMSG (OXZONE_ALERT, (L"--lowmem!ReadPageTables: Process 0x%08x not processed, bState = %d\r\n", pProcess->bState));
        hRes = S_OK;
        goto Exit;
    }

    if (!pProcess->ppdir)
    {
        DEBUGGERMSG (OXZONE_ALERT, (L"--lowmem!ReadPageTables: Process 0x%08x lacks a pagetable directory!\r\n", pProcess->bState));
        hRes = E_FAIL;
        goto Exit;
    }


    // Not a standard ++ on increment because ARM needs +=4 when traversing
    // the page directory entry array.
    for (DWORD ipd = idxStart; (ipd < idxEnd) && (ipd < VM_NUM_PD_ENTRIES); ipd = NextPDEntry(ipd))
    {
        DWORD const pde = pProcess->ppdir->pte[ipd];

        if (IsPDEntryValid(pde))
        {
            if (IsSectionMapped (pde))
            {
                // Large page mapped
                
                dwPager = GetPagerType(pde);
                dwAddr = (ipd << VM_SECTION_SHIFT);

                // Get page data
                type = GetPageData(pde, dwAddr, dwPager, pProcess);

                if (type < NUM_PAGETYPES)
                {
                    pageCount[type] += 1;
                }
                else
                {
                    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"lowmem!ReadPageTables:  Unrecognized page type 0x%08x\r\n", type));
                }
            }
            else
            {
                // Go to second level page table.
                PPAGETABLE ppt = GetPageTable (pProcess->ppdir, ipd);

                if (ppt)
                {
                    idx  = 0;
                    type = PAGETYPE_UNKNOWN;

                    for (UINT iBlock = 0; iBlock < nBlocks; iBlock++)
                    {
                        // Get pager type for block

                        dwPager = GetPagerType(ppt->pte[idx]);

                        for (UINT i = 0; i < VM_PAGES_PER_BLOCK; i++)
                        {
                            dwEntry = ppt->pte[idx];
                            dwAddr = (ipd << VM_SECTION_SHIFT) | (idx << VM_PAGE_SHIFT);

                            // Get page data
                            type = GetPageData(dwEntry, dwAddr, dwPager, pProcess);

                            if (type < NUM_PAGETYPES)
                            {
                                pageCount[type] += 1;
                            }
                            else
                            {
                                DEBUGGERMSG(OXZONE_DIAGNOSE, (L"lowmem!ReadPageTables:  Unrecognized page type 0x%08x\r\n", type));
                            }
                            
                            //dwAddr += dwSize;
                            idx++;
                        }
                    }
                }

            }
        }
    }

    hRes = S_OK;

Exit:
    
    DEBUGGERMSG(OXZONE_DIAGNOSE,(L"--lowmem!ReadPageTables: Leave, hRes=0x%08X\r\n", hRes));
    return hRes;
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void
InitMemInfo(
            void
            )
{
    MEMORYINFO *pMemoryInfo = (MEMORYINFO *)g_pKData->aInfo[KINX_MEMINFO];
    if (pMemoryInfo)
    {
        g_dwPfnRAMStart = ddx_GetPFN ((LPVOID) PAGEALIGN_DOWN ((DWORD)pMemoryInfo->pKData));
        g_dwPfnRAMEnd   = ddx_GetPFN ((LPVOID) PAGEALIGN_UP   ((DWORD)pMemoryInfo->pKEnd - VM_PAGE_SIZE)) + VM_PAGE_SIZE; // pKEnd points to after the RAM end
    }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL GenerateDiagnosis (CEDUMP_SYS_MEMINFO* pMemInfo, UINT cbSize)
{
    UINT  index = g_nDiagnoses;
    CEL_LOWMEM_DATA* pOomData = (CEL_LOWMEM_DATA*) DD_ExceptionState.exception->ExceptionInformation[0];

    BeginDDxLogging();

    if (g_nDiagnoses >= DDX_MAX_DIAGNOSES)
    {
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"DDxLowMem!GenerateDiagnosis:  Diagnosis limit exceeded\r\n"));
        return FALSE;
    }
    
    g_nDiagnoses++;

    g_ceDmpDiagnoses[index].Type       = Type_LowMemory;
    g_ceDmpDiagnoses[index].SubType    = 0;                     
    g_ceDmpDiagnoses[index].Scope      = Scope_System; 
    g_ceDmpDiagnoses[index].Depth      = Depth_Symptom;
    g_ceDmpDiagnoses[index].Severity   = Severity_Recoverable;
    g_ceDmpDiagnoses[index].Confidence = Confidence_Certain;


    ddxlog(L"\r\n");
    ddxlog(L"[Description]\r\n");
    ddxlog(L"\r\n");
    ddxlog(L"A severe low memory condition (OOM) was detected.\r\n");
    ddxlog(L"\r\n");
    
    if (pMemInfo && pOomData)
    {
        pMemInfo->pageFreeCount       = pOomData->pageFreeCount;
        pMemInfo->cpNeed              = pOomData->cpNeed;
        pMemInfo->cpLowThreshold      = pOomData->cpLowThreshold;
        pMemInfo->cpCriticalThreshold = pOomData->cpCriticalThreshold;
        pMemInfo->cpLowBlockSize      = pOomData->cpLowBlockSize;
        pMemInfo->cpCriticalBlockSize = pOomData->cpCriticalBlockSize;
    }

    g_ceDmpDiagnoses[index].pDataTemp = (ULONG32) pMemInfo;
    g_ceDmpDiagnoses[index].Description.DataSize = EndDDxLogging();
    g_ceDmpDiagnoses[index].Data.DataSize = cbSize;

    
    // Bucket params

    HRESULT hr = GetBucketParameters(&g_ceDmpDDxBucketParameters, pCurThread, pCurThread->pprcActv);

    return (SUCCEEDED(hr));
}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DDxResult_e
DiagnoseOOM (
             DWORD ExceptionCode
             )
{
    DDxResult_e res  = DDxResult_Positive;  // If we get called it is because the kernel signalled OOM.
                                            // The only other result is Error.
    HRESULT hRes     = E_FAIL;
    DWORD idxStart;
    DWORD idxEnd;

    InitMemInfo();

    UINT nProcs = GetNumProcesses();
    UINT cbSize = sizeof(CEDUMP_SYS_MEMINFO) + (nProcs * sizeof(CEDUMP_PROC_MEMINFO));

    CEDUMP_SYS_MEMINFO* pMemInfo = (CEDUMP_SYS_MEMINFO*) DDxAlloc(cbSize, LMEM_ZEROINIT);

    if (!pMemInfo)
    {
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"lowmem!DiagnoseOOM: Unable to alloc CEDUMP_PROC_MEMINFO, bailing ...\r\n"));
        return DDxResult_Error;
    }

    pMemInfo->cbSize = cbSize;
    pMemInfo->nProcs = nProcs;


    PDLIST ptrav   = (PDLIST)g_pprcNK;
    PPROCESS pProc = NULL;
    int idx = 0;

   

    while (ptrav)
    {
        pProc = (PPROCESS) ptrav;

        pMemInfo->ProcMemInfo[idx].cbSize = sizeof(CEDUMP_PROC_MEMINFO);
        pMemInfo->ProcMemInfo[idx].procId = pProc->dwId;
        pMemInfo->ProcMemInfo[idx].nPageTypes = NUM_PAGETYPES;


        // If the scanned process is a usermode process, then this loop only needs to
        // iterate over 00000000 to 7FFFFFFF.  Stop the scan at the start of the
        // kernel mode base.  This is to avoid the ability of ARMV6 to automagically
        // switch to the kernel pagetable entries for VA with the MSB set.
        //
        // Otherwise, just scan the entire page directory.
        if (pProc == g_pprcNK)
        {
            idxStart = VA2PDIDX (VM_SHARED_HEAP_BASE);
            idxEnd   = VA2PDIDX (VM_KMODE_BASE);
        
            hRes = ReadPageTables(pProc, idxStart, idxEnd, pMemInfo->ProcMemInfo[idx].pageCount);

            idxStart = VA2PDIDX (g_pKData->dwKVMStart);
            idxEnd   = VA2PDIDX (VM_CPU_SPECIFIC_BASE);

            hRes = ReadPageTables(pProc, idxStart, idxEnd, pMemInfo->ProcMemInfo[idx].pageCount);
        }
        else
        {
            idxStart = VA2PDIDX (0x00000000);
            idxEnd   = VA2PDIDX (VM_SHARED_HEAP_BASE);
        
            hRes = ReadPageTables(pProc, idxStart, idxEnd, pMemInfo->ProcMemInfo[idx].pageCount);
        }


        // List reuslts if debug zone enabled
        DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"Process %s\r\n", pProc->lpszProcName));

        for (int i = 0; i < NUM_PAGETYPES; i++)
        {
            DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"  %s:  %d\r\n", c_wszPageType[i], pMemInfo->ProcMemInfo[idx].pageCount[i]));
        }
        DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"\r\n"));



        ptrav = ptrav->pFwd;
        
        if (ptrav == (PDLIST)g_pprcNK)
            break;

        idx++;
    }

    // Get heap size info

    // TODO: We need to do this for all processes, but VM switch
    // is potentially problematic (see ).

    // For now, get NK heaps (which include the shared heap)

    int nHeaps     = CountHeaps(g_pprcNK);
    int index      = 0;
    int cbHeapList = nHeaps * sizeof(CEDUMP_HEAP_INFO);

    CEDUMP_HEAP_INFO* pHeapInfo = (CEDUMP_HEAP_INFO*) DDxAlloc(cbHeapList, LMEM_ZEROINIT);

    if (pHeapInfo)
    {
        GetProcessHeapInfo(g_pprcNK, pHeapInfo, &index);
        
        pMemInfo->nHeaps            = nHeaps;
        pMemInfo->HeapInfo.Rva      = (ULONG32) pHeapInfo;
        pMemInfo->HeapInfo.DataSize = cbHeapList;

        // TODO: For verification of dump file contents - remove
        DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"\r\n"));
        DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"Heap Data:\r\n"));

        for (int j = 0; j < index; j++)
        {
            DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"\r\n"));
            DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"  pProcess:  0x%08x\r\n", pHeapInfo[j].pProcess));
            DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"  ProcId  :  0x%08x\r\n", pHeapInfo[j].ProcId));
            DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"  nItems  :  0x%08x\r\n", pHeapInfo[j].nItems));
            DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"  cbSize  :  %d\r\n",     pHeapInfo[j].cbSize));
            DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"  cbVaSize:  %d\r\n",     pHeapInfo[j].cbVaSize));
        }
        DEBUGGERMSG(OXZONE_DIAGNOSE, ( L"\r\n"));
    }
        
    
    // Add the diagnosis to the list

    GenerateDiagnosis(pMemInfo, pMemInfo->cbSize);


    return res;
}


