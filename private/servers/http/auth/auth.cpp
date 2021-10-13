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
/*--
Module Name: AUTH.CPP
Abstract: Authentication
--*/

#include "httpd.h"


const DWORD g_fAuthModule = TRUE;

//**********************************************************************
// Authentication
//**********************************************************************
#define SECURITY_DOMAIN TEXT("Domain")     // dummy data.  
#define SEC_SUCCESS(Status) ((Status) >= 0)

// Channel Binding
VOID AdjustFlagsForCbt(DWORD *pSecFlags,
                             SecBufferDesc *pInSecDesc,
                             PSecPkgContext_Bindings UniqueBindings
    );

NTSTATUS CheckSpn(
    PCtxtHandle hCtxt,
    PSecPkgContext_Bindings UniqueBindings
    );

NTSTATUS MapSecurityError(
    SECURITY_STATUS secStatus
    );

BOOL SecurityServerContext(
            PSEC_WINNT_AUTH_IDENTITY pAuthIdentity,
            PAUTH_STATE pAS,BYTE *pIn, DWORD cbIn, BYTE *pOut, 
            DWORD *pcbOut, BOOL *pfDone, const TCHAR *szPackageName,
            PSecPkgContext_Bindings UniqueBindings
     );

BOOL SecurityClientContext(
            PSEC_WINNT_AUTH_IDENTITY pAuthIdentity,
            PAUTH_STATE pAS, BYTE *pIn, DWORD cbIn, 
            BYTE *pOut, DWORD *pcbOut
     );

BOOL CGlobalVariables::InitChannelNameList(void)
{
    PWCHAR pAliasBuf = NULL;
    PWCHAR pBufStart = NULL;
    LONG   pwCount = 0; // PWCHAR count
    ULONG   index = 0;

    if (!m_SpnNameCheckList)
    {
        return FALSE;
    }

    pAliasBuf = m_SpnNameCheckList;
    pBufStart = m_SpnNameCheckList;
    m_ChannelBindingInfo.NumberOfServiceNames = 0;

    //
    // If no spn check, don't need to do this.
    //
    if (m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_NO_SERVICE_NAME_CHECK)
    {
        m_ChannelBindingInfo.NumberOfServiceNames = 0;
        return TRUE;
    }

    if (m_ChannelBindingInfo.Hardening > HttpAuthenticationHardeningLegacy)
    {
        m_ChannelBindingInfo.Flags |= HTTP_CHANNEL_BIND_SECURE_CHANNEL_TOKEN;
    }

    //
    // "proxy cohosting" is valid only when "proxy" is set.
    //
    if ((m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_PROXY_COHOSTING) &&
        !(m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_PROXY))
    {
        return FALSE;
    }

    //
    // If hardening is medium, flags "proxy", "proxy cohosting" and 
    // "no service check" result in no checks. This would be equivalent to legacy.
    // Therefore the combination is not allowed.
    //
    if (m_ChannelBindingInfo.Hardening == HttpAuthenticationHardenningMedium &&
        (m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_PROXY) &&
        (m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_PROXY_COHOSTING) &&
        (m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_NO_SERVICE_NAME_CHECK))
    {
        return FALSE;
    }

    // parse the list L"name0$name1$name2\0" into global server list.
    while (*pAliasBuf != L'\0')
    {
        if (*pAliasBuf == L'$')
        {
            m_ChannelBindingInfo.NumberOfServiceNames++;
        }
        pAliasBuf++;
    }
    // adding the last L'\0'
    m_ChannelBindingInfo.NumberOfServiceNames++;

    m_ChannelBindingInfo.pServiceNames = MyRgAllocZ(HTTP_SERVICE_BINDING_W, m_ChannelBindingInfo.NumberOfServiceNames);
    if (!m_ChannelBindingInfo.pServiceNames)
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: Fail to malloc channel name list, failing request\r\n"));
        goto FAILURE;
    }

    RtlZeroMemory(m_ChannelBindingInfo.pServiceNames, 
        m_ChannelBindingInfo.NumberOfServiceNames * sizeof(HTTP_SERVICE_BINDING_W));

    pAliasBuf = m_SpnNameCheckList;
    pBufStart = m_SpnNameCheckList;
    index = 0;

    while (*pAliasBuf != L'\0')
    {
        if (*pAliasBuf == L'$')
        {
            *pAliasBuf = L'\0';
            
            if (InitChannelFlagsCheck(pBufStart))
            {
                pwCount = pAliasBuf - pBufStart + 1;  //adding the last L'\0'
                m_ChannelBindingInfo.pServiceNames[index].Buffer = (PWCHAR)MyRgAllocZ(WCHAR, pwCount) ;
                
                if (!m_ChannelBindingInfo.pServiceNames[index].Buffer)
                {
                    DEBUGMSG(ZONE_ERROR,(L"HTTPD: Fail to malloc service name, failing request\r\n"));
                    goto FAILURE;
                }
                
                RtlZeroMemory(m_ChannelBindingInfo.pServiceNames[index].Buffer, pwCount * sizeof(WCHAR));
                memcpy(m_ChannelBindingInfo.pServiceNames[index].Buffer, pBufStart, pwCount * sizeof(WCHAR));
                m_ChannelBindingInfo.pServiceNames[index].BufferSize = pwCount * sizeof(WCHAR);
                
            }

            *pAliasBuf = L'$';
            
            pAliasBuf++; // skip the L'$';
            pBufStart = pAliasBuf;
            index++;
                
        }
        else  
        {
            pAliasBuf++;
        }
    }

    if (InitChannelFlagsCheck(pBufStart))
    {
        // adding the last L'\0'
        pwCount = pAliasBuf - pBufStart + 1;
        m_ChannelBindingInfo.pServiceNames[index].Buffer = (PWCHAR)MyRgAllocZ(WCHAR, pwCount) ;
        if (!m_ChannelBindingInfo.pServiceNames[index].Buffer)
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: Fail to malloc service name, failing request\r\n"));
            goto FAILURE;
        }
        RtlZeroMemory(m_ChannelBindingInfo.pServiceNames[index].Buffer, pwCount * sizeof(WCHAR));
        memcpy(m_ChannelBindingInfo.pServiceNames[index].Buffer, pBufStart, pwCount * sizeof(WCHAR));
        m_ChannelBindingInfo.pServiceNames[index].BufferSize = pwCount * sizeof(WCHAR);
    }
    return TRUE;
    
