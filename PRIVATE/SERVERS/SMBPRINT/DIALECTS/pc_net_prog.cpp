//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <bldver.h>
#include <creg.hxx>
#include <winsock.h>

#include "SMB_Globals.h"
#include "PC_NET_PROG.h"
#include "SMBCommands.h"
#include "SMBErrors.h"
#include "ShareInfo.h"
#include "PrintQueue.h"
#include "utils.h"
#include "rapi.h"
#include "security.h"
#include "FileServer.h"
#include "SMBPackets.h"
#include "ConnectionManager.h"
#include "Cracker.h"
#ifdef SMB_NMPIPE
#include "ipcstream.h"
#endif

#include "serveradmin.h"

extern SERVERNAME_HARDENING_LEVEL SmbServerNameHardeningLevel;

//
// Forward declarations
DWORD SMB_ReadX_Helper(SMB_PACKET *pSMB,
                 SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 USHORT MaxCount,
                 USHORT FID,
                 ULONG FileOffset);

DWORD SMB_WriteX_Helper(SMB_PACKET *pSMB,
                 SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 USHORT FID,
                 ULONG Offset,
                 ULONG Requested,
                 BYTE *pData);

HRESULT TCP_TerminateSession(ULONG ulConnectionID);
HRESULT NB_TerminateSession(ULONG ulConnectionID);






BYTE *memmem(const BYTE *src, UINT uiSrcSize, const BYTE *search, UINT uiSearchSize)
{
    BYTE *pRet = NULL;

    //
    // While we have memory keep looking
    while(uiSrcSize && NULL != src) {

        //
        // Find the first character, if it exists see if the
        //    search is there
        BYTE *pTemp = (BYTE *)memchr(src, *search, uiSrcSize);

        if(NULL == pTemp) {
            goto Done;
        }

        uiSrcSize -= (pTemp - src);
        src = pTemp;

        //
        // If the buffers are the same for the next uiSearchSize return the ptr
        if(0 == memcmp(src, search, uiSearchSize)) {
            pRet = (BYTE *)src;
            goto Done;
        } else {
            //
            // Advance beyond this character
            src ++;
            uiSrcSize --;
        }
    }

    Done:
        return pRet;
}

LONG
GetTimeZoneBias()
{
    TIME_ZONE_INFORMATION TzInfo;
    if (GetTimeZoneInformation(&TzInfo) == 0xffffffff) {
        TRACEMSG(ZONE_ERROR,(L"SMBSRV: Error in GetTimeZoneInfo: %d",GetLastError()));
        return 0;
    }
    else
        return TzInfo.Bias;
}

HRESULT RebasePointer(CHAR **pOldPtr, RAPIBuilder *pBuilder, UINT uiIdx, CHAR *pBasePtr)
{
    if(NULL == pOldPtr || NULL == pBuilder || NULL == pBasePtr || 0xFFFFFFFF == uiIdx)
        return E_INVALIDARG;

    if(NULL == *pOldPtr)
        *pOldPtr = NULL;

    CHAR *pNewPtr;
    HRESULT hr = pBuilder->MapFloatToFixed(uiIdx, (BYTE **)&pNewPtr);
    if(FAILED(hr))
        goto Done;

    *pOldPtr = (CHAR *)(pNewPtr - pBasePtr);

    Done:
        return hr;
}




HRESULT AddStringToFloat(const CHAR *pString, CHAR **pDest, CHAR *pBasePtr, RAPIBuilder *pBuilder, UINT *pTotalUsed = NULL)
{
    if(NULL == pDest || NULL == pBuilder)
        return E_INVALIDARG;

    HRESULT hr;

    //do strings first
    if(NULL == pString) {
        *pDest = NULL;
    }
    else {
        UINT uiSize = strlen(pString)+1;

        if(pTotalUsed) {
            *pTotalUsed += uiSize;
        }

        if(FAILED(hr = pBuilder->ReserveFloatBlock(uiSize, pDest, pBasePtr)))
            return hr;

        memmove(*pDest, pString, uiSize);
    }

    return S_OK;
}

HRESULT AddStringToFloat(const CHAR *pString, CHAR **pDest, RAPIBuilder *pBuilder, UINT *uiIdx)
{
    if(NULL == pDest || NULL == pBuilder || NULL == uiIdx)
        return E_INVALIDARG;

    HRESULT hr;

    //do strings first
    if(NULL == pString) {
        *pDest = NULL;
        *uiIdx = 0xFFFFFFFF;
    }
    else {
        UINT uiSize = strlen(pString)+1;
        CHAR *pFloat;
        if(FAILED(hr = pBuilder->ReserveFloatBlock(uiSize, uiIdx)))
            return hr;
        if(FAILED(hr = pBuilder->MapFloatToFixed(*uiIdx, (BYTE **)&pFloat)))
            return hr;

        memmove(pFloat, pString, uiSize);
        *pDest = pFloat;
    }

    return S_OK;
}


UINT FindLength(BYTE **ppString, UINT *pLenRemaining) {
    UINT uiStringLen = 0;
    UINT uiRemaining = *pLenRemaining;
    BYTE *pString = *ppString;

    while(uiStringLen <= uiRemaining && 0!=pString[uiStringLen])
         uiStringLen ++;

    if(uiStringLen > uiRemaining)
        return 0xFFFFFFFF;
    else  {
        *ppString = pString;
        *pLenRemaining -= uiStringLen;
        ASSERT(*pLenRemaining >= 0);
        return uiStringLen;
    }
}




