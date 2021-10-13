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
#define NDIS51_MINIPORT     1

#include <windows.h>
#include <ndis.h>
#include <winsock2.h>

#include <svsutil.hxx>

#include <bt_buffer.h>
#include <bt_ddi.h>

#include <bt_debug.h>

#include <bt_api.h>

#include <bthapi.h>
#include <bt_sdp.h>
#include <sdpnode.h>
#include <sdplib.h>
#include <intsafe.h>

#define BTHPAN_PACKETS                  25
#define BTHPAN_BUFFERS                  25

#define BTHPAN_NDIS_MAJOR_VERSION       5
#define BTHPAN_NDIS_MINOR_VERSION       1

#define BTHPAN_CHECK_FOR_HANG_TIMEOUT   0

#define BTHPAN_MEDIADELAY_MIN           30000
#define BTHPAN_MEDIADELAY_DEFAULT       120000

#define BTHPAN_CONNECT_WAIT             1000
#define BTHPAN_CONNECT_THRESHOLD        60000

#define BTHPAN_FILTER_RETRY_TO          10000

#define BTHPAN_CRT_MIN                  1000
#define BTHPAN_CRT_DEFAULT              20000
#define BTHPAN_CRT_MAX                  30000

#define BTHPAN_CONNECTIONS_MAX          20
#define BTHPAN_MAXCONNECTIONS_DEFAULT   6

#define BTHPAN_INQUIRY_DEFAULT          8

#define BTHPAN_MIN_FRAME                1691
#define BTHPAN_PSM                      0x000f

#define ETH_ADDR_SIZE                   6
#define ETH_MAX_PAYLOAD                 1500

#define GUID_STR_LENGTH                 39

typedef struct _ETH_HDR {
    unsigned char  destAddr[6];
    unsigned char  srcAddr[6];
    unsigned short ethType;
} ETH_HDR, *PETH_HDR;

#define ETH_HDR_SIZE        sizeof(ETH_HDR)
#define ETH_MAX_HDR_SIZE    sizeof(ETH_HDR)
#define ETH_MIN_HDR_SIZE    sizeof(ETH_HDR)
#define ETH_MAX_PACKET_SIZE (ETH_MAX_PAYLOAD + ETH_MAX_HDR_SIZE)

#define BTHPAN_MAX_MCAST_LIST                  32

#define BTHPAN_VERSION      0x00010005

enum FILTER_TYPE {
    MCAST,
    NETTYPE,
    ALL
};

enum Type {
    NONE,
    PANU,
    GN,
    NAP
};

enum State {
    DOWN,
    SDP,
    CONNECTING,
    CONFIG_LINK,
    CONFIG_BNEP,
    CONFIG_SECURITY,
    UP,
    CLOSING
};

#define CONFIG_OUT_DONE     1
#define CONFIG_IN_DONE      2
#define CONFIG_DONE         3

#define BTHPAN_MAX_CONNECTIONS      7
#define BTHPAN_MAX_ADAPTERS         1

#define BTHPAN_MAX_NETFILTERS       16
#define BTHPAN_MAX_MCASTFILTERS     16

#define NET_ACCESS_TYPE_ETH_100MB       0x0005
#define MAX_NET_ACCESS_RATE_ETH_100MB   100000
#define SDP_LANGID_ENGLISH              0x656E
#define MIBENUM_ENCODING_UTF16          0x03F7
#define MIBENUM_ENCODING_UTF8           0x6A

#define BNEP_VERSION       0x0100
#define BNEP_MAJOR_VERSION 0x01
#define BNEP_MINOR_VERSION 0x00

#define LANG_DEFAULT_ID                         0x0100

unsigned char gRates[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x48};

enum SDP_WALK_PROTO_DESC_LIST_STATE {
    SDP_WALK_PROTO_DESC_LIST_STATE_ROOT_SEQ = 0,
    SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_DESC_SEQ,
    SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_UUID,
    SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_INFO
};

enum SDP_WALK_CLASS_ID_LIST_STATE {
    SDP_WALK_CLASS_ID_LIST_STATE_ROOT_SEQ = 0,
    SDP_WALK_CLASS_ID_LIST_STATE_UUID
};

struct BTHPAN_SDP_WALK_CONTEXT {
    ULONG               attribState;
    UINT16              currProtocol;
    BOOLEAN             successfullParse;
    UINT16              psm;
    GUID                *pservice_id;
};

struct BTHPAN_LANG_ATTRIB {
    UINT16 ui16LangID;
    UINT16 ui16Encoding;
    UINT16 ui16AttribID;

    // Service name
    WCHAR *panServiceName;

    // Service description
    WCHAR *panServiceDesc;
};

struct BTHPAN_ADAPTER;

struct NETTYPERANGE {
    unsigned short  from;
    unsigned short  to;
};

struct MCASTRANGE {
    BD_ADDR         from;
    BD_ADDR         to;
};

struct FILTER {
    unsigned int cNetTypes;
    unsigned int cMCasts;

    NETTYPERANGE    ant[BTHPAN_MAX_NETFILTERS];
    MCASTRANGE      amc[BTHPAN_MAX_MCASTFILTERS];
};

struct BNEPPacket {
    BNEPPacket  *pNext;
    BD_BUFFER   *pBuffer;

    BNEPPacket (BD_BUFFER* pBuff) {
        memset (this, 0, sizeof(this));
        pBuffer = pBuff;
    }

    ~BNEPPacket () {
        if (pBuffer) {
            pBuffer->pFree (pBuffer);
        }
    }

    void *operator new (size_t iSize);
    void operator delete (void *ptr);
};

struct BTHPAN_CONNECTION {
    unsigned short      cid;

    BD_ADDR             ba;
    unsigned char       eth[6];

    State               state;
    unsigned int        fConfigState;

    union {
        struct {
            unsigned int        fPeerInitiated   : 1;
            unsigned int        fNetTypeMsgOut   : 1;
            unsigned int        fNetTypeMsgReq   : 1;
            unsigned int        fMCastMsgOut     : 1;
            unsigned int        fMCastMsgReq     : 1;
            unsigned int        fAuthenticated   : 1;
            unsigned int        fEncrypted       : 1;
        };
        unsigned int flags;
    };

    int                 nAdapter;

    unsigned int        uiRef;  // This is instance reference (incremented when the struct is freed)

    SVSCookie           scTimeout;

    GUID                dest_service_id;

    FILTER              filter;

    BNEPPacket          *pPacketsPending;

    SVSCookie           scMCastFilter;
    SVSCookie           scNetFilter;

    BD_BUFFER           *pMCastFilterRetryBuff;
    BD_BUFFER           *pNetFilterRetryBuff;

    void Reinit (void) {
        memset (this, 0, sizeof(*this));
        nAdapter = -1;
        state    = DOWN;
    }
};

#define BTHPAN_MAX_ASSOCIATIONS     8

struct BTHPAN_ASSOCIATION {
    WCHAR               szSSID[32];

    BD_ADDR             ba;

    GUID                service_id;

    unsigned int        pri;
};

struct BTHPAN_ADAPTER {
    long                lRef;       // This is reference for temporary locking
    unsigned int        uiRef;      // This is instance reference (incremented when the struct is freed)

    State               state;      // Adapter's state indicates configuration state of adapter, but not it's connection state

    Type                type;

    // NDIS part
    NDIS_HANDLE         hAdapter;

    NDIS_HANDLE         NdisPacketPool;
    NDIS_HANDLE         NdisBufferPool;

    WCHAR               szIfName[_MAX_PATH];
    WCHAR               szFriendlyName[_MAX_PATH];
    WCHAR               szDescription[_MAX_PATH];

    unsigned long       ulPacketFilter;
    unsigned long       ulLookAhead;

    FILTER              filter;    

    //  Addresses are set through registry
    unsigned char       ethAddr[6];

    FixedMemDescr       *pfmdEthHeaders;

    NDIS_STATUS         LastIndicatedStatus;

    // Statistics
    unsigned __int64    ul64XmitOk;
    unsigned __int64    ul64RcvOk;
    unsigned __int64    ul64XmitError;
    unsigned __int64    ul64RcvError;
    unsigned __int64    ul64RcvNoBuffer;
    unsigned int        ui32RcvErrAlign;
    unsigned int        ui32XmitOneCollision;
    unsigned int        ui32XmitMoreCollisions;

    // Bluetooth part

    BTHPAN_ASSOCIATION  aAssoc[BTHPAN_MAX_ASSOCIATIONS];
    unsigned int        cAssoc;

    unsigned int        cMaxConn;

    ULONG               ulSdpRecordId;

    GUID                service_id;

    unsigned int        fAcceptIncoming : 1;
    unsigned int        fAddressSet : 1;

    unsigned int        uiCrtTimeout;

    void Reinit (void) {
        memset (this, 0, sizeof(*this));

        state = DOWN;
        type  = NONE;

        LastIndicatedStatus = 0;    // neither connect, nor disconnect. Forces the first indication
    }

    long AddRef (void) {
        return InterlockedIncrement (&lRef);
    }

    long DelRef (void) {
        long ll = InterlockedDecrement (&lRef);

        SVSUTIL_ASSERT (ll >= 0);

        return ll;
    }

    long GetRefCount (void) {
        return lRef;
    }
};

#define BTHPAN_MAX_NETWORKS         12

#define REF_EMPTY       0xff
#define REF_CONNECT     0xfe
#define REF_DISCOVERY   3

struct BTHPAN_NETWORK {
    BTHPAN_ASSOCIATION      a;
    unsigned int            iRef;
};

struct PAN_CONTEXT : public SVSAllocClass, public SVSSynch, public SVSRefObj {
    State               state;

    FixedMemDescr       *pfmdPackets;

    BD_ADDR             ba;
    unsigned char       eth[6];

    HANDLE              hL2CAP;
    L2CAP_INTERFACE     l2cap_if;

    int                 cDataHeaders;
    int                 cDataTrailers;

    BTHPAN_ADAPTER      aAdapters[BTHPAN_MAX_ADAPTERS];

    BTHPAN_CONNECTION   aConn[BTHPAN_MAX_CONNECTIONS];

    BTHPAN_NETWORK      aNetworks[BTHPAN_MAX_NETWORKS];

    HANDLE              hMediaThread;
    HANDLE              hRefreshEvent;
    unsigned int        mediaDelay;
    unsigned char       ucInquiryLength;

    NDIS_HANDLE         ndisDriverHandle;

    unsigned int        ActivateRefCount;

    unsigned int        fAuthenticate : 1;
    unsigned int        fEncrypt : 1;
    unsigned int        fStackUp: 1;

    void Reinit (void) {
        state = DOWN;
        pfmdPackets = NULL;

        ba.NAP = 0;
        ba.SAP = 0;
        memset (eth, 0, sizeof(eth));

        hL2CAP = NULL;
        memset (&l2cap_if, 0, sizeof(l2cap_if));
        cDataHeaders = 0;
        cDataTrailers = 0;

        ActivateRefCount = 0;

        int i;
        for (i = 0 ; i < SVSUTIL_ARRLEN(aAdapters) ; ++i)
            aAdapters[i].Reinit ();

        for (i = 0 ; i < SVSUTIL_ARRLEN(aConn) ; ++i)
            aConn[i].Reinit ();

        memset (aNetworks, 0, sizeof(aNetworks));
        for (i = 0 ; i < SVSUTIL_ARRLEN(aNetworks) ; ++i)
            aNetworks[i].iRef = REF_EMPTY;

        hMediaThread = NULL;
        hRefreshEvent = NULL;
        mediaDelay = BTHPAN_MEDIADELAY_DEFAULT;
        ucInquiryLength = BTHPAN_INQUIRY_DEFAULT;

        fAuthenticate = TRUE;
        fEncrypt = TRUE;
        fStackUp = FALSE;
    }

    PAN_CONTEXT (void) {
        Reinit ();
    }
};

// Ascending order list as preferred by ndis
static NDIS_OID g_oidSupported[] =
{
    // Required GEN oids
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_MEDIA_CONNECT_STATUS,
    OID_GEN_MAXIMUM_SEND_PACKETS,
    OID_GEN_VENDOR_DRIVER_VERSION,

    // Optional GEN oids
    OID_GEN_SUPPORTED_GUIDS,
    // OID_GEN_MEDIA_CAPABILITIES,
    // OID_GEN_PHYSICAL_MEDIUM,

    // Required GEN statistic oids
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,

    // Required 802.3 oids
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,

    // Required 802.3 statistic oids
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,

    // Power management oids
    // OID_PNP_CAPABILITIES,
    // OID_PNP_SET_POWER,
    // OID_PNP_QUERY_POWER,
    // OID_PNP_ADD_WAKE_UP_PATTERN,
    // OID_PNP_REMOVE_WAKE_UP_PATTERN,
    // OID_PNP_ENABLE_WAKE_UP

    // Bluetooth PAN oids
    OID_PAN_CONNECT,
    OID_PAN_DISCONNECT,
    OID_PAN_AUTHENTICATE,
    OID_PAN_ENCRYPT,
    // OID_PAN_CONFIG,
    // OID_PAN_ENUM_CONNECTIONS,
    // OID_PAN_ENUM_DEVICES
};

static PAN_CONTEXT     *gpPAN;

static void MpAttemptConnection (BTHPAN_ADAPTER *pAdapter, BD_ADDR *pba, GUID *pService);
static void MpGetAssociations (BTHPAN_ADAPTER *pAdapter);
static VOID MpMediaStatusIndicate (BTHPAN_ADAPTER *pAdapter);
static int StartAuthenticatingThread (BTHPAN_CONNECTION *pConn);
static void SetConnectionToUp (BTHPAN_CONNECTION *pConn);
static DWORD GetNumAdapterConnections (BTHPAN_ADAPTER *pAdapter);

//
//  General utilities
//

void *BNEPPacket::operator new (size_t iSize) {
    SVSUTIL_ASSERT (gpPAN->pfmdPackets);
    SVSUTIL_ASSERT (gpPAN->IsLocked ());
    SVSUTIL_ASSERT (iSize == sizeof(BNEPPacket));

    void *pRes = svsutil_GetFixed (gpPAN->pfmdPackets);

    SVSUTIL_ASSERT (pRes);

    return pRes;
}

void BNEPPacket::operator delete(void *ptr) {
    SVSUTIL_ASSERT (gpPAN->IsLocked ());
    SVSUTIL_ASSERT (ptr);

    svsutil_FreeFixed (ptr, gpPAN->pfmdPackets);
}


#if defined (DEBUG) || defined (_DEBUG)
static int DumpBnepControl (unsigned char *&p) {
    DebugOut (DEBUG_PAN_PACKETS, L"BNEP CONTROL : ");
    switch (*p) {
        case 0x00:
            DebugOut (DEBUG_PAN_PACKETS, L"CONTROL NOT UNDERSTOOD, type=0x%02x", p[1]);
            p += 2;
            break;

        case 0x01:
            {
                DebugOut (DEBUG_PAN_PACKETS, L"SETUP CONNECTION REQUEST ");
                int cUuidSize = p[1];
                switch (cUuidSize) {
                    case 2:
                        DebugOut (DEBUG_PAN_PACKETS, L"dst=0x%04x src=0x%04x\n", (p[2] << 8) | p[3], (p[4] << 8) | p[5]);
                        break;
                    case 4:
                        DebugOut (DEBUG_PAN_PACKETS, L"dst=0x%08x src=0x%08x\n", (p[2] << 24) | (p[3] << 16) | (p[4] << 8) | p[5],
                            (p[6] << 24) | (p[7] << 16) | (p[8] << 8) | p[9]);
                        break;
                    case 16:
                        DebugOut (DEBUG_PAN_PACKETS, L"\n    dst=%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
                            (p[2] << 24) | (p[3] << 16) | (p[4] << 8) | p[5],
                            (p[6] << 8) | p[7],
                            (p[8] << 8) | p[9],
                            p[10], p[11],
                            p[12], p[13], p[14], p[15], p[16], p[17]);
                        DebugOut (DEBUG_PAN_PACKETS, L"\n    src=%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
                            (p[18] << 24) | (p[19] << 16) | (p[20] << 8) | p[21],
                            (p[22] << 8) | p[23],
                            (p[24] << 8) | p[25],
                            p[26], p[27],
                            p[28], p[29], p[30], p[31], p[32], p[33]);
                        break;
                }

                p += 2 + cUuidSize * 2;
            }
            break;

        case 0x02:
            {
                unsigned short usResult = (p[1] << 8) | p[2];
                WCHAR *szResult = L"UNKNOWN";
                switch (usResult) {
                    case 0x0000:
                        szResult = L"SUCCESS";
                        break;
                    case 0x0001:
                        szResult = L"FAILED, bad dest UUID";
                        break;
                    case 0x0002:
                        szResult = L"FAILED, bad src UUID";
                        break;
                    case 0x0003:
                        szResult = L"FAILED, bad UUID size";
                        break;
                    case 0x0004:
                        szResult = L"FAILED, not allowed";
                        break;
                }
                DebugOut (DEBUG_PAN_PACKETS, L"SETUP CONNECTION RESPONSE, 0x%04x (%s)\n", usResult, szResult);
                p += 3;
            }
            break;

        case 0x03:
            {
                int cPairs = ((p[1] << 8) | p[2]) / 4;
                DebugOut (DEBUG_PAN_PACKETS, L"FILTER NET TYPE SET, %d pairs\n", cPairs);

                if (p[2] & 3)
                    DebugOut (DEBUG_PAN_PACKETS, L"ERROR: bad length (%d)!\n", p[1]);

                p += 3;

                for (int i = 0 ; i < cPairs ; ++i, p += 4)
                    DebugOut (DEBUG_PAN_PACKETS, L"    0x%04x-0x%04x\n", (p[0] << 8) | p[1], (p[2] << 8) | p[3]);

            break;
            }

        case 0x04:
            {
                unsigned short usResult = (p[1] << 8) | p[2];
                WCHAR *szResult = L"UNKNOWN";
                switch (usResult) {
                    case 0x0000:
                        szResult = L"SUCCESS";
                        break;
                    case 0x0001:
                        szResult = L"Unsupported";
                        break;
                    case 0x0002:
                        szResult = L"FAILED, invalid range";
                        break;
                    case 0x0003:
                        szResult = L"FAILED, too many";
                        break;
                    case 0x0004:
                        szResult = L"FAILED, security";
                        break;
                }
                DebugOut (DEBUG_PAN_PACKETS, L"FILTER NET TYPE RESPONSE, 0x%04x (%s)\n", usResult, szResult);
                p += 3;
            }
            break;

        case 0x05:
            {
                int cPairs = ((p[1] << 8) | p[2]) / 12;
                DebugOut (DEBUG_PAN_PACKETS, L"FILTER MULTI ADDR SET, %d pairs\n", cPairs);

                if (cPairs * 12 != ((p[1] << 8) | p[2]))
                    DebugOut (DEBUG_PAN_PACKETS, L"ERROR: bad length (%d)!\n", p[1]);

                p += 3;

                for (int i = 0 ; i < cPairs ; ++i, p += 12)
                    DebugOut (DEBUG_PAN_PACKETS, L"    %02x%02x%02x%02x%02x%02x-%02x%02x%02x%02x%02x%02x\n", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11]);

            break;
            }

        case 0x06:
            {
                unsigned short usResult = (p[1] << 8) | p[2];
                WCHAR *szResult = L"UNKNOWN";
                switch (usResult) {
                    case 0x0000:
                        szResult = L"SUCCESS";
                        break;
                    case 0x0001:
                        szResult = L"Unsupported";
                        break;
                    case 0x0002:
                        szResult = L"FAILED, invalid range";
                        break;
                    case 0x0003:
                        szResult = L"FAILED, too many";
                        break;
                    case 0x0004:
                        szResult = L"FAILED, security";
                        break;
                }
                DebugOut (DEBUG_PAN_PACKETS, L"FILTER MULTI ADDR RESPONSE, 0x%04x (%s)\n", usResult, szResult);
                p += 3;
            }
            break;
        default:
            DebugOut (DEBUG_PAN_PACKETS, L"UNKNOWN BNEP CONTROL 0x%02x\n", *p);
            return FALSE;
    }

    return TRUE;
}

static void DumpBnepPacket (WCHAR *sz, BD_BUFFER *pBuff) {
    DebugOut (DEBUG_PAN_PACKETS, L"%s\n", sz);
    DumpBuff (DEBUG_PAN_PACKETS, pBuff->pBuffer + pBuff->cStart, BufferTotal (pBuff));

    unsigned char *p = pBuff->pBuffer + pBuff->cStart;
    unsigned char cType = *p & 0x7f;
    int fExt = (*p & 0x80) != 0;

    ++p;

    switch (cType) {
        case 0x00:
            DebugOut (DEBUG_PAN_PACKETS, L"BNEP GENERAL ETHERNET : dst=%02x%02x%02x%02x%02x%02x src=%02x%02x%02x%02x%02x%02x proto=0x%04x\n",
                p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], (p[12] << 8) | p[13]);
            p+=14;
            break;

        case 0x01:
            if (! DumpBnepControl (p))
                return;

            break;

        case 0x02:
            DebugOut (DEBUG_PAN_PACKETS, L"BNEP COMPRESSED ETHERNET proto=0x%04x\n", (p[0] << 8) | p[1]);
            p += 2;
            break;

        case 0x03:
            DebugOut (DEBUG_PAN_PACKETS, L"BNEP COMPRESSED ETHERNET SOURCE ONLY: src=%02x%02x%02x%02x%02x%02x proto=0x%04x\n",
                p[0], p[1], p[2], p[3], p[4], p[5], (p[6] << 8) | p[7]);
            p+=8;
            break;

        case 0x04:
            DebugOut (DEBUG_PAN_PACKETS, L"BNEP COMPRESSED ETHERNET DEST ONLY: dst=%02x%02x%02x%02x%02x%02x proto=0x%04x\n",
                p[0], p[1], p[2], p[3], p[4], p[5], (p[6] << 8) | p[7]);
            p+=8;
            break;

        default:
            DebugOut (DEBUG_PAN_PACKETS, L"UNKNOWN BNEP TYPE 0x%02x\n", cType);
            return;
    }

    if (p > pBuff->pBuffer + pBuff->cEnd) {
        DebugOut (DEBUG_PAN_PACKETS, L"ERROR! Buffer overrun!\n");
        return;
    }

    while (fExt) {
        fExt = (*p & 0x80) != 0;
        cType = *p & 0x7f;
        int cLen = p[1];
        if (cType == 0x00) {
            DebugOut (DEBUG_PAN_PACKETS, L"EXTENSION CONTROL\n");
            unsigned char *q = p + 2;
            if (DumpBnepControl (q)) {
                if (q != p + cLen + 2) {
                    DebugOut (DEBUG_PAN_PACKETS, L"ERROR! Buffer size mismatch in extension processing!\n");
                }
            }
        } else {
            DebugOut (DEBUG_PAN_PACKETS, L"UNKNOWN EXTENSION 0x%02x, skipping\n", cType);
        }
        p += cLen + 2;

        if (p > pBuff->pBuffer + pBuff->cEnd) {
            DebugOut (DEBUG_PAN_PACKETS, L"ERROR! Buffer overrun!\n");
            return;
        }
    }
}
#endif

static int GetBD (WCHAR *pString, BD_ADDR *pba) {
    pba->NAP = 0;

    for (int i = 0 ; i < 4 ; ++i, ++pString) {
        if (! iswxdigit (*pString))
            return FALSE;

        int c = *pString;
        if (c >= 'a')
            c = c - 'a' + 0xa;
        else if (c >= 'A')
            c = c - 'A' + 0xa;
        else c = c - '0';

        if ((c < 0) || (c > 16))
            return FALSE;

        pba->NAP = pba->NAP * 16 + c;
    }

    pba->SAP = 0;

    for (i = 0 ; i < 8 ; ++i, ++pString) {
        if (! iswxdigit (*pString))
            return FALSE;

        int c = *pString;
        if (c >= 'a')
            c = c - 'a' + 0xa;
        else if (c >= 'A')
            c = c - 'A' + 0xa;
        else c = c - '0';

        if ((c < 0) || (c > 16))
            return FALSE;

        pba->SAP = pba->SAP * 16 + c;
    }

    if (*pString != '\0')
        return FALSE;

    return TRUE;
}

static void BtToEth (unsigned char *pEth, BD_ADDR *pbd_addr) {
    unsigned char *pbd = (unsigned char *)pbd_addr;
    for (int i = 0 ; i < 6 ; ++i) {
        pEth[i] = pbd[5-i];
    }
}

static void EthToBt (BD_ADDR *pbd_addr, unsigned char *pEth) {
    unsigned char *pbd = (unsigned char *)pbd_addr;
    for (int i = 0 ; i < 6 ; ++i) {
        pbd[i] = pEth[5-i];
    }
}

unsigned __int64 net48tohost64(unsigned char* pAddr) {
    return ((__int64)(pAddr[0]) << 40) | ((__int64)(pAddr[1]) << 32) | ((__int64)(pAddr[2]) << 24) | ((__int64)(pAddr[3]) << 16) | ((__int64)(pAddr[4]) << 8) | (__int64)(pAddr[5]);    
}

static BOOL VerifyPanUUID (GUID* pguid) {
    if ((*pguid == PANUServiceClass_UUID) || (*pguid == NAPServiceClass_UUID) || (*pguid == GNServiceClass_UUID))
        return TRUE;
    
    return FALSE;
}

static int CopyNdisToFlat
(
NDIS_BUFFER        *pNdisB,
unsigned char    *pFlatBuf,
int                cCount,
int                cStartOffset
) {
    int cOffset = 0;
    int cCopied = 0;
    while (pNdisB) {
        unsigned char *pVA = NULL;
        unsigned int uiLength = 0;

        NdisQueryBuffer (pNdisB, (PVOID *)&pVA, &uiLength);

        if (! pVA)
            break;

        if (cOffset + (int)uiLength > cStartOffset) {
            SVSUTIL_ASSERT (cOffset <= cStartOffset);
            int cBytesAvailable = cOffset + uiLength - cStartOffset;
            int cOffsetInBuffer = cStartOffset - cOffset;
            int cBytesToCopy = cBytesAvailable > cCount ? cCount : cBytesAvailable;
            memcpy (pFlatBuf, pVA + cOffsetInBuffer, cBytesToCopy);

            cCount -= cBytesToCopy;
            cStartOffset += cBytesToCopy;
            cCopied += cBytesToCopy;
            pFlatBuf += cBytesToCopy;

            SVSUTIL_ASSERT (cCount >= 0);

            if (cCount == 0)
                break;
        }

        cOffset += uiLength;
        NdisGetNextBuffer(pNdisB, &pNdisB);
    }

    return cCopied;
}

static int MpNdisStrCmp (UNICODE_STRING *pUStr, WCHAR *szStr) {
    int cStrLen  = wcslen (szStr);
    int cUStrLen = pUStr->Length/sizeof(WCHAR);

    if (cStrLen != cUStrLen)
        return cUStrLen - cStrLen;

    return wcsnicmp (pUStr->Buffer, szStr, cStrLen);
}

static BOOL HexStringToDword(WCHAR *&lpsz, DWORD &Value, int cDigits, WCHAR chDelim) {
    Value = 0;
    for (int Count = 0; Count < cDigits; Count++, lpsz++) {
        if (*lpsz >= '0' && *lpsz <= '9')
            Value = (Value << 4) + *lpsz - '0';
        else if (*lpsz >= 'A' && *lpsz <= 'F')
            Value = (Value << 4) + *lpsz - 'A' + 10;
        else if (*lpsz >= 'a' && *lpsz <= 'f')
            Value = (Value << 4) + *lpsz - 'a' + 10;
        else
            return(FALSE);
    }

    if (chDelim != 0)
        return *lpsz++ == chDelim;
    else
        return TRUE;
}

