//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************
* 
*
*  File:   irlapp.h 
*
*  Description: IRLAP Protocol and control block definitions
*
*  Date:   4/15/95
*
*/

#define UNICODE_CHAR_SET                0xFF
#define IRLAP_MEDIA_SENSE_TIME          520
#define IRLAP_SLOT_TIMEOUT              50
#define IRLAP_DSCV_SENSE_TIME           80
#define IRLAP_FAST_POLL_TIME            10
#define IRLAP_FAST_POLL_COUNT           10

// XID Format
#define IRLAP_XID_DSCV_FORMAT_ID     0x01
#define IRLAP_XID_NEGPARMS_FORMAT_ID 0x02
typedef struct 
{
    UCHAR    SrcAddr[IRDA_DEV_ADDR_LEN];
    UCHAR    DestAddr[IRDA_DEV_ADDR_LEN];
    UCHAR    NoOfSlots:2;
    UCHAR    GenNewAddr:1;
    UCHAR    Reserved:5;
    UCHAR    SlotNo;
    UCHAR    Version;
    UCHAR    FirstDscvInfoByte;
} IRLAP_XID_DSCV_FORMAT;

// Frame Reject Format
typedef struct
{
    UCHAR    CntlField;
    UCHAR    Fill1:1;
    UCHAR    Vs:3;
    UCHAR    CRBit:1;
    UCHAR    Vr:3;
    UCHAR    W:1;
    UCHAR    X:1;
    UCHAR    Y:1;
    UCHAR    Z:1;
    UCHAR    Fill2:4;
} IRLAP_FRMR_FORMAT;

// SNRM Frame Format
typedef struct
{
    UCHAR    SrcAddr[IRDA_DEV_ADDR_LEN];
    UCHAR    DestAddr[IRDA_DEV_ADDR_LEN];
    UCHAR    ConnAddr; // actually shifted for CRBit -> 
    UCHAR    FirstQosByte;
} IRLAP_SNRM_FORMAT;

// UA Frame Format
typedef struct
{
    UCHAR    SrcAddr[IRDA_DEV_ADDR_LEN];
    UCHAR    DestAddr[IRDA_DEV_ADDR_LEN];
    UCHAR    FirstQosByte;
} IRLAP_UA_FORMAT;

#define IRLAP_MAX_TX_MSG_LIST_LEN       8
#define IRLAP_MAX_DATA_SIZE             4096
#define IRLAP_MAX_SLOTS                 16
#define IRLAP_CONTENTION_BAUD           9600
#define IRLAP_CONTENTION_DATA_SIZE      64
#define IRLAP_CONTENTION_MAX_TAT        500
#define IRLAP_CONTENTION_BOFS           10
#define IRLAP_CONTENTION_WIN_SIZE       1

// Default Qos if not found in registry
#define IRLAP_DEFAULT_DATASIZE          (DATA_SIZE_64  | DATA_SIZE_128 | \
                                         DATA_SIZE_256 | DATA_SIZE_512 | \
                                         DATA_SIZE_1024| DATA_SIZE_2048)
#define IRLAP_DEFAULT_WINDOWSIZE        (FRAMES_1 | FRAMES_2 | FRAMES_3 | \
                                         FRAMES_4 | FRAMES_5 | FRAMES_6 | \
                                         FRAMES_7)
#define IRLAP_DEFAULT_MAXTAT            (MAX_TAT_500)
#define IRLAP_DEFAULT_SLOTS             6

/*
   Hint bit definitions

The first service hint byte. The individual bits have the following meaning:
    Bit 0 (0x01) - PnP Compatible
    Bit 1 (0x02) - PDA/Palmtop
    Bit 2 (0x04) - Computer
    Bit 3 (0x08) - Printer
    Bit 4 (0x10) - Modem
    Bit 5 (0x20) - Fax
    Bit 6 (0x40) - LAN Access
    Bit 7 (0x80) - Extension 
    
The second service hint byte. The individual bits have the following meaning:
    Bit 0 (0x01) - Telephony
    Bit 1 (0x02) - File Server
    Bit 2 (0x04) - IrCOMM
    Bit 3 (0x08) - reserved
    Bit 4 (0x10) - reserved
    Bit 5 (0x20) - IrOBEX
    Bit 6 (0x40) - reserved
    Bit 7 (0x80) - Extension 
*/

