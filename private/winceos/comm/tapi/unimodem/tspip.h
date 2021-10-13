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
//
//  
//  Module Name:
//  
//      tspip.h
//  
//  Abstract:
//  
//      Private header file for tspi 
//  
//

#include <termctrl.h>
#include "unimodem.h"

#define DEFAULT_UNIMODEM_THREAD_PRIO 200
#define EXCEPTION_ACCESS_VIOLATION STATUS_ACCESS_VIOLATION 

#ifdef DEBUG
#define ZONE_INIT		DEBUGZONE(0)
#define ZONE_TEMP		DEBUGZONE(1)    // temporary zone for debugging new code.
#define ZONE_ASYNC      DEBUGZONE(2)    // async operation stuff
#define ZONE_DIAL       DEBUGZONE(7)    // Dial string manipulation
#define ZONE_THREAD     DEBUGZONE(8)    // UnimodemControlThread
#define ZONE_LIST       DEBUGZONE(9)    // Linked list walks
#define ZONE_CALLS		DEBUGZONE(10)
#define ZONE_MISC		DEBUGZONE(11)
#define ZONE_ALLOC		DEBUGZONE(12)
#define ZONE_FUNCTION	DEBUGZONE(13)
#define ZONE_FUNC	    ZONE_FUNCTION
#define ZONE_WARN		DEBUGZONE(14)
#define ZONE_ERROR		DEBUGZONE(15)
#endif

#define  WM_MDMMESSAGE      WM_USER+0x0100
#define  WM_MDMCHANGE       WM_USER+0x0101
#define  WM_MDMCANCEL       WM_USER+0x0102

#define  MDM_SUCCESS        0
#define  MDM_PENDING        1
#define  MDM_FAILURE        2
#define  MDM_HANGUP         3
#define  MDM_BUSY           4
#define  MDM_NOANSWER       5
#define  MDM_NOCARRIER      6
#define  MDM_NODIALTONE     7

#ifndef OFFSETOF
#define OFFSETOF(Structure,Field)           ((DWORD)&(((Structure*)0)->Field))
#endif

#define SPI_VERSION     TAPI_CURRENT_VERSION
#define  VALIDATE_VERSION(version)  \
    {if (version != SPI_VERSION) \
        { \
        DEBUGMSG(1|ZONE_ERROR, (TEXT("Invalid SPI Version x%X\r\n"), version)); \
        return LINEERR_OPERATIONFAILED; \
        } \
    }


#define SUCCESS                             0x0

#define  MDM_ID_NULL        0xffff  // Async ID for an unexpected message

#define  SZWCHAR (sizeof(WCHAR))

// BUGBUG - what is actual device name max
#define MAXDEVICENAME       128
#define  MAXADDRESSLEN      TAPIMAXDESTADDRESSSIZE
// BUGBUG - what is actual class name max
#define MAX_CLASS_NAME_LEN       128

#define  INVALID_DEVICE     0xFFFFFFFF
#define  INVALID_PENDINGID  0xFFFFFFFF

// Check for an error code
//
#define  IS_TAPI_ERROR(err)         (BOOL)(HIWORD(err) & 0x8000)

// Device Class and Information
//
#define TAPILINE            0
#define COMM                1
#define COMMMODEM           2
#define NDIS                3
#define MAX_SUPPORT_CLASS   4

typedef struct  _GETIDINFO {
    LPWSTR       szClassName;
    DWORD       dwFormat;
}   GETIDINFO;

extern const GETIDINFO   aGetID[MAX_SUPPORT_CLASS];

// Pending operation type
//
#define INVALID_PENDINGOP       0
#define PENDING_LINEMAKECALL    1
#define PENDING_LINEANSWER      2
#define PENDING_LINEDROP        3
#define PENDING_LINEDIAL        4
#define PENDING_LINEACCEPT      5
#define PENDING_LINEDEVSPECIFIC 6
#define PENDING_LISTEN          254
#define PENDING_EXIT            255

// Flags for setting passthrough mode
#define PASSTHROUGH_ON                 1
#define PASSTHROUGH_OFF                2
#define PASSTHROUGH_OFF_BUT_CONNECTED  3

// Flags for resources
//
#define LINEDEVFLAGS_OUTOFSERVICE   0x00000001
#define LINEDEVFLAGS_REMOVING       0x00000002