FAILURE:
    if (m_ChannelBindingInfo.pServiceNames)
    {
        for (index = 0; index < m_ChannelBindingInfo.NumberOfServiceNames; index++)
        {
            MyFree(m_ChannelBindingInfo.pServiceNames[index].Buffer);
        }

        MyFree(m_ChannelBindingInfo.pServiceNames);
    }

    return FALSE;
    
}

BOOL CGlobalVariables::InitChannelFlagsCheck(PWCHAR spnString)
{
    INT i = 0;
    PWCHAR ptr = NULL;

    if(!(m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_DOTLESS_SERVICE) &&
        wcschr(spnString, L'.') == NULL)
    {
        return FALSE;
    }

    //
    // As CE HTTPD doesn't support port selecting, the 
    // binding is format of <http|https>/<DNSname|ip> 
    // or SPN
    //
    if (!(wcsstr(spnString, L"http") || wcsstr(spnString, L"HTTP")) &&
        !(wcsstr(spnString, L"https") || wcsstr(spnString, L"HTTPS")))
    {
        // Compatiable with CE7 client, no more check is
        // done when protocol name is not included.
        return TRUE;
    }
    else
    {
        if (ptr = wcsstr(spnString, L"/"))
        {
            return TRUE;
        }
        else
        {
            return FALSE;
        }   
    }

    return TRUE;
    
}


void CGlobalVariables::InitAuthentication(CReg *pReg) 
{
    m_fBasicAuth   =  pReg->ValueDW(RV_BASIC);

    // On root site, always load the security library whether we are using NTLM/negotiate
    // or not.  Do this in the case that one of our sub-websites is using NTLM/negotiate,
    // so we can point it back to the global function pointer table.
    if (! InitSecurityLib())
    {
        return;
    }

    m_fNTLMAuth      = pReg->ValueDW(RV_NTLM);
    m_fNegotiateAuth = pReg->ValueDW(RV_NEGOTIATE);

    if (m_fNTLMAuth)
    {
        //
        // if enabled NTLM auth, read channel binding info too
        //
        if (m_fRootSite)
        {
            // On root site, duplicate string value from registry.
            m_SpnNameCheckList = MySzDupW(pReg->ValueSZ(RV_SPNLIST));
            m_ChannelBindingInfo.Hardening = (HTTP_AUTHENTICATION_HARDENING_LEVELS)(pReg->ValueDW(RV_CHECKLEVEL));
            m_ChannelBindingInfo.Flags = pReg->ValueDW(RV_CHECKFLAGS);

            if (!InitChannelNameList())
            {
                DEBUGMSG(ZONE_ERROR,(L"HTTPD: Channel binding SPN list init failed.\r\n"));
            }
        }
        else
        {
            // On non-root site, copy this values from global variable.
            m_SpnNameCheckList = MySzDupW(g_pVars->m_SpnNameCheckList);
            m_ChannelBindingInfo.Hardening = g_pVars->m_ChannelBindingInfo.Hardening;
            m_ChannelBindingInfo.Flags = g_pVars->m_ChannelBindingInfo.Flags;

            if (!InitChannelNameList())
            {
                DEBUGMSG(ZONE_ERROR,(L"HTTPD: Channel binding SPN list init failed.\r\n"));
            }
        }
    }
    
}


// Called after we've mapped the virtual root.
BOOL CHttpRequest::HandleAuthentication(void) 
{
    if (m_pszAuthType && m_pszRawRemoteUser) 
    {
        if (AllowBasic() && 0==strcmpi(m_pszAuthType, cszBasic)) 
        {
            FreePersistedAuthInfo();
            HandleBasicAuth(m_pszRawRemoteUser);
        }
        else if (AllowNTLM() && 0==strcmpi(m_pszAuthType, cszNTLM)) 
        {
            FreePersistedAuthInfo();
            if ((m_AuthState.m_AuthType != AUTHTYPE_NONE) && (m_AuthState.m_AuthType != AUTHTYPE_NTLM))
                return FALSE;

            m_AuthState.m_AuthType = AUTHTYPE_NTLM;
            HandleNTLMNegotiateAuth(m_pszRawRemoteUser,TRUE);
        }
        else if (AllowNegotiate() && 0==strcmpi(m_pszAuthType,cszNegotiate)) 
        {
            FreePersistedAuthInfo();

            if ((m_AuthState.m_AuthType != AUTHTYPE_NONE) && (m_AuthState.m_AuthType != AUTHTYPE_NEGOTIATE))
            {
                return FALSE;
            }

            m_AuthState.m_AuthType = AUTHTYPE_NEGOTIATE;
            HandleNTLMNegotiateAuth(m_pszRawRemoteUser,FALSE);
        }
        else 
        {
            DEBUGMSG(ZONE_PARSER,(L"HTTPD: Unknown authorization type requested or requested type not enabled\r\n"));
            m_AuthLevelGranted = DeterminePermissionGranted(GetUserList(),m_AuthLevelGranted);
        }
    }
    else if (IsSecure() && m_SSLInfo.m_pClientCertContext) 
    {
        HandleSSLClientCertCheck();
    }
    else 
    {
        m_AuthLevelGranted = DeterminePermissionGranted(GetUserList(),m_AuthLevelGranted);
    }

    return TRUE;
}


