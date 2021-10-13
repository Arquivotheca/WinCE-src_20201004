//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#include "credtest.h"

// Doesn't have 
DWORD g_dwTypeList[]={
	CRED_TYPE_NTLM, 	 					 		
	CRED_TYPE_KERBEROS,
	CRED_TYPE_DOMAIN_PASSWORD,
	CRED_TYPE_CERTIFICATE,				   		
	CRED_TYPE_PLAINTEXT_PASSWORD,
	CRED_TYPE_GENERIC 			
};

PTSTR g_dwTypeInStr[]={
	TEXT("65538"), 						 		
	TEXT("65540"),		
	TEXT("65537"),		
	TEXT("65544"), 				   		
	TEXT("65542"), 	
	TEXT("65546") 	 			
};
PTSTR g_dwTypeStr[]={
	TEXT("CRED_TYPE_NTLM"), 						 		
	TEXT("CRED_TYPE_KERBEROS"),		
	TEXT("CRED_TYPE_DOMAIN_PASSWORD"),		
	TEXT("CRED_TYPE_CERTIFICATE"), 				   		
	TEXT("CRED_TYPE_PLAINTEXT_PASSWORD"), 	
	TEXT("CRED_TYPE_GENERIC") 	 			
};

DWORD g_dwFlags[]={
	CRED_FLAG_PERSIST,
	CRED_FLAG_DEFAULT,
	CRED_FLAG_SENSITIVE,
	CRED_FLAG_TRUSTED
};

PTSTR g_dwFlagsStr[]={
	TEXT("CRED_FLAG_PERSIST"), 	
	TEXT("CRED_FLAG_DEFAULT"), 	
	TEXT("CRED_FLAG_SENSITIVE"), 	
	TEXT("CRED_FLAG_TRUSTED")
};

DWORD g_dwOSPrimTypeList[MAX_USER_PRIM_CREDS]={0};		
DWORD g_dwUserPrimTypeList[MAX_USER_PRIM_CREDS]={0};		
DWORD g_dwAllPrimTypeList[MAX_USER_PRIM_CREDS*2]={0};	 // Max of 10 OS & 10 user creds Total 20

DWORD g_dwOSVirtTypeList[MAX_USER_VIRT_CREDS]={0};		
DWORD g_dwUserVirtTypeList[MAX_USER_VIRT_CREDS]={0};		
DWORD g_dwAllVirtTypeList[MAX_USER_VIRT_CREDS*2]={0};		// Max of 5 OS & 5 user creds Total 10

DWORD g_dwPrimCredTypes=0;
DWORD g_dwVirtCredTypes=0;
DWORD g_dwAllCredTypes=0;

DWORD g_dwAllCredTypeList[MAX_CREDS]={0};	 // Max of 20 OS & 10 user creds Total 30


TCHAR 	g_szUser[CRED_MAX_USER_LEN] = TEXT("SomeUser");
TCHAR 	g_szTarget[CRED_MAX_TARGET_LEN] = TEXT("SomeTarget");
TCHAR 	g_Blob[MAX_PATH] = TEXT("OriginalPassword");


BOOL ConvertStrToTypeStr(LPTSTR szKeyName, LPTSTR szType){
	DWORD dwNum=0;
	BOOL fRet=FALSE;
	for(dwNum=0; dwNum < countof(g_dwTypeInStr); dwNum++){
		if(0 == _tcsicmp(g_dwTypeInStr[dwNum], szKeyName)){
			_tcscpy(szType, g_dwTypeStr[dwNum]);
			fRet= TRUE;
		}
	}	

	return fRet;
}

DWORD DisplayCredentialTypes(PDWORD pCredTypeTable){
	DWORD dwNumTypes=0;
	DWORD dwType=0;

	while(dwType = (pCredTypeTable[dwNumTypes++])){
		LOG(TEXT("%0x\n"), dwType); 
	}
	return dwNumTypes;
}


BOOL IsOSCredType(DWORD dwTempType){

	if(( 0x00010000 < dwTempType) && ( dwTempType < 0xFFFFFFFF )){
		return TRUE;
	}
	return FALSE;
}