// Enumerated States of the line device
typedef enum DevStates  {
    DEVST_DISCONNECTED      = 0,
    DEVST_PORTSTARTPRETERMINAL,
    DEVST_PORTPRETERMINAL,
    DEVST_PORTCONNECTINIT,
    DEVST_PORTCONNECTWAITFORLINEDIAL,  // this is a resting state.  ie. we sit here waiting for a lineDial.
    DEVST_PORTCONNECTDIALTONEDETECT,
    DEVST_PORTCONNECTDIAL,
    DEVST_PORTCONNECTING,
    DEVST_PORTPOSTTERMINAL,
    DEVST_CONNECTED,
    DEVST_PORTLISTENINIT,
    DEVST_PORTLISTENING,
    DEVST_PORTLISTENOFFER,
}   DEVSTATES;

typedef enum _MDMSTATE
{
    MDMST_UNKNOWN       = 1,
    MDMST_INITIALIZING,       
    MDMST_DISCONNECTED,       
    MDMST_DIALING,                    
    MDMST_CONNECTED,                  
    MDMST_DIALED,             
    MDMST_ORIGINATING,        
    MDMST_HANGING_UP_REMOTE,  // This is when the remote side hangs up.
                              // modem: Wait for response and then:
                              //        - send MODEM_HANGUP
                              //        - set MDMSTATE to MDMSTATE_DISCONNECTED
    MDMST_HANGING_UP_DTR,     // After dropping DTR and waiting for 1200ms, check RLSD:
                              //   If RLSD is low, raise DTR and set state to
                              //     modem: MDMSTATE_HANGING_UP_NON_CMD
                              //     null-modem: MDMSTATE_DISCONNECTED
                              //   Else set state to:
                              //     modem: MDMSTATE_HANGING_UP_NON_COMMAND and send "+++"
                              //     null-modem: same, wait another 200ms (keeping count, stop at 3 or so)
    MDMST_HANGING_UP_NON_CMD, // After sending a \r to hangup or sending +++ or getting RLSD low:
                              // Wait for any response or timeout and then:
                              // - send ATH<cr>
                              // - set state to MDMSTATE_HANGING_UP_CMD
    MDMST_HANGING_UP_CMD,     // Wait for a response to ATH<cr>
                              // If you get one, you are hung up, raise DTR, set state to
                              //   MDMSTATE_DISCONNECTED and return MODEM_SUCCESS.
                              // Else if you don't get one, consider dropping DTR, waiting 200ms more
                              //   and setting state to MDMSTATE_HANGING_UP_DTR. (keep track of
                              //   how many times you do this, max out at 3 or so.)
} MDMSTATE;


// Flags for the call attributes
//
#define CALL_ALLOCATED   0x00000001
#define CALL_ACTIVE      0x00000002
#define CALL_INBOUND     0x00000004
#define CALL_DROPPING    0x00000008

// BUGBUG these should be defined for win32, but I can't find them
#define AnsiNext(x)         ((x)+1)
#define AnsiPrev(y,x)       ((x)-1)

// Flags for the fwOptions field of DEVCFGHDR
//
#define TERMINAL_NONE       0x0000
#define TERMINAL_PRE        0x0001
#define TERMINAL_POST       0x0002
#define MANUAL_DIAL         0x0004
#define LAUNCH_LIGHTS       0x0008

#define  MIN_WAIT_BONG      0
#define  MAX_WAIT_BONG      60
#define  DEF_WAIT_BONG      8
#define  INC_WAIT_BONG      2

// Device Setting Information
//
typedef struct  tagDEVCFGHDR  {
    WORD        fwOptions;
    WORD        wWaitBong;
}   DEVCFGHDR;

typedef struct  tagDEVCFG  {
    DEVCFGHDR   dfgHdr;
    COMMCONFIG  commconfig;
}   DEVCFG, *PDEVCFG, FAR* LPDEVCFG;

// In TAPI32, the devcfg structure is passed back to applications, 
// which store it in their address books.  So, in CE TAPI I define
// a compact version of the devcfg that contains all of the fields
// that we care about.  It is this structure that gets returned to
// the apps and stored in the registry.  Then when a lineSetDevConfig
// call occurs, I copy individual fields out of the devMiniCfg into
// the memory resident devcfg.
//
// The comments next to each field indicate which data structure
// they belong to when expanded.