static int MpStrToGUID (WCHAR *szStr, GUID *pguid) {
    WCHAR *lpsz = szStr;
    DWORD dw;

    if (*lpsz++ != '{')
        return FALSE;

    if (!HexStringToDword(lpsz, pguid->Data1, sizeof(DWORD)*2, '-'))
        return FALSE;

    if (!HexStringToDword(lpsz, dw, sizeof(WORD)*2, '-'))
        return FALSE;

    pguid->Data2 = (WORD)dw;

    if (!HexStringToDword(lpsz, dw, sizeof(WORD)*2, '-'))
        return FALSE;

    pguid->Data3 = (WORD)dw;

    if (!HexStringToDword(lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[0] = (BYTE)dw;

    if (!HexStringToDword(lpsz, dw, sizeof(BYTE)*2, '-'))
        return FALSE;

    pguid->Data4[1] = (BYTE)dw;

    if (!HexStringToDword(lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[2] = (BYTE)dw;

    if (!HexStringToDword(lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[3] = (BYTE)dw;

    if (!HexStringToDword(lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[4] = (BYTE)dw;

    if (!HexStringToDword(lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[5] = (BYTE)dw;

    if (!HexStringToDword(lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[6] = (BYTE)dw;
    if (!HexStringToDword(lpsz, dw, sizeof(BYTE)*2, 0))
        return FALSE;

    pguid->Data4[7] = (BYTE)dw;

    if (*lpsz++ != '}')
        return FALSE;

    if (*lpsz++ != '\0')
        return FALSE;

    return TRUE;
}

static int MpNdisStrToGUID (UNICODE_STRING *pUStr, GUID *pguid) {
    WCHAR szStr[MAX_PATH];
    DWORD dwLen = min(pUStr->Length/sizeof(WCHAR), MAX_PATH-1);
    wcsncpy (szStr, pUStr->Buffer, dwLen);
    szStr[dwLen] = '\0';

    return MpStrToGUID (szStr, pguid);
}

static void QueuePendingPacket (BTHPAN_CONNECTION *pConn, BNEPPacket *pPendingPacket) {
    SVSUTIL_ASSERT (gpPAN->IsLocked ());
    
    if (! pConn->pPacketsPending) {
        pConn->pPacketsPending = pPendingPacket;            
    }
    else {
        BNEPPacket *pRunner = pConn->pPacketsPending;
        while (pRunner->pNext) {
            pRunner = pRunner->pNext;
        }
        pRunner->pNext = pPendingPacket;
    }
}

static void UUIDToWire (unsigned char *pBuff, GUID *pGuid) {
    *pBuff++ = (unsigned char)(pGuid->Data1 >> 24);
    *pBuff++ = (unsigned char)(pGuid->Data1 >> 16);
    *pBuff++ = (unsigned char)(pGuid->Data1 >> 8);
    *pBuff++ = (unsigned char)(pGuid->Data1);
    *pBuff++ = (unsigned char)(pGuid->Data2 >> 8);
    *pBuff++ = (unsigned char)(pGuid->Data2);
    *pBuff++ = (unsigned char)(pGuid->Data3 >> 8);
    *pBuff++ = (unsigned char)(pGuid->Data3);

    memcpy (pBuff, pGuid->Data4, sizeof(pGuid->Data4));
}

static BTHPAN_ADAPTER *AdapterFromCtx (void *pContext) {
    SVSUTIL_ASSERT (gpPAN->IsLocked());

    BTHPAN_ADAPTER *pAdapter = (BTHPAN_ADAPTER *)(((unsigned long)pContext) & ~0x3);   // Real pointer is 4-bytes aligned
    unsigned int uiRefCount = ((unsigned long)pContext) & 0x3;

    int iAdapter = pAdapter - gpPAN->aAdapters;
    if ((pAdapter == &gpPAN->aAdapters[iAdapter]) && (uiRefCount == (pAdapter->uiRef & 0x3)))
        return pAdapter;

    return NULL;
}

static void *AdapterToCtx (BTHPAN_ADAPTER *pAdapter) {
    SVSUTIL_ASSERT ((((unsigned long)pAdapter) & 0x3) == 0);
    return (void *)(((unsigned long)pAdapter) | (pAdapter->uiRef & 0x3));
}

static BTHPAN_ADAPTER *AdapterFromUUID (GUID *pguid) {
    for (int i = 0 ; i < SVSUTIL_ARRLEN (gpPAN->aAdapters) ; ++i) {
        if ((gpPAN->aAdapters[i].state == UP) && (gpPAN->aAdapters[i].service_id == *pguid))
            return &gpPAN->aAdapters[i];
    }

    return NULL;
}

static BTHPAN_ADAPTER *MpFindAndRefAdapter (void *pContext) {
    gpPAN->Lock ();

    BTHPAN_ADAPTER *pAdapter = AdapterFromCtx (pContext);

    if (pAdapter)
        pAdapter->AddRef ();
    else
        pAdapter = NULL;

    gpPAN->Unlock ();

    return pAdapter;
}

static void *ConnToCtx (BTHPAN_CONNECTION *pConn) {
    SVSUTIL_ASSERT ((((unsigned long)pConn) & 0x3) == 0);
    return (void *)(((unsigned long)pConn) | (pConn->uiRef & 0x3));
}

static BTHPAN_CONNECTION *ConnFromCtx (void *pCtx) {
    SVSUTIL_ASSERT (gpPAN->IsLocked ());

    BTHPAN_CONNECTION *pConn = (BTHPAN_CONNECTION *)(((unsigned long)pCtx) & ~0x3);
    unsigned int uiRefCount = ((unsigned long)pCtx) & 0x3;

    int iConn = pConn - gpPAN->aConn;
    if ((pConn == &gpPAN->aConn[iConn]) && ((pConn->uiRef & 0x3) == uiRefCount))
        return pConn;

    return NULL;
}

static BTHPAN_CONNECTION *ConnFromCid (unsigned short cid) {
    SVSUTIL_ASSERT (gpPAN->IsLocked ());
    for (int i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aConn) ; ++i) {
        if ((gpPAN->aConn[i].state != DOWN) && (gpPAN->aConn[i].cid == cid))
            return &gpPAN->aConn[i];
    }

    return NULL;
}

static int CountConn (BTHPAN_ADAPTER *pAdapter) {
    SVSUTIL_ASSERT (gpPAN->IsLocked ());

    int nAdapter = pAdapter - gpPAN->aAdapters;

    int iCount = 0;

    for (int i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aConn) ; ++i) {
        if (gpPAN->aConn[i].nAdapter == nAdapter)
            ++iCount;
    }

    return iCount;
}

static int IsBluetoothBasedGUID (GUID *pGUID, unsigned int *puiOffset) {
    if (memcmp (&pGUID->Data2, &Bluetooth_Base_UUID.Data2, sizeof (GUID) - offsetof (GUID, Data2)) != 0)
        return FALSE;

    if (pGUID->Data1 < Bluetooth_Base_UUID.Data1)
        return FALSE;

    *puiOffset = pGUID->Data1 - Bluetooth_Base_UUID.Data1;

    return TRUE;
}

//
// Bluetooth utilities
//
static void SetMediaParms (void) {
    HKEY hk;
    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"software\\microsoft\\bluetooth\\pan", 0, KEY_READ, &hk)) {
        DWORD dw;
        DWORD dwSize = sizeof(dw);
        DWORD dwType = 0;
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"MediaDelay", NULL, &dwType, (LPBYTE)&dw, &dwSize)) && (dwType == REG_DWORD) &&
            (dwSize == sizeof(dw)) && ((dw == 0) || (dw >= BTHPAN_MEDIADELAY_MIN)))
            gpPAN->mediaDelay = dw;

        if (gpPAN->mediaDelay == 0) {
            gpPAN->mediaDelay = INFINITE;
        }

        dwSize = sizeof(dw);
        dwType = 0;
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"InquiryLength", NULL, &dwType, (LPBYTE)&dw, &dwSize)) && (dwType == REG_DWORD) &&
            (dwSize == sizeof(dw)) && (dw > 0) && (dw < 256))
            gpPAN->ucInquiryLength = (unsigned char)dw;

        dwSize = sizeof(dw);
        dwType = 0;
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"ActivateOnBoot", NULL, &dwType, (LPBYTE)&dw, &dwSize)) && (dwType == REG_DWORD) &&
            (dwSize == sizeof(dw)))
            gpPAN->ActivateRefCount = dw ? 1 : 0;

        dwSize = sizeof(dw);
        dwType = 0;
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"Authenticate", NULL, &dwType, (LPBYTE)&dw, &dwSize)) && (dwType == REG_DWORD) &&
            (dwSize == sizeof(dw)))
            gpPAN->fAuthenticate = (BOOL)dw;

        dwSize = sizeof(dw);
        dwType = 0;
        if ((ERROR_SUCCESS == RegQueryValueEx (hk, L"Encrypt", NULL, &dwType, (LPBYTE)&dw, &dwSize)) && (dwType == REG_DWORD) &&
            (dwSize == sizeof(dw)))
            gpPAN->fEncrypt = (BOOL)dw;

        RegCloseKey (hk);
    }
}

// We are linked into btd.dll
int BthNotifyEvent (PBTEVENT pbtEvent, DWORD dwEventClass);

static void IndicatePANConnections(BTHPAN_CONNECTION *pConn, BTHPAN_ADAPTER *pAdapter)
{
    BTEVENT bte;
    memset (&bte, 0, sizeof(BTEVENT));
    bte.dwEventId = BTE_PAN_CONNECTIONS;

    PBT_PAN_NUM_CONNECTIONS pEvent = (PBT_PAN_NUM_CONNECTIONS) bte.baEventData;
    pEvent->dwSize = sizeof(BT_PAN_NUM_CONNECTIONS);
    pEvent->NumConnections = GetNumAdapterConnections(pAdapter);

    BthNotifyEvent(&bte, BTE_CLASS_PAN);
}

static void SetConnectionToDown (BTHPAN_CONNECTION *pConn) {
    SVSUTIL_ASSERT (gpPAN->IsLocked ());

    BTHPAN_ADAPTER *pAdapter = NULL;
    if (pConn->nAdapter != -1) {
        pAdapter = &gpPAN->aAdapters[pConn->nAdapter];
    }
    
    pConn->state = DOWN;
    pConn->cid   = 0;
    pConn->fConfigState = 0;
    pConn->flags = 0;
    pConn->nAdapter = -1;

    memset (&pConn->dest_service_id, 0, sizeof(pConn->dest_service_id));
    memset (&pConn->filter, 0, sizeof(pConn->filter));

    ++pConn->uiRef;

    if (pAdapter) {
        IndicatePANConnections(pConn, pAdapter);
    }
}

static void SetConnectionToConnecting (BTHPAN_CONNECTION *pConn, int fPeerInit, unsigned short cid, BD_ADDR *pba, int nAdapter) {
    // Opening and closing stages are the only ones that need adapter locked. This MUST be called locked.

    pConn->state = CONNECTING;
    pConn->cid   = cid;
    pConn->fPeerInitiated = fPeerInit;
    pConn->nAdapter = nAdapter;

    if (pba) {
        pConn->ba       = *pba;
        BtToEth (pConn->eth, pba);
    }

    pConn->fConfigState = 0;
}

static void SetConnectionToUp (BTHPAN_CONNECTION *pConn) {
    SVSUTIL_ASSERT (gpPAN->IsLocked ());
    
    pConn->state = UP;

    BTHPAN_ADAPTER *pAdapter = NULL;
    if (pConn->nAdapter != -1) {
        pAdapter = &gpPAN->aAdapters[pConn->nAdapter];
    }

    if (pAdapter) {
        IndicatePANConnections(pConn, pAdapter);
    }
}

static void CloseConnection (BTHPAN_CONNECTION *pConn, BTHPAN_ADAPTER *pAdapter) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"CloseConnection 0x%08x (cid:0x%04x, ba %04x%08x)\n", pConn, pConn->cid, pConn->ba.NAP, pConn->ba.SAP));

    while (pConn->pPacketsPending) {
        BNEPPacket *pFree = pConn->pPacketsPending;
        pConn->pPacketsPending = pConn->pPacketsPending->pNext;
        delete pFree;
    }

    if (pConn->scTimeout) {
        btutil_UnScheduleEvent (pConn->scTimeout);
        pConn->scTimeout = 0;
    }

    if (! pAdapter)
        pAdapter = pConn->nAdapter == -1 ? NULL : &gpPAN->aAdapters[pConn->nAdapter];

    if (pAdapter)
        pAdapter->AddRef ();

    if (pConn->cid && (pConn->state != CLOSING)) {
        pConn->state = CLOSING;
        void *pContext = ConnToCtx (pConn);
        gpPAN->AddRef ();
        gpPAN->Unlock ();
        int iRes = gpPAN->l2cap_if.l2ca_Disconnect_In (gpPAN->hL2CAP, pContext, pConn->cid);
        gpPAN->Lock ();
        gpPAN->DelRef ();
        if (iRes != ERROR_SUCCESS) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] CloseConnection : l2ca_Disconnect_In failed, %d\n", iRes));
            SetConnectionToDown (pConn);
        }
    } else if (pConn->state == CONNECTING)
        SetConnectionToDown (pConn);

    if (pConn->state == DOWN) {
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"CloseConnection : connection properly closed\n"));

        if (pAdapter) {
            gpPAN->AddRef ();
            gpPAN->Unlock ();
            MpMediaStatusIndicate (pAdapter);
            gpPAN->Lock ();
            gpPAN->DelRef ();
        }
    }

    if (pAdapter)
        pAdapter->DelRef ();
}

static BTHPAN_CONNECTION *GetAdapterConnection (BTHPAN_ADAPTER *pAdapter) {
    SVSUTIL_ASSERT (gpPAN->IsLocked ());

    int nAdapter = pAdapter - gpPAN->aAdapters;
    int i;
    for (i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aConn) ; ++i) {
        if ((gpPAN->aConn[i].state != DOWN) && (gpPAN->aConn[i].nAdapter == nAdapter)) {
            return &gpPAN->aConn[i];
        }
    }

    return NULL;
}

static DWORD GetNumAdapterConnections (BTHPAN_ADAPTER *pAdapter) {
    SVSUTIL_ASSERT (gpPAN->IsLocked ());

    int nAdapter = pAdapter - gpPAN->aAdapters;
    int i;
    int count = 0;
    for (i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aConn) ; ++i) {
        if ((gpPAN->aConn[i].state == UP) && (gpPAN->aConn[i].nAdapter == nAdapter)) {
            count++;
        }
    }

    return count;
}

static DWORD WINAPI TimeoutConnection (void *pCallContext) { // Aborted call == immediate death...
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"TimeoutConnection 0x%08x\n", pCallContext));

    if (! gpPAN) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] TimeoutConnection: context doesn't exist\n"));
        return ERROR_SUCCESS;
    }

    gpPAN->Lock ();
    if (gpPAN->state != UP) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] TimeoutConnection: context not up\n"));
        gpPAN->Unlock ();
        return ERROR_SUCCESS;
    }

    BTHPAN_CONNECTION *pConn = ConnFromCtx (pCallContext);

    if (pConn) {
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"TimeoutConnection : aborting connection %04x%08x cid=0x%04x\n", pConn->ba.NAP, pConn->ba.SAP, pConn->cid));

        CloseConnection (pConn, NULL);
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] TimeoutConnection: connection not found!\n"));
    }

    gpPAN->Unlock ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"TimeoutConnection DONE\n"));

    return ERROR_SUCCESS;
}

static void ConnectionStarted (BTHPAN_CONNECTION *pConn) {
    SVSUTIL_ASSERT (gpPAN->IsLocked ());
    SVSUTIL_ASSERT (pConn->state == CONFIG_LINK);
    SVSUTIL_ASSERT (pConn->fConfigState == CONFIG_DONE);

    pConn->state = CONFIG_BNEP;
    pConn->fConfigState = 0;

    if (pConn->fPeerInitiated)  // Nothing to do, just wait for the other guy
        return;

    // We started connection, so we KNOW what adapter it is.
    SVSUTIL_ASSERT (pConn->nAdapter >= 0);

    unsigned char packet_body[256];

    SVSUTIL_ASSERT(gpPAN->cDataHeaders + gpPAN->cDataTrailers < 128);
    unsigned char *p = packet_body + gpPAN->cDataHeaders;

    *p++ = 0x01;            // Control
    *p++ = 0x01;            // Setup connection request
    unsigned int uiDestUUID, uiSrcUUID;

    if (IsBluetoothBasedGUID (&pConn->dest_service_id, &uiDestUUID) &&
        IsBluetoothBasedGUID (&gpPAN->aAdapters[pConn->nAdapter].service_id, &uiSrcUUID)) {
        if ((uiSrcUUID <= 0xffff) && (uiDestUUID <= 0xffff)) {
            *p++ = 2;
            *p++ = (unsigned char)(uiDestUUID >> 8);
            *p++ = (unsigned char)(uiDestUUID);
            *p++ = (unsigned char)(uiSrcUUID >> 8);
            *p++ = (unsigned char)(uiSrcUUID);
        } else {    // 32 bits
            *p++ = 4;
            *p++ = (unsigned char)(uiDestUUID >> 24);
            *p++ = (unsigned char)(uiDestUUID >> 16);
            *p++ = (unsigned char)(uiDestUUID >> 8);
            *p++ = (unsigned char)(uiDestUUID);
            *p++ = (unsigned char)(uiSrcUUID >> 24);
            *p++ = (unsigned char)(uiSrcUUID >> 16);
            *p++ = (unsigned char)(uiSrcUUID >> 8);
            *p++ = (unsigned char)(uiSrcUUID);
        }
    } else {    // Full UUIDs
        *p++ = sizeof(GUID);    // UUID size - full UUIDs

        SVSUTIL_ASSERT (sizeof(GUID) == sizeof(pConn->dest_service_id));
        SVSUTIL_ASSERT (sizeof(GUID) == sizeof(gpPAN->aAdapters[pConn->nAdapter].service_id));

        UUIDToWire (p, &pConn->dest_service_id);
        p += sizeof(GUID);
        UUIDToWire (p, &gpPAN->aAdapters[pConn->nAdapter].service_id);
        p += sizeof(GUID);
    }

    BD_BUFFER Buff;
    
    Buff.cSize = (p - packet_body) + gpPAN->cDataTrailers;
    Buff.cStart = gpPAN->cDataHeaders;
    Buff.cEnd = Buff.cSize - gpPAN->cDataTrailers;
    Buff.pFree = BufferFree;
    Buff.fMustCopy = TRUE;
    Buff.pBuffer   = packet_body;

    unsigned int uiConnRef = pConn->uiRef;

    pConn->scTimeout = btutil_ScheduleEvent (TimeoutConnection, ConnToCtx (pConn), gpPAN->aAdapters[pConn->nAdapter].uiCrtTimeout);

    if (! StartAuthenticatingThread (pConn)) {
        gpPAN->AddRef ();
        gpPAN->Unlock ();

        IFDBG(DumpBnepPacket (L"Sending connection request.", &Buff));

        int iRes = gpPAN->l2cap_if.l2ca_DataDown_In (gpPAN->hL2CAP, ConnToCtx (pConn), pConn->cid, &Buff);

        gpPAN->Lock ();
        gpPAN->DelRef ();

        if ((iRes != ERROR_SUCCESS) && (pConn->uiRef == uiConnRef))
            CloseConnection (pConn, NULL);
    } else {    // Delay request
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ConnectionStarted : connection request delayed pending authentication\n"));

        BNEPPacket *pPendingPacket = new BNEPPacket(BufferCopy(&Buff));
        if (pPendingPacket)
            QueuePendingPacket(pConn, pPendingPacket);        
    }
}

static void AddConfigFlag (BTHPAN_CONNECTION *pConn, unsigned int uiFlag) {
    pConn->fConfigState |= uiFlag;

    if (pConn->fConfigState == CONFIG_DONE)
        ConnectionStarted (pConn);
}

static void MpCleanUpConnections (void) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"MpCleanUpConnections\n"));

    // Go through all adapters and set states of everything to disconnected

    gpPAN->Lock ();

    int i;
    for (i = 0 ; i < SVSUTIL_ARRLEN (gpPAN->aConn) ; ++i)
        SetConnectionToDown (&gpPAN->aConn[i]);

    gpPAN->Unlock ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"MpCleanUpConnections DONE\n"));
}

static int FilterByMCast (FILTER *pFilter, unsigned char *pAddr) {
    if (pFilter->cMCasts == 0)
        return FALSE;

    unsigned __int64 ui64Addr = net48tohost64(pAddr);
    for (int i = 0 ; i < (int)pFilter->cMCasts ; ++i) {
        pAddr = (unsigned char *)&pFilter->amc[i].from;
        unsigned __int64 ui64from = net48tohost64(pAddr);
        pAddr = (unsigned char *)&pFilter->amc[i].to;
        unsigned __int64 ui64to = net48tohost64(pAddr);

        if ((ui64Addr >= ui64from) && (ui64Addr <= ui64to))
            return FALSE;
    }

    return TRUE;
}

static int FilterByType (FILTER *pFilter, unsigned int uiNetType) {
    // Returns TRUE if packet is filtered out (NOT sent)

    if (pFilter->cNetTypes == 0)
        return FALSE;

    for (int i = 0 ; i < (int)pFilter->cNetTypes ; ++i) {
        if ((uiNetType >= pFilter->ant[i].from) && (uiNetType <= pFilter->ant[i].to))
            return FALSE;
    }

    return TRUE;
}

static void InvertAddress (unsigned char *pAddr1, unsigned char *pAddr2, unsigned char *pAddr3) {
    if (memcmp (pAddr1, pAddr3, 6) == 0)
        memcpy (pAddr1, pAddr2, 6);
    else if (memcmp (pAddr1, pAddr2, 6) == 0)
        memcpy (pAddr1, pAddr3, 6);
}

static int ComputeTotalExtensionLengthByType (unsigned char *pExt, int cTotal, BOOL fIgnoreKnown) { // return > 0 or -1
    // Total can be bigger than extensions.
    int fExt = TRUE;
    int iLen = 0;

    while (fExt) {
        if (cTotal < 2)
            return -1;

        unsigned char eType = *pExt & 0x7f;
        fExt = *pExt >> 7;
        unsigned char eLen = pExt[1];

        if (eLen < 1)
            return -1;

        cTotal -= eLen;
        pExt += eLen + 2;

        if (!fIgnoreKnown || (eType != 0))
            iLen += eLen + 2;
    }

    return (cTotal >= 0) ? iLen : -1;
}

static int ComputeTotalExtensionLength (unsigned char *pExt, int cTotal) { 
    return ComputeTotalExtensionLengthByType (pExt, cTotal, FALSE);
}

static void ProcessArpData (BTHPAN_ADAPTER *pAdapter, unsigned char *pPacket, int cPacket) {
    if ((cPacket != 28) && (cPacket != 46)) {
        IFDBG(DebugOut (DEBUG_WARN, L"[PAN] Warning - ARP packet with unexpected zie %d\n", cPacket));
        return;
    }

    if (pAdapter->fAddressSet) {
        // Substitute our internal addresses for Bluetooth and vice versa in ARP packets.
        InvertAddress (pPacket + 8, pAdapter->ethAddr, gpPAN->eth);
        InvertAddress (pPacket + 18, pAdapter->ethAddr, gpPAN->eth);        
    }
}

static int ProcessEthernet (
        BTHPAN_CONNECTION *pConnRecv, 
        unsigned char *pDestEther, 
        unsigned char *pSrcEther,
        unsigned int uiNetType, 
        unsigned char *p8021q, 
        unsigned char *pExt,
        int iExtSize,
        unsigned char *pData, 
        int cData) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessEthernet\n"));

    SVSUTIL_ASSERT (gpPAN->IsLocked ());
    unsigned int uiConnRef = pConnRecv->uiRef;

    if (pConnRecv->state == CONFIG_SECURITY) {
        IFDBG(DebugOut (DEBUG_WARN, L"ProcessEthernet : packet dropped because security setup is still in process\n"));
        return TRUE;
    }

    if ((pConnRecv->state != UP) || (pConnRecv->nAdapter == -1) || (gpPAN->aAdapters[pConnRecv->nAdapter].state != UP)) {
        IFDBG(DebugOut (DEBUG_WARN, L"ProcessEthernet : packet on a closed (or closing) connection or on inactive adapter\n"));
        return FALSE;
    }

    if (! pSrcEther)
        pSrcEther = pConnRecv->eth;

    if (! pDestEther)
        pDestEther = gpPAN->eth;

    BTHPAN_ADAPTER *pAdapter = &gpPAN->aAdapters[pConnRecv->nAdapter];
    pAdapter->AddRef ();
    int nAdapter = pAdapter - gpPAN->aAdapters;

    int fMCast       = ETH_IS_MULTICAST(pDestEther);
    int fBroad       = ETH_IS_BROADCAST(pDestEther);
    int fUnicast     = (! (fMCast || fBroad));
    int fUnicastToMe = fUnicast && (memcmp (gpPAN->eth, pDestEther, 6) == 0);

    int fConsumerFound = FALSE;

    if ((! fUnicastToMe) && (pAdapter->type != PANU)) {   // Do the forwarding...    
        for (int i = 0 ; (pAdapter->state == UP) && (i < SVSUTIL_ARRLEN (gpPAN->aConn)) ; ++i) {
            if ((pConnRecv != &gpPAN->aConn[i]) && (gpPAN->aConn[i].nAdapter == nAdapter) && (gpPAN->aConn[i].state == UP)) {
                BTHPAN_CONNECTION *pConnForward = &gpPAN->aConn[i];

                // Broadcast addresses are sent everywhere
                // Multicast addresses are sent everywhere except where not filtered
                // If this adapter is PANU, it sends everything over this one and only one connection
                int fSendSrc = TRUE;    // Source is always sent - this packet came from elsewhere
                int fSendDst = (memcmp (pConnForward->eth, pDestEther, 6) != 0);
                if ((! (fBroad || fMCast)) && fSendDst)
                    continue;

                if (! fSendDst)    // got it!
                    fConsumerFound = TRUE;

                BOOL fOmitPayload = FALSE;
                
                int iUnkExtSize = 0;
                if (pExt && (iExtSize > 0))
                    iUnkExtSize = ComputeTotalExtensionLengthByType(pExt, iExtSize, TRUE);

                if (iUnkExtSize < 0) {
                    // Something has gone wrong!  We should never get here since we should have
                    // already verified that the extension headers look good.
                    SVSUTIL_ASSERT(0);
                    return FALSE;
                }

                if (FilterByType (&pConnForward->filter, uiNetType)) {
                    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessEthernet :: packet filtered by net type rule\n"));
                    if (iUnkExtSize == 0)
                        continue;
                    fOmitPayload = TRUE;
                    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessEthernet :: forwarding *just* unknown extensions\n"));
                }

                if (fMCast && FilterByMCast (&pConnForward->filter, pDestEther)) {
                    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessEthernet :: packet filtered by mcast rule\n"));
                    if (iUnkExtSize == 0)
                        continue;
                    fOmitPayload = TRUE;
                    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessEthernet :: forwarding *just* unknown extensions\n"));
                }                               

                // Send the thing...
                //Changing original allocation to avoid integer overflow:
                //BD_BUFFER *pBuff = BufferAlloc (gpPAN->cDataHeaders + gpPAN->cDataTrailers + (fSendSrc ? 6 : 0) + (fSendDst ? 6 : 0) + 3 + iUnkExtSize + (p8021q ? 4 : 0) + (fOmitPayload ? 0 : cData));
                uint uitemp1, uitemp2, uitemp3, uitemp4, uitemp5;
                int itemp;
                bool bfailed = FALSE;
                //Adding small numbers, no overflow
                uitemp1 = (fSendSrc ? 6 : 0) + (fSendDst ? 6 : 0) + 3 + (p8021q ? 4 : 0);
                if(!SUCCEEDED(IntToUInt(gpPAN->cDataHeaders, &uitemp2))||
                    !SUCCEEDED(IntToUInt(gpPAN->cDataTrailers, &uitemp3))||
                    !SUCCEEDED(IntToUInt(iUnkExtSize, &uitemp4)))
                    bfailed = TRUE;
                
                if(!SUCCEEDED(CeUIntAdd4(uitemp1, uitemp2, uitemp3, uitemp4, &uitemp5)))
                    bfailed = TRUE;
                
                if(!fOmitPayload){
                    if(!SUCCEEDED(IntToUInt(cData, &uitemp1))||
                        !SUCCEEDED(UIntAdd(uitemp5, uitemp1, &uitemp5)))
                        bfailed = TRUE;
                }
                if(!SUCCEEDED(UIntToInt(uitemp5, &itemp))) 
                    bfailed = TRUE;
                
                BD_BUFFER *pBuff = NULL;

                if(!bfailed) //only attmpt to allocate memory if no overflow
                    pBuff = BufferAlloc (itemp);
                
                if (pBuff) {
                    pBuff->cStart = gpPAN->cDataHeaders;
                    pBuff->cEnd   = pBuff->cSize - gpPAN->cDataTrailers;

                    // ** Add addresses **

                    unsigned char *p = pBuff->pBuffer + pBuff->cStart;
                    if (fSendSrc && fSendDst) {
                        *p++ = (iUnkExtSize ? 0x80 : 0x00);    // general ether
                        memcpy (p, pDestEther, 6);
                        p += 6;
                        memcpy (p, pSrcEther, 6);
                        p += 6;                        
                    } else if (fSendSrc) {
                        *p++ = (iUnkExtSize ? 0x83 : 0x03);    // source only
                        memcpy (p, pSrcEther, 6);
                        p += 6;
                    } else if (fSendDst) {
                        *p++ = (iUnkExtSize ? 0x84 : 0x04);    // dest only
                        memcpy (p, pDestEther, 6);
                        p += 6;
                    } else {
                        *p++ = (iUnkExtSize ? 0x82 : 0x02);    // compressed
                    }

                    // ** Add protocol type **

                    if (p8021q) { // always use 802.1q header if present
                        *p++ = 0x81;
                        *p++ = 0x00;
                    } else if (fOmitPayload) { // zero out protocol when filtered
                        *p++ = 0x00;
                        *p++ = 0x00;
                    } else {
                        *p++ = uiNetType >> 8;
                        *p++ = uiNetType & 0xff;
                    }

                    // ** Add unknown extensions **

                    while (iUnkExtSize > 0) {        
                        unsigned char eType = *pExt & 0x7f;
                        unsigned int eLen = pExt[1];

                        if (eType != 0) {
                            if ((iUnkExtSize - (2 + eLen)) > 0)
                                *pExt |= 0x80; // more extensions
                            else
                                *pExt &= 0x7f; // last extension
                            
                            // Add this unknown extension to the packet
                            memcpy (p, pExt, eLen + 2);
                            p += (2 + eLen);
                            
                            iUnkExtSize -= (2 + eLen);
                        }
                        
                        pExt += (2 + eLen);                        
                    }

                    SVSUTIL_ASSERT(iUnkExtSize == 0);

                    // ** Add 802.1q header **
                    
                    if (p8021q) {
                        memcpy(p, p8021q, 4);
                        p += 2;
                        if (fOmitPayload) {
                            *p++ = 0x00; 
                            *p++ = 0x00;        
                        } else
                            p += 2;                       
                    }

                    // ** Add payload **

                    if (! fOmitPayload) {
                        memcpy (p, pData, cData);
                        p += cData;
                    }

                    SVSUTIL_ASSERT (p == pBuff->pBuffer + pBuff->cEnd);

                    gpPAN->AddRef ();
                    gpPAN->Unlock ();

                    IFDBG(DumpBnepPacket (L"Routing packet.", pBuff));

                    if (ERROR_SUCCESS != gpPAN->l2cap_if.l2ca_DataDown_In (gpPAN->hL2CAP, ConnToCtx (pConnForward), pConnForward->cid, pBuff)) {
                        if (pBuff->pFree)
                            pBuff->pFree (pBuff);
                    }

                    gpPAN->Lock ();
                    gpPAN->DelRef ();
                }
            }
        }
    }

    if (fBroad || fMCast || (fUnicast && (! fConsumerFound))) {
           if ((pAdapter->ulPacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS)               ||
            (fBroad && (pAdapter->ulPacketFilter & NDIS_PACKET_TYPE_BROADCAST))     ||
            (fUnicast && (pAdapter->ulPacketFilter & NDIS_PACKET_TYPE_DIRECTED))    ||
            (fMCast && (pAdapter->ulPacketFilter & (NDIS_PACKET_TYPE_ALL_MULTICAST))) ||
            (fMCast && (pAdapter->ulPacketFilter & (NDIS_PACKET_TYPE_MULTICAST)) && (! FilterByMCast (&pAdapter->filter, pDestEther)))) {

            PREFAST_SUPPRESS(12011, L"No overflow since cData is L2CAP MTU at most.");
            unsigned char *pBuff = (unsigned char *)LocalAlloc (LMEM_FIXED, sizeof(ETH_HDR) + cData);
            if (pBuff) {
                if (pAdapter->fAddressSet && (memcmp (pDestEther, gpPAN->eth, 6) == 0))
                    memcpy (pBuff, pAdapter->ethAddr, 6);
                else
                    memcpy (pBuff, pDestEther, 6);

                memcpy (pBuff + 6, pSrcEther, 6);
                pBuff[12] = uiNetType >> 8;
                pBuff[13] = uiNetType & 0xff;
                memcpy (pBuff + 14, pData, cData);

                if ((uiNetType == 0x0806) || (uiNetType == 0x8035))
                    ProcessArpData (pAdapter, pBuff + 14, cData);

                NDIS_PACKET *pPacket = NULL;
                NDIS_STATUS status;

                NdisAllocatePacket(&status, &pPacket, pAdapter->NdisPacketPool);
                if (status == NDIS_STATUS_SUCCESS) {
                    // Set OOB information
                    NDIS_SET_PACKET_HEADER_SIZE(pPacket, sizeof(ETH_HDR));

                    // Allocate buffer 
                    NDIS_BUFFER *pBuffer = NULL;
                    NdisAllocateBuffer (&status, &pBuffer, pAdapter->NdisBufferPool, pBuff, sizeof(ETH_HDR) + cData);
          
                    if (status == NDIS_STATUS_SUCCESS) {
                        // Chain allocated buffer to packet
                        NdisChainBufferAtFront(pPacket, pBuffer);

                        // Packet is ok, indicate it up      
                        NdisAdjustBufferLength(pBuffer, sizeof(ETH_HDR) + cData);
                        pPacket->Private.ValidCounts = FALSE;
                        NDIS_SET_PACKET_STATUS(pPacket, NDIS_STATUS_SUCCESS);
                        *(void **)&pPacket->MiniportReserved[0] = pBuff;

                        gpPAN->AddRef ();
                        gpPAN->Unlock ();

                        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessEthernet :: indicating packet up\n"));

                        IFDBG(DebugOut (DEBUG_PAN_PACKETS, L"PACKET UP <ETHER>\n"));
                        DumpBuff (DEBUG_PAN_PACKETS, pBuff, sizeof(ETH_HDR) + cData);

                        NdisMIndicateReceivePacket(pAdapter->hAdapter, &pPacket, 1);

                        gpPAN->Lock ();
                        gpPAN->DelRef ();
                    } else {
                        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessEthernet :: OOM allocating buffer descriptor\n"));
                        NdisFreePacket (pPacket);
                        LocalFree (pBuff);
                    }
                } else {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessEthernet :: OOM allocating packet descriptor\n"));
                    LocalFree (pBuff);
                }
            } else {
                IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessEthernet :: OOM allocating buffer\n"));
            }
        }
    }

    pAdapter->DelRef ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessEthernet DONE\n"));
    return TRUE;
}

