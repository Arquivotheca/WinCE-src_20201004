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
#include <types.h>
#include <ceddk.h>
#include <regmani.h>

BOOL RegManipulate::SetupOneKey(REGINI* pRegIni, int iElement, LPCTSTR lpszKeyPath){
    if(pRegIni == NULL || iElement == 0 || lpszKeyPath == NULL)
        return FALSE;
    
    RegManipulate   RegMani(HKEY_LOCAL_MACHINE);
    REG_KEY_INFO  KeyInfo = {0};
    BOOL    bRet = TRUE;

    //check if this key already exists, if so, return
    if(RegMani.IsAKeyValidate(lpszKeyPath) == TRUE){
        NKMSG(_T("Key %s already exists!"), lpszKeyPath);
        return FALSE;
    }
    
    wcscpy_s(KeyInfo.szRegPath, _countof(KeyInfo.szRegPath), lpszKeyPath);
    
    for(int i = 0; i < iElement; i++){
        bRet = RegMani.StoreAnItem(&KeyInfo, 
                                        pRegIni[i].lpszVal, 
                                        wcslen(pRegIni[i].lpszVal), 
                                        pRegIni[i].dwType, 
                                        pRegIni[i].pData, 
                                        pRegIni[i].dwLen);
        if(bRet == FALSE)
            goto Error;
    }

    //now create the key in registry
    if(RegMani.SetAKey(&KeyInfo, FALSE) == FALSE){
        NKMSG(_T("Can not create key %s"), lpszKeyPath);
        bRet = FALSE;
    }
    
Error:        

    if(bRet == FALSE){//sth. wrong, delete that key 
        RegMani.DeleteAKey(lpszKeyPath);
    }

    RegMani.DeleteValues(&KeyInfo, TRUE);

    return bRet;

}

BOOL RegManipulate::OpenKey(LPCTSTR szPath, PHKEY phKey, BOOL fTry){

    if(szPath == NULL || phKey == NULL)
        return FALSE;
    
    if (RegOpenKeyEx( m_hMainKey, szPath, 0, 0,  phKey) != ERROR_SUCCESS) {
        if(fTry == FALSE)
            NKMSG(_T("Can not open reg key: %s"), szPath);
        
        *phKey = NULL;
        return FALSE;
    }

    return TRUE;
}

BOOL 
RegManipulate::IsAKeyValidate(LPCTSTR szPath){

    HKEY hKey;
    
    if (RegOpenKeyEx( m_hMainKey, szPath, 0, 0,  &hKey) != ERROR_SUCCESS) {
        return FALSE;
    }

    RegCloseKey(hKey);
    return TRUE;
}

//retrieve one value item according to the index value
LONG RegManipulate::EnumAValue(HKEY  hKey, DWORD dwIndex, PREG_KEY_INFO pKInfo){

    LONG    retVal = ERROR_SUCCESS;
    TCHAR   szValName[128];
    BYTE    tempData[128];
    DWORD   dwValLen = 128;
    DWORD   dwType, dwDataLen = 128;

    if(dwIndex == 0){//first one, so clear pKInfo stucture
        pKInfo->uItems = 0;
        pKInfo->pValueHead = NULL;
    }
    
    //query this value
    retVal = RegEnumValue( hKey,
                         dwIndex,
                         szValName,
                         &dwValLen,
                         NULL,
                         &dwType,
                         tempData,
                         &dwDataLen);

    if(retVal != ERROR_SUCCESS)//sth. wrong 
        return retVal;
    if(StoreAnItem(pKInfo, szValName, dwValLen, dwType, tempData, dwDataLen) == FALSE)
        return ERROR_INVALID_DATA;
    else
        return ERROR_SUCCESS;
}

BOOL RegManipulate::InsertTail(PREG_KEY_INFO pKeyInfo, PREG_VALUE pRegVal){
    if(pKeyInfo == NULL || pRegVal == NULL)
        return FALSE;

    PREG_VALUE pCur = pKeyInfo->pValueHead;
    if(pCur == NULL) {
        pKeyInfo->pValueHead = pRegVal;
    }
    else {
        while(pCur->pNext != NULL)
            pCur = pCur->pNext;
        pCur->pNext = pRegVal;

    }

    pKeyInfo->uItems ++;

    return TRUE;
}