// DEVMINCFG_VERSION version 0x0020 includes changes to support wireless modems 
// requiring an address to connect to

#define DEVMINCFG_VERSION 0x0030
//
// Version 0x0030 changes:
// - increase the size of szDialModifier
//
#define DIAL_MODIFIER_LEN 256
#define MAX_CFG_BLOB	  126
#define MAX_NAME_LENGTH   8
typedef struct  tagDEVMINICFG  {
    WORD  wVersion;
    WORD  wWaitBong;             // DevCfgHdr
    
    DWORD dwCallSetupFailTimer;  // CommConfig.ModemSettings
    DWORD dwModemOptions;        // CommConfig.ModemSettings
                                 // MDM_BLIND_DIAL	   MDM_FLOWCONTROL_SOFT
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

} DEVMINICFG, *PDEVMINICFG;


#define DEVMINCFG_VERSION_0020 0x0020
//
// Version 0x0020 changes:
// - add wszDriverName, pConfigBlob and hPort to support dynamic Bluetooth modem devices
//
#define DIAL_MODIFIER_LEN_0020 40

typedef struct  tagDEVMINICFG_0020 {
    WORD  wVersion;
    WORD  wWaitBong;             // DevCfgHdr
    
    DWORD dwCallSetupFailTimer;  // CommConfig.ModemSettings
    DWORD dwModemOptions;        // CommConfig.ModemSettings
                                 // MDM_BLIND_DIAL	   MDM_FLOWCONTROL_SOFT
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

    WCHAR szDialModifier[DIAL_MODIFIER_LEN_0020+1];    // Unique to MiniCfg

    // Dynamic devices configuration
    WCHAR   wszDriverName[MAX_NAME_LENGTH+1];
    BYTE    pConfigBlob[MAX_CFG_BLOB];
    HANDLE  hPort;

} DEVMINICFG_0020, *PDEVMINICFG_0020;

#define DEVMINCFG_VERSION_0010  0x0010
#define DIAL_MODIFIER_LEN_0010  40

typedef struct  tagDEVMINICFG_0010 {
    WORD  wVersion;
    WORD  wWaitBong;             // DevCfgHdr
    
    DWORD dwCallSetupFailTimer;  // CommConfig.ModemSettings
    DWORD dwModemOptions;        // CommConfig.ModemSettings
                                 // MDM_BLIND_DIAL	   MDM_FLOWCONTROL_SOFT
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

    WCHAR szDialModifier[DIAL_MODIFIER_LEN_0010+1];    // Unique to MiniCfg

} DEVMINICFG_0010, *PDEVMINICFG_0010;

#define MIN_DEVCONFIG_SIZE (sizeof(DEVMINICFG_0010)-(DIAL_MODIFIER_LEN_0010*sizeof(WCHAR)))

#define MAX_CMD_LENGTH       DIAL_MODIFIER_LEN+4
#define DEF_CMD_SEND_DELAY   0
#define MAX_CMD_SEND_DELAY   500

#define DEFAULT_ESCAPE_DELAY    0      // Number of milliseconds to delay before issuing '+++'
#define DEFAULT_ESCAPE_WAIT     200    // Number of milliseconds to wait after issuing '+++'
#define DEFAULT_HANGUP_WAIT     4000   // Number of milliseconds to wait after issuing 'ATH'


