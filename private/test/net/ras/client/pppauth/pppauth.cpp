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
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include "pppauth.h"
#include "connect.h"
#include <tux.h>
#include "common.h"

#define EXPECTED_PASS 0
#define EXPECTED_FAIL -1
#define PPPAUTH_MAX_AUTH_OPTIONS    11
#define PPPAUTH_HAS_X_AUTHTYPE(a,b) ((a) & (b))

typedef struct _PPPAuthParamsDef
{
    DWORD dwEAPExtension;
    DWORD dwThisAuthOption;
    DWORD dwExpectedValue;
}PPPAuthParamsDef;

extern CmdLineParams cmdParameters;
extern RASConnectionType eRasConnection;

TCHAR lpszEntryName[RAS_MaxEntryName + 1];

static TCHAR pppauthRasConnectionStrPrefix[RAS_INVALID+1][MAX_RAS_CONN_STR_PREFIX_LENGTH] = 
{
    TEXT("PPTP"),
    TEXT("L2TP"),
    TEXT("Serial"),
    TEXT("MODEM"),
    TEXT("PPPOE"),
    TEXT("INVALID")
};

static const sAuthOptions * 
pppauthGetAuthOptions(
    void
    )
{
    static const sAuthOptions AuthOptions[] =
    {
        {RASEO_ProhibitPAP,             TEXT("RASEO_ProhibitPAP")},
        {RASEO_ProhibitMsCHAP2,         TEXT("RASEO_ProhibitMSCHAPv2")},
        {RASEO_ProhibitMsCHAP,          TEXT("RASEO_ProhibitMSCHAP")},
        {RASEO_ProhibitCHAP,            TEXT("RASEO_ProhibitCHAP")},
        {RASEO_ProhibitEAP,             TEXT("RASEO_ProhibitEAP")},
        {RASEO_RequireEncryptedPw,      TEXT("RASEO_RequireEncryptedPw")},
        {RASEO_RequireMsEncryptedPw,    TEXT("RASEO_RequireMsEncryptedPw")},
        {RASEO_RequireDataEncryption,   TEXT("RASEO_RequireDataEncryption")},
        {RASEO_RequireEncryptedPw | RASEO_RequireMsEncryptedPw,
                                        TEXT("RASEO_RequireEncryptedPw | RASEO_RequireMsEncryptedPw")},
        {RASEO_RequireMsEncryptedPw | RASEO_RequireDataEncryption,
                                        TEXT("RASEO_RequireMsEncryptedPw | RASEO_RequireDataEncryption")},
        {RASEO_RequireEncryptedPw | RASEO_RequireMsEncryptedPw | RASEO_RequireDataEncryption,
                                        TEXT("RASEO_RequireEncryptedPw | RASEO_RequireMsEncryptedPw | RASEO_RequireDataEncryption")}
    };    
    return &AuthOptions[0];
}

static RASConnectionType 
pppauthGetRasConnectionType(
    DWORD dwUserData
    )
{
    RASConnectionType rasConn = RAS_INVALID;
    
    if (USE_PPTP & dwUserData)
    {
        rasConn = RAS_VPN_PPTP;
    }
    else if (USE_L2TP & dwUserData)
    {
        rasConn = RAS_VPN_L2TP;
    }
    else if (USE_DCC & dwUserData)
    {
        rasConn = RAS_DCC_MODEM;
    }
    else
    {
        ;// Taken care of by initialization
    }
    RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("pppauthGetRasConnectionType() : %s"),pppauthRasConnectionStrPrefix[rasConn]);
    return rasConn;
}

static void
SetPPPAuthParamsPAP(
    DWORD               dwUserData,
    PPPAuthParamsDef    *pAuthParams
    )
{
    RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("SetPPPAuthParamsPAP..."));
    if (NULL != pAuthParams)
    {
        pAuthParams->dwThisAuthOption |= AUTH_MASK_PAP;
        switch(LOWORD(dwUserData))
        {
            case REQUIRE_NO_FLAGS:
            case PROHIBIT_CHAP:
            case PROHIBIT_MSCHAP:
            case PROHIBIT_MSCHAPV2:
            case PROHIBIT_EAP:
                pAuthParams->dwExpectedValue = EXPECTED_PASS;
                break;
            default:
                pAuthParams->dwExpectedValue = (DWORD)EXPECTED_FAIL;
                break;
        }
    }
    else
    {
        RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("pAuthParams == NULL!!!!"));
    }        
}

