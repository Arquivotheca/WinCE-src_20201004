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

#include "hdstub_p.h"
#include "hddatabreakpoint.h"

//TODO: We need to be informed of virtual memory changes.
//Is this too high an over head?
//We can't tell if the user changed the permission mask.
//e.g. if there is a data bp on a page, then user changes to read only, we don't know what to restore original page protection to.


//Globals
//Index 0-7 are for software data breakpoints.  Index 8-15 are for hardware data breakpoints.
DATA_BREAKPOINT_ENTRY g_DataBreakpointTable [MAX_DATA_BREAKPOINTS];
DATA_BREAKPOINT_PAGE g_SWDataBreakpointPages [MAX_SW_DATA_BREAKPOINTS];

DWORD g_dwNumSwDBP = 0;
DWORD g_dwNumHwDBP = 0;
DWORD g_dwMAXHwDBP = 0;


extern BOOL HdstubSetIoctl(DWORD dwIoControlCode, PKD_BPINFO pkdbpi);

void PrintDataBreakpoints();


void DataBreakpointsInit()
{
    KD_BPINFO kdBPI = {0};
    
    g_dwNumSwDBP = 0;
    g_dwNumHwDBP = 0;
    g_dwMAXHwDBP = 0;
    memset(g_DataBreakpointTable, 0, sizeof(g_DataBreakpointTable));
    memset(g_SWDataBreakpointPages, 0, sizeof(g_SWDataBreakpointPages));

    if(DD_IoControl(KD_IOCTL_INIT, &kdBPI, sizeof (KD_BPINFO)))
    {
        
        //retrive the number of hardware bp's on this platform
        if(DD_IoControl(KD_IOCTL_QUERY_DBP, &kdBPI, sizeof (KD_BPINFO)))
        {        
            g_dwMAXHwDBP = kdBPI.ulCount;
            if(g_dwMAXHwDBP > MAX_HW_DATA_BREAKPOINTS)
            {
                g_dwMAXHwDBP = MAX_HW_DATA_BREAKPOINTS;
            }
        }
    }
}

void DataBreakpointsDeInit()
{
    //TODO: disable all breakpoints!
}


//Software Data Breakpoint Implementation
HRESULT UpdatePagePermissions(PDATA_BREAKPOINT_PAGE pDBPage)
{
    DEBUGGERMSG(HDZONE_DBP, (L"++UpdatePagePermissions: pDBPage=0x%08x\r\n", pDBPage));

    HRESULT hr = E_FAIL;

    PTENTRY ptEntry = GetPDEntry (pDBPage->pVM->ppdir, pDBPage->dwAddress);
    PPAGETABLE pptbl;

    if(IsSectionMapped (ptEntry)) 
    {
        hr = KDBG_E_DBP_SECTION_MAPPED;
        goto exit;
    } 
    pptbl = GetPageTable (pDBPage->pVM->ppdir, VA2PDIDX (pDBPage->dwAddress));
    if(pptbl == NULL)
    {
        hr = KDBG_E_DBP_INVALID_ADDRESS;
        goto exit;
    }
    ptEntry = pptbl->pte[VA2PT2ND(pDBPage->dwAddress)];
    if(PFNfromEntry(pDBPage->PTEOriginal) != PFNfromEntry(ptEntry))
    {
        //This shouldn't happen.  if so, we missed a pageIn notification.
        hr = KDBG_E_DBP_PAGE_REMAPPED;
        goto exit;
    }

    //TODO: should we cache the last PTE and see if the user changed it?
    if(pDBPage->wReadRefCount)
    {
        ptEntry = ReadOnlyEntry(ptEntry); //read only
        ptEntry = ptEntry & ~PG_VALID_MASK; //no access
    }
    else if(pDBPage->wWriteRefCount)
    {
        ptEntry = ReadOnlyEntry(ptEntry); //read only
#ifdef x86
        //technically, we need to change the PD entry in order to
        //make this work correctly.  Or, we can just mark this as 
        //invalid since it's on a shared heap page and will never
        //be used in callstack/exception handling.
        if(IsInSharedHeap(pDBPage->dwAddress))
        {
            ptEntry &= ~(PG_USER_MASK | PG_VALID_MASK);
        }
#endif
    }
    else
    {
        ptEntry = pDBPage->PTEOriginal;  //restore original.
    }
        
    DEBUGGERMSG(HDZONE_DBP, (L"  origPTE: 0x%08x currentPTE: 0x%08x\r\n", pDBPage->PTEOriginal, pptbl->pte[VA2PT2ND(pDBPage->dwAddress)]));
    pptbl->pte[VA2PT2ND(pDBPage->dwAddress)] = ptEntry;
    DEBUGGERMSG(HDZONE_DBP, (L"  newPTE: 0x%08x\r\n", pptbl->pte[VA2PT2ND(pDBPage->dwAddress)]));
    NKCacheRangeFlush (0, 0, CACHE_SYNC_ALL);          // Clear caches and TLB.

    hr = S_OK;                

exit:
    if(FAILED(hr))
    {
        //We can't use the BP anymore.
        //TODO: permenantly disable BP.
    }
           
    DEBUGGERMSG(HDZONE_DBP, (L"--UpdatePagePermissions: hr=0x%08x\r\n", hr));
    return hr;
}


