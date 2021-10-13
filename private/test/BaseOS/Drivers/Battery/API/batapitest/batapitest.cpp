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

#include "testmain.h"

typedef DWORD (WINAPI *PFN_GetSystemPowerStatusEx2)(
    PSYSTEM_POWER_STATUS_EX2 pSystemPowerStatusEx2,
    DWORD dwLen,
    BOOL fUpdate
);
typedef LONG (WINAPI* PFN_BatteryDrvrGetLevels)();
typedef BOOL (WINAPI* PFN_BatteryDrvrSupportsChangeNotification)();
typedef VOID (WINAPI* PFN_BatteryGetLifeTimeInfo)(
    LPSYSTEMTIME st, 
    DWORD* pdwUsage, 
    DWORD* pdwPreviousUsag
);


//funciton entries
PFN_GetSystemPowerStatusEx2 g_pfnGetSystemPowerStatusEx2 = NULL;
PFN_BatteryDrvrGetLevels g_pfnBatteryDrvrGetLevels = NULL;
PFN_BatteryDrvrSupportsChangeNotification g_pfnBatteryDrvrSupportsChangeNotification = NULL;
PFN_BatteryGetLifeTimeInfo g_pfnBatteryGetLifeTimeInfo = NULL;

HINSTANCE g_hCoreDll = NULL;

// get function entries from dll's export tale and initialize function pointers
BOOL GetBatteryFunctionEntries()
{
#pragma prefast(disable:321, "Prefast disable: Potential use of relative pathname in call to LoadLibrary.")
    g_hCoreDll = LoadLibrary(_T("coredll.dll"));
#pragma prefast(enable:321, "Prefast enable: Potential use of relative pathname in call to LoadLibrary.")

    if (g_hCoreDll == NULL) {
        ERRFAIL("LoadLibrary(coredll.dll) failed!");
        return FALSE;
    }
    else {
        g_pfnGetSystemPowerStatusEx2 = (PFN_GetSystemPowerStatusEx2)GetProcAddress(g_hCoreDll, _T("GetSystemPowerStatusEx2"));
        g_pfnBatteryDrvrGetLevels = (PFN_BatteryDrvrGetLevels)GetProcAddress(g_hCoreDll, _T("BatteryDrvrGetLevels"));
        g_pfnBatteryDrvrSupportsChangeNotification = (PFN_BatteryDrvrSupportsChangeNotification)GetProcAddress(g_hCoreDll, _T("BatteryDrvrSupportsChangeNotification"));
        g_pfnBatteryGetLifeTimeInfo = (PFN_BatteryGetLifeTimeInfo)GetProcAddress(g_hCoreDll, _T("BatteryGetLifeTimeInfo"));
    }

    if(g_pfnGetSystemPowerStatusEx2 == NULL){
        WARN("Function entry GetSystemPowerStatusEx2 is not available");
    }
    if(g_pfnBatteryDrvrGetLevels == NULL){
        WARN("Function entry BatteryDrvrGetLevels is not available");
    }
    if(g_pfnBatteryGetLifeTimeInfo == NULL){
        WARN("Function entry BatteryGetLifeTimeInfo is not available");
    }
    if(g_pfnBatteryDrvrSupportsChangeNotification == NULL){
        WARN("Function entry BatteryDrvrSupportsChangeNotification is not available");
    }

    return TRUE;
}


VOID FreeCoreDll()
{
    if(g_hCoreDll != NULL){
        FreeLibrary(g_hCoreDll);
        g_hCoreDll = NULL;
    }
}


BOOL CheckErrorCode(DWORD expectedErrCode)
{
    DWORD dwErrCode = GetLastError();
    
    LOG (_T("GetLastError is %u."), dwErrCode);
    if (dwErrCode != expectedErrCode) {
        LOG(_T("ERROR:  The error code returned is %u, while the expected value is %u."), 
            dwErrCode, expectedErrCode);
        return FALSE;
    }
    return TRUE;
}