DWORD CreatePrimitiveProviderList(){
	TCHAR szKeyName[MAX_PATH]=TEXT("");
	TCHAR szCredProvName[MAX_PATH]=TEXT("");
	DWORD cbKeyName=MAX_PATH;
	DWORD cbCredProvName =  MAX_PATH;
	LONG lRet=0;
	HKEY hKey = NULL, hKeyProv=NULL;
	TCHAR szCredNameFull[MAX_PATH]=TEXT("");
	TCHAR szType[MAX_PATH]=TEXT("");
	LPTSTR szStrToken=NULL;
	DWORD dwUserPrimCredTypes=0, dwOSPrimCredTypes=0, dwTotalPrimCredTypes=0;
	DWORD dwTempType=0;
	// List Primitive Credential Providers
	lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, HKEY_CRED_PROV_PRIMITIVE, 0, 0, &hKey);
	if (lRet != ERROR_SUCCESS)
	{
		LOG(TEXT("Can not open Key %s"), HKEY_CRED_PROV_PRIMITIVE);
		goto Error;
	}
	LOG(TEXT("Opened HKEY_CRED_PROV_PRIMITIVE [%s]"), HKEY_CRED_PROV_PRIMITIVE);

	// Enumerate all installed Primitive Providers
	lRet = ERROR_SUCCESS;
	while (lRet == ERROR_SUCCESS)
	{
		cbKeyName=MAX_PATH;	
		cbCredProvName=MAX_PATH;	
		_tcscpy(szCredNameFull, HKEY_CRED_PROV_PRIMITIVE);
		_tcscat(szCredNameFull, TEXT("\\"));

		lRet = RegEnumKeyEx(hKey, dwTotalPrimCredTypes, szKeyName, &cbKeyName, 0, 0, 0, NULL);
		if (lRet == ERROR_SUCCESS){
			dwTempType =  _wtoi(szKeyName);
			if(IsOSCredType(dwTempType) ){
				g_dwOSPrimTypeList [dwOSPrimCredTypes]= dwTempType;
				dwOSPrimCredTypes++;
			}else{
				g_dwUserPrimTypeList [dwUserPrimCredTypes]= dwTempType;
				dwUserPrimCredTypes++;				
			}

			// Add to total creds table
			g_dwAllPrimTypeList[dwTotalPrimCredTypes] = dwTempType;
			dwTotalPrimCredTypes++;			
		}

	}

Error:
	// cleanup here
	if (hKey)
		RegCloseKey(hKey);


	return dwTotalPrimCredTypes;	
}
DWORD CreateVirtualProviderList(){
	DWORD dwRetVal = FALSE;
	TCHAR szKeyName[MAX_PATH]=TEXT("");
	TCHAR szCredProvName[MAX_PATH]=TEXT("");
	DWORD cbKeyName=MAX_PATH;
	DWORD cbCredProvName =  MAX_PATH;
	LONG lRet=0;
  	HKEY hKey = NULL, hKeyProv=NULL;
	TCHAR szCredNameFull[MAX_PATH]=TEXT("");
	TCHAR szType[MAX_PATH]=TEXT("");
	LPTSTR szStrToken=NULL;
	DWORD dwTotalVirtCredTypes=0, dwUserVirtCredTypes=0, dwOSVirtCredTypes=0, dwTempType=0;

// List Virtual Credential Providers
    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, HKEY_CRED_PROV_VIRTUAL, 0, 0, &hKey);
    if (lRet != ERROR_SUCCESS)
    {
        LOG(TEXT("Can not open Key %s"), HKEY_CRED_PROV_VIRTUAL);
        goto Error;
    }
	LOG(TEXT("Opened HKEY_CRED_PROV_VIRTUAL [%s]"), HKEY_CRED_PROV_VIRTUAL);

    // Enumerate all installed virtual providers Providers
    lRet = ERROR_SUCCESS;
	while (lRet == ERROR_SUCCESS)
	{
		cbKeyName=MAX_PATH;	
		cbCredProvName=MAX_PATH;	
		_tcscpy(szCredNameFull, HKEY_CRED_PROV_VIRTUAL);
		_tcscat(szCredNameFull, TEXT("\\"));

		lRet = RegEnumKeyEx(hKey, dwTotalVirtCredTypes, szKeyName, &cbKeyName, 0, 0, 0, NULL);
		if (lRet == ERROR_SUCCESS){
			dwTempType =  _wtoi(szKeyName);
			if(IsOSCredType(dwTempType) ){
			//	LOG(TEXT("Found [%u]: %s"), dwOSVirtCredTypes, szKeyName);
				g_dwOSVirtTypeList [dwOSVirtCredTypes]= dwTempType;
				dwOSVirtCredTypes++;
			}else{
			//	LOG(TEXT("Found [%u]: %s"), dwUserVirtCredTypes, szKeyName);
				g_dwUserVirtTypeList [dwUserVirtCredTypes]= dwTempType;
				dwUserVirtCredTypes++;				
			}

			// Add to total creds table
			g_dwAllVirtTypeList[dwTotalVirtCredTypes] = dwTempType;
			dwTotalVirtCredTypes++;			
		}
		
	}

