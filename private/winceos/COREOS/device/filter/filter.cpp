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
/* File:    devInit.cpp
 *
 * Purpose: WinCE device manager initialization and built-in device management
 *
 */
#include <windows.h>
#include <tchar.h>
#include <devload.h>
#include <errorrep.h>
#include <devzones.h>

#include <filter.h>
#include <CRegEdit.h>
class FilterFile : public CRegistryEdit {
private:
    HMODULE         m_hFilter;
    pInitEntry      m_pFilterInitEntry;
    PDRIVER_FILTER  m_pFilterEntry;
    DWORD           m_dwLoadOrder;
    LPTSTR          m_lpActiveRegPath;
public:
    FilterFile * const m_pNextFilterFile;
    PDRIVER_FILTER  GetFilterEntry() { return m_pFilterEntry; };
    DWORD       GetLoadOrder() { return m_dwLoadOrder;};
    FilterFile *GetNext() { return m_pNextFilterFile; };
    FilterFile (LPCTSTR pszRegPath, FilterFile * m_pNext = NULL) 
    :   CRegistryEdit(HKEY_LOCAL_MACHINE,pszRegPath) 
    ,   m_pNextFilterFile(m_pNext)
    {
        m_hFilter = NULL;
        m_pFilterEntry = NULL;
        m_dwLoadOrder =0 ;
        m_pFilterEntry = NULL;
        m_lpActiveRegPath = NULL;
        m_pFilterInitEntry = NULL;
        if (pszRegPath) {
            __try {
                size_t cchKeyLen = 0;
                VERIFY(SUCCEEDED(StringCchLength(pszRegPath, MAX_PATH, &cchKeyLen)));
                ++cchKeyLen;
                m_lpActiveRegPath = new TCHAR [cchKeyLen];
                if ( m_lpActiveRegPath) {
                    VERIFY(SUCCEEDED(StringCchCopy(m_lpActiveRegPath, cchKeyLen, pszRegPath)));
                }
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                if (m_lpActiveRegPath) {
                    m_lpActiveRegPath[0] = NULL;
                }
            }
        }
    }
    ~FilterFile() {
        // In order to following the descruction order. 
        // the filter descruction has to be down by FIlter Folder. So don't check it.
        //if (m_pFilterEntry && m_pFilterEntry->fnFilterDeinit ) {
        //    m_pFilterEntry->fnFilterDeinit(m_pFilterEntry);
        //}
        if (m_lpActiveRegPath) {
            delete[] m_lpActiveRegPath;
        }
        if (m_hFilter) {
            FreeLibrary(m_hFilter);
        }
    }
    BOOL Init() {
        if  (IsKeyOpened() && m_pFilterInitEntry == NULL && m_hFilter==NULL) {
            // Get Entry Function and Order and Flags.
            DWORD dwFlags = 0;
            TCHAR dllName[DEVDLL_LEN];
            TCHAR entryName[DEVENTRY_LEN];            
            if (!GetRegValue( DEVLOAD_LOADORDER_VALNAME,(LPBYTE)&m_dwLoadOrder,sizeof(m_dwLoadOrder))) {
                m_dwLoadOrder = 0;
            }
            if (m_dwLoadOrder == MAXDWORD) { // MAXDWORD is terminate. So we can use it.
                m_dwLoadOrder--;
            }
            if (!GetRegValue( DEVLOAD_FLAGS_VALNAME,(LPBYTE)&dwFlags,sizeof(dwFlags))) {
                dwFlags = 0;
            }            
            if (GetRegValue( DRIVER_FILTER_INIT_ENTRY,(LPBYTE)entryName,sizeof(entryName)) &&
                    GetRegValue (DEVLOAD_DLLNAME_VALNAME,(LPBYTE)dllName,sizeof(dllName)) ) {
                m_hFilter = (dwFlags & DEVFLAGS_LOADLIBRARY) ? LoadLibrary(dllName) : LoadDriver(dllName);
                DEBUGMSG((ZONE_WARNING && (m_hFilter==NULL)), (_T("FilterFile:Init!couldn't load '%s' -- error %d\r\n"), 
                    dllName, GetLastError()));
                if (m_hFilter) {
                    m_pFilterInitEntry = (pInitEntry)GetProcAddress(m_hFilter, entryName);
                    DEBUGMSG((ZONE_WARNING && (m_pFilterInitEntry==NULL)), (_T("FilterFile:Init!couldn't get entry '%s' -- error %d\r\n"), 
                        entryName, GetLastError()));
                }
            }
            CloseKey() ;
        }
        ASSERT(m_pFilterInitEntry!=NULL);
        return (m_pFilterInitEntry!=NULL);
    };
    PDRIVER_FILTER IntializeFilter(PDRIVER_FILTER pNext, LPCTSTR lpDeviceRegPath) {
        ASSERT(m_pFilterEntry == NULL);
        if (m_pFilterEntry==NULL && m_pFilterInitEntry!=NULL) {
            m_pFilterEntry = m_pFilterInitEntry(m_lpActiveRegPath, lpDeviceRegPath, pNext);
            if (m_pFilterEntry!=NULL && !CheckEntry(pNext)) {
                DEBUGMSG(ZONE_ERROR, (_T("FilterFile:IntializeFilter!CheckEntry failed' filter unloaded\r\n")));
                if (m_pFilterEntry->fnFilterDeinit) {
                    m_pFilterEntry->fnFilterDeinit(m_pFilterEntry);
                }
                m_pFilterEntry = NULL;
            }
        };
        DEBUGMSG((ZONE_WARNING && (m_pFilterEntry ==NULL)), (_T("FilterFile:IntializeFilter!filterInit failed' -- error %d\r\n"), GetLastError()));
        return m_pFilterEntry;
    }
protected:
    BOOL CheckEntry(PDRIVER_FILTER pNext)
    {
        ASSERT(m_pFilterEntry!=NULL);
        BOOL fOk = TRUE;
        if (pNext) {
            if (fOk && pNext->fnInit!=NULL) {
                fOk = (m_pFilterEntry->fnInit!=NULL);
            }
            if (fOk && pNext->fnPreDeinit!=NULL) {
                fOk = (m_pFilterEntry->fnPreDeinit!=NULL);
            }            
            if (fOk && pNext->fnDeinit!=NULL) {
                fOk = (m_pFilterEntry->fnDeinit!=NULL);
            }
            if (fOk && pNext->fnOpen!=NULL) {
                fOk = (m_pFilterEntry->fnOpen!=NULL);
            }
            if (fOk && pNext->fnPreClose!=NULL) {
                fOk = (m_pFilterEntry->fnPreClose!=NULL);
            }
            if (fOk && pNext->fnClose!=NULL) {
                fOk = (m_pFilterEntry->fnClose!=NULL);
            }
            if (fOk && pNext->fnRead!=NULL) {
                fOk = (m_pFilterEntry->fnRead!=NULL);
            }
            if (fOk && pNext->fnWrite!=NULL) {
                fOk = (m_pFilterEntry->fnWrite!=NULL);
            }
            if (fOk && pNext->fnSeek!=NULL) {
                fOk = (m_pFilterEntry->fnSeek!=NULL);
            }
            if (fOk && pNext->fnControl!=NULL) {
                fOk = (m_pFilterEntry->fnControl!=NULL);
            }
            if (fOk && pNext->fnPowerup!=NULL) {
                fOk = (m_pFilterEntry->fnPowerup!=NULL);
            }
            if (fOk && pNext->fnPowerdn!=NULL) {
                fOk = (m_pFilterEntry->fnPowerdn!=NULL);
            }
            if (fOk && pNext->fnCancelIo!=NULL) {
                fOk = (m_pFilterEntry->fnCancelIo!=NULL);
            }
            
        };
        return fOk;
    }
    FilterFile operator=(FilterFile&rhs) 
    { 
        UNREFERENCED_PARAMETER(rhs);
        ASSERT(FALSE); 
    }
};
class FilterFolder  {
private:
    FilterFile * m_FilterFileList;
    PDRIVER_FILTER m_DriverFilterList;
public:
    FilterFolder() {
        m_FilterFileList = NULL;
        m_DriverFilterList = NULL;
    }
    BOOL   Init(LPCTSTR lpFilterRegPath) {
        BOOL fResult = FALSE;
        if (lpFilterRegPath) {
            TCHAR szFilterPath[DEVKEY_LEN];
            HRESULT hResult = StringCchCopy(szFilterPath,_countof(szFilterPath),DEFAULT_FILTER_REGISTRY_PATH);
            ASSERT(SUCCEEDED(hResult));
            hResult = StringCchCat(szFilterPath,_countof(szFilterPath),lpFilterRegPath);
            if (SUCCEEDED(hResult)) {
                CRegistryEdit *pfilterKey =new CRegistryEdit (HKEY_LOCAL_MACHINE, szFilterPath);
                if (pfilterKey && pfilterKey->IsKeyOpened()) {
                    DWORD NumSubKeys;
                    DWORD MaxSubKeyLen;
                    DWORD MaxClassLen;
                    DWORD NumValues;
                    DWORD MaxValueNameLen;
                    DWORD MaxValueLen;
                    // Get info on Template Key
                    BOOL bSuccess = pfilterKey->RegQueryInfoKey(
                                NULL,               // class name buffer (lpszClass)
                                NULL,               // ptr to length of class name buffer (lpcchClass)
                                NULL,               // reserved
                                &NumSubKeys,        // ptr to number of sub-keys (lpcSubKeys)
                                &MaxSubKeyLen,      // ptr to longest subkey name length (lpcchMaxSubKeyLen)
                                &MaxClassLen,       // ptr to longest class string length (lpcchMaxClassLen)
                                &NumValues,         // ptr to number of value entries (lpcValues)
                                &MaxValueNameLen,  // ptr to longest value name length (lpcchMaxValueNameLen)
                                &MaxValueLen,       // ptr to longest value data length (lpcbMaxValueData)
                                NULL,               // ptr to security descriptor length
                                NULL);              // ptr to last write time
                                    
                    if (!bSuccess) {
                        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("FilterFolder::Init RegQueryInfoKey returned fails.\r\n")));
                    } else for (DWORD Key = 0; Key < NumSubKeys; Key++) {
                        // Get TKey sub-key according to Key
                        WCHAR ValName[DEVKEY_LEN];
                        DWORD ValLen = _countof(ValName);
                        if (! pfilterKey->RegEnumKeyEx(  Key, ValName, &ValLen, NULL,  NULL,  NULL, NULL)){
                            DEBUGMSG(ZONE_INIT,(TEXT("FilterFolder::Init RegEnumKeyEx(%d) returned Error\r\n"), ValName));
                            break;
                        }
                        else {
                            TCHAR activePath[DEVKEY_LEN];
                            __try {
                                if (SUCCEEDED(StringCchCopy(activePath,_countof(activePath),szFilterPath)) &&
                                   SUCCEEDED(StringCchCat(activePath,_countof(activePath), TEXT("\\"))) &&
                                   SUCCEEDED(StringCchCat(activePath,_countof(activePath), ValName))){
                                     activePath[_countof(activePath)-1] = 0 ;
                                }
                                else {
                                    activePath[0] = 0;
                                }
                            }
                            __except(EXCEPTION_EXECUTE_HANDLER) {
                                activePath[0] = 0;
                            }
                            // Open sub-key under TKey
                            if (activePath[0]!=0) {
                                FilterFile * nDevice = new FilterFile(activePath, m_FilterFileList);
                                if (nDevice && nDevice->Init()) {
                                    m_FilterFileList = nDevice;
                                }
                                else if (nDevice) {
                                    delete nDevice;
                                }
                            }
                            fResult = TRUE; // at least success once.
                        }
                    }
                }
                if (pfilterKey)
                    delete pfilterKey;
            }
        }
        return fResult;
    }
    ~FilterFolder() {
        PDRIVER_FILTER pCurFilterList = m_DriverFilterList;
        while (pCurFilterList) {
            PDRIVER_FILTER pNextFilter = pCurFilterList->pNextFilter;
            // delete pCurFilterList
            if (pCurFilterList->fnFilterDeinit) {
                pCurFilterList->fnFilterDeinit(pCurFilterList);
            }
            pCurFilterList = pNextFilter;
        }
        while (m_FilterFileList) {
            FilterFile * pNext = m_FilterFileList->GetNext();
            delete m_FilterFileList;
            m_FilterFileList = pNext;
        }
    };
    DWORD 
    InitializeFilters(
        PDRIVER_FILTER pDriver,
        LPCTSTR lpDeviceRegPath,
        DRIVER_FILTER** ppDriverFilterList
        )
    {
        DWORD dwStatus = ERROR_SUCCESS;
        if (m_DriverFilterList == NULL && m_FilterFileList!=NULL) {
            // Activate Device 
            DWORD dwCurOrder = 0;
            PDRIVER_FILTER pPreviousFilter = pDriver;
            while (dwCurOrder != MAXDWORD) {
                DEBUGMSG(ZONE_INIT,(TEXT(" FilterFolder::InitializeFilters load device filter at order %d \r\n"),dwCurOrder));
                DWORD dwNextOrder = MAXDWORD;
                FilterFile * pCurDevice = m_FilterFileList;
                while (pCurDevice) {
                    DWORD dwDeviceLoadOrder = pCurDevice->GetLoadOrder();
                    if ( dwDeviceLoadOrder == dwCurOrder) {
                        PDRIVER_FILTER pNext = pCurDevice->IntializeFilter(pPreviousFilter,lpDeviceRegPath);
                        if (pNext) {
                            (PDRIVER_FILTER)(pNext->pNextFilter) = pPreviousFilter; // Cast to update it.
                            m_DriverFilterList = pNext;
                            pPreviousFilter = pNext;
                        }
                        else 
                        {
                            //filter driver init failed. fail to load the rest
                            //of the filter chain and driver
                            RETAILMSG(1,(
                                L"ERROR! Failed to load filter for device=%s "
                                L"at order=%d\r\n",
                                lpDeviceRegPath, dwCurOrder
                            ));
                            dwStatus = ERROR_OPEN_FAILED;
                            goto cleanUp;
                        }
                            
                    }
                    else  if (dwDeviceLoadOrder> dwCurOrder  && dwDeviceLoadOrder < dwNextOrder) {
                        dwNextOrder = dwDeviceLoadOrder;
                    }
                    pCurDevice = pCurDevice->m_pNextFilterFile;
                }
                dwCurOrder = dwNextOrder;
            }
            
        }

cleanUp:
        //always set ppDriverFilterList
        if (ppDriverFilterList)
            *ppDriverFilterList = m_DriverFilterList;
            
        return dwStatus;
    }
};