// Line device data structure
//
typedef struct __LineDev   {
    LIST_ENTRY  llist;        // pointer to next LineDev
    DWORD        dwVersion;              // Version stamp
    DWORD        dwDeviceID;             // Local device ID
    WCHAR        szDeviceName[MAXDEVICENAME+1]; // actual device name
    WCHAR        szFriendlyName[MAXDEVICENAME+1]; // friendly device name

    HKEY         hSettingsKey;           // Registry handle for settings key
    
    WORD         wDeviceType;            // the modem type
    WORD         wDeviceAvail;           // Is the modem currently available?
    DWORD        dwPPPMTU;               // MTU that PPP will use on this device.

    DEVMINICFG   DevMiniCfg;             // Compact version of Device configuration

    DWORD        dwDefaultMediaModes;    // Default supported media modes
    DWORD        dwBearerModes;          // supported bearer modes
    DWORD        dwCurBearerModes;       // The current media bearer modes. Plural because
                                         // we keep track of PASSTHROUGH _and_ the real b-mode
                                         // at the same time.
    DWORD        dwMediaModes;           // Current supported media modes
    DWORD        dwCurMediaModes;        // The current media modes
    DWORD        dwDetMediaModes;        // The current detection media modes

    HANDLE       hDynamicPort;           // Handle to dynamic port created by ActivateDevice()
    HANDLE       hDevice;                // Device handle
    HANDLE       hDeviceOwnerProc;       // Device handle current owner process handle
    DWORD        pidDevice;              // Device owner pid
    HANDLE       hDevice_r0;             // ring-0 Device handle
    DWORD        dwWaitMask;
    HTAPILINE    htLine;                 // Tapi line handle
    LINEEVENT    lpfnEvent;              // Line event callback function
    HWND         hwndLine;               // Tapi emulation
    WCHAR        szAddress[MAXADDRESSLEN+1];
    DWORD        dwPendingID;            // async pending ID
    DWORD        dwPendingType;          // pending operation
    DWORD        dwPendingStatus;        // status of pending op
#ifdef DEBUG
    DWORD        dwLastPendingOp;
#endif
    DWORD        dwCallFlags;            // Call attributes
    HTAPICALL    htCall;                 // TAPI call handle
#ifdef USE_TERMINAL_WINDOW  // removing dial terminal window feature
    HWND         hwTermCtrl;             // TermCtrl Window Handle
#endif    
    HANDLE       hTimeoutEvent;          // Event Handle for Call Timeout event
    DWORD        dwCurWatchdog;
    HANDLE       hCallComplete;          // Event Handle for Call Completions   
    DWORD        dwCallState;            // Current call state
    DWORD        dwCallStateMode;        // Further details about current call state
    DWORD        dwNumRings;

    CRITICAL_SECTION OpenCS;             // Critical Section for DevLineClose

    DEVSTATES    DevState;               // intermediate TAPI device state
    
    MDMSTATE     MdmState;               // what state is the modem in
    
    DWORD        dwDialOptions;          // Options set in a lineMakeCall

    BOOL         fTakeoverMode;          // True if unimodem is in takover mode
    WCHAR        szDriverKey[MAX_CLASS_NAME_LEN+10];  // ex. "Modem\0000"

    DWORD        dwDevCapFlags;          // LINEDEVCAPSFLAGS (ie. DIALBILLING, DIALQUIET, DIALDIALTONE)
    DWORD        dwMaxDCERate;           // Max DCE as stored in the Properties line of the registry
    DWORD        dwCurrentBaudRate;      // Extracted from the CONNECT or CARRIER response
    DWORD        dwLastBaudRate;
    DWORD        dwLineStatesMask;       // Filter status messages
    DWORD        dwMaxCmd;
    DWORD        dwCmdSendDelay;         // Time in milliseconds to wait before sending a response to modem
    DWORD        EscapeDelay;
    DWORD        EscapeWait;
    DWORD        HangupWait;
    BOOL         bWatchdogTimedOut;
    BOOL         bMdmLogFile;            // Control logging of modem command history.
    HANDLE       hMdmLog;                // Modem command history for this device.
    
    //
    // Fields used by UnimodemControlThread
    //
    BOOL         bControlThreadRunning;
    LPSTR        lpstrNextCmd;
    WCHAR        chContinuation;        // Modem command continuation character (usually ';')
}   TLINEDEV, *PTLINEDEV;

// Default mask to MDM_ options
//
#define MDM_MASK (MDM_TONE_DIAL | MDM_BLIND_DIAL)

typedef struct _TSPIGLOBALS
{
    HINSTANCE          hInstance;
    DWORD              dwProviderID;
    HPROVIDER          hProvider;
    HKEY               hDefaultsKey;
    LINEEVENT          fnLineEventProc;      // Line Event callback in TAPI
    ASYNC_COMPLETION   fnCompletionCallback; // Completion Create callback in TAPI
    
    LIST_ENTRY         LineDevs;             // Linked List of TLINEDEV
    CRITICAL_SECTION   LineDevsCS;           // Critical section for above list
    HICON              hIconLine;
    BOOL               bExternModems;
} TSPIGLOBALS, *PTSPIGLOBALS;

extern TSPIGLOBALS TspiGlobals;
extern const WCHAR szSettings[];
extern const WCHAR szDialSuffix[];


// Some quick macros till we actually implement MemTracking
#define TSPIAlloc( Size )  LocalAlloc( LPTR, Size )
#define TSPIFree( Ptr )    LocalFree( Ptr )

