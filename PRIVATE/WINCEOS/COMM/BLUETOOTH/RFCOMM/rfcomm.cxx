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
//------------------------------------------------------------------------------
// 
//      Bluetooth RFCOMM Layer
// 
// 
// Module Name:
// 
//      rfcomm.cxx
// 
// Abstract:
// 
//      This file implements Bluetooth RFCOMM Layer
// 
// 
//------------------------------------------------------------------------------
#include <windows.h>
#include <intsafe.h>
#include <svsutil.hxx>

#if ! defined (UNDER_CE)
#include <stddef.h>
#include <malloc.h>
#else
#include <stdlib.h>
#endif

#include <bt_debug.h>
#include <bt_os.h>
#include <bt_buffer.h>
#include <bt_hcip.h>
#include <bt_ddi.h>

#define RFCOMM_T1       60
#define RFCOMM_T2       60
#define RFCOMM_T3       5
#define RFCOMM_N1       127

#define RFCOMM_MAX_CH   31

#define RFCOMM_SCALE    10

//    link stages (phys & log)
#define    DORMANT          0x00
#define CONNECTING          0x01
#define CONNECTED           0x02
#define CONFIG_REQ_DONE     0x04    // L2CAP Only
#define CONFIG_IND_DONE     0x08    // L2CAP Only
#define L2CAPUP             0x0e    // L2CAP Only
#define SABM0UA             0x10    // L2CAP Only
#define UP                  0x1e

#define CALL_L2CAP_LINK_SETUP       0x01
#define CALL_L2CAP_LINK_CONFIG      0x02
#define CALL_RFCOMM_SABM0           0x03
#define CALL_RFCOMM_SABMN           0x04
#define CALL_RFCOMM_DISCN           0x05
#define CALL_RFCOMM_USERDATA        0x06
#define CALL_RFCOMM_SIGNALDATA      0x07

#define RFCOMM_SIGNAL_ASYNC         0x1
#define RFCOMM_SIGNAL_RESPONSE      0x2
#define RFCOMM_SIGNAL_NONE          0x3


//
//    Protocol settings are loosely derived from Whistler RFCOMM protocol.h (by stana)
//
struct TS_FRAME_HEADER {
    unsigned char   ucAddress;
    unsigned char   ucControl;
    unsigned char   ucLength;
};

struct TS_FRAME_HEADER_X {
    unsigned char   ucAddress;
    unsigned char   ucControl;
    unsigned char   ucLength;
    unsigned char   ucExtLength;
    unsigned char   ucCredits;
};

struct TS_FRAME_FOOTER {
    unsigned char   ucFcs;
};

struct TS_FRAME {
    TS_FRAME_HEADER    h;
    TS_FRAME_FOOTER    f;
};

#define EA_BIT              0x01
#define CR_BIT              0x02
#define DLCI_DIRECTION_BIT  0x04

#define CHANNEL_MASK        0x1f
#define DLCI_MASK           0x2f

#define DLCI_FROM_CHANNEL(c, i)  ((((c)&CHANNEL_MASK)<<1) + ((i) ? 0 : 1))
#define DLCI_FROM_ADDRESS(a)     (((a)>>2)&DLCI_MASK)
#define CHANNEL_FROM_ADDRESS(a)  (((a)>>3)&CHANNEL_MASK)


#define CTL_SABM    0x2f
#define CTL_UA      0x63
#define CTL_DM      0x0f
#define CTL_DISC    0x43
#define CTL_UIH     0xef

#define CTL_PF      0x10

inline int TS_LENGTH(unsigned char *p) {
    return (p[0] & EA_BIT ? p[0] : p[0] | (p[1] << 8)) >> 1;
}

#define TS_SIZEOFLENGTH(p)  (((*(unsigned char *)(p))&EA_BIT) ? 1 : 2)
#define TS_SIZEOFHEADER(h)  (sizeof(TS_FRAME_HEADER)+TS_SIZEOFLENGTH(&(h)->Length))

// TEST is an RFCOMM ping.  N bytes of data are sent, and must be echoed back
// exactly to complete the test.
#pragma warning (push)
#pragma warning (disable : 4200)
struct MUX_MSG_TEST {
    unsigned char       Command;
    unsigned char       Length;
    unsigned char       Data[0];
};
#pragma warning (pop)
#define MSG_TEST    0x08

// Turn on aggregate flow control
struct MUX_MSG_FCON {
    unsigned char       Command;
    unsigned char       Length;                 // always 0
};
#define MSG_FCON    0x28

// Turn off aggregate flow control
struct MUX_MSG_FCOFF {
    unsigned char       Command;
    unsigned char       Length;                 // always 0
};
#define MSG_FCOFF   0x18

// Modem status command
struct MUX_MSG_MSC {
    unsigned char       Command;
    unsigned char       Length;                 // 2 or 3
    unsigned char       Address;
    unsigned char       Signals;
    unsigned char       Break;
};
#define MSG_MSC     0x38

#define MSC_EA_BIT      EA_BIT
#define MSC_FC_BIT      BIT(1)      // Flow control, clear if we can receive
#define MSC_RTC_BIT     BIT(2)      // Ready to communicate, set when ready
#define MSC_RTR_BIT     BIT(3)      // Ready to receive, set when ready
#define MSC_IC_BIT      BIT(6)      // Incoming call
#define MSC_DV_BIT      BIT(7)      // Data valid

#define MSC_BREAK_BIT   BIT(1)      // Set if sending break
#define MSC_SET_BREAK_LENGTH(b, l)  ((b) = ((b)&0x3) | (((l)&0xf) << 4))

/* Alignment & packing
struct MUX_MSG_RPN {
    unsigned char       Command;
    unsigned char       Length;                 // 1 or 8
    unsigned char       Address;
    unsigned    Baud                : 8;

    unsigned    DataBits            : 2;
    unsigned    StopBits            : 1;
    unsigned    ParityEnabled       : 1;
    unsigned    ParityType          : 2;
    unsigned    Reserved0           : 2;

    unsigned    XonXoffInput        : 1;
    unsigned    XonXoffOutput       : 1;
    unsigned    RTRInput            : 1;
    unsigned    RTROutput           : 1;
    unsigned    RTCInput            : 1;
    unsigned    RTCOutput           : 1;
    unsigned    Reserved1           : 2;

    unsigned    XonChar             : 8;
    unsigned    XoffChar            : 8;
    union {
        struct {
            unsigned    Baud            : 1;
            unsigned    Data            : 1;
            unsigned    Stop            : 1;
            unsigned    Parity          : 1;
            unsigned    ParityType      : 1;
            unsigned    XonChar         : 1;
            unsigned    XofChar         : 1;
            unsigned    Reserved        : 1;

            unsigned    XonXoffInput    : 1;
            unsigned    XonXoffOutput   : 1;
            unsigned    RTRInput        : 1;
            unsigned    RTROutput       : 1;
            unsigned    RTCInput        : 1;
            unsigned    RTCOutput       : 1;
            unsigned    Reserved1       : 2;
        };
        USHORT          Mask;
    } Change;
}; */

#define MSG_RPN     0x24

#define RPN_BAUD_2400       0
#define RPN_BAUD_4800       1
#define RPN_BAUD_7200       2
#define RPN_BAUD_9600       3
#define RPN_BAUD_19200      4
#define RPN_BAUD_38400      5
#define RPN_BAUD_57600      6
#define RPN_BAUD_115200     7
#define RPN_BAUD_230400     8

#define RPN_DATA_5          0
#define RPN_DATA_6          1
#define RPN_DATA_7          2
#define RPN_DATA_8          3

#define RPN_PARITY_ODD      0
#define RPN_PARITY_EVEN     1
#define RPN_PARITY_MARK     2
#define RPN_PARITY_SPACE    3

// Remote line status
/* Alignment & packing
struct MUX_MSG_RLS {
    unsigned char       Command;
    unsigned char       Length;                     // Always 2
    unsigned char       Address;
    struct  {
        unsigned    Error           : 1;    // An error has occurred

        unsigned    OverrunError    : 1;    //
        unsigned    ParityError     : 1;    // Type of error
        unsigned    FramingError    : 1;    //
    } LineStatus;
}; */
#define MSG_RLS     0x14

// Parameter negotiation
/* Alignment & packing
struct MUX_MSG_PN {
    unsigned char       Command;
    unsigned char       Length;
    unsigned    IType               : 4;    // Always 0, (UIH frames)
    unsigned    ConvergenceLayer    : 4;
    unsigned    Priority            : 6;
    unsigned    Fill0               : 2;
    unsigned    AckTimer            : 8;    // Hundredths of a second
    unsigned    MaxFrameSize        : 16;
    unsigned    MaxRetransmissions  : 8;
    unsigned    WindowSize          : 3;
    unsigned    Fill1               : 5;
}; */
#define MSG_PN  0x20

struct MUX_MSG_NSC {
    unsigned char       Command;
    unsigned char       Length;                     // Always 1
    unsigned char       CommandNotSupported;
};
#define MSG_NSC     0x04

#define MSG_PSC     0x10
#define MSG_CLD     0x30
#define MSG_SNC     0x34

struct DLCI;
struct Task;
struct Session;
struct RFCOMM_CONTEXT;

struct DLCI {
    DLCI            *pNext;             // Next channel

    Session         *pSess;             // Physical session

    RFCOMM_CONTEXT  *pOwner;            // Owner context
    unsigned int    channel    : 5;     // server channel id
    unsigned int    fLocal     : 1;     // Channel is local?
    unsigned int    fStage     : 8;     // Lifestage of a channel
    unsigned int    fFCA       : 1;     // 1 = sent FC signal already

    DLCI (Session *a_pSess, unsigned char a_ch, int a_fLocal, RFCOMM_CONTEXT *a_pOwner) {
        memset (this, 0, sizeof(*this));

        pSess  = a_pSess;
        pOwner = a_pOwner;
        channel= a_ch;
        fLocal = a_fLocal ? TRUE : FALSE;
    }

    void *operator new (size_t iSize);
    void operator delete (void *ptr);
};

struct Task {
    Task            *pNext;         // Next task
    RFCOMM_CONTEXT  *pOwner;        // Owner context
    void            *pContext;      // Call context

    SVSHandle       hCallContext;   // Unique handle for async call lookup

    unsigned int    fWhat               : 8;    // Call type
    unsigned int    channel             : 5;    // Channel server id
    unsigned int    fLocal              : 1;    // Channel is local
    unsigned int    fPF                 : 1;    // Does this block the line?
    unsigned int    fData               : 1;    // Is this data or signal
    unsigned int    fUnused             : 2;    // pad
    unsigned int    eType               : 6;    // Signal type

    unsigned char   signal_length       : 4;    // Signal packet waiting for session to open
    unsigned char   signal_dlci_offset  : 4;    // offset to be filled with DLCI

    unsigned char   signal_body[10];

    Session         *pPhysLink;

    Task (int a_fWhat, RFCOMM_CONTEXT *a_pOwner, Session *a_pSess) {
        memset (this, 0, sizeof(*this));

        pOwner       = a_pOwner;
        fWhat        = a_fWhat;
        pPhysLink    = a_pSess;
        hCallContext = SVSUTIL_HANDLE_INVALID;

        if ((fWhat == CALL_RFCOMM_USERDATA) || (fWhat == CALL_RFCOMM_SIGNALDATA))
            fData = TRUE;
    }

    Task (RFCOMM_CONTEXT *a_pOwner, Session *a_pSess, void *a_pContext, unsigned char a_cType) {
        memset (this, 0, sizeof(*this));

        pOwner       = a_pOwner;
        pContext     = a_pContext;
        fWhat        = CALL_RFCOMM_SIGNALDATA;
        fData        = TRUE;
        eType        = a_cType;
        pPhysLink    = a_pSess;
        hCallContext = SVSUTIL_HANDLE_INVALID;
    }

    void *operator new (size_t iSize);
    void operator delete (void *ptr);
};


struct Session {
    Session         *pNext;         // Next session
    DLCI            *pLogLinks;     // List of logical channels

    BD_ADDR         b;              // Bluetooth address
    unsigned short  cid;            // L2CAP cid

    unsigned int    fStage    : 8;  // Lifestage
    unsigned int    fIncoming : 1;  // DLCI DIRECTION BIT = ! fIncoming
    unsigned int    fBusy     : 1;  // Has PF command outstanding
    unsigned int    fWaitAck  : 1;  // PF command was removed, but UA/DM is needed

    unsigned short  inMTU;          // L2CAP MTU in
    unsigned short  outMTU;         // L2CAP MTU out

    SVSCookie       kTimeoutCookie; // Timer handle

    int             iErr;           // Number of physical errors

    Session (BD_ADDR *pba) {
        memset (this, 0, sizeof(*this));

        b       = *pba;
        fStage  = DORMANT;
    }

    void *operator new (size_t iSize);
    void operator delete (void *ptr);

#if defined (DEBUG) || defined (_DEBUG)
    unsigned char   ucDbgBusyChannel;
#endif
};

struct ServerChannel {
    ServerChannel   *pNext;
    unsigned int    channel : 5;

    void *operator new (size_t iSize);
    void operator delete (void *ptr);
};

struct RFCOMM_CONTEXT : public SVSAllocClass, public SVSRefObj {
    RFCOMM_CONTEXT            *pNext;    // Next context

    RFCOMM_EVENT_INDICATION    ei;        // Event indicators vtable
    RFCOMM_CALLBACKS        c;        // Callbacks vtable

    void                    *pUserContext;    // User context

    ServerChannel            *pReservedChannels;

    unsigned int            fDefaultServer;    // Channel to restrict incoming connections to.

    RFCOMM_CONTEXT (void) {
        pNext = NULL;
        memset (&ei, 0, sizeof(ei));
        memset (&c, 0, sizeof(c));

        pUserContext      = NULL;
        pReservedChannels = NULL;
        fDefaultServer    = FALSE;
    }

};

class RFCOMM : public SVSSynch, public SVSRefObj, public SVSAllocClass {
public:
    RFCOMM_CONTEXT  *pContexts;     // List of contexts
    Session         *pPhysLinks;    // List of sessions
    Task            *pCalls;        // List of calls

    FixedMemDescr   *pfmdSessions;  // FMD for sessions
    FixedMemDescr   *pfmdChannels;  // FMD for channels
    FixedMemDescr   *pfmdCalls;     // FMD for calls
    FixedMemDescr   *pfmdSC;        // FMD for server channels

    HANDLE          hL2CAP;         // L2CAP handle
    L2CAP_INTERFACE l2cap_if;       // L2CAP interface table

    int             cDataHeaders;   // Size of headers at the lower layer
    int             cDataTrailers;  // Size of headers at the upper layer

    DWORD           dwLinger;       // Lingering timeout
    DWORD           dwDiscT1;       // T1 timeout for DISC command
    DWORD           dwSabmT1;       // T1 timeout for SABM command

    unsigned int    fRunning : 1;   // Running?
    unsigned int    fConnected : 1; // Device connected?

    void ReInit (void) {
        pContexts    = NULL;
        pPhysLinks   = NULL;
        pCalls       = NULL;

        pfmdSessions = NULL;
        pfmdChannels = NULL;
        pfmdCalls    = NULL;
        pfmdSC       = NULL;

        hL2CAP       = NULL;
        memset (&l2cap_if, 0, sizeof(l2cap_if));
        cDataHeaders = 0;
        cDataTrailers = 0;

        dwLinger = RFCOMM_LINGER;
        dwDiscT1 = RFCOMM_T1;
        dwSabmT1 = RFCOMM_T1;

        fRunning = FALSE;
        fConnected = FALSE;
    }

    RFCOMM (void) {
        ReInit ();
    }
};

static RFCOMM    *gpRFCOMM    = NULL;

// Adapted from the ETSI specification:
// (GSM 07.10 version 6.3.0 Release 1997)

static const unsigned char crctable[256] = { //reversed, 8-bit, poly=0x07
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75, 0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B,
    0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69, 0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67,
    0x38, 0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D, 0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43,
    0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51, 0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F,
    0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05, 0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B,
    0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19, 0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17,
    0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D, 0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0, 0xA2, 0x33,
    0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21, 0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F,
    0xE0, 0x71, 0x03, 0x92, 0xE7, 0x76, 0x04, 0x95, 0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B,
    0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89, 0xF2, 0x63, 0x11, 0x80, 0xF5, 0x64, 0x16, 0x87,
    0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD, 0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
    0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1, 0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF,
    0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5, 0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB,
    0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9, 0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7,
    0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C, 0xDD, 0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3,
    0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1, 0xBA, 0x2B, 0x59, 0xC8, 0xBD, 0x2C, 0x5E, 0xCF
};

static unsigned char FCSCompute(unsigned char *p, int len)
{
    /*Init*/
    unsigned char fcs = 0xFF;

    /*len is the number of bytes in the message, p points to message*/
    while (len--) {
        fcs = crctable[fcs^*p++];
    }
    /*Ones complement*/
    fcs = 0xFF - fcs;

    return fcs;
}

// Returns TRUE if indicated FCS is valid
static int FCSValidate(unsigned char *p, int len, unsigned char ReceivedFCS)
{
    /*Init*/
    unsigned char fcs = 0xFF;
    /*len is the number of bytes in the message, p points to message*/
    while (len--)
        fcs = crctable[fcs^*p++];

    /*Ones complement*/
    fcs = crctable[fcs^ReceivedFCS];
    /*0xCF is the reversed order of 11110011.*/

    return fcs == 0xCF;
}

static RFCOMM_CONTEXT *VerifyOwner (RFCOMM_CONTEXT *pOwner) {
    RFCOMM_CONTEXT *pCtx = gpRFCOMM->pContexts;
    while (pCtx && (pCtx != pOwner))
        pCtx = pCtx->pNext;

    return pCtx;
}

static Task *VerifyCall (Task *pCall) {
    Task *pC = gpRFCOMM->pCalls;
    while (pC && (pC != pCall))
        pC = pC->pNext;

    return pC;
}

static Session *VerifyLink (Session *pSess) {
    Session *pS = gpRFCOMM->pPhysLinks;
    while (pS && (pS != pSess))
        pS = pS->pNext;

    return pS;
}

static DLCI *VerifyChannel (Session *pSess, DLCI *pChann) {
    DLCI *p = pSess->pLogLinks;
    while (p && (p != pChann))
        p = p->pNext;

    return p;
}

static RFCOMM_CONTEXT *VerifyContext (RFCOMM_CONTEXT *pContext) {
    RFCOMM_CONTEXT *pC = gpRFCOMM->pContexts;

    while (pC && (pC != pContext))
        pC = pC->pNext;

    return pC;
}

static RFCOMM_CONTEXT *FindContextByChannel (unsigned char ch) {
    RFCOMM_CONTEXT *pContext = gpRFCOMM->pContexts;
    while (pContext) {
        ServerChannel *pSC = pContext->pReservedChannels;
        while (pSC) {
            if (pSC->channel == ch)
                return pContext;
            pSC = pSC->pNext;
        }

        pContext = pContext->pNext;
    }

    return NULL;
}

static RFCOMM_CONTEXT *FindContextForDefault (void) {
    RFCOMM_CONTEXT *pContext = gpRFCOMM->pContexts;
    while (pContext) {
        if (pContext->fDefaultServer)
            return pContext;

        pContext = pContext->pNext;
    }

    return NULL;
}

static DLCI *HandleToDLCI (HANDLE hConnection, RFCOMM_CONTEXT *pOwner) {
    if (! hConnection)
        return NULL;

    DLCI *pDLCI = (DLCI *)hConnection;
    Session *pSess = NULL;
    __try {
        pSess = pDLCI->pSess;
    } __except (1) {
        pSess = NULL;
    }

    if (! pSess)
        return NULL;

    Session *pS = gpRFCOMM->pPhysLinks;
    while (pS && (pS != pSess))
        pS = pS->pNext;

    if (! pS)
        return NULL;

    DLCI *pD = pS->pLogLinks;

    while (pD && ((pD != pDLCI) || (pD->pOwner != pOwner)))
        pD = pD->pNext;

    return pD;
}

static Task *NewTask (int fWhat, RFCOMM_CONTEXT *pOwner, Session *pSess) { 
    Task* pTask = new Task (fWhat, pOwner, pSess);

    if (pTask) {
        pTask->hCallContext = btutil_AllocHandle ((LPVOID)pTask);
        if (SVSUTIL_HANDLE_INVALID == pTask->hCallContext) {            
            delete pTask;
            return NULL;
        }
    }
    
    return pTask; 
}

static Task *NewTask (RFCOMM_CONTEXT *a_pOwner, Session *a_pSess, void *a_pContext, unsigned char a_cType) {
    Task* pTask = new Task (a_pOwner, a_pSess, a_pContext, a_cType);

    if (pTask) {
        pTask->hCallContext = btutil_AllocHandle ((LPVOID)pTask);
        if (SVSUTIL_HANDLE_INVALID == pTask->hCallContext) {            
            delete pTask;
            return NULL;
        }
    }

    return pTask;
}