HRESULT AddDBPtoPage(PDATA_BREAKPOINT_PAGE pDBPage, BOOL fReadDBP, BOOL fWriteDBP)
{    
    if(fWriteDBP)
    {
        pDBPage->wWriteRefCount++;
    }
    if(fReadDBP)
    {
        pDBPage->wReadRefCount++;
    }
    return UpdatePagePermissions(pDBPage);    
}


HRESULT RemoveDBPfromPage(PDATA_BREAKPOINT_PAGE pDBPage, BOOL fReadDBP, BOOL fWriteDBP)
{        
    if(fWriteDBP)
    {
        pDBPage->wWriteRefCount--;
    }
    if(fReadDBP)
    {
        pDBPage->wReadRefCount--;
    }
    return UpdatePagePermissions(pDBPage);    
}


HRESULT FreeDataBreakpointPage(PDATA_BREAKPOINT_PAGE pDBPage)
{
    DEBUGGERMSG(HDZONE_DBP, (L"++FreeDataBreakpointPage: pDBPage=0x%08x\r\n", pDBPage));

    pDBPage->wRefCount--;
    if(pDBPage->wRefCount == 0)
    {
        if(pDBPage->wReadRefCount == 0 && pDBPage->wWriteRefCount == 0)
        {
            //This shouldn't be necessary. But, we do it for it safe measure.
            UpdatePagePermissions(pDBPage);
            pDBPage->pVM = 0;
            pDBPage->dwAddress = 0;
            pDBPage->PTEOriginal = 0;
        }
    }
    
    DEBUGGERMSG(HDZONE_DBP, (L"--FreeDataBreakpointPage\r\n"));
    return S_OK;
}


HRESULT AllocDataBreakpointPage(PPROCESS pVM, DWORD dwAddress, DATA_BREAKPOINT_PAGE **ppDBPage)
{
    DEBUGGERMSG(HDZONE_DBP, (L"++AllocDataBreakpointPage: pVM=0x%08x dwAddress=0x%08x\r\n", pVM, dwAddress));    

    DWORD i=0;

    //Setup return values.
    HRESULT hr = KDBG_E_DBP_RESOURCES;
    *ppDBPage = NULL;
   
    //Is page already allocated?
    for(i=0; i<MAX_SW_DATA_BREAKPOINTS; i++)
    {
        PDATA_BREAKPOINT_PAGE pDBPTemp = &g_SWDataBreakpointPages[i];
        if(pDBPTemp->wRefCount && 
            (pDBPTemp->pVM == pVM) && 
            (pDBPTemp->dwAddress == PAGEALIGN_DOWN(dwAddress)))
        {
            
            *ppDBPage = pDBPTemp;
            hr = S_OK;
            break;
        }
    }
    
    if(*ppDBPage == NULL)
    {
        for(i=0; i<MAX_SW_DATA_BREAKPOINTS; i++)
        {
            PDATA_BREAKPOINT_PAGE pDBPTemp = &g_SWDataBreakpointPages[i];            
            if(pDBPTemp->wRefCount == 0)
            {  
                DWORD    dwEntry = GetPDEntry (pVM->ppdir, dwAddress);
                PPAGETABLE pptbl = GetPageTable (pVM->ppdir, VA2PDIDX (dwAddress));

                if(IsSectionMapped (dwEntry)) 
                {
                    hr = KDBG_E_DBP_SECTION_MAPPED;
                    break;
                } 
                if(pptbl == NULL)
                {
                    hr = KDBG_E_DBP_INVALID_ADDRESS;
                    break;
                }

                dwEntry = pptbl->pte[VA2PT2ND(dwAddress)];
                if(!IsPageCommitted(dwEntry))
                {
                    hr = KDBG_E_DBP_INVALID_ADDRESS;
                    break;
                }
                
                if(!IsPageWritable(dwEntry))
                {
                    hr = KDBG_E_DBP_PAGE_NOT_WRITABLE;
                    break;
                }

                //We have a valid data page that we can set a data bp on.
                pDBPTemp->pVM = pVM;
                pDBPTemp->dwAddress = PAGEALIGN_DOWN(dwAddress);                
                pDBPTemp->PTEOriginal = dwEntry;
                pDBPTemp->wWriteRefCount = 0; 
                pDBPTemp->wReadRefCount = 0;
                *ppDBPage = pDBPTemp;
                hr = S_OK;
                break;

            }
        }
    }    
    
    if(*ppDBPage)
    {
        (*ppDBPage)->wRefCount++;
    }

    DEBUGGERMSG(HDZONE_DBP || FAILED(hr), (L"--AllocDataBreakpointPage: hr=0x%08x pDBPage=0x%08x\r\n", hr, *ppDBPage));    
    return hr;
}