// For calls to Basic Authentication, only called during the parsing stage.
BOOL CHttpRequest::HandleBasicAuth(PSTR pszData) 
{
    char szUserName[MAXUSERPASS];
    PSTR pszFinalUserName;
    DWORD dwLen = sizeof(szUserName);

    DEBUGCHK((m_pszRemoteUser == NULL) && (m_pszPassword == NULL));

    m_AuthLevelGranted = AUTH_PUBLIC;
    m_pszRemoteUser = NULL;

    if (strlen(pszData) >= MAXUSERPASS) 
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: Base64 data > 256 bytes on Basic Auth, failing request\r\n"));
        return FALSE;
    }

    // decode the base64
    if (! svsutil_Base64Decode(pszData,(void*)szUserName,sizeof(szUserName),NULL,TRUE)) 
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: Base64 data is invalid, failing request\r\n"));
        return FALSE;
    }

    // find the password
    PSTR pszPassword = strchr(szUserName, ':');
    if(!pszPassword) 
    {
        DEBUGMSG(ZONE_ERROR, (L"HTTPD: Bad Format for Basic userpass(%a)-->(%a)\r\n", pszData, szUserName));
        return FALSE;
    }
    *pszPassword++ = 0; // seperate user & pass

    // Some  clients prepend a domain name or machine name in front of user name, preceeding by a '\'.
    // Strip this out.
    if (NULL != (pszFinalUserName = strchr(szUserName,'\\'))) 
    {
        DEBUGMSG(ZONE_AUTH,(L"HTTPD: UserName <<%s>> has \\ in it, skip preceeding data\r\n",szUserName));
        pszFinalUserName++;
    }
    else
    {
        pszFinalUserName = szUserName;
    }

    //  We save the data no matter what, for logging purposes and for possible
    //  GetServerVariable call.
    m_pszRemoteUser = MySzDupA(pszFinalUserName);
    m_pszPassword   = MySzDupA(pszPassword);

    if ((NULL==m_pszRemoteUser) || (NULL==m_pszPassword))
    {
        return FALSE;
    }

    if (! CallAuthFilterIfNeeded())
    {
        return FALSE;
    }

    WCHAR wszPassword[MAXUSERPASS];
    WCHAR wszRemoteUser[MAXUSERPASS];
    MyA2W(m_pszPassword, wszPassword, CCHSIZEOF(wszPassword));
    MyA2W(m_pszRemoteUser,wszRemoteUser, CCHSIZEOF(wszRemoteUser));

    // BasicToNTLM and helpers requires m_wszRemoteUser set.
    if (NULL == (m_wszRemoteUser = MySzDupW(wszRemoteUser)))
    {
        return FALSE;
    }

    if (BasicToNTLM(wszPassword))
    {
        return TRUE;
    }

    // On failure, remove m_wszRemoteUser
    ZeroAndFree(m_wszRemoteUser);
    m_AuthLevelGranted = AUTH_PUBLIC;

    DEBUGMSG(ZONE_ERROR, (L"HTTPD: Failed logon with Basic userpass(%a)-->(%a)(%a)\r\n", pszData, pszFinalUserName, pszPassword));
    return FALSE;
}

BOOL CGlobalVariables::InitSecurityLib(void) 
{
    DEBUG_CODE_INIT;
    FARPROC pInit;
    SECURITY_STATUS ss = 0;
    BOOL ret = FALSE;
    PSecurityFunctionTable pSecFun;

    if (!m_fRootSite)  
    {
        // No need to reload DLL and get new interface ptrs for each website.
        if (g_pVars->m_hSecurityLib) 
        {
            memcpy(&m_SecurityInterface,&g_pVars->m_SecurityInterface,sizeof(m_SecurityInterface));
            m_cbMaxToken = g_pVars->m_cbMaxToken;
            return TRUE;
        }
        return FALSE;
    }
    DEBUGCHK(!m_hSecurityLib);

    m_hSecurityLib = LoadLibrary (SECURITY_DLL_NAME);
    if (NULL == m_hSecurityLib)  
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: Unable to load %s, GLE=0x%08x\r\n",SECURITY_DLL_NAME,GetLastError()));
        myleave(700);
    }

    pInit = (FARPROC) GetProcAddress (m_hSecurityLib, SECURITY_ENTRYPOINT_CE);
    if (NULL == pInit)  
    {
        DEBUGCHK(0);
        myleave(701);
    }

    pSecFun = (PSecurityFunctionTable) pInit ();
    if (NULL == pSecFun) 
    {
        DEBUGCHK(0);
        myleave(702);
    }

    memcpy(&m_SecurityInterface,pSecFun,sizeof(SecurityFunctionTable));

    PSecPkgInfo pkgInfo;
    ss = m_SecurityInterface.QuerySecurityPackageInfo (NTLM_PACKAGE_NAME, &pkgInfo);
    if (!SEC_SUCCESS(ss))  
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: QuerySecurityPackageInfo failed, GLE=0x%08x\r\n",GetLastError()));
        myleave(703);
    }

    m_cbMaxToken = pkgInfo->cbMaxToken;
    m_SecurityInterface.FreeContextBuffer (pkgInfo);

    m_SecurityInterface.EncryptMessage = (ENCRYPT_MESSAGE_FN) m_SecurityInterface.Reserved3;
    m_SecurityInterface.DecryptMessage = (DECRYPT_MESSAGE_FN) m_SecurityInterface.Reserved4;

    DEBUGMSG(ZONE_AUTH,(L"HTTPD: Security Library successfully initialized\r\n"));

    ret = TRUE;