static Session *NewSession (BD_ADDR *pba) { return new Session (pba); }
static DLCI *NewLog (Session *pSess, unsigned char channel, unsigned int fLocal, RFCOMM_CONTEXT *pOwner) { return new DLCI (pSess, channel, fLocal, pOwner); }

static void AddTask (Task *pTask) {
    SVSUTIL_ASSERT (pTask->pNext == NULL);

    if (! gpRFCOMM->pCalls)
        gpRFCOMM->pCalls = pTask;
    else {
        Task *pParent = gpRFCOMM->pCalls;
        while (pParent->pNext)
            pParent = pParent->pNext;

        pParent->pNext = pTask;
    }
}

static void DeleteCall (Task *pTask) {
    if (pTask == gpRFCOMM->pCalls)
        gpRFCOMM->pCalls = gpRFCOMM->pCalls->pNext;
    else {
        Task *pP = gpRFCOMM->pCalls;
        while (pP && (pP->pNext != pTask))
            pP = pP->pNext;

        if (pP)
            pP->pNext = pP->pNext->pNext;
        else
            pTask = NULL;
    }

    if (pTask) {
        if (SVSUTIL_HANDLE_INVALID != pTask->hCallContext) {
            btutil_CloseHandle (pTask->hCallContext);
            pTask->hCallContext = SVSUTIL_HANDLE_INVALID;
        }
        
        delete pTask;
    }

    return;
}

static void CancelCall (Task *pCall, int iError, RFCOMM_CONTEXT *pExOwner) {
    void *pUserContext = pCall->pContext;
    RFCOMM_CONTEXT *pOwner = pCall->pOwner;

    SVSUTIL_ASSERT ((! pOwner) || VerifyOwner (pOwner));
    DeleteCall (pCall);

    if (! pOwner)
        pOwner = pExOwner;

    if (! pOwner)
        return;

    BT_LAYER_CALL_ABORTED pCallback = pOwner->c.rfcomm_CallAborted;

    pOwner->AddRef ();
    gpRFCOMM->Unlock ();

    IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"CancelCall : going into rfcomm_CallAborted\n"));

    __try {
        pCallback (pUserContext, iError);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] CancelCall :: exception in rfcomm_CallAborted\n"));
    }

    gpRFCOMM->Lock ();
    pOwner->DelRef ();

    IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"CancelCall : came out of rfcomm_CallAborted\n"));
}

static int SendFrame (Task *pTask, unsigned short cid, BD_BUFFER *pb) {
    IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"SendFrame :: cid = 0x%04x %d bytes\n", cid, pb->cEnd - pb->cStart));

    SVSUTIL_ASSERT (gpRFCOMM->fConnected);

    HANDLE h = gpRFCOMM->hL2CAP;
    SVSHandle hContext = NULL;
    L2CA_DataDown_In pCallback = gpRFCOMM->l2cap_if.l2ca_DataDown_In;

    if (pTask) {
        SVSUTIL_ASSERT (pTask->hCallContext != SVSUTIL_HANDLE_INVALID);        
        hContext = pTask->hCallContext;
    }

    int iRes = ERROR_INTERNAL_ERROR;
    gpRFCOMM->AddRef ();
    gpRFCOMM->Unlock ();

    IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"SendFrame : going into l2ca_DataDown_In\n"));

    __try {
        iRes = pCallback (h, (LPVOID)hContext, cid, pb);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] SendFrame :: exception in l2ca_DataDown_In!\n"));
    }
    gpRFCOMM->Lock ();
    gpRFCOMM->DelRef ();

    IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"SendFrame : came out of l2ca_DataDown_In\n"));
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"SendFrame returns %d\n", iRes));

    return iRes;
}

static int SendFrame (Task *pTask, unsigned short cid, int fIncoming, int cBytes, unsigned char *pBytes) {
#if defined (DEBUG) || defined (_DEBUG)
    WCHAR *szCMD = L"UNKN";
    WCHAR *szQual = L"";

    if (cBytes > 0) {
        if ((*pBytes & CR_BIT) == 0)
            szQual = L"-Resp";

        switch (*pBytes >> 2) {
        case MSG_TEST:
            szCMD = L"MSG_TEST";
            break;
        case MSG_FCON:
            szCMD = L"MSG_FCON";
            break;
        case MSG_FCOFF:
            szCMD = L"MSG_FCOFF";
            break;
        case MSG_MSC:
            szCMD = L"MSG_MSC";
            break;
        case MSG_RPN:
            szCMD = L"MSG_RPN";
            break;
        case MSG_RLS:
            szCMD = L"MSG_RLS";
            break;
        case MSG_PN:
            szCMD = L"MSG_PN";
            break;
        case MSG_NSC:
            szCMD = L"MSG_NSC";
            break;
        case MSG_PSC:
            szCMD = L"MSG_PSC";
            break;
        case MSG_CLD:
            szCMD = L"MSG_CLD";
            break;
        case MSG_SNC:
            szCMD = L"MSG_SNC";
            break;
        }
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"SendFrame <%s%s> :: cid = 0x%04x %d bytes\n", szCMD, szQual, cid, cBytes));
#endif

    SVSUTIL_ASSERT (cBytes <= 127);
    SVSUTIL_ASSERT (cBytes > 0);

    unsigned int cFrameSize;
    if (! SUCCEEDED(CeUIntAdd4(sizeof(TS_FRAME_HEADER) + sizeof(TS_FRAME_FOOTER),
        gpRFCOMM->cDataHeaders, gpRFCOMM->cDataTrailers, cBytes, &cFrameSize)))
        return ERROR_ARITHMETIC_OVERFLOW;
    
    BD_BUFFER *pBuffer = BufferAlloc (cFrameSize);
    if (! pBuffer)
        return ERROR_OUTOFMEMORY;

    pBuffer->cStart = gpRFCOMM->cDataHeaders;
    pBuffer->cEnd = pBuffer->cSize - gpRFCOMM->cDataTrailers - 1;
    TS_FRAME_HEADER *pHeader = (TS_FRAME_HEADER *)&pBuffer->pBuffer[pBuffer->cStart];
    pHeader->ucAddress = EA_BIT | (fIncoming ? 0 : CR_BIT) | 0;
    pHeader->ucControl = CTL_UIH;
    pHeader->ucLength = (unsigned char) (cBytes << 1);
    pHeader->ucLength |= EA_BIT;

    memcpy (pHeader + 1, pBytes, cBytes);

    pBuffer->pBuffer[pBuffer->cEnd++] = FCSCompute (pBuffer->pBuffer + pBuffer->cStart, 2);

    return SendFrame (pTask, cid, pBuffer);
}

static int SendFrame (Task *pTask, unsigned short cid, int dlci, int fIncoming, int cframe) {
    IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"SendFrame :: cid = 0x%04x channel = %d frame = %s\n", cid, dlci >> 1,
        cframe == CTL_DISC ? L"DISC" : (cframe == CTL_SABM ? L"SABM" : (cframe == CTL_UA ? L"UA" : L"DM"))));

    SVSUTIL_ASSERT ((cframe == CTL_DISC) || (cframe == CTL_SABM) || (cframe == CTL_UA) || (cframe == CTL_DM));

    unsigned int cFrameSize;
    if (! SUCCEEDED(CeUIntAdd3(sizeof(TS_FRAME), gpRFCOMM->cDataHeaders, gpRFCOMM->cDataTrailers, &cFrameSize)))
        return ERROR_ARITHMETIC_OVERFLOW;

    BD_BUFFER b;

    b.cSize     = cFrameSize;
    b.cStart    = gpRFCOMM->cDataHeaders;
    b.cEnd      = b.cStart + sizeof(TS_FRAME);
    b.fMustCopy = TRUE;
    b.pFree        = NULL;
    b.pBuffer   = (unsigned char *)_alloca (b.cSize);

    TS_FRAME *pDiscFrame = (TS_FRAME *)(b.pBuffer + b.cStart);
    pDiscFrame->h.ucAddress = EA_BIT | (dlci << 2);

    //    CR bit set for command responses for responder and for command requests for initiator
    if (fIncoming) {
        if ((cframe == CTL_UA) || (cframe == CTL_DM))
            pDiscFrame->h.ucAddress |= CR_BIT;
    } else {
        if ((cframe == CTL_DISC) || (cframe == CTL_SABM))    //command
            pDiscFrame->h.ucAddress |= CR_BIT;
    }

    pDiscFrame->h.ucControl = cframe | CTL_PF;
    pDiscFrame->h.ucLength  = EA_BIT;
    pDiscFrame->f.ucFcs = FCSCompute ((unsigned char *)&pDiscFrame->h, sizeof(pDiscFrame->h));

    return SendFrame (pTask, cid, &b);
}

static inline int RejectFrame (Session *pSess, TS_FRAME *pF) {
    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] Frame ctl 0x%02 addr 0x%02 sess %04x%08x rejected\n", pF->h.ucControl, pF->h.ucAddress, pSess->b.NAP, pSess->b.SAP));

    return SendFrame (NULL, pSess->cid, pF->h.ucAddress >> 2, pSess->fIncoming, CTL_DM);
}

static inline int AcceptFrame (Session *pSess, TS_FRAME *pF) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Frame ctl 0x%02 addr 0x%02 sess %04x%08x accepted\n", pF->h.ucControl, pF->h.ucAddress, pSess->b.NAP, pSess->b.SAP));

    return SendFrame (NULL, pSess->cid, pF->h.ucAddress >> 2, pSess->fIncoming, CTL_UA);
}

static void CloseSession (Session *pSess, int iError, BOOL fCancelCalls);

static DWORD WINAPI TimeoutSession (LPVOID pArg) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"TimeoutSession : 0x%08x\n", pArg));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"TimeoutSession : system shutdown\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpRFCOMM->Lock ();
    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"TimeoutSession : system disconnected\n"));
        gpRFCOMM->Unlock ();

        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Session *pSess = VerifyLink ((Session *)pArg);

    if (! pSess) {
        IFDBG(DebugOut (DEBUG_ERROR, L"TimeoutSession : not found\n"));
        gpRFCOMM->Unlock ();

        return ERROR_NOT_FOUND;
    }

    if (pSess->kTimeoutCookie) {
        // The session has been timed out.  To get into this state we did not recv a command/response
        // from the peer.  We won't bother retrying since the transport layer is reliable.  Let's clean up
        // all LogLinks and disconnect the L2CAP session.
        IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"TimeoutSession :: closing session %04x%08x\n", pSess->b.NAP, pSess->b.SAP));
        CloseSession (pSess, ERROR_TIMEOUT, TRUE);
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"TimeoutSession :: ERROR_SUCCESS\n"));
    gpRFCOMM->Unlock ();

    return ERROR_SUCCESS;
}

static void ScheduleTimeout (Session *pSess, int iTimeoutSec) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"ScheduleTimeout %d : %04x%08x\n", iTimeoutSec, pSess->b.NAP, pSess->b.SAP));

    if (pSess->kTimeoutCookie)
        btutil_UnScheduleEvent (pSess->kTimeoutCookie);

    pSess->kTimeoutCookie = btutil_ScheduleEvent (TimeoutSession, pSess, iTimeoutSec * 1000);
}

static void ClearTimeout (Session *pSess) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"ClearTimeout : %04x%08x\n", pSess->b.NAP, pSess->b.SAP));

    if (pSess->kTimeoutCookie)
        btutil_UnScheduleEvent (pSess->kTimeoutCookie);
    pSess->kTimeoutCookie = 0;
}

static void SetBusy (Session *pSess, Task *pCall, int iTimeoutSec) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"SetBusy : %04x%08x\n", pSess->b.NAP, pSess->b.SAP));
    SVSUTIL_ASSERT (gpRFCOMM->fConnected);
    SVSUTIL_ASSERT (pCall->pPhysLink == pSess);
    SVSUTIL_ASSERT (! pSess->fWaitAck);

#if defined (DEBUG) || defined (_DEBUG)
    Task *pCall2 = gpRFCOMM->pCalls;
    while (pCall2 && ((pCall2->pPhysLink != pSess) || (! pCall2->fPF)))
        pCall2 = pCall2->pNext;

    SVSUTIL_ASSERT (! pCall2);
#endif

    pSess->fBusy = TRUE;
    pCall->fPF = TRUE;

#if defined (DEBUG) || defined (_DEBUG)
    pSess->ucDbgBusyChannel = pCall->channel;
#endif

    ScheduleTimeout (pSess, iTimeoutSec);
}

static void ClearBusy (Session *pSess) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"ClearBusy : %04x%08x\n", pSess->b.NAP, pSess->b.SAP));
    SVSUTIL_ASSERT (gpRFCOMM->fConnected);

    pSess->fBusy = FALSE;

#if defined (DEBUG) || defined (_DEBUG)
    pSess->ucDbgBusyChannel = 0;
#endif

    ClearTimeout (pSess);
}

static void GetConnectionState (void) {
    gpRFCOMM->fConnected = FALSE;

    __try {
        int fConnected = FALSE;
        int dwRet = 0;
        gpRFCOMM->l2cap_if.l2ca_ioctl (gpRFCOMM->hL2CAP, BTH_STACK_IOCTL_GET_CONNECTED, 0, NULL, sizeof(fConnected), (char *)&fConnected, &dwRet);
        if ((dwRet == sizeof(fConnected)) && fConnected)
            gpRFCOMM->fConnected = TRUE;
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] GetConnectionState : exception in hci_ioctl BTH_STACK_IOCTL_GET_CONNECTED\n"));
    }
}
//
//    DIRECTION bit is set on device making the connection (fIncoming && (! fLocal)) or ((! fIncoming) && fLocal)
//

static inline unsigned char GetDLCI (Task *pCall) {
    if (pCall->channel == 0)
        return 0;

    return (pCall->channel << 1) | ((pCall->pPhysLink->fIncoming && (! pCall->fLocal)) || ((! pCall->pPhysLink->fIncoming) && pCall->fLocal));
}

static inline unsigned char GetDLCI (DLCI *pDLCI) {
    SVSUTIL_ASSERT (pDLCI->pSess->fStage != DORMANT);
    return (pDLCI->channel << 1) | ((pDLCI->pSess->fIncoming && (! pDLCI->fLocal)) || ((! pDLCI->pSess->fIncoming) && pDLCI->fLocal));
}

// If (fIncoming && (! DIRECTION)) || ((! fIncoming) && DIRECTION)
static inline int IsLocal (Session *pSess, unsigned char dlci) {
    SVSUTIL_ASSERT (pSess->fStage != DORMANT);

    return (pSess->fIncoming && ((dlci & 1) == 0)) || ((! pSess->fIncoming) && ((dlci & 1) == 1));
}

static int ProcessNextPendingEvent (Session *pSess) {
    SVSUTIL_ASSERT (VerifyLink (pSess));
    SVSUTIL_ASSERT (! pSess->fBusy);
    SVSUTIL_ASSERT (! pSess->fWaitAck);
    SVSUTIL_ASSERT (pSess->fStage == UP);

    Task *pCall = gpRFCOMM->pCalls;
    while (pCall && ((pCall->pPhysLink != pSess) || pCall->fData))
        pCall = pCall->pNext;

    if (! pCall)
        return ERROR_NOT_FOUND;

    SVSUTIL_ASSERT ((pCall->fWhat == CALL_RFCOMM_SABMN) || (pCall->fWhat == CALL_RFCOMM_DISCN));

    if (pCall->fWhat == CALL_RFCOMM_SABMN) {
        SetBusy (pSess, pCall, gpRFCOMM->dwSabmT1);        
        return SendFrame (pCall, pSess->cid, GetDLCI (pCall), pSess->fIncoming, CTL_SABM);
    } else if (pCall->fWhat == CALL_RFCOMM_DISCN) {
        SetBusy (pSess, pCall, gpRFCOMM->dwDiscT1);
        return SendFrame (pCall, pSess->cid, GetDLCI (pCall), pSess->fIncoming, CTL_DISC);
    }

    SVSUTIL_ASSERT (0);
    return ERROR_INTERNAL_ERROR;
}

static int ProcessPendingSignals (Session *pSess) {
    do {
        Task *pTask = gpRFCOMM->pCalls;
        while (pTask && ((pTask->pPhysLink != pSess) || (! pTask->signal_length)))
            pTask = pTask->pNext;

        if (! pTask)
            break;

        SVSUTIL_ASSERT (pTask->fWhat == CALL_RFCOMM_SIGNALDATA);
        SVSUTIL_ASSERT (pTask->eType == MSG_PN);

        // Verify if n1 has to be changed due to l2cap MTU negotiation
        unsigned short n1 = (unsigned short) (pTask->signal_body[6] | (pTask->signal_body[7] << 8));
        if (n1 > (pSess->outMTU - (sizeof(TS_FRAME_HEADER_X) + sizeof(TS_FRAME_FOOTER)))) {
            n1 = pSess->outMTU - (sizeof(TS_FRAME_HEADER_X) + sizeof(TS_FRAME_FOOTER));
            pTask->signal_body[6] = n1 & 0xff;                    // n1 1-8
            pTask->signal_body[7] = (n1 >> 8) & 0xff;            // n1 9-16
        }

        if (pTask->signal_dlci_offset)
            pTask->signal_body[pTask->signal_dlci_offset] = GetDLCI(pTask);

        pTask->signal_dlci_offset = 0;

        int cBytes = pTask->signal_length;
        pTask->signal_length = 0;

        int iRes = SendFrame (pTask, pSess->cid, pSess->fIncoming, cBytes, pTask->signal_body);
        if (iRes != ERROR_SUCCESS)
            return iRes;
    } while (gpRFCOMM->fConnected && VerifyLink (pSess));

    return ERROR_SUCCESS;
}

static int DisconnectPeer (Session *pSess, DLCI *pChan, void *pContext) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"DisconnectPeer : %04x%08x h = 0x%08x\n", pSess->b.NAP, pSess->b.SAP, pChan));
    if (gpRFCOMM->fConnected && (pSess->fStage == UP) && pChan->channel && (pChan->fStage == UP)) {
        Task *pCall = NewTask (CALL_RFCOMM_DISCN, pChan->pOwner, pSess);
        if (pCall) {
            pCall->channel     = pChan->channel;
            pCall->fLocal      = pChan->fLocal;
            pCall->pContext    = pContext;

            AddTask (pCall);

            if (! pSess->fBusy)
                ProcessNextPendingEvent (pSess);

            return TRUE;
        }
    }

    return FALSE;
}

static void DeleteChannel (Session *pSess, DLCI *pChan, int iSignal, int iError, void *pCallContext, int fCloseSessOverride = FALSE) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"DeleteChannel : %04x%08x h = 0x%08x close %d err %d\n", pSess->b.NAP, pSess->b.SAP, pChan, iSignal, iError));

    if (! pChan)
           return;

    SVSUTIL_ASSERT (pChan->pSess == pSess);

    int fstarted = pChan->fStage == UP;
    unsigned char fLocal = pChan->fLocal;
    unsigned char channel = pChan->channel;
    RFCOMM_CONTEXT *pOwner = pChan->pOwner;

    if (pChan == pSess->pLogLinks)
        pSess->pLogLinks = pSess->pLogLinks->pNext;
    else {
        DLCI *pP = pSess->pLogLinks;
        while (pP && (pP->pNext != pChan))
            pP = pP->pNext;
        if (pP)
            pP->pNext = pP->pNext->pNext;
        else
            pChan = NULL;
    }

    // Close the session, but only cancel outstanding calls if we are not going to call up to
    // a disconnect function.
    BOOL fCancelCalls = ! (gpRFCOMM->fConnected && VerifyContext (pOwner));
    if ((! pSess->pLogLinks) && (! fCloseSessOverride))
        CloseSession (pSess, iError, fCancelCalls);

    if (gpRFCOMM->fConnected && VerifyContext (pOwner)) {
        if ((iSignal == RFCOMM_SIGNAL_ASYNC) && fstarted) {
            RFCOMM_Disconnect_Ind pCallback = pOwner->ei.rfcomm_Disconnect_Ind;
            void *pUserContext = pOwner->pUserContext;

            pOwner->AddRef ();
            gpRFCOMM->Unlock ();

            IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"DeleteChannel : going into rfcomm_Disconnect_Ind\n"));

            __try {
                pCallback (pUserContext, pChan);
            } __except (1) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] DeleteChannel : Exception in rfcomm_Disconnect_Ind\n"));
            }

            gpRFCOMM->Lock ();
            pOwner->DelRef ();

            IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"DeleteChannel : out of rfcomm_Disconnect_Ind\n"));
        } else if (iSignal == RFCOMM_SIGNAL_RESPONSE) {
            RFCOMM_Disconnect_Out pCallback = pOwner->c.rfcomm_Disconnect_Out;

            if (pCallback) {
                pOwner->AddRef ();
                gpRFCOMM->Unlock ();

                IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"DeleteChannel : going into rfcomm_Disconnect_Out\n"));

                __try {
                    pCallback (pCallContext, iError);
                } __except (1) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] DeleteChannel : Exception in rfcomm_Disconnect_Out\n"));
                }

                gpRFCOMM->Lock ();
                pOwner->DelRef ();
            }

            IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"DeleteChannel : out of rfcomm_Disconnect_Out\n"));
        }
    }

    if (gpRFCOMM->fRunning)
        delete pChan;
}

