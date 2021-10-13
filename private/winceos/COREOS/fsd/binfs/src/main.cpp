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
#include "binfs.h"


// BUGBUG: Todo list
// 1) Allow more than one volumes
// 2) Validate handles - Verify FSDMGR or HERE
// 3) Verify share mode with previously opened files
// 4) Verify the binfile (probably have an external binutil.dll just like fatutil).
// 5) Investigate exception handling


static CRITICAL_SECTION csMain;
LPSTR szChainDesc = "chain information";

BinVolume *g_pVolume = NULL;
DWORD g_dwChainRegionLength = 0;

BinVolume *FindVolume(HDSK hDsk)
{
    return g_pVolume;
}

void AddVolume(BinVolume *pVolume)
{
    g_pVolume = pVolume;
}

void DeleteVolume(BinVolume *pVolume)
{
    BinDirList *pTemp;
    g_pVolume = NULL;
    DeleteCriticalSection( &pVolume->csComp);
    if (pVolume->pReadBuf)
        delete [] pVolume->pReadBuf;
    while( pVolume->pDirectory) {
        pTemp = pVolume->pDirectory;
        pVolume->pDirectory = pVolume->pDirectory->pNext;
        if (pTemp->szFileName) {
            delete [] pTemp->szFileName;
        }
        if (pTemp->pe32) {
            delete pTemp->pe32;
        }
        if (pTemp->po32) {
            delete [] pTemp->po32;
        }
        delete pTemp;
    }
    delete pVolume;
}

BinVolume *CreateVolume(HDSK hDsk)
{
    BinVolume *pVolume = new BinVolume;
    if (pVolume) {
        pVolume->hDsk = hDsk;
        pVolume->pDirectory = NULL;
        pVolume->dwVolFlags = 0;
        pVolume->dwNumRegions = 0;
        InitializeCriticalSection(&pVolume->csComp);
        pVolume->pCurCompDir = NULL;
        pVolume->dwCurCompBlock = 0;
        pVolume->pReadBuf = NULL;
        pVolume->hVolume = NULL;
        pVolume->pChain = NULL;
        if(!FSDMGR_DiskIoControl(hDsk, IOCTL_DISK_GETINFO, &pVolume->diskInfo, sizeof(pVolume->diskInfo), &pVolume->diskInfo, sizeof(pVolume->diskInfo), NULL, NULL)) {
            FSDMGR_DiskIoControl(hDsk, DISK_IOCTL_GETINFO, &pVolume->diskInfo, sizeof(pVolume->diskInfo), &pVolume->diskInfo, sizeof(pVolume->diskInfo), NULL, NULL);
        }
    } 
    return pVolume;
}