Error:
    // cleanup here
    if (hKey)
       RegCloseKey(hKey);
    return dwTotalVirtCredTypes;
}


DWORD CreateAllCredentialTypeList(){
	DWORD dwPrimTypes=0;
	DWORD dwVirtTypes=0;
	DWORD dwAllTypes=0, dwIdx=0;

// Get all primitive types
	dwPrimTypes = CreatePrimitiveProviderList();

// Get all virtual types
	dwVirtTypes = CreateVirtualProviderList();

// Combine them !!
	dwIdx=0;
	while(dwIdx < dwPrimTypes ){
		g_dwAllCredTypeList[dwAllTypes++]= g_dwAllPrimTypeList[dwIdx++];		
	}

	dwIdx=0;
	while(dwIdx < dwVirtTypes){
		g_dwAllCredTypeList[dwAllTypes++]= g_dwAllVirtTypeList[dwIdx++];		
	}
	
	g_dwAllCredTypes = dwAllTypes;
	return dwAllTypes;
}


void FillCredStruct(PCRED pCredential, DWORD dwType, PTSTR szUser, PTSTR szTarget, 
					PBYTE pBlob, DWORD dwSize, DWORD dwFlags){

	
	pCredential->dwVersion=CRED_VER_1;
	pCredential->dwType=dwType;
	pCredential->wszUser = szUser;
	pCredential->dwUserLen = _tcslen(szUser)+1;
	pCredential->wszTarget = szTarget;
	pCredential->dwTargetLen= _tcslen(szTarget)+1;
	pCredential->pBlob=pBlob;
	pCredential->dwBlobSize=dwSize;
	pCredential->dwFlags=dwFlags;
}

BOOL CreateCredentials(DWORD dwType, PTSTR szUser, PTSTR szTarget, PBYTE pBlob, DWORD dwSize, DWORD dwFlags){
	CRED Cred;
	BOOL fRet=FALSE;
	DWORD dwError=ERROR_SUCCESS;
	
	ZeroMemory(&Cred, sizeof(Cred));
	FillCredStruct(&Cred, dwType, szUser, szTarget,  pBlob, dwSize, dwFlags);

	dwError = CredWrite(&Cred, 0);
	if(ERROR_SUCCESS == dwError){
		fRet = TRUE;
	}else{
		LOG(TEXT("CredWrite failed Error [%08x]"), dwError);
	}
	
	return fRet;
}