done:
    if (FALSE == ret) 
    {
        CReg reg(HKEY_LOCAL_MACHINE,RK_HTTPD);
        MyFreeLib (m_hSecurityLib);
        m_hSecurityLib = 0;

        // only complain if the user requested NTLM.
        if (reg.ValueDW(RV_NTLM) || reg.ValueDW(RV_NEGOTIATE)) 
        {
            if (ss == 0) // if unset above, then API that failed uses SetLastError().
            {
                ss = GetLastError();
            }

            m_pLog->WriteEvent(IDS_HTTPD_AUTH_INIT_ERROR,ss);
        }
    }

    return ret;
}


// This function is called 2 times during an NTLM auth session.  The first
// time it has the Client user name, domain,... which it forwards to DC or
// looks in registry for (this NTLM detail is transparent to httpd)
// On 2nd time it has the client's response, which either is or is not 
// enough to grant access to page.  On 2nd pass, free up all NTLM context data.

// FILTER NOTES:  On IIS no user name or password info is given to the filter
// on NTLM calls, so neither do we.  (WinCE does give BASIC data, though).

BOOL CHttpRequest::HandleNTLMNegotiateAuth(PSTR pszSecurityToken, BOOL fUseNTLM) 
{
    DEBUG_CODE_INIT;
    BOOL ret = FALSE;
    DWORD dwIn;
    DWORD dwOut;
    DWORD dwSecurityOutBufLen;
    BOOL fDone = FALSE;
    PBYTE pOutBuf = NULL;
    PBYTE pInBuf = NULL;   // Base64 decoded data from pszSecurityToken
    const TCHAR *szPackageName = fUseNTLM ? NTLM_PACKAGE_NAME : NEGOTIATE_PACKAGE_NAME ;

    DEBUGCHK((fUseNTLM && AllowNTLM()) || (!fUseNTLM && AllowNegotiate()));
    
    if (!g_pVars->m_hSecurityLib)
    {
        myretleave(FALSE,94);
    }

    dwOut = g_pVars->m_cbMaxToken;
    dwSecurityOutBufLen = 1 + (dwOut + 2) / 3 * 4;
    if (NULL == (pOutBuf = MyRgAllocNZ(BYTE,dwOut)))
    {
        myleave(360);
    }

    if (NULL == m_pszSecurityOutBuf) 
    {
        // We will later Base64Encode pOutBuf, encoding writes 4 outbut bytes
        // for every 3 input bytes
        if (NULL == (m_pszSecurityOutBuf = MyRgAllocNZ(CHAR,dwSecurityOutBufLen)))
        {
            myleave(361);
        }
    }

    dwIn = strlen(pszSecurityToken) + 1;
    if (NULL == (pInBuf = MyRgAllocNZ(BYTE,dwIn)))
    {
        myleave(363);
    }

    if (! svsutil_Base64Decode(pszSecurityToken,pInBuf,dwIn,&dwIn,FALSE))
    {
        myleave(365);
    }

    //  On the 1st pass this gets a data blob to be sent back to the client
    //  broweser in pOutBuf, which is encoded to m_pszSecurityOutBuf.  On the 2nd
    //  pass it either authenticates or fails.


    if (! SecurityServerContext(NULL, &m_AuthState, pInBuf,
                            dwIn,pOutBuf,&dwOut,&fDone,szPackageName,
                            &(m_SSLInfo.channelBindingToken)))
    {
        // Note:  We MUST free the m_pszNTMLOutBuf on 2nd pass failure.  If the 
        // client receives the blob on a failure
        // it will consider the web server to be malfunctioning and will not send
        // another set of data, and will not prompt the user for a password.
        MyFree(m_pszSecurityOutBuf);


        // Setting to DONE will cause the local structs to be freed; they must
        // be fresh in case browser attempts to do NTLM again with new user name/
        // password on same session.  Don't bother unloading the lib.
        m_AuthState.m_Stage = SEC_STATE_DONE;
        myleave(362);
    }
    
    if (fDone) 
    {
        DEBUGMSG(ZONE_AUTH,(L"HTTPD: Successfully authenticated user\r\n"));
        GetUserAndGroupInfo(&m_AuthState);
        m_AuthLevelGranted = DeterminePermissionGranted(GetUserList(),AUTH_USER);
        
        m_dwAuthFlags |= m_AuthLevelGranted;
        m_AuthState.m_Stage = SEC_STATE_DONE;
        MyFree(m_pszSecurityOutBuf);
        
        myretleave(TRUE,0);
    }

    ret = svsutil_Base64Encode(pOutBuf, dwOut, m_pszSecurityOutBuf, dwSecurityOutBufLen, NULL);
done:
    DEBUGMSG_ERR(ZONE_ERROR,(L"HTTPD: HandleNTLMNegotiateAuth died, err = %d, gle = %d\r\n",err,GetLastError()));
    
    MyFree(pOutBuf);
    MyFree(pInBuf);
    return ret;
}



//  Unload the contexts.  The library is NOT freed in this call, only freed
//  in CHttpRequest destructor.
void FreeSecContextHandles(PAUTH_STATE pAuthState) 
{
    if (NULL == pAuthState || NULL == g_pVars->m_hSecurityLib)
    {
        return;
    }
        
    if (pAuthState->m_fHaveCtxtHandle)
    {
        g_pVars->m_SecurityInterface.DeleteSecurityContext (&pAuthState->m_hctxt);
    }

    if (pAuthState->m_fHaveCredHandle)
    {
        g_pVars->m_SecurityInterface.FreeCredentialHandle (&pAuthState->m_hcred);
    }

    pAuthState->m_fHaveCredHandle = FALSE;
    pAuthState->m_fHaveCtxtHandle = FALSE;
    pAuthState->m_AuthType        = AUTHTYPE_NONE;
}




//  Given Basic authentication data, we try to "forge" and NTLM request
//  This fcn simulates a client+server talking to each other, though it's in the
//  same proc.  The client is "virtual," doesn't refer to the http client