static void CloseSession (Session *pSess, int iError, BOOL fCancelCalls) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"CloseSession : 0x%08x\n", pSess));

    if (pSess == gpRFCOMM->pPhysLinks)
        gpRFCOMM->pPhysLinks = gpRFCOMM->pPhysLinks->pNext;
    else {
        Session *pP = gpRFCOMM->pPhysLinks;
        while (pP && (pP->pNext != pSess))
            pP = pP->pNext;

        if (pP)
            pP->pNext = pP->pNext->pNext;
        else
            pSess = NULL;
    }

    unsigned short cid = INVALID_CID;

    if (pSess) {    // Get rid of channels
        IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"CloseSession:: closing %04x%08x\n", pSess->b.NAP, pSess->b.SAP));
        while (pSess->pLogLinks && gpRFCOMM->fRunning)
            DeleteChannel (pSess, pSess->pLogLinks, RFCOMM_SIGNAL_ASYNC, iError, NULL);

        ClearTimeout (pSess);

        if (gpRFCOMM->fConnected)
            cid = pSess->cid;
    } else
        IFDBG(DebugOut (DEBUG_WARN, L"CloseSession:: Session 0x%08x not found!\n", pSess));

    Task *pCall = gpRFCOMM->pCalls;
    while (pCall) {
        if (pCall->pPhysLink == pSess) {
            if (fCancelCalls)
                CancelCall (pCall, iError, NULL);
            else
                DeleteCall (pCall);
            if (! gpRFCOMM->fConnected) {
                cid = INVALID_CID;
                break;
            }

            pCall = gpRFCOMM->pCalls;
        } else
            pCall = pCall->pNext;
    }

    if (pSess && gpRFCOMM->fRunning)
        delete pSess;

    if (cid != INVALID_CID) {    // Close CID
        SVSUTIL_ASSERT (pSess->fStage >= CONNECTED);

        HANDLE h = gpRFCOMM->hL2CAP;

        L2CA_Disconnect_In pCallback = gpRFCOMM->l2cap_if.l2ca_Disconnect_In;
        gpRFCOMM->AddRef ();
        gpRFCOMM->Unlock ();

        IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"CloseSession : going into l2ca_Disconnect_In\n"));

        __try {
            pCallback (h, NULL, cid);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] CloseSession :: Exception in l2ca_Disconnect_In\n"));
        }
        gpRFCOMM->Lock ();
        gpRFCOMM->DelRef ();

        IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"CloseSession : came out of l2ca_Disconnect_In\n"));
    }
}

static void IncrHWErr (Session *pSess) {
    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] IncrHWErr : %04x%08x (to %d)\n", pSess->b.NAP, pSess->b.SAP, pSess->iErr + 1));

    pSess->iErr++;
    if (pSess->iErr > RFCOMM_MAX_ERR) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] Session to %04x%08x closed because of hardware errors (%d total)\n",
            pSess->b.NAP, pSess->b.SAP, pSess->iErr));

        CloseSession (pSess, ERROR_INVALID_DATA, TRUE);
    }
}

//
//    Communicate stack event up
//
static void DispatchStackEvent (int iEvent) {
    RFCOMM_CONTEXT *pContext = gpRFCOMM->pContexts;
    while (pContext && gpRFCOMM->fRunning) {
        BT_LAYER_STACK_EVENT_IND pCallback = pContext->ei.rfcomm_StackEvent;
        if (pCallback) {
            void *pUserContext = pContext->pUserContext;

            IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Going into StackEvent notification\n"));
            pContext->AddRef ();
            gpRFCOMM->Unlock ();

            __try {
                pCallback (pUserContext, iEvent, NULL);
            } __except (1) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] DispatchStackEvent: exception in RFCOMM_StackEvent!\n"));
            }

            gpRFCOMM->Lock ();
            pContext->DelRef ();
            IFDBG(DebugOut (DEBUG_L2CAP_CALLBACK, L"Came back StackEvent notification\n"));
        }
        pContext = pContext->pNext;
    }
}

static DWORD WINAPI StackDown (LPVOID pArg) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Disconnect stack\n"));

    for ( ; ; ) {
        if (! gpRFCOMM) {
            IFDBG(DebugOut (DEBUG_ERROR, L"StackDown : ERROR_SERVICE_DOES_NOT_EXIST\n"));
            return ERROR_SERVICE_DOES_NOT_EXIST;
        }

        gpRFCOMM->Lock ();

        if ((! gpRFCOMM->fConnected)  || (! gpRFCOMM->fRunning)) {
            IFDBG(DebugOut (DEBUG_ERROR, L"StackDown : ERROR_SERVICE_NOT_ACTIVE\n"));
            gpRFCOMM->Unlock ();
            return ERROR_SERVICE_NOT_ACTIVE;
        }

        if (gpRFCOMM->GetRefCount () == 1)
            break;

        IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Waiting for ref count in StackDown\n"));
        gpRFCOMM->Unlock ();
        Sleep (100);
    }

    gpRFCOMM->AddRef ();

    gpRFCOMM->fConnected = FALSE;

    while (gpRFCOMM->pPhysLinks && gpRFCOMM->fRunning)
        CloseSession (gpRFCOMM->pPhysLinks, ERROR_SHUTDOWN_IN_PROGRESS, TRUE);

    while (gpRFCOMM->pCalls && gpRFCOMM->fRunning)
        CancelCall (gpRFCOMM->pCalls, ERROR_SHUTDOWN_IN_PROGRESS, NULL);

    DispatchStackEvent (BTH_STACK_DOWN);

    gpRFCOMM->DelRef ();

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"-StackDown\n"));
    gpRFCOMM->Unlock ();

    return ERROR_SUCCESS;
}

static DWORD WINAPI StackUp (LPVOID pArg) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Connect stack\n"));

    for ( ; ; ) {
        if (! gpRFCOMM) {
            IFDBG(DebugOut (DEBUG_ERROR, L"StackUp : ERROR_SERVICE_DOES_NOT_EXIST\n"));
            return ERROR_SERVICE_DOES_NOT_EXIST;
        }

        gpRFCOMM->Lock ();

        if (gpRFCOMM->fConnected || (! gpRFCOMM->fRunning)) {
            IFDBG(DebugOut (DEBUG_ERROR, L"StackUp : ERROR_SERVICE_NOT_ACTIVE\n"));
            gpRFCOMM->Unlock ();
            return ERROR_SERVICE_NOT_ACTIVE;
        }

        if (gpRFCOMM->GetRefCount () == 1)
            break;

        IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Waiting for ref count in StackUp\n"));
        gpRFCOMM->Unlock ();
        Sleep (100);
    }

    GetConnectionState ();

    if (gpRFCOMM->fConnected) {
        gpRFCOMM->AddRef ();
        DispatchStackEvent (BTH_STACK_UP);
        gpRFCOMM->DelRef ();
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"-StackUp\n"));
    gpRFCOMM->Unlock ();

    return ERROR_SUCCESS;
}

static DWORD WINAPI StackReset (LPVOID pArg) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Reset stack\n"));

    StackDown (NULL);
    StackUp (NULL);

    return NULL;
}

static DWORD WINAPI StackDisconnect (LPVOID pArg) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Connect stack\n"));

    rfcomm_CloseDriverInstance ();

    return NULL;
}


static int SendConfigRequest (Session *pSess) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"SendConfigRequest %04x%08x\n", pSess->b.NAP, pSess->b.SAP));

    Task *pTask = NewTask (CALL_L2CAP_LINK_CONFIG, NULL, pSess);
    if (! pTask) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] SendConfigRequest:: ERROR_OUTOFMEMORY\n"));
        return ERROR_OUTOFMEMORY;
    }

    SVSUTIL_ASSERT (pTask->hCallContext != SVSUTIL_HANDLE_INVALID);

    AddTask (pTask);

    HANDLE h = gpRFCOMM->hL2CAP;
    SVSHandle hContext = pTask->hCallContext;
    L2CA_ConfigReq_In pCallback = gpRFCOMM->l2cap_if.l2ca_ConfigReq_In;
    unsigned short cid = pSess->cid;

    gpRFCOMM->AddRef ();
    gpRFCOMM->Unlock ();

    IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"SendConfigRequest : going into l2ca_ConfigReq_In\n"));

    int iRes = ERROR_INTERNAL_ERROR;
    __try {
        iRes = pCallback (h, (LPVOID)hContext, cid, pSess->inMTU, 0xffff, NULL, 0, NULL);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"RFCOMM :: Exception in l2ca_ConfigReq_In!\n"));
    }

    gpRFCOMM->Lock ();
    gpRFCOMM->DelRef ();

    IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"SendConfigRequest : came out of l2ca_ConfigReq_In\n"));

    if ((iRes != ERROR_SUCCESS) && gpRFCOMM->fRunning && VerifyLink (pSess))
        CloseSession (pSess, iRes, TRUE);

    return iRes;
}

static void SessionIsUp (Session *pSess) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"SessionIsUp %04x%08x\n", pSess->b.NAP, pSess->b.SAP));

    SVSUTIL_ASSERT (gpRFCOMM->fConnected);
    SVSUTIL_ASSERT (pSess->fStage == L2CAPUP);

    // Start channels. Start redirector.
    if (! pSess->fIncoming) {
        Task *pSendSABM0 = NewTask (CALL_RFCOMM_SABM0, NULL, pSess);
        if (pSendSABM0) {
            SVSUTIL_ASSERT (pSendSABM0->channel == 0);

            AddTask (pSendSABM0);

            SVSUTIL_ASSERT (! pSess->fBusy);
            SVSUTIL_ASSERT (! pSess->fWaitAck);

            SetBusy (pSess, pSendSABM0, gpRFCOMM->dwSabmT1);

            int iRes = SendFrame (pSendSABM0, pSess->cid, 0, pSess->fIncoming, CTL_SABM);

            if ((iRes != ERROR_SUCCESS) && gpRFCOMM->fRunning)
                CloseSession (pSess, iRes, TRUE);
        } else
            CloseSession (pSess, ERROR_OUTOFMEMORY, TRUE);
    } else if (! pSess->pLogLinks)
        ScheduleTimeout (pSess, RFCOMM_T1);
}

static Task *GetTaskByAddress (Session *pSess, unsigned char ucAddress) {
    unsigned char channel = ucAddress >> 3;
    unsigned char fLocal  = IsLocal (pSess, ucAddress >> 2);

    Task *pT = gpRFCOMM->pCalls;
    while (pT) {
        if ((pT->fWhat == CALL_RFCOMM_SIGNALDATA) && (pT->pPhysLink == pSess) && (pT->channel == channel) && (pT->fLocal == fLocal))
            break;

        pT = pT->pNext;
    }

    return pT;
}

static Task *GetTaskByResponse (Session *pSess, unsigned char cType, unsigned char ucAddress) {
    unsigned char channel = ucAddress >> 3;
    unsigned char fLocal  = IsLocal (pSess, ucAddress >> 2);

    Task *pT = gpRFCOMM->pCalls;
    while (pT) {
        if ((pT->fWhat == CALL_RFCOMM_SIGNALDATA) && (pT->pPhysLink == pSess) && (pT->eType == cType) && (pT->channel == channel) && (pT->fLocal == fLocal))
            break;

        pT = pT->pNext;
    }

    return pT;
}

static Task *GetTaskByResponse (Session *pSess, unsigned char cType) {
    Task *pT = gpRFCOMM->pCalls;
    while (pT) {
        if ((pT->fWhat == CALL_RFCOMM_SIGNALDATA) && (pT->pPhysLink == pSess) && (pT->eType == cType))
            break;

        pT = pT->pNext;
    }

    return pT;
}

static DLCI *DLCIFromAddress (Session *pSess, unsigned char ucAddress) {
    unsigned char channel = ucAddress >> 3;
    unsigned char fLocal  = IsLocal (pSess, ucAddress >> 2);

    DLCI *pChann = pSess->pLogLinks;

    while (pChann && ((pChann->channel != channel) || (pChann->fLocal != fLocal)))
        pChann = pChann->pNext;

    return pChann;
}

#define BAUD_ARRAY_SIZE    9
static int byte_to_baud[BAUD_ARRAY_SIZE] = {CBR_2400, CBR_4800, 7200, CBR_9600, CBR_19200, CBR_38400, CBR_57600, CBR_115200, 230400};

static int ByteToBaud (unsigned char b) {
    if (BAUD_ARRAY_SIZE <= b) {
        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ByteToBaud : Unknown baud rate byte, byte=%d!\n", b));
        return -1;
    }

    return byte_to_baud[b];
}

static int BaudToByte (int baud) {
    for (int i = 0 ; i < BAUD_ARRAY_SIZE ; ++i) {
        if (byte_to_baud[i] == baud)
            return i;
    }

    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ByteToBaud : baud rate translation failed, baud = %d!\n", baud));
    return -1;
}

static int ByteToData(unsigned char b) {
    SVSUTIL_ASSERT (b < 4);
    return 5 + b;
}

static int DataToByte (unsigned char data) {
#if defined (DEBUG) || defined (_DEBUG)
    if ((data < 5) || (data > 8))
        DebugOut (DEBUG_WARN, L"[RFCOMM] DataToByte : incorrect data = %d!\n", data);
#endif

    return (data - 5) & 3;
}

static int ByteToPT (unsigned char b) {
    SVSUTIL_ASSERT (b < 4);
    return b + 1;
}

static int PTToByte (int parity) {
#if defined (DEBUG) || defined (_DEBUG)
    if (parity > 4)
        DebugOut (DEBUG_WARN, L"[RFCOMM] DataToByte : incorrect parity = %d!\n", parity);
#endif

    return (parity - 1) & 3;
}

static void ProcessSignallingData (Session *pSess, BD_BUFFER *pBuff) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[RFCOMM] +ProcessSignallingData session with %04x%08x\n", pSess->b.NAP, pSess->b.SAP));

    int fError = FALSE;
    unsigned char cTypeByte = 0;
    unsigned char cType = 0;
    unsigned char cLen  = 0;

    fError = ! BufferGetByte (pBuff, &cTypeByte);

#if defined (DEBUG) || defined (_DEBUG) || defined (RETAILLOG)
    if (fError)
        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : empty!\n"));