BOOL  VerifyCredential(PCRED pCredential, DWORD dwType, PTSTR szUser, PTSTR szTarget, PBYTE pBlob, DWORD dwFlags){
	BOOL  fRet=TRUE;

	if(CRED_VER_1 != pCredential->dwVersion){
		LOG(TEXT("pCredential->dwVersion did not match"));
		LOG(TEXT("pCredential->dwVersion = [%08x], dwVersion = [%08x]"), pCredential->dwVersion, CRED_VER_1);
		goto ErrorExit;		
	}
	
	if(pCredential->dwType !=dwType){
		LOG(TEXT("pCredential->dwType did not match"));
		LOG(TEXT("pCredential->dwType = [%08x], dwType = [%08x]"), pCredential->dwType, dwType);
		goto ErrorExit;		
	}

	if(0 != _tcscmp(pCredential->wszUser, szUser)){
		LOG(TEXT("pCredential->wszUser did not match"));
		LOG(TEXT("Expected user's len [%u], but got [%u]"), _tcslen(szUser), _tcslen(pCredential->wszUser));
		LOG(TEXT("Expected [%s], but got [%s]"), szUser, pCredential->wszUser);
		goto ErrorExit;		
	}

	if(0 != _tcscmp(pCredential->wszTarget, szTarget)){
		LOG(TEXT("pCredential->wszTarget did not match"));
		LOG(TEXT("Expected [%s], but got [%s]"), szTarget, pCredential->wszTarget);
		goto ErrorExit;		
	}

	if(!pBlob){
		if(NULL != pCredential->pBlob){
		LOG(TEXT("pCredential->pvBlob is NULL"));
			goto ErrorExit;		
		}
	}

	if(!pCredential->dwFlags && !dwFlags){

	}else{
		if(!(pCredential->dwFlags & dwFlags)){
			LOG(TEXT("pCredential->dwFlags did not match"));
			LOG(TEXT("pCredential->dwFlags = [%08x], dwFlags = [%08x]"), pCredential->dwFlags, dwFlags);
			goto ErrorExit;		
		}
	}
CommonExit:

	return fRet;
ErrorExit: 
	fRet= FALSE;
	goto CommonExit;

}

BOOL InitCredTypes(DWORD dwHowToInit){
	DWORD dwPrimCredTypes=0, dwVirtCredTypes=0, dwAllCredTypes=0;
	BOOL fRetVal=TRUE;

	switch(dwHowToInit){
	case CRED_TYPES_PRIMITIVE:
	// 	Creating List of OS Primitive providers
		LOG(TEXT("\n\n Creating List of Primitive providers"));
		dwPrimCredTypes = CreatePrimitiveProviderList();
		if(0 == dwPrimCredTypes){
			LOG(TEXT("CreatePrimitiveProviderList found no Primitive providers"));
			goto Error;
		}else{
			LOG(TEXT("CreatePrimitiveProviderList found [%u] Primitive providers"), dwPrimCredTypes);
			// Set up the global variable
			g_dwPrimCredTypes = dwPrimCredTypes;		
		}
		break;

	case CRED_TYPES_VIRTUAL:	
	// 	Creating List of OS Virtual providers
		LOG(TEXT("\n\n Creating List of Virtual providers"));
		dwVirtCredTypes = CreateVirtualProviderList();
		if(0 == dwVirtCredTypes){
			LOG(TEXT("CreateVirtualProviderList found no Virtual providers"));
			goto Error;
		}else{
			LOG(TEXT("CreateVirtualProviderList  found [%u] Virtual providers"), dwVirtCredTypes);
			// Set up the global variable
			g_dwVirtCredTypes = dwVirtCredTypes;
			
		}
		break;
 
	case CRED_TYPES_ALL:	
	// 	Creating List of  all credential types
		LOG(TEXT("\n\n Creating List of Virtual providers"));
		dwAllCredTypes = CreateAllCredentialTypeList();
		if(0 == dwAllCredTypes){
			LOG(TEXT("CreateAllCredentialTypeList found no providers"));
			goto Error;
		}else{
			LOG(TEXT("CreateAllCredentialTypeList  found [%u] providers"), dwAllCredTypes);
			// Set up the global variable
			g_dwAllCredTypes = dwAllCredTypes;
			
		}
		break;
	
	default:
		LOG(TEXT("unknown value for dwHowToInit. Cannot initialize credential types. Exiting ..."));
	}
CommonExit:
	return fRetVal;
Error:
	fRetVal = FALSE;
    	goto CommonExit;	
}