//  pAuthState is CHttpRequest::m_AuthState
BOOL CHttpRequest::BasicToNTLM(WCHAR * wszPassword) 
{
    DEBUG_CODE_INIT;

    AUTH_STATE  ClientState;            // forges the client role
    AUTH_STATE  ServerState;            // forges the server role
    BOOL fDone = FALSE;
    PBYTE pClientOutBuf = NULL;
    PBYTE pServerOutBuf = NULL;
    DWORD cbServerBuf;
    DWORD cbClientBuf;
    
    DEBUGCHK(wszPassword != NULL && m_wszRemoteUser != NULL);
        
    SEC_WINNT_AUTH_IDENTITY AuthIdentityClient = 
    {
                        m_wszRemoteUser, wcslen(m_wszRemoteUser), 
                        SECURITY_DOMAIN,SVSUTIL_CONSTSTRLEN(SECURITY_DOMAIN),
                        wszPassword, wcslen(wszPassword), 
                        0};        // dummy domain needed

    memset(&ServerState,0,sizeof(AUTH_STATE));
    memset(&ClientState,0,sizeof(AUTH_STATE));

    if (!g_pVars->m_hSecurityLib)
    {
        myleave(368);
    }

    // NTLM auth functions seem to expect that these buffer will be zeroed.
    pClientOutBuf = MyRgAllocZ(BYTE,g_pVars->m_cbMaxToken);        
    if (NULL == pClientOutBuf)
    {
        myleave(370);
    }

    pServerOutBuf = MyRgAllocZ(BYTE,g_pVars->m_cbMaxToken);        
    if (NULL == pServerOutBuf)
    {
        myleave(371);
    }

    ServerState.m_Stage = SEC_STATE_NO_INIT_CONTEXT;
    ClientState.m_Stage = SEC_STATE_NO_INIT_CONTEXT;

    cbClientBuf = cbServerBuf = g_pVars->m_cbMaxToken;

    //  Main loop that forges client and server talking.
    while (!fDone) 
    {
        cbClientBuf = g_pVars->m_cbMaxToken;
        if (! SecurityClientContext(&AuthIdentityClient,&ClientState,pServerOutBuf,
                                cbServerBuf, pClientOutBuf, &cbClientBuf))
        {
            myleave(372);
        }

        cbServerBuf = g_pVars->m_cbMaxToken;
        if (! SecurityServerContext(&AuthIdentityClient,&ServerState, pClientOutBuf,
                              cbClientBuf, pServerOutBuf, &cbServerBuf, &fDone,NTLM_PACKAGE_NAME, 
                              &(m_SSLInfo.channelBindingToken)))
        {
            myleave(373);
        }
    }

done:
    DEBUGMSG_ERR(ZONE_ERROR,(L"HTTPD: Unable to convert Basic Auth to NTLM Auth, err = %d\r\n",err));
    
    if (fDone) 
    {
        GetUserAndGroupInfo(&ServerState);
        m_AuthLevelGranted = DeterminePermissionGranted(GetUserList(),AUTH_USER);
    }
    
    MyFree(pClientOutBuf);
    MyFree(pServerOutBuf);
    
    FreeSecContextHandles(&ServerState);
    FreeSecContextHandles(&ClientState);

    return fDone;
}

// Adjust Security Flags for CBT 
//
VOID AdjustFlagsForCbt(DWORD *pSecFlags,
                             SecBufferDesc *pInSecDesc,
                             PSecPkgContext_Bindings UniqueBindings)
{
    BOOL fHttps = TRUE;
    SecBuffer *cbtSecBuff;

    //
    // For legacy mode, return right away
    //
    if (g_pVars->m_ChannelBindingInfo.Hardening == HttpAuthenticationHardeningLegacy)
    {
        return ;
    }

    cbtSecBuff = pInSecDesc->pBuffers + pInSecDesc->cBuffers;

    // secure channel is "unbound"
    if (UniqueBindings->Bindings == NULL ||
        UniqueBindings->BindingsLength == 0)
    {
        fHttps = FALSE;
    }

    //
    // Secure channel bindings are not enforced for
    // 1. no Https and no proxy
    // 2. proxy-cohosting
    // If any of these is true, return
    //
    if ((!fHttps && (g_pVars->m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_PROXY) == 0) ||
        (g_pVars->m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_PROXY_COHOSTING))
    {
        return ;
    }

    if (g_pVars->m_ChannelBindingInfo.Hardening == HttpAuthenticationHardenningMedium)
    {
        *pSecFlags |= ASC_REQ_ALLOW_MISSING_BINDINGS;
    }

    if (g_pVars->m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_PROXY)
    {
        *pSecFlags |= ASC_REQ_PROXY_BINDINGS;
    }
    else
    {
        cbtSecBuff->BufferType = SECBUFFER_CHANNEL_BINDINGS;
        cbtSecBuff->pvBuffer = UniqueBindings->Bindings;
        cbtSecBuff->cbBuffer = UniqueBindings->BindingsLength;

        pInSecDesc->cBuffers++;
    }

    return ;
}