#endif

    if ((! fError) && (! (cTypeByte & EA_BIT))) {
        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : command 0x%02x no EA_BIT!\n", cTypeByte));
        fError = TRUE;
    } else
        cType = cTypeByte >> 2;

    if ((! fError) && (! (fError = ! BufferGetByte (pBuff, &cLen)))) {
        if (! (cLen & EA_BIT)) {
            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : command 0x%02x too long!\n", cTypeByte));
            fError = TRUE;
        } else if ((cLen = cLen >> 1) != BufferTotal (pBuff)) {
            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : command 0x%02x length mismatch!\n", cTypeByte));
            fError = TRUE;
        }
    }

    if ((! fError) && (cTypeByte & CR_BIT)) {    // Command
        switch (cType) {
        case MSG_TEST:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_TEST command arrived (handling)\n"));
                pBuff->cStart -= 2;
                pBuff->pBuffer[pBuff->cStart] &= (~ CR_BIT);    // Response
                SendFrame (NULL, pSess->cid, pSess->fIncoming, BufferTotal(pBuff), pBuff->pBuffer + pBuff->cStart);
                break;
            }

        case MSG_FCON:
        case MSG_FCOFF:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_FCON/FCOFF command arrived...\n"));

                if (cLen != 0) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_FCON/FCOFF command : incorrect length %d (need 0)...\n", cLen));
                    fError = TRUE;
                    break;
                }

                unsigned char body[2];

                body[0] = cTypeByte & (~CR_BIT);
                body[1] = 0 | EA_BIT;

                IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"ProcessSignallingData/MSG_FCON/FCOFF : sending response\n"));
                SendFrame (NULL, pSess->cid, pSess->fIncoming, sizeof(body), body);

                while (gpRFCOMM->fConnected && (pSess == VerifyLink (pSess))) {
                    DLCI *pDLCI = pSess->pLogLinks;
                    while (pDLCI && pDLCI->fFCA)
                        pDLCI = pDLCI->pNext;

                    if (! pDLCI)
                        break;

                    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"ProcessSignallingData/MSG_FCON/FCOFF : notifying connection 0x%08x\n", pDLCI));

                    pDLCI->fFCA = 1;
                    RFCOMM_CONTEXT *pOwner = pDLCI->pOwner;
                    RFCOMM_FC_Ind pCallback = pOwner->ei.rfcomm_FC_Ind;
                    void *pUserContext = pOwner->pUserContext;
                    pOwner->AddRef ();
                    gpRFCOMM->Unlock ();

                    __try {
                        pCallback (pUserContext, pDLCI, (cType == MSG_FCON) ? TRUE : FALSE);
                    } __except (1) {
                        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessSignallingData : exception in rfcomm_FC_Ind\n"));
                    }

                    gpRFCOMM->Lock ();
                    pOwner->DelRef ();
                }

                if (gpRFCOMM->fConnected && (pSess == VerifyLink (pSess))) {
                    DLCI *pDLCI = pSess->pLogLinks;
                    while (pDLCI) {
                        pDLCI->fFCA = 0;
                        pDLCI = pDLCI->pNext;
                    }
                }
                break;
            }

        case MSG_MSC:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_MSC command arrived...\n"));

                if ((cLen != 2) && (cLen != 3)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_MSC command : incorrect length %d (need 2 or 3)...\n", cLen));
                    fError = TRUE;
                    break;
                }

                unsigned char body[5];

                body[0] = cTypeByte & (~CR_BIT);
                body[1] = (cLen << 1) | EA_BIT;
                BufferGetChunk (pBuff, cLen, body + 2);

                IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"ProcessSignallingData/MSG_MSC : sending response\n"));
                SendFrame (NULL, pSess->cid, pSess->fIncoming, cLen + 2, body);

                DLCI *pDLCI = NULL;

                if (gpRFCOMM->fConnected && (pSess == VerifyLink (pSess)) && (pDLCI = DLCIFromAddress (pSess, body[2]))) {
                    RFCOMM_CONTEXT *pOwner = pDLCI->pOwner;
                    RFCOMM_MSC_Ind pCallback = pOwner->ei.rfcomm_MSC_Ind;
                    void *pUserContext = pOwner->pUserContext;
                    pOwner->AddRef ();
                    gpRFCOMM->Unlock ();

                    __try {
                        pCallback (pUserContext, pDLCI, body[3], cLen == 3 ? body[4] : 0xff);
                    } __except (1) {
                        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessSignallingData : exception in RFCOMM_MSC_Ind\n"));
                    }

                    gpRFCOMM->Lock ();
                    pOwner->DelRef ();
                } else
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : Disconnected or channel not found\n"));

                break;
            }

        case MSG_RLS:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_RLS command arrived...\n"));

                if (cLen != 2) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_RLS command : incorrect length %d (need 2)...\n", cLen));
                    fError = TRUE;
                    break;
                }

                unsigned char body[4];

                body[0] = cTypeByte & (~CR_BIT);
                body[1] = (2 << 1) | EA_BIT;
                BufferGetChunk (pBuff, 2, body + 2);

                IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"ProcessSignallingData/MSG_RLS : sending response\n"));
                SendFrame (NULL, pSess->cid, pSess->fIncoming, sizeof(body), body);

                DLCI *pDLCI = NULL;

                if (gpRFCOMM->fConnected && (pSess == VerifyLink (pSess)) && (pDLCI = DLCIFromAddress (pSess, body[2]))) {
                    RFCOMM_CONTEXT *pOwner = pDLCI->pOwner;
                    RFCOMM_RLS_Ind pCallback = pOwner->ei.rfcomm_RLS_Ind;
                    void *pUserContext = pOwner->pUserContext;
                    pOwner->AddRef ();
                    gpRFCOMM->Unlock ();

                    __try {
                        pCallback (pUserContext, pDLCI, body[3]);
                    } __except (1) {
                        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessSignallingData : exception in RFCOMM_RLS_Ind\n"));
                    }

                    gpRFCOMM->Lock ();
                    pOwner->DelRef ();
                } else
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : Disconnected or channel not found\n"));

                break;
            }


        case MSG_PN:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_PN command arrived...\n"));

                if (cLen != 8) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_PN command : incorrect length %d (need 8)...\n", cLen));
                    fError = TRUE;
                    break;
                }

                unsigned char body[8];

                BufferGetChunk (pBuff, 8, body);

                DLCI *pDLCI = DLCIFromAddress (pSess, body[0] << 2);    // Cheat, convert to address...

                if (! pDLCI) {
                    if (! IsLocal (pSess, body[0])) {
                        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] trying to PN non-local channel (0x%02x) session %04x%08x\n", body[0], pSess->b.NAP, pSess->b.SAP));
                        fError = TRUE;
                        break;
                    }

                    unsigned char channel = body[0] >> 1;

                    RFCOMM_CONTEXT *pOwner = FindContextByChannel (channel);

                    if (! pOwner)
                        pOwner = FindContextForDefault ();
    
                    if (! pOwner) {
                        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] No owner for PN request!\n"));
                        fError = TRUE;
                        break;
                    }

                    pDLCI = NewLog (pSess, channel, TRUE, pOwner);

                    if (pDLCI) {
                        pDLCI->pNext = pSess->pLogLinks;
                        pSess->pLogLinks = pDLCI;
                    } else {
                        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ERROR_OUTOFMEMORY in PN processing (channel creation!)\n"));
                        fError = TRUE;
                        break;
                    }
                }

                if (pDLCI) {
                    // Make sure the value of N1 that we hand up to the upper layer does not exceed the outgoing L2CAP MTU
                    // with RFCOMM headers/footers accounted for.
                    unsigned short n1Max = pDLCI->pSess->outMTU - (sizeof(TS_FRAME_HEADER_X) + sizeof(TS_FRAME_FOOTER));
                    unsigned short n1 = (body[5] << 8) | body[4];
                    if (n1 > n1Max)
                        n1 = n1Max;
                
                    RFCOMM_CONTEXT *pOwner = pDLCI->pOwner;
                    RFCOMM_PNREQ_Ind pCallback = pOwner->ei.rfcomm_PNREQ_Ind;
                    void *pUserContext = pOwner->pUserContext;
                    pOwner->AddRef ();
                    gpRFCOMM->Unlock ();

                    int iRes = ERROR_SUCCESS;
                    __try {
                        iRes = pCallback (pUserContext, pDLCI, body[2] & 0x3f, n1, (body[1] >> 4) & 0xf, body[7] & 0x7);
                    } __except (1) {
                        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessSignallingData : exception in RFCOMM_PN_Ind\n"));
                    }

                    if (iRes != ERROR_SUCCESS)
                        fError = TRUE;

                    gpRFCOMM->Lock ();
                    pOwner->DelRef ();
                } else
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : Disconnected or channel not found\n"));

                break;
            }

        case MSG_RPN:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_RPN command arrived...\n"));

                if ((cLen != 8) && (cLen != 1)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_RPN command : incorrect length %d (need 8)...\n", cLen));
                    fError = TRUE;
                    break;
                }

                unsigned char body[8];

                BufferGetChunk (pBuff, cLen, body);

                DLCI *pDLCI = DLCIFromAddress (pSess, body[0]);    // Cheat, convert to address...

                if (pDLCI) {
                    RFCOMM_CONTEXT *pOwner = pDLCI->pOwner;
                    RFCOMM_RPNREQ_Ind pCallback = pOwner->ei.rfcomm_RPNREQ_Ind;
                    void *pUserContext = pOwner->pUserContext;
                    pOwner->AddRef ();
                    gpRFCOMM->Unlock ();

                    int iRes = ERROR_INTERNAL_ERROR;

                    __try {
                        if (cLen == 1)
                            iRes = pCallback (pUserContext, pDLCI, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                        else
                            iRes = pCallback (pUserContext, pDLCI,
                                        (body[7] << 8) | body[6],
                                        ByteToBaud(body[1]),
                                        ByteToData(body[2] & 3),
                                        (body[2] & 4) ? ONE5STOPBITS : ONESTOPBIT,
                                        (body[2] >> 3) & 1,
                                        ByteToPT((body[2] >> 4) & 3),
                                        body[3],
                                        body[4],
                                        body[5]);
                    } __except (1) {
                        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessSignallingData : exception in rfcomm_RPNREQ_Ind\n"));
                    }

                    if (iRes != ERROR_SUCCESS)
                        fError = TRUE;

                    gpRFCOMM->Lock ();
                    pOwner->DelRef ();
                } else
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : Disconnected or channel not found\n"));

                break;
            }

        case MSG_NSC:    // This IS incorrect - it should have response bit!
        case MSG_PSC:
        case MSG_SNC:
        case MSG_CLD:
        default:
            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : command 0x%02x type %d not supported!\n", cTypeByte, cType));
            fError = TRUE;
        }
    }

    if ((! fError) && (! (cTypeByte & CR_BIT))) {    //Response
        Task *pTask = NULL;

        switch (cType) {
        case MSG_PN:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_PN response arrived\n"));
                unsigned char body[8];
                if ((cLen == 8) && BufferGetChunk (pBuff, 8, body)) {
                    pTask = GetTaskByResponse (pSess, cType, body[0] << 2);
                    if (pTask) {
                        RFCOMM_CONTEXT *pOwner = pTask->pOwner;
                        void *pContext = pTask->pContext;
                        RFCOMM_PNREQ_Out pCallback = pOwner->c.rfcomm_PNREQ_Out;

                        DeleteCall (pTask);

                        pOwner->AddRef ();
                        gpRFCOMM->Unlock ();

                        __try {
                            pCallback (pContext, ERROR_SUCCESS, body[2] & 0x3f, (body[5] << 8) | body[4], (body[1] >> 4) & 0xf, body[7] & 0x7);
                        } __except (1) {
                            IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessSignallingData : Exception in rfcomm_PNREQ_Out\n"));
                        }

                        gpRFCOMM->Lock ();
                        pOwner->DelRef ();
                    } else
                        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_PN response - found no owner, ignoring...\n"));
                } else {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_PN response incorrect\n"));
                    fError = TRUE;
                }

                break;
            }

        case MSG_PSC:
            IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_PSC response arrived (swallowing)\n"));
            break;

        case MSG_TEST:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_TEST response arrived\n"));
                pTask = GetTaskByResponse (pSess, cType);
                if (pTask) {
                    RFCOMM_CONTEXT *pOwner = pTask->pOwner;
                    void *pContext = pTask->pContext;
                    RFCOMM_TEST_Out pCallback = pOwner->c.rfcomm_TEST_Out;

                    DeleteCall (pTask);

                    if (pCallback) {
                        pOwner->AddRef ();
                        gpRFCOMM->Unlock ();

                        __try {
                            pCallback (pContext, ERROR_SUCCESS, cLen, pBuff->pBuffer + pBuff->cStart);
                        } __except (1) {
                            IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessSignallingData : Exception in rfcomm_TEST_Out\n"));
                        }

                        gpRFCOMM->Lock ();
                        pOwner->DelRef ();
                    }
                } else
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_TEST response - found no owner, ignoring...\n"));

                break;
            }

        case MSG_FCON:
        case MSG_FCOFF:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_FCON/OFF response arrived\n"));
                if (cLen == 0) {
                    pTask = GetTaskByResponse (pSess, cType);
                    if (pTask) {
                        RFCOMM_CONTEXT *pOwner = pTask->pOwner;
                        void *pContext = pTask->pContext;
                        RFCOMM_FC_Out pCallback = pOwner->c.rfcomm_FC_Out;

                        DeleteCall (pTask);

                        if (pCallback) {
                            pOwner->AddRef ();
                            gpRFCOMM->Unlock ();

                            __try {
                                pCallback (pContext, ERROR_SUCCESS);
                            } __except (1) {
                                IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessSignallingData : Exception in rfcomm_FC_Out\n"));
                            }

                            gpRFCOMM->Lock ();
                            pOwner->DelRef ();
                        }
                    } else
                        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_FCON/OFF response - found no owner, ignoring...\n"));
                } else {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_FCON/OFF response incorrect\n"));
                    fError = TRUE;
                }

                break;
            }

        case MSG_MSC:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_MSC response arrived\n"));
                unsigned char body[3];
                if (((cLen == 2) || (cLen == 3)) && BufferGetChunk (pBuff, cLen, body)) {
                    pTask = GetTaskByResponse (pSess, cType, body[0]);
                    if (pTask) {
                        RFCOMM_CONTEXT *pOwner = pTask->pOwner;
                        void *pContext = pTask->pContext;
                        RFCOMM_MSC_Out pCallback = pOwner->c.rfcomm_MSC_Out;

                        DeleteCall (pTask);

                        if (pCallback) {
                            pOwner->AddRef ();
                            gpRFCOMM->Unlock ();

                            __try {
                                pCallback (pContext, ERROR_SUCCESS);
                            } __except (1) {
                                IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessSignallingData : Exception in rfcomm_MSC_Out\n"));
                            }

                            gpRFCOMM->Lock ();
                            pOwner->DelRef ();
                        }
                    } else
                        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_MSC response - found no owner, ignoring...\n"));
                } else {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_MSC response incorrect\n"));
                    fError = TRUE;
                }

                break;
            }

        case MSG_NSC:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_NSC response arrived\n"));
                if (cLen == 1) {
                    BufferGetByte (pBuff, &cType);
                    cType >>= 2;

                    pTask = GetTaskByResponse (pSess, cType);
                    if (pTask)
                        CancelCall (pTask, ERROR_NOT_FOUND, NULL);
                    else
                        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_NSC response - found no owner, ignoring...\n"));
                } else {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_NSC response incorrect\n"));
                    fError = TRUE;
                }

                break;
            }

        case MSG_RPN:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_RPN response arrived\n"));
                unsigned char body[8];
                if (((cLen == 1) || (cLen == 8)) && BufferGetChunk (pBuff, cLen, body)) {
                    pTask = GetTaskByResponse (pSess, cType, body[0]);
                    if (pTask) {
                        RFCOMM_CONTEXT *pOwner = pTask->pOwner;
                        void *pContext = pTask->pContext;
                        RFCOMM_RPNREQ_Out pCallback = pOwner->c.rfcomm_RPNREQ_Out;

                        DeleteCall (pTask);

                        pOwner->AddRef ();
                        gpRFCOMM->Unlock ();

                        __try {
                            if (cLen == 1)
                                pCallback (pContext, ERROR_SUCCESS, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                            else
                                pCallback (pContext, ERROR_SUCCESS,
                                        (body[7] << 8) | body[6],
                                        ByteToBaud(body[1]),
                                        ByteToData(body[2] & 3),
                                        (body[2] & 4) ? ONE5STOPBITS : ONESTOPBIT,
                                        (body[2] >> 3) & 1,
                                        ByteToPT((body[2] >> 4) & 3),
                                        body[3],
                                        body[4],
                                        body[5]);
                        } __except (1) {
                            IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessSignallingData : Exception in rfcomm_PNREQ_Out\n"));
                        }

                        gpRFCOMM->Lock ();
                        pOwner->DelRef ();
                    } else
                        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_RPN response - found no owner, ignoring...\n"));
                } else {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_RPN response incorrect\n"));
                    fError = TRUE;
                }

                break;
            }

        case MSG_RLS:
            {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessSignallingData : MSG_RLS response arrived\n"));
                unsigned char body[2];
                if ((cLen == 2) && BufferGetChunk (pBuff, cLen, body)) {
                    pTask = GetTaskByResponse (pSess, cType, body[0]);
                    if (pTask) {
                        RFCOMM_CONTEXT *pOwner = pTask->pOwner;
                        void *pContext = pTask->pContext;
                        RFCOMM_RLS_Out pCallback = pOwner->c.rfcomm_RLS_Out;

                        DeleteCall (pTask);

                        if (pCallback) {
                            pOwner->AddRef ();
                            gpRFCOMM->Unlock ();

                            __try {
                                pCallback (pContext, ERROR_SUCCESS);
                            } __except (1) {
                                IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessSignallingData : Exception in rfcomm_RLS_Out\n"));
                            }

                            gpRFCOMM->Lock ();
                            pOwner->DelRef ();
                        }
                    } else
                        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_RLS response - found no owner, ignoring...\n"));
                } else {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : MSG_RLS response incorrect\n"));
                    fError = TRUE;
                }

                break;
            }

        case MSG_SNC:
        case MSG_CLD:
        default:
            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : response 0x%02x type %d not supported!\n", cTypeByte, cType));
            fError = TRUE;
        }
    }

    if (pBuff->pFree)
        pBuff->pFree (pBuff);

    if ((cTypeByte & CR_BIT) && fError && gpRFCOMM && gpRFCOMM->fConnected && (pSess == VerifyLink (pSess))) { // Send NSC...
        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessSignallingData : command 0x%02x type %d -- processing error condition.\n", cTypeByte, cType));

        unsigned char ucCommand[3];
        ucCommand[0] = EA_BIT | (MSG_NSC << 2);
        ucCommand[1] = EA_BIT | (1 << 1);
        ucCommand[2] = cTypeByte;
        SendFrame (NULL, pSess->cid, pSess->fIncoming, sizeof(ucCommand), ucCommand);
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"[RFCOMM] -ProcessSignallingData\n"));
}

static void ProcessFrame (Session *pSess, TS_FRAME *pFrame) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"ProcessFrame %04x%08x ctl 0x%02x addr 0x%02x\n", pSess->b.NAP, pSess->b.SAP, pFrame->h.ucControl, pFrame->h.ucAddress));
    SVSUTIL_ASSERT (pSess->fStage >= L2CAPUP);
    SVSUTIL_ASSERT (gpRFCOMM->fConnected);

    if (pFrame->h.ucControl == (CTL_SABM | CTL_PF)) {    // SABM : connect
        IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessFrame : Got SABM\n"));

        unsigned char cDLCI = pFrame->h.ucAddress >> 2;
        if (cDLCI == 0) {    // Start MUX
            IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessFrame : SABM (0) - MUX start\n"));
            if ((! pSess->fIncoming) || (pSess->fStage != L2CAPUP)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] SABM(0) out of synch session %04x%08x!\n", pSess->b.NAP, pSess->b.SAP));
                RejectFrame (pSess, pFrame);
                return;
            }

            ClearTimeout (pSess);

            pSess->fStage |= SABM0UA;
            SVSUTIL_ASSERT (pSess->fStage == UP);
            SVSUTIL_ASSERT (! pSess->fBusy);
            SVSUTIL_ASSERT (! pSess->fWaitAck);

            IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessFrame : responding with UA\n"));

            int iRes = SendFrame (NULL, pSess->cid, 0, pSess->fIncoming, CTL_UA);

            if ((iRes == ERROR_SUCCESS) && VerifyLink (pSess))
                ProcessPendingSignals (pSess);

            if ((iRes == ERROR_SUCCESS) && VerifyLink (pSess) && (! pSess->fBusy))
                ProcessNextPendingEvent (pSess);

            return;
        }

        SVSUTIL_ASSERT (pSess->fStage == UP);

        IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessFrame : SABM channel = %d\n", pFrame->h.ucAddress >> 3));

        if (! IsLocal (pSess, cDLCI)) {
            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] trying to SABM non-local channel (0x%02x) session %04x%08x\n", cDLCI, pSess->b.NAP, pSess->b.SAP));
            RejectFrame (pSess, pFrame);
            return;
        }

        unsigned char channel = cDLCI >> 1;
        DLCI *pDLCI = pSess->pLogLinks;
        while (pDLCI && ((! pDLCI->fLocal) || (pDLCI->channel != channel)))
            pDLCI = pDLCI->pNext;

        if (pDLCI && (pDLCI->fStage != DORMANT)) {
            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] dlci 0x%02x session %04x%08x already started!\n", channel, pSess->b.NAP, pSess->b.SAP));
            RejectFrame (pSess, pFrame);
            return;
        }

        RFCOMM_CONTEXT *pOwner = pDLCI ? pDLCI->pOwner : NULL;

        if (! pOwner) {
            pOwner = FindContextByChannel (channel);

            if (! pOwner)
                pOwner = FindContextForDefault ();
        }

        int iRes = ERROR_INTERNAL_ERROR;
    
        if (! pDLCI) {
            pDLCI = NewLog (pSess, channel, TRUE, pOwner);

            if (pDLCI) {
                pDLCI->pNext = pSess->pLogLinks;
                pSess->pLogLinks = pDLCI;
            } else {
                IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ERROR_OUTOFMEMORY in ProcessFrame (channel creation!)\n"));
                iRes = ERROR_OUTOFMEMORY;
                pOwner = NULL;
            }
        }

        if (pDLCI) {
            pDLCI->fStage = CONNECTED;
        }

        if (pOwner) {
            void *pUserContext = pOwner->pUserContext;
            RFCOMM_ConnectRequest_Ind pCallback = pOwner->ei.rfcomm_ConnectRequest_Ind;
            BD_ADDR b = pSess->b;

            pOwner->AddRef ();
            gpRFCOMM->Unlock ();

            IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"ProcessFrame : going into rfcomm_ConnectRequest_Ind\n"));

            __try {
                iRes = pCallback (pUserContext, pDLCI, &b, channel);
            } __except (1) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] Exception in rfcomm_ConnectRequest_Ind!\n"));
            }

            gpRFCOMM->Lock ();
            pOwner->DelRef ();

            IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"ProcessFrame : came out of rfcomm_ConnectRequest_Ind\n"));
        }

        if (gpRFCOMM->fRunning && (iRes != ERROR_SUCCESS) && VerifyLink (pSess)) {
            if (VerifyChannel (pSess, pDLCI))
                DeleteChannel (pSess, pDLCI, RFCOMM_SIGNAL_NONE, iRes, NULL);

            if (gpRFCOMM->fConnected && VerifyLink (pSess))
                RejectFrame (pSess, pFrame);
        }

        return;
    }

    if ((pFrame->h.ucControl == (CTL_UA | CTL_PF)) || (pFrame->h.ucControl == (CTL_DM | CTL_PF))) {    // ACK or NACK
        IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessFrame : Got UA/DM channel = %d\n", pFrame->h.ucAddress >> 3));

#if defined (DEBUG) || defined (_DEBUG)
        // If this is a UA frame it should be in response to a SABM or DISC so  
        // we must be in the busy state.
        if (pFrame->h.ucControl == (CTL_UA | CTL_PF)) {
            SVSUTIL_ASSERT (pSess->fBusy);
            SVSUTIL_ASSERT (pSess->ucDbgBusyChannel == pFrame->h.ucAddress >> 3);
        }
#endif

        ClearBusy (pSess);

        Task *pCall = gpRFCOMM->pCalls;
        while (pCall && ((pCall->pPhysLink != pSess) || (! pCall->fPF)))
            pCall = pCall->pNext;

        if (! pCall) {    // Swallowing it...
            if (! pSess->fWaitAck)
                pCall = GetTaskByAddress (pSess, pFrame->h.ucAddress);
            else
                pSess->fWaitAck = FALSE;

            if (pCall) {
                IFDBG(DebugOut (DEBUG_WARN, L"ProcessFrame : call cancelled on channel = %d\n", pFrame->h.ucAddress >> 3));
                CancelCall (pCall, ERROR_CONNECTION_REFUSED, NULL);
                return;
            }

            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessFrame : Orphaned UA/DM on channed = %d!\n", pFrame->h.ucAddress >> 3));
            if (pSess->fStage == UP)
                ProcessNextPendingEvent (pSess);

            return;
        }

        SVSUTIL_ASSERT (! pSess->fWaitAck);

        unsigned char dlci = pFrame->h.ucAddress >> 2;
        unsigned char channel = pFrame->h.ucAddress >> 3;

        if (dlci != GetDLCI (pCall)) {    // We are in total disconnect
            CloseSession (pSess, ERROR_PROTOCOL_UNREACHABLE, TRUE);
            return;
        }

        switch (pCall->fWhat) {    // What is it we (n)acked?
        case CALL_RFCOMM_SABM0:
            SVSUTIL_ASSERT (! pSess->fIncoming);
            if (pSess->fStage != L2CAPUP) {
                IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] UA/DM for SABM(0) out of synch session %04x%08x!\n", pSess->b.NAP, pSess->b.SAP));
                return;
            }

            ClearTimeout (pSess);
            DeleteCall (pCall);

            if (pFrame->h.ucControl == (CTL_DM | CTL_PF)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] DM for SABM(0) for session %04x%08x -- closing everything\n", pSess->b.NAP, pSess->b.SAP));
                CloseSession (pSess, ERROR_PROTOCOL_UNREACHABLE, TRUE);
                return;
            } else {
                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"[RFCOMM] ProcessFrame : UA SABM0\n"));

                pSess->fStage |= SABM0UA;
                ProcessPendingSignals (pSess);

                SVSUTIL_ASSERT (pSess->fStage == UP);
                SVSUTIL_ASSERT (! pSess->fBusy);
            }
            break;

        case CALL_RFCOMM_SABMN:
            {
                SVSUTIL_ASSERT (pSess->fStage == UP);
                SVSUTIL_ASSERT (! IsLocal (pSess, dlci));

                DLCI *pDLCI = pSess->pLogLinks;
                while (pDLCI && (pDLCI->fLocal || (pDLCI->channel != channel)))
                    pDLCI = pDLCI->pNext;

                if ((! pDLCI) || (pDLCI->fStage != CONNECTING)) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] Internal error! Call references non-existent channel or channel in bad mode (ptr = 0x%08x)\n", pDLCI));
                    SVSUTIL_ASSERT (0);
                    CancelCall (pCall, ERROR_INTERNAL_ERROR, NULL);
                    break;
                }

                SVSUTIL_ASSERT (! pDLCI->fLocal);
                pDLCI->fStage = (pFrame->h.ucControl == (CTL_UA | CTL_PF)) ? UP : DORMANT;

                void *pCallContext = pCall->pContext;
                RFCOMM_CONTEXT *pOwner = pCall->pOwner;

                DeleteCall (pCall);
                RFCOMM_ConnectRequest_Out pCallback = pOwner->c.rfcomm_ConnectRequest_Out;

                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"[RFCOMM] ProcessFrame : %s SABM channel = %d\n", (pFrame->h.ucControl == (CTL_UA | CTL_PF)) ? L"UA" : L"DM", channel));

                pOwner->AddRef ();
                gpRFCOMM->Unlock ();

                IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"ProcessFrame : going into rfcomm_ConnectRequest_Out\n"));
                int iRes = ERROR_INTERNAL_ERROR;
                __try {
                    iRes = pCallback (pCallContext,
                        (pFrame->h.ucControl == (CTL_UA | CTL_PF)) ? ERROR_SUCCESS : ERROR_CONNECTION_REFUSED,
                        pDLCI);
                } __except (1) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ProcessFrame : exception in rfcomm_ConnectRequest_Out\n"));
                }

                gpRFCOMM->Lock ();
                pOwner->DelRef ();

                IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"ProcessFrame : came out of rfcomm_ConnectRequest_Out (ret = %d)\n", iRes));
            }
            break;

        case CALL_RFCOMM_DISCN:
            {
                SVSUTIL_ASSERT (pSess->fStage == UP);

                IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"[RFCOMM] ProcessFrame : %s DISC channel = %d\n", (pFrame->h.ucControl == (CTL_UA | CTL_PF)) ? L"UA" : L"DM", channel));

                void *pCallContext = pCall->pContext;
                DeleteCall (pCall);

                unsigned int fLocal = IsLocal (pSess, dlci);

                DLCI *pDLCI = pSess->pLogLinks;

                while (pDLCI && ((pDLCI->channel != channel) || (pDLCI->fLocal != fLocal)))
                    pDLCI = pDLCI->pNext;

                if (! pDLCI) {
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] CALL_RFCOMM_DISCN arrived for nonexistent channel!\n"));
                    break;
                }

                DeleteChannel (pSess, pDLCI, RFCOMM_SIGNAL_RESPONSE, ERROR_GRACEFUL_DISCONNECT, pCallContext);
            }
            break;
        }

        if (gpRFCOMM->fConnected && VerifyLink (pSess) && (! pSess->fBusy))
            ProcessNextPendingEvent (pSess);

        return;
    }

    if (pFrame->h.ucControl == (CTL_DISC | CTL_PF)) {
        IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"ProcessFrame : got DISC channel = %d\n", pFrame->h.ucAddress >> 3));

        unsigned char channel = pFrame->h.ucAddress >> 3;
        if (channel == 0) {
            IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"DISC arrived for the multiplexor - going down!\n"));

            AcceptFrame (pSess, pFrame);

            while (VerifyLink (pSess) && pSess->pLogLinks)
                DeleteChannel (pSess, pSess->pLogLinks, RFCOMM_SIGNAL_ASYNC, ERROR_GRACEFUL_DISCONNECT, NULL, TRUE);

            return;
        }

        unsigned char dlci = pFrame->h.ucAddress >> 2;
        unsigned int fLocal = IsLocal (pSess, dlci);

        DLCI *pDLCI = pSess->pLogLinks;

        while (pDLCI && ((pDLCI->channel != channel) || (pDLCI->fLocal != fLocal)))
            pDLCI = pDLCI->pNext;

        if (! pDLCI) {
            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] DISC arrived for nonexistent channel!\n"));
            RejectFrame (pSess, pFrame);

            return;
        }

        int iCallSignal = RFCOMM_SIGNAL_ASYNC;
        void *pCallContext = NULL;

        Task *pCall = gpRFCOMM->pCalls;
        while (pCall && ((pCall->pPhysLink != pSess) || (pCall->fLocal != pDLCI->fLocal) || (pCall->channel != pDLCI->channel) ||
            (pCall->fWhat != CALL_RFCOMM_DISCN)))
            pCall = pCall->pNext;

        if (pCall) {    // Kill our pending DISC call while not unblocking the session
            IFDBG(DebugOut (DEBUG_WARN, L"Converting DISC event to satisfy outstanding DISC call for channel = %d\n", channel));
            if (pCall->fPF)
                pSess->fWaitAck = TRUE;

            pCallContext = pCall->pContext;

            DeleteCall (pCall);

            iCallSignal = RFCOMM_SIGNAL_RESPONSE;
        }

        BOOL fCancelCalls = ! (gpRFCOMM->fConnected && VerifyContext (pDLCI->pOwner));

        SVSUTIL_ASSERT (pDLCI->fStage == UP);
        DeleteChannel (pSess, pDLCI, iCallSignal, ERROR_GRACEFUL_DISCONNECT, pCallContext, TRUE);

        if (VerifyLink (pSess))
            AcceptFrame (pSess, pFrame);

        if (VerifyLink (pSess) && (! pSess->pLogLinks))
            ScheduleTimeout(pSess, RFCOMM_T3);

        return;
    }

    if (pFrame->h.ucControl == CTL_DM) {
        Task *pCall = GetTaskByAddress (pSess, pFrame->h.ucAddress);
        if (pCall) {
            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessFrame : Got DM in response to control request. Cancelling it.\n"));
            CancelCall (pCall, ERROR_CONNECTION_REFUSED, NULL);
        } else {
            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ProcessFrame : Got DM with no P/F flag set. Just ignoring it\n"));
        }
        return;
    }

    SVSUTIL_ASSERT (0);
    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] Incorrect ctl = 0x%02x\n", pFrame->h.ucControl));

    return;
}

