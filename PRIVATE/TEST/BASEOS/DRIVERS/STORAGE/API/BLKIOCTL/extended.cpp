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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  BLKIOCTL TUX DLL
//
//  Module: extended.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

////////////////////////////////////////////////////////////////////////////////
// Tst_IOCTL_DISK_GETNAME
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_IOCTL_DISK_GETNAME(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    WCHAR szName[MAX_PATH] = L"";
    DWORD cBytes = 0;

    if (INVALID_HANDLE_VALUE == g_hDisk) {
        g_pKato->Log(LOG_SKIP, L"there are no valid block devices present for testing"); 
        goto done;
    }

    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_GETNAME with WCHAR[MAX_PATH] string as output parameter");
    if(DeviceIoControl(g_hDisk, IOCTL_DISK_GETNAME, NULL, 0, szName, MAX_PATH, &cBytes, NULL)) {
        g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_GETNAME succeeded with WCHAR[MAX_PATH] string as output parameter: \"%s\"", szName);
    } else {
        g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_GETNAME failed with WCHAR[MAX_PATH] string as output parameter");
        g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_GETNAME with WCHAR[MAX_PATH] string as input parameter");    
        if(DeviceIoControl(g_hDisk, IOCTL_DISK_GETNAME, szName, MAX_PATH, NULL, 0, &cBytes, NULL)) {
            g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_GETNAME succeeded with WCHAR[MAX_PATH] string as input parameter: \"%s\"", szName);
            g_pKato->Log(LOG_DETAIL, L"block driver incorrectly supports IOCTL_DISK_GETNAME");
        } else {
            g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_GETNAME failed with WCHAR[MAX_PATH] string as input parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver does not support IOCTL_DISK_GETNAME");
            g_pKato->Log(LOG_DETAIL, L"this block driver may only support the depricated DISK_IOCTL_GETNAME control code");
            goto done;
        }
    }

    if(wcslen(szName)+1 == cBytes) {
        g_pKato->Log(LOG_DETAIL, L"PASS: IOCTL_DISK_GETNAME returned string length + NULL in lpBytesReturned");
    } else {
        g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_GETNAME returned lpBytesReturned=%u instead of string length + NULL", cBytes);
    }