NTSTATUS CheckSpn(
    PCtxtHandle hCtxt,
    PSecPkgContext_Bindings UniqueBindings
    )
{
    SecPkgContext_ClientSpecifiedTarget TargetSpn;
    SECURITY_STATUS scRet = SEC_E_OK;
    BOOL fHttps = TRUE;
    NTSTATUS ntRet;
    UINT i = 0;

    if (g_pVars->m_ChannelBindingInfo.Hardening == HttpAuthenticationHardeningLegacy)
    {
        return STATUS_SUCCESS;
    }


    // secure channel is "unbound"
    if (UniqueBindings->Bindings == NULL ||
        UniqueBindings->BindingsLength == 0)
    {
        fHttps = FALSE;
    }

    // https and no proxy, cbt check is enough 
    if (fHttps & !(g_pVars->m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_PROXY))
    {
        return STATUS_SUCCESS;
    }


    if ((g_pVars->m_ChannelBindingInfo.Flags & HTTP_CHANNEL_BIND_NO_SERVICE_NAME_CHECK))
    {
        return STATUS_SUCCESS;
    }

    scRet = g_pVars->m_SecurityInterface.QueryContextAttributes(
                                        hCtxt,
                                        SECPKG_ATTR_CLIENT_SPECIFIED_TARGET,
                                        &TargetSpn);

    ntRet = MapSecurityError(scRet);

    if (STATUS_SUCCESS == ntRet)
    {
        DEBUGMSG(ZONE_AUTH,(L"HTTPD: SPN name is %s.\r\n", TargetSpn.sTargetName));
        
    }
    else if ((STATUS_NOT_SUPPORTED == ntRet) ||
                 (STATUS_BAD_NETWORK_PATH == ntRet)) // SEC_E_TARGET_UNKNOWN was translated to STATUS_BAD_NETWORK_PATH
    {
        //
        // This represents a downlevel client that did not specify a target, and
        // we need to reject this request if server is fully hardening checking running.
        if (STATUS_BAD_NETWORK_PATH == ntRet &&
            g_pVars->m_ChannelBindingInfo.Hardening == HttpAuthenticationHardeningStrict)
        {
            DEBUGMSG(ZONE_AUTH,(L"HTTPD: Fail to downlevel SSP.\r\n"));
            ntRet = STATUS_ACCESS_DENIED;
        }
        else
        {
            ntRet = STATUS_SUCCESS;
            return ntRet;
        }
    }
    else
    {
        ntRet = STATUS_LOGON_FAILURE;
    }

    if (FAILED(ntRet))
    {
        return ntRet;
    }

    for (i=0; i<g_pVars->m_ChannelBindingInfo.NumberOfServiceNames; i++)
    {
        if (g_pVars->m_ChannelBindingInfo.pServiceNames[i].Buffer == NULL)
        {
            //
            // This is a NULL SPN
            //
            continue;
        }
        else
        {
            if( g_pVars->m_ChannelBindingInfo.pServiceNames[i].Base.Type == HttpServiceBindingTypeW )
            {
                ntRet = STATUS_LOGON_FAILURE;
                goto Finished;
            }
            else
            {
                if (_wcsicmp(g_pVars->m_ChannelBindingInfo.pServiceNames[i].Buffer,
                              TargetSpn.sTargetName) == 0)
                {
                    ntRet = STATUS_SUCCESS;
                    goto Finished;
                }
            }
        }
    }

    ntRet = STATUS_LOGON_FAILURE;

Finished:
    g_pVars->m_SecurityInterface.FreeContextBuffer(TargetSpn.sTargetName);
    return ntRet;
                                        
}

NTSTATUS MapSecurityError(
    SECURITY_STATUS secStatus   
)
/*++

    Routine Description:
        Map Security_Status code to NTSTATUS code

    Arguments:
        secStatus : Security status code.

    Return Value:
        Please refer NTSTATUS code definition here.

    Notes:

        None.

--*/
{
    switch(secStatus)
    {
        case SEC_E_NOT_SUPPORTED:
            return STATUS_NOT_SUPPORTED;

        case SEC_E_OK:
            return STATUS_SUCCESS;

        case SEC_E_TARGET_UNKNOWN:
            return STATUS_BAD_NETWORK_PATH;

        default:
            return STATUS_UNSUCCESSFUL;
    }
}




//  This calls the DC (or goes to registry in local case), either getting a
//  data blob to return to client or granting auth or denying.