BOOL AllValidTest(){
	DWORD dwNumCredsToInit=0, dwResult=0, dwReadFlags=0, dwFlagsToCheck=0;
	TCHAR szTarget[CRED_MAX_TARGET_LEN]=TEXT("SomeTarget");
	TCHAR szUser[CRED_MAX_USER_LEN]=TEXT("SomeUser");
	TCHAR szPassword[MAX_PATH]=TEXT("OriginalPassword");
	TCHAR szChangedPwd[MAX_PATH]=TEXT("ChangedPwd");
	DWORD dwSize=0, dwIdx=0, dwType=0, dwFlags=0, dwTargetLen=0, dwFlagIdx=0;
	PCRED pCredential=NULL;
	BOOL fRet=TRUE;
	DWORD dwErrorCount=0, dwInfoToUpdate=0;
		
// Write
// Read - verify
// Change blob
// Write
// Read - verify
// Delete
// Read - verify

// Write
// Read - verify
	for(dwIdx=0; dwIdx< g_dwAllCredTypes; dwIdx++){
		dwType = g_dwAllCredTypeList[dwIdx];
		_tcscpy(szUser, g_szUser);
		_tcscpy(szTarget, g_szTarget);
		dwTargetLen =  _tcslen(szTarget)+1; // for terminating NULL;This size in in # of chars

		_tcscpy(szPassword, (LPTSTR)g_Blob);
		dwSize = (_tcslen(szPassword)+1)* sizeof(TCHAR);

		// For each flag 
		for(dwFlagIdx=0; dwFlagIdx < countof(g_dwFlags); dwFlagIdx++){
			dwFlags = g_dwFlags[dwFlagIdx];
			LOG(TEXT("Creating using [%s] flag"), g_dwFlagsStr[dwFlagIdx]);
			// Write
			if(!CreateCredentials(dwType, szUser,  szTarget,
								(PBYTE)szPassword, dwSize, dwFlags)){
				LOG(TEXT("FAILFAIL:: CreateCredentials failed "));
				// Print the working set
				dwReadFlags =0;
				LOG( TEXT(" Type [%u]"), dwType);
				LOG( TEXT(" User [%s]"), szUser);
				LOG( TEXT(" Tgt [%s]"), szTarget);
				LOG( TEXT(" Blob [%s]"), szPassword);
				LOG( TEXT(" Blob size [%u]"), dwSize);
				LOG( TEXT(" Flags size [%u]"), dwFlags);
				
				goto ErrorExit;		
			}
			LOG(TEXT(""));
			LOG(TEXT("Created the credential successfully"));
			
			// Read - verify
			dwReadFlags =0;
			pCredential=NULL;
			dwResult = CredRead(szTarget, dwTargetLen, dwType, dwReadFlags, &pCredential);
			if(ERROR_SUCCESS !=  dwResult){
				LOG( TEXT("FAILFAIL:: CredRead failed [%08x]"), dwResult);
				// Print the working set
				LOG(TEXT(" NOT Found this cred when reading with following parameters"));
				goto ErrorExit;				
			}

			dwFlagsToCheck = dwFlags;
			if(dwFlagsToCheck & CRED_FLAG_DEFAULT){
				dwFlagsToCheck ^=CRED_FLAG_DEFAULT; 
			}

#ifdef SSPs_TRUSTED
			if((CRED_TYPE_NTLM == dwType) ||(CRED_TYPE_KERBEROS ==dwType) || 
					(CRED_TYPE_DOMAIN_PASSWORD ==dwType ) ){ // Trusted creds
				dwFlagsToCheck |=CRED_FLAG_TRUSTED;
			}
#endif		
			if(!VerifyCredential(pCredential, dwType, szUser, szTarget, (PBYTE)szPassword, dwFlagsToCheck)){
				LOG(TEXT("VerifyCredential failed"));
				goto ErrorExit;
			}
			LOG(TEXT("VerifyCredential succeeded"));
			

			// Change the password 

			_tcscpy(szPassword, szChangedPwd);
			dwSize = (_tcslen(szPassword)+1)* sizeof(TCHAR);

		// Write (Update it
		/*
			if(!CreateCredentials(dwType, szUser,  szTarget,
								(PBYTE)szPassword, dwSize, dwFlags)){
			*/
			dwInfoToUpdate = CRED_FLAG_UPDATE_BLOB;

			pCredential->pBlob = (PBYTE)szPassword;
			pCredential->dwBlobSize = dwSize;

			dwResult = CredUpdate(szTarget, dwTargetLen, dwType, pCredential, dwInfoToUpdate);
			if(ERROR_SUCCESS != dwResult){


				LOG(TEXT("FAILFAIL:: CreateCredentials failed when Changing blob"));
				// Print the working set
				dwReadFlags =0;
				LOG( TEXT(" User [%s]"), szUser);
				LOG( TEXT(" Tgt [%s]"), szTarget);
								
			}
			LOG(TEXT("Updated the credential's blob successfully"));
			CredFree((PBYTE)pCredential);

		// Read - verify
			dwReadFlags =0;
			pCredential=NULL;

			dwResult = CredRead(szTarget, dwTargetLen, dwType, dwReadFlags, &pCredential);
			if(ERROR_SUCCESS !=  dwResult){
				LOG( TEXT(" CredRead failed [%08x]"), dwResult);
				goto ErrorExit;				
			}

			dwFlagsToCheck = dwFlags;
			if(dwFlagsToCheck & CRED_FLAG_DEFAULT){
				dwFlagsToCheck ^=CRED_FLAG_DEFAULT; 
			}

#ifdef SSPs_TRUSTED
			if((CRED_TYPE_NTLM == dwType) ||(CRED_TYPE_KERBEROS ==dwType) || 
					(CRED_TYPE_DOMAIN_PASSWORD ==dwType ) ){ // Trusted creds
				dwFlagsToCheck |=CRED_FLAG_TRUSTED;
			}
#endif		
			if(!VerifyCredential(pCredential, dwType, szUser, szTarget, (PBYTE)szPassword, dwFlagsToCheck)){
				LOG(TEXT("VerifyCredential failed"));
				goto ErrorExit;
			}
			LOG(TEXT("VerifyCredential succeeded"));

		// Delete

			dwResult = CredDelete(szTarget, dwTargetLen, dwType, 0);
			if(ERROR_SUCCESS !=  dwResult){
				LOG(TEXT("FAILFAIL:: CredDelete failed [%08x]"), dwResult);
				// Print the working set
				LOG(TEXT(" Found this cred when deleting with following parameters"));
				dwReadFlags =0;
				dwErrorCount++;
				continue;
			}else{
					LOG(TEXT("Deleted the cred SUCCESSFULLY"));		
			}

	// Read - verify
			dwReadFlags =0;
			
			pCredential = NULL;
			dwReadFlags= CRED_FLAG_NO_DEFAULT | CRED_FLAG_NO_IMPLICIT_DEFAULT;	
			dwResult = CredRead(szTarget, dwTargetLen, dwType, dwReadFlags, &pCredential);
			if(ERROR_SUCCESS ==  dwResult){
				LOG( TEXT("FAILFAIL::  CredRead passed, but should have failed"));
				// Print the working set
				LOG(TEXT(" Found this cred when reading with following parameters"));
				CredFree((PBYTE)pCredential);
			}
			LOG(TEXT("Did not find the credential as expected, because it was deleted previously"));
		}
	}
CommonExit:

	if(dwErrorCount){
		fRet=FALSE;	
	}

	return fRet;
	
ErrorExit:
	LOG(TEXT("FAILFAIL::"));
	fRet=FALSE;
	goto CommonExit;
	

}