done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_IOCTL_DISK_DEVICE_INFO
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_IOCTL_DISK_DEVICE_INFO(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    
    STORAGEDEVICEINFO sdi = {0};
    DWORD cBytes = 0;

    if (INVALID_HANDLE_VALUE == g_hDisk) {
        g_pKato->Log(LOG_SKIP, L"there are no valid block devices present for testing"); 
        goto done;
    }

    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_DEVICE_INFO with STORAGEDEVICEINFO struct as input parameter");
    if(DeviceIoControl(g_hDisk, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, &cBytes, NULL)) {
        g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_DEVICE_INFO succeeded with STORAGEDEVICEINFO struct as input parameter");
    } else {
        g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_DEVICE_INFO failed with STORAGEDEVICEINFO struct as input parameter");
        g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_DEVICE_INFO with STORAGEDEVICEINFO struct as output parameter");    
        if(DeviceIoControl(g_hDisk, IOCTL_DISK_DEVICE_INFO, NULL, 0, &sdi, sizeof(STORAGEDEVICEINFO), &cBytes, NULL)) {
            g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_DEVICE_INFO succeeded with STORAGEDEVICEINFO struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver incorrectly supports IOCTL_DISK_DEVICE_INFO");
        } else {
            g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_DEVICE_INFO failed with STORAGEDEVICEINFO struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver does not support IOCTL_DISK_DEVICE_INFO");
            goto done;
        }
    }
    
    if(sizeof(STORAGEDEVICEINFO) != cBytes) {
        g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_DEVICE_INFO did not return correct byte count in lpBytesReturned");
    }
    
done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_IOCTL_DISK_GET_STORAGEID
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_IOCTL_DISK_GET_STORAGEID(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    
    STORAGE_IDENTIFICATION* psi = NULL, *pTmp = NULL;
    DWORD cBytes = 0, cTmp = 0;;

    if (INVALID_HANDLE_VALUE == g_hDisk) {
        g_pKato->Log(LOG_SKIP, L"there are no valid block devices present for testing"); 
        goto done;
    }

    // allocate a buffer
    psi = (STORAGE_IDENTIFICATION*)LocalAlloc(LPTR, sizeof(STORAGE_IDENTIFICATION));
    if(NULL == psi) {
        g_pKato->Log(LOG_SKIP, L"unable to allocate buffer to store storage id");
        goto done;
    }

    psi->dwSize = sizeof(STORAGE_IDENTIFICATION);
    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_GET_STORAGEID with STORAGE_IDENTIFICATION struct as output parameter");
    if(!DeviceIoControl(g_hDisk, IOCTL_DISK_GET_STORAGEID, NULL, 0, psi, sizeof(STORAGE_IDENTIFICATION), &cBytes, NULL)) {        
        if(ERROR_INSUFFICIENT_BUFFER == GetLastError() && psi->dwSize > sizeof(STORAGE_IDENTIFICATION)) {
            // buffer size was insufficient, psi->dwSize contains the required size
            g_pKato->Log(LOG_DETAIL, L"IOCTL_DISK_GET_STORAGEID failed with ERROR_INSUFFICIENT_BUFFER; reallocating buffer and trying again");
            cTmp = psi->dwSize;
            pTmp = (STORAGE_IDENTIFICATION*)LocalReAlloc(psi, cTmp, LMEM_MOVEABLE);
            if(NULL == pTmp) {
                g_pKato->Log(LOG_SKIP, L"unable to allocate buffer to store storage id");
                goto done;
            }
            psi = pTmp;
            psi->dwSize = cTmp;
            if(DeviceIoControl(g_hDisk, IOCTL_DISK_GET_STORAGEID, NULL, 0, psi, cTmp, &cBytes, NULL)) {
                g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_GET_STORAGEID succeeded with STORAGE_IDENTIFICATION struct as output parameter");
            } else {
                g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_GET_STORAGEID failed with STORAGE_IDENTIFICATION struct as output parameter");
            }
        } else {
            g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_GET_STORAGEID failed with STORAGE_IDENTIFICATION struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_GET_STORAGEID with STORAGE_IDENTIFICATION struct as input parameter");
            if(!DeviceIoControl(g_hDisk, IOCTL_DISK_GET_STORAGEID, psi, sizeof(STORAGE_IDENTIFICATION), NULL, 0, &cBytes, NULL)) {        
                if(ERROR_INSUFFICIENT_BUFFER == GetLastError() && psi->dwSize > sizeof(STORAGE_IDENTIFICATION)) {
                    // buffer size was insufficient, psi->dwSize contains the required size
                    g_pKato->Log(LOG_DETAIL, L"IOCTL_DISK_GET_STORAGEID failed with ERROR_INSUFFICIENT_BUFFER; reallocating buffer and trying again");
                    cTmp = psi->dwSize;
                    pTmp = (STORAGE_IDENTIFICATION*)LocalReAlloc(psi, cTmp, LMEM_MOVEABLE);
                    if(NULL == pTmp) {
                        g_pKato->Log(LOG_SKIP, L"unable to allocate buffer to store storage id");
                        goto done;
                    }
                    psi = pTmp;
                    psi->dwSize = cTmp;
                    if(DeviceIoControl(g_hDisk, IOCTL_DISK_GET_STORAGEID, psi, cTmp, NULL, 0, &cBytes, NULL)) {
                        g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_GET_STORAGEID succeeded with STORAGE_IDENTIFICATION struct as input parameter");
                        g_pKato->Log(LOG_DETAIL, L"block driver incorrectly supports IOCTL_DISK_GET_STORAGEID");
                    } else {
                        g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_GET_STORAGEID failed with STORAGE_IDENTIFICATION struct as input parameter");
                    }
                } else {
                    g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_GET_STORAGEID failed with STORAGE_IDENTIFICATION struct as input parameter");
                    g_pKato->Log(LOG_DETAIL, L"block driver does not support IOCTL_DISK_DEVICE_INFO");
                    goto done;
                }
            }
        }
    }

    if(cTmp != cBytes) {
        g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_GET_STORAGEID did not return correct byte count in lpBytesReturned");
    }
    
done:
    if(psi) {
        LocalFree(psi);
    }
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_IOCTL_DISK_DELETE_CLUSTER
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_IOCTL_DISK_DELETE_CLUSTER(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD cluster = 1;
    DWORD cBytes = 0;

    if (INVALID_HANDLE_VALUE == g_hDisk) {
        g_pKato->Log(LOG_SKIP, L"there are no valid block devices present for testing"); 
        goto done;
    }

    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_DELETE_CLUSTER with DWORD as input parameter");
    if(DeviceIoControl(g_hDisk, IOCTL_DISK_DELETE_CLUSTER, &cluster, sizeof(DWORD), NULL, 0, &cBytes, NULL)) {
        g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_DELETE_CLUSTER succeeded with DWORD as input parameter");
    } else {
        g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_DELETE_CLUSTER failed with DWORD as input parameter");
        g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_DELETE_CLUSTER with DWORD as output parameter");    
        if(DeviceIoControl(g_hDisk, IOCTL_DISK_DELETE_CLUSTER, NULL, 0, &cluster, sizeof(DWORD), &cBytes, NULL)) {
            g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_DELETE_CLUSTER succeeded with DWORD as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver incorrectly supports IOCTL_DISK_DELETE_CLUSTER");
        } else {
            g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_DELETE_CLUSTER failed with DWORD as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver does not support IOCTL_DISK_DELETE_CLUSTER");
            goto done;
        }
    }
    
done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_IOCTL_DISK_DELETE_SECTORS
//  Executes one test.
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI Tst_IOCTL_DISK_DELETE_SECTORS(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    
    DELETE_SECTOR_INFO dsi = {0};
    DWORD cBytes = 0;

    if (INVALID_HANDLE_VALUE == g_hDisk) {
        g_pKato->Log(LOG_SKIP, L"there are no valid block devices present for testing"); 
        goto done;
    }

    dsi.cbSize = sizeof(DELETE_SECTOR_INFO);
    dsi.startsector = 0;
    dsi.numsectors = 1;
    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_DELETE_SECTORS with DELETE_SECTOR_INFO struct as input parameter");
    if(DeviceIoControl(g_hDisk, IOCTL_DISK_DELETE_SECTORS, &dsi, sizeof(DELETE_SECTOR_INFO), NULL, 0, &cBytes, NULL)) {
        g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_DELETE_SECTORS succeeded with DELETE_SECTOR_INFO struct as input parameter");
    } else {
        g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_DELETE_SECTORS failed with DELETE_SECTOR_INFO struct as input parameter");
        g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_DELETE_SECTORS with DELETE_SECTOR_INFO struct as output parameter");    
        if(DeviceIoControl(g_hDisk, IOCTL_DISK_DELETE_SECTORS, NULL, 0, &dsi, sizeof(DELETE_SECTOR_INFO), &cBytes, NULL)) {
            g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_DELETE_SECTORS succeeded with DELETE_SECTOR_INFO struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver incorrectly supports IOCTL_DISK_DELETE_SECTORS");
        } else {
            g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_DELETE_SECTORS failed with DELETE_SECTOR_INFO struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver does not support IOCTL_DISK_DELETE_SECTORS");
            goto done;
        }
    }
    
done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