BOOL 
RegManipulate::StoreAnItem(PREG_KEY_INFO pKeyInfo, LPCTSTR szValName, DWORD dwNameLen, DWORD dwType, PBYTE pData, DWORD dwLen){

    if(pKeyInfo == NULL || szValName == NULL || dwNameLen == 0|| pData == NULL || dwLen == 0)
        return FALSE;

    PREG_VALUE  pRegVal = NULL;

    pRegVal = new REG_VALUE;
    if(pRegVal == NULL)
        return FALSE;

    memset(pRegVal, 0, sizeof(REG_VALUE));
    wcscpy_s(pRegVal->szValName, _countof(pRegVal->szValName), szValName);

    pRegVal->dwType = dwType;
    pRegVal->dwSize = dwLen;
   
    switch(dwType){
        case REG_BINARY:    //binary data
            memcpy((PBYTE)&(pRegVal->u.binData), pData, dwLen);
            break;
        case REG_DWORD: //dword data
            memcpy((PBYTE)&(pRegVal->u.dwData), pData, sizeof(DWORD));
            break;
        case REG_SZ: //string
            memcpy((PBYTE)(pRegVal->u.szData), pData, dwLen);
            if(wcslen(pRegVal->u.szData) * sizeof(TCHAR) == dwLen){
                NKMSG(_T("CAMEHERE"));
                pRegVal->dwSize += sizeof(TCHAR); //to include null ending
            }
            break;
        case REG_MULTI_SZ: // BUGBUG: currently we only convert it to multiple dwords
//          if(MultiSZtoDataList(pKeyInfo->u[dwIndex].dwDataList, pData, dwLen) == FALSE)
//              return FALSE;
            memcpy((PBYTE)(pRegVal->u.szData), pData, dwLen);
            break;
        default: //unknow type
            delete pRegVal;
            return FALSE;
    }

    InsertTail(pKeyInfo, pRegVal);
    
    return TRUE;
}

BOOL
RegManipulate::MultiSZtoDataList(PDWORD pdwDataList, PBYTE pData, DWORD dwLen){
    PWCHAR  szPT = (PWCHAR)pData;

    if(pdwDataList == NULL || pData == NULL || dwLen == 0)
        return FALSE;

    PWCHAR endP;
    DWORD   dwNum = 0;
    for(UINT i = 0; i < REG_MULTIPLE_DATA_NUM; i ++){

                if (*szPT == (WCHAR)'\0') //end
                    break;

        //convert one data
            pdwDataList[i+1] = wcstoul(szPT, &endP, 16);

            if (szPT == endP) //got nothing 
                    break;
        //
            dwNum++;
        //move to next substring
            szPT = ++endP;
    }

    pdwDataList[0] = dwNum;

    return TRUE;
}

BOOL
RegManipulate::SetAnItem(HKEY hKey, LPCTSTR szValName, DWORD dwType, U_ITEM u, DWORD dwSize){

    if(hKey == NULL || szValName == NULL)
        return FALSE;
    if(dwSize < 0 || dwSize > REG_STRING_LENGTH)
        return FALSE;
    
    switch(dwType){
        case REG_BINARY:
            if(RegSetValueEx(hKey, szValName, 0, REG_BINARY, u.binData, dwSize) != ERROR_SUCCESS){
                NKMSG(_T("Can not set binary data for item %s"), szValName);
                return FALSE;
            }
            break;
        case REG_SZ:
            if(RegSetValueEx(hKey, szValName, 0, REG_SZ, (PBYTE)(u.szData), dwSize) != ERROR_SUCCESS){
                NKMSG(_T("Can not set string data for item %s"), szValName);
                return FALSE;
            }
            break;
        case REG_DWORD:
            if(SetSingleVal(hKey, szValName, u.dwData) == FALSE){
                NKMSG(_T("Can not set dword data for item %s"), szValName);
                return FALSE;
            }
            break;
        case REG_MULTI_SZ:
            if(RegSetValueEx(hKey, szValName, 0, REG_MULTI_SZ, (PBYTE)(u.szData), dwSize) != ERROR_SUCCESS){
                NKMSG(_T("Can not set string data for item %s"), szValName);
                return FALSE;
            }
            break;
//          if(SetValList(hKey, szValName, u.dwDataList[0], &(u.dwDataList[1])) == FALSE){
//              NKMSG(_T("Can not set multi_sz data for item %s"), szValName);
//              return FALSE;
//          }
//          break;
        default:
            return FALSE;
    }
            
    return TRUE;
}