// check the value in SYSTEM_POWER_STATUS_EX2 structure returned from API
BOOL CheckSystemPowerStatusEx2(LPSYSTEM_POWER_STATUS_EX2 pSps)
{
    BOOL retVal = TRUE;
    
    // check ACLineStatus
    if (pSps->ACLineStatus != AC_LINE_OFFLINE 
        && pSps->ACLineStatus != AC_LINE_ONLINE 
        && pSps->ACLineStatus != AC_LINE_BACKUP_POWER 
        && pSps->ACLineStatus != AC_LINE_UNKNOWN)
    {
        retVal = FALSE;
    }
    
    // check percentage
    if (pSps->BatteryLifePercent > 100 
        && pSps->BatteryLifePercent != BATTERY_PERCENTAGE_UNKNOWN)
    {
        retVal = FALSE;
    }

    // check backup percentage
    if (pSps->BackupBatteryLifePercent > 100 
        && pSps->BackupBatteryLifePercent != BATTERY_PERCENTAGE_UNKNOWN)
    {
        retVal = FALSE;
    }
    
    // check battery voltage
    if (pSps->BatteryVoltage > 65535) {
        retVal = FALSE;
    }
    
    // check battery current
    if ((INT32)(pSps->BatteryCurrent) > 32767 || (INT32)(pSps->BatteryCurrent) < -32768) {
        retVal = FALSE;
    }
    
    // check battery average current
    if ((INT32)(pSps->BatteryAverageCurrent) > 32767 || (INT32)(pSps->BatteryAverageCurrent) < -32768) {
        retVal = FALSE;
    }
    
    // check long-term cumulative average discharge in milliamperes per hour
    if ((INT32)pSps->BatterymAHourConsumed < -32768 || (INT32)pSps->BatterymAHourConsumed > 0) {
        retVal = FALSE;
    }
    
    // check battery temperature
    if ((INT32)pSps->BatteryTemperature < -32768 || (INT32)pSps->BatteryTemperature > 32767) {
        retVal = FALSE;
    }

    // check backup battery voltage
    if (pSps->BackupBatteryVoltage > 65535) {
        retVal = FALSE;
    }
    
    // check battery chemistry
    if (pSps->BatteryChemistry != BATTERY_CHEMISTRY_ALKALINE 
        && pSps->BatteryChemistry != BATTERY_CHEMISTRY_NICD
        && pSps->BatteryChemistry != BATTERY_CHEMISTRY_NIMH
        && pSps->BatteryChemistry != BATTERY_CHEMISTRY_LION
        && pSps->BatteryChemistry != BATTERY_CHEMISTRY_LIPOLY
        && pSps->BatteryChemistry != BATTERY_CHEMISTRY_ZINCAIR
        && pSps->BatteryChemistry != BATTERY_CHEMISTRY_UNKNOWN)
    {
        retVal = FALSE;
    }
    
    return retVal;
}