static int rfcomm_connect_ind (void *pUserContext, BD_ADDR *pba, unsigned short cid, unsigned char id, unsigned short psm) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_connect_ind\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_connect_ind : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_connect_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Session *pSess = gpRFCOMM->pPhysLinks;
    while (pSess && (pSess->b != *pba))
        pSess = pSess->pNext;

    int fError = FALSE;

    L2CA_ConnectResponse_In pCallback = gpRFCOMM->l2cap_if.l2ca_ConnectResponse_In;
    HANDLE h = gpRFCOMM->hL2CAP;

    if ((! pSess) || (pSess->fStage == DORMANT)) {
        gpRFCOMM->AddRef ();
        gpRFCOMM->Unlock ();

        IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_connect_ind : going into l2ca_ConnectResponse_In\n"));

        int iRes = ERROR_INTERNAL_ERROR;
        __try {
            iRes = pCallback (h, NULL, pba, id, cid, 0, 0);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] rfcomm_connect_ind :: exception in l2ca_ConnectResponse_In\n"));
        }

        gpRFCOMM->Lock ();
        gpRFCOMM->DelRef ();

        IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_connect_ind : came out of l2ca_ConnectResponse_In\n"));

        if ((iRes == ERROR_SUCCESS) && gpRFCOMM->fConnected) {    // Create session and task, and send config request
            pSess = pSess ? VerifyLink (pSess) : NULL;

            if (! pSess) {
                pSess = NewSession (pba);
                if (pSess) {
                    pSess->pNext = gpRFCOMM->pPhysLinks;
                    gpRFCOMM->pPhysLinks = pSess;
                }
            }

            if (pSess) {
                SVSUTIL_ASSERT (pSess->fIncoming == FALSE);
                pSess->fIncoming = TRUE;

                pSess->cid = cid;
                pSess->fStage = CONNECTED;

                fError = (SendConfigRequest (pSess) != ERROR_SUCCESS);
            } else {
                IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ERROR_OUTOFMEMORY in rfcomm_connect_ind (session)\n"));
                fError = TRUE;
            }
        }
    } else
        fError = TRUE;
    
    if (fError && gpRFCOMM->fConnected) {    // Negative response
        gpRFCOMM->AddRef ();
        gpRFCOMM->Unlock ();

        __try {
            pCallback (h, NULL, pba, id, cid, 4, 0);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] rfcomm_connect_ind :: exception in l2ca_ConnectResponse_In\n"));
        }

        gpRFCOMM->Lock ();
        gpRFCOMM->DelRef ();
    }

    int iRes = ERROR_SUCCESS;

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_connect_ind: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_config_ind (void *pUserContext, unsigned char id, unsigned short cid, unsigned short usOutMTU, unsigned short usInFlushTO, struct btFLOWSPEC *pInFlow, int cOptNum, struct btCONFIGEXTENSION **ppExtendedOptions) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_config_ind\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_config_ind : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_config_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Session *pSess = gpRFCOMM->pPhysLinks;
    while (pSess && (pSess->cid != cid))
        pSess = pSess->pNext;

    if (pSess) {
        int iRes = ERROR_PROTOCOL_UNREACHABLE;

        if ((usOutMTU && (usOutMTU < RFCOMM_N1_MIN + sizeof(TS_FRAME))) || (usInFlushTO != 0xffff) || (pInFlow && (pInFlow->service_type != 0x01))) {
            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] rfcomm_config_ind :: not supported parameters!\n"));

            struct btFLOWSPEC flowspec;

            if (pInFlow && (pInFlow->service_type != 0x01)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] rfcomm_config_ind :: not supported flowspec!\n"));
                flowspec = *pInFlow;
                pInFlow = &flowspec;
                pInFlow->service_type = 0x01;
            } else
                pInFlow = NULL;

            if (usInFlushTO != 0xffff) {
                IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] rfcomm_config_ind :: not supported flush timeout!\n"));
                usInFlushTO = 0xffff;
            } else
                usInFlushTO = 0;

            if (usOutMTU && (usOutMTU < RFCOMM_N1_MIN + sizeof(TS_FRAME))) {
                IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] rfcomm_config_ind :: not supported MTU!\n"));
                usOutMTU = RFCOMM_N1_MIN + sizeof(TS_FRAME);
            } else
                usOutMTU = 0;

            HANDLE h = gpRFCOMM->hL2CAP;
            L2CA_ConfigResponse_In pCallback = gpRFCOMM->l2cap_if.l2ca_ConfigResponse_In;
            gpRFCOMM->AddRef ();
            gpRFCOMM->Unlock ();
    
            IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_config_ind : going into l2ca_ConfigResponse_In (rejecting the flow)\n"));

            iRes = ERROR_INTERNAL_ERROR;

            __try {
                iRes = pCallback (h, NULL, id, cid, 1, usOutMTU, usInFlushTO, pInFlow, 0, NULL);
            } __except (1) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] rfcomm_config_ind Exception in l2ca_ConfigResponse_In\n"));
            }

            gpRFCOMM->Lock ();
            gpRFCOMM->DelRef ();

            IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_config_ind : coming out of l2ca_ConfigResponse_In\n"));

        } else if (! btutil_VerifyExtendedL2CAPOptions (cOptNum, ppExtendedOptions))
            IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] rfcomm_config_ind :: unsupported extended options!\n"));
        else {    // Yesssss!
            HANDLE h = gpRFCOMM->hL2CAP;
            L2CA_ConfigResponse_In pCallback = gpRFCOMM->l2cap_if.l2ca_ConfigResponse_In;
            gpRFCOMM->AddRef ();
            gpRFCOMM->Unlock ();
    
            IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_config_ind : going into l2ca_ConfigResponse_In\n"));

            iRes = ERROR_INTERNAL_ERROR;

            __try {
                iRes = pCallback (h, NULL, id, cid, 0, 0, 0xffff, NULL, 0, NULL);
            } __except (1) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] rfcomm_config_ind Exception in l2ca_ConfigResponse_In\n"));
            }

            gpRFCOMM->Lock ();
            gpRFCOMM->DelRef ();

            IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_config_ind : coming out of l2ca_ConfigResponse_In\n"));

            if (gpRFCOMM->fConnected && (iRes == ERROR_SUCCESS)) {
                pSess->outMTU = usOutMTU ? usOutMTU : L2CAP_MTU;
                                                                
                pSess->fStage |= CONFIG_IND_DONE;
                if (pSess->fStage == L2CAPUP)
                    SessionIsUp (pSess);


            }
        }

        if (iRes != ERROR_SUCCESS)
            CloseSession (pSess, iRes, TRUE);
    } else
        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] rfcomm_config_ind: Unknown cid 0x%04x\n", cid));

    int iRes = ERROR_SUCCESS;

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_config_ind: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_disconnect_ind (void *pUserContext, unsigned short cid, int iErr) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_disconnect_ind\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_disconnect_ind : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_disconnect_ind : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Session *pSess = gpRFCOMM->pPhysLinks;
    while (pSess && (pSess->cid != cid))
        pSess = pSess->pNext;

    if (pSess)
        CloseSession (pSess, (((pSess->fStage == UP) && (! pSess->pLogLinks)) ? ERROR_GRACEFUL_DISCONNECT : ERROR_PROTOCOL_UNREACHABLE), TRUE);

    int iRes = ERROR_SUCCESS;

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_disconnect_ind: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

//    This is main data loop
static int rfcomm_data_up_ind (void *pUserContext, unsigned short cid, BD_BUFFER *pBuffer) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_data_up_ind\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_data_up_ind : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    IFDBG(DebugOut (DEBUG_RFCOMM_PACKETS, L"Data Packet :: cid 0x%04x %d bytes\n", cid, BufferTotal (pBuffer)));
    IFDBG(DumpBuff (DEBUG_RFCOMM_PACKETS, pBuffer->pBuffer + pBuffer->cStart, BufferTotal (pBuffer)));

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_data_up_ind : ERROR_SERVICE_NOT_ACTIVE\n"));

        if (pBuffer->pFree)
            pBuffer->pFree (pBuffer);

        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Session *pSess = gpRFCOMM->pPhysLinks;
    while (pSess && (pSess->cid != cid))
        pSess = pSess->pNext;

    if (! pSess) {
        IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_data_up_ind: returned %d\n", ERROR_NOT_FOUND));
        if (pBuffer->pFree)
            pBuffer->pFree (pBuffer);

        gpRFCOMM->Unlock ();

        return ERROR_NOT_FOUND;
    }

    TS_FRAME fm;
    if ((! BufferGetChunk (pBuffer, sizeof (fm.h), (unsigned char *)&fm)) || (BufferTotal (pBuffer) < 1)) {
        IncrHWErr (pSess);

        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] invalid data received - buffer too small..."));

        if (pBuffer->pFree)
            pBuffer->pFree (pBuffer);

        gpRFCOMM->Unlock ();

        return ERROR_SUCCESS;
    }

    fm.f.ucFcs = pBuffer->pBuffer[--pBuffer->cEnd];

    int fValid;

    unsigned short len = 0;
    unsigned char  add_credits = 0;
    int cVal = 3;

    if ((fm.h.ucControl & (~CTL_PF)) == CTL_UIH) {
        cVal = 2;
        fValid = TRUE;

        len = fm.h.ucLength >> 1;

        if ((fm.h.ucLength & EA_BIT) == 0) {
            unsigned char c;
            if (! BufferGetByte (pBuffer, &c))
                fValid = FALSE;
            else
                len = len + (c << 7);
        }

        if ((fm.h.ucControl & CTL_PF) && (! BufferGetByte (pBuffer, &add_credits)))
            fValid = FALSE;
    } else
        fValid = ((fm.h.ucControl == (CTL_SABM | CTL_PF)) || (fm.h.ucControl == (CTL_DISC | CTL_PF)) ||
                  (fm.h.ucControl == (CTL_DM | CTL_PF)) || (fm.h.ucControl == CTL_DM) ||
                  (fm.h.ucControl == (CTL_UA | CTL_PF))) &&
                  (fm.h.ucLength == EA_BIT);

    fValid = fValid && (BufferTotal (pBuffer) == len) && (fm.h.ucAddress & EA_BIT) &&
                                    FCSValidate ((unsigned char *)&fm.h, cVal, fm.f.ucFcs);;

    int fPassedUp = FALSE;

    if (! fValid) {
        IncrHWErr (pSess);
        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] invalid data received - incorrect buffer/FCS failure..."));
    } else {
        if ((fm.h.ucControl & (~CTL_PF)) == CTL_UIH) { // payload or signalling data...
            if ((fm.h.ucAddress >> 2) == 0) {
                if ((fm.h.ucControl & CTL_PF) == 0) {
                    fPassedUp = TRUE;
                    ProcessSignallingData (pSess, pBuffer);
                } else {
                    IncrHWErr (pSess);
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] invalid data received - credit-based FC on signalling channel..."));
                }
            } else {    // Send data up...
                DLCI *pChann = DLCIFromAddress (pSess, fm.h.ucAddress);
                if (pChann) {
                    RFCOMM_CONTEXT *pOwner = pChann->pOwner;
                    RFCOMM_DataUp_Ind pCallback = pOwner->ei.rfcomm_DataUp_Ind;
                    void *pContext = pOwner->pUserContext;

                    fPassedUp = TRUE;

                    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Passing %d bytes up...\n", BufferTotal (pBuffer)));

                    pOwner->AddRef ();
                    gpRFCOMM->Unlock ();

                    IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_data_up_ind : going into rfcomm_DataUp_Ind\n"));

                    __try {
                        pCallback (pContext, pChann, pBuffer, add_credits);
                    } __except (1) {
                        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] Exception in rfcomm_DataUp_Ind\n"));
                    }

                    gpRFCOMM->Lock ();
                    pOwner->DelRef ();

                    IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_data_up_ind : came out of rfcomm_DataUp_Ind\n"));
                } else
                    IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] invalid data received - dlci not found..."));
            }
        } else
            ProcessFrame (pSess, &fm);
    }

    if ((! fPassedUp) && (pBuffer->pFree))
        pBuffer->pFree (pBuffer);

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_data_up_ind: returned ERROR_SUCCESS\n"));
    gpRFCOMM->Unlock ();

    return ERROR_SUCCESS;
}

static int rfcomm_stack_event (void *pUserContext, int iEvent, void *pEventContext) {
    SVSUTIL_ASSERT (pUserContext == gpRFCOMM);

    switch (iEvent) {
    case BTH_STACK_RESET:
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM : Stack up\n"));
        btutil_ScheduleEvent (StackReset, NULL);
        break;

    case BTH_STACK_DOWN:
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM : Stack down\n"));
        btutil_ScheduleEvent (StackDown, NULL);
        break;

    case BTH_STACK_UP:
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM : Stack up\n"));
        btutil_ScheduleEvent (StackUp, NULL);
        break;

    case BTH_STACK_DISCONNECT:
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM : Stack down\n"));
        btutil_ScheduleEvent (StackDisconnect, NULL);
        break;


    default:
        IFDBG(DebugOut (DEBUG_ERROR, L"RFCOMM: unknown stack event. Disconnecting out of paranoia.\n"));
        btutil_ScheduleEvent (StackDown, NULL);
    }

    return ERROR_SUCCESS;
}

static int rfcomm_call_aborted (void *pCallContext, int iError) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_call_aborted\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_call_aborted : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fRunning) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_call_aborted : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Task *pCall = (Task*) btutil_TranslateHandle ((SVSHandle)pCallContext);    
    if (! pCall) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_call_aborted : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (VerifyCall (pCall));

    if (pCall->fPF)
        ClearBusy (pCall->pPhysLink);

    switch (pCall->fWhat) {
    case CALL_L2CAP_LINK_SETUP:
    case CALL_L2CAP_LINK_CONFIG:
    case CALL_RFCOMM_SABM0:
    case CALL_RFCOMM_SABMN:
    case CALL_RFCOMM_DISCN:
    case CALL_RFCOMM_USERDATA:
    case CALL_RFCOMM_SIGNALDATA:
        CloseSession (pCall->pPhysLink, iError, TRUE);
        break;

    default:
        SVSUTIL_ASSERT (0);
    }

    if (gpRFCOMM->fRunning && VerifyCall (pCall))
        CancelCall (pCall, iError, NULL);

    int iRes = ERROR_SUCCESS;

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_call_aborted: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_connect_req_out (void *pCallContext, unsigned short cid, unsigned short result, unsigned short status) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_connect_req_out\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_connect_req_out : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_connect_req_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Task *pCall = (Task*) btutil_TranslateHandle ((SVSHandle)pCallContext);
    if ((! pCall) || (pCall->fWhat != CALL_L2CAP_LINK_SETUP) || (! VerifyLink (pCall->pPhysLink)) || (pCall->pPhysLink->fStage != CONNECTING)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_connect_req_out : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (VerifyCall (pCall));
    SVSUTIL_ASSERT (! pCall->pPhysLink->cid);
    
    Session *pSess = pCall->pPhysLink;

    SVSUTIL_ASSERT (VerifyLink (pSess));

    DeleteCall(pCall);

    if (result == 0) {
        pSess->cid = cid;
        pSess->fStage = CONNECTED;
        int iErr = SendConfigRequest (pSess);
        if ((iErr != ERROR_SUCCESS) && gpRFCOMM->fRunning && VerifyLink (pSess))
            CloseSession (pSess, iErr, TRUE);
    } else {
        int iErr = ERROR_INTERNAL_ERROR;
        if (result == 2)
            iErr = ERROR_UNKNOWN_PORT;
        else if (result == 3)
            iErr = ERROR_ACCESS_DENIED;
        else if (result == 4)
            iErr = ERROR_REMOTE_SESSION_LIMIT_EXCEEDED;

        CloseSession (pSess, iErr, TRUE);
    }

    int iRes = ERROR_SUCCESS;

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_connect_req_out: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_config_req_out (void *pCallContext, unsigned short usResult, unsigned short usOutMTU, unsigned short usOutFlushTO, struct btFLOWSPEC *pOutFlow, int cOptNum, struct btCONFIGEXTENSION **ppExtendedOptions) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_config_req_out\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_config_req_out : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_config_req_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Task *pCall = (Task*) btutil_TranslateHandle ((SVSHandle)pCallContext);
    if ((! pCall) || (pCall->fWhat != CALL_L2CAP_LINK_CONFIG) || (! VerifyLink (pCall->pPhysLink)) || ((pCall->pPhysLink->fStage & CONNECTED) != CONNECTED)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_config_req_out : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (VerifyCall (pCall));

    Session *pSess = pCall->pPhysLink;

    SVSUTIL_ASSERT (VerifyLink (pSess));

    DeleteCall (pCall);

    if (usResult == 0) {
        if (usOutMTU) 
            pSess->outMTU = usOutMTU;
        else if (pSess->outMTU == 0)
            pSess->outMTU = L2CAP_MTU;
        
        pSess->fStage |= CONFIG_REQ_DONE;
        if (pSess->fStage == L2CAPUP)                        
            SessionIsUp (pSess);
    } else
        CloseSession (pSess, ERROR_PROTOCOL_UNREACHABLE, TRUE);

    int iRes = ERROR_SUCCESS;

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_config_req_out: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}


static int rfcomm_connect_response_out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_connect_response_out result = %d\n", result));
    return ERROR_SUCCESS;
}