#ifdef UNDER_CE
#define IRLAP_DEFAULT_HINTS             0x8204  
#define IRLAP_DEFAULT_CHARSET           0x0     
#else
#define IRLAP_DEFAULT_HINTCHARSET       0x840400
#endif 

#define IRLAP_DEFAULT_DISCONNECTTIME    DISC_TIME_12 | DISC_TIME_8 | DISC_TIME_3

// Macros for extracting various fields
#define IRLAP_GET_ADDR(addr)        (addr >> 1)
#define IRLAP_GET_CRBIT(addr)       (addr & 1) 
#define IRLAP_GET_PFBIT(cntl)       ((cntl >>4) & 1)
#define IRLAP_GET_UCNTL(cntl)       (cntl & 0xEF)
#define IRLAP_GET_SCNTL(cntl)       (cntl & 0x0F)
#define IRLAP_FRAME_TYPE(cntl)      (cntl & 0x01 ? (cntl & 3) : 0)
#define IRLAP_GET_NR(cntl)          ((cntl & 0xE0) >> 5)
#define IRLAP_GET_NS(cntl)          ((cntl & 0xE) >> 1)     

// IRLAP constants
#define IRLAP_BROADCAST_CONN_ADDR   0x7F
#define IRLAP_END_DSCV_SLOT_NO      0xFF
#define IRLAP_CMD                   1
#define IRLAP_RSP                   0
#define IRLAP_PFBIT_SET             1
#define IRLAP_PFBIT_CLEAR           0
#define IRLAP_GEN_NEW_ADDR          1
#define IRLAP_NO_NEW_ADDR           0

// Macro for creating Number of Slots of Discovery Flags Field of XID Format
// if S(Slots) <= 1 return 0, <= 6 return 1, <= 8 return 2, else return 3
#define IRLAP_SLOT_FLAG(S)  (S <= 1 ? 0 : (S <= 6 ? 1 : (S <= 8 ? 2 : 3)))

int _inline IRLAP_RAND(int Min, int Max)
{
#ifdef UNDER_CE
    return ((GetTickCount() % (Max+1-Min)) + Min);
#else
    LARGE_INTEGER   li;

    KeQueryTickCount(&li);
    
    return ((li.LowPart % (Max+1-Min)) + Min);
#endif // !UNDER_CE
}

// Backoff time is a random time between 0.5 and 1.5 times the time to
// send a SNRM. _SNRM_TIME() is actually half (1000ms/2) time to send
// SNRM_LEN of characters at 9600 (9600/10 bits per char).
#define _SNRM_LEN               32
#define _SNRM_TIME()            (_SNRM_LEN*500/960)
#define IRLAP_BACKOFF_TIME()    IRLAP_RAND(_SNRM_TIME(), 3*_SNRM_TIME())

#define QOS_PI_BAUD        0x01
#define QOS_PI_MAX_TAT     0x82
#define QOS_PI_DATA_SZ     0x83
#define QOS_PI_WIN_SZ      0x84
#define QOS_PI_BOFS        0x85
#define QOS_PI_MIN_TAT     0x86
#define QOS_PI_DISC_THRESH 0x08


#define IRLAP_I_FRAME         		0x00
#define IRLAP_S_FRAME         		0x01
#define IRLAP_U_FRAME         		0x03

// Unnumbered Frame types with P/F bit set to 0
#define IRLAP_UI             0x03
#define IRLAP_XID_CMD        0x2f
#define IRLAP_TEST           0xe3
#define IRLAP_SNRM           0x83
#define IRLAP_RNRM			 0x83
#define IRLAP_DISC           0x43
#define IRLAP_RD			 0x43
#define IRLAP_UA             0x63
#define IRLAP_FRMR           0x87
#define IRLAP_DM             0x0f
#define IRLAP_XID_RSP        0xaf

// Supervisory Frames
#define IRLAP_RR             0x01
#define IRLAP_RNR            0x05
#define IRLAP_REJ            0x09
#define IRLAP_SREJ           0x0d

