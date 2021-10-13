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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module: test.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include "inputlang.h"

//******************************************************************************
#define HELP _T("?")

static TCHAR *szNULLStr=_T("\0");

BOOL GetKeyboardHandleFromInputLocaleID (HKL hInputLocale, HKL *phKeyboardLayoutHandle)
{
    HKEY hKey = NULL;
    TCHAR szRegKey[_countof (LAYOUT_PATH) + 8] = { NULL }; //8 is the # of TCHARs in an HKL
    TCHAR szKeyboardLayoutHandle[MAX_PATH] = { NULL };
    ULONG ulHKLSize = sizeof (szKeyboardLayoutHandle);
    BOOL fReturn = TRUE;

    StringCchPrintf (szRegKey, _countof (szRegKey), TEXT ("%s%08x"), LAYOUT_PATH, (UINT_PTR)hInputLocale);
    
    //Read value "Keyboard Layout" from HKLM\System\CurrentControlSet\Control\Layouts\<HKL>
    if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx (hKey, TEXT ("Keyboard Layout"), NULL, NULL, (LPBYTE)szKeyboardLayoutHandle, &ulHKLSize) == ERROR_SUCCESS) {
            *phKeyboardLayoutHandle = (HKL)(_tcstoul (szKeyboardLayoutHandle, &szNULLStr, 16));
        } else { LOGF_FAIL (TEXT ("RegQueryValue Ex () failed")); fReturn = FALSE; }
        RegCloseKey (hKey);
    } else { LOGF_FAIL (TEXT ("RegOpenKeyEx () failed")); fReturn = FALSE; }

    return fReturn;
}

 BOOL InitList()
{
    TCHAR szRegKey[_countof (PRELOAD_PATH) + 2] = {NULL}; // 2 is the # of TCHARS in the MAX_HKL_PRELOAD_COUNT
    HKEY hKey = 0;
    TCHAR szLayout[8 + 1] = {NULL}; //8 is the # of TCHARS in an HKL
    DWORD ulLayoutSize = sizeof (szLayout);
    BOOL fDefaultInputLocale = FALSE;

    //Query Keyboard Layout\\Preload, "Default" to get the default Input Locale Identifier
    if (RegOpenKeyEx (HKEY_CURRENT_USER, PRELOAD_PATH, 0, 0, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx (hKey, _T("Default"), NULL, NULL, (LPBYTE) szLayout, &ulLayoutSize) == ERROR_SUCCESS) {
            g_hDefaultInputLocale = (HKL)_tcstoul (szLayout, &szNULLStr, 16);
            fDefaultInputLocale = TRUE;
        } else { LOGF_DETAIL (TEXT ("RegQueryValueEx () failed")); }
        RegCloseKey (hKey);
    } else { LOGF_FAIL (TEXT ("RegOpenKeyEx () failed")); }

    for (int iPreloadNum = 1; iPreloadNum <= MAX_HKL_PRELOAD_COUNT; iPreloadNum++){
        ulLayoutSize = sizeof (szLayout);
        StringCchPrintf (szRegKey, _countof (szRegKey), TEXT ("%s%d"), PRELOAD_PATH, iPreloadNum);
        
        //query the registry and retrieve the current values for input layouts 
        if (RegOpenKeyEx(HKEY_CURRENT_USER, (LPCWSTR)szRegKey, 0,0,&hKey)==ERROR_SUCCESS){
            if(RegQueryValueEx(hKey,_T("Default"),NULL,NULL,(LPBYTE)szLayout, &ulLayoutSize)==ERROR_SUCCESS){

                g_rghInputLocale[g_cInputLocale++] = (HKL)_tcstoul(szLayout,&szNULLStr,16);

                if(fDefaultInputLocale != TRUE){
                    //find the first entry and set it as the default or current input layout
                    g_hDefaultInputLocale = g_rghInputLocale[g_cInputLocale - 1];
                    fDefaultInputLocale=TRUE;
                }
                RegCloseKey(hKey);
            } else { LOGF_FAIL (TEXT ("RegQueryValueEx () failed")); }
        } else { LOGF_DETAIL (TEXT("RegOpenKeyEx () failed")); }
    }
    return getCode ();
}

