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
#include "cred.h"
#include "cred_ioctl.h"
using namespace ce;

#define CRED_API_HEURISTIC_CRED_SIZE 200

HANDLE ghCredSvc = INVALID_HANDLE_VALUE;


BOOL GetCredSvcHandle(void) {
    HANDLE ghtCredSvc = INVALID_HANDLE_VALUE;
    if(INVALID_HANDLE_VALUE == ghCredSvc){
        ghtCredSvc = CreateFile(TEXT("CRD0:"),
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ|FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, 0, 0);
        if(InterlockedCompareExchangePointer(&ghCredSvc,
                        ghtCredSvc,
                        INVALID_HANDLE_VALUE) != INVALID_HANDLE_VALUE){
            CloseHandle(ghtCredSvc);
        }
        return (INVALID_HANDLE_VALUE != ghCredSvc);
    }else{
        return TRUE;
    }
}

DWORD
CredWrite(
    __in const PCRED pCred,
         DWORD       dwFlags
){

    if(!GetCredSvcHandle()) return ERROR_SERVICE_NOT_ACTIVE;

    if(NULL == pCred){
        return ERROR_INVALID_PARAMETER;
    }

    ce::ioctl_proxy<> proxy(ghCredSvc,IOCTL_CREDSVC_PSL_MARSHAL);

    return proxy.call(IOCTL_CREDSVC_WRITE, pCred, dwFlags);

}

DWORD
CredRead(
    __in            PCWCHAR     wszTarget,
                    DWORD       dwTargetLen,
                    DWORD       dwType,
                    DWORD       dwFlags,
    __deref_out_opt PPCRED      ppCred
){
    if(!GetCredSvcHandle()) return ERROR_SERVICE_NOT_ACTIVE;

    if(NULL == ppCred){
        return ERROR_INVALID_PARAMETER;
    }
    
    ce::ioctl_proxy<> proxy(ghCredSvc,IOCTL_CREDSVC_PSL_MARSHAL);

    DWORD dwCredSize = CRED_API_HEURISTIC_CRED_SIZE;
    BOOL bRetry = FALSE;
    ce::auto_ptr<BYTE> pCred;

    // Any error conditions we exit directly from the loop
    // We try first with a reasonably sized buffer
    // If that fails we try one more time with the buffer size specified by credman
    // If that fails then hard error
    for (;;)
    {

        pCred = new BYTE [dwCredSize];

        if(!pCred.valid()) return ERROR_OUTOFMEMORY;

        // Zero Memory to ensure writeback doesn't 
        // try to unmarshal bogus pointers.
        memset(pCred,0,dwCredSize);

        const DWORD ret =  proxy.call(IOCTL_CREDSVC_READ,
                wszTarget,
                dwTargetLen,
                dwType,
                dwFlags,
                out(CRED_OUT(reinterpret_cast<PCRED>(static_cast<BYTE*>(pCred)),dwCredSize)),
                out(&dwCredSize));

        if(ERROR_SUCCESS == ret){
            // We got the credential. Break out.
            break;
        }else if(ERROR_MORE_DATA == ret){
            // We need to pass in a bigger buffer
            if(bRetry){
                // Second time. Hard error. release memory and return
                return ret;
            }else{
                // We will retry since first time didn't have large enough buffer.
                // dwCredSize should have new buffer size.
                assert(dwCredSize > CRED_API_HEURISTIC_CRED_SIZE );
                pCred.close();
                bRetry = TRUE;
            }
        }else{
            // Some other error. Hard error. release memory and return.
            return ret;
        }
    }

    *ppCred = reinterpret_cast<PCRED>(pCred.release());
    return ERROR_SUCCESS;
}

DWORD
CredDelete(
    __in PCWCHAR     wszTarget,
         DWORD       dwTargetLen,
         DWORD       dwType,
         DWORD       dwFlags
){
    if(!GetCredSvcHandle()) return ERROR_SERVICE_NOT_ACTIVE;

    ce::ioctl_proxy<> proxy(ghCredSvc,IOCTL_CREDSVC_PSL_MARSHAL);

    return proxy.call(IOCTL_CREDSVC_DELETE, wszTarget, dwTargetLen, dwType, dwFlags);
}

DWORD
CredFree(
    __in PBYTE       pbBuffer
){
    // buffer allocated in CredRead
    delete[] pbBuffer;
    return ERROR_SUCCESS;
}