int RetryConnectionFilter (BTHPAN_CONNECTION *pConn, BD_BUFFER *pBuff) {
    SVSUTIL_ASSERT(gpPAN->IsLocked());    
                
    gpPAN->AddRef ();
    gpPAN->Unlock ();

    int iRes = gpPAN->l2cap_if.l2ca_DataDown_In (gpPAN->hL2CAP, ConnToCtx (pConn), pConn->cid, pBuff);

    gpPAN->Lock ();
    gpPAN->DelRef ();

    if (ERROR_SUCCESS != iRes) {
        if (pBuff->pFree)
            pBuff->pFree (pBuff);
    }

    return iRes;
}

// Need to retry sending filter at least once to pass PAN qualification
static DWORD WINAPI RetryNetConnectionFilter (LPVOID pvArg) {
    gpPAN->Lock ();

    BTHPAN_CONNECTION *pConn = ConnFromCtx (pvArg);
    if (pConn && (pConn->nAdapter != -1) && (pConn->state == UP)) {
        if (pConn->pNetFilterRetryBuff) {            
            IFDBG(DumpBnepPacket (L"Sending retry for net filter.", pConn->pNetFilterRetryBuff));            
            RetryConnectionFilter (pConn, pConn->pNetFilterRetryBuff);

            pConn->scNetFilter = btutil_ScheduleEvent (TimeoutConnection, ConnToCtx (pConn), BTHPAN_FILTER_RETRY_TO);

            pConn->pNetFilterRetryBuff = NULL;
        }
        else
            pConn->scNetFilter = NULL;
    }

    gpPAN->Unlock ();

    return 0;
}

// Need to retry sending filter at least once to pass PAN qualification
static DWORD WINAPI RetryMCastConnectionFilter (LPVOID pvArg) {
    gpPAN->Lock ();

    BTHPAN_CONNECTION *pConn = ConnFromCtx (pvArg);
    if (pConn && (pConn->nAdapter != -1) && (pConn->state == UP)) {
        if (pConn->pMCastFilterRetryBuff) {            
            IFDBG(DumpBnepPacket (L"Sending retry for mcast filter.", pConn->pMCastFilterRetryBuff));            
            RetryConnectionFilter (pConn, pConn->pMCastFilterRetryBuff);

            pConn->scMCastFilter = btutil_ScheduleEvent (TimeoutConnection, ConnToCtx (pConn), BTHPAN_FILTER_RETRY_TO);

            pConn->pMCastFilterRetryBuff = NULL;
        }
        else
            pConn->scMCastFilter = NULL;
    }

    gpPAN->Unlock ();

    return 0;       
}

static void SetConnectionFilter (BTHPAN_CONNECTION *pConn, FILTER_TYPE which) {
// Only one filter message can be outstanding at a time within a category.
    SVSUTIL_ASSERT (gpPAN->IsLocked ());

    if ((which == ALL) || (which == MCAST)) {
        if (pConn->fMCastMsgOut)
            pConn->fMCastMsgReq = TRUE;
    }

    if ((which == ALL) || (which == NETTYPE)) {
        if (pConn->fNetTypeMsgOut)
            pConn->fNetTypeMsgReq = TRUE;
    }

    //set up variables to check for integer overflow
    uint uiDataHeaders = 0, uiDataTrailers = 0, uiMult, uitemp1;
    int itemp;
    bool bfailed = FALSE;
    if(!SUCCEEDED(IntToUInt(gpPAN->cDataHeaders, &uiDataHeaders))||
        !SUCCEEDED(IntToUInt(gpPAN->cDataTrailers, &uiDataTrailers)))
        bfailed = TRUE;
    
    if (((which == ALL) || (which == MCAST)) && (! pConn->fMCastMsgOut)) {
        pConn->fMCastMsgOut = TRUE;

        //integer overflow checks, behave the same way as no memory allocation failure, since don't know how much memory to allocate
        //BD_BUFFER *pBuff = BufferAlloc (gpPAN->cDataHeaders + gpPAN->cDataTrailers + 4 + 12 * pConn->filter.cMCasts);
        if(!SUCCEEDED(UIntMult(pConn->filter.cMCasts, 12, &uiMult)))
            bfailed = TRUE;
        
        if(!SUCCEEDED(CeUIntAdd4(uiDataHeaders, uiDataTrailers, uiMult, 4, &uitemp1)))
            bfailed = TRUE;
        
        if(!SUCCEEDED(UIntToInt(uitemp1, &itemp))) 
            bfailed = TRUE;
        
        BD_BUFFER *pBuff = NULL;
        if(!bfailed)
            pBuff = BufferAlloc (itemp);
        
        if (pBuff) {
            pBuff->cStart = gpPAN->cDataHeaders;
            pBuff->cEnd   = pBuff->cSize - gpPAN->cDataTrailers;

            unsigned short usListSize = pConn->filter.cMCasts * 12;
            unsigned char *p = pBuff->pBuffer + pBuff->cStart;
            *p++ = 0x01;    // Control
            *p++ = 0x05;    // mcast filter
            *p++ = usListSize >> 8;      // Length
            *p++ = usListSize & 0xff;

            for (int i = 0 ; i < (int)pConn->filter.cMCasts ; ++i) {
                memcpy (p, &pConn->filter.amc[i].from, 6);
                p += 6;
                memcpy (p, &pConn->filter.amc[i].to, 6);
                p += 6;
            }

            SVSUTIL_ASSERT (p == pBuff->pBuffer + pBuff->cEnd);

            pConn->pMCastFilterRetryBuff = BufferCopy (pBuff);
            pConn->scMCastFilter = btutil_ScheduleEvent (RetryMCastConnectionFilter, ConnToCtx (pConn), BTHPAN_FILTER_RETRY_TO);

            gpPAN->AddRef ();
            gpPAN->Unlock ();

            IFDBG(DumpBnepPacket (L"Setting multicast filter.", pBuff));

            int iRes = gpPAN->l2cap_if.l2ca_DataDown_In (gpPAN->hL2CAP, ConnToCtx (pConn), pConn->cid, pBuff);

            gpPAN->Lock ();
            gpPAN->DelRef ();

            if (ERROR_SUCCESS != iRes) {
                if (pConn->scMCastFilter) {
                    btutil_UnScheduleEvent (pConn->scMCastFilter);
                    pConn->scMCastFilter = NULL;
                }
                if (pConn->pMCastFilterRetryBuff && pConn->pMCastFilterRetryBuff->pFree) {
                    pConn->pMCastFilterRetryBuff->pFree (pConn->pMCastFilterRetryBuff);
                    pConn->pMCastFilterRetryBuff = NULL;
                }
                if (pBuff->pFree)
                    pBuff->pFree (pBuff);
            }
        }
    }

    if (((which == ALL) || (which == NETTYPE)) && (! pConn->fNetTypeMsgOut)) {
        pConn->fNetTypeMsgOut = TRUE;
        //BD_BUFFER *pBuff = BufferAlloc (gpPAN->cDataHeaders + gpPAN->cDataTrailers + 4 + 4 * pConn->filter.cNetTypes);
        //integer overflow checks, behave the same way as no memory allocation failure, since don't know how much memory to allocate 
        if(!SUCCEEDED(UIntMult(pConn->filter.cNetTypes, 4, &uiMult))) 
            bfailed = TRUE;

        if(!SUCCEEDED(CeUIntAdd4(uiDataHeaders, uiDataTrailers, uiMult, 4, &uitemp1)))
            bfailed = TRUE;
        
        if(!SUCCEEDED(UIntToInt(uitemp1, &itemp))) 
            bfailed = TRUE;

        BD_BUFFER *pBuff = NULL;
        if(!bfailed)
             pBuff = BufferAlloc (itemp);
        
        if (pBuff) {
            pBuff->cStart = gpPAN->cDataHeaders;
            pBuff->cEnd   = pBuff->cSize - gpPAN->cDataTrailers;

            unsigned short usListSize = pConn->filter.cNetTypes * 4;
            unsigned char *p = pBuff->pBuffer + pBuff->cStart;
            *p++ = 0x01;    // Control
            *p++ = 0x03;    // mcast filter
            *p++ = usListSize >> 8;      // Length
            *p++ = usListSize & 0xff;

            for (int i = 0 ; i < (int)pConn->filter.cNetTypes ; ++i) {
                *p++ = pConn->filter.ant[i].from >> 8;
                *p++ = pConn->filter.ant[i].from & 0xff;
                *p++ = pConn->filter.ant[i].from >> 8;
                *p++ = pConn->filter.ant[i].from & 0xff;
            }

            SVSUTIL_ASSERT (p == pBuff->pBuffer + pBuff->cEnd);

            pConn->pNetFilterRetryBuff = BufferCopy (pBuff);
            pConn->scNetFilter = btutil_ScheduleEvent (RetryNetConnectionFilter, ConnToCtx (pConn), BTHPAN_FILTER_RETRY_TO);

            gpPAN->AddRef ();
            gpPAN->Unlock ();

            IFDBG(DumpBnepPacket (L"Setting net type filter.", pBuff));

            int iRes = gpPAN->l2cap_if.l2ca_DataDown_In (gpPAN->hL2CAP, ConnToCtx (pConn), pConn->cid, pBuff);

            gpPAN->Lock ();
            gpPAN->DelRef ();            

            if (ERROR_SUCCESS != iRes) {
                if (pConn->scNetFilter) {
                    btutil_UnScheduleEvent (pConn->scNetFilter);
                    pConn->scNetFilter = NULL;
                }
                if (pConn->pNetFilterRetryBuff && pConn->pNetFilterRetryBuff->pFree) {
                    pConn->pNetFilterRetryBuff->pFree (pConn->pNetFilterRetryBuff);
                    pConn->pNetFilterRetryBuff = NULL;
                }
                if (pBuff->pFree)
                    pBuff->pFree (pBuff);
            }
        }
    }
}

static DWORD WINAPI AuthenticatingThread (void *pvArg) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"Authenticating thread started for connection 0x%08x\n", pvArg));

    int fAuth = FALSE;
    int fEncr = FALSE;

    BT_ADDR bt;
    gpPAN->Lock ();
    BTHPAN_CONNECTION *pConn = ConnFromCtx (pvArg);
    if (pConn && ((pConn->state == CONFIG_SECURITY) || (pConn->state == CONFIG_BNEP)) && (pConn->nAdapter != -1)) {
        BTHPAN_ADAPTER *pAdapter = &gpPAN->aAdapters[pConn->nAdapter];
        bt = SET_NAP_SAP(pConn->ba.NAP, pConn->ba.SAP);
        fAuth = gpPAN->fAuthenticate && (! pConn->fAuthenticated);
        fEncr = gpPAN->fEncrypt && (! pConn->fEncrypted);
    } else
        pConn = NULL;

    gpPAN->Unlock ();

    if (! pConn) {
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"Authenticating thread for connection 0x%08x exiting without authentication\n", pvArg));
        return 0;
    }

    int iErr = ERROR_SUCCESS;

    if (fAuth)
        iErr = BthAuthenticate (&bt);

    if (fEncr && (iErr == ERROR_SUCCESS))
        iErr = BthSetEncryption (&bt, TRUE);

    gpPAN->Lock ();
    if (ConnFromCtx (pvArg) == pConn) {
        if (iErr != ERROR_SUCCESS) { // CloseConnection
            IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] Authenticating for connection 0x%08x failed, error %d\n", pvArg, iErr));
            CloseConnection (pConn, NULL);
        } else {
            IFDBG(DebugOut (DEBUG_PAN_TRACE, L"Authenticating for connection 0x%08x successful. Connection UP.\n", pvArg));
            pConn->fAuthenticated = fAuth;
            pConn->fEncrypted = fEncr;

            if (pConn->state == CONFIG_SECURITY)
                SetConnectionToUp(pConn);

            IFDBG(DebugOut (DEBUG_PAN_TRACE, L"Completing pending packets for authenticated connection.\n"));

            BTHPAN_ADAPTER *pAdapter = &gpPAN->aAdapters[pConn->nAdapter];

            while (pConn->pPacketsPending) {
                BD_BUFFER *pBuff = pConn->pPacketsPending->pBuffer;
                pBuff->fMustCopy = TRUE;

                SVSUTIL_ASSERT(pBuff);                

                pAdapter->AddRef ();
                gpPAN->AddRef ();
                gpPAN->Unlock ();

                IFDBG(DumpBnepPacket (L"[PAN] Now sending pending packet:", pBuff));

                if (ERROR_SUCCESS != gpPAN->l2cap_if.l2ca_DataDown_In (gpPAN->hL2CAP, pvArg, pConn->cid, pBuff)) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] Connection 0x%08x - Error sending pending packet in authentication thread\n", pvArg));
                }

                gpPAN->Lock ();
                gpPAN->DelRef ();
                pAdapter->DelRef ();
                
                if (pConn->pPacketsPending)
                {
                    BNEPPacket *pFree = pConn->pPacketsPending;
                    pConn->pPacketsPending = pConn->pPacketsPending->pNext;
                    delete pFree;
                }
            }

            if (pConn->state == UP)
                MpMediaStatusIndicate (pAdapter);

            if ((ConnFromCtx (pvArg) == pConn) && (pConn->state == UP))
                SetConnectionFilter (pConn, ALL);
        }
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] Connection 0x%08x disappeared in authentication thread\n", pvArg));
    }

    gpPAN->Unlock ();

    return 0;
}

static int StartAuthenticatingThread (BTHPAN_CONNECTION *pConn) {
    BTHPAN_ADAPTER *pAdapter = &gpPAN->aAdapters[pConn->nAdapter];
    if ((gpPAN->fAuthenticate && (! pConn->fAuthenticated)) || (gpPAN->fEncrypt && (! pConn->fEncrypted))) {
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"New connection 0x%08x transitioned in authenticating phase\n", pConn));
        btutil_ScheduleEvent (AuthenticatingThread, ConnToCtx (pConn));
        return TRUE;
    }

    return FALSE;
}

static int ProcessControl (BTHPAN_CONNECTION *pConn,
                           int fExtensions, unsigned char *pData, int cData) {
// Returns how many bytes the control message took. In particular...
//  -1 if an error requires abortive closure of connection
//  0 if command was not understood and extension processing needs to abort, but
//      the connection may proceed.

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl\n"));

    if (cData < 1) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl ERROR: packet too short\n"));

        return -1;
    }

    int iSkip = 0;
    unsigned int uiConnRef = pConn->uiRef;

    switch (*pData) {
        case 0x00:       // Control command not understood
            {
                if (cData < 2) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"ProcessControl ERROR: packet too short\n"));
    
                    return -1;
                }

                unsigned char cMissedType = pData[1];
                IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl: control command not understood (%d)\n", cMissedType));

                if ((cMissedType < 0x03) || (cMissedType > 0x06)) { // Can only not accept the filter command
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl : the other side does not understand mandatory commands - closing...\n"));
    
                    return -1;
                }

                iSkip = 2;                
            }
            break;

        case 0x01:      // Setup connection request
            {
                if (cData < 6) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl ERROR: packet too short\n"));
    
                    return -1;
                }

                unsigned char cUuidLen = pData[1];
                IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl: setup connection request (uuid len=%d)\n", cUuidLen));

                unsigned short usResponseCode = 0; // assume success
                int fMediaIndicate = FALSE;
                BTHPAN_ADAPTER *pAdapter = NULL;

                if ((pConn->nAdapter != -1) || (pConn->state != CONFIG_BNEP)) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl : connection request on established BNEP connection - rejecting\n"));

                    usResponseCode = 0x0004; // Connection not allowed
                } else if (((cUuidLen != 2) && (cUuidLen != 4) && (cUuidLen != 16)) || (cData < 2 + cUuidLen * 2)) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl : uuid length malformed or packet too short - rejecting...\n"));

                    usResponseCode = 0x0003; // invalid UUID size
                } else {

                    // Get destination service UUID

                    GUID guid = Bluetooth_Base_UUID;
                    DWORD dwOffset = 0;
                    if (cUuidLen == 16)
                        memcpy (&guid, pData + 2, sizeof(guid));
                    else if (cUuidLen == 4)
                        dwOffset = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
                    else if (cUuidLen == 2)
                        dwOffset = (pData[2] << 8) | pData[3];

                    guid.Data1 += dwOffset;

                    pAdapter = AdapterFromUUID (&guid);
                    if (! pAdapter) {
                        IFDBG(DebugOut (DEBUG_WARN, L"[PAN] ProcessControl : no adapter or wrong state, rejecting command state.\n"));

                        usResponseCode = 0x0001; // invalid dest UUID
                    }

                    // Get source service UUID

                    if (usResponseCode == 0) {
                        guid = Bluetooth_Base_UUID;
                        dwOffset = 0;
                        if (cUuidLen == 16)
                            memcpy (&guid, pData + 2 + cUuidLen, sizeof(guid));
                        else if (cUuidLen == 4)
                            dwOffset = (pData[2 + cUuidLen] << 24) | (pData[3 + cUuidLen] << 16) | (pData[4 + cUuidLen] << 8) | pData[5 + cUuidLen];
                        else if (cUuidLen == 2)
                            dwOffset = (pData[2 + cUuidLen] << 8) | pData[3 + cUuidLen];

                        guid.Data1 += dwOffset;

                        if (! VerifyPanUUID(&guid)) {
                            IFDBG(DebugOut (DEBUG_WARN, L"[PAN] ProcessControl : invalid source service UUID, rejecting command state.\n"));

                            usResponseCode = 0x0002; // invalid source UUID
                        }
                    }
                }

                // If no error occured, transition the adapter into open state
                if (usResponseCode == 0) {
                    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl : Adapter successfully transitioned into open state.\n"));
                    btutil_UnScheduleEvent (pConn->scTimeout);
                    pConn->nAdapter = pAdapter - gpPAN->aAdapters;
                    if (! StartAuthenticatingThread (pConn)) {
                        SetConnectionToUp(pConn);   // Adapter has started. Format the success response.
                        fMediaIndicate = TRUE;
                        pAdapter->AddRef ();
                    } else {
                        pConn->state = CONFIG_SECURITY;
                    }
                }

                unsigned char packet_body[256];

                packet_body[gpPAN->cDataHeaders]     = 0x01;
                packet_body[gpPAN->cDataHeaders + 1] = 0x02;
                packet_body[gpPAN->cDataHeaders + 2] = usResponseCode >> 8;
                packet_body[gpPAN->cDataHeaders + 3] = usResponseCode & 0xff;

                BD_BUFFER Buff;
                Buff.cSize = 4 + gpPAN->cDataTrailers + gpPAN->cDataHeaders;
                Buff.cStart = gpPAN->cDataHeaders;
                Buff.cEnd = Buff.cSize - gpPAN->cDataTrailers;
                Buff.pFree = BufferFree;
                Buff.fMustCopy = TRUE;
                Buff.pBuffer = packet_body;

                int iRes = ERROR_SUCCESS;
                if (pConn->state == CONFIG_SECURITY) {    // Delay...
                    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl : response delayed\n"));

                    BNEPPacket *pPendingPacket = new BNEPPacket(BufferCopy(&Buff));
                    if (pPendingPacket)
                        QueuePendingPacket(pConn, pPendingPacket);
                } else {
                    gpPAN->AddRef ();
                    gpPAN->Unlock ();

                    IFDBG(DumpBnepPacket (L"Sending connection response.", &Buff));

                    iRes = gpPAN->l2cap_if.l2ca_DataDown_In (gpPAN->hL2CAP, ConnToCtx (pConn), pConn->cid, &Buff);

                    if (fMediaIndicate) {
                        MpMediaStatusIndicate (pAdapter);
                        pAdapter->DelRef ();
                    }

                    gpPAN->Lock ();
                    gpPAN->DelRef ();
                }

                iSkip = (iRes == ERROR_SUCCESS) ? 2 + cUuidLen * 2 : -1;

                // TODO: Should we send this if extension is already setting filter?
                if ((iSkip != -1) && (usResponseCode == 0) && (pConn->state == UP) &&
                            (pConn->uiRef == uiConnRef))
                    SetConnectionFilter (pConn, ALL);

            }
            break;

        case 0x02:  // Setup connection response
            {
                if (cData < 3) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl ERROR: packet too short\n"));
    
                    return -1;
                }

                unsigned short usReason = (pData[1] << 8) | pData[2];

                IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl: setup connection response (reason=%d)\n", usReason));

                if ((pConn->nAdapter == -1) || (pConn->state != CONFIG_BNEP)) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl : connection in incorrect state for this message...\n"));
    
                    return -1;
                }

                int nAdapter = pConn->nAdapter;
                if (usReason == 0x0000) {
                    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl : Adapter successfully transitioned into open state.\n"));
                    btutil_UnScheduleEvent (pConn->scTimeout);
                    iSkip = 3;

                    BTHPAN_ADAPTER *pAdapter = &gpPAN->aAdapters[nAdapter];

                    if ((gpPAN->fAuthenticate && (! pConn->fAuthenticated)) || (gpPAN->fEncrypt && (! pConn->fEncrypted))) {
                        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl : CONNECTION NOT AUTHENTICATED\n"));
                        SVSUTIL_ASSERT (0);
                        iSkip = -1;
                    } else {
                        SetConnectionToUp(pConn);
                        pAdapter->AddRef ();
                        gpPAN->AddRef ();
                        gpPAN->Unlock ();
                        MpMediaStatusIndicate (&gpPAN->aAdapters[pConn->nAdapter]);
                        gpPAN->Lock ();
                        gpPAN->DelRef ();
                        pAdapter->DelRef ();

                        if (pConn->uiRef == uiConnRef)
                            SetConnectionFilter (pConn, ALL);
                    }
                } else {
                    IFDBG(DebugOut (DEBUG_WARN, L"[PAN] ProcessControl : adapter failed to transition into open state.\n"));
                    iSkip = -1;
                }                
            }
            break;

        case 0x03:  // Filter net type set
            {
                if (cData < 3) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl ERROR: packet too short\n"));
    
                    return -1;
                }

                unsigned short usResponseCode = 0x0000;
                unsigned short usNumBytes = (pData[1] << 8) | pData[2];
                unsigned short usNum = usNumBytes / 4;

                if (((3 + 4 * usNum) > cData) || (usNum * 4 != usNumBytes)) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl : packet too short - closing...\n"));
    
                    return -1;
                }

                IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl: filter net set (%d filters)\n", usNum));

                if ((pConn->nAdapter == -1) || ((pConn->state != UP) && (pConn->state != CONFIG_SECURITY))) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl : connection in incorrect state, rejecting...\n"));
    
                    usResponseCode = 0x0004;
                }

                if (usResponseCode == 0x0000) {
                    if (usNum <= SVSUTIL_ARRLEN(pConn->filter.ant)) {    // Put it in, otherwise reject...
                        pConn->filter.cNetTypes = usNum;
                        unsigned char *p = pData + 3;
                        for (int i = 0 ; i < (int)usNum ; ++i) {
                            pConn->filter.ant[i].from = (p[0] << 8) | p[1];
                            pConn->filter.ant[i].to   = (p[2] << 8) | p[3];
                            if (pConn->filter.ant[i].from > pConn->filter.ant[i].to) {
                                IFDBG(DebugOut (DEBUG_WARN, L"[PAN] ProcessControl : incorrect filter value.\n"));
                                pConn->filter.cNetTypes = 0;
                                usResponseCode = 0x0002;
                                break;
                            }

                            p += 4;
                        }
                    } else {
                        IFDBG(DebugOut (DEBUG_WARN, L"[PAN] ProcessControl : filter list too big.\n"));
                        usResponseCode = 0x0003;
                    }
                }
                
                unsigned char packet_body[256];

                packet_body[gpPAN->cDataHeaders]     = 0x01;
                packet_body[gpPAN->cDataHeaders + 1] = 0x04;
                packet_body[gpPAN->cDataHeaders + 2] = usResponseCode >> 8;
                packet_body[gpPAN->cDataHeaders + 3] = usResponseCode & 0xff;

                BD_BUFFER Buff;
                Buff.cSize = 4 + gpPAN->cDataTrailers + gpPAN->cDataHeaders;
                Buff.cStart = gpPAN->cDataHeaders;
                Buff.cEnd = Buff.cSize - gpPAN->cDataTrailers;
                Buff.pFree = BufferFree;
                Buff.fMustCopy = TRUE;
                Buff.pBuffer = packet_body;

                int iRes = ERROR_SUCCESS;
                if (pConn->state == CONFIG_SECURITY) {  // Delay...
                    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl : response delayed\n"));

                    BNEPPacket *pPendingPacket = new BNEPPacket(BufferCopy(&Buff));
                    if (pPendingPacket)
                        QueuePendingPacket(pConn, pPendingPacket);
                } else {
                    gpPAN->AddRef ();
                    gpPAN->Unlock ();

                    IFDBG(DumpBnepPacket (L"Sending net type filter response.", &Buff));

                    iRes = gpPAN->l2cap_if.l2ca_DataDown_In (gpPAN->hL2CAP, ConnToCtx (pConn), pConn->cid, &Buff);

                    gpPAN->Lock ();
                    gpPAN->DelRef ();
                }
                
                iSkip = (iRes == ERROR_SUCCESS) ? 3 + usNum * 4 : -1;                
            }
            break;

        case 0x04:  // Filter net type response
            {
                if (cData < 3) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl ERROR: packet too short\n"));
    
                    return -1;
                }

                unsigned short usReason = (pData[1] << 8) | pData[2];

                IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl: filter net set (reason=%d)\n", usReason));

                if ((pConn->nAdapter == -1) || (pConn->state != UP)) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl : connection in incorrect state for this message...\n"));
    
                    return -1;
                }

                if (pConn->scNetFilter) {
                    btutil_UnScheduleEvent (pConn->scNetFilter);
                    pConn->scNetFilter = NULL;
                }
                if (pConn->pNetFilterRetryBuff && pConn->pNetFilterRetryBuff->pFree) {
                    pConn->pNetFilterRetryBuff->pFree (pConn->pNetFilterRetryBuff);
                    pConn->pNetFilterRetryBuff = NULL;
                }

                pConn->fNetTypeMsgOut = FALSE;
                if (pConn->fNetTypeMsgReq) {
                    pConn->fNetTypeMsgReq = FALSE;
                    SetConnectionFilter (pConn, NETTYPE);
                }            

                iSkip = 3;
            }
            break;

        case 0x05:  // Filter multicast set
            {
                if (cData < 3) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl ERROR: packet too short\n"));
    
                    return -1;
                }

                unsigned short usResponseCode = 0x0000;
                unsigned short usNumBytes = (pData[1] << 8) | pData[2];
                int usNum = usNumBytes / 12;

                if (((3 + 12 * usNum) > cData) || ((12 * usNum) != usNumBytes)) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl : packet too short or inconsistent size - closing...\n"));
    
                    return -1;
                }

                IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl: filter mcast set (%d filters)\n", usNum));

                if ((pConn->nAdapter == -1) || ((pConn->state != UP) && (pConn->state != CONFIG_SECURITY))) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl : connection in incorrect state, rejecting...\n"));
    
                    usResponseCode = 0x0004;
                }

                if (usResponseCode == 0x0000) {
                    if (usNum <= SVSUTIL_ARRLEN(pConn->filter.amc)) {    // Put it in, otherwise reject...
                        pConn->filter.cMCasts = usNum;
                        unsigned char *p = pData + 3;
                        for (int i = 0 ; i < (int)usNum ; ++i) {
                            memcpy (&pConn->filter.amc[i].from, p,     6);
                            memcpy (&pConn->filter.amc[i].to,   p + 6, 6);
                            if (SET_NAP_SAP(pConn->filter.amc[i].from.NAP, pConn->filter.amc[i].from.SAP) >
                                SET_NAP_SAP(pConn->filter.amc[i].to.NAP, pConn->filter.amc[i].to.SAP)) {
                                IFDBG(DebugOut (DEBUG_WARN, L"[PAN] ProcessControl : incorrect filter value.\n"));
                                pConn->filter.cMCasts = 0;
                                usResponseCode = 0x0002;
                                break;
                            }

                            p += 12;
                        }
                    } else {
                        IFDBG(DebugOut (DEBUG_WARN, L"[PAN] ProcessControl : filter list too big.\n"));
                        usResponseCode = 0x0003;
                    }
                }
                
                unsigned char packet_body[256];

                packet_body[gpPAN->cDataHeaders]     = 0x01;
                packet_body[gpPAN->cDataHeaders + 1] = 0x06;
                packet_body[gpPAN->cDataHeaders + 2] = usResponseCode >> 8;
                packet_body[gpPAN->cDataHeaders + 3] = usResponseCode & 0xff;

                BD_BUFFER Buff;
                Buff.cSize = 4 + gpPAN->cDataTrailers + gpPAN->cDataHeaders;
                Buff.cStart = gpPAN->cDataHeaders;
                Buff.cEnd = Buff.cSize - gpPAN->cDataTrailers;
                Buff.pFree = BufferFree;
                Buff.fMustCopy = TRUE;
                Buff.pBuffer = packet_body;

                int iRes = ERROR_SUCCESS;
                if (pConn->state == CONFIG_SECURITY) {  // Delay...
                    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl : response delayed\n"));

                    BNEPPacket *pPendingPacket = new BNEPPacket(BufferCopy(&Buff));
                    if (pPendingPacket)
                        QueuePendingPacket(pConn, pPendingPacket);                    
                } else {
                    gpPAN->AddRef ();
                    gpPAN->Unlock ();

                    IFDBG(DumpBnepPacket (L"Sending multicast filter response.", &Buff));

                    iRes = gpPAN->l2cap_if.l2ca_DataDown_In (gpPAN->hL2CAP, ConnToCtx (pConn), pConn->cid, &Buff);

                    gpPAN->Lock ();
                    gpPAN->DelRef ();
                }
                
                iSkip = (iRes == ERROR_SUCCESS) ? 3 + usNum * 12 : -1;                
            }
            break;

        case 0x06:  // Filter multicast response
            {
                if (cData < 3) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl ERROR: packet too short\n"));
    
                    return -1;
                }

                unsigned short usReason = (pData[1] << 8) | pData[2];

                IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl: filter mcast set (reason=%d)\n", usReason));

                if ((pConn->nAdapter == -1) || (pConn->state != UP)) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl : connection in incorrect state for this message...\n"));
    
                    return -1;
                }

                if (pConn->scMCastFilter) {
                    btutil_UnScheduleEvent (pConn->scMCastFilter);
                    pConn->scMCastFilter = NULL;
                }
                if (pConn->pMCastFilterRetryBuff && pConn->pMCastFilterRetryBuff->pFree) {
                    pConn->pMCastFilterRetryBuff->pFree (pConn->pMCastFilterRetryBuff);
                    pConn->pMCastFilterRetryBuff = NULL;
                }

                pConn->fMCastMsgOut = FALSE;
                if (pConn->fMCastMsgReq) {
                    pConn->fMCastMsgReq = FALSE;
                    SetConnectionFilter (pConn, MCAST);
                }                

                iSkip = 3;
            }
            break;

        default:    // Send command not understood...
            {   
                IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] ProcessControl: command not understood\n"));

                unsigned char packet_body[256];

                packet_body[gpPAN->cDataHeaders]     = 0x01;
                packet_body[gpPAN->cDataHeaders + 1] = 0x00;
                packet_body[gpPAN->cDataHeaders + 2] = *pData;

                BD_BUFFER Buff;
                Buff.cSize = 3 + gpPAN->cDataTrailers + gpPAN->cDataHeaders;
                Buff.cStart = gpPAN->cDataHeaders;
                Buff.cEnd = Buff.cSize - gpPAN->cDataTrailers;
                Buff.pFree = BufferFree;
                Buff.fMustCopy = TRUE;
                Buff.pBuffer = packet_body;

                // TODO: Queue this up if we are in CONFIG_SECURITY?

                gpPAN->AddRef ();
                gpPAN->Unlock ();

                IFDBG(DumpBnepPacket (L"Sending command not understood response.", &Buff));

                int iRes = gpPAN->l2cap_if.l2ca_DataDown_In (gpPAN->hL2CAP, ConnToCtx (pConn), pConn->cid, &Buff);

                gpPAN->Lock ();
                gpPAN->DelRef ();

                iSkip = (iRes == ERROR_SUCCESS) ? 0 : -1; 
            }
            break;
    }

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"ProcessControl DONE\n"));

    return iSkip;
}