BOOL InitInputLang(){
    TCHAR szProcName[11 + 1]={NULL};
    HKEY hKey = 0;
    TCHAR szRegKey[_countof (LAYOUT_PATH) + 8] = {NULL};
    HMODULE hKbdMouseDll = 0;
    TCHAR szKbdMouseDll[MAX_PATH] = {NULL};
    ULONG ulKbdMouseDllLen = _countof (szKbdMouseDll);
    HKL hDefaultKeyboardLayout = g_hDefaultInputLocale;
    PFN_INPUT_LANGUAGE_ENTRY pfnInputLangEntry;

    if ((0xE0000000 & (UINT_PTR)g_hDefaultInputLocale)) {
        if (GetKeyboardHandleFromInputLocaleID(g_hDefaultInputLocale, &hDefaultKeyboardLayout) != TRUE) {
            LOGF_FAIL (TEXT ("GetKeyboardHandleFromInputLocalID () failed"));
        }
    }

    StringCchPrintf (szRegKey, _countof (szRegKey), TEXT ("%s%08x"), LAYOUT_PATH, hDefaultKeyboardLayout);

    if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, TEXT ("Layout File"), NULL, NULL, (LPBYTE) szKbdMouseDll, &ulKbdMouseDllLen) == ERROR_SUCCESS) {
        } else { LOGF_FAIL (TEXT ("RegQueryValueEx () failed")); }
    } else { LOGF_FAIL (TEXT ("RegOpenKeyEX () failed")); }

    //load library the keybd driver so we can use the exported IL_000kbdlocale function to retrieve the information about the keyboard
    hKbdMouseDll = LoadLibrary(szKbdMouseDll);
    if(!hKbdMouseDll){
        LOGF_FAIL (TEXT ("LoadLibrary () failed"));
        goto cleanup;
    }

    StringCchPrintf (szProcName, _countof (szProcName), TEXT ("IL_%08x"), hDefaultKeyboardLayout);
    InputLangInfo.dwSize=sizeof(INPUT_LANGUAGE); 

    //get proc address of the exported function. 
    pfnInputLangEntry=(PFN_INPUT_LANGUAGE_ENTRY)GetProcAddress(hKbdMouseDll, szProcName);

    if (!pfnInputLangEntry){
        LOGF_FAIL (TEXT ("GetProcAddress () failed"));
        goto cleanup;
    }

    //call the function and retrieve the input language information. 
    if(!pfnInputLangEntry(&InputLangInfo)){
        g_pKato->Log(LOG_FAIL,TEXT("IL_%08x () failed at %s:%d\r\n"), hDefaultKeyboardLayout,__FILE__, __LINE__);
        goto cleanup;
    }

cleanup:

    if (hKbdMouseDll)
        FreeLibrary(hKbdMouseDll);

    return getCode();
}

