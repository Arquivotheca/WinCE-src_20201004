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
    CRED_FLAG_DEFAULT
};

PTSTR g_dwFlagsStr[]={
    TEXT("CRED_FLAG_PERSIST"),     
    TEXT("CRED_FLAG_DEFAULT")
};

DWORD g_dwOSPrimTypeList[MAX_USER_PRIM_CREDS]={0};        
DWORD g_dwUserPrimTypeList[MAX_USER_PRIM_CREDS]={0};        
DWORD g_dwAllPrimTypeList[MAX_USER_PRIM_CREDS*2]={0};     // Max of 10 OS & 10 user creds Total 20

DWORD g_dwOSVirtTypeList[MAX_USER_VIRT_CREDS]={0};        
DWORD g_dwUserVirtTypeList[MAX_USER_VIRT_CREDS]={0};        
DWORD g_dwAllVirtTypeList[MAX_USER_VIRT_CREDS*2]={0};        // Max of 5 OS & 5 user creds Total 10

DWORD g_dwPrimCredTypes=0;
DWORD g_dwVirtCredTypes=0;
DWORD g_dwAllCredTypes=0;

DWORD g_dwAllCredTypeList[MAX_CREDS]={0};     // Max of 20 OS & 10 user creds Total 30


TCHAR     g_szUser[CRED_MAX_USER_LEN] = TEXT("SomeUser");
TCHAR     g_szTarget[CRED_MAX_TARGET_LEN] = TEXT("SomeTarget");
TCHAR     g_Blob[MAX_PATH] = TEXT("OriginalPassword");


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
            
                g_dwOSVirtTypeList [dwOSVirtCredTypes]= dwTempType;
                dwOSVirtCredTypes++;
            }else{
           
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

BOOL VerifyCredential(PCRED pCredential, DWORD dwType, PTSTR szUser, PTSTR szTarget, PBYTE pBlob, DWORD dwFlags){
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
    //     Creating List of OS Primitive providers
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
    //     Creating List of OS Virtual providers
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
    //     Creating List of  all credential types
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