BOOL ListPrimitiveProviders(){
	TCHAR szKeyName[MAX_PATH]=TEXT("");
	TCHAR szCredProvName[MAX_PATH]=TEXT("");
	DWORD dwIdx=0, cbKeyName=MAX_PATH;
	DWORD cbCredProvName =  MAX_PATH;
	LONG lRet=0;
  	HKEY hKey = NULL, hKeyProv=NULL;
	TCHAR szCredNameFull[MAX_PATH]=TEXT("");
	TCHAR szType[MAX_PATH]=TEXT("");
	LPTSTR szStrToken=NULL;
	BOOL dwRetVal=FALSE;
	
	// List Primitive Credential Providers
    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, HKEY_CRED_PROV_PRIMITIVE, 0, 0, &hKey);
    if (lRet != ERROR_SUCCESS)
    {
        LOG(TEXT("Can not open Key %s"), HKEY_CRED_PROV_PRIMITIVE);
        goto Error;
    }
	LOG(TEXT("Opened HKEY_CRED_PROV_PRIMITIVE [%s]"), HKEY_CRED_PROV_PRIMITIVE);

    // Enumerate all installed Primitive  Providers
    lRet = ERROR_SUCCESS;
    while (lRet == ERROR_SUCCESS)
    {
 	cbKeyName=MAX_PATH;	
	cbCredProvName=MAX_PATH;	
	_tcscpy(szCredNameFull, HKEY_CRED_PROV_PRIMITIVE);
	_tcscat(szCredNameFull, TEXT("\\"));

       lRet = RegEnumKeyEx(hKey, dwIdx, szKeyName, &cbKeyName, 0, 0, 0, NULL);
        if (lRet == ERROR_SUCCESS){
		LOG(TEXT(""));
		LOG(TEXT("Found Primitive type [%s]"),  szKeyName);
		_tcscat(szCredNameFull, szKeyName);

		// Open this Key
		lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szCredNameFull, 0, 0, &hKeyProv);
		if (lRet != ERROR_SUCCESS)
		{
			LOG(TEXT("Can not open Key %s. Error [%08x] "), szCredNameFull, lRet);
			goto Error;
		}

		LOG(TEXT("Opened szCredNameFull [%s]"), szCredNameFull);
		lRet = RegQueryValueEx(hKeyProv, HKEY_CRED_DLL, NULL, NULL, (BYTE*)szCredProvName, &cbCredProvName);
		if (lRet != ERROR_SUCCESS)
		{
			LOG(TEXT("Could not detect Credential Provider. Error [%08x]"), lRet);
			goto Error;
		}
		if(!ConvertStrToTypeStr(szKeyName, szType)){
			LOG(TEXT("[%s] is user defined type"), szKeyName);
			LOG(TEXT("Found Provider  [%s] for Primitive type [%s]"), szCredProvName, szKeyName);

		}else{
			LOG(TEXT("Found Provider  [%s] for Primitive type [%s]"), szCredProvName, szType);
		}
		if (hKeyProv)
		       RegCloseKey(hKeyProv);

        }
        dwIdx++;
    }
	dwRetVal=TRUE;