static void
SetPPPAuthParamsEAP(
    DWORD               dwUserData,
    PPPAuthParamsDef    *pAuthParams
    )
{
    RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("SetPPPAuthParamsEAP()"));
    if (NULL != pAuthParams)
    {
        pAuthParams->dwThisAuthOption |= AUTH_MASK_EAP;
        pAuthParams->dwEAPExtension = PPP_EAP_MSCHAPv2;//Replaced MD5-CHALLENGE with MSHCHAPv2;
        if (PPP_EAP_MSCHAPv2 == pAuthParams->dwEAPExtension)
        {
            if (PROHIBIT_EAP == LOWORD(dwUserData))
            {
                pAuthParams->dwExpectedValue = (DWORD)EXPECTED_FAIL;
            }  
        }
        else if(PPP_EAP_CHAP == pAuthParams->dwEAPExtension)
        {
            if (PROHIBIT_EAP            == LOWORD(dwUserData) ||
                REQUIRE_MS_DATA_ENC_PW  == LOWORD(dwUserData) ||
                REQUIRE_MS_DATA_ENC     == LOWORD(dwUserData))
            {
                pAuthParams->dwExpectedValue = (DWORD)EXPECTED_FAIL;
            }
        }
    }
    else
    {
        RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("pAuthParams == NULL!!!!"));
    }     
}

static void
SetPPPAuthParamsCHAP(
    DWORD               dwUserData,
    PPPAuthParamsDef    *pAuthParams
    )
{
    RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("SetPPPAuthParamsCHAP()"));
    if (NULL != pAuthParams)
    {
        pAuthParams->dwThisAuthOption |= AUTH_MASK_CHAP_MD5;
        if (PROHIBIT_CHAP           == LOWORD(dwUserData) ||
            REQUIRE_MS_ENC_PW       == LOWORD(dwUserData) ||
            REQUIRE_DATA_ENC        == LOWORD(dwUserData) ||
            REQUIRE_ENC_MS_PW       == LOWORD(dwUserData) ||
            REQUIRE_MS_DATA_ENC     == LOWORD(dwUserData) ||
            REQUIRE_MS_DATA_ENC_PW  == LOWORD(dwUserData))
        {
            pAuthParams->dwExpectedValue = (DWORD)EXPECTED_FAIL;
        }
    }
    else
    {
        RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("pAuthParams == NULL!!!!"));
    }
}

static void
SetPPPAuthParamsMSCHAP(
    DWORD               dwUserData,
    PPPAuthParamsDef    *pAuthParams
    )
{
    RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("SetPPPAuthParamsMSCHAP()"));
    if (NULL != pAuthParams)
    {
        pAuthParams->dwThisAuthOption |= AUTH_MASK_CHAP_MS;
        if (PROHIBIT_MSCHAP == LOWORD(dwUserData))
        {
            pAuthParams->dwExpectedValue = (DWORD)EXPECTED_FAIL;
        }
    }
    else
    {
        RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("pAuthParams == NULL!!!!"));
    }        
}

static void
SetPPPAuthParamsMSCHAPv2(
    DWORD               dwUserData,
    PPPAuthParamsDef    *pAuthParams
    )
{
    RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("SetPPPAuthParamsMSCHAPv2()"));
    if (NULL != pAuthParams)
    {
        pAuthParams->dwThisAuthOption |= AUTH_MASK_CHAP_MSV2;
        if (PROHIBIT_MSCHAPV2 == LOWORD(dwUserData))
        {
            pAuthParams->dwExpectedValue = (DWORD)EXPECTED_FAIL;
        }
    }
    else
    {
        RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("pAuthParams == NULL!!!!"));
    }         
}

static void 
SetPPPAuthParams(
    DWORD               dwUserData,
    PPPAuthParamsDef    *pAuthParams
    )
{
    RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("SetPPPAuthParams()"));
    // DCC connections will always pass. This is how the RAS Servers
    // in the automation lab are set-up - no authentication.
    //
    if (USE_DCC & dwUserData)
    {
        pAuthParams->dwThisAuthOption = AUTH_MASK_MIX;
        pAuthParams->dwExpectedValue = EXPECTED_PASS;
    }
    else
    {
        if(PPPAUTH_HAS_X_AUTHTYPE(USE_NODCC,dwUserData))
        {
            pAuthParams->dwThisAuthOption |= AUTH_MASK_MIX;
        }
        else if(PPPAUTH_HAS_X_AUTHTYPE(USE_EAP,dwUserData))
        {
            SetPPPAuthParamsEAP(dwUserData, pAuthParams);
        }
        else if(PPPAUTH_HAS_X_AUTHTYPE(USE_PAP,dwUserData))
        {
            SetPPPAuthParamsPAP(dwUserData, pAuthParams);
        }
        else if(PPPAUTH_HAS_X_AUTHTYPE(USE_CHAP,dwUserData))
        {
            SetPPPAuthParamsCHAP(dwUserData, pAuthParams);
        }
        else if(PPPAUTH_HAS_X_AUTHTYPE(USE_MSCHAP,dwUserData))
        {
            SetPPPAuthParamsMSCHAP(dwUserData, pAuthParams);
        }
        else if(PPPAUTH_HAS_X_AUTHTYPE(USE_MSCHAPv2,dwUserData))
        {
            SetPPPAuthParamsMSCHAPv2(dwUserData, pAuthParams);
        }
    }
    
}