//Data Breakpoints Implementation
//Internal interfaces used by hd.dll/kd.dll/osaxs.dll
HRESULT FindUnusedDataBp(DWORD *dwIdx, BOOL fHardware)
{
    HRESULT hr = KDBG_E_DBP_RESOURCES;  
    //software uses index 0 to 7.  hardware uses index 8 to (8+number of HW DBP supported)
    DWORD dwIdxStart = (fHardware ? MAX_SW_DATA_BREAKPOINTS : 0);
    DWORD dwIdxEnd = (fHardware ? (MAX_SW_DATA_BREAKPOINTS + g_dwMAXHwDBP) : MAX_SW_DATA_BREAKPOINTS);

    for(; dwIdxStart < dwIdxEnd; dwIdxStart++)
    {
        if(g_DataBreakpointTable[dwIdxStart].fInUse == 0)
        {
            *dwIdx = dwIdxStart;
            hr = S_OK;
            if(fHardware)
            {
                g_dwNumHwDBP++;
            }
            else
            {
                g_dwNumSwDBP++;
            }
            break;
        }        
    }

    if(FAILED(hr))
    {
        if(fHardware)        
        {
            if(g_dwNumHwDBP == 0)
            {
                hr = KDBP_E_DBP_HWDBP_UNSUPPORTED;
            }
            else
            {
                hr = KDBP_E_DBP_OUT_OF_HWDBP;
            }
        }
        else
        {
            hr = KDBP_E_DBP_OUT_OF_SWDBP;       
        }
    }
    
    return hr;
}


//internal
HRESULT EnableDataBreakpoint(DWORD dwHandle)
{
    DEBUGGERMSG(HDZONE_DBP, (L"++EnableDataBreakpoint: dwHandle=0x%08x\r\n", dwHandle));    
    
    HRESULT hr = E_FAIL;
    PDATA_BREAKPOINT_ENTRY pDBE = &g_DataBreakpointTable[HANDLE_TO_INDEX(dwHandle)];

    if(!pDBE->fEnabled)
    {
        if(pDBE->fHardware)
        {
            KD_BPINFO bpInfo;
            bpInfo.nVersion = 1;
            bpInfo.ulAddress = pDBE->dwAddress;
            bpInfo.ulFlags = KD_HBP_FLAG_WRITE | (pDBE->fReadDBP ? KD_HBP_FLAG_READ : 0);
            bpInfo.ulHandle = 0;
            if (DD_IoControl(KD_IOCTL_SET_DBP, &bpInfo, sizeof (KD_BPINFO)))
            {
                pDBE->dwCookie = bpInfo.ulHandle;
                pDBE->fEnabled = TRUE;                                   
                hr = S_OK;
            }
            else
            {
                hr = E_FAIL;
            }
        }
        else
        {           
            hr = AddDBPtoPage(pDBE->pDBPage, pDBE->fReadDBP, pDBE->fWriteDBP);
            if(FAILED(hr))
            {
                goto Exit;
            }
            pDBE->fEnabled = TRUE;                   
        }
    }
    else
    {
        hr = S_OK;
    }

Exit:
    DEBUGGERMSG(HDZONE_DBP || FAILED(hr), (L"--EnableDataBreakpoint: hr=0x%08x\r\n", hr));
    return hr;
}


//intenral
HRESULT DisableDataBreakpoint(DWORD dwHandle)
{
    DEBUGGERMSG(HDZONE_DBP, (L"++DisableDataBreakpoint: dwHandle=0x%08x\r\n", dwHandle));
    
    HRESULT hr = E_FAIL;
    PDATA_BREAKPOINT_ENTRY pDBE = &g_DataBreakpointTable[HANDLE_TO_INDEX(dwHandle)];

    if(pDBE->fEnabled)
    {        
        if(pDBE->fHardware)
        {
            KD_BPINFO bpInfo;
            bpInfo.nVersion = 1;
            bpInfo.ulHandle = pDBE->dwCookie;
            bpInfo.ulFlags = 0;
            bpInfo.ulAddress = 0;
            if (DD_IoControl (KD_IOCTL_CLEAR_DBP, &bpInfo, sizeof(KD_BPINFO)))
            {
                hr = S_OK;
            }
            else
            {            
                //TODO: what do we do on failure?
                hr = E_FAIL;
            }
            pDBE->fEnabled = FALSE;
        }
        else
        {
            pDBE->fEnabled = FALSE;
            hr = RemoveDBPfromPage(pDBE->pDBPage, pDBE->fReadDBP, pDBE->fWriteDBP);        
        }    
    }
    else
    {
        hr = S_OK;
    }

    DEBUGGERMSG(HDZONE_DBP || FAILED(hr), (L"--DisableDataBreakpoint: hr=0x%08x\r\n", hr));
    return hr;
}


//internal
BOOL IsAccessViolation(DWORD dwAddress, BOOL fWrite)
{
    BOOL fAccessViolation = FALSE;
    
    DWORD mode = (pCurThread->pcstkTop->dwPrcInfo & CST_MODE_FROM_USER)? USER_MODE : KERNEL_MODE;

    if((mode == USER_MODE) &&
        ((dwAddress > VM_KMODE_BASE) ||
        (IsInSharedHeap(dwAddress) && fWrite)))
    {
        //If user mode AND dwAddress > 0x80000000 - access violation.
        //If user mode AND InSharedHeap and write - access violation.
        //Real access violation.  Don't search for data bp.
        fAccessViolation = TRUE;
    }
    return fAccessViolation;
}