extern "C" 
DWORD   DriverInit(fsdev_t * pDev, DWORD dwContext,LPVOID lpParam)
{
    return ((DriverFilterMgr *)pDev)->DriverInit(dwContext,lpParam);
}
extern "C" 
BOOL    DriverPreDeinit(fsdev_t * pDev,DWORD dwContext) {
    return ((DriverFilterMgr *)pDev)->DriverPreDeinit(dwContext);
}
extern "C" 
BOOL    DriverDeinit(fsdev_t * pDev, DWORD dwContext) {
    return ((DriverFilterMgr *)pDev)->DriverDeinit(dwContext);
}
extern "C" 
DWORD    DriverOpen(fsdev_t * pDev,DWORD dwConext,DWORD dwAccessCode,DWORD SharedMode) {
    return ((DriverFilterMgr *)pDev)->DriverOpen(dwConext,dwAccessCode,SharedMode);
}

extern "C" 
BOOL    DriverPreClose(fsdev_t * pDev, DWORD dwOpenCont) {
    return ((DriverFilterMgr *)pDev)->DriverPreClose(dwOpenCont) ;
}
extern "C" 
BOOL    DriverClose(fsdev_t * pDev,DWORD dwOpenCont) {
    return ((DriverFilterMgr *)pDev)->DriverClose(dwOpenCont) ;
}
extern "C" 
DWORD   DriverRead(fsdev_t * pDev, DWORD dwOpenCont,LPVOID pInBuf,DWORD cbSize,HANDLE hAsyncRef) {
    return ((DriverFilterMgr *)pDev)->DriverRead(dwOpenCont,pInBuf,cbSize,hAsyncRef);
}
extern "C" 
DWORD   DriverWrite(fsdev_t * pDev, DWORD dwOpenCont,LPCVOID pOutBuf,DWORD cbSize,HANDLE hAsyncRef) {
    return ((DriverFilterMgr *)pDev)->DriverWrite( dwOpenCont,pOutBuf,cbSize,hAsyncRef) ;
}
extern "C" 
DWORD   DriverSeek(fsdev_t * pDev, DWORD dwOpenCont,long lDistanceToMove,DWORD dwMoveMethod) 
{
    return ((DriverFilterMgr *)pDev)->DriverSeek(dwOpenCont,lDistanceToMove,dwMoveMethod) ;
}
extern "C" 
BOOL    DriverControl(fsdev_t * pDev,DWORD dwOpenCont,DWORD dwCode, PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut,
          PDWORD pdwActualOut,HANDLE hAsyncRef)
{
    return ((DriverFilterMgr *)pDev)->DriverControl(dwOpenCont,dwCode,pBufIn,dwLenIn,pBufOut,dwLenOut,
          pdwActualOut,hAsyncRef);
}
extern "C" 
void    DriverPowerup(fsdev_t * pDev, DWORD dwConext) {
    ((DriverFilterMgr *)pDev)->DriverPowerup(dwConext) ;
}
extern "C" 
void    DriverPowerdn(fsdev_t * pDev, DWORD dwConext) {
    ((DriverFilterMgr *)pDev)->DriverPowerdn(dwConext) ;
}
extern "C" 
BOOL    DriverCancelIo(fsdev_t * pDev, DWORD dwOpenCont, HANDLE hAsyncHandle)
{
    return ((DriverFilterMgr *)pDev)->DriverCancelIo(dwOpenCont, hAsyncHandle) ;
}