DWORD SMB_Com_Session_Setup_ANDX_LM(SMB_PACKET *pSMB,
                                    SMB_PROCESS_CMD *pRequest,
                                    SMB_PROCESS_CMD *pResponse,
                                    UINT *puiUsed)
{
    DWORD dwRet = 0;
    SMB_COM_SESSION_SETUP_REQUEST_NTLM *pSessionRequest =
        (SMB_COM_SESSION_SETUP_REQUEST_NTLM *)pRequest->pDataPortion;
    SMB_COM_SESSION_SETUP_RESPONSE_NTLM *pSessionResponse =
        (SMB_COM_SESSION_SETUP_RESPONSE_NTLM *)pResponse->pDataPortion;
    UINT uiLeftInRequest = 0;
    BYTE *pEndOfRequest = NULL;
    BOOL fIsGuest = TRUE;
    BYTE         *pNativeOS = NULL;
    BYTE         *pNativeLanman = NULL;

    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    StringTokenizer SecurityTokens;

    //
    // Verify that we have enough data to satisfy requests
    if(pRequest->uiDataSize <= sizeof(SMB_COM_SESSION_SETUP_REQUEST_NTLM)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-COM_SESSION_SETUP_ANDX -- requst too large!!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(pResponse->uiDataSize <= sizeof(SMB_COM_SESSION_SETUP_RESPONSE_NTLM)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-COM_SESSION_SETUP_ANDX -- response too large!!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    uiLeftInRequest = pRequest->uiDataSize - sizeof(SMB_COM_SESSION_SETUP_REQUEST_NTLM);
    pEndOfRequest = (BYTE *)(pSessionRequest+1);

    //
    // Find our connection state
    pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB);
    if(!pMyConnection) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: COM_SESSION_SETUP_ANDX: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }


    //
    // Put in any capabilities they support/want
    if(0 != (pSMB->pInSMB->Flags2 & SMB_FLAGS2_UNICODE)) {
        pMyConnection->SetUnicode(TRUE);
    }

    //
    // Clean up the response
    memset(pSessionResponse, 0, sizeof(SMB_COM_SESSION_SETUP_RESPONSE_NTLM));


    if(13 == pSessionRequest->ANDX.WordCount) {
        BYTE *pInsensitivePass = NULL;
        BYTE *pSensitivePass = NULL;

        // At the end we will find the security information
        SecurityTokens.Reset(pEndOfRequest, pRequest->uiDataSize-sizeof(SMB_COM_SESSION_SETUP_REQUEST_NTLM));

        if(FAILED(SecurityTokens.GetByteArray(&pInsensitivePass,
                                              pSessionRequest->CaseInsensitivePasswordLength))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: COM_SESSION_SETUP_ANDX: -- error getting insensitive password!"));
            ASSERT(FALSE);
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        if(FAILED(SecurityTokens.GetByteArray(&pSensitivePass,
                                              pSessionRequest->CaseSensitivePasswordLength))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: COM_SESSION_SETUP_ANDX: -- error getting sensitive password!"));
            ASSERT(FALSE);
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Handle user name
        if(TRUE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
            WCHAR *pUserName = NULL;
            UINT uiNameLen;
            if(FAILED(SecurityTokens.GetUnicodeString(&pUserName, &uiNameLen))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: COM_SESSION_SETUP_ANDX: -- error getting username!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
            if(FAILED(pMyConnection->SetUserName(pUserName))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: COM_SESSION_SETUP_ANDX: -- error setting username!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
        } else {
            CHAR *pUserName = NULL;
            UINT uiNameLen;
            if(FAILED(SecurityTokens.GetString(&pUserName, &uiNameLen))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: COM_SESSION_SETUP_ANDX: -- error getting username!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
            if(uiNameLen >1 && FAILED(pMyConnection->SetUserName(StringConverter(pUserName).GetString()))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: COM_SESSION_SETUP_ANDX: -- error setting username!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
        }

        //
        // See if they put in a password here
        if(0 != pSessionRequest->CaseInsensitivePasswordLength) {
            BYTE *pPassword = (BYTE *)(pSessionRequest + 1);

            if(pResponse->uiDataSize <= sizeof(SMB_COM_SESSION_SETUP_RESPONSE_NTLM)+pSessionRequest->CaseInsensitivePasswordLength) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV-COM_SESSION_SETUP_ANDX -- response too large!!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            if(TRUE == VerifyPassword(pMyConnection, pPassword, pSessionRequest->CaseInsensitivePasswordLength)) {
                TRACEMSG(ZONE_SECURITY, (L"SMBSRV COM_SESSION_SETUP_ANDX: access granted"));
                pMyConnection->SetGuest(FALSE);
            } else {
                 CReg RegAllowAll;
                 BOOL fAllowAll = FALSE;
                 if(TRUE == RegAllowAll.Open(HKEY_LOCAL_MACHINE, L"Services\\SMBServer\\Shares")) {
                     fAllowAll = !(RegAllowAll.ValueDW(L"UseAuthentication", TRUE));
                 }

                 if(TRUE == fAllowAll) {
                    RETAILMSG(1, (L"SMBSRV:  Even though this user didnt provide proper credentials we are letting in because of UseAuthentication reg setting"));
                    pMyConnection->SetGuest(FALSE);
                 } else {
                     TRACEMSG(ZONE_SECURITY, (L"SMBSRV COM_SESSION_SETUP_ANDX: access DENIED"));
                     dwRet = ERROR_CODE(STATUS_WRONG_PASSWORD);
                     goto Done;
                 }
            }
        }

        {
        StringConverter NativeOS;
        StringConverter NativeLanman;
        UINT            uiNativeOS;
        UINT            uiNativeLanman;
        UINT  uiLeftInResponse = pResponse->uiDataSize - sizeof(SMB_COM_SESSION_SETUP_RESPONSE_NTLM);
        BYTE *pEndOfResponse = (BYTE *)(pSessionResponse+1);

        pSessionResponse->Action = 0;//(TRUE == fIsGuest?1:0);
        pSessionResponse->ByteCount = 0;

        NativeOS.append(L"Windows CE");
        NativeLanman.append("Windows CE");

        pNativeOS = NativeOS.NewSTRING(&uiNativeOS, pMyConnection->SupportsUnicode(pSMB->pInSMB));
        pNativeLanman = NativeLanman.NewSTRING(&uiNativeLanman, pMyConnection->SupportsUnicode(pSMB->pInSMB));

        if(NULL != pNativeOS && NULL != pNativeLanman) {
            if((UINT)pEndOfResponse % 2) {
                *pEndOfResponse = 0;
                pEndOfResponse ++;
                pSessionResponse->ByteCount ++;
                uiLeftInResponse --;
            }

            memcpy(pEndOfResponse, pNativeOS, uiNativeOS);
                pEndOfResponse += uiNativeOS;
                pSessionResponse->ByteCount += uiNativeOS;
                uiLeftInResponse -= uiNativeOS;

            if((UINT)pEndOfResponse % 2) {
                *pEndOfResponse = 0;
                pEndOfResponse ++;
                pSessionResponse->ByteCount ++;
                uiLeftInResponse --;
            }
            memcpy(pEndOfResponse, pNativeLanman, uiNativeLanman);
                pEndOfResponse += uiNativeLanman;
                pSessionResponse->ByteCount += uiNativeLanman;
                uiLeftInResponse -= uiNativeLanman;
        } else {
            ASSERT(FALSE);
            goto Done;
        }
        dwRet = 0;

        //
        // Fill in word count -- subtract 3 for the WordCount byte and ByteCount(which doesnt count as a word)
        pSessionResponse->ANDX.WordCount = (sizeof(SMB_COM_SESSION_SETUP_RESPONSE_NTLM)-3)/sizeof(WORD);
        pSessionResponse->ANDX.AndXCommand = 0xFF; //assume we are the last command
        pSessionResponse->ANDX.AndXReserved = 0;
        pSessionResponse->ANDX.AndXOffset = 0;
        *puiUsed = sizeof(SMB_COM_SESSION_SETUP_RESPONSE_NTLM) + pSessionResponse->ByteCount;
        }
    } else {
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_ACCESS_DENIED);
        goto Done;
   }

    Done:
        if(NULL != pNativeOS) {
            LocalFree(pNativeOS);
        }
        if(NULL != pNativeLanman) {
            LocalFree(pNativeLanman);
        }
        return dwRet;
}



DWORD SMB_Com_Session_Setup_ANDX_NTLM(SMB_PACKET *pSMB,
                                      SMB_PROCESS_CMD *pRequest,
                                      SMB_PROCESS_CMD *pResponse,
                                      UINT *puiUsed)
{
    DWORD dwRet = 0;
    SMB_COM_SESSION_SETUP_REQUEST_EXTENDED_NTLM *pSessionRequest =
        (SMB_COM_SESSION_SETUP_REQUEST_EXTENDED_NTLM *)pRequest->pDataPortion;
    SMB_COM_SESSION_SETUP_RESPONSE_EXTENED_NTLM *pSessionResponse =
        (SMB_COM_SESSION_SETUP_RESPONSE_EXTENED_NTLM *)pResponse->pDataPortion;
    UINT uiLeftInResponse = 0;
    BYTE *pEndOfResponse = NULL;

    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    StringTokenizer SecurityTokens;
    CtxtHandle    newContextHandle;
    SecBufferDesc secBufferDescIn;
    SecBufferDesc secBufferDescOut;
    SecBuffer     secBufferIn;
    SecBuffer     secBufferOut;
    ULONG         ulContextAttrs;
    TimeStamp     TimeExpire;
    SECURITY_STATUS secStatus;
    SECURITY_STATUS qcaStatus;
    NTSTATUS   status;  
    BYTE          PassedToken[1024];
    BYTE         *pPassedToken = PassedToken;
    BYTE          OutToken[1024];
    BYTE         *pOutToken = OutToken;
    BYTE         *pUnAlignedToken = NULL;
    SecPkgInfo   *pPackageInfo = NULL;
    BOOL          fIsGuest = FALSE;
    BYTE         *pNativeOS = NULL;
    BYTE         *pNativeLanman = NULL;

    //
    // Verify that we have enough data to satisfy requests
    if(pRequest->uiDataSize <= sizeof(SMB_COM_SESSION_SETUP_REQUEST_EXTENDED_NTLM)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-COM_SESSION_SETUP_ANDX -- requst too large!!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto SendError;
    }
    if(pResponse->uiDataSize <= sizeof(SMB_COM_SESSION_SETUP_RESPONSE_EXTENED_NTLM)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-COM_SESSION_SETUP_ANDX -- response too large!!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto SendError;
    }
    uiLeftInResponse = pResponse->uiDataSize - sizeof(SMB_COM_SESSION_SETUP_RESPONSE_EXTENED_NTLM);
    pEndOfResponse = (BYTE *)(pSessionResponse+1);

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: COM_SESSION_SETUP_ANDX: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Put in any capabilities they support/want
    if(0 != (pSMB->pInSMB->Flags2 & SMB_FLAGS2_UNICODE)) {
        pMyConnection->SetUnicode(TRUE);
    }

    //
    // Mark that we are using NT status codes
    pResponse->pSMBHeader->Flags2 |= SMB_FLAGS2_NT_STATUS_CODE;
    pSMB->pInSMB->Flags2 |= SMB_FLAGS2_NT_STATUS_CODE;


    //
    // Clean up the response
    memset(pSessionResponse, 0, sizeof(SMB_COM_SESSION_SETUP_RESPONSE_EXTENED_NTLM));

    // At the end we will find the security information
    SecurityTokens.Reset((BYTE *)(pSessionRequest+1), pRequest->uiDataSize-sizeof(SMB_COM_SESSION_SETUP_REQUEST_EXTENDED_NTLM));

    //
    // If we couldnt find the auth information, we must create it out for ourselves
    if(!pMyConnection->m_fContextSet) {
        CredHandle CredHandle;
        TimeStamp TimeStamp;

        //
        // Call into SSPI to get the credentials handle -- cache off the handle for
        //    use later
        secStatus = AcquireCredentialsHandle(NULL,
                                             L"Negotiate",
                                             SECPKG_CRED_INBOUND,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL,
                                             &CredHandle,
                                             &TimeStamp);
        //
        // On error
        if(SEC_E_OK != secStatus) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRVR SessionSetup: error calling AcquireCredentialsHandle (%d)", secStatus));
            goto SendError;
        }

        //
        // Add the handle to our list
        //
        pMyConnection->m_Credentials = CredHandle;
        pMyConnection->m_fContextSet = FALSE;
        pMyConnection->SetGuest(TRUE);
        IFDBG(pMyConnection->m_uiAcceptCalled = 0;);

        //
        // Context set must be false so we dont pass in a handle to ASC
        ASSERT(FALSE == pMyConnection->m_fContextSet);

    }


    //
    // At this point in SessionSetup it doesnt really matter if we just
    //   created the credentials handle or not... everything is the same to us
    //   we just pass blobs back and forth to SSPI
    //
    // Setup SecBufferDec for AcceptSecurityContext
    if(FAILED(SecurityTokens.GetByteArray(&pUnAlignedToken, pSessionRequest->PasswordLength))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRVR SessionSetup: Error -- not enough data to get password blob!!"));
        goto SendError;
    }
    if(pSessionRequest->PasswordLength > sizeof(PassedToken)) {
        pPassedToken = new BYTE[pSessionRequest->PasswordLength];
        if(NULL == pPassedToken) {
            goto SendError;
        }
    }

    //
    // Query for maximum token size (for our out buffer)
    if(SEC_E_OK != QuerySecurityPackageInfo(L"Negotiate", &pPackageInfo)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRVR SessionSetup: Error -- cant get max token size!!"));
        goto SendError;
    }
    if(sizeof(OutToken) < pPackageInfo->cbMaxToken) {
        pOutToken = new BYTE[pPackageInfo->cbMaxToken];
        if(NULL == pOutToken) {
            goto SendError;
        }
    }

    //
    // The memory must be aligned for NTLM to be happy
    memcpy(pPassedToken, pUnAlignedToken, pSessionRequest->PasswordLength);

    secBufferDescIn.ulVersion = SECBUFFER_VERSION;
    secBufferDescIn.cBuffers = 1;
    secBufferDescIn.pBuffers = &secBufferIn;
    secBufferIn.cbBuffer = pSessionRequest->PasswordLength;
    secBufferIn.BufferType = SECBUFFER_TOKEN;
    secBufferIn.pvBuffer = pPassedToken;

    secBufferDescOut.ulVersion = SECBUFFER_VERSION;
    secBufferDescOut.cBuffers = 1;
    secBufferDescOut.pBuffers = &secBufferOut;
    secBufferOut.cbBuffer = pPackageInfo->cbMaxToken;
    secBufferOut.BufferType = SECBUFFER_TOKEN;
    secBufferOut.pvBuffer = pOutToken;

    IFDBG(pMyConnection->m_uiAcceptCalled++;);
    ASSERT(pMyConnection->m_uiAcceptCalled <= 2);
    TRACEMSG(ZONE_SECURITY, (L"SMBSRV: ASC Handle: 0x%x   Connection: %d  MID: %d  PID: %d", (UINT)&(pMyConnection->m_Credentials), pSMB->ulConnectionID, pSMB->pInSMB->Mid, pSMB->pInSMB->Pid));
    secStatus = AcceptSecurityContext(&pMyConnection->m_Credentials,
                                       pMyConnection->m_fContextSet?&pMyConnection->m_ContextHandle:NULL,
                                       &secBufferDescIn,
                                       ASC_REQ_MUTUAL_AUTH | ASC_REQ_FRAGMENT_TO_FIT | ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_ALLOW_NULL_SESSION,
                                       SECURITY_NATIVE_DREP,
                                       &newContextHandle,
                                       &secBufferDescOut,
                                       &ulContextAttrs,
                                       &TimeExpire);

    ASSERT(pMyConnection->IsGuest());
    if(SEC_E_OK == secStatus) {
        BOOL fAllow = FALSE;

        SecPkgContext_Names ctxName;
        TRACEMSG(ZONE_SECURITY, (L"SMBSRVR Security: verified user with NTLM (doesnt mean they have permission.. but they are who they say they are!!"));
        ASSERT(TRUE == pMyConnection->m_fContextSet);
        pMyConnection->m_ContextHandle = newContextHandle;

        if(SEC_E_OK != QueryContextAttributes(&pMyConnection->m_ContextHandle, SECPKG_ATTR_NAMES, &ctxName)) {
            TRACEMSG(ZONE_SECURITY, (L"SMBSRVR Security: verified user with NTLM but couldnt get the user name! treating as guest"));
            ASSERT(FALSE);
        } else {
            if(0 == wcscmp(L"", ctxName.sUserName)) {
                pMyConnection->SetGuest(TRUE);
                TRACEMSG(ZONE_SECURITY, (L"   -- Authed Guest (NULL Session)"));
            } else {
                fAllow = TRUE;
                 pMyConnection->SetGuest(FALSE);
                TRACEMSG(ZONE_SECURITY, (L"   -- User: %s verified", ctxName.sUserName));
                pMyConnection->SetUserName(ctxName.sUserName);
             }
            FreeContextBuffer(ctxName.sUserName);
        }

     TRACEMSG(ZONE_SECURITY, (L"SMBSRVR Security: SPN checking here NTLM"));

     if (SmbServerNameHardeningLevel != SmbServerNameNoCheck)
     {
        SecPkgContext_ClientSpecifiedTarget targetName;
        PWSTR serverName = NULL;
        qcaStatus = QueryContextAttributes(
                                    &pMyConnection->m_ContextHandle,
                                    SECPKG_ATTR_CLIENT_SPECIFIED_TARGET,
                                    &targetName);

        status = MapSecurityError(qcaStatus);
        if (STATUS_SUCCESS == status)
        {
            TRACEMSG(ZONE_SECURITY, (L"Spn check: The SPN name is %s\n", targetName.sTargetName));

            serverName = (PWSTR)targetName.sTargetName;

            status = SrvAdminCheckSpn( serverName, SmbServerNameHardeningLevel);

            if (STATUS_SUCCESS != status)
            {
                TRACEMSG(ZONE_SECURITY, (L"Spn check: fail to match the SPN List.\n", targetName.sTargetName));

                goto SendError;
            }
        }
        else if ((status == STATUS_NOT_SUPPORTED) || 
                (status == STATUS_BAD_NETWORK_PATH)) //SEC_E_TARGET_UNKNOWN  was translated to STATUS_BAD_NETWORK_PATH
        {
            //
            // Authentication protocol do not support SPN query, we take it as success
            //
            TRACEMSG(ZONE_SECURITY, (L"Spn check: Authentication protocol does not support SPN query 0x%x\n", qcaStatus));

            //
            // This represents a downlevel client that did not specify a target, and 
            // we need to reject this request if server is fully hardening checking running.
            //
            if (status == STATUS_BAD_NETWORK_PATH &&
                SmbServerNameHardeningLevel == SmbServerNameFullCheck)
            {
                status = STATUS_ACCESS_DENIED;
                TRACEMSG(ZONE_SECURITY, (L"Spn check: fail to downlevel SSP\n"));
                goto SendError;
            }
        }
        else
        {
            TRACEMSG(ZONE_SECURITY, (L"Spn check: fail to query SPN info 0x%x\n", status));
            goto SendError;
        }
     }
        
        ASSERT(TRUE == pMyConnection->m_fContextSet);
        pMyConnection->m_fContextSet = FALSE;
        FreeCredentialsHandle(&(pMyConnection->m_Credentials));
        DeleteSecurityContext(&(pMyConnection->m_ContextHandle));

        //
        // If we dont know who they are, reject them
        if(!fAllow) {
            TRACEMSG(ZONE_SECURITY, (L"   -- User: REJECTED"));
            goto SendError;
        }
        goto SendSuccess;
    }
    else if(SEC_I_CONTINUE_NEEDED == secStatus) {
        TRACEMSG(ZONE_SECURITY, (L"SMBSRVR Security: need continue -- sending back that in response!!"));
        pMyConnection->m_ContextHandle = newContextHandle;
        pMyConnection->m_fContextSet = TRUE;
        goto SendContinue;
    }
    else if(SEC_E_LOGON_DENIED == secStatus) {
        ASSERT(TRUE == pMyConnection->m_fContextSet);
        FreeCredentialsHandle(&(pMyConnection->m_Credentials));
        DeleteSecurityContext(&(pMyConnection->m_ContextHandle));
        pMyConnection->m_fContextSet = FALSE;

        // Security Token is not needed or filled in
        secBufferOut.cbBuffer = 0;

        //
        // See if we should allow anyone in?
        CReg RegAllowAll;
        BOOL fAllowAll = FALSE;
        if(TRUE == RegAllowAll.Open(HKEY_LOCAL_MACHINE, L"Services\\SMBServer\\Shares")) {
            fAllowAll = !(RegAllowAll.ValueDW(L"UseAuthentication", TRUE));
        }
        if(TRUE == fAllowAll) {
            pMyConnection->SetGuest(FALSE);
           pMyConnection->SetUserName(L"");
           goto SendSuccess;
        } else {
            TRACEMSG(ZONE_SECURITY, (L"SMBSRVR Security: denied access to this user!!!!"));
            pMyConnection->SetGuest(TRUE);
            goto SendError;
        }
    }
    else {
        TRACEMSG(ZONE_ERROR, (L"SMBSRVR Security: unknown response from AcceptSecurityContext (0x%x)!!", secStatus));
        //ASSERT(FALSE);

        if(TRUE == pMyConnection->m_fContextSet) {
            FreeCredentialsHandle(&(pMyConnection->m_Credentials));
            DeleteSecurityContext(&(pMyConnection->m_ContextHandle));
        }
        TRACEMSG(ZONE_SECURITY, (L"SMBSRVR Security: denied access to this user!!!!"));
        pMyConnection->m_fContextSet = FALSE;
        pMyConnection->SetGuest(TRUE);
        goto SendError;
   }

    //
    // If we get here something errored!
    ASSERT(FALSE);
    goto SendError;

    //
    // From here on down we are in return mode -- there are 3 possibilities
    //  Continue -- continue on the session with security blob
    //  Error    -- fail the session
    //  Success  -- verified the user
    SendContinue:
        {
            StringConverter NativeOS;
            StringConverter NativeLanman;
            UINT            uiNativeOS;
            UINT            uiNativeLanman;

            pSessionResponse->Action = 0;//(TRUE == fIsGuest?0:1);
            pSessionResponse->SecurityBlobLength = 0;

            //
            // Copy in our return token
            if((USHORT)secBufferDescOut.pBuffers[0].cbBuffer >= uiLeftInResponse)
                goto SendError;
            pSessionResponse->SecurityBlobLength += (USHORT)secBufferDescOut.pBuffers[0].cbBuffer;
            memcpy(pEndOfResponse, secBufferDescOut.pBuffers[0].pvBuffer, secBufferDescOut.pBuffers[0].cbBuffer);
            uiLeftInResponse -= secBufferDescOut.pBuffers[0].cbBuffer;
            pEndOfResponse += secBufferDescOut.pBuffers[0].cbBuffer;
            pSessionResponse->ByteCount = pSessionResponse->SecurityBlobLength;

            NativeOS.append(L"Windows CE");
            NativeLanman.append("Windows CE");

            pNativeOS = NativeOS.NewSTRING(&uiNativeOS, pMyConnection->SupportsUnicode(pSMB->pInSMB));
            pNativeLanman = NativeLanman.NewSTRING(&uiNativeLanman, pMyConnection->SupportsUnicode(pSMB->pInSMB));

            if(NULL != pNativeOS && NULL != pNativeLanman) {

                if(0 != (UINT)pEndOfResponse % 2) {
                    *pEndOfResponse = 0;
                    pEndOfResponse ++;
                    pSessionResponse->ByteCount ++;
                    uiLeftInResponse --;
                }
                memcpy(pEndOfResponse, pNativeOS, uiNativeOS);
                    pEndOfResponse += uiNativeOS;
                    pSessionResponse->ByteCount += uiNativeOS;
                    uiLeftInResponse -= uiNativeOS;

                if(0 != (UINT)pEndOfResponse % 2) {
                    *pEndOfResponse = 0;
                    pEndOfResponse ++;
                    pSessionResponse->ByteCount ++;
                    uiLeftInResponse --;
                }
                memcpy(pEndOfResponse, pNativeLanman, uiNativeLanman);
                    pEndOfResponse += uiNativeLanman;
                    pSessionResponse->ByteCount += uiNativeLanman;
                    uiLeftInResponse -= uiNativeLanman;
            } else {
                goto SendError;
            }

            //
            // At this point, we have constructed our blobs -- send them out
            dwRet = ERROR_CODE(STATUS_MORE_PROCESSING_REQUIRED);

            //
            // Fill in word count -- subtract 3 for the WordCount byte and ByteCount(which doesnt count as a word)
            pSessionResponse->ANDX.WordCount = (sizeof(SMB_COM_SESSION_SETUP_RESPONSE_EXTENED_NTLM)-3)/sizeof(WORD);
            pSessionResponse->ANDX.AndXCommand = 0xFF; //assume we are the last command
            pSessionResponse->ANDX.AndXReserved = 0;
            pSessionResponse->ANDX.AndXOffset = 0;
            *puiUsed = sizeof(SMB_COM_SESSION_SETUP_RESPONSE_EXTENED_NTLM) + pSessionResponse->ByteCount;
        }
        goto Done;

    SendError:
        *puiUsed = 0;

        dwRet = ERROR_CODE(STATUS_LOGON_FAILURE);
        goto Done;

    SendSuccess:
        {
            StringConverter NativeOS;
            StringConverter NativeLanman;
            UINT            uiNativeOS;
            UINT            uiNativeLanman;

            pSessionResponse->SecurityBlobLength = 0;
            pSessionResponse->ByteCount = 0;

            //
            // Copy in our return token
            if((USHORT)secBufferDescOut.pBuffers[0].cbBuffer >= uiLeftInResponse)
                goto SendError;
            pSessionResponse->SecurityBlobLength += (USHORT)secBufferDescOut.pBuffers[0].cbBuffer;
            memcpy(pEndOfResponse, secBufferDescOut.pBuffers[0].pvBuffer, secBufferDescOut.pBuffers[0].cbBuffer);
            uiLeftInResponse -= secBufferDescOut.pBuffers[0].cbBuffer;
            pEndOfResponse += secBufferDescOut.pBuffers[0].cbBuffer;
            pSessionResponse->ByteCount = pSessionResponse->SecurityBlobLength;

            //
            // Copy in the OS strings
            NativeOS.append(L"Windows CE");
            NativeLanman.append("Windows CE");

            pNativeOS = NativeOS.NewSTRING(&uiNativeOS, pMyConnection->SupportsUnicode(pSMB->pInSMB));
            pNativeLanman = NativeLanman.NewSTRING(&uiNativeLanman, pMyConnection->SupportsUnicode(pSMB->pInSMB));

            if(NULL != pNativeOS && NULL != pNativeLanman) {
                if(0 != (UINT)pEndOfResponse % 2) {
                    *pEndOfResponse = 0;
                    pEndOfResponse ++;
                    pSessionResponse->ByteCount ++;
                    uiLeftInResponse --;
                }

                memcpy(pEndOfResponse, pNativeOS, uiNativeOS);
                    pEndOfResponse += uiNativeOS;
                    pSessionResponse->ByteCount += uiNativeOS;
                    uiLeftInResponse -= uiNativeOS;

                if(0 != (UINT)pEndOfResponse % 2) {
                    *pEndOfResponse = 0;
                    pEndOfResponse ++;
                    pSessionResponse->ByteCount ++;
                    uiLeftInResponse --;
                }
                memcpy(pEndOfResponse, pNativeLanman, uiNativeLanman);
                    pEndOfResponse += uiNativeLanman;
                    pSessionResponse->ByteCount += uiNativeLanman;
                    uiLeftInResponse -= uiNativeLanman;
            } else {
                goto SendError;
            }

            //
            // let them in -- give status 0
            dwRet = 0;

            //fill in word count -- subtract 3 for the WordCount byte and ByteCount(which doesnt count as a word)
            pSessionResponse->ANDX.WordCount = (sizeof(SMB_COM_SESSION_SETUP_RESPONSE_EXTENED_NTLM)-3)/sizeof(WORD);
            pSessionResponse->ANDX.AndXCommand = 0xFF; //assume we are the last command
            pSessionResponse->ANDX.AndXReserved = 0;
            pSessionResponse->ANDX.AndXOffset = 0;
            pSessionResponse->Action = 0; 



            *puiUsed = sizeof(SMB_COM_SESSION_SETUP_RESPONSE_EXTENED_NTLM) + pSessionResponse->ByteCount;
        }
        goto Done;

    Done:
        if(pNativeOS) {
            LocalFree(pNativeOS);
        }
        if(pNativeLanman) {
            LocalFree(pNativeLanman);
        }

        if(pPassedToken && pPassedToken != PassedToken) {
            delete [] pPassedToken;
        }
        if(pPassedToken && pOutToken != OutToken) {
            delete [] pOutToken;
        }
        if(NULL != pPackageInfo) {
            FreeContextBuffer(pPackageInfo);
        }
        return dwRet;
}


HRESULT CloseConnectionTransport(ULONG ulConnectionID)
{
    HRESULT hr;

    //
    // Try removing the session from each transport
    if(SUCCEEDED(hr = NB_TerminateSession(ulConnectionID))) {
        goto Done;
    }
    if(SUCCEEDED(hr = TCP_TerminateSession(ulConnectionID))) {
        goto Done;
    }
    hr = E_FAIL;

    Done:
        ASSERT(SUCCEEDED(hr));
        return hr;
}


DWORD SMB_Com_Session_Setup_ANDX(SMB_PACKET *pSMB, SMB_PROCESS_CMD *pRequest, SMB_PROCESS_CMD *pResponse, UINT *puiUsed)
{
    SMB_COM_ANDX_HEADER *pHeader = (SMB_COM_ANDX_HEADER *)pRequest->pDataPortion;
    DWORD dwRet = 0;

    //
    // Add this connection to the global connection list
    //   If the Session Setup fails, remove it
    if(!SMB_Globals::g_pConnectionManager->FindConnection(pSMB)) {
        TRACEMSG(ZONE_DETAIL, (L"SMB_SRV: Creating new connection to active list"));

        //
        // Make sure we dont exceed max # of connections
        if(SMB_Globals::g_pConnectionManager->NumConnections(pSMB->ulConnectionID)+1 > SMB_Globals::g_uiMaxConnections) {
            RETAILMSG(1, (L"SMB_SRV:  Error -- max connections exceeded -- checking for stale connection"));

            ULONG ulConnectionToTerm = SMB_Globals::g_pConnectionManager->FindStaleConnection(SMB_Globals::g_uiAllowBumpAfterIdle);

            if(0xFFFFFFFF != ulConnectionToTerm) {
                 RETAILMSG(1, (L"SMB_SRV:  Recycling stale connection b/c max connections exceeded"));
                 //SMB_Globals::g_pConnectionManager->RemoveConnection(pStaleConnection->ConnectionID(), 0xFFFF);

                 //
                 // Tell the transport to kill the session (and any other sessions)
                 CloseConnectionTransport(ulConnectionToTerm);

            } else {
                RETAILMSG(1, (L"SMB_SRV:  No stale connections, rejecting connection due to too many users"));
                dwRet = ERROR_CODE(STATUS_LOGON_FAILURE);
                goto Done;
            }
        }

        //
        // Add the connection
        if(FAILED(SMB_Globals::g_pConnectionManager->AddConnection(pSMB))) {
           TRACEMSG(ZONE_SMB, (L"SMBSRV-NEGOTIATE: error adding connection ID!"));
           ASSERT(FALSE);
           dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
           goto Done;
        }
    }

    if(13 == pHeader->WordCount) {
        dwRet = SMB_Com_Session_Setup_ANDX_LM(pSMB, pRequest, pResponse, puiUsed);
    } else if(12 == pHeader->WordCount) {
        dwRet =  SMB_Com_Session_Setup_ANDX_NTLM(pSMB, pRequest, pResponse, puiUsed);
    } else {
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    if((0 != dwRet) && (ERROR_CODE(STATUS_MORE_PROCESSING_REQUIRED) != dwRet)) {
        SMB_Globals::g_pConnectionManager->RemoveConnection(pSMB->ulConnectionID,
                                                            pSMB->pInSMB->Uid);
    }
    Done:
        return dwRet;
}



//Info from CIFS9f.DOC
DWORD SMB_Com_Logoff_ANDX(SMB_PACKET *pSMB, SMB_PROCESS_CMD *pRequest, SMB_PROCESS_CMD *pResponse, UINT *puiUsed)
{

        //
        // Remove state for this connection
        SMB_Globals::g_pConnectionManager->RemoveConnection(pSMB->ulConnectionID,
                                                            pSMB->pInSMB->Uid);


        SMB_COM_ANDX_GENERIC_RESPONSE *pMyResponse =
        (SMB_COM_ANDX_GENERIC_RESPONSE *)pResponse->pDataPortion;

        *puiUsed = sizeof(SMB_COM_ANDX_GENERIC_RESPONSE);

        pMyResponse->ByteCount = 0;
        pMyResponse->ANDX.AndXCommand = 0xFF;
        pMyResponse->ANDX.AndXReserved = 0;
        pMyResponse->ANDX.AndXOffset = 0;
        pMyResponse->ANDX.WordCount = 2;

        return 0;
}


DWORD SMB_Com_Negotiate(SMB_PROCESS_CMD *pRequest,
                        SMB_PROCESS_CMD *pResponse,
                        UINT *puiUsed,
                        SMB_PACKET *pSMB)
{
    SMB_COM_NEGOTIATE_CLIENT_REQUEST *pRequestNegHeader =
        (SMB_COM_NEGOTIATE_CLIENT_REQUEST *)pRequest->pDataPortion;
    BYTE *pDialects = (BYTE *)(pRequestNegHeader) + sizeof(SMB_COM_NEGOTIATE_CLIENT_REQUEST);
    USHORT usRemaining = pRequestNegHeader->ByteCount;

    BOOL fHaveNTLM               = FALSE;
    UINT uiNTLMIdx               = -1;
    DWORD dwRet;
    UINT uiDialectIdx = 0;

    //make sure they (the client) arent trying to blow our buffers
    if(usRemaining > (pRequest->uiDataSize - sizeof(SMB_COM_NEGOTIATE_CLIENT_REQUEST))) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_SMB, (L"SMBSRV-NEGOTIATE: NEGOTIATE -- invalid sizes from client"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //parse out dialects (NOTE: we only support NTLM 0.12)
    while(usRemaining) {
        UINT uiStringLen=0;

        if(0 == usRemaining || 0x02 != *pDialects) {
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        pDialects++; usRemaining--;

        while(uiStringLen <= usRemaining && 0!=pDialects[uiStringLen])
            uiStringLen ++;

        if(0 != pDialects[uiStringLen]) {
            TRACEMSG(ZONE_SMB, (L"SMBSRV-NEGOTIATE: NEGOTIATE -- couldnt find Dialect"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        uiStringLen ++; //null

        if(0 == strcmp((char *)pDialects, "NT LM 0.12")) {
            fHaveNTLM = TRUE;
            uiNTLMIdx = uiDialectIdx;
        }

        usRemaining -= uiStringLen;
        pDialects += uiStringLen;
        uiDialectIdx ++;
    }

    // The ordering of which dialect we choose is important -- chose them in order of
    //    age (if ever there are more dialects -- right now there we only have
    //    NTLM 0.12
    if(fHaveNTLM && -1 != uiDialectIdx) {
        SYSTEMTIME SysTime;
        FILETIME   FileTime;

        SMB_COM_NEGOTIATE_SERVER_RESPONSE_DIALECT_LM *pNegPortion =
           (SMB_COM_NEGOTIATE_SERVER_RESPONSE_DIALECT_LM *)pResponse->pDataPortion;

        if(NULL == pResponse->pDataPortion || pResponse->uiDataSize < sizeof(SMB_COM_NEGOTIATE_SERVER_RESPONSE_DIALECT_LM)) {
            TRACEMSG(ZONE_SMB, (L"SMBSRV-NEGOTIATE: NTLM -- not enough data for dialect"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Get the current time, and convert to FILETIME
        GetLocalTime(&SysTime);

        if(0 == SystemTimeToFileTime(&SysTime, &FileTime)) {
          TRACEMSG(ZONE_SMB, (L"SMBSRV-NEGOTIATE: Cant convert SysTime to Filetime"));
          ASSERT(FALSE);
          dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
          goto Done;
        }

        pNegPortion->inner.WordCount = (sizeof(SMB_COM_NEGOTIATE_SERVER_RESPONSE)-1)/sizeof(WORD);
        pNegPortion->inner.DialectIndex = uiNTLMIdx;
        pNegPortion->inner.SecurityMode = 3;//bit 0 = share level security, bit 1 = encrypt passwords
//        pNegPortion->inner.SecurityMode = 2;//bit 0 = share level security, bit 1 = encrypt passwords
        pNegPortion->inner.MaxMpxCount = 50;
        pNegPortion->inner.MaxCountVCs = 1;
        pNegPortion->inner.MaxTransmitBufferSize = SMB_Globals::MAX_PACKET_SIZE;
        pNegPortion->inner.MaxRawSize = SMB_Globals::MAX_PACKET_SIZE;
        pNegPortion->inner.SessionKey = 0;
        pNegPortion->inner.Capabilities = 0;
//       pNegPortion->inner.Capabilities |= CAP_RAW_MODE;
//       pNegPortion->inner.Capabilities |= CAP_MPX_MODE;
        pNegPortion->inner.Capabilities |= CAP_NT_FIND;
        pNegPortion->inner.Capabilities |= CAP_LARGE_FILES;
        pNegPortion->inner.Capabilities |= CAP_NT_SMBS;
        pNegPortion->inner.Capabilities |= CAP_UNICODE;
        pNegPortion->inner.Capabilities |= CAP_LEVEL_II_OPLOCKS;
        pNegPortion->inner.Capabilities |= CAP_STATUS32;


        //<--CAP_EXTENDED_SECURITY put in only if they put extended security in their flags2 field
        TRACEMSG(ZONE_SMB, (L"SMBSRV--NEGOTIATE Caps set to: 0x%x", pNegPortion->inner.Capabilities));


        pNegPortion->inner.SystemTimeLow = FileTime.dwLowDateTime;
        pNegPortion->inner.SystemTimeHigh = FileTime.dwHighDateTime;
        pNegPortion->inner.ServerTimeZone = (USHORT)GetTimeZoneBias();

        pNegPortion->inner.EncryptionKeyLength = 0;
        pNegPortion->ByteCount = 0;

        //
        // LM passwords
        if(!(SMB_FLAGS2_EXTENDED_SECURITY  & pSMB->pInSMB->Flags2)) {
            pNegPortion->inner.EncryptionKeyLength = 8;
            if(0 == CeGenRandom(8, pNegPortion->Blob)) {
                ASSERT(FALSE);
                memset(pNegPortion->Blob, 0, 8);
            }
            if(FAILED(SMB_Globals::g_pConnectionManager->AddChallenge(pSMB->ulConnectionID, pNegPortion->Blob))) {
                ASSERT(FALSE);
            }

            *puiUsed = sizeof(SMB_COM_NEGOTIATE_SERVER_RESPONSE_DIALECT_LM) - 16 + 8; //remove BLOB and add key
            pNegPortion->ByteCount += 8;
        } else {
            BYTE *pBuffer = pNegPortion->Blob;

            pNegPortion->inner.Capabilities |= CAP_EXTENDED_SECURITY;
            ASSERT(16 == sizeof(SMB_Globals::g_ServerGUID));
            memcpy(pBuffer, SMB_Globals::g_ServerGUID, sizeof(SMB_Globals::g_ServerGUID));
            pNegPortion->ByteCount += sizeof(SMB_Globals::g_ServerGUID);
            pBuffer += sizeof(SMB_Globals::g_ServerGUID);
            *puiUsed = sizeof(SMB_COM_NEGOTIATE_SERVER_RESPONSE_DIALECT_LM) - 16 + 16; //remove BLOB and add GUID


            //
            // Copy in a security blob
            CredHandle hNegSecurityHandle;
            TimeStamp ts;
            if(SEC_E_OK != AcquireCredentialsHandle(NULL, L"Negotiate", SECPKG_CRED_INBOUND,NULL,NULL,NULL,NULL,&hNegSecurityHandle, &ts)) {
                 TRACEMSG(ZONE_ERROR, (L"SMBSRV-NEGOTITAE: couldnt get cred handle"));
                 ASSERT(FALSE);
            }

            ULONG Attributes;
            SecBufferDesc OutputToken;
            TimeStamp Expiry;
            SecBuffer OutputBuffer;
            CtxtHandle HandleOut;

            OutputToken.pBuffers = &OutputBuffer;
            OutputToken.cBuffers = 1;
            OutputToken.ulVersion = 0;
            OutputBuffer.pvBuffer = 0;
            OutputBuffer.cbBuffer = 0;
            OutputBuffer.BufferType = SECBUFFER_TOKEN;

            if(SEC_I_CONTINUE_NEEDED == AcceptSecurityContext(&hNegSecurityHandle,
                                                   NULL,NULL,
                                                   ISC_REQ_MUTUAL_AUTH | ISC_REQ_FRAGMENT_TO_FIT | ISC_REQ_ALLOCATE_MEMORY | ASC_REQ_ALLOW_NULL_SESSION,
                                                   SECURITY_NATIVE_DREP,
                                                   &HandleOut, &OutputToken, &Attributes, &Expiry)) {

                memcpy(pBuffer, OutputBuffer.pvBuffer, OutputBuffer.cbBuffer);
                pNegPortion->ByteCount += (USHORT)OutputBuffer.cbBuffer;
                *puiUsed += OutputBuffer.cbBuffer;
                FreeContextBuffer(OutputBuffer.pvBuffer);
                DeleteSecurityContext(&HandleOut);
            }
            FreeCredentialsHandle(&hNegSecurityHandle);

        }
    }
    else {
        TRACEMSG(ZONE_SMB, (L"SMBSRV-NEGOTIATE: we dont have any dialects in common"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    dwRet = 0;
    Done:
        return dwRet;
}



DWORD SMB_TRANS_API_ShareEnum(SMB_PACKET *pSMB,
                               SMB_PROCESS_CMD *_pRawRequest,
                               SMB_PROCESS_CMD *_pRawResponse,
                               UINT *puiUsed,
                               StringTokenizer &RequestTokenizer)
{
    DWORD dwRet;
    WORD wLevel;
    WORD wRecvBufferSize;
    UINT uiNumShares = 0;
    CHAR *pAPIParam = NULL;
    CHAR *pAPIData = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;

    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    if(FAILED(RequestTokenizer.GetString(&pAPIParam))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_ShareEnum -- Error fetching apiParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetString(&pAPIData))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_ShareEnum -- Error fetching smpParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"...processing SMB_TRANS_API_ShareEnum"));
    //
    // Special to ShareEnum are two WORD's  -- level and recv buffer size
    if(FAILED(RequestTokenizer.GetWORD(&wLevel))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ShareEnum -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetWORD(&wRecvBufferSize))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ShareEnum -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ShareEnum: -- cant find connection 0x%x!", pSMB->ulConnectionID));
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Verify the param/data/level to make sure they are what we expected
    if((0 != strcmp("WrLeh", (CHAR *)pAPIParam)) ||
        (0 != strcmp("B13BWz", (CHAR *)pAPIData)) ||
        (1 != wLevel))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ShareEnum -- invalid apiparam for api"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    ASSERT(0 == RequestTokenizer.RemainingSize());

    //
    // Make sure we have more space than they do
    if(wRecvBufferSize > _pRawResponse->uiDataSize) {
        wRecvBufferSize = _pRawResponse->uiDataSize - sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    }
    {
    //make 3 pointers... like this (main body plus params)
    //[SMB][TRANS_RESPONSE][[TRANS_PARAM_HEADER][SMB_SHARE_INFO * uiNumVisibleShares][sz-strings]]
    //   the pointers will point to a PARAM header, the array of SMB_SHARE_INFO's,
    //   and the other to the sz-strings
    RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);
    SMB_RAP_RESPONSE_PARAM_HEADER *pParamHeader;
    SMB_SHARE_INFO_1              *pFirstShareInfo = NULL;
    UINT uiIdx = 0xFFFFFFFF;

    
    if(FAILED(RAPI.ReserveParams(sizeof(SMB_RAP_RESPONSE_PARAM_HEADER), (BYTE**)&pParamHeader))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ShareEnum -- not enough memory in request"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // We wont be alligned if we dont make a gap
    while(0 != ((UINT)RAPI.DataPtr() % sizeof(DWORD)))
        RAPI.MakeParamByteGap(1);

    //
    // Build a RAPI response with our share information
    for(UINT i=0; i<SMB_Globals::g_pShareManager->NumShares(); i++) {
        SMB_SHARE_INFO_1      *pShareInfo;
        Share *pShare = SMB_Globals::g_pShareManager->SearchForShare(i);
        HRESULT hr;
        if(!pShare || pShare->IsHidden() || FAILED(pShare->AllowUser(pMyConnection->UserName(),SEC_READ)))
            continue;

        if(uiIdx == 0xFFFFFFFF)
            uiNumShares = 1;
        else
            uiNumShares ++;

        const CHAR *pRemark = pShare->GetRemark();
        UINT uiRemarkLen = (NULL == pRemark)?1:(strlen(pRemark) + 1);
        CHAR *pUnbasedRemark;

        


        hr = RAPI.ReserveBlock(sizeof(SMB_SHARE_INFO_1), (BYTE**)&pShareInfo);
        if(SUCCEEDED(hr))
            hr = RAPI.ReserveFloatBlock(uiRemarkLen, &uiIdx);
        if(SUCCEEDED(hr))
            hr = RAPI.MapFloatToFixed(uiIdx, (BYTE **)&pUnbasedRemark);
        if(FAILED(hr)) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ShareEnum -- not enough memory in request"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Keep track of the first shareInfo structure
        if(NULL == pFirstShareInfo)
            pFirstShareInfo = pShareInfo;

        //
        // Write in the remark
        if(uiRemarkLen == 1)
            *pUnbasedRemark = (CHAR)0x00;
        else
            memcpy(pUnbasedRemark, pRemark, uiRemarkLen);

        //
        // Populate the share blob
        UINT uiShareNameLen = strlen(pShare->GetShareNameSZ()) + 1;
        pShareInfo->Type = pShare->GetShareType();
        pShareInfo->Remark = pUnbasedRemark; //note: this is not rebased yet!! we have to do that after coalesce()
        pShareInfo->Pad = 0;
        memset(pShareInfo->Netname, 0, sizeof(pShareInfo->Netname));
        memcpy(pShareInfo->Netname, pShare->GetShareNameSZ(), uiShareNameLen);
            ASSERT(pShareInfo->Type <= 3);

    }

    //
    //  Coalesce and rebase pointers
    if(FAILED(RAPI.Coalesce())) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ShareEnum -- internal error with coalesce()!!"));
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Rebase
    if(uiIdx != 0xFFFFFFFF) {
        SMB_SHARE_INFO_1      *pShareTemp = pFirstShareInfo;
        CHAR *pReBasedRemark;
        for(UINT i=0; i<(uiIdx+1); i++) {
            if(FAILED(RAPI.MapFloatToFixed(i, (BYTE **)&pReBasedRemark))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ShareEnum -- internal error with coalesce()!!"));
                ASSERT(FALSE);
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
            if(pShareTemp->Remark)
                pShareTemp->Remark = (char *)(pReBasedRemark - (CHAR *)pFirstShareInfo);
            pShareTemp ++; //advance an entire SHARE_INFO_1!
        }
    }

    //fill out response SMB
    //PERFPERF: we dont HAVE to zero this out -- if we cover all fields
    //   run in profiler to see if its a hot spot
    memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));
    pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);
    pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
    pResponse->TotalDataCount = RAPI.DataBytesUsed();
    pResponse->ParameterCount = RAPI.ParamBytesUsed();
    pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
    pResponse->ParameterDisplacement = 0;
    pResponse->DataCount = RAPI.DataBytesUsed();
    pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
    pResponse->DataDisplacement = 0;
    pResponse->SetupCount = 0;
    pResponse->ByteCount = RAPI.TotalBytesUsed();
    ASSERT(10 == pResponse->WordCount);

    //
    // Fill out the param headers
    pParamHeader->ConverterWord = 0;
    pParamHeader->ErrorStatus = 0;
    pParamHeader->NumberEntries = uiNumShares;
    pParamHeader->TotalEntries = uiNumShares;

    //
    // Set the number of bytes we used
    *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    dwRet = 0;
    }
    Done:
        return dwRet;
}



DWORD SMB_TRANS_API_WNetWkstaGetInfo(
                                    SMB_PACKET *pSMB,
                                    SMB_PROCESS_CMD *_pRawRequest,
                                    SMB_PROCESS_CMD *_pRawResponse,
                                    UINT *puiUsed,
                                    StringTokenizer &RequestTokenizer)
{
    DWORD dwRet = 0;
    CHAR *pAPIParam = NULL;
    CHAR *pAPIData = NULL;
    WORD wLevel;
    WORD wRecvBufferSize;
    SMB_USERINFO_11 *pUserInfo = NULL;
    WNetWkstaGetInfo_RESPONSE_PARAMS *pResponseParams = NULL;
    ActiveConnection  *pMyConnection = NULL;

    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    if(FAILED(RequestTokenizer.GetString(&pAPIParam))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_WNetWkstaGetInfo -- Error fetching apiParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetString(&pAPIData))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_WNetWkstaGetInfo -- Error fetching smpParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"...processing API_WNetWkstaGetInfo"));
    //
    // Verify the param/data/level to make sure they are what we expected

    
    if( (0 != strcmp("WrLh", (CHAR *)pAPIParam)) ||
        (0 != strcmp("zzzBBzz", (CHAR *)pAPIData)))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_WNetWkstaGetInfo -- invalid apiparam for api"));
        dwRet = ERROR_CODE(STATUS_NOT_SUPPORTED);
        goto Done;
    }

    //
    // Fetch request level information
    if(FAILED(RequestTokenizer.GetWORD(&wLevel))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_WNetWkstaGetInfo -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetWORD(&wRecvBufferSize))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_WNetWkstaGetInfo -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Make sure we have more space than they do
    if(wRecvBufferSize > _pRawResponse->uiDataSize) {
        wRecvBufferSize = _pRawResponse->uiDataSize - sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    }

    {
        RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);
        CHAR *pCName = NULL;
        UINT uiCNameLen = 0;
        CHAR UserName[MAX_PATH];

        //
        // Find our connection state
        if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
           TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_WNetWkstaGetInfo: -- cant find connection 0x%x!", pSMB->ulConnectionID));
           ASSERT(FALSE);
           dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
           goto Done;
        }

        //
        // Convert the username to ASCII
        if(0 == WideCharToMultiByte(CP_ACP, 0, pMyConnection->UserName(), -1, UserName, MAX_PATH,NULL,NULL)) {
           TRACEMSG(ZONE_ERROR, (L"Conversion of username (%s) failed!!!", pMyConnection->UserName()));
           ASSERT(FALSE);
           goto Done;
        }


        
        if(FAILED(RAPI.ReserveParams(sizeof(WNetWkstaGetInfo_RESPONSE_PARAMS), (BYTE**)&pResponseParams))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_WNetWkstaGetInfo -- not enough memory in request -- sending back request for more"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // We wont be alligned if we dont make a gap
        while(0 != ((UINT)RAPI.DataPtr() % sizeof(DWORD)))
            RAPI.MakeParamByteGap(1);


        //
        // Allocate enough memory for the response block
        if(FAILED(RAPI.ReserveBlock(sizeof(SMB_USERINFO_11), (BYTE**)&pUserInfo))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_WNetWkstaGetInfo -- not enough memory in request"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Fill in the response parameters
        pResponseParams->ReturnStatus = 0;
        pResponseParams->ConverterWord = 0;
        pResponseParams->TotalBytes = 1000;


        SMB_Globals::GetCName((BYTE **)&pCName, &uiCNameLen);

        //
        // Fill in the data section
        if(FAILED(AddStringToFloat(pCName,&pUserInfo->pComputerName, (CHAR *)pUserInfo, &RAPI))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_WNetWkstaGetInfo -- error adding computername to floating blob"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }


        if(FAILED(AddStringToFloat(UserName,&pUserInfo->pUserName, (CHAR *)pUserInfo, &RAPI))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_WNetWkstaGetInfo -- error adding username to floating blob"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        if(FAILED(AddStringToFloat(SMB_Globals::g_szWorkGroup,&pUserInfo->pLanGroup, (CHAR *)pUserInfo, &RAPI))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_WNetWkstaGetInfo -- error adding langroup to floating blob"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        CHAR *pTempAlignDomain = NULL;      //<for alignment! (reset pointers *AFTER* Coalesce called!)
        CHAR *pTempAlignOtherDomain = NULL; //<for alignment!

        if(FAILED(AddStringToFloat(NULL,&pTempAlignDomain, (CHAR *)pUserInfo, &RAPI))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_WNetWkstaGetInfo -- error adding domain to floating blob"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        if(FAILED(AddStringToFloat(NULL,&pTempAlignOtherDomain, (CHAR *)pUserInfo, &RAPI))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_WNetWkstaGetInfo -- error adding other domains to floating blob"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        pUserInfo->verMajor = CE_MAJOR_VER;
        pUserInfo->verMinor = CE_MINOR_VER;


        //
        // Coalesce the buffer
        if(FAILED(RAPI.Coalesce())) {
             TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_WNetWkstaGetInfo -- COULDNT coalesce buffer!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // *MUST* be done AFTER Coalesce since the stack data has been modified
        pUserInfo->pDomain = pTempAlignDomain;
        pUserInfo->pOtherDomains = pTempAlignOtherDomain;

        //
        //fill out response SMB
        memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));

        //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
        pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);
        pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
        pResponse->TotalDataCount = RAPI.DataBytesUsed();
        pResponse->ParameterCount = RAPI.ParamBytesUsed();
        pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->ParameterDisplacement = 0;
        pResponse->DataCount = RAPI.DataBytesUsed();
        pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->DataDisplacement = 0;
        pResponse->SetupCount = 0;
        pResponse->ByteCount = RAPI.TotalBytesUsed();

        ASSERT(10 == pResponse->WordCount);

        //set the number of bytes we used
        *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
        dwRet = 0;
    }
    Done:
        return dwRet;
}


DWORD SMB_TRANS_API_ServerGetInfo(SMB_PACKET *pSMB, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, StringTokenizer &RequestTokenizer)
{
    DWORD dwRet = 0;
    CHAR *pAPIParam = NULL;
    CHAR *pAPIData = NULL;
    WORD wLevel;
    WORD wRecvBufferSize;

    ServerGetInfo_RESPONSE_PARAMS *pResponseParams = NULL;

    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    if(FAILED(RequestTokenizer.GetString(&pAPIParam))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_ServerGetInfo -- Error fetching apiParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetString(&pAPIData))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_ServerGetInfo -- Error fetching smpParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"...processing API_WNetWkstaGetInfo"));
    //
    // Verify the param/data/level to make sure they are what we expected
    if( (0 != strcmp("WrLh", (CHAR *)pAPIParam)) ||
        ((0 != strcmp("zzzBBzz", (CHAR *)pAPIData)) && (0 != strcmp("B16BBDz", (CHAR *)pAPIData))))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ServerGetInfo -- invalid apiparam for api"));
        dwRet = ERROR_CODE(STATUS_NOT_SUPPORTED);
        goto Done;
    }

    //
    // Fetch request level information
    if(FAILED(RequestTokenizer.GetWORD(&wLevel))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ServerGetInfo -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetWORD(&wRecvBufferSize))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ServerGetInfo -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Make sure we have more space than they do
    if(wRecvBufferSize > _pRawResponse->uiDataSize) {
        wRecvBufferSize = _pRawResponse->uiDataSize - sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    }

    {
        RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);
        CHAR *pCName = NULL;
        UINT uiCNameLen = 0;

        //
        // Get the CName from globals
        if(FAILED(SMB_Globals::GetCName((BYTE **)&pCName, &uiCNameLen))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ServerGetInfo -- not enough memory in request -- sending back request for more"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Reserve parameter information
        if(FAILED(RAPI.ReserveParams(sizeof(ServerGetInfo_RESPONSE_PARAMS), (BYTE**)&pResponseParams))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ServerGetInfo -- not enough memory in request -- sending back request for more"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // We wont be alligned if we dont make a gap, so make one
        while(0 != ((UINT)RAPI.DataPtr() % sizeof(DWORD)))
            RAPI.MakeParamByteGap(1);


        if(0 == wLevel) {
            SMB_SERVER_INFO_0 *pServerInfo = NULL;
            UINT uiToWrite = 0;

            //
            // Allocate enough memory for the response block
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_SERVER_INFO_0), (BYTE**)&pServerInfo))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ServerGetInfo -- not enough memory in request"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            if(uiCNameLen > sizeof(pServerInfo->sv0_name)) {
                uiCNameLen = sizeof(pServerInfo->sv0_name) - 1;
            }

            memcpy(pServerInfo->sv0_name, pCName, uiCNameLen);
            memset(pServerInfo->sv0_name + uiCNameLen, 0, sizeof(pServerInfo->sv0_name) - uiCNameLen);
        }
        else if(1 == wLevel) {
            SMB_SERVER_INFO_1 *pServerInfo = NULL;
            UINT uiToWrite = 0;

            //
            // Allocate enough memory for the response block
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_SERVER_INFO_1), (BYTE**)&pServerInfo))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ServerGetInfo -- not enough memory in request"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            if(uiCNameLen > sizeof(pServerInfo->Name)) {
                uiCNameLen = sizeof(pServerInfo->Name) - 1;
            }

            memcpy(pServerInfo->Name, pCName, uiCNameLen);
            memset(pServerInfo->Name + uiCNameLen, 0, sizeof(pServerInfo->Name) - uiCNameLen);

            pServerInfo->version_major = CE_MAJOR_VER;
            pServerInfo->version_minor = CE_MINOR_VER;
            pServerInfo->Type = SV_TYPE_SERVER | (g_fPrintServer?SV_TYPE_PRINTQ_SERVER:0);

            CHAR *pTempAlign = NULL; //<for alignment!

            //
            // Get the CName
            if(FAILED(SMB_Globals::GetCName((BYTE**)&pCName, &uiCNameLen))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ServerGetInfo -- error getting CName"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            
            if(FAILED(AddStringToFloat(pCName,&pTempAlign, (CHAR *)pServerInfo, &RAPI))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ServerGetInfo -- error server description"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
            if(FAILED(RAPI.Coalesce())) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ServerGetInfo -- error Coalese() failed!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            //
            // Must be done after Coalesce since stack modified!
            pServerInfo->comment_or_master_browser = pTempAlign;
        }
        else {
            ASSERT(FALSE);
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_ServerGetInfo -- level %d not supported", wLevel));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Fill in the response parameters
        pResponseParams->ReturnStatus = 0;
        pResponseParams->ConverterWord = 0;
        pResponseParams->TotalBytes = 1000;

        //
        //fill out response SMB
        memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));

        //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
        pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);
        pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
        pResponse->TotalDataCount = RAPI.DataBytesUsed();
        pResponse->ParameterCount = RAPI.ParamBytesUsed();
        pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->ParameterDisplacement = 0;
        pResponse->DataCount = RAPI.DataBytesUsed();
        pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->DataDisplacement = 0;
        pResponse->SetupCount = 0;
        pResponse->ByteCount = RAPI.TotalBytesUsed();

        ASSERT(10 == pResponse->WordCount);

        //set the number of bytes we used
        *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
        dwRet = 0;
    }
    Done:
        return dwRet;
}



