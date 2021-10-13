//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name:  
	Common.h

Abstract:

    Header file for common data structure and functions

--*/


#ifndef _COMMON_H_
#define _COMMON_H_
#include "TestMain.h"

//debug output
#ifdef DEBUG
#define NKDMSG	NKDbgPrintfW
#else
#define NKDMSG
#endif

#define NKMSG	NKDbgPrintfW

#define 	UNDEFINED_VALUE		0xFF
#define	TEST_WAIT_TIME			1000  // 1 sec.
#define	TEST_MAX_CARDS		32

#define  IO_START_ADDRESS		0x4000

//client data definition
typedef struct  {
	UINT				uClientID;
	CARD_CLIENT_HANDLE	hClient;
	HANDLE  				hEvent;
     	UINT32  				uEventGot;
     	UINT32  				fEventGot;
   	UINT32  				uCardReqStatus;
	UINT32				uReqReturn;
} CLIENT_CONTEXT, *PCLIENT_CONTEXT;

#define MAX_WINDOWS_RANGES   4   // limit this to 4, it can be as high as 16
//
// @struct PARSED_PCMCFTABLE | Structure used by CardGetParsedTuple for parsed
// CISTPL_CFTABLE_ENTRY tuples.
//
typedef struct _PARSED_PCMCFTABLE {
    POWER_DESCR VccDescr;   // @field Power description of Vcc
    POWER_DESCR Vpp1Descr;  // @field Power description of Vpp1
    POWER_DESCR Vpp2Descr;  // @field Power description of Vpp2

    DWORD   IOLength[MAX_WINDOWS_RANGES];    // @field IOLength[i] is the number of bytes in 
    DWORD   IOBase[MAX_WINDOWS_RANGES];      // @field the I/O address range starting at IOBase[i]
    DWORD   MemLength[MAX_WINDOWS_RANGES];    // @field IOLength[i] is the number of bytes in 
    DWORD   MemBase[MAX_WINDOWS_RANGES];      // @field the I/O address range starting at IOBase[i]
    
                                        // There are NumIOEntries in each array.
    UINT8   NumIOEntries;   // @field Number of I/O address range entries in the two arrays:
                            // IOLength[] and IOBase[]
    UINT8   NumMemEntries;  // @field Number of Memory address range entries in the two arrays:
                            // MemLength[] and MemBase[]
    
    UINT8   ConfigIndex;    // @field Value to be written to the configuration option
                            // register to enable this CFTABLE_ENTRY.
    UINT8   ContainsDefaults;   // @field True if this entry contains defaults to be used by subsequent entries.

    UINT8   IFacePresent;   // @field True if the following 5 fields are valid:
    UINT8   IFaceType;      // @field 0 = memory, 1 = I/O, 2,3,8-15 = rsvd, 4-7 = custom
    UINT8   BVDActive;      // @field True if BVD1 and BVD2 are active in the Pin Replacement Register (PRR)
    UINT8   WPActive;       // @field True if the card write protect is active in the PRR
    UINT8   ReadyActive;    // @field True if READY is active in the PRR
    UINT8   WaitRequired;   // @field True if the WAIT signal is required for memory cycles

    UINT8   IOAccess;       // @field 1 = 8 bit only, 2 = 8 bit access to 16 bit registers is not supported,
                            // 3 = 8 bit access to 16 bit registers is supported

    UINT8   NumIOAddrLines; // @field 0 = socket controller provides address decode
                            // 1-26 = number of address lines decoded
    UINT16  wIrqMask;      // BitMask For supported IRQ;
    DWORD   dwSysIntr;      // System Interrupt ID allocated for driver.
    UINT8   bIrqNumber;     // Irq number associated with this configuration.
    
} PARSED_PCMCFTABLE, * PPARSED_PCMCFTABLE;


//function prototype
STATUS CallBackFn_Client(
    CARD_EVENT EventCode,
    CARD_SOCKET_HANDLE hSocket,
    PCARD_EVENT_PARMS pParms
    );
LPTSTR  FindEventName( CARD_EVENT EventCode);
LPTSTR  FindStatusName(STATUS StatusCode);
void updateError (UINT n);
BOOL NormalRequestConfig(CARD_CLIENT_HANDLE hClient, UINT8 uSock, UINT8 uFunc);
BOOL RetrieveRegisterValues(PCARD_CONFIG_INFO pConfInfo, CARD_SOCKET_HANDLE hSocket);

//tuple function that does not exposed by pcc_serv
STATUS PcmciaCardGetParsedTuple(CARD_SOCKET_HANDLE hSocket, UINT8  DesiredTuple, PVOID pBuf,PUINT pnItems);
STATUS ParseConfig(CARD_SOCKET_HANDLE hSocket, PVOID pBuf, PUINT   pnItems );
UINT32 VarToFixed(UINT32 VarSize, PUCHAR pBytes);
STATUS ParseCfTable(CARD_SOCKET_HANDLE hSocket, PVOID pBuf,PUINT   pnItems );
UINT ConvertVoltage(PUCHAR pCIS, PUSHORT pDescr);
UINT ParseVoltageDescr(PUCHAR pVolt, PPOWER_DESCR pDescr);

//tuple test related definitions, structures and function prototypes

#define CARD_IO  1            // Required for I/O cards
#define CARD_MEMORY 2         // Required for memory cards
#define CARD_IO_RECOMMENDED 4 // Recomended for I/O cards, note required bits are clear.

#define NUM_EXT_REGISTERS 		4
#define NUM_REGISTERS			5

#define TEST_IDLE_TIME	5000   //  5 sec
#define TOTAL_VALID_TUPLECODES		31

typedef struct 
{
    TCHAR *text ; 
    unsigned int index ; 
    int cardTypesSupportingTuple ;
} CISStrings ;

//Globals that will be used through out the whole test module
extern EVENT_NAME_TBL v_EventNames[];
extern UINT8   rgWinSpeed[MAX_WIN_SPEED];
extern UINT16   rgWinAttr[MAX_WIN_ATTR];
extern CLIENT_CONTEXT client_data;
extern TCHAR *RtnCodes [];
extern CISStrings TupleCodes [];
extern UINT 	LastError;
extern int  nThreadsDone   ;
extern SOCKET_DESCRIPTOR	g_SocketDescs[MAX_SOCKETS];
extern CARD_DESCRIPTOR		g_CardDescs[MAX_SOCKETS*2];
extern DWORD		g_dwTotalSockets;
extern DWORD		g_dwTotalCards;

#endif