//set a dword type item to registry
BOOL RegManipulate::SetSingleVal(HKEY hKey, LPCTSTR szValName, DWORD dwData){

    DWORD   dwTemp = dwData;
    
    if(hKey == NULL || szValName == NULL)
        return FALSE;
    
    if(RegSetValueEx(hKey, szValName, 0, REG_DWORD, (PBYTE)&dwTemp, sizeof(DWORD)) != ERROR_SUCCESS)
        return FALSE;

    return TRUE;
}

//convert dword list to multi_sz and add the item to registry
BOOL RegManipulate::SetValList(HKEY hKey, LPCTSTR szValName, DWORD dwNum, PDWORD dwDataList){

    WCHAR   ValList[2*REG_STRING_LENGTH];
    DWORD   dwSubSize = 0, dwTotalSize = 0;
    WCHAR   SubItem[11];
    PWCHAR  pw = ValList;

    if(hKey == NULL || szValName == NULL ||dwNum > REG_MULTIPLE_DATA_NUM || dwNum < 2 || dwDataList == NULL)
        return FALSE;

    //conver each dword value
    for(DWORD i = 0; i < dwNum; i++){
        _ultow_s(dwDataList[i], SubItem, _countof(SubItem), 16);
        dwSubSize = wcslen(SubItem) + 1;
        if((dwTotalSize + dwSubSize + 1) > 2*REG_STRING_LENGTH){
            NKMSG(_T("DWORD list too long!"));
            return FALSE;
        }

        DWORD dwTempTotal = dwSubSize+dwTotalSize;
        if(dwTempTotal > MAX_PATH || dwTempTotal < dwSubSize || dwTempTotal < dwTotalSize){//overflow or underflow
            break;
        }
        wcscpy_s(pw, dwSubSize, SubItem);
        pw += dwSubSize;
        dwTotalSize += dwSubSize;
    }

    *pw = (WCHAR)'\0';
    dwTotalSize ++;

    if(RegSetValueEx(hKey, szValName, 0, REG_MULTI_SZ, (PBYTE)ValList, dwTotalSize * sizeof(WCHAR)) != ERROR_SUCCESS)
        return FALSE;

    return TRUE;
    
}

//retrieve a reg key's info into the structure
BOOL RegManipulate::GetAKey(PREG_KEY_INFO pKeyInfo){

    HKEY    hCurKey = NULL;

    if(pKeyInfo == NULL)
        return FALSE;

    if(OpenKey(pKeyInfo->szRegPath, &hCurKey, FALSE) == FALSE)
        return FALSE;
    
    DWORD   dwIndex = 0;
    LONG    ret = ERROR_SUCCESS;
    //retrieve each value item

    while((ret = EnumAValue(hCurKey, dwIndex, pKeyInfo)) == ERROR_SUCCESS)
        dwIndex ++;

    if(ret != ERROR_NO_MORE_ITEMS){ //failed
        NKMSG(_T("enumerate key %s failed!"), pKeyInfo->szRegPath);
        RegCloseKey(hCurKey);
        return  FALSE;
    }

    RegCloseKey(hCurKey);
    return TRUE;
}

