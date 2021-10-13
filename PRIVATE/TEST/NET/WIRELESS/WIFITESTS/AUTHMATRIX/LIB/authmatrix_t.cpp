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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the AuthMatrix_t class.
//
// ----------------------------------------------------------------------------

#include "AuthMatrix_t.hpp"

#include <assert.h>
#include <netcmn.h>

#include <APPool_t.hpp>

#include "Factory_t.hpp"
#include "IPUtils.hpp"
#include "WiFiAccount_t.hpp"

using namespace ce::qa;

// Current test being processed:
static AuthMatrix_t       *s_pCurrentTest = NULL;
static ce::critical_section s_CurrentTestLocker;

// ----------------------------------------------------------------------------
//
// Overall test configuration:
//

// Authentication modes to be skipped:
//
static bool s_AuthModesToSkip   [NumberAPAuthModes];
static bool s_CipherModesToSkip [NumberAPCipherModes];
static bool s_EapAuthModesToSkip[NumberAPEapAuthModes];

// Access Point names:
//
static const int MaximumSelectedAPNames = 100;
static TCHAR     *s_pSelectedAPNames[MaximumSelectedAPNames];
static int   s_NumberSelectedAPNames = 0;
static ce::tstring s_SelectedAPNamesBuffer;

// Number test passes:
//
static long s_NumberTestPasses        =   1; // def =   1
static long s_NumberTestPassesMinimum =   1; // min =   1
static long s_NumberTestPassesMaximum = 100; // max = 100

// Time to wait after configuring WZC for a connection to be established:
//
static long s_ConnectionTimeLimit        =   140*1000; // def = 140 seconds
static long s_ConnectionTimeLimitMinimum =    10*1000; // min =  10 seconds
static long s_ConnectionTimeLimitMaximum = 15*60*1000; // max =  15 minutes

// EAP authentication credentials:
//
static bool s_EnrollmentsCompleted = false;

static bool          s_PEAPCredentialsEnrolled = false;
static WiFiAccount_t s_PEAPCredentialsProto(
                                            TEXT("eappeap"),
                                            TEXT("eappeap"),
                                            TEXT("wince"));

static bool          s_TLSCredentialsEnrolled = false;
static WiFiAccount_t s_TLSCredentialsProto(
                                            TEXT("eaptls"),
                                            TEXT("eaptls"),
                                            TEXT("wince"));

static bool          s_MD5CredentialsEnrolled = false;
static WiFiAccount_t s_MD5CredentialsProto(
                                            TEXT("eapmd5"),
                                            TEXT("eapmd5"),
                                            TEXT("wince"));

// ----------------------------------------------------------------------------
//
// Command-line options:
//
struct AMOption_t
{
    enum OptionType_e
    {
       DisableAuthOpen,
       DisableAuthShared,
       DisableAuth802,
       DisableAuthEAP,
       DisableAuthPSK,
       DisableAuthWPA2,
       DisableCipherClear,
       DisableCipherWEP,
       DisableCipherTKIP,
       DisableCipherAES,
       DisableEapAuthPEAP,
       DisableEapAuthTLS,
       DisableEapAuthMD5,
       SpecifyTestPasses,
       SpecifyConnectTime,
       SpecifyAPNames,
       SpecifyPEAPCredentials,
       SpecifyTLSCredentials,
       SpecifyMD5Credentials,
    };
    const TCHAR *arg;
    OptionType_e type;
    const TCHAR *desc;
};
static AMOption_t s_OptionsArray[] = {
    { TEXT("dOpen"),     AMOption_t::DisableAuthOpen,
      TEXT("disable (skip) Open System (no authentication) tests") },

    { TEXT("dShared"),   AMOption_t::DisableAuthShared,
      TEXT("disable (skip) Shared (WEP authentication) tests") },

    { TEXT("d802"),      AMOption_t::DisableAuth802,
      TEXT("disable (skip) 802.1X (dynamic WEP) tests") },

    { TEXT("dEAP"),      AMOption_t::DisableAuthEAP,
      TEXT("disable (skip) EAP (Radius) tests (WEP 802.1X, WPA and WPA2)") },

    { TEXT("dPSK"),      AMOption_t::DisableAuthPSK,
      TEXT("disable (skip) PSK tests (WPA-PSK and WPA2-PSK)") },
#ifdef WZC_IMPLEMENTS_WPA2
    { TEXT("dWPA2"),     AMOption_t::DisableAuthWPA2,
      TEXT("disable (skip) WPA2 tests (AES, WPA2 and WPA2-PSK)") },
#endif
    { TEXT("dClear"),    AMOption_t::DisableCipherClear,
      TEXT("disable (skip) ClearText (no encryption) tests") },

    { TEXT("dWEP"),      AMOption_t::DisableCipherWEP,
      TEXT("disable (skip) WEP encryption tests") },

    { TEXT("dTKIP"),     AMOption_t::DisableCipherTKIP,
      TEXT("disable (skip) TKIP encryption tests") },
#ifdef WZC_IMPLEMENTS_WPA2
    { TEXT("dAES"),      AMOption_t::DisableCipherAES,
      TEXT("disable (skip) AES encryption tests") },
#endif
    { TEXT("dPEAP"),     AMOption_t::DisableEapAuthPEAP,
      TEXT("disable (skip) PEAP EAP-authentication tests") },

    { TEXT("dTLS"),      AMOption_t::DisableEapAuthTLS,
      TEXT("disable (skip) TLS EAP-authentication tests") },
#ifdef EAP_MD5_SUPPORTED
    { TEXT("dMD5"),      AMOption_t::DisableEapAuthMD5,
      TEXT("disable (skip) MD5 EAP-authentication tests") },
#endif
    { TEXT("lTLS"),      AMOption_t::SpecifyTLSCredentials,
      TEXT("TLS login credentials (default \"eaptls:eaptls:wince\")") },

    { TEXT("lPEAP"),     AMOption_t::SpecifyPEAPCredentials,
      TEXT("PEAP login credentials (default \"eappeap:eappeap:wince\")") },
#ifdef EAP_MD5_SUPPORTED
    { TEXT("lMD5"),      AMOption_t::SpecifyMD5Credentials,
      TEXT("MD5 login credentials (default \"eapmd5:eapmd5:wince\")") },
#endif
    { TEXT("tPasses"),   AMOption_t::SpecifyTestPasses,
      TEXT("number times to repeat each connection (default 1)") },

    { TEXT("tConnTime"), AMOption_t::SpecifyConnectTime,
      TEXT("seconds to await a connection (default 140 seconds)") },

    { TEXT("tAPNames"),  AMOption_t::SpecifyAPNames,
      TEXT("choose from comma-separated list of AP names (default any)") },
};
static int s_NumberOptions = COUNTOF(s_OptionsArray);