DWORD SMB_TRANS_API_NetShareGetInfo(
                                   SMB_PACKET *pSMB,
                                   SMB_PROCESS_CMD *_pRawRequest,
                                   SMB_PROCESS_CMD *_pRawResponse,
                                   UINT *puiUsed,
                                   StringTokenizer &RequestTokenizer)
{
    DWORD dwRet = 0;
    CHAR *pAPIParam = NULL;
    CHAR *pAPIData = NULL;
    CHAR *pszShareName = NULL;
    BOOL fNotSupported = FALSE;
    WORD wRecvBufferSize;
    WORD wLevel;
    Share *pMyShare = NULL;
    SMBPrintQueue *pPrintQueue = NULL;
    SMB_PRQ_GETINFO_RESPONSE_PARMS *pResponseParams = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;

    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
      TRACEMSG(ZONE_ERROR, (L"SMBSRV: COM_SESSION_SETUP_ANDX: -- cant find connection 0x%x!", pSMB->ulConnectionID));
      ASSERT(FALSE);
      dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
      goto Done;
    }

    if(FAILED(RequestTokenizer.GetString(&pAPIParam))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_NetShareGetInfo -- Error fetching apiParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetString(&pAPIData))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_NetShareGetInfo -- Error fetching smpParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"...processing API_NetShareGetInfo"));
    //
    // Verify the param/data/level to make sure they are what we expected

    
    if((0 != strcmp("zWrLh", (CHAR *)pAPIParam)) ||
        (0 != strcmp("B13BWz", (CHAR *)pAPIData)))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_NetShareGetInfo -- invalid apiparam for api"));
        dwRet = ERROR_CODE(STATUS_NOT_SUPPORTED);
        goto Done;
    }

    //
    // Fetch print queue get info params
    if(FAILED(RequestTokenizer.GetString(&pszShareName))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_NetShareGetInfo -- cant get sharename"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    if(FAILED(RequestTokenizer.GetWORD(&wLevel))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_NetShareGetInfo -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetWORD(&wRecvBufferSize))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_NetShareGetInfo -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Make sure we have more space than they do
    if(wRecvBufferSize > _pRawResponse->uiDataSize) {
        wRecvBufferSize = _pRawResponse->uiDataSize - sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    }

    {
        RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);


        
        if(FAILED(RAPI.ReserveParams(sizeof(SMB_NETSHARE_GETINFO_RESPONSE_PARMS), (BYTE**)&pResponseParams))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_NetShareGetInfo -- not enough memory in request -- sending back request for more"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Setup default error code (this is what WinME sends back)
        pResponseParams->ReturnStatus = 0x41;
        pResponseParams->ConverterWord = 0;
        pResponseParams->AvailableBytes = 0x2900;


        //fill out response SMB
        //PERFPERF: we dont HAVE to zero this out -- if we cover all fields
        //   run in profiler to see if its a hot spot
        memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));

        //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
        pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);
        pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
        pResponse->TotalDataCount = RAPI.DataBytesUsed();
        pResponse->ParameterCount = RAPI.ParamBytesUsed();
        pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->ParameterDisplacement = 0;
        pResponse->DataCount = RAPI.DataBytesUsed();
        pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->DataDisplacement = 0;
        pResponse->SetupCount = 0;
        pResponse->ByteCount = RAPI.TotalBytesUsed();

        ASSERT(10 == pResponse->WordCount);

        //set the number of bytes we used
        *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
        dwRet = 0;
    }
    Done:
        return dwRet;
}