static DWORD PanConnect (BTHPAN_ADAPTER *pAdapter, BD_ADDR *pba, GUID *pDestServiceId) {
    pAdapter->AddRef ();
    gpPAN->AddRef ();
    gpPAN->Unlock ();
    MpAttemptConnection(pAdapter, pba, pDestServiceId);
    gpPAN->Lock ();
    gpPAN->DelRef ();
    pAdapter->DelRef ();
    return ERROR_SUCCESS;
}

static DWORD PanDisconnect (BTHPAN_ADAPTER *pAdapter, BD_ADDR *pba) {    
    // Close all connections to specified BD_ADDR
    for (int i = 0 ; i < SVSUTIL_ARRLEN (gpPAN->aConn) ; ++i) {
        if (gpPAN->aConn[i].ba == *pba)
            CloseConnection(&gpPAN->aConn[i], pAdapter);        
    }

    return ERROR_SUCCESS;
}

void NotifyNDISOfPanState (BOOL fActivate) {
    if (fActivate) {
        NDIS_STRING ndisString;
        NdisInitUnicodeString(&ndisString, L"BTPAN1");

        if (gpPAN->ndisDriverHandle && 
            (NDIS_STATUS_SUCCESS != NdisIMInitializeDeviceInstance(gpPAN->ndisDriverHandle, &ndisString))) {
            ASSERT(0);
        }
    } else {
        BTHPAN_ADAPTER *pAdapter = NULL;
        for (int i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aAdapters) ; ++i) {
            if (gpPAN->aAdapters[i].state != DOWN) {
                if (gpPAN->ndisDriverHandle &&
                    (NDIS_STATUS_SUCCESS != NdisIMDeInitializeDeviceInstance(gpPAN->aAdapters[i].hAdapter))) {
                    ASSERT(0);
                }
            }
        }
    }
}

static DWORD WINAPI StackUp (LPVOID lpVoid) {
    BT_ADDR bt;
    int iAttempts = 0;
    int status;

    do {
        iAttempts++;
        status = BthReadLocalAddr (&bt);
        if (status != ERROR_SUCCESS)
            Sleep(500);
    } while ((iAttempts <= 3) && (status != ERROR_SUCCESS));
    
    gpPAN->ba.NAP = GET_NAP(bt);
    gpPAN->ba.SAP = GET_SAP(bt);
    BtToEth (gpPAN->eth, &gpPAN->ba);

    gpPAN->fStackUp = TRUE;

    MpCleanUpConnections();

    if (gpPAN->ActivateRefCount)
        NotifyNDISOfPanState(TRUE);

    return 0;
}

static DWORD WINAPI StackDown (LPVOID lpVoid) {
    gpPAN->ba.NAP = 0;
    gpPAN->ba.SAP = 0;
    memset (gpPAN->eth, 0, sizeof(gpPAN->eth));

    gpPAN->fStackUp = FALSE;

    MpCleanUpConnections();

    if (gpPAN->ActivateRefCount)
        NotifyNDISOfPanState(FALSE);

    return 0;
}

static DWORD WINAPI StackReset (LPVOID lpVoid) {
    MpCleanUpConnections ();
    BT_ADDR bt;
    int iAttempts = 0;
    int status;

    do {
        iAttempts++;
        status = BthReadLocalAddr (&bt);
        if (status != ERROR_SUCCESS)
            Sleep(500);
    } while ((iAttempts <= 3) && (status != ERROR_SUCCESS));

    gpPAN->ba.NAP = GET_NAP(bt);
    gpPAN->ba.SAP = GET_SAP(bt);
    BtToEth (gpPAN->eth, &gpPAN->ba);

    return 0;
}

//
// Bluetooth interface functions
//
static int pan_data_up_ind (void *pUserContext, unsigned short cid, BD_BUFFER *pBuffer) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_data_up_ind 0x%08x cid=0x%04x, len=%d\n", pUserContext, cid, BufferTotal (pBuffer)));

    IFDBG(DumpBnepPacket (L"Received BNEP packet.", pBuffer));

    if (! gpPAN) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_data_up_ind: context doesn't exist\n"));
        return ERROR_SUCCESS;
    }

    gpPAN->Lock ();
    if (gpPAN->state != UP) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_data_up_ind: context not up\n"));
        gpPAN->Unlock ();
        return ERROR_SUCCESS;
    }

    BTHPAN_CONNECTION *pConn = ConnFromCid (cid);

    if (pConn && ((pConn->state == CONFIG_BNEP) || (pConn->state == CONFIG_SECURITY) || (pConn->state == UP))) {
        unsigned int uiConnRef = pConn->uiRef;
        unsigned char uCmd = ((unsigned char)pBuffer->pBuffer[pBuffer->cStart]) & 0x7f;
        int fExt = ((unsigned char)pBuffer->pBuffer[pBuffer->cStart]) >> 7;
        unsigned int uiNetType = 0;
        unsigned char *p8021q = NULL;
        unsigned char *pData = NULL;
        int cData = 0;
        unsigned char *pExt = NULL;
        int iExtSize = 0;
        BOOL fAbortConnection = FALSE;

        if (uCmd == 0x01) {// Control message
            int iSkip = ProcessControl (pConn, FALSE, &pBuffer->pBuffer[pBuffer->cStart + 1], BufferTotal (pBuffer)-1);
            if (fExt && iSkip > 0) {
                pExt = &pBuffer->pBuffer[pBuffer->cStart + 1 + iSkip];
                iExtSize = ComputeTotalExtensionLength (&pBuffer->pBuffer[pBuffer->cStart + 1 + iSkip], BufferTotal (pBuffer) - iSkip - 1);
            }
        }
        else if (uCmd == 0x00) { // General Ethernet packet
            //
            // For General Ethernet packets we use off set of "15" to jump to extensions (if present) or payload.
            //
        
            if (fExt) {
                pExt = &pBuffer->pBuffer[pBuffer->cStart + 15];            
                iExtSize = ComputeTotalExtensionLength (pExt, BufferTotal (pBuffer) - 15);
            }            

            if (iExtSize >= 0) {
                uiNetType = (unsigned int) ((pBuffer->pBuffer[pBuffer->cStart + 13] << 8) | pBuffer->pBuffer[pBuffer->cStart + 13 + 1]);
                
                if (0x8100 == uiNetType) { // handle 802.1q header
                    p8021q = &pBuffer->pBuffer[pBuffer->cStart + 15 + iExtSize];
                    uiNetType = (unsigned int) ((pBuffer->pBuffer[pBuffer->cStart + 15 + iExtSize + 2] << 8) | 
                        pBuffer->pBuffer[pBuffer->cStart + 15 + iExtSize + 3]);
                    
                    pData = &pBuffer->pBuffer[pBuffer->cStart + 15 + iExtSize + 4];
                    cData = pBuffer->cEnd - (pBuffer->cStart + 15 + iExtSize + 4);
                } else {
                    pData = &pBuffer->pBuffer[pBuffer->cStart + 15 + iExtSize];
                    cData = pBuffer->cEnd - (pBuffer->cStart + 15 + iExtSize);
                }

                if (! ProcessEthernet (pConn, 
                        &pBuffer->pBuffer[pBuffer->cStart + 1],     // dest addr
                        &pBuffer->pBuffer[pBuffer->cStart + 1 + 6], // source addr
                        uiNetType,                                  // protocol type
                        p8021q,                                     // 801.1q header
                        pExt,                                       // extensions
                        iExtSize,                                   // extensions length
                        pData,                                      // payload
                        cData))                                     // payload size
                    fAbortConnection = TRUE;                    
            }
            else
                fAbortConnection = TRUE;
        }
        else if (uCmd == 0x02) { // Compressed Ethernet packet
            //
            // For Compressed Ethernet packets we use off set of "3" to jump to extensions (if present) or payload.
            //
        
            if (fExt) {
                pExt = &pBuffer->pBuffer[pBuffer->cStart + 3];            
                iExtSize = ComputeTotalExtensionLength (pExt, BufferTotal (pBuffer) - 3);
            }

            if (iExtSize >= 0) {
                uiNetType = (unsigned int) ((pBuffer->pBuffer[pBuffer->cStart + 1] << 8) | pBuffer->pBuffer[pBuffer->cStart + 1 + 1]);
                
                if (0x8100 == uiNetType) { // handle 802.1q header
                    p8021q = &pBuffer->pBuffer[pBuffer->cStart + 3 + iExtSize];
                    uiNetType = (unsigned int) ((pBuffer->pBuffer[pBuffer->cStart + 3 + iExtSize + 2] << 8) | 
                        pBuffer->pBuffer[pBuffer->cStart + 3 + iExtSize + 3]);
                    
                    pData = &pBuffer->pBuffer[pBuffer->cStart + 3 + iExtSize + 4];
                    cData = pBuffer->cEnd - (pBuffer->cStart + 3 + iExtSize + 4);
                } else {
                    pData = &pBuffer->pBuffer[pBuffer->cStart + 3 + iExtSize];
                    cData = pBuffer->cEnd - (pBuffer->cStart + 3 + iExtSize);
                }

                if (! ProcessEthernet (pConn, 
                        NULL,                   // no dest addr in compressed eth
                        NULL,                   // no source addr in compressed eth
                        uiNetType,              // protocol type
                        p8021q,                 // 802.1q header
                        pExt,                   // extensions
                        iExtSize,               // extensions length
                        pData,                  // payload
                        cData))                 // payload length
                    fAbortConnection = TRUE;
            }
            else
                fAbortConnection = TRUE;
        }
        else if (uCmd == 0x03) { // Compressed Ethernet packet source only
            //
            // For Source Only Ethernet packets we use off set of "9" to jump to extensions (if present) or payload.
            //
        
            if (fExt) {
                pExt = &pBuffer->pBuffer[pBuffer->cStart + 9];
                iExtSize = ComputeTotalExtensionLength (pExt, BufferTotal (pBuffer) - 9);
            }

            if (iExtSize >= 0) {
                uiNetType = (unsigned int) ((pBuffer->pBuffer[pBuffer->cStart + 7] << 8) | pBuffer->pBuffer[pBuffer->cStart + 7 + 1]);
                
                if (0x8100 == uiNetType) { // handle 802.1q header
                    p8021q = &pBuffer->pBuffer[pBuffer->cStart + 9 + iExtSize];
                    uiNetType = (unsigned int) ((pBuffer->pBuffer[pBuffer->cStart + 9 + iExtSize + 2] << 8) | 
                        pBuffer->pBuffer[pBuffer->cStart + 9 + iExtSize + 3]);
                    
                    pData = &pBuffer->pBuffer[pBuffer->cStart + 9 + iExtSize + 4];
                    cData = pBuffer->cEnd - (pBuffer->cStart + 9 + iExtSize + 4);
                } else {
                    pData = &pBuffer->pBuffer[pBuffer->cStart + 9 + iExtSize];
                    cData = pBuffer->cEnd - (pBuffer->cStart + 9 + iExtSize);
                }

                if (! ProcessEthernet (pConn, 
                        NULL,                                   // no dest addr in source only
                        &pBuffer->pBuffer[pBuffer->cStart + 1], // source addr
                        uiNetType,                              // protocol type
                        p8021q,                                 // 802.1q header
                        pExt,                                   // extensions
                        iExtSize,                               // extensions length
                        pData,                                  // payload
                        cData))                                 // payload length
                    fAbortConnection = TRUE;                    
            }
            else
                fAbortConnection = TRUE;
        }
        else if (uCmd == 0x04) { // Compressed Ethernet packet dest only
            //
            // For Dest Only Ethernet packets we use off set of "9" to jump to extensions (if present) or payload.
            //
        
            if (fExt) {
                pExt = &pBuffer->pBuffer[pBuffer->cStart + 9];
                iExtSize = ComputeTotalExtensionLength (pExt, BufferTotal (pBuffer) - 9);
            }
            
            if (iExtSize >= 0) {
                uiNetType = (unsigned int) ((pBuffer->pBuffer[pBuffer->cStart + 7] << 8) | pBuffer->pBuffer[pBuffer->cStart + 7 + 1]);

                if (0x8100 == uiNetType) { // handle 802.1q header
                    p8021q = &pBuffer->pBuffer[pBuffer->cStart + 9 + iExtSize];
                    uiNetType = (unsigned int) ((pBuffer->pBuffer[pBuffer->cStart + 9 + iExtSize + 2] << 8) | 
                        pBuffer->pBuffer[pBuffer->cStart + 9 + iExtSize + 3]);
                    
                    pData = &pBuffer->pBuffer[pBuffer->cStart + 9 + iExtSize + 4];
                    cData = pBuffer->cEnd - (pBuffer->cStart + 9 + iExtSize + 4);
                } else {
                    pData = &pBuffer->pBuffer[pBuffer->cStart + 9 + iExtSize];
                    cData = pBuffer->cEnd - (pBuffer->cStart + 9 + iExtSize);
                }

                if (! ProcessEthernet (pConn, 
                        &pBuffer->pBuffer[pBuffer->cStart + 1], // dest addr
                        NULL,                                   // no source addr in dest only
                        uiNetType,                              // protocol type
                        p8021q,                                 // 802.1q header
                        pExt,                                   // extensions
                        iExtSize,                               // extensions length
                        pData,                                  // payload
                        cData))                                 // payload length
                    fAbortConnection = TRUE;
            }
            else
                fAbortConnection = TRUE;
        }

        if ((iExtSize > 0) && fExt) { // Now process the extensions...
            while (fExt && (pConn->uiRef == uiConnRef) && ((pConn->state == CONFIG_BNEP) || (pConn->state == CONFIG_SECURITY) || (pConn->state == UP))) {
                unsigned char eType = *pExt & 0x7f;
                fExt = *pExt >> 7;
                unsigned int eLen = pExt[1];

                iExtSize -= (2 + eLen);

                if (iExtSize < 0) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_data_up_ind: buffer overrun in processing extensions\n"));
                    break;
                }

                if (eType != 0) {
                    // Ignore unknown extension headers
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_data_up_ind: unsupported extension type (%d)\n", eType));
                }
                else { 
                    int iReadLen = ProcessControl (pConn, TRUE, pExt + 2, eLen);
                    if (iReadLen == 0) {
                        // Ignore unknown control commands in extension                        
                    }
                    else if (eLen != iReadLen) {
                        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_data_up_ind: length mismatch in processing extension\n"));
                        fAbortConnection = TRUE;                    
                        break;
                    }
                }

                pExt += 2 + eLen;
            }

            if (iExtSize != 0) {    // Something went terribly wrong...
                IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_data_up_ind: data length mismatch in processing extensions\n"));
                fAbortConnection = TRUE;
            }
        }

        if (fAbortConnection && (pConn->uiRef == uiConnRef)) {   // Error - close connection
            IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_data_up_ind: malformed packet - closing connection\n"));
            CloseConnection (pConn, NULL);
        }
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_data_up_ind: connection not found!\n"));
    }

    gpPAN->Unlock ();

    // Free the buffer
    if (pBuffer->pFree)
        pBuffer->pFree (pBuffer);

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_data_up_ind DONE\n"));

    return ERROR_SUCCESS;
}

static int pan_stack_event (void *pUserContext, int iEvent, void *pEventContext) {
    SVSUTIL_ASSERT (pUserContext == gpPAN);

    switch (iEvent) {
    case BTH_STACK_RESET:
        IFDBG(DebugOut (DEBUG_PAN_INIT, L"Stack up\n"));
        btutil_ScheduleEvent (StackReset, NULL);
        break;

    case BTH_STACK_DOWN:
        IFDBG(DebugOut (DEBUG_PAN_INIT, L"Stack down\n"));
        btutil_ScheduleEvent (StackDown, NULL);
        break;

    case BTH_STACK_UP:
        IFDBG(DebugOut (DEBUG_PAN_INIT, L"Stack up\n"));
        btutil_ScheduleEvent (StackUp, NULL);
        break;

    case BTH_STACK_DISCONNECT:
        IFDBG(DebugOut (DEBUG_PAN_INIT, L"Stack disconnect\n"));
        btutil_ScheduleEvent (StackDown, NULL);
        break;

    default:
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] unknown stack event. Disconnecting out of paranoia.\n"));
        btutil_ScheduleEvent (StackDown, NULL);
    }

    return ERROR_SUCCESS;
}

static int pan_call_aborted (void *pCallContext, int iError) { // Aborted call == immediate death...
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_call_aborted 0x%08x, err=%d\n", pCallContext, iError));

    if (! gpPAN) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_call_aborted: context doesn't exist\n"));
        return ERROR_SUCCESS;
    }

    gpPAN->Lock ();
    if (gpPAN->state != UP) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_call_aborted: context not up\n"));
        gpPAN->Unlock ();
        return ERROR_SUCCESS;
    }

    BTHPAN_CONNECTION *pConn = ConnFromCtx (pCallContext);

    if (pConn) {
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_call_aborted : aborting connection %04x%08x cid=0x%04x\n", pConn->ba.NAP, pConn->ba.SAP, pConn->cid));

        BTHPAN_ADAPTER *pAdapter = pConn->nAdapter == -1 ? NULL : &gpPAN->aAdapters[pConn->nAdapter];
        SetConnectionToDown (pConn);
        CloseConnection (pConn, pAdapter);
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_call_aborted: connection not found!\n"));
    }

    gpPAN->Unlock ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_call_aborted DONE\n"));

    return ERROR_SUCCESS;
}

static int pan_disconnect_ind (void *pUserContext, unsigned short cid, int iErr) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_disconnect_ind err=%d\n", iErr));

    if (! gpPAN) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_disconnect_ind: context doesn't exist\n"));
        return ERROR_SUCCESS;
    }

    gpPAN->Lock ();
    if (gpPAN->state != UP) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_disconnect_ind: context not up\n"));
        gpPAN->Unlock ();
        return ERROR_SUCCESS;
    }

    BTHPAN_CONNECTION *pConn = ConnFromCid (cid);

    if (pConn) {
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_disconnect_ind : closing connection %04x%08x cid=0x%04x\n", pConn->ba.NAP, pConn->ba.SAP, pConn->cid));

        BTHPAN_ADAPTER *pAdapter = pConn->nAdapter == -1 ? NULL : &gpPAN->aAdapters[pConn->nAdapter];
        SetConnectionToDown (pConn);
        CloseConnection (pConn, pAdapter);
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_disconnect_ind: connection not found!\n"));
    }

    gpPAN->Unlock ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_disconnect_ind DONE\n"));

    return ERROR_SUCCESS;
}

static int pan_disconnect_out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_disconnect_out 0x%08x, res=%d\n", pCallContext, result));

    if (! gpPAN) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_disconnect_out: context doesn't exist\n"));
        return ERROR_SUCCESS;
    }

    gpPAN->Lock ();
    if (gpPAN->state != UP) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_disconnect_out: context not up\n"));
        gpPAN->Unlock ();
        return ERROR_SUCCESS;
    }

    BTHPAN_CONNECTION *pConn = ConnFromCtx (pCallContext);

    if (pConn) {
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_disconnect_out : closing connection %04x%08x cid=0x%04x\n", pConn->ba.NAP, pConn->ba.SAP, pConn->cid));

        SVSUTIL_ASSERT (pConn->state == CLOSING);

        BTHPAN_ADAPTER *pAdapter = pConn->nAdapter == -1 ? NULL : &gpPAN->aAdapters[pConn->nAdapter];
        SetConnectionToDown (pConn);
        CloseConnection (pConn, pAdapter);
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_disconnect_out: connection not found!\n"));
    }

    gpPAN->Unlock ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_disconnect_out DONE\n"));

    return ERROR_SUCCESS;
}

static int pan_data_down_out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_data_down_out Data down call 0x%08x result %d\n", pCallContext, result));
    return ERROR_SUCCESS;
}

static int pan_config_response_out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_config_response_out result = %d\n", result));
    return ERROR_SUCCESS;
}

static int pan_config_ind (void *pUserContext, unsigned char id, unsigned short cid, unsigned short usOutMTU, unsigned short usInFlushTO, struct btFLOWSPEC *pInFlow, int cOptNum, struct btCONFIGEXTENSION **ppExtendedOptions) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_config_ind cid=0x%04x mtu = %d\n", cid, usOutMTU));

    if (! gpPAN) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_config_ind: context doesn't exist\n"));
        return ERROR_SUCCESS;
    }

    gpPAN->Lock ();
    if (gpPAN->state != UP) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_config_ind: context not up\n"));
        gpPAN->Unlock ();
        return ERROR_SUCCESS;
    }

    BTHPAN_CONNECTION *pConn = ConnFromCid (cid);

    if (pConn && (pConn->state == CONFIG_LINK)) {
        unsigned int uiConnRef = pConn->uiRef;

        if ((usOutMTU < BTHPAN_MIN_FRAME) || (usInFlushTO != 0xffff) || (pInFlow && (pInFlow->service_type != 0x01))) {
            IFDBG(DebugOut (DEBUG_WARN, L"[PAN] pan_config_ind :: not supported parameters!\n"));

            struct btFLOWSPEC flowspec;

            if (pInFlow && (pInFlow->service_type != 0x01)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[PAN] pan_config_ind :: not supported flowspec!\n"));
                flowspec = *pInFlow;
                pInFlow = &flowspec;
                pInFlow->service_type = 0x01;
            } else
                pInFlow = NULL;

            if (usInFlushTO != 0xffff) {
                IFDBG(DebugOut (DEBUG_WARN, L"[PAN] pan_config_ind :: not supported flush timeout!\n"));
                usInFlushTO = 0xffff;
            } else
                usInFlushTO = 0;

            if (usOutMTU && (usOutMTU < BTHPAN_MIN_FRAME)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[PAN] pan_config_ind :: not supported MTU (%d)!\n", usOutMTU));
                usOutMTU = BTHPAN_MIN_FRAME;
            } else
                usOutMTU = 0;

            gpPAN->AddRef ();
            gpPAN->Unlock ();

            int iRes = gpPAN->l2cap_if.l2ca_ConfigResponse_In (gpPAN->hL2CAP, NULL, id, cid, 1, usOutMTU, usInFlushTO, pInFlow, 0, NULL);

            gpPAN->Lock ();
            gpPAN->DelRef ();

            if ((iRes != ERROR_SUCCESS) && (pConn->uiRef == uiConnRef))
                CloseConnection (pConn, NULL);
        
            gpPAN->Unlock ();

            IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_config_ind DONE\n"));

            return ERROR_SUCCESS;
        }

        gpPAN->AddRef ();
        gpPAN->Unlock ();
        int iRes = gpPAN->l2cap_if.l2ca_ConfigResponse_In (gpPAN->hL2CAP, NULL, id, cid, 0, 0, 0xffff, NULL, 0, NULL);
        gpPAN->Lock ();
        gpPAN->DelRef ();

        if (pConn->uiRef == uiConnRef) {
            if (iRes == ERROR_SUCCESS)
                AddConfigFlag (pConn, CONFIG_IN_DONE);
            else
                CloseConnection (pConn, NULL);
        }
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_config_ind: connection not found!\n"));
    }

    gpPAN->Unlock ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_config_ind DONE\n"));

    return ERROR_SUCCESS;
}

static int pan_config_req_out (void *pCallContext, unsigned short usResult, unsigned short usInMTU, unsigned short usOutFlushTO, struct btFLOWSPEC *pOutFlow, int cOptNum, struct btCONFIGEXTENSION **ppExtendedOptions) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_config_req_out 0x%08x, res=%d mtu = %d\n", pCallContext, usResult, usInMTU));

    if (! gpPAN) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_config_req_out: context doesn't exist\n"));
        return ERROR_SUCCESS;
    }

    gpPAN->Lock ();
    if (gpPAN->state != UP) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_config_req_out: context not up\n"));
        gpPAN->Unlock ();
        return ERROR_SUCCESS;
    }

    BTHPAN_CONNECTION *pConn = ConnFromCtx (pCallContext);

    if (pConn && (pConn->state == CONFIG_LINK)) {
        if (usResult != 0)
            CloseConnection (pConn, NULL);
        else
            AddConfigFlag (pConn, CONFIG_OUT_DONE);
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_config_req_out: connection not found!\n"));
    }

    gpPAN->Unlock ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_config_req_out DONE\n"));

    return ERROR_SUCCESS;
}

static int pan_connect_response_out (void *pCallContext, unsigned short result) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_connect_response_out result = %d\n", result));
    return ERROR_SUCCESS;
}