extern "C" 
PVOID   CreateDriverFilter(fsdev_t * pDev, LPCTSTR lpFilter)
{
    if (pDev) {
        return (PVOID)((DriverFilterMgr *)pDev)->CreateDriverFilter(lpFilter);
    }
    return NULL;
}
    
DriverFilterMgr::DriverFilterMgr()
{
    memset((fsdev_t *)this, 0, sizeof(fsdev_t));
    m_pFilter = NULL;
    m_pFilterFolder = NULL;
    memset(&m_DummyFilter,0,sizeof(m_DummyFilter));
    m_DummyFilter.dwFilterInterfaceVersion = DRIVER_FILTER_INTERFACE_VERSION;
}
DriverFilterMgr::~DriverFilterMgr()
{
    if (m_pFilterFolder) {
        delete(m_pFilterFolder);
    }
}
FilterFolder * DriverFilterMgr::CreateDriverFilter(LPCTSTR lpFilter)
{
    if (m_pFilterFolder == NULL) {
        m_pFilterFolder = new FilterFolder();
        if (m_pFilterFolder && !m_pFilterFolder->Init(lpFilter)) {
            delete m_pFilterFolder;
            m_pFilterFolder = NULL;
        }
    }
    return m_pFilterFolder;
}
DWORD DriverFilterMgr::InitDriverFilters(LPCTSTR lpDeviceRegPath)
{
    DWORD dwStatus = ERROR_SUCCESS;

    if (m_pFilterFolder!=NULL && m_pFilter==NULL) {
        // Initial Dummy Filter.
        m_DummyFilter.dwFilterInterfaceVersion = DRIVER_FILTER_INTERFACE_VERSION;
        m_DummyFilter.fnFilterDeinit = NULL;
        m_DummyFilter.pNextFilter = NULL;
    
        // Driver's Entry
        m_DummyFilter.fnInit = (pFilterInitFn)fnInit;             
        m_DummyFilter.fnPreDeinit = (pFilterPreDeinitFn)fnPreDeinit;
        m_DummyFilter.fnDeinit = (pFilterDeinitFn)fnDeinit;
        m_DummyFilter.fnOpen = (pFilterOpenFn) fnOpen;
        m_DummyFilter.fnPreClose = (pFilterPreCloseFn)fnPreClose;
        m_DummyFilter.fnClose = (pFilterCloseFn)fnClose;
        m_DummyFilter.fnRead = (pFilterReadFn)fnRead;
        m_DummyFilter.fnWrite = (pFilterWriteFn)fnWrite;
        m_DummyFilter.fnSeek = (pFilterSeekFn)fnSeek;
        m_DummyFilter.fnControl = (pFilterControlFn)fnControl;
        m_DummyFilter.fnPowerup = (pFilterPowerupFn)fnPowerup ;       
        m_DummyFilter.fnPowerdn = (pFilterPowerdnFn)fnPowerdn;       
        m_DummyFilter.fnCancelIo = (pFilterCancelIoFn)fnCancelIo;
        dwStatus = m_pFilterFolder->InitializeFilters(
                                        &m_DummyFilter,
                                        lpDeviceRegPath,
                                        &m_pFilter);
    }
    return dwStatus;
}