// Default CommMask for SetCommMask/WaitCommEvent
#define EV_DEFAULT (EV_BREAK|EV_CTS|EV_DSR|EV_ERR|EV_RING|EV_RLSD|EV_RXCHAR)

// Routines from config.c
LONG DevSpecificLineConfigEdit(PTLINEDEV pLineDev, PUNIMDM_CHG_DEVCFG pChg);
LONG DevSpecificLineConfigGet(PTLINEDEV pLineDev, PUNIMDM_CHG_DEVCFG pChg);

// Routines from dialer.c
void Dialer(PTLINEDEV pLineDev);
BOOL ModemInit(PTLINEDEV pLineDev);
LPWSTR PendingOpName(DWORD dwPendingOp);

// Helper routines defined in misc.c
void ConvertDevConfig(PDEVMINICFG pInDevMiniCfg, DWORD dwSize, PDEVMINICFG pOutDevMiniCfg);
VOID getDefaultDevConfig(PTLINEDEV ptLineDev, PDEVMINICFG pDevMiniCfg);
void TSPIDLL_Load(void);
PTLINEDEV createLineDev(HKEY hActiveKey, LPCWSTR lpszDevPath, LPCWSTR lpszDeviceName);
PTLINEDEV GetLineDevfromID(DWORD dwDeviceID);
PTLINEDEV GetLineDevfromName(LPCWSTR lpszDeviceName,LPCWSTR lpszFriendlyName);
PTLINEDEV GetLineDevfromHandle (DWORD handle);
PTLINEDEV LineExists( PTLINEDEV ptNewLine);
BOOL ValidateDevCfgClass (LPCWSTR lpszDeviceClass);
DWORD NullifyLineDevice (PTLINEDEV pLineDev, BOOL bDefaultConfig);
LONG DevlineGetDefaultConfig(PTLINEDEV pLineDev);
LONG ValidateAddress( PTLINEDEV pLineDev, LPCWSTR lpszInAddress, LPWSTR lpszOutAddress);
LONG DevlineDial( PTLINEDEV pLineDev );
#ifdef USE_TERMINAL_WINDOW  // removing dial terminal window feature
BOOL DisplayTerminalWindow(PTLINEDEV pLineDev, DWORD dwTitle);
#endif
void CallLineEventProc(PTLINEDEV ptLine, HTAPICALL htCall, DWORD dwMsg, DWORD dwParam1, DWORD dwParam2, DWORD dwParam3);
void CompleteAsyncOp(PTLINEDEV pLineDev);
BOOL StartCompleteAsyncOp(PTLINEDEV pLineDev);
void NewCallState(PTLINEDEV pLineDev,DWORD dwCallState,DWORD dwCallState2);
void InitVarData(LPVOID lpData, DWORD dwSize);
DWORD SetAsyncID(PTLINEDEV pLineDev, DWORD dwID);
void SetAsyncStatus(PTLINEDEV pLineDev, DWORD dwNewStatus);
void SetAsyncOp(PTLINEDEV pLineDev, DWORD dwNewOp);

BOOL IsDynamicDevice(PTLINEDEV pLineDev);
BOOL IsNullModem(PTLINEDEV pLineDev);
#define  IS_NULL_MODEM(pLineDev)    (IsNullModem(pLineDev))

// Routines from modem.c
LPWSTR GetPendingName(DWORD dwPendingType);
LONG DevlineClose (PTLINEDEV pLineDev, BOOL fDoDrop);
LONG DevlineDetectCall(PTLINEDEV pLineDev);
LONG DevlineDrop(PTLINEDEV pLineDev);
LONG DevlineOpen(PTLINEDEV pLineDev);
LONG ControlThreadCmd(PTLINEDEV pLineDev,DWORD dwPendingOP,DWORD dwPendingID);
void GetHangupCommands(PTLINEDEV pLineDev, LPSTR pEscapeCmd, LPSTR pHangupCmd, LPSTR pResetCmd);

// Routines from registry.c
DWORD MdmRegGetValue(PTLINEDEV ptLineDev, LPCWSTR szKeyName, LPCWSTR szValueName,
                     DWORD dwType, LPBYTE lpData, LPDWORD lpdwSize);
void MdmRegGetLineValues(PTLINEDEV ptLineDev);
void EnumExternModems( void );