static int pan_connect_ind (void *pUserContext, BD_ADDR *pba, unsigned short cid, unsigned char id, unsigned short psm) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_connect_ind bd=%04x%08x cid=0x%04x\n", pba->NAP, pba->SAP, cid));

    if (! gpPAN) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_connect_ind: context doesn't exist\n"));
        return ERROR_SUCCESS;
    }

    gpPAN->Lock ();
    if (gpPAN->state != UP) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_connect_ind: context not up\n"));
        gpPAN->Unlock ();
        return ERROR_SUCCESS;
    }

    SVSUTIL_ASSERT (psm == BTHPAN_PSM);

    // First, check if ANYBODY can accept connections.
    BTHPAN_CONNECTION *pConn = NULL;
    BTHPAN_ADAPTER    *pAdapter = NULL;
    unsigned int      uiConnRef = 0;
    void              *pContext = NULL;

    int i;
    for (i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aAdapters) ; ++i) {
        if ((gpPAN->aAdapters[i].state == UP) && gpPAN->aAdapters[i].fAcceptIncoming && (CountConn (&gpPAN->aAdapters[i]) < (int)gpPAN->aAdapters[i].cMaxConn)) {
            for (int j = 0 ; j < SVSUTIL_ARRLEN(gpPAN->aConn) ; ++j) {
                if (gpPAN->aConn[j].state == DOWN) {
                    pAdapter = &gpPAN->aAdapters[i];
                    pConn = &gpPAN->aConn[j];
                    uiConnRef   = pConn->uiRef;
                    pContext    = ConnToCtx (pConn);
                    SetConnectionToConnecting (pConn, TRUE, cid, pba, -1);
                    pConn->state = CONFIG_LINK;
                    break;
                }
            }

            break;
        }
    }

    gpPAN->AddRef ();
    gpPAN->Unlock ();

    int iRes = ERROR_SUCCESS;

    if (! pConn) {
        int res = (i == SVSUTIL_ARRLEN(gpPAN->aAdapters)) ? 2 : 4;

        IFDBG(DebugOut (DEBUG_WARN, L"[PAN] pan_connect_ind - rejected, %s\n", (res == 2) ? L"no handler" : L"no resources"));

        gpPAN->l2cap_if.l2ca_ConnectResponse_In (gpPAN->hL2CAP, NULL, pba, id, cid, res, 0);
    } else {
        iRes = gpPAN->l2cap_if.l2ca_ConnectResponse_In (gpPAN->hL2CAP, pContext, pba, id, cid, 0, 0);
        if (iRes == ERROR_SUCCESS)
            iRes = gpPAN->l2cap_if.l2ca_ConfigReq_In (gpPAN->hL2CAP, pContext, cid, BTHPAN_MIN_FRAME, 0xffff, NULL, 0, NULL);
    }
    gpPAN->Lock ();
    gpPAN->DelRef ();

    if (pConn && (pConn->uiRef == uiConnRef) && (iRes != ERROR_SUCCESS)) {
        IFDBG(DebugOut (DEBUG_WARN, L"[PAN] pan_connect_ind l2ca_{ConnectResponse or ConfigReq}_In failed, %d\n", iRes));
        CloseConnection (pConn, NULL);
    }

    gpPAN->Unlock ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_connect_ind DONE\n"));

    return ERROR_SUCCESS;
}

static int pan_connect_req_out (void *pCallContext, unsigned short cid, unsigned short result, unsigned short status) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_connect_req_out 0x%08x, 0x%04x %d %d\n", pCallContext, cid, result, status));

    if (! gpPAN) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_connect_req_out: context doesn't exist\n"));
        return ERROR_SUCCESS;
    }

    gpPAN->Lock ();
    if (gpPAN->state != UP) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_connect_req_out: context not up\n"));
        gpPAN->Unlock ();
        return ERROR_SUCCESS;
    }

    BTHPAN_CONNECTION *pConn = ConnFromCtx (pCallContext);

    if (pConn && (pConn->state == CONNECTING)) {
        if (result != 0)
            CloseConnection (pConn, NULL);
        else if (pConn->state == CONNECTING) {
            unsigned int uiConnRef = pConn->uiRef;

            pConn->state = CONFIG_LINK;
            pConn->cid   = cid;

            gpPAN->AddRef ();
            gpPAN->Unlock ();

            int iRes = gpPAN->l2cap_if.l2ca_ConfigReq_In (gpPAN->hL2CAP, pCallContext, cid, BTHPAN_MIN_FRAME, 0xffff, NULL, 0, NULL);

            gpPAN->Lock ();
            gpPAN->DelRef ();

            if ((iRes != ERROR_SUCCESS) && (pConn->uiRef == uiConnRef)) {
                CloseConnection (pConn, NULL);
                IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_connect_req_out config attempt failed\n"));
            }
        }
    } else {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_connect_req_out: connection not found!\n"));
    }

    gpPAN->Unlock ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"pan_connect_req_out DONE\n"));

    return ERROR_SUCCESS;
}


//
//  Miniport utilities. In all cases in this subsection, adapter is referenced,
//  but gpPAN is not locked.
//
static void MpFilterMulticastStartOnAllConnections (BTHPAN_ADAPTER *pAdapter) {
//#error Filter not used
    SVSUTIL_ASSERT (gpPAN->IsLocked ());

    int nAdapter = pAdapter - gpPAN->aAdapters;
    for (int i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aConn) ; ++i) {
        if ((gpPAN->state == UP) && (gpPAN->aConn[i].state == UP))
            SetConnectionFilter (&gpPAN->aConn[i], MCAST);
    }
}

static NDIS_STATUS MpPacketSend (BTHPAN_ADAPTER *pAdapter, NDIS_PACKET *pPacket, int fIndicateUp) {
    ETH_HDR hdr;
    unsigned int uiPacketLength = 0;
    NDIS_BUFFER  *pNdisBuffer   = NULL;

    NdisQueryPacket (pPacket, NULL, NULL, &pNdisBuffer, &uiPacketLength);
    if (! pNdisBuffer) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] MpPacketSend: bad packet - no buffer\n"));
        
        return NDIS_STATUS_INVALID_ADDRESS;
    }

    if (uiPacketLength < sizeof(ETH_HDR)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] MpPacketSend: bad packet - too short\n"));
        
        return NDIS_STATUS_BUFFER_TOO_SHORT;
    }

    if (sizeof(hdr) != CopyNdisToFlat (pNdisBuffer, (unsigned char *)&hdr, sizeof(hdr), 0)) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] MpPacketSend: bad packet - no ethernet header\n"));

        return NDIS_STATUS_INVALID_DATA;
    }

    // Substitute source address, if necessary
    if (pAdapter->fAddressSet && (memcmp (&hdr.srcAddr, pAdapter->ethAddr, 6) == 0))
        memcpy (hdr.srcAddr, gpPAN->eth, 6);

    int fMCast = ETH_IS_MULTICAST(hdr.destAddr);
    int fBroad = ETH_IS_BROADCAST(hdr.destAddr);
    unsigned int uiNetType = (((unsigned char)hdr.ethType) << 8) | ((unsigned char)(hdr.ethType >> 8));

// Support for 802.3 length is optional
//    if ((uiNetType <= 1500) && (uiNetType >= 46)) { // PAN only supports packets with net type
//        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] MpPacketSend: net type not supported\n"));
//
//        return NDIS_STATUS_NOT_SUPPORTED;
//    }

    unsigned int uiDataLength = uiPacketLength - sizeof(ETH_HDR);

    gpPAN->Lock ();
    
    int nAdapter = pAdapter - gpPAN->aAdapters;
    int fPANU    = pAdapter->type == PANU;
    int fSendSrc = memcmp (gpPAN->eth, hdr.srcAddr, 6);

    for (int i = 0 ; (pAdapter->state == UP) && (i < SVSUTIL_ARRLEN (gpPAN->aConn)) ; ++i) {
        if ((gpPAN->aConn[i].nAdapter == nAdapter) && (gpPAN->aConn[i].state == UP)) {
            BTHPAN_CONNECTION *pConn = &gpPAN->aConn[i];

            // Broadcast addresses are sent everywhere
            // Multicast addresses are sent everywhere except where not filtered
            // If this adapter is PANU, it sends everything over this one and only one connection
            int fSendDst = (memcmp (pConn->eth, hdr.destAddr, 6) != 0);
            if ((! (fBroad || fMCast || fPANU)) && fSendDst)
                continue;

            if (uiNetType > 0x05dc) {
                if (FilterByType (&pConn->filter, uiNetType))
                    continue;
            }

            if (fMCast && FilterByMCast (&pConn->filter, hdr.destAddr))
                continue;

            // Send the thing...
            //BD_BUFFER *pBuff = BufferAlloc (gpPAN->cDataHeaders + gpPAN->cDataTrailers + (fSendSrc ? 6 : 0) + (fSendDst ? 6 : 0) + 1 + 2 + uiDataLength);
            uint uitemp1, uitemp2, uitemp3 = 0, uitemp4;
            int itemp;
            bool bfailed = FALSE;
            uitemp1 = (fSendSrc ? 6 : 0) + (fSendDst ? 6 : 0) + 1 + 2;
            if(!SUCCEEDED(IntToUInt(gpPAN->cDataHeaders, &uitemp2))||
               !SUCCEEDED(IntToUInt(gpPAN->cDataTrailers, &uitemp3)))
                bfailed = TRUE;
            
            if(!SUCCEEDED(CeUIntAdd4(uitemp1, uitemp2, uitemp3, uiDataLength, &uitemp4)))
                bfailed = TRUE;
            
            if(!SUCCEEDED(UIntToInt(uitemp4, &itemp))) 
                bfailed = TRUE;
            
            BD_BUFFER *pBuff = NULL;

            if(!bfailed)
                pBuff = BufferAlloc (itemp);
                
            if (pBuff) {
                pBuff->cStart = gpPAN->cDataHeaders;
                pBuff->cEnd   = pBuff->cSize - gpPAN->cDataTrailers;

                unsigned char *p = pBuff->pBuffer + pBuff->cStart;
                if (fSendSrc && fSendDst) {
                    *p++ = 0x00;    // general ether
                    memcpy (p, &hdr, sizeof(hdr));
                    p += sizeof(hdr);
                } else if (fSendSrc) {
                    *p++ = 0x03;    // source only
                    memcpy (p, hdr.srcAddr, 6);
                    p += 6;
                    *p++ = uiNetType >> 8;
                    *p++ = uiNetType & 0xff;
                } else if (fSendDst) {
                    *p++ = 0x04;    // dest only
                    memcpy (p, hdr.destAddr, 6);
                    p += 6;
                    *p++ = uiNetType >> 8;
                    *p++ = uiNetType & 0xff;
                } else {
                    *p++ = 0x02;    // compressed
                    *p++ = uiNetType >> 8;
                    *p++ = uiNetType & 0xff;
                }

                if (uiDataLength != CopyNdisToFlat (pNdisBuffer, p, uiDataLength, sizeof(hdr))) {
                    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] MpPacketSend: bad packet - inconsistent buffer chain\n"));
                    gpPAN->Unlock ();
                    return NDIS_STATUS_INVALID_DATA;
                }

                if ((uiNetType == 0x0806) || (uiNetType == 0x8035))
                    ProcessArpData (pAdapter, p, uiDataLength);

                p += uiDataLength;

                SVSUTIL_ASSERT (p == pBuff->pBuffer + pBuff->cEnd);

                gpPAN->AddRef ();
                gpPAN->Unlock ();

                IFDBG(DumpBnepPacket (L"Sending data packet.", pBuff));

                if (ERROR_SUCCESS != gpPAN->l2cap_if.l2ca_DataDown_In (gpPAN->hL2CAP, ConnToCtx (pConn), pConn->cid, pBuff)) {
                    if (pBuff->pFree)
                        pBuff->pFree (pBuff);
                }

                gpPAN->Lock ();
                gpPAN->DelRef ();
            }
        }
    }

    gpPAN->Unlock ();

    return NDIS_STATUS_SUCCESS;
}

static void MpGetAssociations (BTHPAN_ADAPTER *pAdapter) {
    SVSUTIL_ASSERT ((pAdapter->type == PANU) || (pAdapter->type == GN));

    pAdapter->cAssoc = 0;

    WCHAR szBaseKey[_MAX_PATH + 50];
    StringCbPrintfW(szBaseKey, sizeof(szBaseKey), L"COMM\\%s\\Associations", pAdapter->szIfName);

    HKEY hk;
    if (ERROR_SUCCESS != RegOpenKeyEx (HKEY_LOCAL_MACHINE, szBaseKey, 0, KEY_ALL_ACCESS, &hk))
        return;

    DWORD dwIndex = 0;
    for ( ; ; ) {
        WCHAR szName[_MAX_PATH];
        DWORD cchName = _MAX_PATH;
        if (ERROR_SUCCESS != RegEnumKeyEx (hk, dwIndex, szName, &cchName, NULL, NULL, NULL, NULL))
            break;

        ++dwIndex;

        if (pAdapter->cAssoc > SVSUTIL_ARRLEN (pAdapter->aAssoc))
            break;

        HKEY hkEnum;
        if (ERROR_SUCCESS != RegOpenKeyEx (hk, szName, 0, KEY_ALL_ACCESS, &hkEnum))
            continue;

        WCHAR szBdAddr[_MAX_PATH];
        WCHAR szSSID[32];
        WCHAR szServiceId[64];
        GUID  service_id;
        DWORD dwPriority = 0;
        BD_ADDR ba;

        DWORD dwType, dwSize;

        dwSize = sizeof(szBdAddr);
        if ((RegQueryValueEx (hkEnum, L"Address", NULL, &dwType, (LPBYTE)szBdAddr, &dwSize) != ERROR_SUCCESS) || 
            (dwType != REG_SZ) || (dwSize >= sizeof(szBdAddr)) || (! GetBD (szBdAddr, &ba))) {
            RegCloseKey (hkEnum);
            continue;
        }

        dwSize = sizeof(szSSID);
        if ((RegQueryValueEx (hkEnum, L"SSID", NULL, &dwType, (LPBYTE)szSSID, &dwSize) != ERROR_SUCCESS) || 
            (dwType != REG_SZ) || (dwSize >= sizeof(szSSID))) {
            RegCloseKey (hkEnum);
            continue;
        }

        dwSize = sizeof(dwPriority);
        if ((RegQueryValueEx (hkEnum, L"Priority", NULL, &dwType, (LPBYTE)&dwPriority, &dwSize) != ERROR_SUCCESS) ||
            (dwType != REG_DWORD)) {
            RegCloseKey (hkEnum);
            continue;
        }

        dwSize = sizeof(szServiceId);
        if ((RegQueryValueEx (hkEnum, L"ServiceId", NULL, &dwType, (LPBYTE)szServiceId, &dwSize) != ERROR_SUCCESS) ||
            (dwType != REG_SZ) || (dwSize >= sizeof(szServiceId)) || (! MpStrToGUID(szServiceId, &service_id))) {
            RegCloseKey (hkEnum);
            continue;
        }
        RegCloseKey (hkEnum);
        int j;
        for (j = 0 ; j < (int)pAdapter->cAssoc ; ++j) {
            if (pAdapter->aAssoc[j].pri > dwPriority)
                break;
        }
        int cNumMove = pAdapter->cAssoc - j;
        if (cNumMove)
            memmove (&pAdapter->aAssoc[j+1], &pAdapter->aAssoc[j], cNumMove * sizeof(pAdapter->aAssoc[0]));

        pAdapter->aAssoc[j].ba = ba;
        wcscpy (pAdapter->aAssoc[j].szSSID, szSSID);
        pAdapter->aAssoc[j].pri = dwPriority;
        pAdapter->aAssoc[j].service_id = service_id;

        pAdapter->cAssoc += 1;

        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"Found association: %s %04x%08x pri=%d\n", szSSID, ba.NAP, ba.SAP, dwPriority));
    }

    RegCloseKey (hk);
}


static void MpFreePacket (BTHPAN_ADAPTER *pAdapter, NDIS_PACKET *pPacket) {
    void *pBuff = *(void **)&pPacket->MiniportReserved[0];
    if (pBuff)
        LocalFree (pBuff);

    NDIS_BUFFER *pNdisBuf = NULL;
    NdisQueryPacket(pPacket, NULL, NULL, &pNdisBuf, NULL);

    while (pNdisBuf) {
        NDIS_BUFFER *pNext = NULL;
        NdisGetNextBuffer (pNdisBuf, &pNext);
        NdisFreeBuffer (pNdisBuf);
        pNdisBuf = pNext;
    }

    NdisFreePacket (pPacket);
}

static VOID MpSendComplete (BTHPAN_ADAPTER *pAdapter, PNDIS_PACKET pNdisPacket, NDIS_STATUS ndisStatus) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpSendComplete\n"));

    ASSERT(pNdisPacket != NULL);

    NDIS_SET_PACKET_STATUS (pNdisPacket, ndisStatus);

    NdisMSendComplete (pAdapter->hAdapter, pNdisPacket, ndisStatus);

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpSendComplete\n"));
}

static VOID MpMediaStatusIndicate (BTHPAN_ADAPTER *pAdapter) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpMediaStatusIndicate\n"));

    NDIS_STATUS  ndisStatus = NDIS_STATUS_SUCCESS;
    
    if ((pAdapter->type == PANU) || (pAdapter->type == GN)) {
        gpPAN->Lock ();
        if (GetAdapterConnection (pAdapter))
            ndisStatus = NDIS_STATUS_MEDIA_CONNECT;
        else
            ndisStatus = NDIS_STATUS_MEDIA_DISCONNECT;
        gpPAN->Unlock ();
    } else {
        ndisStatus = NDIS_STATUS_MEDIA_CONNECT;
    }

    //
    // Indicate the status up to NDIS.
    //
    if (pAdapter->LastIndicatedStatus != ndisStatus) {
        pAdapter->LastIndicatedStatus = ndisStatus;
        NdisMIndicateStatus (pAdapter->hAdapter, ndisStatus, NULL, 0);

        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"Adapter: 0x%08x Media Connect Status: %s (0x%08x)\n",
           (UINT) pAdapter, ( (ndisStatus == NDIS_STATUS_MEDIA_CONNECT) ? L"Connected" : L"Disconnected" ),
           ndisStatus));
    
    }

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpMediaStatusIndicate\n"));
}

static void MpAttemptConnection (BTHPAN_ADAPTER *pAdapter, BD_ADDR *pba, GUID *pService) {   // Adapter is already ref'd
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpAttemptConnection : trying to connect to %04x%08x\n", pba->NAP, pba->SAP));

    gpPAN->Lock ();

    if (GetAdapterConnection (pAdapter)) {
        gpPAN->Unlock ();
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpAttemptConnection : already connected\n"));
        return;
    }

    void *pContext = NULL;
    unsigned int uiConnRef = 0;
    BTHPAN_CONNECTION *pConn = NULL;
    int nAdapter = pAdapter - gpPAN->aAdapters;

    int i;
    for (i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aConn) ; ++i) {
        if (gpPAN->aConn[i].state == DOWN) {
            pConn = &gpPAN->aConn[i];
            uiConnRef = pConn->uiRef;
            pContext = ConnToCtx (pConn);

            SetConnectionToConnecting (pConn, FALSE, 0, pba, nAdapter);

            pConn->dest_service_id = *pService;
            break;
        }
    }

    if (pContext) {
        gpPAN->AddRef ();
        gpPAN->Unlock ();

        int iRes = gpPAN->l2cap_if.l2ca_ConnectReq_In (gpPAN->hL2CAP, pContext, BTHPAN_PSM, pba);

        gpPAN->Lock ();
        gpPAN->DelRef ();

        if ((iRes != ERROR_SUCCESS) && (pConn->uiRef == uiConnRef)) {
            CloseConnection (pConn, NULL);
            IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] pan_connect_req_out config attempt failed\n"));
        }
    }

    gpPAN->Unlock ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpAttemptConnection\n"));
}


/*++

Routine Description:

    The walk function that is passed to SdpWalkStream for the
    ServiceClassId attribute.

Arguments:

    IN  pContext  - The context that holds parsing status and also
                    the values of the attributes
    IN  dataType  - type of the data item for which this function is called
    IN  dataSize  - size of the data item for which this function is called
    IN  data      - The data item for which this function is called

Return Value:

    STATUS_SUCCESS -  Success
    else           -  Failure

--*/
static NTSTATUS SdpWalkServiceClassIdList (BTHPAN_SDP_WALK_CONTEXT *pContext, UCHAR dataType, ULONG dataSize, PUCHAR data) {
    NTSTATUS  ntStatus = STATUS_SUCCESS;

    if (data == NULL)
        goto error;

    switch (pContext->attribState) {
        case SDP_WALK_CLASS_ID_LIST_STATE_ROOT_SEQ:
        {
            if (dataType != SDP_TYPE_SEQUENCE) {
                IFDBG(DebugOut(DEBUG_WARN, L"[PAN] SdpWalkServiceClassIdList: dataType:%d != SDP_TYPE_SEQUENCE\n", dataType));
                pContext->successfullParse = FALSE;
                ntStatus = STATUS_UNSUCCESSFUL;
                break;
            }

            pContext->attribState = SDP_WALK_CLASS_ID_LIST_STATE_UUID;

            break;
        }

        case SDP_WALK_CLASS_ID_LIST_STATE_UUID:
        {
            if (dataType != SDP_TYPE_UUID) {
                pContext->successfullParse = FALSE;
                ntStatus = STATUS_UNSUCCESSFUL;
                break;
            }

            SdpRetrieveUuidFromStream (data, dataSize, pContext->pservice_id, 0);

            if (NAPServiceClass_UUID == *pContext->pservice_id) {
                IFDBG(DebugOut(DEBUG_PAN_TRACE, L"[PAN] SdpWalkServiceClassIdList: found NAP\n"));
            } else if (GNServiceClass_UUID == *pContext->pservice_id) {
                IFDBG(DebugOut(DEBUG_PAN_TRACE, L"[PAN] SdpWalkServiceClassIdList: found GN\n"));
            } else if (PANUServiceClass_UUID == *pContext->pservice_id) {
                IFDBG(DebugOut(DEBUG_PAN_TRACE, L"[PAN] SdpWalkServiceClassIdList: found PANU\n"));
            } else {
                IFDBG(DebugOut(DEBUG_PAN_TRACE, L"[PAN] SdpWalkServiceClassIdList: found unknown class UUID\n"));
            }

            break;
        }

        default:
            pContext->successfullParse = FALSE;
            ntStatus = STATUS_UNSUCCESSFUL;
            IFDBG(DebugOut(DEBUG_WARN, L"[PAN] SdpWalkServiceClassIdList: INVALID class ID LIST\n"));
            break;
    }

error:

    return ntStatus;
}

/*++

Routine Description:

    The walk function that is passed to SdpWalkStream for the
    Protocol Descriptol List attribute.

Arguments:

    IN  pContext  - The context that holds parsing status and also
                    the values of the attributes
    IN  dataType  - type of the data item for which this function is called
    IN  dataSize  - size of the data item for which this function is called
    IN  data      - The data item for which this function is called

Return Value:

    STATUS_SUCCESS -  Success
    else           -  Failure

--*/
static NTSTATUS SdpWalkProtoDescList (BTHPAN_SDP_WALK_CONTEXT *pContext, UCHAR dataType, ULONG dataSize, PUCHAR data) {
    NTSTATUS  ntStatus = STATUS_SUCCESS;

    if (data == NULL) {
        switch(pContext->attribState) {
            case SDP_WALK_PROTO_DESC_LIST_STATE_ROOT_SEQ:
            case SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_DESC_SEQ:
                break;

            case SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_UUID:
            case SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_INFO:
                pContext->attribState = SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_DESC_SEQ;
                break;

            default:
                break;
        }

        goto error;
    }

    switch (pContext->attribState) {
        case SDP_WALK_PROTO_DESC_LIST_STATE_ROOT_SEQ:
        {
            // For the time being data element alternative
            // is not supported
            if (dataType != SDP_TYPE_SEQUENCE) {
                IFDBG(DebugOut(DEBUG_WARN, L"[PAN] SdpWalkProtoDescList: dataType:%d != SDP_TYPE_SEQUENCE\n", dataType));
                pContext->successfullParse = FALSE;
                ntStatus = STATUS_UNSUCCESSFUL;
                break;
            }

            pContext->attribState = SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_DESC_SEQ;

            break;
        }

        case SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_DESC_SEQ:
        {
            if (dataType != SDP_TYPE_SEQUENCE) {
                IFDBG(DebugOut(DEBUG_WARN, L"[PAN] SdpWalkProtoDescList: dataType:%d != SDP_TYPE_SEQUENCE\n", dataType));
                pContext->successfullParse = FALSE;
                ntStatus = STATUS_UNSUCCESSFUL;
                break;
            }

            pContext->currProtocol = 0;
            pContext->attribState = SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_UUID;

            break;
        }

        case SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_UUID:
        {
            GUID  uuid;

            SdpRetrieveUuidFromStream(data, dataSize, &uuid, 0);

            if (IsEqualUuid(&L2CAP_PROTOCOL_UUID, &uuid)) {
                pContext->currProtocol = L2CAP_PROTOCOL_UUID16;
                pContext->psm = 0;
            } else if (IsEqualUuid(&BNEP_PROTOCOL_UUID, &uuid)) {
                pContext->currProtocol = BNEP_PROTOCOL_UUID16;
            }

            pContext->attribState = SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_INFO;

            break;
        }

        case SDP_WALK_PROTO_DESC_LIST_STATE_PROTO_INFO:
        {
            if (pContext->currProtocol == L2CAP_PROTOCOL_UUID16) {
                if ((dataType != SDP_TYPE_UINT) || (dataSize != 2)) {
                    IFDBG(DebugOut(DEBUG_WARN, L"[PAN] SdpWalkProtoDescList: Only UINT16 is valid as protocol info for L2CAP\n"));
                    break;
                }
                pContext->psm = (data[0] << 8) | data[1];
            }
            break;
        }

        default:
            break;
    }

error:
    return ntStatus;
}

static VOID SdpInitWalkContext (BTHPAN_SDP_WALK_CONTEXT *pSdpWalkContext, GUID *pservice_id) {
    memset (pSdpWalkContext, 0, sizeof(*pSdpWalkContext));
    pSdpWalkContext->successfullParse = TRUE;
    pSdpWalkContext->pservice_id = pservice_id;
}


static int MpParseSdpRecord (unsigned char *stream, unsigned int streamLength, GUID *pservice_id) {
    IFDBG(DebugOut(DEBUG_PAN_TRACE, L"++MpParseSdp\n"));

    PUCHAR                   attribVal;
    ULONG                    attribValLen;

    NTSTATUS ntStatus = SdpFindAttributeInStream(stream, streamLength, SDP_ATTRIB_CLASS_ID_LIST, &attribVal, &attribValLen);

    if (ntStatus || (! attribVal) || (attribValLen == 0)) {
        IFDBG(DebugOut(DEBUG_PAN_TRACE, L"--MpParseSdp : class id list not found!\n"));
        return FALSE;
    }

    BTHPAN_SDP_WALK_CONTEXT  context;
    SdpInitWalkContext(&context, pservice_id);

    context.attribState = SDP_WALK_CLASS_ID_LIST_STATE_ROOT_SEQ;

    SdpWalkStream (attribVal, attribValLen, (PSDP_STREAM_WALK_FUNC)SdpWalkServiceClassIdList, &context);

    if (! context.successfullParse) {
        IFDBG(DebugOut(DEBUG_PAN_TRACE, L"--MpParseSdp : walk 1 failed!\n"));
        return FALSE;
    }

    ntStatus = SdpFindAttributeInStream(stream,
                                      streamLength,
                                      SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST,
                                      &attribVal,
                                      &attribValLen);

    if (ntStatus || (! attribVal) || (attribValLen == 0)) {
        IFDBG(DebugOut(DEBUG_PAN_TRACE, L"--MpParseSdp : protocol descriptor list not found!\n"));
        return FALSE;
    }

    context.attribState = SDP_WALK_PROTO_DESC_LIST_STATE_ROOT_SEQ;

    SdpWalkStream(attribVal,
                  attribValLen,
                  (PSDP_STREAM_WALK_FUNC)SdpWalkProtoDescList,
                  &context);

    if (! context.successfullParse) {
        IFDBG(DebugOut(DEBUG_PAN_TRACE, L"--MpParseSdp : walk 2 failed!\n"));
        return FALSE;
    }

    IFDBG(DebugOut(DEBUG_PAN_TRACE, L"--MpParseSdp\n"));

    return TRUE;
}

static int MpParseSdp(unsigned char *stream, unsigned int streamLength, GUID *pservice_id) {
    NTSTATUS status = ValidateStream(stream, streamLength, NULL, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        return FALSE;
    }

    UCHAR type;
    UCHAR sizeIndex;
    LONG  elementSize;
    ULONG storageSize;

    // Read information about the top sequence node - sequence of sequences.
    SdpRetrieveHeader(stream, type, sizeIndex);
    SdpRetrieveVariableSize(stream+1, sizeIndex, (ULONG*) &elementSize, &storageSize);

    if (SDP_TYPE_SEQUENCE != type)
        return FALSE;

    stream += (storageSize+1); // skip past data containing stream length

    // scan through each sequence.
    while (elementSize > 0) {
        ULONG subElementSize;
        ULONG subStorageSize;
        
        SdpRetrieveHeader(stream, type, sizeIndex);
        SdpRetrieveVariableSize(stream+1, sizeIndex, (ULONG*) &subElementSize, &subStorageSize);

        if (SDP_TYPE_SEQUENCE != type)
            return FALSE;

        ULONG subTotalSize = subElementSize+subStorageSize+1; // total len of sequence and its header.

        elementSize -= subTotalSize;
        if (elementSize < 0)
            return FALSE;

        if (MpParseSdpRecord(stream,subTotalSize,pservice_id))
            return TRUE;

        // Could not find a match.
        stream += subTotalSize;
    }

    return FALSE;    
}


#define SDP_CREATE_PROTO(nodeProtoDescList, UUID, nodeProto, nodeProtoUUID)  \
        {                                                                    \
            nodeProto = SdpCreateNodeSequence();                             \
            if (nodeProto == NULL)                                           \
            {                                                                \
                ntStatus = STATUS_INSUFFICIENT_RESOURCES;                    \
                goto error;                                                  \
            }                                                                \
                                                                             \
            SdpAppendNodeToContainerNode(                                    \
                nodeProtoDescList,                                           \
                nodeProto);                                                  \
                                                                             \
            nodeProtoUUID = SdpCreateNodeUUID16(UUID);                       \
            if (nodeProtoUUID == NULL)                                       \
            {                                                                \
                ntStatus = STATUS_INSUFFICIENT_RESOURCES;                    \
                goto error;                                                  \
            }                                                                \
                                                                             \
            SdpAppendNodeToContainerNode(                                    \
                nodeProto,                                                   \
                nodeProtoUUID);                                              \
        }

#define SDP_CREATE_UINT16(nodeParent, ui16Val, nodeChild)                    \
        {                                                                    \
            nodeChild = SdpCreateNodeUInt16(ui16Val);                        \
            if (nodeChild == NULL)                                           \
            {                                                                \
                ntStatus = STATUS_INSUFFICIENT_RESOURCES;                    \
                goto error;                                                  \
            }                                                                \
                                                                             \
            SdpAppendNodeToContainerNode(                                    \
                nodeParent,                                                  \
                nodeChild);                                                  \
        }

