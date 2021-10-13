//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
/*++
Module Name:

    Regmani.cpp

Abstract:

    This file implements the a registry manipulation class
--*/

#include <windows.h>
#include <types.h>
#include <ceddk.h>
#include "regmani.h"

//---------------------------------------------------------------------------------------
//   Open a key by the given registry path
//---------------------------------------------------------------------------------------
BOOL 
CRegManipulate::OpenKey(LPCTSTR szPath, PHKEY phKey){

    if(szPath == NULL || phKey == NULL)
        return FALSE;
        
    if (RegOpenKeyEx( m_hMainKey, szPath, 0, 0,  phKey) != ERROR_SUCCESS) {
        *phKey = NULL;
        return FALSE;
    }

    return TRUE;
}

//---------------------------------------------------------------------------------------
//   Verify a registry key does exist
//---------------------------------------------------------------------------------------
BOOL 
CRegManipulate::IsAKeyValidate(LPCTSTR szPath){

    if(szPath == NULL)
        return FALSE;
        
    HKEY hKey;

    //try to open this key
    if (RegOpenKeyEx( m_hMainKey, szPath, 0, 0,  &hKey) != ERROR_SUCCESS) {
        return FALSE;
    }

    RegCloseKey(hKey);
	return TRUE;
}

//---------------------------------------------------------------------------------------
//retrieve one value item according to the index value
//---------------------------------------------------------------------------------------
LONG 
CRegManipulate::EnumAValue(HKEY  hKey, DWORD dwIndex, PREG_KEY_INFO pKInfo){

    LONG        retVal = ERROR_SUCCESS;
    TCHAR      szValName[128];
    BYTE        tempData[128];
    DWORD     dwValLen = 128;
    DWORD     dwType, dwDataLen = 128;

    if(dwIndex == 0){//first one, so clear pKInfo stucture
        pKInfo->uItems = 0;
        pKInfo->uBitmap = 0;
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
    if(retVal != ERROR_SUCCESS){//sth. wrong 
        if(retVal == ERROR_INVALID_DATA)//it could caused by mui_sz:, just ignore it here
            return ERROR_SUCCESS;
        else//otherwise something is really wrong
            return retVal;
    }

    //store this value
    if(StoreAnItem(pKInfo, szValName, dwValLen, dwIndex, dwType, tempData, dwDataLen) == FALSE)
    	return ERROR_INVALID_DATA;
    else
    	return ERROR_SUCCESS;
}

//---------------------------------------------------------------------------------------
//store a value item into data structure
//---------------------------------------------------------------------------------------
BOOL 
CRegManipulate::StoreAnItem(PREG_KEY_INFO pKeyInfo, LPCTSTR szValName, DWORD dwNameLen, DWORD dwIndex, DWORD dwType, PBYTE pData, DWORD dwLen){

    if(pKeyInfo == NULL || szValName == NULL || dwNameLen == 0|| pData == NULL || dwLen == 0)
        return FALSE;
	
    switch(dwType){
        case REG_BINARY:	//binary data
            memcpy((PBYTE)&(pKeyInfo->u[dwIndex].binData), pData, dwLen);
            pKeyInfo->dwSize[dwIndex] = dwLen;
            break;
        case REG_DWORD: //dword data
            memcpy((PBYTE)&(pKeyInfo->u[dwIndex].dwData), pData, sizeof(DWORD));
            pKeyInfo->dwSize[dwIndex] = dwLen;
            break;
        case REG_SZ: //string
            memcpy((PBYTE)(pKeyInfo->u[dwIndex].szData), pData, dwLen);
            pKeyInfo->dwSize[dwIndex] = dwLen;
            break;
        case REG_MULTI_SZ: //multiple-string 
            memcpy((PBYTE)(pKeyInfo->u[dwIndex].szData), pData, dwLen);
            pKeyInfo->dwSize[dwIndex] = dwLen;
            break;
        default: //unknow type
            return FALSE;
    }

    memcpy((PBYTE)(pKeyInfo->szValName[dwIndex]), szValName, dwNameLen*sizeof(TCHAR));
    pKeyInfo->dwType[dwIndex] = dwType;
    //set bitmap
    pKeyInfo->uBitmap |= 1<<pKeyInfo->uItems;
    pKeyInfo->uItems ++;

    return TRUE;
    
}

//---------------------------------------------------------------------------------------
//convert multi_sz to dword list
//---------------------------------------------------------------------------------------
BOOL
CRegManipulate::MultiSZtoDataList(PDWORD pdwDataList, PBYTE pData, DWORD dwLen){

    if(pdwDataList == NULL || pData == NULL || dwLen == 0)
        return FALSE;

    PWCHAR  szPT = (PWCHAR)pData;
    PWCHAR  endP;
    DWORD   dwNum = 0;

    for(UINT i = 0; i < REG_MULTIPLE_DATA_NUM; i ++){
        if (*szPT == (WCHAR)'\0') //end
            break;
        //convert one data
        pdwDataList[i+1] = wcstoul(szPT, &endP, 16);
        if (szPT == endP) //got nothing 
            break;
        dwNum++;
        //move to next substring
        szPT = ++endP;
    }

    pdwDataList[0] = dwNum;
    return TRUE;

}

//---------------------------------------------------------------------------------------
//Write a value item in data structure to registry
//---------------------------------------------------------------------------------------
BOOL
CRegManipulate::SetAnItem(HKEY hKey, LPCTSTR szValName, DWORD dwType, U_ITEM u, DWORD dwSize){

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
        default:
            return TRUE;
    }
			
    return TRUE;
}