BOOL  ProcessCmdLine(LPCTSTR CmdLine)
{
    if (CmdLine)
        Debug(TEXT("\t\t there is no commandline needed for this test\n"));
        
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// kbdlayout
//  Gives the current layout of the system will be modified to give the layout of a the current thread
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Getkbdlayout(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{

    if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }

    HKL ilocale;
    
    //bad param test
    if (GetKeyboardLayout((DWORD)-1)){
        g_pKato->Log(LOG_FAIL,TEXT("bad param test failed for GetKeyboardLayout at %s %d"), __FILE__, __LINE__);
        goto cleanup;
        
        }
    
    //test for default keyboard layout entry 
    if(g_hDefaultInputLocale!=GetKeyboardLayout(0)){
        g_pKato->Log(LOG_FAIL,TEXT("Failed for default parameters in %s %d"), __FILE__, __LINE__);
        goto cleanup;
        
        }

    //activate each keyboard layout that was retrieved one by one do a use getkeyboardlayout    
    for (UINT i=0;i< g_cInputLocale;i++){
        if(!ActivateKeyboardLayout(g_rghInputLocale[i], 0)){
            g_pKato->Log(LOG_FAIL, TEXT("ActivateKeyboard Failed in %s %d"), __FILE__, __LINE__);
            goto cleanup;
            }

        ilocale=GetKeyboardLayout(0);
        if (ilocale!=g_rghInputLocale[i]){
            g_pKato->Log(LOG_FAIL,TEXT("GetKeyboardLayout returned an invalid value for HKL value %d in %s %d"), g_rghInputLocale[i], __FILE__, __LINE__);
            goto cleanup;
            }
    }
    //retore the orignal state 
    if(!ActivateKeyboardLayout(g_hDefaultInputLocale, 0)){
            g_pKato->Log(LOG_FAIL, TEXT("ActivateKeyboard Failed in %s %d"), __FILE__, __LINE__);
            goto cleanup;
    }

cleanup:
         return getCode();
}




//Activatekbdlayout
TESTPROCAPI Activatekbdlayout(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    if( TPM_EXECUTE != uMsg ) {
        return TPR_NOT_HANDLED;
    }

    //activate each keyboard layout and see if they are activated
    for (UINT i=0;i<g_cInputLocale;i++){
        if(!ActivateKeyboardLayout(g_rghInputLocale[i], 0)){
            g_pKato->Log(LOG_FAIL, TEXT("ActivateKeyboard Failed in %s %d"), __FILE__, __LINE__);
            goto cleanup;
        }
    }
    
    if(!ActivateKeyboardLayout(g_hDefaultInputLocale, 0)){
        g_pKato->Log(LOG_FAIL, TEXT("ActivateKeyboard Failed in %s %d"), __FILE__, __LINE__);
        goto cleanup;
    }

    cleanup: 
        return getCode();
}


//GetKbdLayoutList

TESTPROCAPI GetKbdLayoutList(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    if(TPM_EXECUTE != uMsg) {
    return TPR_NOT_HANDLED;
    }

    HKL rghInputLocale[] = {NULL}; //To avoid PreFAST warning#6387
    HKL *phInputLocale = NULL;
    UINT cInputLocale = 0;

    //get the default layout size and compare with the size of loclist
    cInputLocale = GetKeyboardLayoutList(0, rghInputLocale);

    if(cInputLocale == 0) {
        LOGF_FAIL (TEXT ("GetKeyboardLayoutList returned ZERO"));
        goto cleanup;
    }

    if(cInputLocale != g_cInputLocale) {
        LOGF_FAIL (TEXT ("GetKeyboardLayoutList returned incorrect number of Input Locales"));
        goto cleanup;
    }

    //allocate space for those HKL
    phInputLocale = (HKL*) LocalAlloc (LPTR, cInputLocale * sizeof(HKL));
    if(!phInputLocale){
        LOGF_FAIL (TEXT ("Cannot allocate for phInputLocale"));
        goto cleanup;
    }

    //get all the HKL taht the system supports
    cInputLocale = GetKeyboardLayoutList(cInputLocale, phInputLocale);
    if(cInputLocale == 0){
        LOGF_FAIL (TEXT ("GetKeyboardLayoutList returned ZERO"));
        goto cleanup;
    }

    for (UINT uiInputLocale = 0;uiInputLocale < cInputLocale;uiInputLocale++) {
    //compare the global list with the values returned by the GetKeyboardLoyoutlist function. Assumed the order placed in the registry
        if (g_rghInputLocale[uiInputLocale]!=phInputLocale[uiInputLocale]){
            g_pKato->Log(LOG_FAIL,TEXT("Registry Locale Value = 0x%08x /GetKeyboardLayoutList () Value = 0x%08x\r\n"),g_rghInputLocale[uiInputLocale], phInputLocale[uiInputLocale]);
            LOGF_FAIL (TEXT ("Incorrect Value returned by GetKeyboardLayoutList ()"));
        }
    }

cleanup:
    if(phInputLocale)
        LocalFree(phInputLocale);

return getCode();
}

//GetKbdTest
TESTPROCAPI GetKbdTest (UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    if(TPM_EXECUTE != uMsg) {
        return TPR_NOT_HANDLED;
    }

    INT iReturn = -1;

    //bad param test
    if(GetKeyboardType(-1)){
        LOGF_FAIL (TEXT ("Bad Parameter Test for GetKbdTest failed"));
        goto cleanup;
    }

    //keyboard type test
    iReturn = GetKeyboardType(KBD_TYPE);

    if((iReturn > 8) || (iReturn < 1)){ //The maximum value returned by GetKeyboardType () will be 8 for Korean (0x412). Look at \public\common\oak\drivers\keybd\keybd.reg
        LOGF_FAIL (TEXT ("GetKeyboardType (KBD_TYPE) returned invalid value"));
        goto cleanup;
    }

    //test to find the real value and validate it needs to be added 
    if(iReturn != (INT)InputLangInfo.dwType){
        g_pKato->Log(LOG_FAIL,TEXT("Keyboard Type: Expected Value = %d / Returned Value = %d\r\n"), InputLangInfo.dwType, iReturn);
        LOGF_FAIL (TEXT ("Keyboard Type Mismatch"));
        goto cleanup;
    }

    //keyboard subtype test
    iReturn = GetKeyboardType(KBD_SUBTYPE);
    if(iReturn < 0){
        LOGF_FAIL (TEXT ("GetKeyboardType (KBD_SUBTYPE) returned invalid value"));
        goto cleanup;
    }

    if(iReturn != (INT)InputLangInfo.dwSubType){
        g_pKato->Log(LOG_FAIL,TEXT("Keyboard SubType: Expected Value = %d / Returned Value = %d\r\n"), InputLangInfo.dwSubType, iReturn);
        LOGF_FAIL (TEXT ("Keyboard SubType Mismatch"));
        goto cleanup;
    }

    //keyboard number of function keys test  
    iReturn = GetKeyboardType(KBD_FUNCKEY);

    if(iReturn != InputLangInfo.bcFnKeys){
        g_pKato->Log(LOG_FAIL,TEXT("Keyboard Function Key Count: Expected Value = %d / Returned Value = %d\r\n"), InputLangInfo.bcFnKeys, iReturn);
        LOGF_FAIL (TEXT ("Keyboard Function Key Count Mismatch"));
        goto cleanup;
    }

cleanup:
    return getCode();
}

//mapvirtualKey test Converting VKEY To XT Scan Codes

//MapVKeyTest

TESTPROCAPI MapVKeyTest(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/){
    if(TPM_EXECUTE != uMsg) {
        return TPR_NOT_HANDLED;
    }

    UINT uiScanCode = 0;
    UINT uiTableScanCode = 0;
    UINT uiType = 0;

    if (InputLangInfo.pVkToScanCodeTable == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("No ScanCode Table Found"));
        goto cleanup;
    }

    //call verify VKEY to XT Scancode conversion. 
    for (UINT uiVK=0; uiVK < COUNT_VKEYS; uiVK++){
        //grab the ScanCode through MapVirtualKey
        uiScanCode=MapVirtualKey(uiVK, 0);

        //retrieve the ScanCode vs the Vkey from the table
        uiTableScanCode=InputLangInfo.pVkToScanCodeTable[uiVK].uiVkToSc;

        //find the type if its a SC_E0, put "EO" in the high byte. If it is SC_E11D, put "E11D" in the byte[3,2] of an UINT
        if ((uiType = InputLangInfo.pVkToScanCodeTable[uiVK].uiType) == SC_E0) {
            uiTableScanCode |= 0xE000;
        }else if (uiType == SC_E11D) {
            uiTableScanCode |= 0xE11D00;
        }

        if(uiScanCode != uiTableScanCode){
            g_pKato->Log(LOG_FAIL,TEXT("MapVKeyTest for VKEY 0x%04x. Expected Value = 0x%04x / Returned Value = 0x%04x\r\n"), uiVK, uiTableScanCode, uiScanCode);
            LOGF_FAIL (TEXT ("Incorrect value returned by MapVirtualKey"));
        }
    }

cleanup:
    return getCode();
}

////////////////////////////////////////////////////////////////////////////////
