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
#ifndef PPPAUTH_H_
#define PPP_AUTH_H_

#include <winsock.h>
#include <windows.h>
#include <tchar.h>
#include <ras.h>
#include <raserror.h>
#include <tapi.h>
#include "tapiinfo.h"
#include <unimodem.h>

const static LPTSTR szHelp[] =
{
    TEXT("\n++ WindowCE PPPAuth ++\n\n"),
    TEXT("pppauth < -device id > < -user Username > < -pwd Password > < -domain Domain > < -dccuser Username > < -dccpwd Password > < -dccdomain Domain >\n"),
    TEXT("        [ -phone Phone ] [ -svr SvrName ] [ -opt ServOpt ] [ -? ]\n\n"),
    TEXT("options:\n\n"),
    TEXT("    -device choose available RAS device \n"),
    TEXT("    -user   user name\n"),
    TEXT("    -pwd    user password\n"),
    TEXT("    -domain dial-in domain \n"),
    TEXT("    -dccuser Direct cable connection user name\n"),
    TEXT("    -dccpwd Direct cable connection user password\n"),
    TEXT("    -dccdomain Direct cable connection dial-in domain \n"),
    TEXT("    -phone  dial-in phone number \n"),
    TEXT("    -svr    VPN server name\n"),
    TEXT("    -opt    ras server option\n"),
    TEXT("    -?      show this information\n\n")
};

#define MAX_CMD_PARAM 32
#define MAX_AUTH_NAME 128

#ifndef STRUCT_CMDLINEPARAMS
#define STRUCT_CMDLINEPARAMS
    typedef struct _cmdLineParams
    {
        TCHAR strUserName[MAX_CMD_PARAM];
        TCHAR strPassword[MAX_CMD_PARAM];
        TCHAR strDomain[MAX_CMD_PARAM];
        TCHAR strPhoneNum[MAX_CMD_PARAM];
        TCHAR strServerName[MAX_CMD_PARAM];
        TCHAR strServOpt[MAX_CMD_PARAM];
        TCHAR strEntryName[MAX_CMD_PARAM];
        TCHAR strPresharedKey[MAX_CMD_PARAM];
        TCHAR strDCCUserName[MAX_CMD_PARAM];
        TCHAR strDCCPassword[MAX_CMD_PARAM];
        TCHAR strDCCDomain[MAX_CMD_PARAM];
        DWORD dwDeviceID;
        DWORD dwRASAuthTypes;
    } CmdLineParams;

#endif

#ifndef ENUM_RASCONNECTION
#define ENUM_RASCONNECTION
    typedef enum _RASConnection {    RAS_VPN_PPTP,
                                    RAS_VPN_L2TP,
                                    RAS_DCC_MODEM,
                                    RAS_PPP_MODEM,
                                    RAS_PPP_PPPoE} RASConnection;
#endif

#ifndef DEBUG
    #define DEFAULT_SLEEP_INTERVAL 2000
    #define SLEEP() Sleep(DEFAULT_SLEEP_INTERVAL)
#else
    #define SLEEP()
#endif

typedef struct _sAuthOptions
{
    DWORD dwAuthOptions;
    TCHAR strAuthName[MAX_AUTH_NAME];
} sAuthOptions;

#define DCC_USER TEXT("GUEST")
#define DCC_PWD TEXT("Guest")
#define DCC_DOMAIN TEXT("")

#define CMD_DCC_USER_NAME TEXT("-dccuser")
#define CMD_DCC_PASSWORD TEXT("-dccpw")
#define CMD_DCC_DOMAIN TEXT("-dccdomain")

#define CMD_USER_NAME TEXT("-user")
#define CMD_PASSWORD TEXT("-pwd")
#define CMD_DOMAIN TEXT("-domain")
#define CMD_PHONE_NUMBER TEXT("-phone")
#define CMD_SERVER_NAME TEXT("-svr")
#define CMD_SERVER_OPT TEXT("-opt")
#define CMD_DEVICE_ID TEXT("-device")
#define CMD_PSK TEXT("-psk")
#define CMD_HELP_REQD TEXT("-?")


#define TXT_NODCC TEXT("NODCC")
#define TXT_EAP TEXT("EAP")
#define TXT_PAP TEXT("PAP")
#define TXT_MSCHAPv2 TEXT("MSCHAPv2")
#define TXT_MSCHAP TEXT("MSCHAP")
#define TXT_MD5CHAP TEXT("MD5CHAP")

#define MD5_CHALLENGE 4

#define NODCC 0x1
#define EAP 0x10
#define PAP 0x100
#define MSCHAPv2 0x1000
#define MSCHAP 0x10000
#define MD5CHAP 0x100000

#define    AUTH_MASK_PAP        (RASEO_ProhibitCHAP | RASEO_ProhibitMsCHAP | RASEO_ProhibitMsCHAP2 | RASEO_ProhibitEAP | RASEO_NoUserPwRetryDialog)
#define AUTH_MASK_CHAP_MD5    (RASEO_ProhibitPAP  | RASEO_ProhibitMsCHAP | RASEO_ProhibitMsCHAP2 | RASEO_ProhibitEAP | RASEO_NoUserPwRetryDialog)
#define AUTH_MASK_CHAP_MS    (RASEO_ProhibitPAP  | RASEO_ProhibitCHAP   | RASEO_ProhibitMsCHAP2 | RASEO_ProhibitEAP | RASEO_NoUserPwRetryDialog)
#define AUTH_MASK_CHAP_MSV2    (RASEO_ProhibitPAP  | RASEO_ProhibitCHAP   | RASEO_ProhibitMsCHAP  | RASEO_ProhibitEAP | RASEO_NoUserPwRetryDialog)
#define AUTH_MASK_EAP        (RASEO_ProhibitPAP  | RASEO_ProhibitCHAP   | RASEO_ProhibitMsCHAP  | RASEO_ProhibitMsCHAP2 | RASEO_NoUserPwRetryDialog)
#define AUTH_MASK_MIX        (0)