// --------------------------------------------------------------------
TESTPROCAPI 
Tst_GetSystemPowerStatusEx2(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(tpParam);
    UNREFERENCED_PARAMETER(lpFTE);

    if(uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    if(g_pfnGetSystemPowerStatusEx2 == NULL) {
        WARN("Function GetSystemPowerStatusEx2 is not exposed in this image! Test will be skipped");
        return TPR_SKIP;
    }

    DWORD retVal = TPR_PASS;
    
    SYSTEM_POWER_STATUS_EX2 sps;

    LOG(_T("Try calling GetSystemPowerStatus with correct parameters"));
    
    // get status, should success
    DWORD dwLen = 0;
    __try{
        LOG(_T("Calling GetSystemPowerStatusEx2(x, x, TRUE)"));
        dwLen = g_pfnGetSystemPowerStatusEx2(&sps, sizeof(sps), TRUE);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("GetSystemPowerStatusEx2(x, x, TRUE) causes exception! \r\n");
        return TPR_FAIL;
    }  
    if(dwLen != sizeof(sps)) {
        ERRFAIL("GetSystemPowerStatusEx2(x, x, TRUE) failed \r\n");
        retVal = TPR_FAIL;
    } else {
        BOOL bSuccess = CheckSystemPowerStatusEx2(&sps);
        if (!bSuccess) {
            retVal = TPR_FAIL;
        }
        LOG(_T("Calling GetSystemPowerStatusEx2(x, x, TRUE) %s!"), 
            bSuccess?_T("succeed"):_T("failed"));
        
        LOG(_T("Line status is %s (0x%x)\r\n"), 
            sps.ACLineStatus == AC_LINE_ONLINE ? _T("AC") : 
            sps.ACLineStatus == AC_LINE_OFFLINE ? _T("Offline") :
            sps.ACLineStatus == AC_LINE_BACKUP_POWER ? _T("Backup") :
            sps.ACLineStatus == AC_LINE_UNKNOWN ? _T("Unknown") : _T("???"), 
            sps.ACLineStatus);
        LOG(_T("Main battery flag 0x%02x, %02u%%, %lu (0x%08x) seconds remaining of %lu (0x%08x)\r\n"),
            sps.BatteryFlag,  sps.BatteryLifePercent, 
            sps.BatteryLifeTime, sps.BatteryLifeTime, 
            sps.BatteryFullLifeTime, sps.BatteryFullLifeTime);
        LOG(_T("Backup battery flag 0x%02x, %02u%%, %lu (0x%08x) seconds remaining of %lu (0x%08x)\r\n"),
            sps.BackupBatteryFlag,  sps.BackupBatteryLifePercent, 
            sps.BackupBatteryLifeTime, sps.BackupBatteryLifeTime, 
            sps.BackupBatteryFullLifeTime, sps.BackupBatteryFullLifeTime);
        LOG(_T("%u mV, %d mA, avg %d ma, avg interval %d, consumed %d mAH, %d C, backup %u mV, chem 0x%x\r\n"),
            sps.BatteryVoltage, sps.BatteryCurrent, sps.BatteryAverageCurrent, 
            sps.BatteryAverageInterval, sps.BatterymAHourConsumed, 
            sps.BatteryTemperature, sps.BackupBatteryVoltage, 
            sps.BatteryChemistry);
    }
    
    // get status again, should success
    __try{
        LOG(_T("Calling GetSystemPowerStatusEx2(x, x, FALSE)"));
        dwLen = g_pfnGetSystemPowerStatusEx2(&sps, sizeof(sps), FALSE);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("GetSystemPowerStatusEx2(x, x, FALSE) causes exception! \r\n");
        return TPR_FAIL;        
    }
    if(dwLen != sizeof(sps)) {
        ERRFAIL("GetSystemPowerStatusEx2(x, x, FALSE) failed \r\n");
        retVal = TPR_FAIL;
    } else {
        BOOL bSuccess = CheckSystemPowerStatusEx2(&sps);
        if (!bSuccess) {
            retVal = TPR_FAIL;
        }
        LOG(_T("calling GetSystemPowerStatusEx2(x, x, FALSE) %s!"), 
            bSuccess?_T("succeed"):_T("failed"));
        
        LOG(_T("Line status is %s (0x%x)\r\n"), 
            sps.ACLineStatus == AC_LINE_ONLINE ? _T("AC") : 
            sps.ACLineStatus == AC_LINE_OFFLINE ? _T("Offline") :
            sps.ACLineStatus == AC_LINE_BACKUP_POWER ? _T("Backup") :
            sps.ACLineStatus == AC_LINE_UNKNOWN ? _T("Unknown") : _T("???"), 
            sps.ACLineStatus);
        LOG(_T("Main battery flag 0x%02x, %02u%%, %lu (0x%08x) seconds remaining of %lu (0x%08x)\r\n"),
            sps.BatteryFlag,  sps.BatteryLifePercent, 
            sps.BatteryLifeTime, sps.BatteryLifeTime, 
            sps.BatteryFullLifeTime, sps.BatteryFullLifeTime);
        LOG(_T("Backup battery flag 0x%02x, %02u%%, %lu (0x%08x) seconds remaining of %lu (0x%08x)\r\n"),
            sps.BackupBatteryFlag,  sps.BackupBatteryLifePercent, 
            sps.BackupBatteryLifeTime, sps.BackupBatteryLifeTime, 
            sps.BackupBatteryFullLifeTime, sps.BackupBatteryFullLifeTime);
        LOG(_T("%u mV, %d mA, avg %d ma, avg interval %d, consumed %d mAH, %d C, backup %u mV, chemistry 0x%x\r\n"),
            sps.BatteryVoltage, sps.BatteryCurrent, sps.BatteryAverageCurrent, 
            sps.BatteryAverageInterval, sps.BatterymAHourConsumed, 
            sps.BatteryTemperature, sps.BackupBatteryVoltage, 
            sps.BatteryChemistry);
    }

    // buffer size is correct size plus one, should success
    __try{
        LOG(_T("Calling GetSystemPowerStatusEx2(x, right size + 1, TRUE)"));
        dwLen = g_pfnGetSystemPowerStatusEx2(&sps, sizeof(sps) + 1, TRUE);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("GetSystemPowerStatusEx2(x, right size + 1, TRUE) causes exception! \r\n");
        return TPR_FAIL;        
    }
    if (dwLen != sizeof(sps)) {
        ERRFAIL("GetSystemPowerStatusEx2(x, right size + 1, TRUE) should success!\r\n");
        retVal = TPR_FAIL;
    } else {
        BOOL bSuccess = CheckSystemPowerStatusEx2(&sps);
        if (!bSuccess) {
            retVal = TPR_FAIL;
        }
        LOG(_T("Calling GetSystemPowerStatusEx2(x, right size + 1, TRUE) %s!"), 
            bSuccess?_T("succeed"):_T("failed"));
    }
    
    // buffer size is correct size minus one, should fail with ERROR_INVALID_PARAMETER
    __try{
        LOG(_T("Calling GetSystemPowerStatusEx2(x, right size - 1, x)"));
        dwLen = g_pfnGetSystemPowerStatusEx2(&sps, sizeof(sps) - 1, FALSE);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("GetSystemPowerStatusEx2(x, right size - 1, x) causes exception! \r\n");
        return TPR_FAIL;        
    }
    if(dwLen != 0) {
        ERRFAIL("GetSystemPowerStatusEx2(x, right size - 1, x) should fail!\r\n");
        retVal = TPR_FAIL;
    }
    if (!CheckErrorCode(ERROR_INVALID_PARAMETER)) {
        retVal = TPR_FAIL;
    }

    // buffer size is 0, should fail with ERROR_INVALID_PARAMETER
    __try{
        LOG(_T("Calling GetSystemPowerStatusEx2(x, 0, x)"));
        dwLen = g_pfnGetSystemPowerStatusEx2(&sps, 0, FALSE);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("GetSystemPowerStatusEx2(x, 0, x) causes exception! \r\n");
        return TPR_FAIL;        
    }
    if(dwLen != 0) {
        ERRFAIL("GetSystemPowerStatusEx2(x, 0, x) should fail!\r\n");
        retVal = TPR_FAIL;
    }
    if (!CheckErrorCode(ERROR_INVALID_PARAMETER)) {
        retVal = TPR_FAIL;
    }    
  
    // get status with NULL input pointer, should fail with ERROR_INVALID_PARAMETER
    __try{
        LOG(_T("Calling GetSystemPowerStatusEx2(NULL, x, x)"));
        dwLen = g_pfnGetSystemPowerStatusEx2(NULL, sizeof(sps), FALSE);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("GetSystemPowerStatusEx2(NULL, x, x) causes exception! \r\n");
        return TPR_FAIL;        
    }
    if(dwLen != 0) {
        ERRFAIL("GetSystemPowerStatusEx2(NULL, x, x) should fail!\r\n");
        retVal = TPR_FAIL;
    } 
    if (!CheckErrorCode(ERROR_INVALID_PARAMETER)) {
        retVal = TPR_FAIL;
    }
    
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_BatteryDrvrGetLevels(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(tpParam);
    UNREFERENCED_PARAMETER(lpFTE);

    if(uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    if(g_pfnBatteryDrvrGetLevels == NULL){
        WARN("Function BatteryDrvrGetLevels is not exposed in this image! Test will be skipped");
        return TPR_SKIP;
    }
    
    // get number of levels
    DWORD dwValue = (DWORD)-1;
    LOG(_T("Calling BatteryDrvrGetLevels()"));
    __try{
        dwValue = (DWORD) g_pfnBatteryDrvrGetLevels();
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("BatteryDrvrGetLevels() causes exception! \r\n");
        return TPR_FAIL;
    }
    
    WORD wMainLevels = LOWORD(dwValue);
    WORD wBackupLevels = HIWORD(dwValue);

    LOG(_T("******************************************************************"));
    LOG(_T(""));
    LOG(_T("Main battery level: %u backup battery level: %u"), wMainLevels, wBackupLevels);
    LOG(_T("Please check the values to see whether they are correct or not"));
    LOG(_T(""));
    LOG(_T(""));
    LOG(_T("******************************************************************"));
    Sleep(1000*10);

    return TPR_PASS;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_BatteryDrvrSupportsChangeNotification(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(tpParam);
    UNREFERENCED_PARAMETER(lpFTE);

    if(uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    if(g_pfnBatteryDrvrSupportsChangeNotification == NULL){
        WARN("Function BatteryDrvrSupportsChangeNotification is not exposed in this image! Test will be skipped");
        return TPR_SKIP;
    }
    
    // do we support change notifications?
    BOOL fSupportsChange = FALSE;
    __try{
        LOG(_T("Calling BatteryDrvrSupportsChangeNotification()"));
        fSupportsChange = g_pfnBatteryDrvrSupportsChangeNotification();
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("Calling BatteryDrvrSupportsChangeNotification() causes exception! \r\n");
        return TPR_FAIL;
    }

    LOG(_T("Battery driver %s notification"), fSupportsChange == TRUE ? _T("supports") : _T("doesn't support"));

    return TPR_PASS;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_BatteryGetLifeTimeInfo(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(tpParam);
    UNREFERENCED_PARAMETER(lpFTE);

    if(uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    if(g_pfnBatteryDrvrSupportsChangeNotification == NULL){
        WARN("Function BatteryDrvrSupportsChangeNotification is not exposed in this image! Test will be skipped");
        return TPR_SKIP;
    }

    if(g_pfnBatteryGetLifeTimeInfo == NULL){
        WARN("Function BatteryBatteryGetLifeTimeInfo is not exposed in this image! Test will be skipped");
        return TPR_SKIP;
    }
    
    // do we support change notifications?
    BOOL fSupportsChange = g_pfnBatteryDrvrSupportsChangeNotification();
    
    TCHAR szBuf[256];
    SYSTEMTIME st;
    DWORD dwUsage, dwPreviousUsage;
    int iLen;
    DWORD retVal = TPR_PASS;
    
    __try{
        LOG(_T("Calling BatteryGetLifeTimeInfo(x, x, x)\r\n"));        
        g_pfnBatteryGetLifeTimeInfo(&st, &dwUsage, &dwPreviousUsage);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("Calling BatteryGetLifeTimeInfo(x, x, x) causes exception! \r\n");
        return TPR_FAIL;
    }   
    if(fSupportsChange == TRUE){
        iLen = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, szBuf, 
            sizeof(szBuf) / sizeof(szBuf[0]));
        if(iLen == 0){
            ERRFAIL("GetTimeFormat call failed!");
            retVal = TPR_FAIL;
        } else {
            LOG(_T("Batteries last changed at %s\r\n"), szBuf);
        }
        iLen = GetDateFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, szBuf, 
            sizeof(szBuf) / sizeof(szBuf[0]));
        
        if(iLen == 0){
            ERRFAIL("GetDateFormat call failed!");
            retVal = TPR_FAIL;
        } else {
            LOG(_T("  on %s\r\n"), szBuf);
        }
        LOG(_T("%lu ms usage, %lu ms previous usage\r\n"), dwUsage,
            dwPreviousUsage);
    }

    __try{
        LOG(_T("Calling BatteryGetLifeTimeInfo(NULL, x, x)\r\n"));
        g_pfnBatteryGetLifeTimeInfo(NULL, &dwUsage, &dwPreviousUsage);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("Calling BatteryGetLifeTimeInfo(NULL, x, x) causes exception! \r\n");
        return TPR_FAIL;
    }
    
    __try{
        LOG(_T("Calling BatteryGetLifeTimeInfo(x, NULL, x)\r\n"));
        g_pfnBatteryGetLifeTimeInfo(&st, NULL, &dwPreviousUsage);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("Calling BatteryGetLifeTimeInfo(x, NULL, x) causes exception! \r\n");
        return TPR_FAIL;
    }
    
    __try{
        LOG(_T("Calling BatteryGetLifeTimeInfo(x, x, NULL)\r\n"));
        g_pfnBatteryGetLifeTimeInfo(&st, &dwUsage, NULL);
    }
    __except(EXCEPTION_EXECUTE_HANDLER){
        ERRFAIL("Calling BatteryGetLifeTimeInfo(x, x, NULL) causes exception! \r\n");
        return TPR_FAIL;
    }
    
    return retVal;
}