static void MpCreateSdpRecord (
    BTHNS_SETBLOB *pBlob,
    int cBlobData,
    WCHAR* szFriendlyName,
    WCHAR* szDescription,
    GUID service_id,
    DWORD pan_role,
    ULONG* pulSdpRecordId
    ) {
    // Set the defaults...

    BOOL fFreeBlob = FALSE;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PSDP_NODE nodeRoot = NULL;

    if (! pBlob) {
        fFreeBlob = TRUE;
        
        UINT16 pui16NetTypeArray[3];
        pui16NetTypeArray[0] = 0x0800;
        pui16NetTypeArray[1] = 0x0806;
        pui16NetTypeArray[2] = 0x86DD;    
        UINT16 ui16NumNetTypes = 3;

        BTHPAN_LANG_ATTRIB aLangAttrib[1];
        aLangAttrib[0].ui16LangID   = SDP_LANGID_ENGLISH;
        aLangAttrib[0].ui16Encoding = MIBENUM_ENCODING_UTF8;
        aLangAttrib[0].ui16AttribID = LANG_DEFAULT_ID;
        aLangAttrib[0].panServiceName = szFriendlyName;
        aLangAttrib[0].panServiceDesc = szDescription;

        UINT16 ui16NumLangAttribs = 1;

        PSM_LIST             psmList              = {0};

        PSDP_NODE            nodeClassIDList      = NULL;
        PSDP_NODE            nodeClass0           = NULL;

        PSDP_NODE            nodeProtoDescList    = NULL;

        PSDP_NODE            nodeProto0           = NULL;
        PSDP_NODE            nodeProto0UUID       = NULL;
        PSDP_NODE            nodeProto0SParam0    = NULL;

        PSDP_NODE            nodeProto1           = NULL;
        PSDP_NODE            nodeProto1UUID       = NULL;
        PSDP_NODE            nodeProto1SParam0    = NULL;
        PSDP_NODE            nodeProto1SParam1    = NULL;

        PSDP_NODE            nodeLangAttribList   = NULL;

        PSDP_NODE            nodeProfileDescList  = NULL;
        PSDP_NODE            nodeProfile0         = NULL;
        PSDP_NODE            nodeProfile0UUID     = NULL;
        PSDP_NODE            nodeProfile0Param0   = NULL;

        PSDP_NODE            nodeSvcName          = NULL;

        PSDP_NODE            nodeSvcDesc          = NULL;

        PSDP_NODE            nodeSecurityDesc     = NULL;

        PSDP_NODE            nodeNetAccessType    = NULL;

        PSDP_NODE            nodeMaxNetAccessRate = NULL;

        PSDP_NODE            nodeTemp             = NULL;
        UINT16               ui16Ctr              = 0;
        BTHPAN_LANG_ATTRIB   *pLangAttrib          = NULL;
        UINT16               ui16AttribID         = 0;

        //
        // Create the root node.
        //

        nodeRoot = SdpCreateNodeTree();
        if (nodeRoot == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error;
        }

        //
        // Create the Class ID list.
        //

        nodeClassIDList = SdpCreateNodeSequence();
        if (nodeClassIDList == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error;
        }

        SdpAddAttributeToTree (nodeRoot, SDP_ATTRIB_CLASS_ID_LIST, nodeClassIDList);

        nodeClass0 = SdpCreateNodeUUID(&service_id);
        if (nodeClass0 == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error;
        }

        SdpAppendNodeToContainerNode (nodeClassIDList, nodeClass0);

        //
        // Create the protocol descriptor list.
        //

        nodeProtoDescList = SdpCreateNodeSequence();
        if (nodeProtoDescList == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error;
        }

        SdpAddAttributeToTree (nodeRoot, SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST, nodeProtoDescList);

        //
        // Protocol 0.
        //

        SDP_CREATE_PROTO(nodeProtoDescList, L2CAP_PROTOCOL_UUID16, nodeProto0, nodeProto0UUID);
        SDP_CREATE_UINT16(nodeProto0, BTHPAN_PSM, nodeProto0SParam0);

        //
        // Protocol 1.
        //

        SDP_CREATE_PROTO(nodeProtoDescList, BNEP_PROTOCOL_UUID16, nodeProto1, nodeProto1UUID);
        SDP_CREATE_UINT16 (nodeProto1, BNEP_VERSION, nodeProto1SParam0);

        nodeProto1SParam1 = SdpCreateNodeSequence();
        if (nodeProto1SParam1 == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error;
        }

        SdpAppendNodeToContainerNode (nodeProto1, nodeProto1SParam1);

        for (ui16Ctr = 0; ui16Ctr < ui16NumNetTypes; ui16Ctr++) {
            SDP_CREATE_UINT16 (nodeProto1SParam1, pui16NetTypeArray[ui16Ctr], nodeTemp);
        }

        NdisZeroMemory(&psmList, sizeof(psmList));
        ntStatus = SdpValidateProtocolContainer(nodeProtoDescList, &psmList);

        if (!NT_SUCCESS(ntStatus)) {
            IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] MpCreateSdpRecord : Verification failed for Protocol descriptor list\n"));
            goto error;
        }

        //
        // Language Attribute list
        //

        nodeLangAttribList = SdpCreateNodeSequence();
        if (nodeLangAttribList == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error;
        }

        SdpAddAttributeToTree( nodeRoot, SDP_ATTRIB_LANG_BASE_ATTRIB_ID_LIST, nodeLangAttribList);

        for (ui16Ctr = 0, pLangAttrib = aLangAttrib; ui16Ctr < ui16NumLangAttribs; ui16Ctr++, pLangAttrib++) {
            SDP_CREATE_UINT16 (nodeLangAttribList, pLangAttrib->ui16LangID, nodeTemp);
            SDP_CREATE_UINT16 (nodeLangAttribList, pLangAttrib->ui16Encoding, nodeTemp);
            SDP_CREATE_UINT16 (nodeLangAttribList, pLangAttrib->ui16AttribID, nodeTemp);

            //
            // Service name for this language
            //
            char aszString[256];
            char *paString = aszString;
            int iLen = WideCharToMultiByte (CP_ACP, 0, pLangAttrib->panServiceName, -1, aszString, sizeof(aszString), NULL, NULL);
            if (iLen == 0) {
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    iLen = WideCharToMultiByte (CP_ACP, 0, pLangAttrib->panServiceName, -1, NULL, 0, NULL, NULL);
                    if (iLen) {
                        paString = (char *)LocalAlloc (LMEM_FIXED, iLen);
                        if (paString)
                            iLen = WideCharToMultiByte (CP_ACP, 0, pLangAttrib->panServiceName, -1, paString, iLen, NULL, NULL);
                        else
                            iLen = 0;
                    }
                }
            }

            nodeSvcName = iLen ? SdpCreateNodeString(paString, iLen) : NULL;

            if (paString && (paString != aszString))
                LocalFree (paString);

            if (nodeSvcName == NULL) {
                ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                goto error;
            }

            SdpAddAttributeToTree (nodeRoot, pLangAttrib->ui16AttribID + STRING_NAME_OFFSET, nodeSvcName);

            //
            // Service description for this language
            //
            paString = aszString;
            iLen = WideCharToMultiByte (CP_ACP, 0, pLangAttrib->panServiceDesc, -1, aszString, sizeof(aszString), NULL, NULL);
            if (iLen == 0) {
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    iLen = WideCharToMultiByte (CP_ACP, 0, pLangAttrib->panServiceDesc, -1, NULL, 0, NULL, NULL);
                    if (iLen) {
                        paString = (char *)LocalAlloc (LMEM_FIXED, iLen);
                        if (paString)
                            iLen = WideCharToMultiByte (CP_ACP, 0, pLangAttrib->panServiceDesc, -1, paString, iLen, NULL, NULL);
                        else
                            iLen = 0;
                    }
                }
            }

            nodeSvcDesc = iLen ? SdpCreateNodeString (paString, iLen) : NULL;

            if (paString && (paString != aszString))
                LocalFree (paString);

            if (nodeSvcDesc == NULL) {
                ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                goto error;
            }

            SdpAddAttributeToTree (nodeRoot, pLangAttrib->ui16AttribID + STRING_DESCRIPTION_OFFSET, nodeSvcDesc);
        }

        //
        // Optional attribute: Service availability
        // We are not setting this attribute right now.
        //

        //
        // Profile descriptor list
        //

        nodeProfileDescList = SdpCreateNodeSequence();
        if (nodeProfileDescList == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error;
        }

        SdpAddAttributeToTree (nodeRoot, SDP_ATTRIB_PROFILE_DESCRIPTOR_LIST, nodeProfileDescList);

        nodeProfile0 = SdpCreateNodeSequence();
        if (nodeProfile0 == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error;
        }

        SdpAppendNodeToContainerNode (nodeProfileDescList, nodeProfile0);

        nodeProfile0UUID = SdpCreateNodeUUID(&service_id);
        if (nodeProfile0UUID == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error;
        }

        SdpAppendNodeToContainerNode (nodeProfile0, nodeProfile0UUID);

        SDP_CREATE_UINT16 (nodeProfile0, (UINT16) ((BNEP_MAJOR_VERSION << (sizeof(UINT8) * 8)) | BNEP_MINOR_VERSION), nodeProfile0Param0);

        //
        // Security description
        //

        nodeSecurityDesc = SdpCreateNodeUInt16 (0);
        if (nodeSecurityDesc == NULL) {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error;
        }

        SdpAddAttributeToTree (nodeRoot, SDP_ATTRIB_PAN_SECURITY_DESCRIPTION, nodeSecurityDesc);

        if (pan_role == NAP) {
            //
            // Net access type
            //

            nodeNetAccessType = SdpCreateNodeUInt16(NET_ACCESS_TYPE_ETH_100MB);

            if (nodeNetAccessType == NULL) {
                ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                goto error;
            }

            SdpAddAttributeToTree (nodeRoot, SDP_ATTRIB_PAN_NET_ACCESS_TYPE, nodeNetAccessType);

            //
            // Maximum access rate
            //

            nodeMaxNetAccessRate = SdpCreateNodeUInt32(MAX_NET_ACCESS_RATE_ETH_100MB);
            if (nodeMaxNetAccessRate == NULL) {
                ntStatus = STATUS_INSUFFICIENT_RESOURCES;
                goto error;
            }

            SdpAddAttributeToTree (nodeRoot, SDP_ATTRIB_PAN_MAX_NET_ACCESS_RATE, nodeMaxNetAccessRate);
        }

        //
        // Optional attribute: Add IPv4 and IPv6 subnet
        // We are not setting this attribute right now.
        //

        //
        // Make a stream from the monster we just created
        //
        ntStatus = SdpStreamFromTreeEx (nodeRoot, (PUCHAR *)&pBlob, (PULONG)&cBlobData, offsetof(BTHNS_SETBLOB, pRecord), 0);

        if (!NT_SUCCESS(ntStatus)) {
            goto error;
        }

        ntStatus = SdpVerifyServiceRecord (pBlob->pRecord, cBlobData, VERIFY_CHECK_MANDATORY_LOCAL, &ui16AttribID);

        if (!NT_SUCCESS(ntStatus))
            goto error;

        memset (pBlob, 0, offsetof(BTHNS_SETBLOB, pRecord));
    }
    
    if (pBlob) {    // Register SDP record
        *pulSdpRecordId = 0;

        ULONG ulSdpVersion = BTH_SDP_VERSION;

        pBlob->pRecordHandle  = pulSdpRecordId;
        pBlob->ulRecordLength = cBlobData;
        pBlob->pSdpVersion    = &ulSdpVersion;

        BLOB blob;
        blob.cbSize    = sizeof(BTHNS_SETBLOB) + cBlobData - 1;
        blob.pBlobData = (PBYTE) pBlob;

        WSAQUERYSET wqs;
        memset (&wqs, 0, sizeof(wqs));
        wqs.dwSize = sizeof(WSAQUERYSET);
        wqs.dwNameSpace = NS_BTH;
        wqs.lpBlob = &blob;
        BthNsSetService (&wqs, RNRSERVICE_REGISTER, 0);

        if (fFreeBlob)
            SdpFreePool (pBlob);
    }

error:

    if (!NT_SUCCESS(ntStatus)) {
        if (ntStatus != STATUS_INSUFFICIENT_RESOURCES) {
            IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] SDP Verification failed\n"));
        }

        if (pBlob && fFreeBlob)
            SdpFreePool(pBlob);
    }

    if (nodeRoot != NULL)
        SdpFreeTree(nodeRoot);

    return;    
    
}

static void AddSDPRecord(void) {
    IFDBG(DebugOut(DEBUG_PAN_TRACE, L"++AddSDPRecord\n"));
    
    BOOL fApplySDPBlob = FALSE;

    DWORD pan_role = NONE;
    WCHAR szFriendlyName[MAX_PATH];
    WCHAR szDescription[MAX_PATH];
    GUID service_id;
    
    HKEY hk = NULL;
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Comm\\BTPAN1\\Parms", 0, 0, &hk))
    {
        BOOL fDoSdpNow = FALSE;
        DWORD cbSize = sizeof(fDoSdpNow);
        if (ERROR_SUCCESS != RegQueryValueEx(hk, L"PublishSdpOnBoot", NULL, NULL, (LPBYTE)&fDoSdpNow, &cbSize) ||
            !fDoSdpNow)
        {
            IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] AddSDPRecord: Failed to query PublicSdpOnBoot value.\n"));
            goto exit;
        }
        
        WCHAR szAdapterType[5];
        cbSize = sizeof(szAdapterType);
        if (ERROR_SUCCESS != RegQueryValueEx(hk, L"AdapterType", NULL, NULL, (LPBYTE)szAdapterType, &cbSize))
        {
            IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] AddSDPRecord: Failed to query AdapterType value.\n"));
            goto exit;
        }
        
        if (! wcscmp(szAdapterType, L"PANU"))
        {
            pan_role = PANU;
        }
        else if (! wcscmp(szAdapterType, L"GN"))
        {
            pan_role = GN;
        }
        else if (! wcscmp(szAdapterType, L"NAP"))
        {
            pan_role = NAP;
        }
        else
        {
            IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] AddSDPRecord: Invalid AdapterType value.\n"));
            goto exit;
        }

        cbSize = sizeof(szFriendlyName);
        if (ERROR_SUCCESS != RegQueryValueEx(hk, L"FriendlyName", NULL, NULL, (LPBYTE)szFriendlyName, &cbSize))
        {
            IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] AddSDPRecord: Failed to query FriendlyName value.\n"));
            goto exit;
        }

        cbSize = sizeof(szDescription);
        if (ERROR_SUCCESS != RegQueryValueEx(hk, L"Description", NULL, NULL, (LPBYTE)szDescription, &cbSize))
        {
            IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] AddSDPRecord: Failed to query Description value.\n"));
            goto exit;
        }

        WCHAR szServiceId[GUID_STR_LENGTH];
        cbSize = sizeof(szServiceId);
        if (ERROR_SUCCESS != RegQueryValueEx(hk, L"ServiceId", NULL, NULL, (LPBYTE)szServiceId, &cbSize) ||
            !MpStrToGUID(szServiceId, &service_id))
        {
            IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] AddSDPRecord: Failed to query or invalid ServiceId value.\n"));
            goto exit;
        }

        ULONG ulSdpRecordId = 0; // we never unregister
        MpCreateSdpRecord (NULL, 0, szFriendlyName, szDescription, service_id, pan_role, &ulSdpRecordId);
    }    
   
exit:
    if (hk) {
        RegCloseKey(hk);
    }

    IFDBG(DebugOut(DEBUG_PAN_TRACE, L"--AddSDPRecord\n"));
    
    return;
}



PREFAST_SUPPRESS(262,"This call stack will not be deep, don’t want to use heap.");
static void MpDoSDP (BT_ADDR *pbt, BTHPAN_NETWORK *pNet, WCHAR *name) {
    IFDBG(DebugOut(DEBUG_PAN_TRACE, L"++MpDoSDP for %s\n", name));
    
    union {
        BTHNS_RESTRICTIONBLOB RBlob;
        unsigned char ucBlob[offsetof(BTHNS_RESTRICTIONBLOB, pRange) + sizeof(SdpAttributeRange) * 2];
    };

    memset (&ucBlob, 0, sizeof(ucBlob));

    RBlob.type = SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST;
    RBlob.numRange = 2;
    RBlob.pRange[0].minAttribute = SDP_ATTRIB_CLASS_ID_LIST;
    RBlob.pRange[0].maxAttribute = SDP_ATTRIB_CLASS_ID_LIST;
    RBlob.pRange[1].minAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
    RBlob.pRange[1].maxAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
    RBlob.uuids[0].uuidType = SDP_ST_UUID16;
    RBlob.uuids[0].u.uuid16 = BNEP_PROTOCOL_UUID16;       // BNEP

    BLOB blob;
    blob.cbSize = offsetof(BTHNS_RESTRICTIONBLOB, pRange) + sizeof(SdpAttributeRange) * 2;
    blob.pBlobData = (BYTE *)&RBlob;

    SOCKADDR_BTH    sa;

    memset (&sa, 0, sizeof(sa));

    *(BT_ADDR *)(&sa.btAddr) = *pbt;
    sa.addressFamily = AF_BT;

    CSADDR_INFO        csai;

    memset (&csai, 0, sizeof(csai));
    csai.RemoteAddr.lpSockaddr = (sockaddr *)&sa;
    csai.RemoteAddr.iSockaddrLength = sizeof(sa);

    WSAQUERYSET        wsaq;

    memset (&wsaq, 0, sizeof(wsaq));
    wsaq.dwSize      = sizeof(wsaq);
    wsaq.dwNameSpace = NS_BTH;
    wsaq.lpBlob      = &blob;
    wsaq.lpcsaBuffer = &csai;

    HANDLE hLookup;
    int iRet = BthNsLookupServiceBegin (&wsaq, 0, &hLookup);

    int fFound = FALSE;

    if (ERROR_SUCCESS == iRet) {
        CHAR  buf[5000];
        LPWSAQUERYSET pwsaResults = (LPWSAQUERYSET) buf;
        DWORD dwSize = sizeof(buf);

        memset(pwsaResults,0,sizeof(WSAQUERYSET));
        pwsaResults->dwSize      = sizeof(WSAQUERYSET);
        pwsaResults->dwNameSpace = NS_BTH;
        pwsaResults->lpBlob      = NULL;

        iRet = BthNsLookupServiceNext (hLookup, 0, &dwSize, pwsaResults);
        if ((iRet == ERROR_SUCCESS) && MpParseSdp (pwsaResults->lpBlob->pBlobData, pwsaResults->lpBlob->cbSize, &pNet->a.service_id)) {
            WCHAR *pszSvc = NULL;
            WCHAR szSvc[_MAX_PATH];
            if (pNet->a.service_id == NAPServiceClass_UUID)
                pszSvc = L"NAP";
            else if (pNet->a.service_id == GNServiceClass_UUID)
                pszSvc = L"GN";
            else if (pNet->a.service_id == PANUServiceClass_UUID)
                pszSvc = L"PANU";
            else {
                if ((pNet->a.service_id.Data2 == Bluetooth_Base_UUID.Data2) &&
                    (pNet->a.service_id.Data3 == Bluetooth_Base_UUID.Data3) &&
                    (memcmp (pNet->a.service_id.Data4, Bluetooth_Base_UUID.Data4, 8) == 0)) {
                        // Guid is an offset from Bluetooth' base GUID
                    unsigned int offset = pNet->a.service_id.Data1 - Bluetooth_Base_UUID.Data1;
                    wsprintf (szSvc, L"0x%x", offset);
                } else {
                    wsprintf (szSvc, L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                            pNet->a.service_id.Data1, pNet->a.service_id.Data2, pNet->a.service_id.Data3,
                            pNet->a.service_id.Data4[0], pNet->a.service_id.Data4[1], pNet->a.service_id.Data4[2], pNet->a.service_id.Data4[3], 
                            pNet->a.service_id.Data4[4], pNet->a.service_id.Data4[5], pNet->a.service_id.Data4[6], pNet->a.service_id.Data4[7]); 
                }

                pszSvc = szSvc;
            }

            int cchN = wcslen (name);
            int cchP = wcslen (pszSvc);

            WCHAR *pszFmt = NULL;
            if (cchN + cchP + 4 <= 32) { // all fit, with space and parenthesis
                pszFmt = L"%s (%s)";
            } else if (cchN + cchP + 2 <= 32) { // all fit, with space
                pszFmt = L"%s %s";
            } else if (cchN + cchP + 1 <= 32) { // all fit, with no space
                pszFmt = L"%s%s";
            } else if (cchP <= 10) {    // fit all guid
                pszFmt = L"%20s-%s";
            } else if (cchN <= 20) {    // fit all name
                pszFmt = L"%s-%10s";
            } else {
                pszFmt = L"%20s-%10s";
            }

            wsprintf (pNet->a.szSSID, pszFmt, name, pszSvc);
            pNet->iRef = REF_DISCOVERY;

            fFound = TRUE;

            IFDBG(DebugOut(DEBUG_PAN_TRACE, L"MpDoSDP : service found, SSID: %s\nMpDoSDP: guid %08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x\n", pNet->a.szSSID, pNet->a.service_id.Data1, pNet->a.service_id.Data2, pNet->a.service_id.Data3,
                pNet->a.service_id.Data4[0], pNet->a.service_id.Data4[1], pNet->a.service_id.Data4[2], pNet->a.service_id.Data4[3], 
                pNet->a.service_id.Data4[4], pNet->a.service_id.Data4[5], pNet->a.service_id.Data4[6], pNet->a.service_id.Data4[7]));
        }

        BthNsLookupServiceEnd(hLookup);
    }
    if (! fFound) {
        IFDBG(DebugOut(DEBUG_PAN_TRACE, L"MpDoSDP %s : service NOT FOUND\n", name));
        pNet->iRef = REF_EMPTY;
        memset (&pNet->a, 0, sizeof(pNet->a));
    }
    
    IFDBG(DebugOut(DEBUG_PAN_TRACE, L"--MpDoSDP\n"));
}

static DWORD PanConnectCycle (BOOL *pfImmediateRescan) {
    DWORD dwResult = ERROR_SUCCESS;

    BTHPAN_ADAPTER *pAdapter = NULL;
    BTHPAN_ASSOCIATION *pAssoc = NULL;

    gpPAN->Lock ();
    
    for (int i = 0 ; (i < SVSUTIL_ARRLEN(gpPAN->aAdapters)) && (! pAssoc) ; ++i) {
        if (gpPAN->aAdapters[i].state != UP)
            continue;

        if (gpPAN->aAdapters[i].type == NAP)
            continue;

        if (CountConn (&gpPAN->aAdapters[i]))
            continue;

        for (int j = 0 ; (j < (int)gpPAN->aAdapters[i].cAssoc) && (! pAssoc); ++j) {
            for (int k = 0 ; k < SVSUTIL_ARRLEN(gpPAN->aNetworks) ; ++k) {
                if (gpPAN->aNetworks[k].iRef != REF_DISCOVERY)  // Only connect to things that were discovered right now
                    continue;

                if (gpPAN->aAdapters[i].aAssoc[j].ba == gpPAN->aNetworks[k].a.ba) {
                    if (gpPAN->aAdapters[i].aAssoc[j].service_id == gpPAN->aNetworks[k].a.service_id) {
                        pAdapter = &gpPAN->aAdapters[i];
                        pAssoc = &gpPAN->aAdapters[i].aAssoc[j];
                        *pfImmediateRescan = FALSE;

                        pAdapter->AddRef ();

                        break;
                    }
                    else {
                        // Service Id between association and network do not match!                                
                        IFDBG(DebugOut(DEBUG_PAN_TRACE, L"NetworkMonitorThread: Service Id of association and network do not match.\n"));
                        gpPAN->aNetworks[k].iRef = REF_EMPTY;
                        *pfImmediateRescan = TRUE;
                    }
                }
            }
        }

    }
    
    gpPAN->Unlock ();

    if (pAdapter) {
        MpAttemptConnection (pAdapter, &pAssoc->ba, &pAssoc->service_id);

        pAdapter->DelRef ();
    }

    return dwResult;
}

PREFAST_SUPPRESS(262,"This call stack will not be deep, don’t want to use heap.");
static void DoPanInquiry (void) {
    int i;

    gpPAN->Lock ();
    
    for (i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aNetworks) ; ++i) {
        if (gpPAN->aNetworks[i].iRef == 0)
            gpPAN->aNetworks[i].iRef = REF_EMPTY;
        else if (gpPAN->aNetworks[i].iRef <= REF_DISCOVERY)
            --gpPAN->aNetworks[i].iRef;
    }
    
    gpPAN->Unlock ();
    
    BthInquiryResult inq[255];
    unsigned int     cinq = 0;
    BthPerformInquiry (BT_GIAC, gpPAN->ucInquiryLength, 0, SVSUTIL_ARRLEN(inq), &cinq, inq);
    if (! cinq)
        return;

    for (i = 0 ; i < (int)cinq ; ++i) {
        BD_ADDR ba;
        ba.NAP = GET_NAP(inq[i].ba);
        ba.SAP = GET_SAP(inq[i].ba);

        gpPAN->Lock ();

        int iFreeSlot  = -1;
        for (int j = 0 ; j < SVSUTIL_ARRLEN(gpPAN->aNetworks) ; ++j) {
            if (gpPAN->aNetworks[j].iRef == REF_EMPTY) {
                if (iFreeSlot == -1)
                    iFreeSlot = j;
                continue;
            }

            if (gpPAN->aNetworks[j].a.ba == ba) {
                gpPAN->aNetworks[j].iRef = REF_DISCOVERY;
                break;
            }
        }

        if ((j < SVSUTIL_ARRLEN(gpPAN->aNetworks)) || (iFreeSlot == -1)) {
            gpPAN->Unlock ();
            return;
        }

        memset (&gpPAN->aNetworks[iFreeSlot].a, 0, sizeof(gpPAN->aNetworks[iFreeSlot].a));

        gpPAN->aNetworks[iFreeSlot].iRef = REF_CONNECT;
        gpPAN->aNetworks[iFreeSlot].a.ba = ba;

        gpPAN->Unlock ();

        // Get the name...
        WCHAR name[248];
        unsigned int cchRet = 0;
        int iRes = BthRemoteNameQuery (&inq[i].ba, SVSUTIL_ARRLEN(name), &cchRet, name);
        if (iRes == ERROR_SUCCESS) { // Derive SSID from friendly name
            name[31] = '\0';
        } else {    // Derive SSID from address
            wsprintf (name, L"%04x%08x", ba.NAP, ba.SAP);
        }

        if (gpPAN->state != UP)
            break;

        MpDoSDP (&inq[i].ba, &gpPAN->aNetworks[iFreeSlot], name);
    }
}

static DWORD WINAPI NetworkMonitorThread (LPVOID lpUnused) {
    IFDBG(DebugOut(DEBUG_PAN_INIT, L"++NetworkMonitorThread - thread started\n"));

    gpPAN->AddRef ();

    while (gpPAN->state == CONNECTING)
        Sleep (100);

    BOOL fImmediateRescan = FALSE;
    
    while (gpPAN->state == UP) {
        if (! fImmediateRescan)
            WaitForSingleObject (gpPAN->hRefreshEvent, gpPAN->mediaDelay);

        fImmediateRescan = FALSE;

        if (gpPAN->state != UP)
            break;

        BOOL fDoInquiry = FALSE;

        gpPAN->Lock ();

        for (int i = 0 ; (i < SVSUTIL_ARRLEN(gpPAN->aAdapters)) ; ++i) {
            if ((gpPAN->aAdapters[i].state == UP) &&
                (gpPAN->aAdapters[i].type != NAP) && 
                (CountConn (&gpPAN->aAdapters[i]) == 0))
            {
                MpGetAssociations (&gpPAN->aAdapters[i]);   // Refresh...
                if (gpPAN->aAdapters[i].cAssoc > 0) {
                    fDoInquiry = TRUE;
                    break;
                }
            }
        }

        gpPAN->Unlock ();

        if (fDoInquiry) {            
            do {
                DoPanInquiry ();

                fImmediateRescan = FALSE;
                PanConnectCycle (&fImmediateRescan);
            } while (fImmediateRescan);
        }
    }

    gpPAN->DelRef ();

    IFDBG(DebugOut(DEBUG_PAN_INIT, L"--NetworkMonitorThread - thread exited\n"));

    return 0;
}


static VOID MpSetInformationEpilogue (BTHPAN_ADAPTER *pAdapter, NDIS_OID Oid) {
    UINT  ui = 0;
    
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpSetInformationEpilogue\n"));
    
    switch (Oid)
    {
    case OID_802_3_MULTICAST_LIST:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"Adapter: 0x%08x, Multicast list count: 0x%08x\n", (UINT) pAdapter, pAdapter->filter.cMCasts));

        for (ui = 0; ui < pAdapter->filter.cMCasts; ui++) {
            IFDBG(DebugOut (DEBUG_PAN_TRACE, L"    [%04x%08x - %04x%08x]\n", pAdapter->filter.amc[ui].from.NAP, 
                pAdapter->filter.amc[ui].from.SAP, pAdapter->filter.amc[ui].to.NAP, pAdapter->filter.amc[ui].to.SAP));
        }
        break;

    case OID_GEN_CURRENT_PACKET_FILTER:
        //
        // Start the multicast filter set process on all open connections.
        // This needs to be done for the packet filter as well in case
        // NDIS decides to turn off broadcast packets.
        //        
        MpFilterMulticastStartOnAllConnections (pAdapter);
        break;
    }
    
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpSetInformationEpilogue\n"));
}