Error:
    // cleanup here
    if (hKey)
       RegCloseKey(hKey);
	
    return dwRetVal;
}
BOOL ListVirtualProviders(){
	DWORD dwRetVal = FALSE;
	TCHAR szKeyName[MAX_PATH]=TEXT("");
	TCHAR szCredProvName[MAX_PATH]=TEXT("");
	DWORD dwIdx=0, cbKeyName=MAX_PATH;
	DWORD cbCredProvName =  MAX_PATH;
	LONG lRet=0;
  	HKEY hKey = NULL, hKeyProv=NULL;
	TCHAR szCredNameFull[MAX_PATH]=TEXT("");
	TCHAR szType[MAX_PATH]=TEXT("");
	LPTSTR szStrToken=NULL;

// List Virtual Credential Providers
    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, HKEY_CRED_PROV_VIRTUAL, 0, 0, &hKey);
    if (lRet != ERROR_SUCCESS)
    {
        LOG(TEXT("Can not open Key %s"), HKEY_CRED_PROV_VIRTUAL);
        goto Error;
    }
	LOG(TEXT("Opened HKEY_CRED_PROV_VIRTUAL [%s]"), HKEY_CRED_PROV_VIRTUAL);

    // Enumerate all installed virtual providers Providers
    lRet = ERROR_SUCCESS;
    while (lRet == ERROR_SUCCESS)
    {
 	cbKeyName=MAX_PATH;	
	cbCredProvName=MAX_PATH;	
	_tcscpy(szCredNameFull, HKEY_CRED_PROV_VIRTUAL);
	_tcscat(szCredNameFull, TEXT("\\"));

       lRet = RegEnumKeyEx(hKey, dwIdx, szKeyName, &cbKeyName, 0, 0, 0, NULL);
        if (lRet == ERROR_SUCCESS){
		LOG(TEXT(""));
		LOG(TEXT("Found Virtual type  [%s]"), szKeyName);
		_tcscat(szCredNameFull, szKeyName);

		// Open this Key
		lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szCredNameFull, 0, 0, &hKeyProv);
		if (lRet != ERROR_SUCCESS)
		{
			LOG(TEXT("Can not open Key %s. Error [%08x]"), szCredNameFull, lRet);
			goto Error;
		}

		LOG(TEXT("Opened szCredNameFull [%s]"), szCredNameFull);
		
		// Found the value for ids in the format ids = 123, 456, 678
		// Parse each ID and display the type

		lRet = RegQueryValueEx(hKeyProv, HKEY_CRED_V_ID, NULL, NULL, (BYTE*)szCredProvName, &cbCredProvName);
		if (lRet != ERROR_SUCCESS)
		{
			LOG(TEXT("Could not detect Credential Provider. Error [%08x]"), lRet);
			goto Error;
		}
		if(!ConvertStrToTypeStr(szKeyName, szType)){
			LOG(TEXT("[%s] is user defined type"), szKeyName);
			LOG(TEXT("Found   Provider  [%s] for Virtual type [%s]"), szCredProvName, szKeyName);

		}else{
			LOG(TEXT("Found  Primitive types  [%s] for Virtual type [%s]"), szCredProvName, szType);
			LOG(TEXT("Primitive  for this Virtual type are:"));
		}
		
		
		szStrToken = _tcstok(szCredProvName, TEXT(","));

		while(NULL != szStrToken){
			if(ConvertStrToTypeStr(szStrToken, szType)){
				
				LOG(TEXT("Type %s\n"), szType);
			}
			szStrToken = _tcstok(NULL, TEXT(","));
        	}

        }
        dwIdx++;
    }
	dwRetVal=TRUE;
