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

#ifndef __REGMANI_H_
#define __REGMANI_H_
#include <devload.h>
#include <pcibus.h>

#define  NKMSG NKDbgPrintfW 

#define REG_BINARY_LENGTH   128
#define REG_STRING_LENGTH   260
#define REG_MULTIPLE_DATA_NUM   16
#define REG_MULTIPLE_STRING_NUM 4
#define REG_MULTIPLE_STRING_LENGTH  32 
#define REG_ITEM_NUM    32

//union struct holds for each value item under a reg key
typedef union _U_ITEM{
    BYTE    binData[    REG_BINARY_LENGTH]; //REG_BINARY
    DWORD   dwData; //REG_DWORD
    TCHAR   szData[REG_STRING_LENGTH];  //REG_SZ
    DWORD   dwDataList[REG_MULTIPLE_DATA_NUM+1];  //REG_MULTI_SZ, contents are dword type data, first dword hold the info of how many values 
    TCHAR   szDataList[REG_MULTIPLE_STRING_NUM][REG_MULTIPLE_STRING_LENGTH]; //REG_MULTI_SZ, contents are strings. seldom used
}U_ITEM;

//structure to hold each key value
typedef struct _REG_VALUE{
    TCHAR   szValName[REG_STRING_LENGTH];
    DWORD   dwType; //type
    DWORD   dwSize; //size in bytes of each item
    U_ITEM  u;    //values
    _REG_VALUE*  pNext; //next node
}REG_VALUE, *PREG_VALUE;

//structure that holds the content of a whole reg key
typedef struct _REG_KEY_INFO{
    TCHAR   szRegPath[REG_STRING_LENGTH]; 
    UINT    uItems;     //numbers of values under this key
    PREG_VALUE  pValueHead; //head to the value list
}REG_KEY_INFO, *PREG_KEY_INFO;


class RegManipulate {
public:
    RegManipulate(HKEY hMainKey):m_hMainKey(hMainKey){;};
    ~RegManipulate(){;};
    BOOL GetAKey(PREG_KEY_INFO pKeyInfo);

    // if bNew set to TRUE - it will always force to create a new key
    // Else it will update the existing key
    BOOL SetAKey(PREG_KEY_INFO pKeyInfo, BOOL bForceNew);
    BOOL ModifyKeyValue(PREG_KEY_INFO pKeyInfo, PREG_VALUE pRegVal, DWORD dwType, DWORD dwSize, U_ITEM u);
    BOOL AddKeyValue(PREG_KEY_INFO pKeyInfo, LPCTSTR szValName, DWORD dwType, DWORD dwSize, U_ITEM u);
    BOOL DeleteKeyValue(PREG_KEY_INFO pKeyInfo, PREG_VALUE pRegVal);
    PREG_VALUE FindKeyValue(PREG_KEY_INFO pKeyInfo, LPCTSTR szValName);
    BOOL OpenKey(LPCTSTR szPath, PHKEY phKey, BOOL fTry=FALSE);
    BOOL CopyAKey(LPCTSTR szDestPath, LPCTSTR szSrcPath);
    BOOL DeleteAKey(LPCTSTR szPath);
    BOOL IsAKeyValidate(LPCTSTR szPath);
    VOID DeleteValues(PREG_KEY_INFO pKeyInfo, BOOL bCleanAll);
    BOOL FindMatchedKeyValue(PREG_KEY_INFO pKeyInfo,  LPCTSTR szValName, DWORD dwType, DWORD dwSize, PBYTE pData);
    BOOL StoreAnItem(PREG_KEY_INFO pKeyInfo, LPCTSTR szValName, DWORD dwNameLen, DWORD dwType, PBYTE pData, DWORD dwLen);
    BOOL SetAValue(LPCTSTR szRegPath, LPCTSTR szValName, DWORD dwType, DWORD cbSize, PBYTE pData);
    BOOL GetAValue(LPCTSTR szRegPath, LPCTSTR szValName, DWORD& dwType, DWORD& cbSize, PBYTE pData);

// Static helper functions
    static BOOL SetupOneKey(REGINI* pRegIni, int iElement, LPCTSTR lpszKeyPath);

private:
    HKEY    m_hMainKey;
    BOOL InsertTail(PREG_KEY_INFO pKeyInfo, PREG_VALUE pRegVal);

    BOOL MultiSZtoDataList(PDWORD pdwDataList, PBYTE pData, DWORD dwLen);
    BOOL SetAnItem(HKEY hKey, LPCTSTR szValName, DWORD dwType, U_ITEM u, DWORD dwSize);
    BOOL SetSingleVal(HKEY hKey, LPCTSTR szValName, DWORD dwData);
    BOOL SetValList(HKEY hKey, LPCTSTR szValName, DWORD dwNum, PDWORD dwDataList);
    LONG  EnumAValue(HKEY  hKey, DWORD dwIndex, PREG_KEY_INFO pKInfo);
};


#endif  