BOOL InitBinDescriptors(BinVolume *pVolume)
{
    BOOL bRet = TRUE;
    ROMHDR *pToc = NULL;
    ROMPID *pRomPid = NULL;
    ChainData chain = {0};
    XIPCHAIN_SUMMARY *pChain = NULL;
    XIPCHAIN_SUMMARY *pChainHeader = NULL;
    DWORD dwChainSize = 0;
    DWORD dwRet = 0;
    pVolume->dwNumRegions = 0;
    if (KernelIoControl( IOCTL_HAL_GET_BIN_CHAIN, NULL, 0, &dwChainSize, sizeof(DWORD), &dwRet) && dwChainSize) {
        pChainHeader = (XIPCHAIN_SUMMARY *)new BYTE[dwChainSize];
        if (pChainHeader) {
            if (!KernelIoControl( IOCTL_HAL_GET_BIN_CHAIN, (LPBYTE)pChainHeader, dwChainSize, NULL, 0, &dwRet)) {
                delete [] pChainHeader;
                pChainHeader = NULL;
                dwChainSize = 0;
            }    
        } else {
            dwChainSize = 0;
        }    
    }
    pToc = (ROMHDR *)UserKInfo[KINX_PTOC]; 
    if (pChainHeader || pToc->pExtensions) {
        if (!pChainHeader) {
            pRomPid = (ROMPID *)pToc->pExtensions;
            ROMPID *pItem = (ROMPID *)pRomPid->pNextExt;
            while(pItem) {
                if ((memcmp( pItem->name, szChainDesc, strlen(szChainDesc)) == 0) && pItem->length) {
                    pChainHeader = (XIPCHAIN_SUMMARY *) pItem->pdata;
                    dwChainSize = pItem->length;
                    break;
                }
                pItem = (ROMPID *)pItem->pNextExt;
            }
        }    
        if (!dwChainSize || !pChainHeader) {
            return FALSE;
        }    
        pVolume->dwNumRegions = (dwChainSize / sizeof(XIPCHAIN_SUMMARY)) - 1; 
        g_dwChainRegionLength = pChainHeader->dwMaxLength;
        pChain = pChainHeader+1; // Exclude the chain header
        if (pVolume->dwNumRegions) {
            pVolume->pChain = new ChainData[pVolume->dwNumRegions];
            if (pVolume->pChain) {
                    for (DWORD i=0; i < pVolume->dwNumRegions; i++, pChain++) {
                        DEBUGMSG( ZONE_INIT, (L"BINFS: ChainInfo - Address=%08X Length=%ld Order=%ld Flags=%04X\r\n", pChain->pvAddr, pChain->dwMaxLength, pChain->usOrder, pChain->usFlags));
                        memcpy( &chain, pChain, sizeof(XIPCHAIN_SUMMARY));
                        if (chain.wOrder < pVolume->dwNumRegions) {
                            WORD wOrder = (WORD)(pVolume->dwNumRegions-1)-chain.wOrder;
                            memcpy( &pVolume->pChain[wOrder], &chain, sizeof(chain));
                            if (pToc->physfirst == chain.dwAddress) {
                                memcpy( &pVolume->pChain[wOrder].Toc, pToc, sizeof(ROMHDR));
                                pVolume->pChain[wOrder].dwType = CHAIN_TYPE_XIP;
                            } else {
                                pVolume->pChain[wOrder].dwType = CHAIN_TYPE_BIN;
                            }
                            pVolume->pChain[wOrder].pDirectory = NULL;
                            pVolume->pChain[wOrder].pDirectoryLast = NULL;
                            pVolume->pChain[wOrder].dwBinOffset = 0;
                        } else {
                            DEBUGCHK(0);
                        }    
                    }
            } else {
                bRet = FALSE;
            }
        }    
    } else {
        // No additional regions found.. Not a Multi XIP/BIN Image
        bRet = FALSE;
    }    
    if (pChainHeader && !pRomPid) {
        delete [] pChainHeader;
    }
    return TRUE;
}