static int rfcomm_config_response_out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_config_response_out result = %d\n", result));
    return ERROR_SUCCESS;
}

static int rfcomm_data_down_out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_data_down_out\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_data_down_out : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_data_down_out : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    Task *pCall = (Task*) btutil_TranslateHandle ((SVSHandle)pCallContext);
    if (! pCall) {
        IFDBG(DebugOut (pCallContext ? DEBUG_ERROR : DEBUG_RFCOMM_TRACE, L"rfcomm_data_down_out : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    SVSUTIL_ASSERT (VerifyCall (pCall));

    if (! pCall->fData) {    // Ignore it!
        IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_data_down_out (ignored): ERROR_SUCCESS\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SUCCESS;
    }

    SVSUTIL_ASSERT ((pCall->fWhat == CALL_RFCOMM_USERDATA) || (pCall->fWhat == CALL_RFCOMM_SIGNALDATA));

    int iRes = ERROR_SUCCESS;

    if (pCall->fWhat == CALL_RFCOMM_USERDATA) {
        RFCOMM_CONTEXT *pOwner = pCall->pOwner;
        void *pContext = pCall->pContext;

        DeleteCall (pCall);

        RFCOMM_DataDown_Out pCallback = pOwner->c.rfcomm_DataDown_Out;

        pOwner->AddRef ();
        gpRFCOMM->Unlock ();

        IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_data_down_out : going into rfcomm_DataDown_Out\n"));
        __try {
            pCallback (pContext, ERROR_SUCCESS);
        } __except (1) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] rfcomm_data_down_out : Exception in rfcomm_DataDown_Out\n"));
        }

        gpRFCOMM->Lock ();
        pOwner->DelRef ();

        IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_data_down_out : came out of rfcomm_DataDown_Out\n"));
    } else
        IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_data_down_out : Signal data forwarded; deferring processing until response...\n"));

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_data_down_out: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_disconnect_out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_disconnect_out result = %d\n", result));
    return ERROR_SUCCESS;
}

static int StartSession (Session *pSess) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"StartSession %04x%08x\n", pSess->b.NAP, pSess->b.SAP));

    SVSUTIL_ASSERT (gpRFCOMM->fConnected);
    SVSUTIL_ASSERT (pSess->fStage == DORMANT);
    SVSUTIL_ASSERT (VerifyLink (pSess));

    BD_ADDR b = pSess->b;

    Task *pTask = NewTask (CALL_L2CAP_LINK_SETUP, NULL, pSess);
    if (! pTask) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] StartSession :: ERROR_OUTOFMEMORY\n"));
        return ERROR_OUTOFMEMORY;
    }

    SVSUTIL_ASSERT (pTask->hCallContext != SVSUTIL_HANDLE_INVALID);

    AddTask (pTask);

    pSess->fStage = CONNECTING;
    SVSUTIL_ASSERT (pSess->fIncoming == FALSE);

    HANDLE h = gpRFCOMM->hL2CAP;
    SVSHandle hContext = pTask->hCallContext;
    L2CA_ConnectReq_In pCallback = gpRFCOMM->l2cap_if.l2ca_ConnectReq_In;
    gpRFCOMM->AddRef ();
    gpRFCOMM->Unlock ();

    IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"StartSession : going into l2ca_ConnectReq_In\n"));

    int iRes = ERROR_INTERNAL_ERROR;
    __try {
        iRes = pCallback (h, (LPVOID)hContext, RFCOMM_PSM, &b);
    } __except (1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] Exception in l2ca_ConnectReq_In\n"));
    }

    gpRFCOMM->Lock ();
    gpRFCOMM->DelRef ();

    IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"StartSession : came out of l2ca_ConnectReq_In\n"));

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] StartSession :: ERROR_OPERATION_ABORTED\n"));
        return ERROR_OPERATION_ABORTED;
    }

    if (iRes != ERROR_SUCCESS) {
        DeleteCall (pTask);
        if (VerifyLink (pSess))
            pSess->fStage = DORMANT;
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"StartSession %04x%08x returns %d\n", b.NAP, b.SAP, iRes));
    return iRes;
}

static int StartChannel (Session *pSess, DLCI *pChann, void *pUserContext) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"StartChannel %04x%08x dlci 0x%02x\n", pSess->b.NAP, pSess->b.SAP, pSess->fStage != DORMANT ? GetDLCI (pChann) : 0));

    SVSUTIL_ASSERT (pChann->fStage == DORMANT);
    SVSUTIL_ASSERT (! pChann->fLocal);
    SVSUTIL_ASSERT (pSess == pChann->pSess);
    SVSUTIL_ASSERT (VerifyLink (pSess));
    SVSUTIL_ASSERT (VerifyChannel (pSess, pChann));

    Task *pTask = NewTask (CALL_RFCOMM_SABMN, pChann->pOwner, pSess);
    if (! pTask) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] StartChannel :: ERROR_OUTOFMEMORY\n"));
        return ERROR_OUTOFMEMORY;
    }

    pTask->pContext = pUserContext;
    pTask->channel  = pChann->channel;
    pTask->fLocal   = pChann->fLocal;

    AddTask (pTask);

    pChann->fStage = CONNECTING;

    if ((pSess->fStage == UP) && (! pSess->fBusy))
        return ProcessNextPendingEvent (pSess);
    else if (pSess->fStage == DORMANT)
        return StartSession (pSess);

    return ERROR_SUCCESS;    // Just pinned it there waiting...
}

static void CleanUpRFCOMMState (void) {
    if (gpRFCOMM->pfmdChannels) {
        svsutil_ReleaseFixedEmpty (gpRFCOMM->pfmdChannels);
        gpRFCOMM->pfmdChannels = NULL;
    }

    if (gpRFCOMM->pfmdSessions) {
        svsutil_ReleaseFixedEmpty (gpRFCOMM->pfmdSessions);
        gpRFCOMM->pfmdSessions = NULL;
    }

    if (gpRFCOMM->pfmdCalls) {
        svsutil_ReleaseFixedEmpty (gpRFCOMM->pfmdCalls);
        gpRFCOMM->pfmdCalls = NULL;
    }

    if (gpRFCOMM->pfmdSC) {
        svsutil_ReleaseFixedEmpty (gpRFCOMM->pfmdSC);
        gpRFCOMM->pfmdSC = NULL;
    }

    gpRFCOMM->ReInit ();
}


//
//    API
//
static int check_io_control_parms
(
int cInBuffer,
char *pInBuffer,
int cOutBuffer,
char *pOutBuffer,
int *pcDataReturned,
char *pSpace,
int cSpace
) {
    --cSpace;

    __try {
        if (pcDataReturned) {
            *pcDataReturned = 0;
            memset (pOutBuffer, 0, cOutBuffer);
        } else if (cOutBuffer || pOutBuffer)
            return FALSE;

        int i = 0;
        while (cInBuffer > 0) {
            pSpace[i = (i < cSpace) ? i + 1 : 0] = *pInBuffer++;
            --cInBuffer;
        }
    } __except(1) {
        return FALSE;
    }

    return TRUE;
}

