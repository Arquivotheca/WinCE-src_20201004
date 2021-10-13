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
	regmani.h

Abstract:

    Declare a registry manipulation class

--*/

#ifndef __REGMANI_H_
#define __REGMANI_H_
#include <devload.h>
#include <pcibus.h>

#define  NKMSG NKDbgPrintfW 

#define REG_BINARY_LENGTH	128
#define REG_STRING_LENGTH	256
#define REG_MULTIPLE_DATA_NUM	16
#define REG_MULTIPLE_STRING_NUM	4
#define REG_MULTIPLE_STRING_LENGTH  32 
#define REG_ITEM_NUM	32

//union struct holds for each value item under a reg key
typedef union _U_ITEM{
	BYTE	binData[	REG_BINARY_LENGTH]; //REG_BINARY
	DWORD	dwData;	//REG_DWORD
	TCHAR	szData[REG_STRING_LENGTH];	//REG_SZ
	DWORD	dwDataList[REG_MULTIPLE_DATA_NUM+1];  //REG_MULTI_SZ, contents are dword type data, first dword hold the info of how many values 
	TCHAR	szDataList[REG_MULTIPLE_STRING_NUM][REG_MULTIPLE_STRING_LENGTH]; //REG_MULTI_SZ, contents are strings. seldom used
}U_ITEM;


//structure that holds the content of a whole reg key
typedef struct _REG_KEY_INFO{
	TCHAR	szRegPath[REG_STRING_LENGTH]; 
	UINT	uItems;		//numbers of values under this key
	TCHAR	szValName[REG_ITEM_NUM][REG_STRING_LENGTH];
	DWORD	dwType[REG_ITEM_NUM]; //types
	DWORD	dwSize[REG_ITEM_NUM]; //size in bytes of each item
	U_ITEM 	u[REG_ITEM_NUM];	//values
	UINT	uBitmap;	//bitmap to indicate which types/values are valid
}REG_KEY_INFO, *PREG_KEY_INFO;


class CRegManipulate {
public:
    CRegManipulate(HKEY hMainKey):m_hMainKey(hMainKey){;};
    ~CRegManipulate(){;};
    BOOL GetAKey(PREG_KEY_INFO pKeyInfo);
    BOOL SetAKey(PREG_KEY_INFO pKeyInfo, BOOL bNew);
    BOOL ModifyKeyValue(PREG_KEY_INFO pKeyInfo, DWORD dwIndex, DWORD dwType, DWORD dwSize, U_ITEM u);
    BOOL AddKeyValue(PREG_KEY_INFO pKeyInfo, LPCTSTR szValName, DWORD dwType, DWORD dwSize, U_ITEM u);
    BOOL DeleteKeyValue(PREG_KEY_INFO pKeyInfo, DWORD dwIndex);
    DWORD FindKeyValue(PREG_KEY_INFO pKeyInfo, LPCTSTR szValName);
    BOOL OpenKey(LPCTSTR szPath, PHKEY phKey);
    BOOL CopyAKey(LPCTSTR szDestPath, LPCTSTR szSrcPath);
    BOOL DeleteAKey(LPCTSTR szPath);
    BOOL IsAKeyValidate(LPCTSTR szPath);
    BOOL SetSingleVal(HKEY hKey, LPCTSTR szValName, DWORD dwData);
    BOOL SetValList(HKEY hKey, LPCTSTR szValName, DWORD dwNum, PDWORD dwDataList);

private:

    HKEY	m_hMainKey;

    BOOL StoreAnItem(PREG_KEY_INFO pKeyInfo, LPCTSTR szValName, DWORD dwNameLen, DWORD dwIndex, DWORD dwType, PBYTE pData, DWORD dwLen);
    BOOL MultiSZtoDataList(PDWORD pdwDataList, PBYTE pData, DWORD dwLen);
    BOOL SetAnItem(HKEY hKey, LPCTSTR szValName, DWORD dwType, U_ITEM u, DWORD dwSize);
    LONG  EnumAValue(HKEY  hKey, DWORD dwIndex, PREG_KEY_INFO pKInfo);
    
};

#endif