BOOL ReadDirEntries(BinVolume *pVolume, DWORD dwCurRegion, DWORD dwRomHdrOffset)
{
    DWORD dwRead = 0;
    DWORD dwNumFiles = 0;
    DWORD dwNumModules = 0;
    BYTE *pBuffer = NULL;
    BOOL fRet = FALSE;
    BinDirList *pTemp = NULL;
    WCHAR szFileName[MAX_PATH];
    unsigned char szAscii[MAX_PATH];
    BinDirList *pDirectory = NULL;
    ROMHDR romhdr = {0};
    
    szFileName[0] = L'\0';
    szAscii[0] = '\0';

    if (!ReadAtOffset(pVolume, &romhdr, sizeof(ROMHDR), &dwRead, dwRomHdrOffset)) {
        DEBUGMSG( ZONE_INIT, (L"Could not read the romhdr at %08X in region %ld\r\n", dwRomHdrOffset, dwCurRegion));
        DEBUGCHK(0);
        return FALSE;
    }    
    if (romhdr.physfirst != pVolume->pChain[dwCurRegion].dwAddress) {
        DEBUGMSG( ZONE_INIT, (L"Address of chain and region did not match in region %ld, chain=%08X Region=%08X \r\n", dwCurRegion, pVolume->pChain[dwCurRegion].dwAddress, romhdr.physfirst));
        DEBUGCHK(0);
        return FALSE;
    }
    
    memcpy( &pVolume->pChain[dwCurRegion].Toc, &romhdr, sizeof(ROMHDR));
   
    dwNumModules = romhdr.nummods;
    dwNumFiles = romhdr.numfiles;

    PRINTROMHEADER(&romhdr);

    if (dwNumModules) {
        pBuffer = new BYTE[sizeof(TOCentry) * dwNumModules];
        if (pBuffer) {
            fRet = ReadAtOffset(pVolume, pBuffer, sizeof(TOCentry) * dwNumModules, &dwRead, dwRomHdrOffset + sizeof(ROMHDR));
            if (fRet ) {
                DWORD i, j;
                TOCentry* pte = (TOCentry*) pBuffer;
                for (i=0;i < dwNumModules; i++) {
                    fRet = ReadAtOffset( pVolume, (PBYTE) szAscii,  MAX_PATH, &dwRead, AdjustOffset(pVolume, dwCurRegion, (DWORD) pte[i].lpszFileName));
                    j = 0;
                    memset( szFileName, 0, MAX_PATH);
                    while( (j < MAX_PATH) && szAscii[j]) {
                        szFileName[j] = szAscii[j];
                        j++;
                    }   
                    pTemp = new BinDirList;
                    pTemp->dwRegion = dwCurRegion;
                    pTemp->dwAddress = AdjustOffset( pVolume, dwCurRegion, pte[i].ulLoadOffset);
                    pTemp->dwCompFileSize = 0;
                    pTemp->dwRealFileSize = pte[i].nFileSize;
                    pTemp->dwAttributes = pte[i].dwFileAttributes | FILE_ATTRIBUTE_INROM | FILE_ATTRIBUTE_ROMMODULE;
                    memcpy( &pTemp->ft, &(pte[i].ftTime), sizeof(FILETIME)); 
                    pTemp->szFileName = new WCHAR[wcslen(szFileName)+1];
                    if (pTemp->szFileName) {
                        wcscpy( pTemp->szFileName, szFileName);
                    }                    
                    pTemp->pNext = NULL;
                    pTemp->pNext = NULL;
                    pTemp->pe32 = new e32_rom;
                    if (pTemp->pe32) {
                        ReadAtOffset(pVolume, (PBYTE)pTemp->pe32, sizeof(e32_rom), &dwRead, AdjustOffset(pVolume, dwCurRegion, pte[i].ulE32Offset));
                        if (pTemp->pe32->e32_objcnt > MAX_O32_SECTIONS) {
                            DEBUGMSG(ZONE_ERROR || ZONE_INIT, (L"BINFS!ReadDirEntries: ERROR! module \"%s\" has %u o32 sections (max sections = %u)\r\n", pTemp->szFileName, pTemp->pe32->e32_objcnt, MAX_O32_SECTIONS));
                        }
                        pTemp->po32 = new o32_rom[pTemp->pe32->e32_objcnt];                    
                        if (pTemp->po32) {
                            ReadAtOffset( pVolume, (PBYTE)pTemp->po32, sizeof(o32_rom)*pTemp->pe32->e32_objcnt, &dwRead, AdjustOffset(pVolume, dwCurRegion, pte[i].ulO32Offset));
                            for (j = 0; j < pTemp->pe32->e32_objcnt; j++) {
                                pTemp->po32[j].o32_dataptr = AdjustOffset( pVolume, dwCurRegion, pTemp->po32[j].o32_dataptr);
                            }
                        }
                    }    
                    if (pDirectory) {
                        pDirectory->pNext = pTemp;
                    } else {
                        pVolume->pChain[dwCurRegion].pDirectory = pTemp;
                    }
                    pDirectory = pTemp;
                }
            }
            delete [] pBuffer;
        }    
    }

    if (dwNumFiles) {
        pBuffer = new BYTE[sizeof(FILESentry) * dwNumFiles];    
        if (pBuffer) {
            fRet = ReadAtOffset(pVolume, pBuffer, sizeof(FILESentry) * dwNumFiles, &dwRead, dwRomHdrOffset+ sizeof(ROMHDR) + (sizeof(TOCentry) * dwNumModules));
            if (fRet) {
                FILESentry* pfe = (FILESentry*) pBuffer;
                DWORD i, j;
                
                for (i=0;i < dwNumFiles; i++) {
                    fRet = ReadAtOffset(pVolume, (PBYTE) szAscii,  MAX_PATH, &dwRead, AdjustOffset( pVolume, dwCurRegion , (DWORD) pfe[i].lpszFileName));
                    j = 0;
                    memset( szFileName, 0, MAX_PATH);
                    while( (j < MAX_PATH) && szAscii[j]) {
                        szFileName[j] = szAscii[j];
                        j++;
                    }   
                    pTemp = new BinDirList;
                    if (pTemp) {
                        pTemp->dwRegion = dwCurRegion;
                        pTemp->dwAddress = AdjustOffset( pVolume, dwCurRegion, pfe[i].ulLoadOffset);
                        pTemp->dwCompFileSize = pfe[i].nCompFileSize;
                        pTemp->dwRealFileSize = pfe[i].nRealFileSize;
                        pTemp->dwAttributes = pfe[i].dwFileAttributes | FILE_ATTRIBUTE_INROM;
                        pTemp->pe32 = NULL;
                        pTemp->po32 = NULL;
                        memcpy( &pTemp->ft, &(pfe[i].ftTime), sizeof(FILETIME)); 
                        pTemp->szFileName = new WCHAR[wcslen(szFileName)+1];
                        if (pTemp->szFileName) {
                            wcscpy( pTemp->szFileName, szFileName);
                        }
                        pTemp->pNext = NULL;
                        if (pDirectory) {
                            pDirectory->pNext = pTemp;
                        } else {
                            pVolume->pChain[dwCurRegion].pDirectory = pTemp;
                        }
                        pDirectory = pTemp;
                    }
                }
            } else {
                DEBUGMSG( ZONE_INIT, (L"BINFS: InitVolume - Couldn't locate Files data\n"));
            }
            delete [] pBuffer;
        }    
    }    
    PRINTFILELISTING( pVolume->pChain[dwCurRegion].pDirectory);
    pVolume->pChain[dwCurRegion].pDirectoryLast = pDirectory;
    return TRUE;
}