// ----------------------------------------------------------------------------
//
// Tests:
//
static const AuthMatrix_t s_WEPTests[] =
{
    AuthMatrix_t(AuthMatrix_t::TestIdStart,
        TEXT("Auth=Open Cipher=ClearText"),
        WiFiConfig_t(APAuthOpen,
                     APCipherClearText)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 100,
        TEXT("Auth=Open Cipher=WEP 40-bit key (random)"),
        WiFiConfig_t(APAuthOpen,
                     APCipherWEP,
                     APEapAuthTLS,
                     0,
                     TEXT("01.23.45.67.89"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 110,
        TEXT("Auth=Open Cipher=WEP 40-bit key (semi-null)"),
        WiFiConfig_t(APAuthOpen,
                     APCipherWEP,
                     APEapAuthTLS,
                     1,
                     TEXT("01.02.03.04.05"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 120,
        TEXT("Auth=Open Cipher=WEP 40-bit key (semi-ones)"),
        WiFiConfig_t(APAuthOpen,
                     APCipherWEP,
                     APEapAuthTLS,
                     2,
                     TEXT("FF.FE.FD.FC.FB"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 130,
        TEXT("Auth=Open Cipher=WEP 104-bit key (random)"),
        WiFiConfig_t(APAuthOpen,
                     APCipherWEP,
                     APEapAuthTLS,
                     3,
                     TEXT("23.45.67.89.ab.cd.ef.01.23.45.67.89.01"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 140,
        TEXT("Auth=Open Cipher=WEP 104-bit key (semi-null)"),
        WiFiConfig_t(APAuthOpen,
                     APCipherWEP,
                     APEapAuthTLS,
                     2,
                     TEXT("02.03.04.05.06.07.08.09.00.01.02.03.04"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 150,
        TEXT("Auth=Open Cipher=WEP 104-bit key (semi-ones)"),
        WiFiConfig_t(APAuthOpen,
                     APCipherWEP,
                     APEapAuthTLS,
                     1,
                     TEXT("FE.FD.FC.FB.FA.FB.FC.FD.FE.FF.FE.FD.FC"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 200,
        TEXT("(Negative test) AP: Auth=Open Cipher=WEP")
                      TEXT(" STA: Auth=Open Cipher=TKIP"),
        WiFiConfig_t(APAuthOpen,
                     APCipherWEP,
                     APEapAuthTLS,
                     0,
                     TEXT("FE.FD.FC.FB.FA.FB.FC.FD.FE.FF.FE.FD.FC")),
        WiFiConfig_t(APAuthOpen,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("FE.FD.FC.FB.FA.FB.FC.FD.FE.FF.FE.FD.FC")),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 300,
        TEXT("(Negative test) AP: Auth=Open Cipher=WEP")
                      TEXT(" STA: Auth=Open Cipher=AES"),
        WiFiConfig_t(APAuthOpen,
                     APCipherWEP,
                     APEapAuthTLS,
                     0,
                     TEXT("02.03.04.05.06.07.08.09.00.01.02.03.04")),
        WiFiConfig_t(APAuthOpen,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("02.03.04.05.06.07.08.09.00.01.02.03.04")),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 1000,
        TEXT("(Negative test) AP: Auth=Shared Cipher=WEP")
                      TEXT(" STA: Auth=Shared Cipher=ClearText"),
        WiFiConfig_t(APAuthShared,
                     APCipherWEP,
                     APEapAuthTLS,
                     0,
                     TEXT("02.03.04.05.06.07.08.09.00.01.02.03.04")),
        WiFiConfig_t(APAuthShared,
                     APCipherClearText),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 1100,
        TEXT("Auth=Shared Cipher=WEP 40-bit key (random)"),
        WiFiConfig_t(APAuthShared,
                     APCipherWEP,
                     APEapAuthTLS,
                     0,
                     TEXT("01.23.45.67.89"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 1110,
        TEXT("Auth=Shared Cipher=WEP 40-bit key (semi-null)"),
        WiFiConfig_t(APAuthShared,
                     APCipherWEP,
                     APEapAuthTLS,
                     1,
                     TEXT("01.02.03.04.05"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 1120,
        TEXT("Auth=Shared Cipher=WEP 40-bit key (semi-ones)"),
        WiFiConfig_t(APAuthShared,
                     APCipherWEP,
                     APEapAuthTLS,
                     2,
                     TEXT("FF.FE.FD.FC.FB"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 1130,
        TEXT("Auth=Shared Cipher=WEP 104-bit key (random)"),
        WiFiConfig_t(APAuthShared,
                     APCipherWEP,
                     APEapAuthTLS,
                     3,
                     TEXT("23.45.67.89.ab.cd.ef.01.23.45.67.89.01"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 1140,
        TEXT("Auth=Shared Cipher=WEP 104-bit key (semi-null)"),
        WiFiConfig_t(APAuthShared,
                     APCipherWEP,
                     APEapAuthTLS,
                     2,
                     TEXT("02.03.04.05.06.07.08.09.00.01.02.03.04"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 1150,
        TEXT("Auth=Shared Cipher=WEP 104-bit key (semi-ones)"),
        WiFiConfig_t(APAuthShared,
                     APCipherWEP,
                     APEapAuthTLS,
                     1,
                     TEXT("FE.FD.FC.FB.FA.FB.FC.FD.FE.FF.FE.FD.FC"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 1200,
        TEXT("(Negative test) AP: Auth=Shared Cipher=WEP")
                      TEXT(" STA: Auth=Shared Cipher=TKIP"),
        WiFiConfig_t(APAuthShared,
                     APCipherWEP,
                     APEapAuthTLS,
                     0,
                     TEXT("FE.FD.FC.FB.FA.FB.FC.FD.FE.FF.FE.FD.FC")),
        WiFiConfig_t(APAuthShared,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("FE.FD.FC.FB.FA.FB.FC.FD.FE.FF.FE.FD.FC")),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 1300,
        TEXT("(Negative test) AP: Auth=Shared Cipher=WEP")
                      TEXT(" STA: Auth=Shared Cipher=AES"),
        WiFiConfig_t(APAuthShared,
                     APCipherWEP,
                     APEapAuthTLS,
                     0,
                     TEXT("02.03.04.05.06.07.08.09.00.01.02.03.04")),
        WiFiConfig_t(APAuthShared,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("02.03.04.05.06.07.08.09.00.01.02.03.04")),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 2000,
        TEXT("(Negative test) AP: Auth=WEP_802_1X Cipher=WEP")
                      TEXT(" STA: Auth=WEP_802_1X Cipher=ClearText"),
        WiFiConfig_t(APAuthWEP_802_1X,
                     APCipherWEP,
                     APEapAuthTLS),
        WiFiConfig_t(APAuthWEP_802_1X,
                     APCipherClearText),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 2100,
        TEXT("Auth=WEP_802_1X Cipher=WEP EAP=TLS"),
        WiFiConfig_t(APAuthWEP_802_1X,
                     APCipherWEP,
                     APEapAuthTLS)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 2110,
        TEXT("Auth=WEP_802_1X Cipher=WEP EAP=MD5"),
        WiFiConfig_t(APAuthWEP_802_1X,
                     APCipherWEP,
                     APEapAuthMD5)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 2120,
        TEXT("Auth=WEP_802_1X Cipher=WEP EAP=PEAP"),
        WiFiConfig_t(APAuthWEP_802_1X,
                     APCipherWEP,
                     APEapAuthPEAP)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 2200,
        TEXT("(Negative test) AP: Auth=WEP_802_1X Cipher=WEP")
                      TEXT(" STA: Auth=WEP_802_1X Cipher=TKIP"),
        WiFiConfig_t(APAuthWEP_802_1X,
                     APCipherWEP,
                     APEapAuthTLS),
        WiFiConfig_t(APAuthWEP_802_1X,
                     APCipherTKIP,
                     APEapAuthTLS),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 2300,
        TEXT("(Negative test) AP: Auth=WEP_802_1X Cipher=WEP")
                      TEXT(" STA: Auth=WEP_802_1X Cipher=AES"),
        WiFiConfig_t(APAuthWEP_802_1X,
                     APCipherWEP,
                     APEapAuthTLS),
        WiFiConfig_t(APAuthWEP_802_1X,
                     APCipherAES,
                     APEapAuthTLS),
        true,   // bad security modes
        false   // key value ok
    ),
};

static const AuthMatrix_t s_WPATests[] =
{
    AuthMatrix_t(AuthMatrix_t::TestIdStart + 3000,
        TEXT("(Negative test) AP: Auth=WPA Cipher=TKIP")
                      TEXT(" STA: Auth=WPA Cipher=ClearText"),
        WiFiConfig_t(APAuthWPA,
                     APCipherTKIP,
                     APEapAuthTLS),
        WiFiConfig_t(APAuthWPA,
                     APCipherClearText),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 3100,
        TEXT("(Negative test) AP: Auth=WPA Cipher=TKIP")
                      TEXT(" STA: Auth=WPA Cipher=WEP"), 
        WiFiConfig_t(APAuthWPA,
                     APCipherTKIP,
                     APEapAuthTLS),
        WiFiConfig_t(APAuthWPA,
                     APCipherWEP,
                     APEapAuthTLS,
                     0,
                     TEXT("02.03.04.05.06.07.08.09.00.01.02.03.04")),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 3200,
        TEXT("Auth=WPA Cipher=TKIP EAP=TLS"),
        WiFiConfig_t(APAuthWPA,
                     APCipherTKIP,
                     APEapAuthTLS)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 3210,
        TEXT("Auth=WPA Cipher=TKIP EAP=MD5"),
        WiFiConfig_t(APAuthWPA,
                     APCipherTKIP,
                     APEapAuthMD5)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 3220,
        TEXT("Auth=WPA Cipher=TKIP EAP=PEAP"),
        WiFiConfig_t(APAuthWPA,
                     APCipherTKIP,
                     APEapAuthPEAP)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 3300,
        TEXT("Auth=WPA Cipher=AES EAP=TLS"),
        WiFiConfig_t(APAuthWPA,
                     APCipherAES,
                     APEapAuthTLS)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 3310,
        TEXT("Auth=WPA Cipher=AES EAP=MD5"),
        WiFiConfig_t(APAuthWPA,
                     APCipherAES,
                     APEapAuthMD5)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 3320,
        TEXT("Auth=WPA Cipher=AES EAP=PEAP"),
        WiFiConfig_t(APAuthWPA,
                     APCipherAES,
                     APEapAuthPEAP)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4000,
        TEXT("(Negative test) AP: Auth=WPA_PSK Cipher=TKIP")
                      TEXT(" STA: Auth=WPA_PSK Cipher=ClearText"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("0123456789")),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherClearText),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4100,
        TEXT("(Negative test) AP: Auth=WPA_PSK Cipher=TKIP")
                      TEXT(" STA: Auth=WPA_PSK Cipher=WEP"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("FF.FE.FD.FC.FB")),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherWEP,
                     APEapAuthTLS,
                     0,
                     TEXT("FF.FE.FD.FC.FB")),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4200,
        TEXT("Auth=WPA_PSK Cipher=TKIP 63-digit passphrase (random)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("This is a maximum-sized key.")
                     TEXT(" Any longer and 802 would complain."))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4210,
        TEXT("Auth=WPA_PSK Cipher=TKIP 8-digit passphrase (random)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("MyPhrase"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4220,
        TEXT("(Negative test) Auth=WPA_PSK Cipher=TKIP 7-digit passphrase"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("7Lucky77")),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("7Lucky7")),
        false,  // security modes ok
        true    // bad key value
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4230,
        TEXT("Auth=WPA_PSK Cipher=TKIP 63-digit passphrase (semi-ones)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("~~~z~~~~z~~~~z~~~~z~~~~z~~~~z~~~")
                     TEXT("~z~~~~z~~~~z~~~~z~~~~z~~~~z~~~~"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4240,
        TEXT("Auth=WPA_PSK Cipher=TKIP 8-digit passphrase (semi-ones)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("~~z~~z~~"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4250,
        TEXT("(Negative test) Auth=WPA_PSK Cipher=TKIP 64-digit passphrase"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("This key is too long.")
                     TEXT(" The maximum legal passphrase length is 63")),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("This key is too long.")
                     TEXT(" The maximum legal passphrase length is 63.")),
        false,  // security modes ok
        true    // bad key value
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4260,
        TEXT("Auth=WPA_PSK Cipher=TKIP 63-digit passphrase (semi-null)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("! !!!!!!! !!! !!!!!!! !!! !!!!!!")
                     TEXT("! !!! !!! !!!!!!! !!! !!!!!!! !"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4270,
        TEXT("Auth=WPA_PSK Cipher=TKIP 8-digit passphrase (semi-null)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("! !!!! !"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4300,
        TEXT("Auth=WPA_PSK Cipher=AES 63-digit passphrase (random)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("This is a maximum-sized key.")
                     TEXT(" Any longer and 802 would complain."))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4310,
        TEXT("Auth=WPA_PSK Cipher=AES 8-digit passphrase (random)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("MyPhrase"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4320,
        TEXT("(Negative test) Auth=WPA_PSK Cipher=AES 7-digit passphrase"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("7Lucky77")),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("7Lucky7")),
        false,  // security modes ok
        true    // bad key value
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4330,
        TEXT("Auth=WPA_PSK Cipher=AES 63-digit passphrase (semi-ones)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("~~~z~~~~z~~~~z~~~~z~~~~z~~~~z~~~")
                     TEXT("~z~~~~z~~~~z~~~~z~~~~z~~~~z~~~~"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4340,
        TEXT("Auth=WPA_PSK Cipher=AES 8-digit passphrase (semi-ones)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("~~z~~z~~"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4350,
        TEXT("(Negative test) Auth=WPA_PSK Cipher=AES 64-digit passphrase"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("This key is too long.")
                     TEXT(" The maximum legal passphrase length is 63")),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("This key is too long.")
                     TEXT(" The maximum legal passphrase length is 63.")),
        false,  // security modes ok
        true    // bad key value
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4360,
        TEXT("Auth=WPA_PSK Cipher=AES 63-digit passphrase (semi-null)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("! !!!!!!! !!! !!!!!!! !!! !!!!!!")
                     TEXT("! !!! !!! !!!!!!! !!! !!!!!!! !"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 4370,
        TEXT("Auth=WPA_PSK Cipher=AES 8-digit passphrase (semi-null)"),
        WiFiConfig_t(APAuthWPA_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("! !!!! !"))
    ),
};

static const AuthMatrix_t s_WPA2Tests[] =
{
    AuthMatrix_t(AuthMatrix_t::TestIdStart + 5000,
        TEXT("(Negative test) AP: Auth=WPA2 Cipher=TKIP")
                      TEXT(" STA: Auth=WPA2 Cipher=ClearText"),
        WiFiConfig_t(APAuthWPA2,
                     APCipherTKIP,
                     APEapAuthTLS),
        WiFiConfig_t(APAuthWPA2,
                     APCipherClearText),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 5100,
        TEXT("(Negative test) AP: Auth=WPA2 Cipher=TKIP")
                      TEXT(" STA: Auth=WPA2 Cipher=WEP"),
        WiFiConfig_t(APAuthWPA2,
                     APCipherTKIP,
                     APEapAuthTLS),
        WiFiConfig_t(APAuthWPA2,
                     APCipherWEP,
                     APEapAuthTLS,
                     0,
                     TEXT("01.23.45.67.89")),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 5200,
        TEXT("Auth=WPA2 Cipher=TKIP EAP=TLS"),
        WiFiConfig_t(APAuthWPA2,
                     APCipherTKIP,
                     APEapAuthTLS)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 5210,
        TEXT("Auth=WPA2 Cipher=TKIP EAP=MD5"),
        WiFiConfig_t(APAuthWPA2,
                     APCipherTKIP,
                     APEapAuthMD5)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 5220,
        TEXT("Auth=WPA2 Cipher=TKIP EAP=PEAP"),
        WiFiConfig_t(APAuthWPA2,
                     APCipherTKIP,
                     APEapAuthPEAP)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 5300,
        TEXT("Auth=WPA2 Cipher=AES EAP=TLS"),
        WiFiConfig_t(APAuthWPA2,
                     APCipherAES,
                     APEapAuthTLS)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 5310,
        TEXT("Auth=WPA2 Cipher=AES EAP=MD5"),
        WiFiConfig_t(APAuthWPA2,
                     APCipherAES,
                     APEapAuthMD5)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 5320,
        TEXT("Auth=WPA2 Cipher=AES EAP=PEAP"),
        WiFiConfig_t(APAuthWPA2,
                     APCipherAES,
                     APEapAuthPEAP)
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6000,
        TEXT("(Negative test) AP: Auth=WPA2_PSK Cipher=TKIP")
                      TEXT(" STA: Auth=WPA2_PSK Cipher=ClearText"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("MyPhrase")),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherClearText),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6100,
        TEXT("(Negative test) AP: Auth=WPA2_PSK Cipher=TKIP")
                      TEXT(" STA: Auth=WPA2_PSK Cipher=WEP"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("01.23.45.67.89")),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherWEP,
                     APEapAuthTLS,
                     0,
                     TEXT("01.23.45.67.89")),
        true,   // bad security modes
        false   // key value ok
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6200,
        TEXT("Auth=WPA2_PSK Cipher=TKIP 63-digit passphrase (random)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("This is a maximum-sized key.")
                     TEXT(" Any longer and 802 would complain."))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6210,
        TEXT("Auth=WPA2_PSK Cipher=TKIP 8-digit passphrase (random)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("MyPhrase"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6220,
        TEXT("(Negative test) Auth=WPA2_PSK Cipher=TKIP 7-digit passphrase"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("7Lucky77")),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("7Lucky7")),
        false,  // security modes ok
        true    // bad key value
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6230,
        TEXT("Auth=WPA2_PSK Cipher=TKIP 63-digit passphrase (semi-ones)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("~~~z~~~~z~~~~z~~~~z~~~~z~~~~z~~~")
                     TEXT("~z~~~~z~~~~z~~~~z~~~~z~~~~z~~~~"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6240,
        TEXT("Auth=WPA2_PSK Cipher=TKIP 8-digit passphrase (semi-ones)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("~~z~~z~~"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6250,
        TEXT("(Negative test) Auth=WPA2_PSK Cipher=TKIP 64-digit passphrase"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("This key is too long.")
                     TEXT(" The maximum legal passphrase length is 63")),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("This key is too long.")
                     TEXT(" The maximum legal passphrase length is 63.")),
        false,  // security modes ok
        true    // bad key value
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6260,
        TEXT("Auth=WPA2_PSK Cipher=TKIP 63-digit passphrase (semi-null)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("! !!!!!!! !!! !!!!!!! !!! !!!!!!")
                     TEXT("! !!! !!! !!!!!!! !!! !!!!!!! !"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6270,
        TEXT("Auth=WPA2_PSK Cipher=TKIP 8-digit passphrase (semi-null)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherTKIP,
                     APEapAuthTLS,
                     0,
                     TEXT("! !!!! !"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6300,
        TEXT("Auth=WPA2_PSK Cipher=AES 63-digit passphrase (random)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("This is a maximum-sized key.")
                     TEXT(" Any longer and 802 would complain."))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6310,
        TEXT("Auth=WPA2_PSK Cipher=AES 8-digit passphrase (random)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("MyPhrase"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6320,
        TEXT("(Negative test) Auth=WPA2_PSK Cipher=AES 7-digit passphrase"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("7Lucky77")),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("7Lucky7")),
        false,  // security modes ok
        true    // bad key value
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6330,
        TEXT("Auth=WPA2_PSK Cipher=AES 63-digit passphrase (semi-ones)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("~~~z~~~~z~~~~z~~~~z~~~~z~~~~z~~~")
                     TEXT("~z~~~~z~~~~z~~~~z~~~~z~~~~z~~~~"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6340,
        TEXT("Auth=WPA2_PSK Cipher=AES 8-digit passphrase (semi-ones)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("~~z~~z~~"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6350,
        TEXT("(Negative test) Auth=WPA2_PSK Cipher=AES 64-digit passphrase"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("This key is too long.")
                     TEXT(" The maximum legal passphrase length is 63")),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("This key is too long.")
                     TEXT(" The maximum legal passphrase length is 63.")),
        false,  // security modes ok
        true    // bad key value
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6360,
        TEXT("Auth=WPA2_PSK Cipher=AES 63-digit passphrase (semi-null)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("! !!!!!!! !!! !!!!!!! !!! !!!!!!")
                     TEXT("! !!! !!! !!!!!!! !!! !!!!!!! !"))
    ),

    AuthMatrix_t(AuthMatrix_t::TestIdStart + 6370,
        TEXT("Auth=WPA2_PSK Cipher=AES 8-digit passphrase (semi-null)"),
        WiFiConfig_t(APAuthWPA2_PSK,
                     APCipherAES,
                     APEapAuthTLS,
                     0,
                     TEXT("! !!!! !"))
    ),
};

struct AMTests_t
{
    const AuthMatrix_t *pList;
    const int           count;
};

static const AMTests_t s_AMTests[] =
{
    { s_WEPTests,  COUNTOF(s_WEPTests)  },
    { s_WPATests,  COUNTOF(s_WPATests)  },
    { s_WPA2Tests, COUNTOF(s_WPA2Tests) },
};

static int s_NumberAMTests = COUNTOF(s_AMTests);

// ----------------------------------------------------------------------------
//
// Initializes or cleans up static resources.
//
void
AuthMatrix_t::
StartupInitialize(void)
{
    for (int aux = 0 ; aux < NumberAPAuthModes ; ++aux)
        s_AuthModesToSkip[aux] = false;
    for (int cix = 0 ; cix < NumberAPCipherModes ; ++cix)
        s_CipherModesToSkip[cix] = false;
    for (int eax = 0 ; eax < NumberAPEapAuthModes ; ++eax)
        s_EapAuthModesToSkip[eax] = false;

#ifndef EAP_MD5_SUPPORTED
    s_EapAuthModesToSkip[APEapAuthMD5] = true;
#endif
}

void
AuthMatrix_t::
ShutdownCleanup(void)
{
    // nothing to do.
}

// ----------------------------------------------------------------------------
//
// Displays the command-argument options.
//
void
AuthMatrix_t::
PrintUsage(void)
{
    LogAlways(TEXT("\nauthentication-matrix test options:"));
    TCHAR initialChar = 0;
    for (int opx = 0 ; opx < s_NumberOptions ; ++opx)
    {
        const AMOption_t *opp = &s_OptionsArray[opx];
        const TCHAR *pPattern;
        if (initialChar && opp->arg[0] != initialChar)
             pPattern = TEXT("\n  -%-9s %s");
        else pPattern = TEXT(  "  -%-9s %s");
        initialChar = opp->arg[0];
        LogAlways(pPattern, opp->arg, opp->desc);
    }
}

// ----------------------------------------------------------------------------
//
// Parses the DLL's command-line arguments.
//
DWORD
AuthMatrix_t::
ParseCommandLine(int argc, TCHAR *argv[])
{
    DWORD result;

    for (int opx = 0 ; opx < s_NumberOptions ; ++opx)
    {
        const AMOption_t *opp = &s_OptionsArray[opx];
        int wasOpt = WasOption(argc, argv, opp->arg);
        if (wasOpt < 0)
        {
            if (wasOpt < -1)
            {
                LogError(TEXT("Error parsing command line for option \"-%s\""),
                         opp->arg);
                return ERROR_INVALID_PARAMETER;
            }
            continue;
        }

        TCHAR *optionArg = NULL;
        int wasArg = GetOption(argc, argv, opp->arg, &optionArg);

        switch (opp->type)
        {
            case AMOption_t::DisableAuthOpen:
                s_AuthModesToSkip[APAuthOpen] = true;
                break;

            case AMOption_t::DisableAuthShared:
                s_AuthModesToSkip[APAuthShared] = true;
                break;

            case AMOption_t::DisableAuth802:
                s_AuthModesToSkip[APAuthWEP_802_1X] = true;
                break;

            case AMOption_t::DisableAuthEAP:
                s_AuthModesToSkip[APAuthWEP_802_1X] = true;
                s_AuthModesToSkip[APAuthWPA] = true;
                s_AuthModesToSkip[APAuthWPA2] = true;
                break;

            case AMOption_t::DisableAuthPSK:
                s_AuthModesToSkip[APAuthWPA_PSK] = true;
                s_AuthModesToSkip[APAuthWPA2_PSK] = true;
                break;

            case AMOption_t::DisableAuthWPA2:
                s_AuthModesToSkip[APAuthWPA2] = true;
                s_AuthModesToSkip[APAuthWPA2_PSK] = true;
                s_CipherModesToSkip[APCipherAES] = true;
                break;

            case AMOption_t::DisableCipherClear:
                s_CipherModesToSkip[APCipherClearText] = true;
                break;

            case AMOption_t::DisableCipherWEP:
                s_CipherModesToSkip[APCipherWEP] = true;
                break;

            case AMOption_t::DisableCipherTKIP:
                s_CipherModesToSkip[APCipherTKIP] = true;
                break;

            case AMOption_t::DisableCipherAES:
                s_CipherModesToSkip[APCipherAES] = true;
                break;

            case AMOption_t::DisableEapAuthPEAP:
                s_EapAuthModesToSkip[APEapAuthPEAP] = true;
                break;

            case AMOption_t::DisableEapAuthTLS:
                s_EapAuthModesToSkip[APEapAuthTLS] = true;
                break;
#ifdef EAP_MD5_SUPPORTED
            case AMOption_t::DisableEapAuthMD5:
                s_EapAuthModesToSkip[APEapAuthMD5] = true;
                break;
#endif
            case AMOption_t::SpecifyTestPasses:
            {
                unsigned long optValue; TCHAR *optEnd;

                if (wasArg < 0 || !optionArg || !optionArg[0])
                {
                    LogError(TEXT("Test passes missing after option \"-%s\""),
                             opp->arg);
                    return ERROR_INVALID_PARAMETER;
                }

                optValue = _tcstoul(optionArg, &optEnd, 10);

                s_NumberTestPasses = (long)optValue;
                if (optEnd == NULL || optEnd[0] != TEXT('\0')
                 || s_NumberTestPasses < s_NumberTestPassesMinimum
                 || s_NumberTestPasses > s_NumberTestPassesMaximum)
                {
                    LogError(TEXT("Test passes \"%s\" is invalid")
                             TEXT(" (min=%ld max=%ld)"),
                             optionArg, s_NumberTestPassesMinimum,
                                        s_NumberTestPassesMaximum);
                    return ERROR_INVALID_PARAMETER;
                }
                break;
            }
            
            case AMOption_t::SpecifyConnectTime:
            {
                unsigned long optValue; TCHAR *optEnd;

                if (wasArg < 0 || !optionArg || !optionArg[0])
                {
                    LogError(TEXT("Connect time missing after option \"-%s\""),
                             opp->arg);
                    return ERROR_INVALID_PARAMETER;
                }

                optValue = _tcstoul(optionArg, &optEnd, 10);

                s_ConnectionTimeLimit = (long)(optValue * 1000);
                if (optEnd == NULL || optEnd[0] != TEXT('\0')
                 || s_ConnectionTimeLimit < s_ConnectionTimeLimitMinimum
                 || s_ConnectionTimeLimit > s_ConnectionTimeLimitMaximum)
                {
                    LogError(TEXT("Connect time \"%s\" is invalid")
                             TEXT(" (min=%ld max=%ld seconds)"),
                             optionArg, s_ConnectionTimeLimitMinimum / 1000,
                                        s_ConnectionTimeLimitMaximum / 1000);
                    return ERROR_INVALID_PARAMETER;
                }
                break;
            }

            case AMOption_t::SpecifyAPNames:
                if (wasArg < 0 || !optionArg || !optionArg[0])
                {
                    LogError(TEXT("AP names missing after option \"-%s\""),
                             opp->arg);
                    return ERROR_INVALID_PARAMETER;
                }
                else
                if (!s_SelectedAPNamesBuffer.append(optionArg))
                {
                    LogError(TEXT("Not enough memory for AP-Names list"));
                    return ERROR_OUTOFMEMORY;
                }
                else
                {
                    TCHAR *token = s_SelectedAPNamesBuffer.get_buffer();
                    for (token = _tcstok(token, TEXT(",")) ;
                         token != NULL
                      && s_NumberSelectedAPNames < MaximumSelectedAPNames ;
                         token = _tcstok(NULL, TEXT(",")))
                    {
                        token = Utils::TrimToken(token);
                        if (TEXT('\0') != token[0])
                        {
                            s_pSelectedAPNames[s_NumberSelectedAPNames++] = token;
                        }
                    }
                }
                break;
            
            case AMOption_t::SpecifyPEAPCredentials:
            {
                if (wasArg < 0 || !optionArg || !optionArg[0])
                {
                    LogError(TEXT("PEAP credentials missing after \"-%s\""),
                             opp->arg);
                    return ERROR_INVALID_PARAMETER;
                }

                result = s_PEAPCredentialsProto.Assign(optionArg);
                if (ERROR_SUCCESS != result)
                {
                    LogError(TEXT("PEAP credentials \"%s\" are invalid"),
                             optionArg);
                    return result;
                }

                break;
            }
            
            case AMOption_t::SpecifyTLSCredentials:
            {
                if (wasArg < 0 || !optionArg || !optionArg[0])
                {
                    LogError(TEXT("TLS credentials missing after \"-%s\""),
                             opp->arg);
                    return ERROR_INVALID_PARAMETER;
                }

                result = s_TLSCredentialsProto.Assign(optionArg);
                if (ERROR_SUCCESS != result)
                {
                    LogError(TEXT("TLS credentials \"%s\" are invalid"),
                             optionArg);
                    return result;
                }

                break;
            }
            
#ifdef EAP_MD5_SUPPORTED
            case AMOption_t::SpecifyMD5Credentials:
            {
                if (wasArg < 0 || !optionArg || !optionArg[0])
                {
                    LogError(TEXT("MD5 credentials missing after \"-%s\""),
                             opp->arg);
                    return ERROR_INVALID_PARAMETER;
                }

                result = s_MD5CredentialsProto.Assign(optionArg);
                if (ERROR_SUCCESS != result)
                {
                    LogError(TEXT("MD5 credentials \"%s\" are invalid"),
                             optionArg);
                    return result;
                }

                break;
            }
#endif
        }
    }

    // If all the modes requiring EAP authentication are disabled, turn
    // off certificate enrollment.
    if (s_AuthModesToSkip[APAuthWEP_802_1X]
     && s_AuthModesToSkip[APAuthWPA]
     && s_AuthModesToSkip[APAuthWPA2])
    {
        s_EnrollmentsCompleted = true;
    }
    else
    {
        // Turn off the unnecessary enrollments.
        if (s_EapAuthModesToSkip[APEapAuthTLS])  s_TLSCredentialsEnrolled  = true;
        if (s_EapAuthModesToSkip[APEapAuthMD5])  s_MD5CredentialsEnrolled  = true;
        if (s_EapAuthModesToSkip[APEapAuthPEAP]) s_PEAPCredentialsEnrolled = true;
        
        // We don't need a certificate for PEAP authentication but, if
        // we're testing PEAP connections we'll need to get a TLS cert
        // to make sure we have the root (server) cert.
        else
        {
            s_PEAPCredentialsEnrolled = true;
            s_TLSCredentialsEnrolled = false;
        }
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Determines whether the user wants to skip these security modes.
//
static bool
SkipSecurityModes(
    const WiFiConfig_t &Config)
{
    if (s_AuthModesToSkip[Config.GetAuthMode()]
     || s_CipherModesToSkip[Config.GetCipherMode()])
        return true;
    
    switch (Config.GetAuthMode())
    {
        case APAuthWEP_802_1X:
        case APAuthWPA:
        case APAuthWPA2:
            if (s_EapAuthModesToSkip[Config.GetEapAuthMode()])
                return true;
            break;
    }
    
    return false;
}

// ----------------------------------------------------------------------------
//
// Adds all the tests to the specified factory's function-table.
//
DWORD
AuthMatrix_t::
AddFunctionTable(Factory_t *pFactory)
{
    for (int lx = 0 ; lx < s_NumberAMTests ; ++lx)
    {
        const AuthMatrix_t *pTest = s_AMTests[lx].pList;
        int                 count = s_AMTests[lx].count;
        for (int tx = 0 ; tx < count ; ++tx, ++pTest)
        {
#ifndef WZC_IMPLEMENTS_WPA2
            if (APAuthWPA2     == pTest->m_Config.GetAuthMode()
             || APAuthWPA2_PSK == pTest->m_Config.GetAuthMode()
             || APCipherAES    == pTest->m_Config.GetCipherMode())
                continue;
#endif

#ifndef EAP_MD5_SUPPORTED
            if (APEapAuthMD5 == pTest->m_Config.GetEapAuthMode())
                continue;
#endif

            DWORD result = pFactory->AddFunctionTableEntry(pTest->m_pTestName, 0, 0,
                                                           pTest->m_TestId);
            if (ERROR_SUCCESS != result)
                return result;
        }
    }
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Generates an object for processing the specified test
//
DWORD
AuthMatrix_t::
CreateTest(
    const FUNCTION_TABLE_ENTRY *pFTE,
    WiFiBase_t                **ppTest)
{
    assert(pFTE->dwUniqueID >= AuthMatrix_t::TestIdStart);
    assert(pFTE->dwUniqueID <= AuthMatrix_t::TestIdEnd);

    // Look up the specified test id.
    const AuthMatrix_t *pProto = NULL;
    for (int lx = s_NumberAMTests ; lx-- > 0 ;)
    {
        for (int tx = s_AMTests[lx].count ; tx-- > 0 ;)
        {
            pProto = &(s_AMTests[lx].pList[tx]);
            if (pProto->m_TestId == pFTE->dwUniqueID)
            {
                lx = -1;
                break;
            }
        }
    }

    // Allocate the AuthMatrix object.
    WiFiBase_t *pTest = new AuthMatrix_t(*pProto);
    if (pTest == NULL)
    {
        LogError(TEXT("Can't allocate %u byte AuthMatrix object"),
                 sizeof(AuthMatrix_t));
        return ERROR_OUTOFMEMORY;
    }

   *ppTest = pTest;
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets the current WiFi configuration under test.
// Returns false if there is no test running.
//
bool
AuthMatrix_t::
GetTestConfig(
    WiFiConfig_t *pAPConfig,
    WiFiConfig_t *pConfig,
    long         *pStartTime)
{
    ce::gate<ce::critical_section> locker(s_CurrentTestLocker);
    if (NULL != s_pCurrentTest)
    {
       *pAPConfig  = s_pCurrentTest->m_APConfig;
       *pConfig    = s_pCurrentTest->m_Config;
       *pStartTime = s_pCurrentTest->m_StartTime;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
//
// Constructors:
//
AuthMatrix_t::
AuthMatrix_t(
    DWORD               TestId,
    const TCHAR        *pTestName,
    const WiFiConfig_t &Config)
    : WiFiBase_t(),
      m_TestId(TestId),
      m_pTestName(pTestName),
      m_Config(Config),
      m_APConfig(Config),
      m_IsBadModes(false),
      m_IsBadKey(false),
      m_pEapCredentials(NULL),
      m_StartTime(0)
{
    assert(ValidateSecurityModes(Config.GetAuthMode(),
                                 Config.GetCipherMode()));
}

AuthMatrix_t::
AuthMatrix_t(
    DWORD               TestId,
    const TCHAR        *pTestName,
    const WiFiConfig_t &APConfig,
    const WiFiConfig_t &Config,
    bool                IsBadModes,
    bool                IsBadKey)
    : WiFiBase_t(),
      m_TestId(TestId),
      m_pTestName(pTestName),
      m_Config(Config),
      m_APConfig(APConfig),
      m_IsBadModes(IsBadModes),
      m_IsBadKey(IsBadKey),
      m_pEapCredentials(NULL),
      m_StartTime(0)
{
    assert(IsBadModes || ValidateSecurityModes(Config.GetAuthMode(),
                                               Config.GetCipherMode()));
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
AuthMatrix_t::
~AuthMatrix_t(void)
{
    // Deregister this as the current test.
    // (This shouldn't be necessary, but...)
    ce::gate<ce::critical_section> locker(s_CurrentTestLocker);
    s_pCurrentTest = NULL;
    locker.leave();
    
    if (NULL != m_pEapCredentials)
    {
        delete m_pEapCredentials;
        m_pEapCredentials = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Generates the appropriate type of EAP-autentication credentials object
// (if any) required to authenticate the specified connection.
//
static DWORD
InitEapCredentials(
    const WiFiConfig_t &Config,
    WiFiAccount_t     **ppCredentials)
{
    assert(NULL != ppCredentials);
   *ppCredentials = NULL;

    switch (Config.GetAuthMode())
    {
        case APAuthWEP_802_1X:
        case APAuthWPA:
        case APAuthWPA2:

            WiFiAccount_t *pProto = NULL;
            switch (Config.GetEapAuthMode())
            {
                default:            pProto = &s_TLSCredentialsProto;  break;
                case APEapAuthPEAP: pProto = &s_PEAPCredentialsProto; break;
                case APEapAuthMD5:  pProto = &s_MD5CredentialsProto;  break;
            }

           *ppCredentials = new WiFiAccount_t(pProto->GetUserName(),
                                              pProto->GetPassword(),
                                              pProto->GetDomain());
            if (NULL == *ppCredentials)
            {
                LogError(TEXT("Can't allocate %u bytes for EAP credentials"),
                         sizeof(WiFiAccount_t));
                return ERROR_OUTOFMEMORY;
            }
            break;
    }
    
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Initializes this concrete test class.
// Returns ERROR_CALL_NOT_IMPLEMENTED if the test is to be skipped.
//
DWORD
AuthMatrix_t::
Init(void)
{
    DWORD result;
    
    do
    {
        // Make sure we're not supposed to skip these test modes.
        if (SkipSecurityModes(m_Config))
        {
            result = ERROR_CALL_NOT_IMPLEMENTED;
            break;
        }

        // Initialize the WZC interface.
        result = InitWZCService();
        if (ERROR_SUCCESS != result)
            break;

        // Make sure the wireless adapter can support these security modes.
        WZCIntfEntry_t intf;
        result = GetWZCService()->QueryAdapterInfo(&intf);
        if (ERROR_SUCCESS != result)
            break;

        if (!intf.IsSecurityModeSupported(m_APConfig.GetWZCAuthMode(),
                                          m_APConfig.GetWZCCipherMode(),
                                          m_APConfig.GetWZCEapAuthMode()))
        {
            // Most of the time, the WiFi adapter can say it doesn't
            // support a security mode and we'll just skip it. There
            // are certain modes, however, that we require. This code
            // checks for those modes and changes the following message
            // from a warning to an error.
            void (*logFunc)(const TCHAR *, ...) = LogWarn;
            switch (m_APConfig.GetAuthMode())
            {
                case APAuthOpen:
                    logFunc = LogError;
                    break;
                case APAuthWPA:
                case APAuthWPA_PSK:
                    if (m_APConfig.GetCipherMode() == APCipherTKIP)
                        logFunc = LogError;
                    break;
            }
            logFunc(TEXT("Wireless adapter %s does not support")
                    TEXT(" auth %s cipher %s EAP %s"),
                    GetWZCService()->GetAdapterName(),
                    m_APConfig.GetAuthName(),
                    m_APConfig.GetCipherName(),
                    m_APConfig.GetEapAuthName());
            if (LogError != logFunc)
                result = ERROR_CALL_NOT_IMPLEMENTED;
            else
            {
                LogError(TEXT("These security modes MUST be supported!"));
                result = ERROR_NOT_SUPPORTED;
            }
            break;
        }

        // Initialize the AP-control server. If necessary this connects to
        // the server's fixed-configuration Access Point.
        result = InitAPPool();
        if (ERROR_SUCCESS != result)
            break;

        // If necessary, initialize the EAP-authentication credentials.
        result = InitEapCredentials(m_Config, &m_pEapCredentials);
        if (ERROR_SUCCESS != result)
            break;
        
        // Register this as the current test.
        ce::gate<ce::critical_section> locker(s_CurrentTestLocker);
        s_pCurrentTest = this;
        m_StartTime = GetTickCount();
        locker.leave();
    }
    while (false);

    return result;
}

// ----------------------------------------------------------------------------
//
// If this is the first successful wireless connection, get
// authentication certificates.
//
static DWORD
GetCertificates(void)
{
    DWORD result = ERROR_SUCCESS;

    for (int ex = 0 ; ERROR_SUCCESS == result ; ++ex)
    {
        if (ex >= NumberAPEapAuthModes)
        {
            s_EnrollmentsCompleted = true;
            break;
        }

        bool          *pEnrol = NULL;
        WiFiAccount_t *pProto = NULL;
        switch ((APEapAuthMode_e) ex)
        {
            default:
                pEnrol = &s_TLSCredentialsEnrolled;
                pProto = &s_TLSCredentialsProto;
                break;
            case APEapAuthPEAP:
                pEnrol = &s_PEAPCredentialsEnrolled;
                pProto = &s_PEAPCredentialsProto;
                break;
            case APEapAuthMD5:
                pEnrol = &s_MD5CredentialsEnrolled;
                pProto = &s_MD5CredentialsProto;
                break;
        }

        if (!(*pEnrol))
        {
            ce::auto_ptr<WiFiAccount_t> pCred;
            pCred = new WiFiAccount_t(pProto->GetUserName(),
                                      pProto->GetPassword(),
                                      pProto->GetDomain());
            if (pCred.valid())
            {
                result = pCred->Enroll();
                if (ERROR_SUCCESS == result)
                   *pEnrol = true;
            }
            else
            {
                LogError(TEXT("Can't get %u bytes for EAP creds"),
                         sizeof(WiFiAccount_t));
                result = ERROR_OUTOFMEMORY;
            }
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Runs the test.
// Returns ERROR_CALL_NOT_IMPLEMENTED if the test is to be skipped.
//
DWORD
AuthMatrix_t::
Run(void)
{
    // Select and configure an Access Point.
    DWORD result = ConfigureAccessPoint();

    // Repeat this as often as required...
    for (int passno = 1 ; ERROR_SUCCESS == result ; ++passno)
    {
        LogDebug(TEXT("\n"));
        if (1 < s_NumberTestPasses)
            LogDebug(TEXT("Test pass %d #==#==#==#==#==#==#==#==#==#==#==#==#"),
                     passno);

        // Capture error-messages.
        ce::tstring errorBuffer;
        ErrorLock errorLock(&errorBuffer);

        // Connect.
        result = m_Config.ConnectWiFi(GetWZCService(), m_pEapCredentials);
        if (ERROR_SUCCESS == result)
            result = CheckConnected();

        // If this is the first successful connection and it doesn't use EAP
        // authentication, get authentication certificates.
        if (ERROR_SUCCESS == result
         && NULL == m_pEapCredentials
         && !s_EnrollmentsCompleted)
            result = GetCertificates();

        // Look for the expected errors.
        if (m_IsBadModes)
        {
            if (ERROR_INVALID_FLAGS      == result
             || ERROR_INVALID_PARAMETER  == result
             || ERROR_CONNECTION_INVALID == result)
            {
                result = ERROR_SUCCESS;
            }
            else
            if (ERROR_SUCCESS == result)
            {
                LogError(TEXT("%s connected with bad auth %s and cipher %s"),
                         GetWZCService()->GetAdapterName(),
                         m_Config.GetAuthName(),
                         m_Config.GetCipherName());
                result = ERROR_INVALID_FLAGS;
            }
        }
        if (m_IsBadKey)
        {
            if (ERROR_INVALID_DATA       == result
             || ERROR_INVALID_PARAMETER  == result
             || ERROR_CONNECTION_INVALID == result)
            {
                result = ERROR_SUCCESS;
            }
            else
            if (ERROR_SUCCESS == result)
            {
                LogError(TEXT("%s connected with invalid key \"%s\""),
                         GetWZCService()->GetAdapterName(),
                         m_Config.GetKey());
                result = ERROR_INVALID_DATA;
            }
        }

        // Log the captured error-messages (if any).
        errorLock.StopCapture();
        if (0 != errorBuffer.length())
        {
            if (ERROR_SUCCESS == result)
                 LogDebug(errorBuffer);
            else LogError(errorBuffer);
        }

        // Stop if there's an error or we're finished.
        // Otherwise, disconnect and do it again.
        if (1 == s_NumberTestPasses)
            break;
        if (ERROR_SUCCESS == result && s_NumberTestPasses > passno)
            result = Cleanup();
        if (ERROR_SUCCESS == result)
        {
            LogDebug(TEXT("Test pass %d #==#==#==#==#==#==#==#==#==#==#==#==#")
                     TEXT(" PASSED"),
                     passno);
            if (s_NumberTestPasses <= passno)
                break;
        }
        else
        {
            LogDebug(TEXT("Test pass %d #==#==#==#==#==#==#==#==#==#==#==#==#")
                     TEXT(" FAILED: %s"),
                     passno, Win32ErrorText(result));
            break;
        }
    }
    
    // Deregister this as the current test.
    ce::gate<ce::critical_section> locker(s_CurrentTestLocker);
    s_pCurrentTest = NULL;
    locker.leave();
    
    LogDebug(TEXT("\n"));
    return result;
}

// ----------------------------------------------------------------------------
//
// Cleans up after the test finishes.
//
DWORD
AuthMatrix_t::
Cleanup(void)
{
    DWORD result = ERROR_SUCCESS;

    // If we're not skipping these security modes and the WZC service
    // was successfully initialized...
    if (!SkipSecurityModes(m_Config))
    {
        WZCService_t *pWZCService = GetWZCService();
        if (NULL != pWZCService) 
        {
            // Turn off the user-login dialog sub-thread (if any).
            if (NULL != m_pEapCredentials)
            {
                DWORD closeWaitTime = m_pEapCredentials->GetLogonCloseTime();
                m_pEapCredentials->CloseUserLogon(closeWaitTime);
            }

            // Gracefully disconnect the WiFi APControl server (if any).
            DissociateAPPool();

            // Dissociate all remaining WiFi connections.
            result = pWZCService->Clear();

            // Delay a few seconds to give WZC time to notice the config
            // is gone before we try the next test.
            pWZCService->Refresh();
            LogStatus(TEXT("Wait %d seconds for WZC disconnection"),
                      IPUtils::DefaultStableTimeMS / 1000);
            Sleep(IPUtils::DefaultStableTimeMS);
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Selects and configures the appropriate Access Point.
//
DWORD
AuthMatrix_t::
ConfigureAccessPoint()
{
    HRESULT hr;
    DWORD result;

    DWORD startTime = GetTickCount();
    
    // Select an Access Point Controller.
    // Returns ERROR_CALL_NOT_IMPLEMENTED if none of the APs can support
    // the specified security modes.
    APController_t *pAP = NULL;
    result = GetAPController(m_APConfig, &pAP, s_pSelectedAPNames,
                                          s_NumberSelectedAPNames);
    if (ERROR_SUCCESS != result)
        return result;
    assert(NULL != pAP);
    
    LogDebug(TEXT("\n"));
    LogStatus(TEXT("Configuring %s \"%s\": a %s %s"),
             pAP->GetSsid(), pAP->GetAPName(), 
             pAP->GetVendorName(), pAP->GetModelNumber());

#if 0
    LogDebug(TEXT("  setting SSID to \"%s\""), pSsid);
    hr = pAP->SetSsid(m_APConfig.GetSSIDName());
    if (FAILED(hr))
    {
        LogError(TEXT("Can't set AP %s SSID to %s: %s"),
                 pAP->GetAPName(), pSsid,
                 HRESULTErrorText(hr));
        return HRESULT_CODE(hr);
    }
#else
    m_Config.SetSSIDName(pAP->GetSsid());
    m_APConfig.SetSSIDName(pAP->GetSsid());
#endif

    LogDebug(TEXT("  enabling AP's radio"));
    hr = pAP->SetRadioState(true);
    if (FAILED(hr))
    {
        LogError(TEXT("Can't enable AP %s radio: %s"),
                 pAP->GetAPName(),
                 HRESULTErrorText(hr));
        return HRESULT_CODE(hr);
    }

    LogDebug(TEXT("  setting security modes to auth %s and cipher %s"),
             m_APConfig.GetAuthName(),
             m_APConfig.GetCipherName());
    hr = pAP->SetSecurityMode(m_APConfig.GetAuthMode(),
                              m_APConfig.GetCipherMode());
    if (FAILED(hr))
    {
        LogError(TEXT("Can't set AP %s to auth %s and cipher %s: %s"),
                 pAP->GetAPName(), m_APConfig.GetAuthName(),
                                   m_APConfig.GetCipherName(),
                 HRESULTErrorText(hr));
        return HRESULT_CODE(hr);
    }

    if (TEXT('\0') != m_APConfig.GetKey()[0])
    {
        if (APCipherWEP == m_APConfig.GetCipherMode())
        {
            WEPKeys_t newKeys = pAP->GetWEPKeys();
            LogDebug(TEXT("  setting WEP key #%d to \"%s\""),
                     m_APConfig.GetKeyIndex()+1, m_APConfig.GetKey());

            hr = newKeys.FromString(m_APConfig.GetKeyIndex(),
                                    m_APConfig.GetKey());
            if (FAILED(hr))
            {
                LogError(TEXT("Bad WEP key-data \"%s\": %s"),
                         m_APConfig.GetKey(),
                         HRESULTErrorText(hr));
                return HRESULT_CODE(hr);
            }

            hr = pAP->SetWEPKeys(newKeys);
            if (FAILED(hr))
            {
                LogError(TEXT("Can't set AP %s WEP #%d to \"%s\": %s"),
                         pAP->GetAPName(),
                         m_APConfig.GetKeyIndex()+1,
                         m_APConfig.GetKey(),
                         HRESULTErrorText(hr));
                return HRESULT_CODE(hr);
            }
        }
        else
        {
            LogDebug(TEXT("  setting passphrase to \"%s\""),
                     m_APConfig.GetKey());
            hr = pAP->SetPassphrase(m_APConfig.GetKey());
            if (FAILED(hr))
            {
                LogError(TEXT("Can't set %s passphrase to \"%s\": %s"),
                         pAP->GetAPName(),
                         m_APConfig.GetKey(),
                         HRESULTErrorText(hr));
                return HRESULT_CODE(hr);
            }
        }
    }

    LogDebug(TEXT("  updating AP's configuration"));
    hr = pAP->SaveConfiguration();
    if (FAILED(hr))
    {
        LogError(TEXT("Can't update AP %s configuration: %s"),
                 pAP->GetAPName(),
                 HRESULTErrorText(result));
        return HRESULT_CODE(hr);
    }

    long runDuration = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
    LogDebug(TEXT("  configuration completed in %.02f seconds"),
             (double)runDuration / 1000.0);

    // If we connected to the AP-control server using WiFi, get the
    // authentication certificates (if necessary) and dissociate from
    // the AP-control Access Point.
    if (IsWiFiServerAPConfigured())
    {
        if (!s_EnrollmentsCompleted)
        {
            result = GetCertificates();
            if (ERROR_SUCCESS != result)
                return result;
        }

        result = DissociateAPPool();
        if (ERROR_SUCCESS != result)
            return result;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Tests ICMP connectivity.
//
static DWORD
CheckICMPConnection(long Timeout)
{
    const long MinimumPingTimeout = 10000;
    if (Timeout < MinimumPingTimeout)
        Timeout = MinimumPingTimeout;

    LogDebug(TEXT("\n"));
    LogStatus(TEXT("Testing WiFi ICMP connection"));
    return IPUtils::SendPingMessages(WiFiBase_t::GetWiFiServerHost(),
                                     IPUtils::DefaultPingPort,
                                     Timeout);
}

// ----------------------------------------------------------------------------
//
// Tests UDP connectivity.
//
static DWORD
CheckUDPConnection(long Timeout)
{
    const long MinimumEchoTimeout = 10000;
    if (Timeout < MinimumEchoTimeout)
        Timeout = MinimumEchoTimeout;

    LogDebug(TEXT("\n"));
    LogStatus(TEXT("Testing WiFi UDP/IP connection"));
    return IPUtils::SendEchoMessages(WiFiBase_t::GetWiFiServerHost(),
                                     IPUtils::DefaultEchoPort,
                                     Timeout,
                                     false);
}

// ----------------------------------------------------------------------------
//
// Tests TCP connectivity.
//
static DWORD
CheckTCPConnection(long Timeout)
{
    const long MinimumEchoTimeout = 10000;
    if (Timeout < MinimumEchoTimeout)
        Timeout = MinimumEchoTimeout;

    LogDebug(TEXT("\n"));
    LogStatus(TEXT("Testing WiFi TCP/IP connection"));
    return IPUtils::SendEchoMessages(WiFiBase_t::GetWiFiServerHost(),
                                     IPUtils::DefaultEchoPort,
                                     Timeout,
                                     true);
}

// ----------------------------------------------------------------------------
//
// Makes sure the wireless adapter is properly connected.
//
DWORD
AuthMatrix_t::
CheckConnected(void)
{
    DWORD result;

    // Wait for WZC to connect.
    result = IPUtils::MonitorConnection(GetWZCService(),
                                        m_Config.GetSSIDName(),
                                        s_ConnectionTimeLimit);
    if (ERROR_SUCCESS != result)
        return (ERROR_TIMEOUT == result)? ERROR_CONNECTION_INVALID : result;

    DWORD startTime = GetTickCount();
    long runDuration = 0;

    // ICMP test:
    result = CheckICMPConnection(s_ConnectionTimeLimit);
    if (ERROR_SUCCESS != result)
        return result;

    // UDP/IP test:
    runDuration = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
    result = CheckUDPConnection(s_ConnectionTimeLimit - runDuration);
    if (ERROR_SUCCESS != result)
        return result;

    // TCP/IP test:
    runDuration = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
    result = CheckTCPConnection(s_ConnectionTimeLimit - runDuration);
    if (ERROR_SUCCESS != result)
        return result;

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