#define _MAKE_ADDR(Addr, CRBit)		 ((Addr << 1) + (CRBit & 1))
#define _MAKE_UCNTL(Cntl, PFBit)	 (Cntl + ((PFBit & 1)<< 4))
#define _MAKE_SCNTL(Cntl, PFBit, Nr) (Cntl + ((PFBit & 1)<< 4) + (Nr <<5))

#define IRLAP_CB_SIG    0x7f2a364bUL

// IRLAP Control Block
typedef struct
{
    IRDA_MSG        *pMsg[IRLAP_MOD];
    UINT            Start;
    UINT            End;
#ifdef TEMPERAMENTAL_SERIAL_DRIVER
    int             FCS[IRLAP_MOD];
#endif    
} IRLAP_WINDOW;

typedef enum
{
    PRIMARY,
    SECONDARY
} IRLAP_STN_TYPE;

typedef enum // KEEP IN SYNC with IRLAP_StateStr in irlaplog.c
{
    NDM,                // Normal Disconnect Mode
    DSCV_MEDIA_SENSE,   // Discovery Media Sense (Before Discovery)
    DSCV_QUERY,         // Discovery Query (Discovery initiated)
    DSCV_REPLY,         // Discovery Reply (Received DSCV XID cmd from Remote)
    CONN_MEDIA_SENSE,   // Connect Media Sense (Before connection estab)
    SNRM_SENT,          // SNRM sent - waiting for UA or DM from Remote
    BACKOFF_WAIT,       // Waiting random backoff before sending next SNRM
    SNRM_RECEIVED,      // SNRM rcvd - waiting for response from upper layer
    P_XMIT,             // Primary transmit
    P_RECV,             // Primary receive
    P_DISCONNECT_PEND,  // Upper layer request disconnect while in P_RECV
    P_CLOSE,            // Sent DISC, waiting for response
    S_NRM,              // Secondary Normal Response Mode XMIT/RECV
    S_DISCONNECT_PEND,  // Upper layer request disconnect while in S_NRM
    S_ERROR,            // Waiting for PF bit then send a FRMR
    S_CLOSE,            // Requested disconnect (RD) waiting for DISC command
} IRLAP_STATE;