BOOL SecurityServerContext(
            PSEC_WINNT_AUTH_IDENTITY pAuthIdentity,
            PAUTH_STATE pAS,        // security state info
            BYTE *pIn,
            DWORD cbIn,
            BYTE *pOut,
            DWORD *pcbOut,
            BOOL *pfDone,
            const TCHAR *szPackageName,
            PSecPkgContext_Bindings UniqueBindings)
{
    SECURITY_STATUS    ss;
    TimeStamp        Lifetime;
    SecBufferDesc    OutBuffDesc;
    SecBuffer        OutSecBuff;
    SecBufferDesc    InBuffDesc;
    SecBuffer        InSecBuff[2];
    ULONG            ContextAttributes;
    ULONG            SecFlags;
    
    if (SEC_STATE_NO_INIT_CONTEXT == pAS->m_Stage)  
    {
        ss = g_pVars->m_SecurityInterface.AcquireCredentialsHandle (
                            NULL,    // principal
                            (TCHAR *)szPackageName,
                            SECPKG_CRED_INBOUND,
                            NULL,    // LOGON id
                            pAuthIdentity,    
                            NULL,    // get key fn
                            NULL,    // get key arg
                            &pAS->m_hcred,
                            &Lifetime
                            );
        if (SEC_SUCCESS (ss))
        {
            pAS->m_fHaveCredHandle = TRUE;
        }
        else 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: NTLM AcquireCreds failed: %X\n", ss));
            return(FALSE);
        }
    }

    // prepare output buffer
    //
    OutBuffDesc.ulVersion = 0;
    OutBuffDesc.cBuffers = 1;
    OutBuffDesc.pBuffers = &OutSecBuff;

    OutSecBuff.cbBuffer = *pcbOut;
    OutSecBuff.BufferType = SECBUFFER_TOKEN;
    OutSecBuff.pvBuffer = pOut;

    // prepare input buffer
    //
    InBuffDesc.ulVersion = 0;
    InBuffDesc.cBuffers = 1;
    InBuffDesc.pBuffers = InSecBuff;

    InSecBuff[0].cbBuffer = cbIn;
    InSecBuff[0].BufferType = SECBUFFER_TOKEN;
    InSecBuff[0].pvBuffer = pIn;


    SecFlags = ASC_REQ_DELEGATE | ASC_REQ_EXTENDED_ERROR;
    
    //
    // Adjust for CBT
    //
    AdjustFlagsForCbt(&SecFlags,
                      &InBuffDesc,
                      UniqueBindings );

    ss = g_pVars->m_SecurityInterface.AcceptSecurityContext (
                        &pAS->m_hcred,
                        (pAS->m_Stage == SEC_STATE_PROCESSING) ?  &pAS->m_hctxt : NULL,
                        &InBuffDesc,
                        SecFlags,    // context requirements
                        SECURITY_NATIVE_DREP,
                        &pAS->m_hctxt,
                        &OutBuffDesc,
                        &ContextAttributes,
                        &Lifetime
                        );
    if (!SEC_SUCCESS (ss))  
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: NTLM init context failed: %X\n", ss));
        return FALSE;
    }

    pAS->m_fHaveCtxtHandle = TRUE;

    // Complete token -- if applicable
    //
    if ((SEC_I_COMPLETE_NEEDED == ss) || (SEC_I_COMPLETE_AND_CONTINUE == ss))  
    {
        if (g_pVars->m_SecurityInterface.CompleteAuthToken) 
        {
            ss = g_pVars->m_SecurityInterface.CompleteAuthToken (&pAS->m_hctxt, &OutBuffDesc);
            if (!SEC_SUCCESS(ss))  
            {
                DEBUGMSG(ZONE_ERROR,(L"HTTPD:  NTLM complete failed: %X\n", ss));
                return FALSE;
            }
        }
        else 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: Complete not supported.\n"));
            return FALSE;
        }
    }

    *pcbOut = OutSecBuff.cbBuffer;
    pAS->m_Stage = SEC_STATE_PROCESSING;

    *pfDone = !((SEC_I_CONTINUE_NEEDED == ss) ||
                (SEC_I_COMPLETE_AND_CONTINUE == ss));

    if (*pfDone == TRUE)
    {
        if (CheckSpn(&pAS->m_hctxt, UniqueBindings) != STATUS_SUCCESS)
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: SPN check error.\n"));
            return FALSE;
        }
    }

    return TRUE;
}


//  Forges the client browser's part in NTLM communication if the browser
//  sent a Basic request.
BOOL SecurityClientContext(
            PSEC_WINNT_AUTH_IDENTITY pAuthIdentity,
            PAUTH_STATE pAS,        // NTLM state info
            BYTE *pIn,
            DWORD cbIn,
            BYTE *pOut,
            DWORD *pcbOut) 
{
    SECURITY_STATUS    ss;
    TimeStamp        Lifetime;
    SecBufferDesc    OutBuffDesc;
    SecBuffer        OutSecBuff;
    SecBufferDesc    InBuffDesc;
    SecBuffer        InSecBuff;
    ULONG            ContextAttributes;

    if (SEC_STATE_NO_INIT_CONTEXT == pAS->m_Stage)  
    {
        ss = g_pVars->m_SecurityInterface.AcquireCredentialsHandle (
                            NULL,    // principal
                            NTLM_PACKAGE_NAME,
                            SECPKG_CRED_OUTBOUND,
                            NULL,    // LOGON id
                            pAuthIdentity,    // auth data
                            NULL,    // get key fn
                            NULL,    // get key arg
                            &pAS->m_hcred,
                            &Lifetime
                            );
        if (SEC_SUCCESS (ss))
        {
            pAS->m_fHaveCredHandle = TRUE;
        }
        else 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: AcquireCreds failed: %X\n", ss));
            return(FALSE);
        }
    }

    // prepare output buffer
    //
    OutBuffDesc.ulVersion = 0;
    OutBuffDesc.cBuffers = 1;
    OutBuffDesc.pBuffers = &OutSecBuff;

    OutSecBuff.cbBuffer = *pcbOut;
    OutSecBuff.BufferType = SECBUFFER_TOKEN;
    OutSecBuff.pvBuffer = pOut;

    // prepare input buffer
    
    if (SEC_STATE_NO_INIT_CONTEXT != pAS->m_Stage)  
    {
        InBuffDesc.ulVersion = 0;
        InBuffDesc.cBuffers = 1;
        InBuffDesc.pBuffers = &InSecBuff;

        InSecBuff.cbBuffer = cbIn;
        InSecBuff.BufferType = SECBUFFER_TOKEN;
        InSecBuff.pvBuffer = pIn;
    }

    ss = g_pVars->m_SecurityInterface.InitializeSecurityContext (
                        &pAS->m_hcred,
                        (pAS->m_Stage == SEC_STATE_PROCESSING) ? &pAS->m_hctxt : NULL,
                        NULL,  
                        0,    // context requirements
                        0,    // reserved1
                        SECURITY_NATIVE_DREP,
                        (pAS->m_Stage == SEC_STATE_PROCESSING) ?  &InBuffDesc : NULL,
                        0,    // reserved2
                        &pAS->m_hctxt,
                        &OutBuffDesc,
                        &ContextAttributes,
                        &Lifetime
                        );
    if (!SEC_SUCCESS (ss))  
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: init context failed: %X\n", ss));
        return FALSE;
    }

    pAS->m_fHaveCtxtHandle = TRUE;

    // Complete token -- if applicable
    //
    if ((SEC_I_COMPLETE_NEEDED == ss) || (SEC_I_COMPLETE_AND_CONTINUE == ss))  
    {
        if (g_pVars->m_SecurityInterface.CompleteAuthToken) 
        {
            ss = g_pVars->m_SecurityInterface.CompleteAuthToken (&pAS->m_hctxt, &OutBuffDesc);
            if (!SEC_SUCCESS(ss))  
            {
                DEBUGMSG(ZONE_ERROR,(L"HTTPD: complete failed: %X\n", ss));
                return FALSE;
            }
        }
        else 
        {
            DEBUGMSG(ZONE_ERROR,(L"HTTPD: Complete not supported.\n"));
            return FALSE;
        }
    }

    *pcbOut = OutSecBuff.cbBuffer;
    pAS->m_Stage = SEC_STATE_PROCESSING;

    return TRUE;
}


