//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
////////////////////////////////////////////////////////////////////////////////
//
//  BLKIOCTL TUX DLL
//
//  Module: standard.cpp
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h" 

////////////////////////////////////////////////////////////////////////////////
// Tst_IOCTL_DISK_GETINFO
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

TESTPROCAPI Tst_IOCTL_DISK_GETINFO(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DISK_INFO diskInfo = {0};
    DWORD cBytes = 0;

    if (INVALID_HANDLE_VALUE == g_hDisk) {
        g_pKato->Log(LOG_SKIP, L"there are no valid block devices present for testing"); 
        goto done;
    }

    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_GETINFO with DISK_INFO struct as input parameter");
    if(DeviceIoControl(g_hDisk, IOCTL_DISK_GETINFO, &diskInfo, sizeof(DISK_INFO), NULL, 0, &cBytes, NULL)) {
        g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_GETINFO succeeded with DISK_INFO struct as input parameter");
    } else {
        g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_GETINFO failed with DISK_INFO struct as input parameter");
        g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_GETINFO with DISK_INFO struct as output parameter");    
        cBytes = 0;
        if(DeviceIoControl(g_hDisk, IOCTL_DISK_GETINFO, NULL, 0, &diskInfo, sizeof(DISK_INFO), &cBytes, NULL)) {
            g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_GETINFO succeeded with DISK_INFO struct as output parameter");         
            g_pKato->Log(LOG_DETAIL, L"block driver incorrectly supports IOCTL_DISK_GETINFO");
        } else {
            g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_GETINFO failed with DISK_INFO struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver does not support IOCTL_DISK_GETINFO");
            g_pKato->Log(LOG_DETAIL, L"this block driver may only support the depricated DISK_IOCTL_GET_INFO control code");
            goto done;
        }
    }

    if(sizeof(DISK_INFO) != cBytes) {
        g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_GETINFO did not return byte count in lpBytesReturned");
    }
    
done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_IOCTL_DISK_SETINFO
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

TESTPROCAPI Tst_IOCTL_DISK_SETINFO(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DISK_INFO diskInfo = {0};
    DWORD cBytes = 0;

    if (INVALID_HANDLE_VALUE == g_hDisk) {
        g_pKato->Log(LOG_SKIP, L"there are no valid block devices present for testing"); 
        goto done;
    }
    
    if(!DeviceIoControl(g_hDisk, IOCTL_DISK_GETINFO, &diskInfo, sizeof(DISK_INFO), &diskInfo, sizeof(DISK_INFO), &cBytes, NULL)
        && !DeviceIoControl(g_hDisk, DISK_IOCTL_GETINFO, &diskInfo, sizeof(DISK_INFO), &diskInfo, sizeof(DISK_INFO), &cBytes, NULL)) {
        g_pKato->Log(LOG_SKIP, L"unable to test IOCTL_DISK_SETINFO because both IOCTL_DISK_GETINFO and DISK_IOCTL_GETINFO failed");
        goto done;
    }

    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_SETINFO with DISK_INFO struct as input parameter");
    if(DeviceIoControl(g_hDisk, IOCTL_DISK_SETINFO, &diskInfo, sizeof(DISK_INFO), NULL, 0, &cBytes, NULL)) {
        g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_SETINFO succeeded with DISK_INFO struct as input parameter");
    } else {
        g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_SETINFO failed with DISK_INFO struct as input parameter");
        g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_SETINFO with DISK_INFO struct as output parameter");    
        if(DeviceIoControl(g_hDisk, IOCTL_DISK_SETINFO, NULL, 0, &diskInfo, sizeof(DISK_INFO), &cBytes, NULL)) {
            g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_SETINFO succeeded with DISK_INFO struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver incorrectly supports IOCTL_DISK_SETINFO");
        } else {
            g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_SETINFO failed with DISK_INFO struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver does not support IOCTL_DISK_SETINFO");
            g_pKato->Log(LOG_DETAIL, L"this block driver may only support the depricated DISK_IOCTL_SETINFO control code");
        }
    }

done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_IOCTL_DISK_READ
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

TESTPROCAPI Tst_IOCTL_DISK_READ(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    static BYTE control[SECTOR_SIZE];
    static BYTE sgBuffers[MAX_SG_BUF][SECTOR_SIZE];
    static BYTE sgReq[sizeof(SG_REQ)+(MAX_SG_BUF)*sizeof(SG_BUF)];
    PSG_REQ pSGReq = (PSG_REQ)sgReq;
    DWORD cBytes = 0;
    UINT i, j;
    BOOL fRet;
    
    BOOL fCorrectPolarity = FALSE;
    DWORD cMaxSGCount = 0;

    if (INVALID_HANDLE_VALUE == g_hDisk) {
        g_pKato->Log(LOG_SKIP, L"there are no valid block devices present for testing"); 
        goto done;
    }

    memset(control, 0xCE, SECTOR_SIZE);
    
    // initialize SG request
    pSGReq->sr_start = 0;
    pSGReq->sr_num_sec = 1;
    pSGReq->sr_num_sg = 1;
    pSGReq->sr_status = 0;
    pSGReq->sr_callback = NULL;  
    memset(sgBuffers[0], 0xCE, SECTOR_SIZE);
    pSGReq->sr_sglist[0].sb_len = SECTOR_SIZE;
    pSGReq->sr_sglist[0].sb_buf = sgBuffers[0]; 
        
    // run a single sg buffer test to verify polarity
    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_READ with SG_REQ struct as input parameter");
    if(DeviceIoControl(g_hDisk, IOCTL_DISK_READ, pSGReq, sizeof(SG_REQ) , NULL, 0, &cBytes, NULL)) {
        g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_READ succeeded with SG_REQ struct as input parameter");
        fCorrectPolarity = TRUE;
        cMaxSGCount = 1;
    } else {
        g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_READ failed with SG_REQ struct as input parameter");        
        g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_READ with SG_REQ struct as output parameter");
        if(DeviceIoControl(g_hDisk, IOCTL_DISK_READ, NULL, 0, pSGReq, sizeof(SG_REQ), &cBytes, NULL)) {
            g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_READ succeeded with SG_REQ struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver incorrectly supports IOCTL_DISK_READ");
            fCorrectPolarity = FALSE;
            cMaxSGCount = 1;
        }
    }

    if(1 != cMaxSGCount) {
        // bail here because the block driver didn't even support 1 SG buffer
        g_pKato->Log(LOG_DETAIL, L"block driver does not support IOCTL_DISK_READ");
        g_pKato->Log(LOG_DETAIL, L"this block driver may only support the depricated DISK_IOCTL_READ control code");
        goto done;
    }

    // run a multiple sg test
    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_READ with multiple SG buffers");
    for(i = 1; i < MAX_SG_BUF; i++) {

        // initialize SG request
        pSGReq->sr_start = 0;
        pSGReq->sr_num_sec = i+1;
        pSGReq->sr_num_sg = i+1;
        pSGReq->sr_status = 0;
        pSGReq->sr_callback = NULL;

        cBytes = 0;
        
        // initialize SG buffers
        for(j = 0; j <= i; j++) {
            memset(sgBuffers[j], 0xCE, SECTOR_SIZE);
            pSGReq->sr_sglist[j].sb_len = SECTOR_SIZE;
            pSGReq->sr_sglist[j].sb_buf = sgBuffers[j];
        }

        g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_READ with %u SG buffers", i+1);
        if(fCorrectPolarity) {
            fRet = DeviceIoControl(g_hDisk, IOCTL_DISK_READ, pSGReq, sizeof(SG_REQ)+i*sizeof(SG_BUF), NULL, 0, &cBytes, NULL);
        } else {
            fRet = DeviceIoControl(g_hDisk, IOCTL_DISK_READ, NULL, 0, pSGReq, sizeof(SG_REQ)+i*sizeof(SG_BUF), &cBytes, NULL);
        }

        // verify call did not corrupt members of the sg_req struct
        if(pSGReq->sr_start != 0) {
            g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_READ corrupted sr_start value of SG buffer");
        }
        if(pSGReq->sr_num_sec != SECTOR_SIZE + SECTOR_SIZE*i) {
            g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_READ corrupted sr_num_sec value of SG buffer");
        }
        if(pSGReq->sr_num_sg != i+1) {
            g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_READ corrupted sr_num_sg value of SG buffer");
        }
        for(j = 0; j <= i; j++) {
            if(pSGReq->sr_sglist[j].sb_len != SECTOR_SIZE) {
                g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_READ corrupted sr_sglist[%u].sb_len value of SG buffer", j);
            }
        }
        
        if(fRet) {
            g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_READ succeeded with %u SG buffers", i+1);
            cMaxSGCount = i+1;

            // verify data requested was changed
            for(j = 0; j <= i; j++) {
                if(0 == memcmp(sgBuffers[j], control, SECTOR_SIZE)) {
                    // appears that the input buffer was not modified on success
                    g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_READ did not copy data to SG buffer %u", j);
                }
            }        
        } else {
            g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_READ failed with %u SG buffers", i+1);

            // verify data requested was not changed
            for(j = 0; j <= i; j++) {
                if(0 != memcmp(sgBuffers[j], control, SECTOR_SIZE)) {
                    // appears that the input buffer was modified on failure
                    g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_READ modified data to SG buffer %u even though the call failed", j);
                }
            }        
            break;
        }
    }

done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_IOCTL_DISK_WRITE
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

TESTPROCAPI Tst_IOCTL_DISK_WRITE(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    static BYTE origsectors[MAX_SG_BUF][SECTOR_SIZE];
    static BYTE sgBuffers[MAX_SG_BUF][SECTOR_SIZE];
    static BYTE sgReq[sizeof(SG_REQ)+(MAX_SG_BUF)*sizeof(SG_BUF)];
    PSG_REQ pSGReq = (PSG_REQ)sgReq;
    DWORD cBytes = 0;
    UINT i, j;
    BOOL fRet;
    
    BOOL fCorrectPolarity = FALSE;
    DWORD cMaxSGCount = 0;

    if (INVALID_HANDLE_VALUE == g_hDisk) {
        g_pKato->Log(LOG_SKIP, L"there are no valid block devices present for testing"); 
        goto done;
    }

    // before we do anything, read the data off the disk-- this is the data that will
    // be written so that the disk contents are not destroyed (still a risk)
    for(i = 0; i < MAX_SG_BUF; i++) {
        pSGReq->sr_start = i;
        pSGReq->sr_num_sec = 1;
        pSGReq->sr_num_sg = 1;
        pSGReq->sr_status = 0;
        pSGReq->sr_callback = NULL;
        pSGReq->sr_sglist[0].sb_len = SECTOR_SIZE;
        pSGReq->sr_sglist[0].sb_buf = origsectors[i];
        if(!DeviceIoControl(g_hDisk, IOCTL_DISK_READ, pSGReq, sizeof(SG_REQ), pSGReq, sizeof(SG_REQ), &cBytes, NULL) &&
           !DeviceIoControl(g_hDisk, DISK_IOCTL_READ, pSGReq, sizeof(SG_REQ), pSGReq, sizeof(SG_REQ), &cBytes, NULL) ) {
            g_pKato->Log(LOG_SKIP, L"unable to test IOCTL_DISK_WRITE because both IOCTL_DISK_READ and DISK_IOCTL_READ failed on sector %u", i);
            goto done;
        }
    }
    
    // initialize SG request
    pSGReq->sr_start = 0;
    pSGReq->sr_num_sec = 1;
    pSGReq->sr_num_sg = 1;
    pSGReq->sr_sglist[0].sb_len = SECTOR_SIZE;
    pSGReq->sr_sglist[0].sb_buf = sgBuffers[0]; 
    memcpy(sgBuffers[0], origsectors[0], SECTOR_SIZE);  
    
    // run a single sg buffer test to verify polarity
    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_WRITE with SG_REQ struct as input parameter");
    if(DeviceIoControl(g_hDisk, IOCTL_DISK_WRITE, pSGReq, sizeof(SG_REQ) , NULL, 0, &cBytes, NULL)) {
        g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_WRITE succeeded with SG_REQ struct as input parameter");
        fCorrectPolarity = TRUE;
        cMaxSGCount = 1;
    } else {
        g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_WRITE failed with SG_REQ struct as input parameter");        
        g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_WRITE with SG_REQ struct as output parameter");
        if(DeviceIoControl(g_hDisk, IOCTL_DISK_WRITE, NULL, 0, pSGReq, sizeof(SG_REQ), &cBytes, NULL)) {
            g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_WRITE succeeded with SG_REQ struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver incorrectly supports IOCTL_DISK_WRITE");
            fCorrectPolarity = FALSE;
            cMaxSGCount = 1;
        }
    }

    if(1 != cMaxSGCount) {
        // bail here because the block driver didn't even support 1 SG buffer
        g_pKato->Log(LOG_DETAIL, L"block driver does not support IOCTL_DISK_WRITE");
        g_pKato->Log(LOG_DETAIL, L"this block driver may only support the depricated DISK_IOCTL_WRITE control code");
        goto done;
    }

    // run a multiple sg test
    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_WRITE with multiple SG buffers");
    for(i = 0; i < MAX_SG_BUF; i++) {

        // initialize SG request
        pSGReq->sr_start = 0;
        pSGReq->sr_num_sec = i+1;
        pSGReq->sr_num_sg = i+1;
        pSGReq->sr_status = 0;
        pSGReq->sr_callback = NULL;

        cBytes = 0;
        
        // initialize SG buffers
        for(j = 0; j <= i; j++) {
            memcpy(sgBuffers[j], origsectors[j], SECTOR_SIZE);
            pSGReq->sr_sglist[j].sb_len = SECTOR_SIZE;
            pSGReq->sr_sglist[j].sb_buf = sgBuffers[j];
        }

        g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_WRITE with %u SG buffers", i+1);
        if(fCorrectPolarity) {
            fRet = DeviceIoControl(g_hDisk, IOCTL_DISK_WRITE, pSGReq, sizeof(SG_REQ)+i*sizeof(SG_BUF), NULL, 0, &cBytes, NULL);
        } else {
            fRet = DeviceIoControl(g_hDisk, IOCTL_DISK_WRITE, NULL, 0, pSGReq, sizeof(SG_REQ)+i*sizeof(SG_BUF), &cBytes, NULL);
        }

        // verify call did not corrupt members of the sg_req struct
        if(pSGReq->sr_start != 0) {
            g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_WRITE corrupted sr_start value of SG buffer");
        }
        if(pSGReq->sr_num_sec != SECTOR_SIZE + SECTOR_SIZE*i) {
            g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_WRITE corrupted sr_num_sec value of SG buffer");
        }
        if(pSGReq->sr_num_sg != i+1) {
            g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_WRITE corrupted sr_num_sg value of SG buffer");
        }
        for(j = 0; j <= i; j++) {
            if(pSGReq->sr_sglist[j].sb_len != SECTOR_SIZE) {
                g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_WRITE corrupted sr_sglist[%u].sb_len value of SG buffer", j);
            }
            if(0 != memcmp(sgBuffers[j], origsectors[j], SECTOR_SIZE)) {
                g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_WRITE corrupted sr_sglist[%u].sb_buf data in SG buffer", j);
            }
        }
        
        if(fRet) {
            g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_WRITE succeeded with %u SG buffers", i+1);
            cMaxSGCount = i+1;
        } else {
            g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_WRITE failed with %u SG buffers", i+1);
            break;
        }
    }

done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Tst_IOCTL_DISK_FORMAT_MEDIA
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

TESTPROCAPI Tst_IOCTL_DISK_FORMAT_MEDIA(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DISK_INFO diskInfo = {0}, newDiskInfo = {0};
    DWORD cBytes = 0;

    if (INVALID_HANDLE_VALUE == g_hDisk) {
        g_pKato->Log(LOG_SKIP, L"there are no valid block devices present for testing"); 
        goto done;
    }
    
    if(!DeviceIoControl(g_hDisk, IOCTL_DISK_GETINFO, &diskInfo, sizeof(DISK_INFO), &diskInfo, sizeof(DISK_INFO), &cBytes, NULL)
        && !DeviceIoControl(g_hDisk, DISK_IOCTL_GETINFO, &diskInfo, sizeof(DISK_INFO), &diskInfo, sizeof(DISK_INFO), &cBytes, NULL)) {
        g_pKato->Log(LOG_SKIP, L"unable to test IOCTL_DISK_FORMAT_MEDIA because both IOCTL_DISK_GETINFO and DISK_IOCTL_GETINFO failed");
        goto done;
    }

    g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_FORMAT_MEDIA with DISK_INFO struct as input parameter");
    if(DeviceIoControl(g_hDisk, IOCTL_DISK_FORMAT_MEDIA, &diskInfo, sizeof(DISK_INFO), NULL, 0, &cBytes, NULL)) {
        g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_FORMAT_MEDIA succeeded with DISK_INFO struct as input parameter");
    } else {
        g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_FORMAT_MEDIA failed with DISK_INFO struct as input parameter");
        g_pKato->Log(LOG_DETAIL, L"TEST: IOCTL_DISK_FORMAT_MEDIA with DISK_INFO struct as output parameter");    
        if(DeviceIoControl(g_hDisk, IOCTL_DISK_FORMAT_MEDIA, NULL, 0, &diskInfo, sizeof(DISK_INFO), &cBytes, NULL)) {
            g_pKato->Log(LOG_PASS, L"PASS: IOCTL_DISK_FORMAT_MEDIA succeeded with DISK_INFO struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver incorrectly supports IOCTL_DISK_FORMAT_MEDIA");
        } else {
            g_pKato->Log(LOG_FAIL, L"FAIL: IOCTL_DISK_FORMAT_MEDIA failed with DISK_INFO struct as output parameter");
            g_pKato->Log(LOG_DETAIL, L"block driver does not support IOCTL_DISK_FORMAT_MEDIA");
            goto done;
        }
    }

    if(0 != memcmp(&diskInfo, &newDiskInfo, sizeof(DISK_INFO))) {
        g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_FORMAT_MEDIA returned incorrect DISK_INFO structure");
    }
    
    if(sizeof(DISK_INFO) != cBytes) {
        g_pKato->Log(LOG_WARN, L"WARNING: IOCTL_DISK_FORMAT_MEDIA did not return correct byte count in lpBytesReturned");
    }

done:
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