DWORD FindDataBreakpoint(PPROCESS pVM, DWORD dwAddress, BOOL fWrite)
{
    DEBUGGERMSG(HDZONE_DBP, (L"++FindDataBreakpoint: pVM=0x%08x dwAddress=0x%08x fWrite=0x%08x\r\n", pVM, dwAddress, fWrite));    

    DWORD dwHandle = INVALID_DBP_HANDLE;
    DWORD i = 0;

    if(IsAccessViolation(dwAddress, fWrite))
    {
        goto Exit;
    }

    for(i=0; i<MAX_DATA_BREAKPOINTS; i++)
    {       
        PDATA_BREAKPOINT_ENTRY pDBE = &g_DataBreakpointTable[i];
        if(pDBE->fInUse && pDBE->fEnabled)
        {
            if((pVM == pDBE->pVM) && 
                (pDBE->fHardware || ((fWrite == pDBE->fWriteDBP) || (!fWrite == pDBE->fReadDBP))) &&
                (dwAddress >= pDBE->dwAddress) &&
                (dwAddress < pDBE->dwAddress + pDBE->dwBytes))
            {
                dwHandle = INDEX_TO_HANDLE(i);
                break;
            }
        }
    }
    
Exit:
    DEBUGGERMSG(HDZONE_DBP, (L"--FindDataBreakpoint: dwHandle=0x%08x\r\n", dwHandle));
    return dwHandle;
}


DWORD FindSWDataBreakpointsOnPage(PPROCESS pVM, DWORD dwAddress, BOOL fWrite)
{
    DEBUGGERMSG(HDZONE_DBP, (L"++FindSWDataBreakpointsOnPage: pVM=0x%08x dwAddress=0x%08x\r\n", pVM, dwAddress));

    PDATA_BREAKPOINT_ENTRY pDBE = NULL;
    DWORD dwCount = 0;
    DWORD i = 0;

    if(IsAccessViolation(dwAddress, fWrite))
    {
        goto Exit;
    }

    for(i=0; i<MAX_SW_DATA_BREAKPOINTS; i++)
    {       
        pDBE = &g_DataBreakpointTable[i];
        if(pDBE->fInUse && !pDBE->fHardware)
        {
            if((pVM == pDBE->pVM) && 
                (PAGEALIGN_DOWN(dwAddress) == PAGEALIGN_DOWN(pDBE->dwAddress)))
            {           
                dwCount++;
            }
        }
    }
    
Exit:
    DEBUGGERMSG(HDZONE_DBP, (L"--FindSWDataBreakpointsOnPage: dwCount=0x%08x\r\n", dwCount));
    return dwCount;
}


DWORD FindHWDataBreakpointsOnPage(PPROCESS pVM, DWORD dwAddress, BOOL fWrite)
{
    DEBUGGERMSG(HDZONE_DBP, (L"++FindHWDataBreakpointsOnPage: pVM=0x%08x dwAddress=0x%08x\r\n", pVM, dwAddress));

    PDATA_BREAKPOINT_ENTRY pDBE = NULL;
    DWORD dwCount = 0;
    DWORD i = 0;

    if(IsAccessViolation(dwAddress, fWrite))
    {
        goto Exit;
    }

    for(i=MAX_SW_DATA_BREAKPOINTS; i<MAX_DATA_BREAKPOINTS; i++)
    {       
        pDBE = &g_DataBreakpointTable[i];
        if(pDBE->fInUse)
        {
            if(PAGEALIGN_DOWN(dwAddress) == PAGEALIGN_DOWN(pDBE->dwAddress))
            {           
                dwCount++;
            }
        }
    }

Exit:
    DEBUGGERMSG(HDZONE_DBP, (L"--FindHWDataBreakpointsOnPage: dwCount=0x%08x\r\n", dwCount));
    return dwCount;
}


//internal
DWORD EnableDataBreakpointsOnPage(PPROCESS pVM, DWORD dwAddress)
{
    DEBUGGERMSG(HDZONE_DBP, (L"++EnableDataBreakpointsOnPage: pVM=0x%08x dwAddress=0x%08x\r\n", pVM, dwAddress));

    PDATA_BREAKPOINT_ENTRY pDBE = NULL;
    DWORD dwCount = 0;
    DWORD i = 0;
    
    for(i=0; i<MAX_SW_DATA_BREAKPOINTS; i++)
    {       
        pDBE = &g_DataBreakpointTable[i];
        if((pDBE->fInUse) && (pDBE->pVM == pVM) && 
            (PAGEALIGN_DOWN(dwAddress) == PAGEALIGN_DOWN(pDBE->dwAddress)))
        {
            EnableDataBreakpoint(INDEX_TO_HANDLE(i));
        //TODO: deal with failure                    
            dwCount++;
        }        
    }

    for(i=MAX_SW_DATA_BREAKPOINTS; i<MAX_DATA_BREAKPOINTS; i++)
    {       
        pDBE = &g_DataBreakpointTable[i];
        if((pDBE->fInUse) && 
            (PAGEALIGN_DOWN(dwAddress) == PAGEALIGN_DOWN(pDBE->dwAddress)))
        {            
            EnableDataBreakpoint(INDEX_TO_HANDLE(i));
        //TODO: deal with failure                    
            dwCount++;
        }        
    }

    DEBUGGERMSG(HDZONE_DBP, (L"--EnableDataBreakpointsOnPage: dwCount=%d\r\n", dwCount));
    return dwCount;
}