DWORD SMB_TRANS_API_DosPrintQGetInfo(
                                   SMB_PACKET *pSMB,
                                   SMB_PROCESS_CMD *_pRawRequest,
                                   SMB_PROCESS_CMD *_pRawResponse,
                                   UINT *puiUsed,
                                   StringTokenizer &RequestTokenizer)
{
    DWORD dwRet = 0;
    CHAR *pAPIParam = NULL;
    CHAR *pAPIData = NULL;
    CHAR *pQueueTemp = NULL;
    BOOL fNotSupported = FALSE;
    WORD wRecvBufferSize;
    WCHAR *pszPrintQueue = NULL;
    StringConverter PrintQueueConverter;

    WORD wLevel;
    Share *pMyShare = NULL;
    SMBPrintQueue *pPrintQueue = NULL;
    SMB_PRQ_GETINFO_RESPONSE_PARMS *pResponseParams = NULL;

    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    if(FAILED(RequestTokenizer.GetString(&pAPIParam))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo Error fetching apiParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetString(&pAPIData))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo Error fetching smpParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"...processing API_DosPrintQGetInfo"));
    //
    // Verify the param/data/level to make sure they are what we expected

    











    //
    // Fetch print queue get info params
    if(FAILED(RequestTokenizer.GetString(&pQueueTemp))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    if(FAILED(PrintQueueConverter.append(pQueueTemp))) {
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    pszPrintQueue = (WCHAR *)PrintQueueConverter.GetString();

    if(FAILED(RequestTokenizer.GetWORD(&wLevel))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetWORD(&wRecvBufferSize))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Make sure we have more space than they do
    if(wRecvBufferSize > _pRawResponse->uiDataSize) {
        wRecvBufferSize = _pRawResponse->uiDataSize - sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    }

    {
        RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);


        
        if(FAILED(RAPI.ReserveParams(sizeof(SMB_PRQ_GETINFO_RESPONSE_PARMS), (BYTE**)&pResponseParams))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- not enough memory in request -- sending back request for more"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Setup default error code
        pResponseParams->ReturnStatus = -1;
        pResponseParams->ConverterWord = 0;
        pResponseParams->AvailableBytes = 0;

        //
        // Search for the print spool that they specified
        if(NULL == (pMyShare = SMB_Globals::g_pShareManager->SearchForShare(pszPrintQueue))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- the client gave a share name but its not a print queue!"));
            pResponseParams->ReturnStatus = NERR_QNotFound;
            pResponseParams->ConverterWord = 0;
            pResponseParams->AvailableBytes = 0;
            goto FillResponse;

        }

        if(NULL == (pPrintQueue = pMyShare->GetPrintQueue())) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- the client gave a share name but its not a print queue!"));
            pResponseParams->ReturnStatus = NERR_QNotFound;
            pResponseParams->ConverterWord = 0;
            pResponseParams->AvailableBytes = 0;
            goto FillResponse;
        }

        //
        // If the level is not 3 just ask for more data (it will be level 0)
        if(3 == wLevel)
        {
            SMB_PRQINFO_3 *pPrintInfo;

            //
            // We wont be alligned if we dont make a gap
            while(0 != ((UINT)RAPI.DataPtr() % sizeof(DWORD)))
                RAPI.MakeParamByteGap(1);

            
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_PRQINFO_3), (BYTE**)&pPrintInfo))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- not enough memory in request"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            HRESULT hr;

            //
            // Add strings to the floating pool
            hr = AddStringToFloat(pPrintQueue->GetName(),
                                  &pPrintInfo->pszName, (CHAR *)pPrintInfo,
                                  &RAPI);

            if(SUCCEEDED(hr))
            hr = AddStringToFloat(pPrintQueue->GetSepFile(),
                                  &pPrintInfo->pSepFile, (CHAR *)pPrintInfo,
                                  &RAPI);
            if(SUCCEEDED(hr))
            hr = AddStringToFloat(pPrintQueue->GetPrProc(),
                                  &pPrintInfo->pPrProc, (CHAR *)pPrintInfo,
                                  &RAPI);
            if(SUCCEEDED(hr))
            hr = AddStringToFloat(pPrintQueue->GetParams(),
                                  &pPrintInfo->pParams, (CHAR *)pPrintInfo,
                                  &RAPI);
            if(SUCCEEDED(hr))
            hr = AddStringToFloat(pPrintQueue->GetComment(),
                                  &pPrintInfo->pComment, (CHAR *)pPrintInfo,
                                  &RAPI);
            if(SUCCEEDED(hr))
            hr = AddStringToFloat(pPrintQueue->GetPrinters(),
                                  &pPrintInfo->pPrinters, (CHAR *)pPrintInfo,
                                  &RAPI);
            if(SUCCEEDED(hr))
            hr = AddStringToFloat(pPrintQueue->GetDriverName(),
                                  &pPrintInfo->pDriverName, (CHAR *)pPrintInfo,
                                  &RAPI);
            if(SUCCEEDED(hr))
            hr = AddStringToFloat(pPrintQueue->GetDriverData(),
                                  &pPrintInfo->pDriverData, (CHAR *)pPrintInfo,
                                  &RAPI);
            if(FAILED(hr)) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- error adding floating string!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
            pPrintInfo->Priority = pPrintQueue->GetPriority();
            pPrintInfo->StartTime = pPrintQueue->GetStartTime();
            pPrintInfo->UntilTime = pPrintQueue->GetUntilTime();
            pPrintInfo->Status = pPrintQueue->GetStatus();
            pPrintInfo->cJobs = pPrintQueue->GetNumJobs();

            //
            // Coalesce the buffer
            if(FAILED(RAPI.Coalesce())) {
                 TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- COULDNT coalesce buffer!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            pResponseParams->ReturnStatus = 0;
            pResponseParams->ConverterWord = 0;
            pResponseParams->AvailableBytes = RAPI.DataBytesUsed();

        } else if (2 == wLevel) {
            SMB_PRQINFO_2 *pPrintInfo;
            CHAR *pReturnInfoType;
            UINT uiReturnInfoTypeLen;

            //
            // We wont be alligned if we dont make a gap
            while(0 != ((UINT)RAPI.DataPtr() % 4))
                RAPI.MakeParamByteGap(1);

            
            if(FAILED(RAPI.ReserveBlock(sizeof(SMB_PRQINFO_2), (BYTE**)&pPrintInfo))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- not enough memory in request for data block -- reporting back (this is okay)"));
                dwRet = 0;
                pResponseParams->ReturnStatus = ERROR_MORE_DATA;
                pResponseParams->AvailableBytes = SMB_Globals::MAX_PACKET_SIZE;
                pResponseParams->ConverterWord = 0;
                goto FillResponse;
            }

            HRESULT hr = S_OK;

            




            //
            // Add the queue Name
            strcpy(pPrintInfo->Name, "PRNTQUEUE ");

            //
            // Fill in priority etc
            pPrintInfo->Priority = 1;
            pPrintInfo->StartTime = 0;
            pPrintInfo->UntilTime = 0;


            //
            // Add strings to the floating pool
            if(SUCCEEDED(hr))
            hr = AddStringToFloat(pPrintQueue->GetSepFile(),
                                  &pPrintInfo->pSepFile, (CHAR *)pPrintInfo,
                                  &RAPI);
            if(SUCCEEDED(hr))
            hr = AddStringToFloat(pPrintQueue->GetPrProc(),
                                  &pPrintInfo->pPrProc, (CHAR *)pPrintInfo,
                                  &RAPI);

            if(SUCCEEDED(hr))
            hr = AddStringToFloat(NULL,
                                  &pPrintInfo->pDest, (CHAR *)pPrintInfo,
                                  &RAPI);

            if(SUCCEEDED(hr))
            hr = AddStringToFloat(pPrintQueue->GetParams(),
                                  &pPrintInfo->pParams, (CHAR *)pPrintInfo,
                                  &RAPI);

            if(SUCCEEDED(hr))
            hr = AddStringToFloat(NULL,
                                  &pPrintInfo->pDontKnow, (CHAR *)pPrintInfo,
                                  &RAPI);


            if(FAILED(hr)) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- error adding floating string!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            pPrintInfo->Status = pPrintQueue->GetStatus();
            pPrintInfo->AuxCount = 0;


            //
            // See what kind of return info they want (print job info)
            if(FAILED(RequestTokenizer.GetString(&pReturnInfoType, &uiReturnInfoTypeLen))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- return type info string"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            //
            // Loop building up job types
            if(0 == strcmp((CHAR *)pReturnInfoType, "WB21BB16B10zWWzDDz")) \
            {
                ce::list<PrintJob *, PRINTJOB_LIST_ALLOC > JobList;
                UINT uiNumJobs = 0;

                //
                // Build a list of all jobs (NOTE: we must Release() each of these)
                if(FAILED(pPrintQueue->GetJobsInQueue(&JobList))) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- error building list of jobs in queue"));
                    ASSERT(FALSE);
                    dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                    goto Done;
                }

                //
                // Fill in size specific info and then enum through all jobs
                uiNumJobs = JobList.size();
                pPrintInfo->AuxCount = uiNumJobs;

                for(UINT i=0; i<uiNumJobs; i++)
                {
                    SMB_PRQINFO_1 *pInfo1;
                    //
                    // Reserve enough memory for our info type
                    if(FAILED(RAPI.ReserveBlock(sizeof(SMB_PRQINFO_1), (BYTE**)&pInfo1))) {
                        TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- not enough memory in request for data block -- reporting back (this is okay)"));
                        dwRet = 0;
                        pResponseParams->ReturnStatus = ERROR_MORE_DATA;
                        pResponseParams->AvailableBytes = SMB_Globals::MAX_PACKET_SIZE;
                        pResponseParams->ConverterWord = 0;

                        //
                        // Release all the jobs we have
                        while(JobList.size()) {
                            (JobList.front())->Release();
                            JobList.pop_front();
                        }
                        goto FillResponse;  
                    }

                    memset(pInfo1, 0, sizeof(SMB_PRQINFO_1));


                    PrintJob *pPrintJob = JobList.front();
                    JobList.pop_front();

                    ASSERT(pPrintJob);
                    if(NULL != pPrintJob)
                        pInfo1->JobID = pPrintJob->JobID();
                    else
                        pInfo1->JobID = 0xFFFF;


                    //
                    // Put in all STRING values
                    {
                        BYTE *pUserName = NULL;
                        UINT uiSize;
                        StringConverter newString;
                        newString.append(pPrintJob->GetOwnerName());
                        pUserName = newString.NewSTRING(&uiSize, FALSE);
                        if(NULL == pUserName) {
                            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                            goto Done;
                        }
                        strcpy(pInfo1->UserName, (CHAR *)pUserName);
                        LocalFree(pUserName);
                    }
                    strcpy(pInfo1->NotifyName, "NotifyName");
                    strcpy(pInfo1->DataType, "RAW");

                    //
                    // Put in the start time
                    pInfo1->ulSubmitted = pPrintJob->GetStartTime();

                    {
                    const WCHAR *pDta = pPrintJob->GetComment();
                    if(pDta) {
                        StringConverter CommentString;
                        BYTE *pCommentSTRING;
                        UINT uiCommentLen;
                        CommentString.append(pDta);

                        pCommentSTRING = CommentString.NewSTRING(&uiCommentLen, FALSE);

                        if(NULL != pCommentSTRING) {
                            RAPI.ReserveFloatBlock(uiCommentLen, &(pInfo1->pComment), (CHAR *)pPrintInfo);
                            memcpy(pInfo1->pComment, pCommentSTRING, uiCommentLen);
                            delete [] pCommentSTRING;
                        }
                    }

                    char Msg[100];
                    BOOL fPaused = (pPrintJob->GetInternalStatus() & JOB_STATUS_PAUSED);
                    sprintf(Msg, "%sBytes: %d", fPaused?"*":"", pPrintJob->TotalBytesWritten());
                    RAPI.ReserveFloatBlock(strlen(Msg)+1, &(pInfo1->pStatus), (CHAR *)pPrintInfo);
                    strcpy(pInfo1->pStatus, Msg);
                    }

                    pInfo1->Position = 1;
                    pInfo1->Status = pPrintJob->GetStatus();
                    pPrintJob->Release();

                }

            } else {
                //
                // If we are here there is something we should support!
                ASSERT(FALSE);
                pPrintInfo->AuxCount = 0;
            }

            //
            // Coalesce the buffer
            if(FAILED(RAPI.Coalesce())) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQGetInfo TRANACT -- COULDNT coalesce buffer!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            //
            // Fill in the response parameters
            pResponseParams->ReturnStatus = 0;
            pResponseParams->ConverterWord = 0;
            pResponseParams->AvailableBytes = RAPI.DataBytesUsed();
        } else if (0 == wLevel) {
            pResponseParams->ReturnStatus = 0x84B; //request more space
            pResponseParams->ConverterWord = 0;
            pResponseParams->AvailableBytes = RAPI.DataBytesUsed();
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: Unsupported wLevel: %d!", wLevel));
            pResponseParams->ReturnStatus = ERROR_ACCESS_DENIED;
            pResponseParams->ConverterWord = 0;
            pResponseParams->AvailableBytes = RAPI.DataBytesUsed();
        }


        FillResponse:
            //fill out response SMB
            //PERFPERF: we dont HAVE to zero this out -- if we cover all fields
            //   run in profiler to see if its a hot spot
            memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));

            //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
            pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);
            pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
            pResponse->TotalDataCount = RAPI.DataBytesUsed();
            pResponse->ParameterCount = RAPI.ParamBytesUsed();
            pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
            pResponse->ParameterDisplacement = 0;
            pResponse->DataCount = RAPI.DataBytesUsed();
            pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
            pResponse->DataDisplacement = 0;
            pResponse->SetupCount = 0;
            pResponse->ByteCount = RAPI.TotalBytesUsed();

            ASSERT(10 == pResponse->WordCount);

            //set the number of bytes we used
            *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
            dwRet = 0;
    }
    Done:
        return dwRet;
}



DWORD SMB_TRANS_API_DosPrintJobPause(SMB_PACKET *pSMB, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, StringTokenizer &RequestTokenizer)
{
    DWORD dwRet;
    WORD wJobID;

    WORD wRecvBufferSize;
    PrintJob *pPrintJob = NULL;
    CHAR *pAPIParam = NULL;
    CHAR *pAPIData = NULL;

    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    if(FAILED(RequestTokenizer.GetString(&pAPIParam))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_DosPrintJobPause -- Error fetching apiParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetString(&pAPIData))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_DosPrintJobPause -- Error fetching smpParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"...processing DosPrintJobPause"));

    //
    // Read level, job, and function
    if(FAILED(RequestTokenizer.GetWORD(&wJobID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT SMB_TRANS_API_DosPrintJobPause -- DosPrintJobPause -- error getting jobID from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    


    if(NULL != *pAPIData) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT SMB_TRANS_API_DosPrintJobPause -- DosPrintJobPause -- error wrong pAPIData value! (its non null!)"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Verify the param/data/level to make sure they are what we expected
    if(0 != strcmp("W", (CHAR *)pAPIParam))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT SMB_TRANS_API_DosPrintJobPause -- DosPrintJobPause -- invalid apiparam for api"));
        dwRet = ERRNotSupported;
        goto Done;
    }

    //
    // Seek out the print job
    if(FAILED(SMB_Globals::g_pShareManager->SearchForPrintJob(wJobID, &pPrintJob, NULL)) || NULL == pPrintJob) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT SMB_TRANS_API_DosPrintJobPause -- DosPrintJobPause -- invalid request -- cant find print job: %d", wJobID));
        dwRet = ERROR_CODE(STATUS_INVALID_HANDLE);
        goto Done;
    }
    ASSERT(pPrintJob);

    //
    // Set the pause bit
    pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() | JOB_STATUS_PAUSED);

    dwRet = 0;
    {
        wRecvBufferSize = 10;
        SMB_PRINT_JOB_PAUSE_SERVER_RESPONSE *pJobResponse;
        RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);


        if(FAILED(RAPI.ReserveParams(sizeof(SMB_PRINT_JOB_PAUSE_SERVER_RESPONSE), (BYTE **)&pJobResponse))) {
           TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_DosPrintJobPause -- not enough memory in request"));
           dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        RAPI.MakeParamByteGap(1);

        //PERFPERF: we dont HAVE to zero this out -- if we cover all fields
        //   run in profiler to see if its a hot spot
        memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));

        //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
        pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);
        pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
        pResponse->TotalDataCount = RAPI.DataBytesUsed();
        pResponse->ParameterCount = RAPI.ParamBytesUsed();
        pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->ParameterDisplacement = 0;
        pResponse->DataCount = RAPI.DataBytesUsed();
        pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->DataDisplacement = 0;
        pResponse->SetupCount = 0;
        pResponse->ByteCount = RAPI.TotalBytesUsed();

        ASSERT(10 == pResponse->WordCount);

        //fill in the setinfo return structure
        pJobResponse->ErrorCode = 0;

        //set the number of bytes we used
        *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
        dwRet = 0;
    }
    Done:
        if(NULL != pPrintJob) {
            pPrintJob->Release();
        }
        return dwRet;
}


DWORD SMB_TRANS_API_DosPrintJobResume(SMB_PACKET *pSMB, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, StringTokenizer &RequestTokenizer)
{
    DWORD dwRet;
    WORD wJobID;

    WORD wRecvBufferSize;
    PrintJob *pPrintJob = NULL;
    CHAR *pAPIParam = NULL;
    CHAR *pAPIData = NULL;

    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    if(FAILED(RequestTokenizer.GetString(&pAPIParam))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_DosPrintJobResume -- Error fetching apiParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetString(&pAPIData))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_TRANS_API_DosPrintJobResume -- Error fetching smpParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"...processing DosPrintJobResume"));

    //
    // Read level, job, and function
    if(FAILED(RequestTokenizer.GetWORD(&wJobID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT DosPrintJobResume -- error getting jobID from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    


    if(NULL != *pAPIData) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT DosPrintJobResume -- error wrong pAPIData value! (its non null!)"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Verify the param/data/level to make sure they are what we expected
    if(0 != strcmp("W", (CHAR *)pAPIParam))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT DosPrintJobResume -- invalid apiparam for api"));
        dwRet = ERRNotSupported;
        goto Done;
    }

    //
    // Seek out the print job
    if(FAILED(SMB_Globals::g_pShareManager->SearchForPrintJob(wJobID, &pPrintJob, NULL)) || NULL == pPrintJob) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT DosPrintJobResume -- invalid request (print resume)-- cant find job %d", wJobID));
        dwRet = ERROR_CODE(STATUS_INVALID_HANDLE);
        goto Done;
    }
    ASSERT(pPrintJob);

    //
    // Set the pause bit
    pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() & (~JOB_STATUS_PAUSED));

    dwRet = 0;
    {
        wRecvBufferSize = 10;
        SMB_PRINT_JOB_PAUSE_SERVER_RESPONSE *pJobResponse;
        RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);


        if(FAILED(RAPI.ReserveParams(sizeof(SMB_PRINT_JOB_PAUSE_SERVER_RESPONSE), (BYTE **)&pJobResponse))) {
           TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT DosPrintJobResume -- not enough memory in request"));
           dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        RAPI.MakeParamByteGap(1);

        //PERFPERF: we dont HAVE to zero this out -- if we cover all fields
        //   run in profiler to see if its a hot spot
        memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));

        //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
        pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);
        pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
        pResponse->TotalDataCount = RAPI.DataBytesUsed();
        pResponse->ParameterCount = RAPI.ParamBytesUsed();
        pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->ParameterDisplacement = 0;
        pResponse->DataCount = RAPI.DataBytesUsed();
        pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->DataDisplacement = 0;
        pResponse->SetupCount = 0;
        pResponse->ByteCount = RAPI.TotalBytesUsed();

        ASSERT(10 == pResponse->WordCount);

        //fill in the setinfo return structure
        pJobResponse->ErrorCode = 0;

        //set the number of bytes we used
        *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
        dwRet = 0;
    }
    Done:
        if(NULL != pPrintJob) {
            pPrintJob->Release();
        }
        return dwRet;
}


DWORD SMB_TRANS_API_DosPrintQSetInfo(SMB_PACKET *pSMB, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, StringTokenizer &RequestTokenizer)
{
    DWORD dwRet;
    WORD wLevel;
    WORD wJobID;
    WORD wFunctionID;

    WORD wRecvBufferSize;
    WORD wErrorCode = 0;
    PrintJob *pPrintJob = NULL;
    CHAR *pAPIParam = NULL;
    CHAR *pAPIData = NULL;

    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    if(FAILED(RequestTokenizer.GetString(&pAPIParam))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQSetInfo Error fetching apiParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetString(&pAPIData))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: DosPrintQSetInfo Error fetching smpParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"...processing API_DosPrintQSetInfo"));

    //
    // Read level, job, and function
    if(FAILED(RequestTokenizer.GetWORD(&wJobID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT DosPrintQSetInfo -- error getting jobID from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetWORD(&wLevel))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT DosPrintQSetInfo -- error getting Level from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetWORD(&wFunctionID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT DosPrintQSetInfo -- error getting functionID from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    


    if((0 == strcmp("WWzWWDDzzzzzzzzzzlz", (CHAR *)pAPIData) && 3 == wLevel) ||
        (0 == strcmp("WB21BB16B10zWWzDDz", (CHAR *)pAPIData) && 1 == wLevel)) {
    } else {
        wLevel = 0xFF;
    }


    //
    // Verify the param/data/level to make sure they are what we expected
    if((0 != strcmp("WWsTP", (CHAR *)pAPIParam)) || wLevel == 0xFF)
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT DosPrintQSetInfo -- invalid apiparam for api"));
        wErrorCode = ERRNotSupported;
        goto Finished_API_PrntQueueSetInfo;
    }

    //
    // Get the comment field from the transaction (the end of data is
    //    the size of the entire packet - the starting location for the data
    //    please NOTE:  we know that DataOffset is less than uiDataSize per
    //    check at the beginning of TRANSACT
    CHAR *pComment;
    {
        UINT uiDataEnd = _pRawRequest->uiDataSize+sizeof(SMB_HEADER) - pRequest->DataOffset;
        ASSERT(_pRawRequest->uiDataSize+sizeof(SMB_HEADER) >= pRequest->DataOffset);
        StringTokenizer DataTokenizer((UCHAR *)((char *)(_pRawRequest->pSMBHeader) + pRequest->DataOffset),
                                      uiDataEnd);

        if(FAILED(DataTokenizer.GetString(&pComment))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_DosPrintQSetInfo -- DosPrintQSetInfo -- (printq set info) invalid request"));
            wErrorCode = ERRNotSupported;
            goto Finished_API_PrntQueueSetInfo;
        }
    }

    if(FAILED(SMB_Globals::g_pShareManager->SearchForPrintJob(wJobID, &pPrintJob, NULL)) || NULL == pPrintJob) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT DosPrintQSetInfo -- (printq set info) invalid request"));
        wErrorCode = ERRbadfid;
        goto Finished_API_PrntQueueSetInfo;
    }
    //
    // Set the new comment
    ASSERT(pPrintJob && pComment);
    pPrintJob->SetComment(StringConverter(pComment).GetString());

    wErrorCode = 0;
    Finished_API_PrntQueueSetInfo:
    {
        wRecvBufferSize = 10;
        SMB_PRINT_JOB_SET_INFO_SERVER_RESPONSE *pJobResponse;
        RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);


        if(FAILED(RAPI.ReserveParams(sizeof(SMB_PRINT_JOB_SET_INFO_SERVER_RESPONSE), (BYTE **)&pJobResponse))) {
           TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT DosPrintQSetInfo -- not enough memory in request"));
           dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        RAPI.MakeParamByteGap(1);

        //PERFPERF: we dont HAVE to zero this out -- if we cover all fields
        //   run in profiler to see if its a hot spot
        memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));

        //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
        pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);
        pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
        pResponse->TotalDataCount = RAPI.DataBytesUsed();
        pResponse->ParameterCount = RAPI.ParamBytesUsed();
        pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->ParameterDisplacement = 0;
        pResponse->DataCount = RAPI.DataBytesUsed();
        pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
        pResponse->DataDisplacement = 0;
        pResponse->SetupCount = 0;
        pResponse->ByteCount = RAPI.TotalBytesUsed();

        ASSERT(10 == pResponse->WordCount);

        //fill in the setinfo return structure
        pJobResponse->ErrorCode = wErrorCode;
        pJobResponse->ConverterWord = 0;

        //set the number of bytes we used
        *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
        dwRet = 0;
    }
    Done:
        if(NULL != pPrintJob) {
            pPrintJob->Release();
        }
        return dwRet;
}