//create a key
BOOL RegManipulate::SetAKey(PREG_KEY_INFO pKeyInfo, BOOL bForceNew){

    HKEY    hCurKey = NULL;
    DWORD   dwDispostion = 0;
    LONG    retVal = ERROR_SUCCESS;
    BOOL bCreateNewKey = TRUE;

    if(pKeyInfo == NULL)
        return FALSE;

    //this key already exists, delete it first
    if(OpenKey(pKeyInfo->szRegPath, &hCurKey, TRUE) == TRUE){
        if(bForceNew == TRUE){//we require to create a fresh new key
            RegCloseKey(hCurKey);
            hCurKey = NULL;
            if(RegDeleteKey(m_hMainKey, pKeyInfo->szRegPath) != ERROR_SUCCESS){
                NKMSG(_T("Can not delete key %s"), pKeyInfo->szRegPath);
                return FALSE;
            }
        }
    else {
        bCreateNewKey = FALSE;
           NKMSG(_T("key %s already exists -update existing key"), pKeyInfo->szRegPath);
    }
    /*
        else{
            NKMSG(_T("Key %s already exists"), pKeyInfo->szRegPath);
            return FALSE;
        }*/
    }

    if (bCreateNewKey){//try creating this key
        retVal = RegCreateKeyEx(m_hMainKey,
                              pKeyInfo->szRegPath,
                              0, NULL, 0, 0, NULL,
                              &hCurKey,
                              &dwDispostion);

        if(retVal != ERROR_SUCCESS){
            NKMSG(_T("Can not create key %s"), pKeyInfo->szRegPath);
            return FALSE;
        }
    }
    
    BOOL    bRet = TRUE;
    PREG_VALUE pCur = pKeyInfo->pValueHead;
    for(UINT i = 0; i < pKeyInfo->uItems; i++){
        if(pCur == NULL){//no more, should not come here
            NKMSG(_T("WARNING: number of nodes is not equal to uItems"));
            break;
        }
        
        //set each value item
        bRet = SetAnItem(hCurKey, 
                         pCur->szValName,
                         pCur->dwType,
                         pCur->u,
                         pCur->dwSize);

        if(bRet == FALSE){
            NKMSG(_T("Can not set value item %s"), pCur->szValName);
            RegCloseKey(hCurKey);
            return FALSE;
        }
        pCur = pCur->pNext;
    }

    RegCloseKey(hCurKey);

    return TRUE;
}



//get a value to a key
BOOL RegManipulate::GetAValue(LPCTSTR szRegPath, LPCTSTR szValName, DWORD& dwType, DWORD& cbSize, PBYTE pData){

    HKEY    hCurKey = NULL;

    if(szRegPath == NULL || szValName == NULL)
        return FALSE;

    //this key exists, query value
    if(OpenKey(szRegPath, &hCurKey, FALSE) == TRUE){
        if(RegQueryValueEx(hCurKey, szValName, NULL, &dwType, (PBYTE)pData, &cbSize) != ERROR_SUCCESS){
            RegCloseKey(hCurKey);
            return FALSE;
        }
        RegCloseKey(hCurKey);
        return TRUE;
    }

    return FALSE;
}


//add a value to a key
BOOL RegManipulate::SetAValue(LPCTSTR szRegPath, LPCTSTR szValName, DWORD dwType, DWORD cbSize, PBYTE pData){

    HKEY    hCurKey = NULL;

    if(szRegPath == NULL || szValName == NULL)
        return FALSE;

    //this key exists, add value
    if(OpenKey(szRegPath, &hCurKey, FALSE) == TRUE){
        if(RegSetValueEx(hCurKey, szValName, 0, dwType, (PBYTE)pData, cbSize) != ERROR_SUCCESS){
            RegCloseKey(hCurKey);
            return FALSE;
        }
        RegCloseKey(hCurKey);
        return TRUE;
    }

    return FALSE;
}