static VOID MpHalt (NDIS_HANDLE  mpAdapterContext) {
    IFDBG(DebugOut (DEBUG_PAN_INIT, L"++MpHalt\n"));

    BTHPAN_ADAPTER *pAdapter = (BTHPAN_ADAPTER *)MpFindAndRefAdapter (mpAdapterContext);

    if (! pAdapter) {
        IFDBG(DebugOut(DEBUG_PAN_INIT, L"--MpHalt - no adapter\n"));
        return;
    }

    // Delete Bluetooth primitives

    pAdapter->state = CLOSING;

    if (pAdapter->ulSdpRecordId) {
        ULONG ulSdpVersion = BTH_SDP_VERSION;
        
        BTHNS_SETBLOB nsblob;
        memset (&nsblob, 0, sizeof(nsblob));
        nsblob.pRecordHandle = &pAdapter->ulSdpRecordId;
        nsblob.pSdpVersion = &ulSdpVersion;

        BLOB blob;
        blob.cbSize    = sizeof(BTHNS_SETBLOB);
        blob.pBlobData = (PBYTE) &nsblob;

        WSAQUERYSET wqs;
        memset (&wqs, 0, sizeof(wqs));
        wqs.dwSize = sizeof(WSAQUERYSET);
        wqs.dwNameSpace = NS_BTH;
        wqs.lpBlob = &blob;
        BthNsSetService (&wqs, RNRSERVICE_DELETE, 0);

        pAdapter->ulSdpRecordId = 0;
    }

    int nAdapter = pAdapter - gpPAN->aAdapters;

    gpPAN->Lock ();
    for (int i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aConn) ; ++i) {
        if (gpPAN->aConn[i].nAdapter == nAdapter) {
            gpPAN->aConn[i].nAdapter = -1;
            CloseConnection (&gpPAN->aConn[i], NULL);
        }
    }

    pAdapter->DelRef ();

    while (pAdapter->GetRefCount () > 0) {
        gpPAN->Unlock ();
        Sleep (1000);
        gpPAN->Lock ();
    }

    NdisFreePacketPool (pAdapter->NdisPacketPool);
    NdisFreeBufferPool (pAdapter->NdisBufferPool);

    int uiAdapterRef = pAdapter->uiRef;
    pAdapter->Reinit ();
    pAdapter->uiRef = uiAdapterRef + 1;

    gpPAN->Unlock ();

    IFDBG(DebugOut (DEBUG_PAN_INIT, L"--MpHalt\n"));
}


static NDIS_STATUS MpInitialize(PNDIS_STATUS OpenErrorStatus, PUINT SelectedMediumIndex, PNDIS_MEDIUM MediumArray,
               UINT MediumArraySize, NDIS_HANDLE mpAdapterHandle, NDIS_HANDLE WrapperConfigurationContext) {
    IFDBG(DebugOut(DEBUG_PAN_INIT, L"++MpInitialise\n"));

    UNREFERENCED_PARAMETER(OpenErrorStatus);
    UNREFERENCED_PARAMETER(WrapperConfigurationContext);

    NDIS_HANDLE hConfig     = NULL;
    NDIS_STRING nszKeyword  = {0};

    //
    // Have we got a BTHPAN_MEDIUM?
    //
    UINT uiIdx       = 0;
    while ( (uiIdx < MediumArraySize) && (MediumArray[uiIdx] != NdisMedium802_3) )
        uiIdx++;

    NDIS_STATUS ndisStatus  = NDIS_STATUS_SUCCESS;

    if (uiIdx == MediumArraySize) {
        ndisStatus = NDIS_STATUS_UNSUPPORTED_MEDIA;
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] MpInitialize: Unsupported media 0x%x\n", ndisStatus));
        goto error;
    }

    *SelectedMediumIndex = uiIdx;

    if (! gpPAN) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] MpInitialize: context doesn't exist\n"));
        ndisStatus = NDIS_STATUS_ADAPTER_NOT_READY;
        goto error;
    }

    gpPAN->Lock ();
    if (gpPAN->state != UP) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] MpInitialize: context not up\n"));
        ndisStatus = NDIS_STATUS_ADAPTER_NOT_READY;
        gpPAN->Unlock ();
        goto error;
    }

    BTHPAN_ADAPTER *pAdapter = NULL;
    for (int i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aAdapters) ; ++i) {
        if (gpPAN->aAdapters[i].state == DOWN) {
            pAdapter = &gpPAN->aAdapters[i];
            pAdapter->state = CONNECTING;
            break;
        }
    }

    gpPAN->Unlock ();

    if (! pAdapter) {
        ndisStatus = NDIS_STATUS_RESOURCES;
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN]  OOM 0x%x\n", ndisStatus));
        goto error;
    }

    //
    // Initialize NDIS part
    //
    pAdapter->hAdapter = mpAdapterHandle;

    NdisAllocatePacketPool (&ndisStatus, &pAdapter->NdisPacketPool, BTHPAN_PACKETS, sizeof(void *));
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] could not alloc packet pool 0x%x\n", ndisStatus));
        goto error;
    }

    NdisAllocateBufferPool (&ndisStatus, &pAdapter->NdisBufferPool, BTHPAN_BUFFERS);
    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] could not alloc buffer pool 0x%x\n", ndisStatus));
        goto error;
    }

    //
    // Inform NDIS of the attributes of our adapter. This must be done before
    // calling NdisMRegisterXxx or NdisXxx which depend on infomation supplied
    // to NdisMSetAttributesEx
    //
        
    NdisMSetAttributesEx(
        mpAdapterHandle,
        (NDIS_HANDLE) AdapterToCtx(pAdapter),
        BTHPAN_CHECK_FOR_HANG_TIMEOUT,
        // TODO : Should we add this?
        // NDIS_ATTRIBUTE_NO_HALT_ON_SUSPEND |
        // NDIS_ATTRIBUTE_SURPRISE_REMOVE_OK
        NDIS_ATTRIBUTE_DESERIALIZE |
        NDIS_ATTRIBUTE_NOT_CO_NDIS,
        NdisInterfacePci);

    //
    // Get the miniport name from NDIS.
    //
    NdisOpenConfiguration (&ndisStatus, &hConfig, WrapperConfigurationContext);

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] could not open configuration 0x%x\n", ndisStatus));
        goto error;
    }

    NdisInitUnicodeString (&nszKeyword, L"MiniportName");
    PNDIS_CONFIGURATION_PARAMETER pncpValue   = NULL;
    NdisReadConfiguration (&ndisStatus, &pncpValue, hConfig, &nszKeyword, NdisParameterString);

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] could not read configuration 0x%x\n", ndisStatus));
        goto error;
    }

    if (pncpValue->ParameterData.StringData.Length > ((SVSUTIL_ARRLEN(pAdapter->szIfName)-1) * sizeof(WCHAR))) {
        ndisStatus = NDIS_STATUS_BAD_CHARACTERISTICS;
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] bad interface name 0x%x\n", ndisStatus));
        goto error;
    }

    memcpy (pAdapter->szIfName, pncpValue->ParameterData.StringData.Buffer, pncpValue->ParameterData.StringData.Length);
    *(WCHAR *)((char *)pAdapter->szIfName + pncpValue->ParameterData.StringData.Length) = '\0';

    // Try read a miniport network address from registry
    unsigned int uiAddr = 0;
    void *pvAddr = NULL;

    NdisReadNetworkAddress(&ndisStatus, &pvAddr, &uiAddr, hConfig);
    if ((ndisStatus != NDIS_STATUS_SUCCESS) || (uiAddr != 6)) {
        IFDBG(DebugOut (DEBUG_WARN, L"[PAN] could not read network address 0x%x. Using default Bluetooth address.\n", ndisStatus));
        pAdapter->fAddressSet = FALSE;
    } else {
        NdisMoveMemory(&pAdapter->ethAddr, pvAddr, uiAddr);
        pAdapter->fAddressSet = TRUE;
    }

    NdisInitUnicodeString (&nszKeyword, L"AdapterType");
    pncpValue   = NULL;
    NdisReadConfiguration (&ndisStatus, &pncpValue, hConfig, &nszKeyword, NdisParameterString);

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] could not read AdapterType 0x%x\n", ndisStatus));
        goto error;
    }

    if (MpNdisStrCmp (&pncpValue->ParameterData.StringData, L"PANU") == 0) {
        pAdapter->type = PANU;
        pAdapter->fAcceptIncoming = FALSE;
        pAdapter->cMaxConn = 1;

        MpGetAssociations (pAdapter);
    } else if (MpNdisStrCmp (&pncpValue->ParameterData.StringData, L"GN") == 0) {
        pAdapter->type = GN;
        pAdapter->fAcceptIncoming = TRUE;
        pAdapter->cMaxConn = BTHPAN_MAX_CONNECTIONS;

        MpGetAssociations (pAdapter);
    } else if (MpNdisStrCmp (&pncpValue->ParameterData.StringData, L"NAP") == 0) {
        pAdapter->type = NAP;
        pAdapter->fAcceptIncoming = TRUE;
        pAdapter->cMaxConn = BTHPAN_MAX_CONNECTIONS;
    } else {
        ndisStatus = NDIS_STATUS_ADAPTER_NOT_FOUND;
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] AdapterType is incorrect (should be PANU, GN or NAP)\n"));
        goto error;
    }

    NdisInitUnicodeString (&nszKeyword, L"ServiceId");
    pncpValue   = NULL;
    NdisReadConfiguration (&ndisStatus, &pncpValue, hConfig, &nszKeyword, NdisParameterString);
    if ((ndisStatus != NDIS_STATUS_SUCCESS) || (! MpNdisStrToGUID (&pncpValue->ParameterData.StringData, &pAdapter->service_id))) {
        if (ndisStatus == NDIS_STATUS_SUCCESS)
            ndisStatus = NDIS_STATUS_ADAPTER_NOT_FOUND;
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] could not read service id 0x%x\n", ndisStatus));
        goto error;
    }

    NdisInitUnicodeString (&nszKeyword, L"ConnectionTimeout");
    pncpValue   = NULL;
    NdisReadConfiguration (&ndisStatus, &pncpValue, hConfig, &nszKeyword, NdisParameterInteger);
    if ((ndisStatus == NDIS_STATUS_SUCCESS) && (pncpValue->ParameterData.IntegerData >= BTHPAN_CRT_MIN) && (pncpValue->ParameterData.IntegerData <= BTHPAN_CRT_MAX))
        pAdapter->uiCrtTimeout = pncpValue->ParameterData.IntegerData;
    else
        pAdapter->uiCrtTimeout = BTHPAN_CRT_DEFAULT;

    if (pAdapter->type != NAP) {
        NdisInitUnicodeString (&nszKeyword, L"AcceptConnections");
        pncpValue   = NULL;
        NdisReadConfiguration (&ndisStatus, &pncpValue, hConfig, &nszKeyword, NdisParameterInteger);
        if (ndisStatus == NDIS_STATUS_SUCCESS)
            pAdapter->fAcceptIncoming = pncpValue->ParameterData.IntegerData;
        else
            pAdapter->fAcceptIncoming = FALSE;
    }

    if (pAdapter->type != PANU) {
        NdisInitUnicodeString (&nszKeyword, L"MaxConnections");
        pncpValue   = NULL;
        NdisReadConfiguration (&ndisStatus, &pncpValue, hConfig, &nszKeyword, NdisParameterInteger);
        if (ndisStatus == NDIS_STATUS_SUCCESS)
            pAdapter->cMaxConn = pncpValue->ParameterData.IntegerData;
    }

    if ((pAdapter->type != PANU) || pAdapter->fAcceptIncoming) {
        NdisInitUnicodeString (&nszKeyword, L"FriendlyName");
        NdisReadConfiguration (&ndisStatus, &pncpValue, hConfig, &nszKeyword, NdisParameterString);

        if ((ndisStatus == NDIS_STATUS_SUCCESS) && (pncpValue->ParameterData.StringData.Length <= ((SVSUTIL_ARRLEN(pAdapter->szFriendlyName)-1) * sizeof(WCHAR)))) {
            memcpy (pAdapter->szFriendlyName, pncpValue->ParameterData.StringData.Buffer, pncpValue->ParameterData.StringData.Length);
            *(WCHAR *)((char *)pAdapter->szFriendlyName + pncpValue->ParameterData.StringData.Length) = '\0';
        } else
            wcscpy (pAdapter->szFriendlyName, pAdapter->szIfName);

        NdisInitUnicodeString (&nszKeyword, L"Description");
        NdisReadConfiguration (&ndisStatus, &pncpValue, hConfig, &nszKeyword, NdisParameterString);

        if ((ndisStatus == NDIS_STATUS_SUCCESS) && (pncpValue->ParameterData.StringData.Length <= ((SVSUTIL_ARRLEN(pAdapter->szDescription)-1) * sizeof(WCHAR)))) {
            memcpy (pAdapter->szDescription, pncpValue->ParameterData.StringData.Buffer, pncpValue->ParameterData.StringData.Length);
            *(WCHAR *)((char *)pAdapter->szDescription + pncpValue->ParameterData.StringData.Length) = '\0';
        } else
            wcscpy (pAdapter->szDescription, pAdapter->szIfName);

        BOOL fPublishSdpOnBoot = FALSE;

        NdisInitUnicodeString (&nszKeyword, L"PublishSdpOnBoot");
        pncpValue   = NULL;
        NdisReadConfiguration (&ndisStatus, &pncpValue, hConfig, &nszKeyword, NdisParameterInteger);
        if (ndisStatus == NDIS_STATUS_SUCCESS)
            fPublishSdpOnBoot = pncpValue->ParameterData.IntegerData;

        if (! fPublishSdpOnBoot)
        {
            NdisInitUnicodeString (&nszKeyword, L"SDP");
            pncpValue   = NULL;
            NdisReadConfiguration (&ndisStatus, &pncpValue, hConfig, &nszKeyword, NdisParameterBinary);

            BTHNS_SETBLOB *pBlob = NULL;
            int cBlobData = 0;

            if (ndisStatus == NDIS_STATUS_SUCCESS) {
                pBlob = (BTHNS_SETBLOB *)LocalAlloc (LMEM_FIXED, sizeof(BTHNS_SETBLOB) + pncpValue->ParameterData.BinaryData.Length - 1);
                cBlobData = pncpValue->ParameterData.BinaryData.Length;

                memset (pBlob, 0, sizeof(BTHNS_SETBLOB) + cBlobData - 1);
                memcpy (pBlob->pRecord, pncpValue->ParameterData.BinaryData.Buffer, cBlobData);
                GUID guidNull = {0};
                MpCreateSdpRecord (pBlob, cBlobData, NULL, NULL, guidNull, 0, &pAdapter->ulSdpRecordId);
                LocalFree(pBlob);
            } else {
                MpCreateSdpRecord (NULL, 0, pAdapter->szFriendlyName, pAdapter->szDescription, pAdapter->service_id, pAdapter->type, &pAdapter->ulSdpRecordId);
                ndisStatus = NDIS_STATUS_SUCCESS;
            }
        }
    }

    if (ndisStatus != NDIS_STATUS_SUCCESS)
        goto error;

    gpPAN->Lock ();
    if (pAdapter->state == CONNECTING)
        pAdapter->state = UP;
    else
        ndisStatus = NDIS_STATUS_ADAPTER_NOT_FOUND;
    gpPAN->Unlock ();

    if (ndisStatus == NDIS_STATUS_SUCCESS)
        MpMediaStatusIndicate (pAdapter);

 error:
    if (hConfig != NULL)
        NdisCloseConfiguration(hConfig);

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        //
        // Destroy the adapter nicely.
        //
        if (pAdapter != NULL)
            MpHalt (pAdapter);
    }

    IFDBG(DebugOut(DEBUG_PAN_INIT, L"--MpInitialise (%d)\n", ndisStatus));
    return ndisStatus;
}

static VOID MpPnPEventNotify(
    IN      NDIS_HANDLE            mpAdapterContext,
    IN      NDIS_DEVICE_PNP_EVENT  PnPEvent,
    IN      PVOID                  InformationBuffer,
    IN      ULONG                  InformationBufferLength
    )
/*++

Routine Description:

    A routine that is called to be notified on PnP events.
    TODO: Finish me.
    
Arguments:

    IN  mpAdapterContext        - The adapter context.
    IN  PnPEvent                - The PnP event.
    IN  InformationBuffer       - The information buffer.
    IN  InformationBufferLength - The length of the information buffer.

Returns:

    Nothing.
    
--*/
{
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpPnPEventNotify\n"));

    UNREFERENCED_PARAMETER(mpAdapterContext);
    UNREFERENCED_PARAMETER(PnPEvent);
    UNREFERENCED_PARAMETER(InformationBuffer);
    UNREFERENCED_PARAMETER(InformationBufferLength);

    //
    // TODO: Finish me
    //

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpPnPEventNotify\n"));
}

static NDIS_STATUS MpQueryInformation(
    IN      NDIS_HANDLE  mpAdapterContext,
    IN      NDIS_OID     Oid,
    IN      PVOID        InformationBuffer,
    IN      ULONG        InformationBufferLength,
    OUT     PULONG       BytesWritten,
    OUT     PULONG       BytesNeeded
    )
/*++

Routine Description:

    MiniportQueryInformation handler.

Arguments:

   IN  mpAdapterContext        - Pointer to adapter structure
   IN  Oid                     - Oid for this query
   IN  InformationBuffer       - Buffer for this information
   IN  InformationBufferLength - Size of this buffer
   OUT BytesWritten            - Specifies how much info is written
   OUT BytesNeeded             - Specfies how many bytes are needed in case
                                 buffer is too small

Returns:

    NDIS_STATUS_SUCCESS on success
    NDIS_STATUS_PENDING if the request will be completed asynchronously
    NDIS_STATUS_INVALID_OID if the Oid was not not recognized
    NDIS_STATUS_INVALID_LENGTH if the length does not match the length
                               required by the Oid
    NDIS_STATUS_NOT_ACCEPTED if the Oid could not be processed as desired
    NDIS_STATUS_NOT_SUPPORTED if the Oid is not supported
    NDIS_STATUS_RESOURCES on insufficient resources.

--*/
{
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpQueryInformation\n"));

    BTHPAN_ADAPTER      *pAdapter       = MpFindAndRefAdapter (mpAdapterContext);
    if (! pAdapter) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] Adapter not found in MpQueryInformation\n"));

        return NDIS_STATUS_NOT_RECOGNIZED;
    }

    int nAdapter = pAdapter - gpPAN->aAdapters;

    NDIS_STATUS         ndisStatus      = NDIS_STATUS_SUCCESS;

    USHORT              usInfo          = 0;
    ULONG               ulInfoLen       = 0;
    ULONG               ulInfo          = 0;
    PVOID               pvInfo          = NULL;

    NDIS_MEDIA_STATE    ndisMediaState;

    gpPAN->Lock ();

    //
    //Initialise the result
    //

    *BytesWritten = 0;
    *BytesNeeded = 0;

    switch (Oid)
    {
    case OID_GEN_SUPPORTED_LIST:
        ulInfoLen = sizeof(g_oidSupported);
        pvInfo    = (PVOID) &g_oidSupported[0];
        break;

    case OID_GEN_HARDWARE_STATUS:
        //
        // We dont deal with the hardware. BTHPORT does this for us, since
        // we initialised the connections, we should report ready.
        //
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = NdisHardwareStatusReady;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_MEDIA_SUPPORTED:
    case OID_GEN_MEDIA_IN_USE:
        //
        // We support only 802.3
        //
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = NdisMedium802_3;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_MAXIMUM_LOOKAHEAD:
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = ETH_MAX_PACKET_SIZE;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_MAXIMUM_FRAME_SIZE:
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = ETH_MAX_PAYLOAD;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_LINK_SPEED:
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = 700000;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_TRANSMIT_BUFFER_SPACE:
        //
        // Report buffer space to queue up a single packet alone.
        // TODO: This should probably be the average number of Tx packets
        // allocated per connection.
        //
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = ETH_MAX_PACKET_SIZE;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_RECEIVE_BUFFER_SPACE:
        //
        // Report buffer space to queue up a single packet alone.
        // TODO: This should probably be the average number of Rx packets
        // allocated per connection.
        //
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = ETH_MAX_PACKET_SIZE;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_TRANSMIT_BLOCK_SIZE:
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = ETH_MAX_PACKET_SIZE;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_RECEIVE_BLOCK_SIZE:
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = ETH_MAX_PACKET_SIZE;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_VENDOR_ID:
        if (pAdapter->fAddressSet)
            NdisMoveMemory(&ulInfo, &pAdapter->ethAddr, 3);
        else {
            IFDBG(DebugOut (DEBUG_WARN, L"[PAN] OID_GEN_VENDOR_ID: address is not set, using Bluetooth address\n"));
            if ((gpPAN->ba.NAP == 0) && (gpPAN->ba.SAP == 0)) {
                IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] OID_GEN_VENDOR_ID: WARNING! Bluetooth address is not configured!\n"));
            }
            NdisMoveMemory(&ulInfo, gpPAN->eth, 3);
        }

        ulInfoLen = sizeof(ulInfo);
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_VENDOR_DESCRIPTION:
        ulInfoLen = sizeof("Microsoft");
        pvInfo    = "Microsoft";
        break;

    case OID_GEN_CURRENT_PACKET_FILTER:
        ulInfoLen = sizeof(ulInfo);
        pvInfo    = &pAdapter->ulPacketFilter;
        break;

    case OID_GEN_CURRENT_LOOKAHEAD:
        ulInfoLen = sizeof(ulInfo);
        pvInfo    = &pAdapter->ulLookAhead;
        break;

    case OID_GEN_DRIVER_VERSION:
        ulInfoLen = sizeof(USHORT);
        usInfo    = (USHORT) ( (BTHPAN_NDIS_MAJOR_VERSION << 8) |
                               (BTHPAN_NDIS_MINOR_VERSION) );
        pvInfo    = (PVOID) &usInfo;
        break;

    case OID_GEN_MAXIMUM_TOTAL_SIZE:
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = ETH_MAX_PACKET_SIZE;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_PROTOCOL_OPTIONS:
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = 0;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_MAC_OPTIONS:
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                    NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
                    NDIS_MAC_OPTION_NO_LOOPBACK;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_MEDIA_CONNECT_STATUS:        
        if ((pAdapter->type == NAP) || CountConn (pAdapter))
            ndisMediaState = NdisMediaStateConnected;
        else
            ndisMediaState = NdisMediaStateDisconnected;

        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"Media State is 0x%x\n", ndisMediaState));
        ulInfoLen = sizeof(ndisMediaState);
        pvInfo    = &ndisMediaState;
        break;

    case OID_GEN_MAXIMUM_SEND_PACKETS:
        //
        // This value is ignored since we are deserialised.
        //
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = 1;
        pvInfo    = &ulInfo;
        break;

    case OID_GEN_VENDOR_DRIVER_VERSION:
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = BTHPAN_VERSION;
        pvInfo    = (PVOID) &ulInfo;
        break;

        // case OID_GEN_SUPPORTED_GUIDS:
        //
        // WMI support
        //
        // ulInfoLen = sizeof(g_ngCustom);
        // pvInfo    = (PVOID) &g_ngCustom[0];
        // DINFO(("OID_GEN_SUPPORTED_GUIDS: reporting 0x%x bytes", ulInfoLen));
        // break;

    case OID_GEN_MEDIA_CAPABILITIES:
        //
        // TODO: Determine what we should report in this case.
        //

        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;

    case OID_GEN_PHYSICAL_MEDIUM:
        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;

    case OID_GEN_XMIT_OK:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"XmitOk = 0x%I64X\n",pAdapter->ul64XmitOk));
        ulInfoLen = sizeof(pAdapter->ul64XmitOk);
        pvInfo    = &pAdapter->ul64XmitOk;
        break;

    case OID_GEN_RCV_OK:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"RcvOk = 0x%I64X\r\b",pAdapter->ul64RcvOk));
        ulInfoLen = sizeof(pAdapter->ul64RcvOk);
        pvInfo    = &pAdapter->ul64RcvOk;
        break;

    case OID_GEN_XMIT_ERROR:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"XmitError = 0x%I64X\n",pAdapter->ul64XmitError));
        ulInfoLen = sizeof(pAdapter->ul64XmitError);
        pvInfo    = &pAdapter->ul64XmitError;
        break;

    case OID_GEN_RCV_ERROR:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"RcvError = 0x%I64X\n",pAdapter->ul64RcvError));
        ulInfoLen = sizeof(pAdapter->ul64RcvError);
        pvInfo    = &pAdapter->ul64RcvError;
        break;

    case OID_GEN_RCV_NO_BUFFER:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"RcvNoBuffer = 0x%I64X\n",pAdapter->ul64RcvNoBuffer));
        ulInfoLen = sizeof(pAdapter->ul64RcvNoBuffer);
        pvInfo    = &pAdapter->ul64RcvNoBuffer;
        break;

        //
        // These optional statistics are not supported.
        //
    case OID_GEN_DIRECTED_BYTES_XMIT:
    case OID_GEN_DIRECTED_FRAMES_XMIT:
    case OID_GEN_MULTICAST_BYTES_XMIT:
    case OID_GEN_MULTICAST_FRAMES_XMIT:
    case OID_GEN_BROADCAST_BYTES_XMIT:
    case OID_GEN_BROADCAST_FRAMES_XMIT:
    case OID_GEN_DIRECTED_BYTES_RCV:
    case OID_GEN_DIRECTED_FRAMES_RCV:
    case OID_GEN_MULTICAST_BYTES_RCV:
    case OID_GEN_MULTICAST_FRAMES_RCV:
    case OID_GEN_BROADCAST_BYTES_RCV:
    case OID_GEN_BROADCAST_FRAMES_RCV:
    case OID_GEN_RCV_CRC_ERROR:
    case OID_GEN_TRANSMIT_QUEUE_LENGTH:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"Query for unsupported statistics OID_GEN_* : 0x%x\n", Oid));
        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;

    case OID_802_3_CURRENT_ADDRESS:
    case OID_802_3_PERMANENT_ADDRESS:
        if (pAdapter->fAddressSet) {
            ulInfoLen = sizeof(pAdapter->ethAddr);
            pvInfo    = &pAdapter->ethAddr;
        } else {
            IFDBG(DebugOut (DEBUG_WARN, L"[PAN] OID_802_3_xxx_ADDRESS: address is not set, using Bluetooth address\n"));
            if ((gpPAN->ba.NAP == 0) && (gpPAN->ba.SAP == 0)) {
                IFDBG(DebugOut (DEBUG_WARN, L"[PAN] OID_802_3_xxx_ADDRESS: WARNING! Bluetooth address is not configured!\n"));
                ASSERT(0);                
            }
            
            ulInfoLen = sizeof(gpPAN->eth);
            pvInfo = gpPAN->eth;
        }

        break;

    case OID_802_3_MULTICAST_LIST:
        {
            unsigned __int64 ccMCasts = 0;

            int i;
            for (i = 0 ; i < (int)pAdapter->filter.cMCasts ; ++i) {
                unsigned __int64 from = (unsigned __int64)SET_NAP_SAP(pAdapter->filter.amc[i].from.NAP, pAdapter->filter.amc[i].from.SAP);
                unsigned __int64 to   = (unsigned __int64)SET_NAP_SAP(pAdapter->filter.amc[i].to.NAP,   pAdapter->filter.amc[i].to.SAP);
                ccMCasts += to - from + 1;
            }

            if (ccMCasts * 6 <= InformationBufferLength) {
                *BytesWritten = (ULONG)ccMCasts * 6;
                char *p = (char *)InformationBuffer;

                for (i = 0 ; i < (int)pAdapter->filter.cMCasts ; ++i) {
                    unsigned __int64 from = (unsigned __int64)SET_NAP_SAP(pAdapter->filter.amc[i].from.NAP, pAdapter->filter.amc[i].from.SAP);
                    unsigned __int64 to   = (unsigned __int64)SET_NAP_SAP(pAdapter->filter.amc[i].to.NAP,   pAdapter->filter.amc[i].to.SAP);
                    for (unsigned __int64 cnt = from ; cnt <= to ; ++cnt) {
                        BD_ADDR ba;
                        ba.NAP = GET_NAP(cnt);
                        ba.SAP = GET_SAP(cnt);
                        memcpy (p, &ba, sizeof(ba));
                        p += sizeof(ba);
                    }
                }

                ulInfoLen = 0;
                ndisStatus = NDIS_STATUS_SUCCESS;
            } else {
                ulInfoLen = (ULONG)ccMCasts * 6;
                ndisStatus = NDIS_STATUS_INVALID_LENGTH;
            }
        }
        break;

    case OID_802_3_MAXIMUM_LIST_SIZE:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"MAX_MCAST_LIST = %d\n", BTHPAN_MAX_MCAST_LIST));
        ulInfoLen = sizeof(ulInfo);
        ulInfo    = BTHPAN_MAX_MCAST_LIST;
        pvInfo    = &ulInfo;
        break;

    case OID_802_3_RCV_ERROR_ALIGNMENT:
        ulInfoLen = sizeof(pAdapter->ui32RcvErrAlign);
        pvInfo    = &pAdapter->ui32RcvErrAlign;
        break;

    case OID_802_3_XMIT_ONE_COLLISION:
        ulInfoLen = sizeof(pAdapter->ui32XmitOneCollision);
        pvInfo    = &pAdapter->ui32XmitOneCollision;
        break;

    case OID_802_3_XMIT_MORE_COLLISIONS:
        ulInfoLen = sizeof(pAdapter->ui32XmitMoreCollisions);
        pvInfo    = &pAdapter->ui32XmitMoreCollisions;
        break;

    //
    // Some OIDs that we know that we get but we are not supporting
    //

    case OID_TCP_TASK_OFFLOAD:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"OID_TCP_TASK_OFFLOAD not supported\n"));
        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;

    case OID_FFP_SUPPORT:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"OID_FFP_SUPPORT not supported\n"));
        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;

    //
    // TODO: Finish me. Power management oids
    //

    // OID_PNP_CAPABILITIES,
    // OID_PNP_SET_POWER,
    // OID_PNP_QUERY_POWER,
    // OID_PNP_ADD_WAKE_UP_PATTERN,
    // OID_PNP_REMOVE_WAKE_UP_PATTERN,
    // OID_PNP_ENABLE_WAKE_UP

    case OID_PNP_CAPABILITIES:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"OID_PNP_CAPABILITIES currently not supported\n"));
        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;

    default:
        IFDBG(DebugOut (DEBUG_WARN, L"[PAN] Query for unknown OID : 0x%x\n", Oid));
        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;
    }

    switch (ndisStatus)
    {
    case NDIS_STATUS_SUCCESS:
        //
        // Success so far, copy in if the length is ok.
        //

        if (ulInfoLen == 0)
            break;
        else if (ulInfoLen > InformationBufferLength) {
            ndisStatus = NDIS_STATUS_INVALID_LENGTH;
            *BytesNeeded = ulInfoLen;
            break;
        } else {
            NdisMoveMemory(InformationBuffer, pvInfo, ulInfoLen);
            *BytesWritten = ulInfoLen;
        }
        break;

    case NDIS_STATUS_INVALID_LENGTH:
        //
        // Invalid length, no data written, bytes needed is set
        //
        *BytesNeeded = ulInfoLen;
        break;

    case NDIS_STATUS_PENDING:
    case NDIS_STATUS_NOT_ACCEPTED:
    case NDIS_STATUS_NOT_SUPPORTED:
    case NDIS_STATUS_RESOURCES:
    case NDIS_STATUS_INVALID_OID:
        //
        // The request was issued and was pending or not accepted or invalid
        // or we ran out of resources. Nothing to do in these cases.
        //
        break;

    default:
        //
        // We are returning some value that NDIS doesn't expect. This should
        // never happen.
        //
        ASSERT(FALSE);
        break;
    }

    gpPAN->Unlock ();

    pAdapter->DelRef ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpQueryInformation\n"));

    return ndisStatus;
}