DWORD SMB_TRANS_API_PrintQueueGetInfo(SMB_PACKET *pSMB, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, StringTokenizer &RequestTokenizer)
{
    DWORD dwRet;
    WORD wLevel;
    WORD wJobID;

    WORD wRecvBufferSize;
    PrintJob *pPrintJob = NULL;
    CHAR *pAPIParam = NULL;
    CHAR *pAPIData = NULL;
    PrintQueueGetInfo_PARAM_STRUCT *pResponseParams = NULL;
    USHORT ReturnStatus = 0;
    USHORT ConverterWord = 0;
    BOOL   fNeedMoreMemory = FALSE;
    UINT   uiUsedTemp = 0;
    UINT   uiTotalUsed = 0;
    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;
     wRecvBufferSize = _pRawResponse->uiDataSize - sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);


    RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);

    //
    // Get a pointer to the params
    if(FAILED(RAPI.ReserveParams(sizeof(PrintQueueGetInfo_PARAM_STRUCT), (BYTE **)&pResponseParams)) || NULL == pResponseParams) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT PrintQueueGetInfo -- not enough memory in request -- nothing we can do but fail"));
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }
    uiTotalUsed += sizeof(PrintQueueGetInfo_PARAM_STRUCT);


    if(FAILED(RequestTokenizer.GetString(&pAPIParam))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: PrintQueueGetInfo Error fetching apiParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetString(&pAPIData))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: PrintQueueGetInfo Error fetching smpParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"...processing SMB_TRANS_API_PrintQueueGetInfo"));

    //
    // Read level, job, and function
    if(FAILED(RequestTokenizer.GetWORD(&wJobID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: PrintQueueGetInfo -- error getting jobID from params on API"));
        goto InvalidJob;
    }
    if(FAILED(RequestTokenizer.GetWORD(&wLevel))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: PrintQueueGetInfo -- error getting Level from params on API"));
        goto InvalidJob;
    }

    //
    // Seek our our printjob
    if(FAILED(SMB_Globals::g_pShareManager->SearchForPrintJob(wJobID, &pPrintJob, NULL)) || NULL == pPrintJob) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- PrintQueueGetInfo invalid request -- couldnt find print job (%d)", wJobID));
        goto InvalidJob;
    }

    


    if(0 != strcmp("WWrLh", (CHAR *)pAPIParam)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: PrintQueueGetInfo -- invalid param"));
        goto Error_Unsupported_Level;
    }

    if(0 != strcmp("WB21BB16B10zWWzDDz", (CHAR *)pAPIData) || 1 != wLevel) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: PrintQueueGetInfo -- invalid data"));
        goto Error_Unsupported_Level;
    }

    if(1 == wLevel) {
        PRJINFO_1 *pJobResponse = NULL;
        char *pParams = NULL;
        char *pStatus = NULL;
        char *pComment = NULL;
        char Msg[100];
        ASSERT(NULL != pPrintJob);
        sprintf(Msg, "QueuedBytes: %d  RecvedBytes: %d", pPrintJob->BytesLeftInQueue(),pPrintJob->TotalBytesWritten());

        //
        // We wont be alligned if we dont make a gap
        while(0 != ((UINT)(RAPI.DataPtr()) % sizeof(DWORD))) {
            RAPI.MakeParamByteGap(1);
            uiTotalUsed++;
        }
        if(FAILED(RAPI.ReserveBlock(sizeof(PRJINFO_1), (BYTE **)&pJobResponse ))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT PrintQueueGetInfo -- not enough memory in request -- we are in trouble, they didnt give us even close to enough memory.  requesting 1000 bytes"));
            fNeedMoreMemory = TRUE;
            uiTotalUsed = 1000;
            goto SendResponse;
        }
        uiTotalUsed+=sizeof(PRJINFO_1);

        //
        // Fill in the strings section
        pJobResponse->JobID = pPrintJob->JobID();

        {
            BYTE *pUserName = NULL;
            UINT uiSize;
            StringConverter newString;
            newString.append(pPrintJob->GetOwnerName());
            pUserName = newString.NewSTRING(&uiSize, FALSE);
            if(NULL == pUserName) {
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
            strcpy(pJobResponse->UserName, (CHAR *)pUserName);
            if(FAILED(AddStringToFloat(NULL,&pParams, (CHAR *)pJobResponse, &RAPI, &uiUsedTemp))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT PrintQueueGetInfo -- error adding params to floating blob -- need more memory"));
                fNeedMoreMemory = TRUE;
            }
            LocalFree(pUserName);
        }
        uiTotalUsed+=uiUsedTemp;
        pJobResponse->Pad = 0;
        if(FAILED(AddStringToFloat(Msg,&pStatus, (CHAR *)pJobResponse, &RAPI, &uiUsedTemp))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT PrintQueueGetInfo -- error adding status to floating blob -- need more memory"));
            fNeedMoreMemory = TRUE;
        }
        uiTotalUsed+=uiUsedTemp;
        sprintf(pJobResponse->NotifyName, "NotifyName");
        sprintf(pJobResponse->DataType, "RAW");
        if(FAILED(AddStringToFloat("BUGBUG: Comment here",&pComment, (CHAR *)pJobResponse, &RAPI, &uiUsedTemp))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT PrintQueueGetInfo -- error adding comment to floating blob -- need more memory"));
            fNeedMoreMemory = TRUE;
        }
        uiTotalUsed+=uiUsedTemp;
        pJobResponse->Position = 0;
        pJobResponse->Status = pPrintJob->GetStatus();
        pJobResponse->ulSubmitted = 0;  
        pJobResponse->ulSize = 0;

        //
        // Coalesce the buffers to make our packet tighter
        if(FAILED(RAPI.Coalesce())) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT PrintQueueGetInfo -- not enough memory in request"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Rewrite pointers to make sure everything points correctly
        pJobResponse->pParams = pParams;
        pJobResponse->pStatus = pStatus;
        pJobResponse->pComment = pComment;
        goto SendResponse;
    }
    ASSERT(FALSE); //nothing should ever be here!


    Error_Unsupported_Level:
        dwRet = ERROR_CODE(STATUS_NOT_SUPPORTED);
        goto SendResponse;
    InvalidJob:
        {
            dwRet = 0;
            fNeedMoreMemory = FALSE;
            BYTE *pRetBuf = NULL;
            ASSERT(0 == RAPI.ParamBytesUsed());
            if(FAILED(RAPI.ReserveParams(3, (BYTE **)&pRetBuf ))) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT PrintQueueGetInfo -- not enough memory in request -- we are in trouble, they didnt give us even close to enough memory.  requesting 1000 bytes"));
                fNeedMoreMemory = TRUE;
                uiTotalUsed = 1000;
            } else {
                pRetBuf[0] = 0x32;
                pRetBuf[1] = 0;
                pRetBuf[2] = 0;
            }
            uiTotalUsed += 3;
        }
        goto SendResponse;
    SendResponse:
        {
        memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));

        //
        // If everything went okay and we dont need more memory go ahead and return
        if(!fNeedMoreMemory)  {
            ASSERT(RAPI.TotalBytesUsed() == uiTotalUsed);

            //
            // Word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
            pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);
            pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
            pResponse->TotalDataCount = RAPI.DataBytesUsed();
            pResponse->ParameterCount = RAPI.ParamBytesUsed();
            pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
            pResponse->ParameterDisplacement = 0;
            pResponse->DataCount = RAPI.DataBytesUsed();
            pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
            pResponse->DataDisplacement = 0;
            pResponse->SetupCount = 0;
            pResponse->ByteCount = RAPI.TotalBytesUsed();

            //
            // Fill in the response parameters (NOTE: they might not be set in the case of error)
            if(NULL != pResponseParams) {
                pResponseParams->ReturnStatus = ReturnStatus;
                pResponseParams->ConverterWord = ConverterWord;
                pResponseParams->TotalBytes = RAPI.DataBytesUsed();
            }
            //
            // Set the number of bytes we need to return
            *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
        } else {

            //
            // If we got here we need to make a request for more memory...
            //   dont send back any data, just the request for more
            pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);
            pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
            pResponse->TotalDataCount = 0;
            pResponse->ParameterCount = RAPI.ParamBytesUsed();
            pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
            pResponse->ParameterDisplacement = 0;
            pResponse->DataCount = 0;
            pResponse->DataOffset = 0;
            pResponse->DataDisplacement = 0;
            pResponse->SetupCount = 0;
            pResponse->ByteCount = RAPI.ParamBytesUsed();

            //
            // Fill in the response parameters
            pResponseParams->ReturnStatus =  ERRqtoobig;
            pResponseParams->ConverterWord = ConverterWord;
            pResponseParams->TotalBytes = uiTotalUsed + 100; //get a little more than we need in case a new job comes in

            //
            // Set the number of bytes we need to return
            *puiUsed = RAPI.ParamBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
        }
        ASSERT(10 == pResponse->WordCount);
        dwRet = 0;
    }

    Done:
        if(NULL != pPrintJob) {
            pPrintJob->Release();
        }
        return dwRet;
}