DWORD DisableSWDataBreakpointsOnPage(PPROCESS pVM, DWORD dwAddress)
{    
    DEBUGGERMSG(HDZONE_DBP, (L"++DisableSWDataBreakpointsOnPage: pVM=0x%08x dwAddress=0x%08x\r\n", pVM, dwAddress));    

    PDATA_BREAKPOINT_ENTRY pDBE = NULL;
    DWORD dwCount = 0;
    DWORD i = 0;
    
    for(i=0; i<MAX_SW_DATA_BREAKPOINTS; i++)
    {       
        pDBE = &g_DataBreakpointTable[i];
        if(pDBE->fInUse)
        {
            if((pDBE->pVM == pVM) && 
                (PAGEALIGN_DOWN(dwAddress) == PAGEALIGN_DOWN(pDBE->dwAddress)))
            {           
                DisableDataBreakpoint(INDEX_TO_HANDLE(i));
                dwCount++;
            }
        }
    }

    DEBUGGERMSG(HDZONE_DBP, (L"--DisableSWDataBreakpointsOnPage: dwCount=%d\r\n", dwCount));
    return dwCount;
}


DWORD DisableHWDataBreakpointsOnPage(PPROCESS pVM, DWORD dwAddress)
{    
    DEBUGGERMSG(HDZONE_DBP, (L"++DisableHWDataBreakpointsOnPage: pVM=0x%08x dwAddress=0x%08x\r\n", pVM, dwAddress));    

    PDATA_BREAKPOINT_ENTRY pDBE = NULL;
    DWORD dwCount = 0;
    DWORD i = 0;

    for(i=MAX_SW_DATA_BREAKPOINTS; i<MAX_DATA_BREAKPOINTS; i++)
    {       
        pDBE = &g_DataBreakpointTable[i];
        if(pDBE->fInUse)
        {
            if(PAGEALIGN_DOWN(dwAddress) == PAGEALIGN_DOWN(pDBE->dwAddress))
            {           
                DisableDataBreakpoint(INDEX_TO_HANDLE(i));
                dwCount++;
            }
        }
    }

    DEBUGGERMSG(HDZONE_DBP, (L"--DisableHWDataBreakpointsOnPage: dwCount=%d\r\n", dwCount));
    return dwCount;
}


//Internal
DWORD DisableDataBreakpointsOnPage(PPROCESS pVM, DWORD dwAddress)
{    
    DEBUGGERMSG(HDZONE_DBP, (L"++DisableDataBreakpointsOnPage: pVM=0x%08x dwAddress=0x%08x\r\n", pVM, dwAddress));    

    DWORD dwCount = DisableSWDataBreakpointsOnPage(pVM, dwAddress);
    dwCount += DisableHWDataBreakpointsOnPage(pVM, dwAddress);

    DEBUGGERMSG(HDZONE_DBP, (L"--DisableDataBreakpointsOnPage: dwCount=%d\r\n", dwCount));
    return dwCount;
}


DWORD ToggleAllDataBreakpoints(BOOL fEnabled)
{
//    DEBUGGERMSG(HDZONE_DBP, (L"++ToggleAllDataBreakpoints: fEnabled=%d\r\n", fEnabled));

    PDATA_BREAKPOINT_ENTRY pDBE = NULL;
    DWORD dwCount = 0;
    DWORD i = 0;
    
    for(i=0; i<MAX_DATA_BREAKPOINTS; i++)
    {       
        pDBE = &g_DataBreakpointTable[i];
        if(pDBE->fInUse)
        {
            if(fEnabled)
            {
                EnableDataBreakpoint(INDEX_TO_HANDLE(i));
            }
            else
            {
                DisableDataBreakpoint(INDEX_TO_HANDLE(i));                
            }            
            dwCount++;
        }
    }

//    DEBUGGERMSG(HDZONE_DBP, (L"++ToggleAllDataBreakpoints: dwCount=%d\r\n", dwCount));
    return dwCount;
    
}


//internal
DWORD EnableAllDataBreakpoints()
{    
    return ToggleAllDataBreakpoints(TRUE);
}


//internal
DWORD DisableAllDataBreakpoints()
{    
    return ToggleAllDataBreakpoints(FALSE);
}