static int rfcomm_ioctl (HANDLE hDeviceContext, int fSelector, int cInBuffer, char *pInBuffer, int cOutBuffer, char *pOutBuffer, int *pcDataReturned) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_ioctl\n"));

    char c;
    if (! check_io_control_parms (cInBuffer, pInBuffer, cOutBuffer, pOutBuffer, pcDataReturned, &c, 1)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_ioctl returns ERROR_INVALID_PARAMETER (exception)\n"));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_ioctl : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fRunning) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_ioctl : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_ioctl : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    int iRes = ERROR_INVALID_OPERATION;

    switch (fSelector) {
    case BTH_STACK_IOCTL_RESERVE_PORT:
        {
            unsigned char ch = 0;
            if ((cInBuffer != sizeof(unsigned char)) || (pOutBuffer && (cOutBuffer != sizeof(unsigned char)))) {
                IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_ioctl : BTH_STACK_IOCTL_RESERVE_PORT : invalid buffers\n"));
                iRes = ERROR_INVALID_PARAMETER;
            } else {
                if (*pInBuffer == 0) {
                    do {
                        ++ch;
                    } while (FindContextByChannel (ch) && (ch < RFCOMM_MAX_CH));
                    if (ch == RFCOMM_MAX_CH) {
                        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_ioctl : BTH_STACK_IOCTL_RESERVE_PORT : bad channel %d\n", *pInBuffer));
                        iRes = ERROR_NO_SYSTEM_RESOURCES;
                        ch = 0;
                    }
                } else if ((*pInBuffer >= RFCOMM_MAX_CH) || FindContextByChannel (*pInBuffer)) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_ioctl : BTH_STACK_IOCTL_RESERVE_PORT : bad channel %d\n", *pInBuffer));
                    if (*pInBuffer >= RFCOMM_MAX_CH)
                        iRes = ERROR_INVALID_PARAMETER;
                    else
                        iRes = ERROR_ALREADY_ASSIGNED;
                } else
                    ch = *pInBuffer;
            }

            if (ch) {
                ServerChannel *pSC = new ServerChannel;
                if (pSC) {
                    pSC->channel = ch;
                    pSC->pNext = pOwner->pReservedChannels;
                    pOwner->pReservedChannels = pSC;

                    if (pcDataReturned) {
                        *pcDataReturned = sizeof(unsigned char);
                        *pOutBuffer = ch;
                    }

                    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Successfully reserved channel %d for context 0x%08x\n", ch, pOwner));
                    iRes = ERROR_SUCCESS;

                } else {
                    IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_ioctl : BTH_STACK_IOCTL_RESERVE_PORT : ERROR_OUTOFMEMORY\n"));
                    iRes = ERROR_OUTOFMEMORY;
                }
            }
        }
        break;

    case BTH_STACK_IOCTL_FREE_PORT:
        {
            if (cInBuffer != sizeof(unsigned char)) {
                IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_ioctl : BTH_STACK_IOCTL_RESERVE_PORT : invalid buffers\n"));
                iRes = ERROR_INVALID_PARAMETER;
            } else {
                ServerChannel *pSC = pOwner->pReservedChannels;
                ServerChannel *pParent = NULL;

                while (pSC && (pSC->channel != (unsigned char)*pInBuffer)) {
                    pParent = pSC;
                    pSC = pSC->pNext;
                }

                if (pSC) {
                    if (pParent)
                        pParent->pNext = pSC->pNext;
                    else
                        pOwner->pReservedChannels = pSC->pNext;


                    delete pSC;

                    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Successfully deleted channel %d for context 0x%08x\n", *pInBuffer, pOwner));
                    iRes = ERROR_SUCCESS;
                } else {
                    IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_ioctl : BTH_STACK_IOCTL_RESERVE_PORT : ERROR_NOT_FOUND\n"));
                    iRes = ERROR_NOT_FOUND;
                }
            }
        }
        break;

    case BTH_STACK_IOCTL_GET_CONNECTED:
        if ((cInBuffer == 0) && (cOutBuffer == 4)) {
            iRes = ERROR_SUCCESS;

            int iCount = gpRFCOMM->fConnected;

            pOutBuffer[0] = iCount & 0xff;
            pOutBuffer[1] = (iCount >> 8) & 0xff;
            pOutBuffer[2] = (iCount >> 16) & 0xff;
            pOutBuffer[3] = (iCount >> 24) & 0xff;
            *pcDataReturned = 4;
        } else
            iRes = ERROR_INVALID_PARAMETER;
        break;
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_ioctl: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_abort_call (HANDLE hDeviceContext, void *pCallContext) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_abort_call\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_abort_call : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fRunning) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_abort_call : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_abort_call : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    int iRes = ERROR_NOT_FOUND;
    Task *pTask = gpRFCOMM->pCalls;
    while (pTask && ((pTask->pOwner != pOwner) || (pTask->pContext != pCallContext)))
        pTask = pTask->pNext;

    if (pTask) {
        if (pTask->fPF && pTask->pPhysLink)
            pTask->pPhysLink->fWaitAck = TRUE;

        DeleteCall (pTask);
        iRes = ERROR_SUCCESS;
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_abort_call: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_create_channel (HANDLE hDeviceContext, BD_ADDR *pba, unsigned char channel, HANDLE *phConnection) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_create_channel\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_create_channel : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_create_channel : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_create_channel : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    int iRes = ERROR_NOT_FOUND;

    Session *pSess = gpRFCOMM->pPhysLinks;
    while (pSess && (pSess->b != *pba))
        pSess = pSess->pNext;

    if (! pSess) {
        pSess = NewSession (pba);
        if (pSess) {
            pSess->pNext = gpRFCOMM->pPhysLinks;
            gpRFCOMM->pPhysLinks = pSess;
        } else {
            IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] rfcomm_create_channel :: ERROR_OUTOFMEMORY\n"));
            gpRFCOMM->Unlock ();
            return ERROR_OUTOFMEMORY;
        }
    }
    
    DLCI *pDLCI = pSess->pLogLinks;

    while (pDLCI && ((pDLCI->channel != channel) || (pDLCI->fLocal)))
        pDLCI = pDLCI->pNext;

    if (! pDLCI) {
        pDLCI = NewLog (pSess, channel, FALSE, pOwner);
        if (pDLCI) {
            pDLCI->pNext = pSess->pLogLinks;
            pSess->pLogLinks = pDLCI;

            *phConnection = (HANDLE)pDLCI;

            IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_create_channel: handle 0x%08x created for channel = %d\n", pDLCI, channel));

            iRes = ERROR_SUCCESS;
        } else {
            IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] ERROR_OUTOFMEMORY in rfcomm_create_channel (channel creation!)\n"));
            iRes = ERROR_OUTOFMEMORY;
            pOwner = NULL;
        }
    } else {
        IFDBG(DebugOut (DEBUG_WARN, L"[RFCOMM] ERROR_ALREADY_EXISTS in rfcomm_create_channel (channel creation!)\n"));
        iRes = ERROR_ALREADY_EXISTS;
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_create_channel: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_get_channel_address (HANDLE hDeviceContext, HANDLE hConnection, BD_ADDR *pba, unsigned char *pchannel, int *pfLocal) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_get_channel_address\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_get_channel_address : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_get_channel_address : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_get_channel_address : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    int iRes = ERROR_NOT_FOUND;

    DLCI *pDLCI = HandleToDLCI (hConnection, pOwner);

    if (pDLCI) {
        *pba = pDLCI->pSess->b;
        *pchannel = pDLCI->channel;
        *pfLocal = pDLCI->fLocal;

        iRes = ERROR_SUCCESS;
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_get_channel_address: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_connect_request_in (HANDLE hDeviceContext, void *pCallContext, HANDLE hConnection) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_connect_request_in\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_connect_request_in : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_connect_request_in : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_connect_request_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    int iRes = ERROR_NOT_FOUND;

    DLCI *pDLCI = HandleToDLCI (hConnection, pOwner);

    if (pDLCI) {
        if ((pDLCI->fStage == DORMANT) && (! pDLCI->fLocal))
            iRes = StartChannel (pDLCI->pSess, pDLCI, pCallContext);
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_connect_request_in: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_connect_response_in (HANDLE hDeviceContext, void *pCallContext, HANDLE hConnection, int fAccept) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_connect_response_in\n"));

    if (pCallContext != NULL) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] callbacks in rfcomm_connect_response_in NOT supported!\n"));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_connect_response_in : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_connect_response_in : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_connect_response_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    int iRes = ERROR_NOT_FOUND;

    DLCI *pDLCI = HandleToDLCI (hConnection, pOwner);

    if (pDLCI && (pDLCI->fStage == CONNECTED)) {
        Session *pSess = pDLCI->pSess;
        unsigned char dlci = GetDLCI (pDLCI);
        if (fAccept)
            pDLCI->fStage = UP;
        else
            DeleteChannel (pSess, pDLCI, RFCOMM_SIGNAL_NONE, ERROR_REQUEST_REFUSED, NULL);

        if (gpRFCOMM->fConnected && VerifyLink (pSess))
            iRes = SendFrame (NULL, pSess->cid, dlci, pSess->fIncoming, fAccept ? CTL_UA : CTL_DM);
        else
            iRes = ERROR_DEVICE_NOT_CONNECTED;
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_connect_response_in: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_data_down_in (HANDLE hDeviceContext, void *pCallContext, HANDLE hConnection, BD_BUFFER *pBuffer, int add_credits) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_data_down_in\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_data_down_in : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_data_down_in : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_data_down_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    if ((pBuffer->cStart < (int)(gpRFCOMM->cDataHeaders + sizeof(TS_FRAME_HEADER_X))) &&
            (pBuffer->cEnd > (int)(pBuffer->cSize - gpRFCOMM->cDataTrailers - sizeof(TS_FRAME_FOOTER))) ||
            (BufferTotal (pBuffer) > RFCOMM_MAX_DATA)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_data_down_in : ERROR_INVALID_PARAMETER : sizing incorrect\n"));
        gpRFCOMM->Unlock ();
        return ERROR_INVALID_PARAMETER;
    }

    int iRes = ERROR_NOT_FOUND;

    DLCI *pDLCI = HandleToDLCI (hConnection, pOwner);

    if (pDLCI) {
        Task *pCall = NewTask (CALL_RFCOMM_USERDATA, pOwner, pDLCI->pSess);
        if (pCall) {
            pCall->pContext = pCallContext;

            AddTask (pCall);

            int cDataSize = BufferTotal (pBuffer);
            int cOldStart = pBuffer->cStart;

            pBuffer->cStart -= sizeof(TS_FRAME_HEADER);

            if (cDataSize > 127)
                --pBuffer->cStart;

            if (add_credits)
                --pBuffer->cStart;

            TS_FRAME_HEADER *pHeader = (TS_FRAME_HEADER *)&pBuffer->pBuffer[pBuffer->cStart];
            unsigned char *pucExtStuff = (unsigned char *)&pHeader[1];

            pHeader->ucAddress = EA_BIT | (pDLCI->pSess->fIncoming ? 0 : CR_BIT) | (GetDLCI (pDLCI) << 2);
            pHeader->ucControl = CTL_UIH | (add_credits ? CTL_PF : 0);
            pHeader->ucLength = (unsigned char) (cDataSize << 1);

            if (cDataSize > 127)
                *pucExtStuff++ = (unsigned char)(cDataSize >> 7);
            else
                pHeader->ucLength |= EA_BIT;

            if (add_credits)
                *pucExtStuff++ = (unsigned char)add_credits;

            SVSUTIL_ASSERT (pucExtStuff == &pBuffer->pBuffer[cOldStart]);

            pBuffer->pBuffer[pBuffer->cEnd++] = FCSCompute (pBuffer->pBuffer + pBuffer->cStart, 2);

            iRes = SendFrame (pCall, pDLCI->pSess->cid, pBuffer);

            if ((iRes != ERROR_SUCCESS) && gpRFCOMM->fRunning)
                DeleteCall (pCall);
        } else
            iRes = ERROR_OUTOFMEMORY;
    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_data_down_in: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_disconnect_in (HANDLE hDeviceContext, void *pContext, HANDLE hConnection) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_disconnect_in\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_disconnect_in : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_disconnect_in : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    int iRes = ERROR_NOT_FOUND;

    DLCI *pDLCI = pOwner ? HandleToDLCI (hConnection, pOwner) : NULL;
    if (pDLCI) {
        iRes = ERROR_SUCCESS;
        if (! DisconnectPeer (pDLCI->pSess, pDLCI, pContext))
            DeleteChannel (pDLCI->pSess, pDLCI, RFCOMM_SIGNAL_RESPONSE, ERROR_GRACEFUL_DISCONNECT, pContext);

    }

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_disconnect_in: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

//
//    Signalling channel APIs
//
static int rfcomm_msc_in (HANDLE hDeviceContext, void *pCallContext, HANDLE hConnection, unsigned char v24, unsigned char bs) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_msc_in\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_msc_in : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_msc_in : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_msc_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    DLCI *pDLCI = HandleToDLCI (hConnection, pOwner);
    if ((! pDLCI) || (pDLCI->fStage != UP)) {
        int iResX = pDLCI ? ERROR_SERVICE_NEVER_STARTED : ERROR_NOT_FOUND;

        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_msc_in : %d\n", iResX));
        gpRFCOMM->Unlock ();
        return iResX;
    }

    Task *pTask = NewTask (pOwner, pDLCI->pSess, pCallContext, MSG_MSC);
    if (! pTask) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_msc_in : ERROR_OUTOFMEMORY\n"));
        gpRFCOMM->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    pTask->channel = pDLCI->channel;
    pTask->fLocal  = pDLCI->fLocal;

    AddTask (pTask);

    unsigned char body[5];
    unsigned char len = (bs == 0xff) ? 2 : 3;

    body[0] = (MSG_MSC << 2) | EA_BIT | CR_BIT;
    body[1] = (len << 1) | EA_BIT;
    body[2] = (GetDLCI (pDLCI) << 2) | EA_BIT | CR_BIT;
    body[3] = v24;
    body[4] = bs;

    int iRes = SendFrame (pTask, pDLCI->pSess->cid, pDLCI->pSess->fIncoming, len + 2, body);

    if ((iRes != ERROR_SUCCESS) && gpRFCOMM->fRunning)
        DeleteCall (pTask);

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_msc_in: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_rls_in (HANDLE hDeviceContext, void *pCallContext, HANDLE hConnection, unsigned char rls) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_rls_in\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rls_in : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rls_in : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rls_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    DLCI *pDLCI = HandleToDLCI (hConnection, pOwner);
    if ((! pDLCI) || (pDLCI->fStage != UP)) {
        int iResX = pDLCI ? ERROR_SERVICE_NEVER_STARTED : ERROR_NOT_FOUND;

        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rls_in : %d\n", iResX));
        gpRFCOMM->Unlock ();
        return iResX;
    }

    Task *pTask = NewTask (pOwner, pDLCI->pSess, pCallContext, MSG_RLS);
    if (! pTask) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rls_in : ERROR_OUTOFMEMORY\n"));
        gpRFCOMM->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    pTask->channel = pDLCI->channel;
    pTask->fLocal  = pDLCI->fLocal;

    AddTask (pTask);

    unsigned char body[4];

    body[0] = (MSG_RLS << 2) | EA_BIT | CR_BIT;
    body[1] = (2 << 1) | EA_BIT;
    body[2] = (GetDLCI (pDLCI) << 2) | EA_BIT | CR_BIT;
    body[3] = rls;

    int iRes = SendFrame (pTask, pDLCI->pSess->cid, pDLCI->pSess->fIncoming, 4, body);

    if ((iRes != ERROR_SUCCESS) && gpRFCOMM->fRunning)
        DeleteCall (pTask);

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_rls_in: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_fc_in (HANDLE hDeviceContext, void *pCallContext, HANDLE hConnection, unsigned char fcOn) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_fc_in\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_fc_in : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_fc_in : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_fc_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    DLCI *pDLCI = HandleToDLCI (hConnection, pOwner);
    if ((! pDLCI) || (pDLCI->pSess->fStage != UP)) {
        int iResX = pDLCI ? ERROR_SERVICE_NEVER_STARTED : ERROR_NOT_FOUND;

        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rls_in : %d\n", iResX));
        gpRFCOMM->Unlock ();
        return iResX;
    }

    fcOn = fcOn ? MSG_FCON : MSG_FCOFF;

    Task *pTask = NewTask (pOwner, pDLCI->pSess, pCallContext, fcOn);
    if (! pTask) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_fc_in : ERROR_OUTOFMEMORY\n"));
        gpRFCOMM->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    AddTask (pTask);

    unsigned char body[2];

    body[0] = (fcOn << 2) | EA_BIT | CR_BIT;
    body[1] = EA_BIT;

    int iRes = SendFrame (pTask, pDLCI->pSess->cid, pDLCI->pSess->fIncoming, 2, body);

    if ((iRes != ERROR_SUCCESS) && gpRFCOMM->fRunning)
        DeleteCall (pTask);

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_fc_in: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_pnreq_in (HANDLE hDeviceContext, void *pCallContext, HANDLE hConnection, unsigned char priority, unsigned short n1, int use_credit_fc_in, int initial_credit_in) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_pnreq_in\n"));

    SVSUTIL_ASSERT ((use_credit_fc_in == 0) || (use_credit_fc_in == RFCOMM_PN_CREDIT_IN));
    SVSUTIL_ASSERT (initial_credit_in <= RFCOMM_PN_CREDIT_MAX);
    SVSUTIL_ASSERT ((initial_credit_in == 0) || (use_credit_fc_in == RFCOMM_PN_CREDIT_IN));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_pnreq_in : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_pnreq_in : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_pnreq_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    DLCI *pDLCI = HandleToDLCI (hConnection, pOwner);
    if (! pDLCI) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_pnreq_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    Task *pTask = NewTask (pOwner, pDLCI->pSess, pCallContext, MSG_PN);
    if (! pTask) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_pnreq_in : ERROR_OUTOFMEMORY\n"));
        gpRFCOMM->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    pTask->channel = pDLCI->channel;
    pTask->fLocal  = pDLCI->fLocal;

    AddTask (pTask);

    if (pDLCI->pSess->fStage == UP) {
        SVSUTIL_ASSERT(pDLCI->pSess->outMTU > 0);
        if (n1 > (pDLCI->pSess->outMTU - (sizeof(TS_FRAME_HEADER_X) + sizeof(TS_FRAME_FOOTER))))
            n1 = pDLCI->pSess->outMTU - (sizeof(TS_FRAME_HEADER_X) + sizeof(TS_FRAME_FOOTER));
    } else if (pDLCI->pSess->fStage == DORMANT) {    
        pDLCI->pSess->inMTU = n1 + sizeof(TS_FRAME_HEADER_X) + sizeof(TS_FRAME_FOOTER);
        if (pDLCI->pSess->inMTU < L2CAP_MTU)
            pDLCI->pSess->inMTU = L2CAP_MTU;    
    }

    unsigned char body[10];

    body[0] = (MSG_PN << 2) | EA_BIT | CR_BIT;
    body[1] = (8 << 1) | EA_BIT;
    body[2] = (pDLCI->pSess->fStage == UP) ? GetDLCI (pDLCI) : 0;    // D1-6 !!! This is NOT a !!!
    body[3] = use_credit_fc_in << 4;        // I1-4 CL1-4
    body[4] = priority;                        // P1-6
    body[5] = 0;                            // T1-8
    body[6] = n1 & 0xff;                    // n1 1-8
    body[7] = (n1 >> 8) & 0xff;                // n1 9-16
    body[8] = 0;                            // na1-8
    body[9] = initial_credit_in & 0x7;        // k1-3

    int iRes;
    if (pDLCI->pSess->fStage == UP)
        iRes = SendFrame (pTask, pDLCI->pSess->cid, pDLCI->pSess->fIncoming, 10, body);
    else {
        SVSUTIL_ASSERT (sizeof(pTask->signal_body) >= sizeof(body));
        memcpy (pTask->signal_body, body, sizeof(body));
        pTask->signal_length = sizeof(body);
        pTask->signal_dlci_offset = 2;
        if (pDLCI->pSess->fStage == DORMANT)
            iRes = StartSession (pDLCI->pSess);
        else
            iRes = ERROR_SUCCESS;
    }

    if ((iRes != ERROR_SUCCESS) && gpRFCOMM->fRunning)
        DeleteCall (pTask);

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_pnreq_in: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_pnrsp_in (HANDLE hDeviceContext, void *pCallContext, HANDLE hConnection, unsigned char priority, unsigned short n1, int use_credit_fc_out, int initial_credit_out) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_pnrsp_in\n"));

    SVSUTIL_ASSERT ((use_credit_fc_out == 0) || (use_credit_fc_out == RFCOMM_PN_CREDIT_OUT));
    SVSUTIL_ASSERT (initial_credit_out <= RFCOMM_PN_CREDIT_MAX);
    SVSUTIL_ASSERT ((initial_credit_out == 0) || (use_credit_fc_out == RFCOMM_PN_CREDIT_OUT));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_pnrsp_in : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_pnrsp_in : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_pnrsp_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    DLCI *pDLCI = HandleToDLCI (hConnection, pOwner);
    if (! pDLCI) {
        int iResX = pDLCI ? ERROR_SERVICE_NEVER_STARTED : ERROR_NOT_FOUND;

        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_pnrsp_in : %d\n", iResX));
        gpRFCOMM->Unlock ();
        return iResX;
    }

    unsigned char body[10];

    body[0] = (MSG_PN << 2) | EA_BIT;
    body[1] = (8 << 1) | EA_BIT;
    body[2] = GetDLCI (pDLCI);            // D1-6 !!! This is NOT a !!!
    body[3] = use_credit_fc_out << 4;    // I1-4 CL1-4
    body[4] = priority;                    // P1-6
    body[5] = 0;                        // T1-8
    body[6] = n1 & 0xff;                // n1 1-8
    body[7] = (n1 >> 8) & 0xff;            // n1 9-16
    body[8] = 0;                        // na1-8
    body[9] = initial_credit_out & 0x7;    // k1-3

    int iRes = SendFrame (NULL, pDLCI->pSess->cid, pDLCI->pSess->fIncoming, 10, body);

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_pnrsp_in: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_rpnreq_in (HANDLE hDeviceContext, void *pCallContext, HANDLE hConnection, unsigned short mask, int baud, int data, int stop, int parity, int parity_type, unsigned char flow, unsigned char xon, unsigned char xoff) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_rpnreq_in\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rpnreq_in : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rpnreq_in : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rpnreq_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    DLCI *pDLCI = HandleToDLCI (hConnection, pOwner);
    if ((! pDLCI) || (pDLCI->fStage != UP)) {
        int iResX = pDLCI ? ERROR_SERVICE_NEVER_STARTED : ERROR_NOT_FOUND;

        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rpnreq_in : %d\n", iResX));
        gpRFCOMM->Unlock ();
        return iResX;
    }

    Task *pTask = NewTask (pOwner, pDLCI->pSess, pCallContext, MSG_RPN);
    if (! pTask) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rpnreq_in : ERROR_OUTOFMEMORY\n"));
        gpRFCOMM->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    pTask->channel = pDLCI->channel;
    pTask->fLocal  = pDLCI->fLocal;

    AddTask (pTask);

    unsigned char body[10];
    unsigned char len = (mask == 0x00) ? 1 : 8;

    body[0] = (MSG_RPN << 2) | EA_BIT | CR_BIT;
    body[1] = (len << 1) | EA_BIT;
    body[2] = (GetDLCI (pDLCI) << 2) | EA_BIT | CR_BIT;
    body[3] = mask & RFCOMM_RPN_HAVE_BAUD ? BaudToByte (baud) : 0;
    body[4] = 0;
    if (mask & RFCOMM_RPN_HAVE_DATA)
        body[4] |= DataToByte (data);

    if ((mask & RFCOMM_RPN_HAVE_STOP) && (stop == ONE5STOPBITS))
        body[4] |= (1 << 2);

    if ((mask & RFCOMM_RPN_HAVE_PARITY) && parity)
        body[4] |= (1 << 3);

    if (mask & RFCOMM_RPN_HAVE_PT)
        body[4] |= PTToByte (parity_type) << 4;

    body[5] = flow;
    body[6] = (mask & RFCOMM_RPN_HAVE_XON) ? xon : 0;
    body[7] = (mask & RFCOMM_RPN_HAVE_XOFF) ? xoff : 0;
    body[8] = mask & 0xff;
    body[9] = (mask >> 8) & 0xff;

    int iRes = SendFrame (pTask, pDLCI->pSess->cid, pDLCI->pSess->fIncoming, len + 2, body);

    if ((iRes != ERROR_SUCCESS) && gpRFCOMM->fRunning)
        DeleteCall (pTask);

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_rpnreq_in: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

static int rfcomm_rpnrsp_in (HANDLE hDeviceContext, void *pCallContext, HANDLE hConnection, unsigned short mask, int baud, int data, int stop, int parity, int parity_type, unsigned char flow, unsigned char xon, unsigned char xoff) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_rpnrsp_in\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rpnrsp_in : ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (! gpRFCOMM->fConnected) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rpnrsp_in : ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = VerifyOwner ((RFCOMM_CONTEXT *)hDeviceContext);
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rpnrsp_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    DLCI *pDLCI = HandleToDLCI (hConnection, pOwner);
    if (! pDLCI) {
        IFDBG(DebugOut (DEBUG_ERROR, L"rfcomm_rpnrsp_in : ERROR_NOT_FOUND\n"));
        gpRFCOMM->Unlock ();
        return ERROR_NOT_FOUND;
    }

    unsigned char body[10];
    unsigned char len = (mask == 0x00) ? 1 : 8;

    body[0] = (MSG_RPN << 2) | EA_BIT;
    body[1] = (len << 1) | EA_BIT;
    body[2] = (GetDLCI (pDLCI) << 2) | EA_BIT | CR_BIT;
    body[3] = mask & RFCOMM_RPN_HAVE_BAUD ? BaudToByte (baud) : 0;
    body[4] = 0;
    if (mask & RFCOMM_RPN_HAVE_DATA)
        body[4] |= DataToByte (data);

    if ((mask & RFCOMM_RPN_HAVE_STOP) && (stop == ONE5STOPBITS))
        body[4] |= (1 << 2);

    if ((mask & RFCOMM_RPN_HAVE_PARITY) && parity)
        body[4] |= (1 << 3);

    if (mask & RFCOMM_RPN_HAVE_PT)
        body[4] |= PTToByte (parity_type) << 4;

    body[5] = flow;
    body[6] = (mask & RFCOMM_RPN_HAVE_XON) ? xon : 0;
    body[7] = (mask & RFCOMM_RPN_HAVE_XOFF) ? xoff : 0;
    body[8] = mask & 0xff;
    body[9] = (mask >> 8) & 0xff;

    int iRes = SendFrame (NULL, pDLCI->pSess->cid, pDLCI->pSess->fIncoming, len + 2, body);

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"rfcomm_rpnrsp_in: returned %d\n", iRes));
    gpRFCOMM->Unlock ();

    return iRes;
}

//
//    Public functions
//
int rfcomm_CreateDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM : rfcomm_CreateDriverInstance\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"RFCOMM : not initialized!\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (gpRFCOMM->fRunning) {
        IFDBG(DebugOut (DEBUG_ERROR, L"RFCOMM : already initialized!\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_ALREADY_RUNNING;
    }

    SVSUTIL_ASSERT (! gpRFCOMM->pfmdChannels);
    SVSUTIL_ASSERT (! gpRFCOMM->pfmdSessions);
    SVSUTIL_ASSERT (! gpRFCOMM->pfmdCalls);
    SVSUTIL_ASSERT (! gpRFCOMM->pfmdSC);
    SVSUTIL_ASSERT (! gpRFCOMM->pPhysLinks);
    SVSUTIL_ASSERT (! gpRFCOMM->pCalls);
    SVSUTIL_ASSERT (! gpRFCOMM->pContexts);

    gpRFCOMM->ReInit ();

    gpRFCOMM->pfmdChannels = svsutil_AllocFixedMemDescr (sizeof(DLCI), RFCOMM_SCALE);
    gpRFCOMM->pfmdSessions = svsutil_AllocFixedMemDescr (sizeof(Session), RFCOMM_SCALE);
    gpRFCOMM->pfmdCalls    = svsutil_AllocFixedMemDescr (sizeof(Task), RFCOMM_SCALE);
    gpRFCOMM->pfmdSC       = svsutil_AllocFixedMemDescr (sizeof(ServerChannel), RFCOMM_SCALE);

    if (! (gpRFCOMM->pfmdChannels && gpRFCOMM->pfmdSessions && gpRFCOMM->pfmdCalls && gpRFCOMM->pfmdSC)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"RFCOMM : OOM in init\n"));
        CleanUpRFCOMMState ();
        gpRFCOMM->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    HKEY hk;
    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_BASE, L"software\\Microsoft\\bluetooth\\rfcomm", 0, KEY_READ, &hk)) {
        DWORD dwVal;
        DWORD dwType = REG_DWORD;
        DWORD dwSize = sizeof(dwVal);

        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"Timeout", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD))
            gpRFCOMM->dwLinger = dwVal;

        dwSize = sizeof(dwVal);
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"DiscT1", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD))
            gpRFCOMM->dwDiscT1 = dwVal;

        dwSize = sizeof(dwVal);
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"SabmT1", NULL, &dwType, (LPBYTE) &dwVal, &dwSize)) && (dwType == REG_DWORD))
            gpRFCOMM->dwSabmT1 = dwVal;

        RegCloseKey (hk);
    }

    L2CAP_EVENT_INDICATION lei;
    memset (&lei, 0, sizeof(lei));

    L2CAP_CALLBACKS lc;
    memset (&lc, 0, sizeof(lc));

    lei.l2ca_StackEvent = rfcomm_stack_event;
    lei.l2ca_ConfigInd = rfcomm_config_ind;
    lei.l2ca_ConnectInd = rfcomm_connect_ind;
    lei.l2ca_DisconnectInd = rfcomm_disconnect_ind;
    lei.l2ca_DataUpInd = rfcomm_data_up_ind;

    lc.l2ca_CallAborted = rfcomm_call_aborted;

    lc.l2ca_ConfigReq_Out = rfcomm_config_req_out;
    lc.l2ca_ConfigResponse_Out = rfcomm_config_response_out;
    lc.l2ca_ConnectReq_Out = rfcomm_connect_req_out;
    lc.l2ca_ConnectResponse_Out = rfcomm_connect_response_out;
    lc.l2ca_DataDown_Out = rfcomm_data_down_out;
    lc.l2ca_Disconnect_Out = rfcomm_disconnect_out;


    int iRes;
    if (ERROR_SUCCESS != (iRes =
            L2CAP_EstablishDeviceContext (gpRFCOMM, RFCOMM_PSM, &lei, &lc, &gpRFCOMM->l2cap_if,
            &gpRFCOMM->cDataHeaders, &gpRFCOMM->cDataTrailers, &gpRFCOMM->hL2CAP))) {
        CleanUpRFCOMMState ();
        IFDBG(DebugOut (DEBUG_ERROR, L"RFCOMM : Could not connect to L2CAP, error %d\n", iRes));
        gpRFCOMM->Unlock ();
        return iRes;
    }

    if (! (gpRFCOMM->l2cap_if.l2ca_AbortCall && gpRFCOMM->l2cap_if.l2ca_ConfigReq_In && gpRFCOMM->l2cap_if.l2ca_ConfigResponse_In &&
        gpRFCOMM->l2cap_if.l2ca_ConnectReq_In && gpRFCOMM->l2cap_if.l2ca_ConnectResponse_In && gpRFCOMM->l2cap_if.l2ca_DataDown_In &&
        gpRFCOMM->l2cap_if.l2ca_Disconnect_In)) {
        L2CAP_CloseDeviceContext (gpRFCOMM->hL2CAP);
        CleanUpRFCOMMState ();
        IFDBG(DebugOut (DEBUG_ERROR, L"RFCOMM : Inadequate L2CAP support, error %d\n", iRes));
        gpRFCOMM->Unlock ();
        return iRes;
    }

    gpRFCOMM->fRunning = TRUE;

    GetConnectionState ();

    IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"RFCOMM: initialization successful\n"));
    gpRFCOMM->Unlock ();

    return ERROR_SUCCESS;
}