DWORD SMB_TRANS_API_PrntQueueDelJob(SMB_PACKET *pSMB, SMB_PROCESS_CMD *_pRawRequest, SMB_PROCESS_CMD *_pRawResponse, UINT *puiUsed, StringTokenizer &RequestTokenizer)
{
    DWORD dwRet;
    WORD wJobID;
    WORD wRecvBufferSize = _pRawResponse->uiDataSize - sizeof(SMB_PRINT_JOB_DEL_SERVER_RESPONSE);
    CHAR *pAPIParam = NULL;
    CHAR *pAPIData = NULL;

    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    if(FAILED(RequestTokenizer.GetString(&pAPIParam))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error fetching apiParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(FAILED(RequestTokenizer.GetString(&pAPIData))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error fetching smpParams! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    {
    SMB_PRINT_JOB_DEL_SERVER_RESPONSE *pDelResponse;
    RAPIBuilder RAPI((BYTE *)(pResponse+1), wRecvBufferSize, pRequest->MaxParameterCount, pRequest->MaxDataCount);

    TRACEMSG(ZONE_SMB, (L"...processing API_PrntQueueDelJob"));

    //
    // Verify the param/data/level to make sure they are what we expected
    if((0 != strcmp("", (CHAR *)pAPIData)) ||
        (0 != strcmp("W", (CHAR *)pAPIParam)))
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_PrntQueueDelJob -- invalid apiparam for api"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Read level, job, and function
    if(FAILED(RequestTokenizer.GetWORD(&wJobID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_PrntQueueDelJob -- error getting jobID from params on API"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }


    
    if(FAILED(RAPI.ReserveParams(sizeof(SMB_PRINT_JOB_DEL_SERVER_RESPONSE), (BYTE**)&pDelResponse))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_PrntQueueDelJob -- not enough memory in request -- sending back request for more"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // From this point down, we error out to the Finished_API_PrntQueueDelJob
    //   rather than Done because we know they speak the same language we do
    //
    PrintJob   *pPrintJob = NULL;
    SMBPrintQueue *pPrintQueue = NULL;

    //
    // now find the print job
    if(FAILED(SMB_Globals::g_pShareManager->SearchForPrintJob(wJobID, &pPrintJob, &pPrintQueue)) ||
       NULL == pPrintJob ||
       NULL == pPrintQueue)
    {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_PrntQueueDelJob -- couldnt find print job %d", wJobID));
        dwRet = ERROR_CODE(STATUS_INVALID_HANDLE);
        goto Finished_API_PrntQueueDelJob;
    }

    //
    // Set its abort logic (abort and finished)
    //   NOTE: I dont set finished here because JobsFinished
    //   does that for me (and thats the proper way to get
    //   the FINISHED bit set)
    pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() |
                                 JOB_STATUS_ABORTED);

    //
    // Kill off the job
    pPrintJob->ShutDown();


    // NOTE: we dont delete this job from the TIDState
    //    we could just to be nice to TIDState -- but
    //    when TIDState goes down it will remove our refcnt

    //
    // Remove it from the main job list and invalidate it
    if(FAILED(pPrintQueue->JobsFinished(pPrintJob))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: TRANACT -- SMB_TRANS_API_PrntQueueDelJob -- cant remove job from main print list from the queue -- FID(%d)!", wJobID));
        dwRet = 0;
        goto Finished_API_PrntQueueDelJob;
    }

    //
    // Fill in the response
    pDelResponse->ConverterWord = 0;
    pDelResponse->ErrorCode = 0;

    dwRet = 0;
    Finished_API_PrntQueueDelJob:

    wRecvBufferSize = 10;
    RAPI.MakeParamByteGap(1);

    //PERFPERF: we dont HAVE to zero this out -- if we cover all fields
    //   run in profiler to see if its a hot spot
    memset(pResponse, 0, sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE));

    //word count is 3 bytes (1=WordCount 2=ByteCount) less than the response
    pResponse->WordCount = (sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) - 3) / sizeof(WORD);
    pResponse->TotalParameterCount = RAPI.ParamBytesUsed();
    pResponse->TotalDataCount = RAPI.DataBytesUsed();
    pResponse->ParameterCount = RAPI.ParamBytesUsed();
    pResponse->ParameterOffset = RAPI.ParamOffset((BYTE *)_pRawResponse->pSMBHeader);
    pResponse->ParameterDisplacement = 0;
    pResponse->DataCount = RAPI.DataBytesUsed();
    pResponse->DataOffset = RAPI.DataOffset((BYTE *)_pRawResponse->pSMBHeader);
    pResponse->DataDisplacement = 0;
    pResponse->SetupCount = 0;
    pResponse->ByteCount = RAPI.TotalBytesUsed();

    ASSERT(10 == pResponse->WordCount);

    

    //set the number of bytes we used
    *puiUsed = RAPI.TotalBytesUsed() + sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE);
    dwRet = 0;

     if(NULL != pPrintJob) {
            pPrintJob->Release();
     }
    }
    Done:

        return dwRet;
}

DWORD SMB_Com_Transaction(SMB_PROCESS_CMD *_pRawRequest,
                         SMB_PROCESS_CMD *_pRawResponse,
                         UINT *puiUsed,
                         SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    BYTE *pSMBparam;
    WORD wAPI;
    StringTokenizer RequestTokenizer;
    SMB_COM_TRANSACTION_CLIENT_REQUEST *pRequest = (SMB_COM_TRANSACTION_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_COM_TRANSACTION_SERVER_RESPONSE *pResponse = (SMB_COM_TRANSACTION_SERVER_RESPONSE *)_pRawResponse->pDataPortion;


    //
    // Verify incoming parameters
    if(_pRawRequest->uiDataSize < sizeof(SMB_COM_TRANSACTION_CLIENT_REQUEST) ||
       _pRawResponse->uiDataSize < sizeof(SMB_COM_TRANSACTION_SERVER_RESPONSE) ||
       _pRawRequest->uiDataSize + sizeof(SMB_HEADER) < pRequest->DataOffset) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- transaction -- not enough memory"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(pRequest->WordCount != (14 + pRequest->SetupCount)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error -- word count on transaction has to be 14 + setupcount -- error"));
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    if (pRequest->SetupCount != 0) {
#ifdef SMB_NMPIPE
        HRESULT hr;
        pSMBparam = (BYTE *)(pRequest + 1); //point to the setup params
        RequestTokenizer.Reset(pSMBparam, pRequest->SetupCount*sizeof(WORD));

        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);

        if (pRequest->SetupCount == 2) {
            TRACEMSG(ZONE_SMB, (L"SMBSRV: NamedPipe request received"));
            //the first word is the pipe function and the second word the file id
            WORD wFunc, wFileId;
            hr = RequestTokenizer.GetWORD(&wFunc);
            hr = (FAILED(hr)) ? (hr) : RequestTokenizer.GetWORD(&wFileId);

            if(FAILED(hr)) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error fetching named pipe function params!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
            // Point the pSMBparam into the parameter section of the SMB
            pSMBparam = (BYTE *)_pRawRequest->pSMBHeader + pRequest->ParameterOffset;
            RequestTokenizer.Reset(pSMBparam, pRequest->ParameterCount);

            dwRet = SMB_TRANS_API_HandleNamedPipeFunction(wFunc, wFileId, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer, pSMB);
        }
        else if (pRequest->SetupCount == 3) {
            TRACEMSG(ZONE_SMB, (L"SMBSRV: Mailslot request received. Not supported currently"));
            ASSERT(FALSE);
        }
        else {
            TRACEMSG(ZONE_SMB, (L"SMBSRV: Unknown SetupCount received"));
            ASSERT(FALSE);
        }
#else
        TRACEMSG(ZONE_ERROR, (L"Named pipes not supported!!!"));
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_NOT_SUPPORTED);
#endif

        goto Done;
    }

    //
    // Point the pSMBparam into the parameter section of the SMB
    pSMBparam = (BYTE *)_pRawRequest->pSMBHeader + pRequest->ParameterOffset;
    RequestTokenizer.Reset(pSMBparam, pRequest->ParameterCount);

    //the format for this all API's will be
    //  [WORD][STRING][STRING] <-- strings are null terminated
    //  [wAPI][pAPIParam][pAPIData]
    if(FAILED(RequestTokenizer.GetWORD(&wAPI))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: Error fetching API! from params!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //now do the API call
    TRACEMSG(ZONE_SMB, (L"Processing TransACT-SMB.  API:(%d)", wAPI));
    switch(wAPI) {
        case API_NetShareGetInfo:
            dwRet = SMB_TRANS_API_NetShareGetInfo(pSMB, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
            break;
        case API_DosPrintQGetInfo:
            dwRet = SMB_TRANS_API_DosPrintQGetInfo(pSMB, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
            break;
        case API_WShareEnum:
            dwRet = SMB_TRANS_API_ShareEnum(pSMB, _pRawRequest, _pRawResponse,puiUsed, RequestTokenizer);
            break;
        case API_PrntQueueSetInfo:
            dwRet = SMB_TRANS_API_DosPrintQSetInfo(pSMB, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
            break;
        case API_DosPrintJobPause:
            dwRet = SMB_TRANS_API_DosPrintJobPause(pSMB, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
            break;
        case API_DosPrintJobResume:
            dwRet = SMB_TRANS_API_DosPrintJobResume(pSMB, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
            break;
        case API_PrntQueueGetInfo:
            dwRet = SMB_TRANS_API_PrintQueueGetInfo(pSMB, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
            break;
        case API_PrntQueueDelJob:
            dwRet = SMB_TRANS_API_PrntQueueDelJob(pSMB, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
            break;
        case API_WServerGetInfo:
            dwRet = SMB_TRANS_API_ServerGetInfo(pSMB, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
            break;
        case API_WNetWkstaGetInfo:
            dwRet = SMB_TRANS_API_WNetWkstaGetInfo(pSMB, _pRawRequest, _pRawResponse, puiUsed, RequestTokenizer);
            break;
        default:
            TRACEMSG(ZONE_ERROR, (L"...API NOT SUPPORTED!!! (%d)", wAPI));
            ASSERT(FALSE);
            dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
            break;
    }

    Done:
        return dwRet;
}





DWORD SMB_Com_Tree_Connect(SMB_PROCESS_CMD *pRequest,
                           SMB_PROCESS_CMD *pResponse,
                           UINT *puiUsed,
                           SMB_PACKET *pSMB)
{
    StringConverter PathConverter;
    StringConverter PasswordConverter;
    StringConverter DeviceConverter;
    StringConverter ServiceName;
    StringConverter FileSystem;

    BYTE *pServiceName = NULL;
    UINT uiServiceLen;

    WCHAR *pPath = NULL;
    UINT uiPathLen;

    BYTE *pPassword = NULL;
    WCHAR *pDeviceName = NULL;

    BYTE *pFileSystem = NULL;
    UINT uiFileSystemLen = 0;

    DWORD dwRet = 0;
    Share *pShare;
    ce::smart_ptr<TIDState> pTIDState = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;

    *puiUsed = 0;

    StringTokenizer RequestStrings;

    SMB_COM_TREE_ANDX_CONNECT_CLIENT_REQUEST *pTreeHeader = (SMB_COM_TREE_ANDX_CONNECT_CLIENT_REQUEST *)pRequest->pDataPortion;
    SMB_COM_TREE_ANDX_CONNECT_RESPONSE  *pTreeResponseHeader = (SMB_COM_TREE_ANDX_CONNECT_RESPONSE *)pResponse->pDataPortion;
    HRESULT hr;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
         TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
         ASSERT(FALSE);
         dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
         goto Done;
    }

    //
    // Verify there is some data there
    if(pRequest->uiDataSize < sizeof(SMB_COM_TREE_ANDX_CONNECT_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV SMB_Com_Tree_Connect: Error -- request not larget enough in incoming packet for TREE command!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(pTreeHeader->ByteCount > pRequest->uiDataSize - sizeof(SMB_COM_TREE_ANDX_CONNECT_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV SMB_Com_Tree_Connect: Error -- not enough data left in packet for TREE CONNECT REQUEST (PCNET)!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(pResponse->uiDataSize < sizeof(SMB_COM_TREE_ANDX_CONNECT_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV SMB_Com_Tree_Connect: Error -- not enough data left in outgoing packet for TREE CONNECT REQUEST (PCNET)!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }


    // At the end we will find 3 strings (PASSWORD, PATH, then SERVICE)... NOTE: these are <STRING>
    //NOTE: we can use pTreeHeader->ByteCount with safety because it was verified above
    RequestStrings.Reset((BYTE *)pTreeHeader + sizeof(SMB_COM_TREE_ANDX_CONNECT_CLIENT_REQUEST), pTreeHeader->ByteCount);

    if(FAILED(RequestStrings.GetByteArray(&pPassword, pTreeHeader->PasswordLength))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV SMB_Com_Tree_Connect: Error -- invalid string (password) in tree command!!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Get the path and device name (convert to unicode if required)
    if(TRUE == pMyConnection->SupportsUnicode(pSMB->pInSMB, SMB_COM_TREE_CONNECT_ANDX)) {
        if(FAILED(RequestStrings.GetUnicodeString(&pPath, &uiPathLen))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV SMB_Com_Tree_Connect: Error -- invalid string (path) in tree command!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
    } else {
        CHAR *pPathTemp;
        UINT uiPathLenTemp;

        if(FAILED(RequestStrings.GetString(&pPathTemp, &uiPathLenTemp))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV SMB_Com_Tree_Connect: Error -- invalid string (path) in tree command!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        if(FAILED(PathConverter.append(pPathTemp))) {
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        pPath = (WCHAR *)PathConverter.GetString();
        uiPathLen = PathConverter.Size() + sizeof(WCHAR);
    }


    

    /*if((uiPathLen < uiNameLen + 2)||
       (pPath[uiNameLen+2] != '\\' && pPath[uiNameLen+2] != '/')) {
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(pPath + 2 != (UCHAR *)strstr((char *)pPath , (char *)pName)) {
        TRACEMSG(ZONE_SMB, (L"SMBSRV: Error -- the client is looking for a share not on this machine! (%s)\n", pPath));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    pPath += (uiNameLen + 3); //<--advance beyond the machine name (to get the share out)
    */

    //
    // Search out the share name
    pPath = pPath + (uiPathLen/sizeof(WCHAR));
    while(uiPathLen && *pPath != '\\') {
        pPath --;
        uiPathLen -= sizeof(WCHAR);
    }

    if(*pPath == '\\') {
        pPath++;
    }
    TRACEMSG(ZONE_SMB, (L"SMBSRV SMB_Com_Tree_Connect: Searching for share (%s) in TreeConnect!!", pPath));

    //
    // Find the share
    if(NULL == (pShare = SMB_Globals::g_pShareManager->SearchForShare(pPath))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV SMB_Com_Tree_Connect: Error --   share (%s)!!", pPath));
        dwRet = ERROR_CODE(STATUS_BAD_NETWORK_NAME);
        goto Done;
    }

    //
    // Grab the service name
    if(FAILED(ServiceName.append(pShare->GetServiceName()))) {
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Get the BYTE * representation of the service name
    if(NULL == (pServiceName = ServiceName.NewSTRING(&uiServiceLen, FALSE))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV SMB_Com_Tree_Connect: Error -- invalid string (path) in tree command!!"));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Build the filesystem type
    if(FAILED(FileSystem.append(L"FAT"))) {
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Get its BYTE * reprentation
    if(NULL == (pFileSystem = FileSystem.NewSTRING(&uiFileSystemLen, pMyConnection->SupportsUnicode(pSMB->pInSMB)))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV SMB_Com_Tree_Connect: Error -- invalid string (filesystem) in tree command!!"));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }


    //
    // If the share is protected, dont allow access!
    if(pShare->RequireAuth() && pMyConnection->IsGuest()) {
         TRACEMSG(ZONE_SECURITY, (L"SMBSRV SMB_Com_Tree_Connect: access DENIED"));
         dwRet = ERROR_CODE(STATUS_WRONG_PASSWORD);
         goto Done;
    }

    //
    // At this point we have an authenticated user, but we dont know if they are allowed in for real
    if(FAILED(hr = pShare->AllowUser(pMyConnection->UserName(), SEC_READ))) {
        RETAILMSG(1, (L"SMBSRV: User %s is known but not allowed access to share %s", pMyConnection->UserName(), pShare->GetShareName()));
        dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
        goto Done;
    }

    //
    // Ask the share for a new instance (and verify permissions)
    if(FAILED(hr = pMyConnection->BindToTID(pShare, pTIDState))) {
        if(E_ACCESSDENIED == hr) {
           TRACEMSG(ZONE_SECURITY, (L"SMBSRV SMB_Com_Tree_Connect: Error -- invalid password given!!"));
           dwRet = ERROR_CODE(STATUS_ACCESS_DENIED);
           goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV SMB_Com_Tree_Connect: Error -- could not find new Tid for share!!"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
    }

    TRACEMSG(ZONE_SMB, (L"SMBSRV SMB_Com_Tree_Connect: Using TID %d for Share:%S!!", pTIDState->TID(), pPath));

    //
    // Since we have a Tid, write the value in
    pResponse->pSMBHeader->Tid = pTIDState->TID();

    //
    // ByteCount and WordCount dont count (hense the -3)
    pTreeResponseHeader->ANDX.WordCount = (sizeof(SMB_COM_TREE_ANDX_CONNECT_RESPONSE)-3)/sizeof(WORD);
    pTreeResponseHeader->OptionalSupport = 1;
    pTreeResponseHeader->ByteCount = uiServiceLen + uiFileSystemLen;

    
    memcpy(((BYTE *)pTreeResponseHeader+sizeof(SMB_COM_TREE_ANDX_CONNECT_RESPONSE)), pServiceName, uiServiceLen);
    memcpy(((BYTE *)pTreeResponseHeader+sizeof(SMB_COM_TREE_ANDX_CONNECT_RESPONSE))+uiServiceLen, pFileSystem, uiFileSystemLen);
    *puiUsed = sizeof(SMB_COM_TREE_ANDX_CONNECT_RESPONSE) + uiServiceLen + uiFileSystemLen;

    Done:
        if(0 != dwRet) {
            *puiUsed = 0;
        }
        if(NULL != pServiceName) {
            LocalFree(pServiceName);
        }
        if(NULL != pFileSystem) {
            LocalFree(pFileSystem);
        }
        return dwRet;
}

#pragma pack(1)

struct SMB_COM_IOCTL_CLIENT_REQUEST {
    BYTE WordCount;
    USHORT FileID;
    USHORT IOCTLCategory;
    USHORT IOCTLFunction;
    USHORT TotalParamBytes;
    USHORT TotalDataBytes;
    USHORT MaxParamBytes;
    USHORT MaxDataBytes;
    ULONG TransactTimeout;
    USHORT Pad;
    USHORT ParameterBytes;
    USHORT ParameterOffset;
    USHORT DataBytes;
    USHORT DataOffset;
    USHORT DataCount;
};


struct SMB_COM_IOCTL_SERVER_RESPONSE {
    UCHAR WordCount;
    USHORT TotalParamBytes;
    USHORT TotalDataBytes;
    USHORT NumParamBytes;
    USHORT ParamOffset;
    USHORT ParamByteDisplacement;
    USHORT NumDataBytes;
    USHORT OffsetFromSMBHeaderToDataBytes;
    USHORT DataByteDisplacement;

    //
    // All below dont count in word count
    USHORT TotalBytes; //(this is the SMB_COM_IOCTL_JOB_ID struct + any padding)
    //UCHAR Pad;
};


struct SMB_COM_ECHO_PACKET{ //rather than having a request and response there is just one (they are the same)
    BYTE WordCount;
    USHORT EchoCount;
    USHORT ByteCount;
    //Buffer[ByteCount]
};


struct SMB_COM_IOCTL_JOB_ID
{
    USHORT JobID;
    CHAR ComputerName[16];
    CHAR QueueName[14];
};
#pragma pack()

DWORD
SMB_Com_IOCTL(SMB_PROCESS_CMD *_pRequest,
               SMB_PROCESS_CMD *_pResponse,
               UINT *puiUsed,
               SMB_PACKET *pSMB)
{
    
    SMB_COM_IOCTL_CLIENT_REQUEST *pRequest =
        (SMB_COM_IOCTL_CLIENT_REQUEST *)_pRequest->pDataPortion;
    SMB_COM_IOCTL_SERVER_RESPONSE *pResponse =
        (SMB_COM_IOCTL_SERVER_RESPONSE *)_pResponse->pDataPortion;
    SMB_COM_IOCTL_JOB_ID *pJobRequest = NULL;

    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    ce::smart_ptr<TIDState> pTIDState = NULL;
    SMBPrintQueue *pPrintQueue = NULL;
    PrintJob   *pPrintJob = NULL;
    StringConverter QueueName;
    CHAR *pName = NULL;
    BYTE *pQueueName = NULL;
    UINT uiNameSize;
    UINT uiQueueSize;
    DWORD dwRet = 0;
    *puiUsed = 0;

    //
    // Verify that we have enough memory
    if(_pRequest->uiDataSize < sizeof(SMB_COM_IOCTL_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_IOCTL -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pResponse->uiDataSize < sizeof(SMB_COM_IOCTL_SERVER_RESPONSE)+sizeof(SMB_COM_IOCTL_JOB_ID)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_IOCTL -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Verify this is an IOCTL we process
    if(0x53 != pRequest->IOCTLCategory || 0x60 != pRequest->IOCTLFunction) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_IOCTL -- unknown IOCTL (%d, %d)", pRequest->IOCTLCategory, pRequest->IOCTLFunction));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }


    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a share state
    if(FAILED(pMyConnection->FindTIDState(_pRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_IOCTL -- couldnt find share state!!"));
        dwRet = ERROR_CODE(STATUS_INVALID_HANDLE);
        goto Done;
    }

    //
    // Make sure this is actually a print queue
    if(!pTIDState ||
       !pTIDState->GetShare() ||
       !pTIDState->GetShare()->GetPrintQueue()) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_IOCTL -- the Tid is for a file -- we only do print!!"));
        dwRet = ERROR_CODE(STATUS_INVALID_HANDLE);
        goto Done;
    }

    //
    // Fetch the queue
    if(NULL == (pPrintQueue = pTIDState->GetShare()->GetPrintQueue())) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_IOCTL -- we didnt get a print queue back!!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our print job
    if(FAILED(pPrintQueue->FindPrintJob(pRequest->FileID, &pPrintJob)) || NULL == pPrintJob) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_IOCTL -- couldnt find print job for FID(%d)!", pRequest->FileID));
        dwRet = ERROR_CODE(STATUS_INVALID_HANDLE);
        goto Done;
    }

    //
    // Set a flag saying this has data and wake up the print queue
    //   so it will start printing
    pPrintJob->SetInternalStatus(pPrintJob->GetInternalStatus() | JOB_STATUS_HAS_DATA);
    pPrintQueue->JobReadyForPrint(pPrintJob);

    //
    // Point pJobRequest to 1 byte AFTER the response
    //
    //  memory layout will be [SMB_IOCTL_RESPONSE][1 byte][SMB_IOCTL_JOB_ID]
    pJobRequest = (SMB_COM_IOCTL_JOB_ID *)((BYTE *)(pResponse + 1)+1);

    pResponse->WordCount = (sizeof(SMB_COM_IOCTL_SERVER_RESPONSE) - 3) / sizeof(WORD);
    pResponse->TotalParamBytes = 0;
    pResponse->TotalDataBytes = sizeof(SMB_COM_IOCTL_JOB_ID);
    pResponse->NumParamBytes = 0;
    pResponse->ParamOffset = 0;
    pResponse->ParamByteDisplacement = 0;
    pResponse->NumDataBytes = sizeof(SMB_COM_IOCTL_JOB_ID);
    pResponse->OffsetFromSMBHeaderToDataBytes = ((BYTE*)pJobRequest - (BYTE *)_pResponse->pSMBHeader);
    pResponse->DataByteDisplacement = 0;
    pResponse->TotalBytes = (BYTE*)(pJobRequest+1) - (BYTE *)(pResponse+1);
    ASSERT(pResponse->TotalBytes == sizeof(SMB_COM_IOCTL_JOB_ID) + 1);


    //
    // Setup pJobRequest
    pJobRequest->JobID = pPrintJob->JobID();
    memset(pJobRequest->QueueName, 0, sizeof(pJobRequest->QueueName));
    memset(pJobRequest->ComputerName, 0, sizeof(pJobRequest->ComputerName));

    SMB_Globals::GetCName((BYTE **)&pName, &uiNameSize);

    if(uiNameSize <= sizeof(pJobRequest->ComputerName) - 1) {
        strcpy(pJobRequest->ComputerName, pName);
    } else {
        memcpy(pJobRequest->ComputerName, pName, sizeof(pJobRequest->ComputerName) - 2);
        pJobRequest->ComputerName[sizeof(pJobRequest->ComputerName) - 1] = NULL;
    }

    if(FAILED(QueueName.append(pPrintJob->GetQueueName()))) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_IOCTL -- cant get queue name!!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(NULL == (pQueueName = QueueName.NewSTRING(&uiQueueSize, FALSE))) {
        ASSERT(FALSE);
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_IOCTL -- cant build new string for queue name!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    if(uiQueueSize > sizeof(pJobRequest->QueueName)) {
        uiQueueSize = sizeof(pJobRequest->QueueName)-1;
    }
    memcpy(pJobRequest->QueueName, pQueueName, uiQueueSize);

    *puiUsed = sizeof(SMB_COM_IOCTL_SERVER_RESPONSE) + sizeof(SMB_COM_IOCTL_JOB_ID);
    dwRet = 0;

    Done:
        if(NULL != pPrintJob) {
            pPrintJob->Release();
        }
        if(NULL != pQueueName) {
            LocalFree(pQueueName);
        }
        return dwRet;

}


DWORD
SMB_Com_Echo(SMB_PROCESS_CMD *_pRequest,
               SMB_PROCESS_CMD *_pResponse,
               UINT *puiUsed,
               SMB_PACKET *pSMB)
{
    SMB_COM_ECHO_PACKET *pRequest =
        (SMB_COM_ECHO_PACKET *)_pRequest->pDataPortion;
    SMB_COM_ECHO_PACKET *pResponse =
        (SMB_COM_ECHO_PACKET *)_pResponse->pDataPortion;

    DWORD dwRet = 0;
    *puiUsed = 0;

    //
    // Verify that we have enough memory
    if(_pRequest->uiDataSize < sizeof(SMB_COM_ECHO_PACKET)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Echo -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pResponse->uiDataSize < sizeof(SMB_COM_ECHO_PACKET) + pRequest->ByteCount) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Echo -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    pResponse->WordCount = 1;
    pResponse->EchoCount = 1;
    pResponse->ByteCount = pRequest->ByteCount;
    memcpy(pResponse+1, pRequest+1, pRequest->ByteCount);

    *puiUsed = sizeof(SMB_COM_ECHO_PACKET) + pRequest->ByteCount;
    dwRet = 0;

    Done:
        return dwRet;

}




DWORD
SMB_ComTree_Disconnect(SMB_PROCESS_CMD *_pRequest, SMB_PROCESS_CMD *_pResponse, UINT *puiUsed, SMB_PACKET *pSMB)
{
    SMB_COM_TREE_DISCONNECT_CLIENT_REQUEST *pRequest =
      (SMB_COM_TREE_DISCONNECT_CLIENT_REQUEST *)_pRequest->pDataPortion;
    SMB_COM_TREE_DISCONNECT_CLIENT_RESPONSE *pResponse =
      (SMB_COM_TREE_DISCONNECT_CLIENT_RESPONSE *)_pResponse->pDataPortion;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;

#ifdef DEBUG
    BOOL fFound = FALSE; // <-- this only exists for ASSERTION logic.  just a safety precaution
#endif

    DWORD dwRet = 0;
    *puiUsed = 0;

    //
    // Verify that we have enough memory
    if(_pRequest->uiDataSize < sizeof(SMB_COM_TREE_DISCONNECT_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: ComTreeDisconnect -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pResponse->uiDataSize < sizeof(SMB_COM_TREE_DISCONNECT_CLIENT_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: ComTreeDisconnect -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: ComTreeDisconnect -- cant find connection 0x%x, succeeding anyway (maybe logoff occured)!", pSMB->ulConnectionID));
    }

    //
    // Process the request
    if(pMyConnection && FAILED(pMyConnection->UnBindTID(_pRequest->pSMBHeader->Tid))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: ComTreeDisconnet -- couldnt unbind Tid (%d)",_pRequest->pSMBHeader->Tid));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Alls well  -- send back the response
    pRequest->ByteCount = 0;
    pRequest->WordCount = 0;
    pResponse->ByteCount = 0;
    pResponse->WordCount = 0;
    *puiUsed = sizeof(SMB_COM_TREE_DISCONNECT_CLIENT_RESPONSE);
    dwRet = 0;

    Done:
        return dwRet;
}


DWORD SMB_PacketRead(SMB_PACKET *pSMB,
                 SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed)
{
    DWORD dwRet = 0;
    HRESULT hr = E_FAIL;
    ce::smart_ptr<TIDState> pTIDState;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;

    SMB_READ_CLIENT_REQUEST *pRequest =
            (SMB_READ_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_READ_SERVER_RESPONSE *pResponse =
            (SMB_READ_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    DWORD dwLeftForData = _pRawResponse->uiDataSize - sizeof(SMB_READ_SERVER_RESPONSE);
    DWORD dwToRequest = 0;
    DWORD dwRead = 0;
    BYTE *pDataDest = (BYTE *)(pResponse + 1);

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_READ_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Read -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_READ_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Read -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: SMB_Read request"));


    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Read: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a TID state
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Read -- couldnt find share state!!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Read from it
    if(dwLeftForData <= pRequest->MaxCount) {
        dwLeftForData = pRequest->MaxCount;
    }
    if(pRequest->MaxCount <= dwLeftForData) {
        dwLeftForData = pRequest->MaxCount;
    }

    ASSERT(dwLeftForData <= 0xFFFF);
    if(FAILED(hr = pTIDState->Read(pRequest->FileID, pDataDest, pRequest->FileOffset, dwLeftForData, &dwRead)) && 0 == dwRead) {
        dwRet = ConvertHRToError(hr, pSMB);
        goto Done;
    }
    ASSERT(dwRead <= 0xFFFF);

    //
    // Fill out return params and send back the data
    //
    pResponse->WordCount = 5;
    pResponse->Count = (USHORT)dwRead;
    pResponse->Reserved[0] = 0;
    pResponse->Reserved[1] = 0;
    pResponse->Reserved[2] = 0;
    pResponse->Reserved[3] = 0;
    pResponse->ByteCount = (USHORT)dwRead + 5; //5 is for ByteCount+BufferFormat+DataLength
    pResponse->DataLength = (USHORT)dwRead;

    *puiUsed = sizeof(SMB_READ_SERVER_RESPONSE) + dwRead;
    Done:
        return dwRet;
}


DWORD SMB_ReadX(SMB_PACKET *pSMB,
                 SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed)
{
    SMB_COM_ANDX_HEADER *pANDX = (SMB_COM_ANDX_HEADER *)_pRawRequest->pDataPortion;
    DWORD dwRet;

    //
    // Make sure the return packet can fit
    if(_pRawResponse->uiDataSize < sizeof(SMB_READX_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_ReadX -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Verify we can at least hold an ANDX before reading in
    if(_pRawRequest->uiDataSize < sizeof(SMB_COM_ANDX_HEADER)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_ReadX -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // If the packet has a 12 byte wordcount its "NT" (per cifs9f.doc),
    //   if its 10 its downlevel
    if(12 == pANDX->WordCount) {
        SMB_READX_CLIENT_REQUEST_NT *pRequest =
                (SMB_READX_CLIENT_REQUEST_NT *)_pRawRequest->pDataPortion;
        //
        // Verify that we have enough memory
        if(_pRawRequest->uiDataSize < sizeof(SMB_READX_CLIENT_REQUEST_NT)) {
           TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_ReadX -- not enough memory for request (WC=12)!"));
           dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
           goto Done;
        }
        dwRet = SMB_ReadX_Helper(pSMB, _pRawRequest, _pRawResponse, puiUsed, pRequest->MaxCount, pRequest->FileID, pRequest->FileOffset);

    } else if(10 == pANDX->WordCount) {
        SMB_READX_CLIENT_REQUEST *pRequest =
                (SMB_READX_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
        //
        // Verify that we have enough memory
        if(_pRawRequest->uiDataSize < sizeof(SMB_READX_CLIENT_REQUEST)) {
           TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_ReadX -- not enough memory for request (WC=10)!"));
           dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
           goto Done;
        }
        dwRet = SMB_ReadX_Helper(pSMB, _pRawRequest, _pRawResponse, puiUsed, pRequest->MaxCount, pRequest->FileID, pRequest->FileOffset);
    } else {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV: READX -- wordcount isnt expected"));
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
    }

    Done:
        return dwRet;
}


DWORD SMB_ReadX_Helper(SMB_PACKET *pSMB,
                 SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed,
                 USHORT MaxCount,
                 USHORT FID,
                 ULONG FileOffset)
{
    DWORD dwRet = 0;
    ce::smart_ptr<TIDState> pTIDState;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    HRESULT hr = E_FAIL;

    DWORD dwLeftForData = _pRawResponse->uiDataSize - sizeof(SMB_READX_SERVER_RESPONSE);
    DWORD dwToRequest = 0;
    DWORD dwRead = 0;
    SMB_READX_SERVER_RESPONSE *pResponse =
            (SMB_READX_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    BYTE *pDataDest = (BYTE *)(pResponse + 1);

    TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: SMB_ReadX request"));

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a TID state
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_ReadX -- couldnt find share state!!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Read from it
    if(dwLeftForData <= MaxCount) {
        dwLeftForData = MaxCount;
    }
    if(MaxCount <= dwLeftForData) {
        dwLeftForData = MaxCount;
    }
    if(FAILED(hr = pTIDState->Read(FID, pDataDest, FileOffset, dwLeftForData, &dwRead)) && 0 == dwRead) {
        dwRet = ConvertHRToError(hr, pSMB);
        goto Done;
    }

    //
    // Fill out return params and send back the data
    //
    pResponse->BytesLeft = 0;
    pResponse->Reserved = 0;
    pResponse->DataLength = (USHORT)dwRead;
    pResponse->DataOffset = (BYTE *)pDataDest - (BYTE *)_pRawResponse->pSMBHeader;
    pResponse->Reserved1 = 0;
    pResponse->Reserved2 = 0;
    pResponse->Reserved3 = 0;
    pResponse->ByteCount = (USHORT)dwRead;
    pResponse->ANDX.WordCount = (sizeof(SMB_READX_SERVER_RESPONSE) - 3) / sizeof(WORD);
    ASSERT(12 == pResponse->ANDX.WordCount);

    *puiUsed = sizeof(SMB_READX_SERVER_RESPONSE) + dwRead;

    goto Done;

    Done:
        return dwRet;
}




//
// Information taken from cifs9f.doc page 47
DWORD SMB_NT_Create(SMB_PACKET *pSMB,
                          SMB_PROCESS_CMD *_pRawRequest,
                          SMB_PROCESS_CMD *_pRawResponse,
                          UINT *puiUsed)
{
    StringTokenizer   RequestTokenizer;
    StringConverter   RequestedFile;
    ce::smart_ptr<TIDState>         pTIDState = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    WCHAR            *pRequestedFile = NULL;
    SMBFileStream    *pFileStream = NULL;
    DWORD             dwFileSize;
    DWORD             dwRet = 0;
    DWORD             dwAccess = 0;
    DWORD             dwAttributes = 0;
    DWORD             dwDisposition = 0;
    DWORD             dwShareMode = 0;
    DWORD             dwActionTaken = 0;
    DWORD             dwFlags = 0;
    DWORD             dwTIDMode = SEC_READ;

    // Cached values (to keep from reading in packed struct over and over)
    ULONG             ulcAccessMask;
    DWORD             dwcAttributes;
    DWORD             dwcShareMode;
    DWORD             dwcCreationDisposition;
    DWORD             dwcCreateOptions = 0;
    DWORD             dwcFlags = 0;
    SMBFile_OpLock_PassStruct OpLockPassStruct;
    DWORD             dwWriteCnt = 0;
    BOOL                 fFlippedToOpenExisting = FALSE;

    HRESULT           hr;

    SMB_NT_CREATE_CLIENT_REQUEST *pRequest =
            (SMB_NT_CREATE_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_NT_CREATE_SERVER_RESPONSE *pResponse =
            (SMB_NT_CREATE_SERVER_RESPONSE *)_pRawResponse->pDataPortion;


    //
    //  send the SMBntcreatex request to the debug output
    //
    TRACE_PROTOCOL_SMB_NT_CREATE_REQUEST( pRequest );

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_NT_CREATE_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_NT_Create -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_NT_CREATE_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_NT_Create -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: SMB_NT_Create request"));

    //
    // Figure out the access rights
    ulcAccessMask = pRequest->DesiredAccess;
    dwcAttributes = pRequest->ExtFileAttributes;
    dwcShareMode = pRequest->ShareAccess;
    dwcCreationDisposition = pRequest->CreateDisposition;
    dwcCreateOptions = pRequest->CreateOptions;
    dwcFlags = pRequest->Flags;

   /*if(NT_CREATE_GENERIC_ALL & ulcAccessMask) {
        dwAccess |= GENERIC_ALL;
        IFDBG(ulcAccessMask &= GENERIC_ALL);
    }*/
    if(NT_CREATE_GENERIC_EXECUTE & ulcAccessMask) {
        dwAccess |= GENERIC_READ;
    }
    if(NT_CREATE_GENERIC_READ & ulcAccessMask) {
        dwAccess |= GENERIC_READ;
    }
    if(NT_CREATE_GENERIC_WRITE & ulcAccessMask) {
        dwAccess |= GENERIC_WRITE;
        dwTIDMode |= SEC_WRITE;dwWriteCnt++;
    }


    //
    //  only support "batch" oplocks for now
    //  if "batch" was not selected then indicate NO_OPLOCK
    //  the filesystem manager will figure out whether or not to grant the oplock
    //  NOTE:  Windows NT could also receive a "level2" oplock which is less than "batch"
    //
    if(NT_CREATE_FLAG_OPBATCH & dwcFlags) {
        OpLockPassStruct.dwRequested = NT_CREATE_BATCH_OPLOCK;
    } else {
        OpLockPassStruct.dwRequested = NT_CREATE_NO_OPLOCK;
    }
    OpLockPassStruct.pfnQueuePacket = pSMB->pfnQueueFunction;
    OpLockPassStruct.pfnCopyTranportToken = pSMB->pfnCopyTranportToken;
    OpLockPassStruct.pfnDeleteTransportToken = pSMB->pfnDeleteTransportToken;
    OpLockPassStruct.pTransportToken = pSMB->pToken;
    OpLockPassStruct.ulConnectionID = pSMB->ulConnectionID;
    OpLockPassStruct.usLockTID = pSMB->pInSMB->Tid;

    //
    // Figure out file attributes
    if(0 == dwcAttributes) {
         dwAttributes |= FILE_ATTRIBUTE_NORMAL;
        IFDBG(dwcAttributes &= ~(FILE_ATTRIBUTE_NORMAL));
    }
    if(FILE_ATTRIBUTE_ARCHIVE & dwcAttributes) {
        dwAttributes |= FILE_ATTRIBUTE_ARCHIVE;
        IFDBG(dwcAttributes &= ~(FILE_ATTRIBUTE_ARCHIVE));
    }
    if(FILE_ATTRIBUTE_HIDDEN & dwcAttributes) {
        dwAttributes |= FILE_ATTRIBUTE_HIDDEN;
        IFDBG(dwcAttributes &= ~(FILE_ATTRIBUTE_HIDDEN));
    }
    if(FILE_ATTRIBUTE_NORMAL & dwcAttributes) {
        dwAttributes |= FILE_ATTRIBUTE_NORMAL;
        IFDBG(dwcAttributes &= ~(FILE_ATTRIBUTE_NORMAL));
    }
    if(FILE_ATTRIBUTE_READONLY & dwcAttributes) {
        dwAttributes |= FILE_ATTRIBUTE_READONLY;
        IFDBG(dwcAttributes &= ~(FILE_ATTRIBUTE_READONLY));
    }
    if(FILE_ATTRIBUTE_SYSTEM & dwcAttributes) {
        dwAttributes |= FILE_ATTRIBUTE_SYSTEM;
        IFDBG(dwcAttributes &= ~(FILE_ATTRIBUTE_SYSTEM));
    }
    if(FILE_ATTRIBUTE_DIRECTORY & dwcAttributes) {
        dwAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        IFDBG(dwcAttributes &= ~(FILE_ATTRIBUTE_DIRECTORY));
    }

    #ifdef DEBUG
    if(FILE_ATTRIBUTE_TEMPORARY & dwcAttributes) {
        dwcAttributes &= ~(FILE_ATTRIBUTE_TEMPORARY);
        TRACEMSG(ZONE_ERROR, (L"SMB_SRVR: NT_Create, FILE_ATTRIBUTE_TEMP not supported in WinCE.  ignoring"));
    }
    #endif


    //
    // Per WindowsNT FileSystem Internals (O'Reilly) -- if
    //  CREATE_OPTION_DIRECTORY is set it *MUST* be a directory, so we
    //  set the FILE_ATTRIBUTE_DIRECTORY bit
    //
    // In summery:
    //  CREATE_OPTON_DIRECTORY -- file *MUST* be a directory
    //  NT_CREATE_OPTION_ON_NON_DIR means the file *CANT* be a directory
    if(NT_CREATE_OPTION_DIRECTORY & dwcCreateOptions) {
        dwAttributes |= FILE_ATTRIBUTE_MUST_BE_DIR;
        IFDBG(dwcCreateOptions &= ~(NT_CREATE_OPTION_DIRECTORY));
    }
    if(NT_CREATE_OPTION_ON_NON_DIR & dwcCreateOptions) {
        dwAttributes |= FILE_ATTRIBUTE_CANT_BE_DIR;
    }
    //IFDBG(ASSERT(0 == dwcAttributes));



    //
    // Figure out sharing modes
    if(FILE_SHARE_READ & dwcShareMode) {
        dwShareMode |= FILE_SHARE_READ;
        IFDBG(dwcShareMode &= ~FILE_SHARE_READ);
    }
    if(FILE_SHARE_WRITE & dwcShareMode) {
        dwShareMode |= FILE_SHARE_WRITE;
        IFDBG(dwcShareMode &= ~FILE_SHARE_WRITE);
    }
    if(FILE_SHARE_DELETE & dwcShareMode) {
        dwShareMode |= FILE_SHARE_DELETE;
        IFDBG(dwcShareMode &= ~FILE_SHARE_DELETE);
    }
    IFDBG(ASSERT(0 == dwcShareMode));

    #define FILE_OPEN_EXISTING              0x00000001 //If exist open, else fail
    #define FILE_CREATE_NEW                 0x00000002 //If exists fail, else create the file
    #define FILE_OPEN_ALWAYS                0x00000003 //if exist, open, else create
    #define FILE_TRUNCATE_EXISTING          0x00000004
    #define FILE_CREATE_ALWAYS              0x00000005 //If exists, open and overwrite, else create
    //
    // Figure out file creation disposition
    switch(dwcCreationDisposition) {
        case FILE_CREATE_ALWAYS:
            dwDisposition = CREATE_ALWAYS;
            dwTIDMode |= SEC_WRITE;dwWriteCnt++;
            break;
        case FILE_CREATE_NEW:
            dwDisposition = CREATE_NEW;
            dwTIDMode |= SEC_WRITE;dwWriteCnt++;
            break;
        case FILE_OPEN_EXISTING:
            dwDisposition = OPEN_EXISTING;
            break;
        case FILE_OPEN_ALWAYS:
            dwDisposition = OPEN_ALWAYS;
            dwTIDMode |= SEC_WRITE;dwWriteCnt++;
            break;
            /*
        case FILE_CREATE:
            dwDisposition = CREATE_ALWAYS;
            break;
        case FILE_OPEN:
            dwDisposition = OPEN_ALWAYS;
            break;
        case FILE_OVERWRITE_IF:
            dwDisposition = TRUNCATE_EXISTING;
            break;*/
        default:
            ASSERT(FALSE);
            break;
    }


    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_NT_Create: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a share state
    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, dwTIDMode)) || !pTIDState)  {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_NT_Create -- couldnt find share state!!"));
        if(E_ACCESSDENIED == hr) {

        //
        // if access was denied and they want OPEN_ALWAYS try it again with a readonly OPEN_EXISTING
        if(dwDisposition == OPEN_ALWAYS && 1 == dwWriteCnt) {
                dwDisposition = OPEN_EXISTING;
                dwTIDMode &= ~SEC_WRITE;
                dwWriteCnt --;
                fFlippedToOpenExisting = TRUE;
                hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, dwTIDMode);
            }
            if(FAILED(hr)) {
                dwRet = ERROR_CODE(STATUS_ACCESS_DENIED);
            }
        }
        if(FAILED(hr)) {
            goto DenyRequest;
        }
    }

    //
    // Init then fetch the file name from the request tokenizer
    RequestTokenizer.Reset((BYTE*)(pRequest+1), _pRawRequest->uiDataSize - sizeof(SMB_OPEN_PRINT_SPOOL_CLIENT_REQUEST));


ASSERT(0 == pRequest->RootDirectoryFID);

    if(TRUE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
        if(FAILED(RequestTokenizer.GetUnicodeString(&pRequestedFile))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_NT_Create -- error getting file name in unicode"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
    } else {
        CHAR *pTempString;
        if(FAILED(RequestTokenizer.GetString(&pTempString))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_NT_Create -- error getting file name in ASCII"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        if(FAILED(RequestedFile.append(pTempString))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_NT_Create -- error getting file name on append of non unicode"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        pRequestedFile = (WCHAR *)RequestedFile.GetString();
    }

    //
    //  output the request filename and length
    //
    TRACE_PROTOCOL_SMB_NT_CREATE_REQUEST_FILENAME( pRequestedFile, pRequest );


    //
    // Use the TID state to create a new stream (on printer this is a print
    //    stream, on file its a file stream)
    //
    // Unless the type is IPC create it with the factory
    if(STYPE_IPC != pTIDState->GetShare()->GetShareType()) {
        if(NULL == (pFileStream = pTIDState->CreateFileStream(pMyConnection))) {
            TRACEMSG(ZONE_FILES, (L"SMBSRV-CRACKER: SMB_NT_Create -- error getting File/Print Stream"));
            goto DenyRequest;
        }
    } else {
        dwRet = ConvertHRToError(HRESULT_FROM_WIN32(ERROR_PIPE_NOT_CONNECTED), pSMB);
        goto DenyRequest_ErrorSet;
    }

    //
    // Register it with the TIDState
    if(FAILED(pTIDState->AddFileStream(pFileStream))) {
        delete pFileStream;
        pFileStream = NULL;
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Open up the file (first invalidate any ptrs that may change due to blocking)
    hr = pFileStream->Open(pRequestedFile,
                           dwAccess,
                           dwDisposition,
                           dwAttributes,
                           dwShareMode,
                           &dwActionTaken,
                           &OpLockPassStruct);

    if(FAILED(hr)) {
        dwRet = ConvertHRToError(hr, pSMB);

        if(fFlippedToOpenExisting &&
             (hr==HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
              hr==HRESULT_FROM_WIN32(ERROR_DIRECTORY))) {
            dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
        }
        goto DenyRequest_ErrorSet;
    }

    //
    // Get the file size (NOTE this might not be implemented on things like printer)
    if(FAILED(pFileStream->GetFileSize(&dwFileSize))) {
        dwFileSize = 0;
    }

    //
    // Fill out return params and send back the data

    FILETIME CreateTime;
    FILETIME LastAccessTime;
    FILETIME LastWriteTime;
    FILETIME ChangeTime;

    WIN32_FIND_DATA w32FD;

    //
    // Query the file for all file times
    if(FAILED(pFileStream->GetFileTime(&CreateTime, &LastAccessTime, &LastWriteTime))) {
        CreateTime.dwLowDateTime = 0;
        CreateTime.dwHighDateTime = 0;
        LastAccessTime.dwLowDateTime = 0;
        LastAccessTime.dwHighDateTime = 0;
        LastWriteTime.dwLowDateTime = 0;
        LastWriteTime.dwHighDateTime = 0;
        ChangeTime.dwLowDateTime = 0;
        ChangeTime.dwHighDateTime = 0;
    }

    //
    // Query the file for attributes
    if(FAILED(pFileStream->QueryFileInformation(&w32FD))) {
        w32FD.dwFileAttributes = 0;
    }

    //
    // Get the file type (print,ipc,disk, etc)
    switch(pTIDState->GetShare()->GetShareType()) {
        case STYPE_DISKTREE:
            pResponse->FileType = 0;
            break;
#ifdef SMB_NMPIPE
        case STYPE_IPC:
            pResponse->FileType = 2;
            break;
#endif
        case STYPE_PRINTQ:
            pResponse->FileType = 3;
            break;
        default:
            pResponse->FileType = 0xFF;
            ASSERT(FALSE); //Open should have failed before here!!
            goto DenyRequest;
    }

    pResponse->OplockLevel = OpLockPassStruct.cGiven;
    pResponse->FID = pFileStream->FID();
    pResponse->CreationAction = dwActionTaken;
    pResponse->CreationTime = CreateTime;
    pResponse->LastAccessTime= LastAccessTime;
    pResponse->LastWriteTime = LastWriteTime;
    pResponse->ChangeTime = LastWriteTime;
    pResponse->ExtFileAttributes = w32FD.dwFileAttributes;
    pResponse->AllocationSize.LowPart= dwFileSize;
    pResponse->AllocationSize.HighPart = 0;
    pResponse->EndOfFile.LowPart = dwFileSize;
    pResponse->EndOfFile.HighPart = 0;

    //pResponse->FileType = 0; // set above
    pResponse->DeviceState = 0;
    pResponse->Directory = (w32FD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)?TRUE:FALSE;


    pResponse->ANDX.WordCount = (sizeof(SMB_NT_CREATE_SERVER_RESPONSE) - 3) / sizeof(WORD);
    *puiUsed = sizeof(SMB_NT_CREATE_SERVER_RESPONSE);

    goto Done;

    DenyRequest:
        dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
    DenyRequest_ErrorSet:
        *puiUsed = 3;
        _pRawResponse->pDataPortion[0] = 0x00;
        _pRawResponse->pDataPortion[1] = 0x00;
        _pRawResponse->pDataPortion[2] = 0x00;
    Done:

        if(0 != dwRet) {
            if(NULL != pFileStream) {
                //
                // Unregister it with the TIDState
                pTIDState->RemoveFileStream(pFileStream->FID());
            }

        }

        //
        //  send the SMBntcreatex response to the debug output
        //
        TRACE_PROTOCOL_SMB_NT_CREATE_RESPONSE( dwRet, pResponse );
        return dwRet;
}


DWORD SMB_DownLevel_Write(SMB_PACKET *pSMB,
                 SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed)
{
    DWORD dwRet = 0;
    ce::smart_ptr<TIDState> pTIDState;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    HRESULT hr = E_FAIL;

    SMB_WRITE_CLIENT_REQUEST *pRequest =
            (SMB_WRITE_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_WRITE_SERVER_RESPONSE *pResponse =
            (SMB_WRITE_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    DWORD dwWritten = 0;

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_WRITE_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Write -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_WRITE_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Write -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: SMB_Write request"));


    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Write: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a TID state
    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_WRITE)) || !pTIDState) {
        if(E_ACCESSDENIED == hr) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_Write -- access denied!!"));
            dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
            goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Write -- couldnt find share state!!"));
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done;
        }
    }

    //
    // If 0 bytes are given to us, rather than writing, truncate the file
    //    this is per SMBHLP.zip spec
    if(0 == pRequest->IOBytes) {
        TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: TRUNCATE (FID=%d) -- offset=%d\n", pRequest->FileID, pRequest->FileOffset));
        if(FAILED(hr = pTIDState->SetEndOfStream(pRequest->FileID, pRequest->FileOffset))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Write -- couldnt set end of stream!!", pRequest->FileOffset));
            dwRet = ConvertHRToError(hr, pSMB);
            goto Done;
        }
    } else {
        //
        // Write our data
        TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: WriteX(FID=0x%x) -- byte=%d\n", pRequest->FileID, pRequest->IOBytes));

        if(FAILED(hr = pTIDState->Write(pRequest->FileID, (BYTE*)(pRequest+1), pRequest->FileOffset, pRequest->IOBytes, &dwWritten))) {
            dwRet = ConvertHRToError(hr, pSMB);
            goto Done;
        }
    }

    //
    // Fill out return params and send back the data
    pResponse->ByteCount = 0;
    pResponse->DataLength = (USHORT)dwWritten;
    // 3 because of WordCount and ByteCount
    pResponse->WordCount = (sizeof(SMB_WRITE_SERVER_RESPONSE) - 3) / sizeof(WORD);
    *puiUsed = sizeof(SMB_WRITE_SERVER_RESPONSE);

    Done:
        return dwRet;
}


DWORD SMB_WriteX(SMB_PACKET *pSMB,
                 SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed)
{
    DWORD dwRet = 0;
    ce::smart_ptr<TIDState> pTIDState;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    HRESULT hr = E_FAIL;


    SMB_COM_ANDX_HEADER *pRequestANDX = (SMB_COM_ANDX_HEADER *)_pRawRequest->pDataPortion;

    SMB_WRITEX_SERVER_RESPONSE *pResponse =
            (SMB_WRITEX_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    DWORD dwWritten = 0;
    USHORT FID;
    ULONG Offset;
    ULONG Size;
    BYTE *pPtr;

    //
    // Verify that we have enough memory
    if(_pRawResponse->uiDataSize < sizeof(SMB_WRITEX_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_WriteX -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_COM_ANDX_HEADER)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_WriteX -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: SMB_WriteX request"));


    if(14 == pRequestANDX->WordCount) {
            SMB_WRITEX_CLIENT_REQUEST_NT *pRequest = (SMB_WRITEX_CLIENT_REQUEST_NT *)_pRawRequest->pDataPortion;

            if(_pRawResponse->uiDataSize < sizeof(SMB_WRITEX_CLIENT_REQUEST_NT)) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_WriteX -- not enough memory for request!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            FID = pRequest->FID;
            Offset = pRequest->Offset;
            Size = pRequest->DataLength;
            pPtr = (BYTE *)_pRawRequest->pSMBHeader + pRequest->DataOffset;
    } else if(12 == pRequestANDX->WordCount) {
            SMB_WRITEX_CLIENT_REQUEST *pRequest = (SMB_WRITEX_CLIENT_REQUEST *)_pRawRequest->pDataPortion;

            if(_pRawResponse->uiDataSize < sizeof(SMB_WRITEX_CLIENT_REQUEST)) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_WriteX -- not enough memory for request!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }

            FID = pRequest->FID;
            Offset = pRequest->Offset;
            Size = pRequest->DataLength;
            pPtr = (BYTE *)_pRawRequest->pSMBHeader + pRequest->DataOffset;
    } else {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_WriteX -- unknown word count %d!", pRequestANDX->WordCount));
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_WriteX: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a TID state
    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_WRITE)) || !pTIDState) {
        if(E_ACCESSDENIED == hr) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_WriteX -- access denied!!"));
            dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
            goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_WriteX -- couldnt find share state!!"));
            dwRet = SMB_ERR(ERRSRV, ERRerror);
            goto Done;
        }
    }

    //
    // Write our data
    TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: WriteX(FID=0x%x) -- byte=%d\n", FID, Size));

    if(FAILED(hr = pTIDState->Write(FID, pPtr, Offset, Size, &dwWritten))) {
        dwRet = ConvertHRToError(hr, pSMB);
    }


    //
    // Fill out return params and send back the data
    pResponse->Count = (USHORT)dwWritten;
    pResponse->Remaining = 0;
    pResponse->Reserved = (USHORT)dwWritten;
    pResponse->ByteCount = 0;
    pResponse->ANDX.WordCount = (sizeof(SMB_WRITEX_SERVER_RESPONSE) - 3) / sizeof(WORD);

    *puiUsed = sizeof(SMB_WRITEX_SERVER_RESPONSE);

    Done:
        return dwRet;
}


//
// Information taken from the smb help file (smbhlp.zip)
DWORD SMB_OpenX(SMB_PACKET *pSMB,
                 SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed)
{
    StringTokenizer   RequestTokenizer;
    StringConverter   RequestedFile;
    ce::smart_ptr<TIDState> pTIDState = NULL;
    WCHAR            *pRequestedFile = NULL;
    SMBFileStream    *pFileStream = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    DWORD             dwFileSize;
    DWORD             dwRet = 0;
    DWORD             dwAccess = 0;
    DWORD             dwSecAccess = SEC_READ;
    DWORD             dwAttributes = 0;
    DWORD             dwDisposition = 0;
    DWORD             dwShareMode = 0;
    DWORD             dwActionTaken = 0;
    USHORT            usMode = 0;
    DWORD             dwWriteCnt = 0;
    HRESULT           hr;
    BOOL                fFlippedToOpenExisting = TRUE;

    SMB_OPENX_CLIENT_REQUEST *pRequest =
            (SMB_OPENX_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_OPENX_SERVER_RESPONSE *pResponse =
            (SMB_OPENX_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_OPENX_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_OPENX_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: SMB_OpenX request"));

     //
    // Check out the requested Open mode

    //
    // FCB Compatibility mode
    usMode = pRequest->Mode; //<-- we use this a bunch and the struct is not aligned
    if(usMode == 0x00FF) {
        dwAccess = GENERIC_WRITE;
        dwAttributes = FILE_ATTRIBUTE_NORMAL;
        //TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- Genric Write"));
        //TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- Attribute Normal"));
    } else {
        if(usMode & 0x4000) {
            dwAttributes = FILE_FLAG_WRITE_THROUGH;
            //TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- WriteThrough"));
        } else {
            dwAttributes = FILE_ATTRIBUTE_NORMAL;
            //TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- Attribute Normal"));
        }
        switch(usMode & 0x0070) {
            case 0: //DOS compat
            case 0x0010: //deny all
                dwShareMode = 0;
                break;
            case 0x0020: // DENY WRITE
                dwShareMode = FILE_SHARE_READ;
                //TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- FILE_SHARE_READ"));
                break;
            case 0x0030: // DENY READ
                dwShareMode = FILE_SHARE_WRITE;
                //TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- FILE_SHARE_WRITE"));
                break;
            case 0x0040: // DENY NONE
                dwShareMode = FILE_SHARE_READ|FILE_SHARE_WRITE;
                //TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- FILE_SHARE_WRITE|FILE_SHARE_WRITE"));
                break;
            default:    // whatever...
                dwShareMode = 0;
                break;
        }

        ASSERT(0 == dwAccess);
        switch (usMode & 0x0007) {
            case 0: // read
            case 3: // execute
                dwAccess |= GENERIC_READ;
                //TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- GENERIC_READ"));
                break;
            case 2: // read and write
                dwAccess |= GENERIC_READ;
                //TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- GENERIC_READ"));
            case 1: // write
                dwAccess |= GENERIC_WRITE;
                dwSecAccess |= SEC_WRITE;dwWriteCnt++;
                //TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- GENERIC_WRITE"));
                break;
            default:
                dwAccess |= GENERIC_READ;
                //TRACEMSG(ZONE_FILES,(L"SMBSRV:  OpenX -- GENERIC_READ"));
                break;
        }
    }

    //
    // Parse out the open disposition (per spec in smbhlp.zip)
    if(pRequest->OpenFunction)
    {
        switch(pRequest->OpenFunction) {
            case DISP_OPEN_EXISTING:      //0x01
                dwDisposition = OPEN_EXISTING;
                TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- OPEN_EXISTING"));
                break;
            case DISP_CREATE_NEW:         //0x10
                dwDisposition = CREATE_NEW;
                dwSecAccess |= SEC_WRITE;dwWriteCnt++;
                TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- CREATE_NEW"));
                break;
            case DISP_OPEN_ALWAYS:        //0X11
                dwDisposition = OPEN_ALWAYS;
                dwSecAccess |= SEC_WRITE;dwWriteCnt++;
                TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- TRUNCATE_EXISTING"));
                break;
            case DISP_CREATE_ALWAYS:     //0x12
                dwDisposition = CREATE_ALWAYS;
                dwSecAccess |= SEC_WRITE;dwWriteCnt++;
                TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- CREATE_ALWAYS"));
                break;
            default:
                dwDisposition = OPEN_ALWAYS;
                dwSecAccess |= SEC_WRITE;dwWriteCnt++;
                TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- OPEN_ALWAYS"));
                break;
        }

    }

    //
    // Set attributes
    if(pRequest->FileAttributes) {

        if(pRequest->FileAttributes & OPENX_ATTR_READ_ONLY) {
            //
            // If any other attribute is set, we cant have normal
            dwAttributes &= (~FILE_ATTRIBUTE_NORMAL);
            dwAttributes |= FILE_ATTRIBUTE_READONLY;
            TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- Setting ReadOnly attribute"));
        }
        if(pRequest->FileAttributes & OPENX_ATTR_HIDDEN) {
            //
            // If any other attribute is set, we cant have normal
            dwAttributes &= (~FILE_ATTRIBUTE_NORMAL);
            dwAttributes |= FILE_ATTRIBUTE_HIDDEN;
            TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- Setting Hidden attribute"));
        }
        if(pRequest->FileAttributes & OPENX_ATTR_SYSTEM) {
            //
            // If any other attribute is set, we cant have normal
            dwAttributes &= (~FILE_ATTRIBUTE_NORMAL);
            dwAttributes |= FILE_ATTRIBUTE_SYSTEM;
            TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- Setting System attribute"));
        }
        if(pRequest->FileAttributes & OPENX_ATTR_ARCHIVE) {
            //
            // If any other attribute is set, we cant have normal
            dwAttributes &= (~FILE_ATTRIBUTE_NORMAL);
            dwAttributes |= FILE_ATTRIBUTE_ARCHIVE;
            TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- Setting Archive attribute"));
        }
    }

    //
    // On down-level dont allow opening a directory
    dwAttributes |= FILE_ATTRIBUTE_CANT_BE_DIR;

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a share state
    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, dwSecAccess)) || !pTIDState)  {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- couldnt find share state!!"));
        if(E_ACCESSDENIED == hr) {
            //
            // if access was denied and they want OPEN_ALWAYS try it again with a readonly OPEN_EXISTING
            if(dwDisposition == OPEN_ALWAYS && 1 == dwWriteCnt) {
                dwDisposition = OPEN_EXISTING;
                dwSecAccess &= ~SEC_WRITE;
                dwWriteCnt --;
                fFlippedToOpenExisting = TRUE;
                hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, dwSecAccess);
            }
            if(FAILED(hr)) {
                dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
                goto Done;
            }
        }
        if(FAILED(hr)) {
            goto DenyRequest;
        }
    }


    //
    // Init then fetch the file name from the request tokenizer
    RequestTokenizer.Reset((BYTE*)(pRequest+1), _pRawRequest->uiDataSize - sizeof(SMB_OPEN_PRINT_SPOOL_CLIENT_REQUEST));

    if(TRUE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
        if(FAILED(RequestTokenizer.GetUnicodeString(&pRequestedFile))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- error getting file name in unicode"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
    } else {
        CHAR *pTempString;
        if(FAILED(RequestTokenizer.GetString(&pTempString))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- error getting file name in ASCII"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        if(FAILED(RequestedFile.append(pTempString))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_OpenX -- error getting file name on append of non unicode"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        pRequestedFile = (WCHAR *)RequestedFile.GetString();
    }

    //
    // Use the TID state to create a new stream (on printer this is a print
    //    stream, on file its a file stream)
    if(NULL == (pFileStream = pTIDState->CreateFileStream(pMyConnection))) {
        TRACEMSG(ZONE_FILES, (L"SMBSRV-CRACKER: SMB_OpenX -- error getting File/Print Stream"));
        goto DenyRequest;
    }

    //
    // Open up the file
    hr = pFileStream->Open(pRequestedFile,
                           dwAccess,
                           dwDisposition,
                           dwAttributes,
                           dwShareMode,
                           &dwActionTaken);

    if(FAILED(hr)) {
        dwRet = ConvertHRToError(hr, pSMB);
        if(fFlippedToOpenExisting && hr==HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ) {
            dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
        }
        goto DenyRequest_ErrorSet;
    }

    //
    // If the file didnt exist and was created, set its creation date to what the
    //    client passed in
    if(2 == dwActionTaken) {
        FILETIME ftCreation;
        FILETIME ftUTCCreation;

        if(0 != FileTimeFromSecSinceJan1970(pRequest->Time, &ftCreation) &&
           0 != LocalFileTimeToFileTime(&ftCreation, &ftUTCCreation)) {
                pFileStream->SetFileTime(&ftUTCCreation, &ftUTCCreation, &ftUTCCreation);
        }
    }

    //
    // Get the file size (NOTE this might not be implemented on things like printer)
    if(FAILED(pFileStream->GetFileSize(&dwFileSize))) {
        dwFileSize = 0;
    }

    //
    // Fill out return params and send back the data (SMBHLP.ZIP)
    FILETIME WriteTime;
    FILETIME LocalWriteTime;
    WIN32_FIND_DATA w32FD;
    if(FAILED(pFileStream->GetFileTime(NULL, NULL, &WriteTime))) {
        WriteTime.dwLowDateTime = 0;
        WriteTime.dwHighDateTime = 0;
    }
    pResponse->ByteCount = 0;
    pResponse->FileID = pFileStream->FID();
    if(SUCCEEDED(pFileStream->QueryFileInformation(&w32FD))) {
        pResponse->FileAttribute = Win32AttributeToDos(w32FD.dwFileAttributes);
    } else {
        pResponse->FileAttribute = 0;
    }

    if(0 == FileTimeToLocalFileTime(&WriteTime, &LocalWriteTime)) {
        memcpy(&LocalWriteTime, &WriteTime, sizeof(FILETIME));
    }
    pResponse->LastModifyTime = SecSinceJan1970_0_0_0(&LocalWriteTime);
    pResponse->FileSize = dwFileSize;
    pResponse->OpenMode = usMode; //send back the same mode they asked for (if we get here, we could open the file as they desire)

    switch(pTIDState->GetShare()->GetShareType()) {
        case STYPE_DISKTREE:
            pResponse->FileType = 0;
            break;
        case STYPE_IPC:
            pResponse->FileType = 2;
            break;
        case STYPE_PRINTQ:
            pResponse->FileType = 3;
            break;
        default:
            pResponse->FileType = 0xFF;
            ASSERT(FALSE); //Open should have failed before here!!
            goto DenyRequest;
    }
    pResponse->ActionTaken = (USHORT)dwActionTaken;
    pResponse->ServerFileHandle = 0;
    pResponse->ByteCount = 0;
    pResponse->ANDX.WordCount = (sizeof(SMB_OPENX_SERVER_RESPONSE) - 3) / sizeof(WORD);
    ASSERT(15 == pResponse->ANDX.WordCount);
    *puiUsed = sizeof(SMB_OPENX_SERVER_RESPONSE);

    goto Done;
    DenyRequest:
        dwRet = ERROR_CODE(STATUS_ACCESS_DENIED);
    DenyRequest_ErrorSet:
        *puiUsed = 3;
        _pRawResponse->pDataPortion[0] = 0x00;
        _pRawResponse->pDataPortion[1] = 0x00;
        _pRawResponse->pDataPortion[2] = 0x00;
    Done:

        if(0 != dwRet) {
            if(NULL != pFileStream) {
                delete pFileStream;
            }
        } else {
            //
            // Register it with the TIDState
            if(FAILED(pTIDState->AddFileStream(pFileStream))) {
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
        }
        return dwRet;
}

//
// Information taken from the smb help file (smbhlp.zip)
DWORD SMB_PacketOpen(SMB_PACKET *pSMB,
                 SMB_PROCESS_CMD *_pRawRequest,
                 SMB_PROCESS_CMD *_pRawResponse,
                 UINT *puiUsed)
{
    StringTokenizer   RequestTokenizer;
    StringConverter   RequestedFile;
    ce::smart_ptr<TIDState>         pTIDState = NULL;
    WCHAR            *pRequestedFile = NULL;
    SMBFileStream    *pFileStream = NULL;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    DWORD             dwFileSize;
    DWORD             dwRet = 0;
    DWORD             dwAccess = 0;
    DWORD             dwSecAccess = SEC_READ;
    DWORD        dwWriteCnt = 0;
    DWORD             dwAttributes = 0;
    DWORD             dwDisposition = 0;
    DWORD             dwShareMode = 0;
    DWORD             dwActionTaken = 0;
    USHORT            usAccessGiven = 0;
    HRESULT           hr;
    USHORT            usMode = 0;


    SMB_OPEN_CLIENT_REQUEST *pRequest =
            (SMB_OPEN_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_OPEN_SERVER_RESPONSE *pResponse =
            (SMB_OPEN_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_OPEN_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Open -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_OPEN_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Open -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: SMB_Open request"));

    //
    // Check out the requested Open mode

    //
    // FCB Compatibility mode
    usMode = pRequest->Mode; //<-- we use this a bunch and the struct is not aligned

    dwDisposition = OPEN_ALWAYS;
    switch((usMode & 0x70) >> 4) {
        case 0:
            dwShareMode = 0;
            break;
        case 1:
            dwShareMode = 0;
            break;
        case 2:
            dwShareMode = FILE_SHARE_READ;
            break;
        case 3:
            dwShareMode = FILE_SHARE_WRITE;
            break;
        case 4:
            dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
            break;
    }

    switch(usMode & 0x000F) {
        case 0:
            dwAccess = GENERIC_READ;
            usAccessGiven = 0;
            break;
        case 1:
            dwAccess = GENERIC_WRITE;
            usAccessGiven = 1;
         dwSecAccess |= SEC_WRITE; dwWriteCnt++;
            break;
        case 2:
            dwAccess = GENERIC_READ | GENERIC_WRITE;
            usAccessGiven = 2;
         dwSecAccess |= SEC_WRITE; dwWriteCnt++;
            break;
    }

    //
    // Set attributes
    if(pRequest->FileAttributes) {
        if(pRequest->FileAttributes & OPENX_ATTR_READ_ONLY) {
            dwAttributes |= FILE_ATTRIBUTE_READONLY;
            TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- Setting ReadOnly attribute"));
        }
        if(pRequest->FileAttributes & OPENX_ATTR_HIDDEN) {
            dwAttributes |= FILE_ATTRIBUTE_HIDDEN;
            TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- Setting Hidden attribute"));
        }
        if(pRequest->FileAttributes & OPENX_ATTR_SYSTEM) {
            dwAttributes |= FILE_ATTRIBUTE_SYSTEM;
            TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- Setting System attribute"));
        }
        if(pRequest->FileAttributes & OPENX_ATTR_ARCHIVE) {
            dwAttributes |= FILE_ATTRIBUTE_ARCHIVE;
            TRACEMSG(ZONE_FILES, (L"SMBSRV:  OpenX -- Setting Archive attribute"));
        }
    } else {
        dwAttributes = FILE_ATTRIBUTE_NORMAL;
    }


    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }


    //
    // Find a share state
    if(FAILED(hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, dwSecAccess)) || !pTIDState)  {
     TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Open -- couldnt find share state!!"));
        if(E_ACCESSDENIED == hr) {

         //
         // if access was denied and they want OPEN_ALWAYS try it again with a readonly OPEN_EXISTING
         if(dwDisposition == OPEN_ALWAYS && 1 == dwWriteCnt) {
             dwDisposition = OPEN_EXISTING;
            dwSecAccess &= ~SEC_WRITE;
            dwWriteCnt --;
            hr = pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, dwSecAccess);
         }
         if(FAILED(hr)) {
                dwRet = ERROR_CODE(STATUS_ACCESS_DENIED);
         }
        }
    if(FAILED(hr)) {
           goto DenyRequest;
    }
    }




    //
    // Init then fetch the file name from the request tokenizer
    RequestTokenizer.Reset((BYTE*)(pRequest+1), _pRawRequest->uiDataSize - sizeof(SMB_OPEN_PRINT_SPOOL_CLIENT_REQUEST));

    if(TRUE == pMyConnection->SupportsUnicode(pSMB->pInSMB)) {
        if(FAILED(RequestTokenizer.GetUnicodeString(&pRequestedFile))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Open -- error getting file name in unicode"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
    } else {
        CHAR *pTempString;
        if(FAILED(RequestTokenizer.GetString(&pTempString))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Open -- error getting file name in ASCII"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }

        //
        // Advance beyond the string header
        if(0x04 == *pTempString) {
            pTempString ++;
        } else {
            ASSERT(FALSE);
        }
        if(FAILED(RequestedFile.append(pTempString))) {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Open -- error getting file name on append of non unicode"));
            dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
            goto Done;
        }
        pRequestedFile = (WCHAR *)RequestedFile.GetString();
    }

    //
    // Use the TID state to create a new stream (on printer this is a print
    //    stream, on file its a file stream)
    if(NULL == (pFileStream = pTIDState->CreateFileStream(pMyConnection))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Open -- error getting File/Print Stream"));
        goto DenyRequest;
    }

    //
    // Open up the file
    hr = pFileStream->Open(pRequestedFile,
                           dwAccess,
                           dwDisposition,
                           dwAttributes,
                           dwShareMode,
                           &dwActionTaken);
    if(FAILED(hr)) {
        dwRet = ConvertHRToError(hr, pSMB);
        goto DenyRequest_ErrorSet;
    }

    //
    // Get the file size
    if(FAILED(pFileStream->GetFileSize(&dwFileSize))) {
        dwFileSize = 0;
    }

    //
    // Fill out return params and send back the data (SMBHLP.ZIP)
    FILETIME WriteTime;
    FILETIME UTCWriteTime;

    WIN32_FIND_DATA w32FD;
    if(FAILED(pFileStream->GetFileTime(NULL, NULL, &WriteTime))) {
        WriteTime.dwLowDateTime = 0;
        WriteTime.dwHighDateTime = 0;
    }
    pResponse->FileID = pFileStream->FID();
    if(SUCCEEDED(pFileStream->QueryFileInformation(&w32FD))) {
        pResponse->FileAttribute = Win32AttributeToDos(w32FD.dwFileAttributes);
    } else {
        pResponse->FileAttribute = 0;
    }

    if(0 == FileTimeToLocalFileTime(&WriteTime, &UTCWriteTime)) {
        memcpy(&UTCWriteTime, &WriteTime, sizeof(FILETIME));
    }
    pResponse->LastModifyTime = SecSinceJan1970_0_0_0(&UTCWriteTime);
    pResponse->FileSize = dwFileSize;
    pResponse->OpenMode = usAccessGiven;
    pResponse->ByteCount = 0;
    *puiUsed = sizeof(SMB_OPEN_SERVER_RESPONSE);

    goto Done;
    DenyRequest:
        dwRet = ERROR_CODE(STATUS_NETWORK_ACCESS_DENIED);
    DenyRequest_ErrorSet:
        *puiUsed = 3;
        _pRawResponse->pDataPortion[0] = 0x00;
        _pRawResponse->pDataPortion[1] = 0x00;
        _pRawResponse->pDataPortion[2] = 0x00;
    Done:
        if(0 != dwRet) {
            if(NULL != pFileStream) {
                delete pFileStream;
            }
        } else {
            //
            // Register it with the TIDState
            if(FAILED(pTIDState->AddFileStream(pFileStream))) {
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
        }
        return dwRet;
}


DWORD SMB_Com_Close(SMB_PROCESS_CMD *_pRawRequest,
                    SMB_PROCESS_CMD *_pRawResponse,
                    UINT *puiUsed,
                    SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;
    SMB_CLOSE_CLIENT_REQUEST *pRequest =
            (SMB_CLOSE_CLIENT_REQUEST *)_pRawRequest->pDataPortion;
    SMB_CLOSE_SERVER_RESPONSE *pResponse =
            (SMB_CLOSE_SERVER_RESPONSE *)_pRawResponse->pDataPortion;

    StringTokenizer RequestTokenizer;
    ce::smart_ptr<TIDState> pTIDState;
    ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
    HRESULT hr;

    //
    // Verify that we have enough memory
    if(_pRawRequest->uiDataSize < sizeof(SMB_CLOSE_PRINT_SPOOL_CLIENT_REQUEST)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Close -- not enough memory for request!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }
    if(_pRawResponse->uiDataSize < sizeof(SMB_CLOSE_PRINT_SPOOL_SERVER_RESPONSE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Close -- not enough memory for response!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Find our connection state
    if(!(pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {
       TRACEMSG(ZONE_ERROR, (L"SMBSRV: SMB_INFO_ALLOCATION: -- cant find connection 0x%x!", pSMB->ulConnectionID));
       ASSERT(FALSE);
       dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
       goto Done;
    }

    //
    // Find a share state
    if(FAILED(pMyConnection->FindTIDState(_pRawRequest->pSMBHeader->Tid, pTIDState, SEC_READ)) || !pTIDState) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Close -- couldnt find share state!!"));
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Close it up
    if(FAILED(hr = pTIDState->Close(pRequest->FileID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Close -- Close() on the job failed!!!"));

        dwRet = ConvertHRToError(hr, pSMB);
        goto Done;
    }

    //
    // Remove the share from the TIDState (to prevent it from being aborted
    //   if the connection goes down)
    if(FAILED(pTIDState->RemoveFileStream(pRequest->FileID))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_Com_Close -- couldnt find filestream for FID(%x) to remove from share state!", pRequest->FileID));
        ASSERT(FALSE);
        dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
        goto Done;
    }

    //
    // Fill out return params and send back the data (this is good even for errors)
    Done:
        pResponse->WordCount = 0;
        pResponse->ByteCount = 0;
        *puiUsed = sizeof(SMB_CLOSE_SERVER_RESPONSE);

        return dwRet;
}



DWORD PC_NET_PROG::CrackSMB(SMB_HEADER *pRequestSMB,
               UINT uiRequestSize,
               SMB_HEADER *pResponseSMB,
               UINT uiResponseRemaining,
               UINT *puiUsed,
               SMB_PACKET *pSMB)
{
    DWORD dwRet = 0;

    //
    // Proper SMB signature
    char pSig[4];
    pSig[0] = (char)0xFF;
    pSig[1] = (char)'S';
    pSig[2] = (char)'M';
    pSig[3] = (char)'B';


    UINT uiUsed = 0;        //represents bytes USED during ONE SMB
    UINT uiTotalUsed = 0;   //represents TOTAL bytes for ALL SMBs (for case of ANDX)
    *puiUsed = 0;           //init used to 0
    SMB_PROCESS_CMD RequestSMB;
    SMB_PROCESS_CMD ResponseSMB;

    RequestSMB.pSMBHeader = pRequestSMB;
    RequestSMB.pDataPortion = (BYTE *)pRequestSMB + sizeof(SMB_HEADER);
    RequestSMB.uiDataSize = uiRequestSize - sizeof(SMB_HEADER);

    ResponseSMB.pSMBHeader = pResponseSMB;
    ResponseSMB.pDataPortion = (BYTE *)pResponseSMB + sizeof(SMB_HEADER);
    ResponseSMB.uiDataSize = uiResponseRemaining - sizeof(SMB_HEADER);

    BOOL                 fANDX = FALSE;
    BOOL                 fHandledError = FALSE;

    SMB_COM_ANDX_HEADER *pPrevANDXHeader = NULL;                                        //only valud if fANDX is true
    SMB_COM_ANDX_HEADER *pRequestANDX = (SMB_COM_ANDX_HEADER *)RequestSMB.pDataPortion; //only valud if fANDX is true
    SMB_COM_ANDX_HEADER *pFirstResponseXHeader = (SMB_COM_ANDX_HEADER *)ResponseSMB.pDataPortion;

    BYTE                 bCommand = pRequestSMB->Command;


    while(bCommand != 0xFF) {
        TRACEMSG(ZONE_DETAIL, (L"SMBSRV-CRACKER: Processing CMD: 0x%x -- %10s", bCommand, GetCMDName(bCommand)));

        switch(bCommand) {
            case SMB_COM_WRITE_ANDX:            //0x2F
                dwRet = SMB_WriteX(pSMB, &RequestSMB, &ResponseSMB, &uiUsed);
                fANDX = TRUE;
                break;
            case SMB_COM_READ:                  //0x0A
                dwRet = SMB_PacketRead(pSMB, &RequestSMB, &ResponseSMB, &uiUsed);
                fANDX = FALSE;
                break;
            case SMB_COM_READ_ANDX:            //0x2E
                dwRet = SMB_ReadX(pSMB, &RequestSMB, &ResponseSMB, &uiUsed);
                fANDX = TRUE;
                break;
            case SMB_COM_WRITE:                //0xB
                dwRet = SMB_DownLevel_Write(pSMB, &RequestSMB, &ResponseSMB, &uiUsed);
                fANDX = FALSE;
                break;
            case SMB_COM_IOCTL:                //0x27
                dwRet = SMB_Com_IOCTL(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                fANDX = FALSE;
                break;
            case SMB_COM_NEGOTIATE:           //0x72
                dwRet = SMB_Com_Negotiate(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                fANDX = FALSE;
                break;
            case SMB_COM_TREE_CONNECT_ANDX:   //0x75
                dwRet = SMB_Com_Tree_Connect(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                fANDX = TRUE;
                break;
            case SMB_COM_TREE_DISCONNECT:     //0x71
                dwRet = SMB_ComTree_Disconnect(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                fANDX = FALSE;
                break;
            case SMB_COM_CLOSE:               //0x04
                dwRet = SMB_Com_Close(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                fANDX = FALSE;
                break;
            case SMB_COM_NT_CREATE_ANDX:     //0xA2
                dwRet = SMB_NT_Create(pSMB, &RequestSMB, &ResponseSMB, &uiUsed);
                fANDX = TRUE;
                break;
            case SMB_COM_OPEN_ANDX:          //0x2D
                dwRet = SMB_OpenX(pSMB, &RequestSMB, &ResponseSMB, &uiUsed);
                fANDX = TRUE;
                break;
            case SMB_COM_OPEN:               //0x02
                dwRet = SMB_PacketOpen(pSMB, &RequestSMB, &ResponseSMB, &uiUsed);
                fANDX = FALSE;
                break;
            case SMB_COM_OPEN_PRINT_FILE:    //0xC0
                if(g_fPrintServer) {
                    dwRet = PrintQueue::SMB_Com_Open_Spool(pSMB, &RequestSMB, &ResponseSMB, &uiUsed);
                } else {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: COM_CLOSE_OPEN_PRINT_FILE -- cant process as we dont have printserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                }
                fANDX = FALSE;
                break;
            case SMB_COM_CLOSE_PRINT_FILE:  //0xC2
                fANDX = FALSE;
                if(g_fPrintServer) {
                    dwRet = PrintQueue::SMB_Com_Close_Spool(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                } else {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: COM_CLOSE_PRINTFILE -- cant process as we dont have printserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                }
                break;
            case SMB_COM_TRANSACTION:      //0x25
                dwRet = SMB_Com_Transaction(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                break;

            case SMB_COM_SESSION_SETUP_ANDX: //0x73
                dwRet = SMB_Com_Session_Setup_ANDX(pSMB, &RequestSMB, &ResponseSMB, &uiUsed);

                if(dwRet == ERROR_CODE(STATUS_MORE_PROCESSING_REQUIRED)) {
                    fHandledError = TRUE;
                }
                fANDX = TRUE;
                break;

             case SMB_COM_LOGOFF_ANDX: //0x74
                dwRet = SMB_Com_Logoff_ANDX(pSMB, &RequestSMB, &ResponseSMB, &uiUsed);
                fANDX = TRUE;
                break;

             case SMB_COM_ECHO: //0x2B
                dwRet = SMB_Com_Echo(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                fANDX = FALSE;
                break;


            //
            //
            //  Special SMB's just used for file sharing
            //
            //
            case SMB_COM_CHECK_DIRECTORY: //0x10
                if(!g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_COM_CHECK_DIRECTORY -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_CheckPath(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }

            case SMB_COM_SEARCH:         //0x81
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SMB_COM_SEARCH -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_Search(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }

            case SMB_COM_SEEK:          //0x12
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SEEK -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_Seek(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }

            case SMB_COM_FLUSH:        //0x05
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: FLUSH -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_Flush(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_SET_INFORMATION2: //0x22
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SETINFO2 -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_SetInformation2(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_SET_INFORMATION:  //0x09
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: SET INFO -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_SetInformation(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_NT_TRANSACT:     //0xA0
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: TRANS -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_NT_TRASACT(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_NT_CANCEL:     //0xA4
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: TRANS -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_NT_Cancel(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_FIND_CLOSE2:     //0x34
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: FindClose -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_FindClose2(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_LOCKING_ANDX:    //0x24
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: LockAndX -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_LockX(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    fANDX = TRUE;
                    break;
                }
            case SMB_COM_DELETE:         //0x06
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: Delete -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_DeleteFile(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_RENAME:        //0x07
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: Rename -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_RenameFile(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_TRANSACTION2:  //0x32
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: TRANS2 -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_TRANS2(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_QUERY_INFORMATION2:     //0x23
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: query info2 -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_Query_EX_Information(pSMB, &RequestSMB, &ResponseSMB, &uiUsed);
                    break;
                }

            case SMB_COM_QUERY_INFORMATION:      //0x08
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: queryinfo -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_Query_Information(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_QUERY_INFORMATION_DISK: //0x80
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: query info disk -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_Query_Information_Disk(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_CREATE_DIRECTORY:      //0x00
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: Create Dir -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_MakeDirectory(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            case SMB_COM_DELETE_DIRECTORY:     //0x01
                if(! g_fFileServer) {
                    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: DelDir -- cant process as we dont have fileserver support!"));
                    dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                    fANDX = FALSE;
                    break;
                } else {
                    dwRet = SMB_FILE::SMB_Com_DelDirectory(&RequestSMB, &ResponseSMB, &uiUsed, pSMB);
                    break;
                }
            default:
                TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: PC_NETWORK_PROGRAM_1.0 -- unknown opcode (0x%x)!",pRequestSMB->Command));
                dwRet = ERROR_CODE(ERROR_NOT_SUPPORTED);
                ASSERT(FALSE);
                fANDX = FALSE;
                break;
        }

        //
        // If this was a successful ANDX patch up the PREVIOUS command (to point
        //    to this SMB
        if(fANDX) {
            if(pPrevANDXHeader) {
                pPrevANDXHeader->AndXCommand = bCommand;
                pPrevANDXHeader->AndXOffset = (USHORT)((BYTE *)(ResponseSMB.pDataPortion) - (BYTE *)(ResponseSMB.pSMBHeader));
                pPrevANDXHeader->AndXReserved = 0;
            }

            //
            // Setup the previous (we become the previous) and advance the data pointer
            //   also assume we are the LAST command
            pPrevANDXHeader = (SMB_COM_ANDX_HEADER *)ResponseSMB.pDataPortion;
            pPrevANDXHeader->AndXCommand = 0xFF; //assume we are the last command
            pPrevANDXHeader->AndXReserved = 0;
            pPrevANDXHeader->AndXOffset = 0;
            ResponseSMB.pDataPortion += uiUsed;
            ResponseSMB.uiDataSize -= uiUsed;
            uiTotalUsed += uiUsed;

            //
            // Advance the data portions
            if(0xFF != pRequestANDX->AndXCommand && (sizeof(SMB_HEADER) + RequestSMB.uiDataSize) >= pRequestANDX->AndXOffset) {
                bCommand = pRequestANDX->AndXCommand;
                pRequestANDX = (SMB_COM_ANDX_HEADER *)(((BYTE *)RequestSMB.pSMBHeader) + pRequestANDX->AndXOffset);
                RequestSMB.pDataPortion = (BYTE *)pRequestANDX;
                RequestSMB.uiDataSize -= pRequestANDX->AndXOffset;
            } else if(0xFF == pRequestANDX->AndXCommand) {
                bCommand = 0xFF;
            } else {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: ANDX offset is BEYOND packet!!! aborting!"));
                dwRet = ERROR_CODE(STATUS_INTERNAL_ERROR);
                goto Done;
            }
        }
        else {//not ANDX
            bCommand = 0xFF;
            uiTotalUsed += uiUsed;
        }

        //
        // If the packet has been queued, dont do anything here
        if(dwRet != SMB_ERR(ERRInternal, PACKET_QUEUED)) {
            




            if(fANDX && 0 != dwRet && FALSE == fHandledError) {
                ResponseSMB.pDataPortion[0] = 0x0;
                ResponseSMB.pDataPortion[1] = 0x0;
                ResponseSMB.pDataPortion[2] = 0x0;

                ResponseSMB.pDataPortion += 3;
                ResponseSMB.uiDataSize -= 3;
                uiTotalUsed += 3;
                *puiUsed = uiTotalUsed;
                TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: ANDX command in chain failed, stop processing"));
                goto Done;
            }
        }
    }


    *puiUsed = uiTotalUsed;
    Done:
        return dwRet;
}