//External Interfaces (Used by PB's KdStub)
HRESULT AddDataBreakpoint(PPROCESS pVM, DWORD dwAddress, DWORD dwBytes, BOOL fHardware, BOOL fReadDBP, BOOL fWriteDBP, DWORD *dwBPHandle)
{
    DEBUGGERMSG(HDZONE_DBP, (L"++AddDataBreakpoint: pVM=0x%08x dwAddress=0x%08x ", pVM, dwAddress));
    DEBUGGERMSG(HDZONE_DBP, (L"dwBytes=0x%08x fHardware=%d freadDBP=%d fWriteDBP=%d\r\n", dwBytes, fHardware, fReadDBP, fWriteDBP));

    HRESULT hr = E_FAIL;
    DWORD dwIndex=0;
    PDATA_BREAKPOINT_ENTRY pDBE = NULL;
    DWORD dwHandle = INVALID_DBP_HANDLE;

    if((dwBytes == 0) || (dwBPHandle == NULL))
    {
        hr = E_INVALIDARG;
        goto Exit;
    }

    if(pVM == NULL)
    {
        hr = KDBG_E_DBP_INVALIDPROCESS;
        goto Exit;
    }

    if(PAGEALIGN_DOWN(dwAddress) != PAGEALIGN_DOWN(dwAddress + dwBytes - 1))
    {
        hr = KDBG_E_DBP_PAGEBOUNDARY;
        goto Exit;
    }    

    if(!fHardware && fReadDBP)
    {
        //We can't set R/W software data breakpoint on the same page as the TLS.
        PTHREAD pth = (PTHREAD) pVM->thrdList.pFwd;
        while(pth && (((PDLIST) pth) != &pVM->thrdList))
        {
            if((PAGEALIGN_DOWN((DWORD)pth->tlsNonSecure) == PAGEALIGN_DOWN(dwAddress)) ||
               (PAGEALIGN_DOWN((DWORD)pth->tlsSecure) == PAGEALIGN_DOWN(dwAddress)))
            {
                hr = KDBP_E_DBP_TLS;
                goto Exit;
            }      
            pth = (PTHREAD) pth->thLink.pFwd;
        }
    }

    if(!fHardware)
    {
        //Disallow software data breakpoints on the secure stack.

        PPROCESS pprc = g_pprcNK;
        do
        {
            PTHREAD pth = (PTHREAD) pprc->thrdList.pFwd;
            while(pth && (((PDLIST) pth) != &pprc->thrdList))
            {                
                DWORD stkBase = pth->tlsSecure[PRETLS_STACKBASE];                
                DWORD stkSize = pth->tlsSecure[PRETLS_STACKSIZE];
                DWORD stkUpperBound = stkBase + stkSize;

                if((dwAddress+dwBytes-1 >= stkBase) && (dwAddress <= stkUpperBound))
                {
                    hr = KDBP_E_DBP_SECURE_STACK;
                    goto Exit;            
                }

                pth = (PTHREAD) pth->thLink.pFwd;                
            }            
            pprc = (PPROCESS) pprc->prclist.pFwd;

        } while(pprc && (pprc != g_pprcNK));        
        
    }

    hr = FindUnusedDataBp(&dwIndex, fHardware);
    if(FAILED(hr))
    {
        goto Exit;
    }

    pDBE = &g_DataBreakpointTable[dwIndex];
    dwHandle = INDEX_TO_HANDLE(dwIndex);
    
    pDBE->pVM = pVM;
    pDBE->dwAddress = dwAddress;
    pDBE->dwBytes = dwBytes;
    pDBE->fHardware = (fHardware ? 1 : 0);
    pDBE->fReadDBP = (fReadDBP ? 1 : 0);
    pDBE->fWriteDBP = (fWriteDBP ? 1 : 0);
    pDBE->fInUse = TRUE;
    pDBE->fEnabled = FALSE;
    pDBE->pDBPage = 0;
    pDBE->fCommitted = FALSE;

    if(!fHardware)
    {
        hr = AllocDataBreakpointPage(pVM, dwAddress, &pDBE->pDBPage);
        if(FAILED(hr))
        {
            goto Error;
        }
    }
        
    hr = EnableDataBreakpoint(dwHandle);
    if(FAILED(hr))
    {
        goto Error;
    }

    //We want to leave the dbp in a disabled state since we are still interacting with the debugger.
    hr = DisableDataBreakpoint(dwHandle);
    if(FAILED(hr))
    {
        goto Error;
    }

    if(fHardware)
    {
        pDBE->fCommitted = FALSE;
    }

    *dwBPHandle = dwHandle;
    
Exit:
    
    DEBUGGERMSG(HDZONE_DBP || FAILED(hr), (L"--AddDataBreakpoint: hr=0x%08x dwBPHandle=0x%08x\r\n", hr, (dwHandle ? *dwBPHandle : 0)));
    return hr;

Error:
    if(dwBPHandle)
    {
        *dwBPHandle = (DWORD)hr;
    }

    if(dwHandle != INVALID_DBP_HANDLE)
    {
        DeleteDataBreakpoint(dwHandle);
    }
    goto Exit;
}