static NDIS_STATUS MpSetInformation(
    IN      NDIS_HANDLE  mpAdapterContext,
    IN      NDIS_OID     Oid,
    IN      PVOID        InformationBuffer,
    IN      ULONG        InformationBufferLength,
    OUT     PULONG       BytesRead,
    OUT     PULONG       BytesNeeded
    )
/*++

Routine Description:

    Sets specific information in the adapter structure. The data to be set is
    specified by the OID in concern.

Arguments:
    IN  mpAdaperContext         - The adapter
    IN  Oid                     - The Oid
    IN  InformationBuffer       - The data to set
    IN  InformationBufferLength - The length of the data
    OUT BytesRead               - The number of bytes read out
    OUT BytesNeeded             - The number of bytes needed to complete the
                                  request.
Returns:

    NDIS_STATUS_SUCCESS on success
    NDIS_STATUS_PENDING if the request will be completed asynchronously
    NDIS_STATUS_INVALID_OID if the Oid was not not recognized
    NDIS_STATUS_INVALID_LENGTH if the length does not match the length
                               required by the Oid
    NDIS_STATUS_INVALID_DATA if the data was invalid
    NDIS_STATUS_NOT_ACCEPTED if the Oid could not be processed as desired
    NDIS_STATUS_NOT_SUPPORTED if the Oid is not supported
    NDIS_STATUS_RESOURCES on insufficient resources.

--*/
{
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpSetInformation\n"));

    BTHPAN_ADAPTER      *pAdapter       = MpFindAndRefAdapter (mpAdapterContext);
    if (! pAdapter) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] Adapter not found in MpSetInformation\n"));

        return NDIS_STATUS_NOT_RECOGNIZED;
    }

    int nAdapter = pAdapter - gpPAN->aAdapters;

    NDIS_STATUS         ndisStatus         = NDIS_STATUS_SUCCESS;
    PVOID               pvInfo             = NULL;
    PVOID               pvEpilogueContext  = NULL;
    ULONG               ulInfoLen          = 0;
    BOOLEAN             bEpilogue          = FALSE;

    int fSkipCopy = FALSE;

    gpPAN->Lock ();

    //
    // Initialise the result
    //

    *BytesRead = 0;
    *BytesNeeded = 0;

    //
    // For each Oid we support, set the source and destination and the number
    // of bytes to move on success.
    //

    switch (Oid)
    {
    case OID_PAN_CONNECT:
        {
            if (InformationBufferLength < (sizeof(BD_ADDR) + sizeof(GUID))) {
                IFDBG(DebugOut (DEBUG_PAN_TRACE, L"OID_PAN_CONNECT : NDIS_STATUS_INVALID_LENGTH\n"));
                ndisStatus = NDIS_STATUS_INVALID_LENGTH;
                break;
            }

            fSkipCopy = TRUE;
            ulInfoLen = InformationBufferLength;

            BD_ADDR ba;
            GUID service;
            
            memcpy(&ba, InformationBuffer, sizeof(ba));
            memcpy(&service, ((PBYTE)InformationBuffer + sizeof(ba)), sizeof(GUID));            
            
            PanConnect (pAdapter, &ba, &service);            
        }
        break;

    case OID_PAN_DISCONNECT:
        {
            if (InformationBufferLength < sizeof(BD_ADDR)) {
                IFDBG(DebugOut (DEBUG_PAN_TRACE, L"OID_PAN_DISCONNECT : NDIS_STATUS_INVALID_LENGTH\n"));
                ndisStatus = NDIS_STATUS_INVALID_LENGTH;
                break;
            }

            fSkipCopy = TRUE;
            ulInfoLen = InformationBufferLength;

            BD_ADDR ba;
            memcpy (&ba, InformationBuffer, sizeof(ba));
            PanDisconnect (pAdapter, &ba);
        }    
        break;

    case OID_PAN_AUTHENTICATE:
        {
            if (InformationBufferLength < sizeof(DWORD)) {
                IFDBG(DebugOut (DEBUG_PAN_TRACE, L"OID_PAN_AUTHENTICATE : NDIS_STATUS_INVALID_LENGTH\n"));
                ndisStatus = NDIS_STATUS_INVALID_LENGTH;
                break;
            }

            fSkipCopy = TRUE;
            ulInfoLen = InformationBufferLength;

            gpPAN->fAuthenticate = *(PDWORD)InformationBuffer;
        }
        break;
 
    case OID_PAN_ENCRYPT:
        {
            if (InformationBufferLength < sizeof(DWORD)) {
                IFDBG(DebugOut (DEBUG_PAN_TRACE, L"OID_PAN_ENCRYPT : NDIS_STATUS_INVALID_LENGTH\n"));
                ndisStatus = NDIS_STATUS_INVALID_LENGTH;
                break;
            }

            fSkipCopy = TRUE;
            ulInfoLen = InformationBufferLength;

            gpPAN->fEncrypt = *(PDWORD)InformationBuffer;
        }
        break;

    case OID_802_3_MULTICAST_LIST:

        //
        // Verify the length
        //
        if (InformationBufferLength % ETH_LENGTH_OF_ADDRESS != 0)
        {
            ndisStatus = NDIS_STATUS_INVALID_LENGTH;
            break;
        }

        if ((int)(InformationBufferLength / ETH_LENGTH_OF_ADDRESS) > SVSUTIL_ARRLEN(pAdapter->filter.amc))
        {
            ndisStatus = NDIS_STATUS_INVALID_LENGTH;
            ulInfoLen = SVSUTIL_ARRLEN(pAdapter->filter.amc) * ETH_LENGTH_OF_ADDRESS;
            break;
        }

        //
        // Save the number of entries in the muliticast list
        //
        {
            pAdapter->filter.cMCasts = 0;
            fSkipCopy = TRUE;

            int cc = InformationBufferLength / ETH_LENGTH_OF_ADDRESS;
            char *p = (char *)InformationBuffer;
            for (int i = 0 ; i < cc ; ) {
                memcpy (&pAdapter->filter.amc[pAdapter->filter.cMCasts].from, p, 6);
                memcpy (&pAdapter->filter.amc[pAdapter->filter.cMCasts].to, p, 6);
                unsigned __int64 btprev = SET_NAP_SAP(pAdapter->filter.amc[pAdapter->filter.cMCasts].to.NAP, pAdapter->filter.amc[pAdapter->filter.cMCasts].to.SAP);

                for ( ; ; ) {
                    p += 6;
                    ++i;
                    if (i >= cc)
                        break;

                    BD_ADDR ba;
                    memcpy (&ba, p, 6);
                    unsigned __int64 btnew  = SET_NAP_SAP(ba.NAP, ba.SAP);

                    if (btnew != btprev + 1)
                        break;

                    pAdapter->filter.amc[pAdapter->filter.cMCasts].to = ba;
                    btprev = btnew;
                }

                ++pAdapter->filter.cMCasts;
            }
        }

        //
        // We have more work to do.
        //
        bEpilogue = TRUE;
        pvEpilogueContext = NULL;
        break;

    case OID_GEN_CURRENT_PACKET_FILTER:

        ulInfoLen = sizeof(pAdapter->ulPacketFilter);
        pvInfo = &pAdapter->ulPacketFilter;
        
        //
        // We have more work to do.
        //
        bEpilogue = TRUE;
        pvEpilogueContext = NULL;
        break;

    case OID_GEN_CURRENT_LOOKAHEAD:

        ulInfoLen = sizeof(pAdapter->ulLookAhead);
        pvInfo    = &pAdapter->ulLookAhead;
        break;

    //
    // Some OIDs that we know that we get but we are not supporting
    //

    case OID_GEN_NETWORK_LAYER_ADDRESSES:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"OID_GEN_NETWORK_LAYER_ADDRESSES not supported\n"));
        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;

    case OID_GEN_TRANSPORT_HEADER_OFFSET:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"OID_GEN_TRANSPORT_HEADER_OFFSET not supported\n"));
        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;

    //
    // TODO: Finish me. Power management oids
    //

    // OID_PNP_CAPABILITIES,
    // OID_PNP_SET_POWER,
    // OID_PNP_QUERY_POWER,
    // OID_PNP_ADD_WAKE_UP_PATTERN,
    // OID_PNP_REMOVE_WAKE_UP_PATTERN,
    // OID_PNP_ENABLE_WAKE_UP

    case OID_PNP_ADD_WAKE_UP_PATTERN:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"OID_PNP_ADD_WAKE_UP_PATTERN not supported\n"));
        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;

    case OID_PNP_REMOVE_WAKE_UP_PATTERN:
        IFDBG(DebugOut (DEBUG_PAN_TRACE, L"OID_PNP_REMOVE_WAKE_UP_PATTERN not supported\n"));
        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;

    default:

        IFDBG(DebugOut (DEBUG_WARN, L"[PAN] Set for unknown OID : 0x%x\n", Oid));
        ndisStatus = NDIS_STATUS_NOT_SUPPORTED;
        break;
    }

    switch(ndisStatus)
    {
    case NDIS_STATUS_SUCCESS:
        //
        // Success, so verify length and copy out information
        //
        if (ulInfoLen != InformationBufferLength)
        {
            ndisStatus = NDIS_STATUS_INVALID_LENGTH;
            *BytesNeeded = ulInfoLen;
            break;
        }

        if (! fSkipCopy) {
            NdisMoveMemory(pvInfo, InformationBuffer, ulInfoLen);

            *BytesRead = ulInfoLen;
        }

        //
        // Do the Epilogue only on success.
        //
        if (bEpilogue == TRUE)
            MpSetInformationEpilogue (pAdapter, Oid);
        break;

    case NDIS_STATUS_PENDING:
    case NDIS_STATUS_NOT_ACCEPTED:
        //
        // The request was issued, will be completed later or was not
        // accepted. We have read out the data.
        //
        *BytesRead = ulInfoLen;
        break;

    case NDIS_STATUS_INVALID_DATA:
    case NDIS_STATUS_INVALID_LENGTH:
        //
        // Invalid length or insufficient resources or invalid data, we
        // havent read out any data.
        //
        *BytesNeeded = ulInfoLen;
        break;

    case NDIS_STATUS_RESOURCES:
    case NDIS_STATUS_NOT_SUPPORTED:
    case NDIS_STATUS_INVALID_OID:
        //
        // OID was not supported or invalid or we are out of resources.
        //
        break;

    default:
        //
        // We are returning an error code that NDIS doesn't expect. This
        // should never happen.
        //
        ASSERT(FALSE);
        break;
    }

    gpPAN->Unlock ();

    pAdapter->DelRef ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpSetInformation\n"));

    return ndisStatus;
}

static VOID MpReturnPacket(
    IN      NDIS_HANDLE   mpAdapterContext,
    IN      PNDIS_PACKET  pNdisPacket
    )
/*++

Routine Description:

    Called when Ndis returns a packet back to us, so that we may use it
    again.

Arguments:

    IN mpAdapterContext - The adapter
    IN pNdisPacket      - The packet that we gave up to ndis, has our packet
                          structure in its miniport reserved parameter

Returns:

    Nothing.

--*/
{
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpReturnPacket\n"));

    BTHPAN_ADAPTER      *pAdapter       = MpFindAndRefAdapter (mpAdapterContext);
    if (! pAdapter) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] Adapter not found in MpReturnPacket\n"));

        return;
    }


    MpFreePacket (pAdapter, pNdisPacket);

    pAdapter->DelRef ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpReturnPacket\n"));
}

static VOID MpSendPackets(
    IN  NDIS_HANDLE    mpAdapterContext,
    IN  PPNDIS_PACKET  pPacketArray,
    IN  UINT           uiNumPackets
    )
/*++

Routine Description:

    The routine to handle send requests from NDIS.

Arguments:

    IN mpAdapterContext - The adapter we must send out on
    IN PacketArray      - The array of packets to send out
    IN NumberOfPackets  - The number of packets in the array

Returns:

    Nothing.

--*/
{
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpSendPackets\n"));

    BTHPAN_ADAPTER      *pAdapter       = MpFindAndRefAdapter (mpAdapterContext);
    if (! pAdapter) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] Adapter not found in MpSendPackets\n"));

        return;
    }

    NDIS_STATUS      ndisStatus  = NDIS_STATUS_SUCCESS;
    UINT             ui          = 0;

    //
    // Dont send the packets out if we are disconnected. Also send up
    // a media status indication to stop further packets. This is needed
    // for older protocols such as IPX that dont check the media connect
    // status before sending.
    //

    gpPAN->Lock ();
    int iConn = CountConn (pAdapter);
    gpPAN->Unlock ();

    if (iConn == 0) {
        if (pAdapter->type != NAP) {            
            ndisStatus = NDIS_STATUS_MEDIA_DISCONNECT;
        }

        MpMediaStatusIndicate (pAdapter);

        for (ui = 0; ui < uiNumPackets; ui++)
            MpSendComplete (pAdapter, pPacketArray[ui], ndisStatus);
        
        goto error;
    }
    
    //
    // Loop through sending all packets.
    //
    for (ui = 0; ui < uiNumPackets; ui++) {
        ndisStatus = MpPacketSend (pAdapter, pPacketArray[ui], FALSE);

        if (ndisStatus != NDIS_STATUS_PENDING)
            MpSendComplete (pAdapter, pPacketArray[ui], ndisStatus);
    }

error:

    pAdapter->DelRef ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpSendPackets\n"));
}


//
// TODO: Implement MpCanceSendPackets()
//


static VOID MpShutdown(
    IN  PVOID  ShutdownContext
    )
/*++

Routine Description:

    Temporary function to pacify NDIS

Arguments:

    IN ShutdownContext - The adapter to shutdown

Returns:

    Nothing.

--*/
{
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpShutdown\n"));

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpShutdown\n"));
}


static BOOLEAN MpCheckForHang(
    IN NDIS_HANDLE mpAdapterContext
    )
/*++

Routine Description:

    Checks if an adapter is in a bad state. Merely asks the pan layer to
    examine if every thing is ok or not.
    
Arguments:

    IN mpAdapterContext - The adapter that must be checked.
    
Returns:

    TRUE if the adapter was determined to be in a bad state.
    FALSE otherwise.
    
--*/
{
    BOOLEAN  bHung    = FALSE;
    
//    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpCheckForHang\n"));
//    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpCheckForHang\n"));

    return bHung;
}

extern "C" NDIS_STATUS
DriverEntry(
    IN  PDRIVER_OBJECT   pDriverObject,
    IN  PUNICODE_STRING  uszRegistryPath
    )
/*++

Routine Description:

    Registers the miniport and returns its status

Arguments:

    IN pDriverObject  - Pointer to driver object
    IN szRegistryPath - Pointer to the registry

Returns:

    NDIS_STATUS_SUCCESS on success or
    status of NdisRegisterMiniport on failure or
    status of L2capifRegisterMiniport on failure

--*/
{
    DEBUGMSG(1, (L"BTHPAN:: ++DriverEntry\r\n"));

    NTSTATUS     ntStatus          = STATUS_SUCCESS;
    NDIS_STATUS  ndisStatus        = NDIS_STATUS_SUCCESS;
    NDIS_MINIPORT_CHARACTERISTICS  mpChar   = {0};

    HANDLE hDevice = RegisterDevice (L"BTD", 0, L"btd.dll", 0);
    DEBUGMSG(1, (L"BTHPAN: NdisMInitializeWrapper loaded Bluetooth stack h=0x%08x, GetLastError = %d\r\n", hDevice, GetLastError ()));

    HANDLE hEvent = OpenEvent (EVENT_ALL_ACCESS, FALSE, BTH_NAMEDEVENT_STACK_INITED);
    if (! hEvent) {
        ndisStatus = NDIS_STATUS_FAILURE;
        DEBUGMSG(1, (L"BTHPAN: NdisMInitializeWrapper failed (no event) 0x%x\r\n", ndisStatus));
        goto error;
    }

    WaitForSingleObject (hEvent, INFINITE);
    CloseHandle (hEvent);
    
    //
    // Notify NDIS wrapper about this driver, get a ndis wrapper handle back
    //
    NDIS_HANDLE hWrapper = NULL;

    NdisMInitializeWrapper (&hWrapper, pDriverObject, uszRegistryPath, NULL);

    if (NULL == hWrapper) {
        ndisStatus = NDIS_STATUS_FAILURE;
        DEBUGMSG(1, (L"BTHPAN: NdisMInitializeWrapper failed 0x%x\r\n", ndisStatus));
        goto error;
    }

    //
    // Fill up the miniport characteristics with the version number and entry
    // points for the for the driver supplied MiniportXXX functions
    //

    NdisZeroMemory(&mpChar, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

    mpChar.Ndis50Chars.Ndis40Chars.Ndis30Chars.MajorNdisVersion        = BTHPAN_NDIS_MAJOR_VERSION;
    mpChar.Ndis50Chars.Ndis40Chars.Ndis30Chars.MinorNdisVersion        = BTHPAN_NDIS_MINOR_VERSION;

    mpChar.Ndis50Chars.Ndis40Chars.Ndis30Chars.HaltHandler             = MpHalt;
    mpChar.Ndis50Chars.Ndis40Chars.Ndis30Chars.InitializeHandler       = MpInitialize;
    mpChar.Ndis50Chars.Ndis40Chars.Ndis30Chars.QueryInformationHandler = MpQueryInformation;
    mpChar.Ndis50Chars.Ndis40Chars.Ndis30Chars.SetInformationHandler   = MpSetInformation;
    mpChar.Ndis50Chars.Ndis40Chars.Ndis30Chars.CheckForHangHandler      = MpCheckForHang;
    mpChar.Ndis50Chars.Ndis40Chars.ReturnPacketHandler     = MpReturnPacket;
    mpChar.Ndis50Chars.Ndis40Chars.SendPacketsHandler      = MpSendPackets;
    mpChar.PnPEventNotifyHandler   = MpPnPEventNotify;
    mpChar.AdapterShutdownHandler  = MpShutdown;

    ndisStatus = NdisIMRegisterLayeredMiniport (hWrapper, &mpChar, sizeof(NDIS_MINIPORT_CHARACTERISTICS), &gpPAN->ndisDriverHandle);

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
        DEBUGMSG(1, (L"BTHPAN: NdisMRegisterMiniport failed 0x%x\r\n", ndisStatus));
        goto error;
    }

 error:

    if (ndisStatus != NDIS_STATUS_SUCCESS) {        
        //
        // Failure, free up resources
        //
        if (hWrapper != NULL)
            NdisTerminateWrapper(
                hWrapper,
                NULL);
    }

    DEBUGMSG(1, (L"BTHPAN:: --DriverEntry (0x%08x)\r\n", ndisStatus));

    return ndisStatus;
}

int pan_InitializeOnce (void) {
    IFDBG(DebugOut(DEBUG_PAN_INIT, L"PAN Init:: entered\n"));

    SVSUTIL_ASSERT (! gpPAN);

    if (gpPAN) {
        IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] init:: ERROR_ALREADY_EXISTS\n"));
        return ERROR_ALREADY_EXISTS;
    }

    gpPAN = new PAN_CONTEXT;

    if (gpPAN) {
        IFDBG(DebugOut (DEBUG_PAN_INIT, L"PAN init:: ERROR_SUCCESS\n"));
        return ERROR_SUCCESS;
    }

    IFDBG(DebugOut (DEBUG_ERROR, L"[PAN] init:: ERROR_OUTOFMEMORY\n"));
    return ERROR_OUTOFMEMORY;
}

int pan_CreateDriverInstance (void) {
    IFDBG(DebugOut(DEBUG_PAN_INIT, L"pan_CreateDriverInstance:: entered\n"));

    gpPAN->state = CONNECTING;

    L2CAP_EVENT_INDICATION lei;
    memset (&lei, 0, sizeof(lei));

    L2CAP_CALLBACKS lc;
    memset (&lc, 0, sizeof(lc));

    lei.l2ca_StackEvent = pan_stack_event;
    lei.l2ca_ConfigInd = pan_config_ind;
    lei.l2ca_ConnectInd = pan_connect_ind;
    lei.l2ca_DisconnectInd = pan_disconnect_ind;
    lei.l2ca_DataUpInd = pan_data_up_ind;

    lc.l2ca_CallAborted = pan_call_aborted;

    lc.l2ca_ConfigReq_Out = pan_config_req_out;
    lc.l2ca_ConfigResponse_Out = pan_config_response_out;
    lc.l2ca_ConnectReq_Out = pan_connect_req_out;
    lc.l2ca_ConnectResponse_Out = pan_connect_response_out;
    lc.l2ca_DataDown_Out = pan_data_down_out;
    lc.l2ca_Disconnect_Out = pan_disconnect_out;

    int psm = BTHPAN_PSM;   // BNEP 

      int iRes = L2CAP_EstablishDeviceContext (gpPAN, 0x00f, &lei, &lc, &gpPAN->l2cap_if,
            &gpPAN->cDataHeaders, &gpPAN->cDataTrailers, &gpPAN->hL2CAP);

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] pan_InitializeOnce:: L2CAP_EstablishDeviceContext failed, %d\n", iRes));
        return iRes;
    }

    if ((! gpPAN->hL2CAP) || (! (gpPAN->l2cap_if.l2ca_AbortCall && gpPAN->l2cap_if.l2ca_ConfigReq_In && gpPAN->l2cap_if.l2ca_ConfigResponse_In &&
        gpPAN->l2cap_if.l2ca_ConnectReq_In && gpPAN->l2cap_if.l2ca_ConnectResponse_In && gpPAN->l2cap_if.l2ca_DataDown_In &&
        gpPAN->l2cap_if.l2ca_Disconnect_In)) || (gpPAN->cDataHeaders + gpPAN->cDataTrailers > 128)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] pan_InitializeOnce:: L2cap context inadequate, ERROR_NOT_SUPPORTED\n"));

        gpPAN->Reinit ();

        return ERROR_NOT_SUPPORTED;
    }

    gpPAN->pfmdPackets = svsutil_AllocFixedMemDescr (sizeof(BNEPPacket), 10);
    if (! gpPAN->pfmdPackets) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] pan_InitializeOnce:: Error allocating fixed mem descriptor, ERROR_OUTOFMEMORY\n"));

        L2CAP_CloseDeviceContext (gpPAN->hL2CAP);
        
        gpPAN->Reinit ();
        
        return ERROR_OUTOFMEMORY;
    }

    SetMediaParms ();

    iRes = ERROR_OUTOFMEMORY;

    gpPAN->hRefreshEvent = CreateEvent (NULL, FALSE, FALSE, BTH_NAMEDEVENT_PAN_REFRESH);
    if (gpPAN->hRefreshEvent) {
        gpPAN->hMediaThread = CreateThread (NULL, 0, NetworkMonitorThread, NULL, 0, NULL);
        if (gpPAN->hMediaThread)
            iRes = ERROR_SUCCESS;
    }

    if (iRes != ERROR_SUCCESS) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] pan_InitializeOnce:: out of memory creating event and media sense thread\n"));

        L2CAP_CloseDeviceContext (gpPAN->hL2CAP);

        if (gpPAN->pfmdPackets) {
            svsutil_ReleaseFixedEmpty (gpPAN->pfmdPackets);
            gpPAN->pfmdPackets = NULL;
        }

        if (gpPAN->hRefreshEvent)
            CloseHandle (gpPAN->hRefreshEvent);

        gpPAN->Reinit ();

        return ERROR_OUTOFMEMORY;
    }

    AddSDPRecord();

    gpPAN->state = UP;

    IFDBG(DebugOut(DEBUG_PAN_INIT, L"pan_CreateDriverInstance:: ERROR_SUCCESS\n"));

    return ERROR_SUCCESS;
}

int pan_Activate (BOOL fActivate) {
    IFDBG(DebugOut(DEBUG_PAN_INIT, L"pan_Activate:: entered\n"));

    if ((! gpPAN) || (gpPAN->state != UP)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] pan_Activate:: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    int iRes = ERROR_SUCCESS;

    if (fActivate) {
        // Activate
        InterlockedIncrement((LONG*)&gpPAN->ActivateRefCount); 
        if ((gpPAN->ActivateRefCount == 1) && gpPAN->fStackUp) {
            // Init PAN instance when stack is up and ref count goes from 0 to 1
            NotifyNDISOfPanState(TRUE);
        }               
    } else if (gpPAN->ActivateRefCount > 0) {
        // Deactivate
        InterlockedDecrement((LONG*)&gpPAN->ActivateRefCount);
        if ((gpPAN->ActivateRefCount == 0) && gpPAN->fStackUp) {
            // Deinit PAN instance when stack is up and ref count hits 0
            NotifyNDISOfPanState(FALSE);
        }
    } else {
        // Ref count is 0 and deactivate was called, error!
        iRes = ERROR_INVALID_STATE;
    }

    IFDBG(DebugOut(DEBUG_PAN_INIT, L"pan_Activate:: iRes=%d\n", iRes));

    return iRes;
}

int pan_CloseDriverInstance (void) {
    IFDBG(DebugOut(DEBUG_PAN_INIT, L"pan_CloseDriverInstance:: entered\n"));

    if ((! gpPAN) || (gpPAN->state != UP)) {
        IFDBG(DebugOut(DEBUG_ERROR, L"[PAN] pan_CloseDriverInstance:: ERROR_SERVICE_NOT_ACTIVE\n"));
        return ERROR_SERVICE_NOT_ACTIVE;
    }

    gpPAN->state = CLOSING;

    if (gpPAN->hRefreshEvent) {
        SetEvent (gpPAN->hRefreshEvent);
        CloseHandle (gpPAN->hRefreshEvent);
    }

    // When this is done, media sense thread will exit

    if (gpPAN->hMediaThread) {
        WaitForSingleObject (gpPAN->hMediaThread, INFINITE);
        CloseHandle (gpPAN->hMediaThread);
    }

    for (int i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aAdapters) ; ++i) {
        if (gpPAN->aAdapters[i].state != DOWN)
            NdisMRemoveMiniport (gpPAN->aAdapters[i].hAdapter);

        MpHalt ((NDIS_HANDLE)&gpPAN->aAdapters[i]);
    }

    gpPAN->Lock ();
    // Close all connections...
    for (int i = 0 ; i < SVSUTIL_ARRLEN(gpPAN->aConn) ; ++i) {
        gpPAN->aConn[i].nAdapter = -1;
        CloseConnection (&gpPAN->aConn[i], NULL);
    }

    gpPAN->state = DOWN;

    if (gpPAN->pfmdPackets) {
        svsutil_ReleaseFixedEmpty (gpPAN->pfmdPackets);
        gpPAN->pfmdPackets = NULL;
    }

    if (gpPAN->hL2CAP) {
        L2CAP_CloseDeviceContext (gpPAN->hL2CAP);
        gpPAN->hL2CAP = NULL;
    }

    gpPAN->Reinit ();

    gpPAN->Unlock ();

    IFDBG(DebugOut(DEBUG_PAN_INIT, L"pan_CloseDriverInstance:: ERROR_SUCCESS\n"));
    return ERROR_SUCCESS;
}

int pan_UninitializeOnce (void) {
    PAN_CONTEXT *pPAN = gpPAN;
    gpPAN = NULL;
    
    delete pPAN;

    return ERROR_SUCCESS;
}

//
//
//
//  Graveyard
//
#if defined (GRAVEYARD)
//
//  Miniport interface: these functions are called from 
//
static VOID MpMediaWatchdog (IN PVOID Specific1, IN PVOID hContext, IN PVOID Specific2, IN PVOID Specific3) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpMediaWatchdog\n"));

    BTHPAN_ADAPTER *pAdapter = (BTHPAN_ADAPTER*)MpFindAndRefAdapter (hContext);

    if (! pAdapter) {
        IFDBG(DebugOut(DEBUG_PAN_INIT, L"--MpMediaWatchdog - no adapter\n"));
        return;
    }

    MpMediaStatusIndicate (pAdapter);
    
    pAdapter->DelRef ();

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpMediaWatchdog\n"));
}

static VOID MpReceiveComplete (BTHPAN_ADAPTER *pAdapter, BOOLEAN bHasResources, PNDIS_PACKET pNdisPacket) {
    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"++MpReceiveComplete\n"));

    NDIS_STATUS   ndisStatus    = NDIS_STATUS_SUCCESS;
    UINT          uiNumPackets  = 0;
    PNDIS_PACKET  pNdisPacketArray[1];

    if (bHasResources == FALSE)
        ndisStatus = NDIS_STATUS_RESOURCES;

    NDIS_SET_PACKET_STATUS(pNdisPacket, ndisStatus);

    uiNumPackets        = 1;
    pNdisPacketArray[0] = pNdisPacket;

    NdisMIndicateReceivePacket (pAdapter->hAdapter, pNdisPacketArray, uiNumPackets);

    if (ndisStatus == NDIS_STATUS_RESOURCES)
        MpFreePacket (pAdapter, pNdisPacket);

    IFDBG(DebugOut (DEBUG_PAN_TRACE, L"--MpReceiveComplete\n"));
}


#endif

//#error Need to verify that SSID are unique
//#error SSID OID for connect path
//#error OID to trigger immediate connection
//#error NEED to edit ARPs (and maybe more) to maintain separate externally visible mac address