typedef struct IrlapControlBlock
{
  IRLAP_STATE       State;
  IRDA_DEVICE       LocalDevice;
  IRDA_DEVICE       RemoteDevice;
  PIRDA_LINK_CB     pIrdaLinkCb;
  IRDA_QOS_PARMS    LocalQos;      // QOS from LMP
  IRDA_QOS_PARMS    RemoteQos;     // QOS of remote taken from SNRM/UA
  IRDA_QOS_PARMS    NegotiatedQos; // Union of remote and local QOS
  int               Baud;          // Type 0 negotiation parm
  int               DisconnectTime;// Type 0 negotiation parm
  int               ThresholdTime; // Type 0 negotiotion parm
  int               LocalMaxTAT;   // Type 1 negotiation parm
  int               LocalDataSize; // Type 1 negotiation parm
  int               LocalWinSize;  // Type 1 negotiation parm
  int               LocalNumBOFS;  // Type 1 negotiation parm
  int               RemoteMaxTAT;  // Type 1 negotiation parm
  int               RemoteDataSize;// Type 1 negotiation parm
  int               RemoteWinSize; // Type 1 negotiation parm
  int               RemoteNumBOFS; // Type 1 negotiation parm
  int               RemoteMinTAT;  // Type 1 negotiation parm
  IRLAP_STN_TYPE    StationType;   // PRIMARY or SECONDARY
  int               ConnAddr;      // Connection Address
  int               SNRMConnAddr;  // Connection address contained in SNRM
                                  // save it until get CONNECT_RESP
  int               CRBit;         // Primary = 1, Secondary = 0
  int               RespSlot;      // Secondary. Slot to respond in
  int               SlotCnt;       // Primary. Current slot number
  int               MaxSlot;       // Maximum slots to send in Dscv
  int               RemoteMaxSlot; // Number of Dscv's remote will send
  LIST_ENTRY        DevList;       // Discovered device list
  UINT              Vs;            // send state variable
  UINT              Vr;            // receive state variable
  IRLAP_WINDOW      RxWin;         // Holds out of sequence rxd frames
  IRLAP_WINDOW      TxWin;         // Holds unacked txd frames
  IRDA_MSG_LIST     TxMsgList;     // DATA_REQ, UDATA_REQ queued here
  int               RetryCnt;      // Count of number of retrans of DSCV,SNRM
  int               N1;            // const# retries before sending status up
  int               N2;            // const# retries before disconnecting
  int               N3;            // const# of connection retries
  int               FastPollCount;
  IRDA_TIMER        SlotTimer;
  IRDA_TIMER        QueryTimer;
  IRDA_TIMER        PollTimer;
  IRDA_TIMER        FinalTimer;
  IRDA_TIMER        WDogTimer;
  IRDA_TIMER        BackoffTimer;
  int               WDogExpCnt;    // Count of WDog expirations
  int               StatusSent;    // Status ind has been sent
  IRLAP_FRMR_FORMAT Frmr;
  BOOLEAN           GenNewAddr;    // Flag indicating whether to set new addr
  BOOLEAN           DscvRespSent;  // Secondary. Sent XID Discv response
  BOOLEAN           RemoteBusy;    // Remote has sent a RNR
  BOOLEAN           LocalBusy;     // Local busy condition, we sent RNR
  BOOLEAN           ClrLocalBusy;  // Send RR 
  BOOLEAN           LocalDiscReq;  // why 2ndary got DISC
  BOOLEAN           ConnAfterClose;// Conn req while in p_close
  BOOLEAN           DscvAfterClose;// Dscv_req while in p_close
  BOOLEAN           NoResponse;    // Final/WD timer exp'd, used with RetryCnt
} IRLAP_CB, *PIRLAP_CB;

#define LINE_CAPACITY(icb)     (icb->RemoteWinSize * \
                               (icb->RemoteDataSize + \
                                6+icb->RemoteNumBOFS))


UCHAR *BuildNegParms(UCHAR *pBuf, IRDA_QOS_PARMS *pQos);

void StoreULAddr(UCHAR Addr[], ULONG ULAddr);

UCHAR *Format_SNRM(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, 
				  UCHAR SAddr[], UCHAR DAddr[], int CAddr, 
				  IRDA_QOS_PARMS *pQos);

UCHAR *Format_DISC(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit);

UCHAR *Format_UI(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit);

UCHAR *Format_DscvXID(IRDA_MSG *pMsg, int ConnAddr, int CRBit, int PFBit, 
					 IRLAP_XID_DSCV_FORMAT *pXidFormat, CHAR DscvInfo[], 
                     int Len);

UCHAR *Format_TEST(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, 
				  UCHAR SAddr[], UCHAR DAddr[]);

UCHAR *Format_RNRM(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit);

UCHAR *Format_UA(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, 
				UCHAR SAddr[], UCHAR DAddr[], IRDA_QOS_PARMS *pQos);

UCHAR *Format_FRMR(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, 
				  IRLAP_FRMR_FORMAT *pFormat);

UCHAR *Format_DM(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit);

UCHAR *Format_RD(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit);

UCHAR *Format_RR(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, int Nr);

UCHAR *Format_RNR(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, int Nr);

UCHAR *Format_REJ(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, int Nr);

UCHAR *Format_SREJ(IRDA_MSG *pMsg, int Addr, int CRBit, int PFBit, int Nr);

UCHAR * Format_I(IRDA_MSG *pMsg, int Addr, int CRBit, 
				int PFBit, int Nr, int Ns);

int GetMyDevAddr(BOOLEAN New);

#ifndef UNDER_CE
#define DBG_FRAME_LOG_INIT()    (0)
#if DBG

CHAR *DecodeIRDA(int  *pFrameType,// returned frame type (-1=bad frame)
                UCHAR *pFrameBuf, // pointer to buffer containing IRLAP frame
                UINT FrameLen,   // length of buffer 
                CHAR *pOutStr,  // string where decoded packet is placed   
                UINT DecodeLayer,// 1,LAP only, 2 LAP/LMP, 3, LAP/LMP/TTP
                int fNoConnAddr,// TRUE->Don't show connection address in str
                int  DispMode 
);