int rfcomm_CloseDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"rfcomm_CloseDriverInstance :: entered\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"rfcomm_CloseDriverInstance :: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpRFCOMM->Lock ();
    if (! gpRFCOMM->fRunning) {
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"rfcomm_CloseDriverInstance :: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpRFCOMM->fRunning = FALSE;
    gpRFCOMM->fConnected = FALSE;

    // Abort outstanding requests
    while (gpRFCOMM->pCalls)
        CancelCall (gpRFCOMM->pCalls, ERROR_OPERATION_ABORTED, NULL);

    // Signal upper layers of the stack; free the handles, too
    while (gpRFCOMM->pContexts) {
        RFCOMM_CONTEXT *pThis = gpRFCOMM->pContexts;
        gpRFCOMM->pContexts = pThis->pNext;

        while (pThis->pReservedChannels) {
            ServerChannel *pNext = pThis->pReservedChannels->pNext;
            delete pThis->pReservedChannels;
            pThis->pReservedChannels = pNext;
        }

        pThis->fDefaultServer = FALSE;

        void *pUser = pThis->pUserContext;
        BT_LAYER_STACK_EVENT_IND pCallback = pThis->ei.rfcomm_StackEvent;

        if (pCallback) {
            pThis->AddRef ();
            gpRFCOMM->Unlock ();

            IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_CloseDriverInstance : going into rfcomm_StackEvent\n"));

            __try {
                pCallback (pUser, BTH_STACK_DOWN, NULL);
            } __except (1) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] rfcomm_CloseDriverInstance :: exception in StackEvent\n"));
            }

            gpRFCOMM->Lock ();
            pThis->DelRef ();

            IFDBG(DebugOut (DEBUG_RFCOMM_CALLBACK, L"rfcomm_CloseDriverInstance : came out of rfcomm_StackEvent\n"));
        }

        while (pThis->GetRefCount () > 1) {
            IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Waiting for ref count in rfcomm_CloseDriverInstance\n"));
            gpRFCOMM->Unlock ();
            Sleep (100);
            gpRFCOMM->Lock ();
        }

        delete pThis;
    }

    // Wait for the ref count to drop now...
    while (gpRFCOMM->GetRefCount () > 1) {
        IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Waiting for ref count in rfcomm_CloseDriverInstance\n"));
        gpRFCOMM->Unlock ();
        Sleep (100);
        gpRFCOMM->Lock ();
    }

    // Close lower layer handles
    if (gpRFCOMM->hL2CAP)
        L2CAP_CloseDeviceContext (gpRFCOMM->hL2CAP);

    CleanUpRFCOMMState ();

    IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"rfcomm_CloseDriverInstance :: ERROR_SUCCESS\n"));
    gpRFCOMM->Unlock ();

    return ERROR_SUCCESS;
}

int rfcomm_InitializeOnce (void) {
    IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM init once:: entered\n"));

    SVSUTIL_ASSERT (! gpRFCOMM);

    if (gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"RFCOMM init once:: ERROR_ALREADY_EXISTS\n"));
        return ERROR_ALREADY_EXISTS;
    }

    gpRFCOMM = new RFCOMM;

    if (gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM init once:: ERROR_SUCCESS\n"));
        return ERROR_SUCCESS;
    }

    IFDBG(DebugOut (DEBUG_ERROR, L"RFCOMM init once:: ERROR_OUTOFMEMORY\n"));
    return ERROR_OUTOFMEMORY;
}

int rfcomm_UninitializeOnce (void) {
    IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM uninit:: entered\n"));
    SVSUTIL_ASSERT (gpRFCOMM);

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_ERROR, L"RFCOMM uninit:: ERROR_SERVICE_DOES_NOT_EXIST\n"));
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    gpRFCOMM->Lock ();

    if (gpRFCOMM->fRunning) {
        IFDBG(DebugOut (DEBUG_ERROR, L"RFCOMM uninit:: ERROR_DEVICE_IN_USE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_DEVICE_IN_USE;
    }

    RFCOMM *pRFCOMM = gpRFCOMM;
    gpRFCOMM = NULL;
    pRFCOMM->Unlock ();

    delete pRFCOMM;

    IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM uninit:: ERROR_SUCCESS\n"));
    return ERROR_SUCCESS;
}

int RFCOMM_EstablishDeviceContext
(
void                    *pUserContext,        /* IN */
unsigned char            channel,            /* IN */
RFCOMM_EVENT_INDICATION    *pInd,                /* IN */
RFCOMM_CALLBACKS        *pCall,                /* IN */
RFCOMM_INTERFACE        *pInt,                /* OUT */
int                        *pcDataHeaders,        /* OUT */
int                        *pcDataTrailers,    /* OUT */
HANDLE                    *phDeviceContext    /* OUT */
) {
    IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_EstablishDeviceContext :: entered\n"));

    if (pCall->rfcomm_ConnectResponse_Out || pCall->rfcomm_RPNRSP_Out || pCall->rfcomm_PNRSP_Out) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] callbacks in rfcomm_connect_response_in NOT supported!\n"));
        return ERROR_INVALID_PARAMETER;
    }

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_EstablishDeviceContext :: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpRFCOMM->Lock ();
    if (! gpRFCOMM->fRunning) {
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_EstablishDeviceContext :: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pCtx = NULL;

    if (channel == RFCOMM_CHANNEL_ALL)
        pCtx = FindContextForDefault ();
    else if ((channel != RFCOMM_CHANNEL_MULTIPLE) && (channel != RFCOMM_CHANNEL_CLIENT_ONLY)) {
        if ((channel > 0) && (channel < RFCOMM_MAX_CH))
            pCtx = FindContextByChannel (channel);
        else {
            IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_EstablishDeviceContext :: ERROR_INVALID_PARAMETER (channel)\n"));
            gpRFCOMM->Unlock ();
            return ERROR_INVALID_PARAMETER;
        }
    }

    if (pCtx) {
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_EstablishDeviceContext :: ERROR_ALREADY_EXISTS\n"));
        gpRFCOMM->Unlock ();
        return ERROR_ALREADY_EXISTS;
    }

    ServerChannel *pSC = NULL;
    if ((channel > 0) && (channel < RFCOMM_MAX_CH)) {
        pSC = new ServerChannel;
        if (! pSC) {
            IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_EstablishDeviceContext :: ERROR_OUTOFMEMORY\n"));
            gpRFCOMM->Unlock ();
            return ERROR_OUTOFMEMORY;
        }

        pSC->channel = channel;
        pSC->pNext = NULL;
    }

    pCtx = new RFCOMM_CONTEXT;

    if (! pCtx) {
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_EstablishDeviceContext :: ERROR_OUTOFMEMORY\n"));
        gpRFCOMM->Unlock ();
        return ERROR_OUTOFMEMORY;
    }

    pCtx->pReservedChannels = pSC;
    pCtx->fDefaultServer    = (channel == RFCOMM_CHANNEL_ALL);

    pCtx->c = *pCall;
    pCtx->ei = *pInd;
    pCtx->pUserContext = pUserContext;
    pCtx->pNext = gpRFCOMM->pContexts;
    gpRFCOMM->pContexts = pCtx;

    *pcDataHeaders = gpRFCOMM->cDataHeaders + sizeof (TS_FRAME_HEADER_X);
    *pcDataTrailers = gpRFCOMM->cDataTrailers + sizeof (TS_FRAME_FOOTER);

    *phDeviceContext = (HANDLE)pCtx;

    pInt->rfcomm_AbortCall = rfcomm_abort_call;
    pInt->rfcomm_CreateChannel = rfcomm_create_channel;
    pInt->rfcomm_GetChannelAddress = rfcomm_get_channel_address;
    pInt->rfcomm_ConnectRequest_In = rfcomm_connect_request_in;
    pInt->rfcomm_ConnectResponse_In = rfcomm_connect_response_in;
    pInt->rfcomm_DataDown_In = rfcomm_data_down_in;
    pInt->rfcomm_Disconnect_In = rfcomm_disconnect_in;
    pInt->rfcomm_FC_In = rfcomm_fc_in;
    pInt->rfcomm_ioctl = rfcomm_ioctl;
    pInt->rfcomm_MSC_In = rfcomm_msc_in;
    pInt->rfcomm_PNREQ_In = rfcomm_pnreq_in;
    pInt->rfcomm_PNRSP_In = rfcomm_pnrsp_in;
    pInt->rfcomm_RLS_In = rfcomm_rls_in;
    pInt->rfcomm_RPNREQ_In = rfcomm_rpnreq_in;
    pInt->rfcomm_RPNRSP_In = rfcomm_rpnrsp_in;

    IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_EstablishDeviceContext :: ERROR_SUCCESS\n"));
    gpRFCOMM->Unlock ();

    return ERROR_SUCCESS;
}

int RFCOMM_CloseDeviceContext
(
HANDLE                    hDeviceContext        /* IN */
) {
    IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_CloseDeviceContext :: entered\n"));

    if (! gpRFCOMM) {
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_CloseDeviceContext :: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpRFCOMM->Lock ();
    if (! gpRFCOMM->fRunning) {
        IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_CloseDeviceContext :: ERROR_SERVICE_NOT_ACTIVE\n"));
        gpRFCOMM->Unlock ();
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    RFCOMM_CONTEXT *pOwner = gpRFCOMM->pContexts;
    RFCOMM_CONTEXT *pParent = NULL;
    while (pOwner && pOwner != hDeviceContext) {
        pParent = pOwner;
        pOwner = pOwner->pNext;
    }
    
    if (! pOwner) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[RFCOMM] RFCOMM_CloseDeviceContext : NOT FOUND!\n"));
        gpRFCOMM->Unlock ();

        return ERROR_NOT_FOUND;
    }

    if (! pParent)
        gpRFCOMM->pContexts = pOwner->pNext;
    else
        pParent->pNext = pOwner->pNext;

    while (pOwner->pReservedChannels) {
        ServerChannel *pNext = pOwner->pReservedChannels->pNext;
        delete pOwner->pReservedChannels;
        pOwner->pReservedChannels = pNext;
    }

    pOwner->fDefaultServer = FALSE;

    // First, cancel all the owned calls. This prevents asserting during the session close
    Task *pCall = gpRFCOMM->pCalls;
    while (pCall && gpRFCOMM->fRunning) {
        if (pCall->pOwner == pOwner) {
            pCall->pOwner = NULL;
            if (pCall->fPF && pCall->pPhysLink)
                pCall->pPhysLink->fWaitAck = TRUE;

            CancelCall (pCall, ERROR_PROCESS_ABORTED, pOwner);
            pCall = gpRFCOMM->pCalls;
        } else
            pCall = pCall->pNext;
    }

    Session *pSess = gpRFCOMM->pPhysLinks;
    while (pSess && gpRFCOMM->fRunning) {
        DLCI *pChann = pSess->pLogLinks;
        while (pChann && pChann->pOwner != pOwner)
            pChann = pChann->pNext;

        if (pChann) {
            DeleteChannel (pSess, pChann, RFCOMM_SIGNAL_NONE, ERROR_PROCESS_ABORTED, NULL);
            pSess = gpRFCOMM->pPhysLinks;
        } else
            pSess = pSess->pNext;
    }

    while (pOwner->GetRefCount () > 1) {
        IFDBG(DebugOut (DEBUG_RFCOMM_TRACE, L"Waiting for ref count in RFCOMM_CloseDeviceContext\n"));
        gpRFCOMM->Unlock ();
        Sleep (100);
        gpRFCOMM->Lock ();
    }

    delete pOwner;

    IFDBG(DebugOut (DEBUG_RFCOMM_INIT, L"RFCOMM_CloseDeviceContext :: ERROR_SUCCESS\n"));
    gpRFCOMM->Unlock ();

    return ERROR_SUCCESS;
}

void *DLCI::operator new (size_t iSize) {
    SVSUTIL_ASSERT (iSize == sizeof(DLCI));
    return svsutil_GetFixed (gpRFCOMM->pfmdChannels);
}

void DLCI::operator delete (void *ptr) {
    SVSUTIL_ASSERT (! HandleToDLCI ((HANDLE)ptr, ((DLCI *)ptr)->pOwner));
    svsutil_FreeFixed (ptr, gpRFCOMM->pfmdChannels);
}

void *Task::operator new (size_t iSize) {
    SVSUTIL_ASSERT (iSize == sizeof(Task));

    return svsutil_GetFixed (gpRFCOMM->pfmdCalls);
}

void Task::operator delete (void *ptr) {
    SVSUTIL_ASSERT (((Task *)ptr)->hCallContext == SVSUTIL_HANDLE_INVALID);
    SVSUTIL_ASSERT (! VerifyCall ((Task *)ptr));
    svsutil_FreeFixed (ptr, gpRFCOMM->pfmdCalls);
}

void *Session::operator new (size_t iSize) {
    SVSUTIL_ASSERT (iSize == sizeof(Session));

    return svsutil_GetFixed (gpRFCOMM->pfmdSessions);
}

void Session::operator delete (void *ptr) {
    SVSUTIL_ASSERT (! VerifyLink ((Session *)ptr));
    svsutil_FreeFixed (ptr, gpRFCOMM->pfmdSessions);
}

void *ServerChannel::operator new (size_t iSize) {
    SVSUTIL_ASSERT (iSize == sizeof(ServerChannel));

    return svsutil_GetFixed (gpRFCOMM->pfmdSC);
}

void ServerChannel::operator delete (void *ptr) {
    svsutil_FreeFixed (ptr, gpRFCOMM->pfmdSC);
}
//
//    Console output
//
#if defined (BTH_CONSOLE)

int rfcomm_ProcessConsoleCommand (WCHAR *pszCommand) {
    if (! gpRFCOMM) {
        DebugOut (DEBUG_OUTPUT, L"RFCOMM help : service not initialized\n");
        return ERROR_SUCCESS;
    }

    gpRFCOMM->Lock ();
    int iRes = ERROR_SUCCESS;
    __try {
        if (wcsicmp (pszCommand, L"help") == 0) {
            DebugOut (DEBUG_OUTPUT, L"RFCOMM Commands:\n");
            DebugOut (DEBUG_OUTPUT, L"    help        prints this text\n");
            DebugOut (DEBUG_OUTPUT, L"    global      dumps global state\n");
            DebugOut (DEBUG_OUTPUT, L"    links       dumps currently active L2CAP sessions\n");
            DebugOut (DEBUG_OUTPUT, L"    calls       dumps tasks in execution\n");
            DebugOut (DEBUG_OUTPUT, L"    contexts    dumps currently installed RFCOMM clients\n");
        } else if (wcsicmp (pszCommand, L"global") == 0) {
            DebugOut (DEBUG_OUTPUT, L"RFCOMM Global State : %s, %s\n\n", gpRFCOMM->fRunning ? L"Running" : L"Not Running",
                                    gpRFCOMM->fConnected ? L"Connected" : L"Not Connected");

            int iCount = 0;

            RFCOMM_CONTEXT    *pContext = gpRFCOMM->pContexts;
            while (pContext) {
                ++iCount;
                pContext = pContext->pNext;
            }

            DebugOut (DEBUG_OUTPUT, L"Installed contexts:         %d\n", iCount);

            Session            *pPhysLink = gpRFCOMM->pPhysLinks;
            iCount = 0;

            while (pPhysLink) {
                ++iCount;
                pPhysLink = pPhysLink->pNext;
            }

            DebugOut (DEBUG_OUTPUT, L"RFCOMM Sessions:            %d\n", iCount);

            Task            *pCall = gpRFCOMM->pCalls;
            iCount = 0;

            while (pCall) {
                ++iCount;
                pCall = pCall->pNext;
            }

            DebugOut (DEBUG_OUTPUT, L"Active tasks:               %d\n", iCount);

            DebugOut (DEBUG_OUTPUT, L"Session memory descriptor:  0x%08x\n", gpRFCOMM->pfmdSessions);
            DebugOut (DEBUG_OUTPUT, L"Server Channels descriptor: 0x%08x\n", gpRFCOMM->pfmdSC);
            DebugOut (DEBUG_OUTPUT, L"Channel memory descriptor:  0x%08x\n", gpRFCOMM->pfmdChannels);
            DebugOut (DEBUG_OUTPUT, L"Task memory descriptor:     0x%08x\n", gpRFCOMM->pfmdCalls);
            DebugOut (DEBUG_OUTPUT, L"L2CAP Handle:               0x%08x\n", gpRFCOMM->hL2CAP);

            DebugOut (DEBUG_OUTPUT, L"L2CAP Data Headers Offset:  %d\n", gpRFCOMM->cDataHeaders);
            DebugOut (DEBUG_OUTPUT, L"L2CAP Data Trailers Offset: %d\n", gpRFCOMM->cDataTrailers);

            DebugOut (DEBUG_OUTPUT, L"Lingering timeout:          %d\n", gpRFCOMM->dwLinger);
        } else if (wcsicmp (pszCommand, L"links") == 0) {
            Session            *pPhysLink = gpRFCOMM->pPhysLinks;

            while (pPhysLink) {
                DebugOut (DEBUG_OUTPUT, L"Session                     %04x%08x 0x%04x\n", pPhysLink->b.NAP, pPhysLink->b.SAP, pPhysLink->cid);
                DebugOut (DEBUG_OUTPUT, L"Stage                       ");
                if (pPhysLink->fStage == DORMANT)
                    DebugOut (DEBUG_OUTPUT, L"Disconnected\n");
                else if (pPhysLink->fStage == UP)
                    DebugOut (DEBUG_OUTPUT, L"UP\n");
                else if (pPhysLink->fStage == L2CAPUP)
                    DebugOut (DEBUG_OUTPUT, L"L2CAP UP\n");
                else {
                    if (pPhysLink->fStage & CONNECTING)
                        DebugOut (DEBUG_OUTPUT, L"Connecting ");

                    if (pPhysLink->fStage & CONNECTED)
                        DebugOut (DEBUG_OUTPUT, L"Connected ");

                    if (pPhysLink->fStage & CONFIG_REQ_DONE)
                        DebugOut (DEBUG_OUTPUT, L"CFGReqComplete ");

                    if (pPhysLink->fStage & CONFIG_IND_DONE)
                        DebugOut (DEBUG_OUTPUT, L"CFGIndDone ");
                    
                    DebugOut (DEBUG_OUTPUT, L"\n");
                }

                DebugOut (DEBUG_OUTPUT, L"Incoming                    %s\n", pPhysLink->fIncoming ? L"yes" : L"no");
                DebugOut (DEBUG_OUTPUT, L"Busy                        %s\n", pPhysLink->fBusy ? L"yes" : L"no");
                DebugOut (DEBUG_OUTPUT, L"inMTU                       %d\n", pPhysLink->inMTU);
                DebugOut (DEBUG_OUTPUT, L"outMTU                      %d\n", pPhysLink->outMTU);
                DebugOut (DEBUG_OUTPUT, L"Timeout cookie              0x%08x\n", pPhysLink->kTimeoutCookie);
                DebugOut (DEBUG_OUTPUT, L"Hardware failures           %d\n", pPhysLink->iErr);

                int iCount = 0;
                DLCI *pLogLink = pPhysLink->pLogLinks;
                while (pLogLink) {
                    DebugOut (DEBUG_OUTPUT, L" Channel %d Owner 0x%08x %s %s\n",
                        pLogLink->channel, pLogLink->pOwner,
                        pLogLink->fLocal ? L"local" : L"remote",
                        pLogLink->fStage == DORMANT ? L"dormant" :
                        (pLogLink->fStage == CONNECTING ? L"connecting" :
                        (pLogLink->fStage == CONNECTED ? L"connected" :
                        (pLogLink->fStage == UP ? L"up" : L"undefined"))));
                    ++iCount;
                    pLogLink = pLogLink->pNext;
                }

                DebugOut (DEBUG_OUTPUT, L"Log links                   %d\n", iCount);
                DebugOut (DEBUG_OUTPUT, L"\n\n");
                pPhysLink = pPhysLink->pNext;
            }
        } else if (wcsicmp (pszCommand, L"calls") == 0) {
            Task *pTask = gpRFCOMM->pCalls;
            while (pTask) {
                WCHAR *p = (pTask->fWhat == CALL_L2CAP_LINK_SETUP ? L"L2CAP link setup" :
                    (pTask->fWhat == CALL_L2CAP_LINK_CONFIG ? L"L2CAP link config" :
                    (pTask->fWhat == CALL_RFCOMM_SABM0 ? L"SABM0" :
                    (pTask->fWhat == CALL_RFCOMM_SABMN ? L"SABMn" :
                    (pTask->fWhat == CALL_RFCOMM_USERDATA ? L"User data" :
                    (pTask->fWhat == CALL_RFCOMM_SIGNALDATA ? L"Signal data" : L"Unknown type"))))));
                DebugOut (DEBUG_OUTPUT, L"Call %s Owner 0x%08x Ctx 0x%08x Session %04x%08x Channel %d %s %s %s\n",
                    p, pTask->pOwner, pTask->pContext,
                    pTask->pPhysLink ? pTask->pPhysLink->b.NAP : 0,
                    pTask->pPhysLink ? pTask->pPhysLink->b.SAP : 0,
                    pTask->channel, pTask->fLocal ? L"local" : L"remote",
                    pTask->fPF ? L"PF" : L"", pTask->fData ? L"Data" : L"");
                pTask = pTask->pNext;
            }
        } else if (wcsicmp (pszCommand, L"contexts") == 0) {
            RFCOMM_CONTEXT *pContext = gpRFCOMM->pContexts;
            while (pContext) {
                DebugOut (DEBUG_OUTPUT, L"Ctx 0x%08x User 0x%08x Channels : ", pContext, pContext->pUserContext);
                ServerChannel *pSC = pContext->pReservedChannels;
                while (pSC) {
                    DebugOut (DEBUG_OUTPUT, L"0x%02x ", pSC->channel);
                    pSC = pSC->pNext;
                }
                DebugOut (DEBUG_OUTPUT, L"\n");
                pContext = pContext->pNext;
            }
        } else
            iRes = ERROR_UNKNOWN_FEATURE;
    } __except (1) {
        DebugOut (DEBUG_ERROR, L"Exception in RFCOMM dump!\n");
    }

    gpRFCOMM->Unlock ();

    return iRes;
}

#endif // BTH_CONSOLE