HRESULT DeleteDataBreakpoint(DWORD dwHandle)
{
    DEBUGGERMSG(HDZONE_DBP, (L"++DeleteDataBreakpoint: dwHandle=0x%08x\r\n", dwHandle));

    HRESULT hr = E_FAIL;
    PDATA_BREAKPOINT_ENTRY pDBE = NULL;
    DWORD dwIndex = HANDLE_TO_INDEX(dwHandle);

    if(dwHandle == INVALID_DBP_HANDLE)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }

    hr = DisableDataBreakpoint(dwHandle);
    if(FAILED(hr))
    {
        //TODO: What do we do on failure ... keep it lying around mark it????
        goto Exit;
    }

    pDBE = &g_DataBreakpointTable[dwIndex];    
    if(!pDBE->fHardware)
    {
        if(pDBE->pDBPage)
        {
            hr = FreeDataBreakpointPage(pDBE->pDBPage);
        }
    }

    if(pDBE->fHardware)
    {        
        g_dwNumHwDBP--;
        if(pDBE->fCommitted)
        {
            KD_BPINFO kdbpi = {0};
            kdbpi.ulHandle = pDBE->dwCookie;
            HdstubSetIoctl(KD_IOCTL_CLEAR_DBP, &kdbpi);
        }
    }
    else
    {
        g_dwNumSwDBP--;                
    }
    
    memset(pDBE, 0, sizeof(DATA_BREAKPOINT_ENTRY));

Exit:
    DEBUGGERMSG(HDZONE_DBP, (L"--DeleteDataBreakpoint: hr = 0x%08x\r\n", hr));
    return hr;
    
}



BOOL DataBreakpointTrap(EXCEPTION_RECORD *pExceptionRecord, CONTEXT *pContext,
        BOOL SecondChance, BOOL *pfIgnore)
{
    DEBUGGERMSG (HDZONE_DBP, (L"++DataBreakpointTrap\r\n"));

    BOOL fHandled = FALSE;

    if(DD_PushExceptionState)
    {
        DD_PushExceptionState(pExceptionRecord, pContext, SecondChance);
    }
    else
    {
        //this only happens during initialization.
        goto Exit;
    }
    
    // (1) Ignore 2nd chance exceptions
    if(SecondChance)
    {
        goto Exit;
    }
    
#ifdef x86
    //x86 hardware data breakpoints come in as single step.
    if(DD_ExceptionState.exception->ExceptionCode == STATUS_SINGLE_STEP)
    {
        KD_EXCEPTION_INFO bpei = {0};
        bpei.nVersion = 1;
        if (DD_IoControl(KD_IOCTL_MAP_EXCEPTION, &bpei, sizeof (PKD_EXCEPTION_INFO)))
        {
            //convert to access violation.
            DD_ExceptionState.exception->ExceptionCode = (DWORD)STATUS_ACCESS_VIOLATION;
            DD_ExceptionState.exception->NumberParameters = AV_EXCEPTION_PARAMETERS;
            DD_ExceptionState.exception->ExceptionInformation[AV_EXCEPTION_ACCESS] = 1;  //x86 only supports write (even though doc says it supports read/write)
            DD_ExceptionState.exception->ExceptionInformation[AV_EXCEPTION_ADDRESS] = bpei.ulAddress;
        }
    }
#endif    

    // (2) Only interested in access violations.
    if(DD_ExceptionState.exception->ExceptionCode != STATUS_ACCESS_VIOLATION)
    {
        goto Exit;
    }
        
    if(DD_ExceptionState.exception->NumberParameters >= AV_EXCEPTION_PARAMETERS)
    {        
        BOOL fWrite = DD_ExceptionState.exception->ExceptionInformation[AV_EXCEPTION_ACCESS];
        DWORD dwAddress = DD_ExceptionState.exception->ExceptionInformation[AV_EXCEPTION_ADDRESS];        
        PPROCESS pVM = (dwAddress > VM_SHARED_HEAP_BASE ? g_pprcNK : GetPCB()->pVMPrc);

        DWORD dwHandle = DD_FindDataBreakpoint(pVM, dwAddress, fWrite);
        if(dwHandle != INVALID_DBP_HANDLE)
        {                            
            DEBUGGERMSG (HDZONE_DBP, (L"  Hit DataBP Handle:%u\r\n", dwHandle));
            fHandled = FALSE;
            *pfIgnore = TRUE;
        }
        else if(FindSWDataBreakpointsOnPage(pVM, dwAddress, fWrite))
        {            
            //Need to disable sw dbp on page.
            DisableSWDataBreakpointsOnPage(pVM, dwAddress);
            DD_SingleStepDBP(TRUE);
            fHandled = TRUE;
            *pfIgnore = TRUE;
        }
        else if(FindHWDataBreakpointsOnPage(pVM, dwAddress, fWrite))
        {
            //Need to disable hw dbp on Page
            DisableHWDataBreakpointsOnPage(pVM, dwAddress);
            DD_SingleStepDBP(TRUE);
            fHandled = TRUE;
            *pfIgnore = TRUE;
        }
        else
        {
            //Real access violation.
        }

    }

Exit:

    if(DD_PopExceptionState)
    {
        DD_PopExceptionState();
    }
    DEBUGGERMSG (HDZONE_DBP, (L"--DataBreakpointTrap: %d\r\n", fHandled));
    return fHandled;

}