static void 
RAS_CreatePBE_Connect_Disconnect(
    PPPAuthParamsDef    *pAuthParams,
    INT                 *piNumFailures
    )
{
    DWORD dwErrorReceived = 0;

    RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("RAS_CreatePBE_Connect_Disconnect()"));
    
    // Create PhoneBookEntry
    if (FALSE == CreatePhoneBookEntry(pAuthParams->dwThisAuthOption, pAuthParams->dwEAPExtension, &dwErrorReceived))
    {
        (*piNumFailures)++;
        RASPPPAuthLog(RAS_LOG_FAIL, TEXT("CreatePhoneBookEntry() FAIL'ed %d dwErrorReceived = %d\n"),*piNumFailures,dwErrorReceived);
    }
                    
    SLEEP();

    // Attempt RAS Connection
    if (FALSE == ConnectRasConnection(&dwErrorReceived))
    {
        (*piNumFailures)++;
        RASPPPAuthLog(RAS_LOG_FAIL, TEXT("ConnectRasConnection() FAIL'ed %d dwErrorReceived = %d\n"),*piNumFailures,dwErrorReceived);
    }
    else
    {
        RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("+++ Connected Successfully +++"));
    }
                    
    SLEEP();

    // Teardown RAS Connection                       
    if (FALSE == DisconnectRasConnection(&dwErrorReceived))
    {
        (*piNumFailures)++;
        RASPPPAuthLog(RAS_LOG_FAIL, TEXT("DisConnectRasConnection() FAIL'ed %d dwErrorReceived = %d"),*piNumFailures,dwErrorReceived);
    }
}

TESTPROCAPI 
RASAuthTest (
    UINT                    uMsg,
    TPPARAM                 tpParam,
    LPFUNCTION_TABLE_ENTRY  lpFTE
    )
{
    INT                 iNumFailures=0;
    PPPAuthParamsDef    pppAuthParams = {0,0,EXPECTED_PASS};
    const sAuthOptions  *pAuthOptions = NULL;
    INT                 dwRetVal = TPR_FAIL;
    
    // Check our message value to see why we have been called
    if (uMsg == TPM_QUERY_THREAD_COUNT) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0 /* DEFAULT_THREAD_COUNT */;
        return TPR_HANDLED;
    }
    else if (uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }

    pAuthOptions = pppauthGetAuthOptions();
    if (NULL == pAuthOptions)
    {
        RASPPPAuthLog(RAS_LOG_FAIL, TEXT("NO AUTH OPTIONS!!!! Cannot continue!"));
    }
    else
    {
        // What is the connection type? Set eRasConnection
        eRasConnection = pppauthGetRasConnectionType(lpFTE->dwUserData);
        if (RAS_INVALID == eRasConnection)
        {
            RASPPPAuthLog(RAS_LOG_FAIL, TEXT("INVALID RAS CONNECTION TYPE specified!!!! Cannot continue!"));    
        }
        else
        {
            //
            // Construct the entry name and also set cmdParameters.dwDeviceID
            //
            if (FALSE == bGetDeviceType())
            {
                RASPPPAuthLog(RAS_LOG_FAIL, TEXT("DEVICE ID NOT FOUND!!!! Cannot continue!"));
            }
            else
            {
                if (IPV6_TEST & lpFTE->dwUserData)
                {
                    _tcscpy_s(cmdParameters.strServerName, cmdParameters.strIPv6Address);
                }
                else
                {
                    _tcscpy_s(cmdParameters.strServerName, cmdParameters.strIPv4Address);
                }

                // Entry Name?
                _stprintf_s(cmdParameters.strEntryName, MAX_CMD_PARAM,pppauthRasConnectionStrPrefix[eRasConnection]);
                COPY_STRINGS(lpszEntryName, cmdParameters.strEntryName, RAS_MaxEntryName + 1);

                pppAuthParams.dwThisAuthOption = pAuthOptions[LOWORD(lpFTE->dwUserData)].dwAuthOptions;
                SetPPPAuthParams(lpFTE->dwUserData,&pppAuthParams);
                
                RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("RAS Flags: %s"), pAuthOptions[LOWORD(lpFTE->dwUserData)].strAuthName);
                
                // Connect to the Server with the parameters specified
                RAS_CreatePBE_Connect_Disconnect(&pppAuthParams, &iNumFailures);
                
                // Was this test case supposed to pass?
                if ((EXPECTED_PASS == pppAuthParams.dwExpectedValue) && (!iNumFailures))
                {
                    dwRetVal = TPR_PASS;
                }
                else if ((EXPECTED_FAIL == pppAuthParams.dwExpectedValue) && (iNumFailures > 0))
                {
                    dwRetVal = TPR_PASS;
                }
                else
                {
                    ;//Handled as part of init
                }
            }
        }
    }
    return dwRetVal;
}

// END OF FILE