//---------------------------------------------------------------------------------------
//retrieve a reg key's info into the structure
//---------------------------------------------------------------------------------------
BOOL CRegManipulate::GetAKey(PREG_KEY_INFO pKeyInfo){

    if(pKeyInfo == NULL)
        return FALSE;

    HKEY	hCurKey = NULL;

    if(OpenKey(pKeyInfo->szRegPath, &hCurKey) == FALSE)
        return FALSE;

    DWORD   dwIndex = 0;
    LONG      ret = ERROR_SUCCESS;
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

//---------------------------------------------------------------------------------------
//create a key in registry
//---------------------------------------------------------------------------------------
BOOL CRegManipulate::SetAKey(PREG_KEY_INFO pKeyInfo, BOOL bNew){
    if(pKeyInfo == NULL)
        return FALSE;

    HKEY      hCurKey = NULL;
    DWORD   dwDispostion = 0;
    LONG      retVal = ERROR_SUCCESS;

    //this key already exists, delete it if needed
    BOOL bOldkeyExist = FALSE;
    if(OpenKey(pKeyInfo->szRegPath, &hCurKey) == TRUE){
        if(bNew == TRUE){//we require to create a fresh new key
            RegCloseKey(hCurKey);
            hCurKey = NULL;
            if(RegDeleteKey(m_hMainKey, pKeyInfo->szRegPath) != ERROR_SUCCESS){
                NKMSG(_T("Can not delete key %s"), pKeyInfo->szRegPath);
                return FALSE;
            }
        }
        else{
            bOldkeyExist = TRUE;
        }
    }

    //try creating this key
    if(bOldkeyExist == FALSE){
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
	
    UINT	uItemVa = 0;
    BOOL	bRet;
    UINT	uBits = pKeyInfo->uBitmap;
    for(UINT i = 0; i < pKeyInfo->uItems; i++){
        while(!(uBits & 1) ){
            uBits = uBits >> 1;
            uItemVa ++;
            if(uItemVa >= REG_ITEM_NUM) {//sth. wrong
                RegCloseKey(hCurKey);
                NKMSG(_T("Item number does not match the bitmap!"));
                return FALSE;
            }
        }

        //set each value item
        bRet = SetAnItem(hCurKey, 
                                    pKeyInfo->szValName[uItemVa],
                                    pKeyInfo->dwType[uItemVa],
                                    pKeyInfo->u[uItemVa],
                                    pKeyInfo->dwSize[uItemVa]);

        if(bRet == FALSE){
            NKMSG(_T("Can not set value item %s"), pKeyInfo->szValName[uItemVa]);
            RegCloseKey(hCurKey);
            return FALSE;
        }

        uItemVa ++;
        uBits = uBits >> 1;
    }

    RegCloseKey(hCurKey);

    return TRUE;
}

//---------------------------------------------------------------------------------------
//copy to a key in regsitry to another key
//---------------------------------------------------------------------------------------
BOOL 
CRegManipulate::CopyAKey(LPCTSTR szDestPath, LPCTSTR szSrcPath){
    if((szDestPath == NULL) || (szSrcPath == NULL))
        return FALSE;

    LPTSTR  szDestSubPath = NULL, szSrcSubPath = NULL;
    HKEY     hSrcKey= NULL;
    PREG_KEY_INFO pKeyInfo = NULL;

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
	
    //open source key
    if(OpenKey(szSrcPath, &hSrcKey) == FALSE){
        NKMSG(_T("Can not open key %s!"), szSrcPath);
        return FALSE;
    }
	
    //enum subkeys
    DWORD   dwIndex = 0;
    LONG      lRet = ERROR_SUCCESS;
    TCHAR    szSubKeyName[REG_STRING_LENGTH];
    DWORD   cbKeyNameLen;

    //copy sub keys	
    while(lRet == ERROR_SUCCESS){
        cbKeyNameLen = REG_STRING_LENGTH;
        lRet = RegEnumKeyEx(hSrcKey, dwIndex,szSubKeyName, &cbKeyNameLen, NULL, NULL, NULL, NULL);
        if(lRet == ERROR_SUCCESS){
            //create subkey destination path
            wcscpy(szDestSubPath, szDestPath);
            wcscat(szDestSubPath, _T("\\"));
            wcscat(szDestSubPath, szSubKeyName);
            //create subkey source path
            wcscpy(szSrcSubPath, szSrcPath);
            wcscat(szSrcSubPath, _T("\\"));
            wcscat(szSrcSubPath, szSubKeyName);

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
    wcscpy(pKeyInfo->szRegPath, szSrcPath);
    if(GetAKey(pKeyInfo) == FALSE){
    	NKMSG(_T("Can not retrieve values from %s"), szSrcPath);
    	goto CLEANUP;
    }

    wcscpy(pKeyInfo->szRegPath, szDestPath);
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
    if(OpenKey(szDestPath, &hSrcKey) == TRUE){//destination key does exist
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

//---------------------------------------------------------------------------------------
//delete a key
//---------------------------------------------------------------------------------------
BOOL 
CRegManipulate::DeleteAKey(LPCTSTR szPath){
    if(RegDeleteKey(m_hMainKey, szPath) != ERROR_SUCCESS)
        return FALSE;

    return TRUE;
}

//---------------------------------------------------------------------------------------
//modify a value item in the data structure
//---------------------------------------------------------------------------------------
BOOL 
CRegManipulate::ModifyKeyValue(PREG_KEY_INFO pKeyInfo, DWORD dwIndex, DWORD dwType, DWORD dwSize, U_ITEM u){

    if(pKeyInfo == NULL || dwIndex < 0 || dwIndex >= REG_ITEM_NUM)
        return FALSE;

    if(dwType == REG_SZ || dwType == REG_BINARY)
        pKeyInfo->dwSize[dwIndex] = dwSize;

    pKeyInfo->dwType[dwIndex] = dwType;
    memcpy((PBYTE)&(pKeyInfo->u[dwIndex]), (PBYTE)&u, sizeof(U_ITEM));
	
    return TRUE;
}

//---------------------------------------------------------------------------------------
//add a value item to the data structure
//---------------------------------------------------------------------------------------
BOOL 
CRegManipulate::AddKeyValue(PREG_KEY_INFO pKeyInfo,  LPCTSTR szValName, DWORD dwType, DWORD dwSize, U_ITEM u){

    if(pKeyInfo == NULL)
        return FALSE;

    DWORD 	dwIndex;
    UINT	uBitMap = pKeyInfo->uBitmap;


    if(pKeyInfo->uItems >=  REG_ITEM_NUM){
        NKMSG(_T("Can not add any more items!"));
        return FALSE;
    }

    if(wcslen(szValName) > REG_STRING_LENGTH){
        NKMSG(_T("Value name too long!"));
        return FALSE;
    }

    //find the first empty entry
    dwIndex = 0;
    while((uBitMap & 1)){
        uBitMap = uBitMap >> 1;
        dwIndex ++;
    }

    //make this entry valid
    pKeyInfo->uBitmap |= (1<<dwIndex);

    //copy the name
    wcscpy(pKeyInfo->szValName[dwIndex], szValName);

    //set type
    if(dwType == REG_SZ || dwType == REG_BINARY)
        pKeyInfo->dwSize[dwIndex] = dwSize;

    //copy item value
    pKeyInfo->dwType[dwIndex] = dwType;
    memcpy((PBYTE)&(pKeyInfo->u[dwIndex]),(PBYTE)&u, sizeof(U_ITEM));

    //increase item
    pKeyInfo->uItems ++;

    return TRUE;
}

//---------------------------------------------------------------------------------------
//delete a value item in the data structure
//---------------------------------------------------------------------------------------
BOOL 
CRegManipulate::DeleteKeyValue(PREG_KEY_INFO pKeyInfo, DWORD dwIndex){

    if(pKeyInfo == NULL || dwIndex < 0 || dwIndex >= REG_ITEM_NUM)
        return FALSE;

    pKeyInfo->uItems --;

    //invalid this entry
    UINT mask = (~0) ^ (1<<dwIndex);
    pKeyInfo->uBitmap &= mask;
    	
    return TRUE;
}

//---------------------------------------------------------------------------------------
//find a value item to the data structure according to the value name
//---------------------------------------------------------------------------------------
DWORD
CRegManipulate::FindKeyValue(PREG_KEY_INFO pKeyInfo, LPCTSTR szValName){

    if(pKeyInfo == NULL)
        return (DWORD)-1;

    DWORD dwIndex = 0;
    UINT	uMask, i;

    uMask = pKeyInfo->uBitmap;
    for(i = 0; i < pKeyInfo->uItems; i ++){
        while(!(uMask & 1)){
            uMask = uMask >> 1;
            dwIndex ++;
            if(dwIndex >= REG_ITEM_NUM){
            	NKMSG(_T("Bit map is not correct!"));
            	return (DWORD)-1;
            }
        }

        if(!wcscmp(pKeyInfo->szValName[dwIndex], szValName))//find it
        	break;
        uMask = uMask >> 1;
        dwIndex ++;
    }

    //not found
    if(i == (UINT)(pKeyInfo->uItems))
        return (DWORD)-1;
        
    return dwIndex;

}

//---------------------------------------------------------------------------------------
//set a dword type item to registry
//---------------------------------------------------------------------------------------
BOOL 
CRegManipulate::SetSingleVal(HKEY hKey, LPCTSTR szValName, DWORD dwData){

    if(hKey == NULL || szValName == NULL)
        return FALSE;

    DWORD   dwTemp = dwData;

    if(RegSetValueEx(hKey, szValName, 0, REG_DWORD, (PBYTE)&dwTemp, sizeof(DWORD)) != ERROR_SUCCESS)
        return FALSE;

    return TRUE;
}

//---------------------------------------------------------------------------------------
//convert dword list to multi_sz and add the item to registry
//---------------------------------------------------------------------------------------
BOOL 
CRegManipulate::SetValList(HKEY hKey, LPCTSTR szValName, DWORD dwNum, PDWORD dwDataList){

    if(hKey == NULL || szValName == NULL ||dwNum > REG_MULTIPLE_DATA_NUM || dwNum < 2 || dwDataList == NULL)
        return FALSE;


    WCHAR   ValList[2*REG_STRING_LENGTH];
    DWORD   dwSubSize = 0, dwTotalSize = 0;
    WCHAR   SubItem[11];
    PWCHAR  pw = ValList;

    //conver each dword value
    for(DWORD i = 0; i < dwNum; i++){
        _ultow(dwDataList[i], SubItem, 16);
        dwSubSize = wcslen(SubItem) + 1;
        if((dwTotalSize + dwSubSize + 1) > 2*REG_STRING_LENGTH){
            NKMSG(_T("DWORD list too long!"));
            return FALSE;
        }

        wcscpy(pw, SubItem);
        pw += dwSubSize;
        dwTotalSize += dwSubSize;
    }

    *pw = (WCHAR)'\0';
    dwTotalSize ++;

    if(RegSetValueEx(hKey, szValName, 0, REG_MULTI_SZ, (PBYTE)ValList, dwTotalSize * sizeof(WCHAR)) != ERROR_SUCCESS)
        return FALSE;

    return TRUE;
	
}