BOOL DataBreakpointVMPageOut(DWORD dwPageAddr, DWORD dwNumPages)
{
//    DEBUGGERMSG (HDZONE_DBP, (L"++DataBreakpointVMPageOut: 0x%08x 0x%08x\r\n", dwPageAddr, dwNumPages));
    
    //return value of FALSE means we want to pass up to the debugger.
    //retrun value of TRUE means the debugger is not interested.
    bool fRet = TRUE;
    PPROCESS pVM = (dwPageAddr > VM_SHARED_HEAP_BASE ? g_pprcNK : GetPCB()->pVMPrc);

    for(DWORD i=0; i<MAX_DATA_BREAKPOINTS; i++)
    {        
        PDATA_BREAKPOINT_ENTRY pDBE = &g_DataBreakpointTable[i];        
        if(pDBE->fInUse &&
            (pDBE->pVM == pVM) &&
            (PAGEALIGN_DOWN(pDBE->dwAddress) >= PAGEALIGN_DOWN(dwPageAddr)) &&
            (PAGEALIGN_DOWN(pDBE->dwAddress) < PAGEALIGN_DOWN(dwPageAddr) + dwNumPages * VM_PAGE_SIZE))
        {
            DEBUGGERMSG (HDZONE_DBP, (L"  DataBreakpointVMPageOut: Found 0x%08x::0x%08x\r\n", pDBE->pVM, pDBE->dwAddress));
            fRet = FALSE;
            break;
        }
    }
//    DEBUGGERMSG (HDZONE_DBP, (L"--DataBreakpointTrap: %d\r\n", fRet));
    return fRet;
}


//External - Called by Kernel to see if the address is a data breakpoint
BOOL HdstubIsDataBreakpoint(DWORD dwAddress, BOOL fWrite)
{
    PPROCESS pVM = (dwAddress > VM_SHARED_HEAP_BASE ? g_pprcNK : GetPCB()->pVMPrc);
    if(g_dwNumSwDBP && FindSWDataBreakpointsOnPage(pVM, dwAddress, fWrite))
    {
        return TRUE;

    }
    if(g_dwNumHwDBP && FindHWDataBreakpointsOnPage(pVM, dwAddress, fWrite))
    {
        return TRUE;
    }
    return FALSE;
}


void HdstubCommitHWDataBreakpoints()
{
//    DEBUGGERMSG (HDZONE_DBP, (L"++HdstubCommitHWDataBreakpoints\r\n"));

    PDATA_BREAKPOINT_ENTRY pDBE = NULL;
    
    for(DWORD i=MAX_SW_DATA_BREAKPOINTS; i<MAX_DATA_BREAKPOINTS; i++)
    {
        pDBE = &g_DataBreakpointTable[i];                    
        if(pDBE->fInUse && !pDBE->fCommitted)
        {
            KD_BPINFO kdbpi = {0};
            kdbpi.ulAddress = pDBE->dwAddress;
            kdbpi.ulFlags = KD_HBP_FLAG_WRITE | (pDBE->fReadDBP ? KD_HBP_FLAG_READ : 0);
            HdstubSetIoctl(KD_IOCTL_SET_DBP, &kdbpi);        
            pDBE->fCommitted = TRUE;
        }
    }
//    DEBUGGERMSG (HDZONE_DBP, (L"--HdstubCommitHWDataBreakpoints: committed:%u\r\n", dwCount));    
}

//DEBUG functions.
void PrintDataBreakpoints()
{
    DWORD i=0;
    for(i=0; i<MAX_DATA_BREAKPOINTS; i++)
    {
        PDATA_BREAKPOINT_ENTRY pDBE = &g_DataBreakpointTable[i];
        if(pDBE->fInUse)
        {
            DEBUGGERMSG (HDZONE_DBP, (L"--Handle:%u---\r\n", INDEX_TO_HANDLE(i)));
            DEBUGGERMSG (HDZONE_DBP, (L"pVM:0x%08x\r\n", pDBE->pVM));
            DEBUGGERMSG (HDZONE_DBP, (L"addr:0x%08x\r\n", pDBE->dwAddress));
            DEBUGGERMSG (HDZONE_DBP, (L"size:0x%08x\r\n", pDBE->dwBytes));
            DEBUGGERMSG (HDZONE_DBP, (L"cookie/pDBPage:0x%08x\r\n", pDBE->pDBPage));
            DEBUGGERMSG (HDZONE_DBP, (L"hardware:%d\r\n", pDBE->fHardware));
            DEBUGGERMSG (HDZONE_DBP, (L"readDBP:%d\r\n", pDBE->fReadDBP));
            DEBUGGERMSG (HDZONE_DBP, (L"writeDBP:%d\r\n", pDBE->fWriteDBP));
            DEBUGGERMSG (HDZONE_DBP, (L"enabled:%d\r\n", pDBE->fEnabled));
            DEBUGGERMSG (HDZONE_DBP, (L"inuse:%d\r\n", pDBE->fInUse));
        }
    }
}