BOOL 
RegManipulate::CopyAKey(LPCTSTR szDestPath, LPCTSTR szSrcPath){
    LPTSTR  szDestSubPath = NULL, szSrcSubPath = NULL;
    HKEY    hSrcKey= NULL;
    PREG_KEY_INFO pKeyInfo = NULL;

    //Parameter checking
    if((szDestPath == NULL) || (szSrcPath == NULL))
        return FALSE;

    //allocate mem for structures
    szDestSubPath = new TCHAR[REG_STRING_LENGTH];
    szSrcSubPath = new TCHAR[REG_STRING_LENGTH];
    if(szDestSubPath == NULL || szSrcSubPath == NULL){
        NKMSG(_T("Out of memory"));
        goto CLEANUP;   
    }
    pKeyInfo = new REG_KEY_INFO;
    if(pKeyInfo == NULL){
        NKMSG(_T("Out of memory"));
        goto CLEANUP;   
    }
    memset(pKeyInfo, 0, sizeof(REG_KEY_INFO));
    
    //open keys
    if(OpenKey(szSrcPath, &hSrcKey, FALSE) == FALSE){
        return FALSE;
    }
    
    //enum subkeys
    DWORD dwIndex = 0;
    LONG  lRet = ERROR_SUCCESS;
    TCHAR   szSubKeyName[REG_STRING_LENGTH];
    DWORD   cbKeyNameLen;

    //copy sub keys 
    while(lRet == ERROR_SUCCESS){
        cbKeyNameLen = REG_STRING_LENGTH;
        lRet = RegEnumKeyEx(hSrcKey, dwIndex,szSubKeyName, &cbKeyNameLen, NULL, NULL, NULL, NULL);
        if(lRet == ERROR_SUCCESS){
            //create subkey destination path
            swprintf_s(szDestSubPath, REG_STRING_LENGTH, TEXT("%s\\%s"), szDestPath, szSubKeyName);

            //create subkey source path
            swprintf_s(szSrcSubPath, REG_STRING_LENGTH, TEXT("%s\\%s"), szSrcPath, szSubKeyName);

            if(CopyAKey(szDestSubPath, szSrcSubPath) == FALSE){
                NKMSG(_T("Copy key from %s to %s failed!"), szSrcSubPath, szDestSubPath);
                RegCloseKey(hSrcKey);
                goto CLEANUP;
            }

            dwIndex ++;
        }
    }
    RegCloseKey(hSrcKey);
    
    //copy values
    wcscpy_s(pKeyInfo->szRegPath, _countof(pKeyInfo->szRegPath), szDestPath);
    if(SetAKey(pKeyInfo, FALSE) == FALSE){

        NKMSG(_T("Can not set values to %s"), szDestPath);
        goto CLEANUP;
    }

    delete[] szSrcSubPath;
    delete[] szDestSubPath;
    delete pKeyInfo;
    return TRUE;
    
CLEANUP:

    //sth. wrong, delete possible already-created destination key
    if(OpenKey(szDestPath, &hSrcKey, FALSE) == TRUE){//destination key does exist
        RegCloseKey(hSrcKey);
        RegDeleteKey(m_hMainKey, szDestPath);
    }

    if(szSrcSubPath != NULL)
        delete[] szSrcSubPath;
    if(szDestSubPath != NULL)
        delete[] szDestSubPath;
    if(pKeyInfo != NULL)
        delete pKeyInfo;

    return FALSE;
    
}

BOOL 
RegManipulate::DeleteAKey(LPCTSTR szPath){
    
    for(int i = 0; i < 6; i ++){
        if(RegDeleteKey(m_hMainKey, szPath) == ERROR_SUCCESS){
            Sleep(100); //yield
            return TRUE;
        }
        Sleep(300);//yield
    }
    
    return FALSE;
}


BOOL 
RegManipulate::ModifyKeyValue(PREG_KEY_INFO pKeyInfo, PREG_VALUE pRegVal, DWORD dwType, DWORD dwSize, U_ITEM u){

    if(pKeyInfo == NULL || pRegVal == NULL)
        return FALSE;

    PREG_VALUE pCur = pKeyInfo->pValueHead;

    while(pCur != NULL){
        if(pCur == pRegVal)
            break;
        pCur = pCur->pNext;
    }

    //can not find it
    if(pCur == NULL)
        return FALSE;
        
    if(dwType == REG_SZ || dwType == REG_BINARY)
        pCur->dwSize = dwSize;

    pCur->dwType = dwType;
    memcpy((PBYTE)&(pCur->u), (PBYTE)&u, sizeof(U_ITEM));
    
    return TRUE;
}