Error:
    // cleanup here
    if (hKey)
       RegCloseKey(hKey);
    if (hKeyProv)
       RegCloseKey(hKeyProv);
    return dwRetVal;

}
TESTPROCAPI ListCredProvPrimitiveTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	DWORD dwRetVal = TPR_FAIL;

   // process incoming tux messages
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // return the thread count that should be used to run this test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return TPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;
    

    // Test case specific setup here
    switch(lpFTE->dwUserData)
    {
    case PR_ALL:
        break;
        
    
    default:
        g_pKato->Log(LOG_FAIL, TEXT("Unknown Test Case %d (Should never be here)\n"), lpFTE->dwUserData);
        goto Error;
    }

	LOG(TEXT(""));
	LOG(TEXT("\n\n Listing Primitive providers"));

	if(!ListPrimitiveProviders()){
		LOG(TEXT("Error while detecting Primitive providers"));
		goto Error;
	}

   //
    //  test passed
    //
    dwRetVal = TPR_PASS;
    
Error:
    // cleanup here
     return dwRetVal;
}

TESTPROCAPI ListCredProvVirtualTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	DWORD dwRetVal = TPR_FAIL;

   // process incoming tux messages
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // return the thread count that should be used to run this test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return TPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;
    

    // Test case specific setup here
    switch(lpFTE->dwUserData)
    {
    case PR_ALL:
        break;
        
    
    default:
        g_pKato->Log(LOG_FAIL, TEXT("Unknown Test Case %d (Should never be here)\n"), lpFTE->dwUserData);
        goto Error;
    }


	LOG(TEXT(""));
	LOG(TEXT("\n\n Listing Virtual providers"));
	if(!ListVirtualProviders()){
		LOG(TEXT("Error while detecting Virtual providers"));
		goto Error;
	}

   //
    //  test passed
    //
    dwRetVal = TPR_PASS;
    
Error:
    // cleanup here
     return dwRetVal;
}

TESTPROCAPI CreateReadUpdateDeleteTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	DWORD dwRetVal = TPR_FAIL;
   // process incoming tux messages
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // return the thread count that should be used to run this test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return TPR_HANDLED;
    }
    else if( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;
    

    // Test case specific setup here
    switch(lpFTE->dwUserData)
    {
    case CD_ALL:
        break;
        
    
    default:
        g_pKato->Log(LOG_FAIL, TEXT("Unknown Test Case %d (Should never be here)\n"), lpFTE->dwUserData);
        goto Error;
    }
	
	if(!InitCredTypes(CRED_TYPES_ALL)){
	LOG(TEXT("Failed to initialize credential types from the registry. Verify that the registry entries are correct\n")); 
		goto Error;
	}
	if(AllValidTest()){	
		dwRetVal = TPR_PASS;
	}

Error:
	return dwRetVal;

}


