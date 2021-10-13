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
#ifndef DIAL_H
#define DIAL_H

#define toupper(ch)    (((ch >= 'a') && (ch <= 'z')) ? ch-'a'+'A':ch)
//#define isxdigit(ch)    (((ch >= 'a') && (ch <= 'f')) || ((ch >= 'A') && (ch <= 'F')) || ((ch >= '0') && (ch <= '9')))
//#define isdigit(ch)     ((ch >= '0') && (ch <= '9'))
#define ctox(ch)        (((ch >='0') && (ch <= '9')) ? ch-'0': toupper(ch)-'A'+10)

#define MAXSTRINGLENGTH     256
#define MAXUINTSTRLENGTH    11  // Max UINT in ascii + terminator

#define REG_NULL            0xFFFFFFFF  // indicates an invalid registry handle

#define CMD_INDEX_START 1  // "1", "2", "3", "4", ...

typedef struct _ModemMacro {
    TCHAR  MacroName[MAXSTRINGLENGTH];
    TCHAR  MacroValue[MAXSTRINGLENGTH];
} MODEMMACRO;

#define LMSCH   '<'
#define RMSCH   '>'

#define APPEND_MACRO        TEXT("<append>")
#define APPEND_MACRO_LENGTH 8
#define CR_MACRO            TEXT("<cr>")
#define CR_MACRO_LENGTH     4
#define LF_MACRO            TEXT("<lf>")
#define LF_MACRO_LENGTH     4
#define INTEGER_MACRO            TEXT("<#>")
#define INTEGER_MACRO_LENGTH     3

#define CR                  '\r'        // 0x0D
#define LF                  '\n'        // 0x0A

#define MAX_COMMAND_TRIES 3  // # of times to try a command before giving up.
#define MAX_HANGUP_TRIES  3  // # of times to try hanging up before giving up.
#define MODEM_ESCAPE_SEQUENCE     "+++"
#define MODEM_ESCAPE_SEQUENCE_LEN 3

#define MDMLOG_RESPONSE     1
#define MDMLOG_COMMAND_OK   2
#define MDMLOG_COMMAND_FAIL 3

typedef enum _ModemRespCodes
{
    MODEM_SUCCESS    = 0,   
    MODEM_PENDING,
    MODEM_CONNECT,
    MODEM_FAILURE,
    MODEM_HANGUP,
    MODEM_NODIALTONE,
    MODEM_BUSY,
    MODEM_NOANSWER,
    MODEM_RING,
    MODEM_CARRIER,
    MODEM_PROTOCOL,
    MODEM_PROGRESS,
    MODEM_UNKNOWN,
    MODEM_IGNORE,     // Used to ignore echo's of original command
    MODEM_EXIT,
    MODEM_ABORT
} MODEMRESPCODES;


typedef struct _MODEM_RESPONSE
{
    PUCHAR pszResponse;
    MODEMRESPCODES ModemRespCode;
} MODEM_RESPONSE, *PTMODEMRESPONSE;
    
//
// Second to last and last parameters to MdmGetResponse
//
#define LEAVECS TRUE
#define NOCS    FALSE
#define CLOSING FALSE
#define OPENED  TRUE

MODEMRESPCODES MdmGetResponse(PTLINEDEV pLineDev, PUCHAR pszCommand, BOOL bLeaveCS, BOOL bOpened);
BOOL MdmSendCommand(PTLINEDEV pLineDev, UCHAR const *pszCommand);
LPSTR MdmConvertCommand(WCHAR const *pszWCommand,LPSTR pchCmd,LPDWORD lpdwSize);

#endif  // DIAL_H