#define HAS_X_AUTHTYPE(a) ((a) & lpFTE->dwUserData)
#define COPY_STRINGS(a, b, c) StringCchCopy((a), (c), (b))
#define CMP_STRINGS(a, b, c) !_tcsnicmp((a), (b), (c))

#define COUNT(x) (sizeof((x))/sizeof((x)[0]))

#define SHOW_HELP()\
{\
    for(int i = 0; i < COUNT(szHelp); i++)\
    {\
        RASPrint(szHelp[i]);\
    }\
}

#define SHOW_VARIABLES()\
{\
    RASPrint(TEXT("User: %s"), cmdParameters.strUserName);\
    RASPrint(TEXT("Password: %s"), cmdParameters.strPassword);\
    RASPrint(TEXT("Domain: %s"), cmdParameters.strDomain);\
    RASPrint(TEXT("Phone Num: %s"), cmdParameters.strPhoneNum);\
    RASPrint(TEXT("Server Name: %s"), cmdParameters.strServerName);\
    RASPrint(TEXT("Server Opt: %s"), cmdParameters.strServOpt);\
    RASPrint(TEXT("Device ID: %d"), cmdParameters.dwDeviceID);\
    RASPrint(TEXT("Entry Name: %s"), cmdParameters.strEntryName);\
}

//
// Function prototypes
//
BOOL bParseCmdLine(int argc, TCHAR* argv[]);
BOOL bParseServOpt();
BOOL bGetDeviceType();

// In connect.cpp
bool CreatePhoneBookEntry(DWORD, DWORD);
bool ConnectRasConnection();
bool DisconnectRasConnection();
DWORD GetRasDeviceNum(LPTSTR, DWORD&);
DWORD RasLinkSetBaudrate(LPTSTR, DWORD, BYTE, BYTE, float);
DWORD GetRasDeviceName(LPTSTR, DWORD);
DWORD GetRasDeviceType(LPTSTR, DWORD);
DWORD GetUnimodemDevConfig(LPTSTR, LPVARSTRING);
DWORD GetUnimodemDeviceName(LPTSTR, LPTSTR);
void CALLBACK lineCallbackFunc(DWORD, DWORD,DWORD, DWORD, DWORD, DWORD);


// Macros etc
#define DIAL_MODIFIER_LEN 256
#define MAX_CFG_BLOB      126
#define MAX_NAME_LENGTH   8

typedef struct  tagDEVMINICFG  {
    WORD  wVersion;
    WORD  wWaitBong;             // DevCfgHdr
    
    DWORD dwCallSetupFailTimer;  // CommConfig.ModemSettings
    DWORD dwModemOptions;        // CommConfig.ModemSettings
                                 // MDM_BLIND_DIAL       MDM_FLOWCONTROL_SOFT
                                 // MDM_CCITT_OVERRIDE MDM_FORCED_EC
                                 // MDM_CELLULAR       MDM_SPEED_ADJUST
                                 // MDM_COMPRESSION    MDM_TONE_DIAL
                                 // MDM_ERROR_CONTROL  MDM_V23_OVERRIDE
                                 // MDM_FLOWCONTROL_HARD
    
    DWORD dwBaudRate;            // DCB

    WORD  fwOptions;             // DevCfgHdr
                                 // TERMINAL_PRE  TERMINAL_POST 
                                 // MANUAL_DIAL

    BYTE  ByteSize;              // DCB
    BYTE  StopBits;              // DCB
    BYTE  Parity;                // DCB

    WCHAR szDialModifier[DIAL_MODIFIER_LEN+1];    // Unique to MiniCfg

    // Dynamic devices configuration
    WCHAR   wszDriverName[MAX_NAME_LENGTH+1];
    BYTE    pConfigBlob[MAX_CFG_BLOB];
    HANDLE  hPort;
} DEVMINICFG;

#define ENTRYBUFSIZE         5000
#define DEFAULT_BAUDRATE    57600

#define UNIMODEM_SERIAL_DEVICE   0
#define UNIMODEM_HAYES_DEVICE    1
#define UNIMODEM_PCMCIA_DEVICE   3
#define UNIMODEM_IR_DEVICE       6

#define DEVCAPSSIZE            1024
#define DEVCONFIGSIZE   sizeof(DEVMINICFG)
#define DEVCONFIGOFFSET sizeof(VARSTRING)
#define DEVBUFSIZE      DEVCONFIGSIZE + DEVCONFIGOFFSET

//
// Constants for Tux Entry tables
//
#define USE_L2TP 0x40000000
#define USE_PPTP 0x80000000
#define USE_DCC 0x20000000

#define USE_PAP 0x2000000
#define USE_CHAP 0x4000000
#define USE_NODCC 0x8000000
#define USE_EAP 0x200000
#define USE_MSCHAP 0x400000
#define USE_MSCHAPv2 0x800000

#define PROHIBIT_PAP 0
#define PROHIBIT_MSCHAPV2 1
#define PROHIBIT_MSCHAP 2
#define PROHIBIT_CHAP 3
#define PROHIBIT_EAP 4
#define REQUIRE_ENC_PW 5
#define REQUIRE_MS_ENC_PW 6
#define REQUIRE_DATA_ENC 7
#define REQUIRE_ENC_MS_PW 8
#define REQUIRE_MS_DATA_ENC 9
#define REQUIRE_MS_DATA_ENC_PW 10
#define REQUIRE_NO_FLAGS 11


#endif // PPP_AUTH_H_