// Gets group and user info from NTLM structure after a user has retrieved info.
// Called after a user has been successfully authenticated with either BASIC or NTLM.
// See \winceos\comm\security\authhlp to see ACL algorithm function/description.
void CHttpRequest::GetUserAndGroupInfo(PAUTH_STATE pAuth)
{
    PWSTR wsz = NULL;
    SecPkgContext_Names pkgName;
    pkgName.sUserName = NULL;

#if defined(UNDER_CE)
    SecPkgContext_GroupNames ContextGroups;    
    ContextGroups.msGroupNames = NULL;
    int ccGroupNamesLen = 0; // null delimited string; wcslen won't work!
#endif    

    // If we're called from NTLM request (pAuth != NULL) then we need 
    // to get the user name if we don't have it.  On BASIC request, we
    // have m_wszRemoteUser name already.
    if ((NULL==m_wszRemoteUser) && ! SEC_SUCCESS(g_pVars->m_SecurityInterface.QueryContextAttributes(&(pAuth->m_hctxt),
                       SECPKG_ATTR_NAMES, &pkgName)))
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: QueryContextAttributes failed, cannot determine user name!\r\n"));
        goto done;     // If we can't get user name, don't bother continuing
    }

#if defined(UNDER_CE)
    g_pVars->m_SecurityInterface.QueryContextAttributes(&(pAuth->m_hctxt),SECPKG_ATTR_GROUP_NAMES,&ContextGroups);
    // Group list is returned to us a NULL delimited list.  Need to calculate len of this multi-string, wcslen() is out.
    if (ContextGroups.msGroupNames) 
    {
        PWSTR szTrav = ContextGroups.msGroupNames;

        while (*szTrav) 
        {
            szTrav += wcslen(szTrav)+1;
        }
        ccGroupNamesLen = szTrav - ContextGroups.msGroupNames + 1;
    }
#endif
        
    DEBUGMSG(ZONE_AUTH,(L"HTTPD: Admin Users = %s, pkgName.sUserName = %s, group name = %s\r\n",
             m_pWebsite->m_wszAdminUsers,pkgName.sUserName,ContextGroups.msGroupNames));

done:
    if (pkgName.sUserName) 
    {
        DEBUGCHK(m_wszRemoteUser == NULL);
        m_wszRemoteUser = MySzDupW(pkgName.sUserName);
        m_pszRemoteUser = MySzDupWtoA(m_wszRemoteUser);
        g_pVars->m_SecurityInterface.FreeContextBuffer(pkgName.sUserName);
    }
#if defined(UNDER_CE)
    DEBUGCHK(m_wszMemberOf == NULL);

    if (ContextGroups.msGroupNames && (ccGroupNamesLen >1)) 
    {
        m_wszMemberOf = MySzAllocW(ccGroupNamesLen);
        if (m_wszMemberOf) 
        {
            memcpy(m_wszMemberOf,ContextGroups.msGroupNames,ccGroupNamesLen*sizeof(WCHAR));
            DEBUGCHK(m_wszMemberOf[ccGroupNamesLen-1] == 0 && m_wszMemberOf[ccGroupNamesLen-2] == 0); // last 2 bytes should = 0.
        }
    }
    if (ContextGroups.msGroupNames)
    {
        g_pVars->m_SecurityInterface.FreeContextBuffer(ContextGroups.msGroupNames);
    }
#endif
}


AUTHLEVEL CHttpRequest::DeterminePermissionGranted(const WCHAR *wszUserList, AUTHLEVEL authDefault) 
{
    if (!m_wszRemoteUser) 
    {
        DEBUGMSG(ZONE_AUTH,(L"HTTPD: DeterminePermissionGranted returns default level %d because there's no user\r\n",authDefault));
        return authDefault;
    }

    // Administrators always get admin access and access to the page, even if
    // they're barred in the VRoot list.  If there is a vroot list and we fail
    // the IsAccessAllowed test, we set the auth granted to 0 - this
    // will deny access.  If no VRoot user list is set, keep us at AUTH_USER.
    if (m_pWebsite->m_wszAdminUsers && IsAccessAllowed(m_wszRemoteUser,m_wszMemberOf,m_pWebsite->m_wszAdminUsers,FALSE)) 
    {
        DEBUGMSG(ZONE_AUTH,(L"HTTPD: DeterminePermissionGranted returns AUTH_ADMIN because %s is in admin user list\r\n",m_wszRemoteUser));
        return AUTH_ADMIN;
    }
    else if (wszUserList && IsAccessAllowed(m_wszRemoteUser,m_wszMemberOf,(WCHAR*)wszUserList,FALSE)) 
    {
        DEBUGMSG(ZONE_AUTH,(L"HTTPD: DeterminePermissionGranted returns AUTH_USER because %s is in user list\r\n",m_wszRemoteUser));
        return AUTH_USER;
    }
    else if (wszUserList) 
    {
        DEBUGMSG(ZONE_AUTH,(L"HTTPD: DeterminePermissionGranted returns AUTH_PUBLIC because %s was not in user list\r\n",m_wszRemoteUser));
        return AUTH_PUBLIC;
    }
    else 
    {
        DEBUGMSG(ZONE_AUTH,(L"HTTPD: DeterminePermissionGranted returns default level permission %d because not in Adminlist/userlist\r\n",authDefault));
        return authDefault;
    }
}