BOOL FindRegion(BinVolume *pVolume, DWORD dwHdrStart, LPDWORD pdwRegion) 
{
    DWORD dwRegion = 0;
    for (dwRegion = 0; dwRegion < pVolume->dwNumRegions; dwRegion++) {
        if ((dwHdrStart >= pVolume->pChain[dwRegion].dwAddress) && (dwHdrStart <= (pVolume->pChain[dwRegion].dwAddress + pVolume->pChain[dwRegion].dwLength))) {
            *pdwRegion = dwRegion;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL InitVolume(BinVolume *pVolume)
{
    DWORD dwOffset = 0;
    DWORD dwSignature = 0;
    DWORD dwHdrStart  = 0;
    DWORD dwRead = 0;
    DWORD dwRomHdrOffset = 0;
    BinDirList *pDirectory = NULL;
    DWORD dwRegion = 0;
    DWORD dwCurRegion = 0;
    DWORD dwDllFirst = 0;
    DWORD cbDlls = 0;

#if DEBUG
    ROMHDR* pToc = (ROMHDR *)UserKInfo[KINX_PTOC];
    DEBUGMSG (ZONE_INIT, (L"BINFS:Existing VM for kernel Dlls, first = %8.8lx, last = %8.8lx\r\n",
        pToc->dllfirst << 16, pToc->dlllast << 16));    
    DEBUGMSG (ZONE_INIT, (L"BINFS:Existing VM for user Dlls, first = %8.8lx, last = %8.8lx\r\n",
        pToc->dllfirst & 0xFFFF0000, pToc->dlllast & 0xFFFF0000));    
#endif
    
    for (dwRegion = 0; dwRegion < pVolume->dwNumRegions; dwRegion++) {
skipchain:        
        if (!ReadAtOffset(pVolume, &dwSignature, sizeof(DWORD), &dwRead, ROM_SIGNATURE_OFFSET+dwOffset)) {
            break;
        }    
        if (dwSignature != ROM_SIGNATURE) {
            DEBUGMSG( ZONE_INIT, (TEXT("BINFS: InitVolume - Signature of ROM Header does not Match- Assuming Chain Region")));
            dwOffset += g_dwChainRegionLength;
            goto skipchain;
        } 
        ReadAtOffset(pVolume, &dwHdrStart, sizeof(DWORD), &dwRead, ROM_SIGNATURE_OFFSET+sizeof(DWORD)+dwOffset);
        if (!FindRegion( pVolume, dwHdrStart, &dwCurRegion)) {
            DEBUGCHK(0);
            return FALSE;
        }
        pVolume->pChain[dwCurRegion].dwBinOffset = dwOffset;
        dwRomHdrOffset = AdjustOffset( pVolume, dwCurRegion, dwHdrStart);
        DEBUGMSG( ZONE_INIT, (TEXT("BINFS: InitVolume - RomHdr starts at %08X(%08X)\r\n"), dwHdrStart, dwRomHdrOffset));
        if (pVolume->pChain[dwCurRegion].dwType == CHAIN_TYPE_BIN) {
            if (!ReadDirEntries(pVolume, dwCurRegion, dwRomHdrOffset)) {
                // TODO: Do we continue or do we stop ???
                DEBUGMSG( ZONE_INIT, (L"Failed reading directory entries for phys region %ld , logical region %ld\r\n", dwRegion, dwCurRegion));
                return FALSE;
            }    
        }
        dwOffset += pVolume->pChain[dwCurRegion].dwLength;
    }
        
    DEBUGMSG( ZONE_INIT, (TEXT("BINFS: Num Regions = %ld\r\n"), pVolume->dwNumRegions));
    for (dwRegion = 0; dwRegion < pVolume->dwNumRegions; dwRegion++) {

        if (pVolume->pChain[dwRegion].dwType == CHAIN_TYPE_BIN) {
            
            if (pDirectory = (BinDirList *)pVolume->pChain[dwRegion].pDirectoryLast) {
                pDirectory->pNext = pVolume->pDirectory;
            }
            
            if (pDirectory = (BinDirList *)pVolume->pChain[dwRegion].pDirectory) {
                pVolume->pDirectory = pDirectory;
            }

            // Adjust kernel module VM range to accomodate kernel Dlls in this region.
            // High-word of kernel Dll range is the low-word of dllfirst-dlllast range.
            dwDllFirst  = ((pVolume->pChain[dwRegion].Toc.dllfirst) << 16);
            cbDlls      = ((pVolume->pChain[dwRegion].Toc.dlllast ) << 16) - dwDllFirst;
            if (cbDlls) {
                DEBUGMSG (ZONE_INIT, (L"BINFS:Reserve additional VM for kernel Dlls, first = %8.8lx, last = %8.8lx\r\n",
                    dwDllFirst, dwDllFirst + cbDlls));
                if (!KernelLibIoControl((HANDLE)KMOD_CORE, IOCTL_KLIB_SETROMDLLBASE, (void*)dwDllFirst, cbDlls, NULL, 0, NULL)) {
                    NKDbgPrintfW( L"BINFS:IOCTL_KLIB_SETROMDLLBASE (first = %8.8lx, last = %8.8lx) failed; error=%u!!!\r\n", 
                        dwDllFirst, dwDllFirst + cbDlls, GetLastError ());
                    DEBUGCHK(0);
                    return FALSE;
                }
            }

            // Adjust user module VM range to accomodate user Dlls in this region.
            // High-word of user Dll range is the high-word of dllfirst-dlllast range.
            dwDllFirst  = ((pVolume->pChain[dwRegion].Toc.dllfirst) & 0xFFFF0000);
            cbDlls      = ((pVolume->pChain[dwRegion].Toc.dlllast ) & 0xFFFF0000) - dwDllFirst;
            if (cbDlls) {
                DEBUGMSG (ZONE_INIT, (L"BINFS:Reserve additional VM for user Dlls, first = %8.8lx, last = %8.8lx\r\n",
                    dwDllFirst, dwDllFirst + cbDlls));
                if (!KernelLibIoControl((HANDLE)KMOD_CORE, IOCTL_KLIB_SETROMDLLBASE, (void*)dwDllFirst, cbDlls, NULL, 0, NULL)) {
                    NKDbgPrintfW( L"BINFS:IOCTL_KLIB_SETROMDLLBASE (first = %8.8lx, last = %8.8lx) failed; error=%u!!!\r\n", 
                        dwDllFirst, dwDllFirst + cbDlls, GetLastError ());
                    DEBUGCHK(0);
                    return FALSE;
                }
            }
        }
    }
        
    PRINTFILELISTING( pVolume->pDirectory);
    return TRUE;
}


BOOL RegisterVolume(BinVolume *pVolume)
{
    TCHAR szFolderName[MAX_PATH];
    DWORD cbRet = 0;
    STORAGEDEVICEINFO sdi = {0};

    szFolderName[0] = _T('\0');

    if (!FSDMGR_GetRegistryString(pVolume->hDsk, L"Folder", szFolderName, MAX_PATH)) {
        wcscpy( szFolderName, DEFAULT_VOLUME_NAME);
    }    
    if ((UserKInfo[KINX_PAGESIZE] == pVolume->diskInfo.di_bytes_per_sect)
        && FSDMGR_DiskIoControl(pVolume->hDsk, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(sdi), NULL, 0, &cbRet, NULL)) {
        // if the sector size == the page size and the block device supports XIP, support XIP modules
        if (STORAGE_DEVICE_FLAG_XIP & sdi.dwDeviceFlags) {
            pVolume->dwVolFlags |= VOL_FLAG_XIP;
        }
    }
    pVolume->hVolume = ::FSDMGR_RegisterVolume(pVolume->hDsk, szFolderName, (PVOLUME)pVolume);
    if (pVolume->hVolume)
    {
        RETAILMSG(1, (L"BINFS: RegisterVolume - Mounted volume '\\%s'\n", szFolderName));
        return TRUE;
    }
    return FALSE;
}

extern "C" BOOL FSD_MountDisk(HDSK hDsk)
{
    BinVolume *pVolume = NULL;
    DWORD dwError = ERROR_SUCCESS;
    BOOL fSuccess = FALSE;
    
    DEBUGMSG(ZONE_API, (L"BINFS: FSD_UnmountDisk\n"));

    pVolume = CreateVolume(hDsk);
    if (pVolume) {
        if (InitBinDescriptors(pVolume) && InitVolume( pVolume)) {
            fSuccess = RegisterVolume( pVolume);
        }    
    } else {
        dwError = ERROR_OUTOFMEMORY;
    }
    if (fSuccess) {
        if (pVolume)
            AddVolume( pVolume);
    } else {
        if (ERROR_SUCCESS != dwError)
            SetLastError(dwError);
        if (pVolume)
            delete pVolume;
    }    
    return fSuccess;
}


/*  FSD_UnmountDisk - Deinitialization service called by FSDMGR.DLL
 *
 *  ENTRY
 *      hdsk == FSDMGR disk handle, or NULL to deinit all
 *      frozen volumes on *any* disk that no longer has any open
 *      files or dirty buffers.
 *
 *  EXIT
 *      0 if failure, or the number of devices that were successfully
 *      unmounted.
 */

extern "C" BOOL FSD_UnmountDisk(HDSK hDsk)
{
    DWORD dwError = ERROR_SUCCESS;
    BOOL fSuccess = FALSE;
    BinVolume *pVolume = FindVolume( hDsk);
    DEBUGMSG(ZONE_API, (L"BINFS: FSD_UnmountDisk\n"));
    if (pVolume) {
        FSDMGR_DeregisterVolume(pVolume->hVolume);
        DeleteVolume( pVolume);
    }    
    if (dwError) {
        SetLastError(dwError);
    }
    return fSuccess;
}



BOOL WINAPI DllMain(HANDLE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    switch(dwReason) {
        case DLL_PROCESS_ATTACH: {
            DEBUGREGISTER((HINSTANCE)hInstance);
            InitializeCriticalSection(&csMain);
            break;
        }
        case DLL_PROCESS_DETACH: {
            DeleteCriticalSection(&csMain);
            break;
        }
        default:
            break;
    }
    return TRUE;
}


