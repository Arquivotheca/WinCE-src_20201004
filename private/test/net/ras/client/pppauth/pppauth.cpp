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
#include "pppauth.h"
#include "connect.h"
#include <tux.h>
#include "common.h"

#define EXPECTED_PASS 0
#define EXPECTED_FAIL -1

extern CmdLineParams cmdParameters;
extern RASConnection eRasConnection;

TCHAR lpszEntryName[RAS_MaxEntryName + 1];

TESTPROCAPI RASAuthTest (UINT uMsg,
                         TPPARAM tpParam,
                         LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT iNumFailures=0;
    DWORD dwEAPExtension=0;
    DWORD dwThisAuthOption=0;
    DWORD dwExpectedValue = EXPECTED_PASS;
    const sAuthOptions Auths[] =
    {
        {
            RASEO_ProhibitPAP,
                TEXT("RASEO_ProhibitPAP")
        },
        {
            RASEO_ProhibitMsCHAP2,
                TEXT("RASEO_ProhibitMSCHAPv2")
            },
            {
                RASEO_ProhibitMsCHAP,
                    TEXT("RASEO_ProhibitMSCHAP")
            },
            {
                RASEO_ProhibitCHAP,
                    TEXT("RASEO_ProhibitCHAP")
                },
                {
                    RASEO_ProhibitEAP,
                        TEXT("RASEO_ProhibitEAP")
                },
                {
                    RASEO_RequireEncryptedPw,
                        TEXT("RASEO_RequireEncryptedPw")
                    },
                    {
                        RASEO_RequireMsEncryptedPw,
                            TEXT("RASEO_RequireMsEncryptedPw")
                    },
                    {
                        RASEO_RequireDataEncryption,
                            TEXT("RASEO_RequireDataEncryption")
                        },
                        {
                            RASEO_RequireEncryptedPw | RASEO_RequireMsEncryptedPw,
                                TEXT("RASEO_RequireEncryptedPw | RASEO_RequireMsEncryptedPw")
                        },
                        {
                            RASEO_RequireMsEncryptedPw | RASEO_RequireDataEncryption,
                                TEXT("RASEO_RequireMsEncryptedPw | RASEO_RequireDataEncryption")
                            },
                            {
                                RASEO_RequireEncryptedPw | RASEO_RequireMsEncryptedPw | RASEO_RequireDataEncryption,
                                    TEXT("RASEO_RequireEncryptedPw | RASEO_RequireMsEncryptedPw | RASEO_RequireDataEncryption")
                            },
                            {
                                0,
                                    TEXT("0")
                                }
    };

    // Check our message value to see why we have been called
    if (uMsg == TPM_QUERY_THREAD_COUNT) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0 /* DEFAULT_THREAD_COUNT */;
        return TPR_HANDLED;
    }
    else if (uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }

    //
    // What is the connection type? Set eRasConnection
    //
    if (USE_PPTP & lpFTE->dwUserData)
        eRasConnection = RAS_VPN_PPTP;
    else if (USE_L2TP & lpFTE->dwUserData)
        eRasConnection = RAS_VPN_L2TP;
    else if (USE_DCC & lpFTE->dwUserData)
        eRasConnection = RAS_DCC_MODEM;

    //
    // Construct the entry name and also set cmdParameters.dwDeviceID
    //
    if (bGetDeviceType()==FALSE)
    {
        return FALSE;
    }

    //
    // Entry Name?
    //
    if (eRasConnection == RAS_VPN_L2TP)
    {
        _stprintf_s(cmdParameters.strEntryName, MAX_CMD_PARAM, TEXT("L2TP @ %s"), cmdParameters.strServerName);
    }
    else if (eRasConnection == RAS_VPN_PPTP)
    {
        _stprintf_s(cmdParameters.strEntryName, MAX_CMD_PARAM, TEXT("PPTP @ %s"), cmdParameters.strServerName);
    }
    else if (eRasConnection == RAS_DCC_MODEM)
    {
        _stprintf_s(cmdParameters.strEntryName, MAX_CMD_PARAM,TEXT("Serial @ %d"), DEFAULT_BAUDRATE);
    }
    else if (eRasConnection == RAS_PPP_MODEM)
    {
        _stprintf_s(cmdParameters.strEntryName, MAX_CMD_PARAM,TEXT("MODEM @ %s"), cmdParameters.strPhoneNum);
    }
    else if (eRasConnection == RAS_PPP_PPPoE)
    {
        _stprintf_s(cmdParameters.strEntryName, MAX_CMD_PARAM,TEXT("PPPOE"));
    }

    COPY_STRINGS(lpszEntryName, cmdParameters.strEntryName, RAS_MaxEntryName + 1);

    dwThisAuthOption = Auths[LOWORD(lpFTE->dwUserData)].dwAuthOptions;
    if(HAS_X_AUTHTYPE(USE_NODCC))
    {
        dwThisAuthOption |= AUTH_MASK_MIX;
    }
    else if(HAS_X_AUTHTYPE(USE_EAP))
    {
        dwThisAuthOption |= AUTH_MASK_EAP;
        dwEAPExtension=MD5_CHALLENGE;
        if (PROHIBIT_EAP == LOWORD(lpFTE->dwUserData) ||
            REQUIRE_MS_DATA_ENC_PW == LOWORD(lpFTE->dwUserData) ||
            REQUIRE_MS_DATA_ENC == LOWORD(lpFTE->dwUserData))
        {
            dwExpectedValue = (DWORD)EXPECTED_FAIL;
        }
    }
    else if(HAS_X_AUTHTYPE(USE_PAP))
    {
        dwThisAuthOption |= AUTH_MASK_PAP;
        if (REQUIRE_NO_FLAGS == LOWORD(lpFTE->dwUserData))
            dwExpectedValue = EXPECTED_PASS;
        else if ((PROHIBIT_CHAP == LOWORD(lpFTE->dwUserData)) ||
            (PROHIBIT_MSCHAP == LOWORD(lpFTE->dwUserData)) ||
            (PROHIBIT_MSCHAPV2== LOWORD(lpFTE->dwUserData)) ||
            (PROHIBIT_EAP == LOWORD(lpFTE->dwUserData)))
            dwExpectedValue = EXPECTED_PASS;
        else
            dwExpectedValue = (DWORD)EXPECTED_FAIL;
    }
    else if(HAS_X_AUTHTYPE(USE_CHAP))
    {
        dwThisAuthOption |= AUTH_MASK_CHAP_MD5;
        if (PROHIBIT_CHAP == LOWORD(lpFTE->dwUserData) ||
            REQUIRE_MS_ENC_PW == LOWORD(lpFTE->dwUserData) ||
            REQUIRE_DATA_ENC == LOWORD(lpFTE->dwUserData) ||
            REQUIRE_ENC_MS_PW == LOWORD(lpFTE->dwUserData) ||
            REQUIRE_MS_DATA_ENC == LOWORD(lpFTE->dwUserData) ||
            REQUIRE_MS_DATA_ENC_PW == LOWORD(lpFTE->dwUserData))
        {
            dwExpectedValue = (DWORD)EXPECTED_FAIL;
        }
    }
    else if(HAS_X_AUTHTYPE(USE_MSCHAP))
    {
        dwThisAuthOption |= AUTH_MASK_CHAP_MS;
        if (PROHIBIT_MSCHAP == LOWORD(lpFTE->dwUserData))
        {
            dwExpectedValue = (DWORD)EXPECTED_FAIL;
        }
    }
    else if(HAS_X_AUTHTYPE(USE_MSCHAPv2))
    {
        dwThisAuthOption |= AUTH_MASK_CHAP_MSV2;
        if (PROHIBIT_MSCHAPV2 == LOWORD(lpFTE->dwUserData))
        {
            dwExpectedValue = (DWORD)EXPECTED_FAIL;
        }
    }

    //
    // DCC connections will always pass. This is how the RAS Servers
    // in the automation lab are set-up - no authentication.
    //
    if (USE_DCC & lpFTE->dwUserData)
    {
        dwThisAuthOption = AUTH_MASK_MIX;
        dwExpectedValue = EXPECTED_PASS;
    }

    //
    // Connect to the Server with the parameters specified
    //
    RASPrint(TEXT("RAS Flags: %s"), Auths[LOWORD(lpFTE->dwUserData)].strAuthName);

    if (CreatePhoneBookEntry(dwThisAuthOption, dwEAPExtension)==FALSE)
    {
        iNumFailures++;
        RASPrint(TEXT("CreatePhoneBookEntry() FAIL'ed"));
    }

    SLEEP();

    if (ConnectRasConnection()==FALSE)
    {
        RASPrint(TEXT("ConnectRasConnection() FAIL'ed"));
        iNumFailures++;
    }
    else
        RASPrint(TEXT("+++ Connected Successfully +++"));

    SLEEP();

    if (DisconnectRasConnection()==FALSE)
    {
        RASPrint(TEXT("DisConnectRasConnection() FAIL'ed"));
        iNumFailures++;
    }

    //
    // Was this test case supposed to pass?
    //
    if ((dwExpectedValue == EXPECTED_PASS) && (iNumFailures == 0))
        return TPR_PASS;
    else if ((dwExpectedValue == EXPECTED_FAIL) && (iNumFailures > 0))
        return TPR_PASS;
    else
        return TPR_FAIL;
}

// END OF FILE