_inline
VOID
DBG_FRAME(int Dir, UCHAR *pFrame, UINT Len)
{    
    if ((DbgSettings & DBG_TXFRAME) == Dir ||
        (DbgSettings & DBG_RXFRAME) == Dir)
    {
        UCHAR StrBuf[256];
        int FrameType;
          
        DecodeIRDA(&FrameType,
                   pFrame, Len,
                   StrBuf,
                   0,
                   TRUE,
                   1);
        DEBUGMSG(1, ("%s %s\n", Dir == DBG_TXFRAME?"Tx:":"Rx:", 
            (char*)StrBuf));
    }
}                   
#else
#define DBG_FRAME(d,p,l) (0)
#endif 
#else // !UNDER_CE
#ifdef DEBUG

#ifdef LOGDEV

#include <logdev.h>

extern int DbgSettings;

__inline 
VOID DBG_FRAME_LOG_INIT()
{
    LogInit();
}

_inline
VOID
DBG_FRAME(
    PIRDA_LINK_CB pIrdaLinkCb,
    int Dir,
    UCHAR *pFrame,
    UINT HdrLen,
    UINT DataLen)
{
    if ((DbgSettings & DBG_TXFRAME) == Dir ||
        (DbgSettings & DBG_RXFRAME) == Dir || Dir == 0)
    {
        LogData(
            (USHORT)LOG_TYPE_IRDA,
            Dir == DBG_RXFRAME ? LOG_SUBTYPE_IRLAP_RX : LOG_SUBTYPE_IRLAP_TX,
            LOG_DTYPE_IR_DATA,
            HdrLen + DataLen,
            (PBYTE)pFrame);
    }
}

#define IRLAP_LOGMSG(msg_in_parens) IrdaLogMsg msg_in_parens

__inline VOID IrlapLogMsg(
    CHAR *lpszFmt, ...
    )
{
    va_list ArgList;
    CHAR szBuf[1024];

    va_start(ArgList, lpszFmt);

    vsprintf(szBuf, lpszFmt, ArgList);

    LogData(
        LOG_TYPE_IRDA,
        LOG_SUBTYPE_IRLAP_MSG,
        LOG_DTYPE_ASCII,
        strlen(szBuf) + 1, (PBYTE)szBuf);

    va_end(ArgList);
}

#define IRLAP_LOG_NDIS_PACKET(d, p) (0)

#else // LOGDEV

#include "decdirda.h"

extern int DbgSettings;

_inline
VOID
DBG_FRAME(
    PIRDA_LINK_CB pIrdaLinkCb,
    int Dir,
    UCHAR *pFrame,
    UINT HdrLen,
    UINT DataLen)
{
    if ((DbgSettings & DBG_TXFRAME) == Dir ||
        (DbgSettings & DBG_RXFRAME) == Dir || Dir == 0)
    {
        TCHAR StrBuf[256];
        int FrameType;

        DecodeIRDA(&FrameType,
                   pFrame,
                   HdrLen,
                   StrBuf,
                   0,
                   TRUE,
                   1);

        DEBUGMSG(1, (TEXT("%hs %s len:%d\n"), 
                 Dir == DBG_TXFRAME ? "Tx:" : Dir == DBG_RXFRAME ? "Rx:" : "R2",
                 (char*)StrBuf,
                 DataLen));
    }
}

#define DBG_FRAME_LOG_INIT()        (0)
#define IRLAP_LOG_NDIS_PACKET(d, p) (0)
#define IRLAP_LOGMSG(msg_in_parens) (0)

#endif // !LOGDEV

#else // DEBUG

#define DBG_FRAME_LOG_INIT()            (0)
#define DBG_FRAME(link, d, p, h, len)   (0)
#define IRLAP_LOG_NDIS_PACKET(d, p)     (0)
#define IRLAP_LOGMSG(msg_in_parens)     (0)

#endif // !DEBUG

#endif // UNDER_CE