BOOL 
RegManipulate::AddKeyValue(PREG_KEY_INFO pKeyInfo,  LPCTSTR szValName, DWORD dwType, DWORD dwSize, U_ITEM u){

    if(pKeyInfo == NULL || szValName == NULL)
        return FALSE;

    if(wcslen(szValName) > REG_STRING_LENGTH){
        NKMSG(_T("Value name too long!"));
        return FALSE;
    }

     PREG_VALUE  pRegVal = NULL;

    pRegVal = new REG_VALUE;
    if(pRegVal == NULL)
        return FALSE;

    memset(pRegVal, 0, sizeof(REG_VALUE));
    wcscpy_s(pRegVal->szValName, _countof(pRegVal->szValName), szValName);
    pRegVal->dwType = dwType;
    pRegVal->dwSize = dwSize;

    //copy item value
    memcpy((PBYTE)&(pRegVal->u),(PBYTE)&u, sizeof(U_ITEM));

    InsertTail(pKeyInfo, pRegVal);
    
    return TRUE;
}

BOOL 
RegManipulate::DeleteKeyValue(PREG_KEY_INFO pKeyInfo, PREG_VALUE pRegVal){

    if(pKeyInfo == NULL || pRegVal == NULL || pKeyInfo->pValueHead == NULL)
        return FALSE;

    PREG_VALUE pCur = pKeyInfo->pValueHead, pPre = NULL;

    while(pCur != NULL){
        if(pCur == pRegVal)
            break;
        pPre = pCur;
        pCur = pCur->pNext;
    }

    //can not find it
    if(pCur == NULL)
        return FALSE;

    //delete it from the list
    pPre->pNext = pCur->pNext;
    delete pCur;
    pKeyInfo->uItems --;
    
    return TRUE;
}

PREG_VALUE
RegManipulate::FindKeyValue(PREG_KEY_INFO pKeyInfo, LPCTSTR szValName){

    if(pKeyInfo == NULL || pKeyInfo->pValueHead == NULL)
        return NULL;
        
    PREG_VALUE pCur = pKeyInfo->pValueHead;
    while(pCur != NULL){
        if(!wcscmp(pCur->szValName, szValName))//find it
            break;
        pCur = pCur->pNext;
    }

    return pCur;

}

BOOL   
RegManipulate::FindMatchedKeyValue(PREG_KEY_INFO pKeyInfo, 
                                            LPCTSTR szValName, 
                                            DWORD dwType, 
                                            DWORD dwSize,
                                            PBYTE pData){
    if(pData == NULL)
        return FALSE;

    PREG_VALUE pVal = FindKeyValue(pKeyInfo, szValName);
    if(pVal == NULL){
        NKMSG(_T("can not find value %s"), szValName);
        return FALSE;
    }

    if(dwType != pVal->dwType){
        NKMSG(_T("type 0x%x != expected value 0x%x"), pVal->dwType, dwType);
        return FALSE;
    }

    if(dwSize != pVal->dwSize){
        NKMSG(_T("Size 0x%x != expected value 0x%x"), pVal->dwSize, dwSize);
        return FALSE;
    }

    if(memcmp((PBYTE)(&(pVal->u)),  pData, dwSize)){
        NKMSG(_T("data is not equivalent"));
        return FALSE;
    }

    return TRUE;
}
                                            
VOID RegManipulate::DeleteValues(PREG_KEY_INFO pKeyInfo, BOOL bCleanAll){
    if(pKeyInfo == NULL)
        return;

    PREG_VALUE pCur = pKeyInfo->pValueHead;

    while(pCur != NULL){
        PREG_VALUE pTemp = pCur;
        pCur = pCur->pNext;
        delete pTemp;
    }

    if(bCleanAll == TRUE){ //clean up everything
        memset(pKeyInfo, 0, sizeof(REG_KEY_INFO));
        return;
    }

    //otherwise keep the path info 
    pKeyInfo->pValueHead = NULL;
    pKeyInfo->uItems = 0;   
}
